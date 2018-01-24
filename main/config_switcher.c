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
 * 
 * @todo Create getter/setter for current config (including fkt tasks & possibly the reloading)
 * @todo Add update config here, enabling updating configs (general & VB) from other parts
 * @todo Add save slot here
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

/** Task handle for the CONTINOUS task responsible for config switching */
TaskHandle_t configswitcher_handle;

/** Currently loaded configuration.*/
generalConfig_t currentConfig;

/** @brief CONTINOUS TASK - Config switcher task, internal config reloading
 * 
 * This task is used to change the full configuration of this device
 * After sending a request to the config_switcher queue, this
 * task is used to unload all virtual buttons tasks and initializing
 * the virtualbutton tasks with the new functionality.
 * @see config_switcher
 * @param params Not used, pass NULL.
 **/
void configSwitcherTask(void * params)
{
  char command[SLOTNAME_LENGTH];
  char tasksignature[8];
  
  uint32_t tid = 0;
  size_t vbparametersize = 0;
  TaskFunction_t newTaskCode = NULL;
  short stacksize = 0;
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
    vTaskDelay(1000/portTICK_PERIOD_MS); 
    if(xQueueReceive(config_switcher,command,1000/portTICK_PERIOD_MS) == pdTRUE)
    {
      //request storage access
      if(halStorageStartTransaction(&tid,20) != ESP_OK)
      {
        ESP_LOGE(LOG_TAG,"Cannot start storage transaction");
        continue;
      }
      
      //command received, load new slot:
      //__NEXT, __PREV, __DEFAULT
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
      } else {
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
      
      //perform task switching...
      for(uint8_t i = 0; i<(NUMBER_VIRTUALBUTTONS*4); i++)
      {
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
          case T_KEYBOARD: 
            newTaskCode = (TaskFunction_t)task_keyboard; 
            currentTaskParameters[i] = malloc(sizeof(taskKeyboardConfig_t));
            vbparametersize = sizeof(taskKeyboardConfig_t);
            //TODO: add additional memory for keyboard memory...
            stacksize = TASK_KEYBOARD_STACKSIZE; 
            ESP_LOGD(LOG_TAG,"creating new keyboard task on VB %d",i);
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
          currentConfig.virtualButtonConfig[i] = currentTaskParameters[i];
          //load VB config to memory
          if(vbparametersize > 0)
          {
            halStorageLoadGetVBConfigs(i,currentConfig.virtualButtonConfig[i], vbparametersize, tid);
          }
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
      
      //clean up
      halStorageFinishTransaction(tid);
      tid = 0;
      ESP_LOGD(LOG_TAG,"----------Config Switch Complete----------");
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
  EventBits_t uxBits;
  //check for param struct
  if(param == NULL)
  {
    ESP_LOGE(LOG_TAG,"param is NULL ");
    vTaskDelete(NULL);
    return;
  }
  //calculate array index of EventGroup array (each 4 VB have an own EventGroup)
  uint8_t evGroupIndex = param->virtualButton / 4;
  //calculate bitmask offset within the EventGroup
  uint8_t evGroupShift = param->virtualButton % 4;
  //final pointer to the EventGroup used by this task
  EventGroupHandle_t *evGroup;
  //ticks between task timeouts
  const TickType_t xTicksToWait = 2000 / portTICK_PERIOD_MS;
  
  //check for valid parameter
  if(evGroupIndex >= NUMBER_VIRTUALBUTTONS)
  {
    ESP_LOGE(LOG_TAG,"virtual button group unsupported: %d ",evGroupIndex);
    vTaskDelete(NULL);
    return;
  }
  
  //test if event groups are already initialized, otherwise exit immediately
  if(virtualButtonsOut[evGroupIndex] == 0)
  {
    ESP_LOGE(LOG_TAG,"uninitialized event group for virtual buttons, quitting this task");
    vTaskDelete(NULL);
    return;
  } else {
    evGroup = virtualButtonsOut[evGroupIndex];
  }
  
  while(1)
  {
    //wait for the flag
    uxBits = xEventGroupWaitBits(evGroup,(1<<evGroupShift),pdTRUE,pdFALSE,xTicksToWait);
    if(uxBits & (1<<evGroupShift))
    {
      xQueueSend(config_switcher,&param->slotName,0);
      ESP_LOGD(LOG_TAG,"requesting slot switch by name: %s",param->slotName);
      
    }
  }
}

/** Initializing the config switching functionality.
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

