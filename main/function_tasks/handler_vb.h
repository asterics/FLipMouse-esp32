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
 * @brief Event Handler - VB general actions
 * 
 * This module is used as an event handler for all VB commands actions,
 * EXCEPT HID (this is done in handler_hid).
 * Currently implemented actions:
 * * IR command sending
 * * Macro execution
 * * Calibration
 * * Slot switching
 * @note Currently, we use the system event queue (because there is already
 * a task attached). Maybe we switch to an unique one.
 */

#ifndef _HANDLER_VB_H
#define _HANDLER_VB_H

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <esp_log.h>
//common definitions & data for all of these functional tasks
#include "common.h"
#include "../config_switcher.h"
#include "fct_macros.h"
#include "fct_infrared.h"


/** @brief Init for the VB handler
 * 
 * We create the mutex and add handler_vb to the system event queue.
 * @return ESP_OK on success, ESP_FAIL on an error.*/
esp_err_t handler_vb_init(void);

/** @brief Get current root of VB command chain
 * @warning Modifying this chain without acquiring the vbCmdSem could result in
 * undefined behaviour!.
 * @return Pointer to root of VB chain
 */
vb_cmd_t *handler_vb_getCmdChain(void);


/** @brief Set current root of VB command chain
 * @warning By using this function, previously used VB commands are cleared!
 * @param chain Pointer to root of VB chain
 * @return ESP_OK if chain is saved, ESP_FAIL otherwise (lock was not free, deleting old chain failed..)
 */
esp_err_t handler_vb_setCmdChain(vb_cmd_t *chain);

/** @brief Remove command for a virtual button
 * 
 * This method removes any command from the list of commands
 * which are assigned to this VB.
 * 
 * @param vb VB which should be removed
 * @return ESP_OK if deleted, ESP_FAIL if not in list */
esp_err_t handler_vb_delCmd(uint8_t vb);

/** @brief Add a new VB command for a virtual button
 * 
 * This method adds the given VB command to the list of VB commands
 * which will be processed if the corresponding VB is triggered.
 * 
 * @note Highest bit determines press/release action. If set, it is a press!
 * @note If VB number is set to VB_SINGLESHOT, the command will be sent immediately.
 * @note We will malloc for each command here. To free the memory, call handler_vb_clearCmds .
 * @param newCmd New command to be added or triggered if vb is VB_SINGLESHOT
 * @param replace If set to != 0, any previously assigned command is removed from list.
 * @return ESP_OK if added, ESP_FAIL if not added (out of memory) */
esp_err_t handler_vb_addCmd(vb_cmd_t *newCmd, uint8_t replace);

/** @brief Clear all stored VB commands.
 * 
 * This method clears all stored VB commands and frees the allocated memory.
 * 
 * @return ESP_OK if commands are cleared, ESP_FAIL otherwise
 * */
esp_err_t handler_vb_clearCmds(void);

/** @brief Reverse Parsing - get AT command of a given VB
 * 
 * This function parses the current configuration of a virtual button
 * to an AT command used to print the configuration.
 * @param output Output string, where the full AT command will be stored
 * @param vb Number of virtual button for getting the AT command
 * @return ESP_OK if everything went fine, ESP_FAIL otherwise
 * */
esp_err_t handler_vb_getAT(char* output, uint8_t vb);

/** @brief Check if a VB is active in this handler
 * 
 * This function returns true if a given vb is active in this handler
 * (is located in the command chain with a given command).
 * 
 * @param vb Number of virtual button to check
 * @return true if active, false if not */
bool handler_vb_active(uint8_t vb);

#endif /* _HANDLER_VB_H */
