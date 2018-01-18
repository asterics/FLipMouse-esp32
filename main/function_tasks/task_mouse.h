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
 * 
 * This file contains the task declaration for mouse control via
 * virtual buttons (triggered by flags). The analog mouse control is
 * done via the hal_adc task.
 * 
 */


#ifndef _TASK_MOUSE_H
#define _TASK_MOUSE_H

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <esp_log.h>
//common definitions & data for all of these functional tasks
#include "common.h"

#define TIMEOUT 10
#define TASK_MOUSE_STACKSIZE 2048

#define MOUSE_BUTTON_LEFT   (1<<0)
#define MOUSE_BUTTON_RIGHT  (1<<1)
#define MOUSE_BUTTON_MIDDLE (1<<2)
#define MOUSE_DEFAULT_WHEELSTEPS 4 

//TODO: setter/getter for static settings (wheel steps)
//TODO: additional task parameter for doing one-shot (task deletes itself) controlling by at commands
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
  //type of this keyboard action: press, release or 
  mouse_action type;
  mouse_action_param actionparam;
  int actionvalue;
  //number of virtual button which this instance will be attached to
  uint virtualButton;
} taskMouseConfig_t;

void task_mouse(taskMouseConfig_t *param);
void mouse_direct(taskMouseConfig_t *param);


#endif
