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
 */
/**
 * @file
 * 
 * @brief This file contains the task implementation for the virtual button
 * debouncer.
 * 
 * Uses the event flags of virtualButtonsIn and if a flag is set there,
 * the debouncer waits for a defined debouncing time and maps the flags
 * to virtualButtonsOut.
 */


#include "task_debouncer.h"

/** Tag for ESP_LOG logging */
#define LOG_TAG "task_debouncer"

/** @brief Debounce timer status - IDLE
 * 
 * Timer is not running & waiting for a new command */
#define TIMER_IDLE      0

/** @brief Debounce timer status - PRESS
 * 
 * Timer is running & waiting for debounce time to elapse,
 * triggering the press flag of a virtual button. */
#define TIMER_PRESS     1

/** @brief Debounce timer status - RELEASE
 * 
 * Timer is running & waiting for debounce time to elapse,
 * triggering the release flag of a virtual button. */
#define TIMER_RELEASE   2


/** @brief Debounce timer status - DEADTIME
 * 
 * Timer is running & waiting for time to elapse,
 * during this timer is running, VBs are locked and not mapped.
 * @todo Implement the deadtime & anti-tremor functions  */
#define TIMER_DEADTIME  3

/** Define for easy access to number with eventgroup iterator (i) and VB number in group (j, 0-3)*/
#define VB_ITER(i,j)  ((i*4) + j)
/** Check for both flags, PRESS and RELEASE */
#define VB_FLAG_BOTH(x) ((1<<x) | (1<<(x+4)) )
/** Check for press flag */
#define VB_FLAG_PRESS(x) ((1<<x))
/** Check for release flag */
#define VB_FLAG_RELEASE(x) ((1<<(x+4)))

/** @brief Array of timer handles
 * 
 * This array contains all possible running timer handles.
 * A maximum of DEBOUNCERCHANNELS timers can be concurrently running.
 * 
 * @see DEBOUNCERCHANNELS
 */
TimerHandle_t xTimers[DEBOUNCERCHANNELS];

/** @brief Direction/function of the corresponding timer
 * 
 * This array signals the direction of the corresponding timer
 * of the xTimers array.
 * 
 * @see xTimers
 * @see TIMER_DEADTIME
 * @see TIMER_PRESS
 * @see TIMER_RELEASE
 * @see TIMER_IDLE
 * */
uint8_t xTimerDirection[DEBOUNCERCHANNELS];

/** @brief Time for each virtual button used for deadtime
 * 
 * This array contains the time for each virtual button, which
 * should be used for deadtime locking.
 * @todo Implement deadtime & use this array for the config switcher
 * @see TIMER_DEADTIME
 * */
uint16_t xTimeDebounceDeadtime[NUMBER_VIRTUALBUTTONS*4];

/** @brief Time for each virtual button used for press debouncing
 * 
 * This array contains the time for each virtual button, which
 * should be used for press debouncing
 * @todo Use this array for the config switcher & control it externally
 * @see TIMER_PRESS
 * */
uint16_t xTimeDebouncePress[NUMBER_VIRTUALBUTTONS*4];

/** @brief Time for each virtual button used for release debouncing
 * 
 * This array contains the time for each virtual button, which
 * should be used for release debouncing
 * @todo Use this array for the config switcher & control it externally
 * @see TIMER_RELEASE
 * */
uint16_t xTimeDebounceRelease[NUMBER_VIRTUALBUTTONS*4];

/** @brief Look for a possibly running debouncing timer
 * 
 * This method does a look-up in the xTimers array to find a running
 * timer with the given VB number (used as timer id).
 * @param virtualButton Number of VB to look for a running timer
 * @return Array offset used for xTimers array if a running timer is found, -1 if no timer was found
 * */
int8_t isDebouncerActive(uint32_t virtualButton)
{
  for(uint8_t i = 0; i<DEBOUNCERCHANNELS; i++)
  {
    if(xTimers[i] != NULL && (uint32_t)pvTimerGetTimerID(xTimers[i]) == virtualButton)
    {
      return i;
    }
  }
  return -1;
}


