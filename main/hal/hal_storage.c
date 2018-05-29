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
 * @see generalConfig_t
 */

#include "hal_storage.h"

#define LOG_TAG "hal_storage"

/** @brief Mutex which is used to avoid multiple access to different loaded slots 
 * @see storageCurrentTID*/
SemaphoreHandle_t halStorageMutex = NULL;
/** @brief Currently active transaction ID, 0 is invalid. */
static uint32_t storageCurrentTID = 0;
/** @brief Currently activated slot number */
static uint8_t storageCurrentSlotNumber = 0;
/** @brief Currently used VB number.
 * 
 * This number is used for halStorageStoreSetVBConfigs only.
 * 
 * @see halStorageStoreSetVBConfigs */
static uint8_t storageCurrentVBNumber = 0;

/** @brief Wear levelling handle */
static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;

/** @brief Partition name (used to define different memory types) */
const static char *base_path = "/spiflash";

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
 * @warning NVS is not as big as FAT storage, use with care! (max ~10kB)
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
  FATFS *fs;
  DWORD fre_clust, fre_sect, tot_sect;
  
  //thanks to Ivan Grokhotkov, according to:
  //https://github.com/espressif/esp-idf/issues/1660
  /* Get volume information and free clusters of drive 0 */
  int res = f_getfree("0:", &fre_clust, &fs);
  if(res == 0)
  {
    /* Get total sectors and free sectors */
    tot_sect = (fs->n_fatent - 2) * fs->csize;
    fre_sect = fre_clust * fs->csize;
    *free = fre_sect*512;
    *total = tot_sect*512;
    return ESP_OK;
  } else {
    return ESP_FAIL;
  }
}


/** @brief internal function to init the filesystem if handle is invalid 
 * @return ESP_OK on success, ESP_FAIL otherwise*/
esp_err_t halStorageInit(void)
{
  esp_err_t ret;
  ESP_LOGD(LOG_TAG, "Mounting FATFS for storage");
  // To mount device we need name of device partition, define base_path
  // and allow format partition in case if it is new one and was not formated before
  const esp_vfs_fat_mount_config_t mount_config = {
          .max_files = 4,
          .format_if_mount_failed = true
  };
  ret = esp_vfs_fat_spiflash_mount(base_path, "storage", &mount_config, &s_wl_handle);
  //return on an error
  if(ret != ESP_OK) { ESP_LOGE(LOG_TAG,"Error mounting FATFS"); return ret; }
  
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
    ESP_LOGE(LOG_TAG,"Caller did not start this transaction, failed!");
    return ESP_FAIL;
  }
  
  //FAT is not mounted, trigger init
  if(s_wl_handle == WL_INVALID_HANDLE)
  {
    ESP_LOGE(LOG_TAG,"Error initializing FAT; cannot continue");
    return ESP_FAIL;
  }
  
  return ESP_OK;
}

/** @brief Create a new default slot
 * 
 * Create a new default slot in memory.
 * The default settings are hardcoded here to provide a fallback
 * solution.
 * 
 * @param tid Valid transaction ID
 * @todo Change this default slot to usable settings
 * */
