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
 * a storage area. In case of the ESP32 we use SPIFFS, which is provided by the esp-idf.
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
 * All methods called from this module will block via a semaphore until
 * the requested operation is finished.
 * 
 * For storing small amounts global, non-volatile data, it is possible
 * to use halStorageNVSLoad & halStorageNVSStore operations. In this case,
 * no transaction id is necessary.
 * 
 * Slots are stored in following naming convention (8.3 rule applies here):
 * general slot config:
 * xxx.set (slot number, e.g., 000.set for slot 1)
 * infrared commands
 * xxx_IR.set
 * 
 * @note Maximum number of slots: 250! (e.g. 000.set - 249.set)
 * @note Maximum number of IR commands: 250 (e.g. IR_000.set - IR_249.set)
 * @note Use halStorageStartTransaction and halStorageFinishTransaction on begin/end of loading&storing (except for halStorageNVS* operations)
 * @warning Adjust the esp-idf (via "make menuconfig") to use 512B sectors
 * and mode <b>safety</b>!
 * 
 */

#include "hal_storage.h"

#define LOG_TAG "hal_storage"
#define LOG_LEVEL_STORAGE ESP_LOG_DEBUG

/** @brief Mutex which is used to avoid multiple access to different loaded slots 
 * @see storageCurrentTID*/
SemaphoreHandle_t halStorageMutex = NULL;
/** @brief Currently active transaction ID, 0 is invalid. */
static uint32_t storageCurrentTID = 0;
/** @brief Currently activated slot number */
static uint8_t storageCurrentSlotNumber = 0;
/** @brief File handle currently used by store slot
 * To append AT commands to a slot, multiple calls of
 * halStorageStore are required. Consequently, this module needs to know
 * which file is appended. This is saved in this file handle, and freed
 * on halStorageFinishTransaction
 * */
static FILE *storeHandle = NULL;

/** @brief Partition name (used to define different memory types) */
const static char *base_path = "/spiffs";

/** @brief Current active storage task
 * 
 * This string is used to determine the currently active task, which
 * holds a tid.
 * @see storageCurrentTID
 * */
char storageCurrentTIDHolder[32];

/** @brief Load a string from NVS (global, no slot assignment)
 * 
 * This method is used to load a string from a non-volatile storage.
 * No TID is necessary, just call this function.
 * 
 * @param key Key to identify this value, same as used on store
 * @param string Buffer for string to be read from flash/eeprom (flash in ESP32)
 * @warning Provide sufficient buffer length, otherwise a CPU exception will happen!
 * @return ESP_OK on success, error codes according to nvs_get_str 
 * */
esp_err_t halStorageNVSLoadString(const char *key, char *string)
{
  nvs_handle my_handle;
  esp_err_t ret;
  size_t len;
  
  //we won't accept null pointers.
  if(key == NULL || string == NULL) return ESP_FAIL;

  // Open
  ret = nvs_open(HAL_STORAGE_NVS_NAMESPACE, NVS_READONLY, &my_handle);
  if (ret != ESP_OK) return ret;

  // Read
  ret = nvs_get_str(my_handle, key, NULL, &len);
  if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) return ret;
  ret = nvs_get_str(my_handle, key, string, &len);
  if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) return ret;
  
  // Close
  nvs_close(my_handle);
  return ESP_OK;
}

/** @brief Store a string into NVS (global, no slot assignment)
 * 
 * This method is used to store a string in a non-volatile storage.
 * No TID is necessary, just call this function.
 * 
 * @warning NVS is not as big as SPIFFS storage, use with care! (max ~10kB)
 * @param key Key to identify this value on read.
 * @param string String to be stored in flash/eeprom (flash in ESP32)
 * @return ESP_OK on success, error codes according to nvs_set_str 
 * */
esp_err_t halStorageNVSStoreString(const char *key, char *string)
{
  nvs_handle my_handle;
  esp_err_t ret;
  
  //we won't accept null pointers.
  if(key == NULL || string == NULL) return ESP_FAIL;

  // Open
  ret = nvs_open(HAL_STORAGE_NVS_NAMESPACE, NVS_READWRITE, &my_handle);
  if (ret != ESP_OK) return ret;

  // Write
  ret = nvs_set_str(my_handle, key, string);
  if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) return ret;
  
  // Commit changes to flash
  ret = nvs_commit(my_handle);
  if (ret != ESP_OK) return ret;
  
  // Close
  nvs_close(my_handle);
  return ESP_OK;
} 
 
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
esp_err_t halStorageGetFree(uint32_t *total, uint32_t *free)
{
  size_t tot,used;
  esp_err_t ret = esp_spiffs_info(NULL,&tot,&used);
  
  //cannot proceed...
  if(ret != ESP_OK) return ESP_FAIL;
  
  //save total to caller
  *total = tot;
  //save total - used bytes to caller
  *free = tot - used;
  //everything went fine.
  return ESP_OK;
}


/** @brief internal function to init the filesystem if handle is invalid 
 * @return ESP_OK on success, ESP_FAIL otherwise*/
esp_err_t halStorageInit(void)
{
  //set log level to given log level
  esp_log_level_set(LOG_TAG,LOG_LEVEL_STORAGE);
  
  esp_err_t ret;
  #if LOG_LEVEL_STORAGE >= ESP_LOG_DEBUG
  ESP_LOGI(LOG_TAG, "Mounting SPIFFS for storage");
  #endif
  
  // To mount device we need name of device partition, define base_path
  // and allow format partition in case if it is new one and was not formated before
  const esp_vfs_spiffs_conf_t mount_config = {
    .base_path = base_path,
    .max_files = 4,
    .format_if_mount_failed = true,
    .partition_label = NULL
  };
  ret = esp_vfs_spiffs_register(&mount_config);
  //return on an error
  if(ret != ESP_OK) { ESP_LOGE(LOG_TAG,"Error mounting SPIFFS"); return ret; }
  
  //initialize nvs
  ret = nvs_flash_init();
  
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
    // NVS partition was truncated and needs to be erased
    // Retry nvs_flash_init
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  return ret;
}

