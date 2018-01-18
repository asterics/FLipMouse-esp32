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
 * This file contains the task implementation for mouse control via
 * virtual buttons (triggered by flags). The analog mouse control is
 * done via the hal_adc task.
 * 
 */


#include "task_mouse.h"


#define LOG_TAG "task_mouse"

void task_mouse(taskMouseConfig_t *param)
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
  //mouse command which is sent.
  mouse_command_t press;
  mouse_command_t release;
  mouse_command_t empty;

  uint8_t buttonmask = 0;
  uint8_t clickcount = 1;
  uint8_t userelease = 0;
  uint8_t autorelease = 0;
  
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
  
  //init the mouse command structure for this instance
  switch(param->type)
  {
    case RIGHT: buttonmask = MOUSE_BUTTON_RIGHT; break;
    case LEFT: buttonmask = MOUSE_BUTTON_LEFT; break;
    case MIDDLE: buttonmask = MOUSE_BUTTON_MIDDLE; break;
    case WHEEL: 
    {
      buttonmask = 0;
      press.wheel = (int8_t) param->actionvalue;
      param->actionparam = M_UNUSED;
      break;
    }
    case X:
    {
      buttonmask = 0;
      press.x = (int8_t) param->actionvalue;
      param->actionparam = M_UNUSED;
      break;
    }
    case Y:
    {
      buttonmask = 0;
      press.y = (int8_t) param->actionvalue;
      param->actionparam = M_UNUSED;
      break;
    }
    default:
      ESP_LOGE(LOG_TAG,"unkown mouse type %d, exiting",param->type);
      vTaskDelete(NULL);
      break;
  }
  
  switch(param->actionparam)
  {
    case M_CLICK:
      press.buttons = buttonmask;
      autorelease = 1;
    case M_HOLD:
      press.buttons = buttonmask;
      release.buttons = 0;
      userelease = 1;
    case M_DOUBLE:
      press.buttons = buttonmask;
      clickcount = 2;
      autorelease = 1;
    case M_UNUSED: break;
    default:
      ESP_LOGE(LOG_TAG,"unkown mouse action param %d, exiting",param->actionparam);
      vTaskDelete(NULL);
      break;
  }
  
  while(1)
  {
      //wait for the flag
      uxBits = xEventGroupWaitBits(evGroup,(1<<evGroupShift)|(1<<(evGroupShift+4)),pdTRUE,pdFALSE,xTicksToWait);
      //test for a valid set flag
      if(uxBits & (1<<evGroupShift))
      {
        for(uint8_t i = 0; i<clickcount; i++)
        {
          //if press is set
          if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_USB) 
            xQueueSend(mouse_movement_usb,&press,TIMEOUT);
          if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_BLE) 
            xQueueSend(mouse_movement_ble,&press,TIMEOUT);
          
          //send second command if set
          if(autorelease)
          {
            if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_USB) 
              xQueueSend(mouse_movement_usb,&empty,TIMEOUT);
            if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_BLE) 
              xQueueSend(mouse_movement_ble,&empty,TIMEOUT);
          }
        }
      }
      if(uxBits & (1<<(evGroupShift+4)))
      {
        //if release flag is used
        if(userelease)
        {
          if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_USB) 
            xQueueSend(mouse_movement_usb,&release,TIMEOUT);
          if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_BLE) 
            xQueueSend(mouse_movement_ble,&release,TIMEOUT);
        }
      }
  }
}

void mouse_direct(taskMouseConfig_t *param)
{
  if(param == NULL) return;
  //mouse command which is sent.
  mouse_command_t press;
  mouse_command_t empty;

  uint8_t buttonmask = 0;
  uint8_t clickcount = 1;
  uint8_t autorelease = 0;
 
  //init the mouse command structure for this instance
  switch(param->type)
  {
    case RIGHT: buttonmask = MOUSE_BUTTON_RIGHT; break;
    case LEFT: buttonmask = MOUSE_BUTTON_LEFT; break;
    case MIDDLE: buttonmask = MOUSE_BUTTON_MIDDLE; break;
    case WHEEL: 
    {
      buttonmask = 0;
      press.wheel = (int8_t) param->actionvalue;
      param->actionparam = M_UNUSED;
      break;
    }
    case X:
    {
      buttonmask = 0;
      press.x = (int8_t) param->actionvalue;
      param->actionparam = M_UNUSED;
      break;
    }
    case Y:
    {
      buttonmask = 0;
      press.y = (int8_t) param->actionvalue;
      param->actionparam = M_UNUSED;
      break;
    }
    default:
      ESP_LOGE(LOG_TAG,"unkown mouse type %d, exiting",param->type);
      return;
  }
  
  switch(param->actionparam)
  {
    case M_CLICK:
      press.buttons = buttonmask;
      autorelease = 1;
    case M_HOLD:
      press.buttons = buttonmask;
      //release.buttons = 0;
      //userelease = 1;
    case M_DOUBLE:
      press.buttons = buttonmask;
      clickcount = 2;
      autorelease = 1;
    case M_UNUSED: break;
    default:
      ESP_LOGE(LOG_TAG,"unkown mouse action param %d, exiting",param->actionparam);
      vTaskDelete(NULL);
      break;
  }
  
  
  for(uint8_t i = 0; i<clickcount; i++)
  {
    //if press is set
    if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_USB) 
      xQueueSend(mouse_movement_usb,&press,TIMEOUT);
    if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_BLE) 
      xQueueSend(mouse_movement_ble,&press,TIMEOUT);
    
    //send second command if set
    if(autorelease)
    {
      if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_USB) 
        xQueueSend(mouse_movement_usb,&empty,TIMEOUT);
      if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_BLE) 
        xQueueSend(mouse_movement_ble,&empty,TIMEOUT);
    }
  }
}
