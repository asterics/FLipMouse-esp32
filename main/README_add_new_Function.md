# Howto add a new function

If you want to add a new function to the FLipMouse/FABI, you have to take care of following steps:

## Implement your function in handler_hid/handler_vb

Please use the corresponding handler. handler_hid for anything related to HID, which will be sent via I2C to the LPC USB chip.
handler_vb should be responsible for any other actions. If you want to implement a handler on your own, please take care of
adding your handler to the command parser.

If you want to add your function to the existing handler, add your type of function to vb_cmd_type_t (handler_vb) and in the
handler_vb function.

## Add command to command parser

The serial command parsing from the host's commands (based on an AT command set) is done in task_commands.c.
If you want to add a new function, add it to the onecmd_t commands[] struct.

In this struct, you can define a field in the generalConfig_t array, which will be modified by the command parser.
You don't have to declare anything further.

The second possibility is a command handler function, which you can declare in this struct. If your
command is recognized and the parameter check passes, your handler will be called.
