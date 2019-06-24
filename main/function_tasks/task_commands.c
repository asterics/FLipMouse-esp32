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
 * Copyright 2019 Benjamin Aigner <aignerb@technikum-wien.at,
 * beni@asterics-foundation.org>
*/
 /** 
 * @file
 * @brief CONTINOUS TASK - Main command parser for serial AT commands.
 * 
 * This module is used to parse any incoming serial data from hal_serial
 * for valid AT commands.
 * If a valid command is detected, the corresponding action is triggered
 * (mostly by invoking task_hid or task_vb via a hid_cmd_t or vb_cmd_t).
 * 
 * By issueing an <b>AT BM</b> command, the next issued AT command
 * will be assigned to a virtual button. This is done via setting
 * the requestVBUpdate variable to the VB number. One time only commands
 * (without AT BM) are defined as VB==VB_SINGLESHOT
 * 
 * @see VB_SINGLESHOT
 * @see hal_serial
 * @see atcmd_api
 * @see hid_cmd_t
 * @see vb_cmd_t
 * 
 * @note Currently we are using unicode_to_keycode, because terminal programs use it this way.
 * For supporting unicode keystreams (UTF-8), the method parse_for_keycode needs to be used
 * 
 * @note The command parser itself is based on the cmd_parser project. See: <addlinkhere>
 * */
#include "task_commands.h"

/** Tag for ESP_LOG logging */
#define LOG_TAG "cmdparser"
/** @brief Set a global log limit for this file */
#define LOG_LEVEL_CMDPARSER ESP_LOG_INFO

/** @brief Global HID command for joystick
 * This command is set by the command handlers.
 * If one field was modified by a handler, it
 * will be sent to task_hid */
hid_cmd_t joystick;

/** @brief Global release HID command for joystick
 * This command is set by the command handlers.
 * If one field was modified by a handler, it
 * will be sent to task_hid */
hid_cmd_t joystickR;

/** @brief Global HID command for mouse
 * This command is set by the command handlers.
 * If one field was modified by a handler, it
 * will be sent to task_hid */
hid_cmd_t mouse;

/** @brief Global command for general data transmitted via the HID (I2C) interface.
 * This command is set by the command handlers.
 * If one field was modified by a handler, it
 * will be sent to the USB chip */
hid_cmd_t general;

/** @brief Global release HID command for mouse
 * This command is set by the command handlers.
 * If one field was modified by a handler, it
 * will be sent to task_hid */
hid_cmd_t mouseR;

/** @brief Global HID command for mouse - 2nd action
 * This command is set by the command handlers.
 * This one is just used for double click.
 * If one field was modified by a handler, it
 * will be sent to task_hid */
hid_cmd_t mouseD;

/** @brief Global HID command for keyboard
 * This command is set by the command handlers.
 * If one field was modified by a handler, it
 * will be sent to task_hid */
hid_cmd_t keyboard;

/** @brief Global release HID command for keyboard
 * This command is set by the command handlers.
 * If one field was modified by a handler, it
 * will be sent to task_hid */
hid_cmd_t keyboardR;

/** @brief Global VB command */
vb_cmd_t vbaction;

/** @brief Pointer to current configuration
 * @warning Must be set before calling the parser, will be modified there!
 */
generalConfig_t *currentCfg = NULL;
 
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
 * */
uint8_t requestVBUpdate = VB_SINGLESHOT;

/** @brief Currently used virtual button number.
 * 
 * If this variable is set to a value != 0, we know we should
 * wait to reset requestVBUpdate to VB_SINGLESHOT until next command.
 * @see VB_SINGLESHOT
 * @see requestVBUpdate
 * */
uint8_t requestBM = 0;

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

/** @brief Helper to route a HID cmd either directly to queue or add it to the list
 * @param sendCmd Hid command */
void sendHIDCmd(hid_cmd_t *sendCmd, uint8_t vb, uint8_t* atorig, uint8_t replace)
{
  //check if this cmd contains data
  hid_cmd_t emptyhid; memset(&emptyhid,0,sizeof(hid_cmd_t));
  if(sendCmd == NULL) return;
  if(memcmp(sendCmd,&emptyhid,sizeof(hid_cmd_t)) == 0) return;
  
  //send it directly, if singleshot is active
  if(requestVBUpdate == VB_SINGLESHOT)
  {
    //post values to mouse queue (USB and/or BLE)
    if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_USB)
    { xQueueSend(hid_usb,sendCmd,0); }
    
    if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_BLE)
    { xQueueSend(hid_ble,sendCmd,0); }
    
    if(sendCmd->atoriginal != NULL) free(sendCmd->atoriginal);
  } else {
    //update HID command (set VB, add original string)
    sendCmd->vb = vb;
    if(atorig != NULL)
    {
      sendCmd->atoriginal = strdup((char*)atorig);
      if(sendCmd->atoriginal == NULL)
      {
        ESP_LOGE(LOG_TAG,"Error allocating AT cmd string");
      }
    } else {
      sendCmd->atoriginal = NULL;
    }
    //add to HID cmd, remove from VB cmd
    handler_vb_delCmd(sendCmd->vb);
    handler_hid_addCmd(sendCmd,replace);
  }
}
/** @brief Helper to route a VB cmd either directly to queue or add it to the list
 * @param sendCmd Hid command */
void sendVBCmd(vb_cmd_t *sendCmd, uint8_t vb, uint8_t* atorig, uint8_t replace)
{
  //check if this cmd contains data
  vb_cmd_t emptyvb; memset(&emptyvb,0,sizeof(vb_cmd_t));
  if(sendCmd == NULL) return;
  if(memcmp(sendCmd,&emptyvb,sizeof(vb_cmd_t)) == 0) return;
  
  //send it directly, if singleshot is active
  if(requestVBUpdate != VB_SINGLESHOT)
  {
    //update HID command (set VB, add original string)
    sendCmd->vb = vb;
    if(atorig != NULL)
    {
      sendCmd->atoriginal = strdup((char*)atorig);
      if(sendCmd->atoriginal == NULL)
      {
        ESP_LOGE(LOG_TAG,"Error allocating AT cmd string");
      }
    } else {
      sendCmd->atoriginal = NULL;
    }
    //add to HID cmd, remove from VB cmd
    handler_hid_delCmd(sendCmd->vb);
    handler_vb_addCmd(sendCmd,replace);
  }
}

