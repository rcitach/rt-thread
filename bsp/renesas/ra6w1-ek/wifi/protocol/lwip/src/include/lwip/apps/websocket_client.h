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
 * Copyright (c) 2021-2022 Modified by Renesas Electronics
 *
**/
#ifndef _WEBSOCKET_CLIENT_H_
#define _WEBSOCKET_CLIENT_H_

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "FreeRTOS.h"
#if (CFG_WIFI == 1)
#include "custom_config_sdk.h"
#include "sys_app_defs.h"
#endif

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "event_groups.h"

#include "transport.h"
#include "transport_tcp.h"
#include "transport_ssl.h"
#include "transport_ws.h"
#include "ws_err.h"
#include "ws_log.h"
#include "ws_mem.h"
#include "ws_bit_defs.h"
#include "http_parser.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WEBSOCKET_PARAM_NAME					"WEBSOCKET_PARAM"

#define WS_CLIENT_MEM_CHECK(TAG, a, action) if (!(a)) {                                         \
        WS_LOGE(TAG,"%s:%d (%s): %s", __FILE__, __LINE__, __FUNCTION__, "Memory exhausted\n");       \
        action;                                                                                     \
        }

#define WS_CLIENT_STATE_CHECK(TAG, a, action) if ((a->state) < WEBSOCKET_STATE_INIT) {           \
        WS_LOGE(TAG,"%s(%d): %s", __FUNCTION__, __LINE__, "Websocket already stop"); \
        action;                                                                                      \
        }

#define WEBSOCKET_TCP_DEFAULT_PORT      (80)
#define WEBSOCKET_SSL_DEFAULT_PORT      (443)
#define WEBSOCKET_BUFFER_SIZE_BYTE      (4*1024)	//RX/TX Buffer 4K
#define WEBSOCKET_TLS_BUFFER_SIZE_BYTE  (8*1024)	//Crypto in/out buffer for TLS, at least 5K for authentication
#define WEBSOCKET_RECONNECT_TIMEOUT_MS  (10*1000)	//10 sec
#define WEBSOCKET_TASK_PRIORITY         (OS_TASK_PRIORITY_USER+5)	//6
#define WEBSOCKET_TASK_STACK            (1*1024)	//1K WORD	//4Kb
#define WEBSOCKET_NETWORK_TIMEOUT_MS    (10*1000)
#define WEBSOCKET_PING_INTERVAL_SEC     (10)
#define WEBSOCKET_EVENT_QUEUE_SIZE      (1)
#define WEBSOCKET_PINGPONG_TIMEOUT_SEC  (120)
#define WEBSOCKET_KEEP_ALIVE_IDLE       (5)
#define WEBSOCKET_KEEP_ALIVE_INTERVAL   (5)
#define WEBSOCKET_KEEP_ALIVE_COUNT      (3)
#define WEBSOCKET_HOST_MAX_LEN			(128)
#define WEBSOCKET_PINGPONG_TIMEOUT_MAX  (7200)
#define WEBSOCKET_PING_INTERVAL_MAX     (7200)


static const int STOPPED_BIT = BIT0;
static const int CLOSE_FRAME_SENT_BIT = BIT1;   // Indicates that a close frame was sent by the client
                                        // and we are waiting for the server to continue with clean close

typedef enum {
    WEBSOCKET_STATE_ERROR = -1,
    WEBSOCKET_STATE_UNKNOW = 0,
    WEBSOCKET_STATE_INIT,
    WEBSOCKET_STATE_CONNECTED,
    WEBSOCKET_STATE_WAIT_TIMEOUT,
    WEBSOCKET_STATE_CLOSING,
} websocket_client_state_t;

typedef struct websocketParam
{
    char    uri[WEBSOCKET_HOST_MAX_LEN + 1];
	int     status;
} websocketParamForRtm;

typedef struct {
    int                         task_stack;
    int                         task_prio;
    char                        *uri;
    char                        *host;
    char                        *path;
    char                        *scheme;
    char                        *username;
    char                        *password;
    int                         port;
    bool                        auto_reconnect;
    void                        *user_context;
    int                         network_timeout_ms;
    char                        *subprotocol;
    char                        *user_agent;
    char                        *headers;
    uint64_t                    pingpong_timeout_sec;
    size_t                      ping_interval_sec;
} websocket_config_storage_t;

