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
 * @brief CONTINOUS TASK - VB handling
 * 
 * This module is used as continous task for all VB command handling,
 * EXCEPT HID (this is done in task_hid).
 * Currently implemented actions:
 * * IR command sending
 * * Macro execution
 * * Calibration
 * * Slot switching
 */
 
#include "task_vb.h"

/** @brief Logging tag for this module */
#define LOG_TAG "task_vb"
/** @brief Set a global log limit for this file */
#define LOG_LEVEL_VB ESP_LOG_INFO

/** @brief Beginning of all VB commands
 * 
 * This pointer is the beginning of the VB command chain.
 * Each time an active VB is triggered, the task_vb walks through this
 * chained list for a corresponding command and triggers the given action.
 * 
 * Adding a new command is done via task_vb_addCmd, the full list
 * is freed and cleared via task_vb_clearCmds.
 * 
 * @see task_vb
 * @see task_vb_addCmd
 * @see task_vb_clearCmds*/
static vb_cmd_t *cmd_chain = NULL;

/** @brief Mask of active VBs, which are located in the VB cmd_chain. */
uint8_t activeVBs[NUMBER_VIRTUALBUTTONS];

/** @brief Synchronization mutex for accessing the VB command chain */
SemaphoreHandle_t vbCmdSem = NULL;


/** @brief CONTINOUS TASK - Trigger VB actions
 * 
 * This task is used to trigger all active VB commands (except HID
 * commands!), which are assigned to virtual buttons.
 * 
 * @param param Unused */
void task_vb(void *param)
{
  //init VB mutex
  if(vbCmdSem != NULL) vSemaphoreDelete(vbCmdSem);
  vbCmdSem = xSemaphoreCreateMutex();
  if(vbCmdSem == NULL)
  {
    ESP_LOGE(LOG_TAG,"Cannot create mutex, exiting!");
    vTaskDelete(NULL);
  }
  xSemaphoreGive(vbCmdSem);
  //set log level to given log level
  esp_log_level_set(LOG_TAG,LOG_LEVEL_VB);
  
  while(1)
  {
    //wait 50ms
    vTaskDelay(50 / portTICK_PERIOD_MS);
    
    if(xSemaphoreTake(vbCmdSem,40) != pdTRUE)
    {
      ESP_LOGW(LOG_TAG,"VB mutex not free for task");
      continue;
    }
    
    //if command chain is not initialized, we cannot do anything.
    if(cmd_chain == NULL) 
    {
      xSemaphoreGive(vbCmdSem);
      continue;
    }
    
    //compare each event group against activated VBs
    for(int i = 0; i<NUMBER_VIRTUALBUTTONS; i++)
    {
      EventBits_t evBits = xEventGroupGetBits(virtualButtonsOut[i]);
      if(evBits & activeVBs[i])
      {
        //check each bit if it is set
        for(int j = 0; j<8; j++)
        {
          //if active bit
          if(evBits & (1<<j))
          {
            //create vb number of current iterators
            //first bit is setting press or release (1 for press, 0 for release)
            //remaining 7 bits are according to VB_* defines
            
            //4 VBs each event group (iterator i)
            //lower nibble is press action, higher nibble is release
            uint8_t vb = i*4 + (j%4);
            if(j<4) vb |= (1<<7);
            
            //count sent commands
            int count = 0;
            
            //begin with head of chain
            vb_cmd_t *current = cmd_chain;
            //iterate through all available vb cmds
            while(current != NULL)
            {
              //send VB command(s), if VB matches
              //this way, we can do more actions on one VB
              if(current->vb == vb)
              {
                count++;
                /* determine action to be triggered */
                switch(current->cmd)
                {
                  case T_MACRO:
                    if(current->cmdparam == NULL)
                    {
                      ESP_LOGE(LOG_TAG,"Param is null, cannot execute macro");
                    } else {
                      fct_macro(current->cmdparam);
                    }
                    break;
                  case T_CONFIGCHANGE:
                    if(current->cmdparam == NULL)
                    {
                      ESP_LOGE(LOG_TAG,"Param is null, cannot request config change");
                    } else {
                      xQueueSend(config_switcher,(void*)current->cmdparam,(TickType_t)10);
                    }
                  case T_CALIBRATE:
                    halAdcCalibrate();
                  case T_SENDIR:
                    if(current->cmdparam == NULL)
                    {
                      ESP_LOGE(LOG_TAG,"Param is null, cannot send IR");
                    } else {
                      ///@todo Send IR command.
                    }
                  default:
                    ESP_LOGE(LOG_TAG,"Unknown VB cmd type");
                    break;
                }
              }
              current = current->next;
            }
            #if LOG_LEVEL_VB >= ESP_LOG_DEBUG
            ESP_LOGD(LOG_TAG,"Sent %d cmds for VB %d", count, vb & 0x7F);
            #endif
          }
        }
        //clear "consumed" flags
        xEventGroupClearBits(virtualButtonsOut[i],activeVBs[i]);
      }
    }
    xSemaphoreGive(vbCmdSem);
  }
}

