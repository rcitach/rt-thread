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

#if defined(__SUPPORT_WEBSOCKET_CLIENT__)

 #include <stdlib.h>
 #include <string.h>
 #include <ctype.h>
 #include "transport.h"
 #include "transport_tcp.h"
 #include "transport_utils.h"
 #include "transport_ws.h"
 #include "ws_log.h"
 #include "ws_mem.h"
 #include "mbedtls/base64.h"
 #include "mbedtls/sha1.h"
 #include "mbedtls/platform.h"
 #include "mbedtls/ctr_drbg.h"
 #include "mbedtls/entropy.h"

 #include "lwip/sockets.h"
 #include "websocket_client.h"
 #if CFG_PMGR
  #include "rm_pmgr_w_instance.h"
extern int websocket_client_read_from_dpm_user_mem(void);

 #endif                                /* CFG_PMGR */
extern websocketParamForRtm websocketParams;

 #define DEFAULT_WS_BUFFER            (1024)
 #define WS_FIN                       0x80
 #define WS_OPCODE_CONT               0x00
 #define WS_OPCODE_TEXT               0x01
 #define WS_OPCODE_BINARY             0x02
 #define WS_OPCODE_CLOSE              0x08
 #define WS_OPCODE_PING               0x09
 #define WS_OPCODE_PONG               0x0a

// Second byte
 #define WS_MASK                      0x80
 #define WS_SIZE16                    126
 #define WS_SIZE64                    127
 #define MAX_WEBSOCKET_HEADER_SIZE    16
 #define WS_RESPONSE_OK               101

typedef struct
{
    uint8_t opcode;
    char    mask_key[4];               /*!< Mask key for this payload */
    int     payload_len;               /*!< Total length of the payload */
    int     bytes_remaining;           /*!< Bytes left to read of the payload  */
} ws_transport_frame_state_t;

typedef struct
{
    char * path;
    char * buffer;
    char * sub_protocol;
    char * user_agent;
    char * headers;
    ws_transport_frame_state_t frame_state;
    ws_transport_handle_t      parent;
} transport_ws_t;

static inline uint8_t ws_get_bin_opcode (ws_transport_opcodes_t opcode)
{
    return (uint8_t) opcode;
}

static ws_transport_handle_t ws_get_payload_transport_handle (ws_transport_handle_t t)
{
    transport_ws_t * ws = ws_transport_get_context_data(t);

    /* Reading parts of a frame directly will disrupt the WS internal frame state,
     *  reset bytes_remaining to prepare for reading a new frame */
    ws->frame_state.bytes_remaining = 0;

    return ws->parent;
}

static char * trimwhitespace (const char * str)
{
    char * end;

    // Trim leading space
    while (isspace((unsigned char) *str))
    {
        str++;
    }

    if (*str == 0)
    {
        return (char *) str;
    }

    // Trim trailing space
    end = (char *) (str + strlen(str) - 1);
    while (end > str && isspace((unsigned char) *end))
    {
        end--;
    }

    // Write new null terminator
    *(end + 1) = 0;

    return (char *) str;
}

extern char * strcasestr(const char * big, const char * little);

static char * get_http_header (const char * buffer, const char * key)
{
    char * found = strcasestr(buffer, key);
    if (found)
    {
        found += strlen(key);
        char * found_end = strstr(found, "\r\n");
        if (found_end)
        {
            found_end[0] = 0;          // terminal string

            return trimwhitespace(found);
        }
    }

    return NULL;
}

static void ws_getrandom (unsigned char * buf, unsigned int buf_size)
{
    int ret = -1;
    mbedtls_ctr_drbg_context ctx;      // The CTR_DRBG context structure.
    mbedtls_entropy_context  entropy;
    mbedtls_ctr_drbg_init(&ctx);

    mbedtls_entropy_init(&entropy);

    ret = mbedtls_ctr_drbg_seed(&ctx, mbedtls_entropy_func, &entropy, (const unsigned char *) "RANDOM_GEN", 10);
    if (ret != 0)
    {
        mbedtls_printf("failed in mbedtls_ctr_drbg_seed: %d\n", ret);
        goto cleanup;
    }

    mbedtls_ctr_drbg_set_prediction_resistance(&ctx, MBEDTLS_CTR_DRBG_PR_OFF);

    ret = mbedtls_ctr_drbg_random(&ctx, buf, buf_size);

    if (ret)
    {
        printf("[%s] Failed to generate random data(0x%0x)\r\n", __func__, -ret);
        goto cleanup;
    }

cleanup:
    mbedtls_ctr_drbg_free(&ctx);
    mbedtls_entropy_free(&entropy);
}