/** @brief Internal helper to check for a valid WL handle and the correct tid 
 * @see storageCurrentTID
 * @param tid Currently used TID
 * @return ESP_OK if all checks are valid, ESP_FAIL otherwise*/
esp_err_t halStorageChecks(uint32_t tid)
{
  //check if caller is allowed to call this function
  if(tid != storageCurrentTID)
  {
    ESP_LOGE(LOG_TAG,"Caller (id: %d) did not start (id: %d - %s) this \
      transaction, failed!",tid,storageCurrentTID,storageCurrentTIDHolder);
    return ESP_FAIL;
  }
  
  //SPIFFS is not mounted, trigger init
  if(esp_spiffs_mounted(NULL) == false)
  {
    ESP_LOGE(LOG_TAG,"Error initializing SPIFFS; cannot continue");
    return ESP_FAIL;
  }
  
  return ESP_OK;
}

/** @brief Create a new default slot
 * 
 * Copy the flashed default.set file to the working config.set file.
 * 
 * @param tid Valid transaction ID
 * */
void halStorageCreateDefault(uint32_t tid)
{
  char file[sizeof(base_path)+32];
  //check tid
  if(halStorageChecks(tid) != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Cannot create default config, checks failed");
    return;
  }
  
  //create filename string to open the default slot (either FABI or FLipMouse)
  #ifdef DEVICE_FLIPMOUSE
    sprintf(file,"%s/flip.set",base_path);
  #endif
  #ifdef DEVICE_FABI
    sprintf(file,"%s/fabi.set",base_path);
  #endif
  FILE *source = fopen(file, "rb");
  if(source == NULL)
  {
    ESP_LOGE(LOG_TAG,"Cannot open default file for factory reset!");
    return;
  }
  
  //current slot number used for filename
  int slotnr = -1;
  //target file pointer
  FILE *target = NULL;

  //credits:
  //https://stackoverflow.com/questions/1006797/tried-and-true-simple-file-copying-code-in-c#1010207
  char *buffer = malloc(512);
  if(buffer == NULL)
  {
    ESP_LOGE(LOG_TAG,"Cannot alloc buf for default slot");
    fclose(source);
    return;
  }

  //read file & open/close individual slot files
  while (fgets(buffer, 512, source) != NULL)
  {
    //if we get a new slot by receiving a string "Slot X:<name>"
    //we close the target file and open a new one.
    //need to do it with strncmp to have a prefix check
    if(strncmp("Slot", buffer, strlen("Slot")) == 0)
    {
      if(target != NULL) fclose(target);
      slotnr++;
      //create target filename (overwrite current config)
      sprintf(file,"%s/%03d.set",base_path,slotnr);
      target = fopen(file, "wb");
      if(target == NULL)
      {
        ESP_LOGE(LOG_TAG,"Cannot open target config file \"%s\" for factory reset!",file);
        free(buffer);
        if(source != NULL) fclose(source);
        return;
      }
    }
    
    //write the buffer to the new file
    if(fputs(buffer, target) == EOF)
    {
        ESP_LOGE(LOG_TAG,"write failed for default slot");
        free(buffer);
        fclose(source);
        fclose(target);
        return;
    }
  }
  
  ESP_LOGI(LOG_TAG,"Factory reset, copied default file over config");
  free(buffer);
  fclose(source);
  fclose(target);
}

/** @brief Get number of currently loaded slot (0-x)
 * 
 * An empty device will return 0, if a slot is loaded, numbering starts
 * with 0.
 * 
 * @see storageCurrentSlotNumber
 * @return Currently loaded slot number
 * */
uint8_t halStorageGetCurrentSlotNumber(void)
{
  return storageCurrentSlotNumber;
}

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
esp_err_t halStorageGetNumberOfSlots(uint32_t tid, uint8_t *slotsavailable)
{
  uint8_t currentSlot = 0;
  char file[sizeof(base_path)+32];
  struct stat st;

  if(halStorageChecks(tid) != ESP_OK) return ESP_FAIL;
  
  do {
    //create filename string to search if this slot is available
    sprintf(file,"%s/%03d.set",base_path,currentSlot);
    
    //open file for reading
    #if LOG_LEVEL_STORAGE >= ESP_LOG_DEBUG
    ESP_LOGD(LOG_TAG,"Opening file %s",file);
    #endif
    
    if (stat(file, &st) == 0) {
      //check next slot if file exists
      currentSlot++;
    } else {
      //if file doesn't exist, exit:
      ESP_LOGI(LOG_TAG,"Available slots: %u",currentSlot);
      *slotsavailable = currentSlot;
      return ESP_OK;
    }
    if(currentSlot == 250) break;
  } while(1);
  
  return ESP_FAIL;
}


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
esp_err_t halStorageGetNameForNumberIR(uint32_t tid, uint8_t slotnumber, char *cmdName)
{
  uint32_t slotnamelen = 0;
  char file[sizeof(base_path)+32];
  FILE *f;

  if(halStorageChecks(tid) != ESP_OK) return ESP_FAIL;
  
  //check for slot number
  if(slotnumber >= 250)
  {
    ESP_LOGE(LOG_TAG,"IR commands maximum: 250");
    return ESP_FAIL;
  }
  
  //create filename string to search if this slot is available
  sprintf(file,"%s/IR_%03d.set",base_path,slotnumber);
  
  //open file for reading
  #if LOG_LEVEL_STORAGE >= ESP_LOG_DEBUG
  ESP_LOGD(LOG_TAG,"Opening file %s",file);
  #endif
  f = fopen(file, "rb");
  
  if(f == NULL)
  {
    ESP_LOGW(LOG_TAG,"Invalid slot number %d, cannot load file",slotnumber);
    return ESP_FAIL;
  }
  
  fseek(f,0,SEEK_SET);

  //read slot name
  fread(&slotnamelen,sizeof(uint32_t),1,f);
  if(slotnamelen > SLOTNAME_LENGTH + 1)
  {
    ESP_LOGE(LOG_TAG,"IR name too long: %u",slotnamelen);
    fclose(f);
    return ESP_FAIL;
  }
  fread(cmdName,sizeof(char),slotnamelen+1,f);
  cmdName[slotnamelen] = '\0';
  #if LOG_LEVEL_STORAGE >= ESP_LOG_DEBUG
  ESP_LOGD(LOG_TAG,"IR name: %s, length %d",cmdName,slotnamelen);
  #endif
  
  //clean up & return
  if(f!=NULL) fclose(f);
  return ESP_OK;
}
/** @brief Delete one or all IR commands
 * 
 * This function is used to delete one IR command or all commands (depending on
 * parameter slotnr)
 * 
 * @param slotnr Number of slot to be deleted. Use -1 to delete all slots
 * @note Setting slotnr to -1 deletes all IR slots.
 * @param tid Transaction id
 * @return ESP_OK if everything is fine, ESP_FAIL otherwise
 * */
