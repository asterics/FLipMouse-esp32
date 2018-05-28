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
 * @brief FUNCTIONAL TASK - trigger keyboard actions
 * 
 * This file contains the task implementation for keyboard key triggering.
 * This task is started by the configuration task and it is assigned
 * to one virtual button or triggered as single shot. As soon as this button is triggered, 
 * the configured keys are sent to either USB, BLE or both.
 * This task can be deleted all the time to change the configuration of
 * one button.
 * 
 * Keycode configuration is set via taskKeyboardConfig_t.
 * 
 * @warning Parsing of key identifiers and text is NOT done here. Due to
 * performance reasons, parsing is done prior to initialising/calling this task.
 * 
 * @see taskKeyboardConfig_t
 * @see VB_SINGLESHOT
 * */

#include "task_kbd.h"

/** @brief Logging tag for this module **/
#define LOG_TAG "task_kbd"

/** @brief Global active keycode array */
static uint8_t keycode_arr[8] = {'K',0,0,0,0,0,0,0};



/** @brief Reverse Parsing - get AT command for keyboard VB
 * 
 * This function parses the current configuration of a virtual button
 * to an AT command used to print the configuration.
 * @param output Output string, where the full AT command will be stored
 * @param cfg Pointer to current keyboard configuration, used to parse.
 * @return ESP_OK if everything went fine, ESP_FAIL otherwise
 * */
esp_err_t task_keyboard_getAT(char* output, void* cfg)
{
  if(cfg == NULL) return ESP_FAIL;
  taskKeyboardConfig_t *conf = (taskKeyboardConfig_t*)cfg;
  uint8_t paramsoffset = 0;
  char identifierarr[30];
  sprintf(output,"AT ");
  
  //determine the AT command
  switch(conf->type)
  {
    case PRESS:
      strcat(output,"KH ");
      break;
    case RELEASE:
      strcat(output,"KR ");
      break;
    case PRESS_RELEASE_BUTTON:
      strcat(output,"KP ");
      break;
    case WRITE:
      strcat(output,"KW ");
      break;
    default: return ESP_FAIL;
  }
  
  if(conf->type != WRITE)
  {
    //add the parameter for KP/KR/KH
    while(conf->keycodes_text[paramsoffset] != 0 && paramsoffset < TASK_KEYBOARD_PARAMETERLENGTH)
    {
      //0xE0, E2, E4 for modifiers, 0xF0 for keycodes
      //append key identifiers
      
      //this is a normal keycode
      if((conf->keycodes_text[paramsoffset] & 0x00FF) != 0)
      {
        if(parseKeycodeToIdentifier((conf->keycodes_text[paramsoffset] & 0x00FF) | 0xF000,identifierarr,30) == 1)
        {
          strcat(output, identifierarr);
          strcat(output, " ");
        } else {
          ESP_LOGW(LOG_TAG,"Cannot find keycode identifier for 0x%04X @ %d",conf->keycodes_text[paramsoffset],paramsoffset);
        }
      }
      //this is a modifier
      if((conf->keycodes_text[paramsoffset] & 0xFF00) != 0)
      {
        if(parseKeycodeToIdentifier(((conf->keycodes_text[paramsoffset] & 0xFF00) >> 8) | 0xE000,identifierarr,30) == 1)
        {
          strncat(output, identifierarr,strnlen(identifierarr,30));
          strcat(output, " ");
        } else if (parseKeycodeToIdentifier(((conf->keycodes_text[paramsoffset] & 0xFF00) >> 8) | 0xE200,identifierarr,30) == 1) {
          strncat(output, identifierarr,strnlen(identifierarr,30));
          strcat(output, " ");
        } else if (parseKeycodeToIdentifier(((conf->keycodes_text[paramsoffset] & 0xFF00) >> 8) | 0xE400,identifierarr,30) == 1) {
          strncat(output, identifierarr,strnlen(identifierarr,30));
          strcat(output, " ");
        } else {
          ESP_LOGW(LOG_TAG,"Cannot find keycode identifier for 0x%04X @ %d",conf->keycodes_text[paramsoffset],paramsoffset);
        }
      }

      paramsoffset++;
    }
  } else {
    //for write we get the original bytes from back to front.
    for(uint8_t i = 1; i<=TASK_KEYBOARD_PARAMETERLENGTH;i++)
    {
      if(conf->keycodes_text[TASK_KEYBOARD_PARAMETERLENGTH-i] == 0 && i>1) break;
      output[i+5] = (char)conf->keycodes_text[TASK_KEYBOARD_PARAMETERLENGTH-i];
    }
  }
  return ESP_OK;
}

/** @brief Helper function sending to queues depending on connection type
 * 
 * This method is used to send data to USB/BLE queues, depending
 * on the bits of connectionRoutingStatus. Either BLE or USB or
 * both can be enabled.
 * @see DATATO_USB
 * @see DATATO_BLE
 * @param key Pointer to command which is sent to these queues
 * */
