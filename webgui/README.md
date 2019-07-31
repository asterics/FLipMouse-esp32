# FlipMouse/FABI Web GUI

This is a JS/HTML WebGUI for FABI and FLipMouse.
It serves as configuration GUI for setting up a FABI or FLipMouse device.

The WiFi hotspot is enabled by pressing the internal button for 5 seconds.
Any WiFi capable device can connect to the hotspot, the ESP32 will act as captive portal.

If the GUI is not loaded automatically, please open this URL in the browser:
__http://192.168.4.1/__

# Install

## Prerequisites

* mkspiffs tool: https://github.com/igrr/mkspiffs
* gzip
* esp-idf (for uploading via esptool.py)
* npm (nodejs)

## Compiling & Uploading

`bash ./makespiffs.sh <FABI/FM> <USB port>`

Example for a FLipMouse on port /dev/ttyUSB0:

`bash ./makespiffs.sh FM /dev/ttyUSB0`

# Folders & Files

* __src__ original WebGUI source files
* __minified__ output of npm for minified source files
* __minified_gz__ output of gzip for compressed&minified source files
* __spiffs_content__ additional files which are packed into the image file (factory configuration)
* __makespiffs.sh__ Bash script for generating & flashing the SPIFFS image to the ESP32
* __spiffs_FABI.img__ FABI WebGUI image with additional files
* __spiffs_FM.img__ FLipMouse WebGUI image with additional files


# Acknowledgements

The code is based on the ESP32 IDF examples,
as well as the ESP32light project by Luca Dentella, see:
https://github.com/lucadentella/esp32lights

and the captive portal DNS code by Jeroen Domburg and Cornelis for, see:
https://github.com/cornelis-61/esp32_Captdns 

Thank you very much for this great contributions, guys !

Licensed with GPLv3, see:
https://www.gnu.org/licenses/gpl-3.0.de.html

