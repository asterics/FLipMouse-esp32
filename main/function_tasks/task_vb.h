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
 * @brief CONTINOUS TASK - VB handling
 * 
 * This module is used as continous task for all VB command handling,
 * EXCEPT HID (this is done in task_hid).
 * Currently implemented actions:
 * * IR command sending
 * * Macro execution
 * * Calibration
 * * Slot switching
 */

#ifndef _TASK_VB_H
#define _TASK_VB_H

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <esp_log.h>
//common definitions & data for all of these functional tasks
#include "common.h"
#include "../config_switcher.h"
#include "fct_macros.h"
#include "fct_infrared.h"

#define TASK_VB_STACKSIZE 4096


/** @brief Enter critical section for modifying/reading the command chain
 * 
 * @warning task_vb_exit_critical MUST be called, otherwise VB commands are blocked.
 * @return ESP_OK if it is safe to proceed, ESP_FAIL otherwise (lock was not free in 10 ticks)
 */
esp_err_t task_vb_enterCritical(void);


/** @brief Exit critical section for modifying/reading the command chain
 * @see task_vb_enterCritical
 */
void task_vb_exitCritical(void);


/** @brief Get current root of VB command chain
 * @note Please call task_vb_enterCritical before reading from cmd chain.
 * @return Pointer to root of VB chain
 */
vb_cmd_t *task_vb_getCmdChain(void);


/** @brief Set current root of VB command chain
 * @note Please do NOT call task_vb_enterCritical before setting the new chain!
 * @warning By using this function, previously used VB commands are cleared!
 * @param chain Pointer to root of VB chain
 * @return ESP_OK if chain is saved, ESP_FAIL otherwise (lock was not free, deleting old chain failed..)
 */
esp_err_t task_vb_setCmdChain(vb_cmd_t *chain);


/** @brief CONTINOUS TASK - Trigger VB actions
 * 
 * This task is used to trigger all active VB commands (except HID
 * commands!), which are assigned to virtual buttons.
 * 
 * @param param Unused */
void task_vb(void *param);

/** @brief Remove command for a virtual button
 * 
 * This method removes any command from the list of commands
 * which are assigned to this VB.
 * 
 * @param vb VB which should be removed
 * @return ESP_OK if deleted, ESP_FAIL if not in list */
esp_err_t task_vb_delCmd(uint8_t vb);

/** @brief Add a new VB command for a virtual button
 * 
 * This method adds the given VB command to the list of VB commands
 * which will be processed if the corresponding VB is triggered.
 * 
 * @note Highest bit determines press/release action. If set, it is a press!
 * @note If VB number is set to VB_SINGLESHOT, the command will be sent immediately.
 * @note We will malloc for each command here. To free the memory, call task_vb_clearCmds .
 * @param newCmd New command to be added or triggered if vb is VB_SINGLESHOT
 * @param replace If set to != 0, any previously assigned command is removed from list.
 * @return ESP_OK if added, ESP_FAIL if not added (out of memory) */
esp_err_t task_vb_addCmd(vb_cmd_t *newCmd, uint8_t replace);



/** @brief Clear all stored VB commands.
 * 
 * This method clears all stored VB commands and frees the allocated memory.
 * 
 * @return ESP_OK if commands are cleared, ESP_FAIL otherwise
 * */
esp_err_t task_vb_clearCmds(void);


/** @brief Reverse Parsing - get AT command of a given VB
 * 
 * This function parses the current configuration of a virtual button
 * to an AT command used to print the configuration.
 * @param output Output string, where the full AT command will be stored
 * @param vb Number of virtual button for getting the AT command
 * @return ESP_OK if everything went fine, ESP_FAIL otherwise
 * */
esp_err_t task_vb_getAT(char* output, uint8_t vb);

#endif /* _TASK_VB_H */
