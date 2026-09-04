/**
 * \addtogroup MID_SYS_SERVICES
 * \{
 * \addtogroup CLOCK_MANAGER Clock Manager Service
 *
 * \brief Clock Manager
 *
 * \{
 */

/**
 ****************************************************************************************
 *
 * @file sys_clock_mgr.h
 *
 * @brief Clock Manager header file.
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

#ifndef SYS_CLOCK_MGR_H_
#define SYS_CLOCK_MGR_H_

#include <stdint.h>
#include <stdbool.h>
#include "bsp_api.h"
#include "bsp_clocks.h"
#include "sdk_defs.h"

#if ((DEVICE_FAMILY == DA1469X) || (DEVICE_FAMILY == DA1487X) || (DEVICE_FAMILY == DA1640X))
typedef enum {
        cm_sysclk_div1_clk_in_use,
        cm_sysclk_ahb_divider_in_use,
        cm_sysclk_pll_used_by_task,
        cm_sysclk_success
} cm_sys_clk_set_status_t;
#endif

#if (DEVICE_FAMILY == DA1640X)
typedef enum {
        cm_cb_priority_coupled_hyper_loosely    = 0, // Do not use it, reserved for system
        cm_cb_priority_coupled_ultra_loosely    = 1,
        cm_cb_priority_coupled_very_loosely     = 2,
        cm_cb_priority_coupled_loosely          = 3,
        cm_cb_priority_coupled_normal           = 4,
        cm_cb_priority_coupled_tightly          = 5,
        cm_cb_priority_coupled_very_tightly     = 6,
        cm_cb_priority_coupled_ultra_tightly    = 7,
        cm_cb_priority_coupled_hyper_tightly    = 8  // Do not use it, reserved for system
} cm_sys_clk_cb_priority_t;

typedef		struct _clock_callback_t_ {
	void	(*func)(sys_clk_is_t , uint32_t , void *);
	void	*param;

        // reserved for sys_clock_mgr, do not use !!
        cm_sys_clk_cb_priority_t   priority;
        struct _clock_callback_t_ *nxt;    
} 	cm_clock_callback_t;
#endif /* DA1640X */


/**
 * \brief Initialize clocks after power-up.
 *
 * \param[in] type The clock source to use as the system clock.
 *
 * \warning It must be called with interrupts enabled! It must be called only once, after power-up.
 */
void cm_sys_clk_init(sys_clk_t type);

/**
 * \brief Calibrate RCX
 */
void cm_rcx_calibrate(void);

#if (DEVICE_FAMILY == DA1468X)
/**
 * \brief Set the system clock.
 *
 * \details It attempts to set the sys_clk to one of the available options. If it is not possible
 *          because, for example, it was requested to switch to the fast RC clock (RCxxM) and a
 *          peripheral is clocked by DIV1, it returns false. If the request involves turning on the
 *          fast XTAL clock (XTALxxM), then the task will block and the XTALxxM will be powered on.
 *          The task will resume execution when the fast XTAL clock (XTALxxM) settles. The CM will
 *          restore the clock to the type set by this function after each wake-up, automatically,
 *          whenever the fast XTAL clock (XTALxxM) settles.
 *          Note: if the SysTick runs then it is the dg_configABORT_IF_SYSTICK_CLK_ERR setting that
 *          controls whether the switch will be aborted or not.
 *
 * \param[in] type The clock source to use as the system clock.
 *
 * \return True if the requested clock switch was applied, else false.
 *
 * \warning It may block. It cannot be called from Interrupt Context.
 */
bool cm_sys_clk_set(sys_clk_t type);
#elif ((DEVICE_FAMILY == DA1469X) || (DEVICE_FAMILY == DA1487X) || (DEVICE_FAMILY == DA1640X))
/**
 * \brief Set the system clock.
 *
 * \details It attempts to set the system clock to one of the available options. If the request
 *          involves turning on the fast XTAL clock (XTALxxM), then the task will block and the
 *          XTALxxM will be powered on. The task will resume execution when the fast XTAL clock
 *          (XTALxxM) settles. The CM will restore the clock to the type set by this function after
 *          each wake-up, automatically, whenever the fast XTAL clock (XTALxxM) settles.
 *
 *          PLL will be used as system clock when at least one task has requested it by setting type
 *          to sysclk_PLL96. System clock will remain to PLL until all tasks that requested PLL have
 *          called cm_sys_clk_set() with type other than sysclk_PLL96. In the meantime, if a task
 *          requests sysclk_XTAL32M or sysclk_RC32 it will get a return value of
 *          cm_sysclk_pll_used_by_task. The system clock will remain to sysclk_PLL96. It will be
 *          changed to sysclk_XTAL32M or sysclk_RC32 when the last task using the PLL requests to do
 *          so.
 *
 *          Note: if the SysTick runs then it is the dg_configABORT_IF_SYSTICK_CLK_ERR setting that
 *          controls whether the switch will be aborted or not.
 *
 * \param[in] type The clock source to use as the system clock.
 *
 * \return cm_sysclk_success            if the requested clock switch was applied
 *         cm_sysclk_din1_clk_in_use    if system clock cannot be switched because a peripheral is
 *                                      clocked by DIV1 clock
 *         cm_sysclk_ahb_divider_in_use if system clock cannot be switched to PLL because the AHB
 *                                      divider is not ahb_div1
 *         cm_sysclk_pll_used_by_task   if another task is using the PLL. In this case the system
 *                                      clock will be switched to XTALxxM when the last task using
 *                                      the PLL calls cm_sys_clk_set() with clock type sysclk_XTAL32M
 *                                      or sysclk_RC32
 *
 * \warning It may block. It cannot be called from Interrupt Context.
 */
