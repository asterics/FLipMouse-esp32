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
 * Copyright 2017 Benjamin Aigner <aignerb@technikum-wien.at,
 * beni@asterics-foundation.org>
*/
 /** @brief CONTINOUS TASK - Main command parser for serial AT commands.
 * 
 * This module is used to parse any incoming serial data from hal_serial
 * for valid AT commands.
 * If a valid command is detected, the corresponding action is triggered
 * (mostly by invoking a FUNCTIONAL task in singleshot mode).
 * 
 * In addition, this parser also takes care of switching to CIM mode if
 * requested.
 * The other way from CIM to AT mode is done by the task_cim module by
 * triggering taskCommandsRestart.
 * 
 * @see VB_SINGLESHOT
 * @see task_cim
 * @see hal_serial
 * @see atcmd_api
 * */
#include "task_commands.h"

/** Tag for ESP_LOG logging */
#define LOG_TAG "cmdparser"
 
static TaskHandle_t currentCommandTask = NULL;
uint8_t doMouseParsing(uint8_t *cmdBuffer, taskMouseConfig_t *mouseinstance);
uint8_t doKeyboardParsing(uint8_t *cmdBuffer, taskKeyboardConfig_t *kbdinstance);
//uint8_t doJoystickParsing(uint8_t *cmdBuffer, taskJoystickConfig_t *instance);
uint8_t doMouthpieceSettingsParsing(uint8_t *cmdBuffer);
uint8_t doStorageParsing(uint8_t *cmdBuffer);
uint8_t doInfraredParsing(uint8_t *cmdBuffer);
uint8_t doGeneralCmdParsing(uint8_t *cmdBuffer);

/** simple helper function which sends back to the USB host "?"
 * and prints an error on the console with the given extra infos. */
void sendErrorBack(const char* extrainfo)
{
  ESP_LOGE(LOG_TAG,"Error parsing cmd: %s",extrainfo);
  halSerialSendUSBSerial(HAL_SERIAL_TX_TO_CDC,"?",sizeof("?"),20);
}

/** just check all queues if they are initialized
 * @return 0 on uninitialized queues, 1 if all are initialized*/
static int checkqueues(void)
{
  // check keyboard queues
  if(keyboard_usb_press == 0) return 0;
  if(keyboard_ble_press == 0) return 0;
  if(keyboard_usb_release == 0) return 0;
  if(keyboard_ble_release == 0) return 0;
  
  // check mouse queues
  if(mouse_movement_ble == 0) return 0;
  if(mouse_movement_usb == 0) return 0;
  
  //check joystick queues
  if(joystick_movement_ble == 0) return 0;
  if(joystick_movement_usb == 0) return 0;
  
  //house-keeping queues
  if(config_switcher == 0) return 0;
  
  //TODO: add IR queue
  return 1;
}
/*
uint8_t doJoystickParsing(uint8_t *cmdBuffer, taskJoystickConfig_t *instance)
{
  
  //not consumed, no command found for joystick
  return 0;
}*/

uint8_t doMouthpieceSettingsParsing(uint8_t *cmdBuffer)
{
  
  //not consumed, no command found for mouthpiece settings
  return 0;
}

uint8_t doStorageParsing(uint8_t *cmdBuffer)
{
  
  //not consumed, no command found for storage
  return 0;
}

uint8_t doInfraredParsing(uint8_t *cmdBuffer)
{
  
  //not consumed, no command found for infrared
  return 0;
}

uint8_t doGeneralCmdParsing(uint8_t *cmdBuffer)
{
  uint16_t param = 0;
  
  /*++++ AT ID ++++*/
  if(memcmp(cmdBuffer,"AT ID",5) == 0) {
    if(halSerialSendUSBSerial(HAL_SERIAL_TX_TO_CDC,IDSTRING,sizeof(IDSTRING),20) == -1) {
      ESP_LOGE(LOG_TAG,"Error sending response of AT ID"); }}
  /*++++ AT BT ++++*/
  if(memcmp(cmdBuffer,"AT BT",5) == 0) {
    param = strtol((char*)&(cmdBuffer[6]),NULL,10);
    switch(param)
    {
      case 1: 
        xEventGroupClearBits(connectionRoutingStatus,DATATO_BLE); 
        xEventGroupSetBits(connectionRoutingStatus,DATATO_USB); 
        break;
      case 2: 
        xEventGroupClearBits(connectionRoutingStatus,DATATO_USB); 
        xEventGroupSetBits(connectionRoutingStatus,DATATO_BLE); 
        break;
      case 3: 
        xEventGroupSetBits(connectionRoutingStatus,DATATO_USB|DATATO_BLE); 
        break;
      default: sendErrorBack("AT BT param");
    }
  }
      
  //not consumed, no general command found
  return 0;
}

