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

#include "function_tasks/task_debouncer.h"
#include "function_tasks/task_commands.h"
#include "function_tasks/task_webgui.h"
#include "hal/hal_adc.h"
#include "hal/hal_io.h"
#include "ble_hid/hal_ble.h"
#include "hal/hal_serial.h"
#include "config_switcher.h"

#include "config.h"

#define LOG_TAG "app_main"

EventGroupHandle_t virtualButtonsOut[NUMBER_VIRTUALBUTTONS];
EventGroupHandle_t virtualButtonsIn[NUMBER_VIRTUALBUTTONS];
EventGroupHandle_t connectionRoutingStatus;
EventGroupHandle_t systemStatus;
SemaphoreHandle_t switchRadioSem;
QueueHandle_t config_switcher;
QueueHandle_t hid_usb;
QueueHandle_t hid_ble;

radio_status_t radio = UNINITIALIZED;

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
    xSemaphoreGive(switchRadioSem);
}

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
        //init flags if not created
        for(uint8_t i = 0; i<NUMBER_VIRTUALBUTTONS; i++)
        {
            virtualButtonsIn[i] =  xEventGroupCreate();
            virtualButtonsOut[i] = xEventGroupCreate();
        }
        //queues
        config_switcher = xQueueCreate(5,sizeof(char)*SLOTNAME_LENGTH);
        hid_ble = xQueueCreate(32,sizeof(hid_cmd_t));
        hid_usb = xQueueCreate(32,sizeof(hid_cmd_t));
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
        
    //start adc continous task
    if(halAdcInit(NULL) == ESP_OK)
    {
        ESP_LOGD(LOG_TAG,"initialized halAdcInit");
    } else {
        ESP_LOGE(LOG_TAG,"error initializing halAdcInit");
    }
    halAdcCalibrate();

    //start hid task
    if(xTaskCreate(task_hid,"hid",TASK_HID_STACKSIZE, 
        (void*)NULL,HID_TASK_PRIORITY, NULL) == pdPASS)
    {
        ESP_LOGD(LOG_TAG,"created new hid task");
    } else {
        ESP_LOGE(LOG_TAG,"error creating new hid task");
    }

    //start VB task
    if(xTaskCreate(task_vb,"vb",TASK_VB_STACKSIZE, 
        (void*)NULL,VB_TASK_PRIORITY, NULL) == pdPASS)
    {
        ESP_LOGD(LOG_TAG,"created new vb task");
    } else {
        ESP_LOGE(LOG_TAG,"error creating new vb task");
    }

    //start BLE (all 3 interfaces active)
    if(halBLEInit(1,1,0) == ESP_OK)
    {
        ESP_LOGD(LOG_TAG,"initialized halBle");
    } else {
        ESP_LOGE(LOG_TAG,"error initializing halBle");
    }
    
    //initialize serial communication task
    //(USB-HID & USB Serial for commands)
    if(halSerialInit() == ESP_OK)
    {
        ESP_LOGD(LOG_TAG,"initialized halSerial");
    } else {
        ESP_LOGE(LOG_TAG,"error initializing halSerial");
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
    //disable wifi & enable bluetooth
    radio = BLE;
    taskWebGUIEnDisable(0);
    halBLEEnDisable(1);
    
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
    
    while(1)
    {
        if(xSemaphoreTake(switchRadioSem,portMAX_DELAY) == pdTRUE)
        {
            switch(radio)
            {
                case BLE:
                    ESP_LOGI(LOG_TAG,"Switching from BLE to WIFI");
                    halBLEEnDisable(0);
                    taskWebGUIEnDisable(1);
                    radio = WIFI;
                    LED(127,255,0,0);
                    break;
                case WIFI:
                    ESP_LOGI(LOG_TAG,"Switching from WIFI to BLE");
                    taskWebGUIEnDisable(0);
                    halBLEEnDisable(1);
                    radio = BLE;
                    LED(0,127,255,0);
                    break;
                case UNINITIALIZED:
                default:
                    ESP_LOGE(LOG_TAG,"Error, radio is in unkown mode. Trying with BLE...");
                    taskWebGUIEnDisable(0);
                    halBLEEnDisable(1);
                    radio = BLE;
                    break;
            }
        }
    }
    vTaskDelete(NULL);
}

