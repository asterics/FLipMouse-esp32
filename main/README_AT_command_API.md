# AT-commands / Communication API

The FLipmouse firmware provides 4 different USB HID device classes:
* Mouse
* Keyboard
* Joystick
* Serial CDC

All configuration is done via the serial interface, which provides the persistent configuration of all functionalities and in addition a live mode (e.g. write "mouse move" to the serial port and the firmware moves the mouse cursor).

The serial port configuration is **115200 8N1** (even these settings are not necessary due to the USB encapsulation).

**Note:**
Please note that all following commands, which do NOT have a FUNCTIONAL task, cannot be assigned to a virtual button (via AT BM).
These commands can be used to configure the FLipMouse/FABI, in a macro or for slot management.

Following commands are currently available:

**General Commands**
| Command | Parameter | Description | Available since | Implemented in v3 | FUNCTIONAL task |
|:--------|:----------|:------------|:--------------|:--------------------|:----------------|
| AT    | --  | returns OK   | v2 | yes | no |
| AT ID | --  | returns the current version string  | v2 | yes | no |
| AT BM | number (1-11)  | set the button, which corresponds to the next command. The button assignments are described on the bottom | v2 | untested | no |
| AT BL | -- | enable/disable output of triggered virtual buttons. Is used with AT BM for command learning | v3 | no | ? |
| AT MA | string | execute macro (space separated list of commands, see [Macros](https://github.com/asterics/FLipMouse/wiki/macros)) | v2 | no | yes (task_macro) |
| AT WA | number | wait/delay (ms); useful for macros. Does nothing if not used in macros. | v2 | no | no |
| AT RO | number (0,90,180,270) | orientation (0 => LEDs on top) | v2 | no | no |
| AT KL | number | Set keyboard locale (locale defines are listed below) | v3 | yes | no |
| AT BT | number (0,1,2) | Bluetooth mode, 1=USB only, 2=BT only, 3=both(default) | v2 | Working for USB, untested for BLE | no |
**USB HID Commands**
| Command | Parameter | Description | Available since | Implemented in v3 | FUNCTIONAL task |
|:--------|:----------|:------------|:--------------|:--------------------|:----------------|
| AT CL | --  | Click left mouse button | v2 | yes | yes (task_mouse) |
| AT CR | --  | Click right mouse button  | v2 | yes | yes (task_mouse) |
| AT CM | --  | Click middle mouse button  | v2 | yes | yes (task_mouse) |
| AT CD | --  | Doubleclick left mouse button  | v2 | yes | yes (task_mouse) |
|       |   |   ||| |
| AT PL | --  | Press+hold left mouse button  | v2 | yes | yes (task_mouse) |
| AT PR | --  | Press+hold right mouse button  | v2 | yes | yes (task_mouse) |
| AT PM | --  | Press+hold middle mouse button  | v2 | yes | yes (task_mouse) |
|       |   |   ||| |
| AT RL | --  | Release left mouse button  | v2 | yes | yes (task_mouse) |
| AT RR | --  | Release right mouse button  | v2 | yes | yes (task_mouse) |
| AT RM | --  | Release middle mouse button  | v2 | yes | yes (task_mouse) |
|       |   |   ||| |
| AT WU | --  | Move mouse wheel up  | v2 | yes | yes (task_mouse) |
| AT WD | --  | Move mouse wheel down  | v2 | yes | yes (task_mouse) |
| AT WS | number (1-)  | Set mousewheel stepsize (e.g.: "AT WS 3" sets the stepsize to 3 rows)| v2 | yes | no |
|       |   |   ||| |
| AT MX | number  | Move mouse (X direction), e.g. AT MX -25  | v2 | yes | yes (task_mouse) |
| AT MY | number  | Move mouse (Y direction), e.g. AT MY 10  | v2 | yes | yes (task_mouse) |
|       |   |   ||| |
| AT KW | string  | Keyboard write (e.g. "AT KW Hi" types "Hi") | v2 | untested | yes (task_keyboard) |
| AT KP | string  | Key press (e.g. "AT KP KEY_UP" presses & releases the up arrow key), a full list of supported key identifiers is provided on the bottom  | v2 | untested | yes (task_keyboard) |
| AT KH | string  | Key hold (e.g. "AT KH KEY_UP" presses & holds the up arrow key), a full list of supported key identifiers is provided on the bottom  | v2 | untested | yes (task_keyboard) |
| AT KR | string  | Key release (e.g. "AT KR KEY_UP" releases the up arrow key)  | v2 | untested | yes (task_keyboard) |
| AT RA | --  | Release all keys  | v2 | untested | no |
**Storage commands** 
| Command | Parameter | Description | Available since | Implemented in v3 | FUNCTIONAL task |
|:--------|:----------|:------------|:--------------|:--------------------|:----------------|
| AT SA | string  | save current configuration at the next free EEPROM slot under the give name (e.g. "AT SA mouse" stores a slot with the name "mouse"  | v2 | no | no |
| AT LO | string  | load a configuration from the EEPROM (e.g. "AT LO mouse")  | v2 | no | yes (task_configswitcher) |
| AT LA | --  | load all slots and print the configuration   | v2 | no | no |
| AT LI | --  | list all available slots   | v2 | no | no |
| AT NE | --  | load next slot (wrap around after the last slot)  | v2 | no | yes (task_configswitcher) |
| AT DE | --  | delete all slots  | v2 | untested | no |
| AT DL | number (1-) | delete one slot.  | v3 | untested | no |
| AT DN | string | delete one slot by name  | v3 | no | no |
| AT NC | --  | do nothing  | v2 | no | no |
| AT E0 | --  | disable debug output  | v2 | never, use make monitor | - |
| AT E1 | --  | enable debug output  | v2 | never, use make monitor | - |
| AT E2 | --  | enable debug output (extended) | v2 | never, use make monitor | - |
**Mouthpiece settings** 
| Command | Parameter | Description | Available since | Implemented in v3 | FUNCTIONAL task |
|:--------|:----------|:------------|:--------------|:--------------------|:----------------|
| AT MM | number (0,1,2)  | use the mouthpiece either as mouse cursor (AT MM 1), alternative function (AT MM 0) or joystick (AT MM 2)  | v2 | untested | no |
| AT SW | --  | switch between cursor and alternative mode  | v2 | untested | no |
| AT SR | --  | start reporting out the raw sensor values | v2 | untested | no |
| AT ER | --  | stop reporting the sensor values  | v2 | untested | no |
| AT CA | --  | trigger zeropoint calibration  | v2 | untested | yes (task_calibration) |
| AT AX | number (0-100)  | sensitivity x-axis  | v2 | untested | no |
| AT AY | number (0-100)  | sensitivity y-axis  | v2 | untested | no |
| AT AC | number (0-100)  | acceleration  | v2 | untested | no |
| AT MS | number (0-100)  | maximum speed  | v2 | no | no |
| AT DX | number (0-10000)  | deadzone x-axis  | v2 | no | no |
| AT DY | number (0-10000)   | deadzone y-axis  | v2 | no | no |
| AT TS | number (0-512)  | sip action threshold  | v2 | untested | no |
| AT SS | number (0-512)  | strong-sip action threshold  | v2 | untested | no |
| AT TP | number (512-1023)  | puff action threshold  | v2 | untested | no |
| AT SP | number (512-1023)  | strong-puff action threshold  | v2 | untested | no |
| AT GU | number (0-100)  | "up" sensor gain  | v2 | untested | no |
| AT GD | number (0-100)  | "down" sensor gain  | v2 | untested | no |
| AT GL | number (0-100)  | "left" sensor gain  | v2 | untested | no |
| AT GR | number (0-100)  | "right" sensor gain  | v2 | untested | no |
**Joystick settings**
| Command | Parameter | Description | Available since | Implemented in v3 | FUNCTIONAL task |
|:--------|:----------|:------------|:--------------|:--------------------|:----------------|
| AT JX | number (0-1023)  | Joystick X-axis  | v2 | no | yes (task_joystick) |
| AT JY | number (0-1023)  | Joystick Y-axis  | v2 | no | yes (task_joystick) |
| AT JZ | number (0-1023)  | Joystick Z-axis  | v2 | no | yes (task_joystick) |
| AT JT | number (0-1023)  | Joystick Z-rotate  | v2 | no | yes (task_joystick) |
| AT JS | number (0-1023)  | Joystick Slider left  | v2 | no | yes (task_joystick) |
| AT JP | number (1-32)  | Button press | v2 | no | yes (task_joystick) |
| AT JR | number (1-32)  | Button release | v2 | no | yes (task_joystick) |
| AT JH | number (-1, 0-315) | Joystick hat (rest position: -1, 0-315 in 45° steps) | v2 | no | yes (task_joystick) |
**Infrared commands** 
| Command | Parameter | Description | Available since | Implemented in v3 | FUNCTIONAL task |
|:--------|:----------|:------------|:--------------|:--------------------|:----------------|
| AT IR | string  | record a new infrared command, store it with the given name  | v2 | no | yes/no? (task_infrared) |
| AT IP | string  | replay a recorded IR command, stored with the given name  | v2 | no | yes (task_infrared) |
| AT IC | string  | clear an IR command, defined by the name  | v2 | no | no |
| AT IW | --  | wipe all IR commands  | v2 | no | no |
| AT IT | number (2-100) | timeout for recording IR commands (time[ms] between 2 edges) | v2 | no | no |
| AT IL |   | list all available stored IR commands  | v2 | no | no |


## Button assignments

The FLipMouse has 1 internal push-button and 2 jack plugs for external buttons. In addition other functions are mapped to virtual buttons, so they can be configured the same.
Following number mapping is used for the __AT BM__ command:

|VB nr | Function |
|:-----|:---------|
| 0    | External button 1|
| 1    | External button 2|
| 2    | Internal button 1|
| 3    | Internal button 2 (might not be implemented) |
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
| 21   |          |
| 22   |          |
| 23   |          |
| 24   |          |
| 25   |          |
| 26   |          |
| 27   |          |
| 28   |          |
| 29   |          |
| 30   |          |
| 31   |          |

These assignments are declared in file common.h.

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

TBA... (see keyboard.h)
