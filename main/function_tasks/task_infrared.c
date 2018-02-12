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

#include "task_infrared.h"

/** @brief Logging tag for this module */
#define LOG_TAG "task_IR"


/**@brief FUNCTIONAL TASK - Infrared command sending
 * 
 * This task is used to trigger an IR command on a VB action.
 * The IR command which should be sent is identified by a name.
 * 
 * @see taskInfraredConfig_t
 * @param param Task config
 * @see VB_SINGLESHOT
 * */
void task_infrared(taskInfraredConfig_t *param)
{
  //check for param struct
  if(param == NULL)
  {
    ESP_LOGE(LOG_TAG,"param is NULL ");
    while(1) vTaskDelay(100000/portTICK_PERIOD_MS);
    return;
  }
  //event bits used for pending on debounced buttons
  EventBits_t uxBits = 0;
  //calculate array index of EventGroup array (each 4 VB have an own EventGroup)
  uint8_t evGroupIndex = param->virtualButton / 4;
  //calculate bitmask offset within the EventGroup
  uint8_t evGroupShift = param->virtualButton % 4;
  //save virtualbutton
  uint vb = param->virtualButton;
  //final pointer to the EventGroup used by this task
  EventGroupHandle_t *evGroup = NULL;
  //local copy of IR command name
  char cmdName[SLOTNAME_LENGTH];
  strcpy(cmdName,param->cmdName);
  
  //do all the eventgroup checking only if this is a persistent task
  //-> virtualButton is NOT set to VB_SINGLESHOT
  if(param->virtualButton != VB_SINGLESHOT)
  {
    //check for correct offset
    if(evGroupIndex >= NUMBER_VIRTUALBUTTONS)
    {
      ESP_LOGE(LOG_TAG,"virtual button group unsupported: %d ",evGroupIndex);
      while(1) vTaskDelay(100000/portTICK_PERIOD_MS);
      return;
    }
    
    //test if event groups are already initialized, otherwise exit immediately
    while(virtualButtonsOut[evGroupIndex] == 0)
    {
      ESP_LOGE(LOG_TAG,"uninitialized event group, retry in 1s");
      vTaskDelay(1000/portTICK_PERIOD_MS);
    }
    //event group intialized, store for later usage 
    evGroup = virtualButtonsOut[evGroupIndex];
  }
  
  while(1)
  {
      //wait for the flag
      if(vb != VB_SINGLESHOT)
      {
        uxBits = xEventGroupWaitBits(evGroup,(1<<evGroupShift)|(1<<(evGroupShift+4)),pdTRUE,pdFALSE,1000);
      }
      //test for a valid set flag or trigger if we are using singleshot
      if((uxBits & (1<<evGroupShift)) || vb == VB_SINGLESHOT)
      {
        //local IR struct
        halIOIR_t *cfg = NULL;
        //transaction ID for IR data
        uint32_t tid;
        if(halStorageStartTransaction(&tid,20) == ESP_OK)
        {
          if(halStorageLoadIR(cmdName,cfg,tid) == ESP_OK)
          {
            //send pointer to IR send queue
            if(cfg != NULL)
            {
              SENDIRSTRUCT(cfg);
            } else {
              ESP_LOGE(LOG_TAG,"IR cfg is NULL!");
            }
            //create tone
            TONE(TONE_IR_SEND_FREQ,TONE_IR_SEND_DURATION);
          } else {
            ESP_LOGE(LOG_TAG,"Error loading IR cmd");
          }
        } else {
          ESP_LOGE(LOG_TAG,"Error starting transaction for IR cmd");
        }
      }

      //function tasks in single shot mode MUST return to its caller
      if(vb == VB_SINGLESHOT) return;
  }
}


/** @brief Reverse Parsing - get AT command for IR VB
 * 
 * This function parses the current configuration of a virtual button
 * to an AT command used to print the configuration.
 * @param output Output string, where the full AT command will be stored
 * @param cfg Pointer to current infrared configuration, used to parse.
 * @return ESP_OK if everything went fine, ESP_FAIL otherwise
 * */
esp_err_t task_infrared_getAT(char* output, void* cfg)
{
  taskInfraredConfig_t *conf = (taskInfraredConfig_t *)cfg;
  if(conf == NULL) return ESP_FAIL;
  //very easy in this case: just extract the slot name.
  sprintf(output,"AT IP %s\r\n",conf->cmdName);
  
  return ESP_OK;
}
