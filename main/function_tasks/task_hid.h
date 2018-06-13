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
/** @file 
 * @brief FUNCTIONAL TASK - HID handling
 * 
 * This module is used as functional task for all HID handling based
 * on virtual buttons, containing:
 * * Mouse clicking, holding, pressing & release as well as movement (X/Y/Z)
 * * Keyboard press,hold,release & write
 * * Joystick press,hold,release & axis movement
 *
 * @note Mouse/keyboard/joystick control by mouthpiece is done in hal_adc!
 * @see hal_adc
 */

#ifndef _TASK_HID_H
#define _TASK_HID_H

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <esp_log.h>
//common definitions & data for all of these functional tasks
#include "common.h"
#include "../config_switcher.h"

#define TASK_HID_STACKSIZE 2048


/** @brief Enter critical section for modifying/reading the command chain
 * 
 * @warning task_hid_exit_critical MUST be called, otherwise HID commands are blocked.
 * @return ESP_OK if it is safe to proceed, ESP_FAIL otherwise (lock was not free in 10 ticks)
 */
esp_err_t task_hid_enterCritical(void);


/** @brief Exit critical section for modifying/reading the command chain
 * @see task_hid_enterCritical
 */
void task_hid_exitCritical(void);


/** @brief Get current root of HID command chain
 * @note Please call task_hid_enterCritical before reading from cmd chain.
 * @return Pointer to root of HID chain
 */
hid_cmd_t *task_hid_getCmdChain(void);


/** @brief Set current root of HID command chain
 * @note Please do NOT call task_hid_enterCritical before setting the new chain!
 * @warning By using this function, previously used HID commands are cleared!
 * @param chain Pointer to root of HID chain
 * @return ESP_OK if chain is saved, ESP_FAIL otherwise (lock was not free, deleting old chain failed..)
 */
esp_err_t task_hid_setCmdChain(hid_cmd_t *chain);


/** @brief CONTINOUS TASK - Trigger HID actions
 * 
 * This task is used to trigger all active HID commands, which are
 * assigned to virtual buttons.
 * 
 * @param param Unused */
void task_hid(void *param);

/** @brief Add a new HID command for a virtual button
 * 
 * This method adds the given HID command to the list of HID commands
 * which will be processed if the corresponding VB is triggered.
 * 
 * @note Highest bit determines press/release action. If set, it is a press!
 * @note If VB number is set to VB_SINGLESHOT, the command will be sent immediately.
 * @note We will malloc for each command here. To free the memory, call task_hid_clearCmds .
 * @param newCmd New command to be added or triggered if vb is VB_SINGLESHOT
 * @param replace If set to != 0, any previously assigned command is removed from list.
 * @return ESP_OK if added, ESP_FAIL if not added (out of memory) */
esp_err_t task_hid_addCmd(hid_cmd_t *newCmd, uint8_t replace);



/** @brief Clear all stored HID commands.
 * 
 * This method clears all stored HID commands and frees the allocated memory.
 * 
 * @return ESP_OK if commands are cleared, ESP_FAIL otherwise
 * */
esp_err_t task_hid_clearCmds(void);


/** @brief Reverse Parsing - get AT command for HID VB
 * 
 * This function parses the current configuration of a virtual button
 * to an AT command used to print the configuration.
 * @param output Output string, where the full AT command will be stored
 * @param vb Number of virtual button for getting the AT command
 * @return ESP_OK if everything went fine, ESP_FAIL otherwise
 * */
esp_err_t task_hid_getAT(char* output, uint8_t vb);

#endif /* _TASK_HID_H */
