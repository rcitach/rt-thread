/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/*******************************************************************************************************************//**
 * @ingroup RENESAS_MONITORING_INTERFACES
 * @defgroup WATCHDOG_SERVICE_API Watchdog Service Interface
 * @brief Interface for Watchdog Service APIs.
 *
 * @section WATCHDOG_SERVICE_API_Summary Summary
 * The Watchdog Service provides watchdog functionality to monitor system tasks and avoid system freezes.
 *
 * @{
 **********************************************************************************************************************/

#ifndef RM_WATCHDOG_SERVICE_API_H
#define RM_WATCHDOG_SERVICE_API_H

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

/* Register definitions, common services and error codes. */
#include "bsp_api.h"
#include "r_wdt_api.h"

/* Common macro for FSP header files. There is also a corresponding FSP_FOOTER macro at the end of this file. */
FSP_HEADER

/**********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

/**********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/** Control block. Allocate an instance specific control block to pass into the API calls.
 */
typedef void watchdog_service_ctrl_t;

/** Configuration parameters. */
typedef struct st_watchdog_service_cfg
{
    wdt_instance_t const * p_wdt;      ///< To use WDT, link a WDT instance here.
    void const * p_context;            ///< Placeholder for user data.
    void const * p_extend;             ///< Placeholder for user extension.
} watchdog_service_cfg_t;

/** Functions implemented at the HAL layer will follow this API. */
typedef struct st_watchdog_service_api
{
    /** Initialize and start the Watchdog Service.
     *
     * @param[in]  p_ctrl       Pointer to control structure.
     * @param[in]  p_cfg        Pointer to pin configuration structure.
     */
    fsp_err_t (* open)(watchdog_service_ctrl_t * const p_ctrl, watchdog_service_cfg_t const * const p_cfg);

    /** Register a current task with the Watchdog Service.
     *
     * @param[in]  p_ctrl       Pointer to control structure.
     * @param[in]  p_cfg        Pointer to pin configuration structure.
     * @param[out] p_id         Pointer to id of registered task.
     */
    fsp_err_t (* registerTask)(watchdog_service_ctrl_t * const p_ctrl, watchdog_service_cfg_t const * const p_cfg,
                               uint8_t * p_id);

    /** Unregister a task from the Watchdog Service.
     *
     * @param[in]  p_ctrl       Pointer to control structure.
     * @param[in]  id           Id of unregistered task.
     */
    fsp_err_t (* unregisterTask)(watchdog_service_ctrl_t * const p_ctrl, uint8_t id);

    /** Suspend monitoring a task by the Watchdog Service.
     *
     * @param[in]  p_ctrl       Pointer to control structure.
     * @param[in]  id           Id of task to suspend monitoring.
     */
    fsp_err_t (* suspend)(watchdog_service_ctrl_t * const p_ctrl, uint8_t id);

    /** Resume monitoring of a task by the Watchdog Service.
     *
     * @param[in]  p_ctrl       Pointer to control structure.
     * @param[in]  id           Id of task to resume monitoring.
     */
    fsp_err_t (* resume)(watchdog_service_ctrl_t * const p_ctrl, uint8_t id);

    /** Notify the Watchdog Service about a task.
     *
     * @param[in]  p_ctrl       Pointer to control structure.
     * @param[in]  p_cfg        Pointer to pin configuration structure.
     * @param[in]  id           Id of task for notification.
     */
    fsp_err_t (* notify)(watchdog_service_ctrl_t * const p_ctrl, watchdog_service_cfg_t const * const p_cfg,
                         uint8_t id);

    /** Resume monitoring of a task and notify the Watchdog Service about the task.
     *
     * @param[in]  p_ctrl       Pointer to control structure.
     * @param[in]  p_cfg        Pointer to pin configuration structure.
     * @param[in]  id           Id of task to resume monitoring and notify.
     */
    fsp_err_t (* resumeAndNotify)(watchdog_service_ctrl_t * const p_ctrl, watchdog_service_cfg_t const * const p_cfg,
                                  uint8_t id);

    /** Notify the Watchdog Service about the idle task.
     *
     * @param[in]  p_ctrl       Pointer to control structure.
     * @param[in]  p_cfg        Pointer to pin configuration structure.
     */
    fsp_err_t (* notifyFromIdle)(watchdog_service_ctrl_t * const p_ctrl, watchdog_service_cfg_t const * const p_cfg);

    /** Set watchdog latency for a task.
     *
     * @param[in]  p_ctrl       Pointer to control structure.
     * @param[in]  id           Id of task for setting latency.
     * @param[in]  latency      Latency value.
     */
    fsp_err_t (* setLatency)(watchdog_service_ctrl_t * const p_ctrl, uint8_t id, uint8_t latency);

    /** Set watchdog ID of the idle task.
     *
     * @param[in]  p_ctrl       Pointer to control structure.
     * @param[in]  id           Id of idle task.
     */
    fsp_err_t (* setIdleId)(watchdog_service_ctrl_t * const p_ctrl, uint8_t id);
} watchdog_service_api_t;

/** This structure encompasses everything that is needed to use an instance of this interface. */
typedef struct st_watchdog_service_instance
{
    watchdog_service_ctrl_t      * p_ctrl;          ///< Pointer to the control structure for this instance
    watchdog_service_cfg_t const * p_cfg;           ///< Pointer to the configuration structure for this instance
    watchdog_service_api_t const * p_api;           ///< Pointer to the API structure for this instance
} watchdog_service_instance_t;

/* Common macro for FSP header files. There is also a corresponding FSP_HEADER macro at the top of this file. */
FSP_FOOTER

#endif

/*******************************************************************************************************************//**
 * @} (end defgroup WATCHDOG_SERVICE_API)
 **********************************************************************************************************************/
