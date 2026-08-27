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
#include "FreeRTOS.h"
#include "lwipopts.h"
	
#if defined ( __SUPPORT_WEBSOCKET_CLIENT__ )

#include <stdlib.h>
#include <strings.h>
#include "transport.h"
#include "transport_ssl_internal.h"
#include "transport_utils.h"
#include "ws_tls.h"

#include "sys/queue.h"
#include "ws_log.h"
#include "ws_mem.h"


typedef int (*get_socket_func)(ws_transport_handle_t t);

/**
 * Transport layer structure, which will provide functions, basic properties for transport types
 */
struct ws_transport_item_t {
    int             port;
    int             socket;         /*!< Socket to use in this transport */
    char            *scheme;        /*!< Tag name */
    void            *context;       /*!< Context data */
    void            *data;          /*!< Additional transport data */
    connect_func    _connect;       /*!< Connect function of this transport */
    io_read_func    _read;          /*!< Read */
    io_func         _write;         /*!< Write */
    trans_func      _close;         /*!< Close */
    poll_func       _poll_read;     /*!< Poll and read */
    poll_func       _poll_write;    /*!< Poll and write */
    trans_func      _destroy;       /*!< Destroy and free transport */
    connect_async_func _connect_async;      /*!< non-blocking connect function of this transport */
    payload_transfer_func  _parent_transfer;        /*!< Function returning underlying transport layer */
    get_socket_func        _get_socket;             /*!< Function returning the transport's socket */	
    ws_tls_error_handle_t     error_handle;            /*!< Pointer to ws-tls error handle */
    ws_transport_keep_alive_t *keep_alive_cfg;    /*!< TCP keep-alive config */
    STAILQ_ENTRY(ws_transport_item_t) next;
};


/**
 * This list will hold all transport available
 */
STAILQ_HEAD(ws_transport_list_t, ws_transport_item_t);

/**
 * Internal transport structure holding list of transports and other data common to all transports
 */
typedef struct ws_transport_internal {
    struct ws_transport_list_t list;                      /*!< List of transports */
    ws_tls_error_handle_t  error_handle;                               /*!< Pointer to the error tracker if enabled  */
} ws_transport_internal_t;

static ws_transport_handle_t ws_transport_get_default_parent(ws_transport_handle_t t)
{
    /*
    * By default, the underlying transport layer handle is the handle itself
    */
    return t;
}

ws_transport_list_handle_t ws_transport_list_init(void)
{
    ws_transport_list_handle_t transport = _ws_calloc(1, sizeof(ws_transport_internal_t));
    WS_TRANSPORT_MEM_CHECK("TRANSPORT", transport, return NULL);
    STAILQ_INIT(&transport->list);
    transport->error_handle = _ws_calloc(1, sizeof(ws_tls_last_error_t));
    return transport;
}

ws_err_t ws_transport_list_add(ws_transport_list_handle_t h, ws_transport_handle_t t, const char *scheme)
{
    if (h == NULL || t == NULL) {
        return WS_ERR_INVALID_ARG;
    }
    t->scheme = _ws_calloc(1, strlen(scheme) + 1);
    WS_TRANSPORT_MEM_CHECK("TRANSPORT", t->scheme, return WS_ERR_NO_MEM);
    strcpy(t->scheme, scheme);
    STAILQ_INSERT_TAIL(&h->list, t, next);
    // Each transport in a list to share the same error tracker
    t->error_handle = h->error_handle;
    return WS_OK;
}

ws_transport_handle_t ws_transport_list_get_transport(ws_transport_list_handle_t h, const char *scheme)
{
    if (!h) {
        return NULL;
    }
    if (scheme == NULL) {
        return STAILQ_FIRST(&h->list);
    }
    ws_transport_handle_t item;
    STAILQ_FOREACH(item, &h->list, next) {
        if (strcasecmp(item->scheme, scheme) == 0) {
            return item;
        }
    }
    return NULL;
}

ws_err_t ws_transport_list_destroy(ws_transport_list_handle_t h)
{
    ws_transport_list_clean(h);
    _ws_free(h->error_handle);
    _ws_free(h);
    return WS_OK;
}

ws_err_t ws_transport_list_clean(ws_transport_list_handle_t h)
{
    ws_transport_handle_t item = STAILQ_FIRST(&h->list);
    ws_transport_handle_t tmp;
    while (item != NULL) {
        tmp = STAILQ_NEXT(item, next);
        ws_transport_destroy(item);
        item = tmp;
    }
    STAILQ_INIT(&h->list);
    return WS_OK;
}

ws_transport_handle_t ws_transport_init(void)
{
    ws_transport_handle_t t = _ws_calloc(1, sizeof(struct ws_transport_item_t));
    WS_TRANSPORT_MEM_CHECK("TRANSPORT", t, return NULL);
    return t;
}

