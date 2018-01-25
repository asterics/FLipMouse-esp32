//
// FlipMouse Web Gui V0.1
// thanks to Luca Dentella for the esp32lights demo, see http://www.lucadentella.it/en/2018/01/08/esp32lights/
// thanks to Jeroen Domburg and Cornelis for the captive portal DNS code, see: https://github.com/cornelis-61/esp32_Captdns
//


#include <stdio.h>
#include <string.h>
#include <time.h>
#include <dirent.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_err.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "spiffs_vfs.h"

#include "driver/gpio.h"

#include "lwip/api.h"
#include "lwip/err.h"
#include "lwip/netdb.h"

#include "cJSON.h"
#include "captdns.h"


// working modes
#define MODE_BASIC		1
#define MODE_ACTION		2
#define MODE_PRESSURE	3

// relay status
#define RELAY_ON		0
#define RELAY_OFF		1

// static headers for HTTP responses
const static char http_html_hdr[] = "HTTP/1.1 200 OK\nContent-type: text/html\n\n";
const static char http_404_hdr[] = "HTTP/1.1 404 NOT FOUND\n\n";

// AP CONFIG values
#ifdef CONFIG_AP_HIDE_SSID
	#define CONFIG_AP_SSID_HIDDEN 1
#else
	#define CONFIG_AP_SSID_HIDDEN 0
#endif	
#ifdef CONFIG_WIFI_AUTH_OPEN
	#define CONFIG_AP_AUTHMODE WIFI_AUTH_OPEN
#endif
#ifdef CONFIG_WIFI_AUTH_WEP
	#define CONFIG_AP_AUTHMODE WIFI_AUTH_WEP
#endif
#ifdef CONFIG_WIFI_AUTH_WPA_PSK
	#define CONFIG_AP_AUTHMODE WIFI_AUTH_WPA_PSK
#endif
#ifdef CONFIG_WIFI_AUTH_WPA2_PSK
	#define CONFIG_AP_AUTHMODE WIFI_AUTH_WPA2_PSK
#endif
#ifdef CONFIG_WIFI_AUTH_WPA_WPA2_PSK
	#define CONFIG_AP_AUTHMODE WIFI_AUTH_WPA_WPA2_PSK
#endif
#ifdef CONFIG_WIFI_AUTH_WPA2_ENTERPRISE
	#define CONFIG_AP_AUTHMODE WIFI_AUTH_WPA2_ENTERPRISE
#endif

// Event group for inter-task communication
static EventGroupHandle_t event_group;
const int WIFI_CONNECTED_BIT = BIT0;

const int STA_CONNECTED_BIT = BIT1;
const int STA_DISCONNECTED_BIT = BIT2;



// actual relay status
bool relay_status;

// nvs handler
nvs_handle my_handle;

// running configuration
int working_mode;
char command[6]="";
char parameter[100]="";
int pressure=10;



// print the list of connected stations
void printStationList() 
{
	printf(" Connected stations:\n");
	printf("--------------------------------------------------\n");
	
	wifi_sta_list_t wifi_sta_list;
	tcpip_adapter_sta_list_t adapter_sta_list;
   
	memset(&wifi_sta_list, 0, sizeof(wifi_sta_list));
	memset(&adapter_sta_list, 0, sizeof(adapter_sta_list));
   
	ESP_ERROR_CHECK(esp_wifi_ap_get_sta_list(&wifi_sta_list));	
	ESP_ERROR_CHECK(tcpip_adapter_get_sta_list(&wifi_sta_list, &adapter_sta_list));
	
	for(int i = 0; i < adapter_sta_list.num; i++) {
		
		tcpip_adapter_sta_info_t station = adapter_sta_list.sta[i];
         printf("%d - mac: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x - IP: %s\n", i + 1,
				station.mac[0], station.mac[1], station.mac[2],
				station.mac[3], station.mac[4], station.mac[5],
				ip4addr_ntoa(&(station.ip)));
	}
	
	printf("\n");
}




// Wifi event handler
static esp_err_t event_handler(void *ctx, system_event_t *event) {
    switch(event->event_id) {
		
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    
	case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(event_group, WIFI_CONNECTED_BIT);
        break;
    
	case SYSTEM_EVENT_STA_DISCONNECTED:
		xEventGroupClearBits(event_group, WIFI_CONNECTED_BIT);
        break;
    		
    case SYSTEM_EVENT_AP_START:
		printf("Access point started\n");
		break;
		
	case SYSTEM_EVENT_AP_STACONNECTED:
		xEventGroupSetBits(event_group, STA_CONNECTED_BIT);
		printf("Station connected!\n");
		printStationList();
		break;

	case SYSTEM_EVENT_AP_STADISCONNECTED:
		xEventGroupSetBits(event_group, STA_DISCONNECTED_BIT);
		printf("Station disconnected!\n");
		printStationList();
		break;		
    
    
	default:
        break;
    }  
	return ESP_OK;
}

