/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/******************************************************************************************************************//**
 * @addtogroup PMGR_W
 * @{
 *********************************************************************************************************************/

#ifndef RM_PMGR_W_H
#define RM_PMGR_W_H

/**********************************************************************************************************************
 * Includes
 *********************************************************************************************************************/
#include "rm_pmgr_api.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "rm_pmgr_cfg.h"

#if CFG_WIFI
 #include "common.h"
 #include "mbedtls/ssl.h"
 #include "mbedtls/ssl_cookie.h"
 #include "mbedtls/error.h"
 #include "mbedtls/debug.h"

 #include "rm_pmgr_w_dpm_internal.h"
 #include "rm_pmgr_w_dpm_socket_internal.h"
 #include "rm_pmgr_w_rtm_internal.h"
#endif                                 /* CFG_WIFI */

/* Common macro for FSP header files. There is also a corresponding FSP_FOOTER macro at the end of this file. */
FSP_HEADER

/**********************************************************************************************************************
 * Macro definitions
 *********************************************************************************************************************/
#if CFG_WIFI
 #define MAX_DPM_INFO_ARRAY            32
 #define MAX_DPM_NAME_LEN              20
 #define MSEC_TO_SEC(ms)    (ms / 1000)
 #define RRQ61X_DPM_TLS_NOT_SAVE_PERR_CERT
 #define RRQ61X_DPM_TLS_DEF_TIMEOUT    100
#endif                                 /* CFG_WIFI */

#define MAX_TIMER_NOTIFIER_ARRAY       2

/** Debug runtime flag for RAM constraints counter cause */
typedef enum e_pmgr_debug_runtime_flag
{
    PMGR_DEBUG_RAM_UNSET    = -1,      ///< Debug runtime flag RAM constraints unset
    PMGR_DEBUG_RAM_DISABLED = 0,       ///< Debug runtime flag RAM constraints disabled
    PMGR_DEBUG_RAM_ENABLED  = 1,       ///< Debug runtime flag RAM constraints enabled
} pmgr_debug_runtime_t;

/***********************************************************************************************************************
 * Typedef definitions
 *********************************************************************************************************************/

/** The pmgr_w extend notifier with the following 2 features:
 *  - Allow multiple registration
 *      to allow multiple registration, the following is added
 *      - notifier_id - used to indicate the specific callback instance.
 *        When call to notifier_registration() notifier_id == UNDEFINE_NOTIFIER_ID
 *      - order - used to define the order of callback notification , higher order executed first
 *  - Allow asynchronous respond.
 */
typedef struct st_pmgr_w_notifier_extend
{
    uint32_t order;                    ///< used to define order of notifier execution, input
    uint32_t notifier_id;              ///< used to identify the notifier, output
} pmgr_w_notifier_extend_t;

typedef struct st_notifier_array_entry
{
    void (* p_callback)(pmgr_callback_args_t *);
    pmgr_callback_args_t   * p_callback_memory;
    pmgr_w_notifier_extend_t notify_extend;
} notifier_array_entry_t;

/** LLD wakeup source */
typedef enum e_pmgr_wake_source
{
    PMGR_WAKE_SOURCE_NONE = (0),          ///< no wake source defined
    PMGR_WAKE_SOURCE_RTC  = (1UL << (0)), ///< RTC wake source
    PMGR_WAKE_SOURCE_GPT  = (1UL << (1)), ///< GPT wake source
    PMGR_WAKE_SOURCE_GPIO = (1UL << (2)), ///< GPIO wake source
    PMGR_WAKE_SOURCE_ADC  = (1UL << (3)), ///< ADC wake source

    /** connectivity wake source. */
    PMGR_WAKE_SOURCE_WIFI = (1UL << (4)), ///< WIFI/MAC wake source
    PMGR_WAKE_SOURCE_BLE  = (1UL << (5)), ///< BLE wake source
} pmgr_wake_source_t;

