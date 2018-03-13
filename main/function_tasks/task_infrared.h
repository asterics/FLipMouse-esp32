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
 * @brief FUNCTIONAL TASK - Infrared remotes
 * 
 * This module is used as functional task for sending pre-recorded
 * infrared commands.
 * Recording of an infrared command is done here, but NOT within a 
 * functional task.
 * 
 * Pinning and low-level interfacing for infrared is done in hal_io.c.
 * 
 * @see hal_io.c
 * @see task_infrared
 * @see VB_SINGLESHOT
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

/**@brief Stack size for infrared functional task */
#define TASK_INFRARED_STACKSIZE 1024

/** @brief Infrared task configuration
 * @see task_infrared
 * @see task_infrared_getAT
 * */
typedef struct taskInfraredConfig {
  /** @brief Name of IR command to be sent
   * @note Lenght is equal to slotname length
   * @see SLOTNAME_LENGTH */
  char cmdName[SLOTNAME_LENGTH];
  /** @brief Number of virtual button which this instance will be attached to.
   * 
   * For one time triggering, use VB_SINGLESHOT
   * @see VB_SINGLESHOT */
  uint virtualButton;
} taskInfraredConfig_t;

/**@brief Set the time between two IR edges which will trigger the timeout
 * (end of received command)
 * 
 * This method is used to set the timeout between two edges which is
 * used to trigger the timeout, therefore the finished signal for an
 * IR command recording.
 * 
 * @see infrared_trigger_record
 * @param timeout Timeout in [ms], 0-200
 * */
esp_err_t infrared_set_edge_timeout(uint8_t timeout);


/** @brief Trigger an IR command recording.
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
esp_err_t infrared_record(char* cmdName, uint8_t outputtoserial);

/**@brief FUNCTIONAL TASK - Infrared command sending
 * 
 * This task is used to trigger an IR command on a VB action.
 * The IR command which should be sent is identified by a name.
 * 
 * @see taskInfraredConfig_t
 * @param param Task config
 * @see VB_SINGLESHOT
 * */
void task_infrared(taskInfraredConfig_t *param);


/** @brief Reverse Parsing - get AT command for IR VB
 * 
 * This function parses the current configuration of a virtual button
 * to an AT command used to print the configuration.
 * @param output Output string, where the full AT command will be stored
 * @param cfg Pointer to current infrared configuration, used to parse.
 * @return ESP_OK if everything went fine, ESP_FAIL otherwise
 * */
esp_err_t task_infrared_getAT(char* output, void* cfg);

#endif
