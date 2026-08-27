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

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "transport.h"
#include "transport_utils.h"
#include "ws_log.h"
#include "ws_err.h"
#include "ws_mem.h"
#include "websocket_client.h"
#include "rm_lwip_w_hooks.h"
//#include "rm_wifi_dpm.h"

#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"

extern int select_ipaddr_type_from_str(char* str, struct sockaddr_storage *ipaddr);
#if CFG_PMGR
extern void RM_WIFI_dpm_tcp_port_delete(unsigned short d_port);
extern fsp_err_t RM_PMGR_W_dpm_job_name_clear(char * dpm_name);
#endif /* CFG_PMGR */

typedef struct {
    int sock;
} transport_tcp_t;

static int resolve_dns(const char *host, struct sockaddr_storage *ip) 
{
    const struct addrinfo hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res;

    int err = getaddrinfo(host, NULL, &hints, &res);
    if(err != 0 || res == NULL) {
        WS_LOGE("TRANS_TCP", "DNS lookup failed err=%d res=%p\n", err, res);
        return WS_FAIL;
    }
    if (res->ai_family == AF_INET) {
#if defined ( __SUPPORT_IPV4__ ) 
        ip->ss_family= AF_INET;
        memcpy(&((struct sockaddr_in *)ip)->sin_addr, &((struct sockaddr_in *)(res->ai_addr))->sin_addr, sizeof(struct in_addr));
        freeaddrinfo(res);
        return IPADDR_TYPE_V4;
#endif // __SUPPORT_IPV4__
    }
#if defined ( __SUPPORT_IPV6__ )
    else if (res->ai_family == AF_INET6) {
        ip->ss_family = AF_INET6;
        memcpy(&((struct sockaddr_in6 *)ip)->sin6_addr, &((struct sockaddr_in6 *)(res->ai_addr))->sin6_addr, sizeof(struct in6_addr));
        freeaddrinfo(res);
        return IPADDR_TYPE_V6;
    }
#endif // __SUPPORT_IPV6__
    return WS_OK;
}

static int tcp_enable_keep_alive(int fd, ws_transport_keep_alive_t *keep_alive_cfg)
{
    int keep_alive_enable = 1;
    int keep_alive_idle = keep_alive_cfg->keep_alive_idle;
    int keep_alive_interval = keep_alive_cfg->keep_alive_interval;
    int keep_alive_count = keep_alive_cfg->keep_alive_count;

    WS_LOGD("TRANS_TCP", "Enable TCP keep alive. idle: %d, interval: %d, count: %d\n", keep_alive_idle, keep_alive_interval, keep_alive_count);
    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &keep_alive_enable, sizeof(keep_alive_enable)) != 0) {
        WS_LOGE("TRANS_TCP", "Fail to setsockopt SO_KEEPALIVE\n");
        return -1;
    }
    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &keep_alive_idle, sizeof(keep_alive_idle)) != 0) {
        WS_LOGE("TRANS_TCP", "Fail to setsockopt TCP_KEEPIDLE\n");
        return -1;
    }
    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &keep_alive_interval, sizeof(keep_alive_interval)) != 0) {
        WS_LOGE("TRANS_TCP", "Fail to setsockopt TCP_KEEPINTVL\n");
        return -1;
    }
    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &keep_alive_count, sizeof(keep_alive_count)) != 0) {
        WS_LOGE("TRANS_TCP", "Fail to setsockopt TCP_KEEPCNT\n");
        return -1;
    }

    return 0;
}