static int ws_connect (ws_transport_handle_t t, const char * host, int port, int timeout_ms)
{
    transport_ws_t * ws = ws_transport_get_context_data(t);

    if (ws_transport_connect(ws->parent, host, port, timeout_ms) < 0)
    {
        WS_LOGE("TRANSPORT_WS", "Error connecting to host %s:%d\n", host, port);

        return -1;
    }

 #if CFG_PMGR
    if (RM_PMGR_W_dpm_is_enabled())
    {
        websocket_client_read_from_dpm_user_mem();
    }
 #endif                                /* CFG_PMGR */

    if (websocketParams.status != WEBSOCKET_STATE_CONNECTED)
    {
        unsigned char random_key[16];

        // getrandom(random_key, sizeof(random_key), 0);
        ws_getrandom(random_key, sizeof(random_key));

        // WS_LOGD("TRANSPORT_WS", "Random Key: %s\n", random_key);

        // Size of base64 coded string is equal '((input_size * 4) / 3) + (input_size / 96) + 6' including Z-term
        unsigned char client_key[28] = {0};

        const char * user_agent_ptr = (ws->user_agent) ? (ws->user_agent) : "Websocket Client";

        size_t outlen = 0;
        mbedtls_base64_encode(client_key, sizeof(client_key), &outlen, random_key, sizeof(random_key));

        WS_LOGD("TRANSPORT_WS", "Client Key:%s\n", client_key);

        int len = snprintf(ws->buffer,
                           DEFAULT_WS_BUFFER,
                           "GET %s HTTP/1.1\r\n"
                           "Connection: Upgrade\r\n"
                           "Host: %s:%d\r\n"
                           "User-Agent: %s\r\n"
                           "Upgrade: websocket\r\n"
                           "Sec-WebSocket-Version: 13\r\n"
                           "Sec-WebSocket-Key: %s\r\n",
                           ws->path,
                           host,
                           port,
                           user_agent_ptr,
                           client_key);
        if ((len <= 0) || (len >= DEFAULT_WS_BUFFER))
        {
            WS_LOGE("TRANSPORT_WS", "Error in request generation, %d\n", len);

            return -1;
        }

        if (ws->sub_protocol)
        {
            WS_LOGI("TRANSPORT_WS", "sub_protocol: %s\n", ws->sub_protocol);
            int r = snprintf(ws->buffer + len,
                             (size_t) (DEFAULT_WS_BUFFER - len),
                             "Sec-WebSocket-Protocol: %s\r\n",
                             ws->sub_protocol);
            len += r;
            if ((r <= 0) || (len >= DEFAULT_WS_BUFFER))
            {
                WS_LOGE("TRANSPORT_WS",
                        "Error in request generation\n"
                        "(snprintf of subprotocol returned %d, desired request len: %d, buffer size: %d\n",
                        r,
                        len,
                        DEFAULT_WS_BUFFER);

                return -1;
            }
        }

        if (ws->headers)
        {
            WS_LOGI("TRANSPORT_WS", "headers: %s\n", ws->headers);
            int r = snprintf(ws->buffer + len, (size_t) (DEFAULT_WS_BUFFER - len), "%s\r\n", ws->headers);
            len += r;
            if ((r <= 0) || (len >= DEFAULT_WS_BUFFER))
            {
                WS_LOGE("TRANSPORT_WS",
                        "Error in request generation\n"
                        "(strncpy of headers returned %d, desired request len: %d, buffer size: %d\n",
                        r,
                        len,
                        DEFAULT_WS_BUFFER);

                return -1;
            }
        }

        int r = snprintf(ws->buffer + len, (size_t) (DEFAULT_WS_BUFFER - len), "\r\n");
        len += r;
        if ((r <= 0) || (len >= DEFAULT_WS_BUFFER))
        {
            WS_LOGE("TRANSPORT_WS",
                    "Error in request generation\n"
                    "(snprintf of header terminal returned %d, desired request len: %d, buffer size: %d\n",
                    r,
                    len,
                    DEFAULT_WS_BUFFER);

            return -1;
        }

        WS_LOGI("TRANSPORT_WS", "Write upgrade request\r\n%s\n", ws->buffer);
        if (ws_transport_write(ws->parent, ws->buffer, len, timeout_ms) <= 0)
        {
            WS_LOGE("TRANSPORT_WS", "Error write Upgrade header %s\n", ws->buffer);

            return -1;
        }

        int header_len = 0;
        do
        {
            if ((len =
                     ws_transport_read(ws->parent, ws->buffer + header_len, DEFAULT_WS_BUFFER - header_len,
                                       timeout_ms)) <= 0)
            {
                WS_LOGE("TRANSPORT_WS", "Error read response for Upgrade header %s\n", ws->buffer);

                return -1;
            }

            header_len            += len;
            ws->buffer[header_len] = '\0';
            WS_LOGD("TRANSPORT_WS", "Read header chunk %d, current header size: %d\n", len, header_len);
        } while (NULL == strstr(ws->buffer, "\r\n\r\n") && header_len < DEFAULT_WS_BUFFER);

        char * server_key = get_http_header(ws->buffer, "Sec-WebSocket-Accept:");
        if (server_key == NULL)
        {
            WS_LOGE("TRANSPORT_WS", "Sec-WebSocket-Accept not found");

            return -1;
        }

        // See mbedtls_sha1_ret() arg size
        unsigned char expected_server_sha1[20];

        // Size of base64 coded string see above
        unsigned char expected_server_key[33] = {0};

        // If you are interested, see https://tools.ietf.org/html/rfc6455
        const char    expected_server_magic[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        unsigned char expected_server_text[sizeof(client_key) + sizeof(expected_server_magic) + 1];
        strcpy((char *) expected_server_text, (char *) client_key);
        strcat((char *) expected_server_text, expected_server_magic);

        size_t key_len = strlen((char *) expected_server_text);
        mbedtls_sha1(expected_server_text, key_len, expected_server_sha1);
        mbedtls_base64_encode(expected_server_key, sizeof(expected_server_key), &outlen, expected_server_sha1,
                              sizeof(expected_server_sha1));
        expected_server_key[(outlen < sizeof(expected_server_key)) ? outlen : (sizeof(expected_server_key) - 1)] = 0;
        WS_LOGI("TRANSPORT_WS",
                "server key=%s, send_key=%s, expected_server_key=%s\n",
                (char *) server_key,
                (char *) client_key,
                expected_server_key);
        if (strcmp((char *) expected_server_key, (char *) server_key) != 0)
        {
            WS_LOGE("TRANSPORT_WS", "Invalid websocket key\n");

            return -1;
        }
    }

 #if CFG_PMGR
    if (RM_PMGR_W_dpm_is_enabled())
    {
        RM_PMGR_W_dpm_sleep_ready_set(APP_WEBSOCKET_CLIENT);
    }
 #endif                                /* CFG_PMGR */
    return 0;
}

static int _ws_write (ws_transport_handle_t t, int opcode, int mask_flag, const char * b, int len, int timeout_ms)
{
    transport_ws_t * ws     = ws_transport_get_context_data(t);
    char           * buffer = (char *) b;
    char             ws_header[MAX_WEBSOCKET_HEADER_SIZE];
    char           * mask;
    int              header_len = 0, i;

    int poll_write;
    if ((poll_write = ws_transport_poll_write(ws->parent, timeout_ms)) <= 0)
    {
        WS_LOGE("TRANSPORT_WS", "Error transport_poll_write\n");

        return poll_write;
    }

    ws_header[header_len++] = (char) opcode;

    if (len <= 125)
    {
        ws_header[header_len++] = (char) (len | mask_flag);
    }
    else if (len < 65536)
    {
        ws_header[header_len++] = (char) (WS_SIZE16 | mask_flag);
        ws_header[header_len++] = (char) (len >> 8);
        ws_header[header_len++] = (char) (len & 0xFF);
    }
    else
    {
        ws_header[header_len++] = (char) (WS_SIZE64 | mask_flag);

        /* Support maximum 4 bytes length */
        ws_header[header_len++] = 0;   // (uint8_t)((len >> 56) & 0xFF);
        ws_header[header_len++] = 0;   // (uint8_t)((len >> 48) & 0xFF);
        ws_header[header_len++] = 0;   // (uint8_t)((len >> 40) & 0xFF);
        ws_header[header_len++] = 0;   // (uint8_t)((len >> 32) & 0xFF);
        ws_header[header_len++] = (char) ((len >> 24) & 0xFF);
        ws_header[header_len++] = (char) ((len >> 16) & 0xFF);
        ws_header[header_len++] = (char) ((len >> 8) & 0xFF);
        ws_header[header_len++] = (char) ((len >> 0) & 0xFF);
    }

    if (mask_flag)
    {
        mask = &ws_header[header_len];

// getrandom(ws_header + header_len, 4, 0);
        ws_getrandom((unsigned char *) (ws_header + header_len), 4);

        header_len += 4;

        for (i = 0; i < len; ++i)
        {
            buffer[i] = (buffer[i] ^ mask[i % 4]);
        }
    }

    if (ws_transport_write(ws->parent, ws_header, header_len, timeout_ms) != header_len)
    {
        WS_LOGE("TRANSPORT_WS", "Error write header\n");

        return -1;
    }

    if (len == 0)
    {
        return 0;
    }

    int ret = ws_transport_write(ws->parent, buffer, len, timeout_ms);

    // in case of masked transport we have to revert back to the original data, as ws layer
    // does not create its own copy of data to be sent
    if (mask_flag)
    {
        mask = &ws_header[header_len - 4];
        for (i = 0; i < len; ++i)
        {
            buffer[i] = (buffer[i] ^ mask[i % 4]);
        }
    }

    return ret;
}

int ws_transport_ws_send_raw (ws_transport_handle_t  t,
                              ws_transport_opcodes_t opcode,
                              const char           * b,
                              int                    len,
                              int                    timeout_ms)
{
    uint8_t op_code = ws_get_bin_opcode(opcode);
    if (t == NULL)
    {
        WS_LOGE("TRANSPORT_WS", "Transport must be a valid ws handle\n");

        return WS_ERR_INVALID_ARG;
    }

    WS_LOGD("TRANSPORT_WS", "Sending raw ws message with opcode %d\n", op_code);

    return _ws_write(t, op_code, WS_MASK, b, len, timeout_ms);
}

static int ws_write (ws_transport_handle_t t, const char * b, int len, int timeout_ms)
{
    if (len == 0)
    {
        // Default transport write of zero length in ws layer sends out a ping message.
        // This behaviour could however be altered in IDF 5.0, since a separate API for sending
        // messages with user defined opcodes has been introduced.
        WS_LOGD("TRANSPORT_WS", "Write PING message\n");

        return _ws_write(t, WS_OPCODE_PING | WS_FIN, WS_MASK, NULL, 0, timeout_ms);
    }

 #if CFG_PMGR
    if (RM_PMGR_W_dpm_is_enabled())
    {
        RM_PMGR_W_dpm_sleep_ready_set(APP_WEBSOCKET_CLIENT);
    }
 #endif                                /* CFG_PMGR */
    return _ws_write(t, WS_OPCODE_BINARY | WS_FIN, WS_MASK, b, len, timeout_ms);
}

static int ws_read_payload (ws_transport_handle_t t, char * buffer, int len, int timeout_ms)
{
    transport_ws_t * ws = ws_transport_get_context_data(t);

    int bytes_to_read;
    int rlen = 0;

    if (ws->frame_state.bytes_remaining > len)
    {
        WS_LOGD("TRANSPORT_WS",
                "Actual data to receive (%d) are longer than ws buffer (%d)\n",
                ws->frame_state.bytes_remaining,
                len);
        bytes_to_read = len;
    }
    else
    {
        bytes_to_read = ws->frame_state.bytes_remaining;
    }

    // Receive and process payload
    if ((bytes_to_read != 0) && ((rlen = ws_transport_read(ws->parent, buffer, bytes_to_read, timeout_ms)) <= 0))
    {
        WS_LOGE("TRANSPORT_WS", "Error read data\n");

        return rlen;
    }

    ws->frame_state.bytes_remaining -= rlen;

    for (int i = 0; i < bytes_to_read; i++)
    {
        buffer[i] = (buffer[i] ^ ws->frame_state.mask_key[i % 4]);
    }

    return rlen;
}

/* Read and parse the WS header, determine length of payload */
static int ws_read_header (ws_transport_handle_t t, char * buffer, int len, int timeout_ms)
{
    RA6W1_UNUSED_ARG(len);

    transport_ws_t * ws = ws_transport_get_context_data(t);
    int              payload_len;

    char   ws_header[MAX_WEBSOCKET_HEADER_SIZE];
    char * data_ptr = ws_header, mask;
    int    rlen;
    int    poll_read;
    if ((poll_read = ws_transport_poll_read(ws->parent, timeout_ms)) <= 0)
    {
        return poll_read;
    }

    // Receive and process header first (based on header size)
    int header   = 2;
    int mask_len = 4;
    if ((rlen = ws_transport_read(ws->parent, data_ptr, header, timeout_ms)) <= 0)
    {
        WS_LOGE("TRANSPORT_WS", "Error read data\n");

        return rlen;
    }

    ws->frame_state.opcode = (*data_ptr & 0x0F);
    data_ptr++;
    mask        = ((*data_ptr >> 7) & 0x01);
    payload_len = (*data_ptr & 0x7F);
    data_ptr++;
    WS_LOGD("TRANSPORT_WS", "Opcode: %d, mask: %d, len: %d\r\n", ws->frame_state.opcode, mask, payload_len);
    if (payload_len == 126)
    {
        // headerLen += 2;
        if ((rlen = ws_transport_read(ws->parent, data_ptr, header, timeout_ms)) <= 0)
        {
            WS_LOGE("TRANSPORT_WS", "Error read data\n");

            return rlen;
        }

        payload_len = (unsigned char) data_ptr[0] << 8 | (unsigned char) data_ptr[1];
    }
    else if (payload_len == 127)
    {
        // headerLen += 8;
        header = 8;
        if ((rlen = ws_transport_read(ws->parent, data_ptr, header, timeout_ms)) <= 0)
        {
            WS_LOGE("TRANSPORT_WS", "Error read data\n");

            return rlen;
        }

        if ((data_ptr[0] != 0) || (data_ptr[1] != 0) || (data_ptr[2] != 0) || (data_ptr[3] != 0))
        {
            // really too big!
            payload_len = (int) 0xFFFFFFFF;
        }
        else
        {
            payload_len = (unsigned char) data_ptr[4] << 24 | (unsigned char) data_ptr[5] << 16 |
                          (unsigned char) data_ptr[6] << 8 | (unsigned char) data_ptr[7];
        }
    }

    if (mask)
    {
        // Read and store mask
        if ((payload_len != 0) && ((rlen = ws_transport_read(ws->parent, buffer, mask_len, timeout_ms)) <= 0))
        {
            WS_LOGE("TRANSPORT_WS", "Error read data\n");

            return rlen;
        }

        memcpy(ws->frame_state.mask_key, buffer, (size_t) mask_len);
    }
    else
    {
        memset(ws->frame_state.mask_key, 0, (size_t) mask_len);
    }

    ws->frame_state.payload_len     = payload_len;
    ws->frame_state.bytes_remaining = payload_len;

    return payload_len;
}

static int ws_read (ws_transport_handle_t t, char * buffer, int len, int timeout_ms)
{
    int              rlen = 0;
    transport_ws_t * ws   = ws_transport_get_context_data(t);

    // If message exceeds buffer len then subsequent reads will skip reading header and read whatever is left of the payload
    if (ws->frame_state.bytes_remaining <= 0)
    {
        if ((rlen = ws_read_header(t, buffer, len, timeout_ms)) <= 0)
        {
            // If something when wrong then we prepare for reading a new header
            ws->frame_state.bytes_remaining = 0;

            return rlen;
        }
    }

    if (ws->frame_state.payload_len)
    {
        if ((rlen = ws_read_payload(t, buffer, len, timeout_ms)) <= 0)
        {
            WS_LOGE("TRANSPORT_WS", "Error reading payload data\n");
            ws->frame_state.bytes_remaining = 0;

            return rlen;
        }
    }

 #if CFG_PMGR
    if (RM_PMGR_W_dpm_is_enabled())
    {
        RM_PMGR_W_dpm_sleep_ready_set(APP_WEBSOCKET_CLIENT);
    }
 #endif                                /* CFG_PMGR */
    return rlen;
}

static int ws_poll_read (ws_transport_handle_t t, int timeout_ms)
{
    transport_ws_t * ws = ws_transport_get_context_data(t);

    return ws_transport_poll_read(ws->parent, timeout_ms);
}

static int ws_poll_write (ws_transport_handle_t t, int timeout_ms)
{
    transport_ws_t * ws = ws_transport_get_context_data(t);

    return ws_transport_poll_write(ws->parent, timeout_ms);;
}

static int ws_close (ws_transport_handle_t t)
{
    transport_ws_t * ws = ws_transport_get_context_data(t);

    return ws_transport_close(ws->parent);
}

static int ws_destroy (ws_transport_handle_t t)
{
    transport_ws_t * ws = ws_transport_get_context_data(t);
    _ws_free(ws->buffer);
    _ws_free(ws->path);
    _ws_free(ws->sub_protocol);
    _ws_free(ws->user_agent);
    _ws_free(ws->headers);
    _ws_free(ws);

    return 0;
}

void ws_transport_ws_set_path (ws_transport_handle_t t, const char * path)
{
    transport_ws_t * ws = ws_transport_get_context_data(t);
    ws->path = _ws_realloc(ws->path, strlen(path) + 1);
    strcpy(ws->path, path);
}

ws_transport_handle_t ws_transport_ws_init (ws_transport_handle_t parent_handle)
{
    ws_transport_handle_t t  = ws_transport_init();
    transport_ws_t      * ws = _ws_calloc(1, sizeof(transport_ws_t));
    WS_TRANSPORT_MEM_CHECK("TRANSPORT_WS", ws, return NULL);
    ws->parent = parent_handle;

    ws->path = _ws_strdup("/");
    WS_TRANSPORT_MEM_CHECK("TRANSPORT_WS",
                           ws->path,
    {
        _ws_free(ws);
        ws_transport_destroy(t);

        return NULL;
    });
    ws->buffer = pvPortMalloc(DEFAULT_WS_BUFFER);
    WS_TRANSPORT_MEM_CHECK("TRANSPORT_WS",
                           ws->buffer,
    {
        _ws_free(ws->path);
        _ws_free(ws);
        ws_transport_destroy(t);

        return NULL;
    });

    ws_transport_set_func(t, ws_connect, ws_read, ws_write, ws_close, ws_poll_read, ws_poll_write, ws_destroy);

    // webocket underlying transfer is the payload transfer handle
    ws_transport_set_parent_transport_func(t, ws_get_payload_transport_handle);

    ws_transport_set_context_data(t, ws);

    return t;
}

ws_err_t ws_transport_ws_set_subprotocol (ws_transport_handle_t t, const char * sub_protocol)
{
    if (t == NULL)
    {
        return WS_ERR_INVALID_ARG;
    }

    transport_ws_t * ws = ws_transport_get_context_data(t);
    if (ws->sub_protocol)
    {
        _ws_free(ws->sub_protocol);
    }

    if (sub_protocol == NULL)
    {
        ws->sub_protocol = NULL;

        return WS_OK;
    }

    ws->sub_protocol = _ws_strdup(sub_protocol);
    if (ws->sub_protocol == NULL)
    {
        return WS_ERR_NO_MEM;
    }

    return WS_OK;
}

ws_err_t ws_transport_ws_set_user_agent (ws_transport_handle_t t, const char * user_agent)
{
    if (t == NULL)
    {
        return WS_ERR_INVALID_ARG;
    }

    transport_ws_t * ws = ws_transport_get_context_data(t);
    if (ws->user_agent)
    {
        _ws_free(ws->user_agent);
    }

    if (user_agent == NULL)
    {
        ws->user_agent = NULL;

        return WS_OK;
    }

    ws->user_agent = _ws_strdup(user_agent);
    if (ws->user_agent == NULL)
    {
        return WS_ERR_NO_MEM;
    }

    return WS_OK;
}

ws_err_t ws_transport_ws_set_headers (ws_transport_handle_t t, const char * headers)
{
    if (t == NULL)
    {
        return WS_ERR_INVALID_ARG;
    }

    transport_ws_t * ws = ws_transport_get_context_data(t);
    if (ws->headers)
    {
        _ws_free(ws->headers);
    }

    if (headers == NULL)
    {
        ws->headers = NULL;

        return WS_OK;
    }

    ws->headers = _ws_strdup(headers);
    if (ws->headers == NULL)
    {
        return WS_ERR_NO_MEM;
    }

    return WS_OK;
}

ws_transport_opcodes_t ws_transport_ws_get_read_opcode (ws_transport_handle_t t)
{
    transport_ws_t * ws = ws_transport_get_context_data(t);

    return ws->frame_state.opcode;
}

int ws_transport_ws_get_read_payload_len (ws_transport_handle_t t)
{
    transport_ws_t * ws = ws_transport_get_context_data(t);

    return ws->frame_state.payload_len;
}

int ws_transport_ws_poll_connection_closed (ws_transport_handle_t t, int timeout_ms)
{
    struct timeval timeout;
    int            sock = ws_transport_get_socket(t);
    fd_set         readset;
    fd_set         errset;
    FD_ZERO(&readset);
    FD_ZERO(&errset);
    FD_SET((unsigned int) sock, &readset);
    FD_SET((unsigned int) sock, &errset);

    int ret = select(sock + 1, &readset, NULL, &errset, ws_transport_utils_ms_to_timeval(timeout_ms, &timeout));
    if (ret > 0)
    {
        if (FD_ISSET((unsigned int) sock, &readset))
        {
            uint8_t buffer;
            if (recv(sock, &buffer, 1, MSG_PEEK) <= 0)
            {

                // socket is readable, but reads zero bytes -- connection cleanly closed by FIN flag
                return 1;
            }

            WS_LOGW("TRANSPORT_WS",
                    "ws_transport_ws_poll_connection_closed: unexpected data readable on socket=%d",
                    sock);
        }
        else if (FD_ISSET((unsigned int) sock, &errset))
        {
            int      sock_errno = 0;
            uint32_t optlen     = sizeof(sock_errno);
            getsockopt(sock, SOL_SOCKET, SO_ERROR, &sock_errno, &optlen);
            WS_LOGD("TRANSPORT_WS",
                    "ws_transport_ws_poll_connection_closed select error %d, errno = %s, fd = %d",
                    sock_errno,
                    strerror(sock_errno),
                    sock);
            if ((sock_errno == ENOTCONN) || (sock_errno == ECONNRESET) || (sock_errno == ECONNABORTED))
            {
                // the three err codes above might be caused by connection termination by RTS flag

                // which we still assume as expected closing sequence of ws-transport connection
                return 1;
            }

            WS_LOGE("TRANSPORT_WS",
                    "ws_transport_ws_poll_connection_closed: unexpected errno=%d on socket=%d",
                    sock_errno,
                    sock);
        }

        return -1;                     // indicates error: socket unexpectedly reads an actual data, or unexpected errno code
    }

    return ret;
}

#endif                                 // __SUPPORT_WEBSOCKET_CLIENT__

/* EOF */
