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

#ifndef _WS_TRANSPORT_H_
#define _WS_TRANSPORT_H_

#include <ws_err.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
*  @brief Keep alive parameters structure
*/
typedef struct ws_transport_keepalive {
    bool            keep_alive_enable;      /*!< Enable keep-alive timeout */
    int             keep_alive_idle;        /*!< Keep-alive idle time (second) */
    int             keep_alive_interval;    /*!< Keep-alive interval time (second) */
    int             keep_alive_count;       /*!< Keep-alive packet retry send count */
} ws_transport_keep_alive_t;

typedef struct ws_transport_internal* ws_transport_list_handle_t;
typedef struct ws_transport_item_t* ws_transport_handle_t;

typedef int (*connect_func)(ws_transport_handle_t t, const char *host, int port, int timeout_ms);
typedef int (*io_func)(ws_transport_handle_t t, const char *buffer, int len, int timeout_ms);
typedef int (*io_read_func)(ws_transport_handle_t t, char *buffer, int len, int timeout_ms);
typedef int (*trans_func)(ws_transport_handle_t t);
typedef int (*poll_func)(ws_transport_handle_t t, int timeout_ms);
typedef int (*connect_async_func)(ws_transport_handle_t t, const char *host, int port, int timeout_ms);
typedef ws_transport_handle_t (*payload_transfer_func)(ws_transport_handle_t);

typedef struct ws_tls_last_error* ws_tls_error_handle_t;

/**
 * @brief      Create transport list
 *
 * @return     A handle can hold all transports
 */
ws_transport_list_handle_t ws_transport_list_init(void);

/**
 * @brief      Cleanup and free all transports, include itself,
 *             this function will invoke ws_transport_destroy of every transport have added this the list
 *
 * @param[in]  list  The list
 *
 * @return
 *     - WS_OK
 *     - WS_FAIL
 */
ws_err_t ws_transport_list_destroy(ws_transport_list_handle_t list);

/**
 * @brief      Add a transport to the list, and define a scheme to indentify this transport in the list
 *
 * @param[in]  list    The list
 * @param[in]  t       The Transport
 * @param[in]  scheme  The scheme
 *
 * @return
 *     - WS_OK
 */
ws_err_t ws_transport_list_add(ws_transport_list_handle_t list, ws_transport_handle_t t, const char *scheme);

/**
 * @brief      This function will remove all transport from the list,
 *             invoke ws_transport_destroy of every transport have added this the list
 *
 * @param[in]  list  The list
 *
 * @return
 *     - WS_OK
 *     - WS_ERR_INVALID_ARG
 */
ws_err_t ws_transport_list_clean(ws_transport_list_handle_t list);

/**
 * @brief      Get the transport by scheme, which has been defined when calling function `ws_transport_list_add`
 *
 * @param[in]  list  The list
 * @param[in]  tag   The tag
 *
 * @return     The transport handle
 */
ws_transport_handle_t ws_transport_list_get_transport(ws_transport_list_handle_t list, const char *scheme);

/**
 * @brief      Initialize a transport handle object
 *
 * @return     The transport handle
 */
ws_transport_handle_t ws_transport_init(void);

/**
 * @brief      Cleanup and free memory the transport
 *
 * @param[in]  t     The transport handle
 *
 * @return
 *     - WS_OK
 *     - WS_FAIL
 */
ws_err_t ws_transport_destroy(ws_transport_handle_t t);

/**
 * @brief      Get default port number used by this transport
 *
 * @param[in]  t     The transport handle
 *
 * @return     the port number
 */
int ws_transport_get_default_port(ws_transport_handle_t t);

/**
 * @brief      Set default port number that can be used by this transport
 *
 * @param[in]  t     The transport handle
 * @param[in]  port  The port number
 *
 * @return
 *     - WS_OK
 *     - WS_FAIL
 */
ws_err_t ws_transport_set_default_port(ws_transport_handle_t t, int port);

/**
 * @brief      Transport connection function, to make a connection to server
 *
 * @param      t           The transport handle
 * @param[in]  host        Hostname
 * @param[in]  port        Port
 * @param[in]  timeout_ms  The timeout milliseconds (-1 indicates wait forever)
 *
 * @return
 * - socket for will use by this transport
 * - (-1) if there are any errors, should check errno
 */
int ws_transport_connect(ws_transport_handle_t t, const char *host, int port, int timeout_ms);

/**
 * @brief      Non-blocking transport connection function, to make a connection to server
 *
 * @param      t           The transport handle
 * @param[in]  host        Hostname
 * @param[in]  port        Port
 * @param[in]  timeout_ms  The timeout milliseconds (-1 indicates wait forever)
 *
 * @return
 * - socket for will use by this transport
 * - (-1) if there are any errors, should check errno
 */
int ws_transport_connect_async(ws_transport_handle_t t, const char *host, int port, int timeout_ms);

/**
 * @brief      Transport read function
 *
 * @param      t           The transport handle
 * @param      buffer      The buffer
 * @param[in]  len         The length
 * @param[in]  timeout_ms  The timeout milliseconds (-1 indicates wait forever)
 *
 * @return
 *  - Number of bytes was read
 *  - (-1) if there are any errors, should check errno
 */
