/**
 * @file
 * HTTP client
 */

/*
 * Copyright (c) 2018 Simon Goldschmidt <goldsimon@gmx.de>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Simon Goldschmidt <goldsimon@gmx.de>
 */

/**
 * @todo:
 * - persistent connections
 * - select outgoing http version
 * - optionally follow redirect
 * - check request uri for invalid characters? (e.g. encode spaces)
 * - IPv6 support
 */

#include "rm_http_client.h"

#include "lwip/altcp_tcp.h"
#include "lwip/dns.h"
#include "lwip/debug.h"
#include "lwip/mem.h"
#include "lwip/altcp_tls.h"
#include "lwip/init.h"
#if CFG_MBEDTLS
 #include "common.h"
 #include "mbedtls/base64.h"
 #include "mbedtls/ssl.h"
#endif                                 /* CFG_MBEDTLS */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if LWIP_TCP && LWIP_CALLBACK_API

/** Set this to 1 to keep server name and uri in request state */
 #ifndef HTTPC_DEBUG_REQUEST
  #define HTTPC_DEBUG_REQUEST    0
 #endif

/** When defined, the TX buffer is allocated from the heap. */
 #define HTTPC_BUF_USE_HEAP

/** This string is passed in the HTTP header as "User-Agent: " */
 #ifndef HTTPC_CLIENT_AGENT
  #define HTTPC_CLIENT_AGENT    "lwIP/" LWIP_VERSION_STRING " (http://savannah.nongnu.org/projects/lwip)"
 #endif

/** Enable the TLS function of http-client */
 #ifndef HTTPC_ENABLE_HTTPS
  #if CFG_MBEDTLS
   #define HTTPC_ENABLE_HTTPS
  #endif
 #endif

/* the various debug levels for this file */
 #define HTTPC_DEBUG_TRACE          (HTTPC_DEBUG | LWIP_DBG_TRACE)
 #define HTTPC_DEBUG_STATE          (HTTPC_DEBUG | LWIP_DBG_STATE)
 #define HTTPC_DEBUG_WARN           (HTTPC_DEBUG | LWIP_DBG_LEVEL_WARNING)
 #define HTTPC_DEBUG_WARN_STATE     (HTTPC_DEBUG | LWIP_DBG_LEVEL_WARNING | LWIP_DBG_STATE)
 #define HTTPC_DEBUG_SERIOUS        (HTTPC_DEBUG | LWIP_DBG_LEVEL_SERIOUS)

 #define HTTPC_POLL_INTERVAL        1                       /* 500ms */
 #define HTTPC_POLL_TIMEOUT         60                      /* 30sec */

/* GET request basic */
 #define HTTPC_REQ_11               "%s %s HTTP/1.1\r\n"    /* URI */                                          \
                                    "User-Agent: %s\r\n"    /* User-Agent */                                   \
                                    "Accept: */*\r\n"                                                          \
                                    "Connection: Close\r\n" /* we don't support persistent connections, yet */ \
                                    "\r\n"
 #define HTTPC_REQ_11_FORMAT(method, uri)                                          HTTPC_REQ_11, method, uri, \
    HTTPC_CLIENT_AGENT

/* GET request with host */
 #define HTTPC_REQ_11_HOST          "%s %s HTTP/1.1\r\n"    /* URI */                                          \
                                    "User-Agent: %s\r\n"    /* User-Agent */                                   \
                                    "Accept: */*\r\n"                                                          \
                                    "Host: %s\r\n"          /* server name */                                  \
                                    "Connection: Close\r\n" /* we don't support persistent connections, yet */ \
                                    "\r\n"
 #define HTTPC_REQ_11_HOST_FORMAT(method, uri, srv_name)                           HTTPC_REQ_11_HOST, method, uri, \
    HTTPC_CLIENT_AGENT, srv_name

/* GET request with proxy */
 #define HTTPC_REQ_11_PROXY         "%s http://%s%s HTTP/1.1\r\n" /* HOST, URI */                                    \
                                    "User-Agent: %s\r\n"          /* User-Agent */                                   \
                                    "Accept: */*\r\n"                                                                \
                                    "Host: %s\r\n"                /* server name */                                  \
                                    "Connection: Close\r\n"       /* we don't support persistent connections, yet */ \
                                    "\r\n"
 #define HTTPC_REQ_11_PROXY_FORMAT(method, host, uri, srv_name)                    HTTPC_REQ_11_PROXY, method, host, \
    uri, HTTPC_CLIENT_AGENT, srv_name

/* GET request with proxy (non-default server port) */
 #define HTTPC_REQ_11_PROXY_PORT    "%s http://%s:%d%s HTTP/1.1\r\n" /* HOST, host-port, URI */                         \
                                    "User-Agent: %s\r\n"             /* User-Agent */                                   \
                                    "Accept: */*\r\n"                                                                   \
                                    "Host: %s\r\n"                   /* server name */                                  \
                                    "Connection: Close\r\n"          /* we don't support persistent connections, yet */ \
                                    "\r\n"
 #define HTTPC_REQ_11_PROXY_PORT_FORMAT(method, host, host_port, uri, srv_name)    HTTPC_REQ_11_PROXY_PORT, method, \
    host, host_port, uri, HTTPC_CLIENT_AGENT, srv_name

// Add -20220118//////////////////////////////////////////////////////////////////////////////////////////
// Authorization: Basic

/* GET request basic */
 #define HTTPC_REQ_11_AUTH               "%s %s HTTP/1.1\r\n"    /* URI */                                          \
                                         "User-Agent: %s\r\n"    /* User-Agent */                                   \
                                         "Accept: */*\r\n"                                                          \
                                         "Authorization: Basic %s\r\n"                                              \
                                         "Connection: Close\r\n" /* we don't support persistent connections, yet */ \
                                         "\r\n"
 #define HTTPC_REQ_11_AUTH_FORMAT(method, uri, auth_basic)    HTTPC_REQ_11_AUTH, \
    method, uri, HTTPC_CLIENT_AGENT, auth_basic

/* GET request with host */
 #define HTTPC_REQ_11_HOST_AUTH          "%s %s HTTP/1.1\r\n"    /* URI */                                          \
                                         "User-Agent: %s\r\n"    /* User-Agent */                                   \
                                         "Accept: */*\r\n"                                                          \
                                         "Host: %s\r\n"          /* server name */                                  \
                                         "Authorization: Basic %s\r\n"                                              \
                                         "Connection: Close\r\n" /* we don't support persistent connections, yet */ \
                                         "\r\n"
 #define HTTPC_REQ_11_HOST_AUTH_FORMAT(method, uri, srv_name,                                                          \
                                       auth_basic)            HTTPC_REQ_11_HOST_AUTH, method, uri, HTTPC_CLIENT_AGENT, \
    srv_name, auth_basic

/* GET request with proxy */
 #define HTTPC_REQ_11_PROXY_AUTH         "%s http://%s%s HTTP/1.1\r\n" /* HOST, URI */                                    \
                                         "User-Agent: %s\r\n"          /* User-Agent */                                   \
                                         "Accept: */*\r\n"                                                                \
                                         "Host: %s\r\n"                /* server name */                                  \
                                         "Authorization: Basic %s\r\n"                                                    \
                                         "Connection: Close\r\n"       /* we don't support persistent connections, yet */ \
                                         "\r\n"
 #define HTTPC_REQ_11_PROXY_AUTH_FORMAT(method, host, uri, srv_name,                                      \
                                        auth_basic)           HTTPC_REQ_11_PROXY_AUTH, method, host, uri, \
    HTTPC_CLIENT_AGENT, srv_name, auth_basic

