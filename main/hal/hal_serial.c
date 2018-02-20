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

/** @file
 * @brief CONTINOUS TASK - This module contains the hardware abstraction for the serial
 * interface to the USB support chip.
 * 
 * 
 * Attention: this module does NOT use UART0 (terminal & program interface)
 * 
 * The hardware abstraction takes care of: <br>
 * -) sending USB-HID commands <br>
 * -) sending USB-serial responses <br>
 * -) sending/receiving serial data (to/from USB-serial) <br>
 * 
 * The interaction to other parts of the firmware consists of:<br>
 * -) pending on queues for USB commands:<br>
 *    * keyboard_usb_press<br>
 *    * keyboard_usb_release<br>
 *    * mouse_movement_usb<br>
 *    * joystick_movement_usb<br>
 * -) halSerialSendUSBSerial / halSerialReceiveUSBSerial for direct sending/receiving<br>
 * 
 * The received serial data can be used by different modules, currently
 * the task_commands_cim is used to fetch & parse serial data.
 * 
 * @note In this firmware, there are 3 pins routed to the USB support chip (RX,TX,signal).
 * Depending on the level of the signal line, data sent to the USB chip is either
 * interpreted as USB-HID commands (keyboard, mouse or joystick reports) or
 * the data is forwarded to the USB-CDC interface, which is used to send data 
 * to the host (config GUI, terminal, AsTeRICS,...).
 * 
 */ 
 
 
#include "hal_serial.h"

#define LOG_TAG "hal_serial"
#define HAL_SERIAL_TASK_STACKSIZE 2048

/** @brief Ticks until the UART receive function has a timeout and gives back received data.*
 * 
 * This value should be adjusted, either if the maximum length of a transmitted command
 * OR the baudrate is changed.
 * If you adjust the
 * 
 * Currently, the value 30ms reflects 115k2 baud and 256 bytes for one AT command:
 * * ~87us are necessary for 1 byte (115k2 baud + 10 bit times) <br>
 * * ~22ms are necessary for maximum command length
 * 
 * -> next bigger value reflectable in ticks is 30ms.
 * 
 * @see ATCMD_LENGTH
 * @todo Documentation updates, this define is now used differently...
 */
#define HAL_SERIAL_UART_TIMEOUT_MS 5000

static const int BUF_SIZE_RX = 512;
/** @brief Length of queue for AT commands
 * @note A maximum of CMDQUEUE_SIZE x ATCMD_LENGTH will be allocated. 
 * @see cmds
 * */
static const int CMDQUEUE_SIZE = 16;
static const int BUF_SIZE_TX = 512;
static uint8_t keycode_modifier;
static uint8_t keycode_arr[8] = {'K',0,0,0,0,0,0,0};
/** @brief Queue for parsed AT commands
 * 
 * This queue is read by halSerialReceiveUSBSerial (which receives
 * from the queue and frees the memory afterwards).
 * AT commands are sent by halSerialRXTask, which parses each byte.
 * @note Pass structs of type atcmd_t, no pointer!
 * @see halSerialRXTask
 * @see halSerialReceiveUSBSerial
 * @see CMDQUEUE_SIZE
 * @see atcmd_t
 * */
QueueHandle_t cmds;

typedef struct atcmd {
  uint8_t *buf;
  uint16_t len;
} atcmd_t;

/** mutex for sending to serial port. Used to avoid wrong set signal pin
 * on different UART sent bytes (either USB-HID or USB-Serial)
 * */
static SemaphoreHandle_t serialsendingsem;

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
  static uint8_t current_level = 0;
    //check if we are currently on target channel/level
    if(current_level != target_level)
    {
      //wait for any previously sent transmission to finish
      if(uart_wait_tx_done(HAL_SERIAL_UART, ticks_to_wait) != ESP_OK)
      {
        ESP_LOGE(LOG_TAG,"tx_done not finished on time");
        return ESP_FAIL;
      }
      ESP_LOGI(LOG_TAG,"Switching USB chip from %d to %d (0 HID, 1 CDC)", \
        current_level,target_level);
      //set GPIO to route data to USB-Serial
      gpio_set_level(HAL_SERIAL_SWITCHPIN, target_level);
      current_level = target_level;
    }
    return ESP_OK;
}


