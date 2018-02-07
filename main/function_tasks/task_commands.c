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

/** Simple macro to short the command comparison (full AT cmd with 5 chars) */
#define CMD(x) (memcmp(cmdBuffer,x,5) == 0)

/** Simple macro to short the command comparison (part of AT cmd with 4 chars) */
#define CMD4(x) (memcmp(cmdBuffer,x,4) == 0)
 
static TaskHandle_t currentCommandTask = NULL;

/** @todo documentate this stuff */
  //function pointer to the task function which will eventuall be called
  //in singleshot mode
  void (*requestVBTask)(void *) = NULL;
  
  // parameter size which needs to be allocated for a VB config update
  size_t requestVBParameterSize = 0;
  
  //pointer to VB parameter struct for VB config udpate
  void *requestVBParameter = NULL;
  
  //reload parameter
  command_type_t requestVBType = T_NOFUNCTION;

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
 * */
typedef enum pstate {NOACTION,WAITFORNEWATCMD,UNKNOWNCMD,TRIGGERTASK} parserstate_t;

/** simple helper function which sends back to the USB host "?"
 * and prints an error on the console with the given extra infos. */
void sendErrorBack(const char* extrainfo)
{
  ESP_LOGE(LOG_TAG,"Error parsing cmd: %s",extrainfo);
  halSerialSendUSBSerial(HAL_SERIAL_TX_TO_CDC,"?\r\n",sizeof("?\r\n"),20);
}

/** @brief Print the current slot configurations (general settings + VBs)
 * 
 * This method prints the current slot configurations to the serial
 * interface. Used for "AT LA" and "AT LI" command, which lists all slots.
 * @param printconfig If set != 0, the config is printed. If 0 only slotnames
 * are printed.
 **/
void printAllSlots(uint8_t printconfig);

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