void sendKbd(hid_command_t *key)
{
  if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_USB)
  {
    xQueueSend(hid_usb,key,TIMEOUT);
  }
  if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_BLE) 
  {
    xQueueSend(hid_ble,key,TIMEOUT);
  }
}

/** @brief FUNCTIONAL TASK - trigger keyboard actions
 * 
 * This task is used to trigger keyboard actions, as it is defined in
 * taskKeyboardConfig_t. Can be used as singleshot method by using 
 * VB_SINGLESHOT as virtual button configuration.
 * @see VB_SINGLESHOT
 * @see taskKeyboardConfig_t
 * @param param Task configuration **/
void task_keyboard(taskKeyboardConfig_t *param)
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
  //command to send to HID
  hid_command_t cmd;
  //store for later use
  uint8_t keyboardType = param->type;
  //calculate array index of EventGroup array (each 4 VB have an own EventGroup)
  uint8_t evGroupIndex = param->virtualButton / 4;
  //calculate bitmask offset within the EventGroup
  uint8_t evGroupShift = param->virtualButton % 4;
  //final pointer to the EventGroup used by this task
  EventGroupHandle_t *evGroup = NULL;
  //ticks between task timeouts
  const TickType_t xTicksToWait = 2000 / portTICK_PERIOD_MS;
  //local virtual button
  uint8_t vb = param->virtualButton;
  //local key array offset for each trigger
  uint8_t keycodeoffset = 0;
  
  //if we are in singleshot mode, we do not need to test
  //the virtual button groups (unused)
  if(vb != VB_SINGLESHOT)
  {
    //check for correct offset
    if(evGroupIndex >= NUMBER_VIRTUALBUTTONS)
    {
      ESP_LOGE(LOG_TAG,"virtual button group unsupported: %d ",evGroupIndex);
      if(vb == VB_SINGLESHOT) return; else vTaskDelete(NULL);
    }
    
    //test if event groups are already initialized, otherwise exit immediately
    while(virtualButtonsOut[evGroupIndex] == 0)
    {
      ESP_LOGE(LOG_TAG,"uninitialized event group for virtual buttons, retry in 1s");
      vTaskDelay(1000/portTICK_PERIOD_MS);
    } 
    //event group initialized, store for later usage
    evGroup = virtualButtonsOut[evGroupIndex];
  }
  
  //test if parameter is correct
  if(keyboardType != PRESS && keyboardType != RELEASE && \
    keyboardType != PRESS_RELEASE_BUTTON && keyboardType != WRITE)
  {
    ESP_LOGE(LOG_TAG,"unknown keyboard action type,cannot continue...");
    if(vb == VB_SINGLESHOT) return; else { vTaskDelay(2); vTaskDelete(NULL); }
  }
  
  //local keystring (size is determined by input keystring)
  uint8_t keylength = 0;
  while(param->keycodes_text[keylength] != 0 && keylength < TASK_KEYBOARD_PARAMETERLENGTH) keylength++;
  
  //if count of keycode is 0, nothing to do here...
  if(keylength == 0)
  {
    ESP_LOGI(LOG_TAG,"Empty kbd instance, quit");
    if(vb == VB_SINGLESHOT) return; else { vTaskDelay(2); vTaskDelete(NULL); }
  }
  
  //copy keys to local buffer
  //after this copying, task parameters could be freed (not recommended although)
  uint16_t *keys = malloc(sizeof(uint16_t)*keylength);
  memcpy(keys,param->keycodes_text,sizeof(uint16_t)*keylength);
  if(keys != NULL)
  {
    ESP_LOGI(LOG_TAG,"allocated %d keycodes",keylength);
  } else {
    ESP_LOGE(LOG_TAG,"cannot allocate %d bytes for keyarray",keylength);
    if(vb == VB_SINGLESHOT) return; else { vTaskDelay(2); vTaskDelete(NULL); }
  }
  
  while(1)
  {
      switch(keyboardType)
      {
        //these action are only triggered by a button press
        case PRESS:
        case RELEASE:
        case WRITE:
          //wait for the flag (only if not in singleshot mode)
          if(vb != VB_SINGLESHOT)
          {
            uxBits = xEventGroupWaitBits(evGroup,(1<<evGroupShift),pdTRUE,pdFALSE,xTicksToWait);
          }
          //reset offset
          keycodeoffset = 0;
          
          while(keycodeoffset < keylength)
          {
            //test for a valid set flag (else branch would be timeout)
            if((uxBits & (1<<evGroupShift)) || vb == VB_SINGLESHOT)
            {
              //add keyocde to array if press or write action is used
              if(keyboardType == PRESS || keyboardType == WRITE)
              {
                //send press action to USB/BLE
                ESP_LOGD(LOG_TAG,"press: 0x%x",keys[keycodeoffset]);

                //add modifier to global mask
                keycode_arr[1] |= (keys[keycodeoffset] & 0xFF00)>>8;
                
                //keycodes, add directly to the keycode array
                switch(add_keycode(keys[keycodeoffset] & 0x00FF,&(keycode_arr[2])))
                {
                  case 0:
                    //keycode is added, now send
                    memcpy(cmd.data,keycode_arr,8);
                    cmd.len = 8;
                    sendKbd(&cmd);
                    break;
                  case 1: ESP_LOGW(LOG_TAG,"keycode already in queue"); break;
                  case 2: ESP_LOGE(LOG_TAG,"no space in keycode arr!"); break;
                  default: ESP_LOGE(LOG_TAG,"add_keycode return unknown code..."); break;
                }
              }
              //remove keycode from array, if release or write is used
              if(keyboardType == RELEASE || keyboardType == WRITE)
              {
                //send release action to USB/BLE
                ESP_LOGD(LOG_TAG,"release: 0x%x",keys[keycodeoffset]);
                
                //remove modifier from global mask
                keycode_arr[1] &= ~((keys[keycodeoffset] & 0xFF00)>>8);
                
                //keycodes, add directly to the keycode array
                switch(remove_keycode(keys[keycodeoffset] & 0x00FF,&(keycode_arr[2])))
                {
                  case 0:
                    //keycode is removed, now send
                    memcpy(cmd.data,keycode_arr,8);
                    cmd.len = 8;
                    sendKbd(&cmd);
                    break;
                  case 1: ESP_LOGW(LOG_TAG,"keycode already in queue"); break;
                  case 2: ESP_LOGE(LOG_TAG,"no space in keycode arr!"); break;
                  default: ESP_LOGE(LOG_TAG,"add_keycode return unknown code..."); break;
                }
              }
            }
            keycodeoffset++;
          }
        break;
        
        //this action is triggered on a button press AND a release
        case PRESS_RELEASE_BUTTON:
          //wait for the flags (press & release)
          if(vb != VB_SINGLESHOT)
          {
            uxBits = xEventGroupWaitBits(evGroup,(1<<evGroupShift)| \
              (1<<(evGroupShift+4)),pdTRUE,pdFALSE,xTicksToWait);
          }
          //reset key array offset
          keycodeoffset = 0;
          while(keycodeoffset < keylength)
          {
            //test for a valid set flag
            if((uxBits & (1<<evGroupShift)) || vb == VB_SINGLESHOT)
            {
                //add modifier to global mask
                keycode_arr[1] |= (keys[keycodeoffset] & 0xFF00)>>8;
                
                //keycodes, add directly to the keycode array
                switch(add_keycode(keys[keycodeoffset] & 0x00FF,&(keycode_arr[2])))
                {
                  case 0:
                    //keycode is added, now send
                    memcpy(cmd.data,keycode_arr,8);
                    cmd.len = 8;
                    sendKbd(&cmd);
                    break;
                  case 1: ESP_LOGW(LOG_TAG,"keycode already in queue"); break;
                  case 2: ESP_LOGE(LOG_TAG,"no space in keycode arr!"); break;
                  default: ESP_LOGE(LOG_TAG,"add_keycode return unknown code..."); break;
                }
            }
            if((uxBits & (1<<(evGroupShift+4))) || vb == VB_SINGLESHOT)
            {
              ESP_LOGD(LOG_TAG,"press&release button 2: 0x%x",keys[keycodeoffset]);
              //remove modifier from global mask
              keycode_arr[1] &= ~((keys[keycodeoffset] & 0xFF00)>>8);
              
              //keycodes, add directly to the keycode array
              switch(remove_keycode(keys[keycodeoffset] & 0x00FF,&(keycode_arr[2])))
              {
                case 0:
                  //keycode is removed, now send
                  memcpy(cmd.data,keycode_arr,8);
                  cmd.len = 8;
                  sendKbd(&cmd);
                  break;
                case 1: ESP_LOGW(LOG_TAG,"keycode already in queue"); break;
                case 2: ESP_LOGE(LOG_TAG,"no space in keycode arr!"); break;
                default: ESP_LOGE(LOG_TAG,"add_keycode return unknown code..."); break;
              }
            }
            keycodeoffset++;
          }
        break;

        default:
          ESP_LOGE(LOG_TAG,"unknown keyboard action type,quit...");
          free(keys);
          if(vb == VB_SINGLESHOT) return; { vTaskDelay(2); vTaskDelete(NULL); }
      }
      
      //function tasks in singleshot must return
      if(vb == VB_SINGLESHOT) 
      {
        free(keys);
        return;
      }
  }
}
