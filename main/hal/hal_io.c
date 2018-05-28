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
 * * RGB LED (or maybe a Neopixel LED)
 * * IR receiver (TSOP)
 * * IR LED (sender)
 * * Buzzer
 * 
 * All assigned buttons are processed via one GPIO ISR, which sets press
 * and release flags in the VB input event group.
 * In addition, one button can be configured for executing an extra handler
 * after a long press (this long press is much longer compared to "long press"
 * actions handled by task_debouncer). This handler is usually used for
 * activating and deactivating the WiFi interface.
 * 
 * The LED output is configurable either to 3 PWM outputs for RGB LEDs
 * or a Neopixel string with variable length (RGB LEDs use ledc facilities,
 * Neopixels use RMT engine). To enable easy color settings, macros are provided
 * (LED(r,g,b,m)).
 * 
 * IR receiving / sending is done via the RMT engine and is supported by macros
 * as well.
 * 
 * @note Compared to the tasks in the folder "function_tasks" all HAL tasks are
 * singletons. Call init to initialize every necessary data structure.
 * 
 * @todo Test LED driver (RGB & Neopixel)
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
 * 
 * Depending on the LED config (either one RGB LED or Neopixels are used),
 * bits 24-31 are used differently:
 * 
 * <b>RGB LED (LED_USE_NEOPIXEL is NOT defined)</b>:
 * 
 * * \<bits 24-31\> Fading time ([10¹ms] -> value of 200 is 2s)
 * 
 * <b>Neopixels (LED_USE_NEOPIXEL is defined)</b>:
 * 
 * * \<bits 24-31\> Animation mode:
 * * <b>0</b> Steady color on all Neopixels
 * * <b>1</b> 3 Neopixels have the given color and are circled around
 * * <b>2-0xFF</b> Currently undefined, further modes might be added.
 * 
 * @note Call halIOInit to initialize this queue.
 * @see halIOInit
 * @see LED_USE_NEOPIXEL
 * @see LED_NEOPIXEL_COUNT
 **/
QueueHandle_t halIOLEDQueue = NULL;

#if defined(DEVICE_FLIPMOUSE) && defined(LED_USE_NEOPIXEL)
/** @brief Double buffering for neopixels - buffer 1 */
struct led_color_t *neop_buf1 = NULL;
/** @brief Double buffering for neopixels - buffer 2 */
struct led_color_t *neop_buf2 = NULL;
/**@brief Config struct for Neopixel strip */
struct led_strip_t led_strip = {
      .rgb_led_type = RGB_LED_TYPE_WS2812,
      .rmt_channel = RMT_CHANNEL_7,
      .gpio = HAL_IO_PIN_NEOPIXEL,
      .led_strip_length = LED_NEOPIXEL_COUNT
  };
#endif

/** @brief Currently active long press handler, null if not used 
 * @see halIOAddLongPressHandler*/
void (*longpress_handler)(void) = NULL;

/**@brief Timer handle for long press action
 * @see halIOAddLongPressHandler */
TimerHandle_t longactiontimer = NULL;

/** @brief Clock divider for RMT engine */
#define RMT_CLK_DIV      100

