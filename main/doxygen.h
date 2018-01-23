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
 
 /** @defgroup FunctionTasks
 *
 * All these listed functions are so called "FunctionTasks".
 * These are tasks, which are loaded and bound to a virtual button.
 * Each time either "press" or "release" flag of this bound virtual button
 * is set, a bound task is waking up and triggering different actions.
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
 */

 
