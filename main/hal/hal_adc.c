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
 * @todo Add CIM reporting (if we implement it at all)
 * */
 
#include "hal_adc.h"

/** @brief Tag for ESP_LOG logging */
#define LOG_TAG "hal_adc"

/** @brief ADC log level */
#define LOG_LEVEL_ADC ESP_LOG_ERROR

/** @brief Define how much ADC readings are skipped until one is printed */
#define HAL_ADC_RAW_DIVIDER 16

/** @brief Current strong sip&puff + mouthpiece mode
 * 
 * It is possible to add another 8 virtual buttons by triggering
 * a strong sip or puff and move the mouthpiece in one of four directions.
 * 
 * @note If the VB for strong sip/puff is assigned (!= "no command"), it is not possible to
 * use strong sip/puff + up/down/left/right!
 */
typedef enum strong_action {STRONG_NORMAL,STRONG_PUFF,STRONG_SIP} strong_action_t;

typedef struct adcData {
    uint32_t up;
    uint32_t down;
    uint32_t left;
    uint32_t right;
    uint32_t pressure;
    int32_t x;
    int32_t y;
    uint8_t calibrate_request;
    strong_action_t strongmode;
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

/** @brief Last calibrate tick time
 * 
 * This value is used to determine the last time calibration was started.
 * 
 * @see HAL_ADC_CALIB_LOCKTIME*/
TickType_t adcCalibLast = 0;

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

/** @brief Timer for strong mode timeout
 * This timer is used for a timeout moving back to STRONG_NORMAL if
 * we entered a STRONG_PUFF or STRONG_SIP mode and no action was triggered*/
TimerHandle_t adcStrongTimeoutTimerHandle;

/** @brief Timer for strong mode delay
 * This timer is used for waiting before checking for the additional action
 * (up,down,left,right)*/
TimerHandle_t adcStrongTimerHandle;

/** @brief Semaphore, to signal a short delay for checking strong mode */
SemaphoreHandle_t adcStrongSem;

/** @brief Validate the input value and replace with default if not matching
 * @param value Value to be validated
 * @param min Minimum value. If "value" is below this value, "default" is returned
 * @param max Maximum value. If "value" is above this value, "default" is returned
 * @param defaultValue Default value if parameter "value" is not within given range
 * @return Fixed value
 * @note Works only for unsigned values! */
uint32_t validate(uint32_t value, uint32_t min, uint32_t max, uint32_t defaultValue)
{
    if(value>max) return defaultValue;
    //we need to test if min != 0, otherwise i
    //if(min != 0 && value<min) return defaultValue;
    if(value<min) return defaultValue;
    return value;
}

/** @brief Trigger strong sip/puff + action according to input data
 * 
 * This method is used to trigger VBs for actions of type STRONG_SIP or
 * STRONG_PUFF combined with an additional mouthpiece movement.
 * It should only be triggered if there is no VB_STRONGPUFF / VB_STRONGSIP
 * action is defined (this is handled in halAdcProcessPressure ).
 * 
 * We use 2 RTOS timers here: One for a delay between entering strong mode
 * and triggering an action. The second timer is used for a timeout,
 * if no additional action is triggered and we should leave the strong
 * mode.
 * 
 * @see halAdcProcessPressure
 * @see VB_STRONGPUFF
 * @see VB_STRONGSIP
 * @see VB_STRONGSIP_UP
 * @see VB_STRONGSIP_DOWN
 * @see VB_STRONGSIP_LEFT
 * @see VB_STRONGSIP_RIGHT
 * @see VB_STRONGPUFF_UP
 * @see VB_STRONGPUFF_DOWN
 * @see VB_STRONGPUFF_LEFT
 * @see VB_STRONGPUFF_RIGHT
 * @param D ADC data from calling task
 * */
void halAdcProcessStrongMode(adcData_t *D)
{
    raw_action_t evt;
    //on a FABI device, we cannot do this combination with analog values.
    //maybe it will be done on another firmware part.
    #ifdef DEVICE_FABI
        D->strongmode = STRONG_NORMAL;
        return;
    #endif
    
    //cannot do anything here if we are not in a special mode.
    if(D->strongmode == STRONG_NORMAL) return;
    
    #ifdef DEVICE_FLIPMOUSE
    if(adcStrongTimerHandle == NULL || adcStrongTimeoutTimerHandle == NULL)
    {
        ESP_LOGE(LOG_TAG,"Strong mode timer uninitialized!!!");
        return;
    }
    
    //timer is not started
    if(xTimerIsTimerActive(adcStrongTimeoutTimerHandle) == pdFALSE)
    {
        //than we need to start a timer for the timeout moving back to NORMAL
        xTimerReset(adcStrongTimeoutTimerHandle,0);
        //in addition, we start a timer to wait for a defined delay
        xTimerReset(adcStrongTimerHandle,0);
        //return in this case & wait for next iteration
        return;
    }
    
    //check if we have a movement in any direction
    if(D->x != 0 || D->y != 0)
    {
        //check if it is possible to trigger the next action
        //(delay has passed -> adcStrongSem is free)
        if(uxSemaphoreGetCount(adcStrongSem) == 0) return;
        xSemaphoreTake(adcStrongSem,0);
        
        //cancel timer for idle time
        xTimerStop(adcStrongTimerHandle,0);
    
        //if yes, trigger action (depending on SIP/PUFF mode)
        if(D->strongmode == STRONG_PUFF)
        {
            if(abs(D->x) > abs(D->y))
            {
                //x has higher values -> use LEFT/RIGHT
                if(D->x > 0) evt.vb = VB_STRONGPUFF_RIGHT;
                else evt.vb = VB_STRONGPUFF_LEFT;
                evt.type = VB_PRESS_EVENT;
                xQueueSendToBack(debouncer_in,&evt,0);
                ESP_LOGI(LOG_TAG,"Exit STRONG: PUFF + LEFT/RIGHT");
            } else {
                //y has higher values -> use UP/DOWN
                if(D->y > 0) evt.vb = VB_STRONGPUFF_DOWN;
                else evt.vb = VB_STRONGPUFF_UP;
                evt.type = VB_PRESS_EVENT;
                xQueueSendToBack(debouncer_in,&evt,0);
                ESP_LOGI(LOG_TAG,"Exit STRONG: PUFF + UP/DOWN");
                
            }
            //stop the timeout timer
            xTimerStop(adcStrongTimeoutTimerHandle,0);
            //reset strong mode after sending the action
            D->strongmode = STRONG_NORMAL;
        }
        if(D->strongmode == STRONG_SIP)
        {
            if(abs(D->x) > abs(D->y))
            {
                //x has higher values -> use LEFT/RIGHT
                if(D->x > 0) evt.vb = VB_STRONGSIP_RIGHT;
                else evt.vb = VB_STRONGSIP_LEFT;
                evt.type = VB_PRESS_EVENT;
                xQueueSendToBack(debouncer_in,&evt,0);
                ESP_LOGI(LOG_TAG,"Exit STRONG: SIP + LEFT/RIGHT");
            } else {
                //y has higher values -> use UP/DOWN
                if(D->y > 0) evt.vb = VB_STRONGSIP_DOWN;
                else evt.vb = VB_STRONGSIP_UP;
                evt.type = VB_PRESS_EVENT;
                xQueueSendToBack(debouncer_in,&evt,0);
                ESP_LOGI(LOG_TAG,"Exit STRONG: SIP + UP/DOWN");
            }
            //stop the timeout timer
            xTimerStop(adcStrongTimeoutTimerHandle,0);
            //reset strong mode after sending the action
            D->strongmode = STRONG_NORMAL;
        }
    }  
    #endif
}


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
    #define REPORT_RAW_COUNT 8
    static int prescaler = 0;
    
    if(adc_conf.reportraw != 0)
    {
        if(prescaler % REPORT_RAW_COUNT == 0)
        {
            char data[48];
            sprintf(data,"VALUES:%d,%d,%d,%d,%d,%d,%d",pressure,up,down,left,right,x,y);
            halSerialSendUSBSerial(data, strnlen(data,48), 0);
        }
        prescaler++;
    }
}