void halStorageCreateDefault(uint32_t tid)
{
  //check tid
  if(halStorageChecks(tid) != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Cannot create default config, checks failed");
    return;
  }
  
  //get storage from heap
  generalConfig_t *defaultCfg = (generalConfig_t *)malloc(sizeof(generalConfig_t));
  if(defaultCfg == NULL) ESP_LOGE(LOG_TAG,"CANNOT CREATE DEFAULT CONFIG, not enough memory");
  void* pConfig = NULL;
  esp_err_t ret;
  
  ESP_LOGW(LOG_TAG,"Creating new default config!");
  
  //fill up default slot
  defaultCfg->ble_active = 1;
  defaultCfg->usb_active = 1;
  defaultCfg->slotversion = STORAGE_ID;
  defaultCfg->locale = LAYOUT_GERMAN;
  defaultCfg->wheel_stepsize = 3;
  defaultCfg->irtimeout = 10;
  defaultCfg->button_learn = 0;
  
  //set any debounce time to 0 (use default)
  defaultCfg->debounce_press = 0;
  defaultCfg->debounce_release = 0;
  defaultCfg->debounce_idle = 0;
  for(uint8_t i = 0; i<NUMBER_VIRTUALBUTTONS*4;i++)
  {
    defaultCfg->debounce_press_vb[i] = 0;
    defaultCfg->debounce_release_vb[i] = 0;
    defaultCfg->debounce_idle_vb[i] = 0;
  }
  
  //default feedback mode: LED+buzzer
  defaultCfg->feedback = 3;
  
  
  strcpy(defaultCfg->slotName,"mouse");
  
  #ifdef DEVICE_FABI
    defaultCfg->deviceIdentifier = 1;
  #endif
  #ifdef DEVICE_FLIPMOUSE
    defaultCfg->deviceIdentifier = 0;
  #endif
  
  defaultCfg->adc.orientation = 0;
  defaultCfg->adc.acceleration = 50;
  defaultCfg->adc.axis = 0;
  defaultCfg->adc.deadzone_x = 40;
  defaultCfg->adc.deadzone_y = 40;
  defaultCfg->adc.max_speed = 50;
  defaultCfg->adc.mode = MOUSE;
  defaultCfg->adc.sensitivity_x = 60;
  defaultCfg->adc.sensitivity_y = 60;
  defaultCfg->adc.threshold_puff = 525;
  defaultCfg->adc.threshold_sip = 500;
  defaultCfg->adc.threshold_strongpuff = 800;
  defaultCfg->adc.threshold_strongsip = 250;
  defaultCfg->adc.reportraw = 0;
  defaultCfg->adc.gain[0] = 50;
  defaultCfg->adc.gain[1] = 50;
  defaultCfg->adc.gain[2] = 50;
  defaultCfg->adc.gain[3] = 50;
  
  
  //initialise all VBs as unused
  for(uint8_t i = 1; i<(NUMBER_VIRTUALBUTTONS*4); i++)
  {
    defaultCfg->virtualButtonCommand[i] = T_NOFUNCTION;
    //we don't need this pointer at all, because the relation
    //between VB and corresponding config is done in config_switcher
    defaultCfg->virtualButtonConfig[i] = NULL;
    defaultCfg->virtualButtonCfgSize[i] = 0;
  }
  
  #ifdef DEVICE_FLIPMOUSE
  
  //add VB functions and config sizes for default slot
  defaultCfg->virtualButtonCommand[VB_SIP] = T_MOUSE;
  defaultCfg->virtualButtonCfgSize[VB_SIP] = sizeof(taskMouseConfig_t);
  defaultCfg->virtualButtonCommand[VB_PUFF] = T_MOUSE;
  defaultCfg->virtualButtonCfgSize[VB_PUFF] = sizeof(taskMouseConfig_t);
  defaultCfg->virtualButtonCommand[VB_STRONGSIP] = T_CALIBRATE;
  defaultCfg->virtualButtonCfgSize[VB_STRONGSIP] = sizeof(taskNoParameterConfig_t);
  defaultCfg->virtualButtonCommand[VB_STRONGPUFF] = T_CONFIGCHANGE;
  defaultCfg->virtualButtonCfgSize[VB_STRONGPUFF] = sizeof(taskConfigSwitcherConfig_t);
  defaultCfg->virtualButtonCommand[VB_EXTERNAL1] = T_KEYBOARD;
  defaultCfg->virtualButtonCfgSize[VB_EXTERNAL1] = sizeof(taskKeyboardConfig_t);
  defaultCfg->virtualButtonCommand[VB_INTERNAL2] = T_CONFIGCHANGE;
  defaultCfg->virtualButtonCfgSize[VB_INTERNAL2] = sizeof(taskConfigSwitcherConfig_t);
  
  #endif
  
  
  #ifdef DEVICE_FABI
    ///@todo Add default VB config for FABI.
  #endif
  
  //store general config
  ret = halStorageStore(tid,defaultCfg,(char*)"mouse",0);
  if(ret != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Error saving default general config!");
    return;
  }
  
  #ifdef DEVICE_FLIPMOUSE
  
  pConfig = malloc(sizeof(taskConfigSwitcherConfig_t));
  if(pConfig != NULL)
  {
    ((taskConfigSwitcherConfig_t *)pConfig)->virtualButton = VB_INTERNAL2;
    memcpy(((taskConfigSwitcherConfig_t *)pConfig)->slotName,"__NEXT",strlen("__NEXT"));
    defaultCfg->virtualButtonConfig[VB_INTERNAL2] = pConfig;
  } else { ESP_LOGE(LOG_TAG,"malloc error VB%u",VB_INTERNAL2); return; }
  

  pConfig = malloc(sizeof(taskKeyboardConfig_t));
  if(pConfig != NULL)
  {
    ((taskKeyboardConfig_t *)pConfig)->type = PRESS_RELEASE_BUTTON;
    ((taskKeyboardConfig_t *)pConfig)->virtualButton = VB_EXTERNAL1;
    //key code for KEY_ESC
    ((taskKeyboardConfig_t *)pConfig)->keycodes_text[0] = 41;
    ((taskKeyboardConfig_t *)pConfig)->keycodes_text[1] = 0;
    defaultCfg->virtualButtonConfig[VB_EXTERNAL1] = pConfig;
  } else { ESP_LOGE(LOG_TAG,"malloc error VB%u",VB_EXTERNAL1); return; }
  
  pConfig = malloc(sizeof(taskMouseConfig_t));
  if(pConfig != NULL)
  {
    ((taskMouseConfig_t *)pConfig)->type = LEFT;
    ((taskMouseConfig_t *)pConfig)->actionparam = M_HOLD;
    ((taskMouseConfig_t *)pConfig)->virtualButton = VB_SIP;
    defaultCfg->virtualButtonConfig[VB_SIP] = pConfig;
  } else { ESP_LOGE(LOG_TAG,"malloc error VB%u",VB_SIP); return; }
  
  pConfig = malloc(sizeof(taskMouseConfig_t));
  if(pConfig != NULL)
  {
    ((taskMouseConfig_t *)pConfig)->type = RIGHT;
    ((taskMouseConfig_t *)pConfig)->actionparam = M_CLICK;
    ((taskMouseConfig_t *)pConfig)->virtualButton = VB_PUFF;
    defaultCfg->virtualButtonConfig[VB_PUFF] = pConfig;
  } else { ESP_LOGE(LOG_TAG,"malloc error VB%u",VB_PUFF); return; }
  
  pConfig = malloc(sizeof(taskNoParameterConfig_t));
  if(pConfig != NULL)
  {
    ((taskNoParameterConfig_t *)pConfig)->virtualButton = VB_STRONGSIP;
    defaultCfg->virtualButtonConfig[VB_STRONGSIP] = pConfig;
  } else { ESP_LOGE(LOG_TAG,"malloc error VB%u",VB_STRONGSIP); return; }
  
  pConfig = malloc(sizeof(taskConfigSwitcherConfig_t));
  if(pConfig != NULL)
  {
    ((taskConfigSwitcherConfig_t *)pConfig)->virtualButton = VB_STRONGPUFF;
    memcpy(((taskConfigSwitcherConfig_t *)pConfig)->slotName,"_NEXT",strlen("_NEXT"));
    defaultCfg->virtualButtonConfig[VB_STRONGPUFF] = pConfig;
  } else { ESP_LOGE(LOG_TAG,"malloc error VB%u",VB_STRONGPUFF); return; }
  
  #endif
  
  #ifdef DEVICE_FABI
    ///@todo Add default VB config for FABI.
  #endif
  
  //now save all VB configs
  ret = halStorageStoreSetVBConfigs(0, defaultCfg, tid);
  if(ret != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Error saving default VB configs");
  } else {
    ESP_LOGI(LOG_TAG,"Created new default slot");
  }
  
  //finally, free configs of VBs
  for(uint8_t i = 0; i<(NUMBER_VIRTUALBUTTONS * 4); i++)
  {
    if(defaultCfg->virtualButtonConfig[i] != NULL)
    {
      free(defaultCfg->virtualButtonConfig[i]);
    }
  }
  //finally finally, free general config
  free(defaultCfg);
}