/** @brief RMT counter value for 10 us.(Source clock is APB clock) */
#define RMT_TICK_10_US    (80000000/RMT_CLK_DIV/100000)   


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
  
  //determine pin of ISR reason
  switch(pin)
  {
    case HAL_IO_PIN_BUTTON_EXT1: vb = VB_EXTERNAL1; break;
    case HAL_IO_PIN_BUTTON_EXT2: vb = VB_EXTERNAL2; break;
    #ifdef DEVICE_FABI
      case HAL_IO_PIN_BUTTON_EXT3: vb = VB_EXTERNAL3; break;
      case HAL_IO_PIN_BUTTON_EXT4: vb = VB_EXTERNAL4; break;
      case HAL_IO_PIN_BUTTON_EXT5: vb = VB_EXTERNAL5; break;
      case HAL_IO_PIN_BUTTON_EXT6: vb = VB_EXTERNAL6; break;
      case HAL_IO_PIN_BUTTON_EXT7: vb = VB_EXTERNAL7; break;
    #endif
    case HAL_IO_PIN_BUTTON_INT1: vb = VB_INTERNAL1; break;
    #ifdef DEVICE_FLIPMOUSE
      case HAL_IO_PIN_BUTTON_INT2: vb = VB_INTERNAL2; break;
    #endif
    default: return;
  }

  //press or release?
  if(gpio_get_level(pin) == 0)
  {
    //set press flag
    xResult = xEventGroupSetBitsFromISR(virtualButtonsIn[vb/4],(1<<(vb%4)),&xHigherPriorityTaskWoken);
    //clear release flag
    xResult = xEventGroupClearBitsFromISR(virtualButtonsIn[vb/4],(1<<(vb%4 + 4)));
    
    //extra handling for long press
    if(pin == HAL_IO_PIN_LONGACTION)
    {
      //check if timer is initialized, if yes reset & start
      if(longactiontimer != NULL)
      {
        xTimerResetFromISR(longactiontimer,&xHigherPriorityTaskWoken);
        xTimerStartFromISR(longactiontimer,&xHigherPriorityTaskWoken);
      }
    }
  } else {
    //set release flag
    xResult = xEventGroupSetBitsFromISR(virtualButtonsIn[vb/4],(1<<(vb%4 + 4)),&xHigherPriorityTaskWoken);
    //clear press flag
    xResult = xEventGroupClearBitsFromISR(virtualButtonsIn[vb/4],(1<<(vb%4)));
    //extra handling for long press
    if(pin == HAL_IO_PIN_LONGACTION)
    {
      //check if timer is initialized, if yes reset & start
      if(longactiontimer != NULL) xTimerStopFromISR(longactiontimer,&xHigherPriorityTaskWoken);
    }
  }
  if(xResult == pdPASS) portYIELD_FROM_ISR();
}

/** @brief Callback to free the memory after finished transmission
 * 
 * This method calls free() on the buffer which is transmitted now.
 * @param channel Channel of RMT enging
 * @param arg rmt_item32_t* pointer to the buffer
 * */
void halIOIRFree(rmt_channel_t channel, void *arg)
{
  if(arg != NULL) free((rmt_item32_t*)arg);
}

/** @brief HAL TASK - IR receiving (recording) task
 * 
 * This task is used to store data from the RMT unit if the receiving
 * of an IR code is started.
 * By sending an element to the queue halIOIRRecvQueue, this task starts
 * to receive any incoming IR signals.
 * The status of the struct is updated (either finished, timeout or error)
 * on any status change.
 * Poll this value to see if the receiver is finished.
 * 
 * @see halIOIR_t
 * @see halIOIRRecvQueue
 * @param param Unused.
 */