/** @brief Cancel a possibly running debouncing timer
 * 
 * This method does a look-up in the xTimers array to find a running
 * timer with the given VB number (used as timer id) and cancel it
 * @param virtualButton Number of VB to look for a running timer
 * @return Array offset where this running timer was found & canceled, -1 if no timer was found
 * */
int8_t cancelTimer(uint8_t virtualButton)
{
  //check all debouncerchannels for a running timer for this
  //virtual button
  for(uint8_t i = 0; i<DEBOUNCERCHANNELS; i++)
  {
    //if timer is found
    if(xTimers[i] != NULL && (uint32_t)pvTimerGetTimerID(xTimers[i]) == virtualButton)
    {
      //delete & return
      xTimerDelete(xTimers[i],0);
      xTimers[i] = NULL;
      return i;
    } 
  }
  //no timer found...
  return -1;
}

/** @brief Timer callback for debouncing
 * 
 * This callback is executed on an expired debounce timer.
 * Depending on the timer functionality (determined by xTimerDirection),
 * either the PRESS or RELEASE virtualbutton flag are set in
 * virtualButtonsOut flag group. In addition, the input flags are cleared
 * as well (virtualButtonsIn).
 * 
 * @see virtualButtonsOut
 * @see virtualButtonsIn
 * @see xTimerDirection
 * @todo Add the deadtime functionality here
 * @param xTimer Timer handle of the expired timer
 * */
void debouncerCallback(TimerHandle_t xTimer) {
  //get own ID (virtual button)
  uint8_t virtualButton = (uint32_t) pvTimerGetTimerID(xTimer);
  int8_t timerindex = isDebouncerActive(virtualButton);
  
  if(timerindex == -1)
  {
    ESP_LOGE(LOG_TAG,"Major error: debounceCallback but no timer for this button %d",virtualButton);
    return;
  }
  
  switch(xTimerDirection[timerindex])
  {
    case TIMER_IDLE:
      ESP_LOGE(LOG_TAG,"Major error: debounceCallback but timer should be idle %d",virtualButton);
      break;
    case TIMER_PRESS:
      ESP_LOGD(LOG_TAG,"Debounce finished, map in to out for press VB%d",virtualButton);
      xEventGroupSetBits(virtualButtonsOut[virtualButton/4],(1<<(virtualButton%4)));
      xEventGroupClearBits(virtualButtonsIn[virtualButton/4],(1<<(virtualButton%4)));
      if(cancelTimer(virtualButton) == -1) ESP_LOGE(LOG_TAG,"Cannot cancel timer!");
      break;
    case TIMER_RELEASE:
      ESP_LOGD(LOG_TAG,"Debounce finished, map in to out for release VB%d",virtualButton);
      xEventGroupSetBits(virtualButtonsOut[virtualButton/4],(1<<((virtualButton%4)+4)));
      xEventGroupClearBits(virtualButtonsIn[virtualButton/4],(1<<((virtualButton%4)+4)));
      if(cancelTimer(virtualButton) == -1) ESP_LOGE(LOG_TAG,"Cannot cancel timer!");
      break;
    case TIMER_DEADTIME:
      //TODO: add deadtime  
      ESP_LOGE(LOG_TAG,"TODO: add deadtime to debouncerCallback");
      break;
  }
}

/** @brief Start a debouncing timer with given debounce time and VB number
 * 
 * This method starts a new timer for the virtual button with the given
 * time. If there is no space to save the timer handle, -1 is returned and
 * no timer started.
 * 
 * @see DEBOUNCERCHANNELS
 * @see xTimers
 * @param virtualButton Number of VB to start a new timer for
 * @param debounceTime Time in [ms] for this timer to expire
 * @return -1 if no free timer slot was found (DEBOUNCERCHANNELS), the timer array index otherwise
 * */
