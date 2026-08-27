/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/*******************************************************************************************************************//**
 * @addtogroup HTTPS_W
 * @{
 **********************************************************************************************************************/

#ifndef RM_HTTPS_W_H
#define RM_HTTPS_W_H

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "bsp_api.h"

#include "lwipopts.h"
#include "lwip/altcp_tcp.h"
#include "lwip/dns.h"
#include "lwip/debug.h"
#include "lwip/mem.h"
#include "lwip/altcp_tls.h"
#include "lwip/init.h"
#include "rm_httpd.h"
#include "rm_http_client.h"

#include "rm_https_api.h"
#include "rm_https_w_cfg.h"

/* Common macro for FSP header files. There is also a corresponding FSP_FOOTER macro at the end of this file. */
FSP_HEADER

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
/* Stack size of HTTP Client */
#define HTTPC_STACK_SZ                  ((1024 * 8) / 4) /* WORD */
#define HTTPC_TASK_PRI                  (OS_TASK_PRIORITY_USER + 2)

/* Default delay for HTTP Client */
#define HTTPC_DEF_TIMEOUT               (30)

#define HTTP_SERVER_PORT                (80)
#define HTTPS_SERVER_PORT               (443)

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/** Private control block. DO NOT MODIFY. Initialization occurs when RM_WATCHDOG_SERVICE_W_Open() is called. */
typedef struct st_https_instance_ctrl
{
    uint32_t                open;                   ///< Indicates whether the open() API has been successfully called.
    https_cfg_t const     * p_cfg;                  ///< Pointer to instance configuration
    https_client_status_t   server_status;          ///< Server Status
    void (* p_callback)(https_callback_args_t *);   ///< Pointer to callback
    https_callback_args_t * p_callback_memory;      ///< Pointer to optional callback argument memory
    void const            * p_context;              ///< Pointer to context to be passed into callback function
#if HTTPS_W_CFG_CLIENT_ENABLE
    http_client_conf_t  http_client_conf;
#endif
} https_w_instance_ctrl_t;

/**********************************************************************************************************************
 * Exported global variables
 **********************************************************************************************************************/

/** @cond INC_HEADER_DEFS_SEC */
/** Filled in Interface API structure for this Instance. */
extern const https_api_t g_https_w;

/** @endcond */

/**********************************************************************************************************************
 * Function Prototypes
 **********************************************************************************************************************/
BSP_WEAK_REFERENCE void g_https0_callback(https_callback_args_t *p_args);
fsp_err_t RM_HTTPS_W_Open (https_ctrl_t * const p_api_ctrl, https_cfg_t const * const p_cfg);
fsp_err_t RM_HTTPS_W_Close (https_ctrl_t * const p_api_ctrl);
fsp_err_t RM_HTTPS_W_CallbackSet (https_ctrl_t * const p_api_ctrl,
                                      void (* p_callback)(https_callback_args_t *),
                                      void const * const p_context,
                                      https_callback_args_t * const p_callback_memory);
fsp_err_t RM_HTTPS_W_ServerStart (https_ctrl_t * const p_api_ctrl, https_server_sec_t * p_sec);
fsp_err_t RM_HTTPS_W_ServerStop (https_ctrl_t * const p_api_ctrl);
fsp_err_t RM_HTTPS_W_ServerGetStatus (https_ctrl_t * const p_api_ctrl, https_server_status_t * const p_status);
fsp_err_t RM_HTTPS_W_ClientSendRequest (https_ctrl_t * const p_api_ctrl, http_client_request_t * p_request);
fsp_err_t RM_HTTPS_W_ClientSetTlsVersion (https_ctrl_t * const p_api_ctrl, int32_t tls_ver);
fsp_err_t RM_HTTPS_W_ClientSetAuthMode (https_ctrl_t * const p_api_ctrl, int32_t auth_mode);

/*******************************************************************************************************************//**
 * @} (end addtogroup HTTPS_W)
 **********************************************************************************************************************/

/* Common macro for FSP header files. There is also a corresponding FSP_HEADER macro at the top of this file. */
FSP_FOOTER

#endif /* RM_HTTPS_W_H */
