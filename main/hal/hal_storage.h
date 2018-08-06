 
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
 * @brief HAL TASK - This file contains the abstraction layer for storing FLipMouse/FABI configs
 * 
 * This module stores general configs & virtual button configurations to
 * a storage area. In case of the ESP32 we use FAT with wear leveling,
 * which is provided by the esp-idf.
 * <b>Warning:</b> Please adjust the esp-idf to us 512Byte sectors and <b>mode "safety"</b>!
 * We don't do any safe copying, just building a checksum to detect
 * faulty slots.
 * 
 * This module can be used to store a config struct (generalConfig_t)
 * and the corresponding virtual button config structs to the storage.
 * 
 * Following commands are implemented here:
 * * list all slot names
 * * load default slot
 * * load next or previous slot
 * * load slot by name or number
 * * store a given slot
 * * delete one slot
 * * delete all slots
 * * delete one or all IR commands
 * * get number of stored IR commands
 * * load an IR command
 * * get names for IR commands
 * 
 * This module starts one task on its own for maintaining the storage
 * access.
 * All methods called from this module will block via a semaphore until
 * the requested operation is finished.
 * 
 * For storing small amounts global, non-volatile data, it is possible
 * to use halStorageNVSLoad & halStorageNVSStore operations. In this case,
 * no transaction id is necessary.
 * 
 * Slots are stored in following naming convention (8.3 rule applies here):
 * general slot config:
 * xxx.fms (slot number, e.g., 001.fms for slot 1)
 * virtual button config for slot xxx
 * xxx_VB.fms
 * 
 * @note Maximum number of slots: 250! (e.g. 250.fms)
 * @note Maximum number of IR commands: 100 (0-100, e.g. IR_99.fms)
 * @note Use halStorageStartTransaction and halStorageFinishTransaction on begin/end of loading&storing (except for halStorageNVS* operations)
 * @warning Adjust the esp-idf (via "make menuconfig") to use 512B sectors
 * and mode <b>safety</b>!
 * 
 * @todo Put delete/save to a task, takes very long time...
 * 
 * @see generalConfig_t
 */

#ifndef _HAL_STORAGE_H
#define _HAL_STORAGE_H

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>
#include <esp_log.h>
#include <mbedtls/md5.h>
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "nvs_flash.h"
#include "nvs.h"

//common definitions & data for all of these functional tasks
#include "common.h"

//for creating the default slot & storing HID, we need these includes:
#include "../config_switcher.h"
#include "task_hid.h"
#include "task_vb.h"
#include "task_debouncer.h"
#include "hal_adc.h"


//for IR stuff
#include "hal_io.h"

/** @brief Namespace for storing NVS key/value pairs.
 * @warning If changed, all previously data cannot be used!
 * */
#define HAL_STORAGE_NVS_NAMESPACE "devcfg"

typedef enum {
  NEXT, /** load next slot (no name needed) **/
  PREV, /** load previous slot (no name needed) **/
  DEFAULT, /** load default slot (no name needed) **/
  RESTOREFACTORYSETTINGS /** WARNING: deletes all saved slots, 
    except the default slot (which can be overwritten as well) **/
}hal_storage_load_action;

/** @brief Header for a VB config in FAT file
 * 
 * This struct is placed in front of each VB config, which is stored
 * in the VB config file.
 * 
 * Used in store/load VBs.
 * @see halStorageStoreSetVBConfigs
 * @see halStorageLoadGetVBConfigs
 * */
typedef struct storageHeader {
  /** @brief File offset of previous VB header */
  int offsetPrev;
  /** @brief File offset of next VB header */
  int offsetNext;
  /** @brief File offset of this VB header */
  int offsetThis;
  /** @brief Number of this virtual button config */
  uint8_t vb;
} storageHeader_t;


/** @brief Store a HID command chain to flash
 * 
 * This method stores a set of HID commands, starting with the given pointer
 * to the HID command chain root, to flash. A command set is always stored for
 * a defined slot number.
 * 
 * @param tid Transaction id
 * @param cfg Pointer to HID command chain root
 * @param slotnumber Number of the slot on which this config is used. Use 0xFF to ignore and use
 * previous set slot number (by halStorageStore)
 * @param cmdName Name of this IR command
 * @return ESP_OK on success, ESP_FAIL otherwise
 * */
