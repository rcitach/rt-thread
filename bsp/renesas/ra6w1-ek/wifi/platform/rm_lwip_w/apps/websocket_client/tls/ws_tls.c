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

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <http_parser.h>
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/def.h"
#include "ws_tls.h"
#include "ws_tls_error_capture_internal.h"
#include <errno.h>


#ifdef CONFIG_WS_TLS_USING_MBEDTLS
#include "ws_tls_mbedtls.h"
#elif CONFIG_WS_TLS_USING_WOLFSSL
#include "ws_tls_wolfssl.h"
#endif

#include "ws_log.h"
#include "ws_mem.h"

#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"


#ifdef CONFIG_WS_TLS_USING_MBEDTLS
#define _ws_create_ssl_handle              ws_create_mbedtls_handle
#define _ws_tls_handshake                  ws_mbedtls_handshake
#define _ws_tls_read                       ws_mbedtls_read
#define _ws_tls_write                      ws_mbedtls_write
#define _ws_tls_conn_delete                ws_mbedtls_conn_delete
#ifdef CONFIG_WS_TLS_SERVER
#define _ws_tls_server_session_create      ws_mbedtls_server_session_create
#define _ws_tls_server_session_delete      ws_mbedtls_server_session_delete
#endif  /* CONFIG_WS_TLS_SERVER */
#define _ws_tls_get_bytes_avail            ws_mbedtls_get_bytes_avail
#define _ws_tls_init_global_ca_store       ws_mbedtls_init_global_ca_store
#define _ws_tls_set_global_ca_store        ws_mbedtls_set_global_ca_store                 /*!< Callback function for setting global CA store data for TLS/SSL */
#define _ws_tls_get_global_ca_store        ws_mbedtls_get_global_ca_store
#define _ws_tls_free_global_ca_store       ws_mbedtls_free_global_ca_store                /*!< Callback function for freeing global ca store for TLS/SSL */
#elif CONFIG_WS_TLS_USING_WOLFSSL /* CONFIG_WS_TLS_USING_MBEDTLS */
#define _ws_create_ssl_handle              ws_create_wolfssl_handle
#define _ws_tls_handshake                  ws_wolfssl_handshake
#define _ws_tls_read                       ws_wolfssl_read
#define _ws_tls_write                      ws_wolfssl_write
#define _ws_tls_conn_delete                ws_wolfssl_conn_delete
#ifdef CONFIG_WS_TLS_SERVER
#define _ws_tls_server_session_create      ws_wolfssl_server_session_create
#define _ws_tls_server_session_delete      ws_wolfssl_server_session_delete
#endif  /* CONFIG_WS_TLS_SERVER */
#define _ws_tls_get_bytes_avail            ws_wolfssl_get_bytes_avail
#define _ws_tls_init_global_ca_store       ws_wolfssl_init_global_ca_store
#define _ws_tls_set_global_ca_store        ws_wolfssl_set_global_ca_store                 /*!< Callback function for setting global CA store data for TLS/SSL */
#define _ws_tls_free_global_ca_store       ws_wolfssl_free_global_ca_store                /*!< Callback function for freeing global ca store for TLS/SSL */
#else   /* WS_TLS_USING_WOLFSSL */
#error "No TLS stack configured"
#endif

static ws_err_t create_ssl_handle(const char *hostname, size_t hostlen, const void *cfg, ws_tls_t *tls)
{
    return _ws_create_ssl_handle(hostname, hostlen, cfg, tls);
}

static ws_err_t ws_tls_handshake(ws_tls_t *tls, const ws_tls_cfg_t *cfg)
{
    return _ws_tls_handshake(tls, cfg);
}

static ssize_t tcp_read(ws_tls_t *tls, char *data, size_t datalen)
{
    return recv(tls->sockfd, data, datalen, 0);
}

static ssize_t tcp_write(ws_tls_t *tls, const char *data, size_t datalen)
{
    return send(tls->sockfd, data, datalen, 0);
}

/**
 * @brief      Close the TLS connection and free any allocated resources.
 */
void ws_tls_conn_delete(ws_tls_t *tls)
{
    ws_tls_conn_destroy(tls);
}

int ws_tls_conn_destroy(ws_tls_t *tls)
{
    if (tls != NULL) {
        int ret = 0;
        _ws_tls_conn_delete(tls);
        if (tls->sockfd >= 0) {
            ret = close(tls->sockfd);
        }
        _ws_free(tls->error_handle);
        _ws_free(tls);
        return ret;
    }
    return -1; // invalid argument
}

