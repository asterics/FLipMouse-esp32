#ifndef HAL_IO_H
#define HAL_IO_H
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
 * This header file contains the hardware abstraction for all IO related
 * stuff (except ADC). Input buttons are read here, as well as the RGB LED
 * is driven.
 * In addition the infrared (IR) command stuff is also done here.
 * 
 * Compared to the tasks in the folder "function_tasks" all HAL tasks are
 * singletons.
 * Call init to initialize every necessary data structure.
 * 
 */
 
 
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/portmacro.h>
#include "freertos/task.h"
#include "freertos/queue.h"
#include <esp_log.h>
#include "esp_err.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
//common definitions & data for all of these functional tasks
#include "common.h"

#define HAL_IO_PIN_BUTTON_EXT1  26 
#define HAL_IO_PIN_BUTTON_EXT2  27
#define HAL_IO_PIN_BUTTON_INT1  22
#define HAL_IO_PIN_BUTTON_INT2  23

/** initializing IO HAL
 * 
 * This method initializes the IO HAL stuff:
 * -) GPIO interrupt on 2 external and one internal button
 * -) RMT engine (recording and replaying of infrared commands)
 * -) LEDc driver for the RGB LED output
 * */
esp_err_t halIOInit(void);

/** queue to control the LED and change color.
 * @see halIOLED_t */
QueueHandle_t hal_io_led;
/** queue to pend for any incoming infrared remote commands 
 * @see halIOIR_t */
QueueHandle_t hal_io_ir_recv;
/** queue to trigger any sending of infrared remote commands 
 * @see halIOIR_t */
QueueHandle_t hal_io_ir_send;
/** queue to trigger any sending of infrared remote commands 
 * @see halIOBuzzer_t */
QueueHandle_t hal_io_buzzer;

/** output buzzer noise */
typedef struct halIOBuzzer {
  /** frequency of tone */
  uint16_t frequency;
  /** duration of tone */
  uint16_t duration;
  /** fading of tone, not implemented */
  uint16_t fade;
} halIOBuzzer_t;

#endif
