# How configuration is handled

A FABI/FLipmouse device is able to switch between different configurations on the fly.
This config switch can be triggered by a serial command (via AT commands) or by a configurable user-action.

Normally, configuration consists of following parts:

* A set of different, loadable, slots (1-250 are possible). Slots can be named and loaded by name. A typical basic configuration uses 2 slots, the first one for mouse control. Mouse movement is
done by the mouthpiece (FLipMouse) or the first four buttons (FABI). Clicking can be done by other buttons (FABI) or sip and puff (FLipMouse). One button is reserved for slot switch. The second slot is used to trigger keyboard arrow keys by the first four buttons (FABI) or the mouthpiece (FLipMouse).
* General configs for each slot (sensitivity, mouthpiece function, orientation, ...)
* Functions for each possible action (called virtual button). There is a set of possible functions available, as it is described in AT command API.

Note: a normal VB settting is done via a combination of "AT BM xx" and an additional action e.g., "AT CA". The AT BM command tells the firmware, that
the next command will be assigned to this VB.

Note: in memory, the config is saved in a text file, with AT commands. This way, we can do upgrades without worrying about the config file format.

![Configuration organization](slots.png)


