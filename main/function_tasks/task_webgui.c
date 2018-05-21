/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 * 
 * Copyright 2017 Benjamin Aigner <aignerb@technikum-wien.at,
 * beni@asterics-foundation.org>
 */
/**
 * @file 
 * @brief CONTINOUS TASK - Web server & WebGUI handling.
 * 
 * This module is used to serve the web- & DNS server for captive portal
 * mode and the web configuration GUI.
 * 
 * In addition, AT commands are sent to the command parser, the same
 * way as if they would be sent by any host configuration software.
 * In this mode feedback (which is usually sent to the serial interface)
 * is sent back to the webgui for parsing in JS.
 * 
 * @see hal_serial
 * @see task_commands
 * @see atcmd_api
 * 
 * @todo Add wifi settings to cfg struct in hal_storage/common; default fallback should be defined
 * @todo After implementing, test everything....
 * */

#include "task_webgui.h"

/** @brief Logging tag for this module */
#define LOG_TAG "web"

/** @brief Authentication mode for wifi hotspot */
#define CONFIG_AP_AUTHMODE WIFI_AUTH_WPA2_PSK
/** @brief Set AP visibility, 1 if it should be hidden */
#define CONFIG_AP_SSID_HIDDEN 0
/** @brief Name of the wifi hotspot */
#define CONFIG_AP_SSID "FLipMouse"
/** @brief Hotspot default password */
#define CONFIG_AP_PASSWORD "foundation"
/** @brief Wifi channel to be used, 0 means automatic (right?) */
#define CONFIG_AP_CHANNEL 0
/** @brief Wifi max number of connection */
#define CONFIG_AP_MAX_CONNECTIONS 4
/** @brief Wifi beacon interval */
#define CONFIG_AP_BEACON_INTERVAL	100

/** @brief Partition name (used to define different memory types) */
const static char *base_path = "/spiflash";
/** @brief Currently used wifi password */
static char wifipw[64];

/** @brief Wear levelling handle */
static wl_handle_t fat_handle = WL_INVALID_HANDLE;

/** @brief Static HTTP HTML header */
const static char http_html_hdr[] = "HTTP/1.1 200 OK\nContent-type: text/html\n\n";
/** @brief Static HTTP 404 header */
const static char http_404_hdr[] = "HTTP/1.1 404 NOT FOUND\n\n";

/** @brief Timer handle for auto-disabling wifi */
TimerHandle_t wifiTimer;

/** @brief Task handle for websocket task */
TaskHandle_t wifiWSServerHandle_t = NULL;
/** @brief Task handle for http server task */
TaskHandle_t wifiHTTPServerHandle_t = NULL;
/** @brief Signals any Wifi related task to quit, if set to zero */
uint8_t wifiActive = 0;

/** @brief Get the number of currently connected Wifi stations
 * @return Number of connected clients */
static int8_t getNumberOfWifiStations(void)
{
 	wifi_sta_list_t wifi_sta_list;
	tcpip_adapter_sta_list_t adapter_sta_list;
   
	memset(&wifi_sta_list, 0, sizeof(wifi_sta_list));
	memset(&adapter_sta_list, 0, sizeof(adapter_sta_list));
   
	ESP_ERROR_CHECK(esp_wifi_ap_get_sta_list(&wifi_sta_list));	
	ESP_ERROR_CHECK(tcpip_adapter_get_sta_list(&wifi_sta_list, &adapter_sta_list));
  
  return adapter_sta_list.num;
}

/** @brief Simply reset & start OR stop the auto-off timer
 * @see WIFI_OFF_TIME
 * @param onoff If 0, timer will be stopped, started otherwise
 * */