esp_err_t halStorageDeleteIRCmd(int16_t slotnr, uint32_t tid)
{
  char file[sizeof(base_path)+32];
  char filenew[sizeof(base_path)+32];
  int ret;
  
  if(slotnr > 250) 
  {
    ESP_LOGE(LOG_TAG,"Cannot delete IR, slotnr too high");
    return ESP_FAIL;
  }
  
  //delete by starting & ending at given slotnumber 
  uint8_t from = slotnr;
  uint8_t to = slotnr;
  
  //in case of deleting all slots, start at 0 and delete until the last slot
  if(slotnr == -1)
  {
    from = 0;
    if(halStorageGetNumberOfIRCmds(tid,&to) != ESP_OK)
    {
      ESP_LOGE(LOG_TAG,"Cannot get number of IR slots, cannot delete all");
      return ESP_FAIL;
    }
    //if we have 2 slots, we need to delete IR_000.set & IR_001.set -> decrement here
    to--;
  }
  
  //check for valid storage handle
  if(halStorageChecks(tid) != ESP_OK) return ESP_FAIL;
  
  //delete one or all slots
  struct stat st;
  for(uint8_t i = from; i<=to; i++)
  {
    sprintf(file,"%s/IR_%03d.set",base_path,i);
    if (stat(file, &st) == 0) {
        // Delete it if it exists
        unlink(file);
    }
    //not necessary, ESP32 uses preemption
    //taskYIELD();
  }
  
  //re-arrange all following slots (of course, only if not deleting all)
  if(slotnr != -1)
  {
    
    for(uint8_t i = to+1; i<=249; i++)
    {
      sprintf(file,"%s/IR_%03d.set",base_path,i);
      sprintf(filenew,"%s/IR_%03d.set",base_path,i-1);
      ret = rename(file,filenew);
      if(ret != 0)
      {
        ESP_LOGI(LOG_TAG,"Stopped renaming @ IR cmd %d",i);
        break;
      }
    }
  }
  if(slotnr == 250) 
  {
    ESP_LOGW(LOG_TAG,"Deleted all IR commands");
  } else {
    ESP_LOGI(LOG_TAG,"Deleted IR cmd %d",slotnr);
  }
  return ESP_OK;
}

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
esp_err_t halStorageGetNumberOfIRCmds(uint32_t tid, uint8_t *slotsavailable)
{
  uint8_t count = 0;
  char file[sizeof(base_path)+32];
  struct stat st;

  if(halStorageChecks(tid) != ESP_OK) return ESP_FAIL;
  
  do {
    //create filename string to search if this slot is available
    sprintf(file,"%s/IR_%03d.set",base_path,count);
    
    //open file for reading
    #if LOG_LEVEL_STORAGE >= ESP_LOG_DEBUG
    ESP_LOGD(LOG_TAG,"Opening file %s",file);
    #endif
    
    if (stat(file, &st) == 0) {
      //check next slot if file exists
      count++;
    } else {
      ESP_LOGI(LOG_TAG,"Available IR cmds: %u",count);
      *slotsavailable = count;
      return ESP_OK;
    }
    if(count == 250) break;
  } while(1);
  return ESP_FAIL;
}

/** @brief Get the number of first available slot for an IR command
 * 
 * This method returns the number of the first available IR command slot.
 * An empty device will return 0, a full device 250
 * 
 * @param tid Transaction id
 * @param slotavailable Variable where the free slot number will be stored
 * @see halStorageStartTransaction
 * @see halStorageFinishTransaction
 * @return ESP_OK if tid is valid and slot count is valid, ESP_FAIL otherwise (no free slot)
 * */
esp_err_t halStorageGetFreeIRCmdSlot(uint32_t tid, uint8_t *slotavailable)
{
  uint8_t current = 0;

  if(halStorageChecks(tid) != ESP_OK) return ESP_FAIL;
  
  if(halStorageGetNumberOfIRCmds(tid,&current) != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Cannot get number of active IR cmds");
    return ESP_FAIL;
  }
  
  if(current == 250)
  {
    ESP_LOGW(LOG_TAG,"No free IR slot");
    *slotavailable = 250;
    return ESP_FAIL;
  } else {
    *slotavailable = current;
  }
  
  return ESP_OK;
}

//credits:
//https://stackoverflow.com/questions/1515195/how-to-remove-n-or-t-from-a-given-string-in-c
void strip(char *s) {
  char *p2 = s;
  while(*s != '\0') {
    if(*s != '\r' && *s != '\t' && *s != '\n') {
      *p2++ = *s++;
    } else {
      ++s;
    }
  }
  *p2 = '\0';
}

