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
 * @brief FUNCTIONAL TASK - mouse handling
 * 
 * This module is used as functional task for mouse control.
 * It supports right/left/middle clicks (clicking, press&hold, release),
 * left double clicks, mouse wheel and mouse X/Y actions.
 * In addition, single shot is possible.
 * @note Mouse control by mouthpiece is done in hal_adc!
 * @see hal_adc
 * @see task_mouse
 * @see VB_SINGLESHOT
 */

#include "task_mouse.h"

/** @brief Logging tag for this module */
#define LOG_TAG "task_mouse"

/** @brief Set current mouse wheel step size
 * 
 * This method is used to set the current step size for the mouse wheel
 * @param steps Amount of wheel steps, range: 0-126
 * @return 0 if everything was fine, 1 if stepsize is out of range */
uint8_t mouse_set_wheel(uint8_t steps)
{
  generalConfig_t *cfg = configGetCurrent();
  if(cfg == NULL) return 1;
  
  if(steps < 127) 
  {
    cfg->wheel_stepsize = steps;
    return 0;
  } else {
    ESP_LOGE(LOG_TAG,"Cannot set wheel steps, too high: %u",steps);
    return 1;
  }
}

/** @brief Get the current mouse wheel step size
 * 
 * @return Current step size */
uint8_t mouse_get_wheel(void) 
{ 
  generalConfig_t *cfg = configGetCurrent();
  if(cfg != NULL) return cfg->wheel_stepsize;
  else return 0;
}

/** @brief FUNCTIONAL TASK - Trigger mouse action
 * 
 * This task is used to trigger a mouse action, possible actions
 * are defined in taskMouseConfig_t.
 * Can be used as singleshot method by using VB_SINGLESHOT as virtual
 * button configuration.
 * 
 * @param param Task configuration
 * @see VB_SINGLESHOT
 * @see taskMouseConfig_t*/
