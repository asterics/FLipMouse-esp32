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
 */
/** @file
 * @brief CONTINOUS TASK + FUNCTIONAL TASK - This module takes care of
 * configuration loading.
 * 
 * The config_switcher module is used to control all tasks assigned to 
 * virtual buttons (called FUNCTIONAL TASKS).
 * If a new configuration should be loaded from the storage, all
 * previously loaded tasks are deleted and new tasks are loaded.
 * 
 * A slot configuration is provided by the config_storage, which is
 * controlled by this module. 
 * 
 * task_configswitcher is the FUNCTIONAL TASK for triggering a slot switch.
 * configSwitcherTask is the CONTINOUS TASK which monitors the queue for
 * any config switching action.
 * 
 * @note If you want to add a new FUNCTIONAL TASK, please include headers here
 * & add loading functionality to configSwitcherTask. 
 * 
 * @todo Create getter/setter for current config (including fkt tasks & possibly the reloading)
 * @todo Add update config here, enabling updating configs (general & VB) from other parts
 * @todo Add save slot here
 * */

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


/** Stacksize for functional task task_configswitcher.
 * @see task_configswitcher */
#define TASK_CONFIGSWITCHER_STACKSIZE 2048

/** @brief Parameter for functional task task_configswitcher.
 * @see task_configswitcher */
typedef struct taskConfigSwitcherConfig {
  /** Name of the slot to be loaded
   * @note If you want to switch to next, default or previous please use <b>"__NEXT",
   * "__PREV" or "__DEFAULT" as slotName.</b> **/
  char slotName[SLOTNAME_LENGTH];
  /** Number of virtual button which this instance will be attached to.
   * @see VB_SINGLESHOT */
  uint8_t virtualButton;
} taskConfigSwitcherConfig_t;

/** @brief FUNCTIONAL TASK - Load another slot
 * 
 * This task is used to switch the configuration to another slot.
 * It is possible to load a slot by name or a previous/next/default slot.
 * 
 * @param param Task configuration
 * @see taskConfigSwitcherConfig_t*/
void task_configswitcher(taskConfigSwitcherConfig_t *param);

/** Initializing the config switching functionality.
 * 
 * The CONTINOUS task will be loaded to enable slot switches via the
 * task_configswitcher FUNCTIONAL task. 
 * @return ESP_OK if everything is fined, ESP_FAIL otherwise */
esp_err_t configSwitcherInit(void);

#endif
