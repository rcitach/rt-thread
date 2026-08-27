/**
 * \addtogroup MID_SYS_ADAPTERS
 * \{
 * \addtogroup DPM_ADAPTER DPM Adapter
 *
 * \brief Adapter for DPM Timer
 *
 * \{
 */

/**
 ****************************************************************************************
 *
 * @file ad_dpm_timer.h
 *
 * @brief DPM timer Controller access API
 *
 * Copyright (c) 2023-2024 Renesas Electronics. All rights reserved.
 *
 * This software ("Software") is owned by Renesas Electronics.
 *
 * By using this Software you agree that Renesas Electronics retains all
 * intellectual property and proprietary rights in and to this Software and any
 * use, reproduction, disclosure or distribution of the Software without express
 * written permission or a license agreement from Renesas Electronics is
 * strictly prohibited. This Software is solely for use on or in conjunction
 * with Renesas Electronics products.
 *
 * EXCEPT AS OTHERWISE PROVIDED IN A LICENSE AGREEMENT BETWEEN THE PARTIES, THE
 * SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. EXCEPT AS OTHERWISE
 * PROVIDED IN A LICENSE AGREEMENT BETWEEN THE PARTIES, IN NO EVENT SHALL
 * RENESAS ELECTRONICS BE LIABLE FOR ANY DIRECT, SPECIAL, INDIRECT, INCIDENTAL,
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THE SOFTWARE.
 *
 ****************************************************************************************
 */

#ifndef AD_DPM_TIMER_H_
#define AD_DPM_TIMER_H_

#if dg_configDPM_TIMER_ADAPTER
#include "bsp_pd.h"

#define DPM_SCHEDULER_DEBUG

#define DPM_POWER_DOWN		            1
#define DPM_POWER_ON			        0

#define DPM_IDX_USING		            0
#define DPM_IDX_NEXT			        1
#define DPM_IDX_POWER_STATUS            2
#define DPM_IDX_RESERVED0	            3

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __attribute__((packed)) {
    void (*func)(void*);
    void *param;
} ad_dpm_timer_callback_type_t;

typedef struct __attribute__((packed)) _ad_dpm_timer_ {
    union {
        uint32_t space;
        uint8_t content[4];
    } type;
    ad_dpm_timer_callback_type_t callback;
    void *booting_offset;
    int64_t time;
} ad_dpm_timer_t;

typedef struct __attribute__((packed)) _ad_dpm_timer_param_ {
    void *callback_param;
    void *callback_func;
    void *booting_offset;
} ad_dpm_timer_param_t;

#define MAX_DPM_TIMER   16

typedef enum {
    AD_DPM_TIMER_0 = 0,
    AD_DPM_TIMER_1,
    AD_DPM_TIMER_2,
    AD_DPM_TIMER_3,
    AD_DPM_TIMER_4,
    AD_DPM_TIMER_5,
    AD_DPM_TIMER_6,
    AD_DPM_TIMER_7,
    AD_DPM_TIMER_8,
    AD_DPM_TIMER_9,
    AD_DPM_TIMER_10,
    AD_DPM_TIMER_11,
    AD_DPM_TIMER_12,
    AD_DPM_TIMER_13,
    AD_DPM_TIMER_14,
    AD_DPM_TIMER_15,
    AD_DPM_TIMER_ERR
} AD_DPM_TIMER_ID;

typedef struct __attribute__((packed)) _ad_dpm_timer_map_ {
    ad_dpm_timer_t timer[MAX_DPM_TIMER];
    uint32_t time_slice;
    uint32_t boot_condition;
    uint32_t curIdx;
    uint32_t systime_offset;
    uint32_t systime_offset_msec;
    uint64_t timer_offset;
    uint32_t isr_flag;
} ad_dpm_timer_map_t;

#define AD_DPM_TIMER_BASE                        (dg_configSCHEDULER_RTM_ADDR)

/**
 * \brief Get empty DPM timer ID
 *
 * This function:
 * - return empty DPM timer ID
 */
AD_DPM_TIMER_ID ad_dpm_get_empty_id(void);

/**
 * \brief Set DPM timer
 *
 * This function register the Timer. the timer interrupt shall occur after wake-up
 *
 * \param [in] id       DPM Timer ID
 * \param [in] time     Time for interrupt(or wake-up) occur
 * \param [in] param    callback function and it's parameter.
 *
 * \sa ad_dpm_set_wakeup_timer()
 *
 * \return id value if success, if not AD_DPM_TIMER_ERR
 */
AD_DPM_TIMER_ID ad_dpm_set_timer(AD_DPM_TIMER_ID id, uint64_t time, ad_dpm_timer_param_t param);

/**
 * \brief Set DPM timer and enter to the sleep mode3
 *
 *  This function enter to the sleep mode3 and wake up with next ID.
 *
 * \param [in] id       DPM Timer ID
 * \param [in] time     Time for interrupt(or wake-up) occur
 * \param [in] param    callback function and it's parameter.
 *
 * \sa ad_dpm_set_timer()
 *
 * \return enter sleep mode 3 if success, if not AD_DPM_TIMER_ERR
 */
AD_DPM_TIMER_ID ad_dpm_set_wakeup_timer(AD_DPM_TIMER_ID id, int64_t time, ad_dpm_timer_param_t param);

/**
 * \brief Destroy DPM timer
 *
 *  This function destroy DPM timer.
 *
 * \param [in] id       DPM Timer ID
 *
 * \return id if success, if not AD_DPM_TIMER_ERR
 */
AD_DPM_TIMER_ID ad_dpm_kill_timer(AD_DPM_TIMER_ID id);

/* it should be called in the system init step */
/**
 * \brief DPM timer scheduler
 *
 * This function:
 * - Should be called in system initialization.
 * - Initializes retention memory for DPM and make timer interrupt occur.
 * - Manage the DPM timers order.
 *
 *
 * \return wake-up source if success
 */
HW_RTC_WAKEUP_SOURCE ad_dpm_timer_scheduler(void);

#ifdef DPM_SCHEDULER_DEBUG
void ad_dpm_print_timer_list(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* dg_configDPM_TIMER_ADAPTER */

#endif /* AD_DPM_TIMER_H_*/