/** @brief Flush Serial RX input buffer */
void halSerialFlushRX(void)
{
  uart_flush(HAL_SERIAL_UART);
}

/** @brief Event task for pattern (\n) detection
 * 
 * @todo write docs new
 * */
static void halSerialRXTask(void *pvParameters)
{
  uint16_t cmdoffset;
  uint8_t *buf;
  uint8_t data;
  uint8_t parserstate = 0;
  atcmd_t currentcmd;
    
  while(1)
  {
    int len = uart_read_bytes(HAL_SERIAL_UART, &data, 1, portMAX_DELAY);
    
    //only parse if a valid byte is received
    if(len == 1)
    {
      switch(parserstate)
      {
        case 0:
          //check for leading "A"
          if(data == 'A' || data == 'a') 
          { 
            ///@todo When CIM mode is active, switch back here to AT mode
            parserstate++; 
          }
          //if starting with '@', go to CIM mode.
          if(data == '@')
          {
            ///@todo Activate CIM mode here.
            ESP_LOGW(LOG_TAG,"CIM mdoe currently unsupported!");
          }
        break;
        
        
        case 1:
          //check for second 'T'/'t'
          if(data == 'T' || data == 't')
          {
            parserstate++;
            //reset command offset to beginning (but after "AT")
            cmdoffset = 2;
            //allocate new buffer for command & save "AT"
            //free is either done in error case or in halSerialReceiveUSBSerial.
            buf = malloc(ATCMD_LENGTH*sizeof(uint8_t));
            if(buf != NULL)
            {
              buf[0] = 'A';
              buf[1] = 'T';
            } else {
              ESP_LOGE(LOG_TAG,"Cannot allocate buffer for command");
              parserstate = 0;
            }
          } else {
            //reset parser of no leading "AT" is detected
            parserstate = 0;
          }
        break;
        
        
        case 2:
          //now read data until we reach \r or \n
          if(data == '\r' || data == '\n')
          {
            //terminate string
            buf[cmdoffset] = 0;
            //send buffer to queue
            currentcmd.buf = buf;
            currentcmd.len = cmdoffset+1;
            if(cmds != NULL)
            {
              if(xQueueSend(cmds,(void*)&currentcmd,10) != pdTRUE)
              {
                ESP_LOGE(LOG_TAG,"AT cmd queue is full, cannot send cmd");
                free(buf);
              } else {
                ESP_LOGI(LOG_TAG,"Sent AT cmd with len %d to queue",cmdoffset);
              }
            } else {
              ESP_LOGE(LOG_TAG,"AT cmd queue is NULL, cannot send cmd");
              free(buf);
            }
            parserstate = 0;
            break;
          }
          
          //if everything is fine, just save this byte.
          buf[cmdoffset] = data;
          cmdoffset++;
          
          //check for memory length
          if(cmdoffset == ATCMD_LENGTH)
          {
            ESP_LOGW(LOG_TAG,"AT cmd too long, discarding");
            free(buf);
            parserstate = 0;
          }
        break;
        
        default:
          ESP_LOGE(LOG_TAG,"Unknown parser state");
          parserstate = 0;
        break;
      }
    }
  }
  
  //we should never be here...
  vTaskDelete(NULL);
}