#ifdef DEVICE_FABI
/** @brief Read out analog voltages (sip/puff only) - FABI
 * 
 * Internally used function for sensor readings & applying the
 * sensor gain.
 * Furthermore, X&Y values are calculated by subtracting left/right and
 * up/down, offset is used as well. 
 * In addition, the deadzone is calculated as well (based on an elliptic curve).
 * 
 * @note You need to take adcSem before calling this function!
 * 
 * @param values Pointer to struct of all analog values
 * @see adcData_t
 * @see adcSem
 */
void halAdcReadData(adcData_t *values)
{
    //read all sensors
    int32_t tmp = 0;
    int32_t pressure = 0;

    #ifdef HAL_IO_ADC_CHANNEL_PRESSURE
        pressure = adc1_get_raw(HAL_IO_ADC_CHANNEL_PRESSURE);
    #endif
    if(pressure == -1) 
    { 
        ESP_LOGE(LOG_TAG,"Cannot read channel pressure"); return;
    } else { 
        //save value
        values->pressure= pressure;       
    }
}

#endif /* DEVICE_FABI */

#ifdef DEVICE_FLIPMOUSE
/** @brief Read out analog voltages, apply sensor gain and deadzone - FLipMouse
 * 
 * Internally used function for sensor readings & applying the
 * sensor gain.
 * Furthermore, X&Y values are calculated by subtracting left/right and
 * up/down, offset is used as well. 
 * In addition, the deadzone is calculated as well (based on an elliptic curve).
 * 
 * @note You need to take adcSem before calling this function!
 * 
 * @param values Pointer to struct of all analog values
 * @return 0 if measurement was done, -1 if discarded
 * @see adcData_t
 * @see adcSem
 */
