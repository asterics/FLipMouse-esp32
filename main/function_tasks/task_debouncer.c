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
 * Copyright 2019 Benjamin Aigner <aignerb@technikum-wien.at,
 * beni@asterics-foundation.org>
 */
/**
 * @file
 * @brief CONTINOUS TASK - This file contains the task implementation for the virtual button
 * debouncer.
 * 
 * This debouncer task (task_debouncer) waits for incoming events (of
 * type raw_action_t)on the debouncer_in queue. If an incoming event
 * is registered, a timer (esp_timer) will be started. On a finished
 * debounce event, the corresponding event is sent to the system event
 * loop.
 * 
 * The debouncing itself can be controlled via following variables
 * (these settings are located in the global config):
 * 
 * * Press debounce time
 * * Release debounce time
 * * Deadtime between two consecutive actions
 * 
 * If one of these values is set for one dedicated VB, it will be used.
 * If there is no setting, the device's default will be used. If the
 * there is no default setting for this device, a hardcoded debounce time
 * of DEBOUNCETIME_MS is used.
 * 
 * @note For each VB, a debouncer_cfg_t element will be used. The size
 * of this array is determined by VB_MAX. Please set this define accordingly!
 * 
 * @see VB_MAX
 * @see DEBOUNCETIME_MS
 * @see debouncer_in
 * @see raw_action_t
 * @see generalConfig_t
 * 
 */


#include "task_debouncer.h"

/** @brief Tag for ESP_LOG logging */
#define LOG_TAG "task_debouncer"

/** @brief Debouncer log level */
#define LOG_LEVEL_DEBOUNCE ESP_LOG_INFO

/** @brief Debounce timer, current direction for this timer
 * When a debounce timer is started, a parameter struct of
 * type debouncer_param_t is used for starting this timer.
 * It includes the given direction of the timer as it is defined here.
 * @see debouncer_param_t
 */
typedef enum {
  TIMER_IDLE,
  /** @brief Timer is in idle and should not be triggering */
  TIMER_PRESS,
  /** @brief Debouncer timer is active for a press event */
  TIMER_RELEASE,
  /** @brief Debouncer timer is active for a release event */
  TIMER_DEADTIME,
  /** @brief Debouncer timer is active for a deadtime between next event
   * (lock-out) */
  TIMER_ERROR
  /** @brief We have an error. Might be a virtualbutton out of range. */
} debouncer_direction_t;


/** @brief Timer config struct
 * This struct is used to control one timer for debouncing */
typedef struct debouncer_cfg {
  debouncer_direction_t dir;
  /** @brief Direction/function for this timer */
  uint32_t vb;
  /** @brief VB number for this timer */
  esp_timer_handle_t handle;
  /** @brief Timer handle for starting/stopping */
} debouncer_cfg_t;

/** @brief Start a timer with a given config and debounce time
 * @param cfg Config for the timer to be started
 * @param debounceTime Timeout for the new timer, unit: [ms]
 * @return ESP_OK on success, ESP_FAIL if timer cannot be started */
esp_err_t startTimer(debouncer_cfg_t *cfg, uint16_t debounceTime);

/** @brief Array of timer configs
 * 
 * This array contains all possible running timer handles.
 * A maximum of DEBOUNCERCHANNELS timers can be concurrently running.
 * 
 * @see DEBOUNCERCHANNELS
 * @see debouncer_cfg_t
 */
debouncer_cfg_t xTimers[VB_MAX];


/** @brief Look for a possibly running debouncing timer
 * 
 * This method does a look-up in the xTimers array to return the
 * timer status for the given VB number (stored within the debouncer_cfg_t struct).
 * @see debouncer_cfg_t
 * @param virtualButton Number of VB to look for a running timer
 * @return Timer direction of this VB.
 * */
debouncer_direction_t isDebouncerActive(uint32_t virtualButton)
{
  if(virtualButton >= VB_MAX) return TIMER_ERROR;
  return xTimers[virtualButton].dir;
}


