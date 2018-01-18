// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Adaptions done:
// Copyright 2017 Benjamin Aigner <beni@asterics-foundation.org>

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
#include "hal/hal_adc.h"
#include "hal/hal_io.h"
#include "hal/hal_ble.h"
#include "hal/hal_serial.h"
#include "config_switcher.h"

#include "config.h"

//new
    TaskHandle_t debouncer = NULL;
    //initialized in debouncer
    EventGroupHandle_t virtualButtonsOut[NUMBER_VIRTUALBUTTONS];
    EventGroupHandle_t virtualButtonsIn[NUMBER_VIRTUALBUTTONS];
    //initialized here
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
    
//old

#if 0
void process_uart(uint8_t *input, uint16_t len)
{
    //tested commands:
    //ID_FABI
    //ID_FLIPMOUSE
    //ID
    //PMx (PM0 PM1)
    //GP
    //DPx (number of paired device, 1-x)
    //KW (+layouts)
    //KD/KU
    //KR
    //KL
    
    //untested commands:
    //KC
    //M
    
    //TBD: joystick (everything)
    // KK (if necessary)
    
    if(len < 2) return;
    //easier this way than typecast in each str* function
    const char *input2 = (const char *) input;
    int counter;
    char nl = '\n';
    uint8_t keycode = 0;
    esp_ble_bond_dev_t *btdevlist = NULL;
    #define DEBUG_TAG "UART_PARSER"
    
    /**++++ commands without parameters ++++*/
    //get module ID
    if(strcmp(input2,"ID") == 0) 
    {
        uart_write_bytes(EX_UART_NUM, MODULE_ID, sizeof(MODULE_ID));
        ESP_LOGD(DEBUG_TAG,"sending module id (ID)");
        return;
    }

    //get all BT pairings
    if(strcmp(input2,"GP") == 0)
    {
        counter = esp_ble_get_bond_device_num();
        if(counter > 0)
        {
            btdevlist = (esp_ble_bond_dev_t *) malloc(sizeof(esp_ble_bond_dev_t)*counter);
            if(btdevlist != NULL)
            {
                if(esp_ble_get_bond_device_list(&counter,btdevlist) == ESP_OK)
                {
                    ESP_LOGI(DEBUG_TAG,"bonded devices (starting with index 0):");
                    ESP_LOGI(DEBUG_TAG,"---------------------------------------");
                    for(uint8_t i = 0; i<counter;i++)
                    {
                        //print on monitor & external uart
                        esp_log_buffer_hex(DEBUG_TAG, btdevlist[i].bd_addr, sizeof(esp_bd_addr_t));
                        uart_write_bytes(EX_UART_NUM, (char *)btdevlist[i].bd_addr, sizeof(esp_bd_addr_t));
                        uart_write_bytes(EX_UART_NUM,&nl,sizeof(nl)); //newline
                    }
                    ESP_LOGI(DEBUG_TAG,"---------------------------------------");
                } else ESP_LOGE(DEBUG_TAG,"error getting device list");
            } else ESP_LOGE(DEBUG_TAG,"error allocating memory for device list");
        } else ESP_LOGE(DEBUG_TAG,"error getting bonded devices count or no devices bonded");
        return;
    }
    
    //joystick: update data (send a report)
    if(strcmp(input2,"JU") == 0) 
    {
        //TBD: joystick
        ESP_LOGD(DEBUG_TAG,"TBD! joystick: send report (JU)");
        return;
    }
    
    //keyboard: release all
    if(strcmp(input2,"KR") == 0)
    {
        for(uint8_t i = 0; i < sizeof(keycode_arr); i++) keycode_arr[i] = 0;
        keycode_modifier = 0;
        keycode_deadkey_first = 0;
        //esp_hidd_send_keyboard_value(hid_conn_id,keycode_modifier,keycode_arr,sizeof(keycode_arr));
        ESP_LOGD(DEBUG_TAG,"keyboard: release all (KR)");
        return;
    }
    
    /**++++ commands with parameters ++++*/
    switch(input[0])
    {
        case 'K': //keyboard
            //key up
            if(input[1] == 'U' && len == 3) 
            {
                remove_keycode(input[2],keycode_arr);
                esp_hidd_send_keyboard_value(hid_conn_id,keycode_modifier,keycode_arr,sizeof(keycode_arr));
                return;
            }
            //key down
            if(input[1] == 'D' && len == 3) 
            {
                add_keycode(input[2],keycode_arr);
                esp_hidd_send_keyboard_value(hid_conn_id,keycode_modifier,keycode_arr,sizeof(keycode_arr));
                return;
            }
            //keyboard, set locale
            if(input[1] == 'L' && len == 3) 
            { 
                if(input[2] < LAYOUT_MAX) 
                {
                    config.locale = input[2];
                    update_config();
                } else ESP_LOGE(DEBUG_TAG,"Locale out of range");
                return;
            }
            
            //keyboard, write
            if(input[1] == 'W')
            {
                ESP_LOGI(DEBUG_TAG,"sending keyboard write, len: %d (bytes, not characters!)",len-2);
                for(uint16_t i = 2; i<len; i++)
                {
                    if(input[i] == '\0') 
                    {
                        ESP_LOGI(DEBUG_TAG,"terminated string, ending KW");
                        break;
                    }
                    keycode = parse_for_keycode(input[i],config.locale,&keycode_modifier,&keycode_deadkey_first); //send current byte to parser
                    if(keycode == 0) 
                    {
                        ESP_LOGI(DEBUG_TAG,"keycode is 0 for 0x%X, skipping to next byte",input[i]);
                        continue; //if no keycode is found,skip to next byte (might be a 16bit UTF8)
                    }
                    ESP_LOGI(DEBUG_TAG,"keycode: %d, modifier: %d, deadkey: %d",keycode,keycode_modifier,keycode_deadkey_first);
                    //TODO: do deadkey sequence...
                    
                    //if a keycode is found, add to keycodes for HID
                    add_keycode(keycode,keycode_arr);
                    ESP_LOGI(DEBUG_TAG,"keycode arr, adding and removing:");
                    esp_log_buffer_hex(DEBUG_TAG,keycode_arr,6);
                    //send to device
                    esp_hidd_send_keyboard_value(hid_conn_id,keycode_modifier,keycode_arr,sizeof(keycode_arr));
                    //wait 1 tick to release key
                    vTaskDelay(1); //suspend for 1 tick
                    //remove key & send report
                    remove_keycode(keycode,keycode_arr);
                    esp_log_buffer_hex(DEBUG_TAG,keycode_arr,6);
                    esp_hidd_send_keyboard_value(hid_conn_id,keycode_modifier,keycode_arr,sizeof(keycode_arr));
                    vTaskDelay(1); //suspend for 1 tick
                }
                return;
            }
            
            //keyboard, get keycode for unicode bytes
            if(input[1] == 'C' && len == 4) 
            {
                keycode = get_keycode(input[2],config.locale,&keycode_modifier,&keycode_deadkey_first);
                //if the first byte is not sufficient, try with second byte.
                if(keycode == 0) 
                { 
                    keycode = get_keycode(input[3],config.locale,&keycode_modifier,&keycode_deadkey_first);
                }
                //first the keycode + modifier are sent.
                //deadkey starting key is sent afterwards (0 if not necessary)
                uart_write_bytes(EX_UART_NUM, (char *)&keycode, sizeof(keycode));
                uart_write_bytes(EX_UART_NUM, (char *)&keycode_modifier, sizeof(keycode_modifier));
                uart_write_bytes(EX_UART_NUM, (char *)&keycode_deadkey_first, sizeof(keycode_deadkey_first));
                uart_write_bytes(EX_UART_NUM, &nl, sizeof(nl));
                return;
            }
            //keyboard, get unicode mapping between 2 locales
            if(input[1] == 'K' && len == 6) 
            { 
                //cpoint = get_cpoint((input[2] << 8) || input[3],input[4],input[5]);
                //uart_write_bytes(EX_UART_NUM, (char *)&cpoint, sizeof(cpoint));
                //uart_write_bytes(EX_UART_NUM, &nl, sizeof(nl));
                //TODO: is this necessary or useful anyway??
                return;
            }
            break;
        case 'J': //joystick
            //joystick, set X,Y,Z (each 0-1023)
            if(input[1] == 'S' && len == 8) { }
            //joystick, set Z rotate, slider left, slider right (each 0-1023)
            if(input[1] == 'T' && len == 8) { }
            //joystick, set buttons (bitmap for button nr 1-16)
            if(input[1] == 'B' && len == 4) { }
            //joystick, set hat (0-360°)
            if(input[1] == 'H' && len == 4) { }
            //TBD: joystick
            ESP_LOGD(DEBUG_TAG,"TBD! joystick");
            break;
        
        default: //test for management commands
            break;
    }
    //mouse input
    //M<buttons><X><Y><wheel>
    if(input[0] == 'M' && len == 5)
    {
        esp_hidd_send_mouse_value(hid_conn_id,input[1],input[2],input[3],input[4]);
        ESP_LOGD(DEBUG_TAG,"mouse movement");
        return;
    }
    //BT: delete one pairing
    if(input[0] == 'D' && input[1] == 'P' && len == 3)
    {
        counter = esp_ble_get_bond_device_num();
        if(counter == 0)
        {
            ESP_LOGE(DEBUG_TAG,"error deleting device, no paired devices");
            return; 
        }
        
        if(input[2] >= '0' && input[2] <= '9') input[2] -= '0';
        if(input[2] >= counter)
        {
            ESP_LOGE(DEBUG_TAG,"error deleting device, number out of range");
            return;
        }
        if(counter >= 0)
        {
            btdevlist = (esp_ble_bond_dev_t *) malloc(sizeof(esp_ble_bond_dev_t)*counter);
            if(btdevlist != NULL)
            {
                if(esp_ble_get_bond_device_list(&counter,btdevlist) == ESP_OK)
                {
                    esp_ble_remove_bond_device(btdevlist[input[2]].bd_addr);
                } else ESP_LOGE(DEBUG_TAG,"error getting device list");
            } else ESP_LOGE(DEBUG_TAG,"error allocating memory for device list");
        } else ESP_LOGE(DEBUG_TAG,"error getting bonded devices count");
        return;
    }
    //BT: enable/disable discoverable/pairing
    if(input[0] == 'P' && input[1] == 'M' && len == 3)
    {
        if(input[2] == 0 || input[2] == '0')
        {
            if(esp_ble_gap_stop_advertising() != ESP_OK)
            {
                ESP_LOGE(DEBUG_TAG,"error stopping advertising");
            }
        } else if(input[2] == 1 || input[2] == '1') {
            if(esp_ble_gap_start_advertising(&hidd_adv_params) != ESP_OK)
            {
                ESP_LOGE(DEBUG_TAG,"error starting advertising");
            } else {
                //TODO: terminate any connection to be pairable?
            }
        } else ESP_LOGE(DEBUG_TAG,"parameter error, either 0/1 or '0'/'1'");
        ESP_LOGD(DEBUG_TAG,"management: pairing %d (PM)",input[2]);
        return;
    }
    
    //set BT names (either FABI or FLipMouse)
    if(strcmp(input2,"ID_FABI") == 0)
    {
        config.bt_device_name_index = 0;
        update_config();
        ESP_LOGD(DEBUG_TAG,"management: set device name to FABI (ID_FABI)");
        return;
    }
    if(strcmp(input2,"ID_FLIPMOUSE") == 0) 
    {
        config.bt_device_name_index = 1;
        update_config();
        ESP_LOGD(DEBUG_TAG,"management: set device name to FLipMouse (ID_FLIPMOUSE)");
        return;
    }
    
    
    ESP_LOGE(DEBUG_TAG,"No command executed with: %s ; len= %d\n",input,len);
}
#endif

