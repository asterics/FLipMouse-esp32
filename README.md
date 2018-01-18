# FLipMouse goes ESP32

This repository contains the ongoing progress on the firmware for the FLipMouse version3.
It will be API compatible with version 2, but with major improvements on speed (instead of one CortexM0 we will have a dual-core Tensilica and an additional CortexM0 for USB).

If you want to have a look at the FLipMouse, follow [this](https://github.com/asterics/FLipMouse) link to the main repository.

The BLE-HID stuff is based on the other [repository](https://github.com/asterics/esp32_mouse_keyboard), where we use the ESP32 as BLE module replacement (EZKey modules are really buggy and expensive).

## WARNING: THIS EARLY WORK IN PROGRESS