cm_sys_clk_set_status_t cm_sys_clk_set(sys_clk_t type);
#endif


/**
 * \brief Set the CPU clock.
 *
 * \details It attempts to set the sys_clk and the AMBA High speed bus divider to achieve the CPU
 *          clock that is requested. ARM CPU runs using the AHB clock. Any restrictions of the
 *          cm_sys_clk_set(), cm_ahb_set_clock_divider() and cm_apb_set_clock_divider() apply here
 *          as well. The APB bus clock will be set to the maximum frequency.
 *          The function may return false if the requested frequency is not achievable.
 *
 * \param[in] clk The CPU clock frequency.
 *
 * \return True if the requested clock switch was applied, else false.
 *
 * \warning Since some frequencies can be achieved with the fast RC clock (RCxxM), this function
 *          will not change to using the fast XTAL clock (XTALxxM) or the PLL, if the fast RC clock
 *          (RCxxM) is the current system clock. It is the responsibility of the caller to switch
 *          to the fast XTAL clock (XTALxxM) or the PLL before calling this function. After
 *          switching, the function will not revert to using the fast RC clock (RCxxM) at any case.
 *          Thus, switching from/to the fast RC clock (RCxxM) may be considered as "manual" while
 *          the switching from/to any other system clock source is done automatically from this
 *          function. The setting of the clocks is done via calls to cm_sys_clk_set(),
 *          cm_ahb_set_clock_divider() and cm_apb_set_clock_divider(). It may block. It cannot be
 *          called from Interrupt Context.
 */
bool cm_cpu_clk_set(cpu_clk_t clk);

/**
 * \brief Set the system and the AHB bus clock (interrupt safe version).
 *
 * \details It sets the sys_clk to the fast XTAL clock (XTALxxM) or PLL and the AHB divider.
 *
 * \param[in] clk The clock source to use as the system clock.
 * \param[in] hdiv The divider of the AHB clock.
 *
 * \warning It is called with interrupts disabled. The caller must have checked that the current
 *          sys_clk is not the desired one before calling this function.
 *
 */
void cm_cpu_clk_set_fromISR(sys_clk_t clk, ahb_div_t hdiv);

/**
 * \brief Change the divider of the AMBA Peripheral Bus clock.
 *
 * \details The frequency of the APB clock is (system_clock / (1 << cm_ahbclk)) / (1 << cm_apbclk).
 *
 * \param[in] div The new value of the APB divider.
 *
 * \warning It may block. It cannot be called from Interrupt Context.
 */
void cm_apb_set_clock_divider(apb_div_t div);

/**
 * \brief Change the divider of the AMBA High speed Bus clock.
 *
 * \details The frequency of the AHB clock is (system_clock / (1 << cm_ahbclk)).
 *          Note: if the SysTick runs then it is the dg_configABORT_IF_SYSTICK_CLK_ERR setting that
 *          controls whether the switch will be aborted or not.
 *
 * \param[in] div The new value of the AHB divider.
 *
 * \return True if the divider was changed to the requested value, else false.
 *
 * \warning It may block. It cannot be called from Interrupt Context.
 */
bool cm_ahb_set_clock_divider(ahb_div_t div);

/**
 * \brief Returns the sys_clk that the system uses at that moment.
 *
 * \return The real sys_clk used by the system.
 *
 * \warning It may block. It cannot be called from Interrupt Context.
 */
sys_clk_t cm_sys_clk_get(void);

/**
 * \brief Returns the sys_clk that the system uses at that moment (interrupt safe version).
 *
 * \return The real sys_clk used by the system.
 */
sys_clk_t cm_sys_clk_get_fromISR(void);

/**
 * \brief Returns the AMBA Peripheral Bus clock divider.
 *
 * \return The pclk being used.
 *
 * \warning It may block. It cannot be called from Interrupt Context.
 */
