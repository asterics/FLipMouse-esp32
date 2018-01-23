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
 * This file contains the hardware abstraction for all ADC tasks
 * Depending on the configuration either the ADC data is fed directly
 * into the mouse or joystick queue (with deadzoning, averaging and
 * all that stuff) or (in alternativ mode) is routed via virtual buttons
 * (VB)
 * 
 * Compared to the tasks in the folder "function_tasks" all HAL tasks are
 * singletons.
 * Call init to initialize every necessary data structure.
 */
 
#include "hal_adc.h"


#define LOG_TAG "hal_adc"

/** current loaded ADC task handle, used to delete & recreate an ADC task
 * @see halAdcTaskMouse
 * @see halAdcTaskJoystick
 * @see halAdcTaskThreshold */
TaskHandle_t adcHandle = NULL;

/** current activated ADC config.
 * @see adc_config_t
 * */
adc_config_t adc_conf;
/** calibration characteristics, loaded by esp-idf provided methods*/
esp_adc_cal_characteristics_t characteristics;

/** offset values, calibrated via "Calibration middle position" */
static int32_t offsetx,offsety;


void halAdcTaskMouse(void * pvParameters);
void halAdcTaskJoystick(void * pvParameters);
void halAdcTaskThreshold(void * pvParameters);



/** Reload ADC config
 * 
 * This method reloads the ADC config.
 * Depending on the configuration, a task switch might be initiated
 * to switch the mouthpiece mode from e.g., Joystick to Mouse to 
 * Alternative Mode (Threshold operated).
 * @param params New ADC config
 * @return ESP_OK on success, ESP_FAIL otherwise (wrong config, out of memory)
 * */
esp_err_t halAdcUpdateConfig(adc_config_t* params)
{
    //do some parameter validations. TBD!
    if(params == NULL)
    {
        ESP_LOGE(LOG_TAG,"cannot update config, no parameter");
        return ESP_FAIL;
    }
    
    //if new mode is different, delete previous task
    if(params->mode != adc_conf.mode)
    {
        if(adcHandle != NULL) 
        {
            ESP_LOGD(LOG_TAG, "mode change, deleting old task");
            //vTaskDelete(adcHandle);
            adcHandle = NULL;
        } else ESP_LOGW(LOG_TAG, "no valid task handle, no task deleted");
    }
        
    
    //clear pending button flags
    //TBD...
    
    //Just copy content
    memcpy(&adc_conf,params,sizeof(adc_config_t));
    
    //start task according to mode
    if(adcHandle == NULL)
    {
        switch(adc_conf.mode)
        {
            case MOUSE:
                xTaskCreate(halAdcTaskMouse,"ADC_TASK",4096,NULL,HAL_ADC_TASK_PRIORITY,&adcHandle);
                ESP_LOGD("hal_adc","created ADC task for mouse, handle %d",(uint32_t)adcHandle);
                break;
            case JOYSTICK:
                xTaskCreate(halAdcTaskJoystick,"ADC_TASK",4096,NULL,HAL_ADC_TASK_PRIORITY,&adcHandle);
                ESP_LOGD("hal_adc","created ADC task for mouse, handle %d",(uint32_t)adcHandle);
                break;
            case THRESHOLD:
                xTaskCreate(halAdcTaskThreshold,"ADC_TASK",4096,NULL,HAL_ADC_TASK_PRIORITY,&adcHandle);
                ESP_LOGD("hal_adc","created ADC task for mouse, handle %d",(uint32_t)adcHandle);
                break;
            
            default:
               ESP_LOGE("hal_adc","unknown mode (unconfigured), cannot startup task.");
               return ESP_FAIL;
        }
    }
    
    return ESP_OK;
}

void halAdcProcessPressure(uint32_t pressurevalue)
{
    //TODO: sip&puff, strongsip&puff
}

/** Mouse task for ADC
 * 
 * This task is used for the mouse moving mode of the moutpiece.
 * It is invoked on config changes and reads out all 4 sensors,
 * calculates x and y values for processing with acceleration and
 * maximum speed.
 * 
 * The calculated mouse movements are sent to the corresponding
 * mouse movement queues (either BLE, USB or BOTH).
 * 
 * */
