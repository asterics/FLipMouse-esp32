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

/** Stacksize for continous task configSwitcherTask.
 * @see configSwitcherTask */
#define CONFIGSWITCHERTASK_PERMANENT_STACKSIZE 4096

/** @brief Array containing all currently running FUNCTIONAL task handles
 * 
 * This array contains task handles, one for each virtual button.
 * NULL if there is no task loaded, the task handle otherwise.
 * Is used to delete running tasks on a config switch */
TaskHandle_t currentTasks[NUMBER_VIRTUALBUTTONS*4];

/** @brief Array of pointers to FUNCTIONAL task parameters
 * 
 * This array contains pointers to memory, which is allocated here.
 * Each time a task switch is triggered, the parameters are loaded from
 * hal_storage and saved to an allocated memory area. These pointers are
 * used to pass the parameters to the functional tasks and free this memory
 * after unloading/deleting functional tasks. */
void * currentTaskParameters[NUMBER_VIRTUALBUTTONS*4];

/** @brief Array of pointers to FUNCTIONAL task parameters (for updating current config)
 * 
 * This array contains pointers to memory, which is allocated externally.
 * Pointers here have the same function as in currentTaskParameters, except
 * they are used ONLY during config <b>UPDATE</b>.
 * 
 * If an external module wants to update a virtual button config without
 * loading a full slot, following steps are necessary:<br<
 * * Allocate memory for the FUNCTIONAL task parameters
 * * Save this pointer to this array here & fill parameter with data
 * * Set currentConfigLoaded.virtualButtonCommand[vb] accordingly
 * * Request slot update via config_switcher queue
 * 
 * @todo Maybe this information should be moved to the corresponding function.
 *  */
void * currentTaskParametersUpdate[NUMBER_VIRTUALBUTTONS*4];

/** Task handle for the CONTINOUS task responsible for config switching */
TaskHandle_t configswitcher_handle;

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

/** @brief Reverse Parsing - get AT command for configswitcher VB
 * 
 * This function parses the current configuration of a virtual button
 * to an AT command used to print the configuration.
 * @param output Output string, where the full AT command will be stored
 * @param cfg Pointer to current cfgswitcher configuration, used to parse.
 * @return ESP_OK if everything went fine, ESP_FAIL otherwise
 * */
esp_err_t task_configswitcher_getAT(char* output, void* cfg)
{
  taskConfigSwitcherConfig_t *conf = (taskConfigSwitcherConfig_t *)cfg;
  if(conf == NULL) return ESP_FAIL;
  
  if(strcmp(conf->slotName, "__NEXT") == 0) sprintf(output,"AT NE \r\n"); return ESP_OK;
  
  sprintf(output,"AT LO %s \r\n",conf->slotName);
}

/** @brief Trigger a config update
 * 
 * This method is simply sending an "__UPDATE" command to the
 * config_switcher queue to trigger an update by re-loading the
 * currentConfig.
 * 
 * @see config_switcher
 * @see currentConfig
 * @return ESP_OK on success, ESP_FAIL otherwise
 * */
esp_err_t configUpdate(void)
{
  //reload the slot by sending "__UPDATE" to 
  // the config_switcher queue
  char commandname[SLOTNAME_LENGTH];
  strcpy(commandname,"__UPDATE");
  if(config_switcher == NULL) return ESP_FAIL;
  
  if(xQueueSend(config_switcher,commandname,10) == pdPASS)
  {
    ESP_LOGD(LOG_TAG,"requesting config update");
    return ESP_OK;
  } else {
    ESP_LOGE(LOG_TAG,"Error requesting slot update");
    return ESP_FAIL;
  }
}

