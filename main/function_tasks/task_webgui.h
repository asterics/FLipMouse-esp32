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
 * Copyright 2017 Benjamin Aigner <aignerb@technikum-wien.at,
 * beni@asterics-foundation.org>
 */
/** @brief CONTINOUS TASK - Web server & WebGUI handling.
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
 * */
#ifndef _TASK_WEBGUI_H
#define _TASK_WEBGUI_H

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <esp_log.h>
//common definitions & data for all of these functional tasks
#include "common.h"
#include <inttypes.h>

#include "esp_wifi.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_err.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"


#include "lwip/api.h"
#include "lwip/err.h"
#include "lwip/netdb.h"

#include "cJSON.h"
#include "captdns.h"

#define TASK_WEBGUI_MONITOR_STACKSIZE 4096
#define TASK_WEBGUI_SERVER_STACKSIZE 16384

/** @brief Init the web / DNS server and the web gui
 * 
 * TBD: documentation what is done here
 * 
 * @return ESP_OK on success, ESP_FAIL otherwise
 * */
esp_err_t taskWebGUIInit(void);

#endif /* _TASK_WEBGUI_H */