parserstate_t doMouthpieceSettingsParsing(uint8_t *cmdBuffer, taskNoParameterConfig_t *instance)
{
  generalConfig_t *currentcfg = configGetCurrent();
  
  //clear any previous data
  memset(instance,0,sizeof(taskNoParameterConfig_t));
  instance->virtualButton = requestVBUpdate;
  //set everything up for updates
  requestVBTask = (void (*)(void *))&task_calibration;
  requestVBParameterSize = sizeof(taskNoParameterConfig_t);
  requestVBParameter = instance;
  requestVBType = T_CALIBRATE;
  
  /*++++ calibrate mouthpiece ++++*/
  if(CMD("AT CA"))
  {
    ESP_LOGI(LOG_TAG,"Calibrate");
    return TRIGGERTASK;
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
    //assign to gain value
    switch(cmdBuffer[6])
    {
      case '0': currentcfg->adc.mode = THRESHOLD; requestUpdate = 1; return NOACTION;
      case '1': currentcfg->adc.mode = MOUSE; requestUpdate = 1; return NOACTION;
      case '2': currentcfg->adc.mode = JOYSTICK; requestUpdate = 1; return NOACTION;
      default: sendErrorBack("Mode is 0,1 or 2"); return UNKNOWNCMD;
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
        case 'Y': currentcfg->adc.sensitivity_x = param; requestUpdate = 1; return NOACTION;
        case 'C': currentcfg->adc.acceleration = param; requestUpdate = 1; return NOACTION;
        default: return UNKNOWNCMD;
      }
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
      
      case 'T':
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
    unsigned int param = strtol((char*)&(cmdBuffer[5]),NULL,10);
    ESP_LOGI(LOG_TAG,"Threshold strong %c, %d",cmdBuffer[4],param);
    switch(cmdBuffer[4])
    {
      case 'S':
        if(param > 512)
        {
          sendErrorBack("Threshold strong sip is 0-512");
          return UNKNOWNCMD;
        } else {
          currentcfg->adc.threshold_strongsip = param;
          requestUpdate = 1;
          return NOACTION;
        }
      break;
      
      case 'T':
        if(param < 512 || param > 1023)
        {
          sendErrorBack("Threshold strong puff is 512-1023");
          return UNKNOWNCMD;
        } else {
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

parserstate_t doStorageParsing(uint8_t *cmdBuffer, taskConfigSwitcherConfig_t *instance)
{
  char slotname[SLOTNAME_LENGTH];
  uint32_t tid = 0;
  uint8_t slotnumber = 0;
  generalConfig_t * currentcfg = configGetCurrent();
  //clear any previous data
  memset(instance,0,sizeof(taskConfigSwitcherConfig_t));
  
  //set everything up for updates
  instance->virtualButton = requestVBUpdate;
  requestVBTask = (void (*)(void *))&task_configswitcher;
  requestVBParameterSize = sizeof(taskConfigSwitcherConfig_t);
  requestVBParameter = instance;
  requestVBType = T_CONFIGCHANGE;
  
  /*++++ save slot ++++*/
  if(CMD("AT SA"))
  {
    if(halStorageStartTransaction(&tid,10) != ESP_OK)
    {
      ESP_LOGE(LOG_TAG,"Cannot start storage transaction");
      return UNKNOWNCMD;
    }
    char *pch;
    //find end of slotname
    pch = strpbrk((char *)&cmdBuffer[6],"\r\n");
    //set 0 terminator
    *pch = '\0';
    strcpy(slotname,(char*)&cmdBuffer[6]);
    ESP_LOGI(LOG_TAG,"Save slot under name: %s",slotname);
    halStorageGetNumberOfSlots(tid, &slotnumber);
    //store general config
    if(halStorageStore(tid,currentcfg,slotname,slotnumber+1) != ESP_OK)
    {
      ESP_LOGE(LOG_TAG,"Cannot store general cfg");
      halStorageFinishTransaction(tid);
      return UNKNOWNCMD;
    }
    //store each virtual button
    for(uint8_t i = 0; i<(NUMBER_VIRTUALBUTTONS*4); i++)
    {
      size_t sizevb = 0;
      
      switch(currentcfg->virtualButtonCommand[i])
      {
        case T_MOUSE: sizevb = sizeof(taskMouseConfig_t); break;
        case T_KEYBOARD: sizevb = sizeof(taskKeyboardConfig_t); break;
        case T_CONFIGCHANGE: sizevb = sizeof(taskConfigSwitcherConfig_t); break;
        case T_CALIBRATE: sizevb = sizeof(taskNoParameterConfig_t); break;
        //case T_SENDIR: sizevb = sizeof(taskKeyboardConfig_t); break;
        /** @todo Activate T_SENDIR here when IR task is finished */
        case T_NOFUNCTION: sizevb = sizeof(taskNoParameterConfig_t); break;
        default: break;
      }
      if(halStorageStoreSetVBConfigs(slotnumber+1,i,currentcfg->virtualButtonConfig[i], \
        sizevb,tid) != ESP_OK)
      {
        ESP_LOGE(LOG_TAG,"Cannot store VB %d",i);
        halStorageFinishTransaction(tid);
        return UNKNOWNCMD;
      }
    }
    halStorageFinishTransaction(tid);
    return NOACTION;
  }
  
  /*++++ AT NE (load next) +++*/
  if(CMD("AT NE"))
  {
    //save to config
    instance->virtualButton = requestVBUpdate;
    strcpy(instance->slotName,"__NEXT");
    ESP_LOGI(LOG_TAG,"Load next, by parameter: %s",instance->slotName);
    //we want to have the config sent to the task
    return TRIGGERTASK;
  }
  
  /*++++ AT LO (load) ++++*/
  if(CMD("AT LO"))
  {
    //find end of slotname
    char *pch = strpbrk((char *)&cmdBuffer[6],"\r\n");
    //set 0 terminator
    *pch = '\0';
    //save to config
    instance->virtualButton = requestVBUpdate;
    strcpy(instance->slotName,(char*)&cmdBuffer[6]);
    ESP_LOGI(LOG_TAG,"Load slot name: %s",instance->slotName);
    //we want to have the config sent to the task
    return TRIGGERTASK;
  }
  //not consumed, no command found for storage
  return UNKNOWNCMD;
}

parserstate_t doInfraredParsing(uint8_t *cmdBuffer)
{
  
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
  
    
  /*++++ AT RA ++++*/
  if(CMD("AT RA")) {
    //reset the reports (keyboard only, excepting all other parts)
    halBLEReset(0xFE);
    halSerialReset(0xFE);
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
    uint32_t sent = halSerialSendUSBSerial(HAL_SERIAL_TX_TO_CDC,IDSTRING,sizeof(IDSTRING),20);
    if(sent != sizeof(IDSTRING)) 
    {
      ESP_LOGE(LOG_TAG,"Error sending response of AT ID");
      return UNKNOWNCMD;
    } else return NOACTION;
  }
  
  /*++++ AT DE ++++*/
  if(CMD("AT DE")) {
    uint32_t tid;
    if(halStorageStartTransaction(&tid,20) != ESP_OK)
    {
      return UNKNOWNCMD;
    } else {
      if(halStorageDeleteSlot(0,tid) != ESP_OK)
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
      //return 3 to definetly trigger nothing else
      return WAITFORNEWATCMD;
    }
  }
  
  /*++++ AT DL ++++*/
  if(CMD4("AT DL")) {
    uint32_t tid;
    uint8_t slotnumber;
    char *pch;
    
    if(halStorageStartTransaction(&tid,20) != ESP_OK)
    {
      return UNKNOWNCMD;
    }
    
    switch(cmdBuffer[4])
    {
      //delete by given number
      case 'L': slotnumber = strtol((char*)&(cmdBuffer[6]),NULL,10); break;
      //delete by given name
      case 'N': 
        //find end of slotname
        pch = strpbrk((char *)&cmdBuffer[6],"\r\n");
        //set 0 terminator
        *pch = '\0';
        halStorageGetNumberForName(tid,&slotnumber,(char *)&cmdBuffer[6]); 
        break;
      default: return UNKNOWNCMD;
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
    param = strtol((char*)&(cmdBuffer[6]),NULL,10);
    switch(param)
    {
      case 1: 
        currentcfg->ble_active = 0;
        currentcfg->usb_active = 1;
        requestUpdate = 1;
        return NOACTION;
        break;
      case 2: 
        currentcfg->ble_active = 1;
        currentcfg->usb_active = 0;
        requestUpdate = 1;
        return NOACTION;
        break;
      case 3: 
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

parserstate_t doKeyboardParsing(uint8_t *cmdBuffer, taskKeyboardConfig_t *kbdinstance, int length)
{
  //clear any previous data
  memset(kbdinstance,0,sizeof(taskKeyboardConfig_t));
  
  //set everything up for updates
  requestVBTask = (void (*)(void *))&task_keyboard;
  requestVBParameterSize = sizeof(taskKeyboardConfig_t);
  requestVBParameter = kbdinstance;
  requestVBType = T_KEYBOARD;
  
  //use global virtual button (normally VB_SINGLESHOT,
  //can be changed by "AT BM"
  kbdinstance->virtualButton = requestVBUpdate;
  int offset = 0;
  int offsetOut = 0;
  int offsetEnd = TASK_KEYBOARD_PARAMETERLENGTH-1;
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
    //set keyboard to write
    kbdinstance->type = WRITE;
    //remove \r & \n
    //cmdBuffer[length-2] = 0;
    //cmdBuffer[length-1] = 0;
    
    //save each byte from KW string to keyboard instance
    //offset must be less then buffer length - 6 bytes for "AT KW "
    while(offset <= (length - 5))
    {
      
      
      //terminate...
      if(cmdBuffer[offset+6] == '\r' || cmdBuffer[offset+6] == '\n' || \
        cmdBuffer[offset+6] == 0 || offsetOut == offsetEnd) break;
      
      //save original byte
      kbdinstance->keycodes_text[offsetEnd] = cmdBuffer[offset+6];
      offsetEnd--;
      
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
          kbdinstance->keycodes_text[offsetOut] = deadkeyfirst;
          offsetOut++;
          ESP_LOGD(LOG_TAG, "Deadkey 0x%X@%d",deadkeyfirst,offsetOut);
        }
        
        //save keycode + modifier
        kbdinstance->keycodes_text[offsetOut] = keycode | (modifier << 8);
        ESP_LOGD(LOG_TAG, "Keycode 0x%X@%d, modifier: 0x%X",keycode,offsetOut,modifier);
        offsetOut++;
      } else {
        ESP_LOGD(LOG_TAG, "Need another byte (unicode)");
      }
      offset++;
      
      if(offset == (TASK_KEYBOARD_PARAMETERLENGTH - 1))
      {
        sendErrorBack("AT KW parameter too long");
        return UNKNOWNCMD;
      }
    }
    //terminate keycode array with 0
    ESP_LOGD(LOG_TAG,"Terminating @%d with 0",offsetOut);
    kbdinstance->keycodes_text[offsetOut] = 0;
    return TRIGGERTASK;
  }
  
  /*++++ AT KP + KH + KR ++++*/
  // (all commands with key identifiers)
  if(CMD4("AT K")) {
    //set keyboard instance accordingly
    switch(cmdBuffer[4])
    {
      //KP: press with VB press flag, release with VB release flag
      case 'P': kbdinstance->type = PRESS_RELEASE_BUTTON; break;
      //KR: just release
      case 'R': kbdinstance->type = RELEASE; break;
      //KH: just press (hold)
      case 'H': kbdinstance->type = PRESS; break;
      default: return UNKNOWNCMD; break;
    }
    
    //remove \r & \n
    cmdBuffer[length-2] = ' ';
    cmdBuffer[length-1] = ' ';

    char *pch;
    uint8_t cnt = 0;
    uint16_t keycode = 0;
    pch = strpbrk((char *)&cmdBuffer[5]," ");
    while (pch != NULL)
    {
      ESP_LOGD(LOG_TAG,"Token: %s",pch+1);
      
      //check if token seems to be a KEY_*
      if(memcmp(pch+1,"KEY_",4) != 0)
      {
        ESP_LOGW(LOG_TAG,"Not a valid KEY_* identifier");
      } else {
        //parse identifier to keycode
        keycode = parseIdentifierToKeycode(pch+1);

        if(keycode != 0)
        {
          //if found, save...
          ESP_LOGD(LOG_TAG,"Keycode: %d",keycode);
          
          //get deadkeys
          uint16_t deadk = 0;
          deadk = deadkey_to_keycode(keycode, currentcfg->locale);
          //found a deadkey
          if(deadk != 0)
          {
            //save deadkey's keycode & modifier
            kbdinstance->keycodes_text[cnt] = keycode_to_key(deadk) | \
              (keycode_to_modifier(deadk, currentcfg->locale) << 8);
            ESP_LOGI(LOG_TAG,"Deadkey (keycode + modifier): %4X @%d",kbdinstance->keycodes_text[cnt], cnt);
            cnt++;
          }
          
          //KEY_ * identifiers are either a key or a modifier
          //determine which type and save accordingly:
          if(keycode_is_modifier(keycode))
          {
            kbdinstance->keycodes_text[cnt] = keycode_to_key(keycode) << 8;
          } else {
            kbdinstance->keycodes_text[cnt] = keycode_to_key(keycode);
          }
          ESP_LOGI(LOG_TAG,"Keycode + modifier: %4X @ %d",kbdinstance->keycodes_text[cnt], cnt);
          cnt++;
          if(cnt == (TASK_KEYBOARD_PARAMETERLENGTH - 1))
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
      //terminate keycode array with 0
      ESP_LOGD(LOG_TAG,"Terminating @%d with 0",cnt);
      kbdinstance->keycodes_text[cnt] = 0;
      return TRIGGERTASK;
    }
  }
  
  //not consumed, no command found for keyboard
  return UNKNOWNCMD;
}

parserstate_t doMouseParsing(uint8_t *cmdBuffer, taskMouseConfig_t *mouseinstance)
{
  //clear any previous data
  memset(mouseinstance,0,sizeof(taskMouseConfig_t));
  
  //set everything up for updates
  requestVBTask = (void (*)(void *))&task_mouse;
  requestVBParameterSize = sizeof(taskMouseConfig_t);
  requestVBParameter = mouseinstance;
  requestVBType = T_MOUSE;
  
  //use global virtual button (normally VB_SINGLESHOT,
  //can be changed by "AT BM"
  mouseinstance->virtualButton = requestVBUpdate;
  int steps = 3;
  
  /*++++ mouse clicks ++++*/
  //AT CL, AT CR, AT CM, AT CD
  if(CMD4("AT C"))
  {
    mouseinstance->actionparam = M_CLICK;
    switch(cmdBuffer[4])
    {
      //do single clicks (left,right,middle)
      case 'L': mouseinstance->type = LEFT; return TRIGGERTASK;
      case 'R': mouseinstance->type = RIGHT; return TRIGGERTASK;
      case 'M': mouseinstance->type = MIDDLE; return TRIGGERTASK;
      //do left double click
      case 'D': 
        mouseinstance->type = LEFT;
        mouseinstance->actionparam = M_DOUBLE; 
        return TRIGGERTASK;
      //not an AT C? command for mouse, return 0 (not consumed)
      default: return UNKNOWNCMD;
    }
  }
  
  /*++++ mouse wheel up/down; set stepsize ++++*/
  if(CMD4("AT W"))
  {
    mouseinstance->type = WHEEL;
    mouseinstance->actionparam = M_UNUSED;
    switch(cmdBuffer[4])
    {
      //move mouse wheel up/down
      case 'U': mouseinstance->actionvalue = mouse_get_wheel(); return TRIGGERTASK;
      case 'D': mouseinstance->actionvalue = -mouse_get_wheel(); return TRIGGERTASK;
      //set mouse wheel stepsize. If unsuccessful, default will return 0
      case 'S':
        steps = strtol((char*)&(cmdBuffer[6]),NULL,10);
        if(steps > 127 || steps < 127)
        {
          sendErrorBack("Wheel size out of range! (-127 to 127)");
          return UNKNOWNCMD;
        } else { 
          mouse_set_wheel(steps);
          ESP_LOGI(LOG_TAG,"Setting mouse wheel steps: %d",steps);
          return NOACTION;
        }
      default: return UNKNOWNCMD;
    }
  }
  
  /*++++ mouse button press ++++*/
  //AT PL, AT PR, AT PM
  if(CMD4("AT P"))
  {
    mouseinstance->actionparam = M_HOLD;
    switch(cmdBuffer[4])
    {
      case 'L': mouseinstance->type = LEFT; return TRIGGERTASK;
      case 'R': mouseinstance->type = RIGHT; return TRIGGERTASK;
      case 'M': mouseinstance->type = MIDDLE; return TRIGGERTASK;
      default: return UNKNOWNCMD;
    }
  }
  
  /*++++ mouse button release ++++*/
  //AT RL, AT RR, AT RM
  if(CMD4("AT R"))
  {
    mouseinstance->actionparam = M_RELEASE;
    switch(cmdBuffer[4])
    {
      case 'L': mouseinstance->type = LEFT; return TRIGGERTASK;
      case 'R': mouseinstance->type = RIGHT; return TRIGGERTASK;
      case 'M': mouseinstance->type = MIDDLE; return TRIGGERTASK;
      default: return UNKNOWNCMD;
    }
  }  
  
  /*++++ mouse move ++++*/
  //AT RL, AT RR, AT RM
  if(CMD4("AT M"))
  {
    mouseinstance->actionparam = M_UNUSED;
    //mouseinstance->actionvalue = atoi((char*)&(cmdBuffer[6]));
    //mouseinstance->actionvalue = strtoimax((char*)&(cmdBuffer[6]),&endBuf,10);
    int param = strtol((char*)&(cmdBuffer[5]),NULL,10);
    ESP_LOGI(LOG_TAG,"Mouse move %c, %d",cmdBuffer[4],mouseinstance->actionvalue);
    if(param > 127 && param < -127)
    {
      ESP_LOGW(LOG_TAG,"AT MX parameter limit -127 - 127");
      return UNKNOWNCMD;
    } else {
      mouseinstance->actionvalue = param;
    }
    switch(cmdBuffer[4])
    {
      case 'X': mouseinstance->type = X; return TRIGGERTASK;
      case 'Y': mouseinstance->type = Y; return TRIGGERTASK;
      default: return UNKNOWNCMD;
    }
  }

  //no mouse command found
  return UNKNOWNCMD;
}
 
void task_commands(void *params)
{
  uint8_t queuesready = checkqueues();
  int received;
  //used for return values of doXXXParsing functions
  parserstate_t parserstate;
  uint8_t commandBuffer[ATCMD_LENGTH];

  //parameters for different tasks
  taskMouseConfig_t *cmdMouse = malloc(sizeof(taskMouseConfig_t));
  taskKeyboardConfig_t *cmdKeyboard = malloc(sizeof(taskKeyboardConfig_t));
  taskNoParameterConfig_t *cmdNoConfig = malloc(sizeof(taskNoParameterConfig_t));
  taskConfigSwitcherConfig_t *cmdCfgSwitcher = malloc(sizeof(taskConfigSwitcherConfig_t));
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
      //vTaskDelay(30/portTICK_PERIOD_MS);
      
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
      
      //the variable requestVBUpdate which is used to determine if an
      //AT command should be triggered as singleshot (== VB_SINGLESHOT)
      //or should be assigned to a VB.
      
      //mouse parsing
      parserstate = doMouseParsing(commandBuffer,cmdMouse);
      //storage parsing
      if(parserstate == UNKNOWNCMD) parserstate = doStorageParsing(commandBuffer,cmdCfgSwitcher);
      if(parserstate == UNKNOWNCMD) parserstate = doKeyboardParsing(commandBuffer,cmdKeyboard,received);
      if(parserstate == UNKNOWNCMD) parserstate = doGeneralCmdParsing(commandBuffer);
      if(parserstate == UNKNOWNCMD) parserstate = doInfraredParsing(commandBuffer);
      if(parserstate == UNKNOWNCMD) parserstate = doMouthpieceSettingsParsing(commandBuffer,cmdNoConfig);
      
      switch(parserstate)
      {
        case UNKNOWNCMD:
          break;
        case NOACTION:
          //not in singleshot, but parsing parts didn't request anything?!?
          //maybe wrong combination of AT BM and another AT command
          if(requestVBUpdate != VB_SINGLESHOT)
          {
            ESP_LOGE(LOG_TAG,"Error assigning this AT command to a button!");
            requestVBUpdate = VB_SINGLESHOT;
            continue;
          } else {
            continue;
          }
          break;
        //the AT command tells us to trigger a task or save an instance to a VB
        case TRIGGERTASK:
          if(requestVBUpdate == VB_SINGLESHOT)
          {
            //just trigger as singleshot
            if(requestVBTask != NULL && requestVBParameter != NULL)
            {
              //call in singleshot mode
              requestVBTask(requestVBParameter);
              //reset the VB task callback
              requestVBTask = NULL;
              continue;
            } else {
              ESP_LOGE(LOG_TAG,"Should trigger a singleshot, but task or params are NULL");
            }
          } else {
            //attach to VB
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
            } else {
              ESP_LOGI(LOG_TAG,"VB %d updated in config!",requestVBUpdate);
              ESP_LOG_BUFFER_HEXDUMP(LOG_TAG,vbconfigparam,requestVBParameterSize,ESP_LOG_DEBUG);
            }
            //reset to singleshot mode
            requestVBUpdate = VB_SINGLESHOT;
            continue;
          }
          break;
        case WAITFORNEWATCMD:
          break;
        default: break;
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
      if(parserstate != UNKNOWNCMD) continue;
      
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

/** @brief Print the current slot configurations (general settings + VBs)
 * 
 * This method prints the current slot configurations to the serial
 * interface. Used for "AT LA" and "AT LI" command, which lists all slots.
 * @param printconfig If set != 0, the config is printed. If 0 only slotnames
 * are printed.
 **/
void printAllSlots(uint8_t printconfig)
{
  uint8_t slotCount = 0;
  uint32_t tid = 0;
  //contains slotname + "Slot%d:<slotname>\r\n"
  char slotName[SLOTNAME_LENGTH+1];
  char outputstring[SLOTNAME_LENGTH+11];
  char parameterNumber[16];
  generalConfig_t *currentcfg = configGetCurrent();
  
  if(currentcfg == NULL)
  {
    ESP_LOGE(LOG_TAG,"Error, general config is NULL!!!");
    return;
  }
  
  if(halStorageStartTransaction(&tid, 10)!= ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Cannot print slot, unable to obtain storage");
    return;
  }
  
  if(halStorageGetNumberOfSlots(tid, &slotCount) != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Cannot get slotcount");
    return;
  }
  
  //check every slot
  for(uint8_t i = 1; i<=slotCount; i++)
  {
    if(halStorageGetNameForNumber(tid,i,slotName) != ESP_OK)
    {
      ESP_LOGE(LOG_TAG,"cannot get slotname");
      continue;
    }
    
    //print slot name (different for AT LA and AT LI :-( )
    if(printconfig == 0)
    {
      sprintf(outputstring,"Slot%d:%s\r\n",i,slotName);
      halSerialSendUSBSerial(HAL_SERIAL_TX_TO_CDC,outputstring,strlen(outputstring),10);
    } else {
      sprintf(outputstring,"Slot:%s\r\n",slotName);
      halSerialSendUSBSerial(HAL_SERIAL_TX_TO_CDC,outputstring,strlen(outputstring),10);
    }
    
    //only print slot config if requested 
    if(printconfig != 0)
    {
      sprintf(parameterNumber,"AT AX %d\r\n",currentcfg->adc.sensitivity_x);
      halSerialSendUSBSerial(HAL_SERIAL_TX_TO_CDC,parameterNumber,strlen(parameterNumber),10);
      sprintf(parameterNumber,"AT AY %d\r\n",currentcfg->adc.sensitivity_y);
      halSerialSendUSBSerial(HAL_SERIAL_TX_TO_CDC,parameterNumber,strlen(parameterNumber),10);
      sprintf(parameterNumber,"AT DX %d\r\n",currentcfg->adc.deadzone_x);
      halSerialSendUSBSerial(HAL_SERIAL_TX_TO_CDC,parameterNumber,strlen(parameterNumber),10);
      sprintf(parameterNumber,"AT DY %d\r\n",currentcfg->adc.deadzone_y);
      halSerialSendUSBSerial(HAL_SERIAL_TX_TO_CDC,parameterNumber,strlen(parameterNumber),10);
      
      sprintf(parameterNumber,"AT MS %d\r\n",currentcfg->adc.max_speed);
      halSerialSendUSBSerial(HAL_SERIAL_TX_TO_CDC,parameterNumber,strlen(parameterNumber),10);
      sprintf(parameterNumber,"AT AC %d\r\n",currentcfg->adc.acceleration);
      halSerialSendUSBSerial(HAL_SERIAL_TX_TO_CDC,parameterNumber,strlen(parameterNumber),10);
      sprintf(parameterNumber,"AT TS %d\r\n",currentcfg->adc.threshold_sip);
      halSerialSendUSBSerial(HAL_SERIAL_TX_TO_CDC,parameterNumber,strlen(parameterNumber),10);
      sprintf(parameterNumber,"AT TP %d\r\n",currentcfg->adc.threshold_puff);
      halSerialSendUSBSerial(HAL_SERIAL_TX_TO_CDC,parameterNumber,strlen(parameterNumber),10);
      sprintf(parameterNumber,"AT WS %d\r\n",currentcfg->wheel_stepsize);
      halSerialSendUSBSerial(HAL_SERIAL_TX_TO_CDC,parameterNumber,strlen(parameterNumber),10);
      sprintf(parameterNumber,"AT SP %d\r\n",currentcfg->adc.threshold_strongpuff);
      halSerialSendUSBSerial(HAL_SERIAL_TX_TO_CDC,parameterNumber,strlen(parameterNumber),10);
      sprintf(parameterNumber,"AT SS %d\r\n",currentcfg->adc.threshold_strongsip);
      halSerialSendUSBSerial(HAL_SERIAL_TX_TO_CDC,parameterNumber,strlen(parameterNumber),10);
      
      switch(currentcfg->adc.mode)
      {
        case MOUSE: sprintf(parameterNumber,"AT MM 1\r\n"); break;
        case JOYSTICK: sprintf(parameterNumber,"AT MM 2\r\n"); break;
        case THRESHOLD: sprintf(parameterNumber,"AT MM 0\r\n"); break;
      }
      halSerialSendUSBSerial(HAL_SERIAL_TX_TO_CDC,parameterNumber,strlen(parameterNumber),10);
      
      
      sprintf(parameterNumber,"AT GU %d\r\n",currentcfg->adc.gain[0]);
      halSerialSendUSBSerial(HAL_SERIAL_TX_TO_CDC,parameterNumber,strlen(parameterNumber),10);
      sprintf(parameterNumber,"AT GD %d\r\n",currentcfg->adc.gain[1]);
      halSerialSendUSBSerial(HAL_SERIAL_TX_TO_CDC,parameterNumber,strlen(parameterNumber),10);
      sprintf(parameterNumber,"AT GL %d\r\n",currentcfg->adc.gain[2]);
      halSerialSendUSBSerial(HAL_SERIAL_TX_TO_CDC,parameterNumber,strlen(parameterNumber),10);
      sprintf(parameterNumber,"AT GR %d\r\n",currentcfg->adc.gain[3]);
      halSerialSendUSBSerial(HAL_SERIAL_TX_TO_CDC,parameterNumber,strlen(parameterNumber),10);
      sprintf(parameterNumber,"AT RO %d\r\n",currentcfg->orientation);
      halSerialSendUSBSerial(HAL_SERIAL_TX_TO_CDC,parameterNumber,strlen(parameterNumber),10);
      
      
      //return: 0 if nothing is active, 1 for USB only, 2 for BLE only, 3 for both
      uint8_t btret = 0;
      if(currentcfg->ble_active != 0) btret+=2;
      if(currentcfg->usb_active != 0) btret+=1;
      sprintf(parameterNumber,"AT BT %d\r\n",btret);
      halSerialSendUSBSerial(HAL_SERIAL_TX_TO_CDC,parameterNumber,strlen(parameterNumber),10);
      
      for(uint8_t j = 0; j<(NUMBER_VIRTUALBUTTONS*4); j++)
      {
        sprintf(outputstring,"AT BM %02d\r\n",j);
        ESP_LOGD(LOG_TAG,"AT BM %d",j);
        halSerialSendUSBSerial(HAL_SERIAL_TX_TO_CDC,outputstring,strlen(outputstring),10);
        switch(currentcfg->virtualButtonCommand[j])
        {
          case T_MOUSE:
            task_mouse_getAT(outputstring,currentcfg->virtualButtonConfig[j]);
            halSerialSendUSBSerial(HAL_SERIAL_TX_TO_CDC,outputstring,strlen(outputstring),10);
            break;
          case T_KEYBOARD:
            task_keyboard_getAT(outputstring,currentcfg->virtualButtonConfig[j]);
            halSerialSendUSBSerial(HAL_SERIAL_TX_TO_CDC,outputstring,strlen(outputstring),10);
            break;
          case T_CONFIGCHANGE:
            task_configswitcher_getAT(outputstring,currentcfg->virtualButtonConfig[j]);
            halSerialSendUSBSerial(HAL_SERIAL_TX_TO_CDC,outputstring,strlen(outputstring),10);
            break;
          case T_CALIBRATE:
            //no reverse parsing, no parameter...
            sprintf(outputstring,"AT CA\r\n");
            halSerialSendUSBSerial(HAL_SERIAL_TX_TO_CDC,outputstring,strlen(outputstring),10);
            break;
          case T_SENDIR:
            ///@todo Ad reverse parser for IR
            break;
          case T_NOFUNCTION:
          default:
            sprintf(outputstring,"AT NC\r\n");
            halSerialSendUSBSerial(HAL_SERIAL_TX_TO_CDC,outputstring,strlen(outputstring),10);
            break;
        }
        ESP_LOGD(LOG_TAG,"%s",outputstring);
      }
    }
  }
  
  //release storage
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
