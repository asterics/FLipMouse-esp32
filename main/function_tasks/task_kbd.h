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
 * 
 * This file contains the task definitions for keyboard key triggering.
 * This task is started by the configuration task and it is assigned
 * to one virtual button. As soon as this button is triggered, the configured
 * keys are sent to either USB, BLE or both.
 * This task can be deleted all the time to change the configuration of
 * one button.
 */

/** @file 
 * @brief FUNCTIONAL TASK - trigger keyboard actions
 * 
 * This file contains the task implementation for keyboard key triggering.
 * This task is started by the configuration task and it is assigned
 * to one virtual button or triggered as single shot. As soon as this button is triggered, 
 * the configured keys are sent to either USB, BLE or both.
 * This task can be deleted all the time to change the configuration of
 * one button.
 * 
 * Keycode configuration is set via taskKeyboardConfig_t.
 * 
 * @warning Parsing of key identifiers and text is NOT done here. Due to
 * performance reasons, parsing is done prior to initialising/calling this task.
 * 
 * @see taskKeyboardConfig_t
 * @see VB_SINGLESHOT
 * */

#ifndef _TASK_KBD_H
#define _TASK_KBD_H

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <esp_log.h>
//common definitions & data for all of these functional tasks
#include "common.h"

/** @brief Timeout for queue sending of keyboard commands [ticks] */
#define TIMEOUT 10
/** @brief Stacksize for this functional task */
#define TASK_KEYBOARD_STACKSIZE 2048
/** @brief Count of keycodes for parameter array for keycodes
 * @see taskKeyboardConfig_t */
#define TASK_KEYBOARD_PARAMETERLENGTH 45

/** @brief Type of keyboard action which should be triggered by this task.
 * 
 * Following types of keyboard actions are possible: <br>
 * * <b>PRESS:</b> all of the given keycodes are pressed
 * * <b>RELEASE:</b> all of the given keycodes are released
 * * <b>PRESS_RELEASE_BUTTON:</b> all of the given keycodes are pressed and released if the virtual button is released
 * * <b>WRITE:</b> all of the given keycodes are pressed and immediately released
 * */
typedef enum {
  PRESS,
  RELEASE,
  PRESS_RELEASE_BUTTON,
  WRITE
}keyboard_action;

/** @brief Config for task_keyboard
 * @see task_keyboard
 * */
typedef struct taskKeyboardConfig {
  /** @brief type of this keyboard action */
  keyboard_action type;
  /** @brief Number of virtual button which this instance will be attached to.
   * Use VB_SINGLESHOT for one-time execution
   * @see VB_SINGLESHOT */
  uint virtualButton;
  /** list of keycodes+modfiers to be pressed/released.
   * @note Low byte contains the keycode, high byte any modifiers
   * @note If set to WRITE, the original input bytes are saved at the end
   * of this array!
   * */
  uint16_t keycodes_text[TASK_KEYBOARD_PARAMETERLENGTH];
} taskKeyboardConfig_t;


/** @brief FUNCTIONAL TASK - trigger keyboard actions
 * 
 * This task is used to trigger keyboard actions, as it is defined in
 * taskKeyboardConfig_t. Can be used as singleshot method by using 
 * VB_SINGLESHOT as virtual button configuration.
 * @see VB_SINGLESHOT
 * @see taskKeyboardConfig_t
 * @param param Task configuration **/
void task_keyboard(taskKeyboardConfig_t *param);

/** @brief Reverse Parsing - get AT command for keyboard VB
 * 
 * This function parses the current configuration of a virtual button
 * to an AT command used to print the configuration.
 * @param output Output string, where the full AT command will be stored
 * @param cfg Pointer to current keyboard configuration, used to parse.
 * @return ESP_OK if everything went fine, ESP_FAIL otherwise
 * */
esp_err_t task_keyboard_getAT(char* output, void* cfg);

#endif
