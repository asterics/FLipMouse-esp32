/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 * Copyright 2017 Benjamin Aigner <aignerb@technikum-wien.at,
 * beni@asterics-foundation.org>
 */
 
 /** @file
  * @brief Contains extra information/documentation for doxygen only*/
 
 /** @defgroup vbhandlers Virtual Button (VB) Handlers
 *
 * Due to the feature, that every VB can trigger different actions,
 * this firmware has implemented the action triggering itself in
 * the following way:<br>
 * * A triggered action will be sent to the debouncer (via the debouncer_in queue)
 * * The debouncer will start an esp_timer (has a much higher resolution than the FreeRTOS SW timers)
 * * After the timer has expired, an esp_event will be sent (event base: VB_EVENT; event id: vb_event_t)
 * * Each handler is registered to this event loop and will receive these events.
 * * If a handler has an active action for this triggered VB, it will be sent.
 * 
 * Currently, there are 2 different handlers implemented:
 * * handler_hid: handles all HID related actions (they are sent to the LPC chip via I2C)
 * * handler_vb: handles all other actions (infrared, house-keeping, slot switching,...)
 * 
 * @note Currently, we are using the system event loop. This might be changed to an extra event loop
 * 
 * @see handler_hid
 * @see handler_vb
 * @see vb_event_t
 * @see debouncer_in
 * @see VB_EVENT
 */
 
/** @defgroup cmdchain Activating a VB in handler_hid/handler_vb
 * 
 * All active VBs are handled via a chained list in each handler.
 * Via handler_<hid/vb>_addCmd, a new command will be added to this list.
 * 
 * On a slot switch, these lists are cleared (and the corresponding
 * memory will be freed).
 * 
 * @note Take care of setting the "clear" flag right. If you want to have
 * more than one action a single VB, the first call to _addCmd should have
 * the clear flag set, the other calls should not set this flag.
 * 
 * */

/*++++ All global todos, roadmap, ... ++++*/
/**
 * @file 
 * @todo Implement learning mode
*/