struct websocket_client {
    TaskHandle_t                task_handle;
    ws_transport_list_handle_t transport_list;
    ws_transport_handle_t      transport;
    websocket_config_storage_t *config;
    websocket_client_state_t    state;
    uint64_t                    keepalive_tick_ms;
    uint64_t                    reconnect_tick_ms;
    uint64_t                    ping_tick_ms;
    uint64_t                    pingpong_tick_ms;
    uint64_t                    wait_timeout_ms;
    int                         auto_reconnect;
    bool                        run;
    bool                        wait_for_pong_resp;
    EventGroupHandle_t          status_bits;
    xSemaphoreHandle            lock;
    char                        *rx_buffer;
    char                        *tx_buffer;
    int                         buffer_size;
    ws_transport_opcodes_t      last_opcode;
    int                         payload_len;
    int                         payload_offset;
    ws_transport_keep_alive_t  keep_alive_cfg;
    struct ifreq                *if_name;
    int							err;
};

typedef struct websocket_client *websocket_client_handle_t;

/**
 * @brief Websocket Client events id
 */
typedef enum {
    WEBSOCKET_CLIENT_EVENT_ANY = -1,
    WEBSOCKET_CLIENT_EVENT_ERROR = 0,      /*!< This event occurs when there are any errors during execution */
    WEBSOCKET_CLIENT_EVENT_CONNECTED,      /*!< Once the Websocket has been connected to the server, no data exchange has been performed */
    WEBSOCKET_CLIENT_EVENT_DISCONNECTED,   /*!< The connection has been disconnected */
    WEBSOCKET_CLIENT_EVENT_DATA,           /*!< When receiving data from the server, possibly multiple portions of the packet */
    WEBSOCKET_CLIENT_EVENT_CLOSED,         /*!< The connection has been closed cleanly */
    WEBSOCKET_CLIENT_EVENT_FAIL_TO_CONNECT,
    WEBSOCKET_CLIENT_EVENT_ADD_HEADER,
    WEBSOCKET_CLIENT_EVENT_MAX
} websocket_client_event_id_t;


/**
 * @brief Websocket event data
 */
typedef struct {
    const char *data_ptr;                   /*!< Data pointer */
    int data_len;                           /*!< Data length */
    uint8_t op_code;                        /*!< Received opcode */
    websocket_client_handle_t client;   /*!< ws_websocket_client_handle_t context */
    void *user_context;                     /*!< user_data context, from ws_websocket_client_config_t user_data */
    int payload_len;                        /*!< Total payload length, payloads exceeding buffer will be posted through multiple events */
    int payload_offset;                     /*!< Actual offset for the data associated with this event */
} websocket_client_event_data_t;

/**
 * @brief Websocket Client transport
 */
typedef enum {
    WEBSOCKET_CLIENT_TRANSPORT_UNKNOWN = 0x0,  /*!< Transport unknown */
    WEBSOCKET_CLIENT_TRANSPORT_OVER_TCP,       /*!< Transport over tcp */
    WEBSOCKET_CLIENT_TRANSPORT_OVER_SSL,       /*!< Transport over ssl */
} websocket_client_transport_t;

/**
 * @brief Websocket client setup configuration
 */
typedef struct {
    const char                  *uri;                       /*!< Websocket URI, the information on the URI can be overrides the other fields below, if any */
    const char                  *host;                      /*!< Domain or IP as string */
    int                         port;                       /*!< Port to connect, default depend on ws_websocket_transport_t (80 or 443) */
    const char                  *username;                  /*!< Using for Http authentication - Not supported for now */
    const char                  *password;                  /*!< Using for Http authentication - Not supported for now */
    const char                  *path;                      /*!< HTTP Path, if not set, default is `/` */
    bool                        disable_auto_reconnect;     /*!< Disable the automatic reconnect function when disconnected */
    void                        *user_context;              /*!< HTTP user data context */
    int                         task_prio;                  /*!< Websocket task priority */
    int                         task_stack;                 /*!< Websocket task stack */
    int                         buffer_size;                /*!< Websocket buffer size */
    const char                  *cert_pem;                  /*!< SSL Certification, PEM format as string, if the client requires to verify server */
    websocket_client_transport_t   transport;                  /*!< Websocket transport type, see `ws_websocket_transport_t */
    const char                  *subprotocol;               /*!< Websocket subprotocol */
    const char                  *user_agent;                /*!< Websocket user-agent */
    const char                  *headers;                   /*!< Websocket additional headers */
    int                         pingpong_timeout_sec;       /*!< Period before connection is aborted due to no PONGs received */
    bool                        disable_pingpong_discon;    /*!< Disable auto-disconnect due to no PONG received within pingpong_timeout_sec */

    bool                        keep_alive_enable;          /*!< Enable keep-alive timeout */
    int                         keep_alive_idle;            /*!< Keep-alive idle time. Default is 5 (second) */
    int                         keep_alive_interval;        /*!< Keep-alive interval time. Default is 5 (second) */
    int                         keep_alive_count;           /*!< Keep-alive packet retry send count. Default is 3 counts */
    size_t                      ping_interval_sec;          /*!< Websocket ping interval, defaults to 10 seconds if not set */
    struct ifreq                *if_name;                   /*!< The name of interface for data to go through. Use the default interface without setting */
} websocket_client_config_t;


