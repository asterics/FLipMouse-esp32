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
 * Small adaptions done by Benjamin Aigner (2018) <aignerb@technikum-wien.at>:
 * * Replaced strlen by strnlen avoiding security flaws
 * * Changed data sink to be used with the FLipMouse/FABI firmware
 */

#include "websocket.h"

#define WS_CLIENT_KEY_L		24		/**< \brief Length of the Client Key*/
#define SHA1_RES_L			20		/**< \brief SHA1 result*/
#define WS_STD_LEN			125		/**< \brief Maximum Length of standard length frames*/
#define WS_SPRINTF_ARG_L	4		/**< \brief Length of sprintf argument for string (%.*s)*/

/** \brief Opcode according to RFC 6455*/
typedef enum {
	WS_OP_CON = 0x0, 				/*!< Continuation Frame*/
	WS_OP_TXT = 0x1, 				/*!< Text Frame*/
	WS_OP_BIN = 0x2, 				/*!< Binary Frame*/
	WS_OP_CLS = 0x8, 				/*!< Connection Close Frame*/
	WS_OP_PIN = 0x9, 				/*!< Ping Frame*/
	WS_OP_PON = 0xa 				/*!< Pong Frame*/
} WS_OPCODES;

//Reference to open websocket connection
static struct netconn* WS_conn = NULL;

