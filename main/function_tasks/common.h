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
 
/**
 * @file
 * @brief Common data structs and IPC pointers for the firmware
 * 
 * This file contains common structs, which are used for inter-process-communication
 * within the firmware.
 * 
 * In addition, virtual button assignment is done here, depending on
 * device configuration (this firmware can be built for a FABI or a FLipMouse)
 * */

#ifndef FUNCTION_COMMON_H_
#define FUNCTION_COMMON_H_

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/portmacro.h>
#include "freertos/task.h"
#include "freertos/queue.h"
#include <esp_log.h>

#include "keyboard.h"
#include "driver/rmt.h"

/** @brief Enable v2.5 compatibility
 * 
 * This define activates a few behavioural changes in the firmware
 * to be fully compatible to v2.5 GUI + firmware, although some
 * of this behaviour is inconsistent and might be changed in future releases.
 * 
 * Following changes are made by this define:
 * 
 * * AT LA initializes a slot 0 with mouse function, if called after
 * an AT DE command.
 * 
 */
#define ACTIVATE_V25_COMPAT

/** ID String to be printed on "AT ID" command.
 * @todo Change this back to new version, when GUI is compatible */
//#define IDSTRING "FLipMouse V3.0\r\n"
#define IDSTRING "Flipmouse v2.5"
//#define IDSTRING "FABI v3.0\n"

/** @brief Determine used device. Either FABI or FLipMouse.
 * 
 * @note Define one of these, otherwise it will result in a compile error.
 * */
//#define DEVICE_FABI
#define DEVICE_FLIPMOUSE

/** number of virtual button event groups. One event group is used for 4 VBs */
#define NUMBER_VIRTUALBUTTONS 8

/** @brief Set count of used Neopixels
 * 
 * If LED_USE_NEOPIXEL is set, this define is used to determine the
 * count of NEOPIXEL LEDs, which are used for color output
 * */
#define LED_NEOPIXEL_COUNT  1


/** maximum length for a slot name */
#define SLOTNAME_LENGTH   32

/** @brief Maximum size of ANY virtual button config.
 * @warning If this size is set to a value smaller than a VB config, this
 * config will be truncated in flash memory!
 * @note Currently the taskKeyboardConfig_t is the biggest struct with 100Bytes 
 * @see halStorageStoreSetVBConfigs
 * @see halStorageLoadGetVBConfigs*/
#define VB_MAXIMUM_PARAMETER_SIZE 128

/** @brief maximum length for an AT command (including parameters & 'AT ', e.g., macro text) */
#define ATCMD_LENGTH   256

/** ID of storage revision. Is used to determine any data storage upgrades */
#define STORAGE_ID    0xC0FFEE01


/** bitmask for testing if a function task should send data to USB queues */
#define DATATO_USB (1<<7)
/** bitmask for testing if a function task should send data to BLE queues */
#define DATATO_BLE (1<<6)
/** bitmask used to signal a FLipMouse (no FABI!) in CIM mode (no USB/BLE functionality) */
#define DATATO_CIM (1<<5)
/** bitmask used to signal a FLipMouse/FABI has an active Wifi server */
#define WIFI_ACTIVE (1<<4)
/** bitmask used to signal a FLipMouse/FABI has at least one connected Wifi client */
#define WIFI_CLIENT_CONNECTED (1<<3)

/** @brief Flag for signalling config updates
 * 
 * This flag is used to signal a config change.
 * If this flag is set, the system will receive a lot of AT commands
 * at once (config is saved in a .set file with AT commands).
 * Therefore, a few features can be disabled while this flag is set:
 * * Calibrating: ADC calibration can be done after config load, only one time
 * * Connection reset
 * * Debouncing: after this bit is cleared, all debouncer channels should be reset
 * * task_hid: we don't want to send out any spurious hid commands
 * * task_vb: we don't want to send out any spurious commands
 * 
 * @note Due to the limitation of FreeRTOS, which cannot block for clearing bits,
 * we use the flag SYSTEM_STABLECONFIG for the exact counterpart.
 * @see SYSTEM_STABLECONFIG
 * */
