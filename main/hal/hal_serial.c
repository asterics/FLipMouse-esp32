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


#define LOG_LEVEL_SERIAL ESP_LOG_WARN

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
#define HAL_SERIAL_UART_TIMEOUT_MS 10000

/** @brief Timout for receiving ADC data via I2C
 * @see halSerialReceiveI2CADC */
#define HAL_SERIAL_I2C_TIMEOUT_MS 100

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


#define WRITE_BIT I2C_MASTER_WRITE              /** @brief I2C master write */
#define READ_BIT I2C_MASTER_READ                /** @brief I2C master read */
#define ACK_CHECK_EN 0x1                        /** @brief I2C master will check ack from slave*/
#define ACK_CHECK_DIS 0x0                       /** @brief I2C master will not check ack from slave */
#define ACK_VAL 0x0                             /** @brief I2C ack value */
#define NACK_VAL 0x1                            /** @brief I2C nack value */

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
  uint16_t cmdoffset = 0;
  uint8_t *buf;
  uint8_t *bufstatic = malloc(ATCMD_LENGTH);
  uint8_t data;
  atcmd_t currentcmd;
  
  if(bufstatic == NULL)
  {
    ESP_LOGE(LOG_TAG,"Cannot allocate mem");
    vTaskDelete(NULL);
  }
  
  while(1)
  {
    int len = uart_read_bytes(HAL_SERIAL_UART, &data, 1, portMAX_DELAY);
    
    //only parse if a valid byte is received
    if(len == 1)
    {
      //check if we got a \r or \n at the beginning
      //(which is a remaining from the previous command)
      if((data == '\r' || data == '\n') && (cmdoffset == 0))
      {
        //do nothing with it, just discard
        continue;
      }
      
      //now read data until we reach \r or \n
      if(data == '\r' || data == '\n')
      {
        //terminate string
        bufstatic[cmdoffset] = 0;
        //allocate a new buffer for this command to be processed later
        //free is done in halSerialReceiveUSBSerial.
        buf = malloc(cmdoffset+1);
        if(buf == NULL)
        {
          ESP_LOGE(LOG_TAG,"Cannot allocate %d B buffer for new AT cmd",cmdoffset+1);
          cmdoffset = 0;
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
            #if LOG_LEVEL_SERIAL >= ESP_LOG_INFO
            ESP_LOGI(LOG_TAG,"Sent AT cmd with len %d to queue: %s",cmdoffset,bufstatic);
            #endif
          }
        } else {
          ESP_LOGE(LOG_TAG,"AT cmd queue is NULL, cannot send cmd");
          free(buf);
        }
        cmdoffset = 0;
        continue;
      }
      
      //if everything is fine, just save this byte.
      bufstatic[cmdoffset] = data;
      cmdoffset++;
      
      //check for memory length
      if(cmdoffset == ATCMD_LENGTH)
      {
        ESP_LOGW(LOG_TAG,"AT cmd too long, discarding");
        cmdoffset = 0;
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
 * @see hid_command_t
 * @see hid_usb
 * @see HAL_SERIAL_HIDPIN
 * @note Due to the nature of absolute values for some HID input, it is
 * sometimes wanted to update only relative values (e.g. move mouse, but don't
 * change mouse button status). To support only updates of partial reports,
 * the LPC chip supports a feature where the absolute value is NOT changed by
 * sending 0xFF for the corresponding byte. Currently following bytefields support
 * this:
 * * Mouse: buttons
 * */
void halSerialHIDTask(void *param)
{
  hid_cmd_t rx;
  while(1)
  {
    //check if queue is initialized
    if(hid_usb != NULL)
    {
      //pend on MQ, if timeout triggers, just wait again.
      if(xQueueReceive(hid_usb,&rx,portMAX_DELAY))
      {
        //output if debug
        #if LOG_LEVEL_SERIAL >= ESP_LOG_DEBUG
          ESP_LOGD(LOG_TAG,"HID: %02X:%02X:%02X",rx.cmd[0],rx.cmd[1],rx.cmd[2]);
        #endif
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (HAL_SERIAL_I2C_ADDR_LPC << 1) | WRITE_BIT, ACK_CHECK_EN);
        i2c_master_write(cmd, rx.cmd, 3, ACK_CHECK_EN);
        i2c_master_stop(cmd);
        esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_RATE_MS);
        i2c_cmd_link_delete(cmd);
        //we don't care about return code.
        //I2C driver sometimes return TIMEOUT...
        (void) ret;
        
        if(ret != ESP_OK) ESP_LOGW(LOG_TAG,"I2C didn't succeed: 0x%X",ret);
        else {
          #if LOG_LEVEL_SERIAL >= ESP_LOG_DEBUG
          ESP_LOGD(LOG_TAG,"I2C succeed");
          #endif
        }
      }
    } else {
      ESP_LOGW(LOG_TAG,"usb hid queue not initialized, retry in 1s");
      vTaskDelay(1000/portTICK_PERIOD_MS);
    }
  }
}

/** @brief Read ADC data via I2C from LPC chip
 * 
 * This method reads 10Bytes of ADC data from LPC chip via the
 * I2C interface.
 * 
 * @return -1 on error, number of read bytes otherwise
 * @param data Double pointer to save 10 Bytes of data
 * @see HAL_SERIAL_I2C_TIMEOUT_MS
 * */