/*++++ command handlers (implemented before commands[] ++++*/
esp_err_t cmdId(char* orig, void* p1, void* p2) {
  halSerialSendUSBSerial((char*)IDSTRING,sizeof(IDSTRING),20);
  return ESP_OK;
}
esp_err_t cmdBm(char* orig, void* p1, void* p2) {
  requestVBUpdate = (int32_t)p1;
  //signal: we got a new VB, 
  //do not reset it to VB_SINGLESHOT this time
  requestBM = 1;
  return ESP_OK;
}
esp_err_t cmdMa(char* orig, void* p1, void* p2) {
  if(requestVBUpdate == VB_SINGLESHOT)
  {
    fct_macro((char*)p1);
  } else {
    vbaction.cmd = T_MACRO;
    vbaction.cmdparam = malloc(strnlen((char*)p1,ATCMD_LENGTH)+1);
    strncpy(vbaction.cmdparam,(char*)p1,strnlen((char*)p1,ATCMD_LENGTH));
  }
  return ESP_OK;
}
esp_err_t cmdWa(char* orig, void* p1, void* p2) {
  //we don't do anything here. AT WA is just placed in this file
  //for a fully implemented command table.
  //AT WA is implemented in fct_macros.c, where the task is delayed
  //for the given time before further commands are issued.
  return ESP_OK;
}
esp_err_t cmdRo(char* orig, void* p1, void* p2) {
  //check if we are "aligned" to 90°
  if((((int32_t)p1 % 90) != 0) || currentCfg == NULL) return ESP_FAIL;
  currentCfg->adc.orientation = (int32_t)p1;
  return ESP_OK;
}
esp_err_t cmdBt(char* orig, void* p1, void* p2) {
  if(currentCfg == NULL) return ESP_FAIL;
  currentCfg->usb_active = ((int32_t)p1) & 0x01;
  currentCfg->ble_active = (((int32_t)p1) & 0x02)>>1;
  return ESP_OK;
}
esp_err_t cmdTt(char* orig, void* p1, void* p2) {
  if(currentCfg == NULL) return ESP_FAIL;
  ///TODO: not implemented yet.
  return ESP_OK;
}
esp_err_t cmdAp(char* orig, void* p1, void* p2) {
  if(currentCfg == NULL) return ESP_FAIL;
  if(requestVBUpdate == VB_SINGLESHOT) currentCfg->debounce_press = (int32_t)p1;
  else currentCfg->debounce_press_vb[requestVBUpdate] = (int32_t)p1;
  return ESP_OK;
}
esp_err_t cmdAr(char* orig, void* p1, void* p2) {
  if(currentCfg == NULL) return ESP_FAIL;
  if(requestVBUpdate == VB_SINGLESHOT) currentCfg->debounce_release = (int32_t)p1;
  else currentCfg->debounce_release_vb[requestVBUpdate] = (int32_t)p1;
  return ESP_OK;
}
esp_err_t cmdAi(char* orig, void* p1, void* p2) {
  if(currentCfg == NULL) return ESP_FAIL;
  if(requestVBUpdate == VB_SINGLESHOT) currentCfg->debounce_idle = (int32_t)p1;
  else currentCfg->debounce_idle_vb[requestVBUpdate] = (int32_t)p1;
  return ESP_OK;
}
esp_err_t cmdFr(char* orig, void* p1, void* p2) {
  uint32_t free,total;
  if(halStorageGetFree(&total,&free) == ESP_OK)
  {
    char str[32];
    sprintf(str,"FREE:%d%%,%d,%d",(uint8_t)(1-free/total)*100,total-free,free);
    halSerialSendUSBSerial(str,strnlen(str,32),20);
    return ESP_OK;
  } else return ESP_FAIL;
}
esp_err_t cmdPw(char* orig, void* p1, void* p2)
{
  return halStorageNVSStoreString(NVS_WIFIPW,(char*)p1);
}
esp_err_t cmdFw(char* orig, void* p1, void* p2) {
  esp_partition_t* factory;
  //determine update mode
  switch((int32_t) p1)
  {
	  //update ESP32 by setting the factory partition to boot next time
	  case 2:
		factory = esp_partition_find_first(ESP_PARTITION_TYPE_APP,\
		  ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
		if(factory == NULL)
		{
			ESP_LOGE(LOG_TAG,"Cannot find factory partition");
			return ESP_FAIL;
		}
		if(esp_ota_set_boot_partition(factory) != ESP_OK)
		{
			ESP_LOGE(LOG_TAG,"Cannot activate factory partition");
			return ESP_FAIL;
		}
		general.cmd[0] = 0x02;
	  //update LPC by sending a command via I2C
	  case 3:
		general.cmd[0] = 0x03;
	  default: return ESP_FAIL;
  }
  return ESP_OK;
}

/*++++ Mouse HID command handlers ++++*/
esp_err_t cmdCl(char* orig, void* p1, void* p2) {
  mouse.cmd[0] = 0x13; return ESP_OK;
}
esp_err_t cmdCr(char* orig, void* p1, void* p2) {
  mouse.cmd[0] = 0x14; return ESP_OK;
}
esp_err_t cmdCm(char* orig, void* p1, void* p2) {
  mouse.cmd[0] = 0x15; return ESP_OK;
}
esp_err_t cmdCd(char* orig, void* p1, void* p2) {
  mouse.cmd[0] = 0x13;
  mouseD.cmd[0] = 0x13; return ESP_OK;
}
esp_err_t cmdHl(char* orig, void* p1, void* p2) {
  mouse.cmd[0] = 0x16;
  mouseR.cmd[0] = 0x19; return ESP_OK;
}
esp_err_t cmdHr(char* orig, void* p1, void* p2) {
  mouse.cmd[0] = 0x17;
  mouseR.cmd[0] = 0x1A; return ESP_OK;
}
esp_err_t cmdHm(char* orig, void* p1, void* p2) {
  mouse.cmd[0] = 0x18;
  mouseR.cmd[0] = 0x1B; return ESP_OK;
}
esp_err_t cmdRl(char* orig, void* p1, void* p2) {
  mouse.cmd[0] = 0x19; return ESP_OK;
}
esp_err_t cmdRr(char* orig, void* p1, void* p2) {
  mouse.cmd[0] = 0x1A; return ESP_OK;
}
esp_err_t cmdRm(char* orig, void* p1, void* p2) {
  mouse.cmd[0] = 0x1B; return ESP_OK;
}
esp_err_t cmdTl(char* orig, void* p1, void* p2) {
  mouse.cmd[0] = 0x1C; return ESP_OK;
}
esp_err_t cmdTr(char* orig, void* p1, void* p2) {
  mouse.cmd[0] = 0x1D; return ESP_OK;
}
esp_err_t cmdTm(char* orig, void* p1, void* p2) {
  mouse.cmd[0] = 0x1E; return ESP_OK;
}
esp_err_t cmdWu(char* orig, void* p1, void* p2) {
  if(currentCfg == NULL) return ESP_FAIL;
  mouse.cmd[0] = 0x12; 
  //reset to 3, if invalid.
  if(currentCfg->wheel_stepsize == 0) currentCfg->wheel_stepsize = 3;
  mouse.cmd[1] = currentCfg->wheel_stepsize;
  return ESP_OK;
}
esp_err_t cmdWd(char* orig, void* p1, void* p2) {
  if(currentCfg == NULL) return ESP_FAIL;
  mouse.cmd[0] = 0x12;
  //reset to 3, if invalid.
  if(currentCfg->wheel_stepsize == 0) currentCfg->wheel_stepsize = 3;
  mouse.cmd[1] = -currentCfg->wheel_stepsize;
  return ESP_OK;
}
esp_err_t cmdWs(char* orig, void* p1, void* p2) {
  if(currentCfg == NULL) return ESP_FAIL;
  currentCfg->wheel_stepsize = (int32_t)p1;
  return ESP_OK;
}
esp_err_t cmdMx(char* orig, void* p1, void* p2) {
  if(currentCfg == NULL) return ESP_FAIL;
  mouse.cmd[0] = 0x10; 
  mouse.cmd[1] = (int32_t)p1;
  return ESP_OK;
}
esp_err_t cmdMy(char* orig, void* p1, void* p2) {
  if(currentCfg == NULL) return ESP_FAIL;
  mouse.cmd[0] = 0x11; 
  mouse.cmd[1] = (int32_t)p1;
  return ESP_OK;
}

/*++++ Keyboard HID command handlers ++++*/
esp_err_t keyboard_helper_parsekeycode(char t, uint8_t *buf)
{
  hid_cmd_t cmd;  //this would be the press or press&release action
  memset(&cmd,0,sizeof(hid_cmd_t));
  //if the first HID cmd is sent, this flag is set.
  uint8_t deleted = 0;
  char *pch;
  uint8_t cnt = 0;
  uint16_t keycode = 0;
  uint16_t releaseArr[16];
  
  //remove trailing \r/\n
  strip((char*)buf);
  pch = (char*)&buf[5];
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
        //we do the press event here.
        if(deleted == 0) sendHIDCmd(&cmd,requestVBUpdate|0x80,buf,1);        
        else sendHIDCmd(&cmd,requestVBUpdate|0x80,NULL,0);
        deleted = 1;
        ESP_LOGI(LOG_TAG,"Press action 0x%2X, keycode/modifier: 0x%2X",cmd.cmd[0],cmd.cmd[1]);
        //save for later release
        releaseArr[cnt] = keycode;
        cnt++;
        
        if(cnt == 14) //maximum 6 keycodes + 8 modifier bits
        {
          sendErrorBack("AT KP/KH/KR parameter too long");
          return ESP_FAIL;
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
    return ESP_FAIL;
  }
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
      
      if(deleted == 0) sendHIDCmd(&cmd,requestVBUpdate,buf,1);        
      else sendHIDCmd(&cmd,requestVBUpdate,NULL,0);
      deleted = 1;
      ESP_LOGI(LOG_TAG,"Release action 0x%2X, keycode/modifier: 0x%2X",cmd.cmd[0],cmd.cmd[1]);
    }
  }
  return ESP_OK;
}
esp_err_t cmdKw(char* orig, void* p1, void* p2) {
  int offset = 0;
  int offsetOut = 0;
  uint8_t deadkeyfirst = 0;
  uint8_t modifier = 0;
  uint8_t keycode = 0;
  hid_cmd_t cmd;
  //if the first HID cmd is sent, this flag is set.
  uint8_t deleted = 0;
  memset(&cmd,0,sizeof(hid_cmd_t));
  
  //remove trailing \r/\n
  strip(p1);

  //save each byte from KW string to keyboard instance
  //offset must be less then buffer length - 6 bytes for "AT KW "
  size_t length = strlen((char*)p1);
  while(offset <= length)
  {
    //terminate...
    if(((char*)p1)[offset] == 0) break;
    
    
    //parse ASCII/unicode to keycode sequence
    keycode = unicode_to_keycode(((char*)p1)[offset], currentCfg->locale);
    deadkeyfirst = deadkey_to_keycode(keycode,currentCfg->locale);
    if(deadkeyfirst != 0) deadkeyfirst = keycode_to_key(deadkeyfirst);
    modifier = keycode_to_modifier(keycode, currentCfg->locale);
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
        if(deleted == 0) sendHIDCmd(&cmd,requestVBUpdate|0x80,(uint8_t*)orig,1);        
        else sendHIDCmd(&cmd,requestVBUpdate|0x80,NULL,0);
        deleted = 1;
        
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
        if(deleted == 0) sendHIDCmd(&cmd,requestVBUpdate|0x80,(uint8_t*)orig,1);        
        else sendHIDCmd(&cmd,requestVBUpdate|0x80,NULL,0);
        deleted = 1;
      }
      
      cmd.cmd[0] = 0x20; cmd.cmd[1] = keycode;
      //send the cmd either directly or save it to the HID task
      if(deleted == 0) sendHIDCmd(&cmd,requestVBUpdate|0x80,(uint8_t*)orig,1);        
      else sendHIDCmd(&cmd,requestVBUpdate|0x80,NULL,0);
      deleted = 1;
      
      if(modifier){
        cmd.cmd[0] = 0x26; cmd.cmd[1] = modifier;
        //send the cmd either directly or save it to the HID task
        if(deleted == 0) sendHIDCmd(&cmd,requestVBUpdate|0x80,(uint8_t*)orig,1);        
        else sendHIDCmd(&cmd,requestVBUpdate|0x80,NULL,0);
        deleted = 1;
      }
      
      offsetOut++;
    } else {
      ESP_LOGD(LOG_TAG, "Need another byte (unicode)");
    }
    offset++;
  }
  return ESP_OK;
}
esp_err_t cmdKp(char* orig, void* p1, void* p2) {
  return keyboard_helper_parsekeycode('P',(uint8_t*)orig);}
