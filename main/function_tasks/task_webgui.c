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
/** @brief Log level for the web/websocket server */
#define LOG_LEVEL_WEB ESP_LOG_DEBUG

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
const static char *base_path = "/spiffs";
/** @brief Currently used wifi password */
static char wifipw[64];

/** @brief Static HTTP HTML header */
const static char http_html_hdr[] = "HTTP/1.1 200 OK\r\n";
/** @brief Static HTTP redirect header */
const static char http_redir_hdr[] = "HTTP/1.1 302 Found\r\nLocation: http://192.168.4.1/index.htm\r\n\Expires: Mon, 26 Jul 1997 05:00:00 GMT\r\nCache-Control: no-cache, no-store, must-revalidate\r\nPragma: no-cache\r\nCache-Control: post-check=0, pre-check=0\r\nContent-Length: 0\r\n";

/** @brief Mutex to lock FS access (only one file after another will be sent) */
SemaphoreHandle_t  fsSem;

/** @brief Webserver socket - general bind
 * @note This is global to close the socket after the http task is deleted */
int http_sockfd;

/** @brief Webserver socket - new connection
 * @note This is global to close the socket after the http task is deleted */
int http_new_sockfd;

/** @brief Websocket conn reference
 * @note Global reference to close after task is deleted */
struct netconn *ws_conn;

/** @brief AP mode config, filled in init */
/*wifi_config_t ap_config= {
  .ap = {
    //.channel = CONFIG_AP_CHANNEL,
    .authmode = CONFIG_AP_AUTHMODE,
    .ssid_hidden = CONFIG_AP_SSID_HIDDEN,
    .max_connection = CONFIG_AP_MAX_CONNECTIONS,
    .beacon_interval = CONFIG_AP_BEACON_INTERVAL,			
  },
};*/

/** @brief En- or Disable WiFi interface. (non-public method)
 * 
 * This method is used to call wifi related functions for
 * en- / disabling. These cannot be called from ISR context and
 * should be used synchronized some way.
 * Here, we will use taskWebGUIEnDisable to set corresponding flags.
 * 
 * The http_server task will check these flags and control the
 * wifi corresponding to these flags.
 * 
 * @return ESP_OK on success, ESP_FAIL otherwise
 * @param onoff If != 0, switch on WiFi, switch off if 0.
 * @see WIFI_OFF_TIME
 * @see taskWebGUIEnDisable
 * @see http_server
 * @see WIFI_TO_DEACTIVATE
 * @see WIFI_TO_ACTIVATE
 * */
esp_err_t wifiEnDisable(int onoff);

/** @brief Timer handle for auto-disabling wifi */
TimerHandle_t wifiTimer;

/** @brief Task handle for websocket task */
TaskHandle_t wifiWSServerHandle_t = NULL;
/** @brief Task handle for http server task */
TaskHandle_t wifiHTTPServerHandle_t = NULL;

/** @brief Get the number of currently connected Wifi stations
 * @return Number of connected clients */
static int8_t getNumberOfWifiStations(void)
{
 	wifi_sta_list_t wifi_sta_list;
	tcpip_adapter_sta_list_t adapter_sta_list;
   
	memset(&wifi_sta_list, 0, sizeof(wifi_sta_list));
	memset(&adapter_sta_list, 0, sizeof(adapter_sta_list));
   
  if(esp_wifi_ap_get_sta_list(&wifi_sta_list) != ESP_OK) return 0;
  if(tcpip_adapter_get_sta_list(&wifi_sta_list, &adapter_sta_list) != ESP_OK) return 0;
  
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
	//wait until wifi is active
	//we will wait until the flag for WIFI enable request is set.
	while(((xEventGroupWaitBits(connectionRoutingStatus,WIFI_ACTIVE,pdTRUE, \
		pdTRUE, portMAX_DELAY)) & WIFI_ACTIVE) == 0);
	
	//set up new TCP listener
	struct netconn *ws_newconn;
	ws_conn = netconn_new(NETCONN_TCP);
	if(netconn_bind(ws_conn, NULL, TASK_WEBGUI_WSPORT) != ERR_OK)
	{
		ESP_LOGE(LOG_TAG,"WS bind failed");
	} else ESP_LOGI(LOG_TAG,"WS bind");
	if(netconn_listen(ws_conn) != ERR_OK)
	{
		ESP_LOGE(LOG_TAG,"WS listen failed");
	} else ESP_LOGI(LOG_TAG,"WS listen");
    ESP_LOGI(LOG_TAG,"Websocket server started");
    
	//wait for connections
	//and check if wifi is active or is to be activated
	while(1)
	{
		err_t ret = netconn_accept(ws_conn, &ws_newconn);
		if(ret != ERR_OK)
		{
			ESP_LOGE(LOG_TAG,"Error accept: %d",ret);
			vTaskDelay(10);
			continue;
		}
		ESP_LOGI(LOG_TAG,"Incoming WS connection");
		//add the websocket sending functions to hal_serial for getting output data
		halSerialAddOutputStream(WS_write_data);
		ws_server_netconn_serve(ws_newconn);
	}
}