/** @brief Add a new VB command for a virtual button
 * 
 * This method adds the given VB command to the list of VB commands
 * which will be processed if the corresponding VB is triggered.
 * 
 * @note Highest bit determines press/release action. If set, it is a press!
 * @note If VB number is set to VB_SINGLESHOT, the command will be sent immediately.
 * @note We will malloc for each command here. To free the memory, call task_vb_clearCmds .
 * @param newCmd New command to be added or triggered if vb is VB_SINGLESHOT
 * @param replace If set to != 0, any previously assigned command is removed from list.
 * @return ESP_OK if added, ESP_FAIL if not added (out of memory) */
esp_err_t task_vb_addCmd(vb_cmd_t *newCmd, uint8_t replace)
{
  //sanitizing...
  if(newCmd == NULL)
  {
    ESP_LOGE(LOG_TAG,"newCmd is NULL");
    return ESP_FAIL;
  }
  if(vbCmdSem == NULL)
  {
    ESP_LOGE(LOG_TAG,"vbCmdSem is NULL");
    return ESP_FAIL;
  }
  if((newCmd->vb & 0x7F) >= VB_MAX)
  {
    ESP_LOGE(LOG_TAG,"newCmd->vb out of range");
    return ESP_FAIL;
  }
  if(((newCmd->vb & 0x7F) / 4) >= NUMBER_VIRTUALBUTTONS)
  {
    ESP_LOGE(LOG_TAG,"array out of bounds -> unrecoverable!");
    return ESP_FAIL;
  }
  
  //take mutex for modifying
  if(xSemaphoreTake(vbCmdSem,50) != pdTRUE)
  {
    ESP_LOGE(LOG_TAG,"VB mutex not free for adding");
    return ESP_FAIL;
  }
  
  //existing chain, add to end
  vb_cmd_t *current = cmd_chain;
  vb_cmd_t *prev = NULL;
  int count = 0;
  
  //if set, remove any previously set commands.
  if(replace)
  {
    #if LOG_LEVEL_VB >= ESP_LOG_DEBUG
    ESP_LOGD(LOG_TAG,"Active vb before replace @%d: %d",(newCmd->vb & 0x7F) / 4,activeVBs[(newCmd->vb & 0x7F) / 4]);
    #endif
    //do as long as we don't have a null pointer
    while(current != NULL)
    {
      //if the VB number matches (discarding press/release flag)
      if((current->vb & 0x7F) == (newCmd->vb & 0x7F))
      {
        //set pointer from previous element to next one
        //but only if we are not at the head (no previous element)
        if(prev != NULL) prev->next = current->next;
        //if no previous element -> replace 
        else cmd_chain = current->next;
        
        //remove the flags from activeVBs
        if((current->vb & 0x80) != 0) activeVBs[(current->vb & 0x7F) / 4] &= ~(1<<((current->vb & 0x7F)%4));
        else activeVBs[(current->vb & 0x7F) / 4] &= ~(1<<(((current->vb & 0x7F)%4)+4));
        
        //free an AT string
        if(current->atoriginal != NULL) free(current->atoriginal);
        //if set, free param string
        if(current->cmdparam != NULL) free(current->cmdparam);
        //free this element
        free(current);
        //just begin at the front again (easiest way if we removed the head)
        current = cmd_chain;
        prev = NULL;
      } else { //does not match
        //set pointers to next element
        prev = current;
        current = current->next;
      }
    }
    #if LOG_LEVEL_VB >= ESP_LOG_DEBUG
    ESP_LOGD(LOG_TAG,"Active vb after replace @%d: %d",(newCmd->vb & 0x7F) / 4,activeVBs[(newCmd->vb & 0x7F) / 4]);
    #endif
  }
  
  //allocate new command
  current = cmd_chain;
  vb_cmd_t *new;
  new = malloc(sizeof(*new));
  
  if(new != NULL)
  {
    memcpy(new, newCmd, sizeof(*new));
    //activate corresponding VB (task compares this variable against event flags)
    #if LOG_LEVEL_VB >= ESP_LOG_DEBUG
    ESP_LOGD(LOG_TAG,"Old value @%d: %d",(new->vb & 0x7F) / 4,activeVBs[(new->vb & 0x7F) / 4]);
    #endif
    
    if((new->vb & 0x80) != 0) activeVBs[(new->vb & 0x7F) / 4] |= 1<<((new->vb & 0x7F)%4);
    else activeVBs[(new->vb & 0x7F) / 4] |= 1<<(((new->vb & 0x7F)%4)+4);
    
    #if LOG_LEVEL_VB >= ESP_LOG_DEBUG
    ESP_LOGD(LOG_TAG,"New value @%d: %d",(new->vb & 0x7F) / 4,activeVBs[(new->vb & 0x7F) / 4]);
    #endif
    //save pointer of new config to end of chain
    new->next = NULL;
    
    if(cmd_chain == NULL) {
      cmd_chain = new;
    } else {
      while(current->next != NULL)
      {
        #if LOG_LEVEL_VB >= ESP_LOG_DEBUG
        ESP_LOGD(LOG_TAG,"Nr %d, @0x%8X",count,(uint32_t)current);
        #endif
        count++;
        current = current->next;
      }
      current->next = new;
    }
    count++;
    #if LOG_LEVEL_VB >= ESP_LOG_DEBUG
    ESP_LOGI(LOG_TAG,"Added new cmd nr %d, new: 0x%8X, prev: 0x%8X",count,(uint32_t)new,(uint32_t)current);
    #endif
    
    current = new;
    xSemaphoreGive(vbCmdSem);
    return ESP_OK;
  } else {
    ESP_LOGE(LOG_TAG,"Cannot allocate memory for new VB cmd!");
    xSemaphoreGive(vbCmdSem);
    return ESP_FAIL;
  }

  //should not be here, but return ok anyway
  xSemaphoreGive(vbCmdSem);
  return ESP_OK;
}