/** enum ws_close_status - RFC6455 close status codes */
enum ws_close_status {
	WS_CLOSE_STATUS_NOSTATUS				=    0,
	WS_CLOSE_STATUS_NORMAL					= 1000,
	/**< 1000 indicates a normal closure, meaning that the purpose for
      which the connection was established has been fulfilled. */
	WS_CLOSE_STATUS_GOINGAWAY				= 1001,
	/**< 1001 indicates that an endpoint is "going away", such as a server
      going down or a browser having navigated away from a page. */
	WS_CLOSE_STATUS_PROTOCOL_ERR				= 1002,
	/**< 1002 indicates that an endpoint is terminating the connection due
      to a protocol error. */
	WS_CLOSE_STATUS_UNACCEPTABLE_OPCODE			= 1003,
	/**< 1003 indicates that an endpoint is terminating the connection
      because it has received a type of data it cannot accept (e.g., an
      endpoint that understands only text data MAY send this if it
      receives a binary message). */
	WS_CLOSE_STATUS_RESERVED				= 1004,
	/**< Reserved.  The specific meaning might be defined in the future. */
	WS_CLOSE_STATUS_NO_STATUS				= 1005,
	/**< 1005 is a reserved value and MUST NOT be set as a status code in a
      Close control frame by an endpoint.  It is designated for use in
      applications expecting a status code to indicate that no status
      code was actually present. */
	WS_CLOSE_STATUS_ABNORMAL_CLOSE				= 1006,
	/**< 1006 is a reserved value and MUST NOT be set as a status code in a
      Close control frame by an endpoint.  It is designated for use in
      applications expecting a status code to indicate that the
      connection was closed abnormally, e.g., without sending or
      receiving a Close control frame. */
	WS_CLOSE_STATUS_INVALID_PAYLOAD			= 1007,
	/**< 1007 indicates that an endpoint is terminating the connection
      because it has received data within a message that was not
      consistent with the type of the message (e.g., non-UTF-8 [RFC3629]
      data within a text message). */
	WS_CLOSE_STATUS_POLICY_VIOLATION			= 1008,
	/**< 1008 indicates that an endpoint is terminating the connection
      because it has received a message that violates its policy.  This
      is a generic status code that can be returned when there is no
      other more suitable status code (e.g., 1003 or 1009) or if there
      is a need to hide specific details about the policy. */
	WS_CLOSE_STATUS_MESSAGE_TOO_LARGE			= 1009,
	/**< 1009 indicates that an endpoint is terminating the connection
      because it has received a message that is too big for it to
      process. */
	WS_CLOSE_STATUS_EXTENSION_REQUIRED			= 1010,
	/**< 1010 indicates that an endpoint (client) is terminating the
      connection because it has expected the server to negotiate one or
      more extension, but the server didn't return them in the response
      message of the WebSocket handshake.  The list of extensions that
      are needed SHOULD appear in the /reason/ part of the Close frame.
      Note that this status code is not used by the server, because it
      can fail the WebSocket handshake instead */
	WS_CLOSE_STATUS_UNEXPECTED_CONDITION			= 1011,
	/**< 1011 indicates that a server is terminating the connection because
      it encountered an unexpected condition that prevented it from
      fulfilling the request. */
	WS_CLOSE_STATUS_TLS_FAILURE				= 1015,
	/**< 1015 is a reserved value and MUST NOT be set as a status code in a
      Close control frame by an endpoint.  It is designated for use in
      applications expecting a status code to indicate that the
      connection was closed due to a failure to perform a TLS handshake
      (e.g., the server certificate can't be verified). */

	WS_CLOSE_STATUS_CLIENT_TRANSACTION_DONE		= 2000,

	/****** add new things just above ---^ ******/

	WS_CLOSE_STATUS_NOSTATUS_CONTEXT_DESTROY		= 9999,
};


typedef void (*websocket_client_event_callback)(websocket_client_event_id_t, websocket_client_event_data_t *);

/**
 * @brief      Start a Websocket session
 *             This function must be the first function to call,
 *             and it returns a websocket_client_handle_t that you must use as input to other functions in the interface.
 *             This call MUST have a corresponding call to websocket_client_destroy when the operation is complete.
 *
 * @param[in]  config  The configuration
 *
 * @return
 *     - `websocket_client_handle_t`
 *     - NULL if any errors
 */
websocket_client_handle_t websocket_client_init(const websocket_client_config_t *config);