/** @brief Helper, sending redirect header */
void redirect(char* resource, int fd)
{
  send(fd, http_redir_hdr, sizeof(http_redir_hdr) - 1, 0);
  ESP_LOGI(LOG_TAG,"Sending redirect for %s",resource);
}

/** @brief Timer callback for disabling wifi.
 * 
 * This callback is executed, if the time expires after the last client
 * disconnects. It will disable wifi.
 * @param xTimer Timer handle which was used for calling this CB
 * */
void wifi_timer_cb(TimerHandle_t xTimer)
{
  if(taskWebGUIEnDisable(0,false) != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Disabling wifi automatically: error!");
  } else {
    uint8_t slotnr = halStorageGetCurrentSlotNumber();
	if(slotnr == 0) slotnr++;
	LED((slotnr%2)*0xFF,((slotnr/2)%2)*0xFF,((slotnr/4)%2)*0xFF,0);
	ESP_LOGI(LOG_TAG,"Disabling wifi - no clients connected");
  }
}

/** @brief Serve static content from FAT
 * 
 * This method is used to send data from the FAT partition to a client
 * @param resource Name of the resource to be sent
 * @param conn Currently active connection
 */
void fat_serve(char* resource, int fd) {
  //if we serve a html file for a different URL, Content Type
  //is not set correctly (no file ending given).
  //Set this value to != 0 to force ContentType = text/html
  uint8_t force_html = 0;
  
  //testing for special URLS, which should be handled by
  //redirects or other captive portal logic
  //URL source: https://stackoverflow.com/questions/46289283/esp8266-captive-portal-with-pop-up
  
  if(strcmp(resource,"/fwlink") == 0) { redirect(resource, fd); return; }
  if(strcmp(resource,"/connecttest.txt") == 0) { redirect(resource, fd); return; }
  if(strcmp(resource,"/hotspot-detect.html") == 0) { redirect(resource, fd); return; }
  if(strcmp(resource,"/library/test/success.html") == 0) { redirect(resource, fd); return; }
  if(strcmp(resource,"/kindle-wifi/wifistub.html") == 0) { redirect(resource, fd); return; }
  
  //do NOT redirect for Android, instead serve root file.
  //if(strcmp(resource,"/generate_204") == 0) { redirect(resource, fd); return; }
  //if(strcmp(resource,"/gen_204") == 0) { redirect(resource, fd); return; }
  
  //if we have a very long filename, rewrite for index.htm...
  //we had a DoubleExceptionVector for long names (not supported by FAT)
  if(strlen(resource) > 32) sprintf(resource,"/index.htm");
  
  if(xSemaphoreTake(fsSem,200) == pdTRUE)
  {
    //basepath + 8.3 file + folder + margin
    char file[sizeof(base_path)+32];
    
    //Captive portal:
    //for all devices which do NOT want a redirect, serve index file
    if(strcmp(resource,"/generate_204") == 0) {
      sprintf(file,"%s%s",base_path,"/index.htm");
      force_html = 1; //force content type
    } else if(strcmp(resource,"/gen_204") == 0) {
      sprintf(file,"%s%s",base_path,"/index.htm");
      force_html = 1; //force content type
    } else {
      sprintf(file,"%s%s",base_path,resource);
    }
    ESP_LOGI(LOG_TAG,"serving from FAT: %s",file);
		
		// open the file for reading
		FILE* f = fopen(file, "r");
		if(f == NULL) {
      ESP_LOGW(LOG_TAG,"Resource not found: %s, opening index.htm", file);
      sprintf(file,"%s%s",base_path,"/index.htm");
      f = fopen(file, "r");
      force_html = 1; //force content type
      if(f == NULL)
      {
        ESP_LOGE(LOG_TAG,"Index not found? Sending redirect...");
        send(fd, http_redir_hdr, sizeof(http_redir_hdr) - 1, 0);
        xSemaphoreGive(fsSem);
        return;
      }
		}
    
    //get the size of this file (for html header)
    fseek(f, 0L, SEEK_END);
    int sz = ftell(f);
    rewind(f);
    
    //send http header (200)
    send(fd, http_html_hdr, sizeof(http_html_hdr) - 1,MSG_MORE);
    
    //send http header (content type)
    int reslen = strlen(resource);
    char hdr[36];
    if(strcmp(&resource[reslen-4],".css") == 0)
    {
      snprintf(hdr,36,"Content-Type: text/css\r\n");
    } else if(strcmp(&resource[reslen-4],".htm") == 0 || force_html != 0) {
      snprintf(hdr,36,"Content-Type: text/html\r\n");
    } else if(strcmp(&resource[reslen-3],".js") == 0) {
      snprintf(hdr,36,"Content-Type: text/javascript\r\n");
    } else {
      snprintf(hdr,36,"Content-Type: text/plain\r\n");
    }
    send(fd, hdr, strlen(hdr),MSG_MORE);
    #if (LOG_LEVEL_WEB >= ESP_LOG_DEBUG)
    ESP_LOGD(LOG_TAG,"Hdr: %s",hdr);
    #endif
    
    //send http header (content length)
    snprintf(hdr,36,"Content-Length: %d\r\n\r\n",sz);
    send(fd, hdr, strlen(hdr),MSG_MORE);
    #if (LOG_LEVEL_WEB >= ESP_LOG_DEBUG)
    ESP_LOGD(LOG_TAG,"Hdr: %s",hdr);
    #endif
    
		//allocate a buffer in size of one FAT block
		char* buffer = malloc(512);
		int i=0, len=0;
    do {
			len=fread(buffer, 1, 512, f);
			i+=len;
      send(fd, buffer, len, 0);
      vTaskDelay(1);
		} while (len==512);
		
    #if (LOG_LEVEL_WEB >= ESP_LOG_DEBUG)
    ESP_LOGD(LOG_TAG,"Sent %u bytes",i);
    #endif
		fclose(f);
    free(buffer);
    xSemaphoreGive(fsSem);
  } else {
    ESP_LOGE(LOG_TAG,"Timeout waiting for fat mutex!");
  }
}

