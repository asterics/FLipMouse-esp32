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
 * Copyright 2019 Benjamin Aigner <aignerb@technikum-wien.at,
 * beni@asterics-foundation.org>
 */
/**
 * @file
 * @brief CONTINOUS TASK - This file contains the task implementation for the virtual button
 * debouncer.
 * 
 * This debouncer task (task_debouncer) waits for incoming events (of
 * type raw_action_t)on the debouncer_in queue. If an incoming event
 * is registered, a timer (esp_timer) will be started. On a finished
 * debounce event, the corresponding event is sent to the system event
 * loop.
 * 
 * The debouncing itself can be controlled via following variables
 * (these settings are located in the global config):
 * 
 * * Press debounce time
 * * Release debounce time
 * * Deadtime between two consecutive actions
 * 
 * If one of these values is set for one dedicated VB, it will be used.
 * If there is no setting, the device's default will be used. If the
 * there is no default setting for this device, a hardcoded debounce time
 * of DEBOUNCETIME_MS is used.
 * 
 * @note For each VB, a debouncer_cfg_t element will be used. The size
 * of this array is determined by VB_MAX. Please set this define accordingly!
 * 
 * @see VB_MAX
 * @see DEBOUNCETIME_MS
 * @see debouncer_in
 * @see raw_action_t
 * @see generalConfig_t
 * 
 */

#ifndef _TASK_DEBOUNCER_H
#define _TASK_DEBOUNCER_H

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_timer.h>
//common definitions & data for all of these functional tasks
#include "common.h"
#include "../config_switcher.h"

/** @brief Default time before debounce kicks in and a raw_action input
 * event is sent to the system event loop.
 * @note This debounce time is used, if no values are set in generalconfig.
 * */
#define DEBOUNCETIME_MS 50

/** @brief Minimum debounce time, anything below will be mapped directly. */
#define DEBOUNCETIME_MIN_MS 10

/** Stack size for debouncer task */
#define TASK_DEBOUNCER_STACKSIZE 2048

/** @brief Debouncing main task
 * 
 * This task is periodically testing the virtualButtonsIn flags
 * for changes (DEBOUNCE_RESOLUTION_MS is used as delay between task
 * wakeups). If a change is detected (either press or release flag for
 * a virtualbutton is set), a timer is started.<br>
 * Either the timer expires and the flag is mapped to virtualButtonsOut
 * by debouncerCallback.<br>
 * Or the input flag is cleared again by the responsible task and the
 * debouncer cancels the timer as well (input was too short).
 * 
 * 
 * @see DEBOUNCERCHANNELS
 * @see DEBOUNCE_RESOLUTION_MS
 * @see xTimers
 * @see debouncerCallback
 * @see virtualButtonsIn
 * @see virtualButtonsOut
 * @note This task is persistently running
 * */
void task_debouncer(void *param);


#endif /*_TASK_DEBOUNCER_H*/
