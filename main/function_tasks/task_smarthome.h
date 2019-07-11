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
 * Copyright 2019 Benjamin Aigner <aignerb@technikum-wien.at,
 * beni@asterics-foundation.org>
 */
/**
 * @file 
 * @brief CONTINOUS TASK - MQTT handling
 * 
 * This module is used for connecting the FLipMouse/FABI to
 * a MQTT broker.
 * 
 * @warning Please note, only one WiFi mode is possible: either
 * this device operates in AP mode and serves the webGUI (see task_webgui.h)
 * OR it operates in station mode, connects to a given Wifi and
 * transmits data via MQTT (if enabled)
 * @note Initialise this module only if a MQTT command is used in the
 * configuration.
 * @see task_webgui.h
 * 
 * */
#ifndef _TASK_MQTT_H
#define _TASK_MQTT_H

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <esp_log.h>
//common definitions & data for all of these functional tasks
#include "common.h"
#include <inttypes.h>
#include "hal_storage.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "mqtt_client.h"

#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_event_legacy.h"
#include "esp_http_client.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_err.h"

/** @brief Stack size for the MQTT task */
#define TASK_MQTT_STACKSIZE 4096

/** @brief Default mqtt delimter character
 * @note This is the default character, if no other is set via "AT MS".
 * It is used to split the "AT MQ" string into topic and payload. */
#define MQTT_DELIMITER ':'

/** @brief Deinit the MQTT task and the wifi
 * 
 * This function deactives MQTT/WiFi in station mode. It is necessary
 * if the webGUI should be switched on.
 * 
 * @return ESP_OK on success, ESP_FAIL otherwise
 * */
esp_err_t taskMQTTDeInit(void);

/** @brief Publish data via MQTT
 * 
 * This function publishes the given data.
 * The parameter topic_payload will be split into the topic name
 * and the corresponding payload to publish.
 * The splitting is done by the currently set MQTT delimiter character,
 * which can be modified with "AT MS". MQTT_DELIMITER will be used
 * if no other character is set.
 * 
 * @see MQTT_DELIMITER
 * @return ESP_OK on success, ESP_FAIL otherwise
 * */
esp_err_t taskREST(char* URL);

/** @brief Publish data via MQTT
 * 
 * This function publishes the given data.
 * The parameter topic_payload will be split into the topic name
 * and the corresponding payload to publish.
 * The splitting is done by the currently set MQTT delimiter character,
 * which can be modified with "AT MS". MQTT_DELIMITER will be used
 * if no other character is set.
 * 
 * @see MQTT_DELIMITER
 * @param topic_payload Topic name and payload to be published. E.g.,
 * "/topic1:ON". Delimiter can be changed via NVS key NVS_MQTT_DELIM.
 * @return ESP_OK on success, ESP_FAIL otherwise
 * @note Wifi will be enabled here, if MQTT is not active.
 * @note Wifi credentials and broker information must be in NVS BEFORE calling this function.
 * @see NVS_STATIONNAME
 * @see NVS_STATIONPW
 * @see NVS_MQTT_BROKER
 */
esp_err_t taskMQTTPublish(char* topic_payload);

/** @brief Init Wifi
 * 
 * This init function initializes the wifi in station mode.
 * 
 * @see NVS_STATIONNAME
 * @see NVS_STATIONPW
 * @note Please activate only if necessary by any configuration.
 * @return ESP_OK on success, ESP_FAIL otherwise
 * */
esp_err_t taskWifiInit(void);



#endif /* _TASK_WEBGUI_H */
