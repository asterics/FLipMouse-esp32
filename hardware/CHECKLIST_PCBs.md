## Schematic

* Run DRC
* Approve undriven nets
* OSHW logo?
* Check valid pinning (especially for ESP32)
* Revision correct?
* Readable? Use text for annotation and better reading
* Re-annotate, if parts changed (change name in Schematic, import netlist via timestamp in PCB!)
* Export BOM & check for changes (CSV->ODS)
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
* Print out for QM

## General

* Does this change have side-effects on other PCB/firmware (e.g. FLipMouse: adding SDA/SCL to LPC, if we use these communication in firmware, it has to be adopted to FABI as well)