/** @brief Cancel a possibly running debouncing timer
 * 
 * This method does a look-up in the xTimers array to find a running
 * timer with the given VB number (used as timer id) and cancel it
 * @param virtualButton Number of VB to look for a running timer, use
 * VB_MAX to cancel all timers.
 * @param stop Should we stop the timer before deleting (if set != 0)
 * @return Array offset where this running timer was found & canceled,\
 *  -1 if no timer was found (or all were cleared)
 * @note If VB_MAX is given, all timers are canceled.
 * */
int cancelTimer(uint32_t virtualButton, uint8_t stop)
{
  //if we have VB_MAX here, cancel all of them
  if(virtualButton >= VB_MAX)
  {
    for(int i = 0; i<VB_MAX; i++)
    {
      if(xTimers[i].handle != NULL)
      {
        if(stop != 0) esp_timer_stop(xTimers[i].handle);
        esp_timer_delete(xTimers[i].handle);
        xTimers[i].handle = NULL;
      }
      xTimers[i].dir = TIMER_IDLE;
    }
    ESP_LOGD(LOG_TAG,"Canceled ALL timers");
    return -1;
  }
  
  //else: cancel only requested timer
  if(xTimers[virtualButton].handle != NULL)
  {
    if(stop != 0)
    {
      if(esp_timer_stop(xTimers[virtualButton].handle) != ESP_OK)
      { ESP_LOGW(LOG_TAG,"Error stopping timer %d",virtualButton); }
    }
    if(esp_timer_delete(xTimers[virtualButton].handle) != ESP_OK)
    { ESP_LOGW(LOG_TAG,"Error deleting timer %d",virtualButton); }
    xTimers[virtualButton].handle = NULL; //set handle to NULL, so we remember a stopped timer
    xTimers[virtualButton].dir = TIMER_IDLE;
    return virtualButton;
  } else {
    ESP_LOGD(LOG_TAG,"Cannot cancel, no timer");
    //no timer found...
    return -1;
  }
}

/** @brief Send feedback on pressed buttons to host (for button learning)
 * 
 * This method sends back to the host which buttons are pressed, if enabled.
 * Can be enabled by "AT BL 1" and disabled by "AT BL 0".
 * 
 * @param vb Virtual button which was triggered
 * @param press See vb_event_t
 * @param cfg Pointer to current config
 * @see generalConfig_t.button_learn
 * @see vb_event_t
 * */
void sendButtonLearn(uint32_t vb, vb_event_t press, generalConfig_t *cfg)
{
  if(cfg == NULL) return;
  char str[13] = {0};
  
  //send only if enabled 
  if(cfg->button_learn != 0)
  {
    switch(press)
    {
      case VB_PRESS_EVENT: sprintf(str,"%d PRESS",vb); break;
      case VB_RELEASE_EVENT: sprintf(str,"%d RELEASE",vb); break;
      default: break;
    }
    halSerialSendUSBSerial(str,strnlen(str,13),20);
  }
  return;
}

/** @brief Timer callback for debouncing
 * 
 * This callback is executed on an expired debounce timer.
 * Depending on the timer functionality (determined by the argument struct),
 * either the PRESS or RELEASE events are sent via the event loop.
 * 
 * @see debouncer_cfg_t
 * @todo Add/test the deadtime functionality here
 * @param arg Timer argument, of type debouncer_cfg_t
 * */