apb_div_t cm_apb_get_clock_divider(void);

/**
 * \brief Returns the AMBA High speed Bus clock divider.
 *
 * \return The hclk being used.
 *
 * \warning It may block. It cannot be called from Interrupt Context.
 */
ahb_div_t cm_ahb_get_clock_divider(void);

/**
 * \brief Returns the CPU clock frequency.
 *
 * \return The CPU clock being used.
 *
 * \warning Any restrictions of the cm_sys_clk_get() and cm_ahb_get_clock_divider() apply here as
 *          well. It may block. It cannot be called from Interrupt Context.
 */
cpu_clk_t cm_cpu_clk_get(void);

/**
 * \brief Returns the CPU clock frequency (interrupt safe).
 *
 * \return The CPU clock being used.
 *
 * \warning It can be called from Interrupt Context.
 */
cpu_clk_t cm_cpu_clk_get_fromISR(void);

/**
 * \brief Calibrate RC32K.
 */
void cm_calibrate_rc32k(void);

#ifdef OS_FREERTOS
#if (DEVICE_FAMILY == DA1468X)
/**
 * \brief Start RCX calibration.
 */
__RETAINED_CODE void cm_calibrate_rcx_start(void);

/**
 * \brief Get RCX calibration results and update environment variables.
 *
 * \return True, if calibration has finished, else false.
 */
bool cm_calibrate_rcx_update(void);
#endif
/**
 * \brief Converts usec to RCX cycles.
 *
 * \return The number of RCX cycles for the given time period.
 *
 * \warning Maximum time period is 4.095msec.
 */
__RETAINED_CODE uint32_t cm_rcx_us_2_lpcycles(uint32_t usec);

/**
 * \brief Converts time to RCX cycles.
 *
 * \return The number of RCX cycles for the given time period.
 *
 * \warning This is a low accuracy function. To have good accuracy, the minimum time period should
 *        be 1msec and the maximum 200msec. Above 200msec, the function calculates more RCX cycles
 *        than necessary.
 */
uint32_t cm_rcx_us_2_lpcycles_low_acc(uint32_t usec);

#endif

/**
 * \brief Block until the fast XTAL clock (XTALxxM) is ready. If the fast XTAL clock (XTALxxM) is
 *        running then the function exits immediately.
 *
 * \warning It may block. It cannot be called from Interrupt Context.
 */
void cm_wait_xtalm_ready(void);

#ifdef OS_FREERTOS
#if ((DEVICE_FAMILY == DA1469X) || (DEVICE_FAMILY == DA1487X) || (DEVICE_FAMILY == DA1640X))
#if (BSP_CFG_LP_CLOCK_SOURCE == BSP_CLOCKS_SOURCE_LP_CLK_RCX)
/**
 * \brief Initialize the RCX calibration task.
 */
void cm_rcx_calibration_task_init(void);

/**
 * \brief Trigger RCX calibration.
 */
void cm_rcx_trigger_calibration(void);
#endif
#endif
/**
 * \brief Initialize the Low Power clock.
 *
 * \details It initializes and sets as LP clock either the RCX or the XTAL32K. Since the XTAL32K
 *          settling takes a long time, the system is kept in active mode until this completes.
 */
void cm_lp_clk_init(void);

/**
 * \brief Check if the Low Power clock is available.
 *
 * \return true if the LP clock is available, else false.
 *
 * \warning It does not block. It cannot be called from Interrupt Context.
 */
bool cm_lp_clk_is_avail(void);

/**
 * \brief Check if the Low Power clock is available, interrupt safe version.
 *
 * \return true if the LP clock is available, else false.
 *
 * \warning It does not block. It can be called from Interrupt Context.
 */
bool cm_lp_clk_is_avail_fromISR(void);

/**
 * \brief Wait until the Low Power clock is available.
 *
 * \warning It may block. It cannot be called from Interrupt Context.
 */
void cm_wait_lp_clk_ready(void);

/**
 * \brief   Clear the flag that indicates that the Low Power clock is available.
 *
 * \details It is called when the system wakes up from a "forced" deep sleep state and the XTAL32K
 *          is used as the LP clock so that the system won't enter into sleep until the crystal has
 *          settled.
 *
 * \warning It may block. It cannot be called from Interrupt Context.
 */
__RETAINED_CODE void cm_lp_clk_wakeup(void);

#endif /* OS_FREERTOS */

#if ((DEVICE_FAMILY == DA1469X) || (DEVICE_FAMILY == DA1487X) || (DEVICE_FAMILY == DA1640X))
/**
 * \brief Block until the PLL is locked. If the PLL is locked then the function exits
 *        immediately.
 *
 * \warning It may block. It cannot be called from Interrupt Context.
 */
