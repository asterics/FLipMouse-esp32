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
 * */
 
#include "hal_io.h"

/** @brief Log tag */
#define LOG_TAG "halIO"

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
QueueHandle_t halIOLEDQueue = NULL;

/** @brief GPIO ISR handler for buttons (internal/external)
 * 
 * This ISR handler is called on rising&falling edge of each button
 * GPIO.
 * 
 * It sets and clears the VB flags accordingly on each call.
 * These flags are used for button debouncing.
 * @see task_debouncer
 */
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

/** @brief HAL TASK - Buzzer update task
 * 
 * This task takes an instance of the buzzer update struct and creates
 * a tone on the buzzer accordingly.
 * Done via the LEDC driver & vTaskDelays
 * 
 * @see halIOBuzzer_t
 * @see halIOBuzzerQueue
 * 
 */
void halIOBuzzerTask(void * param)
{
  halIOBuzzer_t *recv;
  
  if(halIOBuzzerQueue == NULL)
  {
    ESP_LOGW(LOG_TAG, "halIOLEDQueue not initialised");
    while(halIOBuzzerQueue == NULL) vTaskDelay(1000/portTICK_PERIOD_MS);
  }
  
  while(1)
  {
    //wait for updates
    if(xQueueReceive(halIOBuzzerQueue,&recv,10000))
    {
      //set duty, set frequency
      ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, recv->frequency);
      ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 512);
      ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
      
      //delay for duration
      vTaskDelay(recv->duration / portTICK_PERIOD_MS);
      
      //set duty to 0
      ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
      ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    }
  }
}

/** @brief HAL TASK - LED update task
 * 
 * This task simply takes an uint32_t value from the LED queue
 * and calls the ledc_fading_... methods of the esp-idf.
 * 
 * @see halIOLEDQueue
 */
void halIOLEDTask(void * param)
{
  uint32_t recv = 0;
  uint32_t duty = 0;
  uint32_t fade = 0;
  if(halIOLEDQueue == NULL)
  {
    ESP_LOGW(LOG_TAG, "halIOLEDQueue not initialised");
    while(halIOLEDQueue == NULL) vTaskDelay(1000/portTICK_PERIOD_MS);
  }
  while(1)
  {
    //wait for updates
    if(xQueueReceive(halIOLEDQueue,&recv,10000))
    {
      //updates received, sending to ledc driver
      
      //get fading time, unit is 10ms
      fade = ((recv & 0xFF000000) >> 24) * 10;
      
      //set fade with time (target duty and fading time)
      
      //1.) RED: map to 10bit and set to fading unit
      duty = (recv & 0x000000FF) * 2 * 2; 
      ledc_set_fade_with_time(LEDC_HIGH_SPEED_MODE,LEDC_CHANNEL_0, \
        duty, fade);
      
      //2.) GREEN: map to 10bit and set to fading unit
      duty = ((recv & 0x0000FF00) >> 8) * 2 * 2; 
      ledc_set_fade_with_time(LEDC_HIGH_SPEED_MODE,LEDC_CHANNEL_1, \
        duty, fade);
      
      //3.) BLUE: map to 10bit and set to fading unit
      duty = ((recv & 0x00FF0000) >> 16) * 2 * 2; 
      ledc_set_fade_with_time(LEDC_HIGH_SPEED_MODE,LEDC_CHANNEL_2, \
        duty, fade);
      
      //start fading for RGB
      ledc_fade_start(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, LEDC_FADE_NO_WAIT);
      ledc_fade_start(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, LEDC_FADE_NO_WAIT);
      ledc_fade_start(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_2, LEDC_FADE_NO_WAIT);
    }
  }
}

/** @brief Initializing IO HAL
 * 
 * This method initializes the IO HAL stuff:<br>
 * * GPIO interrupt on 2 external and one internal button
 * * RMT engine (recording and replaying of infrared commands)
 * * LEDc driver for the RGB LED output
 * * PWM for buzzer output
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
  
  /*++++ init infrared drivers (via RMT engine) ++++*/
  //TBD: init IR
  
  /*++++ init RGB LEDc driver ++++*/
  
  //init RGB queue & ledc driver
  halIOLEDQueue = xQueueCreate(8,sizeof(uint32_t));
  ledc_timer_config_t led_timer = {
    .duty_resolution = LEDC_TIMER_10_BIT, // resolution of PWM duty
    .freq_hz = 5000,                      // frequency of PWM signal
    .speed_mode = LEDC_HIGH_SPEED_MODE,           // timer mode
    .timer_num = LEDC_TIMER_0            // timer index
  };
  // Set configuration of timer0 for high speed channels
  ledc_timer_config(&led_timer);
  ledc_channel_config_t led_channel[3] = {
    {
      .channel    = LEDC_CHANNEL_0,
      .duty       = 0,
      .gpio_num   = HAL_IO_PIN_LED_RED,
      .speed_mode = LEDC_HIGH_SPEED_MODE,
      .timer_sel  = LEDC_TIMER_0
    } , {
      .channel    = LEDC_CHANNEL_1,
      .duty       = 0,
      .gpio_num   = HAL_IO_PIN_LED_GREEN,
      .speed_mode = LEDC_HIGH_SPEED_MODE,
      .timer_sel  = LEDC_TIMER_0
    } , {
      .channel    = LEDC_CHANNEL_2,
      .duty       = 0,
      .gpio_num   = HAL_IO_PIN_LED_BLUE,
      .speed_mode = LEDC_HIGH_SPEED_MODE,
      .timer_sel  = LEDC_TIMER_0
    }
  };
  //apply config to LED driver channels
  for (uint8_t ch = 0; ch < 3; ch++) {
    ledc_channel_config(&led_channel[ch]);
  }
  //activate fading
  ledc_fade_func_install(0);
  //start LED update task
  if(xTaskCreate(halIOLEDTask,"ledtask",TASK_HAL_LED_STACKSIZE, 
    (void*)NULL,TASK_HAL_LED_PRIORITY, NULL) == pdPASS)
  {
    ESP_LOGD(LOG_TAG,"created LED task");
  } else {
    ESP_LOGE(LOG_TAG,"error creating task");
    return ESP_FAIL;
  }
  
  
  /*++++ INIT buzzer ++++*/
  //we will use the LEDC unit for the buzzer
  //because RMT has no lower frequency than 611Hz (according to example)
  
  ledc_timer_config_t buzzer_timer = {
    .duty_resolution = LEDC_TIMER_10_BIT, // resolution of PWM duty
    .freq_hz = 100,                      // frequency of PWM signal
    .speed_mode = LEDC_LOW_SPEED_MODE,           // timer mode
    .timer_num = LEDC_TIMER_0            // timer index
  };
  ledc_timer_config(&buzzer_timer);
  ledc_channel_config_t buzzer_channel = {
    .channel    = LEDC_CHANNEL_0,
    .duty       = 0,
    .gpio_num   = HAL_IO_PIN_BUZZER,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .timer_sel  = LEDC_TIMER_0
  };
  ledc_channel_config(&buzzer_channel);
  
  //start buzzer update task
  if(xTaskCreate(halIOBuzzerTask,"buzztask",TASK_HAL_BUZZER_STACKSIZE, 
    (void*)NULL,TASK_HAL_BUZZER_PRIORITY, NULL) == pdPASS)
  {
    ESP_LOGD(LOG_TAG,"created buzzer task");
  } else {
    ESP_LOGE(LOG_TAG,"error creating buzzer task");
    return ESP_FAIL;
  }
  
  
  return ESP_OK;
}