static int tcp_connect(ws_transport_handle_t t, const char *host, int port, int timeout_ms)
{
    struct timeval tv = { 0 };
    transport_tcp_t *tcp = ws_transport_get_context_data(t);
    ws_transport_keep_alive_t *keep_alive_cfg = ws_transport_get_keep_alive(t);

    int ip_type = 0;
    struct sockaddr_storage ipaddr;
    
#if defined ( __SUPPORT_IPV4__ )
    struct sockaddr_in remote_ip;
    bzero(&remote_ip, sizeof(struct sockaddr_in));
#endif // __SUPPORT_IPV4__

#if defined ( __SUPPORT_IPV6__ )
    struct sockaddr_in6 remote_ip6;
    bzero(&remote_ip6, sizeof(struct sockaddr_in6));
#endif // __SUPPORT_IPV6__

    ip_type = select_ipaddr_type_from_str((char *)host, &ipaddr);

    if ( ip_type == IPADDR_TYPE_V4 ) {
#if defined ( __SUPPORT_IPV4__ )
        memcpy(&remote_ip, (struct sockaddr_in *)&ipaddr, sizeof (remote_ip));
#endif // __SUPPORT_IPV4__
    } else if ( ip_type == IPADDR_TYPE_V6 ) {
#if defined ( __SUPPORT_IPV6__ )
        memcpy(&remote_ip6, (struct sockaddr_in6 *)&ipaddr, sizeof (remote_ip6));
#endif // __SUPPORT_IPV6__
    } else {
        ip_type = resolve_dns(host, &ipaddr);
        if ( ip_type == IPADDR_TYPE_V4 ) {
#if defined ( __SUPPORT_IPV4__ )
            memcpy(&remote_ip, (struct sockaddr_in *)&ipaddr, sizeof (remote_ip));
#endif // __SUPPORT_IPV4__
        } else if ( ip_type == IPADDR_TYPE_V6 ) {
#if defined ( __SUPPORT_IPV6__ )
            memcpy(&remote_ip6, (struct sockaddr_in6 *)&ipaddr, sizeof (remote_ip6));
#endif // __SUPPORT_IPV6__
        } else {
            WS_LOGE("TRANS_TCP", "Error IP address/Host name is invalid. \n");
            return -1;
        }
    }

    if ( ip_type == IPADDR_TYPE_V4 ) {
#if defined ( __SUPPORT_IPV4__ )
#if CFG_PMGR
        if (RM_PMGR_W_dpm_is_enabled()) {
            tcp->sock = socket_dpm("websocket", PF_INET, SOCK_STREAM, 0);
        } 
        else 
#endif /* CFG_PMGR */
        {
            tcp->sock = socket(PF_INET, SOCK_STREAM, 0);
        }
#endif // __SUPPORT_IPV4__
    } else if ( ip_type == IPADDR_TYPE_V6 ) {
#if defined ( __SUPPORT_IPV6__ )
#if CFG_PMGR
	    if (RM_PMGR_W_dpm_is_enabled()) {
            tcp->sock = socket_dpm("websocket", PF_INET6, SOCK_STREAM, 0);
        } 
        else 
#endif /* CFG_PMGR */
        {
            tcp->sock = socket(PF_INET6, SOCK_STREAM, 0);
        }
#endif // __SUPPORT_IPV6__
    }
    
    if (tcp->sock < 0) {
        WS_LOGE("TRANS_TCP", "Error create socket\n");
        return -1;
    }

    if ( ip_type == IPADDR_TYPE_V4 ) {
#if defined ( __SUPPORT_IPV4__ )
        remote_ip.sin_family = AF_INET;
        remote_ip.sin_port = htons((u16_t)port);
#endif // __SUPPORT_IPV4__
    } else if ( ip_type == IPADDR_TYPE_V6 ) {
#if defined ( __SUPPORT_IPV6__ )
        remote_ip6.sin6_family = AF_INET6;
        remote_ip6.sin6_port = htons((u16_t)port);
#endif // __SUPPORT_IPV6__
    }

    ws_transport_utils_ms_to_timeval(timeout_ms, &tv); // if timeout=-1, tv is unchanged, 0, i.e. waits forever

    setsockopt(tcp->sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(tcp->sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    // Set socket keep-alive option
    if (keep_alive_cfg && keep_alive_cfg->keep_alive_enable) {
        if (tcp_enable_keep_alive(tcp->sock, keep_alive_cfg) < 0) {
            WS_LOGE("TRANS_TCP", "Error to set tcp [socket=%d] keep-alive\n", tcp->sock);
            close(tcp->sock);
            tcp->sock = -1;
            return -1;
        }
    }
    // Set socket to non-blocking
    int flags;
    if ((flags = fcntl(tcp->sock, F_GETFL, 0)) < 0) {
        WS_LOGE("TRANS_TCP", "[sock=%d] get file flags error: %s\n", tcp->sock, strerror(errno));
        goto error;
    }
    if (fcntl(tcp->sock, F_SETFL, flags |= O_NONBLOCK) < 0) {
        WS_LOGE("TRANS_TCP", "[sock=%d] set nonblocking error: %s\n", tcp->sock, strerror(errno));
        goto error;
    }

    if ( ip_type == IPADDR_TYPE_V4 ) {
#if defined ( __SUPPORT_IPV4__ )
        WS_LOGD("TRANS_TCP", "[sock=%d] Connecting to server. IPv4: %s, Port: %d\n",
            tcp->sock, ipaddr_ntoa((const ip_addr_t*)&remote_ip.sin_addr.s_addr), port);
        
        if (connect(tcp->sock, (struct sockaddr *)(&remote_ip), sizeof(struct sockaddr)) < 0) {
            if (errno == EINPROGRESS) {
                fd_set fdset;

                ws_transport_utils_ms_to_timeval(timeout_ms, &tv);
                FD_ZERO(&fdset);
                FD_SET((unsigned int)(tcp->sock), &fdset);

                int res = select(tcp->sock+1, NULL, &fdset, NULL, &tv);
                if (res < 0) {
                    WS_LOGE("TRANS_TCP", "[sock=%d] select() error: %s\n", tcp->sock, strerror(errno));
                    goto error;
                }
                else if (res == 0) {
                    WS_LOGE("TRANS_TCP", "[sock=%d] select() timeout\n", tcp->sock);
                    goto error;
                } else {
                    int sockerr;
                    socklen_t len = (socklen_t)sizeof(int);

                    if (getsockopt(tcp->sock, SOL_SOCKET, SO_ERROR, (void*)(&sockerr), &len) < 0) {
                        WS_LOGE("TRANS_TCP", "[sock=%d] getsockopt() error: %s\n", tcp->sock, strerror(errno));
                        goto error;
                    }
                    else if (sockerr) {
                        WS_LOGE("TRANS_TCP", "[sock=%d] delayed connect error: %s\n", tcp->sock, strerror(sockerr));
                        goto error;
                    }
                }
            } else {
                WS_LOGE("TRANS_TCP", "[sock=%d] connect() error: %s\n", tcp->sock, strerror(errno));
                goto error;
            }
        }
#endif // __SUPPORT_IPV4__
    } else if ( ip_type == IPADDR_TYPE_V6 ) {    
#if defined ( __SUPPORT_IPV6__ )
        WS_LOGD("TRANS_TCP", "[sock=%d] Connecting to server. IPv6: %s, Port: %d\n",
            tcp->sock, ip6addr_ntoa((const ip_addr_t*)&remote_ip6.sin6_addr.s6_addr), port);
        if (connect(tcp->sock, (struct sockaddr *)(&remote_ip6), sizeof(struct sockaddr_in6)) < 0) {
            if (errno == EINPROGRESS) {
                fd_set fdset;

                ws_transport_utils_ms_to_timeval(timeout_ms, &tv);
                FD_ZERO(&fdset);
                FD_SET((unsigned int)(tcp->sock), &fdset);

                int res = select(tcp->sock+1, NULL, &fdset, NULL, &tv);
                if (res < 0) {
                    WS_LOGE("TRANS_TCP", "[sock=%d] select() error: %s\n", tcp->sock, strerror(errno));
                    goto error;
                }
                else if (res == 0) {
                    WS_LOGE("TRANS_TCP", "[sock=%d] select() timeout\n", tcp->sock);
                    goto error;
                } else {
                    int sockerr;
                    socklen_t len = (socklen_t)sizeof(int);

                    if (getsockopt(tcp->sock, SOL_SOCKET, SO_ERROR, (void*)(&sockerr), &len) < 0) {
                        WS_LOGE("TRANS_TCP", "[sock=%d] getsockopt() error: %s\n", tcp->sock, strerror(errno));
                        goto error;
                    }
                    else if (sockerr) {
                        WS_LOGE("TRANS_TCP", "[sock=%d] delayed connect error: %s\n", tcp->sock, strerror(sockerr));
                        goto error;
                    }
                }
            } else {
                WS_LOGE("TRANS_TCP", "[sock=%d] connect() error: %s\n", tcp->sock, strerror(errno));
                goto error;
            }
        }
#endif // __SUPPORT_IPV6__
    }
    // Reset socket to blocking
    if ((flags = fcntl(tcp->sock, F_GETFL, 0)) < 0) {
        WS_LOGE("TRANS_TCP", "[sock=%d] get file flags error: %s\n", tcp->sock, strerror(errno));
        goto error;
    }
    if (fcntl(tcp->sock, F_SETFL, flags & ~O_NONBLOCK) < 0) {
        WS_LOGE("TRANS_TCP", "[sock=%d] reset blocking error: %s\n", tcp->sock, strerror(errno));
        goto error;
    }
    return tcp->sock;
error:
    close(tcp->sock);
    tcp->sock = -1;
    return -1;
}

static int tcp_write(ws_transport_handle_t t, const char *buffer, int len, int timeout_ms)
{
    int poll;
    transport_tcp_t *tcp = ws_transport_get_context_data(t);
    if ((poll = ws_transport_poll_write(t, timeout_ms)) <= 0) {
        return poll;
    }
    return write(tcp->sock, buffer, (size_t)len);
}

static int tcp_read(ws_transport_handle_t t, char *buffer, int len, int timeout_ms)
{
    transport_tcp_t *tcp = ws_transport_get_context_data(t);
    int poll = -1;
    if ((poll = ws_transport_poll_read(t, timeout_ms)) <= 0) {
        return poll;
    }
    int read_len = read(tcp->sock, buffer, (size_t)len);
    if (read_len == 0) {
        return -1;
    }
    return read_len;
}

static int tcp_poll_read(ws_transport_handle_t t, int timeout_ms)
{
    transport_tcp_t *tcp = ws_transport_get_context_data(t);
    int ret = -1;
    struct timeval timeout;
    fd_set readset;
    fd_set errset;
    FD_ZERO(&readset);
    FD_ZERO(&errset);
    FD_SET((unsigned int)(tcp->sock), &readset);
    FD_SET((unsigned int)(tcp->sock), &errset);

    ret = select(tcp->sock + 1, &readset, NULL, &errset, ws_transport_utils_ms_to_timeval(timeout_ms, &timeout));
    if (ret > 0 && FD_ISSET((unsigned int)(tcp->sock), &errset)) {
        int sock_errno = 0;
        uint32_t optlen = sizeof(sock_errno);
        getsockopt(tcp->sock, SOL_SOCKET, SO_ERROR, &sock_errno, &optlen);
        WS_LOGE("TRANS_TCP", "tcp_poll_read select error %d, errno = %s, fd = %d\n", sock_errno, strerror(sock_errno), tcp->sock);
        ret = -1;
    }
    return ret;
}

static int tcp_poll_write(ws_transport_handle_t t, int timeout_ms)
{
    transport_tcp_t *tcp = ws_transport_get_context_data(t);
    int ret = -1;
    struct timeval timeout;
    fd_set writeset;
    fd_set errset;
    FD_ZERO(&writeset);
    FD_ZERO(&errset);
    FD_SET((unsigned int)(tcp->sock), &writeset);
    FD_SET((unsigned int)(tcp->sock), &errset);

    ret = select(tcp->sock + 1, NULL, &writeset, &errset, ws_transport_utils_ms_to_timeval(timeout_ms, &timeout));
    if (ret > 0 && FD_ISSET((unsigned int)(tcp->sock), &errset)) {
        int sock_errno = 0;
        uint32_t optlen = sizeof(sock_errno);
        getsockopt(tcp->sock, SOL_SOCKET, SO_ERROR, &sock_errno, &optlen);
        WS_LOGE("TRANS_TCP", "tcp_poll_write select error %d, errno = %s, fd = %d\n", sock_errno, strerror(sock_errno), tcp->sock);
        ret = -1;
    }
    return ret;
}

static int tcp_close(ws_transport_handle_t t)
{
    transport_tcp_t *tcp = ws_transport_get_context_data(t);
    int ret = -1;
    if (tcp->sock >= 0) {
        ret = close(tcp->sock);
        tcp->sock = -1;
    }
    return ret;
}

static int tcp_destroy(ws_transport_handle_t t)
{
    transport_tcp_t *tcp = ws_transport_get_context_data(t);
#if CFG_PMGR
    if (RM_PMGR_W_dpm_is_enabled()) {
        RM_WIFI_dpm_tcp_port_delete(tcp->sock);
        RM_PMGR_W_dpm_job_name_clear(APP_WEBSOCKET_CLIENT);
    }
#endif /* CFG_PMGR */
    ws_transport_close(t);
    _ws_free(tcp);
    return 0;
}

void ws_transport_tcp_set_keep_alive(ws_transport_handle_t t, ws_transport_keep_alive_t *keep_alive_cfg)
{
    ws_transport_set_keep_alive(t, keep_alive_cfg);
}

ws_transport_handle_t ws_transport_tcp_init(void)
{
    ws_transport_handle_t t = ws_transport_init();
    transport_tcp_t *tcp = _ws_calloc(1, sizeof(transport_tcp_t));
    WS_TRANSPORT_MEM_CHECK("TRANS_TCP", tcp, {
        ws_transport_destroy(t);
	return NULL;
    });
    tcp->sock = -1;
    ws_transport_set_func(t, tcp_connect, tcp_read, tcp_write, tcp_close, tcp_poll_read, tcp_poll_write, tcp_destroy);
    ws_transport_set_context_data(t, tcp);

    return t;
}

#endif // __SUPPORT_WEBSOCKET_CLIENT__

/* EOF */
