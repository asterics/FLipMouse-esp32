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

/** If any parsing part requests a general config update, this variable
 * is set to 1. task_commands will reset it to 0 after the update */
uint8_t requestUpdate = 0;

/** @brief Currently used virtual button number.
 * 
 * If this variable is set to a value != VB_SINGLESHOT any following
 * AT command will be set for this virtual button.
 * task_commands will recognize this and triggers the VB update after
 * command parsing.
 * @see VB_SINGLESHOT
 * @see requestVBParameterSize
 * */
uint8_t requestVBUpdate = VB_SINGLESHOT;

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
  generalConfig_t *currentcfg;
  
  currentcfg = configGetCurrent();
  if(currentcfg == NULL)
  {
    ESP_LOGE(LOG_TAG,"Current config is null, cannot update general cmd");
    return 0;
  }
  
  /*++++ AT ID ++++*/
  if(memcmp(cmdBuffer,"AT ID",5) == 0) {
    uint32_t sent = halSerialSendUSBSerial(HAL_SERIAL_TX_TO_CDC,IDSTRING,sizeof(IDSTRING),20);
    if(sent != sizeof(IDSTRING)) 
    {
      ESP_LOGE(LOG_TAG,"Error sending response of AT ID"); 
    } else return 1;
  }
  /*++++ AT DE ++++*/
  if(memcmp(cmdBuffer,"AT DE",5) == 0) {
    uint32_t tid;
    if(halStorageStartTransaction(&tid,20) != ESP_OK)
    {
      return 0;
    } else {
      if(halStorageDeleteSlot(0,tid) != ESP_OK)
      {
        sendErrorBack("Error deleting all slots");
        return 0;
      } else {
        return 1;
      }
    }
  }
  /*++++ AT LE ++++*/
  if(memcmp(cmdBuffer,"AT DL",5) == 0) {
    uint32_t tid;
    uint8_t slotnumber;
    slotnumber = strtol((char*)&(cmdBuffer[6]),NULL,10);
    if(halStorageStartTransaction(&tid,20) != ESP_OK)
    {
      return 0;
    } else {
      if(halStorageDeleteSlot(slotnumber,tid) != ESP_OK)
      {
        sendErrorBack("Error deleting slot");
        return 0;
      } else {
        return 1;
      }
    }
  }
  /*++++ AT BT ++++*/
  if(memcmp(cmdBuffer,"AT BT",5) == 0) {
    param = strtol((char*)&(cmdBuffer[6]),NULL,10);
    switch(param)
    {
      case 1: 
        currentcfg->ble_active = 0;
        currentcfg->usb_active = 1;
        requestUpdate = 1;
        break;
      case 2: 
        currentcfg->ble_active = 1;
        currentcfg->usb_active = 0;
        requestUpdate = 1;
        break;
      case 3: 
        currentcfg->ble_active = 1;
        currentcfg->usb_active = 1;
        requestUpdate = 1;
        break;
      default: sendErrorBack("AT BT param"); return 0;
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
  //clear any previous data
  memset(mouseinstance,0,sizeof(taskMouseConfig_t));
  
  //use global virtual button (normally VB_SINGLESHOT,
  //can be changed by "AT BM"
  mouseinstance->virtualButton = requestVBUpdate;
  int steps = 3;
  
  /*++++ mouse clicks ++++*/
  //AT CL, AT CR, AT CM, AT CD
  if(memcmp(cmdBuffer,"AT C",4) == 0)
  {
    mouseinstance->actionparam = M_CLICK;
    switch(cmdBuffer[4])
    {
      //do single clicks (left,right,middle)
      case 'L': mouseinstance->type = LEFT; return 1;
      case 'R': mouseinstance->type = RIGHT; return 1;
      case 'M': mouseinstance->type = MIDDLE; return 1;
      //do left double click
      case 'D': 
        mouseinstance->type = LEFT;
        mouseinstance->actionparam = M_DOUBLE; 
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
      case 'U': mouseinstance->actionvalue = mouse_get_wheel(); return 1;
      case 'D': mouseinstance->actionvalue = -mouse_get_wheel(); return 1;
      //set mouse wheel stepsize. If unsuccessful, default will return 0
      case 'S':
        steps = strtol((char*)&(cmdBuffer[6]),NULL,10);
        if(steps > 127 || steps < 127)
        {
          sendErrorBack("Wheel size out of range! (-127 to 127)");
          return 0;
        } else { 
          mouse_set_wheel(steps);
          ESP_LOGI(LOG_TAG,"Setting mouse wheel steps: %d",steps);
          return 2;
        }
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
      case 'L': mouseinstance->type = LEFT; return 1;
      case 'R': mouseinstance->type = RIGHT; return 1;
      case 'M': mouseinstance->type = MIDDLE; return 1;
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
      case 'L': mouseinstance->type = LEFT; return 1;
      case 'R': mouseinstance->type = RIGHT; return 1;
      case 'M': mouseinstance->type = MIDDLE; return 1;
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
    int param = strtol((char*)&(cmdBuffer[5]),NULL,10);
    ESP_LOGI(LOG_TAG,"Mouse move %c, %d",cmdBuffer[4],mouseinstance->actionvalue);
    if(param > 127 && param < -127)
    {
      ESP_LOGW(LOG_TAG,"AT MX parameter limit -127 - 127");
      return 0;
    } else {
      mouseinstance->actionvalue = param;
    }
    switch(cmdBuffer[4])
    {
      case 'X': mouseinstance->type = X; return 1;
      case 'Y': mouseinstance->type = Y; return 1;
      default: return 0;
    }
  }

  return 0;
}
 
void task_commands(void *params)
{
  uint8_t queuesready = checkqueues();
  int received;
  //used for return values of doXXXParsing functions
  uint8_t parserstate;
  uint8_t commandBuffer[ATCMD_LENGTH];
  
  //function pointer to the task function which will eventuall be called
  //in singleshot mode
  void (*requestVBTask)(void *) = NULL;
  
  // parameter size which needs to be allocated for a VB config update
  size_t requestVBParameterSize = 0;
  
  //pointer to VB parameter struct for VB config udpate
  void *requestVBParameter = NULL;
  
  //reload parameter
  command_type_t requestVBType = T_NOFUNCTION;
  
    
  //parameters for different tasks
  taskMouseConfig_t *cmdMouse = malloc(sizeof(taskMouseConfig_t));
  taskKeyboardConfig_t *cmdKeyboard = malloc(sizeof(taskKeyboardConfig_t));
  //taskJoystickConfig_t *cmdJoystick = malloc(sizeof(taskJoystickConfig_t));
  
  //check if we have all our pointers
  if(cmdMouse == NULL || cmdKeyboard == NULL /*|| cmdJoystick == NULL */)
  {
    ESP_LOGE(LOG_TAG,"Cannot malloc memory for command parsing, EXIT!");
    vTaskDelete(NULL);
    return;
  }

  while(1)
  {
    if(queuesready)
    {
      //wait 30ms, to be nice to other tasks
      vTaskDelay(30/portTICK_PERIOD_MS);
      
      //wait for incoming data
      received = halSerialReceiveUSBSerial(commandBuffer,ATCMD_LENGTH);
      //check received data at least for length
      if(received < 5)
      {
        //special command "AT" without further command:
        if(received >= 2 && memcmp(commandBuffer,"AT",2) == 0)
        {
          halSerialSendUSBSerial(HAL_SERIAL_TX_TO_CDC,"OK\r\n",5,100);
          halSerialFlushRX();
          continue;
        }
        if(received > 0) 
        {
          ESP_LOGW(LOG_TAG,"Invalid AT commandlength %d",received);
          ESP_LOG_BUFFER_HEXDUMP(LOG_TAG,commandBuffer,received,ESP_LOG_DEBUG);
        }
        halSerialFlushRX();
        continue;
      }
      
      //do parsing :-)
      //simplified into smaller functions, to make reading easy.
      //each parser returns either:
      //0 if the command was not consumed (not a valid command for this part)
      //1 if the command was consumed and needs to be forwarded to a task (singleshot/VB reload)
      //2 if the command was consumed but no further action is needed
      
      parserstate = doMouseParsing(commandBuffer,cmdMouse);
      if(parserstate == 2) continue; //don't need further action
      if(parserstate == 1) 
      {
        requestVBTask = task_mouse;
        requestVBParameterSize = sizeof(taskMouseConfig_t);
        requestVBParameter = cmdMouse;
        requestVBType = T_MOUSE;
      }
      
      //if not handled already
      if(parserstate == 0) 
      {
        parserstate = doKeyboardParsing(commandBuffer,cmdKeyboard);
        if(parserstate == 2) continue; //don't need further action
        if(parserstate == 1)
        {
          requestVBTask = task_keyboard;
          requestVBParameterSize = sizeof(taskKeyboardConfig_t);
          requestVBParameter = cmdKeyboard;
          requestVBType = T_KEYBOARD;
        }
      }
      
      //TODO: if necessary, add also the config/task stuff as above...
      if(parserstate == 0) 
      {
        parserstate = doGeneralCmdParsing(commandBuffer);
      }
      if(parserstate == 0) 
      {
        parserstate = doInfraredParsing(commandBuffer);
      }
      if(parserstate == 0) 
      {
        parserstate = doMouthpieceSettingsParsing(commandBuffer);
      }
      
      //call now the task function if in singleshot mode
      if(parserstate == 1 && requestVBUpdate == VB_SINGLESHOT && \
        requestVBTask != NULL && requestVBParameter != NULL)
      {
        //call in singleshot mode
        requestVBTask(requestVBParameter);
        //reset the VB task callback
        requestVBTask = NULL;
        continue;
      }
      
      //if not in singleshot mode, do the config update (virtual buttons)
      if(requestVBUpdate != VB_SINGLESHOT)
      {
        //allocate new memory (the parameter pointers might be differ each time)
        void *vbconfigparam = malloc(requestVBParameterSize);
        if(vbconfigparam != NULL)
        {
          //copy to new memory, which will be permanent after an config update
          memcpy(vbconfigparam,requestVBParameter,requestVBParameterSize);
        } else {
          ESP_LOGE(LOG_TAG,"Error allocating VB config!");
        }
        //trigger VB config update (which loads the config and triggers the update in config_switcher.c)
        if(configUpdateVB(vbconfigparam,requestVBType,requestVBUpdate) != ESP_OK)
        {
          ESP_LOGE(LOG_TAG,"Error updating VB config!");
        }
        //reset to singleshot mode
        requestVBUpdate = VB_SINGLESHOT;
        continue;
      }
      
      //if a general config update is required
      if(requestUpdate != 0)
      {
        if(configUpdate() != ESP_OK)
        {
          ESP_LOGE(LOG_TAG,"Error updating general config!");
        }
        requestUpdate = 0;
        continue;
      }
      
      //after all that parsing & config updates
      //we either have a parserstate == 0 here -> no parser found
      //or we consumed this command.
      if(parserstate != 0) continue;
      
      //check reply by LPC USB chip:
      if(memcmp(commandBuffer,"__OK__",6) == 0) continue;
      
      if(memcmp(commandBuffer,"_parameter error",16) == 0)
      {
        ESP_LOGE(LOG_TAG,"USB reply: parameter error");
        continue;
      }
      if(memcmp(commandBuffer,"_unknown command",16) == 0)
      {
        ESP_LOGE(LOG_TAG,"USB reply: unknown cmd");
        continue;
      }
      if(memcmp(commandBuffer,"_unknown error code",19) == 0)
      {
        ESP_LOGE(LOG_TAG,"USB reply: unknown error code");
        continue;
      }
      
      //if we are here, no parser was finding commands
      ESP_LOGW(LOG_TAG,"Invalid AT cmd (%d characters), flushing:",received);
      ESP_LOG_BUFFER_CHAR_LEVEL(LOG_TAG,commandBuffer,received, ESP_LOG_WARN);
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