// pressure sensor read (dummy)
int get_pressure_value() {

	static int pressure = 20; 
	pressure+=20; if (pressure>1000) pressure=0;
	return pressure;
}

#define SPIFF_READ_BLOCKSIZE 1024

// serve static content from SPIFFS
void spiffs_serve(char* resource, struct netconn *conn) {
	
					
	// check if it exists on SPIFFS
	char full_path[100];
	sprintf(full_path, "/spiffs%s", resource);
	printf("+ Serving static resource: %s\n", full_path);
	struct stat st;
	if (stat(full_path, &st) == 0) {
		netconn_write(conn, http_html_hdr, sizeof(http_html_hdr) - 1, NETCONN_NOCOPY);
		
		// open the file for reading
		FILE* f = fopen(full_path, "r");
		if(f == NULL) {
			printf("Unable to open the file %s\n", full_path);
			return;
		}
		
		// send the file content to the client
		char buffer[SPIFF_READ_BLOCKSIZE];
		int i=0, len=0;

/*
		while(fgets(buffer, 1024, f)) {
			printf("Writing bytes %d\n", ++i * 1024);
			netconn_write(conn, buffer, strlen(buffer), NETCONN_NOCOPY);
		}
		*/
	    do {
			len=fread(buffer, 1, SPIFF_READ_BLOCKSIZE, f);
			i+=len;
			printf("Writing bytes %d\n", i);
			netconn_write(conn, buffer, len, NETCONN_NOCOPY);
		} while (len==SPIFF_READ_BLOCKSIZE);
		
		fclose(f);
		fflush(stdout);
	}
	else {
		printf("Resource not found: %s\n", full_path);
		netconn_write(conn, http_404_hdr, sizeof(http_404_hdr) - 1, NETCONN_NOCOPY);
	}
}
	