int halAdcReadData(adcData_t *values)
{
    //read all sensors
    int32_t tmp = 0;
    int32_t x,y;
    int32_t up,down,left,right,pressure;
    static int32_t up_p,down_p,left_p,right_p,pressure_p;
    up = down = left = right = pressure = 0;
    ///@see HAL_ADC_RAW_DIVIDER
    static uint32_t debug_out_cnt = 0;
    
    //read sensor data via I2C.
    uint8_t adc[10];
    uint8_t *adc2 = adc;
    int rcv = halSerialReceiveI2CADC(&adc2);
    if(rcv != 10)
    {
        ESP_LOGW(LOG_TAG,"I2C recv: 0x%X",rcv);
    }
    left = (int32_t)((uint16_t)(adc[2] + (adc[3] << 8)));
    right = (int32_t)((uint16_t)(adc[6] + (adc[7] << 8)));
    up = (int32_t)((uint16_t)(adc[4] + (adc[5] << 8)));
    down = (int32_t)((uint16_t)(adc[0] + (adc[1] << 8)));
    pressure = (int32_t)((uint16_t)(adc[8] + (adc[9] << 8)));
    //ESP_LOGE(LOG_TAG,"%d,%d,%d,%d,%d",up,down,left,right,pressure);
    
    //before any further processing, check if we have an unusual
    //sensor reading (deviation to last measuring > 200)
    //in this case, discard this measurement and wait for next iteration
    //if it is a wanted high mouthpiece movement, next iteration will be
    //triggering the action (we save the last value, even if discarded)
    int toohigh = 0;
    if((abs(left-left_p) > 200) || (abs(right-right_p) > 200) || (abs(up-up_p) > 200) || \
		(abs(down-down_p) > 200) || (abs(pressure-pressure_p) > 200)) { toohigh = 1; }
		
	left_p = left;
	right_p = right;
	up_p = up;
	down_p = down;
	pressure_p = pressure;
	if(toohigh != 0)
	{
		ESP_LOGW(LOG_TAG,"sensor deviation over rate,discarding");
		return -1;
	}

    //do the mouse rotation
    switch (adc_conf.orientation) {
      case 90: tmp=up; up=left; left=down; down=right; right=tmp; break;
      case 180: tmp=up; up=down; down=tmp; tmp=right; right=left; left=tmp; break;
      case 270: tmp=up; up=right; right=down; down=left; left=tmp;break;
    }
    
    //save raw values to data struct
    values->left = left;
    values->right = right;
    values->up = up;
    values->down = down;
    
    //process pressure
    values->pressure = pressure;
    
    //calculate X and Y values
    x = (left - right) - offsetx;
    y = (up - down) - offsety;
    
    #if HAL_IO_ADC_ELLIPTIC_DEADZONE == 0
    
    //in this case, we use a rectangle deadzone
    
    if (x<-adc_conf.deadzone_x) x+=adc_conf.deadzone_x;  // apply deadzone values x direction
    else if (x>adc_conf.deadzone_x) x-=adc_conf.deadzone_x;
    else x=0;
          
    if (y<-adc_conf.deadzone_y) y+=adc_conf.deadzone_y;  // apply deadzone values y direction
    else if (y>adc_conf.deadzone_y) y-=adc_conf.deadzone_y;
    else y=0;
    
    //otherwise, x&y is 0
    values->x = x;
    values->y = y;
    
    #else
    
    //in this case, we use an elliptic deadzone
    
    //apply elliptic deadzone
    //formula:
    //https://sebadorn.de/2012/01/02/herausfinden-ob-ein-punkt-in-einer-ellipse-liegt
    //A point in an elliptic curve:
    //(px - cx)² / rx² + (py - cy)² / ry² <= 1
    //cx/cy is 0 (we start at 0/0)
    //py/py is our x/y
    //rx/ry are deadzone values
    //to shorten the formulas
    uint8_t a = adc_conf.deadzone_x;
    uint8_t b = adc_conf.deadzone_y;
    float status = pow(x,2) / pow(a,2) + pow(y,2) / pow(b,2);
    
    //check if point is outside deadzone
    if(status > 1.0)
    {
        //if outside deadzone, subtract ellipse itself to start with
        //0 for a movement
        
        //formula for ellipse point:
        //https://math.stackexchange.com/questions/22064/calculating-a-point-that-lies-on-an-ellipse-given-an-angle
        float deadzoneX = 0;
        float deadzoneY = 0;
       
        //angle of the given mouthpiece
        //calculate only if x&y is != 0
        if((x < 0 || x > 0) && (y < 0 || y > 0))
        {
            float angle = atan(y/x);
            deadzoneX = abs((a*b)/sqrt(pow(b,2)+pow(a,2)*pow(tan(angle),2)));
            deadzoneY = abs((a*b)/sqrt(pow(a,2)+ pow(b,2)/pow(tan(angle),2)));
        }
              
        //subtract calculated ellipse coordinates from output X/Y values
        if(x > 0)
        {
            values->x = x - (int)deadzoneX;
        } else {
            values->x = x + (int)deadzoneX;
        }
        if(y > 0)
        {
            values->y = y - (int)deadzoneY;
        } else {
            values->y = y + (int)deadzoneY;
        }
    } else {
        //otherwise, x&y is 0
        values->x = 0;
        values->y = 0;
    }
    #endif 
    if(debug_out_cnt++%HAL_ADC_RAW_DIVIDER == 0)
    {
        ESP_LOGD(LOG_TAG,"raw x/y %d/%d; ",values->x,values->y);
    }
    return 0;
}
#endif /* DEVICE_FLIPMOUSE */

/** @brief Process pressure sensor (sip & puff)
 * 
 * This function takes care of pressure handling.
 * If the pressure value is within the sip/puff range (but not 
 * in strong range), a VB_SIP/VB_PUFF action is triggered.
 * 
 * For STRONG_SIP/STRONG_PUFF, the handling depends on the settings
 * for these buttons (example for puff, the same applies for sip): <br>
 * If VB_STRONGPUFF_UP/DOWN/LEFT/RIGHT is active, the device enters
 * the strong mode. In this case, strong_mode will be set to sip/puff.
 * Further handling is done in halAdcProcessStrongMode.
 * If VB_STRONGPUFF_UP/DOWN/LEFT/RIGHT is unset (none of these are active),
 * the VB_STRONGPUFF action is triggered immediately. 
 * 
 * @see halAdcProcessStrongMode
 * @param D Currently measured ADC data.
 * */
