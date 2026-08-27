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

#include "lwip/netdb.h"
#include "lwip/sockets.h"

#include "http_parser.h"
#include "ws_tls_mbedtls.h"
#include "ws_tls_error_capture_internal.h"
#include <errno.h>
#include "ws_log.h"
#include "ws_mem.h"

#include "websocket_client.h"

#ifdef CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#include "ws_crt_bundle.h"
#endif

#ifdef CONFIG_WS_TLS_USE_SECURE_ELEMENT

#define ATECC608A_TNG_SLAVE_ADDR        0x6A
#define ATECC608A_TFLEX_SLAVE_ADDR      0x6C
#define ATECC608A_TCUSTOM_SLAVE_ADDR    0xC0
/* cryptoauthlib includes */
#include "mbedtls/atca_mbedtls_wrap.h"
#include "tng_atca.h"
#include "cryptoauthlib.h"
static const atcacert_def_t* cert_def = NULL;
/* Prototypes for functions */
static ws_err_t ws_set_atecc608a_pki_context(ws_tls_t *tls, ws_tls_cfg_t *cfg);
#endif /* CONFIG_WS_TLS_USE_SECURE_ELEMENT */


static mbedtls_x509_crt *global_cacert = NULL;

typedef struct ws_tls_pki_t {
    mbedtls_x509_crt *public_cert;
    mbedtls_pk_context *pk_key;
    const unsigned char *publiccert_pem_buf;
    unsigned int publiccert_pem_bytes;
    const unsigned char *privkey_pem_buf;
    unsigned int privkey_pem_bytes;
    const unsigned char *privkey_password;
    unsigned int privkey_password_len;
} ws_tls_pki_t;

#ifdef WS_MBEDTLS_DEBUG
static void ws_secure_debug(void *ctx, int level, const char *file, int line, const char *str)
{
    RA6W1_UNUSED_ARG(ctx);

    const char *p, *basename;

    /* Extract basename from file */
    for ( p = basename = file; *p != '\0'; p++ ) {
        if ( *p == '/' || *p == '\\' ) {
            basename = p + 1;
        }
    }

    printf("%s:%04d: |%d| %s\n", basename, line, level, str );
}
#endif //WS_MBEDTLS_DEBUG

ws_err_t ws_create_mbedtls_handle(const char *hostname, size_t hostlen, const void *cfg, ws_tls_t *tls)
{
    assert(cfg != NULL);
    assert(tls != NULL);
    int ret;
    ws_err_t ws_ret = WS_FAIL;
    tls->server_fd.fd = tls->sockfd;
    mbedtls_ssl_init(&tls->ssl);
    mbedtls_ctr_drbg_init(&tls->ctr_drbg);
    mbedtls_ssl_config_init(&tls->conf);
    mbedtls_entropy_init(&tls->entropy);

	if ((ret = mbedtls_ctr_drbg_seed(&tls->ctr_drbg,
									 mbedtls_entropy_func, &tls->entropy, NULL, 0)) != 0) {
		WS_LOGE("WS-TLS-mbedTLS", "mbedtls_ctr_drbg_seed returned -0x%x\n", -ret);
		WS_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ERR_TYPE_MBEDTLS, -ret);
		ws_ret = WS_ERR_MBEDTLS_CTR_DRBG_SEED_FAILED;
		goto exit;
	}

    if (tls->role == WS_TLS_CLIENT) {
        ws_ret = set_client_config(hostname, hostlen, (ws_tls_cfg_t *)cfg, tls);
        if (ws_ret != WS_OK) {
            WS_LOGE("WS-TLS-mbedTLS", "Failed to set client configurations\n");
            goto exit;
        }
    } else if (tls->role == WS_TLS_SERVER) {
#ifdef CONFIG_WS_TLS_SERVER
        ws_ret = set_server_config((ws_tls_cfg_server_t *) cfg, tls);
        if (ws_ret != 0) {
            WS_LOGE("WS-TLS-mbedTLS", "Failed to set server configurations\n");
            goto exit;
        }
#else
            WS_LOGE("WS-TLS-mbedTLS", "WS_TLS_SERVER Not enabled in Kconfig\n");
            goto exit;
#endif
    }

    mbedtls_ssl_conf_rng(&tls->conf, mbedtls_ctr_drbg_random, &tls->ctr_drbg);