#define SYSTEM_LOADCONFIG (1<<0)

/** @brief Stable & loaded config.
 * Exact counterpart of SYSTEM_LOADCONFIG
 * @see SYSTEM_LOADCONFIG
 * */
#define SYSTEM_STABLECONFIG (1<<1)

/** @brief AT command queue is empty */
#define SYSTEM_EMPTY_CMD_QUEUE (1<<2)

/** @brief Event group array for all virtual buttons, used by functional tasks to be triggered.
 * 
 * Each EventGroupHandle contains 4 virtual buttons (VB), press and release actions are included:
 * (1<<0) press action of VB 0 (or 4, 8, 12, 16, 20, 24, 28, depending on array index)
 * (1<<1) press action of VB 1 (or 5, 9, 13, 17, 21, 25, 29, depending on array index)
 * (1<<2) press action of VB 2 (or 6, 10, 14, 18, 22, 26, 30, depending on array index)
 * (1<<3) press action of VB 3 (or 7, 11, 15, 19, 23, 27, 31, depending on array index)
 * The same for release actions:
 * (1<<4) release for VB 0 (+ the others)
 * (1<<5) release for VB 1 (+ the others) 
 * (1<<6) release for VB 2 (+ the others) 
 * (1<<7) release for VB 3 (+ the others) 
 * 
 * Each functional task should pend on these flags, which will be set by
 * the hal task or the analog task.
 * Please reset the flag after pending.
 * Unused flags can be left set, on a task change for a virtual button,
 * these flags are reset.
 * 
 * @warning If you want to debounce an action, set the corresponding flag in virtualButtonsIn
 * @see virtualButtonsIn
 * */
extern EventGroupHandle_t virtualButtonsOut[NUMBER_VIRTUALBUTTONS];

/** @brief Event group array for all virtual buttons, used by functional tasks to send a signal to the debouncer task
 * 
 * Equal to virtualButtonsOut, only these flags should be used to
 * invoke the debouncer. If one of these flags will be set, this flag
 * will be set in virtualButtonsOut as well (by the debouncer).
 * If this flag is cleared within the debouncing time, the flag will not
 * be mapped to virtualButtonsOut.
 * 
 * VB setting tasks should use this eventgroup if the debouncer
 * should be invoked.
 * If no debouncing is necessary, write directly to virtualButtonsOut
 * 
 * @warning VB pending tasks should always pend for virtualButtonsOut.
 * @see virtualButtonsOut
 * */
extern EventGroupHandle_t virtualButtonsIn[NUMBER_VIRTUALBUTTONS];

/** this flag group is used to determine the routing
 * of different data to either USB, BLE or both.
 * In addition this flag group contains status information
 * (1<<7) should send data to USB HID?
 * (1<<6) should send data to BLE HID?
 * (1<<5) is the FLipMouse in CIM mode?
 * (1<<4) is the wifi active?
 * (1<<3) is a client connected to wifi?
 * 
 * @see DATATO_USB
 * @see DATATO_BLE
 * @see DATATO_CIM
 * @see WIFI_ACTIVE
 * @see WIFI_CLIENT_CONNECTED
 * */
extern EventGroupHandle_t connectionRoutingStatus;

/** @brief General system status 
 * 
 * Flag group signalling the general system status.
 * 
 * @note Detailed status description is given in the flag defines!
 * @see SYSTEM_LOADCONFIG
*/
extern EventGroupHandle_t systemStatus;

/** NOTE:
 * keyboard queues receive uint16 data type, which is either
 * directly used as keycode by either HID or serial task 
 * (if high byte is != 0)
 * OR
 * parsed by keyboard helper functions. In this case
 * multibyte text (unicode) should use only low byte for each
 * byte of the input text
 **/

/** @brief Queue for sending HID commands to USB */
extern QueueHandle_t hid_usb;
/** @brief Queue for sending HID commands to BLE */
extern QueueHandle_t hid_ble;


/** @brief Possible states of the radio (wifi & BLE) */
typedef enum {UNINITIALIZED,WIFI,BLE,BLE_PAIRING} radio_status_t;
/** @brief Currently active radio state */
extern radio_status_t radio;