esp_err_t halStorageStoreHID(uint8_t slotnumber, hid_cmd_t *cfg, uint32_t tid);

/** @brief Load a full HID chain
 * 
 * This method loads an entire HID chain for a given slot.
 * 
 * To start loading the commands, call halStorageStartTransaction to acquire
 * a load/store transaction id. This is necessary to enable multitask access.
 * Finally, if the command is loaded, call halStorageFinishTransaction to
 * free the storage access to the other tasks or the next call.
 * 
 * @see halStorageStartTransaction
 * @see halStorageFinishTransaction
 * @param slotnumber Number of the slot on which this config is used. Use 0xFF to ignore and use
 * previous set slot number (by halStorageLoadXXX) 
 * @param tid Transaction ID, which must match the one given by halStorageStartTransaction
 * @param cfg Pointer which will be the new root of the HID command chain
 * @return ESP_OK if everything is fine, ESP_FAIL if the command was not successful (number not found, error loading)
 * */
esp_err_t halStorageLoadHID(uint8_t slotnumber, hid_cmd_t **cfg, uint32_t tid);

/** @brief Load a string from NVS (global, no slot assignment)
 * 
 * This method is used to load a string from a non-volatile storage.
 * No TID is necessary, just call this function.
 * 
 * @param key Key to identify this value, same as used on store
 * @param string Buffer for string to be read from flash/eeprom (flash in ESP32)
 * @return ESP_OK on success, error codes according to nvs_get_str 
 * */
esp_err_t halStorageNVSLoadString(const char *key, char *string);

/** @brief Store a string into NVS (global, no slot assignment)
 * 
 * This method is used to store a string in a non-volatile storage.
 * No TID is necessary, just call this function.
 * 
 * @param key Key to identify this value on read.
 * @param string String to be stored in flash/eeprom (flash in ESP32)
 * @return ESP_OK on success, error codes according to nvs_set_str 
 * */
esp_err_t halStorageNVSStoreString(const char *key, char *string);

/** @brief Get number of currently loaded slot
 * 
 * An empty device will return 0
 * 
 * @see storageCurrentSlotNumber
 * @return Currently loaded slot number
 * */
uint8_t halStorageGetCurrentSlotNumber(void);

/** Get the name of a slot number
 * 
 * This method returns the name of the given slot number.
 * An invalid slotnumber will return ESP_FAIL and an unchanged slotname
 * 
 * @param tid Transaction id
 * @param slotname Memory to store the slotname to. Attention: minimum length: SLOTNAME_LENGTH+1
 * @param slotnumber Number of slot to be loaded
 * @see halStorageStartTransaction
 * @see SLOTNAME_LENGTH
 * @see halStorageFinishTransaction
 * @return ESP_OK if tid is valid and slot number is valid, ESP_FAIL otherwise
 * */
esp_err_t halStorageGetNameForNumber(uint32_t tid, uint8_t slotnumber, char *slotname);


/** @brief Get the number of a slotname
 * 
 * This method returns the number of the given slotname.
 * An invalid name will return ESP_FAIL and a slotnumber of 0
 * 
 * @param tid Transaction id
 * @param slotname Name of the slot to be looked for
 * @param slotnumber Variable where the slot number will be stored
 * @see halStorageStartTransaction
 * @see halStorageFinishTransaction
 * @return ESP_OK if tid is valid and slot number is valid, ESP_FAIL otherwise
 * */
esp_err_t halStorageGetNumberForName(uint32_t tid, uint8_t *slotnumber, char *slotname);


/** @brief Get the number of stored slots
 * 
 * This method returns the number of available slots (the default slot
 * is not counted).
 * An empty device will return 0
 * 
 * @param tid Transaction id
 * @param slotsavailable Variable where the slot count will be stored
 * @see halStorageStartTransaction
 * @see halStorageFinishTransaction
 * @return ESP_OK if tid is valid and slot count is valid, ESP_FAIL otherwise
 * */