#ifdef WS_MBEDTLS_DEBUG
    mbedtls_debug_set_threshold(4); // 0 ~ 4
    mbedtls_ssl_conf_dbg(&tls->conf, ws_secure_debug, NULL);
#endif

    //set TLS v1.2
    mbedtls_ssl_conf_min_tls_version(&tls->conf,MBEDTLS_SSL_VERSION_TLS1_2);
    mbedtls_ssl_conf_max_tls_version(&tls->conf,MBEDTLS_SSL_VERSION_TLS1_2);


    ret = mbedtls_ssl_conf_content_len(&tls->conf, WEBSOCKET_TLS_BUFFER_SIZE_BYTE, WEBSOCKET_TLS_BUFFER_SIZE_BYTE);
	if (ret != 0)
	{
		ws_ret = WS_ERR_MBEDTLS_SSL_SETUP_FAILED;
		goto exit;
	}

    if ((ret = mbedtls_ssl_setup(&tls->ssl, &tls->conf)) != 0) {
        WS_LOGE("WS-TLS-mbedTLS", "mbedtls_ssl_setup returned -0x%x\n", -ret);
        WS_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ERR_TYPE_MBEDTLS, -ret);
        ws_ret = WS_ERR_MBEDTLS_SSL_SETUP_FAILED;
        goto exit;
    }
    mbedtls_ssl_set_bio(&tls->ssl, &tls->server_fd, mbedtls_net_send, mbedtls_net_recv, NULL);

    return WS_OK;

exit:
    ws_mbedtls_cleanup(tls);
    return ws_ret;

}

int ws_mbedtls_handshake(ws_tls_t *tls, const ws_tls_cfg_t *cfg)
{
    int ret;
    ret = mbedtls_ssl_handshake(&tls->ssl);
    if (ret == 0) {
        tls->conn_state = WS_TLS_DONE;
        return 1;
    } else {
        if (ret != WS_TLS_ERR_SSL_WANT_READ && ret != WS_TLS_ERR_SSL_WANT_WRITE) {
            WS_LOGE("WS-TLS-mbedTLS", "mbedtls_ssl_handshake returned -0x%x\n", -ret);
            WS_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ERR_TYPE_MBEDTLS, -ret);
            WS_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ERR_TYPE_ESP, WS_ERR_MBEDTLS_SSL_HANDSHAKE_FAILED);
            if (cfg->cacert_buf != NULL || cfg->use_global_ca_store == true) {
                /* This is to check whether handshake failed due to invalid certificate*/
                ws_mbedtls_verify_certificate(tls);
            }
            tls->conn_state = WS_TLS_FAIL;
            return -1;
        }
        /* Irrespective of blocking or non-blocking I/O, we return on getting WS_TLS_ERR_SSL_WANT_READ
        or WS_TLS_ERR_SSL_WANT_WRITE during handshake */
        return 0;
    }
}

ssize_t ws_mbedtls_read(ws_tls_t *tls, char *data, size_t datalen)
{

    ssize_t ret = mbedtls_ssl_read(&tls->ssl, (unsigned char *)data, datalen);
    if (ret < 0) {
        if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
            return 0;
        }
        if (ret != WS_TLS_ERR_SSL_WANT_READ  && ret != WS_TLS_ERR_SSL_WANT_WRITE) {
            WS_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ERR_TYPE_MBEDTLS, -ret);
            WS_LOGE("WS-TLS-mbedTLS", "read error :%d:\n", ret);
        }
    }
    return ret;
}

