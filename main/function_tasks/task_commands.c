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
 /** 
 * @file
 * @brief CONTINOUS TASK - Main command parser for serial AT commands.
 * 
 * This module is used to parse any incoming serial data from hal_serial
 * for valid AT commands.
 * If a valid command is detected, the corresponding action is triggered
 * (mostly by invoking a FUNCTIONAL task in singleshot mode).
 * 
 * By issueing an <b>AT BM</b> command, the next issued AT command
 * will be assigned to a virtual button. This is done via setting
 * the requestVBUpdate variable to the VB number. One time only commands
 * (without AT BM) are defined as VB==VB_SINGLESHOT
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
 * 
 * @todo CIM switching
 * 
 * @note Currently we are using unicode_to_keycode, because terminal programs use it this way.
 * For supporting unicode keystreams (UTF-8), the method parse_for_keycode needs to be used
 * */
#include "task_commands.h"

/** Tag for ESP_LOG logging */
#define LOG_TAG "cmdparser"
/** @brief Set a global log limit for this file */
#define LOG_LEVEL_CMDPARSER ESP_LOG_INFO

/** Simple macro to short the command comparison (full AT cmd with 5 chars) */
#define CMD(x) (memcmp(cmdBuffer,x,5) == 0)

/** Simple macro to short the command comparison (part of AT cmd with 4 chars) */
#define CMD4(x) (memcmp(cmdBuffer,x,4) == 0)
 
static TaskHandle_t currentCommandTask = NULL;

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

/** @brief Current state of parser
 * 
 * Following states are possible for the parser here:<br>
 * * <b>NOACTION</b> An AT command was found, but no further action is taken afterwards.
 *   E.g, AT AX sets the accleration directly to the config and requests a general update
 *   via setting requestUpdate to 1.
 * * <b>WAITFORNEWATCMD</b> In this case, AT BM was sent to set a new command for a virtual
 *   button. The next AT command will NOT be triggering anything. It will be used as new cmd.
 *   An error will be raised, if the following command cannot be used for VBs (e.g., AT AX).
 * * <b>UNKNOWNCMD</b> No command was found (is returned for each subparser if no command
 *   is found)
 * * <b>TRIGGERTASK</b> If this is returned by sub-parsers, the main task needs to
 *   trigger/process this command, either by assigning it to a VB or triggering it as
 *   singleshot task.
 * * <b>HID</b> This state is used to determine an HID command, which is either directly sent
 *   the BLE/USB queue (VB_SINGLESHOT) or added to the HID task.
 * */
typedef enum pstate {NOACTION,UNKNOWNCMD,VB,HID} parserstate_t;

/** simple helper function which sends back to the USB host "?"
 * and prints an error on the console with the given extra infos. */
void sendErrorBack(const char* extrainfo)
{
  ESP_LOGE(LOG_TAG,"Error parsing cmd: %s",extrainfo);
  halSerialSendUSBSerial((char*)"?",sizeof("?"),20);
}

/** @brief Print the current slot configurations (general settings + VBs)
 * 
 * This method prints the current slot configurations to the serial
 * interface. Used for "AT LA" and "AT LI" command, which lists all slots.
 * @param printconfig If set != 0, the config is printed. If 0 only slotnames
 * are printed.
 **/
void printAllSlots(uint8_t printconfig);

/** @brief Save current config to flash
 * 
 * This method is used to reverse parse each module's setup to be saved in
 * an AT command format. A valid tid is necessary for saving data.
 * 
 * @param slotname Slot name*/
void storeSlot(char* slotname);

/** just check all queues if they are initialized
 * @return 0 on uninitialized queues, 1 if all are initialized*/
static int checkqueues(void)
{
  // check HID queues
  if(hid_usb == 0) return 0;
  if(hid_ble == 0) return 0;
  
  //house-keeping queues
  if(config_switcher == 0) return 0;
  
  return 1;
}

/** @brief Helper to send a HID cmd directly to queues, depending on connection status
 * @param cmd Hid command */
void sendHIDCmd(hid_cmd_t *cmd)
{
  //post values to mouse queue (USB and/or BLE)
  if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_USB)
  { xQueueSend(hid_usb,cmd,0); }
  
  if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_BLE)
  { xQueueSend(hid_ble,cmd,0); }
}