void halSerialTaskKeyboardPress(void *param)
{
  uint16_t rxK = 0;
  uint8_t keycode;
  uint8_t modifier;
    
  while(1)
  {
    //check if queue is created
    if(keyboard_usb_press != 0)
    {
      //pend on MQ, if timeout triggers, just wait again.
      if(xQueueReceive(keyboard_usb_press,&rxK,300))
      {
        //data here, split into modifier & keycode
        keycode = rxK & 0x00FF;
        modifier = (rxK & 0xFF00)>>8;
        
        //add modifier to global mask
        keycode_modifier |= modifier;
        
        //keycodes, add directly to the keycode array
        switch(add_keycode(keycode,&(keycode_arr[2])))
        {
          case 0:
            keycode_arr[0] = 'K';
            keycode_arr[1] = keycode_modifier;
            uint8_t sent = 0;
            for(uint8_t i = 0; i<8; i++) { sent+= halSerialSendUSBSerial(HAL_SERIAL_TX_TO_HID,(char *)&keycode_arr[i], 1, 30); }
            if(sent != 8) ESP_LOGE(LOG_TAG,"Error sending keyboard HID report");
              
            ESP_LOGD(LOG_TAG,"HID report (on press):");
            esp_log_buffer_hex(LOG_TAG,keycode_arr,8);
            break;
          case 1: ESP_LOGW(LOG_TAG,"keycode already in queue"); break;
          case 2: ESP_LOGE(LOG_TAG,"no space in keycode arr!"); break;
          default: ESP_LOGE(LOG_TAG,"add_keycode return unknown code..."); break;
        }
        
        modifier = 0;
        keycode = 0;
        rxK = 0;
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
  uint8_t modifier;
    
  while(1)
  {
    //check if queue is created
    if(keyboard_usb_release != 0)
    {
      //pend on MQ, if timeout triggers, just wait again.
      if(xQueueReceive(keyboard_usb_release,&rxK,300))
      {
        //data here, split into modifier & keycode
        keycode = rxK & 0x00FF;
        modifier = (rxK & 0xFF00)>>8;
        
        //remove modifier from global mask
        keycode_modifier &= ~modifier;
        
        //keycodes, remove directly from the keycode array
        switch(remove_keycode(keycode,&(keycode_arr[2])))
        {
          case 0:
            keycode_arr[0] = 'K';
            //use global modifier but mask out this released modifier...
            keycode_arr[1] = keycode_modifier;
            uint8_t sent = 0;
            for(uint8_t i = 0; i<8; i++) { sent+= halSerialSendUSBSerial(HAL_SERIAL_TX_TO_HID,(char *)&keycode_arr[i], 1, 30); }
            if(sent != 8) ESP_LOGE(LOG_TAG,"Error sending keyboard HID report");
            ESP_LOGD(LOG_TAG,"HID report (on release):");
            esp_log_buffer_hex(LOG_TAG,keycode_arr,8);
            break;
          case 1: ESP_LOGW(LOG_TAG,"keycode not in queue"); break;
          default: ESP_LOGE(LOG_TAG,"remove_keycode return unknown code..."); break;
        }
        
        modifier = 0;
        keycode = 0;
        rxK = 0;
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
  memset(&emptyReport,0,sizeof(mouse_command_t));
  
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
          uint8_t sent = 0;
          char m = 'M';
          sent += halSerialSendUSBSerial(HAL_SERIAL_TX_TO_HID,&m,1,30); 
          sent += halSerialSendUSBSerial(HAL_SERIAL_TX_TO_HID,(char *)&rxM.buttons,1,30); 
          sent += halSerialSendUSBSerial(HAL_SERIAL_TX_TO_HID,(char *)&rxM.x,1,30); 
          sent += halSerialSendUSBSerial(HAL_SERIAL_TX_TO_HID,(char *)&rxM.y,1,30); 
          sent += halSerialSendUSBSerial(HAL_SERIAL_TX_TO_HID,(char *)&rxM.wheel,1,30); 
          
          if(sent != 5)
          {
            ESP_LOGE(LOG_TAG,"Error sending report:");
          }
          ESP_LOGD(LOG_TAG,"Mouse: %d,%d,%d,%d",rxM.buttons, \
              rxM.x, rxM.y, rxM.wheel);
          //after one non-empty report, we can resend an empty one
          emptySent = 0;
          //delay for 1 tick, allowing the USB chip to parse the cmd
          vTaskDelay(2);
        } else {
          //if empty, check if it was sent once, if not: send empty
          if(emptySent == 0)
          {
            uint8_t sent = 0;
            char m = 'M';
            sent += halSerialSendUSBSerial(HAL_SERIAL_TX_TO_HID,&m,1,30); 
            sent += halSerialSendUSBSerial(HAL_SERIAL_TX_TO_HID,(char *)&rxM.buttons,1,30); 
            sent += halSerialSendUSBSerial(HAL_SERIAL_TX_TO_HID,(char *)&rxM.x,1,30); 
            sent += halSerialSendUSBSerial(HAL_SERIAL_TX_TO_HID,(char *)&rxM.y,1,30); 
            sent += halSerialSendUSBSerial(HAL_SERIAL_TX_TO_HID,(char *)&rxM.wheel,1,30); 
            
            if(sent != 5)
            {
              ESP_LOGE(LOG_TAG,"Error sending report:");
            }

            ESP_LOGD(LOG_TAG,"Mouse: %d,%d,%d,%d",rxM.buttons, \
                rxM.x, rxM.y, rxM.wheel);
            //remember we sent an empty report
            emptySent = 1;
            //delay for 1 tick, allowing the USB chip to parse the cmd
            vTaskDelay(1);
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
 * @see HAL_SERIAL_UART_TIMEOUT_MS
 * @see cmds
 * */
int halSerialReceiveUSBSerial(uint8_t *data, uint32_t length)
{
  atcmd_t recv;
  if(xQueueReceive(cmds,&recv,HAL_SERIAL_UART_TIMEOUT_MS / portTICK_PERIOD_MS))
  {
    //test for valid buffer
    if(recv.buf == NULL)
    {
      ESP_LOGW(LOG_TAG,"buffer is null?!?");
      return -1;
    }
    
    //test if we have enough memory
    if(length < recv.len) 
    {
      //if no, free memory, warn and return
      free(recv.buf);
      ESP_LOGW(LOG_TAG,"not enough buffer provided for halSerialReceiveUSBSerial");
      return -1;
    }
    //save to caller
    memcpy(data,recv.buf,sizeof(uint8_t)*recv.len);
    //free buffer
    free(recv.buf);
    return recv.len;
  } else {
    //no cmd received
    ESP_LOGD(LOG_TAG,"Timeout reading UART");
    return -1;
  }
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
  //check if sem is already initialized
  if(serialsendingsem == NULL)
  {
    ESP_LOGW(LOG_TAG,"Sem not ready, waiting for 1s");
    vTaskDelay(1000/portTICK_PERIOD_MS);
  }
  
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

/** @brief Reset the serial HID report data
 * 
 * Used for slot/config switchers.
 * It resets the keycode array and sets all HID reports to 0
 * (release all keys, avoiding sticky keys on a config change) 
 * @param exceptDevice if you want to reset only a part of the devices, set flags
 * accordingly:
 * 
 * (1<<0) excepts keyboard
 * (1<<1) excepts joystick
 * (1<<2) excepts mouse
 * If nothing is set (exceptDevice = 0) all are reset
 * */
void halSerialReset(uint8_t exceptDevice)
{
  ESP_LOGD(LOG_TAG,"BLE reset reports");
  //reset mouse
  if(!(exceptDevice & (1<<2))) 
  {
    uint8_t sent = 0;
    char m = 'M';
    uint8_t zero = 0;
    int8_t zeroint = 0;
    sent += halSerialSendUSBSerial(HAL_SERIAL_TX_TO_HID,&m,1,30); 
    sent += halSerialSendUSBSerial(HAL_SERIAL_TX_TO_HID,(char *)&zero,1,30); 
    sent += halSerialSendUSBSerial(HAL_SERIAL_TX_TO_HID,(char *)&zeroint,1,30); 
    sent += halSerialSendUSBSerial(HAL_SERIAL_TX_TO_HID,(char *)&zeroint,1,30); 
    sent += halSerialSendUSBSerial(HAL_SERIAL_TX_TO_HID,(char *)&zeroint,1,30); 
    
    if(sent != 5)
    {
      ESP_LOGE(LOG_TAG,"Error resetting mouse HID report");
    }
  }
  //reset keyboard
  if(!(exceptDevice & (1<<0)))
  {
    for(uint8_t i=0;i<8;i++) keycode_arr[i] = 0;
    keycode_modifier = 0;
    keycode_arr[0] = 'K';
    if(halSerialSendUSBSerial(HAL_SERIAL_TX_TO_HID,(char *)keycode_arr, 8, 30) != 8)
    {
      ESP_LOGE(LOG_TAG,"Error resetting keyboard HID report");
    }
  }
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
  if(ret != ESP_OK) 
  {
    ESP_LOGE(LOG_TAG,"UART param config failed"); 
    return ret;
  }
  
  //set IO pins
  ret = uart_set_pin(HAL_SERIAL_UART, HAL_SERIAL_TXPIN, HAL_SERIAL_RXPIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  if(ret != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"UART set pin failed"); 
    return ret;
  }
  
  //Install UART driver with RX and TX buffers
  ret = uart_driver_install(HAL_SERIAL_UART, BUF_SIZE_RX, BUF_SIZE_TX, 0, NULL, 0);
  if(ret != ESP_OK) 
  {
    ESP_LOGE(LOG_TAG,"UART driver install failed"); 
    return ret;
  }
  
  //Setup GPIO for UART sending direction (either use UART data to drive
  //USB HID or send UART data USB-serial)
  ret = gpio_set_direction(HAL_SERIAL_SWITCHPIN,GPIO_MODE_OUTPUT);
  if(ret != ESP_OK) 
  {
    ESP_LOGE(LOG_TAG,"UART GPIO signal set direction failed"); 
    return ret;
  }

  //create mutex for all tasks sending to serial TX queue.
  //avoids splitting of different packets due to preemption
  serialsendingsem = xSemaphoreCreateMutex();
  if(serialsendingsem == NULL) 
  {
    ESP_LOGE(LOG_TAG,"Cannot create semaphore for TX"); 
    return ESP_FAIL;
  }

  //create the AT command queue
  cmds = xQueueCreate(CMDQUEUE_SIZE,sizeof(atcmd_t));
  
  //Set uart pattern detect function.
  //uart_enable_pattern_det_intr(HAL_SERIAL_UART, '\r', 1, 10000, 10, 10);
  //Reset the pattern queue length to record at most 10 pattern positions.
  //uart_pattern_queue_reset(HAL_SERIAL_UART, EVENTQUEUE_SIZE);
  
  //install serial tasks (4x -> keyboard press/release; mouse; joystick)
  xTaskCreate(halSerialTaskKeyboardPress, "serialKbdPress", HAL_SERIAL_TASK_STACKSIZE, NULL, configMAX_PRIORITIES-1, NULL);
  xTaskCreate(halSerialTaskKeyboardRelease, "serialKbdRelease", HAL_SERIAL_TASK_STACKSIZE, NULL, configMAX_PRIORITIES-1, NULL);
  xTaskCreate(halSerialTaskMouse, "serialMouse", HAL_SERIAL_TASK_STACKSIZE, NULL, configMAX_PRIORITIES-1, NULL);
  xTaskCreate(halSerialTaskJoystick, "serialJoystick", HAL_SERIAL_TASK_STACKSIZE, NULL, configMAX_PRIORITIES-1, NULL);
  
  //Create a task to handler UART event from ISR
  xTaskCreate(halSerialRXTask, "serialRX", HAL_SERIAL_TASK_STACKSIZE, NULL, configMAX_PRIORITIES-1, NULL);
  
  //set GPIO to route data to USB-HID
  gpio_set_level(HAL_SERIAL_SWITCHPIN, 0);

  //everything went fine
  ESP_LOGI(LOG_TAG,"Driver installation complete");
  return ESP_OK;
}