ssize_t ws_mbedtls_write(ws_tls_t *tls, const char *data, size_t datalen)
{
    size_t written = 0;
    size_t write_len = datalen;
    while (written < datalen) {
#ifdef _ENABLE_UNUSED_
        if (write_len > MBEDTLS_SSL_OUT_CONTENT_LEN) {
            write_len = MBEDTLS_SSL_OUT_CONTENT_LEN;
        }
        if (datalen > MBEDTLS_SSL_OUT_CONTENT_LEN) {
            WS_LOGD("WS-TLS-mbedTLS", "Fragmenting data of excessive size :%d, offset: %d, size %d", datalen, written, write_len);
        }
#endif
        ssize_t ret = mbedtls_ssl_write(&tls->ssl, (unsigned char*) data + written, write_len);
        if (ret <= 0) {
            if (ret != WS_TLS_ERR_SSL_WANT_READ  && ret != WS_TLS_ERR_SSL_WANT_WRITE && ret != 0) {
                WS_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ERR_TYPE_MBEDTLS, -ret);
                WS_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ERR_TYPE_ESP, WS_ERR_MBEDTLS_SSL_WRITE_FAILED);
                WS_LOGE("WS-TLS-mbedTLS", "write error :%d:\n", ret);
                return ret;
            } else {
                // Exitting the tls-write process as less than desired datalen are writable
                WS_LOGD("WS-TLS-mbedTLS", "mbedtls_ssl_write() returned %d, already written %d, exitting...\n", ret, written);
                return (ssize_t)written;
            }
        }
        written += (size_t)ret;
        write_len = datalen - written;
    }
    return (ssize_t)written;
}

void ws_mbedtls_conn_delete(ws_tls_t *tls)
{
    if (tls != NULL) {
        ws_mbedtls_cleanup(tls);
        if (tls->is_tls) {
            mbedtls_net_free(&tls->server_fd);
            tls->sockfd = tls->server_fd.fd;
        }
    }
}

void ws_mbedtls_verify_certificate(ws_tls_t *tls)
{
    int flags;
    char buf[100];
    if ((flags = (int)mbedtls_ssl_get_verify_result(&tls->ssl)) != 0) {
        WS_LOGI("WS-TLS-mbedTLS", "Failed to verify peer certificate!\n");
        WS_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ERR_TYPE_MBEDTLS_CERT_FLAGS, flags);
        bzero(buf, sizeof(buf));
        mbedtls_x509_crt_verify_info(buf, sizeof(buf), "  ! ", (uint32_t)flags);
        WS_LOGI("WS-TLS-mbedTLS", "verification info: %s\n", buf);
    } else {
        WS_LOGI("WS-TLS-mbedTLS", "Certificate verified.\n");
    }
}

ssize_t ws_mbedtls_get_bytes_avail(ws_tls_t *tls)
{
    if (!tls) {
        WS_LOGE("WS-TLS-mbedTLS", "empty arg passed to ws_tls_get_bytes_avail()\n");
        return WS_FAIL;
    }
    return (ssize_t)mbedtls_ssl_get_bytes_avail(&tls->ssl);
}

void ws_mbedtls_cleanup(ws_tls_t *tls)
{
    if (!tls) {
        return;
    }
    if (tls->cacert_ptr != global_cacert) {
        mbedtls_x509_crt_free(tls->cacert_ptr);
    }
    tls->cacert_ptr = NULL;
#ifdef CONFIG_WS_TLS_SERVER
    mbedtls_x509_crt_free(&tls->servercert);
    mbedtls_pk_free(&tls->serverkey);
#endif
    mbedtls_x509_crt_free(&tls->cacert);
    mbedtls_x509_crt_free(&tls->clientcert);
    mbedtls_pk_free(&tls->clientkey);
    mbedtls_entropy_free(&tls->entropy);
    mbedtls_ssl_config_free(&tls->conf);
    mbedtls_ctr_drbg_free(&tls->ctr_drbg);
    mbedtls_ssl_free(&tls->ssl);
#ifdef CONFIG_WS_TLS_USE_SECURE_ELEMENT
    atcab_release();
#endif
}

static ws_err_t set_ca_cert(ws_tls_t *tls, const unsigned char *cacert, size_t cacert_len)
{
    assert(tls);
    tls->cacert_ptr = &tls->cacert;
    mbedtls_x509_crt_init(tls->cacert_ptr);
    int ret = mbedtls_x509_crt_parse(tls->cacert_ptr, cacert, cacert_len);
    if (ret < 0) {
        WS_LOGE("WS-TLS-mbedTLS", "mbedtls_x509_crt_parse returned -0x%x\n", -ret);
        WS_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ERR_TYPE_MBEDTLS, -ret);
        return WS_ERR_MBEDTLS_X509_CRT_PARSE_FAILED;
    }
    if (ret > 0) {
        /* This will happen if the CA chain contains one or more invalid certs, going ahead as the hadshake
         * may still succeed if the other certificates in the CA chain are enough for the authentication */
        WS_LOGW("WS-TLS-mbedTLS", "mbedtls_x509_crt_parse was partly successful. No. of failed certificates: %d\n", ret);
    }
    mbedtls_ssl_conf_authmode(&tls->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&tls->conf, tls->cacert_ptr, NULL);
    return WS_OK;
}