/** @brief Get number of currently loaded slot
 * 
 * An empty device will return 0
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
  FILE *f;

  if(halStorageChecks(tid) != ESP_OK) return ESP_FAIL;
  
  do {
    //create filename string to search if this slot is available
    sprintf(file,"%s/%03d.fms",base_path,currentSlot);
    
    //open file for reading
    ESP_LOGD(LOG_TAG,"Opening file %s",file);
    
    f = fopen(file, "rb");
    
    //check if this file is available
    //TODO: should we re-arrange slots if one is deleted or should we
    //search through all 255 possible slots? (might be slow if the slot
    //is not found)
    //Currently: return number of last valid found slot
    if(f == NULL)
    {
      ESP_LOGD(LOG_TAG,"Available slots: %u",currentSlot);
      *slotsavailable = currentSlot;
      return ESP_OK;
    } else {
      fclose(f);
      currentSlot++;
    }
  } while(1);
  
  return ESP_OK;
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
  if(slotnumber >= 100)
  {
    ESP_LOGE(LOG_TAG,"IR commands maximum: 99");
    return ESP_FAIL;
  }
  
  //create filename string to search if this slot is available
  sprintf(file,"%s/IR_%02d.fms",base_path,slotnumber);
  
  //open file for reading
  ESP_LOGD(LOG_TAG,"Opening file %s",file);
  f = fopen(file, "rb");
  
  if(f == NULL)
  {
    ESP_LOGD(LOG_TAG,"Invalid slot number %d, cannot load file",slotnumber);
    return ESP_FAIL;
  }
  
  fseek(f,0,SEEK_SET);

  //read slot name
  fread(&slotnamelen,sizeof(uint32_t),1,f);
  if(slotnamelen > SLOTNAME_LENGTH + 1)
  {
    ESP_LOGE(LOG_TAG,"Slotname too long: %u",slotnamelen);
    fclose(f);
    return ESP_FAIL;
  }
  fread(cmdName,sizeof(char),slotnamelen+1,f);
  cmdName[slotnamelen] = '\0';
  ESP_LOGD(LOG_TAG,"Read slotname: %s, length %d",cmdName,slotnamelen);
  
  //clean up & return
  if(f!=NULL) fclose(f);
  return ESP_OK;
}
/** @brief Delete one or all IR commands
 * 
 * This function is used to delete one IR command or all commands (depending on
 * parameter slotnr)
 * 
 * @param slotnr Number of slot to be deleted. Use 100 to delete all slots
 * @note Setting slotnr to 100 deletes all IR slots.
 * @param tid Transaction id
 * @return ESP_OK if everything is fine, ESP_FAIL otherwise
 * */
esp_err_t halStorageDeleteIRCmd(uint8_t slotnr, uint32_t tid)
{
  char file[sizeof(base_path)+32];
  char filenew[sizeof(base_path)+32];
  int ret;
  
  if(slotnr > 100) 
  {
    ESP_LOGE(LOG_TAG,"Cannot delete IR, slotnr too high");
    return ESP_FAIL;
  }
  
  //delete by starting & ending at given slotnumber 
  uint8_t from = slotnr;
  uint8_t to = slotnr;
  
  //in case of deleting all slots, start at 1 and delete until 250
  if(slotnr == 100)
  {
    from = 0;
    to = 99;
  }
  
  //check for valid storage handle
  if(halStorageChecks(tid) != ESP_OK) return ESP_FAIL;
  
  //delete one or all slots
  for(uint8_t i = from; i<=to; i++)
  {
    sprintf(file,"%s/IR_%02d.fms",base_path,i); 
    remove(file);
    //not necessary, ESP32 uses preemption
    //taskYIELD();
  }
  
  //re-arrange all following slots (of course, only if not deleting all)
  if(slotnr != 100)
  {
    
    for(uint8_t i = to+1; i<=99; i++)
    {
      sprintf(file,"%s/IR_%02d.fms",base_path,i);
      sprintf(filenew,"%s/IR_%02d.fms",base_path,i-1);
      ret = rename(file,filenew);
      if(ret != 0)
      {
        ESP_LOGI(LOG_TAG,"Stopped renaming @ IR cmd %d",i);
        break;
      }
      //not necessary, ESP32 uses preemption
      //taskYIELD();
    }
  }
  
  ESP_LOGW(LOG_TAG,"Deleted IR cmd %d (100 means: delete all)",slotnr);
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
  uint8_t current = 0;
  uint8_t count = 0;
  char file[sizeof(base_path)+32];
  FILE *f;

  if(halStorageChecks(tid) != ESP_OK) return ESP_FAIL;
  
  do {
    //create filename string to search if this slot is available
    sprintf(file,"%s/IR_%02d.fms",base_path,current);
    
    f = fopen(file, "rb");
    
    //check if this file is available
    if(f != NULL)
    {
      //open file for reading
      ESP_LOGD(LOG_TAG,"Opening file %s",file);
      count++;
      fclose(f);
    }
    current++;
    if(current == 100) break;
  } while(1);
  
  ESP_LOGD(LOG_TAG,"Available IR cmds: %u",count);
  *slotsavailable = count;
  return ESP_OK;
}

