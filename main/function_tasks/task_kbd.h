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


#ifndef _TASK_KBD_H
#define _TASK_KBD_H

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <esp_log.h>
//common definitions & data for all of these functional tasks
#include "common.h"

#define TIMEOUT 10
#define TASK_KEYBOARD_STACKSIZE 2048
#define TASK_KEYBOARD_PARAMETERLENGTH 45


//TODO: setter/getter for static settings (keyboard layout?)
//TODO: additional task parameter for doing one-shot (task deletes itself) controlling by at commands


/** type of keyboard action which should be triggered by this task:
 * PRESS: all of the given keycodes are pressed
 * RELEASE: all of the given keycodes are released
 * PRESS_RELEASE: all of the given keycodes are pressed and immediately released
 * PRESS_RELEASE_BUTTON: all of the given keycodes are pressed and released if the virtual button is released
 * WRITE: simply write the given string (can ASCII as well as unicode) by pressing
 *        & releasing all necessary keys
 * */
typedef enum {
  PRESS,
  RELEASE,
  PRESS_RELEASE,
  PRESS_RELEASE_BUTTON,
  WRITE
}keyboard_action;

typedef struct taskKeyboardConfig {
  //type of this keyboard action: press, release or 
  keyboard_action type;
  //number of virtual button which this instance will be attached to
  uint virtualButton;
  //link to all keycodes which should be pressed or the text to be written
  //terminated with a \0 character!
  //it is possible to set each value to a direct keycode (as defined in keylayouts.h with KEY_*)
  //or use unicode characters (use one value for each byte!)
  uint16_t keycodes_text[TASK_KEYBOARD_PARAMETERLENGTH];
} taskKeyboardConfig_t;

void task_keyboard(taskKeyboardConfig_t *param);

void keyboard_direct(taskKeyboardConfig_t *param);


#endif