/* GET request with proxy (non-default server port) */
 #define HTTPC_REQ_11_PROXY_PORT_AUTH    "%s http://%s:%d%s HTTP/1.1\r\n" /* HOST, host-port, URI */                         \
                                         "User-Agent: %s\r\n"             /* User-Agent */                                   \
                                         "Accept: */*\r\n"                                                                   \
                                         "Host: %s\r\n"                   /* server name */                                  \
                                         "Authorization: Basic %s\r\n"                                                       \
                                         "Connection: Close\r\n"          /* we don't support persistent connections, yet */ \
                                         "\r\n"
 #define HTTPC_REQ_11_PROXY_PORT_AUTH_FORMAT(method, host, host_port, uri, srv_name,                                 \
                                             auth_basic)      HTTPC_REQ_11_PROXY_PORT_AUTH, method, host, host_port, \
    uri, HTTPC_CLIENT_AGENT, srv_name, auth_basic

/* PUT/POST request basic */
 #define HTTPC_POST_REQ_11               "%s %s HTTP/1.1\r\n"    /* URI */                                          \
                                         "User-Agent: %s\r\n"    /* User-Agent */                                   \
                                         "Accept: */*\r\n"                                                          \
                                         "Connection: Close\r\n" /* we don't support persistent connections, yet */ \
                                         "Content-Length: %d\r\n"                                                   \
                                         "\r\n"                                                                     \
                                         "%s\r\n"
 #define HTTPC_POST_REQ_11_FORMAT(method, uri, data, data_len) \
    HTTPC_POST_REQ_11, method, uri, HTTPC_CLIENT_AGENT, data_len, data

/* PUT/POST request with host */
 #define HTTPC_POST_REQ_11_HOST          "%s %s HTTP/1.1\r\n"    /* URI */                                          \
                                         "User-Agent: %s\r\n"    /* User-Agent */                                   \
                                         "Accept: */*\r\n"                                                          \
                                         "Host: %s\r\n"          /* server name */                                  \
                                         "Connection: Close\r\n" /* we don't support persistent connections, yet */ \
                                         "Content-Length: %d\r\n"                                                   \
                                         "\r\n"                                                                     \
                                         "%s\r\n"

 #define HTTPC_POST_REQ_11_HOST_FORMAT(method, uri, srv_name, data, data_len) \
    HTTPC_POST_REQ_11_HOST, method, uri, HTTPC_CLIENT_AGENT, srv_name, data_len, data

/* PUT/POST request with proxy */
 #define HTTPC_POST_REQ_11_PROXY         "%s http://%s%s HTTP/1.1\r\n" /* HOST, URI */                                    \
                                         "User-Agent: %s\r\n"          /* User-Agent */                                   \
                                         "Accept: */*\r\n"                                                                \
                                         "Host: %s\r\n"                /* server name */                                  \
                                         "Connection: Close\r\n"       /* we don't support persistent connections, yet */ \
                                         "Content-Length: %d\r\n"                                                         \
                                         "\r\n"                                                                           \
                                         "%s\r\n"
 #define HTTPC_POST_REQ_11_PROXY_FORMAT(method, host, uri, srv_name, data, data_len) \
    HTTPC_POST_REQ_11_PROXY, method, host, uri, HTTPC_CLIENT_AGENT, srv_name, data_len, data

/* PUT/POST request with proxy (non-default server port) */
 #define HTTPC_POST_REQ_11_PROXY_PORT    "%s http://%s:%d%s HTTP/1.1\r\n" /* HOST, host-port, URI */                         \
                                         "User-Agent: %s\r\n"             /* User-Agent */                                   \
                                         "Accept: */*\r\n"                                                                   \
                                         "Host: %s\r\n"                   /* server name */                                  \
                                         "Connection: Close\r\n"          /* we don't support persistent connections, yet */ \
                                         "Content-Length: %d\r\n"                                                            \
                                         "\r\n"                                                                              \
                                         "%s\r\n"
 #define HTTPC_POST_REQ_11_PROXY_PORT_FORMAT(method, host, host_port, uri, srv_name, data, data_len) \
    HTTPC_POST_REQ_11_PROXY_PORT, method, host, host_port, uri, HTTPC_CLIENT_AGENT, srv_name, data_len, data

// Authorization: Basic

/* PUT/POST request basic */
 #define HTTPC_POST_REQ_11_AUTH               "%s %s HTTP/1.1\r\n"    /* URI */                                          \
                                              "User-Agent: %s\r\n"    /* User-Agent */                                   \
                                              "Accept: */*\r\n"                                                          \
                                              "Connection: Close\r\n" /* we don't support persistent connections, yet */ \
                                              "Content-Length: %d\r\n"                                                   \
                                              "Authorization: Basic %s\r\n"                                              \
                                              "\r\n"                                                                     \
                                              "%s\r\n"
 #define HTTPC_POST_REQ_11_AUTH_FORMAT(method, uri, auth_basic, data, data_len) \
    HTTPC_POST_REQ_11_AUTH, method, uri, HTTPC_CLIENT_AGENT, data_len, auth_basic, data

/* PUT/POST request with host */
 #define HTTPC_POST_REQ_11_HOST_AUTH          "%s %s HTTP/1.1\r\n"    /* URI */                                          \
                                              "User-Agent: %s\r\n"    /* User-Agent */                                   \
                                              "Accept: */*\r\n"                                                          \
                                              "Host: %s\r\n"          /* server name */                                  \
                                              "Connection: Close\r\n" /* we don't support persistent connections, yet */ \
                                              "Content-Length: %d\r\n"                                                   \
                                              "Authorization: Basic %s\r\n"                                              \
                                              "\r\n"                                                                     \
                                              "%s\r\n"

 #define HTTPC_POST_REQ_11_HOST_AUTH_FORMAT(method, uri, srv_name, auth_basic, data, data_len) \
    HTTPC_POST_REQ_11_HOST_AUTH, method, uri, HTTPC_CLIENT_AGENT, srv_name, data_len, auth_basic, data

/* PUT/POST request with proxy */
 #define HTTPC_POST_REQ_11_PROXY_AUTH         "%s http://%s%s HTTP/1.1\r\n" /* HOST, URI */                                    \
                                              "User-Agent: %s\r\n"          /* User-Agent */                                   \
                                              "Accept: */*\r\n"                                                                \
                                              "Host: %s\r\n"                /* server name */                                  \
                                              "Connection: Close\r\n"       /* we don't support persistent connections, yet */ \
                                              "Content-Length: %d\r\n"                                                         \
                                              "Authorization: Basic %s\r\n"                                                    \
                                              "\r\n"                                                                           \
                                              "%s\r\n"
 #define HTTPC_POST_REQ_11_PROXY_AUTH_FORMAT(method, host, uri, srv_name, auth_basic, data, data_len) \
    HTTPC_POST_REQ_11_PROXY_AUTH, method, host, uri, HTTPC_CLIENT_AGENT, srv_name, data_len, auth_basic, data

/* PUT/POST request with proxy (non-default server port) */
 #define HTTPC_POST_REQ_11_PROXY_PORT_AUTH    "%s http://%s:%d%s HTTP/1.1\r\n" /* HOST, host-port, URI */                         \
                                              "User-Agent: %s\r\n"             /* User-Agent */                                   \
                                              "Accept: */*\r\n"                                                                   \
                                              "Host: %s\r\n"                   /* server name */                                  \
                                              "Connection: Close\r\n"          /* we don't support persistent connections, yet */ \
                                              "Content-Length: %d\r\n"                                                            \
                                              "Authorization: Basic %s\r\n"                                                       \
                                              "\r\n"                                                                              \
                                              "%s\r\n"
 #define HTTPC_POST_REQ_11_PROXY_PORT_AUTH_FORMAT(method, host, host_port, uri, srv_name, auth_basic, data, data_len)    \
    HTTPC_POST_REQ_11_PROXY_PORT_AUTH, method, host, host_port, uri, HTTPC_CLIENT_AGENT, srv_name, data_len, auth_basic, \
    data

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef enum ehttpc_parse_state
{
    HTTPC_PARSE_WAIT_FIRST_LINE = 0,
    HTTPC_PARSE_WAIT_HEADERS,
    HTTPC_PARSE_RX_DATA
} httpc_parse_state_t;

