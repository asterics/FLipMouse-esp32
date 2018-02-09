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
 * 
 * This module starts one task on its own for maintaining the storage
 * access.
 * All methods called from this module will block via a semaphore until
 * the requested operation is finished.
 * 
 * Slots are stored in following naming convention (8.3 rule applies here):
 * general slot config:
 * xxx.fms (slot number, e.g., 001.fms for slot 1)
 * virtual button config for slot xxx
 * xxx_VB.fms
 * 
 * @note Maximum number of slots: 250! (e.g. 250.fms)
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
const char *base_path = "/spiflash";


/** @brief internal function to init the filesystem if handle is invalid 
 * @return ESP_OK on success, ESP_FAIL otherwise*/
esp_err_t halStorageInit(void)
{
  ESP_LOGD(LOG_TAG, "Mounting FATFS for storage");
  // To mount device we need name of device partition, define base_path
  // and allow format partition in case if it is new one and was not formated before
  const esp_vfs_fat_mount_config_t mount_config = {
          .max_files = 4,
          .format_if_mount_failed = true
  };
  return esp_vfs_fat_spiflash_mount(base_path, "storage", &mount_config, &s_wl_handle);
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
  
  
  strcpy(defaultCfg->slotName,"__DEFAULT");
  
  #ifdef DEVICE_FABI
    defaultCfg->deviceIdentifier = 1;
  #endif
  #ifdef DEVICE_FLIPMOUSE
    defaultCfg->deviceIdentifier = 0;
  #endif
  
  defaultCfg->adc.orientation = 0;
  defaultCfg->adc.acceleration = 50;
  defaultCfg->adc.axis = 0;
  defaultCfg->adc.deadzone_x = 20;
  defaultCfg->adc.deadzone_y = 20;
  defaultCfg->adc.max_speed = 50;
  defaultCfg->adc.mode = MOUSE;
  defaultCfg->adc.mode = THRESHOLD;
  defaultCfg->adc.sensitivity_x = 60;
  defaultCfg->adc.sensitivity_y = 60;
  defaultCfg->adc.threshold_puff = 525;
  defaultCfg->adc.threshold_sip = 500;
  defaultCfg->adc.threshold_strongpuff = 700;
  defaultCfg->adc.threshold_strongsip = 300;
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
  }
  
  //add VB functions for default slot
  defaultCfg->virtualButtonCommand[VB_SIP] = T_MOUSE;
  defaultCfg->virtualButtonCommand[VB_PUFF] = T_MOUSE;
  defaultCfg->virtualButtonCommand[VB_STRONGPUFF] = T_CALIBRATE;
  defaultCfg->virtualButtonCommand[VB_INTERNAL1] = T_CONFIGCHANGE;
  defaultCfg->virtualButtonCommand[VB_EXTERNAL1] = T_KEYBOARD;
  /*++++ is not the default slot, just for testing ++++*/
  defaultCfg->virtualButtonCommand[VB_EXTERNAL2] = T_MOUSE;
  defaultCfg->virtualButtonCommand[VB_INTERNAL2] = T_MOUSE;
  /*++++ END is not the default slot, just for testing END ++++*/
  
  //store general config
  ret = halStorageStore(tid,defaultCfg,"DEFAULT",0);
  free(defaultCfg);
  if(ret != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Error saving default general config!");
    return;
  }
  
  //create virtual button configs for each assigned VB
  pConfig = malloc(sizeof(taskKeyboardConfig_t));
  if(pConfig != NULL)
  {
    ((taskKeyboardConfig_t *)pConfig)->type = WRITE;
    //"Hello from ESP32"
    uint16_t strarr[17] = {0x020b,0x08,0x0F,0x0F,0x12,0x2c,0x09,0x15,0x12,0x10,0x2C,0x0208,0x0216,0x0213,0x20,0x1F,0};
    uint16_t strorig[17] = {'H','e','l','l','o',' ','f','r','o','m',' ','E','S','P','3','2',0};
    //strcpy((char*)((taskKeyboardConfig_t *)pConfig)->keycodes_text,"Hello from ESP32");
    //((taskKeyboardConfig_t *)pConfig)->keycodes_text = (uint16_t*)"Hello from ESP32";
    memcpy(((taskKeyboardConfig_t *)pConfig)->keycodes_text,strarr,sizeof(strarr));
    for(uint8_t i = 1; i<=TASK_KEYBOARD_PARAMETERLENGTH; i++)
    {
      ((taskKeyboardConfig_t *)pConfig)->keycodes_text[TASK_KEYBOARD_PARAMETERLENGTH-i] = strorig[i-1];
      if(i== 17) break;
    }
    ((taskKeyboardConfig_t *)pConfig)->virtualButton = VB_EXTERNAL1;
    ret = halStorageStoreSetVBConfigs(0,VB_EXTERNAL1,pConfig,sizeof(taskKeyboardConfig_t),tid);
    //wait for 10ticks, to feed the watchdog (file access seems to block the IDLE task)
    vTaskDelay(10); 
    free(pConfig);
  } else { ESP_LOGE(LOG_TAG,"malloc error VB%u",VB_EXTERNAL1); return; }
  if(ret != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Error saving default VB%u config!",VB_EXTERNAL1);
    return;
  }
  
  /*++++ is not the default slot, just for testing ++++*/
  pConfig = malloc(sizeof(taskMouseConfig_t));
  if(pConfig != NULL)
  {
    ((taskMouseConfig_t *)pConfig)->type = X;
    ((taskMouseConfig_t *)pConfig)->actionvalue = (int8_t) -10;
    ((taskMouseConfig_t *)pConfig)->virtualButton = VB_EXTERNAL2;
    ret = halStorageStoreSetVBConfigs(0,VB_EXTERNAL2,pConfig,sizeof(taskMouseConfig_t),tid);
    //wait for 10ticks, to feed the watchdog (file access seems to block the IDLE task)
    vTaskDelay(10); 
    free(pConfig);
  } else { ESP_LOGE(LOG_TAG,"malloc error VB%u",VB_EXTERNAL2); return; }
  if(ret != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Error saving default VB%u config!",VB_EXTERNAL2);
    return;
  }

  pConfig = malloc(sizeof(taskConfigSwitcherConfig_t));
  if(pConfig != NULL)
  {
    strcpy(((taskConfigSwitcherConfig_t *)pConfig)->slotName, "__NEXT");
    ((taskConfigSwitcherConfig_t *)pConfig)->virtualButton = VB_INTERNAL1;
    ret = halStorageStoreSetVBConfigs(0,VB_INTERNAL1,pConfig,sizeof(taskConfigSwitcherConfig_t),tid);
    //wait for 10ticks, to feed the watchdog (file access seems to block the IDLE task)
    vTaskDelay(10); 
    free(pConfig);
  } else { ESP_LOGE(LOG_TAG,"malloc error VB%u",VB_INTERNAL1); return; }
  if(ret != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Error saving default VB%u config!",VB_INTERNAL1);
    return;
  }
  
  pConfig = malloc(sizeof(taskMouseConfig_t));
  if(pConfig != NULL)
  {
    ((taskMouseConfig_t *)pConfig)->type = Y;
    ((taskMouseConfig_t *)pConfig)->actionvalue = (int8_t) 10;
    ((taskMouseConfig_t *)pConfig)->virtualButton = VB_INTERNAL2;
    ret = halStorageStoreSetVBConfigs(0,VB_INTERNAL2,pConfig,sizeof(taskMouseConfig_t),tid);
    //wait for 10ticks, to feed the watchdog (file access seems to block the IDLE task)
    vTaskDelay(10); 
    free(pConfig);
  } else { ESP_LOGE(LOG_TAG,"malloc error VB%u",VB_INTERNAL2); return; }
  if(ret != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Error saving default VB%u config!",VB_INTERNAL2);
    return;
  }
  
  pConfig = malloc(sizeof(taskMouseConfig_t));
  if(pConfig != NULL)
  {
    ((taskMouseConfig_t *)pConfig)->type = LEFT;
    ((taskMouseConfig_t *)pConfig)->actionparam = M_HOLD;
    ((taskMouseConfig_t *)pConfig)->virtualButton = VB_SIP;
    ret = halStorageStoreSetVBConfigs(0,VB_SIP,pConfig,sizeof(taskMouseConfig_t),tid);
    //wait for 10ticks, to feed the watchdog (file access seems to block the IDLE task)
    vTaskDelay(10); 
    free(pConfig);
  } else { ESP_LOGE(LOG_TAG,"malloc error VB%u",VB_SIP); return; }
  if(ret != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Error saving default VB%u config!",VB_SIP);
    return;
  }
  
  pConfig = malloc(sizeof(taskMouseConfig_t));
  if(pConfig != NULL)
  {
    ((taskMouseConfig_t *)pConfig)->type = RIGHT;
    ((taskMouseConfig_t *)pConfig)->actionparam = M_CLICK;
    ((taskMouseConfig_t *)pConfig)->virtualButton = VB_PUFF;
    ret = halStorageStoreSetVBConfigs(0,VB_PUFF,pConfig,sizeof(taskMouseConfig_t),tid);
    //wait for 10ticks, to feed the watchdog (file access seems to block the IDLE task)
    vTaskDelay(10); 
    free(pConfig);
  } else { ESP_LOGE(LOG_TAG,"malloc error VB%u",VB_PUFF); return; }
  if(ret != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Error saving default VB%u config!",VB_PUFF);
    return;
  }
  
  pConfig = malloc(sizeof(taskNoParameterConfig_t));
  if(pConfig != NULL)
  {
    ((taskNoParameterConfig_t *)pConfig)->virtualButton = VB_STRONGPUFF;
    ret = halStorageStoreSetVBConfigs(0,VB_STRONGPUFF,pConfig,sizeof(taskNoParameterConfig_t),tid);
    //wait for 10ticks, to feed the watchdog (file access seems to block the IDLE task)
    vTaskDelay(10); 
    free(pConfig);
  } else { ESP_LOGE(LOG_TAG,"malloc error VB%u",VB_STRONGPUFF); return; }
  if(ret != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Error saving default VB%u config!",VB_STRONGPUFF);
    return;
  }
  
  ESP_LOGI(LOG_TAG,"Created new default slot");
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
  uint8_t currentSlot = 1;
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
      ESP_LOGD(LOG_TAG,"Available slots: %u",currentSlot-1);
      *slotsavailable = currentSlot - 1;
      return ESP_OK;
    } else {
      fclose(f);
      currentSlot++;
    }
  } while(1);
  
  return ESP_OK;
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
  uint8_t currentSlot = 1;
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
      if(slotCount == storageCurrentSlotNumber) storageCurrentSlotNumber = 1;
      else storageCurrentSlotNumber++;
      return halStorageLoadNumber(storageCurrentSlotNumber,cfg,tid);
    break;
    case PREV:
      //check if we are at the first slot, if yes load last, decrement otherwise
      if(storageCurrentSlotNumber == 1) storageCurrentSlotNumber = slotCount;
      else storageCurrentSlotNumber--;
      return halStorageLoadNumber(storageCurrentSlotNumber,cfg,tid);      
    break;
    case DEFAULT:
      if(slotCount == 0)
      {
        //load default slot via number 0
        return halStorageLoadNumber(0,cfg,tid);
      } else {
        //default slot if not factory reset is nr 1
        return halStorageLoadNumber(1,cfg,tid);
      }
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
      free(slotname);
      halStorageCreateDefault(tid);
      return ESP_FAIL;
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
    ESP_LOG_BUFFER_HEXDUMP(LOG_TAG,cfg,sizeof(generalConfig_t),ESP_LOG_DEBUG);
    fclose(f);
    free(slotname);
    return ESP_FAIL;
  }
  
  //TODO: check slot storage version, upgrade on any difference
  
  ESP_LOGW(LOG_TAG,"Loaded slot %s,nr: %d",slotname,slotnumber);
  ESP_LOG_BUFFER_HEXDUMP(LOG_TAG,cfg,sizeof(generalConfig_t),ESP_LOG_DEBUG);
  
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
  
  ESP_LOGD(LOG_TAG,"Successfully read slotnumber %u with %u bytes payload", slotnumber, sizeof(generalConfig_t));
  
  //clean up
  free(slotname);
  fclose(f);
  
  //save current slot number to access the VB configs
  storageCurrentSlotNumber = slotnumber;
  
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
    ESP_LOGE(LOG_TAG,"Cannot find number for name!");
    return ESP_FAIL;
  }
  
  return halStorageLoadNumber(slotnumber,cfg,tid);
}

