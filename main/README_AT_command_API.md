# AT-commands / Communication API

The FLipmouse firmware provides 4 different USB HID device classes:
* Mouse
* Keyboard
* Joystick
* Serial CDC

All configuration is done via the serial interface, which provides the persistent configuration of all functionalities and in addition a live mode (e.g. write "mouse move" to the serial port and the firmware moves the mouse cursor).

The serial port configuration is **115200 8N1** (even these settings are not necessary due to the USB encapsulation).

**Note:**

The ESP32 chip itself does NOT have a USB connection. Therefore, we integrated a second USB chip on the FABI/FLipMouse PCB, which handles USB to Serial / Serial to HID translation. The corresponding firmware for this chip
(LPC11U14) is located here: [usb_bridge repository](https://github.com/benjaminaigner/usb_bridge)

**Note:**
Please note that all following commands, which do NOT have a fct_* file or are done via a handler, cannot be assigned to a virtual button (via AT BM).
These commands can be used to configure the FLipMouse/FABI, in a macro or for slot management.

Following commands are currently available:

**General Commands**
| Command | Parameter | Description | Available since | Implemented in v3 | fct_* file / handler |
|:--------|:----------|:------------|:--------------|:--------------------|:---------------------|
| AT    | --  | returns OK   | v2 | yes | no |
| AT ID | --  | returns the current version string  | v2 | yes | no |
| AT BM | number (0-VB_MAX-1)  | set the button, which corresponds to the next command. The button assignments are described on the bottom | v2 | yes | no |
| AT BL | number (0,1) | enable/disable output of triggered virtual buttons. Is used with AT BM for command learning | v3 | untested | no (handled in task_debouncer) |
| AT MA | string | execute macro (';' separated list of commands, see [Macros](https://github.com/asterics/FLipMouse/wiki/macros)) <sup>[A](#footnoteA)</sup>  | v2 | yes | fct_macros |
| AT WA | number (0-30000) | wait/delay (ms); useful for macros. Does nothing if not used in macros. | v2 | yes | yes/no <sup>[B](#footnoteB)</sup> |
| AT RO | number (0,90,180,270) | orientation (0 => LEDs on top) | v2 | yes | no |
| AT KL | number | Set keyboard locale (locale defines are listed below) | v3 | yes | no |
| AT BT | number (0,1,2,3) | Bluetooth mode, 0=no HID output, 1=USB only, 2=BT only, 3=both(default) | v2 | yes | no |
| AT TT | number (100-5000) | Threshold time ([ms]) between short and long press actions. Set to 5000 to disable. | v3 | no | no (handled in task_debouncer)  |
| AT AP | number (1-500) | Antitremor delay for button press ([ms]) <sup>[C](#footnoteC)</sup> | v3 | untested | no |
| AT AR | number (1-500) | Antitremor delay for button release ([ms]) <sup>[C](#footnoteC)</sup>| v3 | untested | no |
| AT AI | number (1-500) | Antitremor delay for button idle ([ms]) <sup>[C](#footnoteC)</sup>| v3 | untested | no |
| AT FR | -- | Reports free, used and available config storage space (e.g., "FREE:10%,9000,1000")| v3 | yes | no |
| AT FB | number (0,1,2,3) | Feedback mode, 0=no LED/no buzzer, 1=LED/no buzzer, 2=no LED/buzzer, 3= LED + buzzer | v3 | yes | no |
| AT PW | string | Set a new wifi password. Use at least <b>8</b> characters | v3 | untested | no |
| AT FW | number (2,3) | Update firmware. 2 = update ESP32; 3 = update LPC | v3 | untested | no |

<a name="footnoteA"><b>A</b></a>: If you want to have a semicolon character WITHIN an AT command, please escape it with a backslash sequence: "\;". All other characters can be used normally.

<a name="footnoteB"><b>B</b></a>: AT WA is done in fct_macros, but cannot be used in any other way except a macro ( _AT MA_ ).

<a name="footnoteC"><b>C</b></a>: Either combine the anti-tremor time settings with a previously sent _AT BM_ command to set a debouncing time for an individual virtual button **OR** use this command
individually to set a global value.

**USB HID Commands**
| Command | Parameter | Description | Available since | Implemented in v3 | fct_* file / handler |
|:--------|:----------|:------------|:--------------|:--------------------|:---------------------|
| AT CL | --  | Click left mouse button | v2 | yes | handler_hid |
| AT CR | --  | Click right mouse button  | v2 | yes | handler_hid |
| AT CM | --  | Click middle mouse button  | v2 | yes | handler_hid |
| AT CD | --  | Doubleclick left mouse button  | v2 | yes | handler_hid |
|       |   |   ||| |
| AT HL/AT PL | --  | Press+hold left mouse button (if assigned to a VB, release depends on button release) | v2 | yes | handler_hid |
| AT HR/AT PR | --  | Press+hold right mouse button  (if assigned to a VB, release depends on button release) | v2 | yes | handler_hid |
| AT HM/AT PM | --  | Press+hold middle mouse button  (if assigned to a VB, release depends on button release) | v2 | yes | handler_hid |
|       |   |   ||| |
| AT RL | --  | Release left mouse button  | v2 | yes | handler_hid |
| AT RR | --  | Release right mouse button  | v2 | yes | handler_hid |
| AT RM | --  | Release middle mouse button  | v2 | yes | handler_hid |
|       |   |   ||| |
| AT TL | --  | Toggle left mouse button  | v2 | yes | handler_hid |
| AT TR | --  | Toggle right mouse button  | v2 | yes | handler_hid |
| AT TM | --  | Toggle middle mouse button  | v2 | yes | handler_hid |
|       |   |   ||| |
| AT WU | --  | Move mouse wheel up  | v2 | yes | handler_hid |
| AT WD | --  | Move mouse wheel down  | v2 | yes | handler_hid |
| AT WS | number (1-127)  | Set mousewheel stepsize (e.g.: "AT WS 3" sets the stepsize to 3 rows)| v2 | yes | no |
|       |   |   ||| |
| AT MX | number  | Move mouse (X direction), e.g. AT MX -25  | v2 | yes | handler_hid |
| AT MY | number  | Move mouse (Y direction), e.g. AT MY 10  | v2 | yes | handler_hid |
|       |   |   ||| |
| AT KW | string  | Keyboard write (e.g. "AT KW Hi" types "Hi") | v2 | yes | handler_hid |
| AT KP | string  | Key press ("click") (e.g. "AT KP KEY_UP" presses & releases the up arrow key), a full list of supported key identifiers is provided on the bottom. | v2 | yes | handler_hid |
| AT KH | string  | Key hold (e.g. "AT KH KEY_UP" presses & holds the up arrow key.), a full list of supported key identifiers is provided on the bottom  | v2 | untested | handler_hid |
| AT KR | string  | Key release (e.g. "AT KR KEY_UP" releases the up arrow key)  | v2 | untested | handler_hid |
| AT KT | string  | Key toggle (e.g. "AT KT KEY_UP" toggles the up arrow key)  | v2 | untested | handler_hid |
| AT RA | --  | Release all keys  | v2 | untested | no |
**Storage commands** 
| Command | Parameter | Description | Available since | Implemented in v3 | fct_* file / handler |
|:--------|:----------|:------------|:--------------|:--------------------|:---------------------|
| AT SA | string  | save current configuration at the next free EEPROM slot under the give name (e.g. "AT SA mouse" stores a slot with the name "mouse"  | v2 | yes | no |
| AT LO | string  | load a configuration from the EEPROM (e.g. "AT LO mouse")  | v2 | yes | handler_vb |
| AT LA | --  | load all slots and print the configuration. Note: if no slot is available, this command initializes the mouse slot (this is only active if ACTIVATE_V25_COMPAT is defined)   | v2 | yes | no |
| AT LI | --  | list all available slots   | v2 | yes | no |
| AT NE | --  | load next slot (wrap around after the last slot)  | v2 | yes | handler_vb |
| AT DE | --  | delete all slots  | v2 | yes | no |
| AT DL | number (0-250) | delete one slot.  | v3 | yes | no |
| AT DN | string | delete one slot by name  | v3 | yes | no |
| AT NC | --  | do nothing  | v2 | yes | no |
| AT E0 | --  | disable debug output  | v2 | never, use make monitor | - |
| AT E1 | --  | enable debug output  | v2 | never, use make monitor | - |
| AT E2 | --  | enable debug output (extended) | v2 | never, use make monitor | - |
**Mouthpiece settings** 
| Command | Parameter | Description | Available since | Implemented in v3 | fct_* file / handler |
|:--------|:----------|:------------|:--------------|:--------------------|:---------------------|
| AT MM | number (0,1,2,3)  | use the mouthpiece either as mouse cursor (AT MM 1), alternative function (AT MM 0), joystick (AT MM 2) or disable it (AT MM 3)  | v2 | yes | no |
| AT SW | --  | switch between cursor and alternative mode  | v2 | yes | no |
| AT SR | --  | start reporting out the raw sensor values ("VALUES:<pressure>,<up>,<down>,<left>,<right>,<x>,<y>" | v2 | yes | no |
| AT ER | --  | stop reporting the sensor values  | v2 | yes | no |
| AT CA | --  | trigger zeropoint calibration  | v2 | yes | handler_vb |
| AT AX | number (0-100)  | sensitivity x-axis  | v2 | yes | no |
| AT AY | number (0-100)  | sensitivity y-axis  | v2 | yes | no |
| AT AC | number (0-100)  | acceleration  | v2 | yes | no |
| AT MS | number (0-100)  | maximum speed  | v2 | yes | no |
| AT DX | number (0-10000)  | deadzone x-axis  | v2 | yes | no |
| AT DY | number (0-10000)   | deadzone y-axis  | v2 | yes | no |
| AT TS | number (0-512)  | sip action threshold  | v2 | yes | no |
| AT SS | number (0-512)  | strong-sip action threshold  | v2 | yes | no |
| AT TP | number (512-1023)  | puff action threshold  | v2 | yes | no |
| AT SP | number (512-1023)  | strong-puff action threshold  | v2 | yes | no |
| AT OT | number (0-15)   | On-the-fly calibration, threshold for detecting idle | v3 | yes | no |
| AT OC | number (5-15)   | On-the-fly calibration, idle counter before calibrating | v3 | yes | no |

**Joystick settings**
| Command | Parameter | Description | Available since | Implemented in v3 | fct_* file / handler |
|:--------|:----------|:------------|:--------------|:--------------------|:---------------------|
| AT JX | number (0-1023) + number(0,1)  | Joystick X-axis <sup>[D](#footnoteD)</sup> | v2 | yes | handler_hid |
| AT JY | number (0-1023) + number(0,1)  | Joystick Y-axis <sup>[D](#footnoteD)</sup> | v2 | yes | handler_hid |
| AT JZ | number (0-1023) + number(0,1)  | Joystick Z-axis <sup>[D](#footnoteD)</sup> | v2 | yes | handler_hid |
| AT JT | number (0-1023) + number(0,1)  | Joystick Z-rotate <sup>[D](#footnoteD)</sup> | v2 | yes | handler_hid |
| AT JS | number (0-1023) + number(0,1)  | Joystick Slider left <sup>[D](#footnoteD)</sup> | v2 | yes | handler_hid |
| AT JU | number (0-1023) + number(0,1)  | Joystick Slider right <sup>[D](#footnoteD)</sup> | v3 | untested | handler_hid |
| AT JP | number (1-32)  | Button press (if assigned to a VB, release depends on button release) | v2 | yes | handler_hid |
| AT JC | number (1-32)  | Button click | v3 | yes | handler_hid |
| AT JR | number (1-32)  | Button release | v2 | yes | handler_hid |
| AT JH | number (-1, 0-7) + number(0,1) | Joystick hat (rest position: -1, 0-7 (mapped to 45° steps)) <sup>[D](#footnoteD)</sup> | v2 | yes | handler_hid |

Please note, that joystick is currently not available for Bluetooth connections.

<a name="footnoteD"><b>D</b></a>: The second parameter sets the release mode of this command. 

If set to 0 (or no parameter given, compatible to v2), the axis/slider/hat won't be set to a different value except another VB is used to set it differently. 

If set to 1, the axis/slider/hat will be released to its idle position on a VB release action (idle values: axis - 512; slider - 0; hat - -1). In singleshot mode, the release actions is sent immediately afterwards!

**Infrared commands** 
| Command | Parameter | Description | Available since | Implemented in v3 | fct_* file / handler |
|:--------|:----------|:------------|:--------------|:--------------------|:---------------------|
| AT IR | string (2-32chars)  | record a new infrared command, store it with the given name  | v2 | yes | no |
| AT IP | string (2-32chars)  | replay a recorded IR command, stored with the given name  | v2 | yes | fct_infrared |
| AT IH | string (max: ~250chars)  | play a hex string (replay a given hex string sent by "AT IR") | v3 | no | fct_infrared |
| AT IC | string (2-32chars) | clear an IR command, defined by the name  | v2 | yes | no |
| AT IW | --  | wipe all IR commands  | v2 | yes | no |
| AT IT | number (2-100) | timeout for recording IR commands (time[ms] between 2 edges) | v2 | yes | no |
| AT IL |   | list all available stored IR commands  | v2 | yes | no |
| AT IX | number (1-99) | Delete one IR slot. | v3 | yes | no |
| AT II | string (2-32chars) | Set an idle IR command. Will be sent AFTER EACH normally sent command. | v2.7 | no | no |


**SmartHome interface**
| Command | Parameter | Description | Available since | Implemented in v3 | fct_* file / handler |
|:--------|:----------|:------------|:--------------|:--------------------|:---------------------|
| AT MQ | string (5-240chars)  | publish this data on the given topic, e.g.: "lights/livingroom:ON" <sup>[E](#footnoteE)</sup> | v3 | yes | handler_vb |
| AT MH | string (6-100chars)  | set a new MQTT broker, e.g.: "mqtt://localhost:1883" <sup>[E](#footnoteE)</sup> | v3 | yes | no |
| AT ML | string (1char) | set a new MQTT delimiter symbol (topic vs data, default: ":") for AT MQ | v3 | yes | no |
| AT WP | string (8-63chars) | set a new WiFi password (when this device is connected as WiFi client!) <sup>[F](#footnoteF)</sup>  | v3 | yes | no |
| AT WH | string (4-31chars) | set a new WiFi name to connect to (when this device is connected as WiFi client!) <sup>[F](#footnoteF)</sup>  | v3 | yes | no |
| AT RE | string (8-200chars) | calling a REST API via HTTP get. If not connected yet, Wifi will be activated in station mode <sup>[G](#footnoteG)</sup>  | v3 | yes | handler_vb |

<a name="footnoteE"><b>E</b></a>: No QoS or retain flags are implemented for MQTT. Please note, that WiFi is only started on the first execution of an AT MQ command.
<a name="footnoteF"><b>F</b></a>: Please note, that WiFi in station mode will be activated on the first call of a WiFi related command (e.g. AT MQ or AT RE).
<a name="footnoteG"><b>G</b></a>: Due to a limitation in the esp-idf, you cannot use HTTPS without providing a server certificate. We consider extracting the certificate
for each host too complicated, please use HTTP instead. We also thought of integrating the whole Mozilla certificate DB, but this would take REALLY long to try all certificates.


After WiFi is enabled in station mode, it is possible to activate the SoftAP mode for the configuration GUI, but once this is done, the device must be restarted to restore full functionality!


## Button assignments - FLipMouse

The FLipMouse version 2 has 1 internal push-button and 2 jack plugs for external buttons. 
Version 3 uses the second internal button, which was previously the program button for the Teensy, as virtual button 0.

**Note:** VB0 (internal button 2) is configured as activation button to switch on/off WiFi for configuring the FLipMouse.
Feedback to the user is still not defined, will be announced here.

In addition other functions are mapped to virtual buttons, so they can be configured the same.
Following number mapping is used for the __AT BM__ command:

|VB nr | Function |
|:-----|:---------|
| 0    | Internal button 2|
| 1    | Internal button 1|
| 2    | External button 1|
| 3    | External button 2|
| 4    | Mouthpiece in threshold mode: UP |
| 5    | Mouthpiece in threshold mode: DOWN |
| 6    | Mouthpiece in threshold mode: LEFT |
| 7    | Mouthpiece in threshold mode: RIGHT |
| 8    | Sip      |
| 9    | Puff     |
| 10   | Strong Sip |
| 11   | Strong Puff |
| 12   | Strong Sip + Up |
| 13   | Strong Sip + Down |
| 14   | Strong Sip + Left |
| 15   | Strong Sip + Right |
| 16   | Strong Puff + Up |
| 17   | Strong Puff + Down |
| 18   | Strong Puff + Left |
| 19   | Strong Puff + Right |


These assignments are declared in file common.h.

**Note:** The C# GUI starts with VB 1, so it cannot configure the internal button.

## Button assignments - FABI

The FABI supports up to 9 buttons (normally external, but they can be hardwired like internal buttons).
In addition, one analog channel is support for sip/puff functionality.
Following number mapping is used for the __AT BM__ command:

|VB nr | Function |
|:-----|:---------|
| 0    | Button 1|
| 1    | Button 2|
| 2    | Button 3|
| 3    | Button 4|
| 4    | Button 5|
| 5    | Button 6|
| 6    | Button 7|
| 7    | Button 8|
| 8    | Button 9|
| 9    | Sip      |
| 10   | Puff     |
| 11   | Long Press Button 1 |
| 12   | Long Press Button 2 |
| 13   | Long Press Button 3 |
| 14   | Long Press Button 4 |
| 15   | Long Press Button 5 |
| 16   | Long Press Button 6 |
| 17   | Long Press Button 7 |
| 18   | Long Press Button 8 |
| 19   | Long Press Button 9 |


These assignments are declared in file common.h.

**Note:** The FABI C# GUI cannot configure VB0, a solution is pending.

## Key identifiers

The key identifiers are necessary to determine which key should be pressed or released by either the __AT KP__ or the __AT KR__ command. Following keys are possible:

    KEY_A   KEY_B   KEY_C   KEY_D    KEY_E   KEY_F   KEY_G   KEY_H   KEY_I   KEY_J    KEY_K    KEY_L
    KEY_M   KEY_N   KEY_O   KEY_P    KEY_Q   KEY_R   KEY_S   KEY_T   KEY_U   KEY_V    KEY_W    KEY_X 
    KEY_Y   KEY_Z   KEY_1   KEY_2    KEY_3   KEY_4   KEY_5   KEY_6   KEY_7   KEY_8    KEY_9    KEY_0
    KEY_F1  KEY_F2  KEY_F3  KEY_F4   KEY_F5  KEY_F6  KEY_F7  KEY_F8  KEY_F9  KEY_F10  KEY_F11  KEY_F12
    
    KEY_RIGHT   KEY_LEFT       KEY_DOWN        KEY_UP      KEY_ENTER    KEY_ESC   KEY_BACKSPACE   KEY_TAB	
    KEY_HOME    KEY_PAGE_UP    KEY_PAGE_DOWN   KEY_DELETE  KEY_INSERT   KEY_END	  KEY_NUM_LOCK    KEY_SCROLL_LOCK
    KEY_SPACE   KEY_CAPS_LOCK  KEY_PAUSE       KEY_SHIFT   KEY_CTRL     KEY_ALT   KEY_RIGHT_ALT   KEY_GUI 
    KEY_RIGHT_GUI 

New in version 3:

    KEY_F13     KEY_F14     KEY_F15     KEY_F16     KEY_F17     KEY_F18     
    KEY_F19     KEY_F20     KEY_F21     KEY_F22     KEY_F23     KEY_F24
    KEY_MEDIA_POWER         KEY_MEDIA_RESET         KEY_MEDIA_SLEEP
    KEY_MEDIA_MENU          KEY_MEDIA_SELECTION     KEY_MEDIA_ASSIGN_SEL
    KEY_MEDIA_MODE_STEP     KEY_MEDIA_RECALL_LAST   KEY_MEDIA_QUIT
    KEY_MEDIA_HELP          KEY_MEDIA_CHANNEL_UP    KEY_MEDIA_CHANNEL_DOWN
    KEY_MEDIA_SELECT_DISC   KEY_MEDIA_ENTER_DISC    KEY_MEDIA_REPEAT
    KEY_MEDIA_VOLUME        KEY_MEDIA_BALANCE       KEY_MEDIA_BASS
    
    KEY_MEDIA_PLAY          KEY_MEDIA_PAUSE         KEY_MEDIA_RECORD
    KEY_MEDIA_FAST_FORWARD  KEY_MEDIA_REWIND        KEY_MEDIA_NEXT_TRACK
    KEY_MEDIA_PREV_TRACK    KEY_MEDIA_STOP          KEY_MEDIA_EJECT
    KEY_MEDIA_RANDOM_PLAY   KEY_MEDIA_STOP_EJECT    KEY_MEDIA_PLAY_PAUSE
    KEY_MEDIA_PLAY_SKIP     KEY_MEDIA_MUTE          KEY_MEDIA_VOLUME_INC
    KEY_MEDIA_VOLUME_DEC
    
    KEY_SYSTEM_POWER_DOWN   KEY_SYSTEM_SLEEP        KEY_SYSTEM_WAKE_UP
    KEY_MINUS               KEY_EQUAL               KEY_LEFT_BRACE
    KEY_RIGHT_BRACE         KEY_BACKSLASH           KEY_SEMICOLON
    KEY_QUOTE               KEY_TILDE               KEY_COMMA
    KEY_PERIOD              KEY_SLASH               KEY_PRINTSCREEN
    KEY_MENU
    
    KEYPAD_SLASH            KEYPAD_ASTERIX          KEYPAD_MINUS
    KEYPAD_PLUS             KEYPAD_ENTER            KEYPAD_1
    KEYPAD_2                KEYPAD_3                KEYPAD_4
    KEYPAD_5                KEYPAD_6                KEYPAD_7
    KEYPAD_8                KEYPAD_9                KEYPAD_0

All possible key identifiers are defined in keylayouts.h (no doxygen available, please use the source).

## Keyboard locales

With firmware version 3 it is possible to change the keyboard layout of FLipMouse/FABI by the **AT KL** command.
Please note that the keyboard layout is only relevant for the **AT KW** command, where an ASCII/Unicode text is written by
the FLipMouse/FABI. AT commands controlled by key identifiers (e.g., KEY_Y, KEY_Z) are mapped to an american layout.

It is recommended to use the __AT KW__ command as much as possible when typing text. For keyboard shortcuts, press&release actuated
programs __AT KP/KH/KR__ is recommended.

Following keyboard locales are currently available:


| Keyboard locale | Number (parameter for **AT KL** ) |
|:----------------|:----------------------------------|
| LAYOUT_US_ENGLISH | 0 |
| LAYOUT_US_INTERNATIONAL | 1 |
| LAYOUT_GERMAN | 2 |
| LAYOUT_GERMAN_MAC | 3 |
| LAYOUT_CANADIAN_FRENCH | 4 |
| LAYOUT_CANADIAN_MULTILINGUAL | 5 |
| LAYOUT_UNITED_KINGDOM | 6 |
| LAYOUT_FINNISH | 7 |
| LAYOUT_FRENCH | 8 |
| LAYOUT_DANISH | 9 |
| LAYOUT_NORWEGIAN | 10 |
| LAYOUT_SWEDISH | 11 |
| LAYOUT_SPANISH | 12 |
| LAYOUT_PORTUGUESE | 13 |
| LAYOUT_ITALIAN | 14 |
| LAYOUT_PORTUGUESE_BRAZILIAN | 15 |
| LAYOUT_FRENCH_BELGIAN | 16 |
| LAYOUT_GERMAN_SWISS | 17 |
| LAYOUT_FRENCH_SWISS | 18 |
| LAYOUT_SPANISH_LATIN_AMERICA | 19 |
| LAYOUT_IRISH | 20 |
| LAYOUT_ICELANDIC | 21 |
| LAYOUT_TURKISH | 22 |
| LAYOUT_CZECH | 23 |
| LAYOUT_SERBIAN_LATIN_ONLY | 24 |