/** @brief Queue to receive config changing commands. 
 * 
 * A string is passed to this queue with a maximum length of SLOTNAME_LENGTH.
 * 
 * Either a special command is passed or the slotname which should be loaded.
 * Possible strings to send to this queue:
 * * Name of the slot to be loaded
 * * "__NEXT" for next slot
 * * "__PREVIOUS" for previous slot
 * * "__DEFAULT" for default slot
 * * "__RESTOREFACTORY" to delete all slots & reset default 
 *    slot to factory defaults
 * * "__UPDATE" to reload tasks based on currentConfig. Used to
 *  update the configuration when changed by configuration software/GUI.
 * 
 * @see SLOTNAME_LENGTH
 * @see configSwitcherTask
 * @see currentConfig
 **/
extern QueueHandle_t config_switcher;

/*++++ VIRTUAL BUTTON ASSIGNMENT ++++*/
/** this section assigns the virtual buttons to each part
 * of the FLipMouse/FABI (on a FABI device, of course each external button
 * represents one virtual button).
 * 
 * Currently used mappings:
 * Mouthpiece - Alternative mode
 * External+Internal buttons
 * 
 * Planned for future release:
 * Gesture VBs, a recorded gesture is treated like a virtual button
 * */
#ifdef DEVICE_FLIPMOUSE
  #define VB_INTERNAL2    0
  #define VB_INTERNAL1    1
  #define VB_EXTERNAL1    2
  #define VB_EXTERNAL2    3
  #define VB_UP           4
  #define VB_DOWN         5
  #define VB_LEFT         6
  #define VB_RIGHT        7
  #define VB_SIP          8
  #define VB_STRONGSIP    9
  #define VB_PUFF         10
  #define VB_STRONGPUFF   11
  #define VB_STRONGSIP_UP     12
  #define VB_STRONGSIP_DOWN   13
  #define VB_STRONGSIP_LEFT   14
  #define VB_STRONGSIP_RIGHT  15
  #define VB_STRONGPUFF_UP    16
  #define VB_STRONGPUFF_DOWN  17
  #define VB_STRONGPUFF_LEFT  18
  #define VB_STRONGPUFF_RIGHT 19
  #define VB_MAX          20
#endif

#ifdef DEVICE_FABI
  #define VB_EXTERNAL1    0
  #define VB_EXTERNAL2    1
  #define VB_EXTERNAL3    2
  #define VB_EXTERNAL4    3
  #define VB_EXTERNAL5    4
  #define VB_EXTERNAL6    5
  #define VB_EXTERNAL7    6
  #define VB_EXTERNAL8    7
  #define VB_EXTERNAL9    8
  #define VB_INTERNAL1    9
  #define VB_SIP          10
  #define VB_PUFF         11
  #define VB_STRONGSIP    12
  #define VB_STRONGPUFF   13
  #define VB_MAX          14
#endif

/** special virtual button, which is used to trigger a task immediately.
 * After this single action, each function body of functional tasks
 * is REQUIRED to do a return. */
#define VB_SINGLESHOT   32

/** @brief Easy macro to set a VB (with debouncing for press action)
 * 
 * This macro sets the corresponding VB flag in virtualButtonsIn.
 * The debouncer will map it to virtualButtonsOut after the set debouncing
 * time. Cancel a pending VB action by CLEARVB_PRESS.
 * 
 * @see virtualButtonsIn
 * @see virtualButtonsOut
 * @see task_debouncer
 * @see CLEARVB_PRESS
 * */
#define SETVB_PRESS(x) xEventGroupSetBits(virtualButtonsIn[x/4],(1<<(x%4)))


/** @brief Easy macro to clear a VB (with debouncing for press action)
 * 
 * This macro clears the corresponding VB flag in virtualButtonsIn.
 * @note If the flag is already mapped to the out group, this macro has no effect.
 * 
 * @see virtualButtonsIn
 * @see virtualButtonsOut
 * @see task_debouncer
 * @see SETVB_PRESS
 * */