typedef struct _httpc_state
{
    struct altcp_pcb         * pcb;
    ip_addr_t                  remote_addr;
    u16_t                      remote_port;
    int                        timeout_ticks;
    struct pbuf              * request;
    struct pbuf              * rx_hdrs;
    u16_t                      rx_http_version;
    u16_t                      rx_status;
    altcp_recv_fn              recv_fn;
    const httpc_connection_t * conn_settings;
    void                     * callback_arg;
    u32_t                      rx_content_len;
    u32_t                      hdr_content_len;
    httpc_parse_state_t        parse_state;
 #if HTTPC_DEBUG_REQUEST
    char * server_name;
    char * uri;
 #endif
    struct altcp_tls_config * tls_config;
} httpc_state_t;

u32_t prev_rx_content_len = 0;

 #ifdef HTTPC_ENABLE_HTTPS
static void httpsc_free_state (httpc_state_t * req)
{
    if (req && req->tls_config)
    {
        altcp_tls_free_config(req->tls_config);
        req->tls_config = NULL;

        altcp_tls_free_entropy();
    }
}

 #endif                                // HTTPC_ENABLE_HTTPS

/** Free http client state and deallocate all resources within */
static err_t httpc_free_state (httpc_state_t * req)
{
    struct altcp_pcb * tpcb;

    if (req->request != NULL)
    {
        pbuf_free(req->request);
        req->request = NULL;
    }

    if (req->rx_hdrs != NULL)
    {
        pbuf_free(req->rx_hdrs);
        req->rx_hdrs = NULL;
    }

    tpcb = req->pcb;
    mem_free(req);
    req = NULL;

    if (tpcb != NULL)
    {
        err_t r;
        altcp_arg(tpcb, NULL);
        altcp_recv(tpcb, NULL);
        altcp_err(tpcb, NULL);
        altcp_poll(tpcb, NULL, 0);
        altcp_sent(tpcb, NULL);
        r = altcp_close(tpcb);
        if (r != ERR_OK)
        {
            altcp_abort(tpcb);

            return ERR_ABRT;
        }
    }

    return ERR_OK;
}

/** Close the connection: call finished callback and free the state */
static err_t httpc_close (httpc_state_t * req, httpc_result_t result, u32_t server_response, err_t err)
{
    if (req != NULL)
    {
        if (req->conn_settings != NULL)
        {
            if (req->conn_settings->result_fn != NULL)
            {
                req->conn_settings->result_fn(req->callback_arg, result, req->rx_content_len, server_response, err);
            }

 #ifdef HTTPC_ENABLE_HTTPS
            if (req->conn_settings->insecure)
            {
                httpsc_free_state(req);
            }
 #endif                                // HTTPC_ENABLE_HTTPS
        }

        return httpc_free_state(req);
    }

    return ERR_OK;
}

/** Parse http header response line 1 */
static err_t http_parse_response_status (struct pbuf * p,
                                         u16_t       * http_version,
                                         u16_t       * http_status,
                                         u16_t       * http_status_str_offset)
{
    u16_t end1 = pbuf_memfind(p, "\r\n", 2, 0);
    if (end1 != 0xFFFF)
    {
        /* get parts of first line */
        u16_t space1, space2;
        space1 = pbuf_memfind(p, " ", 1, 0);
        if (space1 != 0xFFFF)
        {
            if ((pbuf_memcmp(p, 0, "HTTP/", 5) == 0) && (pbuf_get_at(p, 6) == '.'))
            {
                char   status_num[10];
                size_t status_num_len;

                /* parse http version */
                u16_t version = pbuf_get_at(p, 5) - '0';
                version     <<= 8;
                version       = (u16_t) (version | (pbuf_get_at(p, 7) - '0'));
                *http_version = version;

                /* parse http status number */
                space2 = pbuf_memfind(p, " ", 1, space1 + 1);
                if (space2 != 0xFFFF)
                {
                    *http_status_str_offset = space2 + 1;
                    status_num_len          = (size_t) (space2 - space1 - 1);
                }
                else
                {
                    status_num_len = (size_t) (end1 - space1 - 1);
                }

                memset(status_num, 0, sizeof(status_num));
                if (pbuf_copy_partial(p, status_num, (u16_t) status_num_len, space1 + 1) == status_num_len)
                {
                    int status = atoi(status_num);
                    if ((status > 0) && (status <= 0xFFFF))
                    {
                        *http_status = (u16_t) status;

                        return ERR_OK;
                    }
                }
            }
        }
    }

    return ERR_VAL;
}

/** Wait for all headers to be received, return its length and content-length (if available) */
static err_t http_wait_headers (struct pbuf * p, u32_t * content_length, u16_t * total_header_len)
{
    u16_t end1 = pbuf_memfind(p, "\r\n\r\n", 4, 0);
    if (end1 < (0xFFFF - 2))
    {
        /* all headers received */
        /* check if we have a content length (@todo: case insensitive?) */
        u16_t content_len_hdr;
        *content_length   = HTTPC_CONTENT_LEN_INVALID;
        *total_header_len = end1 + 4;

        content_len_hdr = pbuf_memfind(p, "Content-Length: ", 16, 0);
        if (content_len_hdr != 0xFFFF)
        {
            u16_t content_len_line_end = pbuf_memfind(p, "\r\n", 2, content_len_hdr);
            if (content_len_line_end != 0xFFFF)
            {
                char  content_len_num[16];
                u16_t content_len_num_len = (u16_t) (content_len_line_end - content_len_hdr - 16);
                memset(content_len_num, 0, sizeof(content_len_num));
                if (pbuf_copy_partial(p, content_len_num, content_len_num_len,
                                      content_len_hdr + 16) == content_len_num_len)
                {
                    int len = atoi(content_len_num);
                    if ((len >= 0) && ((u32_t) len < HTTPC_CONTENT_LEN_INVALID))
                    {
                        *content_length = (u32_t) len;
                    }
                }
            }
        }

        return ERR_OK;
    }

    return ERR_VAL;
}

