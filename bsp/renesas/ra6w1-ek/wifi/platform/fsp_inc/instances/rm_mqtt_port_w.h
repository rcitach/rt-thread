/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/*******************************************************************************************************************//**
 * @addtogroup MQTT_PORT_W
 * @{
 **********************************************************************************************************************/

#ifndef RM_MQTT_PORT_W_H
#define RM_MQTT_PORT_W_H

#include "bsp_api.h"
#include "rm_mqtt_port_w_cfg.h"
#if MQTT_PORT_W_CFG_WATCHDOG_SERVICE_ENABLE
 #include "rm_watchdog_service_w.h"
#endif

#define MQTT_CLIENT_MAX_ALPN                (3)    ///< Maximum number of ALPNs supported by RRQ61000.
#define MQTT_CLIENT_ALPN_MAX_LEN            (24)   ///< Maximum length of ALPN supported by RRQ61000.
#define MQTT_CLIENT_MAX_SNI_LEN             (64)   ///< Maximum length of SNI supported by RRQ61000.
#define MQTT_CLIENT_TLS_CIPHER_SUITE_MAX    (17)   ///< Maximum number of TLS cipher suites supported by RRQ61000.
#define MQTT_CLIENT_TLS_CIPHER_MAX_CNT      (17)   ///< Maximum number of TLS cipher suites supported by RRQ61000.
#define MQTT_CLIENT_MAX_TOPIC_LEN           (64)   ///< Maximum total length for topics supported by RRQ61000.
#define MQTT_CLIENT_MAX_PUBMSG_LEN          (2048) ///< Maximum total length for message supported by RRQ61000.
#define MQTT_CLIENT_MAX_PUBTOPICMSG_LEN     (2112) ///< Maximum total length for message + topic supported by RRQ61000.
#define MQTT_CLIENT_SUBTOPIC_MAX_CNT        (32)   ///< Maximum number of subscription topics allowed.

/** MQTT Quality-of-service (QoS) levels */
typedef enum e_mqtt_client_qos
{
    MQTT_CLIENT_QOS_0 = 0,             ///< Delivery at most once.
    MQTT_CLIENT_QOS_1 = 1,             ///< Delivery at least once.
    MQTT_CLIENT_QOS_2 = 2              ///< Delivery exactly once.
} mqtt_client_qos_t;

/** MQTT TLS Cipher Suites */
typedef enum e_mqtt_client_tls_cipher_suites
{
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA      = 0xC011, ///< TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA protocol.
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA      = 0xC014, ///< TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA protocol.
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256   = 0xC027, ///< TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256 protocol.
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384   = 0xC028, ///< TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384 protocol.
    TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256   = 0xC02F, ///< TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256 protocol.
    TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384   = 0xC030, ///< TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384 protocol.
    TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA    = 0xC009, ///< TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA protocol.
    TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA    = 0xC00A, ///< TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA protocol.
    TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256 = 0xC023, ///< TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256 protocol.
    TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384 = 0xC024, ///< TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384 protocol.
    TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256 = 0xC02B, ///< TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256 protocol.
    TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384 = 0xC02C, ///< TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384 protocol.
} mqtt_client_tls_cipher_suites_t;

/** MQTT callback event */
typedef enum e_mqtt_client_callback_event
{
    MQTT_EVENT_SUBSCRIBED,
    MQTT_EVENT_UNSUBSCRIBED,
    MQTT_EVENT_PUBLISHED,
    MQTT_EVENT_MESSAGED,
    MQTT_EVENT_CONNECTED,
    MQTT_EVENT_DISCONNECTED
} mqtt_client_callback_event_t;

/** MQTT SUBSCRIBE packet parameters */
typedef struct st_mqtt_client_sub_info
{
    mqtt_client_qos_t qos;                 ///< Quality of Service for subscription.
    const char      * p_topic_filter;      ///< Topic filter to subscribe to.
    uint16_t          topic_filter_length; ///< Length of subscription topic filter.
} mqtt_client_sub_info_t;

/** MQTT PUBLISH packet parameters */
typedef struct st_mqtt_client_pub_info
{
    mqtt_client_qos_t qos;               ///< Quality of Service for subscription.
    const char      * p_topic_name;      ///< Topic name on which the message is published.
    uint16_t          topic_name_Length; ///< Length of topic name.
    const char      * p_payload;         ///< Message payload.
    uint32_t          payload_length;    ///< Message payload length.
} mqtt_client_pub_info_t;

/** MQTT callback context structure to be passed to user callback */
typedef struct st_mqtt_client_callback_context
{
    mqtt_client_callback_event_t event; ///< MQTT callback event.
} mqtt_client_callback_context_t;

/** MQTT Packet info structure to be passed to user callback */
typedef struct st_mqtt_client_callback_args
{
    uint8_t    * p_data;               ///< Payload received from subscribed MQTT topic.
    const char * p_topic;              ///< Topic to which the message payload belongs to.
    uint32_t     data_length;          ///< Length of the MQTT payload.
    void const * p_context;            ///< Placeholder for user data.
} mqtt_client_callback_args_t;