ws_tls_t *ws_tls_init(void)
{
    ws_tls_t *tls = (ws_tls_t *)_ws_calloc(1, sizeof(ws_tls_t));
    if (!tls) {
        return NULL;
    }
    tls->error_handle = _ws_calloc(1, sizeof(ws_tls_last_error_t));
    if (!tls->error_handle) {
        _ws_free(tls);
        return NULL;
    }
#ifdef CONFIG_WS_TLS_USING_MBEDTLS
    tls->server_fd.fd = -1;
#endif
    tls->sockfd = -1;
    return tls;
}

static ws_err_t resolve_host_name(const char *host, size_t hostlen, struct addrinfo **address_info)
{
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    char *use_host = _ws_strndup(host, hostlen);
    if (!use_host) {
        return WS_ERR_NO_MEM;
    }

    WS_LOGD("WS-TLS", "Host:%s: strlen %lu\n", use_host, (unsigned long)hostlen);
    if (getaddrinfo(use_host, NULL, &hints, address_info)) {
        WS_LOGE("WS-TLS", "couldn't get hostname for :%s:\n", use_host);
        _ws_free(use_host);
        return WS_ERR_WS_TLS_CANNOT_RESOLVE_HOSTNAME;
    }
    _ws_free(use_host);
    return WS_OK;
}

static void ms_to_timeval(int timeout_ms, struct timeval *tv)
{
    tv->tv_sec = timeout_ms / 1000;
    tv->tv_usec = (timeout_ms % 1000) * 1000;
}

static int ws_tls_tcp_enable_keep_alive(int fd, tls_keep_alive_cfg_t *cfg)
{
    int keep_alive_enable = 1;
    int keep_alive_idle = cfg->keep_alive_idle;
    int keep_alive_interval = cfg->keep_alive_interval;
    int keep_alive_count = cfg->keep_alive_count;

    WS_LOGD("WS-TLS", "Enable TCP keep alive. idle: %d, interval: %d, count: %d\n", keep_alive_idle, keep_alive_interval, keep_alive_count);
    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &keep_alive_enable, sizeof(keep_alive_enable)) != 0) {
        WS_LOGE("WS-TLS", "Fail to setsockopt SO_KEEPALIVE\n");
        return -1;
    }
    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &keep_alive_idle, sizeof(keep_alive_idle)) != 0) {
        WS_LOGE("WS-TLS", "Fail to setsockopt TCP_KEEPIDLE\n");
        return -1;
    }
    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &keep_alive_interval, sizeof(keep_alive_interval)) != 0) {
        WS_LOGE("WS-TLS", "Fail to setsockopt TCP_KEEPINTVL\n");
        return -1;
    }
    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &keep_alive_count, sizeof(keep_alive_count)) != 0) {
        WS_LOGE("WS-TLS", "Fail to setsockopt TCP_KEEPCNT\n");
        return -1;
    }

    return 0;
}

static ws_err_t ws_tcp_connect(const char *host, int hostlen, int port, int *sockfd, const ws_tls_t *tls, const ws_tls_cfg_t *cfg)
{
    ws_err_t ret;
    struct addrinfo *addrinfo;
    if ((ret = resolve_host_name(host, (size_t)hostlen, &addrinfo)) != WS_OK) {
        return ret;
    }

    int fd = socket(addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol);
    if (fd < 0) {
        WS_LOGE("WS-TLS", "Failed to create socket (family %d socktype %d protocol %d)\n", addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol);
        WS_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ERR_TYPE_SYSTEM, errno);
        ret = WS_ERR_WS_TLS_CANNOT_CREATE_SOCKET;
        goto err_freeaddr;
    }

    void *addr_ptr;
    if (addrinfo->ai_family == AF_INET) {
#if LWIP_IPV4
        struct sockaddr_in *p = (struct sockaddr_in *)addrinfo->ai_addr;
        p->sin_port = htons((u16_t)port);
        addr_ptr = p;
#endif // LWIP_IPV4
    }
#if LWIP_IPV6
    else if (addrinfo->ai_family == AF_INET6) {
        struct sockaddr_in6 *p = (struct sockaddr_in6 *)addrinfo->ai_addr;
        p->sin6_port = htons((u16_t)port);
        p->sin6_family = AF_INET6;
        addr_ptr = p;
    }