const char WS_sec_WS_keys[] = "Sec-WebSocket-Key:";
const char WS_sec_conKey[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
const char WS_srv_hs[] ="HTTP/1.1 101 Switching Protocols \r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %.*s\r\n\r\n";


esp_err_t WS_write_data(char* p_data, size_t length) {

	//check if we have an open connection
	if (WS_conn == NULL)
		return ERR_CONN;

	//currently only frames with a payload length <WS_STD_LEN are supported
	if (length > WS_STD_LEN)
		return ERR_VAL;

	//netconn_write result buffer
	err_t result;

	//prepare header
	WS_frame_header_t hdr;
	hdr.FIN = 0x1;
	hdr.payload_length = length;
	hdr.mask = 0;
	hdr.reserved = 0;
	hdr.opcode = WS_OP_TXT;

	//send header
	result = netconn_write(WS_conn, &hdr, sizeof(WS_frame_header_t), NETCONN_COPY);

	//check if header was send
	if (result != ERR_OK)
		return result;

	//send payload
	return netconn_write(WS_conn, p_data, length, NETCONN_COPY);
}


void ws_server_netconn_serve(struct netconn *conn) {

	//Netbuf
	struct netbuf *inbuf;

	//message buffer
	char *buf;

	//pointer to buffer (multi purpose)
	char* p_buf;

	//Pointer to SHA1 input
	char* p_SHA1_Inp;

	//Pointer to SHA1 result
	char* p_SHA1_result;

	//multi purpose number buffer
	uint16_t i;

	//will point to payload (send and receive
	char* p_payload;

	//Frame header pointer
	WS_frame_header_t* p_frame_hdr;

	//allocate memory for SHA1 input
	p_SHA1_Inp = malloc(WS_CLIENT_KEY_L + sizeof(WS_sec_conKey));

	//allocate memory for SHA1 result
	p_SHA1_result = malloc(SHA1_RES_L);

	//Check if malloc suceeded
	if ((p_SHA1_Inp != NULL) && (p_SHA1_result != NULL)) {

		//receive handshake request
		if (netconn_recv(conn, &inbuf) == ERR_OK) {

			//read buffer
			netbuf_data(inbuf, (void**) &buf, &i);

			//write static key into SHA1 Input
			for (i = 0; i < sizeof(WS_sec_conKey); i++)
				p_SHA1_Inp[i + WS_CLIENT_KEY_L] = WS_sec_conKey[i];

			//find Client Sec-WebSocket-Key:
			p_buf = strstr(buf, WS_sec_WS_keys);

			//check if needle "Sec-WebSocket-Key:" was found
			if (p_buf != NULL) {

				//get Client Key
				for (i = 0; i < WS_CLIENT_KEY_L; i++)
					p_SHA1_Inp[i] = *(p_buf + sizeof(WS_sec_WS_keys) + i);

				// calculate hash
                                esp_sha(SHA1, (unsigned char*) p_SHA1_Inp, strlen(p_SHA1_Inp),
						(unsigned char*) p_SHA1_result);

				//hex to base64
				p_buf = (char*) base64_encode((unsigned char*) p_SHA1_result,
						SHA1_RES_L, (size_t*) &i);

				//free SHA1 input
				free(p_SHA1_Inp);

				//free SHA1 result
				free(p_SHA1_result);

				//allocate memory for handshake
				p_payload = malloc(sizeof(WS_srv_hs) + i - WS_SPRINTF_ARG_L);

				//check if malloc suceeded
				if (p_payload != NULL) {

					//prepare handshake
					sprintf(p_payload, WS_srv_hs, i - 1, p_buf);

					//send handshake
                                        netconn_write(conn, p_payload, strlen(p_payload),
							NETCONN_COPY);

					//free base 64 encoded sec key
					free(p_buf);

					//free handshake memory
					free(p_payload);

					//set pointer to open WebSocket connection
					WS_conn = conn;

					//Wait for new data
					while (netconn_recv(conn, &inbuf) == ERR_OK) {

						//read data from inbuf
						netbuf_data(inbuf, (void**) &buf, &i);

						//get pointer to header
						p_frame_hdr = (WS_frame_header_t*) buf;

						//check if clients wants to close the connection
						if (p_frame_hdr->opcode == WS_OP_CLS)
							break;

						//get payload length
                                                uint64_t payloadLen = 0;
                                                uint8_t offset = 0;
                                                //determine payload length type.
                                                //according to RFC6455
                                                switch(p_frame_hdr->payload_length)
                                                {
                                                        ///@todo Is this network to host decoding of 16/64bits correct?!?
                                                        //next 2 bytes are used as 16bit payload length
                                                        case 126: 
                                                                payloadLen = ntohs(buf[sizeof(WS_frame_header_t)]);
                                                                offset = 2;
                                                        //next 4 bytes are used as 64bit payload length
                                                        case 127:
                                                                payloadLen = ntohl(buf[sizeof(WS_frame_header_t)]);
                                                                payloadLen += ntohl(buf[sizeof(WS_frame_header_t)+4]);
                                                                offset = 8;
                                                        //short frames 0-125 bytes
                                                        default: payloadLen = p_frame_hdr->payload_length; break;
                                                }
                                                
						if (p_frame_hdr->payload_length <= WS_STD_LEN) {

							//get beginning of mask or payload
							p_buf = (char*) &buf[sizeof(WS_frame_header_t)];

							//check if content is masked
							if (p_frame_hdr->mask) {

								//allocate memory for decoded message
                                                                p_payload = malloc(payloadLen + 1);

								//check if malloc succeeded
								if (p_payload != NULL) {

									//decode playload
									for (i = 0; i < payloadLen;
											i++)
										p_payload[i] = (p_buf + WS_MASK_L + offset)[i]
												^ p_buf[offset+(i % WS_MASK_L)];
												
                                                                        //add \0 terminator
									p_payload[payloadLen] = 0;
								}
							} else
								//content is not masked
								p_payload = p_buf+offset;

							//do stuff
							if ((p_payload != NULL)	&& (p_frame_hdr->opcode == WS_OP_TXT)) {

                                                                //save incoming buffer pointer & length to an AT command struct
                                                                atcmd_t incoming;
                                                                //payload will be freed in receiving task
                                                                incoming.buf = (uint8_t *)p_payload;
                                                                incoming.len = p_frame_hdr->payload_length;
                                                                
                                                                
                                                                
								//send message
                                                                ESP_LOGI("websocket","Sent incoming command: %s",p_payload);
								xQueueSendFromISR(halSerialATCmds,&incoming,0);
							}

						} //p_frame_hdr->payload_length<126

						//free input buffer
						netbuf_delete(inbuf);

					} //while(netconn_recv(conn, &inbuf)==ERR_OK)
				} //p_payload!=NULL
			} //check if needle "Sec-WebSocket-Key:" was found
		} //receive handshake
	} //p_SHA1_Inp!=NULL&p_SHA1_result!=NULL

	//release pointer to open WebSocket connection
	WS_conn = NULL;

	//delete buffer
	netbuf_delete(inbuf);

	// Close the connection
	netconn_close(conn);

	//Delete connection
	netconn_delete(conn);

} 