void wifiStartStopOffTimer(int onoff)
{
  if(wifiTimer == NULL)
  {
    ESP_LOGE(LOG_TAG,"Wifi timer is NULL, cannot start/stop");
    return;
  }
  
  if(onoff == 0)
  {
    //check if timer is active
    if(xTimerIsTimerActive(wifiTimer) != pdFALSE)
    {
      //check if we can stop the timer
      if(xTimerStop(wifiTimer,0) != pdPASS)
      {
        ESP_LOGE(LOG_TAG,"Error stopping wifi timer, wifi will be disabled automatically!");
      }
    }
  } else {
    //first, reset the timer (if there was already some time passed)
    if(xTimerReset(wifiTimer,0) != pdPASS)
    {
      ESP_LOGE(LOG_TAG,"could not reset auto-off timer, won't start!");
    } else {
      //start timer
      if(xTimerStart(wifiTimer,0) != pdPASS)
      {
        ESP_LOGE(LOG_TAG,"Cannot start auto-off timer!");
      }
    }
  }
}

/** @brief CONTINOUS TASK - Websocket server task
 * 
 * This task is used to handle the websocket on port 1804.
 * Websocket is used for data transmission between a connected client
 * and the FLipMouse/FABI. Incoming data is in the same format
 * as on the serial interface, therefore simply put into the command
 * queue.
 * Outgoing data is equally to serial output data.
 * 
 * @see halSerialATCmds
 * @see halSerialSendUSBSerial
 * */
void ws_server(void *pvParameters) {
	//connection references
	struct netconn *conn, *newconn;

	//set up new TCP listener
	conn = netconn_new(NETCONN_TCP);
	netconn_bind(conn, NULL, TASK_WEBGUI_WSPORT);
	netconn_listen(conn);
  ESP_LOGI(LOG_TAG,"Websocket server started");
	//wait for connections
	while((netconn_accept(conn, &newconn) == ERR_OK) && wifiActive)
  {
    ESP_LOGI(LOG_TAG,"Incoming WS connection");
		ws_server_netconn_serve(newconn);
  }
	//close connection
	netconn_close(conn);
  vTaskDelete(NULL);
}


/** @brief Serve static content from FAT
 * 
 * This method is used to send data from the FAT partition to a client
 * @param resource Name of the resource to be sent
 * @param conn Currently active connection
 */
void fat_serve(char* resource, struct netconn *conn) {
  //basepath + 8.3 file + folder + margin
  char file[sizeof(base_path)+32];
  sprintf(file,"%s%s",base_path,resource);
  ESP_LOGI(LOG_TAG,"serving from FAT: %s",file);
  
  struct stat st;
	if (stat(file, &st) == 0) {
		netconn_write(conn, http_html_hdr, sizeof(http_html_hdr) - 1, NETCONN_NOCOPY);
		
		// open the file for reading
		FILE* f = fopen(file, "r");
		if(f == NULL) {
			ESP_LOGE(LOG_TAG,"Unable to open the file %s\n", file);
			return;
		}
		
		//allocate a buffer in size of one FAT block
		char* buffer = malloc(512);
    
		int i=0, len=0;
    
    do {
			len=fread(buffer, 1, 512, f);
			i+=len;
			ESP_LOGV(LOG_TAG,"Writing bytes %d\n", i);
			netconn_write(conn, buffer, len, NETCONN_NOCOPY);
		} while (len==512);
		
    ESP_LOGI(LOG_TAG,"Sent %u bytes",i);
		fclose(f);
		fflush(stdout);
    free(buffer);
	}
	else {
		ESP_LOGW(LOG_TAG,"Resource not found: %s\n", file);
		netconn_write(conn, http_404_hdr, sizeof(http_404_hdr) - 1, NETCONN_NOCOPY);
	}
}

/** @brief Handle an incoming HTTP connection
 * 
 * This handler is used to server the webpage to a connected client.
 * Data is read from the FAT/VFS partition of the flash.
 * The partition is created and uploaded by *make makefatfs flashfatfs*
 * @param conn Current active net connection
 * */