#endif	/*LWIP_IPV6*/
    else {
        WS_LOGE("WS-TLS", "Unsupported protocol family %d\n", addrinfo->ai_family);
        ret = WS_ERR_WS_TLS_UNSUPPORTED_PROTOCOL_FAMILY;
        goto err_freesocket;
    }

    if (cfg) {
        if (cfg->timeout_ms >= 0) {
            struct timeval tv;
            ms_to_timeval(cfg->timeout_ms, &tv);
            setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
            if (cfg->keep_alive_cfg && cfg->keep_alive_cfg->keep_alive_enable) {
                if (ws_tls_tcp_enable_keep_alive(fd, cfg->keep_alive_cfg) < 0) {
                    WS_LOGE("WS-TLS", "Error setting keep-alive\n");
                    goto err_freesocket;
                }
            }
        }
        if (cfg->non_block) {
            int flags = fcntl(fd, F_GETFL, 0);
            ret = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
            if (ret < 0) {
                WS_LOGE("WS-TLS", "Failed to configure the socket as non-blocking (errno %d)\n", errno);
                goto err_freesocket;
            }
        }
    }

    ret = connect(fd, addr_ptr, addrinfo->ai_addrlen);
    if (ret < 0 && !(errno == EINPROGRESS && cfg && cfg->non_block)) {

        WS_LOGE("WS-TLS", "Failed to connnect to host (errno %d)\n", errno);
        WS_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ERR_TYPE_SYSTEM, errno);
        ret = WS_ERR_WS_TLS_FAILED_CONNECT_TO_HOST;
        goto err_freesocket;
    }

    *sockfd = fd;
    freeaddrinfo(addrinfo);
    return WS_OK;

err_freesocket:
    close(fd);
err_freeaddr:
    freeaddrinfo(addrinfo);
    return ret;
}

static int ws_tls_low_level_conn(const char *hostname, int hostlen, int port, const ws_tls_cfg_t *cfg, ws_tls_t *tls)
{
    if (!tls) {
        WS_LOGE("WS-TLS", "empty ws_tls parameter\n");
        return -1;
    }
    ws_err_t ws_ret;
    /* These states are used to keep a tab on connection progress in case of non-blocking connect,
    and in case of blocking connect these cases will get executed one after the other */
    switch (tls->conn_state) {
    case WS_TLS_INIT:
        tls->sockfd = -1;
        if (cfg != NULL) {
#ifdef CONFIG_WS_TLS_USING_MBEDTLS
            mbedtls_net_init(&tls->server_fd);
#endif
            tls->is_tls = true;
        }
        if ((ws_ret = ws_tcp_connect(hostname, hostlen, port, &tls->sockfd, tls, cfg)) != WS_OK) {
            WS_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ERR_TYPE_ESP, ws_ret);
            return -1;
        }
        if (!cfg) {
            tls->read_f = tcp_read;
            tls->write_f = tcp_write;
            WS_LOGD("WS-TLS", "non-tls connection established\n");
            return 1;
        }
        if (cfg->non_block) {
            FD_ZERO(&tls->rset);
            FD_SET((unsigned int)(tls->sockfd), &tls->rset);
            tls->wset = tls->rset;
        }
        tls->conn_state = WS_TLS_CONNECTING;
    /* falls through */
        //no break
    case WS_TLS_CONNECTING:
        if (cfg->non_block) {
            WS_LOGD("WS-TLS", "connecting...\n");
            struct timeval tv;
            ms_to_timeval(cfg->timeout_ms, &tv);

            /* In case of non-blocking I/O, we use the select() API to check whether
               connection has been established or not*/
            if (select(tls->sockfd + 1, &tls->rset, &tls->wset, NULL,
                       cfg->timeout_ms>0 ? &tv : NULL) == 0) {
                WS_LOGD("WS-TLS", "select() timed out\n");
                return 0;
            }
            if (FD_ISSET((unsigned int)(tls->sockfd), &tls->rset) || FD_ISSET((unsigned int)(tls->sockfd), &tls->wset)) {
                int error;
                unsigned int len = sizeof(error);
                /* pending error check */
                if (getsockopt(tls->sockfd, SOL_SOCKET, SO_ERROR, &error, (socklen_t *)&len) < 0) {
                    WS_LOGD("WS-TLS", "Non blocking connect failed\n");
                    WS_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ERR_TYPE_SYSTEM, errno);
                    WS_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ERR_TYPE_ESP, WS_ERR_WS_TLS_SOCKET_SETOPT_FAILED);
                    tls->conn_state = WS_TLS_FAIL;
                    return -1;
                }
            }
        }
        /* By now, the connection has been established */
        ws_ret = create_ssl_handle(hostname, (size_t)hostlen, cfg, tls);
        if (ws_ret != WS_OK) {
            WS_LOGE("WS-TLS", "create_ssl_handle failed\n");
            WS_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ERR_TYPE_ESP, ws_ret);
            tls->conn_state = WS_TLS_FAIL;
            return -1;
        }
        tls->read_f = _ws_tls_read;
        tls->write_f = _ws_tls_write;
        tls->conn_state = WS_TLS_HANDSHAKE;
    /* falls through */
        //no break
    case WS_TLS_HANDSHAKE:
        WS_LOGD("WS-TLS", "handshake in progress...\n");
        return ws_tls_handshake(tls, cfg);
        break;
    case WS_TLS_FAIL:
        WS_LOGE("WS-TLS", "failed to open a new connection\n");;
        break;
    default:
        WS_LOGE("WS-TLS", "invalid esp-tls state\n");
        break;
    }
    return -1;
}

