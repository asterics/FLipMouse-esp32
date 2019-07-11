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
 * @brief Event Handler - VB general actions
 * 
 * This module is used as an event handler for all VB commands actions,
 * EXCEPT HID (this is done in handler_hid).
 * Currently implemented actions:
 * * IR command sending
 * * Macro execution
 * * Calibration
 * * Slot switching
 * @note Currently, we use the system event queue (because there is already
 * a task attached). Maybe we switch to an unique one.
 */
 
#include "handler_vb.h"

/** @brief Logging tag for this module */
#define LOG_TAG "handler_vb"
/** @brief Set a global log limit for this file */
#define LOG_LEVEL_VB ESP_LOG_INFO

/** @brief Beginning of all VB commands
 * 
 * This pointer is the beginning of the VB command chain.
 * Each time an active VB is triggered, the handler_vb walks through this
 * chained list for a corresponding command and triggers the given action.
 * 
 * Adding a new command is done via handler_vb_addCmd, the full list
 * is freed and cleared via handler_vb_clearCmds.
 * 
 * @see handler_vb
 * @see handler_vb_addCmd
 * @see handler_vb_clearCmds*/
static vb_cmd_t *cmd_chain = NULL;

/** @brief Synchronization mutex for accessing the VB command chain */
SemaphoreHandle_t vbCmdSem = NULL;

/** @brief Bitmap for active VBs. Corresponding bit will be set, if active. 
 * @see handler_vb_active */
static uint64_t vb_active = 0;

/**
 * @brief VB event handler, triggering VB general actions.
 *
 * @param event_handler_arg handler specific arguments
 * @param event_base event base, here is fixed to VB_EVENT
 * @param event_id event id, subscribed to all events
 * @param event_data Contains the VB number
 */
static void handler_vb(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  //if we don't have a stable config, simply return...
  if((xEventGroupGetBits(systemStatus) & SYSTEM_STABLECONFIG) == 0) return;
  //still commands to be processed, shouldn't continue
  if((xEventGroupGetBits(systemStatus) & SYSTEM_EMPTY_CMD_QUEUE) == 0) return;

  //use the mutex to ensure a valid chained list. 
  if(xSemaphoreTake(vbCmdSem,4) != pdTRUE)
  {
    ESP_LOGW(LOG_TAG,"VB mutex not free for handler");
    return;
  }
  //if command chain is empty, we cannot do anything.
  if(cmd_chain == NULL) 
  {
    xSemaphoreGive(vbCmdSem);
    return;
  }
  
  uint8_t vb = 0;
  uint32_t count = 0;
  switch(event_id)
  {
    case VB_PRESS_EVENT: vb |= 0x80; //set uppermost bit for press event.
    case VB_RELEASE_EVENT:
      break;
    default: //might be another type of event, we don't care of.
      return;
  }
  
  //we need an VB number as event data.
  if(event_data == 0)
  {
    ESP_LOGE(LOG_TAG,"Empty event data, cannot proceed!");
    return;
  }
  
  vb |= (*((uint32_t*) event_data)) & 0x7F;
  
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
            #if LOG_LEVEL_VB >= ESP_LOG_DEBUG
            ESP_LOGD(LOG_TAG,"Sent macro %s for VB %d", (char*)current->cmdparam, vb & 0x7F);
            #endif
            fct_macro(current->cmdparam);
          }
          break;
        case T_CONFIGCHANGE:
          if(current->cmdparam == NULL)
          {
            ESP_LOGE(LOG_TAG,"Param is null, cannot request config change");
          } else {
            #if LOG_LEVEL_VB >= ESP_LOG_DEBUG
            ESP_LOGD(LOG_TAG,"Sent CFG change %s for VB %d", (char*)current->cmdparam, vb & 0x7F);
            #endif
            xQueueSend(config_switcher,(void*)current->cmdparam,(TickType_t)10);
          }
          break;
        case T_CALIBRATE:
          halAdcCalibrate();
          break;
        case T_SENDIR:
          if(current->cmdparam == NULL)
          {
            ESP_LOGE(LOG_TAG,"Param is null, cannot send IR");
          } else {
            fct_infrared_send(current->cmdparam);
          }
          break;
        case T_MQTT:
          if(current->cmdparam == NULL)
          {
            ESP_LOGE(LOG_TAG,"Param is null, cannot send MQTT publish");
          } else {
            taskMQTTPublish(current->cmdparam);
          }
          break;
        case T_REST:
          if(current->cmdparam == NULL)
          {
            ESP_LOGE(LOG_TAG,"Param is null, cannot send REST message");
          } else {
            taskREST(current->cmdparam);
          }
          break;
        default:
          ESP_LOGE(LOG_TAG,"Unknown VB cmd type");
          break;
      }
    }
    current = current->next;
  }
  #if LOG_LEVEL_VB >= ESP_LOG_DEBUG
  if(count == 0) ESP_LOGD(LOG_TAG,"Sent %d cmds for VB %d", count, vb & 0x7F);
  #endif
  if(count != 0) ESP_LOGI(LOG_TAG,"Sent %d cmds for VB %d", count, vb & 0x7F);
  xSemaphoreGive(vbCmdSem);
}

