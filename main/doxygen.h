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
 * Copyright 2017 Benjamin Aigner <aignerb@technikum-wien.at,
 * beni@asterics-foundation.org>
 * 
 * This file contains extra information & documentation for doxygen
 */
 
 /** @file
  * @brief Contains extra information/documentation for doxygen only*/
 
 /** @defgroup FunctionTasks
 *
 * All these listed functions are so called "FunctionTasks".
 * These are tasks, which are loaded and bound to a virtual button.
 * Each time either "press" or "release" flag of this bound virtual button
 * is set, a bound task is waking up and triggering different actions.
 * 
 * Each of these functional tasks has a parameter, which contains at least
 * the virtual button number.
 * 
 * If you want to trigger a single action without binding the task to a
 * virtual button, please pass the parameter as usual (depending on
 * the task and the configuration) and <b>set the virtualbutton to
 * VB_SINGLESHOT</b> to force the function to return after the action is
 * triggered.
 * 
 * It is not necessary to create these functions as tasks in single shot mode!
 * 
 * The debouncer task is taking care of debouncing flags from
 * virtualButtonsIn (set by input sensor) to virtualButtonsOut
 * (function tasks are pending on bits there).
 * 
 * Currently implemented function tasks which can be bound by the config
 * switcher:
 * 
 * @see task_keyboard
 * @see task_mouse
 * @see task_configswitcher
 * @see task_calibration
 * 
 * The loading/unloading is done by the config switcher task
 * @see configSwitcherTask
 *
 * Virtual Button number for single shot triggering:
 * @see VB_SINGLESHOT
 * 
 * @warning Do not create a task in single shot mode, just call the function
 * @warning Do not call the function if in FUNCTIONAL mode, the task function will block
 * @warning As long as the function or the task is active, the parameter pointer MUST be valid
 */

//All global todos, roadmap, ...
/**
 @todo Implement learning mode
 @todo Configswitcher NEXT increments slot number, even if not possible. Also check for turn-around. & missing slots.
 @todo Clarify AT KP/KR/KH commands on original FlipMouse, to be compatible... (especially for press/release with buttons & with multiple key identifiers)
 @todo Debouncer testing...
 @todo Re-implement hal_serial's UART data reception: use own interrupt & discard UART driver.
 @todo Maybe store VB configs not in individual files, is slow...
 @todo Problem with AT MX/MY: not always stepping right, but always if it is negativ
*/
