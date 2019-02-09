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
 * Copyright 2019 Benjamin Aigner <aignerb@technikum-wien.at,
 * beni@asterics-foundation.org>
 */
/** @file 
 * @brief Event Handler - HID
 * 
 * This module is used as an event handler for all HID based
 * virtual buttons, containing:
 * * Mouse clicking, holding, pressing & release as well as movement (X/Y/Z)
 * * Keyboard press,hold,release & write
 * * Joystick press,hold,release & axis movement
 * 
 * handler_hid_init is initializing the mutex for adding a command to the
 * chained list and adding handler_hid to the system event queue.
 *
 * @note Currently, we use the system event queue (because there is already
 * a task attached). Maybe we switch to an unique one.
 * @note Mouse/keyboard/joystick control by mouthpiece is done in hal_adc!
 * @see hal_adc
 */

#ifndef _HANDLER_HID_H
#define _HANDLER_HID_H

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <esp_log.h>
#include <esp_event.h>
//common definitions & data for all of these functional tasks
#include "common.h"
#include "fct_macros.h"
#include "../config_switcher.h"

/** @brief Init for the HID handler
 * 
 * We create the mutex and add handler_hid to the system event queue.
 * @return ESP_OK on success, ESP_FAIL on an error.*/
esp_err_t handler_hid_init(void);

/** @brief Get current root of HID command chain
 * @warning Modifying this chain without acquiring the hidCmdSem could result in
 * undefined behaviour!
 * @return Pointer to root of HID chain
 */
hid_cmd_t *handler_hid_getCmdChain(void);

/** @brief Set current root of HID command chain!
 * @warning By using this function, previously used HID commands are cleared!
 * @param chain Pointer to root of HID chain
 * @return ESP_OK if chain is saved, ESP_FAIL otherwise (lock was not free, deleting old chain failed..)
 */
esp_err_t handler_hid_setCmdChain(hid_cmd_t *chain);

/** @brief Add a new HID command for a virtual button
 * 
 * This method adds the given HID command to the list of HID commands
 * which will be processed if the corresponding VB is triggered.
 * 
 * @note Highest bit determines press/release action. If set, it is a press!
 * @note If VB number is set to VB_SINGLESHOT, the command will be sent immediately.
 * @note We will malloc for each command here. To free the memory, call handler_hid_clearCmds .
 * @param newCmd New command to be added.
 * @param replace If set to != 0, any previously assigned command is removed from list.
 * @return ESP_OK if added, ESP_FAIL if not added (out of memory) */
esp_err_t handler_hid_addCmd(hid_cmd_t *newCmd, uint8_t replace);

/** @brief Remove HID command for a virtual button
 * 
 * This method removes any HID command from the list of HID commands
 * which are assigned to this VB.
 * 
 * @param vb VB which should be removed
 * @return ESP_OK if deleted, ESP_FAIL if not in list */
esp_err_t handler_hid_delCmd(uint8_t vb);

/** @brief Clear all stored HID commands.
 * 
 * This method clears all stored HID commands and frees the allocated memory.
 * 
 * @return ESP_OK if commands are cleared, ESP_FAIL otherwise
 * */
esp_err_t handler_hid_clearCmds(void);

/** @brief Reverse Parsing - get AT command for HID VB
 * 
 * This function parses the current configuration of a virtual button
 * to an AT command used to print the configuration.
 * @param output Output string, where the full AT command will be stored
 * @param vb Number of virtual button for getting the AT command
 * @return ESP_OK if everything went fine, ESP_FAIL otherwise
 * */
esp_err_t handler_hid_getAT(char* output, uint8_t vb);

#endif /* _HANDLER_HID_H */