typedef enum e_pmgr_lld_power_mode
{
    PMGR_LLD_POWER_MODE_INVALID = -1,
    PMGR_LLD_POWER_MODE_AUTO    = 0,
    PMGR_LLD_POWER_MODE_SLEEP2,        ///< Remain on: RTC, GPIO
    PMGR_LLD_POWER_MODE_SLEEP3,        ///< Remain on: RTC, GPIO, RTM
    PMGR_LLD_POWER_MODE_DPM,           ///< Remain on: RTC, GPIO, RTM + WiFi Connected
    PMGR_LLD_POWER_MODE_SLEEP4,        ///< Remain on: RTC, GPIO, RTM, RAM
    PMGR_LLD_POWER_MODE_CPU_WFI,
} pmgr_lld_power_mode_t;

/* Backwards compatibility (avoid using) */
#define PMGR_LLD_STATE2    PMGR_LLD_POWER_MODE_SLEEP2
#define PMGR_LLD_STATE3    PMGR_LLD_POWER_MODE_SLEEP3

typedef enum e_pmgr_dbg_ram_constraint_counter_cause
{
    PMGR_DBG_RAM_CONSTRAINT_GENERIC_FORCE, ///< If PMGR cli force DPM was done, this is the cause for decrease any ram constraints
    PMGR_DBG_RAM_CONSTRAINT_PMGR_CLI,
    PMGR_DBG_RAM_CONSTRAINT_DPM_AT,
    PMGR_DBG_RAM_CONSTRAINT_TCP_CLIENT,
    PMGR_DBG_RAM_CONSTRAINT_NET_IFCONFIG,
    PMGR_DBG_RAM_CONSTRAINT_APP_POLL_STATE,
    PMGR_DBG_RAM_CONSTRAINT_SNTP,
    PMGR_DBG_RAM_CONSTRAINT_APP_WEBSOCKET_CLIENT,
    PMGR_DBG_RAM_CONSTRAINT_DPM_SUPPLICANT,
    PMGR_DBG_RAM_CONSTRAINT_DPM_REG_NAME_TIMER,
    PMGR_DBG_RAM_CONSTRAINT_UDP_IPV6_DPM,
    PMGR_DBG_RAM_CONSTRAINT_TCPC_OVER_IPV6_DPM,
    PMGR_DBG_RAM_CONSTRAINT_HTTPC,
    PMGR_DBG_RAM_CONSTRAINT_DPM_LWIP_IPV6,
    PMGR_DBG_RAM_CONSTRAINT_DPM_KEY,
    PMGR_DBG_RAM_CONSTRAINT_WA_CAUSE,
    PMGR_DBG_RAM_CONSTRAINT_RWNX_DRV_TASK,
    PMGR_DBG_RAM_CONSTRAINT_RWNX_MAC_TASK,
    PMGR_DBG_RAM_CONSTRAINT_MQTT_SUB,
    PMGR_DBG_RAM_CONSTRAINT_MAX
} pmgr_dbg_ram_constraint_counter_cause_t;

typedef uint64_t (* timer_callback_t)(void * p_param);

typedef struct st_timer_notifier_array_entry
{
    timer_callback_t p_callback;
    void           * p_param;
} pmgr_timer_notifier_array_entry_t;

/** PMGR private control block. Initialization occurs when RM_PMGR_W_Open() is called. */
typedef struct st_pmgr_instance_ctrl
{
    pmgr_cfg_t const                * p_cfg;                                   ///< Pointer to initial configurations
    struct st_notifier_array_entry    notifier_array[PMGR_MAX_NOTIFIER_ARRAY]; ///< Array of registered applications
    pmgr_timer_notifier_array_entry_t timer_notifier_array[MAX_TIMER_NOTIFIER_ARRAY];
    uint8_t               constraint_counter[PMGR_CONSTRAINT_MAX];             ///< Array of constraints counter
    int16_t               ram_counter_cause[PMGR_DBG_RAM_CONSTRAINT_MAX];      ///< Array for debug ram constraints counter cause
    int                   debug_runtime_flag;                                  ///< Debug runtime flag for ram constraints counter cause
    pmgr_wake_source_t    wake_source_bitmap;                                  ///< Bitmap of wake sources
    pmgr_lld_power_mode_t lld_power_mode;                                      ///< Power mode for distinction if its force
} pmgr_instance_ctrl_t;