/**
 * @brief      Create a new TLS/SSL connection
 */
ws_tls_t *ws_tls_conn_new(const char *hostname, int hostlen, int port, const ws_tls_cfg_t *cfg)
{
    ws_tls_t *tls = ws_tls_init();
    if (!tls) {
        return NULL;
    }
    /* ws_tls_conn_new() API establishes connection in a blocking manner thus this loop ensures that ws_tls_conn_new()
       API returns only after connection is established unless there is an error*/
    size_t start = xTaskGetTickCount();
    while (1) {
        int ret = ws_tls_low_level_conn(hostname, hostlen, port, cfg, tls);
        if (ret == 1) {
            return tls;
        } else if (ret == -1) {
            ws_tls_conn_delete(tls);
            WS_LOGE("WS-TLS", "Failed to open new connection\n");
            return NULL;
        } else if (ret == 0 && cfg->timeout_ms >= 0) {
            size_t timeout_ticks = portCONVERT_MS_2_TICKS(cfg->timeout_ms);
            uint32_t expired = xTaskGetTickCount() - start;
            if (expired >= timeout_ticks) {
                ws_tls_conn_delete(tls);
                WS_LOGE("WS-TLS", "Failed to open new connection in specified timeout\n");
                return NULL;
            }
        }
    }
    return NULL;
}

int ws_tls_conn_new_sync(const char *hostname, int hostlen, int port, const ws_tls_cfg_t *cfg, ws_tls_t *tls)
{
    /* ws_tls_conn_new_sync() is a sync alternative to ws_tls_conn_new_async() with symmetric function prototype
    it is an alternative to ws_tls_conn_new() which is left for compatibility reasons */
    size_t start = xTaskGetTickCount();
    while (1) {
        int ret = ws_tls_low_level_conn(hostname, hostlen, port, cfg, tls);
        if (ret == 1) {
            return ret;
        } else if (ret == -1) {
            WS_LOGE("WS-TLS", "Failed to open new connection\n");
            return -1;
        } else if (ret == 0 && cfg->timeout_ms >= 0) {
            size_t timeout_ticks = portCONVERT_MS_2_TICKS(cfg->timeout_ms);
            uint32_t expired = xTaskGetTickCount() - start;
            if (expired >= timeout_ticks) {
                WS_LOGW("WS-TLS", "Failed to open new connection in specified timeout\n");
                WS_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ERR_TYPE_ESP, WS_ERR_WS_TLS_CONNECTION_TIMEOUT);
                return 0;
            }
        }
    }
    return 0;
}

/*
 * @brief      Create a new TLS/SSL non-blocking connection
 */
int ws_tls_conn_new_async(const char *hostname, int hostlen, int port, const ws_tls_cfg_t *cfg, ws_tls_t *tls)
{
    return ws_tls_low_level_conn(hostname, hostlen, port, cfg, tls);
}