static void http_server_netconn_serve(struct netconn *conn) {

	struct netbuf *inbuf;
	char *buf;
	u16_t buflen;
	err_t err;
  //receive incoming data/request
	err = netconn_recv(conn, &inbuf);

	if (err == ERR_OK) {
	  
		// get the request and terminate the string
		netbuf_data(inbuf, (void**)&buf, &buflen);
		buf[buflen] = '\0';
    char *request_line = strtok(buf, "\n");
		
		if(request_line) {
			// default page -> redirect to index.html
			if(strstr(request_line, "GET / ")) {
				fat_serve((char*)"/INDEX.HTM", conn);
			} else {
				// get the requested resource
        char* method = strtok(request_line, " ");
        //get rid of "unused" variable.
        //strtok although needs this call.
        (void)(method);
				char* resource = strtok(NULL, " ");
				fat_serve(resource, conn);
			}
		}
	}
	
	// close the connection and free the buffer
	netconn_close(conn);
	netbuf_delete(inbuf);
}



/** @brief CONTINOUS TASK - Main webserver task
 * 
 * This task is used to handle incoming connections via netconn_accept.
 * Each time a connection is opened, it will be served by
 * http_server_netconn_serve.
 * 
 * @see http_server_netconn_serve
 * @param pvParameters Unused.
 * */
static void http_server(void *pvParameters) {
	
	struct netconn *conn, *newconn;
	err_t err;
  //create a new TCP connection
	conn = netconn_new(NETCONN_TCP);
  //bind it to incoming port 80 (HTTP)
	netconn_bind(conn, NULL, 80);
  //LISTEN & wait...
	netconn_listen(conn);
	ESP_LOGI(LOG_TAG,"http_server task started");
	do {
    //accept connection & save netconn handle
		err = netconn_accept(conn, &newconn);
		if (err == ERR_OK) {
      //serve data to this connection
			http_server_netconn_serve(newconn);
      //close connection if finished
			netconn_delete(newconn);
		}
		vTaskDelay(2); //allows task to be pre-empted
	} while((err == ERR_OK) && wifiActive);
  //if we run into an error
	netconn_close(conn);
	netconn_delete(conn);
  vTaskDelete(NULL);
}

/** @brief Event handler for wifi status updates
 * 
 * This handler is used to update flags for connected clients & active
 * wifi.
 * In addition, if a client is connected, an additional serial stream is
 * added to halSerial, which is used to transfer outgoing data to the
 * websocket (in addition to the normal serial interface).
 * 
 * @see connectionRoutingStatus
 * @see WIFI_ACTIVE
 * @see WIFI_CLIENT_CONNECTED
 * @return ESP_OK on success, ESP_FAIL otherwise
 * @param ctx Unused
 * @param event Pointer to system event
 * */
static esp_err_t wifi_event_handler(void *ctx, system_event_t *event) {
  //check if event flags are active
  if(connectionRoutingStatus == NULL)
  {
    ESP_LOGE(LOG_TAG,"Status flags for wifi are NULL! Should not receive an event!");
    return ESP_FAIL;
  }
  
  //determine event: print information and set flags accordingly
  switch(event->event_id) {
		
    case SYSTEM_EVENT_STA_START:
      esp_wifi_connect();
      break;

    case SYSTEM_EVENT_AP_START:
      ESP_LOGD(LOG_TAG,"Access point started");
      xEventGroupSetBits(connectionRoutingStatus, WIFI_ACTIVE);
      //start off timer (switch off wifi, if no client connects in timeout)
      wifiStartStopOffTimer(1);
      break;
		
    case SYSTEM_EVENT_AP_STOP:
      ESP_LOGD(LOG_TAG,"Access point stopped");
      xEventGroupClearBits(connectionRoutingStatus, WIFI_ACTIVE);
      break;
		
    case SYSTEM_EVENT_AP_STACONNECTED:
      //a new client connected
      ESP_LOGI(LOG_TAG,"Client connected, currently connected: %d",getNumberOfWifiStations());
      //set client connected flag
      xEventGroupSetBits(connectionRoutingStatus, WIFI_CLIENT_CONNECTED);
            
      //add the websocket sending functions to hal_serial for getting output data
      halSerialAddOutputStream(WS_write_data);
      
      //if the wifi timer is active, disable the timer (client is connected)
      wifiStartStopOffTimer(0);
      break;

    case SYSTEM_EVENT_AP_STADISCONNECTED:
      //a client got disconnected
      ESP_LOGI(LOG_TAG,"Client disconnected, currently connected: %d",getNumberOfWifiStations());
      if(getNumberOfWifiStations() == 0)
      {
        //remove the callback, not necessary if no client is connected
        halSerialRemoveOutputStream();
        //clear client connected flag
        xEventGroupClearBits(connectionRoutingStatus, WIFI_CLIENT_CONNECTED);
        //start the auto-disable timer
        wifiStartStopOffTimer(1);
      }
      break;		
    default:
      break;
  }  
	return ESP_OK;
}