void halAdcProcessPressure(adcData_t *D)
{
    //track changes and fire event only if not issued already.
    //this is done individually for SIP, PUFF, STRONG_SIP & STRONG_PUFF
    //0 means nothing is fired, 1 is a fired press action, 2 is a fired release action
    static uint8_t fired[4] = {0,0,0,0};
    //currently measured pressure value
    uint32_t pressurevalue = D->pressure;
    //currently active general config
    generalConfig_t *cfg = configGetCurrent();
    //the raw event, sent to the debouncer
    raw_action_t evt;
    //cannot proceed if no global config is available
    if(cfg == NULL) return;
    
    //if we are in a special mode don't process further (only on FLipMouse)
    #ifdef DEVICE_FLIPMOUSE
        if(D->strongmode != STRONG_NORMAL) return;
    #endif
    
    //SIP triggered
    if(pressurevalue < cfg->adc.threshold_sip && \
        pressurevalue > cfg->adc.threshold_strongsip)
    {
        if(fired[0] != 1)
        {
            //set/clear VBs
            evt.type = VB_PRESS_EVENT;
            evt.vb = VB_SIP;
            xQueueSendToBack(debouncer_in,&evt,0);
            //save fired state
            fired[0] = 1;
        }
    } else {
        if(fired[0] == 1)
        {
            //set/clear VBs
            evt.type = VB_RELEASE_EVENT;
            evt.vb = VB_SIP;
            xQueueSendToBack(debouncer_in,&evt,0);
            //track fired state
            fired[0] = 0;
        }
    }
    
    //STRONGSIP triggered
    if(pressurevalue < cfg->adc.threshold_strongsip)
    {
        //check if strong sip + up/down/left/right is set.
        //if this is the case, we will proceed with strong mode.
        //Otherwise the VB_STRONGSIP will be triggered.
        #ifdef DEVICE_FLIPMOUSE
        if(handler_hid_active(VB_STRONGSIP_UP) || handler_vb_active(VB_STRONGSIP_UP) || \
            handler_hid_active(VB_STRONGSIP_DOWN) || handler_vb_active(VB_STRONGSIP_DOWN) || \
            handler_hid_active(VB_STRONGSIP_LEFT) || handler_vb_active(VB_STRONGSIP_LEFT) || \
            handler_hid_active(VB_STRONGSIP_RIGHT) || handler_vb_active(VB_STRONGSIP_RIGHT))
        {
            //if at least one strong action is defined, enter strong sip mode
            D->strongmode = STRONG_SIP;
            ESP_LOGI(LOG_TAG,"Enter STRONG SIP");
            TONE(TONE_STRONGSIP_ENTER_FREQ,TONE_STRONGSIP_ENTER_DURATION);
        } else {
        #endif
            if(fired[1] != 1)
            {
                //make a tone
                //TONE(TONE_STRONGSIP_ENTER_FREQ,TONE_STRONGSIP_ENTER_DURATION);
                //no strong sip + <yy> action is defined, trigger strong sip VB.
                
                evt.type = VB_PRESS_EVENT;
                evt.vb = VB_STRONGSIP;
                xQueueSendToBack(debouncer_in,&evt,0);
                //save fired stated
                fired[1] = 1;
            }
        #ifdef DEVICE_FLIPMOUSE
        }
        #endif
    } else {
        if(fired[1] != 2)
        {
            //set/clear VBs
            evt.type = VB_RELEASE_EVENT;
            evt.vb = VB_STRONGSIP;
            xQueueSendToBack(debouncer_in,&evt,0);
            //track fired state
            fired[1] = 2;
        }
    }
    
    //PUFF triggered
    if(pressurevalue > cfg->adc.threshold_puff && \
        pressurevalue < cfg->adc.threshold_strongpuff)
    {
        
        if(fired[2] != 1)
        {
            //set/clear VBs
            evt.type = VB_PRESS_EVENT;
            evt.vb = VB_PUFF;
            xQueueSendToBack(debouncer_in,&evt,0);
            //save fired state
            fired[2] = 1;
        }
    } else {
        if(fired[2] == 1)
        {
            //set/clear VBs
            evt.type = VB_RELEASE_EVENT;
            evt.vb = VB_PUFF;
            xQueueSendToBack(debouncer_in,&evt,0);
            //track fired state
            fired[2] = 0;
        }
    }
    
    //STRONGPUFF triggered
    if(pressurevalue > cfg->adc.threshold_strongpuff)
    {
        //check if strong puff + up/down/left/right is set.
        //if this is the case, we will proceed with strong mode.
        //Otherwise the VB_STRONGPUFF will be triggered.
        #ifdef DEVICE_FLIPMOUSE
        if(handler_hid_active(VB_STRONGPUFF_UP) || handler_vb_active(VB_STRONGPUFF_UP) || \
            handler_hid_active(VB_STRONGPUFF_DOWN) || handler_vb_active(VB_STRONGPUFF_DOWN) || \
            handler_hid_active(VB_STRONGPUFF_LEFT) || handler_vb_active(VB_STRONGPUFF_LEFT) || \
            handler_hid_active(VB_STRONGPUFF_RIGHT) || handler_vb_active(VB_STRONGPUFF_RIGHT))
        {
            //if at least one strong action is defined, enter strong puff mode
            D->strongmode = STRONG_PUFF;
            ESP_LOGI(LOG_TAG,"Enter STRONG PUFF");
            TONE(TONE_STRONGPUFF_ENTER_FREQ,TONE_STRONGPUFF_ENTER_DURATION);
        } else {
        #endif
            if(fired[3] != 1)
            {
                //make a tone
                //TONE(TONE_STRONGPUFF_ENTER_FREQ,TONE_STRONGPUFF_ENTER_DURATION);
                //either no strong puff + <yy> action is defined or strong
                // is used, trigger strong puff VB.
                evt.type = VB_PRESS_EVENT;
                evt.vb = VB_STRONGPUFF;
                xQueueSendToBack(debouncer_in,&evt,0);
                //save fired state
                fired[3] = 1;
            }
            
        #ifdef DEVICE_FLIPMOUSE
        }
        #endif
    } else {
        if(fired[3] != 2)
        {
            //set/clear VBs
            evt.type = VB_RELEASE_EVENT;
            evt.vb = VB_STRONGPUFF;
            xQueueSendToBack(debouncer_in,&evt,0);
            //track fired state
            fired[3] = 2;
        }
    }
}

