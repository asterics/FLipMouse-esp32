# FLipMouse & FABI - version 3

The FLipMouse (Finger- and Lipmouse) is a device, which is intended to be a replacement for a normal PC mouse.
Instead of moving the mouse device with your hand and clicking with your fingers, you move the mouse cursor by applying very low forces to the mouthpiece.
The clicking functionality is done by sip and puff at the mouthpiece.

This device is originally designed for people with motor disablities, which can not or will not efford a medical device as mouse replacement.

Our goal is to provide an affordable solution for everybody who is not able to use a PC or smartphone in the usual way.


![Front view of 3 FLipmice in different colors (black, pink and transparent orange)](FLipmouse1.jpg) 



# The Hardware

A reworked version of the FLipMouse PCB with the ESP32 is now available in the hardware subdirectory. This PCB works with a few modifications (see README in this folder). In addition, a FABI-ESP32 PCB is also added. 
This version is not tested by now, test reports or notifications will be added.

# The Firmware

This repository (an especially this Doxygen documentation) contains all firmware files which are necessary to program a FLipMouse or a FABI device. Please explore this page if you are interested in extending, developing or modifying
the FLipMouse. If you are looking for the firmware for the LPC11U14 USB chip, you will find it in [this](https://github.com/benjaminaigner/usb_bridge) repository.

# The Host Software

Prior to version 3, the main configuration option was a C# based GUI (for FLipMouse and FABI).
Version 3 will be configured via the mobile Wifi hotspot. Additionally, the C# GUI can be used as well (but there might be a reduced availability of new functions).

# Further Information

Most of the work for the original FLipMouse was done within the AsTeRICS Academy project, funded by the City of Vienna ([AsTeRICS Academy homepage](http://www.asterics-academy.net)).
Currently ongoing work on an ESP32 based FLipMouse is done within the [ToRaDes](https://embsys.technikum-wien.at/projects/torades/index.php) project, also party funded by the City of Vienna (municipal department 23, MA23):

If the ESP32 FLipMouse solution is not suitable for you, maybe you want to have a look at our other projects or previous versions of FLipMouse/FABI:

* AsTeRICS: [AsTeRICS framework homepage](http://www.asterics.eu), [AsTeRICS framework GitHub](https://github.com/asterics/AsTeRICS): The AsTeRICS framework provides a much higher flexibility for building assistive solutions. 
The FLipMouse is also AsTeRICS compatible, so it is possible to use the raw input data for a different assistive solution.

* FABI v2.x: [FABI: Flexible Assistive Button Interface GitHub](https://github.com/asterics/FABI): The Flexible Assistive Button Interface (FABI) provides basically the same control methods (mouse, clicking, keyboard,...), but the input
is limited to simple buttons. Therefore, this interface is at a very low price (if you buy the Arduino Pro Micro from China, it can be under 5$).

* FLipMouse v2.x: [FLipMouse: Finger- and Lipmouse GitHub](https://github.com/asterics/FLipMouse)


## Other problems?

Please visit the [wiki](https://github.com/asterics/FLipMouse/wiki). Or open an issue [here](https://github.com/asterics/FLipMouse/issues). 