parserstate_t doMouthpieceSettingsParsing(uint8_t *cmdBuffer)
{
  generalConfig_t *currentcfg = configGetCurrent();
  
  vb_cmd_t cmd;
  //clear any previous data
  memset(&cmd,0,sizeof(vb_cmd_t));
  
  /*++++ calibrate mouthpiece ++++*/
  if(CMD("AT CA"))
  {
    //calibrate now or send command to task_vb
    if(requestVBUpdate == VB_SINGLESHOT)
    {
      halAdcCalibrate();
    } else {
      vb_cmd_t cmd;
      memset(&cmd,0,sizeof(vb_cmd_t));
      cmd.vb = requestVBUpdate | 0x80;
      cmd.cmd = T_CALIBRATE;
      cmd.atoriginal = malloc(strlen("AT CA")+1);
      if(cmd.atoriginal != NULL)
      {
        strcpy(cmd.atoriginal,"AT CA");
      } else ESP_LOGE(LOG_TAG,"Error allocating AT cmd string");
      task_vb_addCmd(&cmd,1);
    }
    ESP_LOGI(LOG_TAG,"Calibrate");
    return VB;
  }
  
  /*++++ stop report raw values ++++*/
  if(CMD("AT ER"))
  {
    currentcfg->adc.reportraw = 0;
    requestUpdate = 1;
    ESP_LOGI(LOG_TAG,"Stop reporting raw values");
    return NOACTION;
  }
  /*++++ start report raw values to serial ++++*/
  if(CMD("AT SR"))
  {
    currentcfg->adc.reportraw = 1;
    requestUpdate = 1;
    ESP_LOGI(LOG_TAG,"Start reporting raw values");
    return NOACTION;
  }
  
  /*++++ mouthpiece mode ++++*/
  if(CMD("AT MM"))
  {
    //assign ADC mode accordingly
    switch(cmdBuffer[6])
    {
      case '0':
        ESP_LOGI(LOG_TAG,"AT MM set to threshold");
        currentcfg->adc.mode = THRESHOLD; 
        requestUpdate = 1;
        return NOACTION;
        break;
      case '1': 
        ESP_LOGI(LOG_TAG,"AT MM set to mouse");
        currentcfg->adc.mode = MOUSE; 
        requestUpdate = 1; 
        return NOACTION;
        break;
      case '2': 
        ESP_LOGI(LOG_TAG,"AT MM set to joystick");
        currentcfg->adc.mode = JOYSTICK; 
        requestUpdate = 1; 
        return NOACTION;
        break;
      default: 
        sendErrorBack("Mode is 0,1 or 2"); 
        return UNKNOWNCMD;
    }
  }
  
  /*++++ mouthpiece mode - switch ++++*/
  if(CMD("AT SW"))
  {
    switch(currentcfg->adc.mode)
    {
      case MOUSE: currentcfg->adc.mode = THRESHOLD; break;
      case THRESHOLD: currentcfg->adc.mode = MOUSE; break;
      case JOYSTICK: sendErrorBack("AT SW switches between mouse and threshold only"); break;
    }
    requestUpdate = 1;
    return NOACTION;
  }
  
  /*++++ mouthpiece gain ++++*/
  //AT GU, GD, GL, GR
  if(CMD4("AT G"))
  {
    unsigned int param = strtol((char*)&(cmdBuffer[5]),NULL,10);
    ESP_LOGI(LOG_TAG,"Gain %c, %d",cmdBuffer[4],param);
    if(param > 100)
    {
      sendErrorBack("Gain is 0-100");
      return UNKNOWNCMD;
    } else {
      //assign to gain value
      switch(cmdBuffer[4])
      {
        case 'U': currentcfg->adc.gain[0] = param; requestUpdate = 1; return NOACTION;
        case 'D': currentcfg->adc.gain[1] = param; requestUpdate = 1; return NOACTION;
        case 'L': currentcfg->adc.gain[2] = param; requestUpdate = 1; return NOACTION;
        case 'R': currentcfg->adc.gain[3] = param; requestUpdate = 1; return NOACTION;
        default: return UNKNOWNCMD;
      }
    }
  }
  
  /*++++ mouthpiece sensitivity/acceleration ++++*/
  //AT AX, AY, AC
  if(CMD4("AT A"))
  {
    unsigned int param = strtol((char*)&(cmdBuffer[5]),NULL,10);
    ESP_LOGI(LOG_TAG,"Sensitivity/accel %c, %d",cmdBuffer[4],param);
    if(param > 100)
    {
      sendErrorBack("Sensitivity/accel is 0-100");
      return UNKNOWNCMD;
    } else {
      //assign to sensitivity/acceleration value
      switch(cmdBuffer[4])
      {
        case 'X': currentcfg->adc.sensitivity_x = param; requestUpdate = 1; return NOACTION;
        case 'Y': currentcfg->adc.sensitivity_y = param; requestUpdate = 1; return NOACTION;
        case 'C': currentcfg->adc.acceleration = param; requestUpdate = 1; return NOACTION;
        default: return UNKNOWNCMD;
      }
    }
  }
  
  /*++++ mouthpiece deadzone ++++*/
  //AT DX, DY
  if(CMD4("AT D"))
  {
    unsigned int param = strtol((char*)&(cmdBuffer[5]),NULL,10);
    ESP_LOGI(LOG_TAG,"Deadzone %c, %d",cmdBuffer[4],param);
    if(param > 10000)
    {
      sendErrorBack("Deadzone is 0-10000");
      return UNKNOWNCMD;
    } else {
      //assign to sensitivity/acceleration value
      switch(cmdBuffer[4])
      {
        case 'X': currentcfg->adc.deadzone_x = param; requestUpdate = 1; return NOACTION;
        case 'Y': currentcfg->adc.deadzone_y = param; requestUpdate = 1; return NOACTION;
        default: return UNKNOWNCMD;
      }
    }
  }
  
  /*++++ on the fly calibration ++++*/
  //AT DX, DY
  if(CMD4("AT O"))
  {
    unsigned int param = strtol((char*)&(cmdBuffer[5]),NULL,10);
    //assign to threshold or counter
    switch(cmdBuffer[4])
    {
      case 'C': 
        if(param < 5 || param > 15)
        {
          sendErrorBack("OTF counter: 5-15");
          return UNKNOWNCMD;
        }
        currentcfg->adc.otf_count = param; requestUpdate = 1; 
        ESP_LOGI(LOG_TAG,"OTF counter: %d",param);
        return NOACTION;
      case 'T': 
        if(param > 15)
        {
          sendErrorBack("OTF idle threshold: 0-15");
          return UNKNOWNCMD;
        }
        currentcfg->adc.otf_idle = param; requestUpdate = 1; 
        ESP_LOGI(LOG_TAG,"OTF idle threshold: %d",param);
        return NOACTION;
      default: return UNKNOWNCMD;
    }
  }
  
  /*++++ mouthpiece mouse - maximum speed ++++*/
  //AT MS
  if(CMD("AT MS"))
  {
    unsigned int param = strtol((char*)&(cmdBuffer[5]),NULL,10);
    ESP_LOGI(LOG_TAG,"Maximum speed %d",param);
    if(param > 100)
    {
      sendErrorBack("Maximum speed is 0-100");
      return UNKNOWNCMD;
    } else {
      //assign to maximum speed
      currentcfg->adc.max_speed = param; 
      requestUpdate = 1; 
      return NOACTION;
    }
  }
  
  /*++++ threshold sip/puff++++*/
  //AT TS/TP
  if(CMD4("AT T"))
  {
    unsigned int param = strtol((char*)&(cmdBuffer[5]),NULL,10);
    ESP_LOGI(LOG_TAG,"Threshold %c, %d",cmdBuffer[4],param);
    switch(cmdBuffer[4])
    {
      case 'S':
        if(param > 512)
        {
          sendErrorBack("Threshold sip is 0-512");
          return UNKNOWNCMD;
        } else {
          currentcfg->adc.threshold_sip = param;
          requestUpdate = 1;
          return NOACTION;
        }
      break;
      
      case 'P':
        if(param < 512 || param > 1023)
        {
          sendErrorBack("Threshold puff is 512-1023");
          return UNKNOWNCMD;
        } else {
          currentcfg->adc.threshold_puff = param;
          requestUpdate = 1;
          return NOACTION;
        }
      break;
      default: return UNKNOWNCMD;
    }
  }
  
  /*++++ threshold strong sip/puff++++*/
  //AT TS/TP
  if(CMD4("AT S"))
  {
    unsigned int param = 0;
    switch(cmdBuffer[4])
    {
      case 'S':
        param = strtol((char*)&(cmdBuffer[5]),NULL,10);
        if(param > 512)
        {
          sendErrorBack("Threshold strong sip is 0-512");
          return UNKNOWNCMD;
        } else {
          
          ESP_LOGI(LOG_TAG,"Threshold strong sip, %d",param);
          currentcfg->adc.threshold_strongsip = param;
          requestUpdate = 1;
          return NOACTION;
        }
      break;
      
      case 'P':
        param = strtol((char*)&(cmdBuffer[5]),NULL,10);
        if(param < 512 || param > 1023)
        {
          sendErrorBack("Threshold strong puff is 512-1023");
          return UNKNOWNCMD;
        } else {
          ESP_LOGI(LOG_TAG,"Threshold strong puff, %d",param);
          currentcfg->adc.threshold_strongpuff = param;
          requestUpdate = 1;
          return NOACTION;
        }
      break;
      default: return UNKNOWNCMD;
    }
  }

  //not consumed, no command found for mouthpiece settings
  return UNKNOWNCMD;
}

parserstate_t doStorageParsing(uint8_t *cmdBuffer)
{
  char slotname[SLOTNAME_LENGTH];
  //generalConfig_t * currentcfg = configGetCurrent();
  vb_cmd_t cmd;
  //clear any data
  memset(&cmd,0,sizeof(vb_cmd_t));
  
  /*++++ save slot ++++*/
  if(CMD("AT SA"))
  {
    //trigger config update
    configUpdate();
    //wait until configuration is stable
    if(configUpdateWaitStable() != ESP_OK)
    {
      ESP_LOGE(LOG_TAG,"Config not stable in time, cannot save");
      return UNKNOWNCMD;
    }
    
    //a copy of slot name
    strncpy(slotname,(char*)&cmdBuffer[6],SLOTNAME_LENGTH);
    //and store it.
    storeSlot(slotname);
    return NOACTION;
  }
  
  /*++++ AT NE (load next) +++*/
  if(CMD("AT NE"))
  {
    strncpy(slotname,"__NEXT",SLOTNAME_LENGTH);
    //save to config
    if(requestVBUpdate == VB_SINGLESHOT)
    {
      xQueueSend(config_switcher,(void*)slotname,(TickType_t)10);
    } else {
      vb_cmd_t cmd;
      memset(&cmd,0,sizeof(vb_cmd_t));
      cmd.vb = requestVBUpdate | 0x80;
      cmd.cmd = T_CONFIGCHANGE;
      cmd.atoriginal = malloc(strlen("AT NE")+1);
      if(cmd.atoriginal != NULL)
      {
        strcpy(cmd.atoriginal,"AT NE");
      } else ESP_LOGE(LOG_TAG,"Error allocating AT cmd string");
      cmd.cmdparam = malloc(strlen("__NEXT")+1);
      if(cmd.cmdparam != NULL)
      {
        strcpy(cmd.cmdparam,"__NEXT");
      } else ESP_LOGE(LOG_TAG,"Error allocating cmdparam strign");
      task_vb_addCmd(&cmd,1);
    }
    ESP_LOGI(LOG_TAG,"Load next");
    return VB;
  }
  
  /*++++ AT LO (load) ++++*/
  if(CMD("AT LO"))
  {
    //save to config
    if(requestVBUpdate == VB_SINGLESHOT)
    {
      xQueueSend(config_switcher,(void*)&cmdBuffer[6],(TickType_t)10);
    } else {
      vb_cmd_t cmd;
      memset(&cmd,0,sizeof(vb_cmd_t));
      cmd.vb = requestVBUpdate | 0x80;
      cmd.cmd = T_CONFIGCHANGE;
      cmd.atoriginal = malloc(strlen((char*)cmdBuffer)+1);
      if(cmd.atoriginal != NULL)
      {
        strcpy(cmd.atoriginal,(char*)cmdBuffer);
      } else ESP_LOGE(LOG_TAG,"Error allocating AT cmd string");
      cmd.cmdparam = malloc(strnlen((char*)&cmdBuffer[6],SLOTNAME_LENGTH)+1);
      if(cmd.cmdparam != NULL)
      {
        strncpy(cmd.cmdparam,(char*)&cmdBuffer[6],SLOTNAME_LENGTH);
      } else ESP_LOGE(LOG_TAG,"Error allocating cmdparam strign");
      task_vb_addCmd(&cmd,1);
    }
    ESP_LOGI(LOG_TAG,"Load slot name: %s",&cmdBuffer[6]);
    return VB;
  }
  //not consumed, no command found for storage
  return UNKNOWNCMD;
}

