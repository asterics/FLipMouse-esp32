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
 * @brief FUNCTIONAL TASK - Execute macros
 * 
 * This module is used as functional task for sending macros to the
 * command parser.
 * 
 * Macros in context of FLipMouse/FABI are a string of concatenated
 * AT commands, equal to normal AT commands sent by the host.
 * 
 * @note AT command separation is done by a semicolon (';')
 * @see task_macros
 * @see VB_SINGLESHOT
 */

#ifndef _TASK_MACROS_H
#define _TASK_MACROS_H

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <esp_log.h>
//common definitions & data for all of these functional tasks
#include "common.h"
#include "hal_serial.h"
//#include "../config_switcher.h"

/**@brief Stack size for macro functional task */
#define TASK_MACROS_STACKSIZE 2048

/** @brief Macro task configuration
 * @see task_macro
 * @see task_macro_getAT
 * */
typedef struct taskMacrosConfig {
  /** @brief Macro to be executed
   * @note Maximum length is equal to slotname length
   * @see SLOTNAME_LENGTH */
  char macro[SLOTNAME_LENGTH];
  /** @brief Number of virtual button which this instance will be attached to.
   * 
   * For one time triggering, use VB_SINGLESHOT
   * @see VB_SINGLESHOT */
  uint virtualButton;
} taskMacrosConfig_t;

/**@brief FUNCTIONAL TASK - Macro execution
 * 
 * This task is used to trigger macro on a VB action.
 * 
 * @see taskMacrosConfig_t
 * @param param Task config
 * @see VB_SINGLESHOT
 * */
void task_macro(taskMacrosConfig_t *param);

/** @brief Reverse Parsing - get AT command for macro VB
 * 
 * This function parses the current configuration of a virtual button
 * to an AT command used to print the configuration.
 * @param output Output string, where the full AT command will be stored
 * @param cfg Pointer to current macro configuration, used to parse.
 * @return ESP_OK if everything went fine, ESP_FAIL otherwise
 * */
esp_err_t task_macro_getAT(char* output, void* cfg);

#endif
