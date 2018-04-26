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

#ifndef _HAL_BLE_H_
#define _HAL_BLE_H_

#include "esp_system.h"
#include "common.h"
//used to get current locale information
#include "../config_switcher.h"


#define TASK_HALBLE_STACKSIZE 2048

//TODO: add release function for buttons of mouse & global button mask

/** initializing BLE HAL
 * 
 * This method initializes the BLE staff:
 * -) Enable BT
 * -) Init GATT & GAP
 * -) Start 4 tasks: Joystick, Mouse, Keyboard Press & Keyboard Release
 * 
 * @param deviceIdentifier Which device is using this firmware? FABI or FLipMouse?
 * @see DEVICE_FABI
 * @see DEVICE_FLIPMOUSE
 * */
esp_err_t halBLEInit(uint8_t deviceIdentifier);

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
void halBLEReset(uint8_t exceptDevice);


/** @brief En- or Disable BLE interface.
 * 
 * This method is used to enable or disable the BLE interface. Currently, the ESP32
 * cannot use WiFi and BLE simultaneously. Therefore, when enabling wifi, it is
 * necessary to disable BLE prior calling taskWebGUIEnDisable.
 * 
 * @note Calling this method prior to initializing BLE via halBLEInit will
 * result in an error!
 * @return ESP_OK on success, ESP_FAIL otherwise
 * @param onoff If != 0, switch on BLE, switch off if 0.
 * */
esp_err_t halBLEEnDisable(int onoff);

#endif