parserstate_t doJoystickParsing(uint8_t *cmdBuffer, int length)
{
  if(CMD4("AT J"))
  {
    //new hid command
    hid_cmd_t cmd;
    //second command for double click or release action
    hid_cmd_t cmd2;
    
    //set everything up for updates:
    //set vb with press action & clear any first command byte.
    cmd.vb = requestVBUpdate | (0x80);
    cmd2.vb = requestVBUpdate;
    cmd.cmd[0] = 0;
    cmd2.cmd[0] = 0;
    //just to shorten following checks a little bit.
    char t = cmdBuffer[4];
    
    //param 1
    char *endparam1;
    int32_t param1 = strtol((char*)&(cmdBuffer[5]),&endparam1,10);
    //param 2 (release mode for commands), starting with end of param1 parsing
    unsigned int param2 = strtol(endparam1,NULL,10);
    
    //parameter checks - buttons
    if(t == 'P' || t == 'C' || t == 'R')
    {
      if(param1 > 32 || param1 < 1)
      {
        ESP_LOGE(LOG_TAG,"Joystick button out of range: %d",param1);
        sendErrorBack("Joystick button invalid");
        return UNKNOWNCMD;
      } else cmd2.cmd[1] = param1; cmd.cmd[1] = param1;
    }
    
    //parameter checks - hat
    if(t == 'H')
    {
      if(param1 > 315 || param1 < -1)
      {
        ESP_LOGE(LOG_TAG,"Joystick hat out of range: %d",param1);
        sendErrorBack("Joystick hat value invalid");
        return UNKNOWNCMD;
      } else cmd2.cmd[1] = param1 | 0x80; cmd.cmd[1] = param1 | 0x80;
    }
    
    //parameter checks - axis
    if(t == 'X' || t == 'Y' || t == 'Z' || t == 'T' || t == 'S' || t == 'U')
    {
      if(param1 > 1023 || param1 < 0)
      {
        ESP_LOGE(LOG_TAG,"Joystick axis out of range: %d",param1);
        sendErrorBack("Joystick axis value invalid");
        return UNKNOWNCMD;
      } else {
        //this is the "set" action, update joystick value.
        cmd.cmd[1] = param1 & 0xFF;
        cmd.cmd[2] = (param1 & 0xFF00)>>8;
        //if used, this would be the release action (setting to 512 for all axis; to 0 for sliders)
        cmd2.cmd[1] = 0x00;
        if(t == 'S' || t == 'U') cmd2.cmd[2] = 0; else cmd2.cmd[2] = 0x02;
      }
    }
    
    //test release mode, but only if not a button.
    if(!(t=='P' || t=='C' || t=='R'))
    {
      if(param2 > 1)
      {
        ESP_LOGW(LOG_TAG,"Joystick release mode invalid: %d, reset to 0",param2);
        param2 = 0;
      }
    } else {
      param2 = 0;
    }
    
    //map AT command (5th character) to joystick command
    switch(t)
    {
      case 'X': cmd.cmd[0] = 0x34; if(param2) { cmd2.cmd[0] = 0x34; } break;
      case 'Y': cmd.cmd[0] = 0x35; if(param2) { cmd2.cmd[0] = 0x35; } break;
      case 'Z': cmd.cmd[0] = 0x36; if(param2) { cmd2.cmd[0] = 0x36; } break;
      case 'T': cmd.cmd[0] = 0x37; if(param2) { cmd2.cmd[0] = 0x37; } break;
      case 'S': cmd.cmd[0] = 0x38; if(param2) { cmd2.cmd[0] = 0x38; } break;
      case 'U': cmd.cmd[0] = 0x39; if(param2) { cmd2.cmd[0] = 0x39; } break;
      case 'H': cmd.cmd[0] = 0x31; if(param2) { cmd2.cmd[0] = 0x32; } break;
      case 'P': cmd.cmd[0] = 0x31; if(param2) { cmd2.cmd[0] = 0x32; } break;
      case 'C': cmd.cmd[0] = 0x30;
      case 'R': cmd.cmd[0] = 0x32;
      default: cmd.cmd[0] = 0; cmd2.cmd[0] = 0;
    }
    
    //if a command is found
    if(cmd.cmd[0] != 0)
    {
      //if we have a singleshot, pass command to queue for immediate process
      if(cmd.vb == VB_SINGLESHOT)
      {
        //post values to mouse queue (USB and/or BLE)
        if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_USB)
        { xQueueSend(hid_usb,&cmd,0); }
        
        if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_BLE)
        { xQueueSend(hid_ble,&cmd,0); }
        
        //if we have a second action (release immediately), send it
        if(cmd2.cmd[0] != 0)
        {
          //post values to mouse queue (USB and/or BLE)
          if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_USB)
          { xQueueSend(hid_usb,&cmd2,0); }
          
          if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_BLE)
          { xQueueSend(hid_ble,&cmd2,0); }
        }
      } else {
        //if it is assigned to a vb, 
        //allocate original string (max len for joystick is ~16)
        //add it to HID task
        cmd.atoriginal = malloc(strnlen((char*)cmdBuffer,16)+1);
        if(cmd.atoriginal == NULL) 
        {
          ESP_LOGE(LOG_TAG,"No memory for original AT");
        } else {
          strncpy(cmd.atoriginal,(char*)cmdBuffer,16);
        }
        if(task_hid_addCmd(&cmd,1) != ESP_OK)
        {
          ESP_LOGE(LOG_TAG,"Error adding to HID task");
        }
        
        //if we have double action (double click)
        //or press/release action, we have to assign a second HID command.
        if(cmd2.cmd[0] != 0)
        {
          if(task_hid_addCmd(&cmd2,0) != ESP_OK)
          {
            ESP_LOGE(LOG_TAG,"Error adding 2nd to HID task");
          }
        }
        //reset to singleshot mode
        requestVBUpdate = VB_SINGLESHOT;
      }
      return HID;
    }
  }
  //not consumed, no command found for storage
  return UNKNOWNCMD;
}

parserstate_t doInfraredParsing(uint8_t *cmdBuffer)
{
  uint32_t tid;
  uint8_t param;
  vb_cmd_t cmd;

  generalConfig_t *currentcfg = configGetCurrent();
  if(currentcfg == NULL)
  {
    ESP_LOGE(LOG_TAG,"Errroorrr: general config is NULL");
    return UNKNOWNCMD;
  }
  
  //use global virtual button (normally VB_SINGLESHOT,
  //can be changed by "AT BM"
  memset(&cmd,0,sizeof(vb_cmd_t));
  cmd.vb = requestVBUpdate | 0x80;

  /*++++ AT IW ++++*/
  if(CMD("AT IW")) {
    if(halStorageStartTransaction(&tid,20,LOG_TAG) == ESP_OK)
    {
      if(halStorageDeleteIRCmd(100,tid) != ESP_OK)
      {
        ESP_LOGE(LOG_TAG,"Cannot delete all IR cmds");
      }
      halStorageFinishTransaction(tid);
    } else {
      ESP_LOGE(LOG_TAG,"Cannot start transaction");
    }
    return NOACTION;
  }
  
  
  /*++++ AT IX (delete one IR cmd by number) ++++*/
  if(CMD("AT IX")) {
    param = strtol((char*)&(cmdBuffer[6]),NULL,10);
    if(param < 1 || param > 99)
    {
      ESP_LOGW(LOG_TAG,"Invalid IR slot nr %d",param);
      return UNKNOWNCMD;
    }
    if(halStorageStartTransaction(&tid,20,LOG_TAG) == ESP_OK)
    {
      if(halStorageDeleteIRCmd(param,tid) != ESP_OK)
      {
        ESP_LOGE(LOG_TAG,"Cannot delete IR cmd %d",param);
      }
      halStorageFinishTransaction(tid);
    } else {
      ESP_LOGE(LOG_TAG,"Cannot start transaction");
    }
    return NOACTION;
  }
  
  /*++++ AT IT ++++*/
  if(CMD("AT IT")) {
    //set the IR timeout in ms
    param = strtol((char*)&(cmdBuffer[6]),NULL,10);
    if(param <= 100 && param >= 2)
    {
      currentcfg->irtimeout = param;
      ESP_LOGD(LOG_TAG,"Set IR timeout to %d ms",param);
      requestUpdate = 1;
    } else {
      ESP_LOGE(LOG_TAG,"IR timeout %d is not possible",param);
    }
    return NOACTION;
  }
  
  /*++++ AT IC ++++*/
  if(CMD("AT IC")) {
    if(halStorageStartTransaction(&tid,20,LOG_TAG) == ESP_OK)
    {
      uint8_t nr = 0;
      if(halStorageGetNumberForNameIR(tid,&nr,(char*)&cmdBuffer[6]) == ESP_OK)
      {
        if(halStorageDeleteIRCmd(nr,tid) != ESP_OK)
        {
          ESP_LOGE(LOG_TAG,"Cannot delete IR cmd %s",&cmdBuffer[6]);
        } else {
          ESP_LOGI(LOG_TAG,"Deleted IR slot %s @%d",&cmdBuffer[6],nr);
          return NOACTION;
        }
      } else {
        ESP_LOGE(LOG_TAG,"No slot found for IR cmd %s",&cmdBuffer[6]);
      }
      halStorageFinishTransaction(tid);
    } else {
      ESP_LOGE(LOG_TAG,"Cannot start transaction");
    }
  }
  
  /*++++ AT IP ++++*/
  if(CMD("AT IP")) {
    //save cmd name to config instance
    
    ///@todo IP singleshot/cmd adding... + malloc strings!!!
    
    //strncpy(instance->cmdName,(char*)&cmdBuffer[6],SLOTNAME_LENGTH);
    ESP_LOGD(LOG_TAG,"Play IR cmd %s",&cmdBuffer[6]);
    return VB;
  }
  
  /*++++ AT IR ++++*/
  if(CMD("AT IR")) {
    //trigger record
    if(infrared_record((char*)&cmdBuffer[6],1) == ESP_OK)
    {
      ESP_LOGD(LOG_TAG,"Recorded IR cmd %s",&cmdBuffer[6]);
      return NOACTION;
    } else {
      return UNKNOWNCMD;
    }
  }
  
  /*++++ AT IL ++++*/
  if(CMD("AT IL")) {
    if(halStorageStartTransaction(&tid,20,LOG_TAG) == ESP_OK)
    {
      uint8_t count = 0;
      uint8_t printed = 0;
      char name[SLOTNAME_LENGTH+1];
      char output[SLOTNAME_LENGTH+10];
      halStorageGetNumberOfIRCmds(tid,&count);
      if(halStorageGetNumberOfIRCmds(tid,&count) == ESP_OK)
      {
        for(uint8_t i = 0; i<100;i++)
        {
          //load name, if available
          if(halStorageGetNameForNumberIR(tid,i,name) == ESP_OK)
          {
            //if available: print out on serial and increase number of 
            //printed IR cmds
            sprintf(output,"IRCommand%d:%s",printed,name);
            halSerialSendUSBSerial(output,strnlen(output,SLOTNAME_LENGTH+10),10);
            printed++;
          }
          //if we have printed the same count as available IR slots, finish
          if(printed == count) 
          {
            halStorageFinishTransaction(tid);
            return NOACTION;
          }
        }
      } else {
        ESP_LOGE(LOG_TAG,"Cannot get IR cmd number");
      }
      halStorageFinishTransaction(tid);
    } else {
      ESP_LOGE(LOG_TAG,"Cannot start transaction");
    }
    return UNKNOWNCMD;
  }
  //not consumed, no command found for infrared
  return UNKNOWNCMD;
}

