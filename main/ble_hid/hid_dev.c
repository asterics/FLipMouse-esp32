// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "hid_dev.h"
#include "keylayouts.h"
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

/** @brief mask for media keycodes to limit to 8bit */
#define M(x) (x&0xFF)


#ifdef LOG_TAG
    #undef LOG_TAG
    #define LOG_TAG "HID_DEV"
#endif

#ifdef __cplusplus
extern "C" {
#endif

static hid_report_map_t *hid_dev_rpt_tbl;
static uint8_t hid_dev_rpt_tbl_Len;

static hid_report_map_t *hid_dev_rpt_by_id(uint8_t id, uint8_t type)
{
    hid_report_map_t *rpt = hid_dev_rpt_tbl;

    for (uint8_t i = hid_dev_rpt_tbl_Len; i > 0; i--, rpt++) {
        if (rpt->id == id && rpt->type == type && rpt->mode == hidProtocolMode) {
            return rpt;
        }
    }

    return NULL;
}

void hid_dev_register_reports(uint8_t num_reports, hid_report_map_t *p_report)
{
    hid_dev_rpt_tbl = p_report;
    hid_dev_rpt_tbl_Len = num_reports;
    return;
}

void hid_dev_send_report(esp_gatt_if_t gatts_if, uint16_t conn_id,
                                    uint8_t id, uint8_t type, uint8_t length, uint8_t *data)
{
    hid_report_map_t *p_rpt;

    // get att handle for report
    if ((p_rpt = hid_dev_rpt_by_id(id, type)) != NULL) {
        // if notifications are enabled
        ESP_LOGE(LOG_TAG,"%s(), send the report, handle = %d", __func__, p_rpt->handle);
        esp_ble_gatts_send_indicate(gatts_if, conn_id, p_rpt->handle, length, data, false);
    }
    
    return;
}

void hid_consumer_build_report(uint8_t *buffer, consumer_cmd_t cmd)
{
    if (!buffer) {
        ESP_LOGE(LOG_TAG,"%s(), the buffer is NULL, hid build report failed.", __func__);
        return;
    }
    
    switch (cmd) {
        case M(KEY_MEDIA_CHANNEL_UP):
            HID_CC_RPT_SET_CHANNEL(buffer, HID_CC_RPT_CHANNEL_UP);
            break;

        case M(KEY_MEDIA_CHANNEL_DOWN):
            HID_CC_RPT_SET_CHANNEL(buffer, HID_CC_RPT_CHANNEL_DOWN);
            break;

        case M(KEY_MEDIA_VOLUME_INC):
            HID_CC_RPT_SET_VOLUME_UP(buffer);
            break;

        case M(KEY_MEDIA_VOLUME_DEC):
            HID_CC_RPT_SET_VOLUME_DOWN(buffer);
            break;

        case M(KEY_MEDIA_MUTE):
            HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_MUTE);
            break;

        case M(KEY_MEDIA_POWER):
            HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_POWER);
            break;

        case M(KEY_MEDIA_RECALL_LAST):
            HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_LAST);
            break;

        case M(KEY_MEDIA_ASSIGN_SEL):
            HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_ASSIGN_SEL);
            break;

        case M(KEY_MEDIA_PLAY):
            HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_PLAY);
            break;

        case M(KEY_MEDIA_PAUSE):
            HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_PAUSE);
            break;

        case M(KEY_MEDIA_RECORD):
            HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_RECORD);
            break;

        case M(KEY_MEDIA_FAST_FORWARD):
            HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_FAST_FWD);
            break;

        case M(KEY_MEDIA_REWIND):
            HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_REWIND);
            break;

        case M(KEY_MEDIA_NEXT_TRACK):
            HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_SCAN_NEXT_TRK);
            break;

        case M(KEY_MEDIA_PREV_TRACK):
            HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_SCAN_PREV_TRK);
            break;

        case M(KEY_MEDIA_STOP):
            HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_STOP);
            break;

        default:
            break;
    }

    return;
}

