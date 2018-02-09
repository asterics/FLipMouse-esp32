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
 */
/** @file
 * @brief HAL TASK - This file contains the hardware abstraction for all IO related
 * stuff (except ADC).
 * 
 * Following peripherals are utilized here:<br>
 * * Input buttons
 * * RGB LED
 * * IR receiver (TSOP)
 * * IR LED (sender)
 * * Buzzer
 * 
 * @note Compared to the tasks in the folder "function_tasks" all HAL tasks are
 * singletons. Call init to initialize every necessary data structure.
 * 
 * @todo Test LED driver
 * @todo Implement Buzzer
 * @todo Implement IR driver
 * @todo Implement an IR struct for queues (pointer + length to RMT)
 * */
 
 
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/portmacro.h>
#include "freertos/task.h"
#include "freertos/queue.h"
#include <esp_log.h>
#include "esp_err.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "driver/rmt.h"
//common definitions & data for all of these functional tasks
#include "common.h"

/** @brief Macro to easily create a tone
 * @param freq Frequency of the tone [Hz]
 * @param length Tone length [ms] */
#define TONE(freq,length) { \
  halIOBuzzer_t tone = {    \
    .frequency = freq,      \
    .duration = length };   \
  xQueueSend(halIOBuzzerQueue, (void*)&tone , (TickType_t) 0 ); }

/** @brief Macro to easily send an IR buffer 
 * @param buf rmt_item32_t pointer to the buffer
 * @param len Count of buffer items */
#define SENDIR(buf,len) { \
  halIOIR_t ir = {    \
    .buffer = buf,      \
    .count = len};   \
  xQueueSend(halIOIRSendQueue, (void*)&ir , (TickType_t) 0 ); }

/** @brief PIN - GPIO pin for external button 1 */
#define HAL_IO_PIN_BUTTON_EXT1  26 
/** @brief PIN - GPIO pin for external button 2 */
#define HAL_IO_PIN_BUTTON_EXT2  27
/** @brief PIN - GPIO pin for internal button 1 */
#define HAL_IO_PIN_BUTTON_INT1  22
/** @brief PIN - GPIO pin for internal button 2 */
#define HAL_IO_PIN_BUTTON_INT2  23
/** @brief PIN - GPIO pin for buzzer
 * @note We will use ledc drivers for the buzzer*/
#define HAL_IO_PIN_BUZZER       25
/** @brief PIN - GPIO pin for IR receiver (TSOP) 
 * @note IR will be done with the RMT driver*/
#define HAL_IO_PIN_IR_RECV      19
/** @brief PIN - GPIO pin for IR sender (IR-LED)
 * @note IR will be done with the RMT driver */
#define HAL_IO_PIN_IR_SEND      21

/** @brief PIN - GPIO pin for status LED (RGB) - RED 
 * @note LEDs are driven by LEDC driver */
#define HAL_IO_PIN_LED_RED      12
/** @brief PIN - GPIO pin for status LED (RGB) - GREEN
 * @note LEDs are driven by LEDC driver */
#define HAL_IO_PIN_LED_GREEN    13
/** @brief PIN - GPIO pin for status LED (RGB) - BLUE
 * @note LEDs are driven by LEDC driver */
#define HAL_IO_PIN_LED_BLUE     14

/** @brief Set the count of memory blocks utilized for IR sending
 * 
 * ESP32's RMT unit has 8 64x32bits buffers for IR waveforms.
 * The IR code in FLipMouse/FABI uses RMT channels 0 and 4, therefor
 * both channels (RX&TX) could use up to 4 blocks for saving waveforms.
 * If there is need for a RMT channel for different purposes, reduce
 * this value to 3 to free one buffer for channel 3 and one for channel 7*/
#define HAL_IO_IR_MEM_BLOCKS    4

/** @brief LED update queue
 * 
 * This queue is used to update the LED color & brightness
 * Please use one uint32_t value with following content:
 * * \<bits 0-7\> RED
 * * \<bits 8-15\> GREEN
 * * \<bits 16-23\> BLUE
 * * \<bits 24-31\> Fading time ([10¹ms] -> value of 200 is 2s)
 * 
 * @note Call halIOInit to initialize this queue.
 * @see halIOInit
 **/
extern QueueHandle_t halIOLEDQueue;

/** @brief Task stacksize for LED update task */
#define TASK_HAL_LED_STACKSIZE 512

/** @brief Task priority for LED update task */
#define TASK_HAL_LED_PRIORITY (tskIDLE_PRIORITY + 2)

/** @brief Task stacksize for buzzer update task */
#define TASK_HAL_BUZZER_STACKSIZE 512

/** @brief Task stacksize for IR send task */
#define TASK_HAL_IR_SEND_STACKSIZE 512

/** @brief Task priority for IR send task */
#define TASK_HAL_IR_SEND_PRIORITY (tskIDLE_PRIORITY + 2)

/** @brief Task stacksize for IR receive task */
#define TASK_HAL_IR_RECV_STACKSIZE 512

/** @brief Task priority for IR receive task */
#define TASK_HAL_IR_RECV_PRIORITY (tskIDLE_PRIORITY + 2)

/** @brief Task priority for buzzer update task */
#define TASK_HAL_BUZZER_PRIORITY (tskIDLE_PRIORITY + 2)

/** @brief Task priority for buzzer update task */
#define TASK_HAL_BUZZER_PRIORITY (tskIDLE_PRIORITY + 2)

/** @brief Initializing IO HAL
 * 
 * This method initializes the IO HAL stuff:<br>
 * * GPIO interrupt on 2 external and one internal button
 * * RMT engine (recording and replaying of infrared commands)
 * * LEDc driver for the RGB LED output
 * * PWM for buzzer output
 * */
esp_err_t halIOInit(void);

/** @brief Queue to pend for any incoming infrared remote commands 
 * @see halIOIR_t */
QueueHandle_t halIOIRRecvQueue;
/** @brief Queue to trigger any sending of infrared remote commands 
 * @see halIOIR_t */
QueueHandle_t halIOIRSendQueue;
/** @brief Queue to trigger a buzzer tone 
 * @see halIOBuzzer_t */
QueueHandle_t halIOBuzzerQueue;

/** @brief Output buzzer noise */
typedef struct halIOBuzzer {
  /** Frequency of tone [Hz]
   * @note Use 0 for a pause without tone. */
  uint16_t frequency;
  /** Duration of tone [ms] */
  uint16_t duration;
} halIOBuzzer_t;

/** @brief Output buzzer noise */
typedef struct halIOIR {
  /** Buffer for IR signal
   * @warning Do not free this buffer! It will be freed by transmitting task
   * @note In case of receiving, this buffer can be freed. */
   rmt_item32_t *buffer;
  /** Count of rmt_item32_t items */
  uint16_t count;
} halIOIR_t;

#endif