void halIOIRRecvTask(void * param)
{
  halIOIR_t* recv;
  RingbufHandle_t rb = NULL;
  //get RMT RX ringbuffer
  rmt_get_ringbuf_handle(4, &rb);
  
  if(halIOIRRecvQueue == NULL)
  {
    ESP_LOGW(LOG_TAG, "halIOIRRecvQueue not initialised");
    while(halIOIRRecvQueue == NULL) vTaskDelay(1000/portTICK_PERIOD_MS);
  }
  
  while(1)
  {
    //wait for updates (triggered receiving)
    if(xQueueReceive(halIOIRRecvQueue,&recv,portMAX_DELAY))
    {
      ESP_LOGI(LOG_TAG,"IR recv triggered.");
      
      //acquire buffer for ALL possible receiving elements
      uint16_t offset = 0;
      size_t rx_size = 0;
      uint16_t local_timeout = 0;
      
      //check buffer
      if(recv->buffer == NULL)
      {
        ESP_LOGE(LOG_TAG,"Please provide a buffer for receiving IR!");
        continue;
      }
      
      //start receiving on channel 4, flush all buffer elements
      rmt_rx_start(4, 1);
      //set target struct
      recv->status = IR_RECEIVING;
      
      do{
        //wait for one item until timeout or data is valid
        rmt_item32_t* item = (rmt_item32_t*) xRingbufferReceive(rb, &rx_size, TASK_HAL_IR_RECV_EDGE_TIMEOUT/portTICK_PERIOD_MS);
        //got one item
        if(item != NULL) {
          //put data into buffer
          memcpy(&recv->buffer[offset], item, sizeof(rmt_item32_t)*rx_size);

          //invert active mode by inverting bits level0 and level1 in each item
          for(size_t i = 0; i<rx_size;i++) 
          {
            recv->buffer[offset+i].level0 = !recv->buffer[offset+i].level0;
            recv->buffer[offset+i].level1 = !recv->buffer[offset+i].level1;
          }
          //increase offset for next received item
          offset+= rx_size;
          
          //give item back to ringbuffer
          vRingbufferReturnItem(rb, (void*) item);
          //too much
          if(offset >= TASK_HAL_IR_RECV_MAXIMUM_EDGES)
          {
            ESP_LOGE(LOG_TAG,"Too much IR edges, finished");
            recv->status = IR_OVERFLOW;
            recv->count = 0;
            break;
          }
        } else {
          local_timeout += TASK_HAL_IR_RECV_EDGE_TIMEOUT;
          //check if timeout is the long one and no edges received...
          if(local_timeout >= TASK_HAL_IR_RECV_TIMEOUT && offset == 0)
          {
            //timeout, cancel
            rmt_rx_stop(4);
            recv->status = IR_TOOSHORT;
            recv->count = 0;
            ESP_LOGE(LOG_TAG,"No cmd");
            break;
          }
          //if we received already something and timeout triggers ->
          //command finished
          if(offset != 0) 
          {
            //update status accordingly
            if(offset > TASK_HAL_IR_RECV_MINIMUM_EDGES)
            {
              //save everything necessary to pointer from queue
              recv->count = offset;
              recv->status = IR_FINISHED;
              ESP_LOGI(LOG_TAG,"Recorded @%d %d edges",(uint32_t)recv->buffer,offset);
            } else {
              //timeout, cancel
              recv->status = IR_TOOSHORT;
              recv->count = 0;
              ESP_LOGE(LOG_TAG,"IR cmd too short");
            }
            rmt_rx_stop(4);
            break;
          }
        }
      } while(1);
    }
  }
}

/** @brief HAL TASK - Buzzer update task
 * 
 * This task takes an instance of the buzzer update struct and creates
 * a tone on the buzzer accordingly.
 * Done via the LEDC driver & vTaskDelays
 * 
 * @see halIOBuzzer_t
 * @see halIOBuzzerQueue
 * @param param Unused.
 */
void halIOBuzzerTask(void * param)
{
  halIOBuzzer_t recv;
  generalConfig_t *cfg = configGetCurrent();
  
  if(halIOBuzzerQueue == NULL)
  {
    ESP_LOGW(LOG_TAG, "halIOLEDQueue not initialised");
    while(halIOBuzzerQueue == NULL) vTaskDelay(1000/portTICK_PERIOD_MS);
  }
  
  while(cfg == NULL)
  {
    ESP_LOGW(LOG_TAG, "generalconfig not initialised");
    vTaskDelay(1000/portTICK_PERIOD_MS);
    cfg = configGetCurrent(); 
  }
  
  while(1)
  {
    //wait for updates
    if(xQueueReceive(halIOBuzzerQueue,(void*)&recv,10000) == pdTRUE)
    {
      
      //check if feedback mode is set to buzzer output. If not: do nothing
      if((cfg->feedback & 0x02) == 0) continue;
      
      ESP_LOGD(LOG_TAG,"Buzz: freq %d, duration %d",recv.frequency,recv.duration);
      
      //set duty, set frequency
      //do a tone only if frequency is != 0, otherwise it is just a pause
      if(recv.frequency != 0)
      {
        //multiply by 2, otherwise it would be half the frequency...
        ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_1, recv.frequency*2);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_3, 512);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_3);
      }
      
      //delay for duration
      vTaskDelay(recv.duration / portTICK_PERIOD_MS);
      
      //set duty to 0
      ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_3, 0);
      ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_3);
    }
  }
}