void debouncerCallback(void *arg) {
  //get own ID (virtual button)
  debouncer_cfg_t *debcfg = (debouncer_cfg_t *)arg;
  
  if(debcfg == NULL)
  {
    ESP_LOGE(LOG_TAG,"Illegal argument");
    return;
  }
  
  uint32_t virtualButton = debcfg->vb;
  generalConfig_t *cfg = configGetCurrent();
  uint16_t deadtime = 0;
  
  if(cfg == NULL)
  {
    ESP_LOGE(LOG_TAG,"Cannot do deadtime for VB %d, global config is NULL!",virtualButton);
    deadtime = 0;
  } else {
    //check if an individual deadtime is set
    if(cfg->debounce_idle_vb[virtualButton] != 0) deadtime = cfg->debounce_idle_vb[virtualButton];
    //check if we have to use global deadtime
    if(deadtime == 0 && cfg->debounce_idle != 0) deadtime = cfg->debounce_idle;
  }

  switch(debcfg->dir)
  {
    case TIMER_IDLE:
      ESP_LOGE(LOG_TAG,"Major error: debounceCallback but timer should be idle %d",virtualButton);
      return;
    case TIMER_PRESS:
      ESP_LOGD(LOG_TAG,"Debounce finished, map in to out for press VB%d",virtualButton);
      //map in to out and clear from in...
      if(esp_event_post(VB_EVENT,VB_PRESS_EVENT,(void*)&virtualButton,sizeof(uint32_t),0) != ESP_OK)
      {
        ESP_LOGW(LOG_TAG,"Cannot post event!");
      }
      //stop timer
      if(cancelTimer(virtualButton,0) == -1) ESP_LOGE(LOG_TAG,"Cannot cancel timer (CB)!");
      //send feedback to host, if enabled
      sendButtonLearn(virtualButton,VB_PRESS_EVENT,cfg);
      break;
    case TIMER_RELEASE:
      ESP_LOGD(LOG_TAG,"Debounce finished, map in to out for release VB%d",virtualButton);
      //map in to out and clear from in...
      if(esp_event_post(VB_EVENT,VB_RELEASE_EVENT,(void*)&virtualButton,sizeof(uint32_t),0) != ESP_OK)
      {
        ESP_LOGW(LOG_TAG,"Cannot post event!");
      }
      //stop timer
      if(cancelTimer(virtualButton,0) == -1) ESP_LOGE(LOG_TAG,"Cannot cancel timer (CB)!");
      //send feedback to host, if enabled
      sendButtonLearn(virtualButton,VB_RELEASE_EVENT,cfg);
      break;
    case TIMER_DEADTIME:
      ESP_LOGD(LOG_TAG,"Deadtime finished, ready");
      if(cancelTimer(virtualButton,0) == -1) ESP_LOGE(LOG_TAG,"Cannot cancel timer (CB)!");
      return;
    default: return;
  }
  //if necessary, start timer again for deadtime.
  if(deadtime != 0)
  {
    if(cancelTimer(virtualButton,0) == -1) ESP_LOGE(LOG_TAG,"Cannot cancel timer (CB)!");
    debcfg->dir = TIMER_DEADTIME;
    if(startTimer(debcfg,deadtime) != ESP_OK) 
    {
      ESP_LOGE(LOG_TAG,"Cannot start timer... (CB)");
    } else {
      ESP_LOGD(LOG_TAG,"Deadtime start for VB %d",virtualButton);
    }
  }
}

/** @brief Start a timer with a given config and debounce time
 * @param cfg Config for the timer to be started
 * @param debounceTime Timeout for the new timer, unit: [ms]
 * @return ESP_OK on success, ESP_FAIL if timer cannot be started */
esp_err_t startTimer(debouncer_cfg_t *cfg, uint16_t debounceTime)
{
  //check if there is a valid VB number (used as index)
  if(cfg->vb >= VB_MAX) return ESP_FAIL;
  
  //if there is a timer running, cancel it before.
  if(isDebouncerActive(cfg->vb) != TIMER_IDLE) cancelTimer(cfg->vb,1);
  
  //create the timer
  esp_timer_create_args_t args;
  xTimers[cfg->vb].dir = cfg->dir;
  args.callback = debouncerCallback; //always the same callback
  args.arg = (void *)&xTimers[cfg->vb]; //we assign a pointer to this array member
  args.dispatch_method = ESP_TIMER_TASK; //no other option possible
  esp_err_t ret = esp_timer_create(&args,&xTimers[cfg->vb].handle);
  
  if(ret != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Cannot create timer, ret: %d",ret);
    return ESP_FAIL;
  }
  
  //now start this timer (only one-shot)
  //we need to multiply the given time (in [ms]) to have the
  //necessary [us] parameter.
  ret = esp_timer_start_once(xTimers[cfg->vb].handle, debounceTime * 1000);
  
  if(ret != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Cannot start timer, ret: %d",ret);
    return ESP_FAIL;
  }
  
  //yeah, everything fine...
  return ESP_OK;
}