void halAdcTaskMouse(void * pvParameters)
{
    //analog values
    uint32_t up, down, left, right, pressure;
    int32_t x,y;
    static uint16_t accelTimeX=0,accelTimeY=0;
    float tempX,tempY,moveVal, accumXpos, accumYpos;
    //todo: use a define for the delay here... (currently used: value from vTaskDelay())
    float accelFactor= 20 / 100000000.0f;
    float max_speed= adc_conf.max_speed / 10.0f;
    mouse_command_t command;
    
    while(1)
    {
        //read out the analog voltages from all 5 channels
        up = adc1_to_voltage(HAL_IO_ADC_CHANNEL_UP, &characteristics);
        down = adc1_to_voltage(HAL_IO_ADC_CHANNEL_DOWN, &characteristics);
        left = adc1_to_voltage(HAL_IO_ADC_CHANNEL_LEFT, &characteristics);
        right = adc1_to_voltage(HAL_IO_ADC_CHANNEL_RIGHT, &characteristics);
        pressure = adc1_to_voltage(HAL_IO_ADC_CHANNEL_PRESSURE, &characteristics);
        
        //TODO: use gain?!?
        //left = left * adc_conf.gainLeft / 50;
        //right = right * adc_conf.gainRight / 50;
        //up = up * adc_conf.gainUp / 50;
        //down = down * adc_conf.gainDown / 50;
        
        //subtract offsets
        tempX = (float)(left - right) - offsetx;
        tempY = (float)(up - down) - offsety;
        
        //apply deadzone
        if (tempX<-adc_conf.deadzone_x) tempX+=adc_conf.deadzone_x;
        else if (tempX>adc_conf.deadzone_x) tempX-=adc_conf.deadzone_x;
        else tempX=0.0;
        if (tempY<-adc_conf.deadzone_y) tempY+=adc_conf.deadzone_y;
        else if (tempY>adc_conf.deadzone_y) tempY-=adc_conf.deadzone_y;
        else tempY=0.0;
  
        //apply acceleration
        if (tempX==0) accelTimeX=0;
        else if (accelTimeX < ACCELTIME_MAX) accelTimeX+=adc_conf.acceleration;
        if (tempY==0) accelTimeY=0;
        else if (accelTimeY < ACCELTIME_MAX) accelTimeY+=adc_conf.acceleration;
                
        //calculate the current X movement by using acceleration, accel factor and sensitivity
        moveVal = tempX * adc_conf.sensitivity_x * accelFactor * accelTimeX;
        //limit value
        if (moveVal>adc_conf.max_speed) moveVal=adc_conf.max_speed;
        if (moveVal< -adc_conf.max_speed) moveVal=-adc_conf.max_speed;
        //add to accumulated movement value
        accumXpos+=moveVal;
        
        //do the same calculations for Y axis
        moveVal = tempY * adc_conf.sensitivity_y * accelFactor * accelTimeY;
        if (moveVal>adc_conf.max_speed) moveVal=adc_conf.max_speed;
        if (moveVal< -adc_conf.max_speed) moveVal=-adc_conf.max_speed;
        accumYpos+=moveVal;
        
        //cast to int again
        int xMove = (int)accumXpos;
        int yMove = (int)accumYpos;
        //limit to int8 values (to fit into mouse report)
        if(xMove > 127) xMove = 127;
        if(xMove < -127) xMove = -127;
        if(yMove > 127) yMove = 127;
        if(yMove < -127) yMove = -127;
        
        //if at least one value is != 0, send to mouse.
        if ((xMove != 0) || (yMove != 0))
        {
            command.x = xMove;
            command.y = yMove;
            accumXpos -= xMove;
            accumYpos -= yMove;
            
            //post values to mouse queue (USB and/or BLE)
            if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_USB)
            {
                ESP_LOGD(LOG_TAG,"Sending mouse X/Y (USB): %d/%d",xMove,yMove);
                xQueueSend(mouse_movement_usb,&command,0);
            }
            
            if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_BLE)
            {
                ESP_LOGD(LOG_TAG,"Sending mouse X/Y (BLE): %d/%d",xMove,yMove);
                xQueueSend(mouse_movement_ble,&command,0);
            }
        }  
                
        //pressure sensor is handled in another function
        halAdcProcessPressure(pressure);
        
        //delay the task.
        vTaskDelay(20/portTICK_PERIOD_MS); 
    }
}

