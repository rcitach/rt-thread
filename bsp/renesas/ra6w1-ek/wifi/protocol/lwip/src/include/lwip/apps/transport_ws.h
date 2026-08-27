/**
 * Copyright 2015-2018 Espressif Systems (Shanghai) PTE LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Copyright (c) 2021-2022 Modified by Renesas Electronics.
 *
**/

#ifndef _WS_TRANSPORT_WS_H_
#define _WS_TRANSPORT_WS_H_

#include "transport.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ws_transport_opcodes {
    WS_TRANSPORT_OPCODES_CONT =  0x00,
    WS_TRANSPORT_OPCODES_TEXT =  0x01,
    WS_TRANSPORT_OPCODES_BINARY = 0x02,
    WS_TRANSPORT_OPCODES_CLOSE = 0x08,
    WS_TRANSPORT_OPCODES_PING = 0x09,
    WS_TRANSPORT_OPCODES_PONG = 0x0a,
    WS_TRANSPORT_OPCODES_FIN = 0x80,
    WS_TRANSPORT_OPCODES_NONE = 0x100,   /*!< not a valid opcode to indicate no message previously received
                                          * from the API ws_transport_ws_get_read_opcode() */
} ws_transport_opcodes_t;

/**
 * @brief      Create web socket transport
 *
 * @return
 *  - transport
 *  - NULL
 */
ws_transport_handle_t ws_transport_ws_init(ws_transport_handle_t parent_handle);

/**
 * @brief       Set HTTP path to update protocol to websocket
 *
 * @param t     websocket transport handle
 * @param path  The HTTP Path
 */
void ws_transport_ws_set_path(ws_transport_handle_t t, const char *path);

/**
 * @brief               Set websocket sub protocol header
 *
 * @param t             websocket transport handle
 * @param sub_protocol  Sub protocol string
 *
 * @return
 *      - WS_OK on success
 *      - One of the error codes
 */
ws_err_t ws_transport_ws_set_subprotocol(ws_transport_handle_t t, const char *sub_protocol);

/**
 * @brief               Set websocket user-agent header
 *
 * @param t             websocket transport handle
 * @param sub_protocol  user-agent string
 *
 * @return
 *      - WS_OK on success
 *      - One of the error codes
 */
ws_err_t ws_transport_ws_set_user_agent(ws_transport_handle_t t, const char *user_agent);

/**
 * @brief               Set websocket additional headers
 *
 * @param t             websocket transport handle
 * @param sub_protocol  additional header strings each terminated with \r\n
 *
 * @return
 *      - WS_OK on success
 *      - One of the error codes
 */
ws_err_t ws_transport_ws_set_headers(ws_transport_handle_t t, const char *headers);

/**
 * @brief               Sends websocket raw message with custom opcode and payload
 *
 * Note that generic ws_transport_write for ws handle sends
 * binary massages by default if size is > 0 and
 * ping message if message size is set to 0.
 * This API is provided to support explicit messages with arbitrary opcode,
 * should it be PING, PONG or TEXT message with arbitrary data.
 *
 * @param[in]  t           Websocket transport handle
 * @param[in]  opcode      ws operation code
 * @param[in]  buffer      The buffer
 * @param[in]  len         The length
 * @param[in]  timeout_ms  The timeout milliseconds (-1 indicates block forever)
 *
 * @return
 *  - Number of bytes was written
 *  - (-1) if there are any errors, should check errno
 */
int ws_transport_ws_send_raw(ws_transport_handle_t t, ws_transport_opcodes_t opcode, const char *b, int len, int timeout_ms);

/**
 * @brief               Returns websocket op-code for last received data
 *
 * @param t             websocket transport handle
 *
 * @return
 *      - Received op-code as enum
 */
ws_transport_opcodes_t ws_transport_ws_get_read_opcode(ws_transport_handle_t t);

/**
 * @brief               Returns payload length of the last received data
 *
 * @param t             websocket transport handle
 *
 * @return
 *      - Number of bytes in the payload
 */
int ws_transport_ws_get_read_payload_len(ws_transport_handle_t t);

/**
 * @brief               Polls the active connection for termination
 *
 * This API is typically used by the client to wait for clean connection closure
 * by websocket server
 *
 * @param t             Websocket transport handle
 * @param[in] timeout_ms The timeout milliseconds
 *
 * @return
 *      0 - no activity on read and error socket descriptor within timeout
 *      1 - Success: either connection terminated by FIN or the most common RST err codes
 *      -1 - Failure: Unexpected error code or socket is normally readable
 */
int ws_transport_ws_poll_connection_closed(ws_transport_handle_t t, int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* _WS_TRANSPORT_WS_H_ */