/** @brief Delete one or all slots
 * 
 * This function is used to delete one slot or all slots (depending on
 * parameter slotnumber)
 * 
 * @param slotnr Number of slot to be deleted. Use 0 to delete all slots
 * @param tid Transaction id
 * @return ESP_OK if everything is fine, ESP_FAIL otherwise
 * */
esp_err_t halStorageDeleteSlot(uint8_t slotnr, uint32_t tid)
{
  char file[sizeof(base_path)+32];
  //delete by starting & ending at given slotnumber 
  uint8_t from = slotnr;
  uint8_t to = slotnr;
  
  //in case of deleting all slots, start at 1 and delete until 250
  if(slotnr == 0)
  {
    from = 1;
    to = 250;
  }
  
  //check for valid storage handle
  if(halStorageChecks(tid) != ESP_OK) return ESP_FAIL;
  
  //delete one or all slots
  for(uint8_t i = from; i<=to; i++)
  {
    sprintf(file,"%s/%03d.fms",base_path,i); 
    remove(file);
    sprintf(file,"%s/%03d_VB.fms",base_path,i); 
    remove(file);
    vTaskDelay(1);
  }
  
  ESP_LOGW(LOG_TAG,"Deleted slot %d (0 means delete all)",slotnr);
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
 * @param vb_config_size Size of config which will be loaded (differs between functions)
 * @param tid Transaction ID, which must match the one given by halStorageStartTransaction
 * @return ESP_OK if everything is fine, ESP_FAIL if the command was not successful (no config found or no slot loaded)
 * */
esp_err_t halStorageLoadGetVBConfigs(uint8_t vb, void * vb_config, size_t vb_config_size, uint32_t tid)
{
  char file[sizeof(base_path)+32];
  
  if(halStorageChecks(tid) != ESP_OK) return ESP_FAIL;
  
  if(vb >= (NUMBER_VIRTUALBUTTONS*4)) 
  {
    ESP_LOGE(LOG_TAG,"VB number too high: %u, maximum %u",vb,(NUMBER_VIRTUALBUTTONS*4));
    return ESP_FAIL;
  }
  
  //file naming convention for general config: xxx.fms
  //file naming convention for VB configs: xxx_VB.fms
  
  //create filename from slotnumber & vb number
  sprintf(file,"%s/%03d_VB.fms",base_path,storageCurrentSlotNumber);
  
  //open file for reading
  ESP_LOGD(LOG_TAG,"Opening file %s",file);
  FILE *f = fopen(file, "rb");
  if(f == NULL)
  {
    ESP_LOGE(LOG_TAG,"cannot open file for reading: %s",file);
    return ESP_FAIL;
  }
  
  fseek(f,vb*VB_MAXIMUM_PARAMETER_SIZE,SEEK_SET);
  
  //read vb config
  if(fread(vb_config, vb_config_size, 1, f) != 1)
  {
    ESP_LOGE(LOG_TAG,"Error reading VB config from %s",file);
    fclose(f);
    return ESP_FAIL;
  }
  
  fclose(f);
  ESP_LOGD(LOG_TAG,"Successfully loaded slotnumber %d, VB%d, payload: %d",storageCurrentSlotNumber,vb,vb_config_size);
  ESP_LOG_BUFFER_HEXDUMP(LOG_TAG,vb_config,vb_config_size,ESP_LOG_DEBUG);
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
  
  if(strlen(slotname) > SLOTNAME_LENGTH)
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
  namelen = strlen(slotname);
  fwrite(&namelen,sizeof(uint32_t),1, f);
  fwrite(slotname,sizeof(char),namelen, f);
  fwrite(&nullterm,sizeof(char),1, f);
  
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
    ESP_LOG_BUFFER_HEXDUMP(LOG_TAG,cfg,sizeof(generalConfig_t),ESP_LOG_DEBUG);
  }
  
  //clean up
  fclose(f);
  
  //save current slot number to access the VB configs
  storageCurrentSlotNumber = slotnumber;
  
  return ESP_OK;
}