static ws_err_t set_pki_context(ws_tls_t *tls, const ws_tls_pki_t *pki)
{
    assert(tls);
    assert(pki);
    int ret;

    if (pki->publiccert_pem_buf != NULL &&
        pki->privkey_pem_buf != NULL &&
        pki->public_cert != NULL &&
        pki->pk_key != NULL) {

        mbedtls_x509_crt_init(pki->public_cert);
        mbedtls_pk_init(pki->pk_key);

        ret = mbedtls_x509_crt_parse(pki->public_cert, pki->publiccert_pem_buf, pki->publiccert_pem_bytes);
        if (ret < 0) {
            WS_LOGE("WS-TLS-mbedTLS", "mbedtls_x509_crt_parse returned -0x%x\n", -ret);
            WS_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ERR_TYPE_MBEDTLS, -ret);
            return WS_ERR_MBEDTLS_X509_CRT_PARSE_FAILED;
        }

        ret = mbedtls_pk_parse_key(pki->pk_key, pki->privkey_pem_buf, pki->privkey_pem_bytes,
                                   pki->privkey_password, pki->privkey_password_len,
                                   mbedtls_ctr_drbg_random, &tls->ctr_drbg);

        if (ret < 0) {
            WS_LOGE("WS-TLS-mbedTLS", "mbedtls_pk_parse_keyfile returned -0x%x\n", -ret);
            WS_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ERR_TYPE_MBEDTLS, -ret);
            return WS_ERR_MBEDTLS_PK_PARSE_KEY_FAILED;
        }

        ret = mbedtls_ssl_conf_own_cert(&tls->conf, pki->public_cert, pki->pk_key);
        if (ret < 0) {
            WS_LOGE("WS-TLS-mbedTLS", "mbedtls_ssl_conf_own_cert returned -0x%x\n", -ret);
            WS_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ERR_TYPE_MBEDTLS, -ret);
            return WS_ERR_MBEDTLS_SSL_CONF_OWN_CERT_FAILED;
        }
    } else {
        return WS_ERR_INVALID_ARG;
    }
    return WS_OK;
}

static ws_err_t set_global_ca_store(ws_tls_t *tls)
{
    assert(tls);
    if (global_cacert == NULL) {
        WS_LOGE("WS-TLS-mbedTLS", "global_cacert is NULL\n");
        return WS_ERR_INVALID_STATE;
    }
    tls->cacert_ptr = global_cacert;
    mbedtls_ssl_conf_authmode(&tls->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&tls->conf, tls->cacert_ptr, NULL);
    return WS_OK;
}


#ifdef CONFIG_WS_TLS_SERVER
ws_err_t set_server_config(ws_tls_cfg_server_t *cfg, ws_tls_t *tls)
{
    assert(cfg != NULL);
    assert(tls != NULL);
    int ret;
    ws_err_t ws_ret;
    int preset = MBEDTLS_SSL_PRESET_RA6WX;

#if defined(__SUPPORT_TLS_HW_CIPHER_SUITES__)
    preset = MBEDTLS_SSL_PRESET_RA6WX_ONLY_HW;
#endif /* __SUPPORT_TLS_HW_CIPHER_SUITES__ */

    if ((ret = mbedtls_ssl_config_defaults(&tls->conf,
                    MBEDTLS_SSL_IS_SERVER,
                    MBEDTLS_SSL_TRANSPORT_STREAM,
                    preset)) != 0) {
        WS_LOGE("WS-TLS-mbedTLS", "mbedtls_ssl_config_defaults returned %d\n", ret);
        WS_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ERR_TYPE_MBEDTLS, -ret);
        return WS_ERR_MBEDTLS_SSL_CONFIG_DEFAULTS_FAILED;
    }

#ifdef CONFIG_MBEDTLS_SSL_ALPN
    if (cfg->alpn_protos) {
        mbedtls_ssl_conf_alpn_protocols(&tls->conf, cfg->alpn_protos);
    }
