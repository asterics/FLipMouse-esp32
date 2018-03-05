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
 * @brief FUNCTIONAL TASK - joystick handling
 * 
 * This module is used as functional task for joystick control.
 * 
 * It supports buttons (clicking, press&hold, release),
 * axis (X/Y/Z), Z rotate and two sliders (left & right).
 * 
 * In addition, single shot is possible.
 * @note Joystick control by mouthpiece is done in hal_adc!
 * @see hal_adc
 * @see task_joystick
 * @see VB_SINGLESHOT
 */
#include "task_joystick.h"

/** @brief Logging tag for this module */
#define LOG_TAG "task_joystick"

/** @brief Global joystick state */
joystick_command_t globalJoystickState;

/** @brief FUNCTIONAL TASK - Trigger joystick action
 * 
 * This task is used to trigger a joystick action, possible actions
 * are defined in taskJoystickConfig_t.
 * Can be used as singleshot method by using VB_SINGLESHOT as virtual
 * button configuration.
 * 
 * @param param Task configuration
 * @see VB_SINGLESHOT
 * @see taskJoystickConfig_t*/
void task_joystick(taskJoystickConfig_t *param)
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

  //local button mask
  uint32_t value = param->value;
  int16_t hat = param->valueS;
  //action type
  joystick_action action = param->type;
  //release mode
  uint8_t releasemode = param->mode;
  
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

  if(!((action == BUTTON_PRESS) || (action == BUTTON_RELEASE) || \
    (action == XAXIS) || (action == YAXIS) || (action == ZAXIS) || \
    (action == ZROTATE) || (action == SLIDER_LEFT) || \
    (action == SLIDER_RIGHT) || (action == HAT)))
  {
    ESP_LOGE(LOG_TAG,"joystick action unknown: %d ",action);
    if(vb == VB_SINGLESHOT) return;
    while(1) vTaskDelay(100000/portTICK_PERIOD_MS);
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
        //press action
        //set different values of global joystick state accordingly.
        switch(action)
        {
          case BUTTON_PRESS: globalJoystickState.buttonmask |= value; break;
          case BUTTON_RELEASE: globalJoystickState.buttonmask &= ~value; break;
          case XAXIS: globalJoystickState.Xaxis = value; break;
          case YAXIS: globalJoystickState.Yaxis = value; break;
          case ZAXIS: globalJoystickState.Zaxis = value; break;
          case ZROTATE: globalJoystickState.Zrotate = value; break;
          case SLIDER_LEFT: globalJoystickState.sliderLeft = value; break;
          case SLIDER_RIGHT: globalJoystickState.sliderRight = value; break;
          case HAT: globalJoystickState.hat = hat; break;
        }
      
        //send either to USB, BLE, both or none
        if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_USB)
        { xQueueSend(joystick_movement_usb, &globalJoystickState,10); }
        if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_BLE) 
        { xQueueSend(joystick_movement_ble, &globalJoystickState,10); }
      }
      //test for vb's release flag or trigger if vb is singleshot
      if((uxBits & (1<<(evGroupShift+4))) || vb == VB_SINGLESHOT)
      {
        //if release flag is used (cannot do for BUTTON_RELEASE)
        if(releasemode != 0 && action != BUTTON_RELEASE)
        {
          //release action
          //set different values of global joystick state accordingly.
          switch(action)
          {
            case BUTTON_PRESS: globalJoystickState.buttonmask &= ~value; break;
            case BUTTON_RELEASE: break; //this line is needed, otherwise compile error.
            case XAXIS: globalJoystickState.Xaxis = 512; break;
            case YAXIS: globalJoystickState.Yaxis = 512; break;
            case ZAXIS: globalJoystickState.Zaxis = 512; break;
            case ZROTATE: globalJoystickState.Zrotate = 512; break;
            case SLIDER_LEFT: globalJoystickState.sliderLeft = 0; break;
            case SLIDER_RIGHT: globalJoystickState.sliderRight = 0; break;
            case HAT: globalJoystickState.hat = -1; break;
          }
          
          if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_USB)
          { xQueueSend(joystick_movement_usb, &globalJoystickState,10); }
          if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_BLE) 
          { xQueueSend(joystick_movement_ble, &globalJoystickState,10); }
        }
      }
      
      //function tasks in single shot mode MUST return to its caller
      if(vb == VB_SINGLESHOT) return;
  }
}


/** @brief Update and send the joystick values.
 * 
 * This method is used to update the globale joystick state and
 * send the updated values to USB/BLE (as configured).
 * Usually this method is used for halAdc to send updates from a 
 * mouthpiece in joystick mode to the axis of the joystick.
 * 
 * @param value1 New value for the axis1 parameter
 * @param axis1 Axis to map the parameter value1 to.
 * @param value2 New value for the axis2 parameter
 * @param axis2 Axis to map the parameter value2 to.
 * @return ESP_OK if updating & sending was fine. ESP_FAIL otherwise (params
 * not valid, e.g. buttons are used, or sending failed)
 * */
esp_err_t joystick_update(int32_t value1, joystick_action axis1, int32_t value2, joystick_action axis2)
{
  /** @todo Update the global values here and send the joystick state */
  return ESP_OK;
}


/** @brief Reverse Parsing - get AT command for joystick VB
 * 
 * This function parses the current configuration of a virtual button
 * to an AT command used to print the configuration.
 * @param output Output string, where the full AT command will be stored
 * @param cfg Pointer to current joystick configuration, used to parse.
 * @return ESP_OK if everything went fine, ESP_FAIL otherwise
 * */
esp_err_t task_joystick_getAT(char* output, void* cfg)
{
  taskJoystickConfig_t *conf = (taskJoystickConfig_t *)cfg;
  if(conf == NULL) return ESP_FAIL;
  
  //set mode to defined value (if it is messed up)
  uint8_t releasemode = 0;
  if(conf->mode != 0) releasemode = 1;
  
  //determine type of action and set output buffer accordingly
  switch(conf->type)
  {
    case BUTTON_PRESS:
      //special for buttons: to be compatible, it is a new command
      //if button press is set together with releasemode (button click)
      if(releasemode == 0)
      {
        sprintf(output,"AT JP %d",conf->value);
      } else {
        sprintf(output,"AT JC %d",conf->value);
      }
      break;
    case BUTTON_RELEASE: sprintf(output,"AT JR %d",conf->value); break;
    
    //all following types have the releasmode in the AT command
    case XAXIS: sprintf(output,"AT JX %d %d",conf->value,releasemode); break;
    case YAXIS: sprintf(output,"AT JY %d %d",conf->value,releasemode); break;
    case ZAXIS: sprintf(output,"AT JZ %d %d",conf->value,releasemode); break;
    case ZROTATE: sprintf(output,"AT JT %d %d",conf->value,releasemode); break;
    case SLIDER_LEFT: sprintf(output,"AT JS %d %d",conf->value,releasemode); break;
    case SLIDER_RIGHT: sprintf(output,"AT JU %d %d",conf->value,releasemode); break;
    case HAT: sprintf(output,"AT JH %d %d",conf->valueS,releasemode); break;
    
    default: return ESP_FAIL; //unknown mode, return fail
  }
  //fine fine fine, return ESP_OK (AT command found and buffer filled).
  return ESP_OK;
}
