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
 * @brief FUNCTIONAL TASK - Execute macros
 * 
 * This module is used as functional task for sending macros to the
 * command parser.
 * 
 * Macros in context of FLipMouse/FABI are a string of concatenated
 * AT commands, equal to normal AT commands sent by the host.
 * 
 * @note AT command separation is done by a semicolon (';')
 * @warning If you want to write the semicolon within a macro, use AT KP with the corresponding keycode!
 * @see task_macros
 * @see VB_SINGLESHOT
 */

#include "task_macros.h"

/** @brief Logging tag for this module */
#define LOG_TAG "macro"


/**@brief FUNCTIONAL TASK - Macro execution
 * 
 * This task is used to trigger macro on a VB action.
 * 
 * @see taskMacrosConfig_t
 * @param param Task config
 * @see VB_SINGLESHOT
 * */
void task_macro(taskMacrosConfig_t *param)
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
  //local copy of macro (much safer, caller might free the config)
  char macro[SLOTNAME_LENGTH];
  strncpy(macro,param->macro,SLOTNAME_LENGTH);
  
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
    
    //test if event groups are already initialized, otherwise exit immediately
    while(halSerialATCmds == NULL)
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
        int offset = 0;
        int start = 0;
        
        
        while(offset < SLOTNAME_LENGTH)
        {
          //end loop if terminators are detected.
          if(macro[offset] == '\r' || macro[offset] == '\n') break;
          
          //do we reach a command terminator?
          if(macro[offset] == ';')
          {
            //check if this character is escaped:
            if(offset > 0)
            {
              if(macro[offset-1] == '\\')
              {
                //if it escaped "\;", just continue.
                offset++;
                continue;
              }
            }
            
            //check if we hit an AT WA (wait)
            if(memcmp(&macro[start],"AT WA",5) == 0)
            {
              //if yes, delay this task.
              uint32_t time = strtol((char*)&(macro[start+6]),NULL,10);
              if(time < 30000)
              {
                vTaskDelay(time / portTICK_PERIOD_MS);
              } else {
                ESP_LOGE(LOG_TAG,"Hit AT WA with a delay time too high: %d",time);
              }
            } else {
              //if not an AT WA, allocate memory and send to queue.
              atcmd_t command;
              uint16_t length = offset-start;
              uint8_t *buffer = malloc(sizeof(uint8_t)*(length+1));
              if(buffer != NULL)
              {
                //copy data
                memcpy(buffer,&macro[start],length);
                //terminate
                buffer[length] = 0;
                //save to queue struct
                command.buf = buffer;
                command.len = length;
                ESP_LOGD(LOG_TAG,"Sent AT cmd: %s",buffer);
                //send to queue, wait maximum 10 ticks (100ms) for a free space.
                if(xQueueSend(halSerialATCmds,(void*)&command,10) != pdTRUE)
                {
                  ESP_LOGE(LOG_TAG,"Cmd queue is full, cannot send command");
                }
                //we do NOT need a taskYIELD here, because ESP32 is running with preemption:
                //https://www.freertos.org/a00020.html#taskYIELD
                //taskYIELD();
              } else {
                ESP_LOGE(LOG_TAG,"Cannot allocate memory for command!");
              }
            }
            
            //save new start position for next command
            start = offset + 1;
          }
          
          //go to next character
          offset++;
        }
      }

      //function tasks in single shot mode MUST return to its caller
      if(vb == VB_SINGLESHOT) return;
  }
}

/** @brief Reverse Parsing - get AT command for macro VB
 * 
 * This function parses the current configuration of a virtual button
 * to an AT command used to print the configuration.
 * @param output Output string, where the full AT command will be stored
 * @param cfg Pointer to current macro configuration, used to parse.
 * @return ESP_OK if everything went fine, ESP_FAIL otherwise
 * */
esp_err_t task_macro_getAT(char* output, void* cfg)
{
  taskMacrosConfig_t *conf = (taskMacrosConfig_t *)cfg;
  if(conf == NULL) return ESP_FAIL;
  //very easy in this case: just extract the macro list of AT cmds.
  sprintf(output,"AT MA %s\r\n",conf->macro);
  
  return ESP_OK;
}
