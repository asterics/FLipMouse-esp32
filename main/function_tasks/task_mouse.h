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
 * @brief FUNCTIONAL TASK - mouse handling
 * 
 * This module is used as functional task for mouse control.
 * It supports right/left/middle clicks (clicking, press&hold, release),
 * left double clicks, mouse wheel and mouse X/Y actions.
 * In addition, single shot is possible.
 * @note Mouse control by mouthpiece is done in hal_adc!
 * @see hal_adc
 * @see task_mouse
 * @see VB_SINGLESHOT
 */

#ifndef _TASK_MOUSE_H
#define _TASK_MOUSE_H

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <esp_log.h>
//common definitions & data for all of these functional tasks
#include "common.h"
#include "../config_switcher.h"

#define TIMEOUT 10
#define TASK_MOUSE_STACKSIZE 2048

#define MOUSE_BUTTON_LEFT   (1<<0)
#define MOUSE_BUTTON_RIGHT  (1<<1)
#define MOUSE_BUTTON_MIDDLE (1<<2)
#define MOUSE_DEFAULT_WHEELSTEPS 4 

//TODO: add release function for buttons

/** type of mouse action which should be triggered by this task:
 * 
 * */
typedef enum {
  RIGHT,
  LEFT,
  MIDDLE,
  WHEEL,
  X,
  Y
} mouse_action;

typedef enum {
  M_CLICK,
  M_HOLD,
  M_RELEASE,
  M_DOUBLE,
  M_UNUSED
} mouse_action_param;

typedef struct taskMouseConfig {
  /** @brief type of this mouse action
   * @see mouse_action **/
  mouse_action type;
  mouse_action_param actionparam;
  int actionvalue;
  //number of virtual button which this instance will be attached to
  uint virtualButton;
} taskMouseConfig_t;

/** return 0 on success, 1 otherwise */
uint8_t mouse_set_wheel(uint8_t steps);
uint8_t mouse_get_wheel(void);
void task_mouse(taskMouseConfig_t *param);


/** @brief Reverse Parsing - get AT command for mouse VB
 * 
 * This function parses the current configuration of a virtual button
 * to an AT command used to print the configuration.
 * @param output Output string, where the full AT command will be stored
 * @param cfg Pointer to current mouse configuration, used to parse.
 * @return ESP_OK if everything went fine, ESP_FAIL otherwise
 * */
esp_err_t task_mouse_getAT(char* output, void* cfg);

#endif