/** Joystick task for ADC
 * 
 * This task is used for the joystick mode of the moutpiece.
 * It is invoked on config changes and reads out all 4 sensors,
 * calculates 2 axis values (depending on config which ones)
 * for processing with sensitivity
 * 
 * The calculated joystick movements are sent to the corresponding
 * joystick command queues (either BLE, USB or BOTH).
 * 
 * */
void halAdcTaskJoystick(void * pvParameters)
{
    //analog values
    uint32_t up, down, left, right, pressure;
    int32_t x,y;
    joystick_command_t command;
    
    while(1)
    {
        //read out the analog voltages from all 5 channels
        up = adc1_to_voltage(HAL_IO_ADC_CHANNEL_UP, &characteristics);
        down = adc1_to_voltage(HAL_IO_ADC_CHANNEL_DOWN, &characteristics);
        left = adc1_to_voltage(HAL_IO_ADC_CHANNEL_LEFT, &characteristics);
        right = adc1_to_voltage(HAL_IO_ADC_CHANNEL_RIGHT, &characteristics);
        pressure = adc1_to_voltage(HAL_IO_ADC_CHANNEL_PRESSURE, &characteristics);
        
        //if a value is outside threshold, activate debounce timer
        x = (int32_t)(left - right) - offsetx;
        y = (int32_t)(up - down) - offsety;
        
        //TODO: acceleration & max speed calc
        
        //TODO: do everything...
        
        //post values to mouse queue (USB and/or BLE)
        if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_USB)
        {
            xQueueSend(joystick_movement_usb,&command,0);
        }
        
        if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_BLE)
        {
            xQueueSend(joystick_movement_ble,&command,0);
        }
                
        //pressure sensor is handled in another function
        halAdcProcessPressure(pressure);
        
        //delay the task.
        vTaskDelay(20/portTICK_PERIOD_MS); 
    }
}

/** calibration funktion
 * 
 * This method is called to calibrate the offset value for x and y
 * axis of the mouthpiece.
 **/
void halAdcCalibrate(void)
{
    return;
}


/**
 * Function task for triggering calibration of up/down/left/right input.
 * 
 * If a calibration is triggered by the virtual button, the method
 * halAdcCalibrate is called to measure the ADC values and create a new
 * calibration value.
 * 
 * @addtogroup Function Tasks
 * @param param No parameter needed, except virtual button number
 * @see taskNoParameterConfig_t
 * @see halAdcCalibrate
 * */
void task_calibration(taskNoParameterConfig_t *param)
{
    EventBits_t uxBits = 0;
    if(param == NULL)
    {
        ESP_LOGE(LOG_TAG,"parameter = NULL, cannot proceed");
        vTaskDelete(NULL);
        return;
    }
    
    //calculate array index of EventGroup array (each 4 VB have an own EventGroup)
    uint8_t evGroupIndex = param->virtualButton / 4;
    //calculate bitmask offset within the EventGroup
    uint8_t evGroupShift = param->virtualButton % 4;
    //final pointer to the EventGroup used by this task
    EventGroupHandle_t *evGroup = NULL;
    //ticks between task timeouts
    const TickType_t xTicksToWait = 2000 / portTICK_PERIOD_MS;
    //local VB
    uint8_t vb = param->virtualButton;
    
    if(vb != VB_SINGLESHOT)
    {
        //check for correct offset
        if(evGroupIndex >= NUMBER_VIRTUALBUTTONS)
        {
            ESP_LOGE(LOG_TAG,"virtual button group unsupported: %d ",evGroupIndex);
            vTaskDelete(NULL);
            return;
        }
        
        //test if event groups are already initialized, otherwise exit immediately
        while(virtualButtonsOut[evGroupIndex] == 0)
        {
            ESP_LOGE("task_calib","uninitialized event group for virtual buttons, retry in 1s");
            vTaskDelay(1000/portTICK_PERIOD_MS);
        } 
        //save final event group for later
        evGroup = virtualButtonsOut[evGroupIndex];
    }

    while(1)
    {
        //in single shot mode, just trigger calibration & return
        if(vb == VB_SINGLESHOT)
        {
            halAdcCalibrate();
            return;
        }
        //wait for the flag
        uxBits = xEventGroupWaitBits(evGroup,(1<<evGroupShift),pdTRUE,pdFALSE,xTicksToWait);
        //test for a valid set flag (else branch would be timeout)
        if(uxBits & (1<<evGroupShift))
        {
            halAdcCalibrate();
        }
    }
}

