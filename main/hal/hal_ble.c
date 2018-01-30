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
 * Copyright 2017, Benjamin Aigner FH Technikum Wien <aignerb@technikum-wien.at>
 * 
 * This header file contains the hardware abstraction for all BLE
 * related HID commands.
 * This source mostly invokes all BLE HID stuff from the folder "ble_hid".
 * Abstraction to support the structure is done here via pending on all
 * necessary queues & flagsets
 * 
 * Compared to the tasks in the folder "function_tasks" all HAL tasks are
 * singletons.
 * Call init to initialize every necessary data structure.
 * 
 */

#include "hal_ble.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"

//in /ble_hid
#include "hid_desc.h"
#include "hid_dev.h"
#include "esp_hidd_prf_api.h"


#include "esp_bt_defs.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "bt_trace.h"

//in /keyboard_layout_help
#include "keyboard.h"

//#define GATTS_TAG "FABI/FLIPMOUSE"

#define LOG_TAG "hal_ble"

void halBleTaskKeyboardPress(void *param);
void halBleTaskKeyboardRelease(void *param);
void halBleTaskMouse(void *param);
void halBleTaskJoystick(void *param);

static uint16_t hid_conn_id = 0;
static bool sec_conn = false;
static uint8_t keycode_modifier;
static uint8_t keycode_deadkey_first;
static uint8_t keycode_arr[6];

static uint8_t deviceIdentifier;

TaskHandle_t halbletasks[4];
char halbletasknames[4][14] = {"hal_ble_kbd_p", "hal_ble_kbd_r", "hal_ble_mouse", "hal_ble_joyst"};
TaskFunction_t  halbletaskfunctions[4] = {
  halBleTaskKeyboardPress,  
  halBleTaskKeyboardRelease,  
  halBleTaskMouse,  
  halBleTaskJoystick,  
};

const char hid_device_name_fabi[] = "FABI";
const char hid_device_name_flipmouse[] = "FLipMouse";
static uint8_t hidd_service_uuid128[] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    //first uuid, 16bit, [12],[13] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x12, 0x18, 0x00, 0x00,
};

static esp_ble_adv_data_t hidd_adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = true,
    .min_interval = 0x20,
    .max_interval = 0x30,
    .appearance = 0x03c0,       //HID Generic,
    .manufacturer_len = 0,
    .p_manufacturer_data =  NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(hidd_service_uuid128),
    .p_service_uuid = hidd_service_uuid128,
    .flag = 0x6,
};

static esp_ble_adv_params_t hidd_adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x30,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    //.peer_addr            =
    //.peer_addr_type       =
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static void hidd_event_callback(esp_hidd_cb_event_t event, esp_hidd_cb_param_t *param)
{
    switch(event) {
        case ESP_HIDD_EVENT_REG_FINISH: {
            if (param->init_finish.state == ESP_HIDD_INIT_OK) {
                //esp_bd_addr_t rand_addr = {0x04,0x11,0x11,0x11,0x11,0x05};
                if(deviceIdentifier == 1)
                {
                    esp_ble_gap_set_device_name(hid_device_name_fabi);
                } else {
                    esp_ble_gap_set_device_name(hid_device_name_flipmouse);
                }
                esp_ble_gap_config_adv_data(&hidd_adv_data);
                
            }
            break;
        }
        case ESP_BAT_EVENT_REG: {
            break;
        }
        case ESP_HIDD_EVENT_DEINIT_FINISH:
	     break;
		case ESP_HIDD_EVENT_BLE_CONNECT: {
            hid_conn_id = param->connect.conn_id;
            sec_conn = true; //TODO: right here?!?
            LOG_ERROR("%s(), ESP_HIDD_EVENT_BLE_CONNECT", __func__);
            break;
        }
        case ESP_HIDD_EVENT_BLE_DISCONNECT: {
            sec_conn = false;
            hid_conn_id = 0;
            LOG_ERROR("%s(), ESP_HIDD_EVENT_BLE_DISCONNECT", __func__);
            esp_ble_gap_start_advertising(&hidd_adv_params);
            break;
        }
        default:
            break;
    }
    return;
}

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        esp_ble_gap_start_advertising(&hidd_adv_params);
        break;
     case ESP_GAP_BLE_SEC_REQ_EVT:
        for(int i = 0; i < ESP_BD_ADDR_LEN; i++) {
             LOG_DEBUG("%x:",param->ble_security.ble_req.bd_addr[i]);
        }
        esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
	 break;
     case ESP_GAP_BLE_AUTH_CMPL_EVT:
        sec_conn = true;
        LOG_ERROR("status = %s, ESP_GAP_BLE_AUTH_CMPL_EVT",param->ble_security.auth_cmpl.success ? "success" : "fail");
        break;
    default:
        break;
    }
}


