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

#ifndef _WS_TRANSPORT_SSL_H_
#define _WS_TRANSPORT_SSL_H_

#include "transport.h"
#include "ws_tls.h"
#include "lwip/sockets.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief       Create new SSL transport, the transport handle must be release ws_transport_destroy callback
 *
 * @return      the allocated ws_transport_handle_t, or NULL if the handle can not be allocated
 */
ws_transport_handle_t ws_transport_ssl_init(void);

/**
 * @brief      Set SSL certificate data (as PEM format).
 *             Note that, this function stores the pointer to data, rather than making a copy.
 *             So this data must remain valid until after the connection is cleaned up
 *
 * @param      t     ssl transport
 * @param[in]  data  The pem data
 * @param[in]  len   The length
 */
void ws_transport_ssl_set_cert_data(ws_transport_handle_t t, const char *data, int len);

/**
 * @brief      Set SSL certificate data (as DER format).
 *             Note that, this function stores the pointer to data, rather than making a copy.
 *             So this data must remain valid until after the connection is cleaned up
 *
 * @param      t     ssl transport
 * @param[in]  data  The der data
 * @param[in]  len   The length
 */
void ws_transport_ssl_set_cert_data_der(ws_transport_handle_t t, const char *data, int len);

/**
 * @brief      Enable global CA store for SSL connection
 *
 * @param      t    ssl transport
 */
void ws_transport_ssl_enable_global_ca_store(ws_transport_handle_t t);

/**
 * @brief      Set SSL client certificate data for mutual authentication (as PEM format).
 *             Note that, this function stores the pointer to data, rather than making a copy.
 *             So this data must remain valid until after the connection is cleaned up
 *
 * @param      t     ssl transport
 * @param[in]  data  The pem data
 * @param[in]  len   The length
 */
void ws_transport_ssl_set_client_cert_data(ws_transport_handle_t t, const char *data, int len);

/**
 * @brief      Set SSL client certificate data for mutual authentication (as DER format).
 *             Note that, this function stores the pointer to data, rather than making a copy.
 *             So this data must remain valid until after the connection is cleaned up
 *
 * @param      t     ssl transport
 * @param[in]  data  The der data
 * @param[in]  len   The length
 */
void ws_transport_ssl_set_client_cert_data_der(ws_transport_handle_t t, const char *data, int len);

/**
 * @brief      Set SSL client key data for mutual authentication (as PEM format).
 *             Note that, this function stores the pointer to data, rather than making a copy.
 *             So this data must remain valid until after the connection is cleaned up
 *
 * @param      t     ssl transport
 * @param[in]  data  The pem data
 * @param[in]  len   The length
 */
void ws_transport_ssl_set_client_key_data(ws_transport_handle_t t, const char *data, int len);

/**
 * @brief      Set SSL client key password if the key is password protected. The configured
 *             password is passed to the underlying TLS stack to decrypt the client key
 *
 * @param      t     ssl transport
 * @param[in]  password  Pointer to the password
 * @param[in]  password_len   Password length
 */
void ws_transport_ssl_set_client_key_password(ws_transport_handle_t t, const char *password, int password_len);

/**
 * @brief      Set SSL client key data for mutual authentication (as DER format).
 *             Note that, this function stores the pointer to data, rather than making a copy.
 *             So this data must remain valid until after the connection is cleaned up
 *
 * @param      t     ssl transport
 * @param[in]  data  The der data
 * @param[in]  len   The length
 */
void ws_transport_ssl_set_client_key_data_der(ws_transport_handle_t t, const char *data, int len);

/**
 * @brief      Set the list of supported application protocols to be used with ALPN.
 *             Note that, this function stores the pointer to data, rather than making a copy.
 *             So this data must remain valid until after the connection is cleaned up
 *
 * @param      t            ssl transport
 * @param[in]  alpn_porot   The list of ALPN protocols, the last entry must be NULL
 */
void ws_transport_ssl_set_alpn_protocol(ws_transport_handle_t t, const char **alpn_protos);

/**
 * @brief      Skip validation of certificate's common name field
 *
 * @note       Skipping CN validation is not recommended
 *
 * @param      t     ssl transport
 */
void ws_transport_ssl_skip_common_name_check(ws_transport_handle_t t);

/**
 * @brief      Set the ssl context to use secure element (atecc608a) for client(device) private key and certificate
 *
  *
 * @param      t     ssl transport
 */
void ws_transport_ssl_use_secure_element(ws_transport_handle_t t);

/**
 * @brief      Set PSK key and hint for PSK server/client verification in esp-tls component.
 *             Important notes:
 *             - This function stores the pointer to data, rather than making a copy.
 *             So this data must remain valid until after the connection is cleaned up
 *             - WS_TLS_PSK_VERIFICATION config option must be enabled in menuconfig
 *             - certificate verification takes priority so it must not be configured
 *             to enable PSK method.
 *
 * @param      t             ssl transport
 * @param[in]  psk_hint_key  psk key and hint structure defined in ws_tls.h
 */
void ws_transport_ssl_set_psk_key_hint(ws_transport_handle_t t, const psk_hint_key_t* psk_hint_key);

/**
 * @brief      Set keep-alive status in current ssl context
 *
 * @param[in]  t               ssl transport
 * @param[in]  keep_alive_cfg  The handle for keep-alive configuration
 */
void ws_transport_ssl_set_keep_alive(ws_transport_handle_t t, ws_transport_keep_alive_t *keep_alive_cfg);

void ws_transport_tcp_set_interface_name(ws_transport_handle_t t, struct ifreq *if_name);

#ifdef __cplusplus
}
#endif
#endif /* _WS_TRANSPORT_SSL_H_ */