/** @brief Update one virtual button.
 * 
 * This function is used to update one virtual button config.
 * For example it is triggered by "AT BM xx" and the following 
 * AT command.
 * It saves the param pointer to the update param array and calls
 * configUpdate to trigger the reload.
 * @see configUpdate
 * @see currentTaskParametersUpdate
 * @param param FUNCTIONAL task parameter memory
 * @param type Type of new VB command
 * @param vb Number of virtualbutton to update
 * @return ESP_OK on success, ESP_FAIL otherwise
 * */
esp_err_t configUpdateVB(void *param, command_type_t type, uint8_t vb)
{
  if(vb >= (NUMBER_VIRTUALBUTTONS*4))
  {
    ESP_LOGE(LOG_TAG,"Cannot update VB%d, vb number out of range",vb);
    return ESP_FAIL;
  }
  
  if(vb == VB_SINGLESHOT)
  {
    ESP_LOGE(LOG_TAG,"Cannot use singleshot task as VB FUNCTIONAL task!");
    return ESP_FAIL;
  }
  
  //save pointer to update list
  currentTaskParametersUpdate[vb] = param;
  //set new command type
  currentConfigLoaded.virtualButtonCommand[vb] = type;
  //trigger config update
  return configUpdate();
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
  char tasksignature[8];
  generalConfig_t currentConfig;
  
  uint32_t tid = 0;
  size_t vbparametersize = 0;
  TaskFunction_t newTaskCode = NULL;
  short stacksize = 0;
  uint8_t justupdate = 0;
  esp_err_t ret;
  
  if(config_switcher == 0)
  {
    ESP_LOGE(LOG_TAG,"config_switcher queue uninitialized, exiting.");
    vTaskDelete(NULL);
  }
  
  //reset all allocated task parameter pointers
  for(uint8_t i = 0;i<NUMBER_VIRTUALBUTTONS*4;i++)
  {
    currentTaskParameters[i] = NULL;
  }
  
  while(1)
  {
    //wait for a command.
    if(xQueueReceive(config_switcher,command,1000/portTICK_PERIOD_MS) == pdTRUE)
    {
      //request storage access
      if(halStorageStartTransaction(&tid,20) != ESP_OK)
      {
        ESP_LOGE(LOG_TAG,"Cannot start storage transaction");
        continue;
      }
      //just to be sure: normally we are not updating...
      justupdate = 0;
      
      //command received, load new slot:
      //__NEXT, __PREV, __DEFAULT, __UPDATE, __RESTOREFACTORY
      if(strcmp(command,"__NEXT") == 0)
      {
        ESP_LOGD(LOG_TAG,"Load next slot");
        ret = halStorageLoad(NEXT,&currentConfig,tid);
      } else if(strcmp(command,"__PREV") == 0) {
        ESP_LOGD(LOG_TAG,"Load prev slot");
        ret = halStorageLoad(PREV,&currentConfig,tid);
      } else if(strcmp(command,"__DEFAULT") == 0) {
        ret = halStorageLoad(DEFAULT,&currentConfig,tid);
        //for default, we try here again (in the case we had to build a new
        //fresh default config
        if(ret != ESP_OK) ret = halStorageLoad(DEFAULT,&currentConfig,tid);
        ESP_LOGD(LOG_TAG,"loading default");
      } else if(strcmp(command,"__RESTOREFACTORY") == 0) {
        ret = halStorageDeleteSlot(0,tid);
        
        halStorageCreateDefault(tid);
        halStorageFinishTransaction(tid);
        if(ret != ESP_OK)
        {
          ESP_LOGE(LOG_TAG,"Error deleting all slots");
        } else {
          ESP_LOGW(LOG_TAG,"Deleted all slots");
        }
        continue;
      } else if(strcmp(command,"__UPDATE") == 0) {
        ESP_LOGD(LOG_TAG,"config update");
        //on config updates, use the currentConfigLoaded,
        //it is possible the currentConfigLoaded was changed by external parts
        memcpy(&currentConfig,&currentConfigLoaded,sizeof(generalConfig_t));
        justupdate = 1;
        ret = ESP_OK;
      } else  {
        ret = halStorageLoadName(command,&currentConfig,tid);
        ESP_LOGD(LOG_TAG,"Load by name: %s",command);
      }
      
      if(ret != ESP_OK)
      {
        halStorageFinishTransaction(tid);
        ESP_LOGE(LOG_TAG,"Error loading general slot config!");
        continue;
      }

      //reload ADC
      if(halAdcUpdateConfig(&currentConfig.adc) != ESP_OK)
      {
        ESP_LOGE(LOG_TAG,"error reloading adc config");
      }
      
      //set other config infos
      ESP_LOGD(LOG_TAG,"setting connection bits (USB: %d, BLE: %d)",currentConfig.usb_active,currentConfig.ble_active);
      if(currentConfig.ble_active != 0)  xEventGroupSetBits(connectionRoutingStatus,DATATO_BLE);
      else xEventGroupClearBits(connectionRoutingStatus,DATATO_BLE);
      if(currentConfig.usb_active != 0)  xEventGroupSetBits(connectionRoutingStatus,DATATO_USB);
      else xEventGroupClearBits(connectionRoutingStatus,DATATO_USB);
      
      //reset HID channels (USB&BLE)
      halBLEReset(0);
      halSerialReset(0);
      
      //perform task switching...
      for(uint8_t i = 0; i<(NUMBER_VIRTUALBUTTONS*4); i++)
      {
        //if we are supposed to update VBs AND this one is not
        //to be updated, skip all the unloading/loading...
        if((justupdate == 1) && (currentTaskParametersUpdate[i] == NULL)) continue;
        
        //unload
        if(currentTasks[i] != NULL) vTaskDelete(currentTasks[i]);
        //if memory was previously allocated, free now
        if(currentTaskParameters[i] != NULL) free(currentTaskParameters[i]);
        
        //determine which tasks should be loaded
        switch(currentConfig.virtualButtonCommand[i])
        {
          case T_MOUSE: 
            newTaskCode = (TaskFunction_t)task_mouse; 
            currentTaskParameters[i] = malloc(sizeof(taskMouseConfig_t));
            vbparametersize = sizeof(taskMouseConfig_t);
            stacksize = TASK_MOUSE_STACKSIZE; 
            ESP_LOGD(LOG_TAG,"creating new mouse task on VB %d",i);
            break;
          case T_MACRO: 
            newTaskCode = (TaskFunction_t)task_macro; 
            currentTaskParameters[i] = malloc(sizeof(taskMacrosConfig_t));
            vbparametersize = sizeof(taskMacrosConfig_t);
            stacksize = TASK_MACROS_STACKSIZE; 
            ESP_LOGD(LOG_TAG,"creating new macro task on VB %d",i);
            break;
          case T_KEYBOARD: 
            newTaskCode = (TaskFunction_t)task_keyboard; 
            currentTaskParameters[i] = malloc(sizeof(taskKeyboardConfig_t));
            vbparametersize = sizeof(taskKeyboardConfig_t);
            stacksize = TASK_KEYBOARD_STACKSIZE; 
            ESP_LOGD(LOG_TAG,"creating new keyboard task on VB %d",i);
            break;
          case T_JOYSTICK: 
            newTaskCode = (TaskFunction_t)task_joystick; 
            currentTaskParameters[i] = malloc(sizeof(taskJoystickConfig_t));
            vbparametersize = sizeof(taskJoystickConfig_t);
            stacksize = TASK_JOYSTICK_STACKSIZE; 
            ESP_LOGD(LOG_TAG,"creating new joystick task on VB %d",i);
            break;
          case T_CONFIGCHANGE: 
            newTaskCode = (TaskFunction_t)task_configswitcher; 
            currentTaskParameters[i] = malloc(sizeof(taskConfigSwitcherConfig_t));
            vbparametersize = sizeof(taskConfigSwitcherConfig_t);
            stacksize = TASK_CONFIGSWITCHER_STACKSIZE;
            ESP_LOGD(LOG_TAG,"creating new configchange task on VB %d",i);
            break;
          case T_CALIBRATE: 
            newTaskCode = (TaskFunction_t)task_calibration; 
            currentTaskParameters[i] = malloc(sizeof(taskNoParameterConfig_t));
            vbparametersize = sizeof(taskNoParameterConfig_t);
            stacksize = TASK_CALIB_STACKSIZE;
            ESP_LOGD(LOG_TAG,"creating new calibrate task on VB %d",i);
            break;
          case T_SENDIR:
            newTaskCode = NULL; 
            //allocate just a little bit to have a pointer
            currentTaskParameters[i] = malloc(sizeof(uint8_t));
            vbparametersize = 0;
            ESP_LOGD(LOG_TAG,"creating new IR task on VB %d",i);
            break;
          case T_NOFUNCTION:  
            newTaskCode = NULL; 
            //allocate just a little bit to have a pointer
            currentTaskParameters[i] = malloc(sizeof(uint8_t));
            vbparametersize = 0;
            break;
          default:
            newTaskCode = NULL;
            ESP_LOGE(LOG_TAG,"unkown button command, cannot load task");
            break;
        }
        
        //check if we allocated memory:
        if(currentTaskParameters[i] != NULL)
        {
          //load VB config to memory
          if(vbparametersize > 0)
          {
            //is there a pointer available for update?
            if(currentTaskParametersUpdate[i] == NULL)
            {
              //if no, load VB config from storage
              halStorageLoadGetVBConfigs(i,currentTaskParameters[i], vbparametersize, tid);
            } else {
              //if yes, use this pointer for task parameter
              memcpy(currentTaskParameters[i],currentTaskParametersUpdate[i],vbparametersize);
              free(currentTaskParametersUpdate[i]);
              //and reset the update pointer
              currentTaskParametersUpdate[i] = NULL;
            }
          }
          currentConfig.virtualButtonConfig[i] = currentTaskParameters[i];
        } else {
          ESP_LOGE(LOG_TAG,"error allocating memory for VB%u config",i);
        }
        
        //create task name
        sprintf(tasksignature,"vb%d",i);
        
        //finally load new task
        if(newTaskCode != NULL)
        {
          if(xTaskCreate(newTaskCode,tasksignature,stacksize, 
            currentConfig.virtualButtonConfig[i],HAL_VB_TASK_PRIORITY, 
            &currentTasks[i]) != pdPASS)
          {
            ESP_LOGE(LOG_TAG,"error creating new virtualbutton task");
          }
        } else {
          currentTasks[i] = NULL;
          ESP_LOGD(LOG_TAG,"unused virtual button %d",i);
        }
      }
      
      //make one or more config tones (depending on slot number)
      uint8_t slotnr = halStorageGetCurrentSlotNumber();
      //if no slot is available, do at least one beep
      if(slotnr == 0) slotnr++;
      for(uint8_t i = 0; i<slotnr; i++)
      {
        //create a tone and a pause (values are from original firmware)
        TONE(TONE_CHANGESLOT_FREQ_BASE + slotnr*TONE_CHANGESLOT_FREQ_SLOTNR, \
          TONE_CHANGESLOT_DURATION);
        TONE(0,TONE_CHANGESLOT_DURATION_PAUSE);
      }
      
      //clean up
      halStorageFinishTransaction(tid);
      //save newly loaded cfg
      memcpy(&currentConfigLoaded,&currentConfig,sizeof(generalConfig_t));
      tid = 0;
      ESP_LOGD(LOG_TAG,"----Config Switch Complete, loaded slot %s----",currentConfigLoaded.slotName);
    }
  }
}

