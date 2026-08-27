/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "rm_watchdog_service_w.h"

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

/** "WDOG" in ASCII, used to determine if the Watchdog Service is open. */
#define WATCHDOG_SERVICE_W_OPEN    (0x57444F47ULL)

#define WATCHDOG_SERVICE_W_CHK_BIT(data, bit)      (0U != (data & (1U << bit)))
#define WATCHDOG_SERVICE_W_SET_BIT(data, bit)      do {data |= (1U << bit);} while (0U)
#define WATCHDOG_SERVICE_W_CLR_BIT(data, bit)      do {data &= ~(1U << bit);} while (0U)
#define WATCHDOG_SERVICE_W_VALIDATE_ID(id, err)    do { \
        if ((id) >= WATCHDOG_SERVICE_W_MAX_TASKS)       \
        {                                               \
            configASSERT(0U);                           \
            FSP_RETURN(err);                            \
        }                                               \
} while (0U)

#define WATCHDOG_SERVICE_W_FALSE               (0xFFU)

#define WATCHDOG_SERVICE_W_NMI_MAGIC_NUMBER    (0xDEADBEEFU)

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private function prototypes
 **********************************************************************************************************************/
static uint8_t rm_watchdog_service_w_get_empty_id(watchdog_service_w_instance_ctrl_t * p_ctrl);
static void    rm_watchdog_service_w_callback(wdt_callback_args_t * p_args);
static void    rm_watchdog_service_w_reset_watchdog(watchdog_service_w_instance_ctrl_t * p_ctrl,
                                                    watchdog_service_cfg_t const * const p_cfg);
static void rm_watchdog_service_w_resume_base(watchdog_service_w_instance_ctrl_t * p_ctrl, uint8_t id);
static void rm_watchdog_service_w_notify_base(watchdog_service_w_instance_ctrl_t * p_ctrl,
                                              watchdog_service_cfg_t const * const p_cfg,
                                              uint8_t                              id);
static void rm_watchdog_service_w_notify_task(watchdog_service_w_instance_ctrl_t * p_ctrl,
                                              watchdog_service_cfg_t const * const p_cfg,
                                              uint8_t                              id);
static void rm_watchdog_service_w_notify_idle(watchdog_service_w_instance_ctrl_t * p_ctrl,
                                              watchdog_service_cfg_t const * const p_cfg,
                                              uint8_t                              id);

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/
static watchdog_service_w_instance_ctrl_t * gp_watchdog_service_ctrl;
static watchdog_service_cfg_t const       * gp_watchdog_service_cfg;

/***********************************************************************************************************************
 * Global variables
 **********************************************************************************************************************/

/** Hold the stack contents when NMI occurs. */
volatile uint32_t g_watchdog_service_w_nmi_event_data[9] __attribute__((section("nmi_info")));

/** Watchdog Service implementation of WATCHDOG_SERVICE_W Driver  */
const watchdog_service_api_t g_watchdog_service_on_watchdog_service_w =
{
    .open            = RM_WATCHDOG_SERVICE_W_Open,
    .registerTask    = RM_WATCHDOG_SERVICE_W_Register,
    .unregisterTask  = RM_WATCHDOG_SERVICE_W_Unregister,
    .suspend         = RM_WATCHDOG_SERVICE_W_Suspend,
    .resume          = RM_WATCHDOG_SERVICE_W_Resume,
    .notify          = RM_WATCHDOG_SERVICE_W_Notify,
    .resumeAndNotify = RM_WATCHDOG_SERVICE_W_ResumeAndNotify,
    .notifyFromIdle  = RM_WATCHDOG_SERVICE_W_NotifyFromIdle,
    .setLatency      = RM_WATCHDOG_SERVICE_W_SetLatency,
    .setIdleId       = RM_WATCHDOG_SERVICE_W_SetIdleId,
};

/*******************************************************************************************************************//**
 * @addtogroup WATCHDOG_SERVICE_W
 * @{
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * Configure and start the Watchdog Service.
 * This should be called before using the Watchdog Service, preferably at application startup.
 *
 * Example:
 * @snippet rm_watchdog_service_w_example.c RM_WATCHDOG_SERVICE_W_Open
 *
 * @retval FSP_SUCCESS                    Watchdog Service was opened successfully.
 * @retval FSP_ERR_ASSERTION              NULL pointer to parameters in argument.
 * @retval FSP_ERR_ALREADY_OPEN           Watchdog Service is already opened.
 * @retval FSP_ERR_INVALID_STATE          Mutex was not created successfully.
 * @retval FSP_ERR_INVALID_HW_CONDITION   HW WDT was not opened successfully.
 *
 * @note If this API returns FSP_SUCCESS, the Watchdog Service creates a mutex in
 *       ((watchdog_service_w_instance_ctrl_t *) p_api_ctrl)->lock. The Watchdog Service never delete the mutex.
 **********************************************************************************************************************/