esp_err_t cmdKh(char* orig, void* p1, void* p2) {
  return keyboard_helper_parsekeycode('H',(uint8_t*)orig);}
esp_err_t cmdKr(char* orig, void* p1, void* p2) {
  return keyboard_helper_parsekeycode('R',(uint8_t*)orig);}
esp_err_t cmdKt(char* orig, void* p1, void* p2) {
  return keyboard_helper_parsekeycode('T',(uint8_t*)orig);}
esp_err_t cmdRa(char* orig, void* p1, void* p2) {
  halBLEReset(0xFE);
  halSerialReset(0xFE);
  return ESP_OK;
}

/*++++ Storage related handlers ++++*/
esp_err_t cmdSa(char* orig, void* p1, void* p2) {
  storeSlot((char*)p1);
  return ESP_OK;
}
esp_err_t cmdLo(char* orig, void* p1, void* p2) {
  if(requestVBUpdate == VB_SINGLESHOT)
  {
    xQueueSend(config_switcher,p1,(TickType_t)10);
  } else {
    vbaction.cmd = T_CONFIGCHANGE;
  }
  return ESP_OK;
}
esp_err_t cmdLa(char* orig, void* p1, void* p2) {
  printAllSlots(1); return ESP_OK;
}
esp_err_t cmdLi(char* orig, void* p1, void* p2) {
  printAllSlots(0); return ESP_OK;
}
esp_err_t cmdNe(char* orig, void* p1, void* p2) {
  if(requestVBUpdate == VB_SINGLESHOT)
  {
    char slotname[SLOTNAME_LENGTH] = "__NEXT";
    xQueueSend(config_switcher,(void*)slotname,(TickType_t)10);
  } else {
    vbaction.cmd = T_CONFIGCHANGE;
    vbaction.cmdparam = malloc(strlen("__NEXT")+1);
    strcpy(vbaction.cmdparam,"__NEXT");
  }
  return ESP_OK;
}
esp_err_t cmdDe(char* orig, void* p1, void* p2) {
  uint32_t tid;
  esp_err_t retval;
  retval = halStorageStartTransaction(&tid,20,LOG_TAG);
  if(retval != ESP_OK) return retval;
  retval = halStorageDeleteSlot(-1,tid);
  halStorageFinishTransaction(tid);
  return retval;
}
esp_err_t cmdDl(char* orig, void* p1, void* p2) {
  uint32_t tid;
  esp_err_t retval;
  retval = halStorageStartTransaction(&tid,20,LOG_TAG);
  if(retval != ESP_OK) return retval;
  retval = halStorageDeleteSlot((int32_t)p1,tid);
  halStorageFinishTransaction(tid);
  return retval;
}
esp_err_t cmdDn(char* orig, void* p1, void* p2) {
  uint32_t tid;
  uint8_t slotnumber;
  esp_err_t retval;
  retval = halStorageStartTransaction(&tid,20,LOG_TAG);
  if(retval != ESP_OK) return retval;
  retval = halStorageGetNumberForName(tid,&slotnumber,(char *)p1); 
  if(retval != ESP_OK) {
    halStorageFinishTransaction(tid);
    return retval;
  }
  retval = halStorageDeleteSlot(slotnumber,tid);
  halStorageFinishTransaction(tid);
  return retval;
}
esp_err_t cmdNc(char* orig, void* p1, void* p2) {
  if(requestVBUpdate != VB_SINGLESHOT)
  {
    handler_hid_delCmd(requestVBUpdate);
    handler_vb_delCmd(requestVBUpdate);
    requestVBUpdate = VB_SINGLESHOT;
  }
  return ESP_OK;
}