#endif

    if (cfg->cacert_buf != NULL) {
        ws_ret = set_ca_cert(tls, cfg->cacert_buf, cfg->cacert_bytes);
        if (ws_ret != WS_OK) {
            return ws_ret;
        }
    } else {
        mbedtls_ssl_conf_authmode(&tls->conf, MBEDTLS_SSL_VERIFY_NONE);
    }

    if (cfg->servercert_buf != NULL && cfg->serverkey_buf != NULL) {
        ws_tls_pki_t pki = {
            .public_cert = &tls->servercert,
            .pk_key = &tls->serverkey,
            .publiccert_pem_buf = cfg->servercert_buf,
            .publiccert_pem_bytes = cfg->servercert_bytes,
            .privkey_pem_buf = cfg->serverkey_buf,
            .privkey_pem_bytes = cfg->serverkey_bytes,
            .privkey_password = cfg->serverkey_password,
            .privkey_password_len = cfg->serverkey_password_len,
        };
        ws_ret = set_pki_context(tls, &pki);
        if (ws_ret != WS_OK) {
            WS_LOGE("WS-TLS-mbedTLS", "Failed to set server pki context\n");
            return ws_ret;
        }
    } else {
        WS_LOGE("WS-TLS-mbedTLS", "Missing server certificate and/or key\n");
        return WS_ERR_INVALID_STATE;
    }
    return WS_OK;
}
#endif /* ! CONFIG_WS_TLS_SERVER */

ws_err_t set_client_config(const char *hostname, size_t hostlen, ws_tls_cfg_t *cfg, ws_tls_t *tls)
{
    assert(cfg != NULL);
    assert(tls != NULL);
    int ret;
    int preset = MBEDTLS_SSL_PRESET_RA6WX;

#if defined(__SUPPORT_TLS_HW_CIPHER_SUITES__)
    preset = MBEDTLS_SSL_PRESET_RA6WX_ONLY_HW;
#endif /* __SUPPORT_TLS_HW_CIPHER_SUITES__ */

    if (!cfg->skip_common_name) {
        char *use_host = NULL;
        if (cfg->common_name != NULL) {
            use_host = _ws_strndup(cfg->common_name, strlen(cfg->common_name));
        } else {
            use_host = _ws_strndup(hostname, hostlen);
        }

        if (use_host == NULL) {
            return WS_ERR_NO_MEM;
        }
        /* Hostname set here should match CN in server certificate */
        if ((ret = mbedtls_ssl_set_hostname(&tls->ssl, use_host)) != 0) {
            WS_LOGE("WS-TLS-mbedTLS", "mbedtls_ssl_set_hostname returned -0x%x\n", -ret);
            WS_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ERR_TYPE_MBEDTLS, -ret);
            _ws_free(use_host);
            return WS_ERR_MBEDTLS_SSL_SET_HOSTNAME_FAILED;
        }
        _ws_free(use_host);
    }

    if ((ret = mbedtls_ssl_config_defaults(&tls->conf,
                                           MBEDTLS_SSL_IS_CLIENT,
                                           MBEDTLS_SSL_TRANSPORT_STREAM,
                                           preset)) != 0) {
        WS_LOGE("WS-TLS-mbedTLS", "mbedtls_ssl_config_defaults returned -0x%x\n", -ret);
        WS_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ERR_TYPE_MBEDTLS, -ret);
        return WS_ERR_MBEDTLS_SSL_CONFIG_DEFAULTS_FAILED;
    }

#ifdef CONFIG_MBEDTLS_SSL_RENEGOTIATION
    mbedtls_ssl_conf_renegotiation(&tls->conf, MBEDTLS_SSL_RENEGOTIATION_ENABLED);
#endif

    if (cfg->alpn_protos) {
#ifdef CONFIG_MBEDTLS_SSL_ALPN
        if ((ret = mbedtls_ssl_conf_alpn_protocols(&tls->conf, cfg->alpn_protos)) != 0) {
            WS_LOGE("WS-TLS-mbedTLS", "mbedtls_ssl_conf_alpn_protocols returned -0x%x\n", -ret);
            WS_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ERR_TYPE_MBEDTLS, -ret);
            return WS_ERR_MBEDTLS_SSL_CONF_ALPN_PROTOCOLS_FAILED;
        }