/** Threshold task for ADC
 * 
 * This task is used for threshold mode of the moutpiece.
 * It is invoked on config changes and reads out all 4 sensors,
 * calculates x and y values and compares it to thresholds.
 * 
 * If one value exceeds the threshold, the corresponding virtual button
 * flags are set or cleared.
 * 
 * */
void halAdcTaskThreshold(void * pvParameters)
{
    //analog values
    uint32_t up, down, left, right, pressure;
    int32_t x,y;
    uint8_t firedx = 0,firedy = 0;
    
    while(1)
    {
        //read out the analog voltages from all 5 channels
        up = adc1_to_voltage(HAL_IO_ADC_CHANNEL_UP, &characteristics);
        down = adc1_to_voltage(HAL_IO_ADC_CHANNEL_DOWN, &characteristics);
        left = adc1_to_voltage(HAL_IO_ADC_CHANNEL_LEFT, &characteristics);
        right = adc1_to_voltage(HAL_IO_ADC_CHANNEL_RIGHT, &characteristics);
        pressure = adc1_to_voltage(HAL_IO_ADC_CHANNEL_PRESSURE, &characteristics);
        
        //if a value is outside threshold, activate debounce timer
        x = (int32_t)(left - right) - offsetx;
        y = (int32_t)(up - down) - offsety;
        
        //LEFT/RIGHT value exceeds threshold (deadzone) value?
        if(abs(x) > adc_conf.deadzone_x)
        {
            //if yes, check if not set already (avoid lot of load)
            if(firedx == 0)
            {
                ESP_LOGD("hal_adc","X-axis fired in alternative mode");
                //set either left or right bit in debouncer input event group
                if(x < 0) xEventGroupSetBits(virtualButtonsIn[VB_LEFT/4],(1<<(VB_LEFT%4)));
                if(x > 0) xEventGroupSetBits(virtualButtonsIn[VB_RIGHT/4],(1<<(VB_RIGHT%4)));
                //remember that we alread set the flag
                firedx = 1;
            }
        } else {
            //below threshold, clear the one-time flag setting variable
            firedx = 0;
            //also clear the debouncer event bits
            xEventGroupClearBits(virtualButtonsIn[VB_LEFT/4],(1<<(VB_LEFT%4)));
            xEventGroupClearBits(virtualButtonsIn[VB_RIGHT/4],(1<<(VB_RIGHT%4)));
        }
        
        //UP/DOWN value exceeds threshold (deadzone) value?
        if(abs(y) > adc_conf.deadzone_y)
        {
            //if yes, check if not set already (avoid lot of load)
            if(firedy == 0)
            {
                ESP_LOGD("hal_adc","Y-axis fired in alternative mode");
                //set either left or right bit in debouncer input event group
                if(y < 0) xEventGroupSetBits(virtualButtonsIn[VB_UP/4],(1<<(VB_UP%4)));
                if(y > 0) xEventGroupSetBits(virtualButtonsIn[VB_DOWN/4],(1<<(VB_DOWN%4)));
                //remember that we alread set the flag
                firedy = 1;
            }
        } else {
            //below threshold, clear the one-time flag setting variable
            firedy = 0;
            //also clear the debouncer event bits
            xEventGroupClearBits(virtualButtonsIn[VB_UP/4],(1<<(VB_UP%4)));
            xEventGroupClearBits(virtualButtonsIn[VB_DOWN/4],(1<<(VB_DOWN%4)));
        }
        
        //pressure sensor is handled in another function
        halAdcProcessPressure(pressure);
        
        //delay the task.
        vTaskDelay(20/portTICK_PERIOD_MS); 
    }
    
}

