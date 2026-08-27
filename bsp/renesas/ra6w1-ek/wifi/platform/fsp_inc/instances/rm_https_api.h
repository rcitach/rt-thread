/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/*******************************************************************************************************************//**
 * @ingroup RENESAS_NETWORKING_INTERFACES
 * @defgroup HTTPS_API HTTPS Interface
 * @brief Interface for HTTPS APIs.
 *
 * @section HTTPS_API_Summary Summary
 * The HTTPS interface provides HTTPS functionality including starting server  or sending a HTTP request.
 *
 * @{
 **********************************************************************************************************************/

#ifndef RM_HTTPS_API_H
#define RM_HTTPS_API_H

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

/* Register definitions, common services and error codes. */
#include "bsp_api.h"
#include "rm_http_client.h"

/* Common macro for FSP header files. There is also a corresponding FSP_FOOTER macro at the end of this file. */
FSP_HEADER

/**********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#define HTTPC_MAX_HOSTNAME_LEN        (256)
#define HTTPC_MAX_PATH_LEN            (256)
#define HTTPC_MAX_NAME                (20)
#define HTTPC_MAX_PASSWORD            (20)

/// Max size of HTTP Client's request data
#define HTTPC_MAX_REQ_DATA            (1024 * 4)

/**********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/** Control block. Allocate an instance specific control block to pass into the API calls.
 */
typedef void https_ctrl_t;
typedef int8_t https_server_status_t;

/** Operation code of HTTP Client */
typedef enum e_https_client_opcode
{
    HTTP_CLIENT_OPCODE_READY,       ///< Init value
    HTTP_CLIENT_OPCODE_HEAD,        ///< Head method request
    HTTP_CLIENT_OPCODE_GET,         ///< GET method request
    HTTP_CLIENT_OPCODE_PUT,         ///< PUT method request
    HTTP_CLIENT_OPCODE_POST,        ///< POST method request
    HTTP_CLIENT_OPCODE_PATCH,       ///< PATCH method request
    HTTP_CLIENT_OPCODE_DELETE,      ///< DELETE method request
    HTTP_CLIENT_OPCODE_CONNECT,     ///< CONNECT method request
    HTTP_CLIENT_OPCODE_TRACE,       ///< TRACE method request
    HTTP_CLIENT_OPCODE_OPTIONS,     ///< OPTIONS method request
    HTTP_CLIENT_OPCODE_MESSAGE,     ///< User Generated Messages request
    HTTP_CLIENT_OPCODE_STATUS,      ///< Display status of HTTP client
    HTTP_CLIENT_OPCODE_HELP,        ///< Display Help menu
    HTTP_CLIENT_OPCODE_STOP,        ///< Stop HTTP Client
} https_client_opcode_t;

/** Events that can trigger a callback function */
typedef enum e_https_event
{
    HTTPS_EVENT_SERVER_RECVED           = 1U << 0, ///< 
    HTTPS_EVENT_SERVER_ERR_RESULT       = 1U << 1, ///< 
    HTTPS_EVENT_CLIENT_HEADER_RECVED    = 1U << 2, ///< 
    HTTPS_EVENT_CLIENT_GET_DONE         = 1U << 3, ///< 
    HTTPS_EVENT_CLIENT_RESULT           = 1U << 4, ///< 
    HTTPS_EVENT_CLIENT_RECVED           = 1U << 5, ///< 
    HTTPS_EVENT_CLIENT_RECVED_DECODED   = 1U << 6, ///< 
} https_event_t;


/** Status of HTTP Client */
typedef enum e_https_client_status
{
    HTTP_CLIENT_STATUS_READY,
    HTTP_CLIENT_STATUS_WAIT,
    HTTP_CLIENT_STATUS_PROGRESS,
} https_client_status_t;

typedef struct st_https_server_sec
{
    uint8_t *   p_tls_srv_key;
    size_t      tls_srv_key_len;
    uint8_t *   p_tls_srv_cert;
    size_t      tls_srv_cert_len;
    uint8_t *   p_priv_pass;
    size_t      priv_pass_len;
    uint32_t    tls_ver_max;
    uint32_t    tls_ver_min;
} https_server_sec_t;