uint8_t doKeyboardParsing(uint8_t *cmdBuffer, taskKeyboardConfig_t *kbdinstance)
{
  
  //not consumed, no command found for keyboard
  return 0;
}

uint8_t doMouseParsing(uint8_t *cmdBuffer, taskMouseConfig_t *mouseinstance)
{
  //we are doing AT command triggered action, so this mouse command
  //is always a singleshot. Simply call the task function.
  mouseinstance->virtualButton = VB_SINGLESHOT;
  
  /*++++ mouse clicks ++++*/
  //AT CL, AT CR, AT CM, AT CD
  if(memcmp(cmdBuffer,"AT C",4) == 0)
  {
    mouseinstance->actionparam = M_CLICK;
    switch(cmdBuffer[4])
    {
      //do single clicks (left,right,middle)
      case 'L': mouseinstance->type = LEFT; task_mouse(mouseinstance); return 1;
      case 'R': mouseinstance->type = RIGHT; task_mouse(mouseinstance); return 1;
      case 'M': mouseinstance->type = MIDDLE; task_mouse(mouseinstance); return 1;
      //do left double click
      case 'D': 
        mouseinstance->type = LEFT;
        mouseinstance->actionparam = M_DOUBLE; 
        task_mouse(mouseinstance); 
        return 1;
      //not an AT C? command for mouse, return 0 (not consumed)
      default: return 0;
    }
  }
  
  /*++++ mouse wheel up/down; set stepsize ++++*/
  if(memcmp(cmdBuffer,"AT W",4) == 0)
  {
    mouseinstance->type = WHEEL;
    mouseinstance->actionparam = M_UNUSED;
    switch(cmdBuffer[4])
    {
      //move mouse wheel up/down
      case 'U': mouseinstance->actionvalue = mouse_get_wheel(); task_mouse(mouseinstance); return 1;
      case 'D': mouseinstance->actionvalue = -mouse_get_wheel(); task_mouse(mouseinstance); return 1;
      //set mouse wheel stepsize. If unsuccessful, default will return 0
      case 'S': 
        if(mouse_set_wheel(strtol((char*)&(cmdBuffer[6]),NULL,10)) == 0) return 1;
      default: return 0;
    }
  }
  
  /*++++ mouse button press ++++*/
  //AT PL, AT PR, AT PM
  if(memcmp(cmdBuffer,"AT P",4) == 0)
  {
    mouseinstance->actionparam = M_HOLD;
    switch(cmdBuffer[4])
    {
      case 'L': mouseinstance->type = LEFT; task_mouse(mouseinstance); return 1;
      case 'R': mouseinstance->type = RIGHT; task_mouse(mouseinstance); return 1;
      case 'M': mouseinstance->type = MIDDLE; task_mouse(mouseinstance); return 1;
      default: return 0;
    }
  }
  
  /*++++ mouse button release ++++*/
  //AT RL, AT RR, AT RM
  if(memcmp(cmdBuffer,"AT R",4) == 0)
  {
    mouseinstance->actionparam = M_RELEASE;
    switch(cmdBuffer[4])
    {
      case 'L': mouseinstance->type = LEFT; task_mouse(mouseinstance); return 1;
      case 'R': mouseinstance->type = RIGHT; task_mouse(mouseinstance); return 1;
      case 'M': mouseinstance->type = MIDDLE; task_mouse(mouseinstance); return 1;
      default: return 0;
    }
  }  
  
  /*++++ mouse move ++++*/
  //AT RL, AT RR, AT RM
  if(memcmp(cmdBuffer,"AT M",4) == 0)
  {
    mouseinstance->actionparam = M_UNUSED;
    //mouseinstance->actionvalue = atoi((char*)&(cmdBuffer[6]));
    //mouseinstance->actionvalue = strtoimax((char*)&(cmdBuffer[6]),&endBuf,10);
    mouseinstance->actionvalue = strtol((char*)&(cmdBuffer[6]),NULL,10);
    if(mouseinstance->actionvalue > 127 || mouseinstance->actionvalue < 217)
    {
      ESP_LOGW(LOG_TAG,"Cannot send mouse command, param unknown");
      return 0;
    }
    switch(cmdBuffer[4])
    {
      case 'X': mouseinstance->type = X; task_mouse(mouseinstance); return 1;
      case 'Y': mouseinstance->type = Y; task_mouse(mouseinstance); return 1;
      default: return 0;
    }
  }
  
  
  /*++++ Click left ++++*/
  if(memcmp(cmdBuffer,"AT CL",5) == 0) {
    mouseinstance->type = LEFT;
    mouseinstance->actionparam = M_CLICK;
    task_mouse(mouseinstance);
    return 1;
  }
  return 0;
}
 
