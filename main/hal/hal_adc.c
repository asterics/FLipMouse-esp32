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
 * @todo Do Strong<Sip/puff>+<UP/DOWN/LEFT/RIGHT>
 * */
 
#include "hal_adc.h"

/** Tag for ESP_LOG logging */
#define LOG_TAG "hal_adc"

typedef struct adcData {
    uint32_t up;
    uint32_t down;
    uint32_t left;
    uint32_t right;
    uint32_t pressure;
} adcData_t;

/** current loaded ADC task handle, used to delete & recreate an ADC task
 * @see halAdcTaskMouse
 * @see halAdcTaskJoystick
 * @see halAdcTaskThreshold */
TaskHandle_t adcHandle = NULL;

/** current activated ADC config.
 * @see adc_config_t
 * */
adc_config_t adc_conf;

/** @brief Semaphore for ADC readings.
 * 
 * This semaphore is used to switch between halADCTask<Joystick,Mouse,Threshold>
 * and calibration.
 * In addition, it is used for strong sip&puff + up/down/left/right.
 * */
SemaphoreHandle_t adcSem = NULL;

/** calibration characteristics, loaded by esp-idf provided methods*/
esp_adc_cal_characteristics_t characteristics;

/** offset values, calibrated via "Calibration middle position" */
static int32_t offsetx,offsety;

/*
void halAdcTaskMouse(void * pvParameters);
void halAdcTaskJoystick(void * pvParameters);
void halAdcTaskThreshold(void * pvParameters);*/

/** @brief Report raw values via serial interface
 * 
 * All values are sent in predefined string: <br>
 * VALUES:\<pressure\>,\<up\>,\<down\>,\<left\>,\<right\>,\<x\>,\<y\> \\r \\n
 * 
 * @param up Up value
 * @param down Down value
 * @param left Left value
 * @param right Right value
 * @param x X value
 * @param y Y value
 * @param pressure Pressure value
 * */
void halAdcReportRaw(uint32_t up, uint32_t down, uint32_t left, uint32_t right, uint32_t pressure, int32_t x, int32_t y)
{
    ///@todo change back to 32
    #define REPORT_RAW_COUNT 2
    static int prescaler = 0;
    
    if(adc_conf.reportraw != 0)
    {
        if(prescaler % REPORT_RAW_COUNT == 0)
        {
            char data[40];
            sprintf(data,"VALUES:%d,%d,%d,%d,%d,%d,%d\r\n",pressure,up,down,left,right,x,y);
            halSerialSendUSBSerial(HAL_SERIAL_TX_TO_CDC,data, strlen(data), 10);
        }
        prescaler++;
    }
}

/** @brief Read out analog voltages & apply sensor gain
 * 
 * Internally used function for sensor readings & applying the
 * sensor gain
 * @param values Pointer to struct of all analog values
 */
void halAdcReadData(adcData_t *values)
{
    //read all sensors
    uint32_t tmp = 0;
    uint32_t up = adc1_to_voltage(HAL_IO_ADC_CHANNEL_UP, &characteristics);
    uint32_t down = adc1_to_voltage(HAL_IO_ADC_CHANNEL_DOWN, &characteristics);
    uint32_t left = adc1_to_voltage(HAL_IO_ADC_CHANNEL_LEFT, &characteristics);
    uint32_t right = adc1_to_voltage(HAL_IO_ADC_CHANNEL_RIGHT, &characteristics);
    values->pressure = adc1_to_voltage(HAL_IO_ADC_CHANNEL_PRESSURE, &characteristics);

    //do the mouse rotation
    switch (adc_conf.orientation) {
      case 90: tmp=up; up=left; left=down; down=right; right=tmp; break;
      case 180: tmp=up; up=down; down=tmp; tmp=right; right=left; left=tmp; break;
      case 270: tmp=up; up=right; right=down; down=left; left=tmp;break;
    }
        
    //apply individual sensor gain
    values->up = up * adc_conf.gain[0] / 50;
    values->down = down * adc_conf.gain[1] / 50;
    values->left = left * adc_conf.gain[2] / 50;
    values->right = right * adc_conf.gain[3] / 50;
}

/** @brief Process pressure sensor (sip & puff)
 * @todo Do everything here, no sip&puff currently available.
 * @param pressurevalue Currently measured pressure.
 * */
