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

#include <string.h>
#include <stdlib.h>
#include "transport.h"
#include "transport_ssl.h"
#include "transport_ssl_internal.h"
#include "transport_utils.h"

#include "task.h"
#include "ws_tls.h"
#include "ws_log.h"
#include "ws_mem.h"


#define INVALID_SOCKET (-1)

#define GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t)         \
    transport_ssl_t *ssl = ssl_get_context_data(t);  \
    if (!ssl) { return; }

typedef enum {
    TRANS_SSL_INIT = 0,
    TRANS_SSL_CONNECTING,
} transport_ssl_conn_state_t;

/**
 *  mbedtls specific transport data
 */
typedef struct transport_ws_tls {
    ws_tls_t                *tls;
    ws_tls_cfg_t            cfg;
    bool                     ssl_initialized;
    transport_ssl_conn_state_t conn_state;
} transport_ssl_t;

static int ssl_close(ws_transport_handle_t t);

static int ssl_connect_async(ws_transport_handle_t t, const char *host, int port, int timeout_ms)
{
    transport_ssl_t *ssl = ws_transport_get_context_data(t);
    if (ssl->conn_state == TRANS_SSL_INIT) {
        ssl->cfg.timeout_ms = timeout_ms;
        ssl->cfg.non_block = true;
        ssl->ssl_initialized = true;
        ssl->tls = ws_tls_init();
        if (!ssl->tls) {
            return -1;
        }
        ssl->conn_state = TRANS_SSL_CONNECTING;
    }
    if (ssl->conn_state == TRANS_SSL_CONNECTING) {
        return ws_tls_conn_new_async(host, (int)strlen(host), port, &ssl->cfg, ssl->tls);
    }
    return 0;
}

static int ssl_connect(ws_transport_handle_t t, const char *host, int port, int timeout_ms)
{
    transport_ssl_t *ssl = ws_transport_get_context_data(t);

    ssl->cfg.timeout_ms = timeout_ms;
    ssl->ssl_initialized = true;
    ssl->tls = ws_tls_init();
    if (ws_tls_conn_new_sync(host, (int)strlen(host), port, &ssl->cfg, ssl->tls) <= 0) {
        WS_LOGE("TRANS_SSL", "Failed to open a new connection\n");
        ws_transport_set_errors(t, ssl->tls->error_handle);
        ws_tls_conn_destroy(ssl->tls);
        ssl->tls = NULL;
        return -1;
    }

    return 0;
}

static int ssl_poll_read(ws_transport_handle_t t, int timeout_ms)
{
    transport_ssl_t *ssl = ws_transport_get_context_data(t);
    int ret = -1;
    int remain = 0;
    struct timeval timeout;
    fd_set readset;
    fd_set errset;
    FD_ZERO(&readset);
    FD_ZERO(&errset);
    FD_SET((unsigned int)(ssl->tls->sockfd), &readset);
    FD_SET((unsigned int)(ssl->tls->sockfd), &errset);

    if ((remain = ws_tls_get_bytes_avail(ssl->tls)) > 0) {
        WS_LOGD("TRANS_SSL", "remain data in cache, need to read again\n");
        return remain;
    }
    ret = select(ssl->tls->sockfd + 1, &readset, NULL, &errset, ws_transport_utils_ms_to_timeval(timeout_ms, &timeout));
    if (ret > 0 && FD_ISSET((unsigned int)(ssl->tls->sockfd), &errset)) {
        int sock_errno = 0;
        uint32_t optlen = sizeof(sock_errno);
        getsockopt(ssl->tls->sockfd, SOL_SOCKET, SO_ERROR, &sock_errno, &optlen);
        WS_LOGE("TRANS_SSL", "ssl_poll_read select error %d, errno = %s, fd = %d\n", sock_errno, strerror(sock_errno), ssl->tls->sockfd);
        ret = -1;
    }
    return ret;
}