parserstate_t doGeneralCmdParsing(uint8_t *cmdBuffer)
{
  uint16_t param = 0;
  uint8_t param8 = 0;
  generalConfig_t *currentcfg;
  
  currentcfg = configGetCurrent();
  if(currentcfg == NULL)
  {
    ESP_LOGE(LOG_TAG,"Current config is null, cannot update general cmd");
    return UNKNOWNCMD;
  }
  
  /*++++ AT FB; feedback ++++*/
  if(CMD("AT FB")) {
    switch(cmdBuffer[6])
    {
      case '0':
      case '1':
      case '2':
      case '3':
        currentcfg->feedback = cmdBuffer[6] - '0';
        ESP_LOGD(LOG_TAG,"Setting feedback mode to %d",currentcfg->feedback);
        break;
      default:
        sendErrorBack("Parameter out of range (0-3)");
        return UNKNOWNCMD;
    }
    requestUpdate = 1;
    return NOACTION;  
  }
  
  /*++++ AT PW; wifi password ++++*/
  if(CMD("AT PW")) {
    if(strnlen((char*)&cmdBuffer[6],ATCMD_LENGTH-6) >= 8 && strnlen((char*)&cmdBuffer[6],ATCMD_LENGTH-6) <= 32)
    {
      halStorageNVSStoreString(NVS_WIFIPW,(char*)&cmdBuffer[6]);
    } else {
      sendErrorBack("Wifi PW len: 8-32 characters");
      return UNKNOWNCMD;
    }
    requestUpdate = 1;
    return NOACTION;  
  }
  
  /*++++ AT BL; button learning ++++*/
  if(CMD("AT BL")) {
    switch(cmdBuffer[6])
    {
      case '0':
      case '1':
        currentcfg->button_learn = cmdBuffer[6] - '0';
        ESP_LOGD(LOG_TAG,"Setting button learn mode to %d",currentcfg->button_learn);
        break;
      default:
        sendErrorBack("Parameter out of range (0-1)");
        return UNKNOWNCMD;
    }
    requestUpdate = 1;
    return NOACTION;  
  }
  
  ///@todo parse anti-tremor commands here.
  /*++++ AT AP,AR,AI; anti-tremor ++++*/
  if(CMD4("AT A")) {
    //get the time value ([ms])
    param = strtol((char*)&(cmdBuffer[6]),NULL,10);
      
    if(cmdBuffer[4] == 'P' || cmdBuffer[4] == 'R' || cmdBuffer[4] == 'I')
    {
      //check parameter range
      if(param == 0 || param > 500)
      {
        sendErrorBack("Time out of range");
        return UNKNOWNCMD;
      }
      
      //should we apply it as global value or for an individual button?
      if(requestVBUpdate == VB_SINGLESHOT)
      {
        switch(cmdBuffer[4])
        {
          case 'P': currentcfg->debounce_press = param;
          case 'R': currentcfg->debounce_release = param;
          case 'I': currentcfg->debounce_idle = param;
        }
      } else {
        switch(cmdBuffer[4])
        {
          case 'P': currentcfg->debounce_press_vb[requestVBUpdate] = param;
          case 'R': currentcfg->debounce_release_vb[requestVBUpdate] = param;
          case 'I': currentcfg->debounce_idle_vb[requestVBUpdate] = param;
        }
        //reset to VB_SINGLESHOT after setting anti-tremor time.
        requestVBUpdate = VB_SINGLESHOT;
      }
      //need to update.
      requestUpdate = 1;
    }
  }
  
  /*++++ AT RO ++++*/
  if(CMD("AT RO")) {
    //set the mouthpiece orientation
    param = strtol((char*)&(cmdBuffer[6]),NULL,10);
    switch(param)
    {
      case 0:
      case 90:
      case 180:
      case 270:
        currentcfg->adc.orientation = param;
        ESP_LOGI(LOG_TAG,"Set orientation to %d",param);
        requestUpdate = 1;
        break;
      default:
        ESP_LOGE(LOG_TAG,"Orientation %d not available",param);
        break;
    }
    return NOACTION;
  }
  
    
  /*++++ AT RA ++++*/
  if(CMD("AT RA")) {
    //reset the reports (keyboard only, excepting all other parts)
    halBLEReset(0xFE);
    halSerialReset(0xFE);
    return NOACTION;
  }
    
  /*++++ AT FR (free space) ++++*/
  if(CMD("AT FR")) {
    uint32_t free,total;
    if(halStorageGetFree(&total,&free) != ESP_OK)
    {
      ESP_LOGE(LOG_TAG,"Error getting free space");
    } else {
      char str[32];
      sprintf(str,"FREE:%d%%,%d,%d",(uint8_t)(1-free/total)*100,total-free,free);
      halSerialSendUSBSerial(str,strnlen(str,32),20);
      ESP_LOGI(LOG_TAG,"Free space: %d, total: %d, percentage: %d",free,total,(uint8_t)(1-free/total)*100);
    }
    return NOACTION;
  }
    
  /*++++ AT LA + LI ++++*/
  if(CMD4("AT L")) {
    switch(cmdBuffer[4])
    {
      //print current slot configurations
      case 'A': printAllSlots(1); return NOACTION;
      //print slot names only
      case 'I': printAllSlots(0); return NOACTION;
      default: return UNKNOWNCMD;
    }
  }
    
  /*++++ AT KL ++++*/
  if(CMD("AT KL")) {
    param8 = strtol((char*)&(cmdBuffer[6]),NULL,10);
    if(param8 < LAYOUT_MAX)
    {
      ESP_LOGI(LOG_TAG,"Changed locale from %d to %d",currentcfg->locale,param8);
      currentcfg->locale = param8;
      requestUpdate = 1;
      return NOACTION;
    } else {
      sendErrorBack("Locale out of range");
      return UNKNOWNCMD;
    }
  }
  
  /*++++ AT ID ++++*/
  if(CMD("AT ID")) {
    uint32_t sent = halSerialSendUSBSerial((char*)IDSTRING,sizeof(IDSTRING),20);
    if(sent != sizeof(IDSTRING)) 
    {
      ESP_LOGE(LOG_TAG,"Error sending response of AT ID");
      return UNKNOWNCMD;
    } else return NOACTION;
  }
  
  /*++++ AT DE ++++*/
  if(CMD("AT DE")) {
    uint32_t tid;
    if(halStorageStartTransaction(&tid,20,LOG_TAG) != ESP_OK)
    {
      return UNKNOWNCMD;
    } else {
      if(halStorageDeleteSlot(-1,tid) != ESP_OK)
      {
        sendErrorBack("Error deleting all slots");
        halStorageFinishTransaction(tid);
        return UNKNOWNCMD;
      } else {
        halStorageFinishTransaction(tid);
        return NOACTION;
      }
    }
  }
  
  /*++++ AT BM ++++*/
  if(CMD("AT BM")) {
    param = strtol((char*)&(cmdBuffer[6]),NULL,10);
    if(param >= VB_MAX)
    {
      sendErrorBack("VB nr too high");
      return UNKNOWNCMD;
    } else {
      requestVBUpdate = param;
      ESP_LOGI(LOG_TAG,"New mode for VB %d:",param);
      return NOACTION;
    }
  }
  /*++++ AT NC ++++*/
  if(CMD("AT NC")) {
    ///@todo remove this VB from HID&VB list!
    
    //reset VB number if NC is triggered -> VB is not used.
    requestVBUpdate = VB_SINGLESHOT;
    return NOACTION;
  }
  
  /*++++ AT DL ++++*/
  if(CMD4("AT D")) {
    uint32_t tid;
    uint8_t slotnumber;
    
    if(halStorageStartTransaction(&tid,20,LOG_TAG) != ESP_OK)
    {
      return UNKNOWNCMD;
    }
    
    switch(cmdBuffer[4])
    {
      //delete by given number
      case 'L': slotnumber = strtol((char*)&(cmdBuffer[6]),NULL,10); break;
      //delete by given name
      case 'N': 
        halStorageGetNumberForName(tid,&slotnumber,(char *)&cmdBuffer[6]); 
        break;
      default: 
        halStorageFinishTransaction(tid);
        return UNKNOWNCMD;
    }
    
    if(halStorageDeleteSlot(slotnumber,tid) != ESP_OK)
    {
      sendErrorBack("Error deleting slot");
      halStorageFinishTransaction(tid);
      return UNKNOWNCMD;
    } else {
      halStorageFinishTransaction(tid);
      return NOACTION;
    }
  }

  /*++++ AT BT ++++*/
  if(CMD("AT BT")) {
    switch(cmdBuffer[6])
    {
      case '0': 
        currentcfg->ble_active = 0;
        currentcfg->usb_active = 0;
        requestUpdate = 1;
        return NOACTION;
        break;
      case '1': 
        currentcfg->ble_active = 0;
        currentcfg->usb_active = 1;
        requestUpdate = 1;
        return NOACTION;
        break;
      case '2': 
        currentcfg->ble_active = 1;
        currentcfg->usb_active = 0;
        requestUpdate = 1;
        return NOACTION;
        break;
      case '3': 
        currentcfg->ble_active = 1;
        currentcfg->usb_active = 1;
        requestUpdate = 1;
        return NOACTION;
        break;
      default: sendErrorBack("AT BT param"); return UNKNOWNCMD;
    }
  }
      
  //not consumed, no general command found
  return UNKNOWNCMD;
}