/** http client tcp recv callback */
static err_t httpc_tcp_recv (void * arg, struct altcp_pcb * pcb, struct pbuf * p, err_t r)
{
    httpc_state_t * req = (httpc_state_t *) arg;
    LWIP_UNUSED_ARG(r);

    if (p == NULL)
    {
        httpc_result_t result;
        if (req->parse_state != HTTPC_PARSE_RX_DATA)
        {
            /* did not get RX data yet */
            result = HTTPC_RESULT_ERR_CLOSED;
        }
        else if ((req->hdr_content_len != HTTPC_CONTENT_LEN_INVALID) &&
                 (req->hdr_content_len != req->rx_content_len))
        {
            /* header has been received with content length but not all data received */
            if (strcmp((char *) (req->conn_settings->method), "HEAD") == 0)
            {
                result = HTTPC_RESULT_OK;
            }
            else
            {
                result = HTTPC_RESULT_ERR_CONTENT_LEN;
            }
        }
        else
        {
            /* receiving data and either all data received or no content length header */
            result = HTTPC_RESULT_OK;
        }

        return httpc_close(req, result, req->rx_status, ERR_OK);
    }

    if (req->parse_state != HTTPC_PARSE_RX_DATA)
    {
        if (req->rx_hdrs == NULL)
        {
            req->rx_hdrs = p;
        }
        else
        {
            pbuf_cat(req->rx_hdrs, p);
        }

        if (req->parse_state == HTTPC_PARSE_WAIT_FIRST_LINE)
        {
            u16_t status_str_off;
            err_t err =
                http_parse_response_status(req->rx_hdrs, &req->rx_http_version, &req->rx_status, &status_str_off);
            if (err == ERR_OK)
            {
                /* don't care status string */
                req->parse_state = HTTPC_PARSE_WAIT_HEADERS;
            }
        }

        if (req->parse_state == HTTPC_PARSE_WAIT_HEADERS)
        {
            u16_t total_header_len;
            err_t err = http_wait_headers(req->rx_hdrs, &req->hdr_content_len, &total_header_len);
            if (err == ERR_OK)
            {
                struct pbuf * q;

                /* full header received, send window update for header bytes and call into client callback */

                if (!req->conn_settings->insecure)
                {
                    altcp_recved(pcb, total_header_len);
                }

                if (req->conn_settings)
                {
                    if (req->conn_settings->headers_done_fn)
                    {
                        err = req->conn_settings->headers_done_fn(req,
                                                                  req->callback_arg,
                                                                  req->rx_hdrs,
                                                                  total_header_len,
                                                                  req->hdr_content_len);
                        if (err != ERR_OK)
                        {
                            return httpc_close(req, HTTPC_RESULT_LOCAL_ABORT, req->rx_status, err);
                        }
                    }
                }

                /* hide header bytes in pbuf */
                q            = pbuf_free_header(req->rx_hdrs, total_header_len);
                p            = q;
                req->rx_hdrs = NULL;

                /* go on with data */
                req->parse_state = HTTPC_PARSE_RX_DATA;
            }
        }
    }

    if ((p != NULL) && (req->parse_state == HTTPC_PARSE_RX_DATA))
    {
        req->rx_content_len += p->tot_len;
        if (req->recv_fn != NULL)
        {
            /* directly return here: the connection migth already be aborted from the callback! */
            /* Modified to receive data larger than the window size */
            if (!req->conn_settings->insecure)
            {
                altcp_recved(pcb, p->tot_len);
            }

            err_t err = req->recv_fn(req->callback_arg, pcb, p, r);
            pbuf_free(p);

            if (err != ERR_OK)
            {
                return httpc_close(req, HTTPC_RESULT_LOCAL_ABORT, req->rx_status, err);
            }

            return err;
        }
        else
        {
            if (!req->conn_settings->insecure)
            {
                altcp_recved(pcb, p->tot_len);
            }

            pbuf_free(p);
        }
    }

    return ERR_OK;
}

/** http client tcp err callback */
static void httpc_tcp_err (void * arg, err_t err)
{
    httpc_state_t * req = (httpc_state_t *) arg;
    if (req != NULL)
    {
        /* pcb has already been deallocated */
        req->pcb = NULL;
        httpc_close(req, HTTPC_RESULT_ERR_CLOSED, 0, err);
    }
}

/** http client tcp poll callback */
static err_t httpc_tcp_poll (void * arg, struct altcp_pcb * pcb)
{
    /* implement timeout */
    httpc_state_t * req = (httpc_state_t *) arg;
    LWIP_UNUSED_ARG(pcb);
    if (req != NULL)
    {
        if (req->timeout_ticks)
        {
            if (prev_rx_content_len != req->rx_content_len)
            {
                prev_rx_content_len = req->rx_content_len;

                // return ERR_OK;
                req->timeout_ticks = HTTPC_POLL_TIMEOUT;
            }

            req->timeout_ticks--;
        }

        if (!req->timeout_ticks)
        {
            return httpc_close(req, HTTPC_RESULT_ERR_TIMEOUT, 0, ERR_OK);
        }
    }

    return ERR_OK;
}

/** http client tcp sent callback */
static err_t httpc_tcp_sent (void * arg, struct altcp_pcb * pcb, u16_t len)
{
    /* nothing to do here for now */
    LWIP_UNUSED_ARG(arg);
    LWIP_UNUSED_ARG(pcb);
    LWIP_UNUSED_ARG(len);

    return ERR_OK;
}

/** http client tcp connected callback */
static err_t httpc_tcp_connected (void * arg, struct altcp_pcb * pcb, err_t err)
{
    err_t           r;
    httpc_state_t * req = (httpc_state_t *) arg;
    LWIP_UNUSED_ARG(pcb);
    LWIP_UNUSED_ARG(err);

    /* send request; last char is zero termination */
    r = altcp_write(req->pcb, req->request->payload, req->request->len - 1, TCP_WRITE_FLAG_COPY);
    if (r != ERR_OK)
    {

        /* could not write the single small request -> fail, don't retry */
        return httpc_close(req, HTTPC_RESULT_ERR_MEM, 0, r);
    }

    /* everything written, we can free the request */
    pbuf_free(req->request);
    req->request = NULL;

    altcp_output(req->pcb);

    return ERR_OK;
}

/** Start the http request when the server IP addr is known */
static err_t httpc_get_internal_addr (httpc_state_t * req, const ip_addr_t * ipaddr)
{
    err_t err;
    LWIP_ASSERT("req != NULL", req != NULL);

    if (&req->remote_addr != ipaddr)
    {
        /* fill in remote addr if called externally */
        req->remote_addr = *ipaddr;
    }

    err = altcp_connect(req->pcb, &req->remote_addr, req->remote_port, httpc_tcp_connected);
    if (err == ERR_OK)
    {
        return ERR_OK;
    }

    LWIP_DEBUGF(HTTPC_DEBUG_WARN_STATE, ("tcp_connect failed: %d\n", (int) err));

    return err;
}

 #if LWIP_DNS

/** DNS callback
 * If ipaddr is non-NULL, resolving succeeded and the request can be sent, otherwise it failed.
 */
static void httpc_dns_found (const char * hostname, const ip_addr_t * ipaddr, void * arg)
{
    httpc_state_t * req = (httpc_state_t *) arg;
    err_t           err;
    httpc_result_t  result;

    LWIP_UNUSED_ARG(hostname);

    if (ipaddr != NULL)
    {
        err = httpc_get_internal_addr(req, ipaddr);
        if (err == ERR_OK)
        {
            return;
        }

        result = HTTPC_RESULT_ERR_CONNECT;
    }
    else
    {
        LWIP_DEBUGF(HTTPC_DEBUG_WARN_STATE,
                    ("httpc_dns_found: failed to resolve hostname: %s\n",
                     hostname));
        result = HTTPC_RESULT_ERR_HOSTNAME;
        err    = ERR_ARG;
    }

    httpc_close(req, result, 0, err);
}

 #endif                                /* LWIP_DNS */

/** Start the http request after converting 'server_name' to ip address (DNS or address string) */
static err_t httpc_get_internal_dns (httpc_state_t * req, const char * server_name)
{
    err_t err;
    LWIP_ASSERT("req != NULL", req != NULL);

 #if LWIP_DNS
    err = dns_gethostbyname(server_name, &req->remote_addr, httpc_dns_found, req);
 #else
    err = ipaddr_aton(server_name, &req->remote_addr) ? ERR_OK : ERR_ARG;
 #endif

    if (err == ERR_OK)
    {
        /* cached or IP-string */
        err = httpc_get_internal_addr(req, &req->remote_addr);
    }
    else if (err == ERR_INPROGRESS)
    {
        return ERR_OK;
    }

    return err;
}

 #ifdef __HTTPC_USER_MSG__
