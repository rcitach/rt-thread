/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef RM_PMGR_API_H
#define RM_PMGR_API_H

/******************************************************************************************************************//**
 * @ingroup RENESAS_POWER_INTERFACES
 * @defgroup PMGR_API PMGR Interface
 * @brief Interface for accessing Middleware power modes.
 *
 * @section PMGR_API_SUMMARY Summary
 * This section defines the API for the PMGR (Power Manager) Module.
 * The PMGR Module provides smart power engine which controlled by application state and requirement.
 *
 * @note The PMGR is automated by default, but can also be configured for manual.
 *
 *
 * @{
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Includes
 *********************************************************************************************************************/

/* Register definitions, common services and error codes. */
#include "bsp_api.h"

/* Common macro for FSP header files. There is also a corresponding FSP_FOOTER macro at the end of this file. */
FSP_HEADER

/**********************************************************************************************************************
 * Macro definitions
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Typedef definitions
 *********************************************************************************************************************/
typedef enum e_pmgr_event
{
    PMGR_EVENT_NOT_SET                     = 0,  /// event of sleep mode is not set yet
    PMGR_EVENT_ENTERING_SLEEP              = 1,  /// entering sleep mode
    PMGR_EVENT_EXITING_SLEEP               = 2,  /// exiting sleep mode
} pmgr_event_t;

/** sleep constraints */
typedef enum e_pmgr_constraints
{
    PMGR_CONSTRAINT_NONE              = (0),        ///< All available sleep modes are allowed
    PMGR_CONSTRAINT_SLEEP_PROHIBITED  = (1UL << 0), ///< Any sleep mode is not allowed
    PMGR_CONSTRAINT_POWER_MAC_HW      = (1UL << 1), ///< MAC HW must be powered on
    PMGR_CONSTRAINT_POWER_RAM         = (1UL << 2), ///< RAM must be powered on
    PMGR_CONSTRAINT_POWER_RETENTION   = (1UL << 3), ///< Retention must powered on
    PMGR_CONSTRAINT_MAX_BIT           = (1UL << 4), ///< Constraint last bit
} pmgr_constraints_t;

#define PMGR_CONSTRAINT_MAX __builtin_ctz(PMGR_CONSTRAINT_MAX_BIT)
#define PMGR_CONSTRAINT_MASK (PMGR_CONSTRAINT_MAX_BIT - 1)

/** User configuration structure, used in open function */
typedef struct st_pmgr_cfg
{
    /** Placeholder for extension. */
    void const * p_extend;
} pmgr_cfg_t;

typedef struct st_pmgr_callback_arg
{
    pmgr_constraints_t  constraints;     ///< Bit map definition of the constraints for entering/exiting sleep mode
    pmgr_event_t        event;           ///< Entering or exitting sleep mode
    void const *        p_context;       ///< Context provided to user during callback
    void *              p_instance_info; ///< Instance specific additional info
} pmgr_callback_args_t;

/** PMGR control block.  Allocate an instance specific control block to pass into the PMGR API calls.
 */
typedef void pmgr_ctrl_t;

/** PMGR driver structure. General PMGR functions implemented at the HAL layer will follow this API. */
typedef struct st_pmgr_api
{
    /** Open the PMGR driver module.
     * Add constraint for RAM
     *
     * @param[in] p_ctrl               Pointer to PMGR device handle.
     * @param[in] p_cfg                Pointer to a configuration structure.
     **/
    fsp_err_t (* open)(pmgr_ctrl_t * const p_ctrl, pmgr_cfg_t const * const p_cfg);

    /** Close the PMGR driver module.
     *
     * @param[in] p_ctrl               Pointer to PMGR device handle
     * @retval FSP_SUCCESS             Configuration was successful.
     **/
    fsp_err_t (* close)(pmgr_ctrl_t * const p_ctrl);

     /**
     * Add a callback notifier function and optional context pointer and working memory pointer.
     * The common implementation defines synchronous approach, where on return from notifier call back the
     * system is ready from power save mode. 
     * The callback function will be executed with the RTOS scheduler suspended and inside critical section (PRIMASK=1)
     * Keep the callback short, do not call RTOS blocking functions, no calls to stdio, and do not clear PRIMASK.
     *
     * @param[in]   p_ctrl                   Pointer to the PMGR control block.
     * @param[in]   p_callback               Callback function
     * @param[in]   p_callback_memory        Pointer to memory of type pmgr_callback_args_t which will be passed to callback function.
     * @param[in,out]   p_extend             Implementation specific parameter
     **/
    fsp_err_t (* notifier_register)(pmgr_ctrl_t * const p_ctrl, void (* p_callback)(pmgr_callback_args_t *),
                                    pmgr_callback_args_t * const p_callback_memory, void const * p_extend);

    /**
     * Update expected system idle time. The expected idle time will be the expected sleep duration
     *         (unless unexpected interrupt occurs) note for the following 2 usecase for this API:
     *              - called by the OS, when it reaches the idle task,
     *              - called by customized application to force sleep immediately
     *         (including tasks delayed wake , timer events etc ...)
     *
     * @param[in]   p_ctrl                   Pointer to the PMGR control block.
     * @param[in]   idle_time_us             Expected system Idle time in usec.
     */
    fsp_err_t (* enter_idle)(pmgr_ctrl_t * const p_ctrl, uint64_t idle_time_us);

    /**
     * Add sleep constraint, 
     * sleep constraints are used by system middleware or customised application to prohibit specific sleep implication
     * the sleep constraints is a bit map representing functionality, each functionality (in the bitmap) is
     * represented by counter in the implementation each time the add_sleep_constraint() with a bit set, the
     * corresponding counter is increased, the functionality is allowed only if counter is zero.
     *
     * @param[in]   p_ctrl                  Pointer to the PMGR control block.
     * @param[in]   constraint              Defines a bit map of single constraint.
     */
    fsp_err_t (* add_sleep_constraint)(pmgr_ctrl_t * const p_ctrl, pmgr_constraints_t constraint);

    /**
     * Remove sleep constraint , see add_sleep_constraint details
     *
     * @param[in]   p_ctrl                   Pointer to the PMGR control block.
     * @param[in]   constraint               Defines a bit map of single constraint.
     */
    fsp_err_t (* remove_sleep_constraint)(pmgr_ctrl_t * const p_ctrl, pmgr_constraints_t constraint);
} pmgr_api_t;

/** This structure encompasses everything that is needed to use an instance of this interface. */
typedef struct st_pmgr_instance
{
    pmgr_ctrl_t            * p_ctrl;    ///< Pointer to the control structure for this instance
    pmgr_cfg_t const * const p_cfg;     ///< Pointer to the configuration structure for this instance
    pmgr_api_t const * const p_api;     ///< Pointer to the API structure for this instance
} pmgr_instance_t;

/** Common macro for FSP header files. There is also a corresponding FSP_HEADER macro at the top of this file. */
FSP_FOOTER

#endif

/******************************************************************************************************************//**
 * @} (end defgroup PMGR_API)
 *********************************************************************************************************************/