/*++++ Mouthpiece mode handlers ++++*/
esp_err_t cmdMm(char* orig, void* p1, void* p2) {
  if(currentCfg == NULL) return ESP_FAIL;
  switch((int32_t)p1)
  {
    case 0: currentCfg->adc.mode = THRESHOLD; break;
    case 1: currentCfg->adc.mode = MOUSE; break;
    case 2: currentCfg->adc.mode = JOYSTICK; break;
    case 3: currentCfg->adc.mode = NONE; break;
    default: return ESP_FAIL;
  }
  return ESP_OK;
}
esp_err_t cmdSw(char* orig, void* p1, void* p2) {
  switch(currentCfg->adc.mode)
  {
    case MOUSE: currentCfg->adc.mode = THRESHOLD; break;
    case THRESHOLD: currentCfg->adc.mode = MOUSE; break;
    case JOYSTICK: case NONE: return ESP_FAIL;
  }
  return ESP_OK;
}
esp_err_t cmdSr(char* orig, void* p1, void* p2) {
  if(currentCfg == NULL) return ESP_FAIL;
  currentCfg->adc.reportraw = 1;
  return ESP_OK;
}
esp_err_t cmdEr(char* orig, void* p1, void* p2) {
  if(currentCfg == NULL) return ESP_FAIL;
  currentCfg->adc.reportraw = 0;
  return ESP_OK;
}
esp_err_t cmdCa(char* orig, void* p1, void* p2) {
  if(requestVBUpdate == VB_SINGLESHOT)
  {
    halAdcCalibrate();
  } else {
    vbaction.cmd = T_CALIBRATE;
  }
  return ESP_OK;
}
/*++++ joystick command handler ++++*/
void joystick_helper_axis(uint8_t val1, uint8_t val2, uint16_t v)
{
  joystick.cmd[0] = val1;
  joystick.cmd[1] = v & 0xFF;
  joystick.cmd[2] = (v & 0xFF00)>>8;
  joystickR.cmd[0] = val2;
}

esp_err_t cmdJx(char* orig, void* p1, void* p2) {
  //if p2 is set, we need a release action.
  if(((int32_t) p2) == 0) joystick_helper_axis(0x34,0,(int32_t) p1);
  else joystick_helper_axis(0x34,0x34,(int32_t) p1);
  return ESP_OK;
}
esp_err_t cmdJy(char* orig, void* p1, void* p2) {
  //if p2 is set, we need a release action.
  if(((int32_t) p2) == 0) joystick_helper_axis(0x35,0,(int32_t) p1);
  else joystick_helper_axis(0x35,0x35,(int32_t) p1);
  return ESP_OK;
}
esp_err_t cmdJz(char* orig, void* p1, void* p2) {
  //if p2 is set, we need a release action.
  if(((int32_t) p2) == 0) joystick_helper_axis(0x36,0,(int32_t) p1);
  else joystick_helper_axis(0x36,0x36,(int32_t) p1);
  return ESP_OK;
}
esp_err_t cmdJt(char* orig, void* p1, void* p2) {
  //if p2 is set, we need a release action.
  if(((int32_t) p2) == 0) joystick_helper_axis(0x37,0,(int32_t) p1);
  else joystick_helper_axis(0x37,0x37,(int32_t) p1);
  return ESP_OK;
}
esp_err_t cmdJs(char* orig, void* p1, void* p2) {
  //if p2 is set, we need a release action.
  if(((int32_t) p2) == 0) joystick_helper_axis(0x38,0,(int32_t) p1);
  else joystick_helper_axis(0x38,0x38,(int32_t) p1);
  return ESP_OK;
}
esp_err_t cmdJu(char* orig, void* p1, void* p2) {
  //if p2 is set, we need a release action.
  if(((int32_t) p2) == 0) joystick_helper_axis(0x39,0,(int32_t) p1);
  else joystick_helper_axis(0x39,0x39,(int32_t)p1);
  return ESP_OK;
}
esp_err_t cmdJp(char* orig, void* p1, void* p2) {
  joystick.cmd[0] = 0x31;
  joystick.cmd[1] = ((int32_t) p1)&0x7F; //high bit determines it is a joystick hat
  return ESP_OK;
}
esp_err_t cmdJc(char* orig, void* p1, void* p2) {
  joystick.cmd[0] = 0x30;
  joystick.cmd[1] = ((int32_t) p1)&0x7F; //high bit determines it is a joystick hat
  return ESP_OK;
}
esp_err_t cmdJr(char* orig, void* p1, void* p2) {
  joystick.cmd[0] = 0x32;
  joystick.cmd[1] = ((int32_t) p1)&0x7F; //high bit determines it is a joystick hat
  return ESP_OK;
}
esp_err_t cmdJh(char* orig, void* p1, void* p2) {
  joystick.cmd[0] = 0x32;
  if(((int32_t) p1) == -1) joystick.cmd[1] = 0x8F;
  else joystick.cmd[1] = (((int32_t) p1)&0x7F) | 0x80; //high bit determines it is a joystick hat
  return ESP_OK;
}
esp_err_t cmdIr(char* orig, void* p1, void* p2) {
  //trigger record
  if(fct_infrared_record((char*)p1,1) == ESP_OK)
  {
    ESP_LOGD(LOG_TAG,"Recorded IR cmd %s",(char*)p1);
    return ESP_OK;
  } else return ESP_FAIL;
}
esp_err_t cmdIp(char* orig, void* p1, void* p2) {
  if(requestVBUpdate == VB_SINGLESHOT)
  {
    fct_infrared_send((char*)p1);
  } else {
    //set action type
    vbaction.cmd = T_SENDIR;
    vbaction.cmdparam = malloc(strnlen((char*)p1,ATCMD_LENGTH)+1);
    strncpy(vbaction.cmdparam,(char*)p1,strnlen((char*)p1,ATCMD_LENGTH));
  }
  return ESP_OK;
}
esp_err_t cmdIh(char* orig, void* p1, void* p2) {
  return ESP_OK;
}
esp_err_t cmdIc(char* orig, void* p1, void* p2) {
  uint32_t tid;
  if(halStorageStartTransaction(&tid,20,LOG_TAG) == ESP_OK)
  {
    uint8_t nr = 0;
    if(halStorageGetNumberForNameIR(tid,&nr,(char*)p1) == ESP_OK)
    {
      if(halStorageDeleteIRCmd(nr,tid) != ESP_OK)
      {
        ESP_LOGE(LOG_TAG,"Cannot delete IR cmd %s",(char*)p1);
      } else {
        ESP_LOGI(LOG_TAG,"Deleted IR slot %s @%d",(char*)p1,nr);
      }
    } else {
      ESP_LOGE(LOG_TAG,"No slot found for IR cmd %s",(char*)p1);
    }
    halStorageFinishTransaction(tid);
  } else {
    ESP_LOGE(LOG_TAG,"Cannot start transaction");
  }
  return ESP_OK;
}
esp_err_t cmdIw(char* orig, void* p1, void* p2) {
  uint32_t tid;
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
  return ESP_OK;
}
esp_err_t cmdIl(char* orig, void* p1, void* p2) {
  uint32_t tid;
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
          return ESP_OK;
        }
      }
    } else {
      ESP_LOGE(LOG_TAG,"Cannot get IR cmd number");
    }
    halStorageFinishTransaction(tid);
  } else {
    ESP_LOGE(LOG_TAG,"Cannot start transaction");
  }
  return ESP_FAIL;
}
esp_err_t cmdIx(char* orig, void* p1, void* p2) {
  uint32_t tid;
  if(halStorageStartTransaction(&tid,20,LOG_TAG) == ESP_OK)
  {
    if(halStorageDeleteIRCmd((int32_t)p1,tid) != ESP_OK)
    {
      ESP_LOGE(LOG_TAG,"Cannot delete IR cmd %d",(int32_t)p1);
    }
    halStorageFinishTransaction(tid);
  } else {
    ESP_LOGE(LOG_TAG,"Cannot start transaction");
  }
  return ESP_OK;
}

#define CMD_TARGET_TYPE generalConfig_t