esp_err_t halStorageGetNumberOfSlots(uint32_t tid, uint8_t *slotsavailable);

/** @brief Get free memory (IR & slot storage)
 * 
 * This method returns the number of total and free bytes in current
 * used persistant memory (used for storing IR commands and slot
 * storage).
 * 
 * @param total Pointer where the number of total possible bytes is stored
 * @param free Pointer where the number of free bytes is stored
 * @return ESP_OK if reading is valid, ESP_FAIL otherwise
 * */
esp_err_t halStorageGetFree(uint32_t *total, uint32_t *free);

/** @brief Load a slot by an action
 * 
 * This method loads a slot & saves the general config to the given
 * config struct pointer.
 * The slot is defined by the navigate parameter, using next/prev/default
 * 
 * To start loading a slot, call halStorageStartTransaction to acquire
 * a load/store transaction id. This is necessary to enable multitask access.
 * 
 * After loading the general config, load all virtual button configs
 * via halStorageLoadGetVBConfigs to have all necessary slot data.
 * 
 * Finally, if all data is loaded, call halStorageFinishTransaction to
 * free the storage access to the other tasks or the next call.
 * 
 * @see halStorageStartTransaction
 * @see halStorageFinishTransaction
 * @see halStorageLoadGetVBConfigs
 * @see hal_storage_load_action
 * @param navigate Action defining the next loaded slot (default or prev/next)
 * @param tid Transaction ID, which must match the one given by halStorageStartTransaction
 * @return ESP_OK if everything is fine, ESP_FAIL if the command was not successful
 * */
esp_err_t halStorageLoad(hal_storage_load_action navigate, uint32_t tid);


/** @brief Load a slot by a slot number (starting with 0)
 * 
 * This method loads a slot & saves the general config to the given
 * config struct pointer.
 * The slot is defined by the slotnumber, starting with 1.
 * If you load the slot number 0, the default slot is loaded
 * 
 * To start loading a slot, call halStorageStartTransaction to acquire
 * a load/store transaction id. This is necessary to enable multitask access.
 * 
 * After loading the general config, load all virtual button configs
 * via halStorageLoadGetVBConfigs to have all necessary slot data
 * 
 * Finally, if all data is loaded, call halStorageFinishTransaction to
 * free the storage access to the other tasks or the next call.
 * 
 * @see halStorageStartTransaction
 * @see halStorageFinishTransaction
 * @param slotnumber Number of the slot to be loaded
 * @param tid Transaction ID, which must match the one given by halStorageStartTransaction
 * @return ESP_OK if everything is fine, ESP_FAIL if the command was not successful (slot number not found)
 * */
esp_err_t halStorageLoadNumber(uint8_t slotnumber, uint32_t tid);


/** @brief Load a slot by a slot name
 * 
 * This method loads a slot & saves the general config to the given
 * config struct pointer.
 * The slot is defined by the slot name.
 * 
 * To start loading a slot, call halStorageStartTransaction to acquire
 * a load/store transaction id. This is necessary to enable multitask access.
 * 
 * After loading the general config, load all virtual button configs
 * via halStorageLoadGetVBConfigs to have all necessary slot data
 * 
 * Finally, if all data is loaded, call halStorageFinishTransaction to
 * free the storage access to the other tasks or the next call.
 * 
 * @see halStorageStartTransaction
 * @see halStorageFinishTransaction
 * @see halStorageLoadGetVBConfigs
 * @param slotname Name of the slot to be loaded
 * @param tid Transaction ID, which must match the one given by halStorageStartTransaction
 * @return ESP_OK if everything is fine, ESP_FAIL if the command was not successful (slot name not found)
 * */
esp_err_t halStorageLoadName(char *slotname, uint32_t tid);

