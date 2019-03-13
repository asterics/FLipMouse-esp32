#ifndef _TONES_H_
#define _TONES_H_
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
  * @brief Contains different tone height & duration definitions
  * @see config_switcher.c
  * @see hal_adc.c
  * @see task_infrared.c
  * @see halIOBuzzerQueue
  * @see TONE
  * */

/*++++ in config_switcher, config related ++++*/

/** @brief Base frequency for slot, TONE_CHANGESLOT_FREQ_SLOTNR * slotnr will be added 
 * @see config_switcher.c */
#define TONE_CHANGESLOT_FREQ_BASE           2000
/** @brief Add this amount of frequency for each slot number
 * @see config_switcher.c */
#define TONE_CHANGESLOT_FREQ_SLOTNR         200
/** @brief Duration of tone
 * @see config_switcher.c */
#define TONE_CHANGESLOT_DURATION            150
/** @brief Duration of pause
 * @see config_switcher.c */
#define TONE_CHANGESLOT_DURATION_PAUSE      50

/*++++ in halADC, sip&puff/calib related ++++*/
/** @brief Frequency of calibrate tone
 * @see hal_adc.c */
#define TONE_CALIB_FREQ                     200
/** @brief Duration of calibrate tone
 * @see hal_adc.c */
#define TONE_CALIB_DURATION                 400

/** @brief Frequency of sip tone
 * @see hal_adc.c */
#define TONE_SIP_FREQ                       200
/** @brief Duration of sip tone
 * @see hal_adc.c */
#define TONE_SIP_DURATION                   40

/** @brief Frequency of puff tone
 * @see hal_adc.c */
#define TONE_PUFF_FREQ                      300
/** @brief Duration of puff tone
 * @see hal_adc.c */
#define TONE_PUFF_DURATION                  40


/** @brief Frequency of enter strongsip mode tone
 * @see hal_adc.c */
#define TONE_STRONGSIP_ENTER_FREQ           300
/** @brief Duration of enter strongsip mode tone
 * @see hal_adc.c */
#define TONE_STRONGSIP_ENTER_DURATION       200

/** @brief Frequency of enter strongpuff mode tone
 * @see hal_adc.c */
#define TONE_STRONGPUFF_ENTER_FREQ          400
/** @brief Duration of enter strongpuff mode tone
 * @see hal_adc.c */
#define TONE_STRONGPUFF_ENTER_DURATION      200

/** @brief Frequency of exit strongsip mode tone
 * @see hal_adc.c */
#define TONE_STRONGSIP_EXIT_FREQ           300
/** @brief Duration of exit strongsip mode tone
 * @see hal_adc.c */
#define TONE_STRONGSIP_EXIT_DURATION       100

/** @brief Frequency of exit strongpuff mode tone
 * @see hal_adc.c */
#define TONE_STRONGPUFF_EXIT_FREQ          400
/** @brief Duration of exit strongpuff mode tone
 * @see hal_adc.c */
#define TONE_STRONGPUFF_EXIT_DURATION      100


/*++++ in task_infrared, IR cmds related ++++*/

/** @brief Frequency of receiving IR tone
 * @see task_infrared.c */
#define TONE_IR_RECV_FREQ                   800
/** @brief Duration of receiving IR tone
 * @see task_infrared.c */
#define TONE_IR_RECV_DURATION               400

/** @brief Frequency of sending IR tone
 * @see task_infrared.c */
#define TONE_IR_SEND_FREQ                   800
/** @brief Duration of sending IR tone
 * @see task_infrared.c */
#define TONE_IR_SEND_DURATION               400

#endif