/*++++ command struct ++++*/
const onecmd_t commands[] = {
  // general commands
  {"ID", {PARAM_NONE,PARAM_NONE},{0,0},{0,0},cmdId,0,NOCAST},
  {"BM", {PARAM_NUMBER,PARAM_NONE},{0,0},{VB_MAX-1,0},cmdBm,0,NOCAST},
  {"BL", {PARAM_NUMBER,PARAM_NONE},{0,0},{1,0},NULL,offsetof(CMD_TARGET_TYPE,button_learn),UINT8},
  {"MA", {PARAM_STRING,PARAM_NONE},{5,0},{ATCMD_LENGTH-strlen(CMD_PREFIX)-CMD_LENGTH,0},cmdMa,0,NOCAST},
  {"WA", {PARAM_NUMBER,PARAM_NONE},{0,0},{30000,0},cmdWa,0,NOCAST},
  {"RO", {PARAM_NUMBER,PARAM_NONE},{0,0},{270,0},cmdRo,0,NOCAST},
  {"KL", {PARAM_NUMBER,PARAM_NONE},{0,0},{24,0},NULL,offsetof(CMD_TARGET_TYPE,locale),UINT8},
  {"BT", {PARAM_NUMBER,PARAM_NONE},{0,0},{3,0},cmdBt,0,NOCAST},
  {"TT", {PARAM_NUMBER,PARAM_NONE},{100,0},{5000,0},cmdTt,0,NOCAST},
  {"AP", {PARAM_NUMBER,PARAM_NONE},{1,0},{500,0},cmdAp,0,NOCAST},
  {"AR", {PARAM_NUMBER,PARAM_NONE},{1,0},{500,0},cmdAr,0,NOCAST},
  {"AI", {PARAM_NUMBER,PARAM_NONE},{1,0},{500,0},cmdAi,0,NOCAST},
  {"FR", {PARAM_NONE,PARAM_NONE},{0,0},{0,0},cmdFr,0,NOCAST},
  {"FB", {PARAM_NUMBER,PARAM_NONE},{0,0},{3,0},NULL,offsetof(CMD_TARGET_TYPE,feedback),UINT8},
  {"PW", {PARAM_STRING,PARAM_NONE},{8,0},{32,0},cmdPw,0,NOCAST},
  {"FW", {PARAM_NUMBER,PARAM_NONE},{0,0},{1,0},cmdFw,0,NOCAST},
  // HID - mouse commands
  {"CL", {PARAM_NONE,PARAM_NONE},{0,0},{0,0},cmdCl,0,NOCAST},
  {"CR", {PARAM_NONE,PARAM_NONE},{0,0},{0,0},cmdCr,0,NOCAST},
  {"CM", {PARAM_NONE,PARAM_NONE},{0,0},{0,0},cmdCm,0,NOCAST},
  {"CD", {PARAM_NONE,PARAM_NONE},{0,0},{0,0},cmdCd,0,NOCAST},
  {"HL", {PARAM_NONE,PARAM_NONE},{0,0},{0,0},cmdHl,0,NOCAST},
  {"PL", {PARAM_NONE,PARAM_NONE},{0,0},{0,0},cmdHl,0,NOCAST},
  {"HR", {PARAM_NONE,PARAM_NONE},{0,0},{0,0},cmdHr,0,NOCAST},
  {"PR", {PARAM_NONE,PARAM_NONE},{0,0},{0,0},cmdHr,0,NOCAST},
  {"HM", {PARAM_NONE,PARAM_NONE},{0,0},{0,0},cmdHm,0,NOCAST},
  {"PM", {PARAM_NONE,PARAM_NONE},{0,0},{0,0},cmdHm,0,NOCAST},
  {"RL", {PARAM_NONE,PARAM_NONE},{0,0},{0,0},cmdRl,0,NOCAST},
  {"RR", {PARAM_NONE,PARAM_NONE},{0,0},{0,0},cmdRr,0,NOCAST},
  {"RM", {PARAM_NONE,PARAM_NONE},{0,0},{0,0},cmdRm,0,NOCAST},
  {"TL", {PARAM_NONE,PARAM_NONE},{0,0},{0,0},cmdTl,0,NOCAST},
  {"TR", {PARAM_NONE,PARAM_NONE},{0,0},{0,0},cmdTr,0,NOCAST},
  {"TM", {PARAM_NONE,PARAM_NONE},{0,0},{0,0},cmdTm,0,NOCAST},
  {"WU", {PARAM_NONE,PARAM_NONE},{0,0},{0,0},cmdWu,0,NOCAST},
  {"WD", {PARAM_NONE,PARAM_NONE},{0,0},{0,0},cmdWd,0,NOCAST},
  {"WS", {PARAM_NUMBER,PARAM_NONE},{1,0},{127,0},cmdWs,0,NOCAST},
  {"MX", {PARAM_NUMBER,PARAM_NONE},{-127,0},{127,0},cmdMx,0,NOCAST},
  {"MY", {PARAM_NUMBER,PARAM_NONE},{-127,0},{127,0},cmdMy,0,NOCAST},
  // HID - keyboard commands
  {"KW", {PARAM_STRING,PARAM_NONE},{1,0},{ATCMD_LENGTH-strlen(CMD_PREFIX)-CMD_LENGTH,0},cmdKw,0,NOCAST},
  {"KP", {PARAM_STRING,PARAM_NONE},{5,0},{ATCMD_LENGTH-strlen(CMD_PREFIX)-CMD_LENGTH,0},cmdKp,0,NOCAST},
  {"KH", {PARAM_STRING,PARAM_NONE},{5,0},{ATCMD_LENGTH-strlen(CMD_PREFIX)-CMD_LENGTH,0},cmdKh,0,NOCAST},
  {"KR", {PARAM_STRING,PARAM_NONE},{5,0},{ATCMD_LENGTH-strlen(CMD_PREFIX)-CMD_LENGTH,0},cmdKr,0,NOCAST},
  {"KT", {PARAM_STRING,PARAM_NONE},{5,0},{ATCMD_LENGTH-strlen(CMD_PREFIX)-CMD_LENGTH,0},cmdKt,0,NOCAST},
  {"RA", {PARAM_NONE,PARAM_NONE},{0,0},{0,0},cmdRa,0,NOCAST},
  // storage commands
  {"SA", {PARAM_STRING,PARAM_NONE},{1,0},{SLOTNAME_LENGTH,0},cmdSa,0,NOCAST},
  {"LO", {PARAM_STRING,PARAM_NONE},{1,0},{SLOTNAME_LENGTH,0},cmdLo,0,NOCAST},
  {"LA", {PARAM_NONE,PARAM_NONE},{0,0},{0,0},cmdLa,0,NOCAST},
  {"LI", {PARAM_NONE,PARAM_NONE},{0,0},{0,0},cmdLi,0,NOCAST},
  {"NE", {PARAM_NONE,PARAM_NONE},{0,0},{0,0},cmdNe,0,NOCAST},
  {"DE", {PARAM_NONE,PARAM_NONE},{0,0},{0,0},cmdDe,0,NOCAST},
  {"DL", {PARAM_NUMBER,PARAM_NONE},{0,0},{250,0},cmdDl,0,NOCAST},
  {"DN", {PARAM_STRING,PARAM_NONE},{1,0},{SLOTNAME_LENGTH,0},cmdDn,0,NOCAST},
  {"NC", {PARAM_NONE,PARAM_NONE},{0,0},{3,0},cmdNc,0,NOCAST},
  // mouthpiece / ADC settings
  {"MM", {PARAM_NUMBER,PARAM_NONE},{0,0},{2,0},cmdMm,0,NOCAST},
  {"SW", {PARAM_NONE,PARAM_NONE},{0,0},{0,0},cmdSw,0,NOCAST},
  {"SR", {PARAM_NONE,PARAM_NONE},{0,0},{0,0},cmdSr,0,NOCAST},
  {"ER", {PARAM_NONE,PARAM_NONE},{0,0},{0,0},cmdEr,0,NOCAST},
  {"CA", {PARAM_NONE,PARAM_NONE},{0,0},{0,0},cmdCa,0,NOCAST},
  {"AX", {PARAM_NUMBER,PARAM_NONE},{0,0},{100,0},NULL,offsetof(CMD_TARGET_TYPE,adc.sensitivity_x),UINT8},
  {"AY", {PARAM_NUMBER,PARAM_NONE},{0,0},{100,0},NULL,offsetof(CMD_TARGET_TYPE,adc.sensitivity_y),UINT8},
  {"AC", {PARAM_NUMBER,PARAM_NONE},{0,0},{100,0},NULL,offsetof(CMD_TARGET_TYPE,adc.acceleration),UINT8},
  {"MS", {PARAM_NUMBER,PARAM_NONE},{0,0},{100,0},NULL,offsetof(CMD_TARGET_TYPE,adc.max_speed),UINT8},
  {"DX", {PARAM_NUMBER,PARAM_NONE},{0,0},{10000,0},NULL,offsetof(CMD_TARGET_TYPE,adc.deadzone_x),UINT8},
  {"DY", {PARAM_NUMBER,PARAM_NONE},{0,0},{10000,0},NULL,offsetof(CMD_TARGET_TYPE,adc.deadzone_y),UINT8},
  {"TS", {PARAM_NUMBER,PARAM_NONE},{0,0},{512,0},NULL,offsetof(CMD_TARGET_TYPE,adc.threshold_sip),UINT16},
  {"SS", {PARAM_NUMBER,PARAM_NONE},{0,0},{512,0},NULL,offsetof(CMD_TARGET_TYPE,adc.threshold_strongsip),UINT16},
  {"TP", {PARAM_NUMBER,PARAM_NONE},{512,0},{1023,0},NULL,offsetof(CMD_TARGET_TYPE,adc.threshold_puff),UINT16},
  {"SP", {PARAM_NUMBER,PARAM_NONE},{512,0},{1023,0},NULL,offsetof(CMD_TARGET_TYPE,adc.threshold_strongpuff),UINT16},
  
  // joystick commands
  {"JX", {PARAM_NUMBER,PARAM_NUMBER},{0,0},{1023,1},cmdJx,0,NOCAST},
  {"JY", {PARAM_NUMBER,PARAM_NUMBER},{0,0},{1023,1},cmdJy,0,NOCAST},
  {"JZ", {PARAM_NUMBER,PARAM_NUMBER},{0,0},{1023,1},cmdJz,0,NOCAST},
  {"JT", {PARAM_NUMBER,PARAM_NUMBER},{0,0},{1023,1},cmdJt,0,NOCAST},
  {"JS", {PARAM_NUMBER,PARAM_NUMBER},{0,0},{1023,1},cmdJs,0,NOCAST},
  {"JU", {PARAM_NUMBER,PARAM_NUMBER},{0,0},{1023,1},cmdJu,0,NOCAST},
  
  {"JP", {PARAM_NUMBER,PARAM_NONE},{1,0},{32,0},cmdJp,0,NOCAST},
  {"JC", {PARAM_NUMBER,PARAM_NONE},{1,0},{32,0},cmdJc,0,NOCAST},
  {"JR", {PARAM_NUMBER,PARAM_NONE},{1,0},{32,0},cmdJr,0,NOCAST},
  {"JH", {PARAM_NUMBER,PARAM_NUMBER},{-1,0},{315,1},cmdJh,0,NOCAST},
  // IR commands
  {"IR", {PARAM_STRING,PARAM_NONE},{2,0},{32,0},cmdIr,0,NOCAST},
  {"IP", {PARAM_STRING,PARAM_NONE},{2,0},{32,0},cmdIp,0,NOCAST},
  {"IH", {PARAM_STRING,PARAM_NONE},{2,0},{ATCMD_LENGTH-strlen(CMD_PREFIX)-CMD_LENGTH,0},cmdIh,0,NOCAST},
  {"IC", {PARAM_STRING,PARAM_NONE},{2,0},{32,0},cmdIc,0,NOCAST},
  {"IW", {PARAM_NONE,PARAM_NONE},{0,0},{0,0},cmdIw,0,NOCAST},
  {"IT", {PARAM_NUMBER,PARAM_NONE},{2,0},{100,0},NULL,offsetof(CMD_TARGET_TYPE,irtimeout),UINT8},
  {"IL", {PARAM_NONE,PARAM_NONE},{0,0},{0,0},cmdIl,0,NOCAST},
  {"IX", {PARAM_NUMBER,PARAM_NONE},{1,0},{99,0},cmdIx,0,NOCAST},
};