void task_mouse(taskMouseConfig_t *param)
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
  //ticks between task timeouts
  const TickType_t xTicksToWait = 2000 / portTICK_PERIOD_MS;
  //mouse command which is sent.
  usb_command_t press;
  usb_command_t release;
  usb_command_t empty;
  //empty them
  memset(&press,0,sizeof(usb_command_t));
  memset(&release,0,sizeof(usb_command_t));
  memset(&empty,0,sizeof(usb_command_t));
  empty.len = 5;
  empty.data[0] = 'M';
  press.len = 5;
  press.data[0] = 'M';
  release.len = 5;
  release.data[0] = 'M';

  //button mask to be used by this task
  uint8_t buttonmask = 0;
  //how much clicks are triggered by this task on an action
  uint8_t clickcount = 1;
  //should we use the release flag as well (drag action)?
  uint8_t userelease = 0;
  //do we need to send the release action (empty report) as well?
  uint8_t autorelease = 0;
  
  //do all the eventgroup checking only if this is a persistent task
  //-> virtualButton is NOT set to VB_SINGLESHOT
  if(vb != VB_SINGLESHOT)
  {
    //check for correct offset
    if(evGroupIndex >= NUMBER_VIRTUALBUTTONS)
    {
      ESP_LOGE(LOG_TAG,"virtual button group unsupported: %d, exiting",evGroupIndex);
      vTaskDelay(2); 
      vTaskDelete(NULL);
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
  
  //init the mouse command structure for this instance
  switch(param->type)
  {
    //right/left/middle are using the button mask
    case RIGHT: buttonmask = MOUSE_BUTTON_RIGHT; break;
    case LEFT: buttonmask = MOUSE_BUTTON_LEFT; break;
    case MIDDLE: buttonmask = MOUSE_BUTTON_MIDDLE; break;
    //wheel action
    case WHEEL: 
    {
      buttonmask = 0;
      press.data[4] = (int8_t) param->actionvalue;
      param->actionparam = M_UNUSED;
      break;
    }
    //move mouse in X direction
    case X:
    {
      buttonmask = 0;
      press.data[3] = (int8_t) param->actionvalue;
      param->actionparam = M_UNUSED;
      break;
    }
    //move mouse in Y direction
    case Y:
    {
      buttonmask = 0;
      press.data[2] = (int8_t) param->actionvalue;
      param->actionparam = M_UNUSED;
      break;
    }
    //unknown/unsupported action...
    default:
      ESP_LOGE(LOG_TAG,"unkown mouse type %d, exiting",param->type);
      if(vb == VB_SINGLESHOT) return; else  { vTaskDelay(2); vTaskDelete(NULL); }
      break;
  }
  
  //depending on parameter of click, set buttonmask accordingly
  //in addition set the amount of mouse clicks
  //as well as release&autorelease flags
  switch(param->actionparam)
  {
    case M_CLICK:
      press.data[1] = buttonmask;
      autorelease = 1;
      break;
    case M_HOLD:
      press.data[1] = buttonmask;
      release.data[1] = 0;
      if(param->virtualButton == VB_SINGLESHOT)
      {
        userelease = 0;
      } else {
        userelease = 1;
      }
      break;
    case M_DOUBLE:
      press.data[1] = buttonmask;
      clickcount = 2;
      autorelease = 1;
      break;
    case M_RELEASE:
      press.data[1] = 0;
    case M_UNUSED: break;
    default:
      ESP_LOGE(LOG_TAG,"unknown mouse action param %d, exiting",param->actionparam);
      if(vb == VB_SINGLESHOT) return; else  { vTaskDelay(2); vTaskDelete(NULL); }
      break;
  }
  
  while(1)
  {
      //wait for the flag
      if(vb != VB_SINGLESHOT)
      {
        uxBits = xEventGroupWaitBits(evGroup,(1<<evGroupShift)|(1<<(evGroupShift+4)),pdTRUE,pdFALSE,xTicksToWait);
      }
      //test for a valid set flag or trigger if we are using singleshot
      if((uxBits & (1<<evGroupShift)) || vb == VB_SINGLESHOT)
      {
        for(uint8_t i = 0; i<clickcount; i++)
        {
          //if press is set
          if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_USB) 
          { xQueueSend(hid_usb,&press,TIMEOUT); }
          //TODO: activate if BLE uses same structure
          /*if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_BLE) 
          { xQueueSend(mouse_movement_ble,&press,TIMEOUT); }*/
          
          //send second command if set
          if(autorelease)
          {
            if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_USB) 
            { xQueueSend(hid_usb,&empty,TIMEOUT); }
            //TODO: activate if BLE uses same structure
            /*if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_BLE) 
            { xQueueSend(mouse_movement_ble,&empty,TIMEOUT); }*/
          }
        }
      }
      //test for vb's release flag or trigger if vb is singleshot
      if((uxBits & (1<<(evGroupShift+4))) || vb == VB_SINGLESHOT)
      {
        //if release flag is used
        if(userelease)
        {
          if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_USB)
          { xQueueSend(hid_usb,&release,TIMEOUT); }
          //TODO: activate if BLE uses same structure
          //if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_BLE) 
          //{ xQueueSend(mouse_movement_ble,&release,TIMEOUT); }
        }
      }
      
      //function tasks in single shot mode MUST return to its caller
      if(vb == VB_SINGLESHOT) return;
  }
}

/** @brief Reverse Parsing - get AT command for mouse VB
 * 
 * This function parses the current configuration of a virtual button
 * to an AT command used to print the configuration.
 * @param output Output string, where the full AT command will be stored
 * @param cfg Pointer to current mouse configuration, used to parse.
 * @return ESP_OK if everything went fine, ESP_FAIL otherwise
 * */
esp_err_t task_mouse_getAT(char* output, void* cfg)
{
  taskMouseConfig_t *conf = (taskMouseConfig_t *)cfg;
  if(conf == NULL) return ESP_FAIL;
  char button = 0;
  
  switch(conf->type)
  {
    case RIGHT: button = 'R'; break;
    case LEFT: button = 'L'; break;
    case MIDDLE: button = 'M'; break;
    case WHEEL:
      if(conf->actionvalue > 0)
      {
        sprintf(output,"AT WU"); break;
      } else {
        sprintf(output,"AT WD"); break;
      }
    case X: sprintf(output,"AT MX %d",conf->actionvalue); break;
    case Y: sprintf(output,"AT MY %d",conf->actionvalue); break;
    default: return ESP_FAIL;
  }
  
  if(button != 0)
  {
    switch(conf->actionparam)
    {
      case M_CLICK:
        sprintf(output,"AT C%c",button);
        break;
      case M_HOLD:
        sprintf(output,"AT P%c",button);
        break;
      case M_RELEASE:
        sprintf(output,"AT R%c",button);
        break;
      case M_DOUBLE:
        sprintf(output,"AT CD");
        break;
      case M_UNUSED:
        ESP_LOGW(LOG_TAG,"Unused actionparam, but type requires...");
        return ESP_FAIL;
    }
  }
  return ESP_OK;
}