void halAdcProcessPressure(uint32_t pressurevalue)
{
    generalConfig_t *cfg = configGetCurrent();
    if(cfg == NULL) return;
    
    //SIP triggered
    if(pressurevalue < cfg->adc.threshold_sip && \
        pressurevalue > cfg->adc.threshold_strongsip)
    {
        
    }
    //STRONGSIP triggered
    if(pressurevalue < cfg->adc.threshold_strongsip)
    {
        
    }
    
    //PUFF triggered
    if(pressurevalue > cfg->adc.threshold_puff && \
        pressurevalue < cfg->adc.threshold_strongpuff)
    {
        
    }
    
    //STRONGPUFF triggered
    if(pressurevalue > cfg->adc.threshold_strongpuff)
    {
        
    }
}

/** @brief HAL TASK - Mouse task for ADC
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
    adcData_t D;
    //int32_t x,y;
    static uint16_t accelTimeX=0,accelTimeY=0;
    int32_t tempX,tempY;
    float moveVal, accumXpos = 0, accumYpos = 0;
    //todo: use a define for the delay here... (currently used: value from vTaskDelay())
    float accelFactor= 20 / 100000000.0f;
    float max_speed= adc_conf.max_speed / 10.0f;
    mouse_command_t command;
    TickType_t xLastWakeTime;
    
    while(1)
    {
        //get mutex
        if(xSemaphoreTake(adcSem, (TickType_t) 30) != pdTRUE)
        {
            ESP_LOGW(LOG_TAG,"Cannot obtain mutex for reading");
            continue;
        }
        
        //read out the analog voltages from all 5 channels
        halAdcReadData(&D);
        
        //subtract offsets
        tempX = (D.left - D.right) - offsetx;
        tempY = (D.up - D.down) - offsety;
        
        
        //apply deadzone
        if (tempX<-adc_conf.deadzone_x) tempX+=adc_conf.deadzone_x;
        else if (tempX>adc_conf.deadzone_x) tempX-=adc_conf.deadzone_x;
        else tempX=0;
        
        if (tempY<-adc_conf.deadzone_y) tempY+=adc_conf.deadzone_y;
        else if (tempY>adc_conf.deadzone_y) tempY-=adc_conf.deadzone_y;
        else tempY=0;
        
        //report raw values.
        halAdcReportRaw(D.up, D.down, D.left, D.right, D.pressure, tempX, tempY);
  
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
        tempX = accumXpos;
        tempY = accumYpos;
        
        //limit to int8 values (to fit into mouse report)
        if(tempX > 127) tempX = 127;
        if(tempX < -127) tempX = -127;
        if(tempY > 127) tempY = 127;
        if(tempY < -127) tempY = -127;
        
        //if at least one value is != 0, send to mouse.
        if ((tempX != 0) || (tempY != 0))
        {
            command.x = tempX;
            command.y = tempY;
            accumXpos -= tempX;
            accumYpos -= tempY;
            
            //post values to mouse queue (USB and/or BLE)
            if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_USB)
            {
                xQueueSend(mouse_movement_usb,&command,0);
            }
            
            if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_BLE)
            {
                xQueueSend(mouse_movement_ble,&command,0);
            }
        }  
                
        //pressure sensor is handled in another function
        halAdcProcessPressure(D.pressure);
        
        
        //give mutex
        xSemaphoreGive(adcSem);
        
        //delay the task.
        vTaskDelayUntil( &xLastWakeTime, 20/portTICK_PERIOD_MS);
    }
}

/** @brief HAL TASK - Joystick task for ADC
 * 
 * This task is used for the joystick mode of the moutpiece.
 * It is invoked on config changes and reads out all 4 sensors,
 * calculates 2 axis values (depending on config which ones)
 * for processing with sensitivity
 * 
 * The calculated joystick movements are sent to the corresponding
 * joystick command queues (either BLE, USB or BOTH).
 * 
 * @todo Implement the full joystick interface. Currently completely unimplemented.
 * 
 * */