/** @brief Enter critical section for modifying/reading the command chain
 * 
 * @warning task_vb_exit_critical MUST be called, otherwise VB commands are blocked.
 * @return ESP_OK if it is safe to proceed, ESP_FAIL otherwise (lock was not free in 10 ticks)
 */
esp_err_t task_vb_enterCritical(void)
{
  if(vbCmdSem == NULL)
  {
    ESP_LOGE(LOG_TAG,"vbCmdSem is NULL");
    return ESP_FAIL;
  }
  //take mutex for modifying
  if(xSemaphoreTake(vbCmdSem,50) != pdTRUE)
  {
    ESP_LOGE(LOG_TAG,"cannot enter critical section");
    return ESP_FAIL;
  } else return ESP_OK;
}

/** @brief Exit critical section for modifying/reading the command chain
 * @see task_vb_enterCritical
 */
void task_vb_exitCritical(void)
{
  //release mutex
  xSemaphoreGive(vbCmdSem);
}

/** @brief Get current root of VB command chain
 * @note Please call task_vb_enterCritical before reading from cmd chain.
 * @return Pointer to root of VB chain
 */
vb_cmd_t *task_vb_getCmdChain(void)
{
  return cmd_chain;
}

/** @brief Set current root of VB command chain
 * @note Please do NOT call task_vb_enterCritical before setting the new chain!
 * @warning By using this function, previously used VB commands are cleared!
 * @param chain Pointer to root of VB chain
 * @return ESP_OK if chain is saved, ESP_FAIL otherwise (lock was not free, deleting old chain failed..)
 */
esp_err_t task_vb_setCmdChain(vb_cmd_t *chain)
{
  esp_err_t ret = ESP_OK;
  
  //check if chain is currently active,
  //if yes clear it before setting a new one
  if(cmd_chain != NULL)
  {
    ret = task_vb_clearCmds();
    if(ret != ESP_OK)
    {
      ESP_LOGE(LOG_TAG,"Cannot clear old chain");
      return ret;
    }
  }
  
  //enter critical section for setting new cmd chain
  ret = task_vb_enterCritical();
  if(ret != ESP_OK) return ret;
  //save the pointer
  cmd_chain = chain;
  
  //clear active flags
  for(int i = 0; i<NUMBER_VIRTUALBUTTONS; i++) activeVBs[i] = 0;
  //set vb flags accordingly, to activate VB task.
  while(chain != NULL)
  {
    if((chain->vb & 0x80) != 0) activeVBs[(chain->vb & 0x7F) / 4] |= 1<<((chain->vb & 0x7F)%4);
    else activeVBs[(chain->vb & 0x7F) / 4] |= 1<<(((chain->vb & 0x7F)%4)+4);
    chain = chain->next;
  }
  
  #if LOG_LEVEL_VB >= ESP_LOG_INFO
  ESP_LOGI(LOG_TAG,"Set new chain, VBs active:");
  for(int i = 0; i<NUMBER_VIRTUALBUTTONS; i++) ESP_LOGI(LOG_TAG,"%d: 0x%2X", i, activeVBs[i]);
  #endif
  
  task_vb_exitCritical();
  return ESP_OK;
}