/** @brief Load an IR command by name
 * 
 * This method loads an IR command from storage.
 * The slot is defined by the slot name.
 * 
 * To start loading a command, call halStorageStartTransaction to acquire
 * a load/store transaction id. This is necessary to enable multitask access.
 * Finally, if the command is loaded, call halStorageFinishTransaction to
 * free the storage access to the other tasks or the next call.
 * 
 * @see halStorageStartTransaction
 * @see halStorageFinishTransaction
 * @param cmdName Name of the slot to be loaded
 * @param tid Transaction ID, which must match the one given by halStorageStartTransaction
 * @param cfg Pointer to a general config struct, which will be used to load the slot into
 * @return ESP_OK if everything is fine, ESP_FAIL if the command was not successful (slot name not found)
 * */
esp_err_t halStorageLoadIR(char *cmdName, halIOIR_t *cfg, uint32_t tid);

/** @brief Delete one or all IR commands
 * 
 * This function is used to delete one IR command or all commands (depending on
 * parameter slotnr)
 * 
 * @param slotnr Number of slot to be deleted. Use 100 to delete all slots
 * @param tid Transaction id
 * @return ESP_OK if everything is fine, ESP_FAIL otherwise
 * */
esp_err_t halStorageDeleteIRCmd(uint8_t slotnr, uint32_t tid);


/** @brief Get the name of an infrared command stored at the given slot number
 * 
 * This method returns the name of the given slot number for IR commands.
 * An invalid slotnumber will return ESP_FAIL and an unchanged slotname
 * 
 * @param tid Transaction id
 * @param cmdName Memory to store the command name to. Attention: minimum length: SLOTNAME_LENGTH+1
 * @param slotnumber Number of slot to be loaded
 * @see halStorageStartTransaction
 * @see SLOTNAME_LENGTH
 * @see halStorageFinishTransaction
 * @return ESP_OK if tid is valid and slot number is valid, ESP_FAIL otherwise
 * */
esp_err_t halStorageGetNameForNumberIR(uint32_t tid, uint8_t slotnumber, char *cmdName);

/** @brief Store an infrared command to storage
 * 
 * This method stores a set of IR edges with a given length and a given
 * command name. 
 * If there is already a cmd with this given name, it is overwritten!
 * 
 * @param tid Transaction id
 * @param cfg Pointer to a IR config, can be freed after this call
 * @param cmdName Name of this IR command
 * @return ESP_OK on success, ESP_FAIL otherwise
 * @see halIOIR_t
 * @see halStorageLoadIR
 * */
esp_err_t halStorageStoreIR(uint32_t tid, halIOIR_t *cfg, char *cmdName);

/** @brief Get the number of an IR command
 * 
 * This method returns the number of the given IR command name.
 * An invalid name will return ESP_FAIL and a slotnumber of 0xFF.
 * 
 * @param tid Transaction id
 * @param cmdName Name of the IR command to look for
 * @param slotnumber Variable where the slot number will be stored
 * @see halStorageStartTransaction
 * @see halStorageFinishTransaction
 * @return ESP_OK if tid is valid and slot number is valid, ESP_FAIL otherwise
 * */
esp_err_t halStorageGetNumberForNameIR(uint32_t tid, uint8_t *slotnumber, char *cmdName);


/** @brief Get the number of stored IR commands
 * 
 * This method returns the number of available IR commands.
 * An empty device will return 0
 * 
 * @param tid Transaction id
 * @param slotsavailable Variable where the slot count will be stored
 * @see halStorageStartTransaction
 * @see halStorageFinishTransaction
 * @return ESP_OK if tid is valid and slot count is valid, ESP_FAIL otherwise
 * */
esp_err_t halStorageGetNumberOfIRCmds(uint32_t tid, uint8_t *slotsavailable);

/** @brief Load one virtual button config for currently loaded slot
 * 
 * This function is used to load one virtual button config after the
 * slot was loaded (either by name, number or action).
 * 
 * Please use the same transaction id as for loading the slot.
 * It is not possible to use the same tid after halStorageFinishTransaction
 * was called.
 * 
 * @see halStorageLoad
 * @see halStorageLoadName
 * @see halStorageLoadNumber
 * @param vb Number of virtual button config to be loaded
 * @param vb_config Pointer to config struct which holds the VB config data
 * @param vb_config_size Size of config which will be loaded (differs between functions)
 * @param tid Transaction ID, which must match the one given by halStorageStartTransaction
 * @return ESP_OK if everything is fine, ESP_FAIL if the command was not successful (no config found or no slot loaded)
 * */