/** @brief HAL TASK - LED update task
 * 
 * This task simply takes an uint32_t value from the LED queue
 * and calls the ledc_fading_... methods of the esp-idf.
 * 
 * @see halIOLEDQueue
 * @param param Unused
 * @todo It might be possible that we use Neopixel LEDs on the FLipMouse as well. Update if it is known.
 */
void halIOLEDTask(void * param)
{
  uint32_t recv = 0;
  #if defined(DEVICE_FLIPMOUSE) && !defined(LED_USE_NEOPIXEL)
  uint32_t duty = 0;
  uint32_t fade = 0;
  #endif
  #ifdef DEVICE_FABI
  hid_command_t cmd;
  #endif
  generalConfig_t *cfg = configGetCurrent();
  
  if(halIOLEDQueue == NULL)
  {
    ESP_LOGW(LOG_TAG, "halIOLEDQueue not initialised");
    while(halIOLEDQueue == NULL) vTaskDelay(1000/portTICK_PERIOD_MS);
  }
  
    
  while(cfg == NULL)
  {
    ESP_LOGW(LOG_TAG, "generalconfig not initialised");
    vTaskDelay(1000/portTICK_PERIOD_MS);
    cfg = configGetCurrent(); 
  }
  
  while(1)
  {
    //wait for updates
    if(xQueueReceive(halIOLEDQueue,&recv,10000))
    {
      //check if feedback mode is set to LED output. If not: do nothing
      if((cfg->feedback & 0x01) == 0) continue;
      
      //one Neopixel LED is mounted on FABI, driven by the USB chip
      #ifdef DEVICE_FABI
      
      //'L' + <red> + <green> + <blue> + '\0'
      cmd.data[0] = 'L';
      cmd.data[1] = recv & 0x000000FF;
      cmd.data[2] = ((recv & 0x0000FF00) >> 8);
      cmd.data[3] = ((recv & 0x00FF0000) >> 16);
      cmd.len = 4;
      //send to USB chip
      xQueueSend(hid_usb,&cmd,10);
      #endif
      
      //FLipMouse with RGB LEDs
      #if defined(DEVICE_FLIPMOUSE) && !defined(LED_USE_NEOPIXEL)
      
      //updates received, sending to ledc driver
      
      //get fading time, unit is 10ms
      fade = ((recv & 0xFF000000) >> 24) * 10;
      ESP_LOGI(LOG_TAG,"LED fade time: %d",fade);
      
      //set fade with time (target duty and fading time)
      
      //1.) RED: map to 10bit and set to fading unit
      duty = (recv & 0x000000FF) * 2 * 2; 
      //if(ledc_set_fade_with_time(LEDC_HIGH_SPEED_MODE,LEDC_CHANNEL_0, 
      //  duty, fade) != ESP_OK)
      if(ledc_set_duty(LEDC_HIGH_SPEED_MODE,LEDC_CHANNEL_0,duty) != ESP_OK)
      {
        ESP_LOGE(LOG_TAG,"LED R: error setting fade");
      }
      ESP_LOGI(LOG_TAG,"LED R: %d",duty);
      
      //2.) GREEN: map to 10bit and set to fading unit
      duty = ((recv & 0x0000FF00) >> 8) * 2 * 2; 
      //if(ledc_set_fade_with_time(LEDC_HIGH_SPEED_MODE,LEDC_CHANNEL_1, 
      //  duty, fade) != ESP_OK)
      if(ledc_set_duty(LEDC_HIGH_SPEED_MODE,LEDC_CHANNEL_1,duty) != ESP_OK)
      {
        ESP_LOGE(LOG_TAG,"LED R: error setting fade");
      }
      ESP_LOGI(LOG_TAG,"LED G: %d",duty);
      
      //3.) BLUE: map to 10bit and set to fading unit
      duty = ((recv & 0x00FF0000) >> 16) * 2 * 2; 
      //if(ledc_set_fade_with_time(LEDC_HIGH_SPEED_MODE,LEDC_CHANNEL_2, 
      //  duty, fade) != ESP_OK)
      if(ledc_set_duty(LEDC_HIGH_SPEED_MODE,LEDC_CHANNEL_2,duty) != ESP_OK)
      {
        ESP_LOGE(LOG_TAG,"LED R: error setting fade");
      }
      ESP_LOGI(LOG_TAG,"LED B: %d",duty);
      
      //start fading for RGB
      //ledc_fade_start(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, LEDC_FADE_NO_WAIT);
      //ledc_fade_start(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, LEDC_FADE_NO_WAIT);
      //ledc_fade_start(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_2, LEDC_FADE_NO_WAIT);
      ledc_update_duty(LEDC_HIGH_SPEED_MODE,LEDC_CHANNEL_0);
      ledc_update_duty(LEDC_HIGH_SPEED_MODE,LEDC_CHANNEL_1);
      ledc_update_duty(LEDC_HIGH_SPEED_MODE,LEDC_CHANNEL_2);
      
      #endif
      
      #if defined(DEVICE_FLIPMOUSE) && defined(LED_USE_NEOPIXEL)
      //determine mode
      switch(((recv & 0x00FF0000) >> 16))
      {
        //currently not supported, mapping to steady color
        case 1:
        
        //steady color
        case 0:
          for(uint8_t i = 0; i<LED_NEOPIXEL_COUNT; i++)
          {
            //set the same color for each LED
            led_strip_set_pixel_rgb(&led_strip, i,(recv & 0x000000FF), \
              ((recv & 0x0000FF00) >> 8), ((recv & 0x00FF0000) >> 16));
          }
          //show new color
          led_strip_show(&led_strip);
          break;
        default:
          ESP_LOGE(LOG_TAG,"Unknown Neopixel animation mode");
          break;
      }
      
      
      #endif
      
    }
  }
}