/** @brief Init for the VB handler
 * 
 * We create the mutex and add handler_vb to the system event queue.
 * @return ESP_OK on success, ESP_FAIL on an error.*/
esp_err_t handler_vb_init(void)
{
  //init VB mutex
  if(vbCmdSem != NULL) vSemaphoreDelete(vbCmdSem);
  vbCmdSem = xSemaphoreCreateMutex();
  if(vbCmdSem == NULL)
  {
    ESP_LOGE(LOG_TAG,"Cannot create mutex, exiting!");
    return ESP_FAIL;
  }
  xSemaphoreGive(vbCmdSem);
  //set log level to given log level
  esp_log_level_set(LOG_TAG,LOG_LEVEL_VB);
  
  return esp_event_handler_register(VB_EVENT,ESP_EVENT_ANY_ID,handler_vb,NULL);
}

/** @brief Remove command for a virtual button
 * 
 * This method removes any command from the list of commands
 * which are assigned to this VB.
 * 
 * @param vb VB which should be removed
 * @return ESP_OK if deleted, ESP_FAIL if not in list */
esp_err_t handler_vb_delCmd(uint8_t vb)
{
  //existing chain
  vb_cmd_t *current = cmd_chain;
  vb_cmd_t *prev = NULL;
  uint count = 0;

  heap_caps_check_integrity_all(true);
  //do as long as we don't have a null pointer
  while(current != NULL)
  {
    //if the VB number matches (discarding press/release flag)
    if((current->vb & 0x7F) == (vb & 0x7F))
    {
      //set pointer from previous element to next one
      //but only if we are not at the head (no previous element)
      if(prev != NULL) prev->next = current->next;
      //if no previous element -> replace 
      else cmd_chain = current->next;
      //free an AT string
      if(current->atoriginal != NULL) free(current->atoriginal);
      //if set, free param string
      if(current->cmdparam != NULL) free(current->cmdparam);
      //free this element
      free(current);
      //just begin at the front again (easiest way if we removed the head)
      current = cmd_chain;
      prev = NULL;
      if((vb & 0x7F) <= 63) vb_active &= ~(1<<(vb & 0x7F)); //delete active flag
    } else { //does not match
      //set pointers to next element
      prev = current;
      current = current->next;
    }
  }
  if(count != 0) return ESP_OK;
  else return ESP_FAIL;
}

/** @brief Add a new VB command for a virtual button
 * 
 * This method adds the given VB command to the list of VB commands
 * which will be processed if the corresponding VB is triggered.
 * 
 * @note Highest bit determines press/release action. If set, it is a press!
 * @note If VB number is set to VB_SINGLESHOT, the command will be sent immediately.
 * @note We will malloc for each command here. To free the memory, call handler_vb_clearCmds .
 * @param newCmd New command to be added or triggered if vb is VB_SINGLESHOT
 * @param replace If set to != 0, any previously assigned command is removed from list.
 * @return ESP_OK if added, ESP_FAIL if not added (out of memory) */