static int ssl_poll_write(ws_transport_handle_t t, int timeout_ms)
{
    transport_ssl_t *ssl = ws_transport_get_context_data(t);
    int ret = -1;
    struct timeval timeout;
    fd_set writeset;
    fd_set errset;
    FD_ZERO(&writeset);
    FD_ZERO(&errset);
    FD_SET((unsigned int)(ssl->tls->sockfd), &writeset);
    FD_SET((unsigned int)(ssl->tls->sockfd), &errset);
    ret = select(ssl->tls->sockfd + 1, NULL, &writeset, &errset, ws_transport_utils_ms_to_timeval(timeout_ms, &timeout));
    if (ret > 0 && FD_ISSET((unsigned int)(ssl->tls->sockfd), &errset)) {
        int sock_errno = 0;
        uint32_t optlen = sizeof(sock_errno);
        getsockopt(ssl->tls->sockfd, SOL_SOCKET, SO_ERROR, &sock_errno, &optlen);
        WS_LOGE("TRANS_SSL", "ssl_poll_write select error %d, errno = %s, fd = %d\n", sock_errno, strerror(sock_errno), ssl->tls->sockfd);
        ret = -1;
    }
    return ret;
}

static int ssl_write(ws_transport_handle_t t, const char *buffer, int len, int timeout_ms)
{
    int poll, ret;
    transport_ssl_t *ssl = ws_transport_get_context_data(t);

    if ((poll = ws_transport_poll_write(t, timeout_ms)) <= 0) {
        WS_LOGW("TRANS_SSL", "Poll timeout or error, errno=%s, fd=%d, timeout_ms=%d\n", strerror(errno), ssl->tls->sockfd, timeout_ms);
        return poll;
    }
    ret = ws_tls_conn_write(ssl->tls, (const unsigned char *) buffer, (size_t)len);
    if (ret < 0) {
        WS_LOGE("TRANS_SSL", "ws_tls_conn_write error, errno=%s\n", strerror(errno));
        ws_transport_set_errors(t, ssl->tls->error_handle);
    }
    return ret;
}

static int ssl_read(ws_transport_handle_t t, char *buffer, int len, int timeout_ms)
{
    int poll, ret;
    transport_ssl_t *ssl = ws_transport_get_context_data(t);

    if ((poll = ws_transport_poll_read(t, timeout_ms)) <= 0) {
        return poll;
    }
    ret = ws_tls_conn_read(ssl->tls, (unsigned char *)buffer, (size_t)len);
    if (ret < 0) {
        WS_LOGE("TRANS_SSL", "ws_tls_conn_read error, errno=%s\n", strerror(errno));
        ws_transport_set_errors(t, ssl->tls->error_handle);
    }
    if (ret == 0) {
        ret = -1;
    }
    return ret;
}

static int ssl_close(ws_transport_handle_t t)
{
    int ret = -1;
    transport_ssl_t *ssl = ws_transport_get_context_data(t);
    if (ssl->ssl_initialized) {
        ret = ws_tls_conn_destroy(ssl->tls);
        ssl->conn_state = TRANS_SSL_INIT;
        ssl->ssl_initialized = false;
    }
    return ret;
}

static int ssl_destroy(ws_transport_handle_t t)
{
    transport_ssl_t *ssl = ws_transport_get_context_data(t);
    ws_transport_close(t);
    _ws_free(ssl);
    return 0;
}

static void ws_transport_ssl_set_interface_name(ws_transport_handle_t t, struct ifreq *if_name)
{
    transport_ssl_t *ssl = ws_transport_get_context_data(t);
    if (t && ssl) {
    	ssl->cfg.if_name = if_name;
    }
}

void ws_transport_ssl_enable_global_ca_store(ws_transport_handle_t t)
{
    transport_ssl_t *ssl = ws_transport_get_context_data(t);
    if (t && ssl) {
        ssl->cfg.use_global_ca_store = true;
    }
}

void ws_transport_ssl_set_psk_key_hint(ws_transport_handle_t t, const psk_hint_key_t* psk_hint_key)
{
    transport_ssl_t *ssl = ws_transport_get_context_data(t);
    if (t && ssl) {
        ssl->cfg.psk_hint_key =  psk_hint_key;
    }
}

void ws_transport_ssl_set_cert_data(ws_transport_handle_t t, const char *data, int len)
{
    transport_ssl_t *ssl = ws_transport_get_context_data(t);
    if (t && ssl) {
        ssl->cfg.cacert_pem_buf = (void *)data;
        ssl->cfg.cacert_pem_bytes = (unsigned int)(len + 1);
    }
}