#define CLEARVB_PRESS(x) xEventGroupClearBits(virtualButtonsIn[x/4],(1<<(x%4)))

/** @brief Easy macro to set a VB (with debouncing for release action)
 * 
 * This macro sets the corresponding VB flag in virtualButtonsIn.
 * The debouncer will map it to virtualButtonsOut after the set debouncing
 * time. Cancel a pending VB action by CLEARVB_PRESS.
 * 
 * @see virtualButtonsIn
 * @see virtualButtonsOut
 * @see task_debouncer
 * @see CLEARVB_RELEASE
 * */
#define SETVB_RELEASE(x) xEventGroupSetBits(virtualButtonsIn[x/4],(1<<(x%4 + 4)))


/** @brief Easy macro to clear a VB (with debouncing for release action)
 * 
 * This macro clears the corresponding VB flag in virtualButtonsIn.
 * @note If the flag is already mapped to the out group, this macro has no effect.
 * 
 * @see virtualButtonsIn
 * @see virtualButtonsOut
 * @see task_debouncer
 * @see SETVB_RELEASE
 * */
#define CLEARVB_RELEASE(x) xEventGroupClearBits(virtualButtonsIn[x/4],(1<<(x%4 + 4)))

/** @brief Easy macro to get a VB release flag (AFTER debouncer!)
 * 
 * This macro gets the value of the corresponding release VB flag in virtualButtonsOut.
 * @note This macro reads the OUT value after the debouncer.
 * 
 * @see virtualButtonsIn
 * @see virtualButtonsOut
 * @see task_debouncer
 * @see GETVB_PRESS
 * */
#define GETVB_RELEASE(x) (xEventGroupGetBits(virtualButtonsOut[x/4])&(1<<(x%4 + 4)))

/** @brief Easy macro to get a VB press flag (AFTER debouncer!)
 * 
 * This macro gets the value of the corresponding press VB flag in virtualButtonsOut.
 * @note This macro reads the OUT value after the debouncer.
 * 
 * @see virtualButtonsIn
 * @see virtualButtonsOut
 * @see task_debouncer
 * @see GETVB_RELEASE
 * */
#define GETVB_PRESS(x) (xEventGroupGetBits(virtualButtonsOut[x/4])&(1<<(x%4)))


/*++++ TASK PRIORITY ASSIGNMENT ++++*/
#define HAL_ADC_TASK_PRIORITY     (tskIDLE_PRIORITY + 2)
#define DEBOUNCER_TASK_PRIORITY  (tskIDLE_PRIORITY + 2)
#define HID_TASK_PRIORITY  (tskIDLE_PRIORITY + 4)
#define VB_TASK_PRIORITY  (tskIDLE_PRIORITY + 4)
/** All BLE tasks in hal_ble.c. */
#define HAL_BLE_TASK_PRIORITY_BASE  (tskIDLE_PRIORITY + 2)
#define HAL_CONFIG_TASK_PRIORITY  (tskIDLE_PRIORITY + 5)
#define TASK_COMMANDS_PRIORITY  (tskIDLE_PRIORITY + 6)

/*++++ MAIN CONFIG STRUCT ++++*/

/**
 * Mode of operation for the mouthpiece:<br>
 * NONE       Do not do anything with the mouthpiece
 * MOUSE      Mouthpiece controls mouse cursor <br>
 *            Used parameters: <br>
 *              * max_speed
 *              * acceleration
 *              * deadzone_x/y
 *              * sensitivity_x/y
 * JOYSTICK   Mouthpiece controls two joystick axis <br>
 *            Used parameters <br>
 *              * deadzone_x/y
 *              * sensitivity_x/y
 *              * axis
 * THRESHOLD  Mouthpiece triggers virtual buttons <br>
 *            Used parameters <br>
 *              * deadzone_x/y
 *              * threshold_x/y
 * @see VB_UP
 * @see VB_DOWN
 * @see VB_LEFT
 * @see VB_RIGHT
 * */
typedef enum mouthpiece_mode {NONE, MOUSE, JOYSTICK, THRESHOLD} mouthpiece_mode_t;