/** @brief FUNCTIONAL TASK - Load another slot
 * 
 * This task is used to switch the configuration to another slot.
 * It is possible to load a slot by name or a previous/next/default slot.
 * 
 * @param param Task configuration
 * @see taskConfigSwitcherConfig_t*/
void task_configswitcher(taskConfigSwitcherConfig_t *param)
{
  EventBits_t uxBits = 0;
  //check for param struct
  if(param == NULL)
  {
    ESP_LOGE(LOG_TAG,"param is NULL ");
    while(1) vTaskDelay(100000/portTICK_PERIOD_MS);
    return;
  }
  //calculate array index of EventGroup array (each 4 VB have an own EventGroup)
  uint8_t evGroupIndex = param->virtualButton / 4;
  //calculate bitmask offset within the EventGroup
  uint8_t evGroupShift = param->virtualButton % 4;
  //final pointer to the EventGroup used by this task
  EventGroupHandle_t *evGroup = NULL;
  //ticks between task timeouts
  const TickType_t xTicksToWait = 2000 / portTICK_PERIOD_MS;
  
  
  //if we are in singleshot mode, we do not need to test
  //the virtual button groups (unused)
  if(param->virtualButton != VB_SINGLESHOT)
  {
    //check for valid parameter
    if(evGroupIndex >= NUMBER_VIRTUALBUTTONS)
    {
      ESP_LOGE(LOG_TAG,"virtual button group unsupported: %d ",evGroupIndex);
      while(1) vTaskDelay(100000/portTICK_PERIOD_MS);
      return;
    }
    
    //test if event groups are already initialized, otherwise exit immediately
    if(virtualButtonsOut[evGroupIndex] == 0)
    {
      ESP_LOGE(LOG_TAG,"uninitialized event group for virtual buttons, quitting this task");
      while(1) vTaskDelay(100000/portTICK_PERIOD_MS);
      return;
    } else {
      evGroup = virtualButtonsOut[evGroupIndex];
    }
  }
  
  while(1)
  {
    //wait for the flag or simply trigger on VB_SINGLESHOT
    if(param->virtualButton != VB_SINGLESHOT)
    {
      uxBits = xEventGroupWaitBits(evGroup,(1<<evGroupShift),pdTRUE,pdFALSE,xTicksToWait);
    }
    //trigger either when the bits are set or we are in singleshot mode
    if((uxBits & (1<<evGroupShift)) || param->virtualButton == VB_SINGLESHOT)
    {
      xQueueSend(config_switcher,&param->slotName,0);
      ESP_LOGD(LOG_TAG,"requesting slot switch by name: %s",param->slotName);
    }
    //FUNCTIONAL tasks in singleshot mode are required to return.
    if(param->virtualButton == VB_SINGLESHOT) return;
  }
}

/** @brief Initializing the config switching functionality.
 * 
 * The CONTINOUS task will be loaded to enable slot switches via the
 * task_configswitcher FUNCTIONAL task. 
 * @return ESP_OK if everything is fined, ESP_FAIL otherwise */
esp_err_t configSwitcherInit(void)
{
  //test if the config_switcher queue is initialized...
  if(config_switcher == NULL)
  {
    ESP_LOGE(LOG_TAG,"Error init config switcher, please create \
      config_switcher queue before calling configSwitcherInit()!");
    return ESP_FAIL;
  }
  
  //reset parameters memory
  for(uint8_t i = 0; i < NUMBER_VIRTUALBUTTONS*4; i++)
  {
    currentTaskParameters[i] = NULL;
    currentTaskParametersUpdate[i] = NULL;
  }
  
  //start configSwitcherTask
  if(xTaskCreate(configSwitcherTask,"configswitcher",CONFIGSWITCHERTASK_PERMANENT_STACKSIZE,(void *)NULL,
    HAL_CONFIG_TASK_PRIORITY,configswitcher_handle) != pdPASS)
  {
    ESP_LOGE(LOG_TAG,"error creating config switcher task, cannot proceed.");
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
