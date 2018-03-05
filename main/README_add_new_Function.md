# Howto add a new function

If you want to add a new function to the FLipMouse as an independent functional task, you have to take care of following steps:

## Implement a function task

Create a file in main/function_tasks with a "task_" prefix, with at least following methods:

* task_<name> A task with your functionality. Please note that each functional task should take care of a virtual button set to VB_SINGLESHOT. In this case
the task is NOT allowed to block the caller, because there should be triggered an action just-in-time without any triggered virtual buttons.
* task_<name>_getAT This method is used to reverse parse an AT command to an original string, which can be sent back to the host for showing the current configuration.

The parameters for your function task need to have at least an uint8_t field for the virtual button number.

## Add a new command type and config

If necessary add your new type of function to the command_type_t enum. This enum is used to group functions. For example, all mouse related commands and actions are covered by "T_MOUSE".
In addition, if you want to store config settings, add it to the generalConfig_t struct. Everything in this struct is stored as general settings for this slot. The current config can
be obtained by configGetCurrent() (include config_switcher.h for this).

## Add files to config switcher

For a task to be loaded on a config switch, it needs to be known to the config_switcher.c file, which is used to reload a config by attaching tasks to virtual buttons (if and only if
the virtual button is NOT VB_SINGLESHOT):

* Add your header file to config_switcher.h
* In configSwitcherTask, add your command type to switch which is used to determine the parameters to be allocated on a task switch (around line 300-360).


## Add command to command parser

The serial command parsing from the host's commands (based on an AT command set) is done in task_commands.c.
Command parsing is split into smaller portions grouped into functions, named do<name>Parsing().

Either you place your new command into one of the existing methods OR you create a new method and add it to the task_commands, which calls each parser.

Do following steps to enable parsing of your new command:

* Add a new parameter memory allocate to the beginning of task_commands. In addition, add a check if the pointer is valid (directly below)!
* Add your do<Name>Parsing method to the list of all parsers, around line 1400.
* Add your reverse parser to the method printAllSlots.

A few additional infos:

* Each do<Name>Parsing method needs to return a value of type parserstate_t. Please consult the source documentation for information which value to return.
* Don't forget to clear the parameter memory before doing anything with it.
* To detect a command, you can use the macros CMD (which compares a full AT command like "AT RO") or CMD4 (used to compare 4 characters like "AT M").