static void http_server_netconn_serve(struct netconn *conn) {

	struct netbuf *inbuf;
	char *buf;
	u16_t buflen;
	err_t err;

	err = netconn_recv(conn, &inbuf);

	if (err == ERR_OK) {
	  
		// get the request and terminate the string
		netbuf_data(inbuf, (void**)&buf, &buflen);
		buf[buflen] = '\0';

		printf("http_serv request: %s\n",buf);

		
		// get the request body and the first line
		char* body = strstr(buf, "\r\n\r\n");
		char *request_line = strtok(buf, "\n");
		
		if(request_line) {
				
			// dynamic page: setConfig
			if(strstr(request_line, "POST /setConfig")) {
			
				cJSON *root = cJSON_Parse(body);
				cJSON *mode_item = cJSON_GetObjectItemCaseSensitive(root, "mode");
				
				if(strstr(mode_item->valuestring, "relais")) {
					
					cJSON *status_item = cJSON_GetObjectItemCaseSensitive(root, "status");
					if(strstr(status_item->valuestring, "on")) {
						printf("! Turning the relay ON\n");
						gpio_set_level(CONFIG_RELAY_PIN, RELAY_ON);
						relay_status = true;	
					}
					else {
						printf("! Turning the relay OFF\n");
						gpio_set_level(CONFIG_RELAY_PIN, RELAY_OFF);
						relay_status = false;	
					}
				}
				else if(strstr(mode_item->valuestring, "basic")) {
					
					working_mode = MODE_BASIC;
					
					cJSON *status_item = cJSON_GetObjectItemCaseSensitive(root, "sensitivityX");
					if (status_item) printf("! Got SensitivityX:%s\n",status_item->valuestring);
					status_item = cJSON_GetObjectItemCaseSensitive(root, "sensitivityY");
					if (status_item) printf("! Got SensitivityY:%s\n",status_item->valuestring);
					status_item = cJSON_GetObjectItemCaseSensitive(root, "deadzoneX");
					if (status_item) printf("! Got DeadzoneX:%s\n",status_item->valuestring);
					status_item = cJSON_GetObjectItemCaseSensitive(root, "deadzoneY");
					if (status_item) printf("! Got DeadzoneY:%s\n",status_item->valuestring);
					
				}
				
				else if(strstr(mode_item->valuestring, "action")) {
				
					working_mode = MODE_ACTION;
					cJSON *status_item = cJSON_GetObjectItemCaseSensitive(root, "command");
					if (status_item) { 
						printf("! Got action command:%s\n",status_item->valuestring);
						strcpy (command, status_item->valuestring);
					}
					status_item = cJSON_GetObjectItemCaseSensitive(root, "parameter");
					if (status_item) {
						printf("! Got action parameter:%s\n",status_item->valuestring);
						strcpy (parameter, status_item->valuestring);
					}
				}
				
				else if(strstr(mode_item->valuestring, "pressure")) {
					
					working_mode = MODE_PRESSURE;
					
					cJSON *pressure_item = cJSON_GetObjectItemCaseSensitive(root, "pressureValue");
					pressure = atoi(pressure_item->valuestring);
				}
			}
			
			// dynamic page: getConfig
			else if(strstr(request_line, "GET /getConfig")) {
			
				cJSON *root = cJSON_CreateObject();
				if(working_mode == MODE_BASIC)	cJSON_AddStringToObject(root, "mode", "basic");
				else if(working_mode == MODE_ACTION) cJSON_AddStringToObject(root, "mode", "action");
				else if(working_mode == MODE_PRESSURE) cJSON_AddStringToObject(root, "mode", "pressure");
				
				if(relay_status == true) cJSON_AddStringToObject(root, "relais", "on");
				else cJSON_AddStringToObject(root, "relais", "off");
				
				cJSON_AddStringToObject(root, "command", command);
				cJSON_AddStringToObject(root, "parameter", parameter);
				cJSON_AddNumberToObject(root, "pressureValue", pressure);
				
				char *rendered = cJSON_Print(root);
				netconn_write(conn, http_html_hdr, sizeof(http_html_hdr) - 1, NETCONN_NOCOPY);
				netconn_write(conn, rendered, strlen(rendered), NETCONN_NOCOPY);
			}
			
			// dynamic page: getPressure
			else if(strstr(request_line, "GET /getPressure")) {
			
				int pressureValue = get_pressure_value();		
				cJSON *root = cJSON_CreateObject();
				cJSON_AddNumberToObject(root, "pressureValue", pressureValue);
				char *rendered = cJSON_Print(root);
				netconn_write(conn, http_html_hdr, sizeof(http_html_hdr) - 1, NETCONN_NOCOPY);
				netconn_write(conn, rendered, strlen(rendered), NETCONN_NOCOPY);
			}
			
			// default page -> redirect to index.html
			else if(strstr(request_line, "GET / ")) {
				spiffs_serve("/index.html", conn);
			}
			// static content, get it from SPIFFS
			else {
				
				// get the requested resource
				char* method = strtok(request_line, " ");
				char* resource = strtok(NULL, " ");
				spiffs_serve(resource, conn);
			}
		}
	}
	
	// close the connection and free the buffer
	netconn_close(conn);
	netbuf_delete(inbuf);
}

static void http_server(void *pvParameters) {
	
	struct netconn *conn, *newconn;
	err_t err;
	conn = netconn_new(NETCONN_TCP);
	netconn_bind(conn, NULL, 80);
	netconn_listen(conn);
	printf("* HTTP Server listening\n");
	do {
		err = netconn_accept(conn, &newconn);
		if (err == ERR_OK) {
			http_server_netconn_serve(newconn);
			netconn_delete(newconn);
		}
		vTaskDelay(10); //allows task to be pre-empted
	} while(err == ERR_OK);
	netconn_close(conn);
	netconn_delete(conn);
}

static void monitoring_task(void *pvParameters) {
	
	printf("* Monitoring task started\n");
	
	while(1) {
	
		// run every 1000ms
		vTaskDelay(1000 / portTICK_PERIOD_MS);

		time_t now = 0;
		time(&now);
		char actual_time[6];
		strftime(actual_time, 6, "%H:%M", localtime(&now));
		
		
		static int i=0;
		if (((i++) % 5)==0) {
			printf("Actual time (%s), working mode: %d \n", actual_time, working_mode);
			// printStationList();
		}
		
	}
}