typedef enum e_pmgr_w_notifier_order
{
    PMGR_W_NOTIFIER_ORDER_SYS_LOW        = (10),  ///< 10-100 are reserved for system low order
    PMGR_W_NOTIFIER_ORDER_CUSTOMISED_LOW = (100), ///< 100-199 are reserved for customised application
    PMGR_W_NOTIFIER_ORDER_SYS_HIGH       = (200), ///< 200-139 are reserved for system high order
    PMGR_W_NOTIFIER_ORDER_HIGHEST        = (240), ///< first executed
} pmgr_w_notifier_order_t;

typedef struct st_pmgr_instance_info
{
    pmgr_lld_power_mode_t power_mode;       ///< selected power mode
    pmgr_wake_source_t    wake_source;      ///< bit map definition of the wake source
    uint64_t              tickless_time_us; ///< Time in us of tickless state (valid only when event is EXITING_SLEEP)
} pmgr_instance_info_t;

#if CFG_WIFI
typedef struct pmgr_w_info_dpm_sleep
{
    char         dpm_name[MAX_DPM_NAME_LEN];
    int          idx_ram_constr_dbg_tbl;
    unsigned int sleep_count;
    unsigned int port_number;
    unsigned int rcv_ready;
    unsigned int init_done;
    bool         is_managed;
} pmgr_w_info_dpm_sleep_t;

typedef enum e_pmgr_w_dpm_event
{
    PMGR_W_DPM_EVENT_NOT_SET = PMGR_EVENT_EXITING_SLEEP + 1, ///< event of sleep mode is not set yet
    PMGR_W_DPM_EVENT_ABNORMAL_WIFI_DISCONN,                  ///< WIFI Connect fail
    PMGR_W_DPM_EVENT_ABNORMAL_DHCP_RENEW_FAIL,               ///< DHCP Renew fail
    PMGR_W_DPM_EVENT_ABNORMAL_ARP_RESP_FAIL,                 ///< ARP response fail
    PMGR_W_DPM_EVENT_ABNORMAL_DPM_POWER_DOWN_FAIL,           ///< DPM Power down fail (30 Sec)
    PMGR_W_DPM_EVENT_ABNORMAL_NO_DATA,                       ///< No data for 5 seconds and DPM is ready
    PMGR_W_DPM_EVENT_ABNORMAL_DPM_APP_FAIL,                  ///< DPM application fail
} pmgr_w_dpm_event_t;
#endif /*CFG_WIFI*/

/**********************************************************************************************************************
 * Exported global variables
 *********************************************************************************************************************/

extern const pmgr_api_t g_pmgr_w_api;

fsp_err_t RM_PMGR_W_Open(pmgr_ctrl_t * const p_api_ctrl, pmgr_cfg_t const * const p_cfg);
fsp_err_t RM_PMGR_W_Close(pmgr_ctrl_t * const p_api_ctrl);
fsp_err_t RM_PMGR_W_notifier_register(pmgr_ctrl_t * const          p_ctrl,
                                      void (                     * p_callback)(pmgr_callback_args_t *),
                                      pmgr_callback_args_t * const p_callback_memory,
                                      void const                 * p_extend);
fsp_err_t RM_PMGR_W_timer_notifier_register(pmgr_ctrl_t * const p_ctrl, timer_callback_t p_callback, void * p_param);
fsp_err_t RM_PMGR_W_timer_notifier_unregister(pmgr_ctrl_t * const p_ctrl, timer_callback_t p_callback);
fsp_err_t RM_PMGR_W_enter_idle(pmgr_ctrl_t * const p_ctrl, uint64_t idle_time_us);

fsp_err_t RM_PMGR_W_add_sleep_constraint(pmgr_ctrl_t * const p_ctrl, pmgr_constraints_t constraint);
fsp_err_t RM_PMGR_W_remove_sleep_constraint(pmgr_ctrl_t * const p_ctrl, pmgr_constraints_t constraint);

bool RM_PMGR_W_IsSleep3Wakeup(void);

/** Instance API extension. */

/**
 * Unregister notifier_id from PMGR Instance
 *
 * @retval     FSP_SUCCESS               Unregistration done successfully
 * @retval     FSP_ERR_INVALID_ARGUMENT  Unregistration failed: such notifier id was not found
 */
fsp_err_t RM_PMGR_W_notifier_unregister(pmgr_ctrl_t * const p_ctrl, uint32_t notifier_id);