void halBleTaskKeyboardPress(void *param)
{
    uint16_t rxK;
    uint8_t keycode;
    uint8_t keycodesLocal[6] = {0,0,0,0,0,0};
    static uint8_t unconnectedWarning = 0;
    generalConfig_t* currentConfig = configGetCurrent();
    
    if(keyboard_ble_press == NULL)
    {
        ESP_LOGE(LOG_TAG,"queue keyboard_ble_press uninitialized, cannot continue");
        vTaskDelete(NULL);
    }
    
    while(1)
    {
        //wait for a keyboard press command
        if(xQueueReceive(keyboard_ble_press, &rxK, 2000/portTICK_PERIOD_MS ))
        {
            //we cannot send if we are not connected
            if(!sec_conn) {
                if(!unconnectedWarning) ESP_LOGW(LOG_TAG,"Not connected, cannot send keyboard");
                unconnectedWarning = 1;
                continue;
            } else unconnectedWarning = 0;
            
            //check if this is a unicode/ascii byte or a keycode
            if((rxK & 0xFF00) == 0)
            {
                //single bytes are ascii or unicode bytes, sent to keycode parser
                keycode = parse_for_keycode((uint8_t) rxK, currentConfig->locale, &keycode_modifier, &keycode_deadkey_first);
                if(keycode != 0)
                {
                    //if a deadkey is issued (no release necessary), we send the deadkey before:
                    if(keycode_deadkey_first != 0)
                    {
                        ESP_LOGD(LOG_TAG,"Sending deadkey first: %d",keycode_deadkey_first);
                        keycodesLocal[0] = keycode_deadkey_first;
                        //send to device
                        esp_hidd_send_keyboard_value(hid_conn_id,keycode_modifier,keycodesLocal,sizeof(keycodesLocal));
                        //wait 1 tick to release key
                        vTaskDelay(1); //suspend for 1 tick
                        //TODO: should we release the key here?!? -> TEST
                        keycodesLocal[0] = keycode_deadkey_first;
                        keycodesLocal[0] = 0;
                        //send to device
                        esp_hidd_send_keyboard_value(hid_conn_id,keycode_modifier,keycodesLocal,sizeof(keycodesLocal));
                    }
                } //we need another byte to calculate keycode...
                    
            } else {
                keycode = rxK & 0x00FF;
            }
            
            //keycodes, add directly to the keycode array
            switch(add_keycode(keycode,keycode_arr))
            {
                case 0: 
                    esp_hidd_send_keyboard_value(hid_conn_id,keycode_modifier,keycode_arr,sizeof(keycode_arr));
                    ESP_LOGD(LOG_TAG,"Kbd: mod:%d, other keycodes: ",keycode_modifier);
                    esp_log_buffer_hex(LOG_TAG,keycode_arr,6);
                    break;
                case 1: ESP_LOGW(LOG_TAG,"keycode already in queue"); break;
                case 2: ESP_LOGE(LOG_TAG,"no space in keycode arr!"); break;
                default: ESP_LOGE(LOG_TAG,"add_keycode return unknown code..."); break;
            }
            
            keycode = 0;
        }
    }
}