#else
        WS_LOGE("WS-TLS-mbedTLS", "alpn_protos configured but not enabled in menuconfig: Please enable MBEDTLS_SSL_ALPN option\n");
        return WS_ERR_INVALID_STATE;
#endif
    }

    if (cfg->crt_bundle_attach != NULL) {
#ifdef CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
        WS_LOGD("WS-TLS-mbedTLS", "Use certificate bundle\n");
        mbedtls_ssl_conf_authmode(&tls->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
        cfg->crt_bundle_attach(&tls->conf);
#else //CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
        WS_LOGE("WS-TLS-mbedTLS", "use_crt_bundle configured but not enabled in menuconfig: Please enable MBEDTLS_CERTIFICATE_BUNDLE option\n");
        return WS_ERR_INVALID_STATE;
#endif
    } else if (cfg->use_global_ca_store == true) {
        ws_err_t ws_ret = set_global_ca_store(tls);
        if (ws_ret != WS_OK) {
            return ws_ret;
        }
    } else if (cfg->cacert_buf != NULL) {
        ws_err_t ws_ret = set_ca_cert(tls, cfg->cacert_buf, cfg->cacert_bytes);
        if (ws_ret != WS_OK) {
            return ws_ret;
        }
        mbedtls_ssl_conf_ca_chain(&tls->conf, tls->cacert_ptr, NULL);
    } else if (cfg->psk_hint_key) {
#if defined(CONFIG_WS_TLS_PSK_VERIFICATION)
        //
        // PSK encryption mode is configured only if no certificate supplied and psk pointer not null
        WS_LOGD("WS-TLS-mbedTLS", "ssl psk authentication\n");
        ret = mbedtls_ssl_conf_psk(&tls->conf, cfg->psk_hint_key->key, cfg->psk_hint_key->key_size,
                                   (const unsigned char *)cfg->psk_hint_key->hint, strlen(cfg->psk_hint_key->hint));
        if (ret != 0) {
            WS_LOGE("WS-TLS-mbedTLS", "mbedtls_ssl_conf_psk returned -0x%x\n", -ret);
            WS_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ERR_TYPE_MBEDTLS, -ret);
            return WS_ERR_MBEDTLS_SSL_CONF_PSK_FAILED;
        }
#else
        WS_LOGE("WS-TLS-mbedTLS", "psk_hint_key configured but not enabled in menuconfig: Please enable WS_TLS_PSK_VERIFICATION option\n");
        return WS_ERR_INVALID_STATE;
#endif
    } else {
        mbedtls_ssl_conf_authmode(&tls->conf, MBEDTLS_SSL_VERIFY_NONE);
    }

    if (cfg->use_secure_element) {
#ifdef CONFIG_WS_TLS_USE_SECURE_ELEMENT
        ret = ws_set_atecc608a_pki_context(tls, (ws_tls_cfg_t *)cfg);
        if (ret != WS_OK) {
            return ret;
        }
#else
        WS_LOGE("WS-TLS-mbedTLS", "Please enable secure element support for ESP-TLS in menuconfig\n");
        return WS_FAIL;
#endif /* CONFIG_WS_TLS_USE_SECURE_ELEMENT */
    } else if (cfg->clientcert_pem_buf != NULL && cfg->clientkey_pem_buf != NULL) {
        ws_tls_pki_t pki = {
            .public_cert = &tls->clientcert,
            .pk_key = &tls->clientkey,
            .publiccert_pem_buf = cfg->clientcert_buf,
            .publiccert_pem_bytes = cfg->clientcert_bytes,
            .privkey_pem_buf = cfg->clientkey_buf,
            .privkey_pem_bytes = cfg->clientkey_bytes,
            .privkey_password = cfg->clientkey_password,
            .privkey_password_len = cfg->clientkey_password_len,
        };
        ws_err_t ws_ret = set_pki_context(tls, &pki);
        if (ws_ret != WS_OK) {
            WS_LOGE("WS-TLS-mbedTLS", "Failed to set client pki context\n");
            return ws_ret;
        }
    } else if (cfg->clientcert_buf != NULL || cfg->clientkey_buf != NULL) {
        WS_LOGE("WS-TLS-mbedTLS", "You have to provide both clientcert_buf and clientkey_buf for mutual authentication\n");
        return WS_ERR_INVALID_STATE;
    }
    return WS_OK;
}