/**
 * Set wake source - system middleware and customised application are allowed to specify which component
 *                   are used as wake source and as such should be kept powered on.
 *                   Wake source are bit map, the implementation is boolean parameter per wake source
 *                   (no matter how many times a wake source is set, single clear will set it to false)
 *
 * @param[in]   p_ctrl                   Pointer to the PMGR control block.
 * @param[in]   wake_source              bit wise of single wake source that should be kept powered on.
 *
 * @retval     FSP_SUCCESS               Set wake source done successfully
 * @retval     FSP_ERR_IN_USE            Set wake source failed due to the semaphore is not available
 */
fsp_err_t RM_PMGR_W_set_wake_source(pmgr_ctrl_t * const p_ctrl, pmgr_wake_source_t wake_source);

/**
 * Clear  wake source - see set_wake_source for info
 *
 * @param[in]   p_ctrl                   Pointer to the PMGR control block.
 * @param[in]   wake_source              bit wise of single wake source that should be kept powered off.
 *
 * @retval     FSP_SUCCESS               Clear wake source done successfully
 * @retval     FSP_ERR_IN_USE            Clear wake source failed due to the semaphore is not available
 */
fsp_err_t RM_PMGR_W_clr_wake_source(pmgr_ctrl_t * const p_ctrl, pmgr_wake_source_t wake_source);

/**
 * Get wake source bitmap
 *
 * @param[in]   p_ctrl                   Pointer to the PMGR control block.
 * @param[in]   wake_source              pointer of wake source bitmap.
 *                                       use pmgr_wake_source_t to check which wake source is set.
 *
 * @retval     FSP_SUCCESS               Get wake source done successfully
 * @retval     FSP_ERR_IN_USE            Get wake source failed due to the semaphore is not available
 */
fsp_err_t RM_PMGR_W_get_wake_source(pmgr_ctrl_t * const p_ctrl, pmgr_wake_source_t * wake_source);

/**
 * @brief Forces a specific power mode, overriding the Power Manager (PMGR) decision making.
 *        This function allows an application to force a low-power sleep mode. The behavior
 *        depends on the desired power mode and the provided idle_time_us value.
 *
 * @note  This override can be permanent (sticky) for certain modes. To revert the force,
 *        and resume to automatic power management, you must explicitly set the mode
 *        to `PMGR_LLD_POWER_MODE_AUTO`.
 *
 *        - Deep sleep modes (RAM Power Off):
 *          For modes: PMGR_LLD_POWER_MODE_SLEEP2, PMGR_LLD_POWER_MODE_SLEEP3, and PMGR_LLD_POWER_MODE_DPM
 *          The forced power mode is lost upon wakeup (PMGR resumes to PMGR_LLD_POWER_MODE_AUTO)
 *          Based on idle_time_us value, the low power mode will be immediate or at next enter_idle call
 *
 *        - Light sleep modes (RAM maintained):
 *          For modes: PMGR_LLD_POWER_MODE_SLEEP4, PMGR_LLD_POWER_MODE_CPU_WFI
 *          The forced power modes is "sticky" and remains until changed by another call to 'force'
 *          idle_time_us value has no effect. The low power mode will be executed at next FreeRTOS idle context
 *
 * @param[in]   p_ctrl        Pointer to the PMGR control block.
 * @param[in]   mode          Defines the power mode to enter
 * @param[in]   idle_time_us  For Deep sleep modes only: The expected idle duration in microseconds.
 *                            - idle_time_us > 0, low power mode executed immediately
 *                            - idle_time_us = 0, low power mode executed at next enter_idle call
 *
 * @retval     FSP_SUCCESS    Force sleep mode done successfully
 */
fsp_err_t RM_PMGR_W_force(pmgr_ctrl_t * const p_ctrl, pmgr_lld_power_mode_t mode, uint64_t idle_time_us);

/**
 * Get the specified sleep constraint count
 *
 * @param[in]   p_ctrl                   Pointer to the PMGR control block.
 * @param[in]   constraint               Defines a bit map of single constraint.
 * @param[out]  counter                  counter value of the specified constraint
 *
 * @retval     FSP_SUCCESS               Getting the specified sleep constraint count done successfully
 * @retval     FSP_ERR_IN_USE            Getting the specified sleep constraint count failed due to the semaphore is not available
 */