ws_transport_handle_t ws_transport_get_payload_transport_handle(ws_transport_handle_t t)
{
    if (t && t->_read) {
        return t->_parent_transfer(t);
    }
    return NULL;
}

ws_err_t ws_transport_destroy(ws_transport_handle_t t)
{
    if (t->_destroy) {
        t->_destroy(t);
    }
    if (t->scheme) {
        _ws_free(t->scheme);
    }
    _ws_free(t);
    return WS_OK;
}

int ws_transport_connect(ws_transport_handle_t t, const char *host, int port, int timeout_ms)
{
    int ret = -1;
    if (t && t->_connect) {
        return t->_connect(t, host, port, timeout_ms);
    }
    return ret;
}

int ws_transport_connect_async(ws_transport_handle_t t, const char *host, int port, int timeout_ms)
{
    int ret = -1;
    if (t && t->_connect_async) {
        return t->_connect_async(t, host, port, timeout_ms);
    }
    return ret;
}

int ws_transport_read(ws_transport_handle_t t, char *buffer, int len, int timeout_ms)
{
    if (t && t->_read) {
        return t->_read(t, buffer, len, timeout_ms);
    }
    return -1;
}

int ws_transport_write(ws_transport_handle_t t, const char *buffer, int len, int timeout_ms)
{
    if (t && t->_write) {
        return t->_write(t, buffer, len, timeout_ms);
    }
    return -1;
}

int ws_transport_poll_read(ws_transport_handle_t t, int timeout_ms)
{
    if (t && t->_poll_read) {
        return t->_poll_read(t, timeout_ms);
    }
    return -1;
}

int ws_transport_poll_write(ws_transport_handle_t t, int timeout_ms)
{
    if (t && t->_poll_write) {
        return t->_poll_write(t, timeout_ms);
    }
    return -1;
}

int ws_transport_close(ws_transport_handle_t t)
{
    if (t && t->_close) {
        return t->_close(t);
    }
    return 0;
}

void *ws_transport_get_context_data(ws_transport_handle_t t)
{
    if (t) {
        return t->data;
    }
    return NULL;
}

ws_err_t ws_transport_set_context_data(ws_transport_handle_t t, void *data)
{
    if (t) {
        t->data = data;
        return WS_OK;
    }
    return WS_FAIL;
}

ws_err_t ws_transport_set_func(ws_transport_handle_t t,
                             connect_func _connect,
                             io_read_func _read,
                             io_func _write,
                             trans_func _close,
                             poll_func _poll_read,
                             poll_func _poll_write,
                             trans_func _destroy)
{
    if (t == NULL) {
        return WS_FAIL;
    }
    t->_connect = _connect;
    t->_read = _read;
    t->_write = _write;
    t->_close = _close;
    t->_poll_read = _poll_read;
    t->_poll_write = _poll_write;
    t->_destroy = _destroy;
    t->_connect_async = NULL;
    t->_parent_transfer = ws_transport_get_default_parent;
    return WS_OK;
}

int ws_transport_get_default_port(ws_transport_handle_t t)
{
    if (t == NULL) {
        return -1;
    }
    return t->port;
}

ws_err_t ws_transport_set_default_port(ws_transport_handle_t t, int port)
{
    if (t == NULL) {
        return WS_FAIL;
    }
    t->port = port;
    return WS_OK;
}

ws_err_t ws_transport_set_async_connect_func(ws_transport_handle_t t, connect_async_func _connect_async_func)
{
    if (t == NULL) {
        return WS_FAIL;
    }
    t->_connect_async = _connect_async_func;
    return WS_OK;
}

ws_err_t ws_transport_set_parent_transport_func(ws_transport_handle_t t, payload_transfer_func _parent_transport)
{
    if (t == NULL) {
        return WS_FAIL;
    }
    t->_parent_transfer = _parent_transport;
    return WS_OK;
}

ws_tls_error_handle_t ws_transport_get_error_handle(ws_transport_handle_t t)
{
    if (t) {
        return t->error_handle;
    }
    return NULL;
}

void ws_transport_set_errors(ws_transport_handle_t t, const ws_tls_error_handle_t error_handle)
{
    if (t)  {
        memcpy(t->error_handle, error_handle, sizeof(ws_tls_last_error_t));
    }
}

void ws_transport_set_keep_alive(ws_transport_handle_t t, ws_transport_keep_alive_t *keep_alive_cfg)
{
    if (t && keep_alive_cfg) {
        t->keep_alive_cfg = keep_alive_cfg;
    }
}

void *ws_transport_get_keep_alive(ws_transport_handle_t t)
{
    if (t) {
        return t->keep_alive_cfg;
    }
    return NULL;
}

int ws_transport_get_socket(ws_transport_handle_t t)
{
    if (t && t->_get_socket)  {
        return  t->_get_socket(t);
    }
    return -1;
}

#endif // __SUPPORT_WEBSOCKET_CLIENT__

/* EOF */