#ifdef DEVICE_FLIPMOUSE
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
 * @note This task is not available on a FABI device.
 * @see DEVICE_FABI
 * @see DEVICE_FLIPMOUSE
 * */
void halAdcTaskMouse(void * pvParameters)
{
    //analog values
    adcData_t D;
    D.strongmode = STRONG_NORMAL;
    //int32_t x,y;
    static uint16_t accelTimeX=0,accelTimeY=0;
    int32_t tempX,tempY;
    float moveVal, accumXpos = 0, accumYpos = 0;
    float accelFactor= 20 / 100000000.0f;
    hid_cmd_t command,command2;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    //set adc data reference for timer
    vTimerSetTimerID(adcStrongTimeoutTimerHandle,&D);
    uint32_t debug_out_cnt = 0;
    
    while(1)
    {
        //get mutex
        if(xSemaphoreTake(adcSem, (TickType_t) 30) != pdTRUE)
        {
            ESP_LOGW(LOG_TAG,"Cannot obtain mutex for reading");
            continue;
        }
        
        //read out the analog voltages from all 5 channels (including deadzone)
        //& set calibrate request to 0 before
        D.calibrate_request = 0;
        uint8_t retry = 0;
        while((halAdcReadData(&D) != 0) && (retry++ < 10));
        if(retry == 10)
        {
			ESP_LOGE(LOG_TAG,"Cannot read ADC");
			vTaskDelay(1000/portTICK_PERIOD_MS);
			xSemaphoreGive(adcSem);
		}
        
        
        //if you want to slow down, uncomment following two lines
        //ESP_LOGD(LOG_TAG,"X/Y square, X/Y ellipse: %d/%d, %d/%d",tempX,tempY,D.x,D.y);
        //vTaskDelay(10);
        
        //report raw values.
        halAdcReportRaw(D.up, D.down, D.left, D.right, D.pressure, D.x, D.y);
        
        //if we are in a special strong mode, do NOT send accumulated data
        //to USB/BLE. Instead, call halAdcProcessStrongMode
        //if in normal mode, proceed with mouse
        if(D.strongmode == STRONG_NORMAL)
        {
            //apply acceleration
            if (D.x==0) accelTimeX=0;
            else if (accelTimeX < ACCELTIME_MAX) accelTimeX+=adc_conf.acceleration;
            if (D.y==0) accelTimeY=0;
            else if (accelTimeY < ACCELTIME_MAX) accelTimeY+=adc_conf.acceleration;
                            
            //calculate the current X movement by using acceleration, accel factor and sensitivity
            moveVal = D.x * adc_conf.sensitivity_x * accelFactor * accelTimeX;
            //limit value
            if (moveVal>adc_conf.max_speed) moveVal=adc_conf.max_speed;
            if (moveVal< -adc_conf.max_speed) moveVal=-adc_conf.max_speed;
            //add to accumulated movement value
            accumXpos+=moveVal;
            
            //do the same calculations for Y axis
            moveVal = D.y * adc_conf.sensitivity_y * accelFactor * accelTimeY;
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
            
            #if LOG_LEVEL_ADC >= ESP_LOG_DEBUG
            if(debug_out_cnt++%HAL_ADC_RAW_DIVIDER == 0)
            {
                ESP_LOGD(LOG_TAG,"mouse x/y %d/%d; ",tempX,tempY);
            }
            #endif
        
            if(tempX != 0 && tempY != 0)
            {
                command.cmd[0] = 0x01;
                command.cmd[1] = tempX;
                command.cmd[2] = tempY;
                
                accumXpos -= tempX;
                accumYpos -= tempY;
                
                //post values to mouse queue (USB and/or BLE)
                if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_USB)
                { xQueueSend(hid_usb,&command,0); }
                
                if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_BLE)
                { xQueueSend(hid_ble,&command,0); }
            }
            
            if(tempX != 0 && tempY == 0)
            {
                command.cmd[1] = tempX;
                accumXpos -= tempX;
                command.cmd[0] = 0x10; //send X axis
                
                //post values to mouse queue (USB and/or BLE)
                if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_USB)
                { xQueueSend(hid_usb,&command,0); }
                
                if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_BLE)
                { xQueueSend(hid_ble,&command,0); }
            }
            if(tempY != 0 && tempX == 0)
            {
                command2.cmd[1] = tempY;
                accumYpos -= tempY;
                command2.cmd[0] = 0x11; //send Y axis
                
                //post values to mouse queue (USB and/or BLE)
                if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_USB)
                { xQueueSend(hid_usb,&command2,0); }
                
                if(xEventGroupGetBits(connectionRoutingStatus) & DATATO_BLE)
                { xQueueSend(hid_ble,&command2,0); }
            }
            
            //pressure sensor is handled in another function
            halAdcProcessPressure(&D);
        } else {
            //in special mode, process strong mdoe
            halAdcProcessStrongMode(&D);
        }
        
        //give mutex
        xSemaphoreGive(adcSem);
        
        //if OTF calibration is requested:
        if(D.calibrate_request != 0) halAdcCalibrate();
        
        //delay the task.
        vTaskDelayUntil( &xLastWakeTime, 10/portTICK_PERIOD_MS);
    }
}