fsp_err_t RM_PMGR_W_get_sleep_constraint_counter(pmgr_ctrl_t * const p_ctrl,
                                                 pmgr_constraints_t  constraint,
                                                 uint8_t           * counter);

/**
 * Get ctrl - Return the global pointer for the PMGR control block.
 *            It is the customer responsibility to verify open was called with not NULL value before.
 *
 * @retval   p_ctrl               Pointer to the PMGR control block.
 */
pmgr_ctrl_t * RM_PMGR_W_get_ctrl(void);

#if CFG_WIFI

/**
 * Register DPM job name to DPM system
 *
 * @param[in]   dpm_name                 DPM job name to add to DPM system.
 * @param[in]   dpm_port                 DPM port to use.
 *
 * @retval     FSP_SUCCESS               Add DPM job name done successfully
 */
fsp_err_t RM_PMGR_W_dpm_job_name_set(char * dpm_name, int dpm_port);

/**
 * Unregister DPM job name from DPM system
 *
 * @param[in]   dpm_name                 DPM job name to clear from DPM system.
 *
 * @retval     FSP_SUCCESS               Clear dpm job name done successfully
 */
fsp_err_t RM_PMGR_W_dpm_job_name_clear(char * dpm_name);

/**
 * Get whether RRQ61x is running in DPM POR (Power On Reset) or DPM Wakeup.
 *
 * @retval      true            If RRQ61x is running in DPM Wakeup.
 * @retval      false           If RRQ61x is running in DPM POR.
 */
int RM_PMGR_W_dpm_is_wakeup(void);

/**
 * Get wakeup reason.
 *
 * @retval      BSP_WAKEUP_RESET                    = 0x00,          Internal reset
 * @retval      BSP_WAKEUP_SOURCE_GPIO              = 0x01,          Boot from GPIO wake up signal
 * @retval      BSP_WAKEUP_SOURCE_WAKEUP_COUNTER    = 0x02,          Boot from wake up counter
 * @retval      BSP_WAKEUP_SOURCE_POR               = 0x04,          Boot from power on reset
 * @retval      BSP_WAKEUP_WATCHDOG                 = 0x08,          Boot from RTC_watch dog (not cpu watchdog)
 * @retval      BSP_WAKEUP_SENSOR                   = 0x10,          Boot from sensor (ADC)
 * @retval      BSP_WAKEUP_PULSE                    = 0x20,          Boot from pulse sensor
 * @retval      BSP_WAKEUP_RETENTION                = 0x80,          Boot from retention
 * @retval      BSP_WAKEUP_SOURCE_UNKNOWN           = 0xff,          Boot from unknown reason
 */
int RM_PMGR_W_dpm_wakeup_src_get(void);

/**
 * Get wakeup type.
 *
 * @param[in]   do_print        print sequence number
 *
 * @retval      DPM_UNKNOWN_WAKEUP      = 0,
 * @retval      DPM_RTCTIME_WAKEUP      = 1,
 * @retval      DPM_PACKET_WAKEUP       = 2,
 * @retval      DPM_USER_WAKEUP         = 3,
 * @retval      DPM_NOACK_WAKEUP        = 4,
 * @retval      DPM_DEAUTH_WAKEUP       = 5,
 * @retval      DPM_TIM_ERR_WAKEUP      = 6,
 * @retval      DPM_DDPS_BUFP_WAKEUP    = 7
 */
int RM_PMGR_W_dpm_wakeup_type_get(int do_print);

/**
 * Notify DPM system that the caller has just finished initialization for its DPM timer callback to be safely invoked.
 * If not set, RTC timer callback function won't be called.
 *
 * @param[in]   mod_name            Name of DPM job.
 *
 * @retval      0(DPM_SET_OK)       if successful.
 */
int RM_PMGR_W_dpm_wakeup_done(char * mod_name);

/**
 * Query whether the socket application has invoked RM_PMGR_W_dpm_rcv_ready_set().
 *
 * @param[in]   mod_name            Name of DPM job.
 *
 * @retval      0(DPM_SET_OK)       if successful.
 */
int RM_PMGR_W_dpm_rcv_ready_get(char * mod_name);