/** @brief Clear all stored VB commands.
 * 
 * This method clears all stored VB commands and frees the allocated memory.
 * 
 * @return ESP_OK if commands are cleared, ESP_FAIL otherwise
 * */
esp_err_t task_vb_clearCmds(void)
{
  if(cmd_chain == NULL)
  {
    ESP_LOGW(LOG_TAG,"VB cmds already empty");
    return ESP_FAIL;
  }
  if(vbCmdSem == NULL)
  {
    ESP_LOGE(LOG_TAG,"vbCmdSem is NULL");
    return ESP_FAIL;
  }
  
  //take mutex for modifying
  if(xSemaphoreTake(vbCmdSem,50) != pdTRUE)
  {
    ESP_LOGE(LOG_TAG,"VB mutex not free for clearing");
    return ESP_FAIL;
  }
  
  //pointers for next and current command
  vb_cmd_t *next = NULL;
  vb_cmd_t *current = cmd_chain;
  int count = 0;
  
  do {
    //load next block
    next = current->next;
    //if set, free the original AT command
    if(current->atoriginal != NULL) free(current->atoriginal);
    current->atoriginal = NULL;
    //if set, free param string
    if(current->cmdparam != NULL) free(current->cmdparam);
    current->cmdparam = NULL;
    //free the current one
    free(current);
    //count for statistics
    count++;
    //previous next is current for next while iteration
    current = next;
    //break the loop if current is NULL (we reached end of chain)
  } while(current != NULL);
  
  //clear active flags
  for(int i = 0; i<NUMBER_VIRTUALBUTTONS; i++) activeVBs[i] = 0;
  
  #if LOG_LEVEL_VB >= ESP_LOG_INFO
  ESP_LOGI(LOG_TAG,"Cleared %d VB cmds",count);
  #endif

  cmd_chain = NULL;
  //release mutex
  xSemaphoreGive(vbCmdSem);
  return ESP_OK;
}

/** @brief Reverse Parsing - get AT command of a given VB
 * 
 * This function parses the current configuration of a virtual button
 * to an AT command used to print the configuration.
 * @param output Output string, where the full AT command will be stored
 * @param vb Number of virtual button for getting the AT command
 * @return ESP_OK if everything went fine, ESP_FAIL otherwise
 * */
esp_err_t task_vb_getAT(char* output, uint8_t vb)
{
  if(vbCmdSem == NULL)
  {
    ESP_LOGE(LOG_TAG,"vbCmdSem is NULL");
    return ESP_FAIL;
  }
  
  if(cmd_chain == NULL)
  {
    ESP_LOGE(LOG_TAG,"Chain empty!");
    return ESP_FAIL;
  }
  //take mutex for reading
  if(xSemaphoreTake(vbCmdSem,50) != pdTRUE)
  {
    ESP_LOGE(LOG_TAG,"VB mutex not free for getting");
    return ESP_FAIL;
  }
  
  //pointers for current command
  vb_cmd_t *current = cmd_chain;
  
  //iterate through all available vb cmds
  while(current != NULL)
  {
    //is this the requested button?
    if((current->vb & 0x7F) == vb)
    {
      //check if we found an AT string
      if(current->atoriginal != NULL)
      {
        strncpy(output,current->atoriginal,ATCMD_LENGTH);
        #if LOG_LEVEL_VB >= ESP_LOG_INFO
        ESP_LOGI(LOG_TAG,"Found original AT cmd: %s",output);
        #endif
        xSemaphoreGive(vbCmdSem);
        return ESP_OK;
      }
    }
    current = current->next;
  }
  ESP_LOGW(LOG_TAG,"No AT command found");
  xSemaphoreGive(vbCmdSem);
  return ESP_FAIL;
}