/** @brief Add long press handler for Wifi button
 * 
 * One button (define by HAL_IO_PIN_LONGACTION) can be used as long press button for enabling/disabling
 * wifi (in addition to normal press/release actions).
 * If this functionality should be used, add a handler with this method,
 * which will be called after the button is pressed longer than
 * HAL_IO_LONGACTION_TIMEOUT.
 * 
 * @note HAL_IO_PIN_LONGACTION cannot be used as individual button.
 * Use a pin, which is already used for a normal action, otherwise the
 * handler won't be used (ISR won't get triggered).
 * 
 * @note Clear the handler by passing a void function pointer.
 * 
 * @see HAL_IO_LONGACTION_TIMEOUT
 * @see HAL_IO_PIN_LONGACTION
 * @param longpress_h Handler for long press action, use a null pointer to disable the handler.
 */
void halIOAddLongPressHandler(void (*longpress_h)(void))
{
  longpress_handler = longpress_h;
}

/** @brief Timer callback for calling longpress handler.
 * @param xTimer Unused */
void halIOTimerCallback(TimerHandle_t xTimer)
{
  if(longpress_handler != NULL) longpress_handler();
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
  generalConfig_t *cfg = configGetCurrent();
  
  if(cfg == NULL)
  {
    ESP_LOGE(LOG_TAG,"general Config is NULL!!!");
    while(cfg == NULL) 
    {
      vTaskDelay(1000/portTICK_PERIOD_MS);
      cfg = configGetCurrent();
    }
  }
  
  /*++++ init GPIO interrupts for 2 external & 2 internal buttons ++++*/
  gpio_config_t io_conf;
  //disable pull-down mode
  io_conf.pull_down_en = 0;
  //interrupt of rising edge
  io_conf.intr_type = GPIO_PIN_INTR_ANYEDGE;
  //bit mask of the pins
  io_conf.pin_bit_mask = (1ull<<HAL_IO_PIN_BUTTON_EXT1) | (1ull<<HAL_IO_PIN_BUTTON_EXT2) | \
    (1ull<<HAL_IO_PIN_BUTTON_INT1);
  //do differently for FABI/FLipmouse  
  #ifdef DEVICE_FLIPMOUSE
    io_conf.pin_bit_mask |= (1ull<<HAL_IO_PIN_BUTTON_INT2);
  #endif
  #ifdef DEVICE_FABI
    io_conf.pin_bit_mask |= (1ull<<HAL_IO_PIN_BUTTON_EXT3) | (1ull<<HAL_IO_PIN_BUTTON_EXT4) | \
      (1ull<<HAL_IO_PIN_BUTTON_EXT5) | (1ull<<HAL_IO_PIN_BUTTON_EXT6) | (1ull<<HAL_IO_PIN_BUTTON_EXT7);
  #endif
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
  #ifdef DEVICE_FLIPMOUSE
    gpio_isr_handler_add(HAL_IO_PIN_BUTTON_INT2, gpio_isr_handler, (void*) HAL_IO_PIN_BUTTON_INT2);
  #endif
  #ifdef DEVICE_FABI
    gpio_isr_handler_add(HAL_IO_PIN_BUTTON_EXT3, gpio_isr_handler, (void*) HAL_IO_PIN_BUTTON_EXT3);
    gpio_isr_handler_add(HAL_IO_PIN_BUTTON_EXT4, gpio_isr_handler, (void*) HAL_IO_PIN_BUTTON_EXT4);
    gpio_isr_handler_add(HAL_IO_PIN_BUTTON_EXT5, gpio_isr_handler, (void*) HAL_IO_PIN_BUTTON_EXT5);
    gpio_isr_handler_add(HAL_IO_PIN_BUTTON_EXT6, gpio_isr_handler, (void*) HAL_IO_PIN_BUTTON_EXT6);
    gpio_isr_handler_add(HAL_IO_PIN_BUTTON_EXT7, gpio_isr_handler, (void*) HAL_IO_PIN_BUTTON_EXT7);
  #endif
  
  /*++++ init long press action timer. ++++*/
  //create a single shot timer with given period (HAL_IO_LONGACTION_TIMEOUT)
  longactiontimer = xTimerCreate("IO_longaction", HAL_IO_LONGACTION_TIMEOUT/portTICK_PERIOD_MS, \
    pdFALSE, (void *) 0, halIOTimerCallback);
  if(longactiontimer == NULL)
  {
    ESP_LOGE(LOG_TAG,"Long action timer cannot be initialized, handler won't be called");
  }
  
  /*++++ init infrared drivers (via RMT engine) ++++*/
  halIOIRRecvQueue = xQueueCreate(8,sizeof(halIOIR_t*));
  
  //transmitter
  rmt_config_t rmtcfg;
  rmtcfg.channel = 0;
  rmtcfg.gpio_num = HAL_IO_PIN_IR_SEND;
  rmtcfg.mem_block_num = HAL_IO_IR_MEM_BLOCKS;
  rmtcfg.clk_div = RMT_CLK_DIV;
  rmtcfg.tx_config.loop_en = false;
  rmtcfg.tx_config.carrier_duty_percent = 50;
  rmtcfg.tx_config.carrier_freq_hz = 38000;
  rmtcfg.tx_config.carrier_level = 1;
  rmtcfg.tx_config.carrier_en = 1;
  rmtcfg.tx_config.idle_level = 0;
  rmtcfg.tx_config.idle_output_en = true;
  rmtcfg.rmt_mode = RMT_MODE_TX;
  if(rmt_config(&rmtcfg) != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Error configuring IR TX");
  }
  if(rmt_driver_install(rmtcfg.channel, 0, 0) != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Error installing rmt driver for IR TX");
  }
  
  //receiver
  rmt_config_t rmt_rx;
  rmt_rx.channel = 4;
  rmt_rx.gpio_num = HAL_IO_PIN_IR_RECV;
  rmt_rx.clk_div = RMT_CLK_DIV;
  rmt_rx.mem_block_num = HAL_IO_IR_MEM_BLOCKS;
  rmt_rx.rmt_mode = RMT_MODE_RX;
  rmt_rx.rx_config.filter_en = true;
  rmt_rx.rx_config.filter_ticks_thresh = RMT_TICK_10_US * 10;
  rmt_rx.rx_config.idle_threshold = cfg->irtimeout * 1000 * (RMT_TICK_10_US);
  //in case default config is 0, use another value
  if(rmt_rx.rx_config.idle_threshold == 0)
  {
    rmt_rx.rx_config.idle_threshold = 20 * 100 * (RMT_TICK_10_US);
  }
  ESP_LOGI(LOG_TAG,"Setting IR RX idle to %d",rmt_rx.rx_config.idle_threshold);
  if(rmt_config(&rmt_rx) != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Error configuring IR RX");
  }
  if(rmt_driver_install(rmt_rx.channel, 1024, 0) != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Error installing rmt driver for IR RX");
  }
  if(xTaskCreate(halIOIRRecvTask,"irrecv",TASK_HAL_IR_RECV_STACKSIZE, 
    (void*)NULL,TASK_HAL_IR_RECV_PRIORITY, NULL) == pdPASS)
  {
    ESP_LOGD(LOG_TAG,"created IR receive task");
  } else {
    ESP_LOGE(LOG_TAG,"error creating IR receive task");
    return ESP_FAIL;
  }
  
  
  
  #if defined(DEVICE_FLIPMOUSE) && !defined(LED_USE_NEOPIXEL)
  /*++++ init RGB LEDc driver (only if Neopixels are not used and we have a FLipMouse) ++++*/
  
  //init RGB queue & ledc driver
  halIOLEDQueue = xQueueCreate(8,sizeof(uint32_t));
  ledc_timer_config_t led_timer = {
    .duty_resolution = LEDC_TIMER_10_BIT, // resolution of PWM duty
    .freq_hz = 5000,                      // frequency of PWM signal
    .speed_mode = LEDC_HIGH_SPEED_MODE,           // timer mode
    .timer_num = LEDC_TIMER_0            // timer index
  };
  // Set configuration of timer0 for high speed channels
  if(ledc_timer_config(&led_timer) != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Error in ledc_timer_config");
  }
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
    if(ledc_channel_config(&led_channel[ch]) != ESP_OK)
    {
      ESP_LOGE(LOG_TAG,"Error in ledc_channel_config");
    }
  }
  //activate fading
  ledc_fade_func_install(0);
  #endif
  
  #if defined(DEVICE_FLIPMOUSE) && defined(LED_USE_NEOPIXEL)
  /*++++ init Neopixel driver (only if we have a FLipMouse configured for Neopixels) ++++*/
  neop_buf1 = malloc(sizeof(struct led_color_t)*LED_NEOPIXEL_COUNT);
  neop_buf2 = malloc(sizeof(struct led_color_t)*LED_NEOPIXEL_COUNT);
  if(neop_buf1 == NULL || neop_buf2 == NULL)
  {
    ESP_LOGE(LOG_TAG,"Not enough memory to initialize Neopixel buffer");
    return ESP_FAIL;
  }
  
  //init remaining stuff of led strip driver struct
  led_strip.access_semaphore = xSemaphoreCreateBinary();
  led_strip.led_strip_buf_1 = neop_buf1;
  led_strip.led_strip_buf_2 = neop_buf2;
  //initialize module
  if(led_strip_init(&led_strip) == false)
  {
    ESP_LOGE(LOG_TAG,"Error initializing led strip (Neopixels)!");
    return ESP_FAIL;
  }
  #endif
  /*++++ INIT buzzer ++++*/
  //we will use the LEDC unit for the buzzer
  //because RMT has no lower frequency than 611Hz (according to example)
  halIOBuzzerQueue = xQueueCreate(32,sizeof(halIOBuzzer_t));
  ledc_timer_config_t buzzer_timer = {
    .duty_resolution = LEDC_TIMER_10_BIT, // resolution of PWM duty
    .freq_hz = 100,                      // frequency of PWM signal
    .speed_mode = LEDC_LOW_SPEED_MODE,           // timer mode
    .timer_num = LEDC_TIMER_1            // timer index
  };
  ledc_timer_config(&buzzer_timer);
  ledc_channel_config_t buzzer_channel = {
    .channel    = LEDC_CHANNEL_3,
    .duty       = 0,
    .gpio_num   = HAL_IO_PIN_BUZZER,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .timer_sel  = LEDC_TIMER_1
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