// setup the NVS partition
void nvs_setup() {
	
	esp_err_t err = nvs_flash_init();
	if(err == ESP_ERR_NVS_NO_FREE_PAGES) {
		const esp_partition_t* nvs_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, NULL);
		if(!nvs_partition) {
			printf("error: unable to find a NVS partition\n");
			while(1) vTaskDelay(10 / portTICK_PERIOD_MS);
		}
		err = (esp_partition_erase_range(nvs_partition, 0, nvs_partition->size));
		if(err != ESP_OK) {
			printf("error: unable to erase the NVS partition\n");
			while(1) vTaskDelay(10 / portTICK_PERIOD_MS);
		}
		err = nvs_flash_init();
		if(err != ESP_OK) {		
			printf("error: unable to initialize the NVS partition\n");
			while(1) vTaskDelay(10 / portTICK_PERIOD_MS);
		}
	}
	
	printf("* NVS configured\n");
}

// setup and start the wifi connection
void wifi_setup() {
		
	tcpip_adapter_init();

	/*
     // setup Wifi in Station Mode
	ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

	wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

	wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD,
        },
    };
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
	
	printf("* Wifi configured and started\n");
	
	// wait for connection to the wifi network
	xEventGroupWaitBits(event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
	printf("* Connected to the wifi network\n");

	*/

    // setup Wifi in Soft AP Mode

	// stop DHCP server
	ESP_ERROR_CHECK(tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP));
	
	// assign a static IP to the network interface
	tcpip_adapter_ip_info_t info;
    memset(&info, 0, sizeof(info));
	IP4_ADDR(&info.ip, 192, 168, 10, 1);
    IP4_ADDR(&info.gw, 192, 168, 10, 1);
    IP4_ADDR(&info.netmask, 255, 255, 255, 0);
	ESP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &info));
	
	// start the DHCP server   
    ESP_ERROR_CHECK(tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP));
	
	// initialize the wifi event handler
	ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
	
	// initialize the wifi stack in AccessPoint mode with config in RAM
	wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

	// configure the wifi connection and start the interface
	wifi_config_t ap_config = {
        .ap = {
            .ssid = CONFIG_AP_SSID,
            .password = CONFIG_AP_PASSWORD,
			.ssid_len = 0,
			.channel = CONFIG_AP_CHANNEL,
			.authmode = CONFIG_AP_AUTHMODE,
			.ssid_hidden = CONFIG_AP_SSID_HIDDEN,
			.max_connection = CONFIG_AP_MAX_CONNECTIONS,
			.beacon_interval = CONFIG_AP_BEACON_INTERVAL,			
        },
    };
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    
	// start the wifi interface
	ESP_ERROR_CHECK(esp_wifi_start());
	printf("Starting access point, SSID=%s\n", CONFIG_AP_SSID);
	
}

// configure the output PIN
void gpio_setup() {
	
	// configure the relay pin as GPIO, output
	gpio_pad_select_gpio(CONFIG_RELAY_PIN);
    gpio_set_direction(CONFIG_RELAY_PIN, GPIO_MODE_OUTPUT);
	
	// set initial status = OFF
	gpio_set_level(CONFIG_RELAY_PIN, RELAY_OFF);
	relay_status = false;
	
	printf("* Relay PIN configured\n");
}



// Main application
void app_main()
{
	// log only errors
	esp_log_level_set("*", ESP_LOG_VERBOSE);
	event_group = xEventGroupCreate();
	
	printf("FlipMouse WebGUI v0.1\n\n");

	// initialize the different modules and components
	vfs_spiffs_register();
	gpio_setup();
	nvs_setup();
	// open the partition in R/W mode
	esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
	if (err != ESP_OK) {
		printf("Error: unable to open the NVS partition\n");
		while(1) vTaskDelay(10 / portTICK_PERIOD_MS);
	}
	
	
	wifi_setup();
	// captdnsInit();
	
	
	// print the local IP address
	tcpip_adapter_ip_info_t ip_info;
	//	ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info));
	ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ip_info));
	printf("\n- NETWORK PARAMETERS -------------------------\n");
	printf(" IP Address:\t%s\n", ip4addr_ntoa(&ip_info.ip));
	printf(" Subnet mask:\t%s\n", ip4addr_ntoa(&ip_info.netmask));
	printf(" Gateway:\t%s\n", ip4addr_ntoa(&ip_info.gw));	
	printf("----------------------------------------------\n\n");
	
	// start the HTTP Server task
    xTaskCreate(&http_server, "http_server", 20000, NULL, 5, NULL);
	
	// start the monitoring task
	xTaskCreate(&monitoring_task, "monitoring_task", 2048, NULL, 5, NULL);
	
	printf("\n");
}