void halBleTaskKeyboardRelease(void *param)
{
    uint16_t rxK;
    uint8_t keycode;
    static uint8_t unconnectedWarning = 0;
    generalConfig_t* currentConfig = configGetCurrent();
    
    if(keyboard_ble_release == NULL)
    {
        ESP_LOGE(LOG_TAG,"queue keyboard_ble_release uninitialized, cannot continue");
        vTaskDelete(NULL);
    }
    
    while(1)
    {
        //wait for a keyboard release command
        if(xQueueReceive(keyboard_ble_release, &rxK, 2000/portTICK_PERIOD_MS ))
        {
            //we cannot send if we are not connected
            if(!sec_conn) {
                if(!unconnectedWarning) ESP_LOGW(LOG_TAG,"Not connected, cannot send keyboard");
                unconnectedWarning = 1;
                continue;
            } else unconnectedWarning = 0;
            
            //check if this is a unicode/ascii byte or a keycode
            if((rxK & 0xFF00) == 0)
            {
                //single bytes are ascii or unicode bytes, sent to keycode parser
                keycode = parse_for_keycode((uint8_t) rxK, currentConfig->locale, &keycode_modifier, &keycode_deadkey_first);                   
            } else {
                keycode = rxK & 0x00FF;
            }
            
            //keycodes, remove directly from the keycode array
            switch(remove_keycode(keycode,keycode_arr))
            {
                case 0: 
                    esp_hidd_send_keyboard_value(hid_conn_id,keycode_modifier,keycode_arr,sizeof(keycode_arr));
                    ESP_LOGD(LOG_TAG,"Kbd: mod:%d; %d,%d,%d,%d,%d,%d",keycode_modifier, \
                        keycode_arr[0],keycode_arr[1],keycode_arr[2], \
                        keycode_arr[3],keycode_arr[4],keycode_arr[5]);
                    break;
                case 1: ESP_LOGW(LOG_TAG,"keycode not in queue"); break;
                default: ESP_LOGE(LOG_TAG,"remove_keycode return unknown code..."); break;
            }
            
            keycode = 0;
        }
    }
}

void halBleTaskMouse(void *param)
{
    mouse_command_t rxM;
    static uint8_t unconnectedWarning = 0;
    static uint8_t emptySent = 0;
    mouse_command_t emptyReport = {0,0,0,0};
    
    if(mouse_movement_ble == NULL)
    {
        ESP_LOGE(LOG_TAG,"queue mouse_movement_ble uninitialized, cannot continue");
        vTaskDelete(NULL);
    }
    
    while(1)
    {
        //wait for a mouse command
        if(xQueueReceive(mouse_movement_ble, &rxM, 2000/portTICK_PERIOD_MS ))
        {
            if(!sec_conn) {
                if(!unconnectedWarning) ESP_LOGW(LOG_TAG,"Not connected, cannot send mouse");
                unconnectedWarning = 1;
            } else {
                unconnectedWarning = 0;
                //send report always if not empty
                if(memcmp(&rxM,&emptyReport,sizeof(mouse_command_t)) != 0)
                {
                    esp_hidd_send_mouse_value(hid_conn_id,rxM.buttons, \
                        rxM.x, rxM.y,rxM.wheel);
                    ESP_LOGD(LOG_TAG,"Mouse: %d,%d,%d,%d",rxM.buttons, \
                        rxM.x, rxM.y, rxM.wheel);
                    //after one non-empty report, we can resend an empty one
                    emptySent = 0;
                } else {
                    //if empty, check if it was sent once, if not: send empty
                    if(emptySent == 0)
                    {
                        esp_hidd_send_mouse_value(hid_conn_id,rxM.buttons, \
                            rxM.x, rxM.y,rxM.wheel);
                        ESP_LOGD(LOG_TAG,"Mouse: %d,%d,%d,%d",rxM.buttons, \
                            rxM.x, rxM.y, rxM.wheel);
                        //remember we sent an empty report
                        emptySent = 1;
                    }
                }
            }
        }
    }
}

void halBleTaskJoystick(void *param)
{
    joystick_command_t rxJ;
    static uint8_t unconnectedWarning = 0;
    
    if(joystick_movement_ble == NULL)
    {
        ESP_LOGE(LOG_TAG,"queue joystick_movement_ble uninitialized, cannot continue");
        vTaskDelete(NULL);
    }
    
    while(1)
    {
        //wait for a mouse command
        if(xQueueReceive(joystick_movement_ble, &rxJ, 2000/portTICK_PERIOD_MS ))
        {
            if(!sec_conn) {
                if(!unconnectedWarning) ESP_LOGW(LOG_TAG,"Not connected, cannot send joystick");
                unconnectedWarning = 1;
            } else {
                unconnectedWarning = 0;
                ESP_LOGW(LOG_TAG,"BLE Joystick unimplemented...");
            }
        }
    }
}