void ws_transport_ssl_set_cert_data_der(ws_transport_handle_t t, const char *data, int len)
{
    transport_ssl_t *ssl = ws_transport_get_context_data(t);
    if (t && ssl) {
        ssl->cfg.cacert_buf = (void *)data;
        ssl->cfg.cacert_bytes = (unsigned int)len;
    }
}

void ws_transport_ssl_set_client_cert_data(ws_transport_handle_t t, const char *data, int len)
{
    transport_ssl_t *ssl = ws_transport_get_context_data(t);
    if (t && ssl) {
        ssl->cfg.clientcert_pem_buf = (void *)data;
        ssl->cfg.clientcert_pem_bytes = (unsigned int)(len + 1);
    }
}

void ws_transport_ssl_set_client_cert_data_der(ws_transport_handle_t t, const char *data, int len)
{
    transport_ssl_t *ssl = ws_transport_get_context_data(t);
    if (t && ssl) {
        ssl->cfg.clientcert_buf = (void *)data;
        ssl->cfg.clientcert_bytes = (unsigned int)len;
    }
}

void ws_transport_ssl_set_client_key_data(ws_transport_handle_t t, const char *data, int len)
{
    transport_ssl_t *ssl = ws_transport_get_context_data(t);
    if (t && ssl) {
        ssl->cfg.clientkey_pem_buf = (void *)data;
        ssl->cfg.clientkey_pem_bytes = (unsigned int)(len + 1);
    }
}

void ws_transport_ssl_set_client_key_password(ws_transport_handle_t t, const char *password, int password_len)
{
    transport_ssl_t *ssl = ws_transport_get_context_data(t);
    if (t && ssl) {
        ssl->cfg.clientkey_password = (void *)password;
        ssl->cfg.clientkey_password_len = (unsigned int)password_len;
    }
}

void ws_transport_ssl_set_client_key_data_der(ws_transport_handle_t t, const char *data, int len)
{
    transport_ssl_t *ssl = ws_transport_get_context_data(t);
    if (t && ssl) {
        ssl->cfg.clientkey_buf = (void *)data;
        ssl->cfg.clientkey_bytes = (unsigned int)len;
    }
}

void ws_transport_ssl_set_alpn_protocol(ws_transport_handle_t t, const char **alpn_protos)
{
    transport_ssl_t *ssl = ws_transport_get_context_data(t);
    if (t && ssl) {
        ssl->cfg.alpn_protos = alpn_protos;
    }
}

void ws_transport_ssl_skip_common_name_check(ws_transport_handle_t t)
{
    transport_ssl_t *ssl = ws_transport_get_context_data(t);
    if (t && ssl) {
        ssl->cfg.skip_common_name = true;
    }
}

void ws_transport_ssl_use_secure_element(ws_transport_handle_t t)
{
    transport_ssl_t *ssl = ws_transport_get_context_data(t);
    if (t && ssl) {
        ssl->cfg.use_secure_element = true;
    }
}

void ws_transport_ssl_set_keep_alive(ws_transport_handle_t t, ws_transport_keep_alive_t *keep_alive_cfg)
{
    transport_ssl_t *ssl = ws_transport_get_context_data(t);
    if (t && ssl) {
        ssl->cfg.keep_alive_cfg = (tls_keep_alive_cfg_t *)keep_alive_cfg;
    }
}

ws_transport_handle_t ws_transport_ssl_init(void)
{
    ws_transport_handle_t t = ws_transport_init();
    transport_ssl_t *ssl = _ws_calloc(1, sizeof(transport_ssl_t));
    WS_TRANSPORT_MEM_CHECK("TRANS_SSL", ssl, {
	ws_transport_destroy(t);
	return NULL;
    });
    ws_transport_set_context_data(t, ssl);
    ws_transport_set_func(t, ssl_connect, ssl_read, ssl_write, ssl_close, ssl_poll_read, ssl_poll_write, ssl_destroy);
    ws_transport_set_async_connect_func(t, ssl_connect_async);
    return t;
}

void ws_transport_tcp_set_interface_name(ws_transport_handle_t t, struct ifreq *if_name)
{
    return ws_transport_ssl_set_interface_name(t, if_name);
}

#endif // __SUPPORT_WEBSOCKET_CLIENT__

/* EOF */
