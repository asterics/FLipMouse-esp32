# FLipMouse / FABI WebGUI

This folder contains the source files for the FABI/FLipMouse WebGUI. The GUI is either flashed on the ESP32 (on FABI/FLipMouse with ESP32) or
delivered as program for configuring the FLipMouse or FABI device.

__Note:__ The procedure is still under development.

# Required tools

* npm
* electron
* electron-packager (installed for CLI usage)

Installation:

1. Install npm according to the official manual
2. Install the npm dependencies: `npm install`
3. Install electron packager (as root): `sudo npm install electron-packager -g`
optional:
4. Install for building macOS installers (as root): `sudo npm install electron-installer-dmg -g`
5. Install for building Win installers (as root): `sudo npm install electron-installer-windows -g`

# Development

Open the corresponding folder (flipmouse or fabi) and start the GUI via: `npm start`

# Building binaries for each platform

Open the corresponding folder (flipmouse or fabi) and build for all platforms:
* __FLipMouse__ `electron-packager . FLipMouseGUI --all --overwrite`
* __FABI__ `electron-packager . FabiGUI --all --overwrite`


Additional packaging:

* __MacOS__ `electron-installer-dmg ./FLipMouseGUI-darwin-x64/FLipMouseGUI.app FLipMouseGUI --overwrite` (only works on macOS)
* __Windows__ `electron-installer-windows --src ./flipmouse/FLipMouseGUI-win32-x64/ --dest .` (attention: execute from one level above!)


# Building installers on Debian

* macOS can be built only on macOS
* For building a windows installer, you have to install `mono-complete`