/** @brief Store a virtual button config struct
 * 
 * This method stores the config struct for the given virtual button
 * If there is already a config with this given VB, it is overwritten!
 * 
 * Due to different sizes of configs for different functionalities,
 * it is necessary to provide the size of the data to be stored.
 * 
 * 
 * @param tid Transaction id
 * @param slotnumber Number of the slot on which this config is used. Use 0xFF to ignore and use
 * previous set slot number (by halStorageStore)
 * @param config Pointer to the VB config
 * @param vb VirtualButton number
 * @param configsize Size of this configuration which is stored
 * @return ESP_OK on success, ESP_FAIL otherwise
 * @see halStorageStore
 * @warning Storing VBs is only in possible consecutive numbers, starting with 0!
 * Any other attempt to store a VB not starting with 0 will fail.
 * */
esp_err_t halStorageStoreSetVBConfigs(uint8_t slotnumber, uint8_t vb, void *config, size_t configsize, uint32_t tid)
{
  char file[sizeof(base_path)+32];
  
  if(halStorageChecks(tid) != ESP_OK) return ESP_FAIL;
  
  //check if we should ignore the slotnumber and take the previously used one
  if(slotnumber == 0xFF) slotnumber = storageCurrentSlotNumber;
  
  //check if the slotnumber is out of range
  if(slotnumber > 250) 
  {
    ESP_LOGE(LOG_TAG,"Slotnumber too high: %u, maximum 250",slotnumber);
    return ESP_FAIL;
  }
  
  if(vb >= (NUMBER_VIRTUALBUTTONS*4)) 
  {
    ESP_LOGE(LOG_TAG,"VB number too high: %u, maximum %u",slotnumber,(NUMBER_VIRTUALBUTTONS*4));
    return ESP_FAIL;
  }
  
  if(vb < storageCurrentVBNumber)
  {
    ESP_LOGE(LOG_TAG,"VB store is only possible in consecutive calls!");
    return ESP_FAIL;
  }
  
  //create filename from slotnumber
  sprintf(file,"%s/%03d_VB.fms",base_path,slotnumber);
  
  //VB_MAXIMUM_PARAMETER_SIZE
  
  //open file for writing
  ESP_LOGD(LOG_TAG,"Opening file %s",file);
  FILE *f = fopen(file, "ab");
  if(f == NULL)
  {
    ESP_LOGE(LOG_TAG,"cannot open file for writing: %s",file);
    return ESP_FAIL;
  }
  
  //set to correct VB position
  fseek(f,vb*VB_MAXIMUM_PARAMETER_SIZE,SEEK_SET);

  //write vb config
  if(fwrite(config, configsize, 1, f) != 1)
  {
    ESP_LOGE(LOG_TAG,"Error writing VB config on %s",file);
    fclose(f);
    return ESP_FAIL;
  }
  
  //fill up until we reach VB_MAXIMUM_PARAMETER_SIZE
  uint8_t fill = 0xAB;
  if(fwrite(&fill, 1, VB_MAXIMUM_PARAMETER_SIZE-configsize, f) != VB_MAXIMUM_PARAMETER_SIZE-configsize)
  {
    ESP_LOGE(LOG_TAG,"Error writing VB fill-pattern on %s",file);
    fclose(f);
    return ESP_FAIL;
  }
  
  storageCurrentVBNumber = vb;
  
  fclose(f);
  ESP_LOGD(LOG_TAG,"Successfully stored slotnumber %d, VB%d, payload: %d",slotnumber,vb,configsize);
  ESP_LOG_BUFFER_HEXDUMP(LOG_TAG,config,configsize,ESP_LOG_DEBUG);
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
 * @return ESP_OK if the tid is valid, ESP_FAIL if other tasks did not freed the access in time
 * */
esp_err_t halStorageStartTransaction(uint32_t *tid, TickType_t tickstowait)
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
    *tid = storageCurrentTID;
    //reset current VB number to zero, detecting non-consecutive access in VB write.
    storageCurrentVBNumber = 0;
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
    ESP_LOGE(LOG_TAG,"cannot obtain mutex");
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
    ESP_LOGE(LOG_TAG,"Not sufficient memory to create mutex, cannot access!");
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
