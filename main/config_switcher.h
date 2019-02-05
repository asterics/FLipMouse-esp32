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
 * */

#ifndef _CONFIG_SWITCHER_H
#define _CONFIG_SWITCHER_H

#include "esp_log.h"
#include "common.h"
#include "tones.h"

//include all hal tasks, making config structs available
#include "hal_adc.h"
#include "hal_storage.h"
#include "hal_io.h"
#include "hal_ble.h"
#include "hal_serial.h"

//include all functional tasks, making config structs available
#include "task_hid.h"
#include "task_vb.h"
#include "task_debouncer.h"

/** Stacksize for functional task task_configswitcher.
 * @see task_configswitcher */
#define TASK_CONFIGSWITCHER_STACKSIZE 2048


/** @brief Initializing the config switching functionality.
 * 
 * The CONTINOUS task will be loaded to enable slot switches via the
 * task_configswitcher FUNCTIONAL task. 
 * @return ESP_OK if everything is fined, ESP_FAIL otherwise */
esp_err_t configSwitcherInit(void);

/** @brief Get the current config struct
 * 
 * This method is used to get a reference to the current config struct.
 * The caller can use this reference to change the configuration and
 * update the config afterwards (via the config_switcher queue).
 * 
 * @see config_switcher
 * @see currentConfig
 * @return Pointer to the current config struct
 * */
generalConfig_t* configGetCurrent(void);

/** @brief Request config update
 * 
 * This method is requesting a config update for the general config.
 * It is used either by the command parser to activate a changed config
 * (by AT commands) or by the config switcher task, to activate a config
 * loaded from flash.
 *  
 * @see config_switcher
 * @see currentConfig
 * @param block Time to wait for finished business before updating. 
 * If 0, returns immediately if not possible.
 * @return ESP_OK on success, ESP_FAIL otherwise
 * */
esp_err_t configUpdate(TickType_t time);


#endif