void cm_wait_pll_lock(void);

#endif /* DA1469X */

/**
 * \brief Check if the fast XTAL clock (XTALxxM) is ready.
 *
 * \return True if the fast XTAL clock (XTALxxM) has settled, else false.
 */
__RETAINED_CODE bool cm_poll_xtalm_ready(void);

#if ((DEVICE_FAMILY == DA1469X) || (DEVICE_FAMILY == DA1487X) || (DEVICE_FAMILY == DA1640X))
/**
 * \brief Start the fast XTAL clock (XTALxxM)
 *
 * \details Checks if the fast XTAL clock (XTALxxM) is started. If not, it checks if there is a PDC
 * entry for starting the fast XTAL clock (XTALxxM). If there is, it uses PDC to start the fast
 * XTAL clock (XTALxxM). Otherwise, it enables the fast XTAL clock (XTALxxM) using
 * hw_clk_enable_sysclk().
 */
void cm_enable_xtalm(void);
#endif

#if (DEVICE_FAMILY == DA1468X)
/**
 * \brief Basic initialization of the system and low power clocks.
 *
 * \details It sets up the low power clock and initializes the system clock.
 *
 * \warning It must be called once, before the OS scheduler is started.
 */
void cm_clk_init_low_level(void);
#endif

/* ---------------------------------------------------------------------------------------------- */

/*
 * Functions intended to be used only by the Clock and Power Manager.
 */

/**
 * \brief Set the system clock (unprotected).
 *
 * \details It attempts to:
 *              - Prepare the system clock for sleep : called when the system is entering power-down mode.
 *                        The system clock settings of the application are kept in order to be able to
 *                        restore them. If the PLL is active it will be turned off.
 *                        (It is called with the scheduler stopped and all interrupts disabled in
 *                        this case.)
 *              - Restore the previous setting : called when the fast XTAL clock (XTALxxM) settles.
 *                        (It is called from ISR context with all interrupts disabled in this case.)
 *
 * \param[in] entering_sleep true if the system is going to sleep, else false.
 * \param[in] pll_off true if the pll clock should be disabled, otherwise false.
 *
 * \warning It must be called from Interrupt Context and/or with all interrupts disabled.
 *          The function is internal to the clock and power managers and should not be used externally!
 */
__RETAINED_CODE void cm_sys_clk_sleep(bool entering_sleep, bool pll_off);

/**
 * \brief Halt until the fast XTAL clock (XTALxxM) has settled.
 *
 * \details It executes a WFI() call waiting for the fast XTAL clock (XTALxxM) Ready interrupt.
 *          Any other interrupts that hit are served.
 */
__RETAINED_CODE void cm_halt_until_xtalm_ready(void);

#if ((DEVICE_FAMILY == DA1469X) || (DEVICE_FAMILY == DA1487X) || (DEVICE_FAMILY == DA1640X))
/**
 * \brief Register a callback function to be called then XTAL32M is ready
 *
 * \details cb pointer to the callback function
 */
void cm_register_xtal_ready_callback(void (*cb)(void));

/**
 * \brief Halt until PLL is locked
 *
 * \details It executes a WFI() call waiting for the PLL_LOCK_IRQn.
 */
__RETAINED_CODE void cm_halt_until_pll_locked(void);

/**
 * \brief Halt until system clock (either PLL or XTAL32M) is ready.
 *
 * \details It executes a WFI() call waiting for the XTALxxM Ready interrupt and PLL LOCK interrupt if needed.
 */
__RETAINED_CODE void cm_halt_until_sysclk_ready(void);

#endif /* DA1469X */


#if (DEVICE_FAMILY == DA1640X)
/**
 * \brief Register a calback function to be called when changing the AHB clock
 *
 * \return true if the callback is registered successfully, else false.
 */
bool    cm_register_clock_callback(cm_clock_callback_t *cevtcb);

/**
 * \brief Deregister a calback function to be called when changing the AHB clock
 *
 * \return true if the callback is unregistered successfully, else false.
 */
bool    cm_deregister_clock_callback(cm_clock_callback_t *cevtcb);

#if dg_configUSE_CLOCK_MGR
void  pll_monitor_status(uint32_t *locktime, uint32_t *maxlocktime, uint32_t *minlocktime, uint32_t *lockcnt, uint32_t *maxlockcnt, uint32_t *minlockcnt);
#ifdef OS_FREERTOS
bool sys_clk_mgr_match_task(const void *elem, const void *ud);
#endif /* OS_FREERTOS */
void XTAL32M_Ready_Handler(void);
void PLL_Lock_Handler(void);

#endif /* dg_configUSE_CLOCK_MGR */
#endif /* DA1640X */

#endif /* SYS_CLOCK_MGR_H_ */

/**
 \}
 \}
 */
