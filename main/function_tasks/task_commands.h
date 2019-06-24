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
 * @brief CONTINOUS TASK - Main command parser for serial AT commands.
 * 
 * This module is used to parse any incoming serial data from hal_serial
 * for valid AT commands.
 * If a valid command is detected, the corresponding action is triggered
 * (mostly by invoking task_hid or task_vb via a hid_cmd_t or vb_cmd_t).
 * 
 * By issueing an <b>AT BM</b> command, the next issued AT command
 * will be assigned to a virtual button. This is done via setting
 * the requestVBUpdate variable to the VB number. One time only commands
 * (without AT BM) are defined as VB==VB_SINGLESHOT
 * 
 * @see VB_SINGLESHOT
 * @see hal_serial
 * @see atcmd_api
 * @see hid_cmd_t
 * @see vb_cmd_t
 * 
 * @note Currently we are using unicode_to_keycode, because terminal programs use it this way.
 * For supporting unicode keystreams (UTF-8), the method parse_for_keycode needs to be used
 * 
 * @note The command parser itself is based on the cmd_parser project. See: <addlinkhere>
 * */
#ifndef _TASK_COMMANDS_H
#define _TASK_COMMANDS_H

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <esp_log.h>
#include "esp_ota_ops.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"
//common definitions & data for all of these functional tasks
#include "common.h"
#include <inttypes.h>

#include "hal_serial.h"
#include "fct_infrared.h"
#include "fct_macros.h"
#include "handler_hid.h"
#include "handler_vb.h"
#include "keyboard.h"
#include "../config_switcher.h"

#define TASK_COMMANDS_STACKSIZE 4096

/** @brief Init the command parser
 * 
 * This method starts the command parser task,
 * which handles incoming UART bytes & responses if necessary.
 * If necessary, this task deletes itself & starts a CIM task (it MUST
 * be the other way as well to restart the AT command parser)
 * @see taskCommandsRestart
 * @return ESP_OK on success, ESP_FAIL otherwise
 * */
esp_err_t taskCommandsInit(void);

/** @brief Restart the command parser task
 * 
 * This method is used, if the CIM parser detects AT commands and stops
 * itself, providing the AT command set again.
 * @return ESP_OK on success, ESP_FAIL otherwise (cannot start task, task already running)
 * */
esp_err_t taskCommandsRestart(void);

/*++++ following parts are used from the cmd_parser project */


///Prefix, which is located directly prior to any implemented command
#define CMD_PREFIX      "AT "
///Maximum length of a command (EXcluding the prefix, e.g. "AT RO" with prefix "AT " results in length 2)
#define CMD_LENGTH      2
///Maximum length of a command (including parameters, prefix and command itself -> full line)
#define CMD_MAXLENGTH   ATCMD_LENGTH

/** @brief Handler function pointer for a recognized command
 * @note First parameter is the full received string
 * @note Although we have void* parameters,
 * the given data is either an (int32_t) or a (char*), depending
 * on given parameter types.*/
typedef esp_err_t(*cmd_handler)(char* , void* , void* );

/** @brief Type of parameter for a command */
typedef enum ParamType {
    PARAM_NONE,
    /** Parameter field is unused. 
     * @warning Do not used as first param type, if the second one is used! */
    PARAM_NUMBER,
    /** Parameter field is interpreted as integer number
     * @note The value will be parsed to an int32_t*/
    PARAM_STRING
    /** Parameter field is copied and passed as a string
     * @note We malloc in the parser for each string parameter. */
}cmd_paramtype;

/** @brief Return status of command parser */
typedef enum ParserRetVal {
    PREFIXONLY,
    /** Special case: only the prefix is detected */
    SUCCESS,
    /** Everything went fine, command+params are valid & handler is executed */
    NOCOMMAND,
    /** String itself was OK (long enough, terminated,...), but no associated
     * command was found */
    HANDLERERROR,
    /** Either the handler returned an error (!= ESP_OK) or the handler
     * itself was not set */
    FORMATERROR,
    /** Formatting error, possible reasons:
     * * too short
     * * prefix missing
     */
    PARAMERROR,
    /** Parameter errors, possible reasons:
     * * parameters out of range
     * * missing parameters
     * * missing space between two parameters
     * * number<->string position changed
     */
     POINTERERROR,
     /** If you want to modify an array with this command (if handler is NULL),
      * this return value signals an invalid pointer */
} cmd_retval;

/** @brief If a field should be modified, this will be the target type
 * @note Only basic datatypes are used! If you want to modify an enum or
 * complex structure, use a handler and modify data there. */
typedef enum ParserTargetType {
  NOCAST,
  /** Do NOT do anything with the pointer + offset +*/
  UINT8, 
  /** Parse data to target uint8_t */
  INT8,
  /** Parse data to target int8_t */
  UINT16, 
  /** Parse data to target uint16_t */
  INT16,
  /** Parse data to target int16_t */
  UINT32, 
  /** Parse data to target uint32_t */
  INT32,
  /** Parse data to target int32_t */
} cmd_typecast;

/** @brief Main parser
 * 
 * This parser is called with one finished (and 0-terminated line).
 * It does following steps:
 * * Parameter checking (empty target pointer, empty data string)
 * * Input length checking (should be at least the prefix)
 * * If the prefix is the only data, it will return a special case
 * * Searching through the constant command array
 * * If a match is found, the command parameter structure is checked
 * * Validating input values against the given ranges
 * * Executing the handler (if it is != NULL) or modifying the target struct
 * 
 * @return See cmd_retval. SUCCESS on success.
 */
cmd_retval cmdParser(char * data, generalConfig_t *target);

/** @brief Type for one new command
 * 
 * @note Either you set a handler fct pointer (so the handler field
 * is != NULL) and this handler will be executed on this command. Or: if
 * this field is left null, the general cfg will be changed, according
 * to type (target data type) and offset within the array
 * @note If the data modification is used, only one numerical parameter
 * will be parsed! */
typedef struct OneCmd{
    /** String which will be recognized as this command.
     * @see CMD_LENGTHv */
    char name[CMD_LENGTH+1];  //command name. e.g. "RO", combined with "AT " as prefix -> command "AT RO"; including \0 here (-> +1)
    /** Array of parameter types. Currently maximum 2 parameters are implemented. */
    cmd_paramtype ptype[2];
    /** Array of minimal values (either min value of an integer or min length of a string) */
    int32_t min[2];        //minimal value for param1 and param2
    /** Array of maximum values (either max value of an integer or max length of a string) */
    int32_t max[2];        //maximum value for param1 and param2
    /** 1st possible action: Handler which will be executed if this command is recognized and the parameters are valid */
    cmd_handler handler;     //action1, triggered if command is recognized
    /** 2nd possible action: modify an offset (in our case: change currentCfg) */
    size_t offset;
    /** ad 2nd To know the casting, we need to specify a type of the target here */
    cmd_typecast type;
} onecmd_t;

#endif /* _TASK_COMMANDS_H */