fsp_err_t RM_WATCHDOG_SERVICE_W_Open (watchdog_service_ctrl_t * const      p_api_ctrl,
                                      watchdog_service_cfg_t const * const p_cfg)
{
    watchdog_service_w_instance_ctrl_t * p_ctrl = (watchdog_service_w_instance_ctrl_t *) p_api_ctrl;
    fsp_err_t err;

#if WATCHDOG_SERVICE_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_ctrl);
    FSP_ASSERT(NULL != p_cfg);
    FSP_ASSERT(NULL != p_cfg->p_wdt);
    FSP_ERROR_RETURN(WATCHDOG_SERVICE_W_OPEN != p_ctrl->open, FSP_ERR_ALREADY_OPEN);
#endif

    /* Initialize values. */
    memset(p_ctrl, 0U, sizeof(watchdog_service_w_instance_ctrl_t));
    p_ctrl->idle_task_id = WATCHDOG_SERVICE_W_NOT_REGISTERED_ID;

    /* Create mutex. */
    p_ctrl->lock = xSemaphoreCreateRecursiveMutex();
    FSP_ERROR_RETURN(NULL != p_ctrl->lock, FSP_ERR_INVALID_STATE);

    /* Open HW WDT. */
    err = p_cfg->p_wdt->p_api->open(p_cfg->p_wdt->p_ctrl, p_cfg->p_wdt->p_cfg);
    if (FSP_SUCCESS != err)
    {
        vSemaphoreDelete(p_ctrl->lock);
        FSP_RETURN(FSP_ERR_INVALID_HW_CONDITION);
    }

    /* Start the watchdog timer. */
    p_cfg->p_wdt->p_api->refresh(p_cfg->p_wdt->p_ctrl);

    p_ctrl->open = WATCHDOG_SERVICE_W_OPEN;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Register current task in the Watchdog Service.
 * Returned id via argument p_id shall be used in all other Watchdog Service API calls from current task.
 * Once registered, the task shall notify the Wathcdog Service periodically using notify API to prevent watchdog
 * expiration. It is up to each task how this is done, but a task can request that it will be triggered periodically
 * using the task notification capability, to notify the Watchdog Service back as a response.
 *
 * Example:
 * @snippet rm_watchdog_service_w_example.c RM_WATCHDOG_SERVICE_W_Register
 *
 * @retval FSP_SUCCESS          Task was registered successfully.
 * @retval FSP_ERR_ASSERTION    NULL pointer to parameters in argument.
 * @retval FSP_ERR_NOT_OPEN     Watchdog Service is not opened.
 * @retval FSP_ERR_IN_USE       Could not register a task because the registered tasks are full.
 **********************************************************************************************************************/
fsp_err_t RM_WATCHDOG_SERVICE_W_Register (watchdog_service_ctrl_t * const      p_api_ctrl,
                                          watchdog_service_cfg_t const * const p_cfg,
                                          uint8_t                            * p_id)
{
    watchdog_service_w_instance_ctrl_t * p_ctrl = (watchdog_service_w_instance_ctrl_t *) p_api_ctrl;

#if WATCHDOG_SERVICE_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_ctrl);
    FSP_ASSERT(NULL != p_cfg);
    FSP_ASSERT(NULL != p_cfg->p_wdt);
    FSP_ASSERT(NULL != p_id);
    FSP_ERROR_RETURN(WATCHDOG_SERVICE_W_OPEN == p_ctrl->open, FSP_ERR_NOT_OPEN);
#endif

    /* Get mutex. */
    xSemaphoreTakeRecursive(p_ctrl->lock, portMAX_DELAY);

    /* Check empty. */
    uint8_t id = rm_watchdog_service_w_get_empty_id(p_ctrl);
    if (WATCHDOG_SERVICE_W_FALSE == id)
    {
        /* Put mutex. */
        xSemaphoreGiveRecursive(p_ctrl->lock);

        /* OS assert. */
        configASSERT(0U);

        FSP_RETURN(FSP_ERR_IN_USE);
    }

    /* Set mask of task. */
    WATCHDOG_SERVICE_W_SET_BIT((p_ctrl->tasks_mask[id / 32U]), (id % 32U));
    WATCHDOG_SERVICE_W_SET_BIT((p_ctrl->tasks_monitored_mask[id / 32U]), (id % 32U));

    /* Get current task. */
    p_ctrl->tasks_handle[id] = xTaskGetCurrentTaskHandle();

    /* Update max ID of tasks. */
    if (id > p_ctrl->max_task_id)
    {
        p_ctrl->max_task_id = id;
    }

    /* Set WDT NMI callback. */
    if (0U == id)
    {
        gp_watchdog_service_ctrl = p_ctrl;
        gp_watchdog_service_cfg  = p_cfg;
        p_cfg->p_wdt->p_api->callbackSet(p_cfg->p_wdt->p_ctrl, (void *) rm_watchdog_service_w_callback, NULL, NULL);
    }

    /* Put mutex. */
    xSemaphoreGiveRecursive(p_ctrl->lock);

    *p_id = id;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Unregister task from the Watchdog Service.
 *
 * Example:
 * @snippet rm_watchdog_service_w_example.c RM_WATCHDOG_SERVICE_W_Unregister
 *
 * @retval FSP_SUCCESS                 Task was unregistered successfully.
 * @retval FSP_ERR_ASSERTION           NULL pointer to parameters in argument.
 * @retval FSP_ERR_NOT_OPEN            Watchdog Service is not opened.
 * @retval FSP_ERR_INVALID_ARGUMENT    Id is out of range.
 **********************************************************************************************************************/
fsp_err_t RM_WATCHDOG_SERVICE_W_Unregister (watchdog_service_ctrl_t * const p_api_ctrl, uint8_t id)
{
    watchdog_service_w_instance_ctrl_t * p_ctrl = (watchdog_service_w_instance_ctrl_t *) p_api_ctrl;

#if WATCHDOG_SERVICE_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_ctrl);
    FSP_ERROR_RETURN(WATCHDOG_SERVICE_W_OPEN == p_ctrl->open, FSP_ERR_NOT_OPEN);
    WATCHDOG_SERVICE_W_VALIDATE_ID(id, FSP_ERR_INVALID_ARGUMENT);
#endif

    /* Get mutex. */
    xSemaphoreTakeRecursive(p_ctrl->lock, portMAX_DELAY);

    /* Clear parameters. */
    WATCHDOG_SERVICE_W_CLR_BIT((p_ctrl->tasks_mask[id / 32U]), (id % 32U));
    WATCHDOG_SERVICE_W_CLR_BIT((p_ctrl->tasks_monitored_mask[id / 32U]), (id % 32U));
    p_ctrl->tasks_latency[id] = 0U;
    p_ctrl->tasks_handle[id]  = NULL;

    /* Update maximum task id. */
    uint8_t idx = WATCHDOG_SERVICE_W_MAX_TASKS;
    while (1)
    {
        idx--;
        if ((WATCHDOG_SERVICE_W_CHK_BIT(p_ctrl->tasks_mask[idx / 32U], (idx % 32U))) || (0U == idx))
        {
            p_ctrl->max_task_id = idx;
            break;
        }
    }

    /* Put mutex. */
    xSemaphoreGiveRecursive(p_ctrl->lock);

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Suspend monitoring a task by the Watchdog Service.
 * A monitor-suspended task is not unregistered entirely, but it is ignored by the Watchdog Service until its monitoring
 * is resumed. It is faster than unregistering and registering the task again.
 *
 * Example:
 * @snippet rm_watchdog_service_w_example.c RM_WATCHDOG_SERVICE_W_Suspend
 *
 * @retval FSP_SUCCESS                 Monitoring task was suspended successfully.
 * @retval FSP_ERR_ASSERTION           NULL pointer to parameters in argument.
 * @retval FSP_ERR_NOT_OPEN            Watchdog Service is not opened.
 * @retval FSP_ERR_INVALID_ARGUMENT    Id is out of range.
 **********************************************************************************************************************/
fsp_err_t RM_WATCHDOG_SERVICE_W_Suspend (watchdog_service_ctrl_t * const p_api_ctrl, uint8_t id)
{
    watchdog_service_w_instance_ctrl_t * p_ctrl = (watchdog_service_w_instance_ctrl_t *) p_api_ctrl;

#if WATCHDOG_SERVICE_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_ctrl);
    FSP_ERROR_RETURN(WATCHDOG_SERVICE_W_OPEN == p_ctrl->open, FSP_ERR_NOT_OPEN);
    WATCHDOG_SERVICE_W_VALIDATE_ID(id, FSP_ERR_INVALID_ARGUMENT);
#endif

    /* Get mutex. */
    xSemaphoreTakeRecursive(p_ctrl->lock, portMAX_DELAY);

    /* Clear mask of monitored task. */
    WATCHDOG_SERVICE_W_CLR_BIT((p_ctrl->tasks_monitored_mask[id / 32U]), (id % 32U));

    /* Put mutex. */
    xSemaphoreGiveRecursive(p_ctrl->lock);

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Resume monitoring of a task by the Watchdog Service.
 * It should be called as soon as the reason that suspend API was called is removed.
 *
 * Example:
 * @snippet rm_watchdog_service_w_example.c RM_WATCHDOG_SERVICE_W_Resume
 *
 * @retval FSP_SUCCESS                 Monitoring task was resumed successfully.
 * @retval FSP_ERR_ASSERTION           NULL pointer to parameters in argument.
 * @retval FSP_ERR_NOT_OPEN            Watchdog Service is not opened.
 * @retval FSP_ERR_INVALID_ARGUMENT    Id is out of range.
 **********************************************************************************************************************/
fsp_err_t RM_WATCHDOG_SERVICE_W_Resume (watchdog_service_ctrl_t * const p_api_ctrl, uint8_t id)
{
    watchdog_service_w_instance_ctrl_t * p_ctrl = (watchdog_service_w_instance_ctrl_t *) p_api_ctrl;

#if WATCHDOG_SERVICE_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_ctrl);
    FSP_ERROR_RETURN(WATCHDOG_SERVICE_W_OPEN == p_ctrl->open, FSP_ERR_NOT_OPEN);
    WATCHDOG_SERVICE_W_VALIDATE_ID(id, FSP_ERR_INVALID_ARGUMENT);
#endif

    /* Resume monitoring. */
    rm_watchdog_service_w_resume_base(p_ctrl, id);

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Notify the Watchdog Service about a task.
 * A registered task shall call this API periodically to notify the Watchdog Service that it is alive.
 * This should be done frequently enough to fit into the watchdog timer interval.
 *
 * Example:
 * @snippet rm_watchdog_service_w_example.c RM_WATCHDOG_SERVICE_W_Notify
 *
 * @retval FSP_SUCCESS                 Task notified successfully.
 * @retval FSP_ERR_ASSERTION           NULL pointer to parameters in argument.
 * @retval FSP_ERR_NOT_OPEN            Watchdog Service is not opened.
 * @retval FSP_ERR_INVALID_ARGUMENT    Id is out of range.
 **********************************************************************************************************************/
fsp_err_t RM_WATCHDOG_SERVICE_W_Notify (watchdog_service_ctrl_t * const      p_api_ctrl,
                                        watchdog_service_cfg_t const * const p_cfg,
                                        uint8_t                              id)
{
    watchdog_service_w_instance_ctrl_t * p_ctrl = (watchdog_service_w_instance_ctrl_t *) p_api_ctrl;

#if WATCHDOG_SERVICE_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_ctrl);
    FSP_ASSERT(NULL != p_cfg);
    FSP_ASSERT(NULL != p_cfg->p_wdt);
    FSP_ERROR_RETURN(WATCHDOG_SERVICE_W_OPEN == p_ctrl->open, FSP_ERR_NOT_OPEN);
    WATCHDOG_SERVICE_W_VALIDATE_ID(id, FSP_ERR_INVALID_ARGUMENT);
#endif

    /* Notification. */
    rm_watchdog_service_w_notify_base(p_ctrl, p_cfg, id);

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Resume monitoring of a task and notify the Watchdog Service about the task.
 *
 * Example:
 * @snippet rm_watchdog_service_w_example.c RM_WATCHDOG_SERVICE_W_ResumeAndNotify
 *
 * @retval FSP_SUCCESS                 Resume monitoring task and task notified successfully.
 * @retval FSP_ERR_ASSERTION           NULL pointer to parameters in argument.
 * @retval FSP_ERR_NOT_OPEN            Watchdog Service is not opened.
 * @retval FSP_ERR_INVALID_ARGUMENT    Id is out of range.
 **********************************************************************************************************************/
fsp_err_t RM_WATCHDOG_SERVICE_W_ResumeAndNotify (watchdog_service_ctrl_t * const      p_api_ctrl,
                                                 watchdog_service_cfg_t const * const p_cfg,
                                                 uint8_t                              id)
{
    watchdog_service_w_instance_ctrl_t * p_ctrl = (watchdog_service_w_instance_ctrl_t *) p_api_ctrl;

#if WATCHDOG_SERVICE_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_ctrl);
    FSP_ASSERT(NULL != p_cfg);
    FSP_ASSERT(NULL != p_cfg->p_wdt);
    FSP_ERROR_RETURN(WATCHDOG_SERVICE_W_OPEN == p_ctrl->open, FSP_ERR_NOT_OPEN);
    WATCHDOG_SERVICE_W_VALIDATE_ID(id, FSP_ERR_INVALID_ARGUMENT);
#endif

    /* Get mutex. */
    xSemaphoreTakeRecursive(p_ctrl->lock, portMAX_DELAY);

    /* Resume monitoring. */
    rm_watchdog_service_w_resume_base(p_ctrl, id);

    /* Notification. */
    rm_watchdog_service_w_notify_task(p_ctrl, p_cfg, id);

    /* Put mutex. */
    xSemaphoreGiveRecursive(p_ctrl->lock);

    /* Notify from idle task. */
    rm_watchdog_service_w_notify_idle(p_ctrl, p_cfg, id);

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Notify the Watchdog Service about the idle task.
 *
 * Example:
 * @snippet rm_watchdog_service_w_example.c RM_WATCHDOG_SERVICE_W_NotifyFromIdle
 *
 * @retval FSP_SUCCESS                 Idle task notified successfully.
 * @retval FSP_ERR_ASSERTION           NULL pointer to parameters in argument.
 * @retval FSP_ERR_NOT_OPEN            Watchdog Service is not opened.
 * @retval FSP_ERR_INVALID_ARGUMENT    Id is out of range.
 **********************************************************************************************************************/
fsp_err_t RM_WATCHDOG_SERVICE_W_NotifyFromIdle (watchdog_service_ctrl_t * const      p_api_ctrl,
                                                watchdog_service_cfg_t const * const p_cfg)
{
    watchdog_service_w_instance_ctrl_t * p_ctrl = (watchdog_service_w_instance_ctrl_t *) p_api_ctrl;

#if WATCHDOG_SERVICE_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_ctrl);
    FSP_ASSERT(NULL != p_cfg);
    FSP_ASSERT(NULL != p_cfg->p_wdt);
    FSP_ERROR_RETURN(WATCHDOG_SERVICE_W_OPEN == p_ctrl->open, FSP_ERR_NOT_OPEN);
    WATCHDOG_SERVICE_W_VALIDATE_ID(p_ctrl->idle_task_id, FSP_ERR_INVALID_ARGUMENT);
#endif

    /* Get mutex. */
    if (pdPASS == xSemaphoreTakeRecursive(p_ctrl->lock, 0UL))
    {
        /* Notify from idle task. */
        rm_watchdog_service_w_notify_task(p_ctrl, p_cfg, p_ctrl->idle_task_id);

        /* Put mutex. */
        xSemaphoreGiveRecursive(p_ctrl->lock);
    }

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Set watchdog latency for a task.
 * This allows a task to miss a given number of notification periods to the Watchdog Service without triggering a system
 * reset. Once set, the task is allowed to not notify the Watchdog Service for “latency” consecutive watchdog timer
 * intervals. This option can be used to facilitate the operation of code that is expected to remain blocked for long
 * periods of time (i.e. computation). This value is set once and does not reload automatically, thus it shall be set
 * every time increased latency is required.
 *
 * Example:
 * @snippet rm_watchdog_service_w_example.c RM_WATCHDOG_SERVICE_W_SetLatency
 *
 * @retval FSP_SUCCESS                 Set latency successfully.
 * @retval FSP_ERR_ASSERTION           NULL pointer to parameters in argument.
 * @retval FSP_ERR_NOT_OPEN            Watchdog Service is not opened.
 * @retval FSP_ERR_INVALID_ARGUMENT    Id is out of range.
 **********************************************************************************************************************/
fsp_err_t RM_WATCHDOG_SERVICE_W_SetLatency (watchdog_service_ctrl_t * const p_api_ctrl, uint8_t id, uint8_t latency)

{
    watchdog_service_w_instance_ctrl_t * p_ctrl = (watchdog_service_w_instance_ctrl_t *) p_api_ctrl;

#if WATCHDOG_SERVICE_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_ctrl);
    FSP_ERROR_RETURN(WATCHDOG_SERVICE_W_OPEN == p_ctrl->open, FSP_ERR_NOT_OPEN);
    WATCHDOG_SERVICE_W_VALIDATE_ID(id, FSP_ERR_INVALID_ARGUMENT);
#endif

    /* Get mutex. */
    xSemaphoreTakeRecursive(p_ctrl->lock, portMAX_DELAY);

    /* Set latency. */
    p_ctrl->tasks_latency[id] = latency;

    /* Put mutex. */
    xSemaphoreGiveRecursive(p_ctrl->lock);

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Set watchdog ID of the idle task.
 *
 * Example:
 * @snippet rm_watchdog_service_w_example.c RM_WATCHDOG_SERVICE_W_SetIdleId
 *
 * @retval FSP_SUCCESS                 Set idle id successfully.
 * @retval FSP_ERR_ASSERTION           NULL pointer to parameters in argument.
 * @retval FSP_ERR_NOT_OPEN            Watchdog Service is not opened.
 * @retval FSP_ERR_INVALID_ARGUMENT    Id is out of range.
 **********************************************************************************************************************/
fsp_err_t RM_WATCHDOG_SERVICE_W_SetIdleId (watchdog_service_ctrl_t * const p_api_ctrl, uint8_t id)
{
    watchdog_service_w_instance_ctrl_t * p_ctrl = (watchdog_service_w_instance_ctrl_t *) p_api_ctrl;

#if WATCHDOG_SERVICE_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_ctrl);
    FSP_ERROR_RETURN(WATCHDOG_SERVICE_W_OPEN == p_ctrl->open, FSP_ERR_NOT_OPEN);
    WATCHDOG_SERVICE_W_VALIDATE_ID(id, FSP_ERR_INVALID_ARGUMENT);
#endif

    /* Set idle id. */
    p_ctrl->idle_task_id = id;

    /* Get handle of idle. */
    p_ctrl->tasks_handle[id] = xTaskGetIdleTaskHandle();

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Find out if the only task monitored is the idle task.
 *
 * @retval FSP_SUCCESS                 Only the idle task is monitored.
 * @retval FSP_ERR_IN_USE              Tasks other than the idle task are monitored.
 * @retval FSP_ERR_ASSERTION           NULL pointer to parameters in argument.
 * @retval FSP_ERR_NOT_OPEN            Watchdog Service is not opened.
 **********************************************************************************************************************/
fsp_err_t RM_WATCHDOG_SERVICE_W_IsMonitorMaskEmpty (watchdog_service_ctrl_t * const p_api_ctrl)
{
    watchdog_service_w_instance_ctrl_t * p_ctrl = (watchdog_service_w_instance_ctrl_t *) p_api_ctrl;

#if WATCHDOG_SERVICE_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_ctrl);
    FSP_ERROR_RETURN(WATCHDOG_SERVICE_W_OPEN == p_ctrl->open, FSP_ERR_NOT_OPEN);
#endif

    if (WATCHDOG_SERVICE_W_NOT_REGISTERED_ID != p_ctrl->idle_task_id)
    {
        for (uint8_t idx = 0U; idx < WATCHDOG_SERVICE_W_MAX_INDEX; idx++)
        {
            if ((p_ctrl->idle_task_id / 32U) == idx)
            {
                if ((1U << p_ctrl->idle_task_id) != p_ctrl->tasks_monitored_mask[idx])
                {
                    FSP_RETURN(FSP_ERR_IN_USE);
                }
            }
            else
            {
                if (0U != p_ctrl->tasks_monitored_mask[idx])
                {
                    FSP_RETURN(FSP_ERR_IN_USE);
                }
            }
        }

        return FSP_SUCCESS;
    }

    FSP_RETURN(FSP_ERR_IN_USE);
}

/*******************************************************************************************************************//**
 * @} (end addtogroup WATCHDOG_SERVICE_W)
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * Store thread info at WDT NMI.
 **********************************************************************************************************************/
BSP_WEAK_REFERENCE uint16_t BSP_SaveOopsDumpThreadInfo (void)
{
    return 0;
}

/*******************************************************************************************************************//**
 * Store system register values at WDT NMI and wait for system reset.
 *
 * @param[in]     p_exception_args   Pointer to system register values at NMI.
 **********************************************************************************************************************/
BSP_WEAK_REFERENCE void rm_watchdog_service_w_nmi_reset (uint32_t * p_exception_args)
{
    if (NULL != p_exception_args)
    {
        g_watchdog_service_w_nmi_event_data[0] = WATCHDOG_SERVICE_W_NMI_MAGIC_NUMBER;
        g_watchdog_service_w_nmi_event_data[1] = p_exception_args[0]; // R0
        g_watchdog_service_w_nmi_event_data[2] = p_exception_args[1]; // R1
        g_watchdog_service_w_nmi_event_data[3] = p_exception_args[2]; // R2
        g_watchdog_service_w_nmi_event_data[4] = p_exception_args[3]; // R3
        g_watchdog_service_w_nmi_event_data[5] = p_exception_args[4]; // R12
        g_watchdog_service_w_nmi_event_data[6] = p_exception_args[5]; // LR
        g_watchdog_service_w_nmi_event_data[7] = p_exception_args[6]; // PC
        g_watchdog_service_w_nmi_event_data[8] = p_exception_args[7]; // PSR

        BSP_SaveOopsDumpThreadInfo();
    }

    /* Wait for system reset. */
    while (1)
    {
        ;
    }
}

/***********************************************************************************************************************
 * Private Functions
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * Get empty task ID.
 *
 * @param[in]     p_ctrl     Pointer to instance control structure.
 **********************************************************************************************************************/
static uint8_t rm_watchdog_service_w_get_empty_id (watchdog_service_w_instance_ctrl_t * p_ctrl)
{
    for (uint8_t idx = 0U; idx < WATCHDOG_SERVICE_W_MAX_TASKS; idx++)
    {
        if (0U == (WATCHDOG_SERVICE_W_CHK_BIT(p_ctrl->tasks_mask[idx / 32U], (idx % 32U))))
        {
            return idx;
        }
    }

    return WATCHDOG_SERVICE_W_FALSE;
}

/*******************************************************************************************************************//**
 * Callback at WDT NMI.
 *
 * @param[in]     p_args     Pointer to WDT callback args.
 **********************************************************************************************************************/
static void rm_watchdog_service_w_callback (wdt_callback_args_t * p_args)
{
    uint32_t tmp_mask[WATCHDOG_SERVICE_W_MAX_INDEX]     = {0U};
    uint32_t latency_mask[WATCHDOG_SERVICE_W_MAX_INDEX] = {0U};
    uint8_t  i;

    for (i = 0U; i <= gp_watchdog_service_ctrl->max_task_id; i++)
    {
        if (0U != gp_watchdog_service_ctrl->tasks_latency[i])
        {
            gp_watchdog_service_ctrl->tasks_latency[i]--;
            WATCHDOG_SERVICE_W_SET_BIT((latency_mask[i / 32U]), (i % 32U));
        }
    }

    for (i = 0U; i < WATCHDOG_SERVICE_W_MAX_INDEX; i++)
    {
        tmp_mask[i]  = gp_watchdog_service_ctrl->tasks_monitored_mask[i];
        tmp_mask[i] &= ~latency_mask[i];

        if ((gp_watchdog_service_ctrl->notified_mask[i] & tmp_mask[i]) != tmp_mask[i])
        {
            break;
        }
    }

    if (WATCHDOG_SERVICE_W_MAX_INDEX > i)
    {
        /* Latency for all tasks expired and some of them still did not notify Watchdog Service.
         * Let Watchdog Service reset the system. */
        rm_watchdog_service_w_nmi_reset((uint32_t *) p_args->p_context);
    }

    rm_watchdog_service_w_reset_watchdog(gp_watchdog_service_ctrl, gp_watchdog_service_cfg);
}

/*******************************************************************************************************************//**
 * Reset watchdog.
 *
 * @param[in]     p_ctrl     Pointer to instance control structure.
 * @param[in]     p_cfg      Pointer to configuration structure.
 **********************************************************************************************************************/
static void rm_watchdog_service_w_reset_watchdog (watchdog_service_w_instance_ctrl_t * p_ctrl,
                                                  watchdog_service_cfg_t const * const p_cfg)
{
    /* Clear mask for notification. */
    memset(p_ctrl->notified_mask, 0U, sizeof(p_ctrl->notified_mask));

    /* Refresh WDT. */
    wdt_instance_t const * p_wdt = p_cfg->p_wdt;
    R_WDOG_W_TimeoutSet(p_wdt->p_ctrl, p_wdt->p_cfg->timeout);
    p_wdt->p_api->refresh(p_wdt->p_ctrl);
}

/*******************************************************************************************************************//**
 * Resume monitoring of a task.
 *
 * @param[in]     p_ctrl     Pointer to instance control structure.
 * @param[in]     id         Id of task.
 **********************************************************************************************************************/
static void rm_watchdog_service_w_resume_base (watchdog_service_w_instance_ctrl_t * p_ctrl, uint8_t id)
{
    /* Update mask of monitoring task. */
    WATCHDOG_SERVICE_W_SET_BIT((p_ctrl->tasks_monitored_mask[id / 32U]), (id % 32U));
    for (uint8_t idx = 0U; idx < WATCHDOG_SERVICE_W_MAX_INDEX; idx++)
    {
        p_ctrl->tasks_monitored_mask[idx] &= p_ctrl->tasks_mask[idx];
    }
}

/*******************************************************************************************************************//**
 * Notify Watchdog Service of task and idle task.
 *
 * @param[in]     p_ctrl     Pointer to instance control structure.
 * @param[in]     p_cfg      Pointer to configuration structure.
 * @param[in]     id         Id of task.
 **********************************************************************************************************************/
static void rm_watchdog_service_w_notify_base (watchdog_service_w_instance_ctrl_t * p_ctrl,
                                               watchdog_service_cfg_t const * const p_cfg,
                                               uint8_t                              id)
{
    /* Get mutex. */
    xSemaphoreTakeRecursive(p_ctrl->lock, portMAX_DELAY);

    /* Notification. */
    rm_watchdog_service_w_notify_task(p_ctrl, p_cfg, id);

    /* Put mutex. */
    xSemaphoreGiveRecursive(p_ctrl->lock);

    /* Notification from idle task. */
    rm_watchdog_service_w_notify_idle(p_ctrl, p_cfg, id);
}

/*******************************************************************************************************************//**
 * Notify Watchdog Service of task.
 *
 * @param[in]     p_ctrl     Pointer to instance control structure.
 * @param[in]     p_cfg      Pointer to configuration structure.
 * @param[in]     id         Id of task.
 **********************************************************************************************************************/
static void rm_watchdog_service_w_notify_task (watchdog_service_w_instance_ctrl_t * p_ctrl,
                                               watchdog_service_cfg_t const * const p_cfg,
                                               uint8_t                              id)
{
    uint8_t idx;

    /* OS assert. */
    configASSERT(WATCHDOG_SERVICE_W_CHK_BIT(p_ctrl->tasks_mask[id / 32U], (id % 32U)));

    if (WATCHDOG_SERVICE_W_CHK_BIT(p_ctrl->tasks_mask[id / 32U], (id % 32U)))
    {
        WATCHDOG_SERVICE_W_SET_BIT((p_ctrl->notified_mask[id / 32U]), (id % 32U));
        p_ctrl->tasks_latency[id] = 0U;
        for (idx = 0U; idx < WATCHDOG_SERVICE_W_MAX_INDEX; idx++)
        {
            if ((p_ctrl->notified_mask[idx] & p_ctrl->tasks_monitored_mask[idx]) != p_ctrl->tasks_monitored_mask[idx])
            {
                break;
            }
        }

        if (idx >= WATCHDOG_SERVICE_W_MAX_INDEX)
        {
            rm_watchdog_service_w_reset_watchdog(p_ctrl, p_cfg);
        }
    }
}

/*******************************************************************************************************************//**
 * Notify Watchdog Service of idle task.
 *
 * @param[in]     p_ctrl     Pointer to instance control structure.
 * @param[in]     p_cfg      Pointer to configuration structure.
 * @param[in]     id         Id of task.
 **********************************************************************************************************************/
static void rm_watchdog_service_w_notify_idle (watchdog_service_w_instance_ctrl_t * p_ctrl,
                                               watchdog_service_cfg_t const * const p_cfg,
                                               uint8_t                              id)
{
    /* Notify the idle task every time one of the monitored tasks notifies the service. */
    if ((id != p_ctrl->idle_task_id) && (WATCHDOG_SERVICE_W_NOT_REGISTERED_ID != p_ctrl->idle_task_id))
    {
        rm_watchdog_service_w_notify_base(p_ctrl, p_cfg, p_ctrl->idle_task_id);
    }
}
