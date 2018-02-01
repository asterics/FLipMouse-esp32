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
 * 
 * This file contains the task implementation for keyboard key triggering.
 * This task is started by the configuration task and it is assigned
 * to one virtual button. As soon as this button is triggered, the configured
 * keys are sent to either USB, BLE or both.
 * This task can be deleted all the time to change the configuration of
 * one button.
 */


#include "task_kbd.h"


#define LOG_TAG "task_kbd"

void sendToQueue(QueueHandle_t queue, uint16_t *data)
{
  //loop until either \0 termination or length of parameter is reached
  xQueueSend(queue,data,TIMEOUT);
}

void task_keyboard(taskKeyboardConfig_t *param)
{
  //check for param struct
  if(param == NULL)
  {
    ESP_LOGE(LOG_TAG,"param is NULL ");
    vTaskDelete(NULL);
    return;
  }
  //event bits used for pending on debounced buttons
  EventBits_t uxBits = 0;
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
  
  if(vb != VB_SINGLESHOT)
  {
    //check for correct offset
    if(evGroupIndex >= NUMBER_VIRTUALBUTTONS)
    {
      ESP_LOGE(LOG_TAG,"virtual button group unsupported: %d ",evGroupIndex);
      vTaskDelete(NULL);
      return;
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
  
  if(keyboardType != PRESS && keyboardType != RELEASE && keyboardType != PRESS_RELEASE && 
    keyboardType != PRESS_RELEASE_BUTTON && keyboardType != WRITE)
  {
    ESP_LOGE(LOG_TAG,"unknown keyboard action type,quit...");
    vTaskDelete(NULL);
    return;
  }
  
  //local keystring (size is determined by input keystring)
  uint8_t keylength = 0;
  while(param->keycodes_text[keylength] != 0 && keylength < TASK_KEYBOARD_PARAMETERLENGTH) keylength++;
  
  //if count of keycode is 0, nothing to do here...
  if(keylength == 0)
  {
    ESP_LOGI(LOG_TAG,"Empty kbd instance, quit");
    if(vb == VB_SINGLESHOT) return; else vTaskDelete(NULL);
  }

  uint16_t *keys = malloc(sizeof(uint16_t)*keylength);
  memcpy(keys,param->keycodes_text,sizeof(uint16_t)*keylength);
  if(keys != NULL)
  {
    ESP_LOGI(LOG_TAG,"allocated %d keycodes",keylength);
  } else {
    ESP_LOGE(LOG_TAG,"cannot allocate %d bytes for keyarray",keylength);
    if(vb == VB_SINGLESHOT) return; else vTaskDelete(NULL);
  }
  
  while(1)
  {
      switch(keyboardType)
      {
        //these action are only triggered by a button press
        case PRESS:
        case RELEASE:
        case PRESS_RELEASE:
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
              if(keyboardType == PRESS)
              {
                ESP_LOGD(LOG_TAG,"press: 0x%x",keys[keycodeoffset]);
                if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_USB)
                {
                  sendToQueue(keyboard_usb_press,&keys[keycodeoffset]);
                }
                if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_BLE) 
                {
                  sendToQueue(keyboard_ble_press,&keys[keycodeoffset]);
                }
              }
              if(keyboardType == RELEASE)
              {
                ESP_LOGD(LOG_TAG,"release: 0x%x",keys[keycodeoffset]);
                if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_USB) 
                {
                  sendToQueue(keyboard_usb_release,&keys[keycodeoffset]);
                }
                if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_BLE) 
                {
                  sendToQueue(keyboard_ble_release,&keys[keycodeoffset]);
                }
              }
              if(keyboardType == PRESS_RELEASE || keyboardType == WRITE)
              {
                ESP_LOGD(LOG_TAG,"press&release 1: 0x%x",keys[keycodeoffset]);
                if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_USB)
                {
                  sendToQueue(keyboard_usb_press,&keys[keycodeoffset]);
                  vTaskDelay(2);
                  sendToQueue(keyboard_usb_release,&keys[keycodeoffset]);
                }
                ESP_LOGD(LOG_TAG,"press&release 2: 0x%x",keys[keycodeoffset]);
                if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_BLE) 
                {
                  sendToQueue(keyboard_ble_press,&keys[keycodeoffset]);
                  vTaskDelay(2);
                  sendToQueue(keyboard_ble_release,&keys[keycodeoffset]);
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
              ESP_LOGD(LOG_TAG,"press&release button 1: 0x%x",keys[keycodeoffset]);
              if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_USB) 
                  sendToQueue(keyboard_usb_press,&keys[keycodeoffset]);
              if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_BLE) 
                  sendToQueue(keyboard_ble_press,&keys[keycodeoffset]);
            }
            if((uxBits & (1<<(evGroupShift+4))) || vb == VB_SINGLESHOT)
            {
              ESP_LOGD(LOG_TAG,"press&release button 2: 0x%x",keys[keycodeoffset]);
              if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_USB) 
                  sendToQueue(keyboard_usb_release,&keys[keycodeoffset]);
              if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_BLE) 
                  sendToQueue(keyboard_ble_release,&keys[keycodeoffset]);
            }
            keycodeoffset++;
          }
        break;
        
        
        break;
        
        default:
          ESP_LOGE(LOG_TAG,"unknown keyboard action type,quit...");
          vTaskDelete(NULL);
          free(keys);
          return;
      }
      
      //function tasks in singleshot must return
      if(vb == VB_SINGLESHOT) 
      {
        free(keys);
        return;
      }
  }
}
