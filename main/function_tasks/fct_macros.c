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
 * @brief FUNCTION - Execute macros
 * 
 * This module is used for sending macros to the command parser.
 * 
 * Macros in context of FLipMouse/FABI are a string of concatenated
 * AT commands, equal to normal AT commands sent by the host.
 * 
 * @note AT command separation is done by a semicolon (';')
 * @warning If you want to write the semicolon within a macro, use AT KP with the corresponding keycode!
 */

#include "fct_macros.h"

/** @brief Logging tag for this module */
#define LOG_TAG "macro"


/**@brief FUNCTION - Macro execution
 * 
 * This function is used to trigger macro on a VB action.
 * 
 * @param param Macro command string
 * @return ESP_OK on success, ESP_FAIL otherwise
 * */
esp_err_t fct_macro(char *param)
{
  //check for param struct
  if(param == NULL)
  {
    ESP_LOGE(LOG_TAG,"param is NULL ");
    return ESP_FAIL;
  }
  
  int offset = 0;
  int start = 0;
  
  while(offset < ATCMD_LENGTH)
  {
    //end loop if terminators are detected.
    if(param[offset] == '\r' || param[offset] == '\n') break;
    
    //do we reach a command terminator?
    if(param[offset] == ';')
    {
      //check if this character is escaped:
      if(offset > 0)
      {
        if(param[offset-1] == '\\')
        {
          //if it escaped "\;", just continue.
          offset++;
          continue;
        }
      }
      
      //check if we hit an AT WA (wait)
      if(memcmp(&param[start],"AT WA",5) == 0)
      {
        //if yes, delay this task.
        uint32_t time = strtol((char*)&(param[start+6]),NULL,10);
        if(time < 30000)
        {
          vTaskDelay(time / portTICK_PERIOD_MS);
          ESP_LOGD(LOG_TAG,"Waiting: %d ms",time);
        } else {
          ESP_LOGE(LOG_TAG,"Hit AT WA with a delay time too high: %d",time);
        }
      } else {
        //if not an AT WA, allocate memory and send to queue.
        atcmd_t command;
        uint16_t length = offset-start;
        uint8_t *buffer = malloc(sizeof(uint8_t)*(length+1));
        if(buffer != NULL)
        {
          //copy data
          memcpy(buffer,&param[start],length);
          //terminate
          buffer[length] = 0;
          //save to queue struct
          command.buf = buffer;
          command.len = length;
          ESP_LOGD(LOG_TAG,"Sent AT cmd: %s",buffer);
          //send to queue, wait maximum 10 ticks (100ms) for a free space.
          if(xQueueSend(halSerialATCmds,(void*)&command,10) != pdTRUE)
          {
            ESP_LOGE(LOG_TAG,"Cmd queue is full, cannot send command");
          }
        } else {
          ESP_LOGE(LOG_TAG,"Cannot allocate memory for command!");
        }
      }
      
      //save new start position for next command
      start = offset + 1;
    }
    
    //go to next character
    offset++;
  }
  
  return ESP_OK;
}
