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
 * @brief TASK - This module takes care of configuration loading.
 * 
 * The config_switcher module is used to update the configuration.
 * If a new configuration should be loaded from the storage, corresponding
 * storage calls are done, as well as user feedback (LED/buzzer)
 * 
 * A slot configuration is provided by the config_storage reference
 * to other modules, which is controlled by this module. 
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

/** @brief Stacksize for functional task task_configswitcher.
 * @see task_configswitcher */
#define TASK_CONFIGSWITCHER_STACKSIZE 2048

/** @brief Initializing the config switching functionality.
 * 
 * The task will be loaded to enable slot switches
 * via the config_switcher queue.
 * @return ESP_OK if everything is fined, ESP_FAIL otherwise */
esp_err_t configSwitcherInit(void);

/** @brief Get the current config struct
 * 
 * This method is used to get a reference to the current config struct.
 * The caller can use this reference to change the configuration and
 * update the config afterwards (via the config_switcher queue).
 * 
 * @see config_switcher
 * @see currentConfigLoaded
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