int ws_transport_read(ws_transport_handle_t t, char *buffer, int len, int timeout_ms);

/**
 * @brief      Poll the transport until readable or timeout
 *
 * @param[in]  t           The transport handle
 * @param[in]  timeout_ms  The timeout milliseconds (-1 indicates wait forever)
 *
 * @return
 *     - 0      Timeout
 *     - (-1)   If there are any errors, should check errno
 *     - other  The transport can read
 */
int ws_transport_poll_read(ws_transport_handle_t t, int timeout_ms);

/**
 * @brief      Transport write function
 *
 * @param      t           The transport handle
 * @param      buffer      The buffer
 * @param[in]  len         The length
 * @param[in]  timeout_ms  The timeout milliseconds (-1 indicates wait forever)
 *
 * @return
 *  - Number of bytes was written
 *  - (-1) if there are any errors, should check errno
 */
int ws_transport_write(ws_transport_handle_t t, const char *buffer, int len, int timeout_ms);

/**
 * @brief      Poll the transport until writeable or timeout
 *
 * @param[in]  t           The transport handle
 * @param[in]  timeout_ms  The timeout milliseconds (-1 indicates wait forever)
 *
 * @return
 *     - 0      Timeout
 *     - (-1)   If there are any errors, should check errno
 *     - other  The transport can write
 */
int ws_transport_poll_write(ws_transport_handle_t t, int timeout_ms);

/**
 * @brief      Transport close
 *
 * @param      t     The transport handle
 *
 * @return
 * - 0 if ok
 * - (-1) if there are any errors, should check errno
 */
int ws_transport_close(ws_transport_handle_t t);

/**
 * @brief      Get user data context of this transport
 *
 * @param[in]  t        The transport handle
 *
 * @return     The user data context
 */
void *ws_transport_get_context_data(ws_transport_handle_t t);

/**
 * @brief      Get transport handle of underlying protocol
 *             which can access this protocol payload directly
 *             (used for receiving longer msg multiple times)
 *
 * @param[in]  t        The transport handle
 *
 * @return     Payload transport handle
 */
ws_transport_handle_t ws_transport_get_payload_transport_handle(ws_transport_handle_t t);

/**
 * @brief      Set the user context data for this transport
 *
 * @param[in]  t        The transport handle
 * @param      data     The user data context
 *
 * @return
 *     - WS_OK
 */
ws_err_t ws_transport_set_context_data(ws_transport_handle_t t, void *data);

/**
 * @brief      Set transport functions for the transport handle
 *
 * @param[in]  t            The transport handle
 * @param[in]  _connect     The connect function pointer
 * @param[in]  _read        The read function pointer
 * @param[in]  _write       The write function pointer
 * @param[in]  _close       The close function pointer
 * @param[in]  _poll_read   The poll read function pointer
 * @param[in]  _poll_write  The poll write function pointer
 * @param[in]  _destroy     The destroy function pointer
 *
 * @return
 *     - WS_OK
 */
ws_err_t ws_transport_set_func(ws_transport_handle_t t,
                             connect_func _connect,
                             io_read_func _read,
                             io_func _write,
                             trans_func _close,
                             poll_func _poll_read,
                             poll_func _poll_write,
                             trans_func _destroy);


/**
 * @brief      Set transport functions for the transport handle
 *
 * @param[in]  t                    The transport handle
 * @param[in]  _connect_async_func  The connect_async function pointer
 *
 * @return
 *     - WS_OK
 *     - WS_FAIL
 */
ws_err_t ws_transport_set_async_connect_func(ws_transport_handle_t t, connect_async_func _connect_async_func);

/**
 * @brief      Set parent transport function to the handle
 *
 * @param[in]  t                    The transport handle
 * @param[in]  _parent_transport    The underlying transport getter pointer
 *
 * @return
 *     - WS_OK
 *     - WS_FAIL
 */
ws_err_t ws_transport_set_parent_transport_func(ws_transport_handle_t t, payload_transfer_func _parent_transport);

/**
 * @brief      Returns ws_tls error handle.
 *             Warning: The returned pointer is valid only as long as ws_transport_handle_t exists. Once transport
 *             handle gets destroyed, this value (ws_tls_error_handle_t) is freed automatically.
 *
 * @param[in]  A transport handle
 *
 * @return
 *            - valid pointer of ws_error_handle_t
 *            - NULL if invalid transport handle
  */
ws_tls_error_handle_t ws_transport_get_error_handle(ws_transport_handle_t t);

/**
 * @brief      Set keep-alive configuration
 *
 * @param[in]  t               The transport handle
 * @param[in]  keep_alive_cfg  The keep-alive config
 */
void ws_transport_set_keep_alive(ws_transport_handle_t t, ws_transport_keep_alive_t *keep_alive_cfg);

/**
 * @brief      Get keep-alive config of this transport
 *
 * @param[in]  t        The transport handle
 *
 * @return     The keep-alive configuration
 */
void *ws_transport_get_keep_alive(ws_transport_handle_t t);

int ws_transport_get_socket(ws_transport_handle_t t);

#ifdef __cplusplus
}
#endif
#endif /* _WS_TRANSPORT_ */