static int get_port(const char *url, struct http_parser_url *u)
{
    if (u->field_data[UF_PORT].len) {
        return strtol(&url[u->field_data[UF_PORT].off], NULL, 10);
    } else {
        if (strncasecmp(&url[u->field_data[UF_SCHEMA].off], "http", u->field_data[UF_SCHEMA].len) == 0) {
            return 80;
        } else if (strncasecmp(&url[u->field_data[UF_SCHEMA].off], "https", u->field_data[UF_SCHEMA].len) == 0) {
            return 443;
        }
    }
    return 0;
}

/**
 * @brief      Create a new TLS/SSL connection with a given "HTTP" url
 */
ws_tls_t *ws_tls_conn_http_new(const char *url, const ws_tls_cfg_t *cfg)
{
    /* Parse URI */
    struct http_parser_url u;
    http_parser_url_init(&u);
    http_parser_parse_url(url, strlen(url), 0, &u);
    ws_tls_t *tls = ws_tls_init();
    if (!tls) {
        return NULL;
    }
    /* Connect to host */
    if (ws_tls_conn_new_sync(&url[u.field_data[UF_HOST].off], u.field_data[UF_HOST].len,
                              get_port(url, &u), cfg, tls) == 1) {
        return tls;
    }
    ws_tls_conn_delete(tls);
    return NULL;
}

/**
 * @brief      Create a new non-blocking TLS/SSL connection with a given "HTTP" url
 */
int ws_tls_conn_http_new_async(const char *url, const ws_tls_cfg_t *cfg, ws_tls_t *tls)
{
    /* Parse URI */
    struct http_parser_url u;
    http_parser_url_init(&u);
    http_parser_parse_url(url, strlen(url), 0, &u);

    /* Connect to host */
    return ws_tls_conn_new_async(&url[u.field_data[UF_HOST].off], u.field_data[UF_HOST].len,
                                  get_port(url, &u), cfg, tls);
}

#ifdef CONFIG_WS_TLS_USING_MBEDTLS

mbedtls_x509_crt *ws_tls_get_global_ca_store(void)
{
    return _ws_tls_get_global_ca_store();
}

#endif /* CONFIG_WS_TLS_USING_MBEDTLS */
#ifdef CONFIG_WS_TLS_SERVER
/**
 * @brief      Create a server side TLS/SSL connection
 */
int ws_tls_server_session_create(ws_tls_cfg_server_t *cfg, int sockfd, ws_tls_t *tls)
{
    return _ws_tls_server_session_create(cfg, sockfd, tls);
}
/**
 * @brief      Close the server side TLS/SSL connection and free any allocated resources.
 */
void ws_tls_server_session_delete(ws_tls_t *tls)
{
    return _ws_tls_server_session_delete(tls);
}
#endif /* CONFIG_WS_TLS_SERVER */

ssize_t ws_tls_get_bytes_avail(ws_tls_t *tls)
{
    return _ws_tls_get_bytes_avail(tls);
}

ws_err_t ws_tls_get_conn_sockfd(ws_tls_t *tls, int *sockfd)
{
    if (!tls || !sockfd) {
        WS_LOGE("WS-TLS", "Invalid arguments passed\n");
        return WS_ERR_INVALID_ARG;
    }
    *sockfd = tls->sockfd;
    return WS_OK;
}

ws_err_t ws_tls_get_and_clear_last_error(ws_tls_error_handle_t h, int *ws_tls_code, int *ws_tls_flags)
{
    if (!h) {
        return WS_ERR_INVALID_STATE;
    }
    ws_err_t last_err = h->last_error;
    if (ws_tls_code) {
        *ws_tls_code = h->ws_tls_error_code;
    }
    if (ws_tls_flags) {
        *ws_tls_flags = h->ws_tls_flags;
    }
    memset(h, 0, sizeof(ws_tls_last_error_t));
    return last_err;
}

ws_err_t ws_tls_init_global_ca_store(void)
{
    return _ws_tls_init_global_ca_store();
}

ws_err_t ws_tls_set_global_ca_store(const unsigned char *cacert_pem_buf, const unsigned int cacert_pem_bytes)
{
    return _ws_tls_set_global_ca_store(cacert_pem_buf, cacert_pem_bytes);
}

void ws_tls_free_global_ca_store(void)
{
    return _ws_tls_free_global_ca_store();
}

#endif // __SUPPORT_WEBSOCKET_CLIENT__

/* EOF */
