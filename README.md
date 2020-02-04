# FLipMouse goes ESP32

This repository contains the ongoing progress on the firmware for the FLipMouse version3.
It will be API compatible with version 2, but with major improvements on speed (instead of one CortexM0 we will have a dual-core Tensilica and an additional CortexM0 for USB).

If you want to have a look at the FLipMouse, follow [this](https://github.com/asterics/FLipMouse) link to the main repository.

The BLE-HID stuff is based on the other [repository](https://github.com/asterics/esp32_mouse_keyboard), where we use the ESP32 as BLE module replacement (EZKey modules are really buggy and expensive).

__Following 3 steps are necessary to completely update/flash the firmware and the necessary support files/devices:__

## Building the ESP32 firmware

Please follow the step-by-step instructions of Espressif's ESP-IDF manual to setup the build infrastructure:
[Setup Toolchain](https://esp-idf.readthedocs.io/en/latest/get-started/index.html#setup-toolchain)

Due to major changes in the esp-idf, the sdkconfig must be recreated for the first build:
* Execute `make menuconfig` in this directory
* Change: `Serial flasher config` -> `Flash size` to __4MB__
* Change: `Partition table` -> `Partition table` to __Custom partition table CSV__
* Change: `Component config` -> `Bluetooth` to enabled

After a successful setup, you should be able to build the FLipMouse/FABI firmware by executing 'make flash monitor'.

## Building the LPC11U14 firmware (USB bridging chip)

The dedicated USB chip firmware is located in this repository: [usb_bridge](https://github.com/benjaminaigner/usb_bridge/).
Please clone this repository, and import it into the MCUXpresso IDE.

You can download the IDE here:
[MCUXpresso IDE](http://www.nxp.com/mcuxpresso/ide)

After a successful build, you need to flash the firmware to the LPC11U14 chip. We recommend using a development board by Embedded Artists,
because it is very useful for initial testing & debugging. It is possible to split this board into two pieces, one LPC11U14 target and a JTAG
programmer, which we use on a fully assembled FLipMouse. Please refer to the corresponding documents of this board how to connect the JTAG header to
our FLipMouse (will be announced as soon as the PCB is final).

LPCXpresso development board:

[LPCXpresso 11U14](http://embeddedartists.com/products/lpcxpresso/lpc11U14_xpr.php)

## Uploading the WebGUI

The FLipMouse with ESP32 contains a web interface for customizing the parameters. The web page itself is located in the FAT partition as the config data.


-----------------------

TBA: I think it is very easy to use mkfats by copying the component into ESP-IDF and simply call **make mkfatfs** to build the tool and **make flashfatfs** to upload the image.

**BUT** we have a 8.3 naming convention here...

Currently proposed workflow:

-----------------------

Please clone/download the [mkfats](https://github.com/jkearins/ESP32_mkfatfs) tool, and copy the folder `ESP32_mkfatfs/components/mkfatfs` into your
ESP-IDF's components path.

Execute `make mkfatfs` in this repository to build the *mkfatfs* tool.

If you want to update the webpage on the FLipMouse/FABI device, execute:

`make makefatfs flashfatfs`

_WARNING:_** All your slot configuration and IR commands will be deleted by this operation.


## WARNING: THIS IS EARLY WORK IN PROGRESS

# Acknowledgements

Supporters, MA23, people using this device and many more for input.

ESP-IDF developers for HOG testcode

mkfatfs project

thanks to Luca Dentella for the esp32lights demo, see http://www.lucadentella.it/en/2018/01/08/esp32lights/

thanks to Jeroen Domburg and Cornelis for the captive portal DNS code, see: https://github.com/cornelis-61/esp32_Captdns

thanks to Thomas Barth for esp32 Websocket demo code, see https://github.com/ThomasBarth/WebSockets-on-the-ESP32/

thanks to Lucas Bruder for the Neopixel library for ESP32, see https://github.com/Lucas-Bruder/ESP32_LED_STRIP/
