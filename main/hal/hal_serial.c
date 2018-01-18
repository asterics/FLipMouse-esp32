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
 * This header file contains the hardware abstraction for the serial
 * interface to the USB support chip.
 * Attention: this module does NOT use UART0 (terminal & program interface)
 * 
 * The hardware abstraction takes care of:
 * -) sending USB-HID commands
 * -) sending USB-serial responses
 * -) sending/receiving serial data (to/from USB-serial)
 * 
 * The interaction to other parts of the firmware consists of:
 * -) pending on queues for USB commands:
 *    * keyboard_usb_press
 *    * keyboard_usb_release
 *    * mouse_movement_usb
 *    * joystick_movement_usb
 * -) halSerialSendUSBSerial / halSerialReceiveUSBSerial for direct sending/receiving
 * 
 * The received serial data can be used by different modules, currently
 * the task_commands_cim is used to fetch & parse serial data
 */ 
 
 
#include "hal_serial.h"

#define LOG_TAG "hal_serial"
#define HAL_SERIAL_TASK_STACKSIZE 512
#define HAL_SERIAL_RECEIVE_TASK_STACKSIZE 1024

static const int BUF_SIZE_RX = 512;
static const int BUF_SIZE_TX = 512;
static uint8_t keycode_modifier;
static uint8_t keycode_deadkey_first;
static uint8_t keycode_arr[8] = {'K',0,0,0,0,0,0,0};

/** mutex for sending to serial port. Used to avoid wrong set signal pin
 * on different UART sent bytes (either USB-HID or USB-Serial)
 * */
SemaphoreHandle_t serialsendingsem;

/** Utility function, changing UART TX channel (HID or Serial)
 * 
 * This function changes the gpio pin, which is used to determine
 * where the sent UART data should be used on the USB-companion chip.
 * Either it is directly forwarded to USB-CDC (USB serial)
 * OR
 * it is parsed as USB-HID commands.
 * 
 * To safely change the direction, this function waits for any ongoing
 * transmission and changes the pin afterwards.
 * No waiting if target_level is already activated on GPIO pin.
 * 
 * @param target_level GPIO target level to check/wait for
 * @param ticks_to_wait Maximum number of ticks to wait for finishing UART TX
 * @return ESP_OK if change was fine (or already done before), ESP_FAIL on a timeout
 * */
esp_err_t halSerialSwitchTXChannel(uint8_t target_level, TickType_t ticks_to_wait)
{
    //check if we are currently on target channel/level
    if(gpio_get_level(HAL_SERIAL_SWITCHPIN) == target_level)
    {
      //wait for any previously sent transmission to finish
      if(uart_wait_tx_done(HAL_SERIAL_UART, ticks_to_wait) != ESP_OK)
      {
        ESP_LOGE(LOG_TAG,"tx_done not finished on time");
        return ESP_FAIL;
      }
      //set GPIO to route data to USB-Serial
      gpio_set_level(HAL_SERIAL_SWITCHPIN, target_level);
    }
    return ESP_OK;
}