/**
 * Notify DPM system that the caller is ready to receive data.
 * Before calling this function, the job should be registered by RM_PMGR_W_dpm_job_name_set() with a valid port number.
 * If not set, received data will be dropped.
 *
 * @param[in]   mod_name        Name of DPM job.
 *
 * @retval      0(DPM_SET_OK)   if successful.
 */
int RM_PMGR_W_dpm_rcv_ready_set(char * mod_name);

/**
 * Query whether or not the job name has been registered.
 *
 * @param[in]   job_name            Name of DPM job.
 *
 * @retval      DPM_NOT_REGISTERED  DPM job name is not registered.
 * @retval      DPM_REG_DUP_NAME    DPM job name is already registered.
 */
int RM_PMGR_W_dpm_job_name_is_set(char * job_name);

/**
 * Get job name with which port_number is registered.
 *
 * @param[in]   port_number     Port number of DPM job name.
 *
 * @retval      job name string if the specified port number is found, NULL if not found.
 */
char * RM_PMGR_W_dpm_port_is_set(unsigned int port_number);

 #if CFG_PMGR

/**
 * Whether it is wakeup from abnormal sleep or not
 *
 * @retval      0           Wakeup from normal DPM sleep
 *
 * @retval      1           Wakeup from Abnormal sleep
 */
UCHAR RM_PMGR_W_dpm_wakeup_is_abnormal(void);

 #endif                                /* CFG_PMGR */

/**
 * In DPM sleep mode, the DPM timer count is also activated,
 * and when the timeout event occurs,
 * the RRQ61x wakes up and calls the registered callback function.
 *
 * @param[in]       task_name Name of the process that wants to use the DPM timer.
 *                      It must be the same as the name registered
 *                      using the RM_PMGR_W_dpm_job_name_set() function.
 * @param[in]       timer_name A string of 7 characters or less as a unique character
 *                       to distinguish timer.
 * @param[in]       callback_func Function pointer to be called when timeout occurs.
 *                          If there is no function you want to call, enter NULL.
 * @param[in]       time Timeout occurrence time in msec.
 * @param[in]       reschedule_time Periodic timeout occurrence time in msec.
 *                  When set to 0, timeout occurs only once with the time set as the value of msec.
 *
 * @retval          Internally set Timer ID 5 ~ 15. If Timer ID > 16, it is a failure.
 */
int RM_PMGR_W_dpm_timer_create(char       * task_name,
                               char       * timer_name,
                               void (     * callback_func)(char * name),
                               unsigned int time,
                               unsigned int reschedule_time);

/**
 * Delete the registered DPM timer.
 *
 * @param[in]       task_name       Name used to register the timer using RM_PMGR_W_dpm_timer_create() function.
 * @param[in]       timer_name      Name of the DPM timer to be deleted.
 *                                  It is the same name used when registering timer using RM_PMGR_W_dpm_timer_create() function.
 *
 * @retval          Success         Timer IDs 5 to 15 are returned.
 * @retval          Failed          -1 is returned.
 */
int RM_PMGR_W_dpm_timer_delete(char * task_name, char * timer_name);

/**
 * Change timeout value of the specified DPM timer running.
 *
 * @param[in]       task_name       Name used to register the timer using RM_PMGR_W_dpm_timer_create() function.
 * @param[in]       timer_name      DPM Timer name to change the time.  This is the same name used
 *                                  when registering the timer using the RM_PMGR_W_dpm_timer_create() function.
 * @param[in]       time           Time value to change in msecs.
 *
 * @retval          Success         Timer IDs 5 to 15 are returned.
 * @retval          Failed         -1 is returned.
 */
int RM_PMGR_W_dpm_timer_change(char * task_name, char * timer_name, unsigned int time);

/**
 * Get the remaining timeout value in msec of the specified running timer.
 *
 * @param[in]       task_name       Name used to register the timer using RM_PMGR_W_dpm_timer_create() function.
 * @param[in]       timer_name      DPM Timer name to check the remaining time.
 *                                  This is the same name used
 *                                  when registering the timer using the RM_PMGR_W_dpm_timer_create() function.
 *
 * @retval          Number of milli-seconds remaining until timer expiration.
 * @retval          -1              Failed
 */
