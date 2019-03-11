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
 * Copyright 2018, Benjamin Aigner <beni@asterics-foundation.org>,
 * <aignerb@technikum-wien.at>
 */
 
/** @file
 * 
 * @brief This is the main entry point of the ESP32 FLipMouse/FABI firmware.
 * 
 * In this file, the app_main function is called by the esp-idf.
 * This function initializes all necessary RTOS services & objects,
 * starts necessary tasks and exits afterwards.
 * 
 * @see app_main
 * 
 * */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_heap_trace.h"

#include "function_tasks/task_debouncer.h"
#include "function_tasks/task_commands.h"
#include "function_tasks/task_webgui.h"
#include "hal/hal_adc.h"
#include "hal/hal_io.h"
#include "ble_hid/hal_ble.h"
#include "hal/hal_serial.h"
#include "config_switcher.h"
#include "function_tasks/handler_hid.h"
#include "function_tasks/handler_vb.h"

#include "config.h"

#define LOG_TAG "app_main"

/** @brief Defining a new event base for VB actions */
ESP_EVENT_DEFINE_BASE(VB_EVENT);
EventGroupHandle_t connectionRoutingStatus;
EventGroupHandle_t systemStatus;
SemaphoreHandle_t switchRadioSem;
QueueHandle_t config_switcher;
QueueHandle_t debouncer_in;
QueueHandle_t hid_usb;
QueueHandle_t hid_ble;
uint8_t isWifiOn = 0;

/** @brief Switch radio mode
 * 
 * This method is used to switch between the radio modes (radio_status_t).
 * It will be called via the IO long press handler.
 * Modes are currently switched in following order (wrap around):
 * * BLE
 * * BLE_PAIRING
 * * WIFI
 * 
 * Actual switching is done in app_main, mostly because this
 * method is executed in timer task context, with limited stack.
 * 
 * @see halIOAddLongPressHandler
 * @see HAL_IO_LONGACTION_TIMEOUT
 */
void switch_radio(void)
{
    if(isWifiOn == 0)
    {
        isWifiOn = 1;
        LED(255,0,255,0);
        taskWebGUIEnDisable(1);
    } else {
        uint8_t slotnr = halStorageGetCurrentSlotNumber();
        if(slotnr == 0) slotnr++;
        LED((slotnr%2)*0xFF,((slotnr/2)%2)*0xFF,((slotnr/4)%2)*0xFF,0);
        isWifiOn = 0;
        taskWebGUIEnDisable(0);
    }
}


#if 0
/** @brief Number of records for heap tracing
 * @note This is not used by default. */
#define NUM_RECORDS 300
/** @brief Heap tracing buffer
 * @note This is not used by default.*/
static heap_trace_record_t trace_record[NUM_RECORDS];
#endif

/** @brief Main task, created by esp-idf
 * 
 * This task is used to initialize all queues & flags.
 * In addition, all necessary tasks are created
 * After initialisation, this task deletes itself.
 * */    