/**
 * config for the ADC task & the analog mode of operation
 * 
 * TBD: describe all parameters...
 * 
 * @todo Remove report raw from here & create new "volatile" config struct -> no need to save, but no need to overwrite on slot change
 * @see mouthpiece_mode_t
 * @see VB_SIP
 * @see VB_PUFF
 * @see VB_STRONGSIP
 * @see VB_STRONGPUFF
 * */
typedef struct adc_config {
  /** mode setting for mouthpiece, @see mouthpiece_mode_t */
  mouthpiece_mode_t mode;
  /** acceleration for x & y axis (mouse & joystick mode) */
  uint8_t acceleration;
  /** max speed for x & y axis (mouse & joystick mode) */
  uint8_t max_speed;
  /** deadzone values for x & y axis (mouse & joystick & threshold mode) */
  uint8_t deadzone_x;
  uint8_t deadzone_y;
  /** sensitivity values for x & y axis (mouse & joystick mode) */
  uint8_t sensitivity_x;
  uint8_t sensitivity_y;
  /** pressure sensor, sip threshold */
  uint16_t threshold_sip;
  /** pressure sensor, puff threshold */
  uint16_t threshold_puff;
  /** pressure sensor, strongsip threshold */
  uint16_t threshold_strongsip;
  /** pressure sensor, strongpuff threshold */
  uint16_t threshold_strongpuff;
  /** Enable report RAW values (!=0), values are sent via halSerialSendUSBSerial */
  uint8_t reportraw;
  /** joystick axis assignment TBD: assign axis to numbers*/
  uint8_t axis;
  /** FLipMouse orientation, 0,90,180 or 270° */
  uint16_t orientation;
  /** On-the-fly calibration, count of idle events before triggering calibration */
  uint8_t otf_count;
  /** On-the-fly calibration, level of detecting idle (all raw values need to change less
   * than this value to be detected as idle) */
  uint8_t otf_idle;
} adc_config_t;

/** @brief Type of VB command
 * @see vb_cmd_t */
typedef enum {
  T_CONFIGCHANGE = 1, /** @brief Config change request */
  T_CALIBRATE, /** @brief Calibrationrequest */
  T_SENDIR, /** @brief Send an IR command */
  T_MACRO /** @brief Trigger macro execution */
} vb_cmd_type_t;

typedef struct generalConfig {
  uint32_t slotversion;
  adc_config_t adc;
  uint8_t ble_active;
  uint8_t usb_active;
  /** mouse wheel: stepsize */
  uint8_t wheel_stepsize;
  /** country code to be used by BLE&USB HID */
  uint8_t countryCode;
  /** keyboard locale to be used by BLE&USB(serial) HID */
  uint8_t locale;
  /** device identifier to be used by BLE&USB HID.
   * 0 => FLipMouse
   * 1 => FABI */
  uint8_t deviceIdentifier;
  /** @brief Timeout between IR edges before command is declared as finished */
  uint8_t irtimeout;
  /** @brief Global anti-tremor time for press */
  uint16_t debounce_press;
  /** @brief Global anti-tremor time for release */
  uint16_t debounce_release;
  /** @brief Global anti-tremor time for idle */
  uint16_t debounce_idle;
  /** @brief Enable/disable button learning mode
   * @todo Move to "volatile" storage, independent from slot change */
  uint8_t button_learn;
  /** @brief Feedback mode.
   * 
   * * 0 disables LED and buzzer
   * * 1 disables buzzer, but LED is on
   * * 2 disables LED, but buzzer gives output
   * * 3 gives LED and buzzer feedback
   * */
  uint8_t feedback;
  /** @brief Anti-tremor (debounce) time for press of each VB */
  uint16_t debounce_press_vb[NUMBER_VIRTUALBUTTONS*4];
  /** @brief Anti-tremor (debounce) time for release of each VB */
  uint16_t debounce_release_vb[NUMBER_VIRTUALBUTTONS*4];
  /** @brief Anti-tremor (debounce) time for idle of each VB */
  uint16_t debounce_idle_vb[NUMBER_VIRTUALBUTTONS*4];
  /** @brief Slotname of this config */
  char slotName[SLOTNAME_LENGTH];
} generalConfig_t;