/** reset the BLE data
 * Used for slot/config switchers.
 * It resets the keycode array and sets all HID reports to 0
 * (release all keys, avoiding sticky keys on a config change) 
 * @param exceptDevice if you want to reset only a part of the devices, set flags
 * accordingly:
 * 
 * (1<<0) excepts keyboard
 * (1<<1) excepts joystick
 * (1<<2) excepts mouse
 * If nothing is set (exceptDevice = 0) all are reset
 * */
void halBLEReset(uint8_t exceptDevice)
{
    if(!sec_conn)
    {
        ESP_LOGW(LOG_TAG,"Not connected, cannot reset reports");
    } else {
        ESP_LOGD(LOG_TAG,"BLE reset reports");
        //reset mouse
        if(!(exceptDevice & (1<<2))) esp_hidd_send_mouse_value(hid_conn_id,0,0,0,0);
        //reset keyboard
        if(!(exceptDevice & (1<<0)))
        {
            for(uint8_t i=0;i<6;i++) keycode_arr[i] = 0;
            keycode_deadkey_first = 0;
            keycode_modifier = 0;
            esp_hidd_send_keyboard_value(hid_conn_id,keycode_modifier,keycode_arr,sizeof(keycode_arr));
        }
    }
}


/** initializing BLE HAL
 * 
 * This method initializes the BLE staff:
 * -) Enable BT
 * -) Init GATT & GAP
 * -) Start 4 tasks: Joystick, Mouse, Keyboard Press & Keyboard Release
 * 
 * @param locale Keyboard locale which should be used by this device
 * @param deviceIdentifier Which device is using this firmware? FABI or FLipMouse?
 * @see DEVICE_FABI
 * @see DEVICE_FLIPMOUSE
 * */
esp_err_t halBLEInit(uint8_t deviceIdentifier)
{
    generalConfig_t* currentConfig = configGetCurrent();
    // Initialize NVS.
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );
    // Read config
    if(deviceIdentifier > 1)
    {
        ESP_LOGE(LOG_TAG,"deviceIdentifier is invalid, using FABI");
        deviceIdentifier = 1;
    }
    
    if(currentConfig->locale >= LAYOUT_MAX)
    {
        ESP_LOGE(LOG_TAG,"locale is invalid: %d, using US_INTERNATIONAL",currentConfig->locale);
        currentConfig->locale = LAYOUT_US_INTERNATIONAL;
    }

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(LOG_TAG, "initialize controller failed\n");
        return ESP_FAIL;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BTDM);
    if (ret) {
        ESP_LOGE(LOG_TAG, "enable controller failed\n");
        return ESP_FAIL;
    }

    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(LOG_TAG,"init bluedroid failed\n");
        return ESP_FAIL;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(LOG_TAG,"init bluedroid failed\n");
        return ESP_FAIL;
    }
    
    //load HID country code for locale before initialising HID
    hidd_set_countrycode(get_hid_country_code(currentConfig->locale));

    if((ret = esp_hidd_profile_init()) != ESP_OK) {
        ESP_LOGE(LOG_TAG,"init bluedroid failed\n");
        return ESP_FAIL;
    }

    ///register the callback function to the gap module
    esp_ble_gap_register_callback(gap_event_handler);
    esp_hidd_register_callbacks(hidd_event_callback);

    /* set the security iocap & auth_req & key size & init key response key parameters to the stack*/
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_BOND;     //bonding with peer device after authentication
    esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;           //set the IO capability to No output No input
    uint8_t key_size = 16;      //the key size should be 7~16 bytes
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
    /* If your BLE device act as a Slave, the init_key means you hope which types of key of the master should distribut to you,
    and the response key means which key you can distribut to the Master;
    If your BLE device act as a master, the response key means you hope which types of key of the slave should distribut to you, 
    and the init key means which key you can distribut to the slave. */
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));
    
    //start tasks, pending for all 4 input queues
    for(uint8_t i = 0; i<4; i++)
    {
        if(xTaskCreate(halbletaskfunctions[i],halbletasknames[i],TASK_HALBLE_STACKSIZE, 
            (void*)NULL,HAL_BLE_TASK_PRIORITY_BASE+i, &halbletasks[i]) == pdPASS)
        {
            ESP_LOGD(LOG_TAG,"created hal_ble task %d",i);
        } else {
            ESP_LOGE(LOG_TAG,"error creating new debouncer task");
        }
    }
    return ESP_OK;
}