void halAdcTaskJoystick(void * pvParameters)
{
    //analog values
    adcData_t D;
    int32_t x,y;
    joystick_command_t command;
    TickType_t xLastWakeTime;
    
    while(1)
    {
        //get mutex
        if(xSemaphoreTake(adcSem, (TickType_t) 30) != pdTRUE)
        {
            ESP_LOGW(LOG_TAG,"Cannot obtain mutex for reading");
            continue;
        }
        
        //read out the analog voltages from all 5 channels
        halAdcReadData(&D);
        
        //if a value is outside threshold, activate debounce timer
        x = (int32_t)(D.left - D.right) - offsetx;
        y = (int32_t)(D.up - D.down) - offsety;
        halAdcReportRaw(D.up, D.down, D.left, D.right, D.pressure, x, y);
        
        //TODO: acceleration & max speed calc
        
        //TODO: do everything...
        
        //post values to mouse queue (USB and/or BLE)
        /*if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_USB)
        {
            xQueueSend(joystick_movement_usb,&command,0);
        }
        
        if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_BLE)
        {
            xQueueSend(joystick_movement_ble,&command,0);
        }*/
                
        //pressure sensor is handled in another function
        halAdcProcessPressure(D.pressure);
        
        //give mutex
        xSemaphoreGive(adcSem);
        
        //delay the task.
        vTaskDelayUntil(&xLastWakeTime, 20/portTICK_PERIOD_MS); 
    }
}

/** @brief Calibration function
 * 
 * This method is called to calibrate the offset value for x and y
 * axis of the mouthpiece.
 * Either triggered by the functional task task_calibration or on a
 * config change.
 * @note Can be called directly.
 **/
void halAdcCalibrate(void)
{
    //get mutex
    if(xSemaphoreTake(adcSem, (TickType_t) 20))
    {
        ESP_LOGI(LOG_TAG,"Starting calibration, offsets: %d/%d",offsetx,offsety);
        uint32_t up = 0;
        uint32_t down = 0;
        uint32_t left = 0;
        uint32_t right = 0;
        //read values itself & accumulate (8sensor readings)
        for(uint8_t i = 0; i<8; i++)
        {
            up += adc1_to_voltage(HAL_IO_ADC_CHANNEL_UP, &characteristics);
            down += adc1_to_voltage(HAL_IO_ADC_CHANNEL_DOWN, &characteristics);
            left += adc1_to_voltage(HAL_IO_ADC_CHANNEL_LEFT, &characteristics);
            right += adc1_to_voltage(HAL_IO_ADC_CHANNEL_RIGHT, &characteristics);
            vTaskDelay(2);
        }
        
        //divide (by shift, easy for 8 readings)
        up = up >> 3;
        down = down >> 3;
        left = left >> 3;
        right = right >> 3;
        //set as offset values
        offsetx = left - right;
        offsety = up - down;
        
        ESP_LOGI(LOG_TAG,"Finished calibration, offsets: %d/%d",offsetx,offsety);
        
        //give mutex to enable tasks again
        xSemaphoreGive(adcSem);
    } else {
        ESP_LOGE(LOG_TAG,"Cannot calibrate, no mutex");
    }
    
    return;
}



/** @brief FUNCTIONAL TASK - Trigger zero-point calibration of mouthpiece
 * 
 * This task is used to trigger a zero-point calibration of
 * up/down/left/right input.
 * 
 * @param param Task configuration
 * @see halAdcCalibrate
 * @see taskNoParameterConfig_t*/
