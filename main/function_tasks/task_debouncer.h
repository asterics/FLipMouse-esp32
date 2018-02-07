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
 * 
 * @brief This file contains the task implementation for the virtual button
 * debouncer.
 * 
 * Uses the event flags of virtualButtonsIn and if a flag is set there,
 * the debouncer waits for a defined debouncing time and maps the flags
 * to virtualButtonsOut.
 */

#ifndef _TASK_DEBOUNCER_H
#define _TASK_DEBOUNCER_H

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <esp_log.h>
//common definitions & data for all of these functional tasks
#include "common.h"

/** Default time before debounce kicks in and flag is mapped 
 * from virtualButtonsIn to virtualButtonsOut. 
 * @see virtualButtonsIn
 * @see virtualButtonsOut
 * @todo Change this value from testing (500ms) to production (~50-100ms)
 * @note Should be a factor of DEBOUNCE_RESOLUTION_MS, to avoid float divisions
 * */
#define DEBOUNCETIME_MS 500

/** Time between each flag is checked and timers are started 
 * if a debounce value less than this is selected, flags are mapped
 * immediately
 * @note Please use a value >10ms, otherwise it might be less than a systick
 * @note Adjust according to DEBOUNCETIME_MS
 * @see DEBOUNCETIME_MS */
 
#define DEBOUNCE_RESOLUTION_MS 10
/** Number of concurrent debouncing channels, each channel represents
 * one software timer handle */
#define DEBOUNCERCHANNELS 32

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
 * @todo Add anti-tremor & deadtime functionality
 * */
void task_debouncer(void *param);


#endif /*_TASK_DEBOUNCER_H*/
