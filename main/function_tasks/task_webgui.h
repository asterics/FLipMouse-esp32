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
#include "esp_event_loop.h"
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

#include "captdns.h"
#include "websocket.h"

/** @brief Stack size for websocket server task */
#define TASK_WEBGUI_WEBSOCKET_STACKSIZE 8192
/** @brief Stack size for web server task */
//#define TASK_WEBGUI_SERVER_STACKSIZE 16384
#define TASK_WEBGUI_SERVER_STACKSIZE 8192

/** @brief Websocket port */
#define TASK_WEBGUI_WSPORT 1804

/** @brief Init the web / DNS server and the web gui
 * 
 * TBD: documentation what is done here
 * 
 * @return ESP_OK on success, ESP_FAIL otherwise
 * */
esp_err_t taskWebGUIInit(void);

/** @brief En- or Disable WiFi interface.
 * 
 * This method is used to enable or disable the wifi interface. In order to
 * provide a safe FLipMouse/FABI device, the Wifi hotspot is for configuration
 * purposes only. Therefore, an enabled Wifi hotspot will be automatically disabled 
 * within a defined time (see WIFI_OFF_TIME) after the last client was disconnected.
 * In addition, other parts of the software might disable the Wifi prior to
 * the automatic disconnect.
 * 
 * @note Calling this method prior to initializing wifi with taskWebGUIInit will
 * result in an error!
 * @return ESP_OK on success, ESP_FAIL otherwise
 * @param onoff If != 0, switch on WiFi, switch off if 0.
 * @see WIFI_OFF_TIME
 * */
esp_err_t taskWebGUIEnDisable(int onoff);

#endif /* _TASK_WEBGUI_H */