void halSerialTaskKeyboardPress(void *param)
{
  uint16_t rxK = 0;
  uint8_t keycode;
  uint8_t keycodesLocal[8] = {'K',0,0,0,0,0,0,0};
    
  while(1)
  {
    //check if queue is created
    if(keyboard_usb_press != 0)
    {
      //pend on MQ, if timeout triggers, just wait again.
      if(xQueueReceive(keyboard_usb_press,&rxK,300))
      {
        //data here, parse keyboard press command
        //check if this is a unicode/ascii byte or a keycode
        if((rxK & 0xFF00) == 0)
        {
          //single bytes are ascii or unicode bytes, sent to keycode parser
          keycode = parse_for_keycode((uint8_t) rxK, currentConfig.locale, &keycode_modifier, &keycode_deadkey_first);
          if(keycode != 0)
          {
            //if a deadkey is issued (no release necessary), we send the deadkey before:
            if(keycode_deadkey_first != 0)
            {
              ESP_LOGD(LOG_TAG,"Sending deadkey first: %d",keycode_deadkey_first);
              memset(&keycodesLocal[1],0,7);
              keycodesLocal[2] = keycode_deadkey_first;
              //send to device (wait maximum of 30 ticks)
              if(halSerialSendUSBSerial(HAL_SERIAL_TX_TO_HID,(char *)keycodesLocal, sizeof(keycodesLocal), 30) != ESP_OK)
              {
                ESP_LOGE(LOG_TAG,"Error sending keyboard deadkey press report");
              }
              //wait 1 tick to release key
              vTaskDelay(1); //suspend for 1 tick
              keycodesLocal[2] = 0;
              //send to device
              if(halSerialSendUSBSerial(HAL_SERIAL_TX_TO_HID,(char *)keycodesLocal, sizeof(keycodesLocal), 30) != ESP_OK)
              {
                ESP_LOGE(LOG_TAG,"Error sending keyboard deadkey release report");
              }
            }
          } //we need another byte to calculate keycode...
              
        } else {
          keycode = rxK & 0x00FF;
        }
        
        //keycodes, add directly to the keycode array
        switch(add_keycode(keycode,&(keycode_arr[2])))
        {
          case 0:
            keycode_arr[0] = 'K';
            keycode_arr[1] = keycode_modifier;
            if(halSerialSendUSBSerial(HAL_SERIAL_TX_TO_HID,(char *)keycode_arr, sizeof(keycode_arr), 30) != ESP_OK)
            {
              ESP_LOGE(LOG_TAG,"Error sending keyboard HID report");
            }
            ESP_LOGD(LOG_TAG,"HID report (on press):");
            esp_log_buffer_hex(LOG_TAG,keycode_arr,8);
            break;
          case 1: ESP_LOGW(LOG_TAG,"keycode already in queue"); break;
          case 2: ESP_LOGE(LOG_TAG,"no space in keycode arr!"); break;
          default: ESP_LOGE(LOG_TAG,"add_keycode return unknown code..."); break;
        }
        
        keycode = 0;
      }
    } else {
      ESP_LOGE(LOG_TAG,"keyboard_usb_press queue not initialized, retry in 1s");
      vTaskDelay(1000/portTICK_PERIOD_MS);
    }
  }
}

void halSerialTaskKeyboardRelease(void *param)
{
  uint16_t rxK = 0;
  uint8_t keycode;
    
  while(1)
  {
    //check if queue is created
    if(keyboard_usb_release != 0)
    {
      //pend on MQ, if timeout triggers, just wait again.
      if(xQueueReceive(keyboard_usb_release,&rxK,300))
      {
        //check if this is a unicode/ascii byte or a keycode
        if((rxK & 0xFF00) == 0)
        {
          //single bytes are ascii or unicode bytes, sent to keycode parser
          keycode = parse_for_keycode((uint8_t) rxK, currentConfig.locale, &keycode_modifier, &keycode_deadkey_first);                   
        } else {
          keycode = rxK & 0x00FF;
        }
        
        //keycodes, remove directly from the keycode array
        switch(remove_keycode(keycode,&(keycode_arr[2])))
        {
          case 0:
            keycode_arr[0] = 'K';
            keycode_arr[1] = keycode_modifier;
            if(halSerialSendUSBSerial(HAL_SERIAL_TX_TO_HID,(char *)keycode_arr, sizeof(keycode_arr), 30) != ESP_OK)
            {
              ESP_LOGE(LOG_TAG,"Error sending keyboard HID report");
            }
            ESP_LOGD(LOG_TAG,"HID report (on release):");
            esp_log_buffer_hex(LOG_TAG,keycode_arr,8);
            break;
          case 1: ESP_LOGW(LOG_TAG,"keycode not in queue"); break;
          default: ESP_LOGE(LOG_TAG,"remove_keycode return unknown code..."); break;
        }
        
        keycode = 0;
      }
    } else {
      ESP_LOGE(LOG_TAG,"keyboard_usb_release queue not initialized, retry in 1s");
      vTaskDelay(1000/portTICK_PERIOD_MS);
    }
  }
}