#ifdef CONFIG_WS_TLS_SERVER
/**
 * @brief      Create TLS/SSL server session
 */
int ws_mbedtls_server_session_create(ws_tls_cfg_server_t *cfg, int sockfd, ws_tls_t *tls)
{
    if (tls == NULL || cfg == NULL) {
        return -1;
    }
    tls->role = WS_TLS_SERVER;
    tls->sockfd = sockfd;
    ws_err_t ws_ret = ws_create_mbedtls_handle(NULL, 0, cfg, tls);
    if (ws_ret != WS_OK) {
        WS_LOGE("WS-TLS-mbedTLS", "create_ssl_handle failed\n");
        WS_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ERR_TYPE_ESP, ws_ret);
        tls->conn_state = WS_TLS_FAIL;
        return -1;
    }
    tls->read_f = ws_mbedtls_read;
    tls->write_f = ws_mbedtls_write;
    int ret;
    while ((ret = mbedtls_ssl_handshake(&tls->ssl)) != 0) {
        if (ret != WS_TLS_ERR_SSL_WANT_READ && ret != WS_TLS_ERR_SSL_WANT_WRITE) {
            WS_LOGE("WS-TLS-mbedTLS", "mbedtls_ssl_handshake returned %d\n", ret);
            tls->conn_state = WS_TLS_FAIL;
            return ret;
        }
    }
    return 0;
}
/**
 * @brief      Close the server side TLS/SSL connection and free any allocated resources.
 */
void ws_mbedtls_server_session_delete(ws_tls_t *tls)
{
    if (tls != NULL) {
        ws_mbedtls_cleanup(tls);
        _ws_free(tls);
    }
};
#endif /* ! CONFIG_WS_TLS_SERVER */

ws_err_t ws_mbedtls_init_global_ca_store(void)
{
    if (global_cacert == NULL) {
        global_cacert = (mbedtls_x509_crt *)_ws_calloc(1, sizeof(mbedtls_x509_crt));
        if (global_cacert == NULL) {
            WS_LOGE("WS-TLS-mbedTLS", "global_cacert not allocated\n");
            return WS_ERR_NO_MEM;
        }
        mbedtls_x509_crt_init(global_cacert);
    }
    return WS_OK;
}

ws_err_t ws_mbedtls_set_global_ca_store(const unsigned char *cacert_pem_buf, const unsigned int cacert_pem_bytes)
{
#ifdef CONFIG_MBEDTLS_DYNAMIC_FREE_CA_CERT
    WS_LOGE("WS-TLS-mbedTLS", "Please disable dynamic freeing of ca cert in mbedtls (CONFIG_MBEDTLS_DYNAMIC_FREE_CA_CERT)\n in order to use the global ca_store\n");
    return WS_FAIL;
#endif
    if (cacert_pem_buf == NULL) {
        WS_LOGE("WS-TLS-mbedTLS", "cacert_pem_buf is null\n");
        return WS_ERR_INVALID_ARG;
    }
    int ret;
    if (global_cacert == NULL) {
        ret = ws_mbedtls_init_global_ca_store();
        if (ret != WS_OK) {
            return ret;
        }
    }
    ret = mbedtls_x509_crt_parse(global_cacert, cacert_pem_buf, cacert_pem_bytes);
    if (ret < 0) {
        WS_LOGE("WS-TLS-mbedTLS", "mbedtls_x509_crt_parse returned -0x%x\n", -ret);
        mbedtls_x509_crt_free(global_cacert);
        _ws_free(global_cacert);
        global_cacert = NULL;
        return WS_FAIL;
    } else if (ret > 0) {
        WS_LOGE("WS-TLS-mbedTLS", "mbedtls_x509_crt_parse was partly successful. No. of failed certificates: %d\n", ret);
        return WS_ERR_MBEDTLS_CERT_PARTLY_OK;
    }
    return WS_OK;
}

mbedtls_x509_crt *ws_mbedtls_get_global_ca_store(void)
{
    return global_cacert;
}

void ws_mbedtls_free_global_ca_store(void)
{
    if (global_cacert) {
        mbedtls_x509_crt_free(global_cacert);
        _ws_free(global_cacert);
        global_cacert = NULL;
    }
}