esp_err_t handler_vb_addCmd(vb_cmd_t *newCmd, uint8_t replace)
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
  
  //take mutex for modifying
  if(xSemaphoreTake(vbCmdSem,50) != pdTRUE)
  {
    ESP_LOGE(LOG_TAG,"VB mutex not free for adding");
    return ESP_FAIL;
  }
  
  //existing chain, add to end
  vb_cmd_t *current = cmd_chain;
  int count = 0;
  
  //debugging...
  #if LOG_LEVEL_VB >= ESP_LOG_DEBUG
  while(current!=NULL)
  {
    count++;
    ESP_LOGD(LOG_TAG,"%d:%2d:0x%X",count,current->vb,(uint32_t)current);
    current = current->next;
  }
  current = cmd_chain;
  count = 0;
  #endif
  
  //if set, remove any previously set commands.
  if(replace) handler_vb_delCmd(newCmd->vb);
  
  //allocate new command
  current = cmd_chain;
  vb_cmd_t *new;
  new = malloc(sizeof(vb_cmd_t));
  
  if(new != NULL)
  {
    memcpy(new, newCmd, sizeof(vb_cmd_t));
    //save pointer of new config to end of chain
    new->next = NULL;
    //if we don't have a head, use this one
    if(cmd_chain == NULL) {
      cmd_chain = new;
    } else {
      //otherwise append to tail.
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
    if((new->vb & 0x7F) <= 63) vb_active |= (1<<(new->vb & 0x7F)); //set active flag
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

/** @brief Get current root of VB command chain
 * @warning Modifying this chain without acquiring the vbCmdSem could result in
 * undefined behaviour!.
 * @return Pointer to root of VB chain
 */
vb_cmd_t *handler_vb_getCmdChain(void)
{
  return cmd_chain;
}

/** @brief Set current root of VB command chain
 * @warning By using this function, previously used VB commands are cleared!
 * @param chain Pointer to root of VB chain
 * @return ESP_OK if chain is saved, ESP_FAIL otherwise (lock was not free, deleting old chain failed..)
 */
esp_err_t handler_vb_setCmdChain(vb_cmd_t *chain)
{
  esp_err_t ret = ESP_OK;
  
  //check if chain is currently active,
  //if yes clear it before setting a new one
  if(cmd_chain != NULL)
  {
    ret = handler_vb_clearCmds();
    if(ret != ESP_OK)
    {
      ESP_LOGE(LOG_TAG,"Cannot clear old chain");
      return ret;
    }
  }
  
  //enter critical section for setting new cmd chain
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
  }
  //save the pointer
  cmd_chain = chain;
  //release mutex
  xSemaphoreGive(vbCmdSem);
  return ESP_OK;
}

/** @brief Clear all stored VB commands.
 * 
 * This method clears all stored VB commands and frees the allocated memory.
 * 
 * @return ESP_OK if commands are cleared, ESP_FAIL otherwise
 * */
esp_err_t handler_vb_clearCmds(void)
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
  
  #if LOG_LEVEL_VB >= ESP_LOG_INFO
  ESP_LOGI(LOG_TAG,"Cleared %d VB cmds",count);
  #endif

  cmd_chain = NULL;
  vb_active = 0;
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
esp_err_t handler_vb_getAT(char* output, uint8_t vb)
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
        ESP_LOGI(LOG_TAG,"BM%02d: %s",vb,output);
        #endif
        xSemaphoreGive(vbCmdSem);
        return ESP_OK;
      }
    }
    current = current->next;
  }
  ESP_LOGD(LOG_TAG,"No AT command found");
  xSemaphoreGive(vbCmdSem);
  return ESP_FAIL;
}


/** @brief Check if a VB is active in this handler
 * 
 * This function returns true if a given vb is active in this handler
 * (is located in the command chain with a given command).
 * 
 * @param vb Number of virtual button to check
 * @return true if active, false if not
 * @note We don't care if press/release is active here. Any associated action will return true. */
bool handler_vb_active(uint8_t vb)
{
  //we check for VB_MAX (firmware specfic) and 63 (size of vb_active)
  if(vb >= VB_MAX || vb >= 63)
  {
    ESP_LOGE(LOG_TAG,"Cannot detect state of VB %d, out of range!",vb);
    return false;
  } else {
    if((vb_active & (1<<(vb&0x7F))) != 0) return true;
    else return false;
  }
  return false;
}