/**
 * @brief      Set URL for client, when performing this behavior, the options in the URL will replace the old ones
 *             Must stop the WebSocket client before set URI if the client has been connected
 *
 * @param[in]  client  The client
 * @param[in]  uri     The uri
 *
 * @return     ws_err_t
 */
ws_err_t websocket_client_set_uri(websocket_client_handle_t client, const char *uri);

/**
 * @brief      Open the WebSocket connection
 *
 * @param[in]  client  The client
 *
 * @return     ws_err_t
 */
ws_err_t websocket_client_start(websocket_client_handle_t client, websocket_client_event_callback callback);

/**
 * @brief      Stops the WebSocket connection without websocket closing handshake
 *
 * This API stops ws client and closes TCP connection directly without sending
 * close frames. It is a good practice to close the connection in a clean way
 * using ws_websocket_client_close().
 *
 *  Notes:
 *  - Cannot be called from the websocket event handler 
 *
 * @param[in]  client  The client
 *
 * @return     ws_err_t
 */
ws_err_t websocket_client_stop(websocket_client_handle_t client);

/**
 * @brief      Destroy the WebSocket connection and free all resources.
 *             This function must be the last function to call for an session.
 *             It is the opposite of the ws_websocket_client_init function and must be called with the same handle as input that a ws_websocket_client_init call returned.
 *             This might close all connections this handle has used.
 * 
 *  Notes:
 *  - Cannot be called from the websocket event handler
 * 
 * @param[in]  client  The client
 *
 * @return     ws_err_t
 */
ws_err_t websocket_client_destroy(websocket_client_handle_t client);

/**
 * @brief      Generic write data to the WebSocket connection; defaults to binary send
 *
 * @param[in]  client  The client
 * @param[in]  data    The data
 * @param[in]  len     The length
 * @param[in]  timeout Write data timeout in RTOS ticks
 *
 * @return
 *     - Number of data was sent
 *     - (-1) if any errors
 */
int websocket_client_send(websocket_client_handle_t client, const char *data, int len, TickType_t timeout);

/**
 * @brief      Write binary data to the WebSocket connection (data send with WS OPCODE=02, i.e. binary)
 *
 * @param[in]  client  The client
 * @param[in]  data    The data
 * @param[in]  len     The length
 * @param[in]  timeout Write data timeout in RTOS ticks
 *
 * @return
 *     - Number of data was sent
 *     - (-1) if any errors
 */
int websocket_client_send_bin(websocket_client_handle_t client, const char *data, int len, TickType_t timeout);

/**
 * @brief      Write textual data to the WebSocket connection (data send with WS OPCODE=01, i.e. text)
 *
 * @param[in]  client  The client
 * @param[in]  data    The data
 * @param[in]  len     The length
 * @param[in]  timeout Write data timeout in RTOS ticks
 *
 * @return
 *     - Number of data was sent
 *     - (-1) if any errors
 */
int websocket_client_send_text(websocket_client_handle_t client, const char *data, int len, TickType_t timeout);

/**
 * @brief      Close the WebSocket connection in a clean way
 *
 * Sequence of clean close initiated by client:
 * * Client sends CLOSE frame
 * * Client waits until server echos the CLOSE frame
 * * Client waits until server closes the connection
 * * Client is stopped the same way as by the `ws_websocket_client_stop()`
 *
 *  Notes:
 *  - Cannot be called from the websocket event handler
 *
 * @param[in]  client  The client
 * @param[in]  timeout Timeout in RTOS ticks for waiting
 *
 * @return     ws_err_t
 */
ws_err_t websocket_client_close(websocket_client_handle_t client, TickType_t timeout);

/**
 * @brief      Close the WebSocket connection in a clean way with custom code/data
 *             Closing sequence is the same as for ws_websocket_client_close()
 *
 *  Notes:
 *  - Cannot be called from the websocket event handler
 *
 * @param[in]  client  The client
 * @param[in]  code    Close status code as defined in RFC6455 section-7.4
 * @param[in]  data    Additional data to closing message
 * @param[in]  len     The length of the additional data
 * @param[in]  timeout Timeout in RTOS ticks for waiting
 *
 * @return     ws_err_t
 */
ws_err_t websocket_client_close_with_code(websocket_client_handle_t client, int code, const char *data, int len, TickType_t timeout);

/**
 * @brief      Check the WebSocket client connection state
 *
 * @param[in]  client  The client handle
 *
 * @return
 *     - true
 *     - false
 */
bool websocket_client_is_connected(websocket_client_handle_t client);


ws_err_t websocket_client_abort_connection(websocket_client_handle_t client);

#ifdef __cplusplus
}
#endif

#endif	/*_WEBSOCKET_CLIENT_H_*/

/* EOF */