#ifdef CONFIG_WS_TLS_USE_SECURE_ELEMENT
static ws_err_t ws_init_atecc608a(uint8_t slave_addr)
{
    cfg_ateccx08a_i2c_default.atcai2c.slave_address = slave_addr;
    int ret = atcab_init(&cfg_ateccx08a_i2c_default);
    if(ret != 0) {
        WS_LOGE("WS-TLS-mbedTLS", "Failed\n !atcab_init returned %02x\n", ret);
        return WS_FAIL;
    }
    return WS_OK;
}

static ws_err_t ws_set_atecc608a_pki_context(ws_tls_t *tls, ws_tls_cfg_t *cfg)
{
    int ret = 0;
    int ws_ret = WS_FAIL;
    WS_LOGI("WS-TLS-mbedTLS", "Initialize the ATECC interface...\n");
#if defined(CONFIG_ATECC608A_TNG) || defined(CONFIG_ATECC608A_TFLEX)
#ifdef CONFIG_ATECC608A_TNG
    ws_ret = ws_init_atecc608a(ATECC608A_TNG_SLAVE_ADDR);
    if (ret != WS_OK) {
        return WS_ERR_WS_TLS_SE_FAILED;
    }
#elif CONFIG_ATECC608A_TFLEX /* CONFIG_ATECC608A_TNG */
    ws_ret = ws_init_atecc608a(ATECC608A_TFLEX_SLAVE_ADDR);
    if (ret != WS_OK) {
        return WS_ERR_WS_TLS_SE_FAILED;
    }
#endif /* CONFIG_ATECC608A_TFLEX */
    mbedtls_x509_crt_init(&tls->clientcert);
    ret = tng_get_device_cert_def(&cert_def);
    if (ret != 0) {
        WS_LOGE("WS-TLS-mbedTLS", "Failed to get device cert def\n");
        return WS_ERR_WS_TLS_SE_FAILED;
    }

    /* Extract the device certificate and convert to mbedtls cert */
    ret = atca_mbedtls_cert_add(&tls->clientcert, cert_def);
    if (ret != 0) {
        WS_LOGE("WS-TLS-mbedTLS", "Failed to parse cert from device, return %02x\n", ret);
        return WS_ERR_WS_TLS_SE_FAILED;
    }
#elif CONFIG_ATECC608A_TCUSTOM
    ws_ret = ws_init_atecc608a(ATECC608A_TCUSTOM_SLAVE_ADDR);
    if (ret != WS_OK) {
        return WS_ERR_WS_TLS_SE_FAILED;
    }
    mbedtls_x509_crt_init(&tls->clientcert);

    if(cfg->clientcert_buf != NULL) {
        ret = mbedtls_x509_crt_parse(&tls->clientcert, (const unsigned char*)cfg->clientcert_buf, cfg->clientcert_bytes);
        if (ret < 0) {
            WS_LOGE("WS-TLS-mbedTLS", "mbedtls_x509_crt_parse returned -0x%x\n", -ret);
            WS_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ERR_TYPE_MBEDTLS, -ret);
            return WS_ERR_MBEDTLS_X509_CRT_PARSE_FAILED;
        }
    } else {
        WS_LOGE("WS-TLS-mbedTLS", "Device certificate must be provided for TrustCustom Certs\n");
        return WS_FAIL;
    }
#endif /* CONFIG_ATECC608A_TCUSTOM */
    ret = atca_mbedtls_pk_init(&tls->clientkey, 0);
    if (ret != 0) {
        WS_LOGE("WS-TLS-mbedTLS", "Failed to parse key from device\n");
        WS_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ERR_TYPE_MBEDTLS, -ret);
        return WS_ERR_WS_TLS_SE_FAILED;
    }

    ret = mbedtls_ssl_conf_own_cert(&tls->conf, &tls->clientcert, &tls->clientkey);
    if (ret != 0) {
        WS_LOGE("WS-TLS-mbedTLS", "Failed\n ! mbedtls_ssl_conf_own_cert returned -0x%x\n", ret);
        WS_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ERR_TYPE_MBEDTLS, -ret);
        return WS_ERR_WS_TLS_SE_FAILED;
    }
    return WS_OK;
}
#endif /* CONFIG_WS_TLS_USE_SECURE_ELEMENT */

#endif // __SUPPORT_WEBSOCKET_CLIENT__

/* EOF */