/** @brief One VB command (not HID), maybe an element of a command chain */
typedef struct vb_cmd vb_cmd_t;

/** @brief One VB command (not HID), maybe an element of a command chain 
 * 
 * This struct is used for VB commands, which are not HID commands.
 * 
 * @todo Document this struct more.
 * @see task_vb_addCmd
 * @see task_vb_clearCmds
 * @see task_vb_getCmdChain
 * @see task_vb_setCmdChain */
struct vb_cmd {
  /** @brief Number of virtual button. MSB signals if this is a press or
   * release action:
   * * If it is set (mask: 0x80), it is a press action
   * * If it is cleared, it is a release action 
   * */
  uint8_t vb;
  /** @brief Type of command */
  vb_cmd_type_t cmd;
  /** @brief Original AT command string, might be NULL if not used */
  char *atoriginal;
  /** @brief Parameter string, e.g. for slot names. Might be NULL if not necessary */
  char *cmdparam;
  /** @brief Pointer to next VB command element, might be NULL. */
  struct vb_cmd *next;
};


/** @brief One HID command, maybe an element of a command chain */
typedef struct hid_cmd hid_cmd_t;

/** @brief  One HID command, maybe an element of a command chain
 *
 * This struct is used either as one element to be passed to hal_serial
 * or the BLE class OR it is used to form a chained list of all HID
 * commands, which are currently active. task_hid takes care
 * of maintaining a list of all active VBs, if one gets triggered (via
 * task_debouncer), the HID task walks through this list for one or more
 * (in case of multiple press/release actions) HID commands, which are
 * sent to the BLE/USB HAL.
 * @see task_hid_addCmd
 * @see task_hid_clearCmds
 * @see task_hid_getCmdChain
 * @see task_hid_setCmdChain */
struct hid_cmd {
  /** @brief Number of virtual button. MSB signals if this is a press or
   * release action:
   * * If it is set (mask: 0x80), it is a press action
   * * If it is cleared, it is a release action 
   * */
  uint8_t vb;
  /** @brief Command to be sent, see HID_kbdmousejoystick.cpp or the
   * usb_bridge for explanations. */
  uint8_t cmd[3];
  /** @brief Original AT command string, might be NULL if not used */
  char *atoriginal;
  /** @brief Pointer to next HID command element, might be NULL. */
  struct hid_cmd *next;
};

/** @brief State of IR receiver
 * 
 * Following states are possible for the IR receiver (task):
 * * IR_IDLE Nothing is done, this halIOIR_t struct is not used for receiving
 * * IR_RECEIVING Receiver is active and storing
 * * IR_TOOSHORT If timeout was triggered and not enough edges are detected
 * * IR_FINISHED If timeout was triggered and enough edges were stored
 * * IR_OVERFLOW Too many edges were detected, could not store
 * 
 * @see TASK_HAL_IR_RECEV_MINIMUM_EDGES
 * @see TASK_HAL_IR_RECV_MAXIMUM_EDGES*/
typedef enum irstate {IR_IDLE,IR_RECEIVING,IR_TOOSHORT,IR_FINISHED,IR_OVERFLOW} irstate_t;

/** @brief Output IR command */
typedef struct halIOIR {
  /** Buffer for IR signal
   * @warning Do not free this buffer! It will be freed by transmitting function
   * @note In case of receiving, this buffer can be freed. */
  rmt_item32_t *buffer;
  /** Count of rmt_item32_t items */
  uint16_t count;
  /** Status of receiver */
  irstate_t status;
} halIOIR_t;

/** @brief Strips away \\r\\t and \\n */
void strip(char *s);

/** @brief NVS key for wifi password */
#define NVS_WIFIPW  "nvswifipw"

/** @brief Minutes between last client disconnected and WiFi is switched off */
#define WIFI_OFF_TIME 5


#endif /*FUNCTION_COMMON_H_*/
