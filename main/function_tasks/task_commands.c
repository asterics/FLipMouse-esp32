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
 * 
 * This file contains the main command parser for serial AT commands.
 * 
 * Supported AT-commands: see header file
 */
 
/** UNIMPLEMENTED AT commands
 *  
          AT                returns "OK"
          AT ID             returns identification string (e.g. "FLipMouse V2.0")
          AT BM <uint>      puts button into programming mode (e.g. "AT BM 2" -> next AT-command defines the new function for button 2)
                            virtual buttons are defined in common.h

    USB HID commands:

          AT RO <uint>      rotate stick orientation (e.g. "AT RO 180" flips x any y movements)  

          AT JX <int>       set joystick x axis (e.g. "AT JX 512" sets the x-axis to middle position)  
          AT JY <int>       set joystick y axis (e.g. "AT JY 1023" sets the y-axis to full up position)  
          AT JZ <int>       set joystick z axis (e.g. "AT JZ 0" sets the z-axis to lowest position)  
          AT JT <int>       set joystick z turn axis (e.g. "AT JR 512" sets the rotation to middle position)  
          AT JS <int>       set joystick slider position (e.g. "AT JS 512" sets the slider to middle position)  
          AT JP <int>       press joystick button (e.g. "AT JP 1" presses joystick button 1)  
          AT JR <int>       release joystick button (e.g. "AT JP 2" releases joystick button 2)  
          AT JH <int>       set joystick hat position (e.g. "AT JH 45" sets joystick hat to 45 degrees)
                            possible values are: 0, 45, 90, 135, 180, 225, 270, 315 and -1 to set center position)   

          AT KW <string>    keyboard write string (e.g." AT KW Hello!" writes "Hello!")    
          AT KP <string>    key press: press/hold keys identifier 
                            (e.g. "AT KP KEY_UP" presses the "Cursor-Up" key, "AT KP KEY_CTRL KEY_ALT KEY_DELETE" presses all three keys)
          AT KH <string>    key hold: sticky hold keys (key will be pressed until next button press or "AT KH" command) 
                            (e.g. "AT KH KEY_A" presses the "A" key until  "AT KH KEY_A" is called again)
                            for a list of supported key identifier strings see below ! 
                            
          AT KR <string>    key release: releases all keys identified in the string    
          AT RA             release all: releases all currently pressed keys and buttons    
          
    Housekeeping commands:

          AT SA <string>  save settings and current button modes to next free eeprom slot under given name (e.g. AT SAVE mouse1)
          AT LO <string>  load button modes from eeprom slot (e.g. AT LOAD mouse1 -> loads profile named "mouse1")
          AT LA           load all slots (displays names and settings of all stored slots) 
          AT LI           list all saved mode names 
          AT NE           next mode will be loaded (wrap around after last slot)
          AT DE           delete EEPROM content (delete all stored slots)
          AT NC           no command (idle operation)
          AT E0           turn echo off (no debug output on serial console, default and GUI compatible)
          AT E1           turn echo on (debug output on serial console)
          AT E2           turn echo on (debug output on serial console), extended output
          AT BT <uint>    set bluetooth mode, 1=USB only, 2=BT only, 3=both(default) 
                          (e.g. AT BT 2 -> send HID commands only via BT if BT-daughter board is available)
          
    FLipMouse-specific settings and commands:

          AT MM <uint>    mouse mode: cursor on (uint==1) or alternative functions on (uint==0)
          AT SW           switch between mouse cursor and alternative functions
          AT SR           start reporting raw values (5 sensor values, starting with "VALUES:") 
          AT ER           end reporting raw values
          AT CA           calibration of zeropoint
          AT AX <uint>    acceleration x-axis  (0-100)
          AT AY <uint>    acceleration y-axis  (0-100)
          AT DX <uint>    deadzone x-axis  (0-1000)
          AT DY <uint>    deadzone y-axis  (0-1000)
          AT MS <uint>    maximum speed  (0-100)
          AT AC <uint>    acceleration time (0-100)
          AT MA <string>  execute a command macro containing multiple commands (separated by semicolon) 
                          example: "AT MA MX 100;MY 100;CL;"  use backslash to mask semicolon: "AT MA KW \;;CL;" writes a semicolon and then clicks left 
          AT WA <uint>    wait (given in milliseconds, useful for macro commands)

          AT TS <uint>    treshold for sip action  (0-512)
          AT TP <uint>    treshold for puff action (512-1023)
          AT SP <uint>    treshold for strong puff (512-1023)
          AT SS <uint>    treshold for strong sip (0-512)
          AT GU <uint>    gain for up sensor (0-100)
          AT GD <uint>    gain for down sensor (0-100)
          AT GL <uint>    gain for left sensor (0-100)
          AT GR <uint>    gain for right sensor (0-100)
  
    Infrared-specific commands:

          AT IR <string>  record new infrared code and store it under given name (e.g. "AT IR vol_up")
          AT IP <string>  play  infrared code with given name (e.g. "AT IP vol_up")
          AT IC <string>  clear infrared code with given name (e.g. "AT IC vol_up")
          AT IW           wipe infrared memory (clear all codes)
          AT IL           lists all stored infrared command names
          AT IT <uint>    set code timeout value for IR Recording (e.g. "AT IT 10" sets 10 milliseconds timeout)
          * 
*/
 
#include "task_commands.h"

#define LOG_TAG "cmdparser"
 
static TaskHandle_t currentCommandTask = NULL;
uint8_t doMouseParsing(uint8_t *cmdBuffer, taskMouseConfig_t *mouseinstance);
uint8_t doKeyboardParsing(uint8_t *cmdBuffer, taskKeyboardConfig_t *kbdinstance);

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

  while(1)
  {
    if(queuesready)
    {
      //wait for incoming data, 100ticks maximum
      received = halSerialReceiveUSBSerial(commandBuffer,ATCMD_LENGTH,100);
      //check received data at least for length
      if(received < 5)
      {
        ESP_LOGW(LOG_TAG,"Invalid AT commandlength %d",received);
        if(received > 0) ESP_LOG_BUFFER_HEXDUMP(LOG_TAG,commandBuffer,received,ESP_LOG_DEBUG);
        continue;
      }
      
      //do parsing :-)
      //simplified into smaller functions, to make reading easy.
      if(doMouseParsing(commandBuffer,&cmdMouse)) continue;
      if(doKeyboardParsing(commandBuffer,&cmdKeyboard)) continue;
      

      //wait 30ms ticks, in case a long UART frame is transmitted
      vTaskDelay(30/portTICK_PERIOD_MS);
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