int8_t startTimer(uint32_t virtualButton, uint16_t debounceTime)
{
  //check all debouncerchannels for a free slot.
  for(uint8_t i = 0; i<DEBOUNCERCHANNELS; i++)
  {
    //if slot is found
    if(xTimers[i] == NULL)
    {
      //create timer
      xTimers[i] = xTimerCreate("debouncer",debounceTime / portTICK_PERIOD_MS, \
        pdFALSE, (void *) virtualButton, debouncerCallback);
      if(xTimerStart(xTimers[i], 0) != pdPASS)
      {
        ESP_LOGE(LOG_TAG,"Cannot start debouncing timer!");
      }
      //return array index for further use
      return i;
    } 
  }
  //no slot is found...
  return -1;
}

/** @brief Debouncing main task
 * 
 * This task is periodically testing the virtualButtonsIn flags
 * for changes (DEBOUNCE_RESOLUTION_MS is used as delay between task
 * wakeups). If a change is detected (either press or release flag for
 * a virtualbutton is set), a timer is started.<br>
 * Either the timer expires and the flag is mapped to virtualButtonsOut
 * by debouncerCallback.<br>
 * Or the input flag is cleared again by the responsible task and the
 * debouncer cancels the timer as well (input was too short).
 * 
 * 
 * @see DEBOUNCERCHANNELS
 * @see DEBOUNCE_RESOLUTION_MS
 * @see xTimers
 * @see debouncerCallback
 * @see virtualButtonsIn
 * @see virtualButtonsOut
 * @note This task is persistently running
 * @todo Add anti-tremor & deadtime functionality
 * */
