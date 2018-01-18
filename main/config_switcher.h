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
 * This file contains the definitions for the config switcher task.
 * It controls all tasks assigned to virtual buttons.
 * If a new configuration should be loaded from the storage, all
 * previously loaded tasks are deleted and new tasks are loaded.
 * 
 * A slot configuration is provided by the config_storage, which is
 * controlled by this module. The config_switcher itself triggers the
 * slot switch if a new slotname is posted to the incoming queue.
 * 
 * The config switcher also provides a task which is used to trigger
 * a slot switch on a virtual button action.
 * 
 */


#ifndef _CONFIG_SWITCHER_H
#define _CONFIG_SWITCHER_H

#include "esp_log.h"

#include "common.h"


//include all hal tasks, making config structs available
#include "hal_adc.h"
#include "hal_io.h"
#include "hal_ble.h"
#include "hal_storage.h"

//include all functional tasks, making config structs available
#include "task_kbd.h"
#include "task_mouse.h"
#include "task_debouncer.h"



#define TASK_CONFIGSWITCHER_STACKSIZE 2048

//TODO: FOR ALL MODULES/TASKS: make a loop for waiting til all queues/flags are initialized (reduces dependency problems)
//TODO: create getter/setter for current config (including fkt tasks & possibly the reloading)

/** type of config switcher action which should be triggered by this task:
 * NAME: load a slot by given name
 * NEXT: load next slot
 * PREV: load previous slot
 * */
/*typedef enum {
  NAME,
  NEXT,
  PREV,
  DEFAULT
}config_switcher_action;*/

/** default config, is used if no config is available 
 * or default is requested. Populated in configSwitcherInit() */
generalConfig_t defaultConfig;


typedef struct taskConfigSwitcherConfig {
  //name of the slot to be loaded
  char slotName[SLOTNAME_LENGTH];
  //number of virtual button which this instance will be attached to
  uint8_t virtualButton;
} taskConfigSwitcherConfig_t;

void task_configswitcher(taskConfigSwitcherConfig_t *param);

/** initializing the config switching functionality.
 * 
 * The task will be loaded and initializes the configuration with
 * a default value.*/
esp_err_t configSwitcherInit();

//TODO: add update config here, enabling updating configs (general & VB) from other parts
//TODO: add save slot here


#endif
