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
 * This file contains the hardware abstraction for all IO related
 * stuff (except ADC). Input buttons are read here, as well as the RGB LED
 * is driven.
 * In addition the infrared (IR) command stuff is also done here.
 * 
 * Compared to the tasks in the folder "function_tasks" all HAL tasks are
 * singletons.
 * Call init to initialize every necessary data structure.
 * 
 */
 
 
#include "hal_io.h"


static void gpio_isr_handler(void* arg)
{
  uint32_t pin = (uint32_t) arg;
  uint8_t vb = 0;
  BaseType_t xResult, xHigherPriorityTaskWoken = pdFALSE;
  
  switch(pin)
  {
    case HAL_IO_PIN_BUTTON_EXT1: vb = VB_EXTERNAL1; break;
    case HAL_IO_PIN_BUTTON_EXT2: vb = VB_EXTERNAL2; break;
    case HAL_IO_PIN_BUTTON_INT1: vb = VB_INTERNAL1; break;
    case HAL_IO_PIN_BUTTON_INT2: vb = VB_INTERNAL2; break;
    default: return;
  }

  if(gpio_get_level(pin) == 0)
  {
    //set press flag
    xResult = xEventGroupSetBitsFromISR(virtualButtonsIn[vb/4],(1<<(vb%4)),&xHigherPriorityTaskWoken);
    //clear release flag
    xResult = xEventGroupClearBitsFromISR(virtualButtonsIn[vb/4],(1<<(vb%4 + 4)));
  } else {
    //set release flag
    xResult = xEventGroupSetBitsFromISR(virtualButtonsIn[vb/4],(1<<(vb%4 + 4)),&xHigherPriorityTaskWoken);
    //clear press flag
    xResult = xEventGroupClearBitsFromISR(virtualButtonsIn[vb/4],(1<<(vb%4)));
  }
  if(xResult == pdPASS) portYIELD_FROM_ISR();
}

/** initializing IO HAL
 * 
 * This method initializes the IO HAL stuff:
 * -) GPIO interrupt on 2 external and one internal button
 * -) RMT engine (recording and replaying of infrared commands)
 * -) LEDc driver for the RGB LED output
 * */
esp_err_t halIOInit(void)
{
  /*++++ init GPIO interrupts for 2 external & 2 internal buttons ++++*/
  gpio_config_t io_conf;
  //disable pull-down mode
  io_conf.pull_down_en = 0;
  //interrupt of rising edge
  io_conf.intr_type = GPIO_PIN_INTR_ANYEDGE;
  //bit mask of the pins
  io_conf.pin_bit_mask = (1<<HAL_IO_PIN_BUTTON_EXT1) | (1<<HAL_IO_PIN_BUTTON_EXT2) | \
    (1<<HAL_IO_PIN_BUTTON_INT1) | (1<<HAL_IO_PIN_BUTTON_INT2);
  //set as input mode    
  io_conf.mode = GPIO_MODE_INPUT;
  //enable pull-up mode
  io_conf.pull_up_en = 1;
  gpio_config(&io_conf);
  
  //TODO: ret vals prüfen
  
  //install gpio isr service
  gpio_install_isr_service(0);
  
  //hook isr handler for specific gpio pin
  gpio_isr_handler_add(HAL_IO_PIN_BUTTON_EXT1, gpio_isr_handler, (void*) HAL_IO_PIN_BUTTON_EXT1);
  gpio_isr_handler_add(HAL_IO_PIN_BUTTON_EXT2, gpio_isr_handler, (void*) HAL_IO_PIN_BUTTON_EXT2);
  gpio_isr_handler_add(HAL_IO_PIN_BUTTON_INT1, gpio_isr_handler, (void*) HAL_IO_PIN_BUTTON_INT1);
  gpio_isr_handler_add(HAL_IO_PIN_BUTTON_INT2, gpio_isr_handler, (void*) HAL_IO_PIN_BUTTON_INT2);
  
  return ESP_OK;
  
  //TBD: init IR
  
  //TBD: init RGB....
  
  //TBD: init buzzer
}