#if 0
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
            handler_vb_delCmd(cmd.vb);
            if(cmd.atoriginal != NULL) 
            {
              handler_hid_addCmd(&cmd,1);
              cmd.atoriginal = NULL;
            } else handler_hid_addCmd(&cmd,0);
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
            handler_vb_delCmd(cmd.vb);
            if(cmd.atoriginal != NULL) 
            {
              handler_hid_addCmd(&cmd,1);
              cmd.atoriginal = NULL;
            } else handler_hid_addCmd(&cmd,0);
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
          handler_vb_delCmd(cmd.vb);
          if(cmd.atoriginal != NULL) 
          {
            handler_hid_addCmd(&cmd,1);
            cmd.atoriginal = NULL;
          } else handler_hid_addCmd(&cmd,0);
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
            handler_vb_delCmd(cmd.vb);
            if(cmd.atoriginal != NULL) 
            {
              handler_hid_addCmd(&cmd,1);
              cmd.atoriginal = NULL;
            } else handler_hid_addCmd(&cmd,0);
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
            handler_vb_delCmd(cmd.vb);
            if(cmd.atoriginal != NULL) 
            {
              handler_hid_addCmd(&cmd,1);
              cmd.atoriginal = NULL;
            } else handler_hid_addCmd(&cmd,0);
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
            handler_vb_delCmd(cmd.vb);
            handler_hid_addCmd(&cmd,0);
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

