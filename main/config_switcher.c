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
 * @brief CONTINOUS TASK + FUNCTIONAL TASK - This module takes care of
 * configuration loading.
 * 
 * The config_switcher module is used to control all tasks assigned to 
 * virtual buttons (called FUNCTIONAL TASKS).
 * If a new configuration should be loaded from the storage, all
 * previously loaded tasks are deleted and new tasks are loaded.
 * 
 * A slot configuration is provided by the config_storage, which is
 * controlled by this module. 
 * 
 * task_configswitcher is the FUNCTIONAL TASK for triggering a slot switch.
 * configSwitcherTask is the CONTINOUS TASK which monitors the queue for
 * any config switching action.
 * 
 * @note If you want to add a new FUNCTIONAL TASK, please include headers here
 * & add loading functionality to configSwitcherTask. 
 * */
#include "config_switcher.h"

/** Tag for ESP_LOG logging */
#define LOG_TAG "cfgsw"
/** @brief Set a global log limit for this file */
#define LOG_LEVEL_CFGSW ESP_LOG_INFO

/** Stacksize for continous task configSwitcherTask.
 * @see configSwitcherTask */
#define CONFIGSWITCHERTASK_PERMANENT_STACKSIZE 4096

/** Task handle for the CONTINOUS task responsible for config switching */
TaskHandle_t configswitcher_handle;

/** @brief Semaphore for detecting pending config updates
 * 
 * Usually, on saving a new slot, each time a value is changed, configUpdate()
 * is called. This is unnecessary, so we have a semaphore which shows, if
 * an update is already pending. In addition, this semaphore can be used
 * to detect if the new configuration is stable (all tasks are loaded),
 * which is necessary for storing the active slot via hal_storage.
 * 
 * @see configUpdate
 * @see configUpdateIsStable
 * */
SemaphoreHandle_t configUpdatePending = NULL;

TimerHandle_t configTimer = NULL;

/** Currently loaded configuration.*/
generalConfig_t currentConfigLoaded;

/** @brief Get the current config struct
 * 
 * This method is used to get a reference to the current config struct.
 * The caller can use this reference to change the configuration and
 * update the config afterwards (via the config_switcher queue).
 * 
 * @see config_switcher
 * @see currentConfigLoaded
 * @return Pointer to the current config struct
 * */
generalConfig_t* configGetCurrent(void)
{
  return &currentConfigLoaded;
}

/** @brief Trigger a config update
 * 
 * This method is simply sending an "__UPDATE" command to the
 * config_switcher queue to trigger an update for the virtual buttons
 * 
 * @see config_switcher
 * @see configUpdate
 * @see currentConfig
 * */
void configTriggerUpdate(void)
{
  //reload the slot by sending "__UPDATE" to 
  // the config_switcher queue
  char commandname[SLOTNAME_LENGTH];
  strcpy(commandname,"__UPDATE");
  if(config_switcher == NULL) return;
  
  if(xQueueSend(config_switcher,commandname,0) == pdPASS)
  {
    ESP_LOGD(LOG_TAG,"requesting config update");
  } else {
    ESP_LOGE(LOG_TAG,"Error requesting slot update");
  }
}

/** @brief Wait until current configuration is stable
 * 
 * If the function configUpdateVB is called, a temporary task parameter
 * buffer is used. The full configuration is loaded if configUpdatePending
 * semaphore is free. If the configuration is to be saved, this semaphore
 * should be checked via this method.
 * 
 * @note This method blocks on the semaphore. 
 * @return ESP_OK if config is stable, ESP_FAIL if timeout happened
 * */
esp_err_t configUpdateWaitStable(void)
{
  //test if semaphore is initialized
  if(configUpdatePending == NULL) 
  {
    ESP_LOGE(LOG_TAG,"Update sem not initialized");
    return ESP_FAIL;
  }
  //wait for a free semaphore. If not getting it, return fail.
  if(xSemaphoreTake(configUpdatePending,portMAX_DELAY) == pdTRUE)
  {
    //give semaphore again, we know that config is stable now.
    xSemaphoreGive(configUpdatePending);
    return ESP_OK;
  } else {
    return ESP_FAIL;
  }
}