static int httpc_create_request_string_user (const httpc_connection_t * settings, char * buffer, size_t buffer_size)
{
    char * ptr           = NULL;
    char * offset        = NULL;
    char * pBuffer       = NULL;
    char * pBuffer_first = NULL;
    int    total_len     = 0;
    int    count_len     = 0;
    int    return_len    = 0;

    if (strlen(settings->post_msg) == 0)
    {
        return 0;
    }

    if ((buffer == NULL) || (buffer_size == 0))
    {
        pBuffer = pvPortMalloc(HTTPC_REQ_DATA_LEN + 1);
        if (pBuffer == NULL)
        {
            return 0;
        }

        memset(pBuffer, 0, HTTPC_REQ_DATA_LEN + 1);
        total_len = HTTPC_REQ_DATA_LEN;
    }
    else
    {
        memset(buffer, 0, buffer_size);
        pBuffer   = buffer;
        total_len = (int) buffer_size;
    }

    pBuffer_first = pBuffer;
    ptr           = (char *) settings->post_msg;
    offset        = ptr;

    while (total_len >= count_len++)
    {
        if ((*ptr == (char) 0x5c) &&
            (*(ptr + 1) == (char) 0x72) &&
            (*(ptr + 2) == (char) 0x5c) &&
            (*(ptr + 3) == (char) 0x6e) &&
            (*(ptr + 4) == (char) 0x5c) &&
            (*(ptr + 5) == (char) 0x72) &&
            (*(ptr + 6) == (char) 0x5c) &&
            (*(ptr + 7) == (char) 0x6e))
        {
            memcpy(pBuffer, offset, (size_t) (ptr - offset));
            pBuffer += ptr - offset;

            *pBuffer       = (char) 0x0d;
            *(pBuffer + 1) = (char) 0x0a;
            *(pBuffer + 2) = (char) 0x0d;
            *(pBuffer + 3) = (char) 0x0a;

            pBuffer += 4;
            ptr     += 8;
            offset   = ptr;
        }
        else if (strncmp(ptr, "\r\n\r\n", strlen("\r\n\r\n")) == 0)
        {
            memcpy(pBuffer, offset, (size_t) (ptr - offset));
            pBuffer += ptr - offset;

            *pBuffer       = (char) 0x0d;
            *(pBuffer + 1) = (char) 0x0a;
            *(pBuffer + 2) = (char) 0x0d;
            *(pBuffer + 3) = (char) 0x0a;

            pBuffer += 4;
            ptr     += 4;
            offset   = ptr;
        }

        if ((*ptr == (char) 0x5c) &&
            (*(ptr + 1) == (char) 0x72) &&
            (*(ptr + 2) == (char) 0x5c) &&
            (*(ptr + 3) == (char) 0x6e))
        {
            memcpy(pBuffer, offset, (size_t) (ptr - offset));
            pBuffer       += ptr - offset;
            *pBuffer       = (char) 0x0d;
            *(pBuffer + 1) = (char) 0x0a;

            pBuffer += 2;
            ptr     += 4;
            offset   = ptr;
        }
        else if (strncmp(ptr, "\r\n", strlen("\r\n")) == 0)
        {
            memcpy(pBuffer, offset, (size_t) (ptr - offset));
            pBuffer       += ptr - offset;
            *pBuffer       = (char) 0x0d;
            *(pBuffer + 1) = (char) 0x0a;

            pBuffer += 2;
            ptr     += 2;
            offset   = ptr;
        }

        ptr++;
    }

    return_len = (int) strlen(pBuffer_first);

    if ((pBuffer_first != NULL) && (buffer == NULL))
    {
        vPortFree(pBuffer_first);
        pBuffer_first = NULL;
    }

    return return_len;
}

 #endif                                // __HTTPC_USER_MSG__

static int httpc_create_request_string (const httpc_connection_t * settings,
                                        const char               * server_name,
                                        int                        server_port,
                                        const char               * uri,
                                        int                        use_host,
                                        char                     * buffer,
                                        size_t                     buffer_size)
{
    char * parse_uri  = NULL;
    char * auth_basic = NULL;
    char * tmp_buf    = NULL;
    char * p          = NULL;
    char * q          = NULL;
    size_t dlen       = 0;
 #if defined(HTTPC_ENABLE_HTTPS) && (CFG_MBEDTLS)
    size_t olen = 0;
 #endif                                // HTTPC_ENABLE_HTTPS
    int ret_len = 0;

    p = (char *) uri;
    q = strstr(p, "://");
    if (q != NULL)
    {
        q += strlen("://");
        p  = q;
    }
    else
    {
        return 0;
    }

    if ((strstr(p, ":")) && (strstr(p, "@")))
    {
        q = strstr(p, "@");
        if (q != NULL)
        {
            tmp_buf = pvPortMalloc((size_t) (q - p) + 1);
            if (tmp_buf != NULL)
            {
                memset(tmp_buf, 0, (size_t) (q - p) + 1);
                memcpy(tmp_buf, (const char *) p, (size_t) (q - p));
                dlen       = (size_t) (q - p) * 2;
                auth_basic = pvPortMalloc(dlen);
                memset(auth_basic, 0, dlen);

 #if defined(HTTPC_ENABLE_HTTPS) && (CFG_MBEDTLS)
                mbedtls_base64_encode((unsigned char *) auth_basic,
                                      dlen,
                                      &olen,
                                      (const unsigned char *) tmp_buf,
                                      strlen(tmp_buf));
 #endif                                // HTTPC_ENABLE_HTTPSs

                if (tmp_buf != NULL)
                {
                    vPortFree(tmp_buf);
                }

                p = ++q;
            }
            else
            {
                return 0;
            }
        }
    }

    parse_uri = pvPortMalloc(strlen(q) + 1);
    if (parse_uri != NULL)
    {
        q = strstr(p, "/");
        if (q != NULL)
        {
            strcpy(parse_uri, q);
        }
        else
        {
            strcpy(parse_uri, "/");
        }
    }
    else
    {
        if (parse_uri != NULL)
        {
            vPortFree(parse_uri);
        }

        return 0;
    }

    if (settings->use_proxy)
    {
        LWIP_ASSERT("server_name != NULL", server_name != NULL);
        if (server_port != HTTP_DEFAULT_PORT)
        {
            if (strlen(settings->post_msg) > 0)
            {
                if ((auth_basic != NULL) && (strlen(auth_basic) > 0))
                {
                    ret_len =
                        snprintf(buffer,
                                 buffer_size,
                                 HTTPC_POST_REQ_11_PROXY_PORT_AUTH_FORMAT(settings->method, server_name, server_port,
                                                                          parse_uri, server_name, auth_basic,
                                                                          settings->post_msg, strlen(
                                                                              settings->post_msg)));
                }
                else
                {
                    ret_len =
                        snprintf(buffer,
                                 buffer_size,
                                 HTTPC_POST_REQ_11_PROXY_PORT_FORMAT(settings->method, server_name, server_port,
                                                                     parse_uri, server_name, settings->post_msg,
                                                                     strlen(settings->post_msg)));
                }

                goto finish;
            }

            if ((auth_basic != NULL) && (strlen(auth_basic) > 0))
            {
                ret_len =
                    snprintf(buffer,
                             buffer_size,
                             HTTPC_REQ_11_PROXY_PORT_AUTH_FORMAT(settings->method, server_name, server_port, parse_uri,
                                                                 server_name, auth_basic));
            }
            else
            {
                ret_len =
                    snprintf(buffer, buffer_size,
                             HTTPC_REQ_11_PROXY_PORT_FORMAT(settings->method, server_name, server_port, parse_uri,
                                                            server_name));
            }

            goto finish;
        }
        else
        {
            if (strlen(settings->post_msg) > 0)
            {
                if ((auth_basic != NULL) && (strlen(auth_basic) > 0))
                {
                    ret_len =
                        snprintf(buffer,
                                 buffer_size,
                                 HTTPC_POST_REQ_11_PROXY_AUTH_FORMAT(settings->method, server_name, parse_uri,
                                                                     server_name, auth_basic, settings->post_msg,
                                                                     strlen(settings->post_msg)));
                }
                else
                {
                    ret_len =
                        snprintf(buffer,
                                 buffer_size,
                                 HTTPC_POST_REQ_11_PROXY_FORMAT(settings->method, server_name, parse_uri, server_name,
                                                                settings->post_msg, strlen(settings->post_msg)));
                }

                goto finish;
            }

            if ((auth_basic != NULL) && (strlen(auth_basic) > 0))
            {
                ret_len =
                    snprintf(buffer, buffer_size,
                             HTTPC_REQ_11_PROXY_AUTH_FORMAT(settings->method, server_name, parse_uri, server_name,
                                                            auth_basic));
            }
            else
            {
                ret_len =
                    snprintf(buffer, buffer_size,
                             HTTPC_REQ_11_PROXY_FORMAT(settings->method, server_name, parse_uri, server_name));
            }

            goto finish;
        }
    }
    else if (use_host)
    {
        LWIP_ASSERT("server_name != NULL", server_name != NULL);

        if (strlen(settings->post_msg) > 0)
        {
            if ((auth_basic != NULL) && (strlen(auth_basic) > 0))
            {
                ret_len =
                    snprintf(buffer,
                             buffer_size,
                             HTTPC_POST_REQ_11_HOST_AUTH_FORMAT(settings->method, parse_uri, server_name, auth_basic,
                                                                settings->post_msg, strlen(settings->post_msg)));
            }
            else
            {
                ret_len =
                    snprintf(buffer,
                             buffer_size,
                             HTTPC_POST_REQ_11_HOST_FORMAT(settings->method, parse_uri, server_name, settings->post_msg,
                                                           strlen(settings->post_msg)));
            }

            goto finish;
        }

        if ((auth_basic != NULL) && (strlen(auth_basic) > 0))
        {
            ret_len =
                snprintf(buffer, buffer_size,
                         HTTPC_REQ_11_HOST_AUTH_FORMAT(settings->method, parse_uri, server_name, auth_basic));
        }
        else
        {
            ret_len = snprintf(buffer, buffer_size, HTTPC_REQ_11_HOST_FORMAT(settings->method, parse_uri, server_name));
        }

        goto finish;
    }
    else
    {
        if (strlen(settings->post_msg) > 0)
        {
            if ((auth_basic != NULL) && (strlen(auth_basic) > 0))
            {
                ret_len =
                    snprintf(buffer,
                             buffer_size,
                             HTTPC_POST_REQ_11_AUTH_FORMAT(settings->method, parse_uri, auth_basic, settings->post_msg,
                                                           strlen(settings->post_msg)));
            }
            else
            {
                ret_len =
                    snprintf(buffer, buffer_size,
                             HTTPC_POST_REQ_11_FORMAT(settings->method, parse_uri, settings->post_msg,
                                                      strlen(settings->post_msg)));
            }

            goto finish;
        }

        if ((auth_basic != NULL) && (strlen(auth_basic) > 0))
        {
            ret_len = snprintf(buffer, buffer_size, HTTPC_REQ_11_AUTH_FORMAT(settings->method, parse_uri, auth_basic));
        }
        else
        {
            ret_len = snprintf(buffer, buffer_size, HTTPC_REQ_11_FORMAT(settings->method, parse_uri));
        }

        goto finish;
    }

finish:
    if (parse_uri != NULL)
    {
        vPortFree(parse_uri);
    }

    if (auth_basic != NULL)
    {
        vPortFree(auth_basic);
    }

    return ret_len;
}