/** @brief En- or Disable WiFi interface.
 * 
 * This method is used to enable or disable the wifi interface. In order to
 * provide a safe FLipMouse/FABI device, the Wifi hotspot is for configuration
 * purposes only. Therefore, an enabled Wifi hotspot will be automatically disabled 
 * within a defined time (see WIFI_OFF_TIME) after the last client was disconnected.
 * In addition, other parts of the software might disable the Wifi prior to
 * the automatic disconnect.
 * 
 * @note Calling this method prior to initializing wifi with taskWebGUIInit will
 * result in an error!
 * @return ESP_OK on success, ESP_FAIL otherwise
 * @param onoff If != 0, switch on WiFi, switch off if 0.
 * @see WIFI_OFF_TIME
 * */
esp_err_t taskWebGUIEnDisable(int onoff)
{
  esp_err_t ret;
  //should we enable or disable wifi?
  if(onoff == 0)
  {
    //clear wifi flags
    xEventGroupClearBits(connectionRoutingStatus, WIFI_ACTIVE | WIFI_CLIENT_CONNECTED);
    
    //stop tasks (free 16k RAM)
    wifiActive = 0;
    
    //disable, call wifi_stop
    ret = esp_wifi_stop();
    //check return value
    if(ret != ESP_OK)
    {
      ESP_LOGE(LOG_TAG,"Please initialize WiFi prior to enable/disable it!");
      return ESP_FAIL;
    } else return ESP_OK;
  } else {
    //start wifi
    ret = esp_wifi_start();
    switch(ret)
    {
      //everything ok
      case ESP_OK: break;
      //determine error
      case ESP_ERR_WIFI_CONN:
        ESP_LOGE(LOG_TAG,"Wifi internal error, control block invalid");
        return ESP_FAIL;
      case ESP_ERR_NO_MEM:
        ESP_LOGE(LOG_TAG,"Wifi internal error, out of memory");
        return ESP_FAIL;
      case ESP_ERR_WIFI_NOT_INIT:
        ESP_LOGE(LOG_TAG,"Please initialize WiFi prior to enable/disable it!");
        return ESP_FAIL;
      case ESP_FAIL:
      case ESP_ERR_INVALID_ARG:
      default:
        ESP_LOGE(LOG_TAG,"Unknown internal Wifi error");
        return ESP_FAIL;
    }
    
    // start the HTTP Server tasks
    wifiActive = 1;
    xTaskCreate(&http_server, "http_server", TASK_WEBGUI_SERVER_STACKSIZE, NULL, 5, &wifiHTTPServerHandle_t);
    xTaskCreate(&ws_server, "ws_server", TASK_WEBGUI_WEBSOCKET_STACKSIZE, NULL, 5, &wifiWSServerHandle_t);
    
    return ESP_OK;
  }
}

/** @brief Timer callback for disabling wifi.
 * 
 * This callback is executed, if the time expires after the last client
 * disconnects. It will disable wifi.
 * @param xTimer Timer handle which was used for calling this CB
 * */
void wifi_timer_cb(TimerHandle_t xTimer)
{
  if(taskWebGUIEnDisable(0) != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Disabling wifi automatically: error!");
  }

  //if wifi is automatically disabled, we need to enable BLE again
  //and set global status accordingly
  if(halBLEEnDisable(1) != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Error enabling BLE by wifi timer");
  }
}