/** Init the ADC driver module
 * 
 * This method initializes the HAL ADC driver with the given config
 * Depending on the configuration, different tasks are initialized
 * to switch the mouthpiece mode from e.g., Joystick to Mouse to 
 * Alternative Mode (Threshold operated).
 * @param params ADC config for intialization
 * @return ESP_OK on success, ESP_FAIL otherwise (wrong config, no memory, already initialized)
 * */
esp_err_t halAdcInit(adc_config_t* params)
{
    esp_err_t ret;
    //Init ADC and Characteristics
    ret = adc1_config_width(ADC_WIDTH_BIT_12);
    if(ret != ESP_OK) { ESP_LOGE("hal_adc","Error setting channel width"); return ret; }
    ret = adc1_config_channel_atten(HAL_IO_ADC_CHANNEL_UP, ADC_ATTEN_DB_11);
    if(ret != ESP_OK) { ESP_LOGE("hal_adc","Error setting atten on channel %d",HAL_IO_ADC_CHANNEL_UP); return ret; }
    ret = adc1_config_channel_atten(HAL_IO_ADC_CHANNEL_DOWN, ADC_ATTEN_DB_11);
    if(ret != ESP_OK) { ESP_LOGE("hal_adc","Error setting atten on channel %d",HAL_IO_ADC_CHANNEL_DOWN); return ret; }
    ret = adc1_config_channel_atten(HAL_IO_ADC_CHANNEL_LEFT, ADC_ATTEN_DB_11);
    if(ret != ESP_OK) { ESP_LOGE("hal_adc","Error setting atten on channel %d",HAL_IO_ADC_CHANNEL_LEFT); return ret; }
    ret = adc1_config_channel_atten(HAL_IO_ADC_CHANNEL_RIGHT, ADC_ATTEN_DB_11);
    if(ret != ESP_OK) { ESP_LOGE("hal_adc","Error setting atten on channel %d",HAL_IO_ADC_CHANNEL_RIGHT); return ret; }
    ret = adc1_config_channel_atten(HAL_IO_ADC_CHANNEL_PRESSURE, ADC_ATTEN_DB_11);
    if(ret != ESP_OK) { ESP_LOGE("hal_adc","Error setting atten on channel %d",HAL_IO_ADC_CHANNEL_PRESSURE); return ret; }
    esp_adc_cal_get_characteristics(ADC_CAL_IDEAL_V_REF, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, &characteristics);
    
    //check if every queue is already initialized
    for(uint8_t i = 0; i<NUMBER_VIRTUALBUTTONS; i++)
    {
        if(virtualButtonsIn[i] == 0)
        {
            ESP_LOGE("hal_adc","virtualButtonsIn uninitialized, exiting");
            return ESP_FAIL;
        }
        if(virtualButtonsOut[i] == 0)
        {
            ESP_LOGE("hal_adc","virtualButtonsOut uninitialized, exiting");
            return ESP_FAIL;
        }
    }
    if(joystick_movement_usb == NULL || joystick_movement_ble == NULL || \
        mouse_movement_ble == NULL || mouse_movement_usb == NULL)
    {
        ESP_LOGE("hal_adc","queue uninitialized, exiting");
        return ESP_FAIL;
    }
    
    //not initializing full config, only ADC
    if(params == NULL) return ESP_OK;
    
    //init remaining parts by updating config.
    return halAdcUpdateConfig(params);
}
