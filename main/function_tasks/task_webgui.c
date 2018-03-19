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
/**
 * @file 
 * @brief CONTINOUS TASK - Web server & WebGUI handling.
 * 
 * This module is used to serve the web- & DNS server for captive portal
 * mode and the web configuration GUI.
 * 
 * In addition, AT commands are sent to the command parser, the same
 * way as if they would be sent by any host configuration software.
 * In this mode feedback (which is usually sent to the serial interface)
 * is sent back to the webgui for parsing in JS.
 * 
 * @see hal_serial
 * @see task_commands
 * @see atcmd_api
 * 
 * @todo Implement everything (or better said: port it from existing demo)
 * @todo How to integrate into hal_serial? Sending commands to task_commands is easy, but the other way round...
 * @todo Add wifi settings to cfg struct in hal_storage/common; default fallback should be defined
 * @todo Add start/stop function (for pairing/config button)
 * @todo After implementing, test everything....
 * */

#include "task_webgui.h"

/** @brief Logging tag for this module */
#define LOG_TAG "web"

/** @brief Init the web / DNS server and the web gui
 * 
 * TBD: documentation what is done here
 * 
 * @return ESP_OK on success, ESP_FAIL otherwise
 * */
esp_err_t taskWebGUIInit(void)
{
  ESP_LOGE(LOG_TAG,"UNIMPLEMENTED");
  return ESP_OK;
}