/** Initialize the connection struct */
static err_t httpc_init_connection_common (httpc_state_t           ** connection,
                                           const httpc_connection_t * settings,
                                           const char               * server_name,
                                           u16_t                      server_port,
                                           const char               * uri,
                                           altcp_recv_fn              recv_fn,
                                           void                     * callback_arg,
                                           int                        use_host)
{
    size_t          alloc_len;
    mem_size_t      mem_alloc_len;
    int             req_len, req_len2;
    httpc_state_t * req;
 #if HTTPC_DEBUG_REQUEST
    size_t server_name_len, uri_len;
 #endif

    LWIP_ASSERT("uri != NULL", uri != NULL);

    /* get request len */
 #ifdef __HTTPC_USER_MSG__
    req_len = httpc_create_request_string_user(settings, NULL, 0);
    if (req_len == 0)
 #endif                                // __HTTPC_USER_MSG__
    req_len = httpc_create_request_string(settings, server_name, server_port, uri, use_host, NULL, 0);
    if ((req_len < 0) || (req_len > 0xFFFF))
    {
        return ERR_VAL;
    }

    /* alloc state and request in one block */
    alloc_len = sizeof(httpc_state_t);
 #if HTTPC_DEBUG_REQUEST
    server_name_len = server_name ? strlen(server_name) : 0;
    uri_len         = strlen(uri);
    alloc_len      += server_name_len + 1 + uri_len + 1;
 #endif
    mem_alloc_len = (mem_size_t) alloc_len;
    if ((mem_alloc_len < alloc_len) || (req_len + 1 > 0xFFFF))
    {
        return ERR_VAL;
    }

    req = (httpc_state_t *) mem_malloc((mem_size_t) alloc_len);
    if (req == NULL)
    {
        return ERR_MEM;
    }

    memset(req, 0, sizeof(httpc_state_t));
    req->timeout_ticks = HTTPC_POLL_TIMEOUT;
    req->request       = pbuf_alloc(PBUF_RAW, (u16_t) (req_len + 1), PBUF_RAM);
    if (req->request == NULL)
    {
        httpc_free_state(req);

        return ERR_MEM;
    }

    if (req->request->next != NULL)
    {
        /* need a pbuf in one piece */
        httpc_free_state(req);

        return ERR_MEM;
    }

    req->hdr_content_len = HTTPC_CONTENT_LEN_INVALID;
 #if HTTPC_DEBUG_REQUEST
    req->server_name = (char *) (req + 1);
    if (server_name)
    {
        memcpy(req->server_name, server_name, server_name_len + 1);
    }

    req->uri = req->server_name + server_name_len + 1;
    memcpy(req->uri, uri, uri_len + 1);
 #endif

 #ifdef HTTPC_ENABLE_HTTPS
    if (settings->insecure)
    {
        const httpc_secure_connection_t * tls_settings = &(settings->tls_settings);

        if ((tls_settings->cert && (tls_settings->cert_len > 0)) &&
            (tls_settings->privkey && (tls_settings->privkey_len > 0)))
        {
            if (tls_settings->ca && (tls_settings->ca_len > 0))
            {
                req->tls_config = altcp_tls_create_config_client_2wayauth(tls_settings->ca,
                                                                          tls_settings->ca_len,
                                                                          tls_settings->privkey,
                                                                          tls_settings->privkey_len,
                                                                          NULL,
                                                                          0,
                                                                          tls_settings->cert,
                                                                          tls_settings->cert_len);
            }
            else
            {
                req->tls_config = altcp_tls_create_config_server_privkey_cert(tls_settings->privkey,
                                                                              tls_settings->privkey_len,
                                                                              NULL,
                                                                              0,
                                                                              tls_settings->cert,
                                                                              tls_settings->cert_len);
            }
        }
        else
        {
            req->tls_config = altcp_tls_create_config_client(tls_settings->ca, tls_settings->ca_len);
        }

        if (req->tls_config == NULL)
        {
            httpc_free_state(req);

            return ERR_VAL;
        }

        /* Set TLS version */
        altcp_tls_config_min_tls_version(req->tls_config, tls_settings->tls_ver_min);
        altcp_tls_config_max_tls_version(req->tls_config, tls_settings->tls_ver_max);

        altcp_tls_config_authmode(req->tls_config, tls_settings->auth_mode);

        if ((tls_settings->incoming_len > 0) && (tls_settings->outgoing_len > 0))
        {
            altcp_tls_config_content_len(req->tls_config, tls_settings->incoming_len, tls_settings->outgoing_len);
        }

        if (tls_settings->alpn_cnt && tls_settings->alpn[0])
        {
            altcp_tls_config_alpn_protocols(req->tls_config, (const char **) tls_settings->alpn);
        }

        req->pcb = altcp_tls_new(req->tls_config, IPADDR_TYPE_ANY);
        if (req->pcb == NULL)
        {
            httpsc_free_state(req);
            httpc_free_state(req);

            return ERR_MEM;
        }

        if (tls_settings->sni_len && tls_settings->sni)
        {
            altcp_tls_config_sni(req->pcb, tls_settings->sni);
        }
    }
    else
    {
        req->pcb = altcp_new_ip_type(settings->altcp_allocator, IPADDR_TYPE_ANY);
        if (req->pcb == NULL)
        {
            httpc_free_state(req);

            return ERR_MEM;
        }
    }

 #else                                 // HTTPC_ENABLE_HTTPS
    req->pcb = altcp_new(settings ? settings->altcp_allocator : NULL);
    if (req->pcb == NULL)
    {
        httpc_free_state(req);

        return ERR_MEM;
    }
 #endif                                // HTTPC_ENABLE_HTTPS
    req->remote_port = settings->use_proxy ? settings->proxy_port : server_port;
    altcp_arg(req->pcb, req);
    altcp_recv(req->pcb, httpc_tcp_recv);
    altcp_err(req->pcb, httpc_tcp_err);
    altcp_poll(req->pcb, httpc_tcp_poll, HTTPC_POLL_INTERVAL);
    altcp_sent(req->pcb, httpc_tcp_sent);
 #if TCP_FINISHED_CALLBACK
    altcp_reg_finish_cb(req->pcb, settings->finish_cb_fn);
 #endif

    /* set up request buffer */
 #ifdef __HTTPC_USER_MSG__
    req_len2 = httpc_create_request_string_user(settings, (char *) req->request->payload, (size_t) (req_len + 1));
    if (req_len2 == 0)
 #endif                                // __HTTPC_USER_MSG__
    req_len2 = httpc_create_request_string(settings,
                                           server_name,
                                           server_port,
                                           uri,
                                           use_host,
                                           (char *) req->request->payload,
                                           (size_t) (req_len + 1));

    if (req_len2 != req_len)
    {
 #ifdef HTTPC_ENABLE_HTTPS
        if (settings->insecure)
        {
            httpsc_free_state(req);
        }
 #endif                                // HTTPC_ENABLE_HTTPS
        httpc_free_state(req);

        return ERR_VAL;
    }

    req->recv_fn       = recv_fn;
    req->conn_settings = settings;
    req->callback_arg  = callback_arg;

    *connection = req;

    return ERR_OK;
}