void task_debouncer(void *param)
{
  for(uint8_t i =0; i<NUMBER_VIRTUALBUTTONS;i++)
  {
    //test if eventgroup is created
    while(virtualButtonsIn[i] == 0 || virtualButtonsOut[i] == 0)
    {
      ESP_LOGE(LOG_TAG,"Eventgroup uninitialized, retry in 1s");
      vTaskDelay(1000/portTICK_PERIOD_MS);
    }
    //set timer direction to idle on startup
    xTimerDirection[i] = TIMER_IDLE;
    
    //set default debounce time for press, release and deadtime
    for(uint8_t j = 0; j<4;j++)
    {
      xTimeDebounceDeadtime[VB_ITER(i,j)] = 0;
      xTimeDebouncePress[VB_ITER(i,j)] = DEBOUNCETIME_MS;
      xTimeDebounceRelease[VB_ITER(i,j)] = DEBOUNCETIME_MS;
    }      
  }
  
  ESP_LOGD(LOG_TAG,"Debouncer started, resolution: %d ms",DEBOUNCE_RESOLUTION_MS);
  
  while(1)
  {
    //check each eventgroup and start timer accordingly
    for(uint8_t i = 0; i<NUMBER_VIRTUALBUTTONS; i++)
    {
      //check each virtual button, press (0-3) & release (4-7) flag
      //iterate over all 4 VBs each group
      for(uint8_t j = 0; j<4;j++)
      {
        //TODO TODO: avoid forwarding short edges (short release should not trigger press again).
        
        
        //get in&out event bits and mask for this virtualbutton
        EventBits_t in = xEventGroupGetBits(virtualButtonsIn[i]) & VB_FLAG_BOTH(j);
        EventBits_t out = xEventGroupGetBits(virtualButtonsOut[i]) & VB_FLAG_BOTH(j);
        int8_t timerId = isDebouncerActive(VB_ITER(i,j));
        
        //no in flag set, nothing to do.
        if(in == 0) continue;
        
        //if both flags are set, we cannot know which one was first
        //so clear both flags & don't start/stop any timer
        if(in == VB_FLAG_BOTH(j))
        {
          ESP_LOGW(LOG_TAG,"press&release set for VB %d, discarding",(i*4)+j);
          xEventGroupClearBits(virtualButtonsIn[i],VB_FLAG_BOTH(j));
          continue;
        }
        
        //if timer is not running, start one with the corresponding
        //edge and set xTimerDirection.
        if(timerId == -1) 
        {
          //check if this is a press & out flag is unset
          if(in == VB_FLAG_PRESS(j) && (out & VB_FLAG_PRESS(j)) == 0)
          {
            if(xTimeDebouncePress[VB_ITER(i,j)] > DEBOUNCE_RESOLUTION_MS)
            {
              timerId = startTimer(VB_ITER(i,j),xTimeDebouncePress[VB_ITER(i,j)]);
              if(timerId == -1) 
              {
                ESP_LOGE(LOG_TAG,"Cannot start timer...");
              } else {
                ESP_LOGD(LOG_TAG,"Press debounce started for VB%d",VB_ITER(i,j));
                xTimerDirection[timerId] = TIMER_PRESS; 
              }
              continue;
            } else {
              //if no debounce time is used
              ESP_LOGD(LOG_TAG,"Min. debounce time, just mapping press");
              //set flag in output eventgroup
              xEventGroupSetBits(virtualButtonsOut[i],VB_FLAG_PRESS(j));
              //delete flag in input eventgroup
              xEventGroupClearBits(virtualButtonsIn[i],VB_FLAG_PRESS(j));
              continue;
            }
          }
          //check if this is a release & out flag is unset
          if(in == VB_FLAG_RELEASE(j) && (out & VB_FLAG_RELEASE(j)) == 0)
          {
            if(xTimeDebounceRelease[VB_ITER(i,j)] > DEBOUNCE_RESOLUTION_MS)
            {
              timerId = startTimer(VB_ITER(i,j),xTimeDebounceRelease[VB_ITER(i,j)]);
              if(timerId == -1) 
              {
                ESP_LOGE(LOG_TAG,"Cannot start timer...");
              } else {
                ESP_LOGD(LOG_TAG,"Release debounce started for VB%d",VB_ITER(i,j));
                xTimerDirection[timerId] = TIMER_RELEASE;
              }
              continue;
            } else {
              //if no debounce time is used
              ESP_LOGD(LOG_TAG,"Min. debounce time, just mapping release");
              //set flag in output eventgroup
              xEventGroupSetBits(virtualButtonsOut[i],VB_FLAG_RELEASE(j));
              //delete flag in input eventgroup
              xEventGroupClearBits(virtualButtonsIn[i],VB_FLAG_RELEASE(j));
              continue;
            }
          }
        } else {
          //if timer is running, check if this flag requests the
          //opposite direction OR the same direction is cleared
          //if yes -> stop & delete this timer
          switch(xTimerDirection[timerId])
          {
            case TIMER_PRESS:
              //press is cleared OR release is set
              if( ((in & VB_FLAG_PRESS(j)) == 0) || ((in & VB_FLAG_RELEASE(j)) != 0) )
              {
                ESP_LOGD(LOG_TAG,"Press canceled for VB%d",VB_ITER(i,j));
                if(cancelTimer(VB_ITER(i,j)) == -1) //stop current press debouncer
                { ESP_LOGE(LOG_TAG,"Cannot cancel press timer!"); }
              }
              break;
            case TIMER_RELEASE:
              //press is set OR release is cleared
              if( ((in & VB_FLAG_PRESS(j)) != 0) || ((in & VB_FLAG_RELEASE(j)) == 0) )
              {
                ESP_LOGD(LOG_TAG,"Release canceled for VB%d",VB_ITER(i,j));
                if(cancelTimer(VB_ITER(i,j)) == -1) //stop current release debouncer
                { ESP_LOGE(LOG_TAG,"Cannot cancel release timer!"); }
              }
              break;
            case TIMER_IDLE:
              ESP_LOGE(LOG_TAG,"Timer is idle but a valid ID?");
              break;
            case TIMER_DEADTIME:
              //TODO: add deadtime
              ESP_LOGE(LOG_TAG,"TODO: add deadtime capability");
              break;
            default:
              ESP_LOGE(LOG_TAG,"Unknown status in xTimerDirection[%d]",timerId);
              break;
          }
        }
      }
    }
    
    //wait for defined resolution
    vTaskDelay(DEBOUNCE_RESOLUTION_MS / portTICK_PERIOD_MS);
  }
}