void halSerialTaskMouse(void *param)
{
  mouse_command_t rxM;
  static uint8_t emptySent = 0;
  mouse_command_t emptyReport = {0,0,0,0};
  uint8_t hidreport[5] = {'M',0,0,0,0};
  
  while(1)
  {
    //check if queue is created
    if(mouse_movement_usb != 0)
    {
      //pend on MQ, if timeout triggers, just wait again.
      if(xQueueReceive(mouse_movement_usb,&rxM,300))
      {
        //send report always if not empty
        if(memcmp(&rxM,&emptyReport,sizeof(mouse_command_t)) != 0)
        {
          //copy mouse command to hid report
          memcpy(&hidreport[1],&rxM,4);
          if(halSerialSendUSBSerial(HAL_SERIAL_TX_TO_HID,(char *)hidreport,sizeof(hidreport),30) != ESP_OK)
          {
            ESP_LOGE(LOG_TAG,"Error sending mouse report");
          }
          ESP_LOGD(LOG_TAG,"Mouse: %d,%d,%d,%d",rxM.buttons, \
              rxM.x, rxM.y, rxM.wheel);
          //after one non-empty report, we can resend an empty one
          emptySent = 0;
        } else {
          //if empty, check if it was sent once, if not: send empty
          if(emptySent == 0)
          {
            memcpy(&hidreport[1],&rxM,4);
            if(halSerialSendUSBSerial(HAL_SERIAL_TX_TO_HID,(char *)hidreport,sizeof(hidreport),30) != ESP_OK)
            {
              ESP_LOGE(LOG_TAG,"Error sending mouse report");
            }
            ESP_LOGD(LOG_TAG,"Mouse: %d,%d,%d,%d",rxM.buttons, \
                rxM.x, rxM.y, rxM.wheel);
            //remember we sent an empty report
            emptySent = 1;
          }
        }
      }
    } else {
      ESP_LOGE(LOG_TAG,"mouse_movement_usb queue not initialized, retry in 1s");
      vTaskDelay(1000/portTICK_PERIOD_MS);
    }
  }
}

void halSerialTaskJoystick(void *param)
{
  joystick_command_t rxJ;
  
  while(1)
  {
    //check if queue is created
    if(joystick_movement_usb != 0)
    {
      //pend on MQ, if timeout triggers, just wait again.
      if(xQueueReceive(joystick_movement_usb,&rxJ,300))
      {
        ESP_LOGE(LOG_TAG,"Joystick over USB not implemented yet");
      }
    } else {
      ESP_LOGE(LOG_TAG,"joystick_movement_usb queue not initialized, retry in 1s");
      vTaskDelay(1000/portTICK_PERIOD_MS);
    }
  }
}


/** Read serial bytes from USB-Serial (USB-CDC)
 * 
 * This method reads bytes from the UART, which receives all data
 * from USB-CDC.
 * 
 * @return -1 on error, number of read bytes otherwise
 * @param data Data to be sent
 * @param length Number of maximum bytes to read
 * @param ticks_to_wait Maximum time to wait for given data amount
 * */
int halSerialReceiveUSBSerial(uint8_t *data, uint32_t length, TickType_t ticks_to_wait)
{
  return uart_read_bytes(HAL_SERIAL_UART, data, length, ticks_to_wait);    
}

/** Send serial bytes to USB-Serial (USB-CDC)
 * 
 * This method sends bytes to the UART.
 * In addition, the GPIO signal pin is set to route this data
 * to the USB Serial instead of USB-HID.
 * After finishing this transmission, the GPIO is set back to USB-HID
 * 
 * @return -1 on error, number of read bytes otherwise
 * @param data Data to be sent
 * @param channel Determines the channel to send this data to (either CDC or HID parser)
 * @param length Number of maximum bytes to send
 * @param ticks_to_wait Maximum time to wait for a free UART
 * 
 * @see HAL_SERIAL_TX_TO_HID
 * @see HAL_SERIAL_TX_TO_CDC
 * */