parserstate_t doKeyboardParsing(uint8_t *cmdBuffer, int length)
{
  
  
  int offset = 0;
  int offsetOut = 0;
  uint8_t deadkeyfirst = 0;
  uint8_t modifier = 0;
  uint8_t keycode = 0;
  
  //get general config (used to get the locale)
  generalConfig_t *currentcfg;
  currentcfg = configGetCurrent();
  
  if(currentcfg == NULL)
  {
    ESP_LOGE(LOG_TAG,"Major error, general config is NULL");
    return UNKNOWNCMD;
  }
  /*++++ AT KW ++++*/
  if(CMD("AT KW")) {
    //remove \r & \n
    //cmdBuffer[length-2] = 0;
    //cmdBuffer[length-1] = 0;
    
    hid_cmd_t cmd;
    memset(&cmd,0,sizeof(hid_cmd_t));
    cmd.vb = requestVBUpdate | (0x80);
    //if it is assigned to a vb, 
    //allocate original string (max len for mouse is 12)
    //add it to HID task
    if(requestVBUpdate != VB_SINGLESHOT)
    {
      if(strnlen((char*)cmdBuffer,ATCMD_LENGTH) == ATCMD_LENGTH)
      {
        ESP_LOGE(LOG_TAG,"Too long AT cmd (or unterminated");
      } else {
        
        cmd.atoriginal = malloc(strnlen((char*)cmdBuffer,ATCMD_LENGTH)+1);
        if(cmd.atoriginal == NULL) 
        {
          ESP_LOGE(LOG_TAG,"No memory for original AT");
        } else {
          strcpy(cmd.atoriginal,(char*)cmdBuffer);
        }
      }
    }
    
    //save each byte from KW string to keyboard instance
    //offset must be less then buffer length - 6 bytes for "AT KW "
    while(offset <= (length - 5))
    {
      //terminate...
      if(cmdBuffer[offset+6] == 0) break;
      
      //parse ASCII/unicode to keycode sequence
      keycode = unicode_to_keycode(cmdBuffer[offset+6], currentcfg->locale);
      deadkeyfirst = deadkey_to_keycode(keycode,currentcfg->locale);
      if(deadkeyfirst != 0) deadkeyfirst = keycode_to_key(deadkeyfirst);
      modifier = keycode_to_modifier(keycode, currentcfg->locale);
      keycode = keycode_to_key(keycode);
      
      //if a keycode is found
      if(keycode != 0)
      {
        //is a deadkey necessary?
        if(deadkeyfirst != 0)
        {
          cmd.cmd[0] = 0x20; //press&release
          cmd.cmd[1] = deadkeyfirst;
          
          //send the cmd either directly or save it to the HID task
          if((cmd.vb & 0x7F) == VB_SINGLESHOT) {
            sendHIDCmd(&cmd);
          } else {
            //if this was the first command in VB mode, we need to
            //remove the pointer to the original string
            //but we don't free it, this is done if the task_hid clears the list
            //in addition, if it was the first command, set the adding to replace any old HID config.
            if(cmd.atoriginal != NULL) 
            {
              task_hid_addCmd(&cmd,1);
              cmd.atoriginal = NULL;
            } else task_hid_addCmd(&cmd,0);
          }
          
          offsetOut++;
          ESP_LOGD(LOG_TAG, "Deadkey 0x%X@%d",deadkeyfirst,offsetOut);
        }
        
        //save keycode + modifier
        ESP_LOGD(LOG_TAG, "Keycode 0x%X@%d, modifier: 0x%X",keycode,offsetOut,modifier);
        
        //we need maximum 3 commands:
        //press a modifier
        //press&release keycode
        //release a modifier
        if(modifier){
          cmd.cmd[0] = 0x25; cmd.cmd[1] = modifier;
          //send the cmd either directly or save it to the HID task
          if((cmd.vb & 0x7F) == VB_SINGLESHOT) {
            sendHIDCmd(&cmd);
            ESP_LOGI(LOG_TAG,"Sent modifier press (0x%2X)",modifier);
          } else {
            //if this was the first command in VB mode, we need to
            //remove the pointer to the original string
            //but we don't free it, this is done if the task_hid clears the list
            //in addition, if it was the first command, set the adding to replace any old HID config.
            if(cmd.atoriginal != NULL) 
            {
              task_hid_addCmd(&cmd,1);
              cmd.atoriginal = NULL;
            } else task_hid_addCmd(&cmd,0);
          }
        }
        
        cmd.cmd[0] = 0x20; cmd.cmd[1] = keycode;
        //send the cmd either directly or save it to the HID task
        if((cmd.vb & 0x7F) == VB_SINGLESHOT) {
          sendHIDCmd(&cmd);
          ESP_LOGI(LOG_TAG,"Sent keycode press&release (0x%2X)",keycode);
        } else {
          //if this was the first command in VB mode, we need to
          //remove the pointer to the original string
          //but we don't free it, this is done if the task_hid clears the list
          //in addition, if it was the first command, set the adding to replace any old HID config.
          if(cmd.atoriginal != NULL) 
          {
            task_hid_addCmd(&cmd,1);
            cmd.atoriginal = NULL;
          } else task_hid_addCmd(&cmd,0);
        }
        
        if(modifier){
          cmd.cmd[0] = 0x26; cmd.cmd[1] = modifier;
          //send the cmd either directly or save it to the HID task
          if((cmd.vb & 0x7F) == VB_SINGLESHOT) {
            sendHIDCmd(&cmd);
            ESP_LOGI(LOG_TAG,"Sent modifier release (0x%2X)",modifier);
          } else {
            //if this was the first command in VB mode, we need to
            //remove the pointer to the original string
            //but we don't free it, this is done if the task_hid clears the list
            //in addition, if it was the first command, set the adding to replace any old HID config.
            if(cmd.atoriginal != NULL) 
            {
              task_hid_addCmd(&cmd,1);
              cmd.atoriginal = NULL;
            } else task_hid_addCmd(&cmd,0);
          }
        }
        
        offsetOut++;
      } else {
        ESP_LOGD(LOG_TAG, "Need another byte (unicode)");
      }
      offset++;

    }
    //reset to singleshot mode
    requestVBUpdate = VB_SINGLESHOT;
    return HID;
  }
  

  /*++++ AT KP + KH + KR + KT ++++*/
  // (all commands with key identifiers)
  if(CMD4("AT K")) {
    
    hid_cmd_t cmd;  //this would be the press or press&release action
    memset(&cmd,0,sizeof(hid_cmd_t));
    cmd.vb = requestVBUpdate | (0x80);
    char t=cmdBuffer[4];
    
    //check if valid type of KB command
    if((t != 'P') && (t != 'T') && (t != 'R') && (t != 'H')) return UNKNOWNCMD;
    
    //if it is assigned to a vb, 
    //allocate original string (max len for mouse is 12)
    //add it to HID task
    if(requestVBUpdate != VB_SINGLESHOT)
    {
      if(strnlen((char*)cmdBuffer,ATCMD_LENGTH) == ATCMD_LENGTH)
      {
        ESP_LOGE(LOG_TAG,"Too long AT cmd (or unterminated");
      } else {
        
        cmd.atoriginal = malloc(strnlen((char*)cmdBuffer,ATCMD_LENGTH)+1);
        if(cmd.atoriginal == NULL) 
        {
          ESP_LOGE(LOG_TAG,"No memory for original AT");
        } else {
          strcpy(cmd.atoriginal,(char*)cmdBuffer);
        }
      }
    }

    char *pch;
    uint8_t cnt = 0;
    uint16_t keycode = 0;
    uint16_t releaseArr[16];
    pch = strpbrk((char *)&cmdBuffer[5]," ");
    while (pch != NULL)
    {
      ESP_LOGD(LOG_TAG,"Token: %s",pch+1);
      
      //check if token seems to be a KEY_*
      if(memcmp(pch+1,"KEY_",4) != 0)
      {
        if(cnt == 0) ESP_LOGW(LOG_TAG,"Not a valid KEY_* identifier");
      } else {
        //parse identifier to keycode
        keycode = parseIdentifierToKeycode(pch+1);

        if(keycode != 0)
        {
          //if found, save...
          ESP_LOGD(LOG_TAG,"Keycode: 0x%4X",keycode);
          
          //because we have KEY identifiers, no deadkey processing is
          //done here.
      
          //KEY_ * identifiers are either a key or a modifier
          //determine which type and save accordingly:
          if(keycode_is_modifier(keycode))
          {
            //we are working with a modifier key here
            cmd.cmd[1] = keycode & 0xFF;
            switch(t)
            {
              case 'H': //KH + KP press on first action
              case 'P': cmd.cmd[0] = 0x25; break;
              case 'R': cmd.cmd[0] = 0x26; break; //KR release on first action
              case 'T': cmd.cmd[0] = 0x27; break; //KT toggle on first action
            }
          } else {
            cmd.cmd[1] = keycode_to_key(keycode);
            switch(t)
            {
              case 'H':
              case 'P': cmd.cmd[0] = 0x21; break;
              case 'R': cmd.cmd[0] = 0x22; break;
              case 'T': cmd.cmd[0] = 0x23; break;
            }
          }

          //send the cmd either directly or save it to the HID task
          if((cmd.vb & 0x7F) == VB_SINGLESHOT) {
            sendHIDCmd(&cmd);
            ESP_LOGI(LOG_TAG,"Sent action 0x%2X, keycode/modifier: 0x%2X",cmd.cmd[0],cmd.cmd[1]);
          } else {
            //if this was the first command in VB mode, we need to
            //remove the pointer to the original string
            //but we don't free it, this is done if the task_hid clears the list
            //in addition, if it was the first command, set the adding to replace any old HID config.
            if(cmd.atoriginal != NULL) 
            {
              task_hid_addCmd(&cmd,1);
              cmd.atoriginal = NULL;
            } else task_hid_addCmd(&cmd,0);
          }
          
          //save for later release
          releaseArr[cnt] = keycode;
          cnt++;
          
          if(cnt == 14) //maximum 6 keycodes + 8 modifier bits
          {
            sendErrorBack("AT KP/KH/KR parameter too long");
            return UNKNOWNCMD;
          }
        } else {
          ESP_LOGW(LOG_TAG,"No keycode found for this token.");
        }
      }
      //split into tokens
      pch = strpbrk(pch+1, " ");
    }
    
    //check if any key identifiers were found
    if(cnt == 0)
    {
      sendErrorBack("No KEY_ identifiers found");
      return UNKNOWNCMD;
    } else {
      //now we need to add the release actions.
      //this is done ONLY for AT KP & AT KH:
      //AT KP releases all keys immediately after pressing them.
      //This is valid in VB mode and via AT commands on serial interface
      //AT KH releases all keys on a VB release trigger. If we
      //got that command via the serial interface, we don't do this (AT KR is needed)
      
      if((t=='P') || ((t=='H')&&(requestVBUpdate != VB_SINGLESHOT)))
      {
        //this has to be a press action too, but at the end
        if(t=='P') cmd.vb = requestVBUpdate | 0x80;
        //we need to have this action on a VB release trigger
        if(t=='H') cmd.vb = requestVBUpdate;
        
        //now either send directly or add to HID task...
        for(uint8_t i = 0; i<cnt; i++)
        {
          //check which command we need (release key or modifier)
          if(keycode_is_modifier(releaseArr[i])) 
          {
            cmd.cmd[0] = 0x26; //release the modifier
            //get key for this one
            cmd.cmd[1] = releaseArr[i] & 0xFF;
          } else {
            cmd.cmd[0] = 0x22; //release the key
            //get key for this one
            cmd.cmd[1] = keycode_to_key(releaseArr[i]);
          }
          
          //send the cmd either directly or save it to the HID task
          if(requestVBUpdate == VB_SINGLESHOT) {
            ESP_LOGI(LOG_TAG,"Sent release action 0x%2X, keycode/modifier: 0x%2X",cmd.cmd[0],cmd.cmd[1]);
            sendHIDCmd(&cmd);
          } else {
            task_hid_addCmd(&cmd,0);
          }
        }
      }
      //reset to singleshot mode
      requestVBUpdate = VB_SINGLESHOT;
      return HID;
    }
  }

  //not consumed, no command found for keyboard
  return UNKNOWNCMD;
}

