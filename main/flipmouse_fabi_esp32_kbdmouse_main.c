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

#include "function_tasks/task_kbd.h"
#include "function_tasks/task_mouse.h"
#include "function_tasks/task_debouncer.h"
#include "function_tasks/task_commands.h"
#include "function_tasks/task_webgui.h"
#include "hal/hal_adc.h"
#include "hal/hal_io.h"
#include "hal/hal_ble.h"
#include "hal/hal_serial.h"
#include "config_switcher.h"

#include "config.h"

#define LOG_TAG "app_main"

EventGroupHandle_t virtualButtonsOut[NUMBER_VIRTUALBUTTONS];
EventGroupHandle_t virtualButtonsIn[NUMBER_VIRTUALBUTTONS];
EventGroupHandle_t connectionRoutingStatus;
QueueHandle_t keyboard_usb_press;
QueueHandle_t keyboard_usb_release;
QueueHandle_t keyboard_ble_press;
QueueHandle_t keyboard_ble_release;
QueueHandle_t mouse_movement_usb;
QueueHandle_t mouse_movement_ble;
QueueHandle_t joystick_movement_usb;
QueueHandle_t joystick_movement_ble;
QueueHandle_t config_switcher;

/** @brief Main task, created by esp-idf
 * 
 * This task is used to initialize all queues & flags.
 * In addition, all necessary tasks are created
 * After initialisation, this task deletes itself.
 * */    
void app_main()
{
    //enter critical section & suspend all tasks for initialising
    vTaskSuspendAll();
        //init all remaining rtos stuff
        //eventgroups
        connectionRoutingStatus = xEventGroupCreate();
        //init flags if not created
        for(uint8_t i = 0; i<NUMBER_VIRTUALBUTTONS; i++)
        {
            virtualButtonsIn[i] =  xEventGroupCreate();
            virtualButtonsOut[i] = xEventGroupCreate();
        }
        //queues
        keyboard_usb_press = xQueueCreate(10,sizeof(uint16_t));
        keyboard_usb_release = xQueueCreate(10,sizeof(uint16_t));
        keyboard_ble_press = xQueueCreate(10,sizeof(uint16_t));
        keyboard_ble_release = xQueueCreate(10,sizeof(uint16_t));
        mouse_movement_usb = xQueueCreate(10,sizeof(mouse_command_t));
        mouse_movement_ble = xQueueCreate(10,sizeof(mouse_command_t));
        joystick_movement_usb = xQueueCreate(10,sizeof(joystick_command_t)); //TBD: right size?
        joystick_movement_ble = xQueueCreate(10,sizeof(joystick_command_t)); //TBD: right size?
        config_switcher = xQueueCreate(5,sizeof(char)*SLOTNAME_LENGTH);
    //exit critical section & resume all tasks for initialising
    xTaskResumeAll();

    //start debouncer
    if(xTaskCreate(task_debouncer,"debouncer",TASK_DEBOUNCER_STACKSIZE, 
        (void*)NULL,DEBOUNCER_TASK_PRIORITY, NULL) == pdPASS)
    {
        ESP_LOGD(LOG_TAG,"created new debouncer task");
    } else {
        ESP_LOGE(LOG_TAG,"error creatin new debouncer task");
    }
    
    //start adc continous task
    if(halAdcInit(NULL) == ESP_OK)
    {
        ESP_LOGD(LOG_TAG,"initialized halAdcInit");
    } else {
        ESP_LOGE(LOG_TAG,"error initializing halAdcInit");
    }
    //start IO continous task
    if(halIOInit() == ESP_OK)
    {
        ESP_LOGD(LOG_TAG,"initialized halIOInit");
    } else {
        ESP_LOGE(LOG_TAG,"error initializing halIOInit");
    }
    //start config switcher
    if(configSwitcherInit() == ESP_OK)
    {
        ESP_LOGD(LOG_TAG,"initialized configSwitcherInit");
    } else {
        ESP_LOGE(LOG_TAG,"error initializing configSwitcherInit");
    }
    //start config switcher
    //TODO: load locale...
    if(halBLEInit(0) == ESP_OK)
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
    
    //initialize web framework
    if(taskWebGUIInit() == ESP_OK)
    {
        ESP_LOGD(LOG_TAG,"initialized webserver/DNS server/webgui");
    } else {
        ESP_LOGE(LOG_TAG,"error initializing webserver/DNS server/webgui");
    } 
    
    //calibrate directly after start-up
    halAdcCalibrate();
    
    //delete this task after initializing.
    ESP_LOGI(LOG_TAG,"Finished intializing, deleting app_main");
    vTaskDelete(NULL);
}

