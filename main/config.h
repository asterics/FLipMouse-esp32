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
 **/
 
/** @file
 * 
 * @brief This module contains configuration variables/defines*/

/** @brief module firmware version
 * 
 * This string is used to determine the running firmware version.
 * It is printed on the "AT FW" command. */
#define MODULE_ID "ESP32BT_v0.1"

/** @brief UART interface for command parsing & sending USB HID
 * @todo Change to external UART; currently set to serial port of monitor for debugging */
#define EX_UART_NUM UART_NUM_0
