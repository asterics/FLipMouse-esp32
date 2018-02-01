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
 * Copyright 2017 Benjamin Aigner <beni@asterics-foundation.org>
 * 
 * Heavily based on Paul Stoffregens usb_api.cpp from Teensyduino
 * http://www.pjrc.com/teensy/teensyduino.html
 * Copyright (c) 2008 PJRC.COM, LLC
 * THANK YOU VERY MUCH FOR THIS EFFORT ON KEYBOARD + LAYOUTS!
 * 
 */
 
#ifndef _KEYBOARD_H_
#define _KEYBOARD_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum keyboard_layouts{
  LAYOUT_US_ENGLISH,
  LAYOUT_US_INTERNATIONAL,
  LAYOUT_GERMAN,
  LAYOUT_GERMAN_MAC,
  LAYOUT_CANADIAN_FRENCH,
  LAYOUT_CANADIAN_MULTILINGUAL,
  LAYOUT_UNITED_KINGDOM,
  LAYOUT_FINNISH,
  LAYOUT_FRENCH,
  LAYOUT_DANISH,
  LAYOUT_NORWEGIAN,
  LAYOUT_SWEDISH,
  LAYOUT_SPANISH,
  LAYOUT_PORTUGUESE,
  LAYOUT_ITALIAN,
  LAYOUT_PORTUGUESE_BRAZILIAN,
  LAYOUT_FRENCH_BELGIAN,
  LAYOUT_GERMAN_SWISS,
  LAYOUT_FRENCH_SWISS,
  LAYOUT_SPANISH_LATIN_AMERICA,
  LAYOUT_IRISH,
  LAYOUT_ICELANDIC,
  LAYOUT_TURKISH,
  LAYOUT_CZECH,
  LAYOUT_SERBIAN_LATIN_ONLY,
  LAYOUT_MAX
};


/** @brief Parse a decoded code point to a keycode, step 2
 * 
 * This method parses a fully assembled code point to a
 * keycode. To get a fully assembled cpoint, use the method parse_for_keycode.
 * 
 * @see parse_for_keycode
 * @see keycodes_masks
 * @see keycodes_ascii
 * @see keycodes_iso_8859_1
 * @param cpoint Fully assembled ASCII or ISO8859-1 code point
 * @param locale Currently used keyboard layout
 * @return 0 if no keycode was found (invalid cpoint), the keycode otherwise
 */
uint16_t unicode_to_keycode(uint16_t cpoint, uint8_t locale);

/** @brief Mask the keycode to get the HID keycode, step 4
 * 
 * This method masks out all modifier bits and returns the direct
 * HID keycode, which can be used in HID reports.
 * 
 * @param keycode Keycode from other parsing methods
 * @return 8-bit keycode for HID
 **/
uint8_t keycode_to_key(uint16_t keycode);

/** @brief Mask the keycode to get the modifiers, step 5
 * 
 * This method masks out all keycode bits and returns the direct
 * HID modifier byte, which can be used in HID reports.
 * 
 * @param keycode Keycode from other parsing methods
 * @param locale Currently used keyboard layout
 * @return 8-bit keycode for HID
 **/
uint8_t keycode_to_modifier(uint16_t keycode, uint8_t locale);

/** @brief Is this keycode a modifier?
 * 
 * This method is used to determine if a keycode is a modifier key
 * (without any other keys)
 * @param keycode Keycode to be tested
 * @return 0 if a normal keycode, 1 if a modifier key
 * */
uint8_t keycode_is_modifier(uint16_t keycode);


/** @brief Parse a keycode for deadkey input, step 3
 * 
 * This method parses a keycode for a possible deadkey
 * sequence. If the parsed keycode needs a deadkey press, the 
 * corresponding keycode is returned. If no deadkey is required, 0 is returned.
 * To get a keycode, use the method unicode_to_keycode.
 * 
 * @see unicode_to_keycode
 * @see keycodes_masks
 * @param keycode Keycode which might need a deadkey pressed
 * @param locale Currently used keyboard layout
 * @return 0 if no deadkey needs to be pressed, the deadkey keycode otherwise
 */
uint16_t deadkey_to_keycode(uint16_t keycode, uint8_t locale);

/** @brief Parse a key identifier to a keycode
 * 
 * This method is used to parse a key identifier (e.g., KEY_A)
 * to a keycode which is used for a task_keyboard config.
 * 
 * @warning If you use key identifiers, no keyboard locale is taken into
 * account!
 * 
 * @param keyidentifier Key identifier string
 * @return Keycode if found, 0 otherwise
 * 
 * @see parseKeycodeToIdentifier
 * */
uint16_t parseIdentifierToKeycode(char* keyidentifier);

/** @brief Parse a keycode to a key identifier
 * 
 * This method is used to parse a key code to a key identifier which
 * can be used for sending back the task_keyboard config.
 * 
 * @warning If you use key identifiers, no keyboard locale is taken into
 * account!
 * 
 * @param keycode Keycode to be parsed to a key identifier
 * @param buffer Char buffer where the key identifier is saved to
 * @param buf_len Length of buffer
 * @return 1 if found, 0 otherwise
 * 
 * @see parseKeycodeToIdentifier
 * */
uint16_t parseKeycodeToIdentifier(uint16_t keycode, char* buffer, uint8_t buf_len);

/** parse an incoming byte for a keycode
 * 
 * This method parses one incoming byte for the given locale.
 * It returns 0 if there is no keycode or another byte is needed (Unicode input).
 * If a modifier is needed the given modifier byte is updated.
 * 
 * @see keyboard_layouts
 * @return 0 if another byte is needed or no keycode is found; the keycode otherwise
 * 
 * */
uint8_t parse_for_keycode(uint8_t inputdata, uint8_t locale, uint8_t *keycode_modifier, uint8_t *deadkey_first_keycode);

/** remove a keycode from the given HID keycode array.
 * 
 * @note The size of the keycode_arr parameter MUST be 6
 * @return 0 if the keycode was removed, 1 if the keycode was not in the array
 * */
uint8_t remove_keycode(uint8_t keycode,uint8_t *keycode_arr);

/** add a keycode to the given HID keycode array.
 * 
 * @note The size of the keycode_arr parameter MUST be 6
 * @return 0 if the keycode was added, 1 if the keycode was already in the array, 2 if there was no space
 * */
uint8_t add_keycode(uint8_t keycode,uint8_t *keycode_arr);


/** get a keycode for the given UTF codepoint
 * 
 * This method parses a 16bit unicode character to a keycode.
 * If a modifier is needed the given modifier byte is updated.
 * 
 * @see keyboard_layouts
 * @return 0 if no keycode is found; the keycode otherwise
 * */
uint8_t get_keycode(uint16_t cpoint,uint8_t locale,uint8_t *keycode_modifier, uint8_t *deadkey_first_keystroke);


/** translate Unicode characters between different locales
 * 
 * This method translates a 16bit Unicode cpoint from one locale to another one.
 * 
 * @see keyboard_layouts
 * @return 0 if no cpoint is found; the cpoint otherwise
 * */
uint16_t get_cpoint(uint16_t cpoint,uint8_t locale_src,uint8_t locale_dst);

/** getting the HID country code for a given locale
 * 
 * @see keyboard_layouts
 * @return bCountryCode value for HID info
 * @param locale Locale number, as defined in keyboard_layouts
 **/
uint8_t get_hid_country_code(uint8_t locale);

#endif
 