#endif
#ifdef DEVICE_FLIPMOUSE
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
 * @note This task is not available on a FABI device.
 * @see DEVICE_FABI
 * @see DEVICE_FLIPMOUSE
 * */
void halAdcTaskJoystick(void * pvParameters)
{
    //analog values
    adcData_t D;
    D.strongmode = STRONG_NORMAL;
    //int32_t x,y;
    //joystick_command_t command;
    TickType_t xLastWakeTime;
    //set adc data reference for timer
    vTimerSetTimerID(adcStrongTimeoutTimerHandle,&D);
    
    while(1)
    {
        //get mutex
        if(xSemaphoreTake(adcSem, (TickType_t) 30) != pdTRUE)
        {
            ESP_LOGW(LOG_TAG,"Cannot obtain mutex for reading");
            continue;
        }
        
        //read out the analog voltages from all 5 channels
        uint8_t retry = 0;
        while((halAdcReadData(&D) != 0) && (retry++ < 10));
        if(retry == 10)
        {
			ESP_LOGE(LOG_TAG,"Cannot read ADC");
			vTaskDelay(1000/portTICK_PERIOD_MS);
			xSemaphoreGive(adcSem);
		}
        halAdcReportRaw(D.up, D.down, D.left, D.right, D.pressure, D.x, D.y);
        
        
        //if we are in a special strong mode, do NOT send accumulated data
        //to USB/BLE. Instead, call halAdcProcessStrongMode
        //if in normal mode, proceed with mouse
        if(D.strongmode == STRONG_NORMAL)
        {
            
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
            halAdcProcessPressure(&D);
        } else {
            //in special mode, process strong mdoe
            halAdcProcessStrongMode(&D);
        }
        
        //give mutex
        xSemaphoreGive(adcSem);
        
        //delay the task.
        vTaskDelayUntil(&xLastWakeTime, 20/portTICK_PERIOD_MS); 
    }
}

