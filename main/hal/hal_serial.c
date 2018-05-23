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
 * The hardware abstraction takes care of:
 * * sending USB-HID commands
 * * sending USB-serial responses
 * * sending/receiving serial data (to/from USB-serial)
 * 
 * The interaction to other parts of the firmware consists of:
 * * pending on queue for USB commands: hid_usb
 * * halSerialSendUSBSerial / halSerialReceiveUSBSerial for direct sending/receiving (USB-CDC)
 * 
 * The received serial data can be used by different modules, currently
 * the task_commands is used to fetch & parse serial data.
 * 
 * @note In this firmware, there are 3 pins routed to the USB support chip (RX,TX,HID).
 * RX/TX lines are used as normal UART, which is converted by the LPC USB chip to 
 * the USB-CDC interface. The third line (HID) is used as a pulse length modulated output
 * for transmitting HID commands to the host.
 */ 
 
 
#include "hal_serial.h"

#define LOG_TAG "hal_serial"
#define HAL_SERIAL_TASK_STACKSIZE 3072

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
#define HAL_SERIAL_UART_TIMEOUT_MS 10000

static const int BUF_SIZE_RX = 512;

/** @brief Output callback
 * 
 * If this callback is != NULL, the function halSerialSendUSBSerial
 * will send data to this callback in addition to the serial interface
 * @see halSerialSendUSBSerial
 * @see serialoutput_h
 * */
serialoutput_h outputcb = NULL;

/** @brief Length of queue for AT commands
 * @note A maximum of CMDQUEUE_SIZE x ATCMD_LENGTH can be allocated (if
 * no task receives the commands)
 * @note C# GUI usually floods the input with AT cmds, set at least to 64
 * to avoid missing AT commands.
 * @see halSerialATCmds
 * */
static const int CMDQUEUE_SIZE = 256;

static const int BUF_SIZE_TX = 512;

/** @brief Mutex for sending to serial port.
 * */
static SemaphoreHandle_t serialsendingsem;
/** @brief Mutex for sending to HID */
static SemaphoreHandle_t hidsendingsem;


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
    return ESP_OK;
}


/** @brief Flush Serial RX input buffer */
void halSerialFlushRX(void)
{
  uart_flush(HAL_SERIAL_UART);
}

/** @brief UART RX task for AT command pattern detection and parsing
 * 
 * This task is used to pend on any incoming UART bytes.
 * After receiving bytes, the incoming data is put together into AT commands.
 * On a fully received AT command (terminated either by '\\r' or '\\n'),
 * the buffer will be sent to the halSerialATCmds queue.
 * 
 * @see halSerialATCmds
 * */