/** @brief Debouncing main task
 * 
 * This task is pending on raw actions, sent to the debouncer_in queue.
 * If there is a raw action posted, this task either:
 * * Starts a new timer (no timer is running)
 * * Cancels a running timer (timer is running in the opposite debouncer direction)
 * * Does nothing (timer is already running in the same direction)
 * 
 * @see DEBOUNCERCHANNELS
 * @see DEBOUNCE_RESOLUTION_MS
 * @see xTimers
 * @see debouncerCallback
 * @todo Add anti-tremor & deadtime functionality
 * */
void task_debouncer(void *param)
{
  generalConfig_t *cfg = configGetCurrent();
  raw_action_t evt;
  debouncer_cfg_t debcfg;
  debcfg.handle = NULL;
  uint16_t time = 0;
  esp_log_level_set(LOG_TAG,LOG_LEVEL_DEBOUNCE);
  
  //test if eventgroup is created
  while(debouncer_in == NULL)
  {
    ESP_LOGE(LOG_TAG,"Eventgroup uninitialized, retry in 1s");
    vTaskDelay(1000/portTICK_PERIOD_MS);
  }
  //clear the xTimers array
  for(int i = 0; i<VB_MAX; i++)
  {
    xTimers[i].dir = TIMER_IDLE;
    xTimers[i].handle = NULL;
    xTimers[i].vb = i;
  }
  
  //wait until config is valid
  while(cfg == NULL)
  {
    ESP_LOGE(LOG_TAG,"Global config uninitialized, retry in 1s");
    vTaskDelay(1000/portTICK_PERIOD_MS);
    cfg = configGetCurrent();
  }
  
  ESP_LOGI(LOG_TAG,"Debouncer started");

  while(1)
  {
    //if config updates are running, cancel all timers and wait for stable config
    if((xEventGroupGetBits(systemStatus) & SYSTEM_STABLECONFIG) == 0)
    {
      //cancel all timers
      cancelTimer(VB_MAX,1);
      //clear all VB events
      xQueueReset(debouncer_in);
      //wait 5 ticks to check again
      //If not set in time, wait again
      if((xEventGroupWaitBits(systemStatus,SYSTEM_STABLECONFIG, \
        pdFALSE,pdFALSE,5) & SYSTEM_STABLECONFIG) == 0)
      {
        ESP_LOGD(LOG_TAG,"Waiting for config");
        continue;
      }
    }
    
    if(xQueueReceive(debouncer_in,&evt,portMAX_DELAY) == pdTRUE)
    {
      if(evt.vb >= VB_MAX)
      {
        ESP_LOGE(LOG_TAG,"VB out of range!");
        continue;
      }
      //if timer is not running, start one with the corresponding
      //edge and set xTimerDirection.
      if(isDebouncerActive(evt.vb) == TIMER_IDLE) 
      {
        //check which time to use (either VB, global value or default)
        uint8_t t_type = TIMER_IDLE;
        
        switch(evt.type)
        {
          case VB_PRESS_EVENT:
            t_type = TIMER_PRESS;
            //is a VB value set in config?
            if(cfg->debounce_press_vb[evt.vb] != 0) time = cfg->debounce_press_vb[evt.vb];
            //is a global value set in config?
            if(time == 0 && cfg->debounce_press != 0) time = cfg->debounce_press;
            //no? just use the default value
            if(time == 0) time = DEBOUNCETIME_MS;
          break;
          case VB_RELEASE_EVENT:
            t_type = TIMER_RELEASE;
            //is a VB value set in config?
            if(cfg->debounce_release_vb[evt.vb] != 0) time = cfg->debounce_release_vb[evt.vb];
            //is a global value set in config?
            if(time == 0 && cfg->debounce_release != 0) time = cfg->debounce_release;
            //no? just use the default value
            if(time == 0) time = DEBOUNCETIME_MS;
          break;
          default: break;
        }
        if(time > DEBOUNCETIME_MIN_MS)
        {
          debcfg.vb = evt.vb;
          debcfg.dir = t_type;
          if(startTimer(&debcfg,time) != ESP_OK) ESP_LOGE(LOG_TAG,"Cannot start timer...");
          else {
            ESP_LOGD(LOG_TAG,"Debounce started for VB%d / T: %d",evt.vb,t_type);
          }
          continue;
        //note: currently, following branch is unused, but maybe we need a directly mapped VB.
        } else {
          //if no debounce time is used
          ESP_LOGD(LOG_TAG,"Map VB%d / T: %d",evt.vb,evt.type);
          if(esp_event_post(VB_EVENT,evt.type,(void*)&evt.vb,sizeof(evt.vb),0) != ESP_OK)
          {
            ESP_LOGW(LOG_TAG,"Cannot post event!");
          }
          continue;
        }
      } else {
        //if timer is running, check if this flag requests the
        //opposite direction OR the same direction is cleared
        //if yes -> stop & delete this timer
        switch(xTimers[evt.vb].dir)
        {
          case TIMER_PRESS:
            //if release is wanted, but press timer is running
            //->cancel timer
            if(evt.type == VB_RELEASE_EVENT)
            {
              ESP_LOGD(LOG_TAG,"Press canceled for VB%d, sending release",evt.vb);
              if(cancelTimer(evt.vb,1) == -1) //stop current press debouncer
              { ESP_LOGE(LOG_TAG,"Cannot cancel press timer!"); }
              ///@note We send here an additional release, just to be sure
              /// to release any actions (avoiding sticky actions for keys...)
              if(esp_event_post(VB_EVENT,evt.type,(void*)&evt.vb,sizeof(evt.vb),0) != ESP_OK)
              {
                ESP_LOGW(LOG_TAG,"Cannot post event!");
              }
            }
            break;
          case TIMER_RELEASE:
            //if press is wanted, but release timer is running
            //->cancel timer
            ///@note I think we should cancel only in the event of a
            /// set anti-tremor time. Otherwise we might loose e.g., key
            /// release events -> sticky keys...
            time = 0;
            //is a VB value set in config?
            if(cfg->debounce_release_vb[evt.vb] != 0) time = cfg->debounce_release_vb[evt.vb];
            //is a global value set in config?
            if(time == 0 && cfg->debounce_release != 0) time = cfg->debounce_release;
            if(evt.type == VB_PRESS_EVENT && time != 0)
            {
              ESP_LOGD(LOG_TAG,"Release canceled for VB%d",evt.vb);
              if(cancelTimer(evt.vb,1) == -1) //stop current press debouncer
              { ESP_LOGE(LOG_TAG,"Cannot cancel release timer!"); }
            }
            break;
          case TIMER_IDLE:
            ESP_LOGE(LOG_TAG,"Timer is idle but a valid ID?");
            break;
          case TIMER_DEADTIME:
            ESP_LOGD(LOG_TAG,"Deadtime active, waiting.");
            break;
          case TIMER_ERROR:
            ESP_LOGE(LOG_TAG,"Timer is in error state [%d]",evt.vb);
          default:
            ESP_LOGE(LOG_TAG,"Unknown status in xTimers[%d].dir",evt.vb);
            break;
        }
      } /* else -> timerId != TIMER_IDLE */
    } /* if(xQueueReceive... */
  } /* while(1) */
} /* task_debouncer */
