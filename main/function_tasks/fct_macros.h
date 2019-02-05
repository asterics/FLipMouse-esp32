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
/** @file 
 * @brief FUNCTION - Execute macros
 * 
 * This module is used for sending macros to the command parser.
 * 
 * Macros in context of FLipMouse/FABI are a string of concatenated
 * AT commands, equal to normal AT commands sent by the host.
 * 
 * @note AT command separation is done by a semicolon (';')
 * @warning If you want to write the semicolon within a macro, use AT KP with the corresponding keycode!
 */

#ifndef _FCT_MACROS_H
#define _FCT_MACROS_H

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <esp_log.h>
//common definitions & data for all of these functional tasks
#include "common.h"
#include "hal_serial.h"

/**@brief FUNCTION - Macro execution
 * 
 * This function is used to trigger macro on a VB action.
 * 
 * @param param Macro command string
 * @return ESP_OK on success, ESP_FAIL otherwise
 * */
esp_err_t fct_macro(char *param);

#endif /*_FCT_MACROS_H*/