int halSerialSendUSBSerial(uint8_t channel, char *data, uint32_t length, TickType_t ticks_to_wait) 
{
  //acquire mutex to have TX permission on UART
  if(xSemaphoreTake(serialsendingsem, ticks_to_wait) == pdTRUE)
  {
    //try to switch to USB-Serial
    if(halSerialSwitchTXChannel(channel,ticks_to_wait)!=ESP_OK)
    {
      ESP_LOGE(LOG_TAG,"switch GPIO channel didn't finish");
      xSemaphoreGive(serialsendingsem);
      return -1;
    }
    //send the data
    int txBytes = uart_write_bytes(HAL_SERIAL_UART, data, length);
    
    //release mutex
    xSemaphoreGive(serialsendingsem);
    
    return txBytes;
  } else return -1;
}

/** Initialize the serial HAL
 * 
 * This method initializes the serial interface & creates
 * all necessary tasks
 * */
esp_err_t halSerialInit(void)
{
  esp_err_t ret = ESP_OK;
  const uart_config_t uart_config = {
    .baud_rate = 115200,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
  };
  //update UART config
  ret = uart_param_config(HAL_SERIAL_UART, &uart_config);
  if(ret != ESP_OK) ESP_LOGE(LOG_TAG,"UART param config failed"); return ret;
  //set IO pins
  ret = uart_set_pin(HAL_SERIAL_UART, HAL_SERIAL_TXPIN, HAL_SERIAL_RXPIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  if(ret != ESP_OK) ESP_LOGE(LOG_TAG,"UART set pin failed"); return ret;
  //Install UART driver with RX and TX buffers
  ret = uart_driver_install(HAL_SERIAL_UART, BUF_SIZE_RX, BUF_SIZE_TX, 0, NULL, 0);
  if(ret != ESP_OK) ESP_LOGE(LOG_TAG,"UART driver install failed"); return ret;
  //Setup GPIO for UART sending direction (either use UART data to drive
  //USB HID or send UART data USB-serial)
  ret = gpio_set_direction(HAL_SERIAL_SWITCHPIN,GPIO_MODE_OUTPUT);
  if(ret != ESP_OK) ESP_LOGE(LOG_TAG,"UART GPIO signal set direction failed"); return ret;

  //create mutex for all tasks sending to serial TX queue.
  //avoids splitting of different packets due to preemption
  serialsendingsem = xSemaphoreCreateMutex();
  if(serialsendingsem == NULL) ESP_LOGE(LOG_TAG,"Cannot create semaphore"); return ESP_FAIL;
  
  //install serial tasks (4x -> keyboard press/release; mouse; joystick)
  xTaskCreate(halSerialTaskKeyboardPress, "serialKbdPress", HAL_SERIAL_TASK_STACKSIZE, NULL, configMAX_PRIORITIES-1, NULL);
  xTaskCreate(halSerialTaskKeyboardRelease, "serialKbdRelease", HAL_SERIAL_TASK_STACKSIZE, NULL, configMAX_PRIORITIES-1, NULL);
  xTaskCreate(halSerialTaskMouse, "serialMouse", HAL_SERIAL_TASK_STACKSIZE, NULL, configMAX_PRIORITIES-1, NULL);
  xTaskCreate(halSerialTaskJoystick, "serialJoystick", HAL_SERIAL_TASK_STACKSIZE, NULL, configMAX_PRIORITIES-1, NULL);
  
  //set GPIO to route data to USB-HID
  gpio_set_level(HAL_SERIAL_SWITCHPIN, 0);

  //everything went fine
  ESP_LOGI(LOG_TAG,"Driver installation complete");
  return ESP_OK;
}
