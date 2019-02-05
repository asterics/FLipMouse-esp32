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
 * @brief FUNCTION - Infrared remotes
 * 
 * This module is used for recording & sending of
 * infrared commands.
 * Pinning and low-level interfacing for infrared is done in hal_io.c,
 * this part here manages loading & storing the commands.
 * 
 * @see hal_io.c
 */
#include "fct_infrared.h"

/** @brief Logging tag for this module */
#define LOG_TAG "fct_IR"


/**@brief FUNCTION - Infrared command sending
 * 
 * This task is used to trigger an IR command on a VB action.
 * The IR command which should be sent is identified by a name.
 * 
 * @see taskInfraredConfig_t
 * @param param Task config
 * @see VB_SINGLESHOT
 * */
void fct_infrared_send(char* cmdName)
{
  //local IR struct
  halIOIR_t *cfg = malloc(sizeof(halIOIR_t));
  //transaction ID for IR data
  uint32_t tid;
  if(halStorageStartTransaction(&tid,20,LOG_TAG) == ESP_OK)
  {
    if(halStorageLoadIR(cmdName,cfg,tid) == ESP_OK)
    {
      //send pointer to IR send queue
      if(cfg != NULL)
      {
        ESP_LOGI(LOG_TAG,"Triggering IR cmd, length %d",cfg->count);
        SENDIRSTRUCT(cfg);
        //free config afterwards (whole config is saved to queue)
        free(cfg);
      } else {
        ESP_LOGE(LOG_TAG,"IR cfg is NULL!");
      }
      //create tone
      TONE(TONE_IR_SEND_FREQ,TONE_IR_SEND_DURATION);
    } else {
      ESP_LOGE(LOG_TAG,"Error loading IR cmd");
    }
    halStorageFinishTransaction(tid);
  } else {
    ESP_LOGE(LOG_TAG,"Error starting transaction for IR cmd");
  }
}

/** @brief FUNCTION - Trigger an IR command recording.
 * 
 * This method is used to record one infrared command.
 * It will block until either the timeout is reached or
 * a command is received.
 * The command is stored vial hal_storage.
 * 
 * @see halStorageStoreIR
 * @see TASK_HAL_IR_RECV_TIMEOUT
 * @param cmdName Name of command which will be used to store.
 * @param outputtoserial If set to !=0, the hex stream will be sent to the serial interface.
 * @return ESP_OK if command was stored, ESP_FAIL otherwise (timeout)
 * */
esp_err_t fct_infrared_record(char* cmdName, uint8_t outputtoserial)
{
  uint16_t timeout = 0;
  if(strnlen(cmdName,SLOTNAME_LENGTH) == SLOTNAME_LENGTH)
  {
    ESP_LOGE(LOG_TAG,"IR command name too long (maximum %d chars)",SLOTNAME_LENGTH);
    return ESP_FAIL;
  }
  //transaction id
  uint32_t tid;
  
  //create the IR struct
  halIOIR_t *cfg = malloc(sizeof(halIOIR_t));
  rmt_item32_t *buf = malloc(sizeof(rmt_item32_t)*TASK_HAL_IR_RECV_MAXIMUM_EDGES);
  cfg->buffer = buf;
  cfg->status = IR_RECEIVING;
  
  //check if memory was allocated
  if(buf == NULL || cfg == NULL)
  {
    ESP_LOGE(LOG_TAG,"Error allocating IR memory");
    if(buf != NULL) free(buf);
    if(cfg != NULL) free(cfg);
    return ESP_FAIL;
  }
  
  
  //put it to queue
  //we assume there is space in the queue
  xQueueSend(halIOIRRecvQueue, (void *)&cfg, (TickType_t)0);
  
  //check status every two ticks
  while(cfg->status == IR_RECEIVING) 
  {
    vTaskDelay(2);
    timeout+=2;
    if((timeout/portTICK_PERIOD_MS) > TASK_HAL_IR_RECV_TIMEOUT)
    {
      ESP_LOGW(LOG_TAG,"IR timeout waiting for status change");
      //free buffers
      free(cfg);
      free(buf);
      return ESP_FAIL;
    }
  }
  
  switch(cfg->status)
  {
    case IR_TOOSHORT:
      ESP_LOGW(LOG_TAG,"IR cmd too short");
      //free buffers
      free(cfg);
      free(buf);
      return ESP_FAIL;
    case IR_FINISHED:
      //finished, storing
      if(halStorageStartTransaction(&tid, 20,LOG_TAG) != ESP_OK)
      {
        ESP_LOGE(LOG_TAG,"Cannot start transaction");
        //free buffers
        free(cfg);
        free(buf);
        return ESP_FAIL;
      }
      if(halStorageStoreIR(tid, cfg, cmdName) != ESP_OK)
      {
        ESP_LOGE(LOG_TAG,"Cannot store IR cmd");
      }
      halStorageFinishTransaction(tid);
      //create tone
      TONE(TONE_IR_RECV_FREQ,TONE_IR_RECV_DURATION);
      //send out a hex stream, if enabled
      if(outputtoserial != 0)
      {
        char output[8*cfg->count + 4];
        for(uint16_t i = 0; i<cfg->count; i++)
        {
          sprintf(&output[i*8],"%08X\r\n",cfg->buffer[i].val);
        }
        halSerialSendUSBSerial(output,strnlen(output,8*cfg->count + 4),10);
      }
      break;
    case IR_OVERFLOW:
      ESP_LOGW(LOG_TAG,"IR cmd too long");
      //free buffers
      free(cfg);
      free(buf);
      return ESP_FAIL;
    default:
      ESP_LOGE(LOG_TAG,"Unknown IR recv status");
      //free buffers
      free(cfg);
      free(buf);
      return ESP_FAIL;
  }

  //everything fine...
  //free buffers
  free(cfg);
  free(buf);
  return ESP_OK;
}


/**@brief FUNCTION - Set the time between two IR edges which will trigger the timeout
 * (end of received command)
 * 
 * This method is used to set the timeout between two edges which is
 * used to trigger the timeout, therefore the finished signal for an
 * IR command recording.
 * 
 * @see infrared_record
 * @warning Normal NEC codes use a ~18ms start signal, so it is recommended to this value at least to 20!
 * @see generalConfig_t
 * @param timeout Timeout in [ms], 2-100
 * @return ESP_OK if parameter is set, ESP_FAIL otherwise (out of range)
 * */
esp_err_t fct_infrared_set_edge_timeout(uint8_t timeout)
{
  if(timeout > 100 || timeout < 2) return ESP_FAIL;
  
  //get config struct to store the value there.
  generalConfig_t *cfg = configGetCurrent();
  if(cfg == NULL) return ESP_FAIL;
  cfg->irtimeout = timeout;
  
  return ESP_OK;
}