int halSerialReceiveI2CADC(uint8_t **data)
{
  //we define 10bytes to be read.
  int size = 10;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (HAL_SERIAL_I2C_ADDR_LPC << 1) | READ_BIT, ACK_CHECK_EN);
  if (size > 1) {
      i2c_master_read(cmd, *data, size - 1, ACK_VAL);
  }
  i2c_master_read_byte(cmd, *data + size - 1, NACK_VAL);
  i2c_master_stop(cmd);
  esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_RATE_MS);
  i2c_cmd_link_delete(cmd);
  if(ret == ESP_OK) return size;
  else return -1;
}

/** @brief Read parsed AT commands from USB-Serial (USB-CDC)
 * 
 * This method reads full AT commands from the halSerialATCmds queue.
 * 
 * @return -1 on error, number of read bytes otherwise
 * @param data Double pointer to save the new allocated buffer to.
 * @warning Free the data pointer after use!
 * @note In timeout, debug information is print. Please uncomment if wanted:
 * * Printing free heap for each VB task
 * * Print task CPU usage (only supported if "Use Trace facilities" is activated)
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
    
    //clear flag, because we surely have an unprocessed command here
    xEventGroupClearBits(systemStatus,SYSTEM_EMPTY_CMD_QUEUE);
    
    return recv.len;
  } else {
    //print heap info
    //heap_caps_print_heap_info(MALLOC_CAP_8BIT);
    ESP_LOGI("mem","Free heap: %dB",xPortGetFreeHeapSize());
    //print the CPU usage
    //You need to activate trace facilities...
    char *taskbuf = (char*)malloc(1024);
    vTaskGetRunTimeStats(taskbuf);
    ESP_LOGD("mem","Tasks:\n%s",taskbuf);
    free(taskbuf);
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
      ESP_LOGE(LOG_TAG,"Additional stream cannot be sent,removing stream!");
      outputcb = NULL;
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
  
  //if all HID types should be resetted,
  //send global reset command (don't send 3 different commands)
  if(exceptDevice == 0)
  {
    hid_cmd_t m;
    m.cmd[0] = 0x00;
    //send to queue
    xQueueSend(hid_usb, &m, 0);
    return;
  }
  
  //reset mouse
  if(!(exceptDevice & (1<<2))) 
  {
    hid_cmd_t m;
    m.cmd[0] = 0x1F;
    //send to queue
    xQueueSend(hid_usb, &m, 0);
  }
  //reset keyboard
  if(!(exceptDevice & (1<<0)))
  {
    hid_cmd_t k;
    k.cmd[0] = 0x2F;
    //send to queue
    xQueueSend(hid_usb, &k, 0);
  }
  //reset joystick
  if(!(exceptDevice & (1<<1)))
  {
    hid_cmd_t j;
    j.cmd[0] = 0x3F;
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

/** @brief Initialize the serial HAL
 * 
 * This method initializes the serial interface & creates
 * all necessary tasks.
 * */
esp_err_t halSerialInit(void)
{
  //set log level to given log level
  esp_log_level_set(LOG_TAG,LOG_LEVEL_SERIAL);
  
  /*++++ UART config (UART to CDC) ++++*/
  
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
  
  //create mutex for all tasks sending to serial TX queue.
  //avoids splitting of different packets due to preemption
  serialsendingsem = xSemaphoreCreateMutex();
  if(serialsendingsem == NULL) 
  {
    ESP_LOGE(LOG_TAG,"Cannot create semaphore for TX"); 
    return ESP_FAIL;
  }
  
  //create the AT command queue
  halSerialATCmds = xQueueCreate(CMDQUEUE_SIZE,sizeof(atcmd_t));

  /*++++ I2C config (sending HID commands; receiving ADC data) ++++*/
  int i2c_master_port = I2C_NUM_0;
  i2c_config_t conf;
  conf.mode = I2C_MODE_MASTER;
  conf.sda_io_num = HAL_IO_PIN_SDA;
  conf.sda_pullup_en = GPIO_PULLUP_DISABLE;
  conf.scl_io_num = HAL_IO_PIN_SCL;
  conf.scl_pullup_en = GPIO_PULLUP_DISABLE;
  conf.master.clk_speed = 100000;
  i2c_param_config(i2c_master_port, &conf);
  if(i2c_driver_install(i2c_master_port, conf.mode,0,0,0) != ESP_OK) 
  {
    ESP_LOGE(LOG_TAG,"Error initializing I2C master");
    return ESP_FAIL;
  }

  /*++++ task setup ++++*/
  //task for sending HID commands via I2C
  xTaskCreate(halSerialHIDTask, "serialHID", HAL_SERIAL_TASK_STACKSIZE+256, NULL, configMAX_PRIORITIES-3, NULL);
  
  //Create a task to handler UART event from ISR
  xTaskCreate(halSerialRXTask, "serialRX", HAL_SERIAL_TASK_STACKSIZE, NULL, configMAX_PRIORITIES-3, NULL);

  //everything went fine
  ESP_LOGI(LOG_TAG,"Driver installation complete");
  return ESP_OK;
}