parserstate_t doMacroParsing(uint8_t *cmdBuffer, int length)
{
  /*++++ AT MA (macro) ++++*/
  if(CMD("AT MA")) {
    vb_cmd_t cmd;
    //clear any previous data
    memset(&cmd,0,sizeof(vb_cmd_t));
    
    //if we have a singleshot, pass command to queue for immediate process
    if(requestVBUpdate == VB_SINGLESHOT)
    {
      fct_macro((char*)&cmdBuffer[6]);
    } else {
      //set VB accordingly
      cmd.vb = requestVBUpdate | 0x80;
      //set action type
      cmd.cmd = T_MACRO;
      //save param string
      cmd.cmdparam = malloc(strnlen((char*)&cmdBuffer[6],ATCMD_LENGTH) + 1);
      if(cmd.cmdparam != NULL)
      {
        strncpy(cmd.cmdparam,(char*)&cmdBuffer[6],ATCMD_LENGTH);
      } else ESP_LOGE(LOG_TAG,"Cannot allocate macro string!");
      //save original AT string
      cmd.atoriginal = malloc(strnlen((char*)&cmdBuffer[6],ATCMD_LENGTH) + 1);
      if(cmd.atoriginal != NULL)
      {
        strncpy(cmd.atoriginal,(char *)cmdBuffer,ATCMD_LENGTH);
      } else ESP_LOGE(LOG_TAG,"Cannot allocate macro string!");
      //save to VB task (replace any existing VB)
      task_vb_addCmd(&cmd,1);
    }
    ESP_LOGD(LOG_TAG,"Saved/triggered macro '%s'",(char*)&cmdBuffer[6]);
    return VB;
  }
  
  //not consumed...
  return UNKNOWNCMD;
}