void configTimerCallback(TimerHandle_t xTimer)
{
  //reload ADC
  if(halAdcUpdateConfig(&currentConfigLoaded.adc) != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"error reloading adc config");
  }
  
  //set other config infos
  ESP_LOGI(LOG_TAG,"setting connection bits (USB: %d, BLE: %d)",currentConfigLoaded.usb_active,currentConfigLoaded.ble_active);
  if(currentConfigLoaded.ble_active != 0)  xEventGroupSetBits(connectionRoutingStatus,DATATO_BLE);
  else xEventGroupClearBits(connectionRoutingStatus,DATATO_BLE);
  if(currentConfigLoaded.usb_active != 0)  xEventGroupSetBits(connectionRoutingStatus,DATATO_USB);
  else xEventGroupClearBits(connectionRoutingStatus,DATATO_USB);
  
  //reset HID channels (USB&BLE)
  halBLEReset(0);
  halSerialReset(0);
}

/** @brief Request config update
 * 
 * This method is requesting a config update for the general config.
 * It is used either by the command parser to activate a changed config
 * (by AT commands) or by the config switcher task, to activate a config
 * loaded from flash.
 *  
 * @see config_switcher
 * @see currentConfig
 * @return ESP_OK on success, ESP_FAIL otherwise
 * */
esp_err_t configUpdate(void)
{
  if(configTimer == NULL) return ESP_FAIL;
  
  if(xTimerIsTimerActive(configTimer) == pdFALSE) xTimerStart(configTimer,10);
  else xTimerReset(configTimer,0);
  return ESP_OK;
}

/** @brief CONTINOUS TASK - Config switcher task, internal config reloading
 * 
 * This task is used to change the full configuration of this device
 * After sending a request to the config_switcher queue, this
 * task is used to unload all virtual buttons tasks and initializing
 * the virtualbutton tasks with the new functionality.
 * @see config_switcher
 * @param params Not used, pass NULL.
 * 
 * @warning On a factory reset, it is necessary to load the default slot again!
 * 
 * @todo Add __RESTOREFACTORY...
 **/
void configSwitcherTask(void * params)
{
  char command[SLOTNAME_LENGTH];
  
  uint32_t tid = 0;
  uint8_t justupdate = 0;
  esp_err_t ret;
  
  if(config_switcher == 0)
  {
    ESP_LOGE(LOG_TAG,"config_switcher queue uninitialized, exiting.");
    vTaskDelete(NULL);
  }
  
  while(1)
  {
    //wait for a command.
    if(xQueueReceive(config_switcher,command,1000/portTICK_PERIOD_MS) == pdTRUE)
    {
      //request storage access
      while(halStorageStartTransaction(&tid,100,LOG_TAG) != ESP_OK)
      {
        ESP_LOGE(LOG_TAG,"Cannot start storage transaction");
        vTaskDelay(100/portTICK_PERIOD_MS);
      }
      //just to be sure: normally we are not updating...
      justupdate = 0;
      
      //command received, load new slot:
      //__NEXT, __PREV, __DEFAULT, __UPDATE, __RESTOREFACTORY
      if(strcmp(command,"__NEXT") == 0)
      {
        ESP_LOGD(LOG_TAG,"Load next slot");
        ret = halStorageLoad(NEXT,tid);
      } else if(strcmp(command,"__PREV") == 0) {
        ESP_LOGD(LOG_TAG,"Load prev slot");
        ret = halStorageLoad(PREV,tid);
      } else if(strcmp(command,"__DEFAULT") == 0) {
        //load default slot (if not available, it will be created)
        ret = halStorageLoad(DEFAULT,tid);
        ESP_LOGD(LOG_TAG,"loading default");
      } else if(strcmp(command,"__RESTOREFACTORY") == 0) {
        ret = halStorageDeleteSlot(0,tid);
        ret = halStorageLoad(DEFAULT,tid);
        if(ret != ESP_OK)
        {
          ESP_LOGE(LOG_TAG,"Error deleting all slots");
        } else {
          ESP_LOGW(LOG_TAG,"Deleted all slots");
        }
        halStorageFinishTransaction(tid);
        continue;
      } else  {
        ret = halStorageLoadName(command,tid);
        ESP_LOGD(LOG_TAG,"Load by name: %s",command);
      }
      
      if(ret != ESP_OK)
      {
        halStorageFinishTransaction(tid);
        ESP_LOGE(LOG_TAG,"Error loading general slot config!");
        continue;
      }
      //reload general config
      configUpdate();
      
      //make one or more config tones (depending on slot number)
      uint8_t slotnr = halStorageGetCurrentSlotNumber() + 1;
      for(uint8_t i = 0; i<slotnr; i++)
      {
        //create a tone and a pause (values are from original firmware)
        TONE(TONE_CHANGESLOT_FREQ_BASE + slotnr*TONE_CHANGESLOT_FREQ_SLOTNR, \
          TONE_CHANGESLOT_DURATION);
        TONE(0,TONE_CHANGESLOT_DURATION_PAUSE);
      }
      
      //LED output on slot switch (steady color on Neopixel, short fading on RGB)
      LED((slotnr%2)*0xFF,((slotnr/2)%2)*0xFF,((slotnr/4)%2)*0xFF,0);
      
      //clean up
      halStorageFinishTransaction(tid);
      tid = 0;
      
      if(justupdate)
      {
        xSemaphoreGive(configUpdatePending);
        ESP_LOGD(LOG_TAG,"----Config Update Complete, loaded slot %s----",currentConfigLoaded.slotName);
      } else {
        halAdcCalibrate();
        ESP_LOGD(LOG_TAG,"----Config Switch Complete, loaded slot %s----",currentConfigLoaded.slotName);
      }
    }
  }
}

