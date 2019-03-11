#ifndef HAL_SERIAL_H
#define HAL_SERIAL_H
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
 
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>
#include <esp_log.h>
#include "driver/uart.h"
#include "driver/i2c.h"
#include "soc/uart_struct.h"
#include "string.h"
//common definitions & data for all of these functional tasks
#include "common.h"
//used for add/remove keycodes from a HID report
#include "keyboard.h"
//used to get current locale information
#include "../config_switcher.h"

/** @brief TX pin for serial IF to LPC chip */
#define HAL_SERIAL_TXPIN      (GPIO_NUM_17)
/** @brief RTX pin for serial IF to LPC chip */
#define HAL_SERIAL_RXPIN      (GPIO_NUM_16)
/** @brief signalling pin for serial IF to LPC chip
 * @see halSerialSendUSBSerial */
#define HAL_SERIAL_HIDPIN     (GPIO_NUM_18)
/** @brief UART unit number for serial IF to LPC chip */
#define HAL_SERIAL_UART       (UART_NUM_2)

/** @brief TX pin for serial IF to addon boards
 * @note Currently unused, reserved for future additions */
#define HAL_SERIAL_EXT_TXPIN      (GPIO_NUM_4)
/** @brief RX pin for serial IF to addon boards
 * @note Currently unused, reserved for future additions */
#define HAL_SERIAL_EXT_RXPIN      (GPIO_NUM_19)

/**@brief Sets line ending character
 * According to FLipMouse GUI PortIO.cs, \r is used */
#define HAL_SERIAL_LINE_ENDING "\r\n"

/** @brief Set RMT channel for HID output */
#define HAL_SERIAL_HID_CHANNEL RMT_CHANNEL_3

/** @brief Duration of a 0 bit for HID output */
#define HAL_SERIAL_HID_DURATION_0   80
/** @brief Duration of a 1 bit for HID output */
#define HAL_SERIAL_HID_DURATION_1   160
/** @brief Duration of a start/stop bit for HID output */
#define HAL_SERIAL_HID_DURATION_S   320

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
QueueHandle_t halSerialATCmds;

/** @brief Function pointer type for an additional output of a serial stream */
typedef esp_err_t (*serialoutput_h)(char* p_data, size_t length);

/** @brief Set an additional function for outputting the serial data
 * 
 * This function sets a callback, which is used if the program needs
 * an additional output despite the serial interface. In this case,
 * the webgui registers a callback, which sends all the serial data
 * to the websocket
 * @param cb Function callback
*/
void halSerialAddOutputStream(serialoutput_h cb);


/** @brief Remove the additional function for outputting the serial data
 * 
 * This function removes the callback, which is used if the program needs
 * an additional output despite the serial interface.
*/
void halSerialRemoveOutputStream(void);

/** @brief AT command type for halSerialATCmds queue
 * 
 * This type of data is used to pass one AT command (in format
 * of "AT MX 100") to any pending task.
 * @see halSerialATCmds
 * */
typedef struct atcmd {
  /** @brief Buffer pointer for the AT command 
   * @note Buffer needs to be freed in pending/receiving functions
   * (currently this is halSerialReceiveUSBSerial)
   * @see halSerialReceiveUSBSerial
   * */
  uint8_t *buf;
  /** @brief Length of the corresponding AT command string */
  uint16_t len;
} atcmd_t;

/** @brief Initialize the serial HAL
 * 
 * This method initializes the serial interface & creates
 * all necessary tasks
 * */
esp_err_t halSerialInit(void);


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
void halSerialReset(uint8_t exceptDevice);

/** @brief Send serial bytes to USB-Serial (USB-CDC)
 * 
 * This method sends bytes to the UART, for USB-CDC
 * 
 * @return -1 on error, number of read bytes otherwise
 * @param data Data to be sent
 * @param length Number of maximum bytes to send
 * @param ticks_to_wait Maximum time to wait for a free UART
 * */
int halSerialSendUSBSerial(char *data, uint32_t length, TickType_t ticks_to_wait);

/** @brief Flush Serial RX input buffer */
void halSerialFlushRX(void);


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
int halSerialReceiveUSBSerial(uint8_t **data);

/** @brief Read ADC data via I2C from LPC chip
 * 
 * This method reads 10Bytes of ADC data from LPC chip via the
 * I2C interface.
 * 
 * @return -1 on error, number of read bytes otherwise
 * @param data Double pointer to save 10 Bytes of data
 * @see HAL_SERIAL_I2C_TIMEOUT_MS
 * */
int halSerialReceiveI2CADC(uint8_t **data);
#endif /* HAL_SERIAL_H */
