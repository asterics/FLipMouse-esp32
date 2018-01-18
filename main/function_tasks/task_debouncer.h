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
 * This file contains the task implementation for the virtual button
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

/** default time before debounce kicks in and flag is mapped */
//todo: this value is high for testing! Usually ~50-100ms
#define DEBOUNCETIME_MS 500
/** time between each flag is checked and timers are started 
 * if a debounce value less than this is selected, flags are mapped
 * immediately */
#define DEBOUNCE_RESOLUTION_MS 10
/** number of concurrent debouncing channels, each channel represents
 * one software timer handle */
#define DEBOUNCERCHANNELS 32

#define TASK_DEBOUNCER_STACKSIZE 2048

void task_debouncer(void *param);


#endif /*_TASK_DEBOUNCER_H*/