/** @brief Get the name of a slot number
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
esp_err_t halStorageGetNameForNumber(uint32_t tid, uint8_t slotnumber, char *slotname)
{
  //file name buffer
  char file[sizeof(base_path)+32];
  //malloc a buffer for SLOTNAME_LENGTH + strlen("Slot XXX:")
  char *slotnamebuf = malloc(SLOTNAME_LENGTH+10);
  FILE *f;
  
  if(slotnamebuf == NULL)
  {
    ESP_LOGE(LOG_TAG,"Cannot alloc buffer for slotname");
    return ESP_FAIL;
  }

  if(halStorageChecks(tid) != ESP_OK) return ESP_FAIL;
  
  //create filename string to search if this slot is available
  sprintf(file,"%s/%03d.set",base_path,slotnumber);
  
  //open file for reading
  #if LOG_LEVEL_STORAGE >= ESP_LOG_DEBUG
  ESP_LOGD(LOG_TAG,"Opening file %s",file);
  #endif
  f = fopen(file, "rb");
  
  if(f == NULL)
  {
    ESP_LOGW(LOG_TAG,"Invalid file");
    free(slotnamebuf);
    return ESP_FAIL;
  }
  
  fseek(f,0,SEEK_SET);

  //read slot name
  fgets(slotnamebuf,SLOTNAME_LENGTH+10,f);
  //check if we have "Slot XXX:"
  if((strncmp(slotnamebuf,"Slot",strlen("Slot")) == 0) && (strpbrk(slotnamebuf,":") != NULL))
  {
    //if yes, strip "Slot..." & save to caller
    char *begin = strpbrk(slotnamebuf,":");
    //remove \n \r
    strip(begin);
    strncpy(slotname,begin+1,SLOTNAME_LENGTH);
  } else {
    //if no, config is invalid
    ESP_LOGE(LOG_TAG,"Missing \"Slot XXX:\" tag (%s)!",slotnamebuf);
    
    
    
    free(slotnamebuf);
    fclose(f);
    return ESP_FAIL;
  }
  
  #if LOG_LEVEL_STORAGE >= ESP_LOG_DEBUG
  ESP_LOGD(LOG_TAG,"Read slotname: %s",slotname);
  #endif
  
  //clean up & return
  if(f!=NULL) fclose(f);
  free(slotnamebuf);
  return ESP_OK;
}

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
esp_err_t halStorageGetNumberForName(uint32_t tid, uint8_t *slotnumber, char *slotname)
{
  uint8_t currentSlot = 0;
  char fileSlotName[SLOTNAME_LENGTH+4];

  if(halStorageChecks(tid) != ESP_OK) return ESP_FAIL;
  
  do {
    //get name for slot number
    if(halStorageGetNameForNumber(tid,currentSlot,fileSlotName) != ESP_OK)
    {
      *slotnumber = 0;
      ESP_LOGI(LOG_TAG,"Cannot find slot %s",slotname);
      return ESP_FAIL;
    }
    
    //compare parameter & file slotname
    if(strcmp(slotname, fileSlotName) == 0)
    {
      //found a slot
      *slotnumber = currentSlot;
      #if LOG_LEVEL_STORAGE >= ESP_LOG_DEBUG
      ESP_LOGD(LOG_TAG,"Found slot \"%s\" @%u",slotname,currentSlot);
      #endif
      return ESP_OK;
    }
    
    //go to next possible slot & clean up
    currentSlot++;
  } while(1);

  //we should never be here...
  return ESP_OK;
}

/** @brief Get the number for an IR cmd name
 * 
 * This method returns the number of the given IR command name.
 * An invalid name will return ESP_OK and a slotnumber of 0xFF.
 * 
 * @param tid Transaction id
 * @param cmdName Name of the IR command to look for
 * @param slotnumber Variable where the slot number will be stored
 * @see halStorageStartTransaction
 * @see halStorageFinishTransaction
 * @return ESP_OK if tid is valid and slot number is valid, ESP_FAIL otherwise
 * */
