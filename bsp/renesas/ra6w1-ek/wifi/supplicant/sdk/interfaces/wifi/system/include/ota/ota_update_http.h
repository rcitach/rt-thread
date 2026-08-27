/**
 ****************************************************************************************
 *
 * @file ota_update_http.h
 *
 * @brief Over the air firmware update by http protocol.
 *
 * Copyright (c) 2016-2022 Renesas Electronics. All rights reserved.
 *
 * This software ("Software") is owned by Renesas Electronics.
 *
 * By using this Software you agree that Renesas Electronics retains all
 * intellectual property and proprietary rights in and to this Software and any
 * use, reproduction, disclosure or distribution of the Software without express
 * written permission or a license agreement from Renesas Electronics is
 * strictly prohibited. This Software is solely for use on or in conjunction
 * with Renesas Electronics products.
 *
 * EXCEPT AS OTHERWISE PROVIDED IN A LICENSE AGREEMENT BETWEEN THE PARTIES, THE
 * SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. EXCEPT AS OTHERWISE
 * PROVIDED IN A LICENSE AGREEMENT BETWEEN THE PARTIES, IN NO EVENT SHALL
 * RENESAS ELECTRONICS BE LIABLE FOR ANY DIRECT, SPECIAL, INDIRECT, INCIDENTAL,
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THE SOFTWARE.
 *
 ****************************************************************************************
 */


#if !defined (__OTA_UPDATE_HTTP_H__)
#define	__OTA_UPDATE_HTTP_H__

#include "FreeRTOS.h"
#include "custom_config_sdk.h"

#include "common_def.h"
#include "ota_update.h"
#include "ota_update_common.h"
#include "iface_defs.h"
#include "common_utils.h"
#include "rm_https_w.h"
#include "rm_https_api.h"
#include "mbedtls/ssl.h"

/// Receive buffer size.
#define OTA_HTTP_RX_BUF_SZ		            OTA_SFLASH_BUF_SZ

#define OTA_HTTPC_MIN_INCOMING_LEN          (1024 * 1)
#define OTA_HTTPC_MAX_INCOMING_LEN          (1024 * 20)
#define OTA_HTTPC_DEF_INCOMING_LEN          (1024 * 4)
#define OTA_HTTPC_MIN_OUTGOING_LEN          (1024 * 1)
#define OTA_HTTPC_MAX_OUTGOING_LEN          (1024 * 20)
#define OTA_HTTPC_DEF_OUTGOING_LEN          (1024 * 4)

#define OTA_HTTPC_MAX_ALPN_CNT              3
#define OTA_HTTPC_MAX_ALPN_LEN              24
#define OTA_HTTPC_MAX_SNI_LEN               64
#define OTA_HTTPC_MAX_STOP_TIMEOUT          (300 * HTTPC_DEF_TIMEOUT)


// Default auth mode value
#define OTA_HTTPC_TLS_AUTHMODE_DEF	        MBEDTLS_SSL_VERIFY_NONE

// Default tls version
#define OTA_HTTPC_TLS_VERSION_DEF	        0 //ONLY_TLS12

// NVRAM name of HTTP-CLIENT TLS version
#define OTA_HTTPC_NVRAM_CONFIG_TLS_VER      "OTA_TLS_VER"
// NVRAM name of HTTP-CLIENT TLS auth_mode
#define OTA_HTTPC_NVRAM_CONFIG_TLS_AUTH     "OTA_TLS_AUTHMODE"
// NVRAM name of HTTP-CLIENT TLS alpn
#define OTA_HTTPC_NVRAM_CONFIG_TLS_ALPN     "OTA_TLS_ALPN"
// NVRAM name of the number of TLS alpn
#define OTA_HTTPC_NVRAM_CONFIG_TLS_ALPN_NUM "OTA_TLS_ALPN_NUM"
// NVRAM name of HTTP-CLIENT TLS SNI
#define OTA_HTTPC_NVRAM_CONFIG_TLS_SNI      "OTA_TLS_SNI"


UINT ota_update_http_client_request(ota_update_proc_t *ota_proc);
UINT ota_http_client_get_downlaod_status(void);
void ota_http_client_set_downlaod_status(UINT status);
UINT ota_http_client_get_result(void);

UINT ota_http_client_set_tls_auth_mode(int tls_auth_mode);
UINT ota_http_client_set_tls_version(int tls_ver);
UINT ota_http_client_set_sni(char *sni);
UINT ota_http_client_set_alpn(char *alpn0, char *alpn1, char *alpn2);

UINT ota_http_client_get_tls_auth_mode(void);
UINT ota_http_client_get_tls_version(void);
size_t ota_http_client_get_sni(char *sni);
size_t ota_http_client_get_alpn0(char *alpn0);
size_t ota_http_client_get_alpn1(char *alpn1);
size_t ota_http_client_get_alpn2(char *alpn2);
void ota_http_client_del_all_alpn(void);

err_t ota_update_httpc_cb_headers_done_fn(httpc_state_t *connection, void *arg, struct pbuf *hdr,  u16_t hdr_len, u32_t content_len);
void ota_update_httpc_cb_result_fn(void *arg, httpc_result_t httpc_result, u32_t rx_content_len, u32_t srv_res, err_t err);

#endif	// (__OTA_UPDATE_HTTP_H__)

/* EOF */