void task_calibration(taskNoParameterConfig_t *param)
{
    EventBits_t uxBits = 0;
    if(param == NULL)
    {
        ESP_LOGE(LOG_TAG,"parameter = NULL, cannot proceed");
        while(1) vTaskDelay(100000/portTICK_PERIOD_MS);
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
            while(1) vTaskDelay(100000/portTICK_PERIOD_MS);
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

/** @brief HAL TASK - Threshold task for ADC
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
    adcData_t D;
    int32_t x,y;
    uint8_t firedx = 0,firedy = 0;
    TickType_t xLastWakeTime;
    
    while(1)
    {
        //get mutex
        if(xSemaphoreTake(adcSem, (TickType_t) 30) != pdTRUE)
        {
            ESP_LOGW(LOG_TAG,"Cannot obtain mutex for reading");
            continue;
        }
        
        //read out the analog voltages from all 5 channels
        halAdcReadData(&D);
        
        //if a value is outside threshold, activate debounce timer
        x = (int32_t)(D.left - D.right) - offsetx;
        y = (int32_t)(D.up - D.down) - offsety;
        
        //LEFT/RIGHT value exceeds threshold (deadzone) value?
        if(abs(x) > adc_conf.deadzone_x)
        {
            //if yes, check if not set already (avoid lot of load)
            if(firedx == 0)
            {
                ESP_LOGD("hal_adc","X-axis fired in alternative mode");
                //set either left or right bit in debouncer input event group
                if(x < 0) 
                {
                    xEventGroupSetBits(virtualButtonsIn[VB_LEFT/4],(1<<(VB_LEFT%4)));
                    x = -1;
                }
                if(x > 0) 
                {
                    xEventGroupSetBits(virtualButtonsIn[VB_RIGHT/4],(1<<(VB_RIGHT%4)));
                    x = 1;
                }
                //remember that we alread set the flag
                firedx = 1;
            }
        } else {
            x = 0;
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
                if(y < 0) 
                {
                    xEventGroupSetBits(virtualButtonsIn[VB_UP/4],(1<<(VB_UP%4)));
                    y = -1;
                }
                if(y > 0) 
                {
                    xEventGroupSetBits(virtualButtonsIn[VB_DOWN/4],(1<<(VB_DOWN%4)));
                    y = 1;
                }
                //remember that we alread set the flag
                firedy = 1;
            }
        } else {
            y = 0;
            //below threshold, clear the one-time flag setting variable
            firedy = 0;
            //also clear the debouncer event bits
            xEventGroupClearBits(virtualButtonsIn[VB_UP/4],(1<<(VB_UP%4)));
            xEventGroupClearBits(virtualButtonsIn[VB_DOWN/4],(1<<(VB_DOWN%4)));
        }
        
        halAdcReportRaw(D.up, D.down, D.left, D.right, D.pressure, x, y);
        
        //pressure sensor is handled in another function
        halAdcProcessPressure(D.pressure);
        
        //give mutex
        xSemaphoreGive(adcSem);
        
        //delay the task.
        vTaskDelayUntil(&xLastWakeTime, 20/portTICK_PERIOD_MS); 
    }
    
}


/** @brief Reload ADC config
 * 
 * This method reloads the ADC config.
 * Depending on the configuration, a task switch might be initiated
 * to switch the mouthpiece mode from e.g., Joystick to Mouse to 
 * Alternative Mode (Threshold operated).
 * @param params New ADC config
 * @todo Clear pending VB flags on a config switch
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
    
    //acquire mutex
    if(xSemaphoreTake(adcSem, (TickType_t) 30) != pdTRUE)
    {
        ESP_LOGW(LOG_TAG,"Cannot obtain mutex for config update");
        return ESP_FAIL;
    }
    
    //if new mode is different, delete previous task
    if(params->mode != adc_conf.mode)
    {
        if(adcHandle != NULL) 
        {
            ESP_LOGD(LOG_TAG, "mode change, deleting old task");
            vTaskDelete(adcHandle);
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
                ESP_LOGD(LOG_TAG,"created ADC task for mouse, handle %d",(uint32_t)adcHandle);
                break;
            case JOYSTICK:
                xTaskCreate(halAdcTaskJoystick,"ADC_TASK",4096,NULL,HAL_ADC_TASK_PRIORITY,&adcHandle);
                ESP_LOGD(LOG_TAG,"created ADC task for joystick, handle %d",(uint32_t)adcHandle);
                break;
            case THRESHOLD:
                xTaskCreate(halAdcTaskThreshold,"ADC_TASK",4096,NULL,HAL_ADC_TASK_PRIORITY,&adcHandle);
                ESP_LOGD(LOG_TAG,"created ADC task for threshold, handle %d",(uint32_t)adcHandle);
                break;
            
            default:
               ESP_LOGE(LOG_TAG,"unknown mode (unconfigured), cannot startup task.");
               return ESP_FAIL;
        }
    } else {
        ESP_LOGI(LOG_TAG,"ADC config reloaded without task switch");
    }
    
    ESP_LOG_BUFFER_HEXDUMP(LOG_TAG,&adc_conf,sizeof(adc_config_t),ESP_LOG_DEBUG);
    //give mutex
    xSemaphoreGive(adcSem);
    return ESP_OK;
}


/** @brief Init the ADC driver module
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
    ret = adc1_config_width(ADC_WIDTH_BIT_10);
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
    
    //initialize ADC semphore as mutex
    adcSem = xSemaphoreCreateMutex();
    
    //not initializing full config, only ADC
    if(params == NULL) return ESP_OK;
    
    //init remaining parts by updating config.
    return halAdcUpdateConfig(params);
}

