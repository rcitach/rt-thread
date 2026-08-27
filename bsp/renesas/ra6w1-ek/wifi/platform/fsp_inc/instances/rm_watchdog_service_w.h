/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/*******************************************************************************************************************//**
 * @addtogroup WATCHDOG_SERVICE_W
 * @{
 **********************************************************************************************************************/

#ifndef RM_WATCHDOG_SERVICE_W_H
#define RM_WATCHDOG_SERVICE_W_H

#include "bsp_api.h"
#include "r_wdt_api.h"
#include "rm_watchdog_service_api.h"
#include "../../src/bsp_w/mcu/ra6w1/bsp_dump_mem.h"
#include "r_wdog_w.h"
#include "FreeRTOS.h"
#include "portmacro.h"
#include "projdefs.h"
#include "semphr.h"
#include "task.h"

#include "rm_watchdog_service_w_cfg.h"

/* Common macro for FSP header files. There is also a corresponding FSP_FOOTER macro at the end of this file. */
FSP_HEADER

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#define WATCHDOG_SERVICE_W_MAX_TASKS            (64U)
#define WATCHDOG_SERVICE_W_MAX_INDEX \
    ((WATCHDOG_SERVICE_W_MAX_TASKS / 32U) + ((WATCHDOG_SERVICE_W_MAX_TASKS % 32U) ? 1U : 0U))
#define WATCHDOG_SERVICE_W_NOT_REGISTERED_ID    (0xFFU)
#define WATCHDOG_SERVICE_W_TIMER_RESET_VALUE    (400U)

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/** Private control block. DO NOT MODIFY. Initialization occurs when RM_WATCHDOG_SERVICE_W_Open() is called. */
typedef struct st_watchdog_service_w_instance_ctrl
{
    uint32_t          open;                                               ///< Indicates whether the open() API has been successfully called.
    SemaphoreHandle_t lock;                                               ///< Semaphore handle.
    uint8_t           max_task_id;                                        ///< Current maximum value of task ID.
    uint8_t           idle_task_id;                                       ///< Id of idle task.
    uint32_t          tasks_mask[WATCHDOG_SERVICE_W_MAX_INDEX];           ///< Bitmask of registered tasks identifiers.
    uint32_t          tasks_monitored_mask[WATCHDOG_SERVICE_W_MAX_INDEX]; ///< Bitmask of monitored tasks identifiers.
    uint32_t          notified_mask[WATCHDOG_SERVICE_W_MAX_INDEX];        ///< Bitmask of tasks which notified.
    uint8_t           tasks_latency[WATCHDOG_SERVICE_W_MAX_TASKS];        ///< Allowed latency set by tasks.
    TaskHandle_t      tasks_handle[WATCHDOG_SERVICE_W_MAX_TASKS];         ///< Handles of monitored tasks.
} watchdog_service_w_instance_ctrl_t;

/**********************************************************************************************************************
 * Exported global variables
 **********************************************************************************************************************/

/** Hold the stack contents when NMI occurs. */
extern volatile uint32_t g_watchdog_service_w_nmi_event_data[9];

/** @cond INC_HEADER_DEFS_SEC */
/** Filled in Interface API structure for this Instance. */
extern const watchdog_service_api_t g_watchdog_service_on_watchdog_service_w;

/** @endcond */

/**********************************************************************************************************************
 * Public APIs
 **********************************************************************************************************************/
fsp_err_t RM_WATCHDOG_SERVICE_W_Open(watchdog_service_ctrl_t * const      p_api_ctrl,
                                     watchdog_service_cfg_t const * const p_cfg);

fsp_err_t RM_WATCHDOG_SERVICE_W_Register(watchdog_service_ctrl_t * const      p_api_ctrl,
                                         watchdog_service_cfg_t const * const p_cfg,
                                         uint8_t                            * p_id);

fsp_err_t RM_WATCHDOG_SERVICE_W_Unregister(watchdog_service_ctrl_t * const p_api_ctrl, uint8_t id);

fsp_err_t RM_WATCHDOG_SERVICE_W_Suspend(watchdog_service_ctrl_t * const p_api_ctrl, uint8_t id);

fsp_err_t RM_WATCHDOG_SERVICE_W_Resume(watchdog_service_ctrl_t * const p_api_ctrl, uint8_t id);

fsp_err_t RM_WATCHDOG_SERVICE_W_Notify(watchdog_service_ctrl_t * const      p_api_ctrl,
                                       watchdog_service_cfg_t const * const p_cfg,
                                       uint8_t                              id);

fsp_err_t RM_WATCHDOG_SERVICE_W_ResumeAndNotify(watchdog_service_ctrl_t * const      p_api_ctrl,
                                                watchdog_service_cfg_t const * const p_cfg,
                                                uint8_t                              id);

fsp_err_t RM_WATCHDOG_SERVICE_W_NotifyFromIdle(watchdog_service_ctrl_t * const      p_api_ctrl,
                                               watchdog_service_cfg_t const * const p_cfg);

fsp_err_t RM_WATCHDOG_SERVICE_W_SetLatency(watchdog_service_ctrl_t * const p_api_ctrl, uint8_t id, uint8_t latency);

fsp_err_t RM_WATCHDOG_SERVICE_W_SetIdleId(watchdog_service_ctrl_t * const p_api_ctrl, uint8_t id);

fsp_err_t RM_WATCHDOG_SERVICE_W_IsMonitorMaskEmpty(watchdog_service_ctrl_t * const p_api_ctrl);

extern void rm_watchdog_service_w_nmi_reset(uint32_t * p_exception_args);

/*******************************************************************************************************************//**
 * @} (end addtogroup WATCHDOG_SERVICE_W)
 **********************************************************************************************************************/

/* Common macro for FSP header files. There is also a corresponding FSP_HEADER macro at the top of this file. */
FSP_FOOTER

#endif