/** @brief Get the number of first available slot for an IR command
 * 
 * This method returns the number of the first available IR command slot.
 * An empty device will return 0
 * 
 * @param tid Transaction id
 * @param slotavailable Variable where the slot number will be stored
 * @see halStorageStartTransaction
 * @see halStorageFinishTransaction
 * @return ESP_OK if tid is valid and slot count is valid, ESP_FAIL otherwise (no free slot)
 * */
esp_err_t halStorageGetFreeIRCmdSlot(uint32_t tid, uint8_t *slotavailable)
{
  uint8_t current = 0;
  char file[sizeof(base_path)+32];
  FILE *f;

  if(halStorageChecks(tid) != ESP_OK) return ESP_FAIL;
  
  do {
    //create filename string to search if this slot is available
    sprintf(file,"%s/IR_%02d.fms",base_path,current);
    
    //open file for reading
    ESP_LOGD(LOG_TAG,"Opening file %s",file);
    
    f = fopen(file, "rb");
    
    //check if this file is available
    if(f == NULL)
    {
      *slotavailable = current;
      return ESP_OK;
    } else fclose(f);
    current++;
    if(current == 100) break;
  } while(1);
  
  ESP_LOGD(LOG_TAG,"No free IR slot");
  return ESP_FAIL;
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
  uint32_t slotnamelen = 0;
  char file[sizeof(base_path)+32];
  FILE *f;

  if(halStorageChecks(tid) != ESP_OK) return ESP_FAIL;
  
  //create filename string to search if this slot is available
  sprintf(file,"%s/%03d.fms",base_path,slotnumber);
  
  //open file for reading
  ESP_LOGD(LOG_TAG,"Opening file %s",file);
  f = fopen(file, "rb");
  
  if(f == NULL)
  {
    ESP_LOGD(LOG_TAG,"Invalid slot number %d, cannot load file",slotnumber);
    return ESP_FAIL;
  }
  
  fseek(f,0,SEEK_SET);

  //read slot name
  fread(&slotnamelen,sizeof(uint32_t),1,f);
  if(slotnamelen > SLOTNAME_LENGTH + 1)
  {
    ESP_LOGE(LOG_TAG,"Slotname length too high: %u",slotnamelen);
    fclose(f);
    return ESP_FAIL;
  }
  fread(slotname,sizeof(char),slotnamelen+1,f);
  slotname[slotnamelen] = '\0';
  ESP_LOGD(LOG_TAG,"Read slotname: %s, length %d",slotname,slotnamelen);
  
  //clean up & return
  if(f!=NULL) fclose(f);
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
  uint32_t slotnamelen = 0;
  char file[sizeof(base_path)+32];
  char fileSlotName[SLOTNAME_LENGTH+4];
  FILE *f;

  if(halStorageChecks(tid) != ESP_OK) return ESP_FAIL;
  
  do {
    //create filename string to search if this slot is available
    sprintf(file,"%s/%03d.fms",base_path,currentSlot);
    
    //open file for reading
    ESP_LOGD(LOG_TAG,"Opening file %s",file);
    f = fopen(file, "rb");
    
    //check if this file is available
    //TODO: should we re-arrange slots if one is deleted or should we
    //search through all 255 possible slots? (might be slow if the slot
    //is not found)
    //Currently: return ESP_FAIL for first not found file
    if(f == NULL)
    {
      ESP_LOGD(LOG_TAG,"Stopped at slot number %u, didn't found the given name",currentSlot);
      *slotnumber = 0;
      return ESP_FAIL;
    }
    
    fseek(f,0,SEEK_SET);
  
    //read slot name
    fread(&slotnamelen,sizeof(uint32_t),1,f);
    if(slotnamelen > SLOTNAME_LENGTH + 1)
    {
      ESP_LOGE(LOG_TAG,"Slotname length too high: %u",slotnamelen);
      fclose(f);
      return ESP_FAIL;
    }
    fread(fileSlotName,sizeof(char),slotnamelen+1,f);
    fileSlotName[slotnamelen] = '\0';
    ESP_LOGD(LOG_TAG,"Read slotname: %s, length %d",slotname,slotnamelen);
      
    //compare parameter & file slotname
    if(strcmp(slotname, fileSlotName) == 0)
    {
      //found a slot
      *slotnumber = currentSlot;
      fclose(f);
      ESP_LOGD(LOG_TAG,"Found slot \"%s\" @%u",slotname,currentSlot);
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

/** @brief Get the number of an IR command
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
  uint32_t slotnamelen = 0;
  char file[sizeof(base_path)+32];
  char fileSlotName[SLOTNAME_LENGTH+4];
  FILE *f;

  if(halStorageChecks(tid) != ESP_OK) return ESP_FAIL;
  
  do {
    //check for limit of slots
    if(currentSlot == 100)
    {
      ESP_LOGI(LOG_TAG,"Finished search, didn't find name %s",cmdName);
      *slotnumber = 0xFF;
      return ESP_OK;
    }
    
    //create filename string to search if this slot is available
    sprintf(file,"%s/IR_%02d.fms",base_path,currentSlot);
    
    //open file for reading
    f = fopen(file, "rb");
    
    //check if this file is available
    //TODO: should we re-arrange slots if one is deleted or should we
    //search through all 255 possible slots? (might be slow if the slot
    //is not found)
    if(f == NULL)
    {
      currentSlot++;
      continue;
    }
    
    fseek(f,0,SEEK_SET);
  
    //read slot name
    fread(&slotnamelen,sizeof(uint32_t),1,f);
    if(slotnamelen > SLOTNAME_LENGTH + 1)
    {
      ESP_LOGE(LOG_TAG,"Slotname length too high: %u",slotnamelen);
      fclose(f);
      return ESP_FAIL;
    }
    fread(fileSlotName,sizeof(char),slotnamelen+1,f);
    fileSlotName[slotnamelen] = '\0';
    ESP_LOGD(LOG_TAG,"Read slotname: %s, length %d",fileSlotName,slotnamelen);
      
    //compare parameter & file slotname
    if(strcmp(cmdName, fileSlotName) == 0)
    {
      //found a slot
      *slotnumber = currentSlot;
      fclose(f);
      ESP_LOGD(LOG_TAG,"Found slot \"%s\" @%u",cmdName,currentSlot);
      return ESP_OK;
    }
    
    //go to next possible slot & clean up
    currentSlot++;
    fclose(f);
  } while(1);

  //we should never be here...
  if(f!=NULL) fclose(f);
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
 * @param cfg Pointer to a general config struct, which will be used to load the slot into
 * @return ESP_OK if everything is fine, ESP_FAIL if the command was not successful
 * */
esp_err_t halStorageLoad(hal_storage_load_action navigate, generalConfig_t *cfg, uint32_t tid)
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
      ret = halStorageLoadNumber(storageCurrentSlotNumber,cfg,tid);
      //if NEXT does not succeed (because of a deleted slot or something else)
      //retry with storageCurrentSlotNumber = 1;
      if(ret != ESP_OK)
      {
        ESP_LOGI(LOG_TAG,"Resetting current slot number to 0");
        storageCurrentSlotNumber = 0;
        ret = halStorageLoadNumber(storageCurrentSlotNumber,cfg,tid);
      }
      return ret;
    break;
    case PREV:
      //check if we are at the first slot, if yes load last, decrement otherwise
      if(storageCurrentSlotNumber == 0) storageCurrentSlotNumber = slotCount - 1;
      else storageCurrentSlotNumber--;
      ret = halStorageLoadNumber(storageCurrentSlotNumber,cfg,tid);
      //if NEXT does not succeed (because of a deleted slot or something else)
      //retry with storageCurrentSlotNumber = 1;
      if(ret != ESP_OK)
      {
        ESP_LOGI(LOG_TAG,"Resetting current slot number to 0");
        storageCurrentSlotNumber = 0;
        ret = halStorageLoadNumber(storageCurrentSlotNumber,cfg,tid);
      }
      return ret;  
    break;
    case DEFAULT:
      return halStorageLoadNumber(0,cfg,tid);
    break;
    case RESTOREFACTORYSETTINGS:
    break;
    default:
      ESP_LOGE(LOG_TAG,"unknown navigate action in halStorageLoad!");
    break;
  }
  return ESP_OK;
}


/** @brief Load a slot by a slot number (starting with 1, 0 is default slot)
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
 * @see halStorageLoadGetVBConfigs
 * @param slotnumber Number of the slot to be loaded
 * @param tid Transaction ID, which must match the one given by halStorageStartTransaction
 * @param cfg Pointer to a general config struct, which will be used to load the slot into
 * @return ESP_OK if everything is fine, ESP_FAIL if the command was not successful (slot number not found)
 * */
esp_err_t halStorageLoadNumber(uint8_t slotnumber, generalConfig_t *cfg, uint32_t tid)
{
  char file[sizeof(base_path)+32];
  uint32_t slotnamelen;
  unsigned char md5sumfile[16], md5sumstruct[16];
  char *slotname = (char *)malloc(SLOTNAME_LENGTH+1);
  
  if(halStorageChecks(tid) != ESP_OK) return ESP_FAIL;
  
  if(slotnumber > 250) 
  {
    ESP_LOGE(LOG_TAG,"Slotnumber too high: %d, maximum 250",slotnumber);
    return ESP_FAIL;
  }
  
  //file naming convention for general config: xxx.fms
  //each general config file contains:
  //Name string (\0 terminated)
  //16Bytes of MD5 checksum (calculated over general config, no slot name)
  //XXXBytes of generalConfig_t struct
  
  //create filename from slotnumber
  sprintf(file,"%s/%03d.fms",base_path,slotnumber);
  
  //open file for reading
  ESP_LOGD(LOG_TAG,"Opening file %s",file);
  FILE *f = fopen(file, "rb");
  if(f == NULL)
  {
    //special case: requesting a default config which is not created on a fresh device
    if(slotnumber == 0)
    {
      ESP_LOGW(LOG_TAG,"no default config. creating one & retry");
      
      halStorageCreateDefault(tid);
      f = fopen(file, "rb");
      if(f == NULL)
      {
        free(slotname);
        return ESP_FAIL;
      }
    } else {
      ESP_LOGE(LOG_TAG,"cannot load requested slot number %u",slotnumber);
      free(slotname);
      return ESP_FAIL;
    }
  }
  
  fseek(f,0,SEEK_SET);
  
  //skip slot name
  fread(&slotnamelen,sizeof(uint32_t),1,f);
  if(slotnamelen > SLOTNAME_LENGTH + 1)
  {
    ESP_LOGE(LOG_TAG,"Slotname length too high: %u",slotnamelen);
    free(slotname);
    fclose(f);
    return ESP_FAIL;
  }
  fread(slotname,sizeof(char),slotnamelen+1,f);
  slotname[slotnamelen] = '\0';
  ESP_LOGD(LOG_TAG,"Read slotname: %s, length %d",slotname,slotnamelen);
  
  //get MD5sum of saved slot
  if(fread(md5sumfile,sizeof(md5sumfile),1,f) != 1)
  {
    //did not get a full checksum, maybe EOF.
    ESP_LOGE(LOG_TAG,"cannot read MD5 sum");
    fclose(f);
    free(slotname);
    return ESP_FAIL;
  } else {
    ESP_LOGD(LOG_TAG,"MD5 hash:");
    ESP_LOG_BUFFER_HEXDUMP(LOG_TAG,md5sumfile,sizeof(md5sumfile),ESP_LOG_DEBUG);
  }
  
  //read remaining general config
  if(fread(cfg,sizeof(generalConfig_t),1,f) != 1)
  {
    //did not get a full config, maybe EOF.
    ESP_LOGE(LOG_TAG,"cannot read general config, feof: %d",feof(f));
    ESP_LOG_BUFFER_HEXDUMP(LOG_TAG,cfg,sizeof(generalConfig_t),ESP_LOG_VERBOSE);
    fclose(f);
    free(slotname);
    return ESP_FAIL;
  }
  
  //TODO: check slot storage version, upgrade on any difference
  
  
  
  //compare checksums
  mbedtls_md5((unsigned char *)cfg, sizeof(generalConfig_t), md5sumstruct);
  if(memcmp(md5sumfile, md5sumstruct, sizeof(md5sumfile)) != 0)
  {
    //error comparing md5 sum...
    ESP_LOGE(LOG_TAG,"MD5 checksum mismatch, maybe slot is corrupted:");
    ESP_LOG_BUFFER_HEXDUMP(LOG_TAG,md5sumfile,sizeof(md5sumfile),ESP_LOG_DEBUG);
    ESP_LOG_BUFFER_HEXDUMP(LOG_TAG,md5sumstruct,sizeof(md5sumstruct),ESP_LOG_DEBUG);
    fclose(f);
    free(slotname);
    return ESP_FAIL;
  }
  
  ESP_LOGI(LOG_TAG,"Loaded slot %s,nr: %d, payload: %u bytes",slotname,slotnumber, sizeof(generalConfig_t));
  ESP_LOG_BUFFER_HEXDUMP(LOG_TAG,cfg,sizeof(generalConfig_t),ESP_LOG_VERBOSE);
  
  //clean up
  free(slotname);
  fclose(f);
  
  //save current slot number to access the VB configs
  storageCurrentSlotNumber = slotnumber;
  //save name of slot
  strncpy(cfg->slotName,slotname,SLOTNAME_LENGTH);
  
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
 * @param cfg Pointer to a general config struct, which will be used to load the slot into
 * @return ESP_OK if everything is fine, ESP_FAIL if the command was not successful (slot name not found)
 * */
esp_err_t halStorageLoadName(char *slotname, generalConfig_t *cfg, uint32_t tid)
{
  uint8_t slotnumber = 0;
  if(halStorageGetNumberForName(tid, &slotnumber, slotname) != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Cannot find number for name: %s",slotname);
    return ESP_FAIL;
  }
  
  return halStorageLoadNumber(slotnumber,cfg,tid);
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
  FILE *f;
  //delete by starting & ending at given slotnumber 
  uint8_t from;
  uint8_t to;
  
  //in case of deleting all slots, start at 1 and delete until 250
  if(slotnr == -1)
  {
    from = 0;
    to = 250;
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
  for(uint8_t i = from; i<=to; i++)
  {
    sprintf(file,"%s/%03d.fms",base_path,i); 
    f = fopen(file, "rb");
    if(f!=NULL)
    {
      fclose(f);
      remove(file);
      f = NULL;
    }
    sprintf(file,"%s/%03d_VB.fms",base_path,i); 
    f = fopen(file, "rb");
    if(f!=NULL)
    {
      fclose(f);
      remove(file);
      f = NULL;
    }
    //not necessary, ESP32 uses preemption
    //taskYIELD();
  }
  
  //re-arrange all following slots (of course, only if not deleting all)
  if(slotnr != -1)
  {
    
    for(uint8_t i = to+1; i<=250; i++)
    {
      sprintf(file,"%s/%03d.fms",base_path,i);
      sprintf(filenew,"%s/%03d.fms",base_path,i-1);
      ret = rename(file,filenew);
      if(ret != 0)
      {
        ESP_LOGI(LOG_TAG,"Stopped renaming @ slot %d",i);
        break;
      }
      sprintf(file,"%s/%03d_VB.fms",base_path,i); 
      sprintf(filenew,"%s/%03d_VB.fms",base_path,i-1); 
      rename(file,filenew);
      if(ret != 0)
      {
        ESP_LOGI(LOG_TAG,"Stopped renaming @ slot %d VBs",i);
        break;
      }
      //not necessary, ESP32 uses preemption
      //taskYIELD();
    }
  }

  ESP_LOGI(LOG_TAG,"Deleted slot %d (-1 means delete all), renamed remaining",slotnr);
  return ESP_OK;
}

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
 * @param vb_config_size Size of config which will be loaded (differs between types of VB actions).
 * @param tid Transaction ID, which must match the one given by halStorageStartTransaction
 * @return ESP_OK if everything is fine, ESP_FAIL if the command was not successful (no config found or no slot loaded)
 * */
esp_err_t halStorageLoadGetVBConfigs(uint8_t vb, void * vb_config, size_t vb_config_size, uint32_t tid)
{
  char file[sizeof(base_path)+32];
  static int16_t currentlyOpened = -1;
  static FILE *f = NULL;
  
  if(halStorageChecks(tid) != ESP_OK) return ESP_FAIL;
  
  if(vb >= (NUMBER_VIRTUALBUTTONS*4)) 
  {
    ESP_LOGE(LOG_TAG,"VB number too high: %u, maximum %u",vb,(NUMBER_VIRTUALBUTTONS*4));
    return ESP_FAIL;
  }
  
  //file naming convention for general config: xxx.fms
  //file naming convention for VB configs: xxx_VB.fms
  
  //open file for reading
  if(currentlyOpened != vb)
  {
    //if previously opened file handler is active
    if(f != NULL) fclose(f);
    
    //create filename from slotnumber & vb number
    sprintf(file,"%s/%03d_VB.fms",base_path,storageCurrentSlotNumber);
    ESP_LOGD(LOG_TAG,"Opening file %s",file);
    f = fopen(file, "rb");
    if(f == NULL)
    {
      ESP_LOGE(LOG_TAG,"cannot open file for reading: %s",file);
      return ESP_FAIL;
    }
  } else {
    //if we can re-use the file handler, seek to beginning
    fseek(f,0,SEEK_SET);
  }
  
  storageHeader_t hdr;
  
  //iterate until VB BEFORE given VB is reached
  //  actual data is read afterwards, we need just the headers
  //  via fseek, we will be at needed header after this loop.
  for(uint8_t i = 0; i<vb; i++)
  {
    //read header
    if(fread(&hdr, sizeof(storageHeader_t), 1, f) != 1)
    {
      ESP_LOGE(LOG_TAG,"Error reading VB header %d from %s",i,file);
      fclose(f);
      return ESP_FAIL;
    }
    //set new offset (via fseek)
    if(fseek(f,hdr.offsetNext,SEEK_SET) != 0)
    {
      ESP_LOGE(LOG_TAG,"Error seeking to VB header %d from %s",i+1,file);
      fclose(f);
      return ESP_FAIL;
    }
  }
  
  //read necessary header for given VB
  if(fread(&hdr, sizeof(storageHeader_t), 1, f) != 1)
  {
    ESP_LOGE(LOG_TAG,"Error reading VB header %d from %s",vb,file);
    fclose(f);
    return ESP_FAIL;
  }
  
  //check if given size equals header sizes
  size_t vblength = hdr.offsetNext - hdr.offsetThis - sizeof(storageHeader_t);
  if(vblength != vb_config_size)
  {
    ESP_LOGE(LOG_TAG,"Given size does not match header size: %d vs %d", \
      vblength,vb_config_size);
    fclose(f);
    return ESP_FAIL;
  }
  
  //we are now at config
  // read to given pointer
  if(fread(vb_config, vblength, 1, f) != 1)
  {
    ESP_LOGE(LOG_TAG,"Error reading VB config (size %d) from %s",vblength,file);
    fclose(f);
    return ESP_FAIL;
  }
  
  ESP_LOGD(LOG_TAG,"Successfully loaded slotnumber %d, VB%d, payload: %d",storageCurrentSlotNumber,vb,vb_config_size);
  //if you need detailed output of loaded VB configs.
  #if 0
  ESP_LOG_BUFFER_HEXDUMP(LOG_TAG,vb_config,vb_config_size,ESP_LOG_DEBUG);
  #endif
  return ESP_OK;
}

/** @brief Store a generalConfig_t struct
 * 
 * This method stores the general config for the given slotnumber.
 * A slot contains: <br>
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
esp_err_t halStorageStore(uint32_t tid, generalConfig_t *cfg, char *slotname, uint8_t slotnumber)
{
  char file[sizeof(base_path)+12];
  char nullterm = '\0';
  uint32_t namelen;
  unsigned char md5sumfile[16];
  
  if(halStorageChecks(tid) != ESP_OK) return ESP_FAIL;
  
  if(slotnumber > 250) 
  {
    ESP_LOGE(LOG_TAG,"Slotnumber too high: %d, maximum 250",slotnumber);
    return ESP_FAIL;
  }
  
  if(strnlen(slotname,SLOTNAME_LENGTH) == SLOTNAME_LENGTH)
  {
    ESP_LOGE(LOG_TAG,"Slotname too long!");
    return ESP_FAIL;
  }
  
  //file naming convention for general config: xxx.fms
  //each general config file contains:
  //Name string (\0 terminated)
  //16Bytes of MD5 checksum (calculated over general config, no slot name)
  //XXXBytes of generalConfig_t struct
  
  //create filename from slotnumber
  sprintf(file,"%s/%03d.fms",base_path,slotnumber);
  
  //open file for writing
  ESP_LOGD(LOG_TAG,"Opening file %s",file);
  FILE *f = fopen(file, "wb");
  if(f == NULL)
  {
    ESP_LOGE(LOG_TAG,"cannot open file for writing: %s",file);
    return ESP_FAIL;
  }
  
  fseek(f,0,SEEK_SET);
  
  //write slot name (size of string + 1x '\0' char)
  namelen = strnlen(slotname,SLOTNAME_LENGTH);
  fwrite(&namelen,sizeof(uint32_t),1, f);
  fwrite(slotname,sizeof(char),namelen, f);
  fwrite(&nullterm,sizeof(char),1, f);
    
  //store slot name to general config
  strcpy(cfg->slotName,slotname);
  
  //write MD5sum of saved slot
  mbedtls_md5((unsigned char *)cfg, sizeof(generalConfig_t), md5sumfile);
  if(fwrite(md5sumfile,sizeof(unsigned char),16,f) != 16)
  {
    //did not write a full checksum
    ESP_LOGE(LOG_TAG,"Error writing checksum");
    fclose(f);
    return ESP_FAIL;
  } else {
    ESP_LOGD(LOG_TAG,"Written MD5 checksum:");
    ESP_LOG_BUFFER_HEXDUMP(LOG_TAG,md5sumfile,sizeof(md5sumfile),ESP_LOG_DEBUG);
  }
  
  //write remaining general config
  if(fwrite(cfg,sizeof(generalConfig_t),1,f) != 1)
  {
    //did not write a full config
    ESP_LOGE(LOG_TAG,"Error writing general config");
    fclose(f);
    return ESP_FAIL;
  } else {
    ESP_LOGD(LOG_TAG,"Successfully stored slotnumber %u with %u bytes payload", slotnumber, sizeof(generalConfig_t));
    ESP_LOG_BUFFER_HEXDUMP(LOG_TAG,cfg,sizeof(generalConfig_t),ESP_LOG_VERBOSE);
  }
  
  //clean up
  fclose(f);
  
  //save current slot number to access the VB configs
  storageCurrentSlotNumber = slotnumber;
  
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
    ESP_LOGE(LOG_TAG,"Cannot check if name is already used");
    return ESP_FAIL;
  } else {
    //if no slot with this name is found, find a free one.
    if(cmdnumber == 0xFF)
    {
      if(halStorageGetFreeIRCmdSlot(tid,&cmdnumber) != ESP_OK)
      {
        ESP_LOGE(LOG_TAG,"Cannot get a free slot for IR cmd");
        return ESP_FAIL;
      } else {
        ESP_LOGD(LOG_TAG,"New IR slot @%d",cmdnumber);
      }
    }
    ESP_LOGD(LOG_TAG,"Overwriting @%d",cmdnumber);
  }
  
  //create filename from slotnumber
  sprintf(file,"%s/IR_%02d.fms",base_path,cmdnumber);
  
  //open file for writing
  ESP_LOGD(LOG_TAG,"Opening file %s",file);
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
    ESP_LOGI(LOG_TAG,"Successfully stored IR cmd %u (%s) with %u bytes payload (length %d)", \
      cmdnumber, cmdName, sizeof(rmt_item32_t)*cfg->count, cfg->count);
    ESP_LOG_BUFFER_HEXDUMP(LOG_TAG,cfg->buffer,sizeof(rmt_item32_t)*cfg->count,ESP_LOG_VERBOSE);
  }
  
  //clean up
  fclose(f);
  return ESP_OK;
}

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
esp_err_t halStorageStoreSetVBConfigs(uint8_t slotnumber, generalConfig_t *config, uint32_t tid)
{
  char file[sizeof(base_path)+32];
  
  if(halStorageChecks(tid) != ESP_OK) return ESP_FAIL;
  
  if(config == NULL)
  {
    ESP_LOGE(LOG_TAG,"Generalconfig == NULL!");
    return ESP_FAIL;
  }
  
  //check if we should ignore the slotnumber and take the previously used one
  if(slotnumber == 0xFF) slotnumber = storageCurrentSlotNumber;
  
  //check if the slotnumber is out of range
  if(slotnumber > 250) 
  {
    ESP_LOGE(LOG_TAG,"Slotnumber too high: %u, maximum 250",slotnumber);
    return ESP_FAIL;
  }
  
  //create filename from slotnumber
  sprintf(file,"%s/%03d_VB.fms",base_path,slotnumber);
  
  //open file for writing
  ESP_LOGD(LOG_TAG,"Opening file %s",file);
  FILE *f = fopen(file, "wb");
  if(f == NULL)
  {
    ESP_LOGE(LOG_TAG,"cannot open file for writing: %s",file);
    return ESP_FAIL;
  }

  //header for storing a VB
  storageHeader_t hdr =
  {
    .offsetNext = 0,
    .offsetPrev = 0,
    .offsetThis = 0,
    .vb = 0
  };
  
  //store each VB with header
  for(uint8_t i = 0; i<(NUMBER_VIRTUALBUTTONS * 4); i++)
  {
    //header: previous offset is former current offset
    hdr.offsetPrev = hdr.offsetThis;
    //header: this offset is the next offset of the previous header
    hdr.offsetThis = hdr.offsetNext;
    //header: offset for next is current offset + header size + current size
    hdr.offsetNext = hdr.offsetThis + sizeof(storageHeader_t) + config->virtualButtonCfgSize[i];
    //header. current VB
    hdr.vb = i;
    
    //save header
    if(fwrite(&hdr, sizeof(storageHeader_t), 1, f) != 1)
    {
      ESP_LOGE(LOG_TAG,"Error writing header %d in %s",i,file);
      fclose(f);
      return ESP_FAIL;
    }
    //check if config for vb is valid
    if(config->virtualButtonConfig[i] == NULL || config->virtualButtonCfgSize[i] == 0)
    {
      ESP_LOGW(LOG_TAG,"VB %d cfg not set, normally unused. (size %d, pointer %d)",i, \
        config->virtualButtonCfgSize[i], (uint32_t) config->virtualButtonConfig[i]);
      continue;
    }
    //save config itself
    if(fwrite(config->virtualButtonConfig[i], config->virtualButtonCfgSize[i], 1, f) != 1)
    {
      ESP_LOGE(LOG_TAG,"Error writing config %d in %s",i,file);
      fclose(f);
      return ESP_FAIL;
    }
    //if you need detailed output of stored VB configs.
    #if 0
    ESP_LOGD(LOG_TAG,"VB config, prev: %u, this: %u, next: %u ::",hdr.offsetPrev,hdr.offsetThis,hdr.offsetNext);
    ESP_LOG_BUFFER_HEXDUMP(LOG_TAG,config->virtualButtonConfig[i],config->virtualButtonCfgSize[i],ESP_LOG_DEBUG);
    #endif
  }

  fclose(f);
  ESP_LOGD(LOG_TAG,"Successfully stored VBs for %d, payload: %d Bytes",slotnumber,hdr.offsetNext-1);
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
    sprintf(file,"%s/IR_%02d.fms",base_path,currentSlot);
    
    //open file for reading
    ESP_LOGD(LOG_TAG,"Opening file %s",file);
    f = fopen(file, "rb");

    //Currently: return ESP_FAIL for first not found file
    if(f == NULL)
    {
      ESP_LOGD(LOG_TAG,"Stopped at IR cmd number %u, didn't found the given name",currentSlot);
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
    ESP_LOGD(LOG_TAG,"Read cmdname: %s, length %d",fileSlotName,slotnamelen);
      
    //compare parameter & file slotname
    if(strcmp(cmdName, fileSlotName) == 0)
    {
      //found a slot
      ESP_LOGD(LOG_TAG,"Found slot \"%s\" @%u",cmdName,currentSlot);
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
    //reset current VB number to zero, detecting non-consecutive access in VB write.
    storageCurrentVBNumber = 0;
    //save caller's name for tracking
    strncpy(storageCurrentTIDHolder,caller,sizeof(storageCurrentTIDHolder));
    if(s_wl_handle == WL_INVALID_HANDLE)
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
    ESP_LOGE(LOG_TAG,"cannot obtain mutex, currently active: %s",storageCurrentTIDHolder);
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
    ESP_LOGE(LOG_TAG,"TID == 0, not a valid transaction id");
    return ESP_FAIL;
  }
  
  //give mutex back
  xSemaphoreGive(halStorageMutex);
  
  return ESP_OK;
}
