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
 * 
 * This header file contains the hardware abstraction for the serial
 * interface to the USB support chip.
 * Attention: this module does NOT use UART0 (terminal & program interface)
 * 
 * The hardware abstraction takes care of:
 * -) sending USB-HID commands
 * -) sending USB-serial responses
 * -) receiving USB-serial AT commands
 * -) sending/receiving CIM data (done via USB serial interface)
 * 
 * The interaction to other parts of the firmware consists of:
 * -) pending on queues for USB commands:
 *    * keyboard_usb_press
 *    * keyboard_usb_release
 *    * mouse_movement_usb
 *    * joystick_movement_usb
 * -) receiving ADC data via queue <TBA>
 * -) sending/receiving Serial Data to command parser via queues:
 *    * <TBA recv>
 *    * <TBA send>
 * 
 * The received serial data is forwarded to 2 different modules,
 * depending on received magic bytes <TODO: described AT & CIM data switching>:
 *  * task_cim For CIM data
 *  * task_commands For AT commands
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

#define HAL_SERIAL_TXPIN      (GPIO_NUM_17)
#define HAL_SERIAL_RXPIN      (GPIO_NUM_16)
#define HAL_SERIAL_SWITCHPIN  (GPIO_NUM_18)
#define HAL_SERIAL_UART       (UART_NUM_2)

#define HAL_SERIAL_TX_TO_HID  0
#define HAL_SERIAL_TX_TO_CDC  1

/** Initialize the serial HAL
 * 
 * This method initializes the serial interface & creates
 * all necessary tasks
 * */
esp_err_t halSerialInit(void);


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
int halSerialSendUSBSerial(uint8_t channel, char *data, uint32_t length, TickType_t ticks_to_wait);

/** @brief Flush Serial RX input buffer */
void halSerialFlushRX(void);


/** Read serial bytes from USB-Serial (USB-CDC)
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
