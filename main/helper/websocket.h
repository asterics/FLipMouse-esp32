#ifndef _WEBSOCKET_H_
#define _WEBSOCKET_H_

/**
 * @section License
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2017, Thomas Barth, barth-dev.de
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
 * Small adaptions done by Benjamin Aigner (2018) <aignerb@technikum-wien.at>
 */

#include "esp_heap_caps.h"
//#include "hwcrypto/sha.h"
#include "mbedtls/sha1.h"
#include "mbedtls/base64.h"
#include "esp_system.h"
//#include "wpa2/utils/base64.h"
#include <string.h>
#include <stdlib.h>

#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_err.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"


#include "lwip/api.h"
#include "lwip/err.h"
#include "lwip/netdb.h"

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <esp_log.h>
//common definitions & data for all of these functional tasks
#include "common.h"
//command structs & queues are declared there
#include "hal_serial.h"
#include <inttypes.h>


#define WS_MASK_L		0x4		/**< \brief Length of MASK field in WebSocket Header*/


/** \brief Websocket frame header type*/
typedef struct {
	uint8_t opcode :WS_MASK_L;
	uint8_t reserved :3;
	uint8_t FIN :1;
	uint8_t payload_length :7;
	uint8_t mask :1;
} WS_frame_header_t;

/** \brief Websocket frame type*/
typedef struct{
	struct netconn* 	conenction;
	WS_frame_header_t	frame_header;
	size_t				payload_length;
	char*				payload;
}WebSocket_frame_t;



void ws_server_netconn_serve(struct netconn *conn);
esp_err_t WS_write_data(char* p_data, size_t length);

#endif  /*_WEBSOCKET_H_*/
