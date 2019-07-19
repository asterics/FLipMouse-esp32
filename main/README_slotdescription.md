# How configuration is handled

A FABI/FLipmouse device is able to switch between different configurations on the fly.
This config switch can be triggered by a serial command (via AT commands) or by a configurable user-action.

Normally, configuration consists of following parts:

* A set of different, loadable, slots (1-250 are possible). Slots can be named and loaded by name. A typical basic configuration uses 2 slots, the first one for mouse control. Mouse movement is
done by the mouthpiece (FLipMouse) or the first four buttons (FABI). Clicking can be done by other buttons (FABI) or sip and puff (FLipMouse). One button is reserved for slot switch. The second slot is used to trigger keyboard arrow keys by the first four buttons (FABI) or the mouthpiece (FLipMouse).
* General configs for each slot (sensitivity, mouthpiece function, orientation, ...)
* Functions for each possible action (called virtual button). There is a set of possible functions available, as it is described in AT command API.
* General settings for the device, independent from the loaded slot (e.g., Wifi password in SoftAP mode, Wifi station/password in station mode, MQTT broker,...)

Note: a normal VB settting is done via a combination of "AT BM xx" and an additional action e.g., "AT CA". The AT BM command tells the firmware, that
the next command will be assigned to this VB.

Note: in memory, the config is saved in a text file, with AT commands. This way, we can do upgrades without worrying about the config file format.
These files are named __000.set__ to __xxx.set__ on the SPIFFS. In factory settings, we program one mouse and one keyboard slot (000.set and 001.set).
An additional file is saved, which will be used for restoring factory settings (flip.set). A factory reset will delete all numbered .set files, and
restore 000.set & 001.set from flip.set.

![Configuration organization](slots.png)

## Infrared configuration

Recorded infrared commands are stored on SPIFFS as well. Possible file names are IR_000.set up to IR_249.set.
These files contain the binary data recorded by the RMT engine of the ESP32.

## Downloading settings via the webbrowser

If you desire to save a configuration file, you could download it in configuration mode (pressing the internal button for 5s and connecting to the Wifi hotspot).

In the browser, you could load the configuration GUI via http://192.168.4.1 (normally, you will be redirected to this IP address anyway because of a captive portal).
Downloading the slots can be done via: __http://192.168.4.1/000.set__ or __http://192.168.4.1/IR_000.set__

Note: currently, there is no possibility to UPLOAD these configs.


