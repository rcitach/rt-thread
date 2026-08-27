// Copyright 2019 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once
#include "ws_tls.h"

/**
 * Internal Callback API for mbedtls_ssl_read
 */
ssize_t ws_mbedtls_read(ws_tls_t *tls, char *data, size_t datalen);

/**
 * Internal callback API for mbedtls_ssl_write
 */
ssize_t ws_mbedtls_write(ws_tls_t *tls, const char *data, size_t datalen);

/**
 * Internal Callback for mbedtls_handshake
 */
int ws_mbedtls_handshake(ws_tls_t *tls, const ws_tls_cfg_t *cfg);

/**
 * Internal Callback for mbedtls_cleanup , frees up all the memory used by mbedtls
 */
void ws_mbedtls_cleanup(ws_tls_t *tls);

/**
 * Internal Callback for Certificate verification for mbedtls
 */
void ws_mbedtls_verify_certificate(ws_tls_t *tls);

/**
 * Internal Callback for deleting the mbedtls connection
 */
void ws_mbedtls_conn_delete(ws_tls_t *tls);

/**
 * Internal Callback for mbedtls_get_bytes_avail
 */
ssize_t ws_mbedtls_get_bytes_avail(ws_tls_t *tls);

/**
 * Internal Callback for creating ssl handle for mbedtls
 */
ws_err_t ws_create_mbedtls_handle(const char *hostname, size_t hostlen, const void *cfg, ws_tls_t *tls);

#ifdef CONFIG_WS_TLS_SERVER
/**
 * Internal Callback for set_server_config
 *
 * /note :- can only be used with mbedtls ssl library
 */
ws_err_t set_server_config(ws_tls_cfg_server_t *cfg, ws_tls_t *tls);

/**
 * Internal Callback for mbedtls_server_session_create
 *
 * /note :- The function can only be used with mbedtls ssl library
 */
int ws_mbedtls_server_session_create(ws_tls_cfg_server_t *cfg, int sockfd, ws_tls_t *tls);

/**
 * Internal Callback for mbedtls_server_session_delete
 *
 * /note :- The function can only be used with mbedtls ssl library
 */
void ws_mbedtls_server_session_delete(ws_tls_t *tls);
#endif

/**
 * Internal Callback for set_client_config_function
 */
ws_err_t set_client_config(const char *hostname, size_t hostlen, ws_tls_cfg_t *cfg, ws_tls_t *tls);

/**
 * Internal Callback for mbedtls_init_global_ca_store
 */
ws_err_t ws_mbedtls_init_global_ca_store(void);

/**
 * Callback function for setting global CA store data for TLS/SSL using mbedtls
 */
ws_err_t ws_mbedtls_set_global_ca_store(const unsigned char *cacert_pem_buf, const unsigned int cacert_pem_bytes);

/**
 * Internal Callback for ws_tls_global_ca_store
 */
mbedtls_x509_crt *ws_mbedtls_get_global_ca_store(void);

/**
 * Callback function for freeing global ca store for TLS/SSL using mbedtls
 */
void ws_mbedtls_free_global_ca_store(void);