/** HTTP Request structure */
typedef struct st_http_client_request
{
    https_client_opcode_t       op_code;                            ///< Operation code
    uint32_t                    iface;                              ///< Interface
    uint32_t                    port;                               ///< Port number of HTTP Server
    uint32_t                    insecure;                           ///< Secure Mode
    char                        hostname[HTTPC_MAX_HOSTNAME_LEN];   ///< Host Name of HTTP request
    char                        path[HTTPC_MAX_PATH_LEN];           ///< Path of HTTP request
    char                        data[HTTPC_MAX_REQ_DATA];           ///< Data of HTTP request
    char                        username[HTTPC_MAX_NAME];           ///< User name of HTTP request
    char                        password[HTTPC_MAX_PASSWORD];       ///< Password of HTTP request
    httpc_secure_connection_t   https_conf;                         ///< TLS Configuration

} http_client_request_t;

/** Data transfer with AT-CMD interface */
typedef struct st_http_client_receive
{
    bool                        chunked;                ///< Flag
    int32_t                     chunked_len;            ///< Parsed Chunked Size
    int32_t                     chunked_remain_len;     ///< Chunked payload not yet received
} http_client_receive_t;

/** HTTP Client configuration */
typedef struct st_http_client_conf_t
{
    https_client_status_t      status;      ///< Status of HTTP Client
    http_client_request_t      request;     ///< HTTP request instance
    http_client_receive_t      receive;     ///< HTTP receive instance
} http_client_conf_t;

/** Callback function parameter data */
typedef struct st_https_callback_args
{
    https_event_t    event;            ///< The event can be used to identify what caused the callback.
    void const     * p_context;        ///< Placeholder for user data.
    void           * payload;
    void           * p_param;
    uint16_t         len;
} https_callback_args_t;

/** Configuration parameters. */
typedef struct st_https_cfg
{
    void (* p_callback)(https_callback_args_t *);   ///< Pointer to callback
    void const * p_context;                         ///< Placeholder for user data.
    void const * p_extend;                          ///< Placeholder for user extension.
} https_cfg_t;

/** Functions implemented at the HAL layer will follow this API. */
typedef struct st_https_api
{
    /** Initialize HTTPS module.
     *
     * @param[in]  p_ctrl       Pointer to control structure.
     * @param[in]  p_cfg        Pointer to pin configuration structure.
     */
    fsp_err_t (* open)(https_ctrl_t * const p_ctrl, https_cfg_t const * const p_cfg);

    /** Start HTTP Server.
     *
     * @param[in]  p_ctrl       Pointer to control structure.
     * @param[in]  p_sec        Pointer to key and certificate.
     */
    fsp_err_t (* serverStart)(https_ctrl_t * const p_ctrl, https_server_sec_t * p_sec);

    /** Stop HTTP Server.
     *
     * @param[in]  p_ctrl       Pointer to control structure.
     */
    fsp_err_t (* serverStop)(https_ctrl_t * const p_ctrl);

    /** Get server status.
     *
     * @param[in]   p_ctrl       Pointer to control structure.
     * @param[out]  p_status     Pointer to server status.
     */
    fsp_err_t (* serverGetStatus)(https_ctrl_t * const p_ctrl, https_server_status_t * p_status);

    /** Set HTTPS module callback.
     *
     * @param[in]  p_ctrl              Pointer to control structure.
     * @param[in]  p_callback          Pointer to callback.
     * @param[in]  p_context           Pointer to data for callback.
     * @param[in]  p_callback_memory   Pointer to memory for callback.
     */
    fsp_err_t (* callbackSet)(https_ctrl_t * const p_ctrl, 
                              void (* p_callback)(https_callback_args_t *),
                              void const * const p_context,
                              https_callback_args_t * const p_callback_memory);

    /** Send request to server.
     *
     * @param[in]  p_ctrl       Pointer to control structure.
     * @param[in]  p_request    Pointer to http request.
     */
    fsp_err_t (* clientSendRequest)(https_ctrl_t * const p_ctrl, http_client_request_t * p_request);

    /** Close HTTPS module.
     *
     * @param[in]  p_ctrl       Pointer to control structure.
     */
    fsp_err_t (* close)(https_ctrl_t * const p_ctrl);
} https_api_t;

/** This structure encompasses everything that is needed to use an instance of this interface. */
typedef struct st_https_instance
{
    https_ctrl_t      * p_ctrl;          ///< Pointer to the control structure for this instance
    https_cfg_t const * p_cfg;           ///< Pointer to the configuration structure for this instance
    https_api_t const * p_api;           ///< Pointer to the API structure for this instance
} https_instance_t;

/* Common macro for FSP header files. There is also a corresponding FSP_HEADER macro at the top of this file. */
FSP_FOOTER

#endif

/*******************************************************************************************************************//**
 * @} (end defgroup HTTPS_API)
 **********************************************************************************************************************/