esp_err_t halStorageLoadGetVBConfigs(uint8_t vb, void * vb_config, size_t vb_config_size, uint32_t tid);

/** @brief Start a storage transaction
 * 
 * This method is used to start a transaction and needs to be called
 * BEFORE any further storage access.
 * If this function is successful (within the given ticks, the storage
 * is getting free), a transaction ID is written, which should be used
 * in further storage access.
 * 
 * After finishing, a transaction is terminated by halStorageFinishTransaction.
 * 
 * @see halStorageFinishTransaction
 * @param tid Transaction if this command was successful, 0 if not.
 * @param tickstowait Maximum amount of ticks to wait for this command to be successful
 * @param caller Name of calling task, used to track storage access
 * @return ESP_OK if the tid is valid, ESP_FAIL if other tasks did not freed the access in time
 * */
esp_err_t halStorageStartTransaction(uint32_t *tid, TickType_t tickstowait, const char* caller);


/** @brief Finish a storage transaction
 * 
 * This method is used to stop a transaction and needs to be called
 * AFTER storage access, enabling other tasks to acquire the storage
 * module.
 * 
 * @see halStorageStartTransaction
 * @param tid Transaction to be finished
 * @return ESP_OK if the tid is freed, ESP_FAIL if the tid is not valid
 * */
esp_err_t halStorageFinishTransaction(uint32_t tid);

/** @brief Store a generalConfig_t struct
 * 
 * This method stores the general config for the given slotnumber.
 * A slot contains:
 * -) General Config
 * -) Slotname
 * -) Slotnumber (used for the filename)
 * -) MD5 sum (to check for corrupted content)
 * 
 * If there is already a slot with this given number, it is overwritten!
 * 
 * !! All following halStorageStoreSetVBConfigs (except the parameter
 * slotnumber is used there) are using this slotnumber
 * until halStorageFinishTransaction is called !!
 * 
 * @param tid Transaction id
 * @param cfg Pointer to general config, can be freed after this call
 * @param slotname Name of this slot
 * @param slotnumber Number where to store this slot
 * @return ESP_OK on success, ESP_FAIL otherwise
 * @see halStorageStoreSetVBConfigs
 * */
esp_err_t halStorageStore(uint32_t tid,generalConfig_t *cfg, char *slotname, uint8_t slotnumber);

/** @brief Store a virtual button config struct
 * 
 * This method stores the config structs for virtual buttons.
 * 
 * Configs are provided via generalConfig struct, which contains
 * necessary information:
 * * Size of config
 * * Pointer to current config
 * 
 * @param tid Transaction id
 * @param slotnumber Number of the slot on which this config is used. Use 0xFF to ignore and use
 * previous set slot number (by halStorageStore)
 * @param config Pointer to the general config struct (containing pointers/size to VB cfg)
 * @return ESP_OK on success, ESP_FAIL otherwise
 * @see halStorageStore
 * @see storageHeader_t
 * @warning size element of generalConfig_t MUST be set to have a successful write.
 * @note This method always overwrites an existing config!
 * */
esp_err_t halStorageStoreSetVBConfigs(uint8_t slotnumber, generalConfig_t *config, uint32_t tid);


/** @brief Create a new default slot
 * 
 * Create a new default slot in memory.
 * The default settings are hardcoded here to provide a fallback
 * solution.
 * 
 * @param tid Valid transaction ID
 * */
void halStorageCreateDefault(uint32_t tid);

/** @brief Delete one or all slots
 * 
 * This function is used to delete one slot or all slots (depending on
 * parameter slotnumber)
 * 
 * @param slotnr Number of slot to be deleted. Use -1 to delete all slots
 * @param tid Transaction id
 * @return ESP_OK if everything is fine, ESP_FAIL otherwise
 * */
esp_err_t halStorageDeleteSlot(int16_t slotnr, uint32_t tid);

#endif /*_HAL_STORAGE_H*/