#endif
void task_commands(void *params)
{
  uint8_t queuesready = checkqueues();
  int received;
  //used for return values of doXXXParsing functions
  uint8_t *commandBuffer = NULL;

  while(1)
  {
    if(queuesready)
    {
      //wait for incoming data
      received = halSerialReceiveUSBSerial(&commandBuffer);
      
      //if no command received, try again...
      if(received == -1 || commandBuffer == NULL) continue;
      
      //before we start parsing anything, we need to be sure
      //all global commands are cleared.
      memset(&mouse,0,sizeof(hid_cmd_t));
      memset(&mouseR,0,sizeof(hid_cmd_t));
      memset(&mouseD,0,sizeof(hid_cmd_t));
      memset(&keyboard,0,sizeof(hid_cmd_t));
      memset(&keyboardR,0,sizeof(hid_cmd_t));
      memset(&joystick,0,sizeof(hid_cmd_t));
      memset(&joystickR,0,sizeof(hid_cmd_t));
      memset(&general,0,sizeof(hid_cmd_t));
      memset(&vbaction,0,sizeof(vb_cmd_t));
      
      //to be sure, we want a valid cfg pointer...
      currentCfg = configGetCurrent();
      if(currentCfg == NULL)
      {
        ESP_LOGE(LOG_TAG,"Cannot proceed with parsing, config is NULL");
        continue;
      }
      //now send it to the parser and validate result.
      cmd_retval retvalparser = cmdParser((char*)commandBuffer,currentCfg);
      
      //take actions according to return value
      //we need to clean up, so we cannot stop after this switch
      switch(retvalparser)
      {
        case PREFIXONLY: //we received the prefix only, return "OK"
          halSerialSendUSBSerial((char*)"OK",4,100);
          break;
        case POINTERERROR:
          ESP_LOGE(LOG_TAG,"Pointer error, parser config illegal!");
          break;
        case HANDLERERROR:
          strip((char*)commandBuffer);
          ESP_LOGE(LOG_TAG,"ERROR: %s",commandBuffer);
          break;
        case PARAMERROR:
          halSerialSendUSBSerial((char*)"? - params:",12,100);
          halSerialSendUSBSerial((char*)commandBuffer,strlen((char*)commandBuffer),100);
          break;
        case FORMATERROR:
          halSerialSendUSBSerial((char*)"? - format:",12,100);
          halSerialSendUSBSerial((char*)commandBuffer,strlen((char*)commandBuffer),100);
          break;
        case NOCOMMAND:
          halSerialSendUSBSerial((char*)"?:",3,100);
          halSerialSendUSBSerial((char*)commandBuffer,strlen((char*)commandBuffer),100);
          break;
        case SUCCESS:
          strip((char*)commandBuffer);
          ESP_LOGI(LOG_TAG,"Success: %s",commandBuffer);
          break;
      }
      
      //do further things only if successful:
      //1.) we need to check if some handler modified any of
      //the hid_cmd_t or vb_cmd_t structs (mouse,general,keyboard,
      //joystick,vbaction).
      //2.) If yes, we need to send these structs to the corresponding
      //queues. We need to do this always, because these structs are used
      //by all handlers.
      //3.) the generalConfig_t struct might be modified too. We
      //call configUpdate(), but only if there are no more commands
      //remaining.
      if(retvalparser == SUCCESS)
      {
        //now send all VBs. They are checked for data in the helper.
        sendVBCmd(&vbaction,requestVBUpdate | (0x80),commandBuffer,1);
        //check if a general action is required
        if(general.cmd[0] != 0)
        {
			//we will wait for 10 ticks maximum, this command should 
			//not be discarded
			xQueueSend(hid_usb,&general,10);
		}
        
        //HID related
        sendHIDCmd(&mouse,requestVBUpdate | (0x80),commandBuffer,1);
        sendHIDCmd(&mouseD,requestVBUpdate | (0x80),commandBuffer,0);
        sendHIDCmd(&mouseR,requestVBUpdate,commandBuffer,0);
        sendHIDCmd(&joystick,requestVBUpdate | (0x80),commandBuffer,1);
        sendHIDCmd(&joystickR,requestVBUpdate,commandBuffer,0);
        //we need to reset requestVBUpdate to VB_SINGLESHOT
        //in the case the processed command here was NOT "AT BM"
        //currently no better solution as comparing the command.
        if(requestBM != 0)
        {
          ESP_LOGD(LOG_TAG,"Got an BM request, not resetting VB now.");
          requestBM = 0;
        } else {
          ESP_LOGD(LOG_TAG,"Resetting to VB_SINGLESHOT");
          requestVBUpdate = VB_SINGLESHOT;
        }
      }
      
      //free used buffer (MANDATORY here!), only if valid
      if(commandBuffer != NULL) free(commandBuffer);

      //if we have processed all commands (queue is empty),
      //we set the corresponding flag
      //check if there are still elements in the queue
      if(uxQueueMessagesWaiting(halSerialATCmds) == 0)
      {
        //no more commands, ready to update config
        if(configUpdate(20) != ESP_OK) ESP_LOGE(LOG_TAG,"Error updating general config!");
        else ESP_LOGD(LOG_TAG,"requesting config update");
        //yeah, processed everything, tell it to the whole firmware :-)
        xEventGroupSetBits(systemStatus,SYSTEM_EMPTY_CMD_QUEUE);
      }
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
  halStorageStore(tid,"\n",250);

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
  
  ///TODO: should be possible to do this following stuff with a nice loop over the command array?!?
  
  sprintf(outputstring,"AT AX %d\n",currentcfg->adc.sensitivity_x);
  halStorageStore(tid,outputstring,250);
  sprintf(outputstring,"AT AY %d\n",currentcfg->adc.sensitivity_y);
  halStorageStore(tid,outputstring,250);
  sprintf(outputstring,"AT DX %d\n",currentcfg->adc.deadzone_x);
  halStorageStore(tid,outputstring,250);
  sprintf(outputstring,"AT DY %d\n",currentcfg->adc.deadzone_y);
  halStorageStore(tid,outputstring,250);
  
  sprintf(outputstring,"AT MS %d\n",currentcfg->adc.max_speed);
  halStorageStore(tid,outputstring,250);
  sprintf(outputstring,"AT AC %d\n",currentcfg->adc.acceleration);
  halStorageStore(tid,outputstring,250);
  sprintf(outputstring,"AT TS %d\n",currentcfg->adc.threshold_sip);
  halStorageStore(tid,outputstring,250);
  sprintf(outputstring,"AT TP %d\n",currentcfg->adc.threshold_puff);
  halStorageStore(tid,outputstring,250);
  sprintf(outputstring,"AT WS %d\n",currentcfg->wheel_stepsize);
  halStorageStore(tid,outputstring,250);
  sprintf(outputstring,"AT SP %d\n",currentcfg->adc.threshold_strongpuff);
  halStorageStore(tid,outputstring,250);
  sprintf(outputstring,"AT SS %d\n",currentcfg->adc.threshold_strongsip);
  halStorageStore(tid,outputstring,250);
  
  switch(currentcfg->adc.mode)
  {
    case MOUSE: sprintf(outputstring,"AT MM 1\n"); break;
    case JOYSTICK: sprintf(outputstring,"AT MM 2\n"); break;
    case THRESHOLD: sprintf(outputstring,"AT MM 0\n"); break;
    case NONE: sprintf(outputstring,"AT MM 3\n"); break;
  }
  halStorageStore(tid,outputstring,250);
  
  sprintf(outputstring,"AT RO %d\n",currentcfg->adc.orientation);
  halStorageStore(tid,outputstring,250);
  sprintf(outputstring,"AT FB %d\n",currentcfg->feedback);
  halStorageStore(tid,outputstring,250);
  
  
  //return: 0 if nothing is active, 1 for USB only, 2 for BLE only, 3 for both
  uint8_t btret = 0;
  if(currentcfg->ble_active != 0) btret+=2;
  if(currentcfg->usb_active != 0) btret+=1;
  sprintf(outputstring,"AT BT %d\n",btret);
  halStorageStore(tid,outputstring,250);
      
  //iterate over all possible VBs.
  for(uint8_t j = 0; j<VB_MAX; j++)
  {
    //print AT BM (button mode) command first
    sprintf(outputstring,"AT BM %02d\n",j);
    ESP_LOGD(LOG_TAG,"AT BM %02d",j);
    halStorageStore(tid,outputstring,250);
    //try to parse command either via HID or VB task
    if(handler_hid_getAT(outputstring,j) != ESP_OK)
    {
      if(handler_vb_getAT(outputstring,j) != ESP_OK)
      {
        //if no command was found, this usually means this one is not used.
        sprintf(outputstring,"AT NC");
      }
    }
    //store reverse parsed at string
    halStorageStore(tid,outputstring,250);
    halStorageStore(tid,"\n",250);
    ESP_LOGD(LOG_TAG,"%s",outputstring);
  }

  //release storage
  free(outputstring);
  halStorageFinishTransaction(tid);
  tid = 0;
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
  xTaskCreate(task_commands, "cmdtask", TASK_COMMANDS_STACKSIZE, NULL, TASK_COMMANDS_PRIORITY, &currentCommandTask);
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


/** @brief Main parser
 * 
 * This parser is called with one finished (and 0-terminated line).
 * It does following steps:
 * * Parameter checking (empty target pointer, empty data string)
 * * Input length checking (should be at least the prefix)
 * * If the prefix is the only data, it will return a special case
 * * Searching through the constant command array
 * * If a match is found, the command parameter structure is checked
 * * Validating input values against the given ranges
 * * Executing the handler (if it is != NULL) or modifying the target struct
 * 
 * @note This is part of an external project, see https://gitlab.com/ba.1150/cmd_parser_esp32
 * @return See cmd_retval. SUCCESS on success.
 */
cmd_retval cmdParser(char * data, generalConfig_t *target)
{
    uint32_t len; //length of input string, 
    esp_err_t retval = ESP_FAIL; //return value of handler
    uint16_t i; //general index
    uint16_t matchedcmds = 0; //count of matched&executed commands
    
    //1.) check for valid pointers
    if(data == NULL) return ESP_FAIL;
    if(target == NULL) return POINTERERROR;
    
    //2.) check if this string is terminated
    //iterate over the string, as long as we don't reach max size or
    //find a \0
    for(i = 0; i<CMD_MAXLENGTH; i++) if(data[i] == '\0') break;
    //if this iteration went until the end, this string is unterminated
    if(i == CMD_MAXLENGTH) return FORMATERROR;
    
    //3.) check for length, should be at least the prefix
    len=strnlen(data,CMD_MAXLENGTH);
    if(len==strlen(CMD_PREFIX) || len==(strlen(CMD_PREFIX)-1))
    {
        //note we check the prefix minus 1 character.
        //helps us if we have "AT" or "AT "
        if(strncasecmp(data,CMD_PREFIX,strlen(CMD_PREFIX)-1) == 0) return PREFIXONLY;
        else return FORMATERROR;
    }
    if(len < (strlen(CMD_PREFIX) + CMD_LENGTH))
    {
        //command is shorter than prefix + command
        //(e.g. "AT RO" -> "AT " 3 + "RO" 2)
        return FORMATERROR;
    }
    //4.) check for the prefix
    if(strncasecmp(data,CMD_PREFIX,strlen(CMD_PREFIX)) != 0)
    {
        //didn't got the prefix, return
        return FORMATERROR;
    }
    
    //5.) now search array of commands for a match
    //
    ESP_LOGV("cmdparser","Comparing: %s with:::",&data[strlen(CMD_PREFIX)]);
    for(uint32_t id = 0; id<(sizeof(commands) / sizeof(onecmd_t)); id++)
    {
        //we iterate over the whole array. length is determined by
        //getting size of full command array and dividing it by size of one command
        
        //compare the command strings
        ESP_LOGV("cmdparser","%s, result: %d", commands[id].name, \
            strncasecmp(&data[strlen(CMD_PREFIX)],commands[id].name,CMD_LENGTH));
        if(strncasecmp(&data[strlen(CMD_PREFIX)],commands[id].name,CMD_LENGTH) == 0)
        {
            //found that command.
            //now we want to execute this command:
            //a.) extract parameters accordingly
            //b.) we need to check the parameter(s) for validity
            //c.) execute handler / modify target data
            //d.) cleanup
            
            //possible future parameters for handlers
            //int32_t parami[2] = {0,0};
            //char* params[2] = {NULL,NULL};
            void * paramFinal[2] = {NULL,NULL};
            uint16_t offsetStart = 0;
            uint16_t offsetEnd = 0;
            char * e;
            
            ESP_LOGD("cmdparser","Found matching cmd at %d",id);
            
            //a.) do the parameter parsing/extraction for both possible active parameters.
            for(i = 0; i<2; i++)
            {
                switch(commands[id].ptype[i])
                {
                    case PARAM_NONE: break; //nothing to do here.
                    case PARAM_NUMBER:
                        //set offset accordingly:
                        //first parameter starts after prefix, command and a space
                        if(i == 0) offsetStart = strlen(CMD_PREFIX) + CMD_LENGTH + 1;
                        //second parameter: we start search first space from the end.
                        if(i == 1)
                        {
                            char* t = &data[len];
                            while(t-- != data && *t != ' ');
                            offsetStart = t-data;
                        }
                    
                        //parse the parameter for a number
                        paramFinal[i] = (void*)strtol(&data[offsetStart],&e,10);
                        //with endptr, we can check if there was a number at all
                        if(e==&data[offsetStart]) return PARAMERROR;
                        
                        ESP_LOGD("cmdparser","Param %d, int: %d",i,(int32_t)paramFinal[i]);
                        //b.) parameter check
                        if((int32_t)paramFinal[i] > commands[id].max[i] || (int32_t)paramFinal[i] < commands[id].min[i]) return PARAMERROR;
                        break;
                    case PARAM_STRING:
                        //for strings we always need the last appearing space char
                        e = &data[len];
                        while(e-- != data && *e != ' ');
                        ESP_LOGV("cmdparser","last space @%d",e-data);
                    
                        //set offset accordingly:
                        //first parameter starts after prefix, command and a space
                        if(i == 0)
                        {
                            offsetStart = strlen(CMD_PREFIX) + CMD_LENGTH + 1;
                            //end is either determined by string length
                            //if there is no space except at the beginning
                            if(e-data <= CMD_LENGTH + strlen(CMD_PREFIX)) offsetEnd = len;
                            //or it is defined by the last occuring space char
                            else offsetEnd = e-data;
                            //special case: one parameter string: don't split at spaces.
                            if(commands[id].ptype[i+1] == PARAM_NONE) offsetEnd = len;
                        }
                        //second parameter: we start from character after last space
                        if(i == 1)
                        {
                            offsetStart = e-data+1;
                            //special case: the detected space character is equal to
                            //the space between cmd and parameter ->
                            //we don't want to include the previous int
                            //-> abort
                            if(offsetStart == strlen(CMD_PREFIX)+CMD_LENGTH+1) return PARAMERROR;
                            //another special case: first a int, then a string.
                            //don't split the string at spaces, especially not from back to front
                            if(commands[id].ptype[i-1] == PARAM_NUMBER)
                            {
                                offsetStart = strlen(CMD_PREFIX)+CMD_LENGTH+1;
                                while(data[offsetStart] != '\0' && data[offsetStart++] != ' ');
                            }
                            offsetEnd = len;
                        }
                        ESP_LOGV("cmdparser","str from %d to %d",offsetStart,offsetEnd);
                        
                        //b.) param check
                        if((offsetEnd - offsetStart) > commands[id].max[i] || \
                            (offsetEnd - offsetStart) < commands[id].min[i]) return PARAMERROR;
                        
                        //allocate memory for this string
                        paramFinal[i] = malloc(offsetEnd - offsetStart+1);
                        if(paramFinal[i] == NULL) return FORMATERROR;
                        //save the string pointer.
                        //Note: no \0  included here.
                        strncpy(paramFinal[i],&data[offsetStart],offsetEnd-offsetStart);
                        //terminate the new string
                        ((char*)paramFinal[i])[offsetEnd-offsetStart] = '\0';
                        ESP_LOGD("cmdparser","Param %d, str: %s",i,(char*)paramFinal[i]);
                        break;
                }
            }
            
            //c.) Now we either execute the handler or modify data
            if(commands[id].handler == NULL)
            {
                //cast the parsed data into the given target type
                //note: we check for each size individually if we
                //write only within target
                size_t length = 0;
                
                switch(commands[id].type)
                {
                    case UINT8: 
                    case INT8: 
                        length = 1;
                        retval = ESP_OK;
                        break;
                    case UINT16: 
                    case INT16: 
                        length = 2;
                        retval = ESP_OK;
                        break;
                    case UINT32: 
                    case INT32: 
                        length = 4;
                        retval = ESP_OK;
                        break;
                    case NOCAST: break; //should not be here. No handler and no given casting...
                }
                //if we found a match, try to copy
                if(retval == ESP_OK)
                {
                    //check for limits
                    if(commands[id].offset > sizeof(CMD_TARGET_TYPE)-length) retval = ESP_FAIL;
                    else memcpy(&(((uint8_t *)target)[commands[id].offset]),&paramFinal[0],length);
                }
            } else retval = commands[id].handler(data,paramFinal[0],paramFinal[1]);
            
            //d.) cleanup (free allocated strings)
            if(commands[id].ptype[0] == PARAM_STRING && paramFinal[0] != NULL) free(paramFinal[0]);
            if(commands[id].ptype[1] == PARAM_STRING && paramFinal[1] != NULL) free(paramFinal[1]);
            matchedcmds++;
            
            //stop if the handler was not successful
            if((commands[id].handler != NULL) && (retval != ESP_OK)) return HANDLERERROR;
            //or if had some kind of pointer error
            if((commands[id].handler == NULL) && (retval != ESP_OK)) return POINTERERROR;
        }
        //if no match, continue search
    }
    ESP_LOGD("cmdparser","Searched %d commands, found %d",(sizeof(commands) / sizeof(onecmd_t)),matchedcmds);
    if(matchedcmds != 0) return SUCCESS;
    else return NOCOMMAND;
}