/** MQTT Configuration */
typedef struct st_mqtt_client_cfg
{
    const uint8_t  use_mqtt_v311;                                                        ///< Flag to use MQTT v3.1.1.
    const uint16_t rx_timeout;                                                           ///< MQTT Rx timeout in milliseconds.
    const uint16_t tx_timeout;                                                           ///< MQTT Tx timeout in milliseconds.

    void (* p_callback)(mqtt_client_callback_args_t * p_args);                           ///< Location of user callback.
    void const   * p_context;                                                            ///< Placeholder for user data. Passed to the user callback in mqtt_client_callback_args_t.
    uint8_t        clean_session;                                                        ///< Whether to establish a new, clean session or resume a previous session.
    uint8_t        alpn_count;                                                           ///< ALPN Protocols count. Max value is 3.
    const char   * p_alpns[MQTT_CLIENT_MAX_ALPN];                                        ///< ALPN Protocols.
    uint8_t        tls_cipher_count;                                                     ///< TLS Cipher suites count. Max value is 17.
    uint16_t       keep_alive_seconds;                                                   ///< MQTT keep alive period.
    const char   * p_client_identifier;                                                  ///< MQTT Client identifier. Must be unique per client.
    uint16_t       client_identifier_length;                                             ///< Length of the client identifier.
    const char   * p_host_name;                                                          ///< MQTT endpoint host name.
    const uint16_t mqtt_port;                                                            ///< MQTT Port number.
    const char   * p_mqtt_user_name;                                                     ///< MQTT user name. Set to NULL if not used.
    uint16_t       user_name_length;                                                     ///< Length of MQTT user name. Set to 0 if not used.
    const char   * p_mqtt_password;                                                      ///< MQTT password. Set to NULL if not used.
    uint16_t       password_length;                                                      ///< Length of MQTT password. Set to 0 if not used.
    const char   * p_root_ca;                                                            ///< String representing a trusted server root certificate.
    uint32_t       root_ca_size;                                                         ///< Size associated with root CA Certificate.
    const char   * p_client_cert;                                                        ///< String representing a Client certificate.
    uint32_t       client_cert_size;                                                     ///< Size associated with Client certificate.
    const char   * p_client_private_key;                                                 ///< String representing Client Private Key.
    uint32_t       private_key_size;                                                     ///< Size associated with Client Private Key.
    const char   * p_will_topic;                                                         ///< String representing Will Topic.
    const char   * p_will_msg;                                                           ///< String representing Will Message.
    const char   * p_sni_name;                                                           ///< Server Name Indication.

    mqtt_client_qos_t               will_qos_level;                                      ///< Will Topic QoS level.
    mqtt_client_tls_cipher_suites_t p_tls_cipher_suites[MQTT_CLIENT_TLS_CIPHER_MAX_CNT]; ///< TLS Cipher suites supported.
#if MQTT_PORT_W_CFG_WATCHDOG_SERVICE_ENABLE
    watchdog_service_instance_t const * p_watchdog_service;                              ///< Pointer to Watchdog Service instance.
#endif
} mqtt_client_cfg_t;

/** MQTT_CLIENT private control block. DO NOT MODIFY. */
typedef struct st_mqtt_client_instance_ctrl
{
    uint32_t open;                     ///< Flag to indicate if MQTT has been opened.
    bool     is_mqtt_connected;        ///< Flag to track MQTT connection status.
    mqtt_client_cfg_t const * p_cfg;   ///< Pointer to p_cfg for MQTT.
} mqtt_client_instance_ctrl_t;

/* MQTT public function prototypes */
fsp_err_t RM_MQTT_CLIENT_Open(mqtt_client_instance_ctrl_t * p_ctrl, mqtt_client_cfg_t const * const p_cfg);
fsp_err_t RM_MQTT_CLIENT_Disconnect(mqtt_client_instance_ctrl_t * p_ctrl);
fsp_err_t RM_MQTT_CLIENT_Connect(mqtt_client_instance_ctrl_t * p_ctrl, uint32_t timeout_ms);
fsp_err_t RM_MQTT_CLIENT_Publish(mqtt_client_instance_ctrl_t * p_ctrl, mqtt_client_pub_info_t * const p_pub_info);
fsp_err_t RM_MQTT_CLIENT_Subscribe(mqtt_client_instance_ctrl_t  * p_ctrl,
                                   mqtt_client_sub_info_t * const p_sub_info,
                                   size_t                         subscription_count);
fsp_err_t RM_MQTT_CLIENT_UnSubscribe(mqtt_client_instance_ctrl_t * p_ctrl, mqtt_client_sub_info_t * const p_sub_info);
fsp_err_t RM_MQTT_CLIENT_Receive(mqtt_client_instance_ctrl_t * p_ctrl, mqtt_client_cfg_t const * const p_cfg);
fsp_err_t RM_MQTT_CLIENT_Close(mqtt_client_instance_ctrl_t * p_ctrl);
fsp_err_t rm_mqtt_client_cmd_mqtt_client(mqtt_client_instance_ctrl_t * p_ctrl, int argc, const char * argv[]);

fsp_err_t rm_mqtt_client_set_atcmd_event_callback(void * const        p_ctrl,
                                                  uint32_t (        * p_callback)(
                                                      void * const    p_ctrl,
                                                      int             index,
                                                      unsigned char * p_in,
                                                      unsigned int    inlen));

/*******************************************************************************************************************//**
 * @} (end addtogroup MQTT_PORT_W)
 **********************************************************************************************************************/

/**********************************************************************************************************************
 * Exported global variables
 **********************************************************************************************************************/

/* TODO: This global pointer is tentative. It needs to be passed as an argument mqtt_client_cfg_t to the function. */
extern mqtt_client_cfg_t const * gp_mqtt_client_cfg;

#endif
