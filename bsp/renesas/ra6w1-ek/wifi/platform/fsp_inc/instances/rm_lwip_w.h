/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef RM_LWIP_W_H
#define RM_LWIP_W_H

/*******************************************************************************************************************//**
 * @addtogroup lwIP
 * @{
 **********************************************************************************************************************/

#include "bsp_api.h"
#include "rm_lwip_w_cfg.h"
#if LWIP_W_CFG_WATCHDOG_SERVICE_ENABLE
 #include "rm_watchdog_service_w.h"
#endif

/* Common macro for FSP header files. There is also a corresponding FSP_FOOTER macro at the end of this file. */
FSP_HEADER

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/** User configuration structure, used in open function */
typedef struct st_lwip_w_cfg
{
#if LWIP_W_CFG_WATCHDOG_SERVICE_ENABLE
    watchdog_service_instance_t const * p_watchdog_service; ///< Pointer to Watchdog Service instance.
#endif
    void const * p_extend;              ///< Pointer to extended configuration by instance of interface.
} lwip_w_cfg_t;

/**********************************************************************************************************************
 * Exported global variables
 **********************************************************************************************************************/

/* TODO: This global pointer is tentative. It needs to be passed as an argument lwip_w_cfg_t to the function. */
extern lwip_w_cfg_t const * gp_lwip_w_cfg;

/** @cond INC_HEADER_DEFS_SEC */
/** Filled in Interface API structure for this Instance. */

/** @endcond */

/**********************************************************************************************************************
 * Public APIs
 **********************************************************************************************************************/
/*******************************************************************************************************************//**
 * @} (end addtogroup lwIP)
 **********************************************************************************************************************/

/* Common macro for FSP header files. There is also a corresponding FSP_HEADER macro at the top of this file. */
FSP_FOOTER

#endif