/** @brief Handle an incoming HTTP connection
 * 
 * This handler is used to server the webpage to a connected client.
 * Data is read from the FAT/VFS partition of the flash.
 * The partition is created and uploaded by *make makefatfs flashfatfs*
 * @param conn Current active net connection
 * */
static void http_server_netconn_serve(int fd) {
  if(fd < 0) return;
  #define MAX_BUFF_SIZE 512
  
  char *buf = malloc(MAX_BUFF_SIZE);
  if(buf == NULL)
  {
    ESP_LOGE(LOG_TAG,"Cannot allocate buf for recv");
    return;
  }

  //read incoming
  int size = recv(fd,buf,MAX_BUFF_SIZE-1,0);
  #if (LOG_LEVEL_WEB >= ESP_LOG_DEBUG)
  ESP_LOGD(LOG_TAG,"recv size: %d",size);
  #endif
  if(size > 0)
  {
    buf[size] = '\0';
    char *request_line = strtok(buf, "\n");
		
		if(request_line) {
			// default page -> redirect to index.html
			if(strstr(request_line, "GET / ")) {
				fat_serve((char*)"/index.htm", fd);
			} else {
				// get the requested resource
        char* method = strtok(request_line, " ");
        //get rid of "unused" variable.
        //strtok although needs this call.
        (void)(method);
				char* resource = strtok(NULL, " ");
				fat_serve(resource, fd);
			}
		}
  }
  free(buf);
}



/** @brief Main webserver task
 * 
 * This task is used to handle incoming connections via netconn_accept.
 * Each time a connection is opened, it will be served by
 * http_server_netconn_serve.
 * 
 * @see http_server_netconn_serve
 * @param pvParameters Unused.
 * */