parserstate_t doMouseParsing(uint8_t *cmdBuffer)
{
  //new hid command
  hid_cmd_t cmd;
  //second command for double click or release action
  hid_cmd_t cmd2;
  
  memset(&cmd,0,sizeof(hid_cmd_t));
  memset(&cmd2,0,sizeof(hid_cmd_t));
  
  //set everything up for updates:
  //set vb with press action & clear any first command byte.
  cmd.vb = requestVBUpdate | (0x80);
  cmd2.vb = requestVBUpdate;
  cmd.cmd[0] = 0;
  cmd2.cmd[0] = 0;
  //get mouse wheel steps
  int steps = 3;
  generalConfig_t *cfg = configGetCurrent();
  if(cfg != NULL) steps = cfg->wheel_stepsize;

  /*++++ mouse clicks ++++*/
  //AT CL, AT CR, AT CM, AT CD
  if(CMD4("AT C"))
  {
    switch(cmdBuffer[4])
    {
      //do single clicks (left,right,middle)
      case 'L': cmd.cmd[0] = 0x13; break;
      case 'R': cmd.cmd[0] = 0x14; break;
      case 'M': cmd.cmd[0] = 0x15; break;
      //do left double click
      case 'D': 
        cmd.cmd[0] = 0x13;
        cmd2.cmd[0] = 0x13;
        break;
      //not an AT C? command for mouse
      default: cmd.cmd[0] = 0;
    }
  }
  
  /*++++ mouse toggle ++++*/
  //AT TL, TR, TM
  if(CMD4("AT T"))
  {
    switch(cmdBuffer[4])
    {
      //do toggles (left,right,middle)
      case 'L': cmd.cmd[0] = 0x1C; break;
      case 'R': cmd.cmd[0] = 0x1D; break;
      case 'M': cmd.cmd[0] = 0x1E; break;
      //not an AT C? command for mouse
      default: cmd.cmd[0] = 0;
    }
  }
  
  /*++++ mouse wheel up/down; set stepsize ++++*/
  if(CMD4("AT W"))
  {
    switch(cmdBuffer[4])
    {
      //move mouse wheel up/down
      case 'U': cmd.cmd[0] = 0x12; cmd.cmd[1] = (int8_t)steps; break;
      case 'D': cmd.cmd[0] = 0x12; cmd.cmd[1] = -((int8_t)steps); break;
      //set mouse wheel stepsize. If unsuccessful, default will return 0
      case 'S':
        steps = strtol((char*)&(cmdBuffer[6]),NULL,10);
        if(steps > 127 || steps < 1) 
        {
          steps = 3;
          ESP_LOGI(LOG_TAG,"Mouse wheel steps out of range, reset to 3");
        }
        if(cfg != NULL) cfg->wheel_stepsize = steps;
        ESP_LOGI(LOG_TAG,"Setting mouse wheel steps: %d",steps);
        return NOACTION;
      default: cmd.cmd[0] = 0;
    }
  }
  
  /*++++ mouse button press ++++*/
  //AT PL, AT PR, AT PM
  if(CMD4("AT P") || CMD4("AT H"))
  {
    switch(cmdBuffer[4])
    {
      case 'L': cmd.cmd[0] = 0x16; cmd2.cmd[0] = 0x19; break;
      case 'R': cmd.cmd[0] = 0x17; cmd2.cmd[0] = 0x1A; break;
      case 'M': cmd.cmd[0] = 0x18; cmd2.cmd[0] = 0x1B; break;
      default: cmd.cmd[0] = 0; cmd2.cmd[0] = 0;
    }
  }
  
  /*++++ mouse button release ++++*/
  //AT RL, AT RR, AT RM
  if(CMD4("AT R"))
  {
    switch(cmdBuffer[4])
    {
      case 'L': cmd.cmd[0] = 0x19; break;
      case 'R': cmd.cmd[0] = 0x1A; break;
      case 'M': cmd.cmd[0] = 0x1B; break;
      default: cmd.cmd[0] = 0; cmd2.cmd[0] = 0;
    }
  }  
  
  /*++++ mouse move ++++*/
  //AT MX MY
  if(CMD4("AT M"))
  {
    int param = strtol((char*)&(cmdBuffer[5]),NULL,10);
    if(param > 127 && param < -127)
    {
      ESP_LOGW(LOG_TAG,"AT M? parameter limit -127 - 127");
      return UNKNOWNCMD;
    } else {
      cmd.cmd[1] = param;
    }
    switch(cmdBuffer[4])
    {
      case 'X': 
        cmd.cmd[0] = 0x10; 
        ESP_LOGI(LOG_TAG,"Mouse move X, %d",cmd.cmd[1]);
        break;;
      case 'Y': 
        cmd.cmd[0] = 0x11;  
        ESP_LOGI(LOG_TAG,"Mouse move Y, %d",cmd.cmd[1]);
        break;
      default: cmd.cmd[0] = 0;
    }
  }

  //if a command is found
  if(cmd.cmd[0] != 0)
  {
    //if we have a singleshot, pass command to queue for immediate process
    if(requestVBUpdate == VB_SINGLESHOT)
    {
      //post values to mouse queue (USB and/or BLE)
      sendHIDCmd(&cmd);
      
      //if we do a double action (double click), send second action also in singleshot
      if(cmd2.cmd[0] == 0x13)
      {
        sendHIDCmd(&cmd2);
      }
    } else {
      //if it is assigned to a vb, 
      //allocate original string (max len for mouse is 12)
      //add it to HID task
      if(strnlen((char*)cmdBuffer,13) == 13)
      {
        ESP_LOGE(LOG_TAG,"Unterminated AT cmd!");
      } else {
        cmd.atoriginal = malloc(strnlen((char*)cmdBuffer,12)+2);
        ESP_LOGD(LOG_TAG,"Alloc %d for atoriginal @0X%08X",strnlen((char*)cmdBuffer,12)+2,(uint32_t)cmd.atoriginal);
        if(cmd.atoriginal == NULL) 
        {
          ESP_LOGE(LOG_TAG,"No memory for original AT");
        } else {
          strcpy(cmd.atoriginal,(char*)cmdBuffer);
        }
      }
      if(task_hid_addCmd(&cmd,1) != ESP_OK)
      {
        ESP_LOGE(LOG_TAG,"Error adding to HID task");
      }
      
      //if we have double action (double click)
      //or press/release action, we have to assign a second HID command.
      if(cmd2.cmd[0] != 0)
      {
        if(task_hid_addCmd(&cmd2,0) != ESP_OK)
        {
          ESP_LOGE(LOG_TAG,"Error adding 2nd to HID task");
        }
      }
      //reset to singleshot mode
      requestVBUpdate = VB_SINGLESHOT;
    }
    return HID;
  //no command found.
  } else { return UNKNOWNCMD; }
}
 
void task_commands(void *params)
{
  uint8_t queuesready = checkqueues();
  int received;
  //used for return values of doXXXParsing functions
  parserstate_t parserstate;
  uint8_t *commandBuffer = NULL;

  while(1)
  {
    if(queuesready)
    {
      //free previously used buffer, only if not null
      if(commandBuffer != NULL) 
      {
        free(commandBuffer);
        commandBuffer = NULL;
      }
      
      //wait for incoming data
      received = halSerialReceiveUSBSerial(&commandBuffer);
      
      
      //if no command received, try again...
      if(received == -1 || commandBuffer == NULL) continue;
      
      ESP_LOGD(LOG_TAG,"received %d: %s",received,commandBuffer);
      
      //special command "AT" without further command:
      //this one is used for serial input (with an additional line ending)
      if(received == 3 && memcmp(commandBuffer,"AT",3) == 0)
      {
        halSerialSendUSBSerial((char*)"OK",2,100);
        continue;
      }
      //this one is used for websocket input (no line ending)
      if(received == 2 && memcmp(commandBuffer,"AT",2) == 0)
      {
        halSerialSendUSBSerial((char*)"OK",2,100);
        continue;
      }

      //do parsing :-)
      //simplified into smaller functions, to make reading easy.
      //
      //the variable requestVBUpdate which is used to determine if an
      //AT command should be triggered as singleshot (== VB_SINGLESHOT)
      //or should be assigned to a VB.
      
      /*++++ Start with parsing which is used for HID task (task_hid) ++++*/
      
      //mouse parsing (start with a defined parser state)
      parserstate = doMouseParsing(commandBuffer);
      //other parsing
      if(parserstate == UNKNOWNCMD) { parserstate = doJoystickParsing(commandBuffer,received); }
      if(parserstate == UNKNOWNCMD) { parserstate = doKeyboardParsing(commandBuffer,received); }
      
      /*++++ Second: parse commands which are used for VB task (task_vb) ++++*/
      
      if(parserstate == UNKNOWNCMD) { parserstate = doGeneralCmdParsing(commandBuffer); }
      if(parserstate == UNKNOWNCMD) { parserstate = doInfraredParsing(commandBuffer); }
      if(parserstate == UNKNOWNCMD) { parserstate = doMouthpieceSettingsParsing(commandBuffer); }
      if(parserstate == UNKNOWNCMD) { parserstate = doMacroParsing(commandBuffer,received); }
      if(parserstate == UNKNOWNCMD) { parserstate = doStorageParsing(commandBuffer); }
      
      ESP_LOGD(LOG_TAG,"Parserstate: %d",parserstate);
      ESP_LOGD(LOG_TAG,"Cfg update: %d",requestUpdate);

      //if a general config update is required
      if(requestUpdate != 0)
      {
        if(configUpdate() != ESP_OK)
        {
          ESP_LOGE(LOG_TAG,"Error updating general config!");
        } else {
          ESP_LOGD(LOG_TAG,"requesting config update");
        }
        requestUpdate = 0;
        continue;
      }
      
      //after all that parsing & config updates
      //we either have a parserstate == 0 here -> no parser found
      //or we consumed this command.
      if(parserstate != UNKNOWNCMD) continue;
      
      //if we are here, no parser was finding commands
      ESP_LOGW(LOG_TAG,"Invalid AT cmd (%d characters), flushing:",received);
      ESP_LOG_BUFFER_CHAR_LEVEL(LOG_TAG,commandBuffer,received, ESP_LOG_WARN);
      //halSerialFlushRX();
      //ESP_LOG_BUFFER_HEXDUMP(LOG_TAG,commandBuffer,received,ESP_LOG_DEBUG);
      halSerialSendUSBSerial((char*)"?",1,100);
    } else {
      //check again for initialized queues
      ESP_LOGE(LOG_TAG,"Queues uninitialized, rechecking in 1s");
      vTaskDelay(1000/portTICK_PERIOD_MS);
      //re-check
      queuesready = checkqueues();
    }
  }
}