void app_main()
{
    //set log level to info
    //esp_log_level_set("*",ESP_LOG_INFO);
    
    //enter critical section & suspend all tasks for initialising
    vTaskSuspendAll();
        //init all remaining rtos stuff
        //eventgroups
        connectionRoutingStatus = xEventGroupCreate();
        systemStatus = xEventGroupCreate();
        xEventGroupSetBits(systemStatus, SYSTEM_STABLECONFIG | SYSTEM_EMPTY_CMD_QUEUE);
        //queues
        config_switcher = xQueueCreate(5,sizeof(char)*SLOTNAME_LENGTH);
        hid_ble = xQueueCreate(32,sizeof(hid_cmd_t));
        hid_usb = xQueueCreate(32,sizeof(hid_cmd_t));
        debouncer_in = xQueueCreate(32,sizeof(raw_action_t));
        //semphores
        switchRadioSem = xSemaphoreCreateBinary();
        
    //exit critical section & resume all tasks for initialising
    xTaskResumeAll();
    
    //start IO continous task
    if(halIOInit() == ESP_OK)
    {
        ESP_LOGD(LOG_TAG,"initialized halIOInit");
        //set LED to first slot directly on startup 
        LED(0xFF,0,0,0);
    } else {
        ESP_LOGE(LOG_TAG,"error initializing halIOInit");
    }
        
    //initialize serial communication task
    //(USB-HID & USB Serial for commands)
    if(halSerialInit() == ESP_OK)
    {
        ESP_LOGD(LOG_TAG,"initialized halSerial");
    } else {
        ESP_LOGE(LOG_TAG,"error initializing halSerial");
    }
        
    //start adc continous task
    if(halAdcInit(NULL) == ESP_OK)
    {
        ESP_LOGD(LOG_TAG,"initialized halAdcInit");
    } else {
        ESP_LOGE(LOG_TAG,"error initializing halAdcInit");
    }
    //we need to calibrate here, otherwise sip/puff is not 512 in idle...
    halAdcCalibrate();
    
    esp_event_loop_create_default();

    //init HID handler
    if(handler_hid_init() == ESP_OK)
    {
        ESP_LOGD(LOG_TAG,"HID handler initialized");
    } else {
        ESP_LOGE(LOG_TAG,"error adding HID handler");
    }

    //init VB handler
    if(handler_vb_init() == ESP_OK)
    {
        ESP_LOGD(LOG_TAG,"VB handler initialized");
    } else {
        ESP_LOGE(LOG_TAG,"error adding VB handler");
    }

    //start BLE (mouse/keyboard interfaces active)
    if(halBLEInit(1,1,0) == ESP_OK)
    {
        ESP_LOGD(LOG_TAG,"initialized halBle");
    } else {
        ESP_LOGE(LOG_TAG,"error initializing halBle");
    }

    //command parser
    if(taskCommandsInit() == ESP_OK)
    {
        ESP_LOGD(LOG_TAG,"initialized taskCommands");
    } else {
        ESP_LOGE(LOG_TAG,"error initializing taskCommands");
    }
    
    //start debouncer
    if(xTaskCreate(task_debouncer,"debouncer",TASK_DEBOUNCER_STACKSIZE, 
        (void*)NULL,DEBOUNCER_TASK_PRIORITY, NULL) == pdPASS)
    {
        ESP_LOGD(LOG_TAG,"created new debouncer task");
    } else {
        ESP_LOGE(LOG_TAG,"error creating new debouncer task");
    }

    
    //initialize web framework
    if(taskWebGUIInit() == ESP_OK)
    {
        ESP_LOGD(LOG_TAG,"initialized webserver/DNS server/webgui");
    } else {
        ESP_LOGE(LOG_TAG,"error initializing webserver/DNS server/webgui");
    }
    //start wifi
    halIOAddLongPressHandler(switch_radio);
    
    //calibrate directly after start-up
    //vTaskDelay(50/portTICK_PERIOD_MS);
    
    
    //start config switcher
    if(configSwitcherInit() == ESP_OK)
    {
        ESP_LOGD(LOG_TAG,"initialized configSwitcherInit");
    } else {
        ESP_LOGE(LOG_TAG,"error initializing configSwitcherInit");
    }
    
    //delete this task after initializing.
    ESP_LOGI(LOG_TAG,"Finished initializing!");
    
    //TESTING
    #if 0
    ESP_ERROR_CHECK( heap_trace_init_standalone(trace_record, NUM_RECORDS) );
    vTaskDelay(100); //1s
    ESP_ERROR_CHECK( heap_trace_start(HEAP_TRACE_ALL) );
    raw_action_t action;
    action.vb = 1;
    action.type = VB_RELEASE_EVENT;
    for(uint8_t i = 0; i<10; i++)
    {
        xQueueSendToBack(debouncer_in,&action,0);
        vTaskDelay(10);
    }
    vTaskDelay(600); //4s
    ESP_ERROR_CHECK( heap_trace_stop() );
    heap_trace_dump();
    #endif
    
    
    vTaskDelete(NULL);
}

