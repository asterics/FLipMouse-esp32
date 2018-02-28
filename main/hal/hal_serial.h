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
 * Depending on the level of the signal line, data sent to the USB chip are either
 * interpreted as USB-HID commands (keyboard, mouse or joystick reports) or
 * the data is forwarded to the USB-CDC interface, which is used to send data 
 * to the host (config GUI, terminal, AsTeRICS,...).
 */ 
 
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>
#include <esp_log.h>
#include "driver/uart.h"
#include "soc/uart_struct.h"
#include "string.h"
//common definitions & data for all of these functional tasks
#include "common.h"
//used for add/remove keycodes from a HID report
#include "keyboard.h"
//used to get current locale information
#include "../config_switcher.h"

#define HAL_SERIAL_TXPIN      (GPIO_NUM_17)
#define HAL_SERIAL_RXPIN      (GPIO_NUM_16)
#define HAL_SERIAL_SWITCHPIN  (GPIO_NUM_18)
#define HAL_SERIAL_UART       (UART_NUM_2)

#define HAL_SERIAL_TX_TO_HID  0
#define HAL_SERIAL_TX_TO_CDC  1

/**@brief Sets line ending character
 * According to FLipMouse GUI PortIO.cs, \r is used */
#define HAL_SERIAL_LINE_ENDING '\r'

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
int halSerialSendUSBSerial(uint8_t channel, char *data, uint32_t length, TickType_t ticks_to_wait);

/** @brief Flush Serial RX input buffer */
void halSerialFlushRX(void);


/** @brief Read serial bytes from USB-Serial (USB-CDC)
 * 
 * This method reads bytes from the UART, which receives all data
 * from USB-CDC.
 * 
 * @return -1 on error, number of read bytes otherwise
 * @param data Data to be sent
 * @param length Number of maximum bytes to read
 * @see HAL_SERIAL_UART_TIMEOUT
 * */
int halSerialReceiveUSBSerial(uint8_t *data, uint32_t length);
#endif /* HAL_SERIAL_H */