int RM_PMGR_W_dpm_timer_remaining_msec_get(char * task_name, char * timer_name);

/**
 * Get remaining socket rx data length from buffer.
 *
 * @param[in]   fd              File descriptor
 *
 * @retval      tot_len         Remaining data length
 */
int RM_PMGR_W_socket_rx_data_is_remaining(int fd);

/**
 * Save TLS session for DPM
 *
 * @param[in]   name        Name of rtm memory.
 * @param[in]   ssl_ctx     Pointer of ssl context. TLS session has to be establihsed.
 *
 * @retval      0           Success
 */
int RM_PMGR_W_dpm_tls_session_set(const char * name, mbedtls_ssl_context * ssl_ctx);

/**
 * Recorver TLS session for DPM
 * @param[in]   name        Name of rtm memory.
 * @param[in]   ssl_ctx     Pointer of ssl context. ssl_ctx has to be initialized.
 *
 * @retval      0           Success
 */
int RM_PMGR_W_dpm_tls_session_get(const char * name, mbedtls_ssl_context * ssl_ctx);

/**
 * Release saved TLS session for DPM
 * @param[in]   name        Name of rtm memory.
 *
 * @retval      0           Success
 * @retval      -1          Failure
 */
int RM_PMGR_W_dpm_tls_session_clear(const char * name);

/**
 * Create user rtm pool
 *
 * @retval          true                Success
 */
unsigned int RM_PMGR_W_user_rtm_pool_create(void);

/**
 * Delete user rtm pool
 *
 * @retval          FSP_SUCCESS          Success
 */
fsp_err_t RM_PMGR_W_user_rtm_pool_delete(void);

/**
 * Get user rtm pointer with the specified name.
 *
 * @param[in]       name                Specified name of allocated memory.
 * @param[out]      data                Pointer to place allocated bytes pointer.
 *
 * @retval          0                   Success
 */
unsigned int RM_PMGR_W_user_rtm_get(char * name, unsigned char ** data);

/**
 * Get the initialization status of user retention memory.
 *
 * @retval          true                If user retention memory is ready to access.
 */
int RM_PMGR_W_user_rtm_is_init_done(void);

/**
 * Allocate bytes from user retention memory area.
 *
 * @param[in]       name                Specified name of allocated memory.
 * @param[out]      memory_ptr          Pointer to place allocated bytes pointer.
 * @param[in]       memory_size         Number of bytes to allocate.
 * @param[in]       wait_option         Suspension option.
 *
 * @retval          0                   Success
 */
unsigned int RM_PMGR_W_user_rtm_pool_alloc(char        * name,
                                           void       ** memory_ptr,
                                           unsigned long memory_size,
                                           unsigned long wait_option);

/**
 * Release previously allocated memory to user retention memory.
 *
 * @param[in]       name                Specified name of allocated memory.
 * @retval          0                   Success
 */
unsigned int RM_PMGR_W_user_rtm_free(char * name);

/**
 * Set to the RTM by rtm_static_key the value and value_ull
 *
 * @param[in]       key                rtm_static_key.
 * @param[in]       value_l            dpm_sntp value arg (defined by the key).
 * @param[in]       value_ull          dpm value arg (defined by the key).
 * @retval          FSP_SUCCESS        Success
 */
fsp_err_t RM_PMGR_W_rtm_static_set(int key, long value_l, unsigned long long value_ull);

/**
 * Get from the RTM by rtm_static_key the value and value_ull
 *
 * @param[in]       key                rtm_static_key.
 * @param[in]       p_value_l          RTM_FLAG_PTR->time_params.__timezone (defined by the key).
 * @param[in]       p_value_ull        dpm value arg (defined by the key).
 * @param[in]       pp_ptr             dpm RTM_SUPP status (defined by the key).
 * @retval          FSP_SUCCESS        Success
 */
fsp_err_t RM_PMGR_W_rtm_static_get(int key, long * p_value_l, unsigned long long * p_value_ull, void ** pp_ptr);

#endif                                 /* CFG_WIFI */

/** Common macro for FSP header files. There is also a corresponding FSP_HEADER macro at the top of this file. */
FSP_FOOTER

#endif

/******************************************************************************************************************//**
 * @} (end addtogroup PMGR_W)
 *********************************************************************************************************************/