/** @brief Init the web / DNS server and the web gui
 * 
 * This init function initializes the wifi, web server, dns server (captive portal)
 * and the websocket.
 * 
 * The web page is served from FAT file system (same partition as config
 * files) via the http_server task. Communication between web page and 
 * FLipMouse/FABI is done by a websocket, which is provided via the ws_server
 * task.
 * 
 * @todo Activate/fix/test captive portal
 * 
 * @note Wifi is not enabled by default (BLE has priority), see taskWebGUIEnDisable
 * @note IP Adress of the device is <b>192.168.10.1</b>
 * @see taskWebGUIEnDisable
 * @see CONFIG_AP_PASSWORD
 * @see NVS_WIFIPW
 * @return ESP_OK on success, ESP_FAIL otherwise
 * */
esp_err_t taskWebGUIInit(void)
{
  if(connectionRoutingStatus == NULL)
  {
    ESP_LOGE(LOG_TAG,"connection queue is uninitialized, please call the web init afterwards!");
    return ESP_FAIL;
  }
  halStorageNVSLoadString(NVS_WIFIPW,wifipw);
  if(strnlen(wifipw,32) < 8 || strnlen(wifipw,32) > 32)
  {
    ESP_LOGI(LOG_TAG,"Wifipassword invalid, using default one");
    strncpy(wifipw,CONFIG_AP_PASSWORD,32);
  }
  
  //ESP_LOGE(LOG_TAG,"Currently disabled");
  //return ESP_OK;
  
  /*++++ initialize WiFi (AP-mode) ++++*/
  esp_log_level_set("wifi", ESP_LOG_VERBOSE); // disable wifi driver logging
	tcpip_adapter_init();
  // stop DHCP server
	/*ESP_ERROR_CHECK(tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP));
  //assign new IP address to Wifi interface
  tcpip_adapter_ip_info_t info;
  memset(&info, 0, sizeof(info));
	IP4_ADDR(&info.ip, 192, 168, 10, 1);
  IP4_ADDR(&info.gw, 192, 168, 10, 1);
  IP4_ADDR(&info.netmask, 255, 255, 255, 0);
	ESP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &info));
  
  // start the DHCP server   
  ESP_ERROR_CHECK(tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP));
	*/
	// initialize the wifi event handler
	ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, NULL));
	
	// initialize the wifi stack in AccessPoint mode with config in RAM
	wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
  //wifi_init_config.event_handler = systemhandler;
	ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
	//ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

	// configure the wifi connection and start the interface
	wifi_config_t ap_config = {
    .ap = {
      .ssid = CONFIG_AP_SSID,
			.ssid_len = strnlen(CONFIG_AP_SSID,32),
			//.channel = CONFIG_AP_CHANNEL,
			.authmode = CONFIG_AP_AUTHMODE,
			.ssid_hidden = CONFIG_AP_SSID_HIDDEN,
			.max_connection = CONFIG_AP_MAX_CONNECTIONS,
			.beacon_interval = CONFIG_AP_BEACON_INTERVAL,			
    },
  };
  memcpy(ap_config.ap.password,wifipw,strnlen(wifipw,32)+1);
  ESP_LOGI(LOG_TAG,"Wifipw: %s",ap_config.ap.password);
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    
  /*++++ initialize capitive portal dns server ++++*/
	//captdnsInit();
  
  /*++++ initialize storage, having a definetly opened partition ++++*/
  uint32_t tid;
  if(halStorageStartTransaction(&tid,20,"webgui") != ESP_OK) {
    ESP_LOGE(LOG_TAG,"Cannot initialize storage");
  }
  halStorageFinishTransaction(tid);
  
  /*++++ initialize auto-off timer */
  //init timer with given number of minutes. No reload, no timer id.
  wifiTimer = xTimerCreate("wifi-autooff",(WIFI_OFF_TIME * 60000) / portTICK_PERIOD_MS, \
    pdFALSE,(void *) 0,wifi_timer_cb);
  if(wifiTimer == NULL)
  {
    ESP_LOGE(LOG_TAG,"Cannot start wifi disabling timer, no auto disable!");
  }
  
  return ESP_OK;
}