#if 0
void uart_stdin(void *pvParameters)
{
    static uint8_t command[50];
    static uint8_t cpointer = 0;
    static uint8_t keycode = 0;
    char character;
    /** demo mouse speed */
    #define MOUSE_SPEED 30
    
    //Install UART driver, and get the queue.
    uart_driver_install(CONFIG_CONSOLE_UART_NUM, UART_FIFO_LEN * 2, UART_FIFO_LEN * 2, 0, NULL, 0);
    
    
    while(1)
    {
        // read single byte
        uart_read_bytes(CONFIG_CONSOLE_UART_NUM, (uint8_t*) &character, 1, portMAX_DELAY);
		
        //sum up characters to one \n terminated command and send it to
        //UART parser
        if(character == '\n' || character == '\r')
        {
            printf("received enter, forward command to UART parser\n");
            command[cpointer] = 0x00;
            process_uart(command, cpointer);
            cpointer = 0;
        } else {
            if(cpointer < 50)
            {
                command[cpointer] = character;
                cpointer++;
            }
        }

        if (!sec_conn) {
            printf("Not connected, ignoring '%c'\n", character);
        } else {
            switch (character){
                case 'a':
                    //esp_hidd_send_mouse_value(hid_conn_id,0,-MOUSE_SPEED,0,0);
                    break;
                case 's':
                    //esp_hidd_send_mouse_value(hid_conn_id,0,0,MOUSE_SPEED,0);
                    break;
                case 'd':
                    //esp_hidd_send_mouse_value(hid_conn_id,0,MOUSE_SPEED,0,0);
                    break;
                case 'w':
                    //esp_hidd_send_mouse_value(hid_conn_id,0,0,-MOUSE_SPEED,0);
                    break;
                case 'l':
                    //esp_hidd_send_mouse_value(hid_conn_id,0x01,0,0,0);
                    //esp_hidd_send_mouse_value(hid_conn_id,0x00,0,0,0);
                    break;
                case 'r':
                    //esp_hidd_send_mouse_value(hid_conn_id,0x02,0,0,0);
                    //esp_hidd_send_mouse_value(hid_conn_id,0x00,0,0,0);
                    break;
                case 'y':
                case 'z':
                    printf("Received: %d\n",character);
                    break;
                case 'Q':
                    //send only lower characters
                    vTaskDelay(1000 / portTICK_PERIOD_MS);
                    keycode = 28;
                    //esp_hidd_send_keyboard_value(hid_conn_id,0,&keycode,1);
                    keycode = 0;
                    //esp_hidd_send_keyboard_value(hid_conn_id,0,&keycode,1);
                    break;
            }
        }

        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}
#endif


void app_main()
{
    esp_err_t ret;
    #define LOG_TAG "app_main"
    char commandname[SLOTNAME_LENGTH];
    
    
    //TODO: start FLIPMOUSE & FABI HERE
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
    xTaskResumeAll();
        //start debouncer
        if(xTaskCreate(task_debouncer,"debouncer",TASK_DEBOUNCER_STACKSIZE, 
            (void*)NULL,DEBOUNCER_TASK_PRIORITY, debouncer) == pdPASS)
        {
            ESP_LOGD(LOG_TAG,"created new debouncer task");
        } else {
            ESP_LOGE(LOG_TAG,"error creatin new debouncer task");
        }
        //TODO: start joystick analog task
        
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
        
        if(halSerialInit() == ESP_OK)
        {
            ESP_LOGD(LOG_TAG,"initialized halSerial");
        } else {
            ESP_LOGE(LOG_TAG,"error initializing halSerial");
        }
        
        if(taskCommandsInit() == ESP_OK)
        {
            ESP_LOGD(LOG_TAG,"initialized taskCommands");
        } else {
            ESP_LOGE(LOG_TAG,"error initializing taskCommands");
        }

    ESP_LOGI(LOG_TAG,"Finished intializing, deleting app_main");
    vTaskDelete(NULL);
        
        
    
    while(1)
    {
        vTaskDelay(50000 / portTICK_PERIOD_MS);
    }


    //init the gpio pin (not needing GPIOs by now...)
    //gpio_demo_init();
    #if 0
    xTaskCreate(&uart_stdin, "stdin", 2048, NULL, 5, NULL);
    #endif

}