#endif
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
    //check for initialized mutex
    if(adcSem == NULL) return;
    
    //if we are updating now, do not calibrate.
    if((xEventGroupGetBits(systemStatus) & SYSTEM_STABLECONFIG) == 0) return;
    
    //get mutex
    if(xSemaphoreTake(adcSem, (TickType_t) 20))
    {
		uint8_t retry = 0;
		do {
			TONE(TONE_CALIB_FREQ,TONE_CALIB_DURATION);
			vTaskDelay(100/portTICK_PERIOD_MS);
			
	        if((xTaskGetTickCount() - adcCalibLast) < (HAL_ADC_CALIB_LOCKTIME / portTICK_PERIOD_MS))
	        {
	            ESP_LOGI(LOG_TAG,"Calibration lock time not passed yet");
	            //give mutex to enable tasks again
	            xSemaphoreGive(adcSem);
	            return;
	        }
	        ESP_LOGI(LOG_TAG,"Starting calibration, offsets: %d/%d",offsetx,offsety);
	        adcData_t D;
	        
	        uint32_t up = 0;
	        uint32_t down = 0;
	        uint32_t left = 0;
	        uint32_t right = 0;
	        
	        //save for next iteration
	        adcCalibLast = xTaskGetTickCount();
	        
	        //read values itself & accumulate (8sensor readings)
	        uint8_t i = 0;
	        do{ 
	            //use read data for acquiring offset correction data
	            if(halAdcReadData(&D) != 0) continue;
	            i++;
	            
	            //accumulate data
	            up+= D.up;
	            left+= D.left;
	            right+= D.right;
	            down+= D.down;
	            
	            //wait 2 ticks
	            vTaskDelay(2);
	        } while(i<8);
	        
	        //divide by 8 readings
	        up = up / 8;
	        down = down / 8;
	        left = left / 8;
	        right = right / 8;
	        //set as offset values
	        offsetx = left - right;
	        offsety = up - down;
	        //increase retry count
	        retry++;
	    } while(((abs(offsetx) > 1000) || (abs(offsety) > 1000)) && (retry < 10));
        
        if(retry < 10)
        {
			ESP_LOGI(LOG_TAG,"Finished calibration, offsets: %d/%d",offsetx,offsety);
		} else {
			ESP_LOGE(LOG_TAG,"Cannot calibrate, sensor defect!");
			while(1)
			{
				TONE(TONE_CALIB_FREQ,TONE_CALIB_DURATION);
				vTaskDelay(1000/portTICK_PERIOD_MS);
			}
        }
        //give mutex to enable tasks again
        xSemaphoreGive(adcSem);
    } else {
        ESP_LOGE(LOG_TAG,"Cannot calibrate, no mutex");
    }
    
    return;
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
    raw_action_t evt;
    D.strongmode = STRONG_NORMAL;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    //set adc data reference for timer
    vTimerSetTimerID(adcStrongTimeoutTimerHandle,&D);
    
    #ifdef DEVICE_FLIPMOUSE
    //1<<0: up, 1<<1: down, 1<<2: left, 1<<3: right; set if press event was sent.
    uint8_t activevbs = 0;
    #endif
    
    while(1)
    {
        //get mutex
        if(xSemaphoreTake(adcSem, (TickType_t) 30) != pdTRUE)
        {
            ESP_LOGW(LOG_TAG,"Cannot obtain mutex for reading");
            continue;
        }
        
        //read out the analog voltages from all 5 channels
        uint8_t retry = 0;
        while((halAdcReadData(&D) != 0) && (retry++ < 10));
        if(retry == 10)
        {
			ESP_LOGE(LOG_TAG,"Cannot read ADC");
			vTaskDelay(1000/portTICK_PERIOD_MS);
			xSemaphoreGive(adcSem);
		}
        
        //for a FABI device, we do not have 4 channels, so not UP/DOWN/LEFT/RIGHT
        #ifdef DEVICE_FLIPMOUSE
        
        
        
        //if we are in a special strong mode, do NOT send accumulated data
        //to USB/BLE. Instead, call halAdcProcessStrongMode
        //if in normal mode, proceed with mouse
        if(D.strongmode == STRONG_NORMAL)
        {
            //LEFT/RIGHT value exceeds threshold (deadzone) value?
            if(D.x != 0)
            {
                if(D.x < 0)
                {
                    //if not already sent, send press action
                    if(!(activevbs & (1<<2)))
                    {
                        evt.type = VB_PRESS_EVENT;
                        evt.vb = VB_LEFT;
                        xQueueSendToBack(debouncer_in,&evt,0);
                        activevbs |= (1<<2);
                    }
                    //if opposite was active, release it.
                    if(activevbs & (1<<3))
                    {
                        evt.type = VB_RELEASE_EVENT;
                        evt.vb = VB_RIGHT;
                        xQueueSendToBack(debouncer_in,&evt,0);
                        activevbs &= ~(1<<3);
                    }
                }
                if(D.x > 0)
                {
                    //if not already sent, send press action
                    if(!(activevbs & (1<<3)))
                    {
                        evt.type = VB_PRESS_EVENT;
                        evt.vb = VB_RIGHT;
                        xQueueSendToBack(debouncer_in,&evt,0);
                        activevbs |= (1<<3);
                    }
                    //if opposite was active, release it.
                    if(activevbs & (1<<2))
                    {
                        evt.type = VB_RELEASE_EVENT;
                        evt.vb = VB_LEFT;
                        xQueueSendToBack(debouncer_in,&evt,0);
                        activevbs &= ~(1<<2);
                    }
                }
            } else {        
                //value is 0 --> idle, release both directions
                //if active
                if(activevbs & (1<<3))
                {
                    evt.type = VB_RELEASE_EVENT;
                    evt.vb = VB_RIGHT;
                    xQueueSendToBack(debouncer_in,&evt,0);
                    activevbs &= ~(1<<3);
                }
                if(activevbs & (1<<2))
                {
                    evt.type = VB_RELEASE_EVENT;
                    evt.vb = VB_LEFT;
                    xQueueSendToBack(debouncer_in,&evt,0);
                    activevbs &= ~(1<<2);
                }
            }
            
            //UP/DOWN value exceeds threshold (deadzone) value?
            if(D.y != 0)
            {
                if(D.y < 0)
                {
                    //if not already sent, send press action
                    if(!(activevbs & (1<<0)))
                    {
                        evt.type = VB_PRESS_EVENT;
                        evt.vb = VB_UP;
                        xQueueSendToBack(debouncer_in,&evt,0);
                        activevbs |= (1<<0);
                    }
                    //if opposite was active, release it.
                    if(activevbs & (1<<1))
                    {
                        evt.type = VB_RELEASE_EVENT;
                        evt.vb = VB_DOWN;
                        xQueueSendToBack(debouncer_in,&evt,0);
                        activevbs &= ~(1<<1);
                    }
                }
                if(D.y > 0)
                {
                    //if not already sent, send press action
                    if(!(activevbs & (1<<1)))
                    {
                        evt.type = VB_PRESS_EVENT;
                        evt.vb = VB_DOWN;
                        xQueueSendToBack(debouncer_in,&evt,0);
                        activevbs |= (1<<1);
                    }
                    //if opposite was active, release it.
                    if(activevbs & (1<<0))
                    {
                        evt.type = VB_RELEASE_EVENT;
                        evt.vb = VB_UP;
                        xQueueSendToBack(debouncer_in,&evt,0);
                        activevbs &= ~(1<<0);
                    }
                }
            } else {        
                //value is 0 --> idle, release both directions
                //if active
                if(activevbs & (1<<0))
                {
                    evt.type = VB_RELEASE_EVENT;
                    evt.vb = VB_UP;
                    xQueueSendToBack(debouncer_in,&evt,0);
                    activevbs &= ~(1<<0);
                }
                if(activevbs & (1<<1))
                {
                    evt.type = VB_RELEASE_EVENT;
                    evt.vb = VB_DOWN;
                    xQueueSendToBack(debouncer_in,&evt,0);
                    activevbs &= ~(1<<1);
                }
            }
        } else {
            //in special mode, process strong mdoe
            halAdcProcessStrongMode(&D);
        }
        
        halAdcReportRaw(D.up, D.down, D.left, D.right, D.pressure, D.x, D.y);
        
        #endif
        
        //pressure sensor is handled in another function
        halAdcProcessPressure(&D);
        
        //give mutex
        xSemaphoreGive(adcSem);
        
        //delay the task.
        vTaskDelayUntil( &xLastWakeTime, 10/portTICK_PERIOD_MS);
        //vTaskDelay(20/portTICK_PERIOD_MS);
    }
    
}