/**
 * Initialize the connection struct
 */
static err_t httpc_init_connection (httpc_state_t           ** connection,
                                    const httpc_connection_t * settings,
                                    const char               * server_name,
                                    u16_t                      server_port,
                                    const char               * uri,
                                    altcp_recv_fn              recv_fn,
                                    void                     * callback_arg)
{
    return httpc_init_connection_common(connection, settings, server_name, server_port, uri, recv_fn, callback_arg, 1);
}

/**
 * Initialize the connection struct (from IP address)
 */
static err_t httpc_init_connection_addr (httpc_state_t           ** connection,
                                         const httpc_connection_t * settings,
                                         const ip_addr_t          * server_addr,
                                         u16_t                      server_port,
                                         const char               * uri,
                                         altcp_recv_fn              recv_fn,
                                         void                     * callback_arg)
{
    char * server_addr_str = ipaddr_ntoa(server_addr);
    if (server_addr_str == NULL)
    {
        return ERR_VAL;
    }

    return httpc_init_connection_common(connection,
                                        settings,
                                        server_addr_str,
                                        server_port,
                                        uri,
                                        recv_fn,
                                        callback_arg,
                                        1);
}

/**
 * @ingroup httpc
 * HTTP client API: get a file by passing server IP address
 *
 * @param server_addr IP address of the server to connect
 * @param port tcp port of the server
 * @param uri uri to get from the server, remember leading "/"!
 * @param settings connection settings (callbacks, proxy, etc.)
 * @param recv_fn the http body (not the headers) are passed to this callback
 * @param callback_arg argument passed to all the callbacks
 * @param connection retreives the connection handle (to match in callbacks)
 * @return ERR_OK if starting the request succeeds (callback_fn will be called later)
 *         or an error code
 */
err_t httpc_get_file (const ip_addr_t          * server_addr,
                      u16_t                      port,
                      const char               * uri,
                      const httpc_connection_t * settings,
                      altcp_recv_fn              recv_fn,
                      void                     * callback_arg,
                      httpc_state_t           ** connection)
{
    err_t           err;
    httpc_state_t * req = NULL;

    LWIP_ERROR("invalid parameters", (server_addr != NULL) && (uri != NULL) && (recv_fn != NULL), return ERR_ARG; );

    err = httpc_init_connection_addr(&req, settings, server_addr, port, uri, recv_fn, callback_arg);
    if (err != ERR_OK)
    {
        return err;
    }

    if (settings->use_proxy)
    {
        err = httpc_get_internal_addr(req, &settings->proxy_addr);
    }
    else
    {
        err = httpc_get_internal_addr(req, server_addr);
    }

    if (err != ERR_OK)
    {
 #ifdef HTTPC_ENABLE_HTTPS
        if (settings && settings->insecure)
        {
            httpsc_free_state(req);
        }
 #endif                                // HTTPC_ENABLE_HTTPS
        httpc_free_state(req);

        return err;
    }

    if (connection != NULL)
    {
        *connection = req;
    }

    return ERR_OK;
}

/**
 * @ingroup httpc
 * HTTP client API: get a file by passing server name as string (DNS name or IP address string)
 *
 * @param server_name server name as string (DNS name or IP address string)
 * @param port tcp port of the server
 * @param uri uri to get from the server, remember leading "/"!
 * @param settings connection settings (callbacks, proxy, etc.)
 * @param recv_fn the http body (not the headers) are passed to this callback
 * @param callback_arg argument passed to all the callbacks
 * @param connection retreives the connection handle (to match in callbacks)
 * @return ERR_OK if starting the request succeeds (callback_fn will be called later)
 *         or an error code
 */
err_t httpc_get_file_dns (const char               * server_name,
                          u16_t                      port,
                          const char               * uri,
                          const httpc_connection_t * settings,
                          altcp_recv_fn              recv_fn,
                          void                     * callback_arg,
                          httpc_state_t           ** connection)
{
    err_t           err;
    httpc_state_t * req = NULL;

    LWIP_ERROR("invalid parameters", (server_name != NULL) && (uri != NULL) && (recv_fn != NULL), return ERR_ARG; );

    err = httpc_init_connection(&req, settings, server_name, port, uri, recv_fn, callback_arg);
    if (err != ERR_OK)
    {
        return err;
    }

    if (settings->use_proxy)
    {
        err = httpc_get_internal_addr(req, &settings->proxy_addr);
    }
    else
    {
        err = httpc_get_internal_dns(req, server_name);
    }

    if (err != ERR_OK)
    {
 #ifdef HTTPC_ENABLE_HTTPS
        if (settings && settings->insecure)
        {
            httpsc_free_state(req);
        }
 #endif                                // HTTPC_ENABLE_HTTPS
        httpc_free_state(req);

        return err;
    }

    if (connection != NULL)
    {
        *connection = req;
    }

    return ERR_OK;
}

 #if LWIP_HTTPC_HAVE_FILE_IO