void halSerialRXTask(void *pvParameters)
{
  uint16_t cmdoffset;
  uint8_t *buf;
  uint8_t bufstatic[ATCMD_LENGTH];
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
            ESP_LOGW(LOG_TAG,"CIM mode currently unsupported!");
          }
        break;
        
        
        case 1:
          //check for second 'T'/'t'
          if(data == 'T' || data == 't')
          {
            parserstate++;
            //reset command offset to beginning (but after "AT")
            cmdoffset = 2;
            bufstatic[0] = 'A';
            bufstatic[1] = 'T';

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
            bufstatic[cmdoffset] = 0;
            //allocate a new buffer for this command to be processed later
            //free is either done in halSerialReceiveUSBSerial.
            buf = malloc(cmdoffset+1);
            if(buf == NULL)
            {
              ESP_LOGE(LOG_TAG,"Cannot allocate %d B buffer for new AT cmd",cmdoffset+1);
              parserstate = 0;
              break;
            }
            memcpy(buf,bufstatic,cmdoffset+1);
            
            //send buffer to queue
            currentcmd.buf = buf;
            currentcmd.len = cmdoffset+1;
            if(halSerialATCmds != NULL)
            {
              if(xQueueSend(halSerialATCmds,(void*)&currentcmd,10) != pdTRUE)
              {
                ESP_LOGE(LOG_TAG,"AT cmd queue is full, cannot send cmd");
                free(buf);
              } else {
                ESP_LOGI(LOG_TAG,"Sent AT cmd with len %d to queue: %s",cmdoffset,bufstatic);
              }
            } else {
              ESP_LOGE(LOG_TAG,"AT cmd queue is NULL, cannot send cmd");
              free(buf);
            }
            parserstate = 0;
            break;
          }
          
          //if everything is fine, just save this byte.
          bufstatic[cmdoffset] = data;
          cmdoffset++;
          
          //check for memory length
          if(cmdoffset == ATCMD_LENGTH)
          {
            ESP_LOGW(LOG_TAG,"AT cmd too long, discarding");
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

/** @brief CONTINOUS TASK - Process HID commands & send via HID wire to LPC
 * 
 * This task is used to receive a byte buffer, which contains a HID command
 * for the LPC. These bytes are converted into either long or short pulses
 * for 0 and 1 bits, finished by a "very long" stop bit.
 * 
 * HID commands are built up following this declarations:
 * Byte 0: type of command: K/M/J/C:
 * * K are keyboard reports
 * * M are mouse reports
 * * J are joystick reports
 * * C are set locale commands
 * 
 * Following parameter lenghts are valid:
 * "K": 7 Bytes; 1 modifier + 6 keycode
 * "M": 4 Bytes; Button mask + X + Y + wheel
 * "J": 13 Bytes; Please consult task_joystick.c for a detailed explanation
 * "C": 1 Byte; Locale code, according to HID specification chapter 6.2.1
 * 
 * 
 * @param param Unused
 * @see usb_command_t
 * @see hid_usb
 * @see HAL_SERIAL_HIDPIN
 * */
void halSerialHIDTask(void *param)
{
  usb_command_t rx; //TODO: use right var type
  //RMT RAM has 64x32bit memory each block
  rmt_item32_t rmtBuf[64];
  uint8_t rmtCount = 0;
  
  //levels are always same, set here.
  rmt_item32_t item;
  item.level0 = 1;
  item.level1 = 0;
  
  while(1)
  {
    //check if queue is initialized
    if(hid_usb != NULL)
    {
      //pend on MQ, if timeout triggers, just wait again.
      if(xQueueReceive(hid_usb,&rx,portMAX_DELAY))
      {
        //reset RMT offset
        rmtCount = 0;
        
        //debug output
        switch(rx.data[0])
        {
          case 'M': ESP_LOGI(LOG_TAG,"USB Mouse: B: %d, X/Y: %d/%d, wheel: %d", \
            rx.data[1],rx.data[2],rx.data[3],rx.data[4]);
            break;
          case 'K': ESP_LOGI(LOG_TAG,"USB Kbd: Mod: %d, keys: %d/%d/%d/%d/%d/%d", \
            rx.data[1],rx.data[2],rx.data[3],rx.data[4],rx.data[5],rx.data[6],rx.data[7]);
            break;
          case 'J': ESP_LOGI(LOG_TAG,"USB joystick:");
            ESP_LOG_BUFFER_HEXDUMP(LOG_TAG,&rx.data[1], 12 ,ESP_LOG_INFO);
            break;
          default: ESP_LOGW(LOG_TAG,"Unknown USB report");
        }
        
        //build RMT buffer
        for(uint8_t i = 0; i<16; i++)
        {
          //if all bytes are processed, quit this loop
          if(i==rx.len) break;
          
          //process 2 bits at once (one rmt item)
          for(uint8_t j = 0; j<4; j++)
          {
            //process even bit
            if((rx.data[i] & (1<<(j*2))) != 0)
            {
              item.duration0 = HAL_SERIAL_HID_DURATION_1; //long timing
            } else {
              item.duration0 = HAL_SERIAL_HID_DURATION_0; //short timing
            }
            //process odd bit
            if((rx.data[i] & (1<<(j*2+1))) != 0)
            {
              item.duration1 = HAL_SERIAL_HID_DURATION_1;
            } else {
              item.duration1 = HAL_SERIAL_HID_DURATION_0;
            }
            //fill buffer 
            //rmtCount = ((i*8+j*2)/2) + 1;
            rmtBuf[rmtCount] = item;
            rmtCount++;
          }
        }
        
        //signal the RMT the end of this transmission
        item.duration0 = HAL_SERIAL_HID_DURATION_S; //stop bit timing
        item.duration1 = 0;
        rmtBuf[rmtCount] = item;
        rmtCount++;
        
        ESP_LOGV(LOG_TAG,"RMT dump:");
        ESP_LOG_BUFFER_HEXDUMP(LOG_TAG,rmtBuf, sizeof(rmt_item32_t)*(rmtCount),ESP_LOG_VERBOSE);
    
        //take semaphore
        if(xSemaphoreTake(hidsendingsem,1000/portTICK_PERIOD_MS))
        {
          //put data into RMT buffer
          if(rmt_fill_tx_items(HAL_SERIAL_HID_CHANNEL, rmtBuf, \
            rmtCount, 0) != ESP_OK)
          {
            ESP_LOGE(LOG_TAG,"Cannot send data to RMT");
          }
          //start TX, reset memory index (always send from beginning)
          if(rmt_tx_start(HAL_SERIAL_HID_CHANNEL, 1) != ESP_OK)
          {
            ESP_LOGE(LOG_TAG,"Cannot start RMT TX");
          }
        } else {
          ESP_LOGE(LOG_TAG,"Timeout on HID mutex, possible error in sending");
        }
      }
    } else {
      ESP_LOGE(LOG_TAG,"joystick_movement_usb queue not initialized, retry in 1s");
      vTaskDelay(1000/portTICK_PERIOD_MS);
    }
  }
}

/** @brief Read parsed AT commands from USB-Serial (USB-CDC)
 * 
 * This method reads full AT commands from the halSerialATCmds queue.
 * 
 * @return -1 on error, number of read bytes otherwise
 * @param data Double pointer to save the new allocated buffer to.
 * @warning Free the data pointer after use!
 * @see HAL_SERIAL_UART_TIMEOUT_MS
 * @see halSerialATCmds
 * @see halSerialRXTask
 * */
int halSerialReceiveUSBSerial(uint8_t **data)
{
  atcmd_t recv;
  if(xQueueReceive(halSerialATCmds,&recv,HAL_SERIAL_UART_TIMEOUT_MS / portTICK_PERIOD_MS))
  {
    //test for valid buffer
    if(recv.buf == NULL)
    {
      ESP_LOGW(LOG_TAG,"buffer is null?!?");
      return -1;
    }
    //save buffer pointer
    *data = recv.buf;
    
    return recv.len;
  } else {
    //no cmd received
    ESP_LOGD(LOG_TAG,"Timeout reading UART");
    //print heap info
    heap_caps_print_heap_info(MALLOC_CAP_8BIT);
    return -1;
  }
}

/** @brief Send serial bytes to USB-Serial (USB-CDC)
 * 
 * This method sends bytes to the UART, for USB-CDC
 * 
 * @return -1 on error, number of read bytes otherwise
 * @param data Data to be sent
 * @param length Number of maximum bytes to send
 * @param ticks_to_wait Maximum time to wait for a free UART
 * */
int halSerialSendUSBSerial(char *data, uint32_t length, TickType_t ticks_to_wait) 
{
  //check if sem is already initialized
  if(serialsendingsem == NULL)
  {
    ESP_LOGW(LOG_TAG,"Sem not ready, waiting for 1s");
    vTaskDelay(1000/portTICK_PERIOD_MS);
  }
  
  //if an additional stream is registered, send data there as well
  //no HID commands will be sent there.
  if(outputcb != NULL)
  {
    if(outputcb(data,length) != ESP_OK)
    {
      ESP_LOGE(LOG_TAG,"Additional stream cannot be sent");
    }
  }
  
  //acquire mutex to have TX permission on UART
  if(xSemaphoreTake(serialsendingsem, ticks_to_wait) == pdTRUE)
  {
    //send the data
    int txBytes = uart_write_bytes(HAL_SERIAL_UART, data, length);
    uart_write_bytes(HAL_SERIAL_UART, HAL_SERIAL_LINE_ENDING, strnlen(HAL_SERIAL_LINE_ENDING,4));
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
  ESP_LOGD(LOG_TAG,"USB-HID reset reports, except mask: %d",exceptDevice);
  //reset mouse
  if(!(exceptDevice & (1<<2))) 
  {
    usb_command_t m;
    m.len = 5;
    m.data[0] = 'M';
    m.data[1] = 0; //buttons
    m.data[2] = 0; //X
    m.data[3] = 0; //Y
    m.data[4] = 0; //wheel
    
    //send to queue
    xQueueSend(hid_usb, &m, 0);
  }
  //reset keyboard
  if(!(exceptDevice & (1<<0)))
  {
    usb_command_t k;
    for(uint8_t i=1;i<=8;i++) k.data[i] = 0;
    k.data[0] = 'K';
    k.len = 9;
    //send to queue
    xQueueSend(hid_usb, &k, 0);
  }
  //reset joystick
  if(!(exceptDevice & (1<<1)))
  {
    usb_command_t j;
    for(uint8_t i=1;i<=12;i++) j.data[i] = 0;
    j.data[0] = 'J';
    j.len = 13;
    
    //send to queue
    xQueueSend(hid_usb, &j, 0);
  }
}


/** @brief Remove the additional function for outputting the serial data
 * 
 * This function removes the callback, which is used if the program needs
 * an additional output despite the serial interface.
*/
void halSerialRemoveOutputStream(void)
{
  outputcb = NULL;
}

/** @brief Set an additional function for outputting the serial data
 * 
 * This function sets a callback, which is used if the program needs
 * an additional output despite the serial interface. In this case,
 * the webgui registers a callback, which sends all the serial data
 * to the websocket
 * @param cb Function callback
*/
void halSerialAddOutputStream(serialoutput_h cb)
{
  outputcb = cb;
}

/** @brief CB for finished HID output (RMT), releases mutex */
void halSerialHIDFinished(rmt_channel_t channel, void *arg)
{
  if(hidsendingsem != NULL) xSemaphoreGive(hidsendingsem);
}

/** @brief Initialize the serial HAL
 * 
 * This method initializes the serial interface & creates
 * all necessary tasks.
 * 
 * @todo Due to long wires & bad cables on the dev board, the baudrate is set to 19200 here. Change back after testing.
 * */
esp_err_t halSerialInit(void)
{
  /*++++ UART config (UART to CDC) ++++*/
  
  esp_err_t ret = ESP_OK;
  const uart_config_t uart_config = {
    .baud_rate = 230400,
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
  
  //create mutex for all tasks sending to serial TX queue.
  //avoids splitting of different packets due to preemption
  serialsendingsem = xSemaphoreCreateMutex();
  if(serialsendingsem == NULL) 
  {
    ESP_LOGE(LOG_TAG,"Cannot create semaphore for TX"); 
    return ESP_FAIL;
  }
  //mutex for sending HID commands
  //acquired before sending, released in RMT finish callback
  hidsendingsem = xSemaphoreCreateMutex();
  if(hidsendingsem == NULL) 
  {
    ESP_LOGE(LOG_TAG,"Cannot create semaphore for HID"); 
    return ESP_FAIL;
  }

  //create the AT command queue
  halSerialATCmds = xQueueCreate(CMDQUEUE_SIZE,sizeof(atcmd_t));

  /*++++ RMT config (sending HID commands) ++++*/
  rmt_config_t hidoutput_rmt;
  
  hidoutput_rmt.rmt_mode = RMT_MODE_TX;  //TX mode
  hidoutput_rmt.channel = HAL_SERIAL_HID_CHANNEL; //use define RMT channel
  hidoutput_rmt.clk_div = 10; //do not divide 80MHz clock; we need to be fast...
  hidoutput_rmt.gpio_num = HAL_SERIAL_HIDPIN; //output pin
  hidoutput_rmt.mem_block_num = 1;
  hidoutput_rmt.tx_config.loop_en = 0;     //do not loop
  hidoutput_rmt.tx_config.carrier_en = 0;  //we don't need a carrier
  hidoutput_rmt.tx_config.idle_output_en = 1;
  hidoutput_rmt.tx_config.idle_level = 0;
  
  if(rmt_config(&hidoutput_rmt) != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Cannot init RMT for HID output");
    return ESP_FAIL;
  }
  
  if(rmt_driver_install(HAL_SERIAL_HID_CHANNEL,0,0) != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Cannot install driver for HID output");
    return ESP_FAIL;
  }
  
  rmt_register_tx_end_callback(halSerialHIDFinished,NULL);
  ///TODO:
  /*
   * task joystick/mouse/kbd entfernen, einen task + 1 queue machen
   * bytebuf mit Jxxxx, Kxxxx, Mxxxx in den entsprechenden tasks erzeugen
   * queue richtig initialisieren (main) */
  
  /*++++ task setup ++++*/
  //task for sending HID commands via RMT
  xTaskCreate(halSerialHIDTask, "serialHID", HAL_SERIAL_TASK_STACKSIZE, NULL, configMAX_PRIORITIES-3, NULL);
  
  //Create a task to handler UART event from ISR
  xTaskCreate(halSerialRXTask, "serialRX", HAL_SERIAL_TASK_STACKSIZE, NULL, 5, NULL);

  //everything went fine
  ESP_LOGI(LOG_TAG,"Driver installation complete");
  return ESP_OK;
}
