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
 * @brief FUNCTIONAL TASK - joystick handling
 * 
 * This module is used as functional task for joystick control.
 * 
 * It supports buttons (clicking, press&hold, release),
 * axis (X/Y/Z), Z rotate and two sliders (left & right).
 * 
 * In addition, single shot is possible.
 * @note Joystick control by mouthpiece is done in hal_adc!
 * @see hal_adc
 * @see task_joystick
 * @see VB_SINGLESHOT
 */

#ifndef _TASK_JOYSTICK_H
#define _TASK_JOYSTICK_H

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <esp_log.h>
//common definitions & data for all of these functional tasks
#include "common.h"
#include "../config_switcher.h"

#define TASK_JOYSTICK_STACKSIZE 2048


/** type of joystick action which should be triggered by this task:
 * 
 * */
typedef enum {
  BUTTON_PRESS,
  BUTTON_RELEASE,
  XAXIS,
  YAXIS,
  ZAXIS,
  ZROTATE,
  SLIDER_LEFT,
  SLIDER_RIGHT,
  HAT
} joystick_action;

typedef struct taskJoystickConfig {
  /** @brief type of this joystick action
   * @see joystick_action **/
  joystick_action type;
  /** @brief Value of this action, either a button shift nr or an axis value
   * 
   * Range for most axis is 0-1023
   * Buttons are ranged between 0 and 31 (shift mask) */
  uint32_t value;
  /** @brief Value (signed) of this action, used for hat only. */
  int16_t valueS;
  /** @brief Release mode
   * 
   * * 0 Axis/Slider/Button value is not released on VB release flag
   * * 1 Value is set to idle position (512 for axis, 0 for sliders) 
   * or button is released on VB release flag
   * 
   * @note If type is set to BUTTON_RELEASE, this value is ignored.
   * */
  uint8_t mode;
  //number of virtual button which this instance will be attached to
  uint virtualButton;
} taskJoystickConfig_t;

/** @brief FUNCTIONAL TASK - Trigger joystick action
 * 
 * This task is used to trigger a joystick action, possible actions
 * are defined in taskJoystickConfig_t.
 * Can be used as singleshot method by using VB_SINGLESHOT as virtual
 * button configuration.
 * 
 * @param param Task configuration
 * @see VB_SINGLESHOT
 * @see taskJoystickConfig_t*/
void task_joystick(taskJoystickConfig_t *param);

/** @brief Update and send the joystick values.
 * 
 * This method is used to update the globale joystick state and
 * send the updated values to USB/BLE (as configured).
 * Usually this method is used for halAdc to send updates from a 
 * mouthpiece in joystick mode to the axis of the joystick.
 * 
 * @param value1 New value for the axis1 parameter
 * @param axis1 Axis to map the parameter value1 to.
 * @param value2 New value for the axis2 parameter
 * @param axis2 Axis to map the parameter value2 to.
 * @return ESP_OK if updating & sending was fine. ESP_FAIL otherwise (params
 * not valid, e.g. buttons are used, or sending failed)
 * */
esp_err_t joystick_update(int32_t value1, joystick_action axis1, int32_t value2, joystick_action axis2);

/** @brief Reverse Parsing - get AT command for joystick VB
 * 
 * This function parses the current configuration of a virtual button
 * to an AT command used to print the configuration.
 * @param output Output string, where the full AT command will be stored
 * @param cfg Pointer to current joystick configuration, used to parse.
 * @return ESP_OK if everything went fine, ESP_FAIL otherwise
 * */
esp_err_t task_joystick_getAT(char* output, void* cfg);

#endif