esp_err_t halStorageGetNumberForNameIR(uint32_t tid, uint8_t *slotnumber, char *cmdName)
{
  uint8_t currentSlot = 0;
  char fileSlotName[SLOTNAME_LENGTH+4];

  if(halStorageChecks(tid) != ESP_OK) return ESP_FAIL;
  
  do {
    //get name for slot number
    if(halStorageGetNameForNumberIR(tid,currentSlot,fileSlotName) != ESP_OK)
    {
      *slotnumber = 0;
      ESP_LOGI(LOG_TAG,"Cannot find IR cmd %s",cmdName);
      return ESP_FAIL;
    }
    
    //compare parameter & file slotname
    if(strcmp(cmdName, fileSlotName) == 0)
    {
      //found a slot
      *slotnumber = currentSlot;
      #if LOG_LEVEL_STORAGE >= ESP_LOG_DEBUG
      ESP_LOGD(LOG_TAG,"Found IR slot \"%s\" @%u",cmdName,currentSlot);
      #endif
      return ESP_OK;
    }
    
    //go to next possible slot & clean up
    currentSlot++;
  } while(1);

  //we should never be here...
  return ESP_FAIL;
}

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
esp_err_t halStorageLoad(hal_storage_load_action navigate, uint32_t tid)
{
  uint8_t slotCount = 0;
  esp_err_t ret;
  
  //fetch current slot count
  if(halStorageGetNumberOfSlots(tid, &slotCount) != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"cannot get number of available slots");
    return ESP_FAIL;
  }
  
  //check what we should load
  switch(navigate)
  {
    case NEXT:
      //check if we are at the last slot, if yes load first, increment otherwise
      if((slotCount-1) == storageCurrentSlotNumber) storageCurrentSlotNumber = 0;
      else storageCurrentSlotNumber++;
      ret = halStorageLoadNumber(storageCurrentSlotNumber,tid,0);
      //if NEXT does not succeed (because of a deleted slot or something else)
      //retry with storageCurrentSlotNumber = 1;
      if(ret != ESP_OK)
      {
        ESP_LOGI(LOG_TAG,"Resetting current slot number to 0");
        storageCurrentSlotNumber = 0;
        ret = halStorageLoadNumber(storageCurrentSlotNumber,tid,0);
      }
      return ret;
    break;
    case PREV:
      //check if we are at the first slot, if yes load last, decrement otherwise
      if(storageCurrentSlotNumber == 0) storageCurrentSlotNumber = slotCount - 1;
      else storageCurrentSlotNumber--;
      ret = halStorageLoadNumber(storageCurrentSlotNumber,tid,0);
      //if NEXT does not succeed (because of a deleted slot or something else)
      //retry with storageCurrentSlotNumber = 1;
      if(ret != ESP_OK)
      {
        ESP_LOGI(LOG_TAG,"Resetting current slot number to 0");
        storageCurrentSlotNumber = 0;
        ret = halStorageLoadNumber(storageCurrentSlotNumber,tid,0);
      }
      return ret;  
    break;
    case DEFAULT:
      storageCurrentSlotNumber = 0;
      return halStorageLoadNumber(0,tid,0);
    break;
    case RESTOREFACTORYSETTINGS:
    break;
    default:
      ESP_LOGE(LOG_TAG,"unknown navigate action in halStorageLoad!");
    break;
  }
  return ESP_OK;
}

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
 * @param outputSerial Either the loaded AT commands are sent to the command parser (== 0) or sent to the serial output
 * @note If sending to serial port, the slot name is printed as well ("Slot <number>:<name>).
 * @note If param outputserial is set to 1, the full config is printed. If set to 2, only slotnames are printed (used for "AT LI").
 * In addition, for compatibility reasons, AT LI outputs e.g. "Slot 1:mouse", AT LA outputs "Slot:mouse".
 * @return ESP_OK if everything is fine, ESP_FAIL if the command was not successful (slot number not found)
 * */
esp_err_t halStorageLoadNumber(uint8_t slotnumber, uint32_t tid, uint8_t outputSerial)
{
  char file[sizeof(base_path)+32];
  char slotname[SLOTNAME_LENGTH+10];
  
  if(halStorageChecks(tid) != ESP_OK) return ESP_FAIL;
  
  if(slotnumber >= 250) 
  {
    ESP_LOGE(LOG_TAG,"Slotnumber too high: %d (0-249)",slotnumber);
    return ESP_FAIL;
  }
  
  //file naming convention for general config: xxx.set
  //create filename from slotnumber
  sprintf(file,"%s/%03d.set",base_path,slotnumber);
  
  //open file for reading
  #if LOG_LEVEL_STORAGE >= ESP_LOG_DEBUG
  ESP_LOGD(LOG_TAG,"Opening file %s",file);
  #endif
  FILE *f = fopen(file, "rb");
  if(f == NULL)
  {
    //special case: requesting a default config which is not created on a fresh device
    if(slotnumber == 0)
    {
      ESP_LOGW(LOG_TAG,"no default config. creating one & retry");
      
      halStorageCreateDefault(tid);
      f = fopen(file, "rb");
      if(f == NULL) return ESP_FAIL;
    } else {
      ESP_LOGE(LOG_TAG,"cannot load requested slot number %u",slotnumber);
      return ESP_FAIL;
    }
  }
  
  fseek(f,0,SEEK_SET);

  /*++++ read slot name ++++*/
  fgets(slotname,SLOTNAME_LENGTH+10,f);
  strip(slotname);
  //check if we have "Slot XXX:"
  if((strncmp(slotname,"Slot",strlen("Slot")) == 0) && (strpbrk(slotname,":") != NULL))
  {
    //if yes, strip "Slot..." & save for logging
    char *begin = strpbrk(slotname,":");
    strncpy(slotname,begin+1,SLOTNAME_LENGTH);
    //output to serial, note that we need to have compatibility to v2.5:
    //"AT LI" -> "Slot 1:mouse"
    //"AT LA" -< "Slot:mouse"
    
    //get a new buffer
    char *serialout = malloc(strnlen(slotname,SLOTNAME_LENGTH)+10);
    if(serialout == NULL && outputSerial != 0)
    {
      ESP_LOGE(LOG_TAG,"Cannot malloc for serial output!");
    } else {
      switch(outputSerial)
      {
        case 1: //"AT LA"
          sprintf(serialout,"Slot:%s",slotname);
          if(halSerialSendUSBSerial(serialout,strnlen(serialout,SLOTNAME_LENGTH+10),10) == -1)
          {
            ESP_LOGE(LOG_TAG,"Buffer overflow on serial");
          }
          break;
        case 2: //"AT LI"
          sprintf(serialout,"Slot %d:%s",slotnumber+1,slotname);
          if(halSerialSendUSBSerial(serialout,strnlen(serialout,SLOTNAME_LENGTH+10),10) == -1)
          {
            ESP_LOGE(LOG_TAG,"Buffer overflow on serial");
          }
        break;
        case 0: //no output
        default: break;
      }
    }
    //free buffer
    if(serialout != NULL) free(serialout);
  } else {
    //if no, config is invalid
    ESP_LOGE(LOG_TAG,"Missing \"Slot XXX:\" tag (%s)!",slotname);
    fclose(f);
    return ESP_FAIL;
  }
  
  #if LOG_LEVEL_STORAGE >= ESP_LOG_DEBUG
  ESP_LOGD(LOG_TAG,"Read slotname: %s, length %d",slotname,strnlen(slotname,SLOTNAME_LENGTH));
  #endif

  /*++++ read each line as AT cmd ++++*/
  uint32_t cmdcount = 0;
  while(outputSerial != 2)
  {
    //allocate one line
    char *at = malloc(ATCMD_LENGTH);
    if(at == NULL)
    {
      //if allocate didn't work first time, delay & wait for other
      //tasks to process (& free memory).
      ESP_LOGW(LOG_TAG,"Cannot alloc mem for AT cmd line, waiting.");
      vTaskDelay(15);
      //retry....
      at = malloc(ATCMD_LENGTH);
      //if it didn't work the second time, handle it like an error.
      if(at == NULL)
      {
        ESP_LOGW(LOG_TAG,"Cannot alloc mem for AT cmd line, aborting!");
        fclose(f);
        return ESP_FAIL;
      }
    }
    //read line, if EOF is reached free unused buffer & break loop
    if(fgets(at,ATCMD_LENGTH,f) == NULL)
    {
        free(at);
        break;
    }
    
    //either we send to serial port (outputSerial != 0) or
    //feed the command to the halSerialATCmds queue, which is processed
    //by task_commands. In this case, we need an AT cmd struct with
    //a buffer that gets freed there after processing.
    //
    //Sending to serial port is used for outputting the config to the
    //serial port (GUI processing via C# GUI). In this case a char
    //buffer is given to halSerial, after sending on the serial port
    //(or an additional stream receiver -> WebGUI's websocket) it is freed.
    
    if(outputSerial == 0)
    {
      //create an atcmd struct
      atcmd_t cmd;
      cmd.buf = (uint8_t *)at;
      cmd.len = strnlen(at,ATCMD_LENGTH);
      
      //wait for an initialized queue
      uint32_t timeout = 0;
      while(halSerialATCmds == NULL)
      {
        vTaskDelay(100/portTICK_PERIOD_MS);
        timeout++;
        if(timeout == 30)
        {
          ESP_LOGE(LOG_TAG,"AT cmd queue is NULL, cannot send cmd");
          fclose(f);
          free(at);
          return ESP_FAIL;
        }
      }
      //send at cmd to queue
      if(xQueueSend(halSerialATCmds,(void*)&cmd,10) != pdTRUE)
      {
        ESP_LOGE(LOG_TAG,"AT cmd queue is full, cannot send cmd");
        free(at);
      } else {
        ///@note we cannot print buffer here, might be already freed by task_commands.c
        //remove \r \n for printing...
        //strip(at);
        //at[cmd.len] = 0;
        //ESP_LOGI(LOG_TAG,"[%d] %s",cmd.len,at);
      }
      cmdcount++;
    } else {
      //remove \r \n for printing...
      strip(at);
      //send data line to serial port
      if(halSerialSendUSBSerial(at,strnlen(at,ATCMD_LENGTH),100/portTICK_PERIOD_MS) == -1)
      {
        ESP_LOGE(LOG_TAG,"Buffer overflow on serial");
      } else {
        ESP_LOGI(LOG_TAG,"Sent serial config with len %d to queue: %s",strnlen(at,ATCMD_LENGTH),at);
      }
      cmdcount++;
      free(at);
    }
  }
  
  //// print out NVS keys to the serial/WS interface ////
  // we don't implement it in the normal loop, because these
  // settings are NOT in the config text file.
  // Therefore, we only need to output these values on request for
  // printing the whole config via serial/WS.
  if(outputSerial == 1)
  {
    char *outputstring = malloc(ATCMD_LENGTH+1);
    if(outputstring != NULL)
    { 
      //print out MQTT broker URL ("AT MH")
      sprintf(outputstring,"AT MQ ");
      esp_err_t ret = halStorageNVSLoadString(NVS_MQTT_BROKER,&outputstring[6]);
      if(ret == ESP_OK) halSerialSendUSBSerial(outputstring, \
        strnlen(outputstring,ATCMD_LENGTH),100/portTICK_PERIOD_MS);
      //print out MQTT delimiter ("AT ML")
      sprintf(outputstring,"AT ML ");
      ret = halStorageNVSLoadString(NVS_MQTT_DELIM,&outputstring[6]);
      if(ret == ESP_OK) halSerialSendUSBSerial(outputstring, \
        strnlen(outputstring,ATCMD_LENGTH),100/portTICK_PERIOD_MS);
      //print out Wifi station name ("AT WH")
      sprintf(outputstring,"AT WH ");
      ret = halStorageNVSLoadString(NVS_STATIONNAME,&outputstring[6]);
      if(ret == ESP_OK) halSerialSendUSBSerial(outputstring, \
        strnlen(outputstring,ATCMD_LENGTH),100/portTICK_PERIOD_MS);
      
      //clean up
      free(outputstring);
    } else ESP_LOGE(LOG_TAG,"Error allocating memory for NVS output!");
  }

  ESP_LOGI(LOG_TAG,"Loaded slot %s,nr: %d, %u commands",slotname,slotnumber,cmdcount);
  
  //save current slot number, if processed by parser
  if(outputSerial == 0) storageCurrentSlotNumber = slotnumber;
  //clean up
  fclose(f);
  
  return ESP_OK;
}


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
esp_err_t halStorageLoadName(char *slotname, uint32_t tid)
{
  uint8_t slotnumber = 0;
  if(halStorageGetNumberForName(tid, &slotnumber, slotname) != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Cannot find number for name: %s",slotname);
    return ESP_FAIL;
  }
  
  return halStorageLoadNumber(slotnumber,tid,0);
}

/** @brief Delete one or all slots
 * 
 * This function is used to delete one slot or all slots (depending on
 * parameter slotnr)
 * 
 * @param slotnr Number of slot to be deleted. Use -1 to delete all slots
 * @param tid Transaction id
 * @warning Deleting all slots, means that a complete factory reset is done. Maybe we change this in release versions.
 * @return ESP_OK if everything is fine, ESP_FAIL otherwise
 * */
esp_err_t halStorageDeleteSlot(int16_t slotnr, uint32_t tid)
{
  char file[sizeof(base_path)+32];
  char filenew[sizeof(base_path)+32];
  int ret;
  //delete by starting & ending at given slotnumber 
  uint8_t from;
  uint8_t to;
  
  //in case of deleting all slots, start at 1 and delete until 250
  if(slotnr == -1)
  {
    from = 0;
    if(halStorageGetNumberOfSlots(tid,&to) != ESP_OK)
    {
      ESP_LOGE(LOG_TAG,"Cannot get number of slots, cannot delete all");
      return ESP_FAIL;
    }
    //if we have 2 slots, we need to delete 000.set & 001.set -> decrement here
    to--;
  } else if(slotnr <= 250 && slotnr >= 0) {
    from = slotnr;
    to = slotnr;
  } else {
    ESP_LOGE(LOG_TAG,"delete parameter error, -1 to 250 is supported");
    return ESP_FAIL;
  }
  
  //check for valid storage handle
  if(halStorageChecks(tid) != ESP_OK) return ESP_FAIL;
  
  //delete one or all slots
  struct stat st;
  for(uint8_t i = from; i<=to; i++)
  {
    sprintf(file,"%s/%03d.set",base_path,i);
    if (stat(file, &st) == 0) {
        // Delete it if it exists
        unlink(file);
    }
    //not necessary, ESP32 uses preemption
    //taskYIELD();
  }
  
  //re-arrange all following slots (of course, only if not deleting all)
  if(slotnr != -1)
  {
    
    for(uint8_t i = to+1; i<=250; i++)
    {
      sprintf(file,"%s/%03d.set",base_path,i);
      sprintf(filenew,"%s/%03d.set",base_path,i-1);
      ret = rename(file,filenew);
      if(ret != 0)
      {
        ESP_LOGI(LOG_TAG,"Stopped renaming @ slot %d",i);
        break;
      }
      //not necessary, ESP32 uses preemption
      //taskYIELD();
    }
  }
  if(slotnr == -1)
  {
    ESP_LOGI(LOG_TAG,"Deleted all slots");
  } else {
    ESP_LOGI(LOG_TAG,"Deleted slot %d, renamed remaining",slotnr);
  }
  return ESP_OK;
}

/** @brief Store a slot
 * 
 * If there is already a slot with this given number, it is overwritten!
 * 
 * !! All following halStorageStoreSetVBConfigs (except the parameter
 * slotnumber is used there) are using this slotnumber
 * until halStorageFinishTransaction is called !!
 * 
 * @param tid Transaction id
 * @param cfgstring Pointer to AT command. On first call use slotname here.
 * @param slotnumber Number where to store this slot. Only used on first call!
 * @return ESP_OK on success, ESP_FAIL otherwise
 * @see halStorageStoreSetVBConfigs
 * */
esp_err_t halStorageStore(uint32_t tid, char *cfgstring, uint8_t slotnumber)
{
  char file[sizeof(base_path)+12];
  
  if(halStorageChecks(tid) != ESP_OK) return ESP_FAIL;
  
  if(storeHandle == NULL && slotnumber >= 250) 
  {
    ESP_LOGE(LOG_TAG,"Slotnumber too high: %d, 0-249",slotnumber);
    return ESP_FAIL;
  }
  
  //on first call, check if slot name length is lower than maximum
  if(storeHandle == NULL && strnlen(cfgstring,SLOTNAME_LENGTH) == SLOTNAME_LENGTH)
  {
    ESP_LOGE(LOG_TAG,"Slotname too long!");
    return ESP_FAIL;
  }
  
  //open file for writing, only if not already opened
  if(storeHandle == NULL)
  {
    //file naming convention for general config: xxx.set
    //create filename from slotnumber
    sprintf(file,"%s/%03d.set",base_path,slotnumber);
    
    #if LOG_LEVEL_STORAGE >= ESP_LOG_DEBUG
    ESP_LOGD(LOG_TAG,"Opening file %s",file);
    #endif
    storeHandle = fopen(file, "wb");
    if(storeHandle == NULL)
    {
      ESP_LOGE(LOG_TAG,"cannot open file for writing: %s",file);
      return ESP_FAIL;
    }
    
    //write slot name if freshly opened file
    char slotname[SLOTNAME_LENGTH+11];
    //we start numbering IN the config file with "1" -> increment given slot number
    sprintf(slotname,"Slot %d:%s",slotnumber+1,cfgstring);
    fputs(slotname,storeHandle);
    
    ///@todo not necessary anymore?
    //save current slot number to access the VB configs
    storageCurrentSlotNumber = slotnumber;
  } else {
    //file was opened on previous call, append AT cmds now.
    fputs(cfgstring,storeHandle);
  }
  return ESP_OK;
}

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
esp_err_t halStorageStoreIR(uint32_t tid, halIOIR_t *cfg, char *cmdName)
{
  char file[sizeof(base_path)+12];
  char nullterm = '\0';
  uint32_t namelen;

  //basic FS checks
  if(halStorageChecks(tid) != ESP_OK) return ESP_FAIL;
  
  if(strnlen(cmdName,SLOTNAME_LENGTH) == SLOTNAME_LENGTH)
  {
    ESP_LOGE(LOG_TAG,"CMD name too long!");
    return ESP_FAIL;
  }
  
  //check if name is already used.
  uint8_t cmdnumber = 0;
  if(halStorageGetNumberForNameIR(tid, &cmdnumber, cmdName) != ESP_OK)
  {
    if(halStorageGetFreeIRCmdSlot(tid,&cmdnumber) != ESP_OK)
    {
      ESP_LOGE(LOG_TAG,"Cannot get a free slot for IR cmd");
      return ESP_FAIL;
    } else {
      ESP_LOGI(LOG_TAG,"New IR slot @%d",cmdnumber);
    }
  } else {
    ESP_LOGI(LOG_TAG,"Overwriting @%d",cmdnumber);
  }
  
  //create filename from slotnumber
  sprintf(file,"%s/IR_%03d.set",base_path,cmdnumber);
  
  //open file for writing
  #if LOG_LEVEL_STORAGE >= ESP_LOG_DEBUG
  ESP_LOGD(LOG_TAG,"Opening file %s",file);
  #endif
  FILE *f = fopen(file, "wb");
  if(f == NULL)
  {
    ESP_LOGE(LOG_TAG,"cannot open file for writing: %s",file);
    return ESP_FAIL;
  }
  
  fseek(f,0,SEEK_SET);
  
  //write cmd name (size of string + 1x '\0' char)
  namelen = strnlen(cmdName,SLOTNAME_LENGTH);
  fwrite(&namelen,sizeof(uint32_t),1, f);
  fwrite(cmdName,sizeof(char),namelen, f);
  fwrite(&nullterm,sizeof(char),1, f);
  
  //write length of IR commands.
  fwrite(&cfg->count,sizeof(uint16_t),1, f);
  
  //write remaining IR command
  if(fwrite(cfg->buffer,sizeof(rmt_item32_t),cfg->count,f) != cfg->count)
  {
    //did not write a full config
    ESP_LOGE(LOG_TAG,"Error writing IR cmd");
    fclose(f);
    return ESP_FAIL;
  } else {
    ESP_LOGI(LOG_TAG,"Stored IR cmd %u (%s) with %u bytes payload (length %d)", \
      cmdnumber, cmdName, sizeof(rmt_item32_t)*cfg->count, cfg->count);
    #if LOG_LEVEL_STORAGE >= ESP_LOG_DEBUG
    ESP_LOG_BUFFER_HEXDUMP(LOG_TAG,cfg->buffer,sizeof(rmt_item32_t)*cfg->count,ESP_LOG_VERBOSE);
    #endif
  }
  
  //clean up
  fclose(f);
  return ESP_OK;
}

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
esp_err_t halStorageLoadIR(char *cmdName, halIOIR_t *cfg, uint32_t tid)
{
  uint8_t currentSlot = 0;
  uint32_t slotnamelen = 0;
  char file[sizeof(base_path)+32];
  char fileSlotName[SLOTNAME_LENGTH+4];
  FILE *f;
  
  //do some checks for file system
  if(halStorageChecks(tid) != ESP_OK) return ESP_FAIL;
  
  if(cfg == NULL) 
  {
    ESP_LOGE(LOG_TAG,"Cannot load IR command in to NULL cfg");
    return ESP_FAIL;
  }
  
  do {
    //create filename string to search if this slot is available
    sprintf(file,"%s/IR_%03d.set",base_path,currentSlot);
    
    //open file for reading
    #if LOG_LEVEL_STORAGE >= ESP_LOG_DEBUG
    ESP_LOGD(LOG_TAG,"Opening file %s",file);
    #endif
    f = fopen(file, "rb");

    //Currently: return ESP_FAIL for first not found file
    if(f == NULL)
    {
      ESP_LOGI(LOG_TAG,"Stopped at IR cmd number %u, didn't found the given name",currentSlot);
      return ESP_FAIL;
    }
    
    fseek(f,0,SEEK_SET);
  
    //read slot name
    fread(&slotnamelen,sizeof(uint32_t),1,f);
    if(slotnamelen > SLOTNAME_LENGTH + 1)
    {
      ESP_LOGE(LOG_TAG,"CMD name too long: %u",slotnamelen);
      fclose(f);
      return ESP_FAIL;
    }
    fread(fileSlotName,sizeof(char),slotnamelen+1,f);
    fileSlotName[slotnamelen] = '\0';
    #if LOG_LEVEL_STORAGE >= ESP_LOG_DEBUG
    ESP_LOGD(LOG_TAG,"Read cmdname: %s, length %d",fileSlotName,slotnamelen);
    #endif
      
    //compare parameter & file slotname
    if(strcmp(cmdName, fileSlotName) == 0)
    {
      //found a slot
      ESP_LOGI(LOG_TAG,"Found IR slot \"%s\" @%u",cmdName,currentSlot);
      //read length of recorded items
      uint16_t irlength = 0;
      fread(&irlength,sizeof(uint16_t),1,f);
      
      //allocate amount of IR edges.
      cfg->buffer = malloc(sizeof(rmt_item32_t)*irlength);
      
      if(cfg->buffer != NULL)
      {
          //read from file to buffer
          if(fread(cfg->buffer,sizeof(rmt_item32_t),irlength,f) != irlength)
          {
            //maybe EOF, didn't read as many bytes as requested
            ESP_LOGE(LOG_TAG,"Cannot read data from file");
            fclose(f);
            free(cfg->buffer);
            return ESP_FAIL;
          }
          //save length to struct as well
          cfg->count = irlength;
          
          //debug output
          ESP_LOG_BUFFER_HEXDUMP(LOG_TAG,cfg->buffer,sizeof(rmt_item32_t)*cfg->count,ESP_LOG_VERBOSE);
      } else {
        //didn't get a buffer pointer
        ESP_LOGE(LOG_TAG,"No memory for IR command");
        fclose(f);
        return ESP_FAIL;
      }
      
      //clean up / return
      fclose(f);
      return ESP_OK;
    }
    
    //go to next possible slot & clean up
    currentSlot++;
    fclose(f);
  } while(1);

  //we should never be here...
  if(f!=NULL) fclose(f);
  return ESP_OK;
}

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
esp_err_t halStorageStartTransaction(uint32_t *tid, TickType_t tickstowait, const char* caller)
{
  //check if mutex is initialized
  if(halStorageMutex == NULL)
  {
    //if not, try to initialize
    halStorageMutex = xSemaphoreCreateMutex();
    if(halStorageMutex == NULL)
    {
      ESP_LOGE(LOG_TAG,"Not sufficient memory to create mutex, cannot access!");
      return ESP_FAIL;
    }
  }
  
  //try to take the mutex
  if(xSemaphoreTake(halStorageMutex, tickstowait) == pdTRUE)
  {
    //TODO: should we do an additional check if storageCurrentTID is 0?
    //if successful, create a random tid & send to caller
    do {
      storageCurrentTID = rand();
    } while (storageCurrentTID == 0); //create random numbers until its not 0
    //save TID to calling task
    *tid = storageCurrentTID;
    //save caller's name for tracking
    strncpy(storageCurrentTIDHolder,caller,sizeof(storageCurrentTIDHolder));
    if(esp_spiffs_mounted(NULL) == false)
    {
      if(halStorageInit() != ESP_OK)
      {
        ESP_LOGE(LOG_TAG,"error halStorageInit");
        return ESP_FAIL;
      }
    }
    return ESP_OK;
  } else {
    //cannot obtain mutex, set tid to 0 and return
    *tid = 0;
    ESP_LOGW(LOG_TAG,"cannot obtain mutex, currently active: %s",storageCurrentTIDHolder);
    return ESP_FAIL;
  }
}


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
esp_err_t halStorageFinishTransaction(uint32_t tid)
{
  //check if mutex is initialized
  if(halStorageMutex == NULL)
  {
    //if not, ERROR
    ESP_LOGE(LOG_TAG,"Mutex is NULL, where did it go?");
    return ESP_FAIL;
  }
  
  //check if tid is valid
  if(tid == 0 || tid != storageCurrentTID)
  {
    ESP_LOGW(LOG_TAG,"Not a valid transaction id (%d). Currently active: %d/%s",\
      tid,storageCurrentTID,storageCurrentTIDHolder);
    return ESP_FAIL;
  }
  
  //if we have used a store file handle, close it.
  if(storeHandle != NULL)
  {
    fclose(storeHandle);
    storeHandle = NULL;
  }
  
  //reset caller & id
  storageCurrentTID = 0;
  strncpy(storageCurrentTIDHolder,"",2);
  
  //give mutex back
  xSemaphoreGive(halStorageMutex);
  
  return ESP_OK;
}
