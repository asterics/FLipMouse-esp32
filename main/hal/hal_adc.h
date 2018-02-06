#ifndef HAL_ADC_H
#define HAL_ADC_H
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
 * @brief HAL TASK + FUNCTIONAL TASK - This module contains the hardware abstraction as well as
 * the calculations of the ADC.
 * 
 * The hal_adc files are used to measure all ADC inputs (4 direction
 * sensors and 1 pressure sensor).
 * In addition, depending on the configuration, different ADC tasks are
 * loaded.
 * One of these tasks is loaded:<br>
 * * halAdcMouse - Use the ADC values as mouse input <br>
 * * halAdcThreshold - Use the ADC values to trigger virtual buttons 
 * (keyboard actions for example) <br>
 * * halAdcJoystick - Use the ADC input to control the HID joystick <br>
 * 
 * These tasks are HAL tasks, which means they might be reloaded/changed,
 * but they are not managed outside this module.<br>
 * In addition, the FUNCTIONAL task task_calibration can be used to
 * trigger a zero-point calibration of the mouthpiece.
 * 
 * @see adc_config_t
 * @todo Add raw value reporting for: CIM mode and serial interface
 * */
 
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <esp_log.h>
#include <driver/adc.h>
#include "esp_adc_cal.h"
//common definitions & data for all of these functional tasks
#include "common.h"
#include "hal_serial.h"

/** ADC input pin for "up" channel of FSR */
#define HAL_IO_PIN_ADC_UP       36
/** ADC input pin for "down" channel of FSR */
#define HAL_IO_PIN_ADC_DOWN     37
/** ADC input pin for "left" channel of FSR */
#define HAL_IO_PIN_ADC_LEFT     38
/** ADC input pin for "right" channel of FSR */
#define HAL_IO_PIN_ADC_RIGHT    39
  /** ADC input pin for pressure sensor MPX */
#define HAL_IO_PIN_ADC_PRESSURE 34

/** ADC channel for FSR up */
#define HAL_IO_ADC_CHANNEL_UP       ADC1_CHANNEL_0
/** ADC channel for FSR down */
#define HAL_IO_ADC_CHANNEL_DOWN     ADC1_CHANNEL_1
/** ADC channel for FSR left */
#define HAL_IO_ADC_CHANNEL_LEFT     ADC1_CHANNEL_2
/** ADC channel for FSR right */
#define HAL_IO_ADC_CHANNEL_RIGHT    ADC1_CHANNEL_3
/** ADC channel for pressure sensor */
#define HAL_IO_ADC_CHANNEL_PRESSURE ADC1_CHANNEL_6

/** Task priority for ADC task */
#define HAL_IO_ADC_TASK_PRIORITY 4
/** Stacksize for functional task task_calibration.
 * @see task_calibration */
#define TASK_CALIB_STACKSIZE 2048

/** Parameter for mouse acceleration calculation */
#define ACCELTIME_MAX 20000


/** @brief FUNCTIONAL TASK - Trigger zero-point calibration of mouthpiece
 * 
 * This task is used to trigger a zero-point calibration of
 * up/down/left/right input.
 * 
 * @param param Task configuration
 * @see halAdcCalibrate
 * @see taskNoParameterConfig_t*/
void task_calibration(taskNoParameterConfig_t *param);

/** @brief Reload ADC config
 * 
 * This method reloads the ADC config.
 * Depending on the configuration, a task switch might be initiated
 * to switch the mouthpiece mode from e.g., Joystick to Mouse to 
 * Alternative Mode (Threshold operated).
 * @param params New ADC config
 * @return ESP_OK on success, ESP_FAIL otherwise (wrong config, out of memory)
 * */
esp_err_t halAdcUpdateConfig(adc_config_t* params);


/** @brief Init the ADC driver module
 * 
 * This method initializes the HAL ADC driver with the given config
 * Depending on the configuration, different tasks are initialized
 * to switch the mouthpiece mode from e.g., Joystick to Mouse to 
 * Alternative Mode (Threshold operated).
 * @param params ADC config for intialization
 * @return ESP_OK on success, ESP_FAIL otherwise (wrong config, no memory, already initialized)
 * */
esp_err_t halAdcInit(adc_config_t* params);

#endif