/** @brief Initializing the config switching functionality.
 * 
 * The CONTINOUS task will be loaded to enable slot switches via the
 * task_configswitcher FUNCTIONAL task. 
 * @return ESP_OK if everything is fined, ESP_FAIL otherwise */
esp_err_t configSwitcherInit(void)
{
  //set log level to given log level
  esp_log_level_set(LOG_TAG,LOG_LEVEL_CFGSW);
  //test if the config_switcher queue is initialized...
  if(config_switcher == NULL)
  {
    ESP_LOGE(LOG_TAG,"Error init config switcher, please create \
      config_switcher queue before calling configSwitcherInit()!");
    return ESP_FAIL;
  }
  
  //init update semaphore
  configUpdatePending = xSemaphoreCreateBinary();
  xSemaphoreGive(configUpdatePending);
  //init timer for slowing down updates
  configTimer = xTimerCreate
     ( /* Just a text name, not used by the RTOS
       kernel. */
       "cfgTimer",
       /* The timer period in ticks, must be
       greater than 0. */
       50/portTICK_PERIOD_MS,
       /* The timers will auto-reload themselves
       when they expire. */
       pdFALSE,
       /* The ID is used to store a count of the
       number of times the timer has expired, which
       is initialised to 0. */
       ( void * ) 0,
       /* Each timer calls the same callback when
       it expires. */
       configTimerCallback
     );
  
  //start configSwitcherTask
  if(xTaskCreate(configSwitcherTask,"configswitcher",CONFIGSWITCHERTASK_PERMANENT_STACKSIZE,(void *)NULL,
    HAL_CONFIG_TASK_PRIORITY,configswitcher_handle) != pdPASS)
  {
    ESP_LOGE(LOG_TAG,"error creating config switcher task, cannot proceed.");
    return ESP_FAIL;
  } else {
    ESP_LOGD(LOG_TAG,"configSwitcherTask created");
  }
  
  //load the default slot by sending "__DEFAULT" to 
  // the config_switcher queue
  char commandname[SLOTNAME_LENGTH];
  strcpy(commandname,"__DEFAULT");
  if(xQueueSend(config_switcher,commandname,10) == pdPASS)
  {
    ESP_LOGD(LOG_TAG,"requesting default slot switch on startup");
  } else {
    ESP_LOGE(LOG_TAG,"Error requesting default slot switch");
  }
  
  return ESP_OK;
}