/** @brief Reload ADC config
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
    
    //check for initialized mutex
    if(adcSem == NULL)
    {
        ESP_LOGE(LOG_TAG,"Mutex not initialized");
        return ESP_FAIL;
    }
    
    //acquire mutex
    if(xSemaphoreTake(adcSem, (TickType_t) 30) != pdTRUE)
    {
        ESP_LOGW(LOG_TAG,"Cannot obtain mutex for config update");
        return ESP_FAIL;
    }
    
    #ifdef DEVICE_FLIPMOUSE
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
    #endif
    
    //check for invalid input
    #ifdef DEVICE_FLIPMOUSE
        params->otf_count = validate(params->otf_count,5,15,HAL_IO_ADC_OTF_COUNT);
        params->otf_idle = validate(params->otf_idle,0,15,HAL_IO_ADC_OTF_THRESHOLD);
    #endif
    
    //clear pending button flags
    //TBD...
    
    //Just copy content
    memcpy(&adc_conf,params,sizeof(adc_config_t));
    
    #ifdef DEVICE_FLIPMOUSE
    //start task according to mode
    if(adcHandle == NULL)
    {
        switch(adc_conf.mode)
        {
            case MOUSE:
                xTaskCreate(halAdcTaskMouse,"ADC_TASK",4096,NULL,HAL_ADC_TASK_PRIORITY,&adcHandle);
                ESP_LOGI(LOG_TAG,"created ADC task for mouse, handle %d",(uint32_t)adcHandle);
                break;
            case JOYSTICK:
                xTaskCreate(halAdcTaskJoystick,"ADC_TASK",4096,NULL,HAL_ADC_TASK_PRIORITY,&adcHandle);
                ESP_LOGI(LOG_TAG,"created ADC task for joystick, handle %d",(uint32_t)adcHandle);
                break;
            case THRESHOLD:
                xTaskCreate(halAdcTaskThreshold,"ADC_TASK",4096,NULL,HAL_ADC_TASK_PRIORITY,&adcHandle);
                ESP_LOGI(LOG_TAG,"created ADC task for threshold, handle %d",(uint32_t)adcHandle);
                break;
            case NONE:
                ESP_LOGI(LOG_TAG,"no ADC task necessary");
                break;
            default:
               ESP_LOGE(LOG_TAG,"unknown mode (unconfigured), cannot startup task.");
               return ESP_FAIL;
        }
    } else {
        ESP_LOGD(LOG_TAG,"ADC config reloaded without task switch");
    }
    #endif
    
    #ifdef DEVICE_FABI
    //start task (independent of mode in FABI)
    if(adcHandle == NULL)
    {
        //just use a threshold task, other channels are masked out in this task.
        xTaskCreate(halAdcTaskThreshold,"ADC_TASK",4096,NULL,HAL_ADC_TASK_PRIORITY,&adcHandle);
        ESP_LOGI(LOG_TAG,"created ADC task for threshold, handle %d",(uint32_t)adcHandle); 
    }
    #endif
    
    ESP_LOG_BUFFER_HEXDUMP(LOG_TAG,&adc_conf,sizeof(adc_config_t),ESP_LOG_DEBUG);
    //give mutex
    xSemaphoreGive(adcSem);
    return ESP_OK;
}

void halAdcStrongDelay( TimerHandle_t xTimer )
{
    xSemaphoreGive(adcStrongSem);
}

void halAdcStrongTimeout( TimerHandle_t xTimer )
{
    //get adc data reference
    adcData_t *D = pvTimerGetTimerID(xTimer);
    if(D == NULL)
    {
        ESP_LOGE(LOG_TAG,"Reference to adcData_t not set, but timeout occured");
        return;
    }
    //set strong mode back to normal after timeout
    if(D->strongmode == STRONG_PUFF)
    {
        ESP_LOGI(LOG_TAG,"Exit STRONG PUFF, timeout");
        TONE(TONE_STRONGPUFF_EXIT_FREQ,TONE_STRONGPUFF_EXIT_DURATION);
    }
    if(D->strongmode == STRONG_SIP)
    {
        ESP_LOGI(LOG_TAG,"Exit STRONG SIP, timeout");
        TONE(TONE_STRONGSIP_EXIT_FREQ,TONE_STRONGSIP_EXIT_DURATION);
    }
    
    D->strongmode = STRONG_NORMAL;
    
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
    esp_log_level_set(LOG_TAG,LOG_LEVEL_ADC);
    
    //ADC on ESP32 does not work at all...
    //The only sh**ty part on this MCU :-)
    
    if(hid_ble == NULL || hid_usb == NULL)
    {
        ESP_LOGE("hal_adc","queue uninitialized, exiting");
        return ESP_FAIL;
    }
    
    //initialize ADC semphore as mutex
    adcSem = xSemaphoreCreateMutex();
    
    //start first calibration
    //halAdcCalibrate();
    
    //initialize SW timer for STRONG mode timeout
    #ifdef DEVICE_FLIPMOUSE
    adcStrongTimeoutTimerHandle = xTimerCreate("strongmode", HAL_ADC_TIMEOUT_STRONGMODE / portTICK_PERIOD_MS, \
        pdFALSE,( void * ) 0,halAdcStrongTimeout);
    adcStrongTimerHandle = xTimerCreate("strongmodedelay", HAL_ADC_DELAY_STRONGMODE / portTICK_PERIOD_MS, \
        pdFALSE,( void * ) 0,halAdcStrongDelay);
    adcStrongSem = xSemaphoreCreateBinary();
    #endif
    
    //not initializing full config, only ADC
    if(params == NULL) return ESP_OK;
    
    //init remaining parts by updating config.
    return halAdcUpdateConfig(params);
}