/** @brief Print the current slot configurations (general settings + VBs)
 * 
 * This method prints the current slot configurations to the serial
 * interface. Used for "AT LA" and "AT LI" command, which lists all slots.
 * @param printconfig If set != 0, the config is printed. If 0 only slotnames
 * are printed.
 **/
void printAllSlots(uint8_t printconfig)
{
  uint32_t tid;
  uint8_t slotCount = 0;
  
  if(halStorageStartTransaction(&tid, 10,LOG_TAG)!= ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Cannot print slot, unable to obtain storage");
    return;
  }
  if(halStorageGetNumberOfSlots(tid, &slotCount) != ESP_OK)
  {
    halStorageFinishTransaction(tid);
    ESP_LOGE(LOG_TAG,"Cannot get slotcount");
    return;
  }
  
  //in compatibility mode, we initalize a slot here if none are available.
  #ifdef ACTIVATE_V25_COMPAT
  ///@todo Is this the same behaviour? Create a slot if AT LI is called...
  if(slotCount == 0 && printconfig == 1)
  {
    ESP_LOGW(LOG_TAG,"V2.5 Compat: creating default slot 0 (mouse)");
    halStorageCreateDefault(tid);
    if(halStorageGetNumberOfSlots(tid, &slotCount) != ESP_OK)
    {
      halStorageFinishTransaction(tid);
      ESP_LOGE(LOG_TAG,"Cannot get slotcount after creating default");
      return;
    }
  }
  #endif
  //check every slot
  for(uint8_t i = 0; i<slotCount; i++)
  {
    //either print slot name ("AT LI")
    if(printconfig == 0)
    {
      //or the whole config
      halStorageLoadNumber(i,tid,2);
    } else {
      //or the whole config
      halStorageLoadNumber(i,tid,1);
    }
  }
  //send "END" 
  halSerialSendUSBSerial("END",strnlen("END",SLOTNAME_LENGTH),10);
  //finish storage session
  halStorageFinishTransaction(tid);
}

/** @brief Save current config to flash
 * 
 * This method is used to reverse parse each module's setup to be saved in
 * an AT command format. A valid tid is necessary for saving data.
 * 
 * @param slotname Slot name*/
void storeSlot(char* slotname)
{
  uint8_t slotnumber = 0;
  uint32_t tid = 0;
  generalConfig_t *currentcfg = configGetCurrent();
  
  if(halStorageStartTransaction(&tid,10,LOG_TAG) != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Cannot start storage transaction");
    return;
  } else {
    ESP_LOGI(LOG_TAG,"Got ID: %d",tid);
  }
  
  //check if name is already used (returns ESP_OK)
  if(halStorageGetNumberForName(tid, &slotnumber, slotname) != ESP_OK)
  {
    //name not used.
    //get number of currently active slots, and add one for a new slot
    halStorageGetNumberOfSlots(tid, &slotnumber);
    ///@todo Should we increment here or not? Should work -> 1 active slot -> save new config @1
    ESP_LOGI(LOG_TAG,"Save new slot %d under name: %s",slotnumber,slotname);
  } else {
    //name is already used, overwrite
    ESP_LOGI(LOG_TAG,"Overwrite slot %d under name: %s",slotnumber,slotname);
  }
    
  //start a new config by calling with the slotname
  if(halStorageStore(tid,slotname,slotnumber) != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Cannot start new slot");
    halStorageFinishTransaction(tid);
    return;
  }
  //add NL character
  halStorageStore(tid,"\n",0);

  char *outputstring = malloc(ATCMD_LENGTH+1);
  if(outputstring == NULL)
  {
    halStorageFinishTransaction(tid);
    ESP_LOGE(LOG_TAG,"Cannot malloc for outputstring");
    return;
  }
  
  if(currentcfg == NULL)
  {
    free(outputstring);
    halStorageFinishTransaction(tid);
    ESP_LOGE(LOG_TAG,"Error, general config is NULL!!!");
    return;
  }
  
  sprintf(outputstring,"AT AX %d\n",currentcfg->adc.sensitivity_x);
  halStorageStore(tid,outputstring,0);
  sprintf(outputstring,"AT AY %d\n",currentcfg->adc.sensitivity_y);
  halStorageStore(tid,outputstring,0);
  sprintf(outputstring,"AT DX %d\n",currentcfg->adc.deadzone_x);
  halStorageStore(tid,outputstring,0);
  sprintf(outputstring,"AT DY %d\n",currentcfg->adc.deadzone_y);
  halStorageStore(tid,outputstring,0);
  
  sprintf(outputstring,"AT MS %d\n",currentcfg->adc.max_speed);
  halStorageStore(tid,outputstring,0);
  sprintf(outputstring,"AT AC %d\n",currentcfg->adc.acceleration);
  halStorageStore(tid,outputstring,0);
  sprintf(outputstring,"AT TS %d\n",currentcfg->adc.threshold_sip);
  halStorageStore(tid,outputstring,0);
  sprintf(outputstring,"AT TP %d\n",currentcfg->adc.threshold_puff);
  halStorageStore(tid,outputstring,0);
  sprintf(outputstring,"AT WS %d\n",currentcfg->wheel_stepsize);
  halStorageStore(tid,outputstring,0);
  sprintf(outputstring,"AT SP %d\n",currentcfg->adc.threshold_strongpuff);
  halStorageStore(tid,outputstring,0);
  sprintf(outputstring,"AT SS %d\n",currentcfg->adc.threshold_strongsip);
  halStorageStore(tid,outputstring,0);
  
  switch(currentcfg->adc.mode)
  {
    case MOUSE: sprintf(outputstring,"AT MM 1\n"); break;
    case JOYSTICK: sprintf(outputstring,"AT MM 2\n"); break;
    case THRESHOLD: sprintf(outputstring,"AT MM 0\n"); break;
  }
  halStorageStore(tid,outputstring,0);
  
  
  sprintf(outputstring,"AT GU %d\n",currentcfg->adc.gain[0]);
  halStorageStore(tid,outputstring,0);
  sprintf(outputstring,"AT GD %d\n",currentcfg->adc.gain[1]);
  halStorageStore(tid,outputstring,0);
  sprintf(outputstring,"AT GL %d\n",currentcfg->adc.gain[2]);
  halStorageStore(tid,outputstring,0);
  sprintf(outputstring,"AT GR %d\n",currentcfg->adc.gain[3]);
  halStorageStore(tid,outputstring,0);
  sprintf(outputstring,"AT RO %d\n",currentcfg->adc.orientation);
  halStorageStore(tid,outputstring,0);
  
  
  //return: 0 if nothing is active, 1 for USB only, 2 for BLE only, 3 for both
  uint8_t btret = 0;
  if(currentcfg->ble_active != 0) btret+=2;
  if(currentcfg->usb_active != 0) btret+=1;
  sprintf(outputstring,"AT BT %d\n",btret);
  halStorageStore(tid,outputstring,0);
      
  //iterate over all possible VBs.
  for(uint8_t j = 0; j<VB_MAX; j++)
  {
    //print AT BM (button mode) command first
    sprintf(outputstring,"AT BM %02d\n",j);
    ///@todo remove this logging tag
    ESP_LOGD(LOG_TAG,"AT BM %02d",j);
    halStorageStore(tid,outputstring,0);
    //try to parse command either via HID or VB task
    if(task_hid_getAT(outputstring,j) != ESP_OK)
    {
      if(task_vb_getAT(outputstring,j) != ESP_OK)
      {
        //if no command was found, this usually means this one is not used.
        ESP_LOGI(LOG_TAG,"Unused VB, neither HID nor VB task found AT string");
        sprintf(outputstring,"AT NC");
      }
    }
    //store reverse parsed at string
    halStorageStore(tid,outputstring,0);
    halStorageStore(tid,"\n",0);
    ///@todo remove this logging tag
    ESP_LOGD(LOG_TAG,"%s",outputstring);
  }

  //release storage
  free(outputstring);
  halStorageFinishTransaction(tid);
}

/** @brief Init the command parser
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
  //set log level to given log level
  esp_log_level_set(LOG_TAG,LOG_LEVEL_CMDPARSER);
  //create receive task
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