static void http_server(void *pvParameters) {
  socklen_t addr_len;
  struct sockaddr_in sock_addr;
	int ret;
  //we will wait until the flag for WIFI enable request is set.
  while(((xEventGroupWaitBits(connectionRoutingStatus,WIFI_TO_ACTIVATE,pdTRUE, \
	pdTRUE, portMAX_DELAY)) & WIFI_TO_ACTIVATE) == 0);
  //lets start the Wifi stack
  wifiEnDisable(1);
	
  //create a new TCP connection
  http_sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (http_sockfd < 0) {
    ESP_LOGE(LOG_TAG,"Cannot create socket");
  }
  //bind it to incoming port 80 (HTTP)
	memset(&sock_addr, 0, sizeof(sock_addr));
  sock_addr.sin_family = AF_INET;
  sock_addr.sin_addr.s_addr = 0;
  sock_addr.sin_port = htons(80);
  ret = bind(http_sockfd, (struct sockaddr*)&sock_addr, sizeof(sock_addr));
  if(ret) {
    ESP_LOGE(LOG_TAG,"Failed to bind: %d",ret);
    close(http_sockfd);
    http_sockfd = -1;
    vTaskDelete(NULL);
  }
  ret = listen(http_sockfd, 1);
    if(ret) {
    ESP_LOGE(LOG_TAG,"Failed to listen: %d",ret);
    close(http_sockfd);
    http_sockfd = -1;
    vTaskDelete(NULL);
  }
	ESP_LOGI(LOG_TAG,"http_server task started");
  
	do {
    //accept connection & save netconn handle
		http_new_sockfd = accept(http_sockfd, (struct sockaddr *)&sock_addr, &addr_len);
	    if (http_new_sockfd < 0) {
	        ESP_LOGE(LOG_TAG, "Failed to accept: %d",http_new_sockfd);
	    } else {
	      //serve
	      http_server_netconn_serve(http_new_sockfd);
	      //close
	      close(http_new_sockfd);
	    }
		vTaskDelay(1); //allows task to be pre-empted
	} while(1);
  //if we run into an error or this task is going to be removed
	close(http_new_sockfd);
	close(http_sockfd);
  //delete task
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
      ESP_LOGE(LOG_TAG,"Shit!");
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
 * @param fromISR Set to != 0 if this function is called from ISR context. Otherwise a reset will happen.
 * @see WIFI_OFF_TIME
 * */
esp_err_t taskWebGUIEnDisable(int onoff, bool fromISR)
{
	//check first if wifi was already used...
	EventBits_t status;
	if(fromISR) status = xEventGroupGetBits(connectionRoutingStatus);
	else status = xEventGroupGetBitsFromISR(connectionRoutingStatus);
	if((status & WIFI_LOCKED) != 0)
	{
		ESP_LOGW(LOG_TAG,"Wifi can be enabled/disabled only once each powercycle!");
		return ESP_FAIL;
	}
	
	BaseType_t xHigherPriorityTaskWoken, xResult;
	if(onoff != 0)
	{
		if(fromISR)
		{
			xHigherPriorityTaskWoken = pdFALSE;
			xResult = xEventGroupSetBitsFromISR(connectionRoutingStatus, \
				WIFI_TO_ACTIVATE, &xHigherPriorityTaskWoken);
			/* Was the message posted successfully? */
			if(xResult != pdFAIL && xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
		} else {
			//en- /disable wifi in non-ISR mode
			xEventGroupSetBits(connectionRoutingStatus,WIFI_TO_ACTIVATE);
		}
	} else {
		//disable wifi by killing all that stuff...
		wifiEnDisable(0);
	}
	return ESP_OK;
}

/** @brief En- or Disable WiFi interface. (non-public method)
 * 
 * This method is used to call wifi related functions for
 * en- / disabling. These cannot be called from ISR context and
 * should be used synchronized some way.
 * Here, we will use taskWebGUIEnDisable to set corresponding flags.
 * 
 * The http_server task will check these flags and control the
 * wifi corresponding to these flags.
 * 
 * @return ESP_OK on success, ESP_FAIL otherwise
 * @warning Not ISR safe, this should be called only with care (HTTP/WS/DNS tasks are killed)
 * @param onoff If != 0, switch on WiFi, switch off if 0.
 * @see WIFI_OFF_TIME
 * @see taskWebGUIEnDisable
 * @see http_server
 * @see WIFI_TO_DEACTIVATE
 * @see WIFI_TO_ACTIVATE
 * */
esp_err_t wifiEnDisable(int onoff)
{
  esp_err_t ret;
  //should we enable or disable wifi?
  if(onoff == 0)
  {
    //clear wifi flags
    xEventGroupClearBits(connectionRoutingStatus, WIFI_ACTIVE | WIFI_CLIENT_CONNECTED);
    
    //killall tasks
    captdnsDeinit();
    if(wifiHTTPServerHandle_t != NULL) vTaskDelete(wifiHTTPServerHandle_t);
    if(wifiWSServerHandle_t != NULL) vTaskDelete(wifiWSServerHandle_t);
    //close remaining sockets
    close(http_new_sockfd);
    close(http_sockfd);
    netconn_close(ws_conn);
    
    //signal an already used wifi
    xEventGroupSetBits(connectionRoutingStatus,WIFI_LOCKED);

    //disable, call wifi_stop
    if(esp_wifi_stop() == ESP_OK)
    {
      ret = esp_wifi_deinit();
    } else {
      ret = ESP_OK;
    }
    //check return value
    if(ret != ESP_OK)
    {
      ESP_LOGE(LOG_TAG,"Please initialize WiFi prior to disable it!");
      return ESP_FAIL;
    } else {
		return ESP_OK;
	}
  } else {
	//based on softAP example of Espressif.
	tcpip_adapter_init();
	//if event loop init fails, we might have an already initialized event loop
	//in this case, just add the handler of this file.
    if(esp_event_loop_init(wifi_event_handler, NULL) != ESP_OK)
    {
		esp_event_loop_set_cb(wifi_event_handler,NULL);
	}
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = CONFIG_AP_SSID,
            .ssid_len = strlen(CONFIG_AP_SSID),
            .password = CONFIG_AP_PASSWORD,
            .max_connection = CONFIG_AP_MAX_CONNECTIONS,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config);
    ret = esp_wifi_start();
	  
	ESP_LOGI(LOG_TAG,"AP - PW: %s (%d)",wifi_config.ap.password,strnlen(wifipw,32));
	ESP_LOGI(LOG_TAG,"AP - SSID: %s (%d)",wifi_config.ap.ssid, wifi_config.ap.ssid_len);

	//check esp_wifi_start's return value
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
    
    //wait 200ms for WiFi stack to settle
    vTaskDelay(200/portTICK_PERIOD_MS);
    
    /*++++ initialize capitive portal dns server ++++*/
	captdnsInit();
    
    /*++++ start Websocket task ++++*/
	xTaskCreate(&ws_server, "ws_server", TASK_WEBGUI_WEBSOCKET_STACKSIZE, NULL, 5, &wifiWSServerHandle_t);
    
    return ESP_OK;
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
  //set log level to given log level
  esp_log_level_set(LOG_TAG,LOG_LEVEL_WEB);
  
  if(connectionRoutingStatus == NULL)
  {
    ESP_LOGE(LOG_TAG,"connection flags are uninitialized!");
    return ESP_FAIL;
  }
  halStorageNVSLoadString(NVS_WIFIPW,wifipw);
  if(strnlen(wifipw,32) < 8 || strnlen(wifipw,32) > 32)
  {
    ESP_LOGI(LOG_TAG,"Wifipassword invalid, using default one");
    strncpy(wifipw,CONFIG_AP_PASSWORD,32);
  }
  
  esp_log_level_set("wifi", ESP_LOG_VERBOSE); // disable wifi driver logging
  
  
  /*++++ initialize storage, having an opened partition ++++*/
  uint32_t tid;
  if(halStorageStartTransaction(&tid,20,"webgui") != ESP_OK) {
    ESP_LOGE(LOG_TAG,"Cannot initialize storage");
  }
  halStorageFinishTransaction(tid);

  /*++++ init mutex for FS access */
  fsSem = xSemaphoreCreateMutex();
  	  
  /*++++ initialize auto-off timer */
  //init timer with given number of minutes. No reload, no timer id.
  wifiTimer = xTimerCreate("wifi-autooff",(WIFI_OFF_TIME * 60000) / portTICK_PERIOD_MS, \
      pdFALSE,(void *) 0,wifi_timer_cb);
  if(wifiTimer == NULL)
  {
    ESP_LOGE(LOG_TAG,"Cannot start wifi disabling timer, no auto disable!");
  }
  
  /*++++ start HTTP task ++++*/
  xTaskCreate(&http_server, "http_server", TASK_WEBGUI_SERVER_STACKSIZE, NULL, 5, &wifiHTTPServerHandle_t);
  
  return ESP_OK;
}

