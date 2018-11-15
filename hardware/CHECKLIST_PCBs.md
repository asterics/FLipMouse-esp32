## Schematic

* Run DRC
* Approve undriven nets
* OSHW logo?
* Check valid pinning (especially for ESP32)
* Revision correct?
* Readable? Use text for annotation and better reading
* Re-annotate, if parts changed (change name in Schematic, import netlist via timestamp in PCB!)
* Export BOM & check for changes (CSV->ODS); print out for QM
* Export PDF of Schematic, print out for QM

## PCB

* When importing netlist, check if any connections have to be done manually
* Silkscreen position
* Fiducials
* DRC!
* Text over vias?
* Logos (CE,WEEE,FABI/FLipMouse)
* Correctly annotated sheet (version, date, product number)
* Product number
* Numbers for partly manual assembly
* Check USB lines (symmetric, GND plane underneath, no sharp corners)
* Print out for QM, save files: one combined PDF for TOP, one for BOT (include: Cu,Silks,CrtYd&EdgeCuts layers); one PDF containing layers on single pages (F/BCu; F/BSilks; F/BCrtYd; EdgeCuts)

## General

* Does this change have side-effects on other PCB/firmware (e.g. FLipMouse: adding SDA/SCL to LPC, if we use these communication in firmware, it has to be adopted to FABI as well)
* Double check pin assignment, especially for ESP32 (some input only pins!)