/* Implementation to disk via fopen/fwrite/fclose follows */

typedef struct _httpc_filestate
{
    const char               * local_file_name;
    FILE                     * file;
    httpc_connection_t         settings;
    const httpc_connection_t * client_settings;
    void                     * callback_arg;
} httpc_filestate_t;

static void httpc_fs_result(void * arg, httpc_result_t httpc_result, u32_t rx_content_len, u32_t srv_res, err_t err);

/** Initialize http client state for download to file system */
static err_t httpc_fs_init (httpc_filestate_t       ** filestate_out,
                            const char               * local_file_name,
                            const httpc_connection_t * settings,
                            void                     * callback_arg)
{
    httpc_filestate_t * filestate;
    size_t              file_len, alloc_len;
    FILE              * f;

    file_len  = strlen(local_file_name);
    alloc_len = sizeof(httpc_filestate_t) + file_len + 1;

    filestate = (httpc_filestate_t *) mem_malloc((mem_size_t) alloc_len);
    if (filestate == NULL)
    {
        return ERR_MEM;
    }

    memset(filestate, 0, sizeof(httpc_filestate_t));
    filestate->local_file_name = (const char *) (filestate + 1);
    memcpy((char *) (filestate + 1), local_file_name, file_len + 1);
    filestate->file            = NULL;
    filestate->client_settings = settings;
    filestate->callback_arg    = callback_arg;

    /* copy client settings but override result callback */
    memcpy(&filestate->settings, settings, sizeof(httpc_connection_t));
    filestate->settings.result_fn = httpc_fs_result;

    f = fopen(local_file_name, "wb");
    if (f == NULL)
    {
        /* could not open file */
        mem_free(filestate);

        return ERR_VAL;
    }

    filestate->file = f;
    *filestate_out  = filestate;

    return ERR_OK;
}

/** Free http client state for download to file system */
static void httpc_fs_free (httpc_filestate_t * filestate)
{
    if (filestate != NULL)
    {
        if (filestate->file != NULL)
        {
            fclose(filestate->file);
            filestate->file = NULL;
        }

        mem_free(filestate);
    }
}

/** Connection closed (success or error) */
static void httpc_fs_result (void * arg, httpc_result_t httpc_result, u32_t rx_content_len, u32_t srv_res, err_t err)
{
    httpc_filestate_t * filestate = (httpc_filestate_t *) arg;
    if (filestate != NULL)
    {
        if (filestate->client_settings->result_fn != NULL)
        {
            filestate->client_settings->result_fn(filestate->callback_arg, httpc_result, rx_content_len, srv_res, err);
        }

        httpc_fs_free(filestate);
    }
}

/** tcp recv callback */
static err_t httpc_fs_tcp_recv (void * arg, struct altcp_pcb * pcb, struct pbuf * p, err_t err)
{
    httpc_filestate_t * filestate = (httpc_filestate_t *) arg;
    struct pbuf       * q;
    LWIP_UNUSED_ARG(err);

    LWIP_ASSERT("p != NULL", p != NULL);

    for (q = p; q != NULL; q = q->next)
    {
        fwrite(q->payload, 1, q->len, filestate->file);
    }

    altcp_recved(pcb, p->tot_len);
    pbuf_free(p);

    return ERR_OK;
}

/**
 * @ingroup httpc
 * HTTP client API: get a file to disk by passing server IP address
 *
 * @param server_addr IP address of the server to connect
 * @param port tcp port of the server
 * @param uri uri to get from the server, remember leading "/"!
 * @param settings connection settings (callbacks, proxy, etc.)
 * @param callback_arg argument passed to all the callbacks
 * @param connection retreives the connection handle (to match in callbacks)
 * @return ERR_OK if starting the request succeeds (callback_fn will be called later)
 *         or an error code
 */
err_t httpc_get_file_to_disk (const ip_addr_t          * server_addr,
                              u16_t                      port,
                              const char               * uri,
                              const httpc_connection_t * settings,
                              void                     * callback_arg,
                              const char               * local_file_name,
                              httpc_state_t           ** connection)
{
    err_t               err;
    httpc_state_t     * req;
    httpc_filestate_t * filestate;

    LWIP_ERROR("invalid parameters",
               (server_addr != NULL) && (uri != NULL) && (local_file_name != NULL),
               return ERR_ARG; );

    err = httpc_fs_init(&filestate, local_file_name, settings, callback_arg);
    if (err != ERR_OK)
    {
        return err;
    }

    err = httpc_init_connection_addr(&req, &filestate->settings, server_addr, port, uri, httpc_fs_tcp_recv, filestate);
    if (err != ERR_OK)
    {
        httpc_fs_free(filestate);

        return err;
    }

    if (settings->use_proxy)
    {
        err = httpc_get_internal_addr(req, &settings->proxy_addr);
    }
    else
    {
        err = httpc_get_internal_addr(req, server_addr);
    }

    if (err != ERR_OK)
    {
        if (settings && settings->insecure)
        {
            httpsc_free_state(req);
        }

        httpc_fs_free(filestate);
        httpc_free_state(req);

        return err;
    }

    if (connection != NULL)
    {
        *connection = req;
    }

    return ERR_OK;
}

  #ifdef HTTPC_ENABLE_HTTPS

/**
 * @ingroup httpc
 * HTTP client API: get a file to disk by passing server name as string (DNS name or IP address string)
 *
 * @param server_name server name as string (DNS name or IP address string)
 * @param port tcp port of the server
 * @param uri uri to get from the server, remember leading "/"!
 * @param settings connection settings (callbacks, proxy, etc.)
 * @param callback_arg argument passed to all the callbacks
 * @param connection retreives the connection handle (to match in callbacks)
 * @return ERR_OK if starting the request succeeds (callback_fn will be called later)
 *         or an error code
 */
err_t httpc_get_file_dns_to_disk (const char               * server_name,
                                  u16_t                      port,
                                  const char               * uri,
                                  const httpc_connection_t * settings,
                                  void                     * callback_arg,
                                  const char               * local_file_name,
                                  httpc_state_t           ** connection)
{
    err_t               err;
    httpc_state_t     * req;
    httpc_filestate_t * filestate;

    LWIP_ERROR("invalid parameters",
               (server_name != NULL) && (uri != NULL) && (local_file_name != NULL),
               return ERR_ARG; );

    err = httpc_fs_init(&filestate, local_file_name, settings, callback_arg);
    if (err != ERR_OK)
    {
        return err;
    }

    err = httpc_init_connection(&req, &filestate->settings, server_name, port, uri, httpc_fs_tcp_recv, filestate);
    if (err != ERR_OK)
    {
        httpc_fs_free(filestate);

        return err;
    }

    if (settings->use_proxy)
    {
        err = httpc_get_internal_addr(req, &settings->proxy_addr);
    }
    else
    {
        err = httpc_get_internal_dns(req, server_name);
    }

    if (err != ERR_OK)
    {
        if (settings && settings->insecure)
        {
            httpsc_free_state(req);
        }

        httpc_fs_free(filestate);
        httpc_free_state(req);

        return err;
    }

    if (connection != NULL)
    {
        *connection = req;
    }

    return ERR_OK;
}

  #endif                               /* HTTPC_ENABLE_HTTPS */
 #endif                                /* LWIP_HTTPC_HAVE_FILE_IO */

#endif                                 /* LWIP_TCP && LWIP_CALLBACK_API */
