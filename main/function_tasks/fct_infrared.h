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
 * @brief FUNCTION - Infrared remotes
 * 
 * This module is used for recording & sending of
 * infrared commands.
 * Pinning and low-level interfacing for infrared is done in hal_io.c,
 * this part here manages loading & storing the commands.
 * 
 * @see hal_io.c
 */

#ifndef _TASK_INFRARED_H
#define _TASK_INFRARED_H

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <esp_log.h>
//used for rmt_item32_t type
#include "driver/rmt.h"
//common definitions & data for all of these functional tasks
#include "common.h"
#include "../config_switcher.h"

/**@brief FUNCTION - Set the time between two IR edges which will trigger the timeout
 * (end of received command)
 * 
 * This method is used to set the timeout between two edges which is
 * used to trigger the timeout, therefore the finished signal for an
 * IR command recording.
 * 
 * @see infrared_record
 * @warning Normal NEC codes use a ~18ms start signal, so it is recommended to this value at least to 20!
 * @see generalConfig_t
 * @param timeout Timeout in [ms], 2-100
 * @return ESP_OK if parameter is set, ESP_FAIL otherwise (out of range)
 * */
esp_err_t fct_infrared_set_edge_timeout(uint8_t timeout);


/** @brief FUNCTION - Trigger an IR command recording.
 * 
 * This method is used to record one infrared command.
 * It will block until either the timeout is reached or
 * a command is received.
 * The command is stored vial hal_storage.
 * 
 * @see halStorageStoreIR
 * @see TASK_HAL_IR_RECV_TIMEOUT
 * @param cmdName Name of command which will be used to store.
 * @param outputtoserial If set to !=0, the hex stream will be sent to the serial interface.
 * @return ESP_OK if command was stored, ESP_FAIL otherwise (timeout)
 * */
esp_err_t fct_infrared_record(char* cmdName, uint8_t outputtoserial);

/**@brief FUNCTION - Infrared command sending
 * 
 * This task is used to trigger an IR command on a VB action.
 * The IR command which should be sent is identified by a name.
 * 
 * @see taskInfraredConfig_t
 * @param param Task config
 * @see VB_SINGLESHOT
 * */
void fct_infrared_send(char* cmdName);


#endif
