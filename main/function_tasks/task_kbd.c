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
  size_t offset = 0;
  while(data[offset] != 0)
  {
    xQueueSend(queue,&data[offset],TIMEOUT);
    offset++;
  }
}

void keyboard_direct(taskKeyboardConfig_t *param)
{
  //TODO: wie am Besten mit task & command parser kombinieren?
}


void task_keyboard(taskKeyboardConfig_t *param)
{
  EventBits_t uxBits;
  //check for param struct
  if(param == NULL)
  {
    ESP_LOGE(LOG_TAG,"param is NULL ");
    vTaskDelete(NULL);
    return;
  }
  //store for later use
  uint8_t keyboardType = param->type;
  //calculate array index of EventGroup array (each 4 VB have an own EventGroup)
  uint8_t evGroupIndex = param->virtualButton / 4;
  //calculate bitmask offset within the EventGroup
  uint8_t evGroupShift = param->virtualButton % 4;
  //final pointer to the EventGroup used by this task
  EventGroupHandle_t *evGroup;
  //ticks between task timeouts
  const TickType_t xTicksToWait = 2000 / portTICK_PERIOD_MS;
  
  //check for correct offset
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
  
  if(keyboardType != PRESS && keyboardType != RELEASE && keyboardType != PRESS_RELEASE && 
    keyboardType != PRESS_RELEASE_BUTTON && keyboardType != WRITE)
  {
    ESP_LOGE(LOG_TAG,"unknown keyboard action type,quit...");
    vTaskDelete(NULL);
    return;
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
          //wait for the flag
          uxBits = xEventGroupWaitBits(evGroup,(1<<evGroupShift),pdTRUE,pdFALSE,xTicksToWait);
          //test for a valid set flag (else branch would be timeout)
          if(uxBits & (1<<evGroupShift))
          {
            if(keyboardType == PRESS)
            {
              if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_USB) 
                sendToQueue(keyboard_usb_press,param->keycodes_text);
              if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_BLE) 
                sendToQueue(keyboard_ble_press,param->keycodes_text);
            }
            if(keyboardType == RELEASE)
            {
              if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_USB) 
                sendToQueue(keyboard_usb_release,param->keycodes_text);
              if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_BLE) 
                sendToQueue(keyboard_ble_release,param->keycodes_text);
            }
            if(keyboardType == PRESS_RELEASE || keyboardType == WRITE)
            {
              if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_USB)
              {
                sendToQueue(keyboard_usb_press,param->keycodes_text);
                vTaskDelay(2);
                sendToQueue(keyboard_usb_release,param->keycodes_text);
              }
              if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_BLE) 
              {
                sendToQueue(keyboard_ble_press,param->keycodes_text);
                vTaskDelay(2);
                sendToQueue(keyboard_ble_release,param->keycodes_text);
              }
            }
          }
        break;
        
        //this action is triggered on a button press AND a release
        case PRESS_RELEASE_BUTTON:
          //wait for the flags (press & release)
          uxBits = xEventGroupWaitBits(evGroup,(1<<evGroupShift)|(1<<(evGroupShift+4)),pdTRUE,pdFALSE,xTicksToWait);
          //test for a valid set flag
          if(uxBits & (1<<evGroupShift))
          {
            if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_USB) 
                sendToQueue(keyboard_usb_press,param->keycodes_text);
            if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_BLE) 
                sendToQueue(keyboard_ble_press,param->keycodes_text);
          }
          if(uxBits & (1<<(evGroupShift+4)))
          {
            if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_USB) 
                sendToQueue(keyboard_usb_release,param->keycodes_text);
            if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_BLE) 
                sendToQueue(keyboard_ble_release,param->keycodes_text);
          }
          
        break;
        
        
        break;
        
        default:
          ESP_LOGE(LOG_TAG,"unknown keyboard action type,quit...");
          vTaskDelete(NULL);
          return;
      }
  }
}