void task_commands(void *params)
{
  uint8_t queuesready = checkqueues();
  int received;
  uint8_t commandBuffer[ATCMD_LENGTH];
  
  taskMouseConfig_t cmdMouse;
  taskKeyboardConfig_t cmdKeyboard;
  //taskJoystickConfig_t cmdJoystick;

  while(1)
  {
    if(queuesready)
    {
      //wait 30ms ticks, in case a long UART frame is transmitted
      //vTaskDelay(30/portTICK_PERIOD_MS);
      
      //wait for incoming data
      received = halSerialReceiveUSBSerial(commandBuffer,ATCMD_LENGTH);
      //check received data at least for length
      if(received < 5)
      {
        //special command "AT" without further command:
        if(received >= 2 && memcmp(commandBuffer,"AT",2) == 0)
        {
          halSerialSendUSBSerial(HAL_SERIAL_TX_TO_CDC,"OK\r\n",3,100);
          continue;
        }
        if(received > 0) 
        {
          ESP_LOGW(LOG_TAG,"Invalid AT commandlength %d",received);
          ESP_LOG_BUFFER_HEXDUMP(LOG_TAG,commandBuffer,received,ESP_LOG_DEBUG);
        }
        continue;
      }
      
      //do parsing :-)
      //simplified into smaller functions, to make reading easy.
      if(doMouseParsing(commandBuffer,&cmdMouse)) continue;
      if(doKeyboardParsing(commandBuffer,&cmdKeyboard)) continue;
      //if(doJoystickParsing(commandBuffer,&cmdJoystick)) continue;
      if(doStorageParsing(commandBuffer)) continue;
      if(doGeneralCmdParsing(commandBuffer)) continue;
      if(doInfraredParsing(commandBuffer)) continue;
      if(doMouthpieceSettingsParsing(commandBuffer)) continue;
      
      //if we are here, no parser was finding commands
      ESP_LOGW(LOG_TAG,"Invalid AT cmd (%d characters), flushing:",received);
      halSerialFlushRX();
      //ESP_LOG_BUFFER_HEXDUMP(LOG_TAG,commandBuffer,received,ESP_LOG_DEBUG);
      halSerialSendUSBSerial(HAL_SERIAL_TX_TO_CDC,"?\r\n",2,100);

    } else {
      //check again for initialized queues
      ESP_LOGE(LOG_TAG,"Queues uninitialized, rechecking in 1s");
      vTaskDelay(1000/portTICK_PERIOD_MS);
      //re-check
      queuesready = checkqueues();
    }
  }
}

/** Init the command parser
 * 
 * This method starts the command parser task,
 * which handles incoming UART bytes & responses if necessary.
 * If necessary, this task deletes itself & starts a CIM task (it MUST
 * be the other way as well to restart the AT command parser)
 * @see taskCommandsRestart
 * @return ESP_OK on success, ESP_FAIL otherwise
 * */
esp_err_t taskCommandsInit(void)
{
  xTaskCreate(task_commands, "uart_rx_task", TASK_COMMANDS_STACKSIZE, NULL, TASK_COMMANDS_PRIORITY, &currentCommandTask);
  if(currentCommandTask == NULL)
  {
    ESP_LOGE(LOG_TAG,"Error initializing command parser task");
    return ESP_FAIL;
  }
  return ESP_OK;
}

/** Restart the command parser task
 * 
 * This method is used, if the CIM parser detects AT commands and stops
 * itself, providing the AT command set again.
 * @return ESP_OK on success, ESP_FAIL otherwise (cannot start task, task already running)
 * */
esp_err_t taskCommandsRestart(void)
{
  if(currentCommandTask != NULL) return ESP_FAIL;
  xTaskCreate(task_commands, "uart_rx_task", TASK_COMMANDS_STACKSIZE, NULL, TASK_COMMANDS_PRIORITY, &currentCommandTask);
  if(currentCommandTask != NULL) return ESP_OK;
  else return ESP_FAIL;
}
