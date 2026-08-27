/**
 ****************************************************************************************
 *
 * @file sys_clock_mgr_da1640x.c
 *
 * @brief Clock Manager
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

/**
 \addtogroup BSP
 \{
 \addtogroup SYSTEM
 \{
 \addtogroup CLOCK_MANAGER
 \{
 */

#include "bsp_api.h"
#include "bsp_definitions.h"

#if (DEVICE_FAMILY == DA1640X)

#if dg_configUSE_CLOCK_MGR

#include "sdk_defs.h"
#include "bsp_otp.h"
#include "sys_clock_mgr.h"
#include "sys_clock_mgr_internal.h"

#include "hw_sys.h"

// TIN_HACK_WIFI
#define BSP_CLOCKS_SOURCE_LP_CLK_RCX          BSP_CLOCKS_SOURCE_CLOCK_RCX

#if (BSP_CFG_LP_CLOCK_SOURCE == BSP_CLOCKS_SOURCE_LP_CLK_RCX)
#include "../sys_man/sys_rcx_calibrate_internal.h"
#endif

#if (dg_configPMU_ADAPTER == 1)
#include "../adapters/src/ad_pmu_internal.h"
#endif

#if (dg_configSYSTEMVIEW)
#include "SEGGER_SYSVIEW_FreeRTOS.h"
#else
#define SEGGER_SYSTEMVIEW_ISR_ENTER()
#define SEGGER_SYSTEMVIEW_ISR_EXIT()
#endif

#ifdef OS_FREERTOS
#include "osal.h"
#include "sdk_list.h"

//#include "sys_tcs.h"

#if (CLK_MGR_USE_TIMING_DEBUG == 1)
#pragma message "Clock manager: GPIO Debugging is on!"
#endif

#ifdef CONFIG_USE_BLE
#include "ad_ble.h"
#endif

#define XTAL40_AVAILABLE                1       // XTAL40M availability
#define LP_CLK_AVAILABLE                2       // LP clock availability
#define PLL_AVAILABLE                   4       // PLL locked
#endif /* OS_FREERTOS */

#define RCX_MIN_HZ                      450
#define RCX_MAX_HZ                      550
/* RCX frequency range varies between 13kHz and 17kHz. RCX_MIN/MAX_TICK_CYCLES correspond to the number of
 * min and max RCX cycles respectively in a 2msec duration, which is the optimum OS tick. */
#define RCX_MIN_TICK_CYCLES             26
#define RCX_MAX_TICK_CYCLES             34

/* ~3 msec for the 1st calibration. This is the maximum allowed value when the 96MHz clock is
 * used. It can be increased when the sys_clk has lower frequency (i.e. multiplied by 2 for 48MHz,
 * 3 for 32MHz). The bigger it is, the longer it takes to complete the power-up
 * sequence. */
#define RCX_CALIBRATION_CYCLES_PUP      44

/* Total calibration time = N*3 msec. Increase N to get a better estimation of the frequency of
 * RCX. */
#define RCX_REPEAT_CALIBRATION_PUP      10

/* Bit field to trigger the RCX Calibration task to start calibration. */
#define RCX_DO_CALIBRATION              1

/* test condition for DA16400(RRQ6100) */
#define TEST_RTC_MR_ACCESS_UNDER_LP32K  (0)     // for VSIM
#define TEST_LP32K_FAST_DETECT          (0)     // for VSIM

#if (dg_configUSE_HW_CLK_DEBUG_GPIO == 1)
#define SUPPORT_PLL_MONITOR             (1)     // for TEST
# define CLK_MGR_DEBUG_GPIO_HIGH()      hw_gpio_set_active(HW_GPIO_PORT_1, HW_GPIO_PIN_11)
# define CLK_MGR_DEBUG_GPIO_LOW()       hw_gpio_set_inactive(HW_GPIO_PORT_1, HW_GPIO_PIN_11)
# define CLK_MGR_ABNORMAL_TOGGLE()      hw_gpio_toggle(HW_GPIO_PORT_1, HW_GPIO_PIN_12)
#else
#define SUPPORT_PLL_MONITOR             (0)     // for TEST
# define CLK_MGR_DEBUG_GPIO_HIGH()
# define CLK_MGR_DEBUG_GPIO_LOW()
# define CLK_MGR_ABNORMAL_TOGGLE()
#endif

/*
 * Global and / or retained variables
 */

__RETAINED uint16_t rcx_clock_hz;
__RETAINED uint8_t rcx_tick_period;                        // # of cycles in 1 tick
__RETAINED uint16_t rcx_tick_rate_hz;
__RETAINED static uint32_t rcx_clock_hz_acc;               // Accurate RCX freq (1/RCX_ACCURACY_LEVEL accuracy)
__RETAINED static uint32_t rcx_clock_period;               // usec multiplied by 1024 * 1024

static const uint64_t rcx_period_dividend = 1048576000000;             // 1024 * 1024 * 1000000;

#if (dg_configRTC_CORRECTION == 1)

/*
 * RTC compensation variables
 */
#define DAY_IN_USEC                     (24 * 60 * 60 * 1000 * 1000LL)
#define HDAY_IN_USEC                    (12 * 60 * 60 * 1000 * 1000LL)
#define HUNDREDTHS_OF_SEC_us            10000

__RETAINED static uint32_t rcx_freq_prev;
__RETAINED static uint64_t rtc_usec_prev;
__RETAINED static int32_t rtc_usec_correction;
__RETAINED static uint32_t initial_rcx_clock_hz_acc;
#endif

__RETAINED_RW static sys_clk_t sysclk = sysclk_LP;      // Invalidate system clock
__RETAINED static ahb_div_t ahbclk;
__RETAINED static apb_div_t apbclk;
__RETAINED static sys_clk_is_t cpuclk;
#if (dg_configPMU_ADAPTER == 0)
//__RETAINED static HW_PMU_1V2_VOLTAGE vdd_voltage;
#endif /* dg_configPMU_ADAPTER */
__RETAINED static uint8_t pll_count;

__RETAINED static void (*xtal_ready_callback)(void);

static sys_clk_t sys_clk_next;
static ahb_div_t ahb_clk_next;
static sys_clk_is_t cpu_clk_next;

static volatile bool xtal40m_settled_notification = false;
static volatile bool xtal40m_settled = false;
static volatile bool pll_locked = false;
static volatile bool dup_call_clk_sleep = false;

#ifdef OS_FREERTOS
__RETAINED static OS_MUTEX xSemaphoreCM;
__RETAINED static OS_EVENT_GROUP xEventGroupCM_xtal;
__RETAINED static OS_TIMER xLPSettleTimer;

#if (BSP_CFG_LP_CLOCK_SOURCE == BSP_CLOCKS_SOURCE_LP_CLK_RCX)
__RETAINED static OS_TASK xRCXCalibTaskHandle;
#endif

typedef struct clk_mgr_task_list_elem_t clk_mgr_task_list_elem_t;
struct clk_mgr_task_list_elem_t {
        clk_mgr_task_list_elem_t *next;
        OS_TASK task;
        uint8_t task_pll_count;
};

__RETAINED static void* clk_mgr_task_list;

#endif /* OS_FREERTOS */

#if (DEVICE_FAMILY == DA1640X)
__RETAINED static cm_clock_callback_t *cevt_cb_list;
__RETAINED static void *cevt_cb_soft_mutex;
__RETAINED static uint32_t cevt_cb_count;
#endif

#define NUM_OF_CPU_CLK_CONF 5

/*
 * Forward declarations
 */
static cm_sys_clk_set_status_t sys_clk_set(sys_clk_t type);
static void apb_set_clock_divider(apb_div_t div);
static bool ahb_set_clock_divider(ahb_div_t div);

#ifdef OS_FREERTOS
#define CM_ENTER_CRITICAL_SECTION() OS_ENTER_CRITICAL_SECTION()
#define CM_LEAVE_CRITICAL_SECTION() OS_LEAVE_CRITICAL_SECTION()

#define CM_EVENT_WAIT() ASSERT_WARNING(xSemaphoreCM != NULL); \
                        OS_EVENT_WAIT(xSemaphoreCM, OS_EVENT_FOREVER)
#define CM_EVENT_SIGNAL() OS_EVENT_SIGNAL(xSemaphoreCM)

#else
#define CM_ENTER_CRITICAL_SECTION() GLOBAL_INT_DISABLE()
#define CM_LEAVE_CRITICAL_SECTION() GLOBAL_INT_RESTORE()

#define CM_EVENT_WAIT()
#define CM_EVENT_SIGNAL()

#endif /* OS_FREERTOS */

/*
 * Function definitions
 */

/**
 * \brief Get the CPU clock frequency in MHz
 *
 * \param[in] clk The system clock
 * \param[in] div The HCLK divider
 *
 * \return The clock frequency
 */
__STATIC_FORCEINLINE uint32_t get_clk_freq(sys_clk_t clk, ahb_div_t div)
{
#if (DEVICE_FAMILY == DA1640X)
        uint32_t clock = (uint32_t)clk;

        if (clock == sysclk_RC32) {
                clock = 1; // 32K @ RRQ61000
        }
        else
        if(clock == sysclk_XTAL40M){
                clock = 40 >> div;
        }
        else
        if(clock == sysclk_PLL480) {
                switch (hw_clk_get_sysclk()) {
#if (HW_DESCOPED_CLOCK == 1)
                case SYS_CLK_IS_PLL240M:
                        clock = 240 >> div;
                        break;
                case SYS_CLK_IS_PLL192M:
                        clock = 192 >> div;
                        break;
#endif
                case SYS_CLK_IS_PLL160M:
                        clock = 160 >> div;
                        break;
                case SYS_CLK_IS_PLL137M:
                        clock = 137 >> div;
                        break;
                case SYS_CLK_IS_PLL106M:
                        clock = 106 >> div;
                        break;
                case SYS_CLK_IS_XTAL40M:
                        clock = 40 >> div;
                        break;
                case SYS_CLK_IS_LP:
                        clock = 1; // 32K @ RRQ61000
                        break;
                default:
                        ASSERT_WARNING(0);
                        clock = 40;
                }
        }

        return clock;
#else

        sys_clk_t clock = clk;

        if (clock == sysclk_RC32) {
                clock = sysclk_XTAL40M;
        }

        return ( 16 >> div ) * clock;
#endif
}

/**
 * \brief Adjust OTP access timings according to the AHB clock frequency.
 *
 * \warning In mirrored mode, the OTP access timings are left unchanged since the system is put to
 *          sleep using the RC32M clock and the AHB divider set to 1, which are the same settings
 *          that the system runs after a power-up or wake-up!
 */
__RETAINED_CODE static void adjust_otp_access_timings(void)
{
#if (dg_configUSE_HW_OTPC == 1)
        uint32_t clk_freq;

        //if (hw_otpc_is_active()) {
                clk_freq = get_clk_freq(sys_clk_next, ahb_clk_next);

                bsp_otp_timings_set(clk_freq);
        //}
#endif
}

#if (DEVICE_FAMILY == DA1640X)
/**
 * \brief Register a calback function to be called when changing the AHB clock
 *
 * \return true if the callback is registered successfully, else false.
 */
bool    cm_register_clock_callback(cm_clock_callback_t *cevtcb)
{
        bool status;

        ASSERT_WARNING(cevtcb != NULL);
        ASSERT_WARNING(cevt_cb_soft_mutex == NULL);

        status = false;

        GLOBAL_INT_DISABLE();

        if(cevt_cb_list != NULL ){
                cm_clock_callback_t *nxt;

                nxt = cevt_cb_list;
                if( nxt->priority > cevtcb->priority ){
                        cevtcb->nxt = nxt;
                        cevt_cb_list = cevtcb ;
                        status = true;
                        cevt_cb_count++;
                }
                else
                {
                do{
                        if( nxt->nxt == NULL ){
                                cevtcb->nxt = NULL;
                                nxt->nxt = cevtcb ;
                                status = true;
                                        cevt_cb_count++;
                                break;
                                }else
                                if( nxt->nxt->priority > cevtcb->priority ){
                                        cevtcb->nxt = nxt->nxt;
                                        nxt->nxt = cevtcb ;
                                        status = true;
                                        cevt_cb_count++;
                                        break;
                        }

                        nxt = nxt->nxt;
                }while(nxt != NULL);
        }
        }
        else
        {
                cevtcb->nxt = NULL;
                cevt_cb_list = cevtcb;
                status = true;
                cevt_cb_count++;
        }

        GLOBAL_INT_RESTORE();

        return status;
}

/**
 * \brief Deregister a calback function to be called when changing the AHB clock
 *
 * \return true if the callback is unregistered successfully, else false.
 */
bool    cm_deregister_clock_callback(cm_clock_callback_t *cevtcb)
{
        bool status;

        ASSERT_WARNING(cevtcb != NULL);
        ASSERT_WARNING(cevt_cb_soft_mutex == NULL);

        status = false;

        GLOBAL_INT_DISABLE();

        if(cevt_cb_list != NULL ){
                if( cevt_cb_list == cevtcb ){
                        cevt_cb_list = cevtcb->nxt;
                        cevtcb->nxt = NULL;
                        status = true;
                        cevt_cb_count--;
                }
                else
                {
                        cm_clock_callback_t *nxt;

                        nxt = cevt_cb_list;

                        do{
                                if( nxt->nxt == cevtcb ){
                                        nxt->nxt = cevtcb->nxt;
                                        cevtcb->nxt = NULL;
                                        status = true;
                                        cevt_cb_count--;
                                        break;
                                }
                                nxt = nxt->nxt;
                        }while(nxt != NULL);
                }
        }

        GLOBAL_INT_RESTORE();

        return status;
}
#endif //(DEVICE_FAMILY == DA1640X)

/**
 * \brief Adjust PERI access timings when changing the AHB clock.
 *
 */
__RETAINED_CODE static void adjust_peri_access_timings_core(bool critical, bool flag)
{
#if (DEVICE_FAMILY == DA1640X)
        uint32_t clk_freq;
        sys_clk_is_t clk_src;

        if( flag == false ){
                clk_freq = 0;
                clk_src = SYS_CLK_IS_INVALID;
        }else{
                int i;
                for(i = 32; i > 0; i--){
                        __ASM volatile ( "nop");
                        __ASM volatile ( "nop");
                        __ASM volatile ( "nop");
                }

                clk_freq = get_clk_freq(sys_clk_next, ahb_clk_next);
                clk_src = hw_clk_get_sysclk();
        }

        if(cevt_cb_list == NULL ){
                return;
        }

        // Lock Soft-Mutex
        cevt_cb_soft_mutex = &cevt_cb_list;

        if( clk_freq == 0 ){
                cm_clock_callback_t *nxt;
                nxt = cevt_cb_list;

                do{
                        if( nxt->func != NULL ){
                                if( ( (critical == true) && (nxt->priority != cm_cb_priority_coupled_hyper_tightly ) ) != true )
                                        nxt->func(clk_src, clk_freq, nxt->param);
                        }
                        nxt = nxt->nxt;
                }while(nxt != NULL);
        }
        else
        {
                uint32_t idx;
                cm_clock_callback_t *nxt;
                cm_clock_callback_t *cblist[cevt_cb_count];

                nxt = cevt_cb_list;

                idx = 0;

                do{
                        cblist[idx] = nxt;
                        nxt = nxt->nxt;
                        idx ++;
                }while(nxt != NULL);


                while(idx > 0){
                        idx--;
                        if( cblist[idx]->func != NULL ){
                                if( ( (critical == true) && (cblist[idx]->priority != cm_cb_priority_coupled_hyper_tightly ) ) != true )
                                        cblist[idx]->func(clk_src, clk_freq, cblist[idx]->param);
                        }
                }
        }

        // Unlock Soft-Mutex
        cevt_cb_soft_mutex = NULL;
#else  //(DEVICE_FAMILY == DA1640X)
        int i;
        for(i = 32; i > 0; i--){
                __ASM volatile ( "nop");
                __ASM volatile ( "nop");
        }
#endif //(DEVICE_FAMILY == DA1640X)
}

__STATIC_INLINE void adjust_peri_access_timings_critical(bool flag)
{
        adjust_peri_access_timings_core(true, flag);
}

__STATIC_INLINE void adjust_peri_access_timings(bool flag)
{
        adjust_peri_access_timings_core(false, flag);
}
/**
 * \brief Lower AHB and APB clocks to the minimum frequency.
 *
 * \warning It can be called only at wake-up.
 */
__STATIC_INLINE void lower_amba_clocks(void)
{
        // Lower the AHB clock (fast --> slow clock switch)
        adjust_peri_access_timings(false);
        hw_clk_set_hclk_div((uint32_t)ahb_div16);
        adjust_otp_access_timings();
        adjust_peri_access_timings(true);
}

/**
 * \brief Restore AHB and APB clocks to the maximum (default) frequency.
 *
 * \warning It can be called only at wake-up.
 */
__STATIC_INLINE void restore_amba_clocks(void)
{
        // Restore the AHB clock (slow --> fast clock switch)
        adjust_peri_access_timings(false);
        adjust_otp_access_timings();
        hw_clk_set_hclk_div(ahbclk);
        adjust_peri_access_timings(true);
}

/**
 * \brief Switch to RC32.
 *
 * \details Set RC32 as the system clock.
 */
static void switch_to_rc32(void)
{
        ASSERT_WARNING(0);

        hw_clk_enable_sysclk(SYS_CLK_IS_LP);

        adjust_peri_access_timings_critical(false);
        // fast --> slow clock switch
        hw_clk_set_sysclk(SYS_CLK_IS_LP);     // Set RC32 as sys_clk

        /*
         * Disable RC32M. RC32M will remain enabled by the hardware as long as it is used
         * as system clock.
         */
        //hw_clk_disable_sysclk(SYS_CLK_IS_LP);

        if (sysclk > sysclk_XTAL40M) {
                adjust_otp_access_timings();     // Adjust OTP timings
        }
        adjust_peri_access_timings_critical(true);
}

/**
 * \brief Switch to XTAL40M.
 *
 * \details Sets the XTAL40M as the system clock.
 *
 * \warning It does not block. It assumes that the caller has made sure that the XTAL40M has
 *          settled.
 */
__STATIC_INLINE /*static*/ void switch_to_xtal40m(void)
{
        if (hw_clk_get_sysclk() != SYS_CLK_IS_XTAL40M) {
                ASSERT_WARNING(hw_clk_is_xtalm_started());

                adjust_peri_access_timings_critical(false);
                hw_clk_set_sysclk(SYS_CLK_IS_XTAL40M);          // Set XTAL40 as sys_clk
                if (sysclk > sysclk_XTAL40M) {                 // slow --> fast clock switch
                        adjust_otp_access_timings();            // Adjust OTP timings
                }
                adjust_peri_access_timings_critical(true);
        }
}

/**
 * \brief Disable PLL
 *
 * \details Restore VDD voltage to 0.9V if required.
 */
__STATIC_INLINE /*static*/ void disable_pll(void)
{
        if (
#if (HW_DESCOPED_CLOCK == 1)
                hw_clk_is_enabled_sysclk(SYS_CLK_IS_PLL240M)
                || hw_clk_is_enabled_sysclk(SYS_CLK_IS_PLL192M)
                ||
#endif
                hw_clk_is_enabled_sysclk(SYS_CLK_IS_PLL160M)
                || hw_clk_is_enabled_sysclk(SYS_CLK_IS_PLL137M)
                || hw_clk_is_enabled_sysclk(SYS_CLK_IS_PLL106M)
                ) {

                //hw_clk_disable_sysclk(SYS_CLK_IS_PLL);

#if (dg_configXTAL_BASED_CPU40M == 0)
                hw_clk_pll_sys_off();
#endif //(dg_configXTAL_BASED_CPU40M == 0)

                // VDD voltage can be lowered since PLL is not the system clock anymore
#if (dg_configPMU_ADAPTER == 1)
                ad_pmu_1v2_force_max_voltage_release();
#else
#if (dg_configUSE_HW_PMU == 1 )
                if (vdd_voltage != HW_PMU_1V2_VOLTAGE_1V2) {
                        HW_PMU_ERROR_CODE error_code;
                        error_code = hw_pmu_1v2_onwakeup_set_voltage(vdd_voltage);
                        ASSERT_WARNING(error_code == HW_PMU_ERROR_NOERROR);
                }
#endif
#endif /* dg_configPMU_ADAPTER */
                pll_locked = false;
                //DBG_SET_LOW(CLK_MGR_USE_TIMING_DEBUG, CLKDBG_PLL_ON);
        }
}

/**
 * \brief Enable PLL
 *
 * \details Changes the VDD voltage to 1.2V if required.
 */
#if  (SUPPORT_PLL_MONITOR == 1)
#define  MONITOR_CLKSRC_RTC   (1)

#if (MONITOR_CLKSRC_RTC == 0)
#include "sys_timer.h"
#define TIMER_FULL_RANGE (LP_CNT_NATIVE_MASK)
static uint32_t oldplllocktime, curplllocktime;
#else
#define TIMER_FULL_RANGE  ((1uLL << 40) - 1uLL)
static uint64_t oldplllocktime, curplllocktime;
#endif

static uint32_t plllocktime, maxplllocktime, minplllocktime = 65536;
static uint32_t plllockiter, maxplllockiter, minplllockiter = 65536;

void  pll_monitor_status(uint32_t *locktime, uint32_t *maxlocktime, uint32_t *minlocktime, uint32_t *lockcnt, uint32_t *maxlockcnt, uint32_t *minlockcnt)
{
        GLOBAL_INT_DISABLE();

        *locktime = plllocktime;
        *maxlocktime = maxplllocktime;
        *minlocktime = minplllocktime;

        *lockcnt = plllockiter;
        *maxlockcnt = maxplllockiter;
        *minlockcnt = minplllockiter;

        GLOBAL_INT_RESTORE();
}
#else
void  pll_monitor_status(uint32_t *locktime, uint32_t *maxlocktime, uint32_t *minlocktime, uint32_t *lockcnt, uint32_t *maxlockcnt, uint32_t *minlockcnt)
{
        (void)(locktime);
        (void)(maxlocktime);
        (void)(minlocktime);
        (void)(lockcnt);
        (void)(maxlockcnt);
        (void)(minlockcnt);
}
#endif

__STATIC_INLINE /*static*/ void enable_pll(void)
{
        if (hw_clk_is_pll_locked()) {
                pll_locked = true;
        }
        else if(
#if (HW_DESCOPED_CLOCK == 1)
                (hw_clk_is_enabled_sysclk(SYS_CLK_IS_PLL240M) == false)
             || (hw_clk_is_enabled_sysclk(SYS_CLK_IS_PLL192M) == false)
             ||
#endif
                (hw_clk_is_enabled_sysclk(SYS_CLK_IS_PLL160M) == false)
             || (hw_clk_is_enabled_sysclk(SYS_CLK_IS_PLL137M) == false)
             || (hw_clk_is_enabled_sysclk(SYS_CLK_IS_PLL106M) == false)
                ){
                ASSERT_WARNING(!pll_locked);

#if (dg_configPMU_ADAPTER == 1)
                ad_pmu_1v2_force_max_voltage_request();
#else
#if (dg_configUSE_HW_PMU == 1 )
                HW_PMU_1V2_RAIL_CONFIG rail_config;
                hw_pmu_get_1v2_active_config(&rail_config);

                // PLL cannot be powered by retention LDO
                ASSERT_WARNING(rail_config.current == HW_PMU_1V2_MAX_LOAD_50 ||
                        rail_config.src_type == HW_PMU_SRC_TYPE_DCDC_HIGH_EFFICIENCY);

                vdd_voltage = rail_config.voltage;
                if (vdd_voltage != HW_PMU_1V2_VOLTAGE_1V2) {
                        // VDD voltage must be set to 1.2V prior to switching clock to PLL
                        HW_PMU_ERROR_CODE error_code;
                        error_code = hw_pmu_1v2_onwakeup_set_voltage(HW_PMU_1V2_VOLTAGE_1V2);
                        ASSERT_WARNING(error_code == HW_PMU_ERROR_NOERROR);
                }
#endif
#endif /* dg_configPMU_ADAPTER */
                // TODO:
#if !(DEVICE_FPGA)

#if  (SUPPORT_PLL_MONITOR == 1)
#if (MONITOR_CLKSRC_RTC == 0)

                oldplllocktime = sys_timer_get_count();
                CLK_MGR_DEBUG_GPIO_HIGH();
                plllockiter = hw_clk_pll_sys_on();
                CLK_MGR_DEBUG_GPIO_LOW();
                curplllocktime = sys_timer_get_count();
#else
                oldplllocktime = R_BSP_SystemRtcCountGet();
                CLK_MGR_DEBUG_GPIO_HIGH();
                plllockiter = hw_clk_pll_sys_on();
                CLK_MGR_DEBUG_GPIO_LOW();
                curplllocktime = R_BSP_SystemRtcCountGet();
#endif

                plllockiter |= (REG_GETF(CRG_TOP, CLK_CTRL_REG, RUNNING_AT_XTAL40M) << 1);

                if (curplllocktime < oldplllocktime){
                        plllockiter |= 1;
                }
                plllocktime = (curplllocktime - oldplllocktime) & TIMER_FULL_RANGE;

                if(plllocktime > 22){
                        CLK_MGR_ABNORMAL_TOGGLE();
                        __ASM volatile ( "nop");
                        __ASM volatile ( "nop");
                        __ASM volatile ( "nop");
                        __ASM volatile ( "nop");
                        __ASM volatile ( "nop");
                        __ASM volatile ( "nop");
                        CLK_MGR_ABNORMAL_TOGGLE();
                }

                if( maxplllocktime < plllocktime ){
                        maxplllocktime = plllocktime;
                }
                if( minplllocktime > plllocktime ){
                        minplllocktime = plllocktime;
                }

                if( maxplllockiter < plllockiter ){
                        maxplllockiter = plllockiter;
                }
                if( minplllockiter > plllockiter ){
                        minplllockiter = plllockiter;
                }

#else   //(SUPPORT_PLL_MONITOR == 0)
                hw_clk_pll_sys_on();
#endif  //(SUPPORT_PLL_MONITOR == 1)

#endif  //!(DEVICE_FPGA)
                hw_clk_enable_sysclk(SYS_CLK_IS_PLL160M);           // Turn on PLL
                //DBG_SET_HIGH(CLK_MGR_USE_TIMING_DEBUG, CLKDBG_PLL_ON);
                pll_locked = true;
        }

#if (0)
        // NOTICE (231109) !! Do not change the cpu clock at this point !!!
        if (!REG_GETF(CRG_TOP, CLK_CTRL_REG, PLL_CPU_ENABLE)) {
                REG_SETF(CRG_TOP, CLK_CTRL_REG, PLL_CPU_ENABLE, 1);
        }
#endif
}

/**
 * \brief Switch to PLL.
 *
 * \details Waits until the PLL has locked and sets it as the system clock.
 */
__STATIC_INLINE /*static*/ void switch_to_pll(void)
{
        if (hw_clk_get_sysclk() == SYS_CLK_IS_XTAL40M) {
                // Slow --> fast clock switch
                adjust_otp_access_timings();                         // Adjust OTP timings
                adjust_peri_access_timings(false);

                /*
                 * If ultra-fast wake-up mode is used, make sure that the startup state
                 * machine is finished and all power regulation is in order.
                 */
                while (REG_GETF(CRG_TOP, SYS_STATUS_REG, SYS_IS_UP) == 0);

                /*
                 * Wait for LDO to be OK. Core voltage may have been changed from 0.9V to
                 * 1.2V in order to switch system clock to PLL
                 */
#if (0)
                while ((REG_GETF(CRG_TOP, ANA_STATUS_REG, LDO_CORE_OK) == 0) &&
                       (REG_GETF(DCDC, DCDC_STATUS1_REG, DCDC_VDD_AVAILABLE) == 0));
#endif
                // TODO:
                hw_clk_set_sysclk(SYS_CLK_IS_PLL160M);                   // Set PLL as sys_clk

                adjust_otp_access_timings();                         // Adjust OTP timings
                adjust_peri_access_timings(true);
        }
}

#ifdef OS_FREERTOS

/**
 * \brief The handler of the XTAL32K LP settling timer.
 */
static void vLPTimerCallback(OS_TIMER pxTimer)
{
        static uint32_t count = 0;
        (void) pxTimer;
        OS_ENTER_CRITICAL_SECTION();                            // Critical section

        if (FSP_SUCCESS == bsp_prv_lpclk_select(LP_CLK_IS_XTAL32K))
        {
                /* Enable RTC freerunning counter mirror */
                bsp_prv_rtc_mirror_init();

#ifdef CONFIG_USE_BLE
                // Inform ble adapter about the availability of the LP clock.
                ad_ble_lpclock_available();
#endif
                OS_LEAVE_CRITICAL_SECTION();                    // Exit critical section

                // Inform (blocked) Tasks about the availability of the LP clock.
                OS_EVENT_GROUP_SET_BITS(xEventGroupCM_xtal, LP_CLK_AVAILABLE);

                // Stop the Timer.
                OS_TIMER_STOP(xLPSettleTimer, 0);
                count = 0;
                return;
        }

        OS_LEAVE_CRITICAL_SECTION();

        if (++count > 2)
        {
                OS_ENTER_CRITICAL_SECTION();
                if (FSP_SUCCESS == bsp_prv_lpclk_select(LP_CLK_IS_RCX))
                {
                        /* Enable RTC freerunning counter mirror */
                        bsp_prv_rtc_mirror_init();

#ifdef CONFIG_USE_BLE
                        // Inform ble adapter about the availability of the LP clock.
                        ad_ble_lpclock_available();
#endif
                        OS_LEAVE_CRITICAL_SECTION();

                        // Inform (blocked) Tasks about the availability of the LP clock.
                        OS_EVENT_GROUP_SET_BITS(xEventGroupCM_xtal, LP_CLK_AVAILABLE);
                }
                else
                {
                        OS_LEAVE_CRITICAL_SECTION();
                }

                // Stop the Timer.
                OS_TIMER_STOP(xLPSettleTimer, 0);
                count = 0;
                return;
        }

        OS_TIMER_RESET(xLPSettleTimer, 0);
}

/**
 * \brief Handle the indication that the XTAL32M has settled.
 *
 */
static OS_BASE_TYPE xtal40m_is_ready(BaseType_t *xHigherPriorityTaskWoken)
{
        OS_BASE_TYPE xResult = OS_FAIL;

        if (xtal40m_settled_notification == false) {
                // Do not send the notification twice
                xtal40m_settled_notification = true;

                //DBG_SET_HIGH(CLK_MGR_USE_TIMING_DEBUG, CLKDBG_XTAL40M_SETTLED);
                if (xtal_ready_callback) {
                        xtal_ready_callback();
                }

                if (xEventGroupCM_xtal != NULL) {
                        // Inform blocked Tasks
                        *xHigherPriorityTaskWoken = pdFALSE;            // Must be initialized to pdFALSE

                        xResult = xEventGroupSetBitsFromISR(xEventGroupCM_xtal, XTAL40_AVAILABLE,
                                                            xHigherPriorityTaskWoken);
                }

                //DBG_SET_LOW(CLK_MGR_USE_TIMING_DEBUG, CLKDBG_XTAL40M_SETTLED);
        }
        return xResult;
}

/**
 * \brief Handle the indication that the PLL is locked and therefore available.
 */
static OS_BASE_TYPE pll_is_locked(BaseType_t *xHigherPriorityTaskWoken)
{
        OS_BASE_TYPE xResult = OS_FAIL;

        if (xEventGroupCM_xtal != NULL) {
                *xHigherPriorityTaskWoken = pdFALSE;            // Must be initialized to pdFALSE

                xResult = xEventGroupSetBitsFromISR(xEventGroupCM_xtal, PLL_AVAILABLE,
                                                    xHigherPriorityTaskWoken);
        }

        return xResult;
}

#endif /* OS_FREERTOS */

/**
 * \brief Calculates the optimum tick rate and the number of LP cycles (RCX) per tick.
 *
 * \param[in] freq The RCX clock frequency (in Hz).
 * \param[out] tick_period The number of LP cycles per tick.
 *
 * \return uint32_t The optimum tick rate.
 */
static uint32_t get_optimum_tick_rate(uint16_t freq, uint8_t *tick_period)
{
        uint32_t optimum_rate = 0;
        uint32_t hz;
        int tick;
        int err = 65536;
        int res;

        for (tick = RCX_MIN_TICK_CYCLES; tick < RCX_MAX_TICK_CYCLES; tick++) {
                hz = (uint32_t) (2 * freq / tick);
                hz = (hz & 1) ? hz / 2 + 1 : hz / 2;

                if ((hz >= RCX_MIN_HZ) && (hz <= RCX_MAX_HZ)) {
                        res = (int) ((int) hz * tick * 65536 / freq);
                        res -= 65536;
                        if (res < 0) {
                                res *= -1;
                        }
                        if (res < err) {
                                err = res;
                                optimum_rate = hz;
                                *tick_period = (uint8_t) tick;
                        }
                }
        }

        return optimum_rate;
}

__RETAINED_CODE void cm_enable_xtalm_if_required(void)
{
        if (sysclk != sysclk_RC32) {
                cm_enable_xtalm();
        }
}

uint32_t cm_get_xtalm_settling_lpcycles(void)
{
        if (sysclk == sysclk_RC32) {
                return 0;
        }

#if (BSP_CFG_LP_CLOCK_SOURCE == BSP_CLOCKS_SOURCE_LP_CLK_RCX)
        return (hw_clk_get_xtalm_settling_time() * rcx_clock_hz);
#else
        return (hw_clk_get_xtalm_settling_time() * dg_configXTAL32K_FREQ);
#endif
}

__RETAINED_CODE void cm_enable_xtalm(void)
{
        GLOBAL_INT_DISABLE();

        xtal40m_settled = hw_clk_is_xtalm_started();

        if (xtal40m_settled == false) {
                if (hw_clk_is_enabled_sysclk(SYS_CLK_IS_XTAL40M) == false) {
                        // PDC is not used. Enable XTAL40M by setting XTAL40M_XTAL_ENABLE bit
                        // in XTAL32M_CTRL1_REG
                        hw_clk_enable_sysclk(SYS_CLK_IS_XTAL40M);
                }
        }

        GLOBAL_INT_RESTORE();
}

#if 0
/* Called when CMAC sends a system message to the mailbox */
void sys_proc_handler(uint32_t status)
{
}
#endif

void cm_clk_init_low_level_internal(void)
{
#if (DEVICE_FAMILY == DA1640X)
        REG_SETF(CRG_TOP, CLK_AMBA_REG, PERI_CLK_ENABLE, 1);
        REG_SETF(CRG_TOP, CLK_AMBA_REG, TIMER_CLK_ENABLE, 1);
#elif (DEVICE_FAMILY == DA1487X)
#else
        NVIC_ClearPendingIRQ(XTAL32M_RDY_IRQn);
        NVIC_EnableIRQ(XTAL32M_RDY_IRQn);                      // Activate XTAL32 Ready IRQ

        NVIC_ClearPendingIRQ(PLL_LOCK_IRQn);
        NVIC_EnableIRQ(PLL_LOCK_IRQn);                         // Activate PLL Lock IRQ

        /*
         * Low power clock
         */
        hw_clk_enable_lpclk(LP_CLK_IS_RC32K);
        hw_clk_set_lpclk(LP_CLK_IS_RC32K);

        ASSERT_WARNING(REG_GETF(CRG_TOP, SYS_STAT_REG, TIM_IS_UP));

        hw_clk_xtalm_configure();

        if (dg_configLP_CLK_SOURCE == LP_CLK_IS_DIGITAL) {
                /* Store PD COM state and restore it after configuring P0_23 */
                bool com_is_up = hw_pd_check_com_status();
                if (!com_is_up) {
                        hw_pd_power_up_com();
                }
                hw_clk_configure_ext32k_pins();                 // Configure Ext32K pins
                hw_gpio_pad_latch_enable(HW_GPIO_PORT_0,HW_GPIO_PIN_23);
                hw_gpio_pad_latch_disable(HW_GPIO_PORT_0,HW_GPIO_PIN_23);
                if (!com_is_up) {
                        hw_pd_power_down_com();
                }
                hw_clk_disable_lpclk(LP_CLK_IS_XTAL32K);        // Disable XTAL32K
                hw_clk_disable_lpclk(LP_CLK_IS_RCX);            // Disable RCX
                hw_clk_set_lpclk(LP_CLK_IS_EXTERNAL);           // Set EXTERNAL as the LP clock
        } else if (BSP_CFG_LP_CLOCK_SOURCE == BSP_CLOCKS_SOURCE_LP_CLK_RCX) {
                hw_clk_enable_lpclk(LP_CLK_IS_RCX);             // Enable RCX
                hw_clk_disable_lpclk(LP_CLK_IS_XTAL32K);        // Disable XTAL32K
                // LP clock will be switched to RCX after RCX calibration
        } else {
                // No need to configure XTAL32K pins. Pins are automatically configured
                // when LP_CLK_IS_XTAL32K is enabled.
                hw_clk_configure_lpclk(LP_CLK_IS_XTAL32K);      // Configure XTAL32K
                hw_clk_enable_lpclk(LP_CLK_IS_XTAL32K);         // Enable XTAL32K
                hw_clk_disable_lpclk(LP_CLK_IS_RCX);            // Disable RCX
                // LP clock cannot be set to XTAL32K here. XTAL32K needs a few seconds to settle after power up.
        }

#endif
}

void cm_rcx_calibrate(void)
{
#if     (DEVICE_FAMILY == DA1640X)
        uint32_t hz_value = dg_configXTAL32K_FREQ;

        rcx_clock_hz_acc = (hz_value + (RCX_REPEAT_CALIBRATION_PUP / 2)) / RCX_REPEAT_CALIBRATION_PUP;
        rcx_clock_hz = (uint16_t) (rcx_clock_hz_acc / RCX_ACCURACY_LEVEL);
        rcx_clock_period = (uint32_t)((rcx_period_dividend * RCX_ACCURACY_LEVEL) / rcx_clock_hz_acc);
        rcx_tick_rate_hz = (uint16_t) get_optimum_tick_rate(rcx_clock_hz, &rcx_tick_period);
#if (dg_configRTC_CORRECTION == 1)
        rcx_freq_prev = rcx_clock_hz_acc;
        initial_rcx_clock_hz_acc = rcx_clock_hz_acc;
#endif

#else
        // Run a dummy calibration to make sure the clock has settled
        hw_clk_start_calibration(CALIBRATE_RCX, CALIBRATE_REF_DIVN, 25);
        hw_clk_get_calibration_data();

        // Run actual calibration
        uint32_t hz_value = 0;
        uint32_t cal_value;
        uint64_t max_clk_count;

        for (int i = 0; i < RCX_REPEAT_CALIBRATION_PUP; i++) {
                hw_clk_start_calibration(CALIBRATE_RCX, CALIBRATE_REF_DIVN, RCX_CALIBRATION_CYCLES_PUP);
                cal_value = hw_clk_get_calibration_data();

                // Process calibration results
                max_clk_count = (uint64_t)dg_configXTAL32M_FREQ * RCX_CALIBRATION_CYCLES_PUP * RCX_ACCURACY_LEVEL;
                hz_value += (uint32_t)(max_clk_count / cal_value);
        }

        rcx_clock_hz_acc = (hz_value + (RCX_REPEAT_CALIBRATION_PUP / 2)) / RCX_REPEAT_CALIBRATION_PUP;
        rcx_clock_hz = rcx_clock_hz_acc / RCX_ACCURACY_LEVEL;
        rcx_clock_period = (uint32_t)((rcx_period_dividend * RCX_ACCURACY_LEVEL) / rcx_clock_hz_acc);
        rcx_tick_rate_hz = get_optimum_tick_rate(rcx_clock_hz, &rcx_tick_period);
#if (dg_configRTC_CORRECTION == 1)
        rcx_freq_prev = rcx_clock_hz_acc;
        initial_rcx_clock_hz_acc = rcx_clock_hz_acc;
#endif
#endif
}

uint32_t cm_get_rcx_clock_hz_acc(void)
{
        return rcx_clock_hz_acc;
}

uint32_t cm_get_rcx_clock_period(void)
{
        return rcx_clock_period;
}

void cm_sys_clk_init(sys_clk_t type)
{
#ifdef OS_FREERTOS
        ASSERT_WARNING(xSemaphoreCM == NULL);               // Called only once!

        xSemaphoreCM = xSemaphoreCreateMutex();             // Create Mutex
        ASSERT_WARNING(xSemaphoreCM != NULL);

        xEventGroupCM_xtal = OS_EVENT_GROUP_CREATE();       // Create Event Group
        ASSERT_WARNING(xEventGroupCM_xtal != NULL);
#endif
        ahbclk = cm_ahb_get_clock_divider();
        apbclk = cm_apb_get_clock_divider();
        cpuclk = hw_clk_get_sysclk();

        sys_clk_next = type;
        ahb_clk_next = ahbclk;
        cpu_clk_next = cpuclk;

        ASSERT_WARNING(type != sysclk_LP);                  // Not Applicable!

        /*
         * Disable RC32M. RC32M will remain enabled by the hardware as long as it is used
         * as system clock.
         */
#if DEVICE_FPGA
#else
        hw_clk_disable_sysclk(SYS_CLK_IS_LP);
#endif

        CM_ENTER_CRITICAL_SECTION();
        if (sys_clk_next == sysclk_LP) {
                if (hw_clk_get_sysclk() != SYS_CLK_IS_LP) {
                        // RC32 is not the System clock
                        switch_to_rc32();
                }
        }
        else {
                cm_enable_xtalm();

                /*
                 * Note: In case that the LP clock is the XTAL32K then we
                 *       simply set the cm_sysclk to the user setting and skip waiting for the
                 *       XTAL32M to settle. In this case, the system clock will be set to the
                 *       XTAL32M (or the PLL) when the XTAL32M_RDY_IRQn hits. Every task or Adapter
                 *       must block until the requested system clock is available. Sleep may have to
                 *       be blocked as well.
                 */
                if (cm_poll_xtalm_ready()) {
                        switch_to_xtal40m();
#if DEVICE_FPGA
#else
                        hw_clk_disable_sysclk(SYS_CLK_IS_LP);
#endif
                        if (sys_clk_next == sysclk_PLL480) {
                                if (hw_clk_is_pll_locked() == false) {
                                        // System clock will be switched to PLL when PLL is locked
                                        enable_pll();
                                }
                                switch_to_pll();
                        }
                        else {
                                disable_pll();
#ifdef OS_FREERTOS
                                OS_EVENT_GROUP_CLEAR_BITS(xEventGroupCM_xtal, PLL_AVAILABLE);
#endif
                        }
                }
        }

        sysclk = sys_clk_next;
        cpuclk = cpu_clk_next;

        CM_EVENT_WAIT();
        pll_count = (sys_clk_next == sysclk_PLL480) ? 1 : 0;
        CM_EVENT_SIGNAL();

        CM_LEAVE_CRITICAL_SECTION();

#ifdef OS_FREERTOS
        /* try to select 32k-crystal as periodical*/
        cm_lp_clk_init();
#endif
}

__STATIC_FORCEINLINE void cm_sys_enable_xtalm(sys_clk_t type)
{
        if (type >= sysclk_XTAL40M) {
                cm_enable_xtalm();

                // Make sure the XTAL32M has settled
                cm_wait_xtalm_ready();
        }
}

__STATIC_FORCEINLINE void sys_enable_pll(void)
{
        enable_pll();
        cm_wait_pll_lock();
}

#ifdef OS_FREERTOS
bool sys_clk_mgr_match_task(const void *elem, const void *ud)
{
        return ((clk_mgr_task_list_elem_t*)elem)->task == ud;
}
#endif

cm_sys_clk_set_status_t cm_sys_clk_set(sys_clk_t type)
{
        cm_sys_clk_set_status_t ret;

        ASSERT_WARNING(type != sysclk_LP);                      // Not Applicable!

#if DEVICE_FPGA
        // PLL is only allowed in 16/48MHz FPGA
        ASSERT_ERROR((dg_configPLL480M_FREQ  == 48000000) && (dg_configXTAL40M_FREQ  == 16000000));
#endif

        if (type == sysclk_PLL480 && cm_ahb_get_clock_divider() != ahb_div1) {
                // PLL can be used only when AHB divider is ahb_div1
                return cm_sysclk_ahb_divider_in_use;
        }

#ifdef OS_FREERTOS
        clk_mgr_task_list_elem_t *elem;
        OS_TASK task = OS_GET_CURRENT_TASK();
#endif

        // Check if system clock can be switched
        if (type != sysclk_PLL480) {
                CM_EVENT_WAIT();
                if (pll_count > 1) {
#ifdef OS_FREERTOS
                        /* Check if the current task is in the list */
                        elem = list_find(&clk_mgr_task_list, sys_clk_mgr_match_task, task);
                        if (elem) {
                                elem->task_pll_count--;
                                if (elem->task_pll_count < 1) {
                                        /* Remove the task and decrease global pll_count */
                                        list_unlink(&clk_mgr_task_list, sys_clk_mgr_match_task,
                                                    task);
                                        OS_FREE(elem);
                                        pll_count--;
                                }
                        }
#else
                        pll_count--;
#endif
                        CM_EVENT_SIGNAL();
                        return cm_sysclk_pll_used_by_task;
                }
#ifdef OS_FREERTOS
                if (pll_count == 1) {
                        if (list_find(&clk_mgr_task_list, sys_clk_mgr_match_task, task) == NULL) {
                                // If this is not the task that has requested PLL
                                CM_EVENT_SIGNAL();
                                return cm_sysclk_pll_used_by_task;
                        }

                }
#endif
                CM_EVENT_SIGNAL();
        }

        cm_sys_enable_xtalm(type);

        if (type == sysclk_PLL480) {
                sys_enable_pll();
                /*switch_to_pll();*/
        }

        CM_EVENT_WAIT();
        if (type == sysclk_RC32 && sysclk != sysclk_RC32) {
                // If RC32 clock is requested, then switch to XTAL32 since switching to RC32 is not allowed.
                // RC32 will be used as system clock the next time the CPU wakes-up.
                ret = sys_clk_set(sysclk_XTAL40M);
                if (ret == cm_sysclk_success) {
                        sysclk = sysclk_RC32;
                }
        }
        else {
                ret = sys_clk_set(type);
        }
        CM_EVENT_SIGNAL();

        if (ret == cm_sysclk_success) {
                CM_EVENT_WAIT();
                if (type == sysclk_PLL480) {
#ifdef OS_FREERTOS
                        elem = list_find(&clk_mgr_task_list, sys_clk_mgr_match_task, task);
                        if (elem == NULL) {
                                // Add the current task in the list
                                elem = OS_MALLOC(sizeof(clk_mgr_task_list_elem_t));
                                OS_ASSERT(elem);
                                elem->task = task;
                                elem->task_pll_count = 1;
                                list_add(&clk_mgr_task_list, elem);
                                pll_count++;
                        } else {
                                elem->task_pll_count++;
                        }
#else
                        pll_count++;
#endif
                }
                else if (pll_count > 0) {
                        ASSERT_WARNING(pll_count == 1);
#ifdef OS_FREERTOS
                        /* The current task must be is in the list. */
                        elem = list_find(&clk_mgr_task_list, sys_clk_mgr_match_task, task);
                        OS_ASSERT(elem);
                        if (elem->task_pll_count > 1) {
                                elem->task_pll_count--;
                        } else {
                                /* Remove the task element and decrease global pll counter */
                                ASSERT_WARNING(elem->task_pll_count == 1);
                                elem = list_unlink(&clk_mgr_task_list, sys_clk_mgr_match_task,
                                                   task);
                                OS_FREE(elem);
                                pll_count--;
                        }
#else
                        pll_count--;
#endif
                }

                CM_EVENT_SIGNAL();
        }

        if (sysclk != sysclk_PLL480) {
                disable_pll();
#ifdef OS_FREERTOS
                OS_EVENT_GROUP_CLEAR_BITS(xEventGroupCM_xtal, PLL_AVAILABLE);
#endif
        }

        return ret;
}

#define CHECK_PER_DIV1_CLK(val, per) ((val & REG_MSK(CRG_COM, CLK_COM_REG, per ## _ENABLE)) && \
                                      (val & REG_MSK(CRG_COM, CLK_COM_REG, per ## _CLK_SEL)))

/**
 * \brief Check if div1 clock is used by a peripheral
 *
 * \return true if div1 is used by a peripheral
 */
static bool sys_clk_check_div1(void)
{
#if (DEVICE_FAMILY == DA1640X)
        // TODO:
        return false;
#else
        uint32_t tmp;

        // Check if SysTick is ON and if it is affected
        if (dg_configABORT_IF_SYSTICK_CLK_ERR) {
                if (SysTick->CTRL & SysTick_CTRL_ENABLE_Msk) {
                        return true;
                }
        }

        // Check if peripherals are clocked by DIV1 clock

        if (hw_pd_check_com_status()) {
                tmp = CRG_COM->CLK_COM_REG;

                // Check SPI clock
                if (CHECK_PER_DIV1_CLK(tmp, SPI)) {
                        return true;
                }

                // Check SPI2 clock
                if (CHECK_PER_DIV1_CLK(tmp, SPI2)) {
                        return true;
                }

                // Check I2C clock
                if (CHECK_PER_DIV1_CLK(tmp, I2C)) {
                        return true;
                }

                // Check I2C2 clock
                if (CHECK_PER_DIV1_CLK(tmp, I2C2)) {
                        return true;
                }

                // Check UART2 clock
                if (CHECK_PER_DIV1_CLK(tmp, UART2)) {
                        return true;
                }

                // Check UART3 clock
                if (CHECK_PER_DIV1_CLK(tmp, UART3)) {
                        return true;
                }
        }

        if (hw_pd_check_periph_status()) {

                // Check GPADC
                if (REG_GETF(GPADC, GP_ADC_CTRL_REG, GP_ADC_EN) && REG_GETF(CRG_PER, CLK_PER_REG, GPADC_CLK_SEL)) {
                        return true;
                }

                // Check PCM clock
                tmp = CRG_PER->PCM_DIV_REG;
                if ((tmp & REG_MSK(CRG_PER, PCM_DIV_REG, CLK_PCM_EN)) &&
                                (tmp & REG_MSK(CRG_PER, PCM_DIV_REG, PCM_SRC_SEL))) {
                        return true;
                }
        }

#if (dg_configUSE_HW_USB == 1)
        // Check USB controller
        if (hw_usb_active()) {
                // Return true only if the PLL is enabled and the USB block
                // is using it
                if ( REG_GETF(CRG_TOP, CLK_CTRL_REG, SYS_CLK_SEL) == 3 &&
                     REG_GETF(CRG_TOP, CLK_CTRL_REG, USB_CLK_SRC) == 0 ){
                        return true;
                }
        }
#endif

#if (dg_configUSE_HW_LCDC == 1)
        // Check LCD controller
        if (hw_lcdc_clk_is_div1()) {
                return true;
        }
#endif
        if (hw_pd_check_tim_status()) {
                // Check CMAC
                tmp = CRG_TOP->CLK_RADIO_REG;
                if ((tmp & REG_MSK(CRG_TOP, CLK_RADIO_REG, CMAC_CLK_ENABLE)) &&
                                 (tmp & REG_MSK(CRG_TOP, CLK_RADIO_REG, CMAC_CLK_SEL))) {
                        return true;
                }
        }

        return false;
#endif
}

__RETAINED_CODE cm_sys_clk_set_status_t sys_clk_set(sys_clk_t type)
{
        cm_sys_clk_set_status_t ret;

        CM_ENTER_CRITICAL_SECTION();

        if (type != sysclk && sys_clk_check_div1()) {
                ret = cm_sysclk_div1_clk_in_use;
        }
        else {
                ret = cm_sysclk_success;

                if (type != sysclk) {
                        sys_clk_next = type;
                        ahb_clk_next = ahbclk;

                        switch (sys_clk_next) {
                        case sysclk_PLL480:
                                if (sysclk == sysclk_RC32) {
                                        // Transition from RC32M to PLL is not allowed.
                                        // Switch to XTAL32M first.
                                        switch_to_xtal40m();
                                }
                                switch_to_pll();
                                break;
                        case sysclk_RC32:
                                if (sysclk == sysclk_PLL480) {
                                        // Transition from PLL to RC32 is not allowed.
                                        // Switch to XTAL32M first.
                                        switch_to_xtal40m();
                                }
                                switch_to_rc32();
                                break;
                        case sysclk_XTAL40M:
                                switch_to_xtal40m();
                                break;
                        default:
                                ASSERT_WARNING(0);
                                break;
                        }
                        sysclk = sys_clk_next;
                }
        }

        CM_LEAVE_CRITICAL_SECTION();

        return ret;
}

void cm_apb_set_clock_divider(apb_div_t div)
{
        CM_EVENT_WAIT();
        apb_set_clock_divider(div);
        CM_EVENT_SIGNAL();
}

static void apb_set_clock_divider(apb_div_t div)
{
#if 0
        hw_clk_set_pclk_div(div);
        apbclk = div;
#else
        apbclk = div;
#endif
}

bool cm_ahb_set_clock_divider(ahb_div_t div)
{
        CM_EVENT_WAIT();
        bool ret = ahb_set_clock_divider(div);
        CM_EVENT_SIGNAL();

        return ret;
}

__RETAINED_CODE bool ahb_set_clock_divider(ahb_div_t div)
{
        bool ret = true;

        CM_ENTER_CRITICAL_SECTION();

        do {
                if (ahbclk == div) {
                        break;
                }

                // Check if SysTick is ON and if it is affected
                if (dg_configABORT_IF_SYSTICK_CLK_ERR) {
                        if (SysTick->CTRL & SysTick_CTRL_ENABLE_Msk) {
                                ret = false;
                                break;
                        }
                }

                ahb_clk_next = div;

                if (ahbclk < div) {
                        // fast --> slow clock switch
                        //TODO: adjust_peri_access_timings(false);
                        hw_clk_set_hclk_div(div);
                        adjust_otp_access_timings();         // Adjust OTP timings
                        //TODO: adjust_peri_access_timings(true);
                } else {
                        // slow --> fast clock switch
                        adjust_otp_access_timings();         // Adjust OTP timings
                        //TODO: adjust_peri_access_timings(false);
                        hw_clk_set_hclk_div(div);
                        //TODO: adjust_peri_access_timings(true);
                }

                ahbclk = div;

        } while (0);

        CM_LEAVE_CRITICAL_SECTION();

        return ret;
}

__RETAINED_CODE bool cm_cpu_clk_set(cpu_clk_t clk)
{
        sys_clk_t new_sysclk;
        sys_clk_t old_sysclk = sysclk;
        ahb_div_t new_ahbclk = ahb_div1;
        sys_clk_is_t    new_cpuclk = cpuclk;
        bool ret = false;

        switch (clk) {
#if (HW_DESCOPED_CLOCK == 1)
        case cpuclk_240M: // descoped
                new_sysclk = sysclk_PLL480;
                new_ahbclk = ahb_div2;
                new_cpuclk = SYS_CLK_IS_PLL240M;
                break;
        case cpuclk_192M: // descoped
                new_sysclk = sysclk_PLL480;
                new_ahbclk = ahb_div2;
                new_cpuclk = SYS_CLK_IS_PLL192M;
                break;
#endif
        case cpuclk_160M:
                new_sysclk = sysclk_PLL480;
                new_ahbclk = ahb_div1;
                new_cpuclk = SYS_CLK_IS_PLL160M;
                break;
        case cpuclk_137M:
                new_sysclk = sysclk_PLL480;
                new_ahbclk = ahb_div1;
                new_cpuclk = SYS_CLK_IS_PLL137M;
                break;
        case cpuclk_106M:
                new_sysclk = sysclk_PLL480;
                new_ahbclk = ahb_div1;
                new_cpuclk = SYS_CLK_IS_PLL106M;
                break;

        case cpuclk_80M:
                new_sysclk = sysclk_PLL480;
                new_ahbclk = ahb_div2;
                new_cpuclk = SYS_CLK_IS_PLL160M;
                break;
        case cpuclk_68M:
                new_sysclk = sysclk_PLL480;
                new_ahbclk = ahb_div2;
                new_cpuclk = SYS_CLK_IS_PLL137M;
                break;
        case cpuclk_53M:
                new_sysclk = sysclk_PLL480;
                new_ahbclk = ahb_div2;
                new_cpuclk = SYS_CLK_IS_PLL106M;
                break;
#if (dg_configXTAL_BASED_CPU40M == 0)
        case cpuclk_40M:
                new_sysclk = sysclk_PLL480;
                new_ahbclk = ahb_div4;
                new_cpuclk = SYS_CLK_IS_PLL160M;
                break;
        case cpuclk_34M:
                new_sysclk = sysclk_PLL480;
                new_ahbclk = ahb_div4;
                new_cpuclk = SYS_CLK_IS_PLL137M;
                break;
        case cpuclk_26M:
                new_sysclk = sysclk_PLL480;
                new_ahbclk = ahb_div4;
                new_cpuclk = SYS_CLK_IS_PLL106M;
                break;
#else   //(dg_configXTAL_BASED_CPU40M == 1)
        case cpuclk_34M:
        case cpuclk_26M:
                ASSERT_WARNING(0);
                return false;
        case cpuclk_40M:
#endif  //(dg_configXTAL_BASED_CPU40M == 0)
        case cpuclk_20M:
        case cpuclk_10M:
        case cpuclk_5M:
        case cpuclk_2M:
                if (pll_count > 1) { // ORG: (pll_count > 0)
                        return false;
                }
                new_sysclk = (sysclk == sysclk_RC32) ? sysclk_RC32 : sysclk_XTAL40M;
                new_ahbclk = (ahb_div_t)(__CLZ((uint32_t)clk) - 26);
                new_cpuclk = (sysclk == sysclk_RC32) ? SYS_CLK_IS_LP : SYS_CLK_IS_XTAL40M;

                break;
        default:
                return false;
        }


        cm_sys_enable_xtalm(new_sysclk);

        if (new_sysclk == sysclk_PLL480) {
                sys_enable_pll();
        }

        CM_EVENT_WAIT();

        adjust_peri_access_timings(false);

        if (sys_clk_set(new_sysclk) == cm_sysclk_success) {
                ret = ahb_set_clock_divider(new_ahbclk);

                if (ret == false) {
                        ASSERT_WARNING(old_sysclk != sysclk_LP);   // Not Applicable!
                        cm_sys_enable_xtalm(old_sysclk);
                        sys_clk_set(old_sysclk);                   // Restore previous setting
                }

                hw_clk_set_sysclk(new_cpuclk);
                cpuclk = new_cpuclk;
        }

        adjust_peri_access_timings(true);

        CM_EVENT_SIGNAL();

        if (sysclk != sysclk_PLL480) {
                disable_pll();
#ifdef OS_FREERTOS
                OS_EVENT_GROUP_CLEAR_BITS(xEventGroupCM_xtal, PLL_AVAILABLE);
#endif
        }

        return ret;
}

void cm_cpu_clk_set_fromISR(sys_clk_t clk, ahb_div_t hdiv)
{
        ASSERT_WARNING(clk != sysclk_LP);               // Not Applicable!
        ASSERT_WARNING(clk != sysclk_RC32);             // Not supported!

        sysclk = clk;
        ahbclk = hdiv;
        cm_sys_clk_sleep(false, false);                        // Pretend an XTAL32M settled event
}

sys_clk_t cm_sys_clk_get(void)
{
        sys_clk_t clk;

        CM_EVENT_WAIT();
        CM_ENTER_CRITICAL_SECTION();

        clk = cm_sys_clk_get_fromISR();

        CM_LEAVE_CRITICAL_SECTION();
        CM_EVENT_SIGNAL();

        return clk;
}

sys_clk_t cm_sys_clk_get_fromISR(void)
{
        switch (hw_clk_get_sysclk()) {
        case SYS_CLK_IS_LP:
                return sysclk_RC32;

        case SYS_CLK_IS_XTAL40M:
                return sysclk_XTAL40M;
#if (HW_DESCOPED_CLOCK == 1)
        case SYS_CLK_IS_PLL240M:
        case SYS_CLK_IS_PLL192M:
#endif
        case SYS_CLK_IS_PLL160M:
        case SYS_CLK_IS_PLL137M:
        case SYS_CLK_IS_PLL106M:
                return sysclk_PLL480;
        default:
                ASSERT_WARNING(0);
                return sysclk_RC32;
        }
}

apb_div_t cm_apb_get_clock_divider(void)
{
        CM_EVENT_WAIT();
        //apb_div_t clk = (apb_div_t)hw_clk_get_pclk_div();
        apb_div_t clk = apbclk;
        CM_EVENT_SIGNAL();

        return clk;
}

ahb_div_t cm_ahb_get_clock_divider(void)
{
        ahb_div_t clk;

        CM_EVENT_WAIT();
        CM_ENTER_CRITICAL_SECTION();                            // Critical section

        clk = (ahb_div_t)hw_clk_get_hclk_div();

        CM_LEAVE_CRITICAL_SECTION();                            // Exit critical section
        CM_EVENT_SIGNAL();
        return clk;
}

cpu_clk_t cm_cpu_clk_get(void)
{
        sys_clk_t curr_sysclk = cm_sys_clk_get();
        ahb_div_t curr_ahbclk = cm_ahb_get_clock_divider();

        return (cpu_clk_t)get_clk_freq(curr_sysclk, curr_ahbclk);
}

#ifdef OS_FREERTOS

cpu_clk_t cm_cpu_clk_get_fromISR(void)
{
        sys_clk_t curr_sysclk = cm_sys_clk_get_fromISR();
        ahb_div_t curr_ahbclk = hw_clk_get_hclk_div();

        return (cpu_clk_t)get_clk_freq(curr_sysclk, curr_ahbclk);
}
#endif

/**
 * \brief Interrupt handler of the XTAL32M_RDY_IRQn.
 *
 */
void XTAL32M_Ready_Handler(void)
{
#if 0
        SEGGER_SYSTEMVIEW_ISR_ENTER();

        DBG_SET_HIGH(CLK_MGR_USE_TIMING_DEBUG, CLKDBG_XTAL32M_READY);

        ASSERT_WARNING(hw_clk_is_xtalm_started());

        if (dg_configXTAL32M_SETTLE_TIME_IN_USEC == 0) {
                if (hw_sys_hw_bsr_try_lock(HW_BSR_MASTER_SYSCPU, HW_BSR_WAKEUP_CONFIG_POS)) {
                        hw_clk_xtalm_update_rdy_cnt();
                        hw_sys_hw_bsr_unlock(HW_BSR_MASTER_SYSCPU, HW_BSR_WAKEUP_CONFIG_POS);
                } else {
                        /*
                         * CMAC has locked the BSR entry so CMAC will update the RDY counter.
                         * No need to do anything.
                         */
                }
        }

        xtal40m_settled = true;

        if (sysclk != sysclk_LP) {
                // Restore system clocks. xtal40m_rdy_cnt is updated in  cm_sys_clk_sleep()
                cm_sys_clk_sleep(false, false);

#ifdef OS_FREERTOS
                if (xEventGroupCM_xtal != NULL) {
                        OS_BASE_TYPE xHigherPriorityTaskWoken, xResult;

                        xResult = xtal40m_is_ready(&xHigherPriorityTaskWoken);

                        if (xResult != OS_FAIL) {
                                /*
                                 * If xHigherPriorityTaskWoken is now set to pdTRUE then a context
                                 * switch should be requested.
                                 */
                                OS_EVENT_YIELD(xHigherPriorityTaskWoken);
                        }
                }
#endif
        }

        DBG_SET_LOW(CLK_MGR_USE_TIMING_DEBUG, CLKDBG_XTAL32M_READY);

        SEGGER_SYSTEMVIEW_ISR_EXIT();
#endif
}

/**
 * \brief Interrupt handler of the PLL_LOCK_IRQn.
 *
 */
void PLL_Lock_Handler(void)
{
#if 1
        SEGGER_SYSTEMVIEW_ISR_ENTER();

        ASSERT_WARNING(hw_clk_is_pll_locked());

        pll_locked = true;

        if (sys_clk_next == sysclk_PLL480) {
                switch_to_pll();
        }

#ifdef OS_FREERTOS
        if (xEventGroupCM_xtal != NULL) {
                OS_BASE_TYPE xHigherPriorityTaskWoken, xResult;

                xResult = pll_is_locked(&xHigherPriorityTaskWoken);

                if (xResult != OS_FAIL) {
                        /* If xHigherPriorityTaskWoken is now set to pdTRUE then a context
                         * switch should be requested. */
                        OS_EVENT_YIELD(xHigherPriorityTaskWoken);
                }
        }
#endif
        SEGGER_SYSTEMVIEW_ISR_EXIT();
#endif
}

void cm_wait_xtalm_ready(void)
{
#ifdef OS_FREERTOS
        ASSERT_WARNING(xEventGroupCM_xtal != NULL);

        if (!xtal40m_settled) {
                // Do not go to sleep while waiting for XTAL32M to settle.
                OS_EVENT_GROUP_WAIT_BITS(xEventGroupCM_xtal,
                                XTAL40_AVAILABLE,
                                OS_EVENT_GROUP_FAIL,            // Don't clear bit after ret
                                OS_EVENT_GROUP_OK,              // Wait for all bits
                                OS_EVENT_GROUP_FOREVER);        // Block forever

                /* If we get here, XTAL32 must have settled */
                ASSERT_WARNING(xtal40m_settled == true);
        }
#else
        cm_halt_until_xtalm_ready();
#endif
}

void cm_wait_pll_lock(void)
{
#ifdef OS_FREERTOS
        ASSERT_WARNING(xEventGroupCM_xtal != NULL);

        pll_locked = hw_clk_is_pll_locked();
        if (!pll_locked) {
                // Do not go to sleep while waiting for PLL to lock.
                OS_EVENT_GROUP_WAIT_BITS(xEventGroupCM_xtal,
                                PLL_AVAILABLE,
                                OS_EVENT_GROUP_FAIL,            // Don't clear bit after ret
                                OS_EVENT_GROUP_OK,              // Wait for all bits
                                OS_EVENT_GROUP_FOREVER);        // Block forever

                /* If we get here, PLL must be locked */
                ASSERT_WARNING(pll_locked == true);
        }
#else
        cm_halt_until_pll_locked();
#endif
}

__RETAINED_CODE void cm_halt_until_sysclk_ready(void)
{
        if (sysclk != sysclk_RC32) {
                cm_halt_until_xtalm_ready();
        }

        if (sysclk == sysclk_PLL480) {
                cm_halt_until_pll_locked();
        }
}

#ifdef OS_FREERTOS

void cm_calibrate_rc32k(void)
{
        // TODO Implement cm_calibrate_rc32()
}

uint32_t cm_rcx_us_2_lpcycles(uint32_t usec)
{
        /* Can only convert up to 4095 usec */
        ASSERT_WARNING(usec < 4096);

        return ((usec << 20) / rcx_clock_period) + 1;
}

uint32_t cm_rcx_us_2_lpcycles_low_acc(uint32_t usec)
{
        return ((1 << 20) / (rcx_clock_period / usec)) + 1;
}

#if (BSP_CFG_LP_CLOCK_SOURCE == BSP_CLOCKS_SOURCE_LP_CLK_RCX)
void cm_rcx_trigger_calibration(void)
{
        uint32_t trigger_value = 100;
        sys_rcx_calibrate_set_bounds(trigger_value, trigger_value);
}
static void cm_notify_rcx_calibration_update(void)
{
        // Get measurement
        uint16_t val2;
        uint16_t val1 = sys_rcx_calibrate_get_value(&val2);
        // Update upper and lower bound
        sys_rcx_calibrate_set_bounds(val1, val2);
        OS_TASK_NOTIFY_FROM_ISR(xRCXCalibTaskHandle, RCX_DO_CALIBRATION, OS_NOTIFY_SET_BITS);
}

#if (dg_configRTC_CORRECTION == 1)

extern void hw_rtc_register_cb(void (*cb)(const hw_rtc_time_t *time));

static void cm_rtc_callback(const hw_rtc_time_t *time)
{
        rtc_usec_prev = ((((time->hour * 60 + time->minute) * 60) + time->sec) * 1000 + time->hsec *10)*1000LL;
        if (time->hour_mode && time->pm_flag) {
                rtc_usec_prev += HDAY_IN_USEC;
        }
        rtc_usec_correction = 0;
        rcx_freq_prev = initial_rcx_clock_hz_acc;
}
/**
 * \brief Apply compensation value to RTC.
 *
 * \p This function takes as input the new hundredths-of-seconds value.
 *
 * \param [in] new_hos value for the field hundredths-of-seconds of RTC.
 *
 * \note This function deals only with hundredths of seconds, nothing bigger than that.
 *
 */
static void cm_apply_rtc_compensation_hos(uint8_t new_hos)
{
        bsp_rtc_time_stop();
        uint32_t reg = RTC->RTC_TIME_REG;
        REG_SET_FIELD(RTC, RTC_TIME_REG, RTC_TIME_H_U,reg, (new_hos % 10) );
        REG_SET_FIELD(RTC, RTC_TIME_REG, RTC_TIME_H_T,reg, (new_hos / 10) );
        RTC->RTC_TIME_REG = reg;
        bsp_rtc_time_start();
}

/**
 * \brief Calculate RTC's compensation value and apply it, if desired.
 *
 * \p This function needs as input the latest calculated freq rcx_clock_hz and the initial one initial_rcx_clock_hz_acc.
 *
 * \note This function compensates up to hundredths of seconds.
 * \warning Must be called with interrupts disabled.
 *
 */
static void cm_calculate_rtc_compensation_value(void)
{
        hw_rtc_time_t current_time;
        uint32_t usec_delta_i, usec_delta_r, mean_rcx_clock_hz_acc;
        uint64_t usec;
        int32_t delta_slp_time;

        uint8_t num_of_hundredths, rtc_time_hundredths, new_rtc_time_hundredths;
        bool negative_offset = 0;
        bool mod_rtc_val;

        // Synchronize compensation process with RCX's rising edge.
        // Wait until Timer2 val changes. This happens in every RCX's rising edge.
        uint32_t val = hw_timer_get_count(HW_SYS_TIMER);
        while ( hw_timer_get_count(HW_SYS_TIMER) == val );

        // Read actual time from RTC
        hw_rtc_get_time_clndr(&current_time, NULL);

        usec = ((((current_time.hour * 60 + current_time.minute) * 60) + current_time.sec) * 1000 + current_time.hsec *10)*1000LL;
        if (current_time.hour_mode && current_time.pm_flag) {
                usec += HDAY_IN_USEC;
        }

        if (usec >= rtc_usec_prev) {
                usec_delta_i = usec - rtc_usec_prev;
        } else {
                usec_delta_i = (DAY_IN_USEC + usec) - rtc_usec_prev;
        }
        // Calculate the mean frequency from the last measurement
        mean_rcx_clock_hz_acc = (rcx_freq_prev + rcx_clock_hz_acc) / 2;

        // Calculate the theoretical time
        usec_delta_r = (uint32_t)(((uint64_t)usec_delta_i * (uint64_t)mean_rcx_clock_hz_acc) / (uint64_t)initial_rcx_clock_hz_acc);

        delta_slp_time = (int32_t)usec_delta_r - (int32_t)usec_delta_i;         // theoretical time - actual time
        rtc_usec_correction += delta_slp_time;                                  // correction factor

        if (rtc_usec_correction / HUNDREDTHS_OF_SEC_us > 0 ) {
                // rcx is rushing, rtc_usec_correction > 0, frequency is greater than initial
                negative_offset = true;
                mod_rtc_val = true;
        } else if (rtc_usec_correction / HUNDREDTHS_OF_SEC_us < 0) {
                // rcx is delayed, rtc_usec_correction < 0, frequency is smaller than initial
                negative_offset = false;
                mod_rtc_val = true;
        } else {
                mod_rtc_val = false;
        }

        rtc_usec_prev = usec;
        rcx_freq_prev = rcx_clock_hz_acc;

        if (mod_rtc_val) {
                // num_of_hundredths must be a positive number
                if (rtc_usec_correction < 0) {
                        num_of_hundredths = (rtc_usec_correction * (-1)) / HUNDREDTHS_OF_SEC_us;
                } else {
                        num_of_hundredths = rtc_usec_correction / HUNDREDTHS_OF_SEC_us;
                }

                rtc_time_hundredths = current_time.hsec;          // RTC's hos should not have changed yet.
                if (!negative_offset) {                         // if rcx is delayed, RTC is delayed too
                        if (rtc_time_hundredths + num_of_hundredths > 99) {
                                num_of_hundredths = 99 - rtc_time_hundredths;
                        }
                        rtc_usec_correction += (HUNDREDTHS_OF_SEC_us * num_of_hundredths);
                        new_rtc_time_hundredths = rtc_time_hundredths + num_of_hundredths;
                        rtc_usec_prev += (HUNDREDTHS_OF_SEC_us * num_of_hundredths);
                } else {                                        // if rcx is rushing, RTC is rushing too
                        if (rtc_time_hundredths < num_of_hundredths) {
                                num_of_hundredths = rtc_time_hundredths;
                        }
                        rtc_usec_correction -= (HUNDREDTHS_OF_SEC_us * num_of_hundredths);
                        new_rtc_time_hundredths = rtc_time_hundredths - num_of_hundredths;
                        rtc_usec_prev -= (HUNDREDTHS_OF_SEC_us * num_of_hundredths);
                }
                if (new_rtc_time_hundredths > 99) {
                        return;
                }
                cm_apply_rtc_compensation_hos(new_rtc_time_hundredths);
        }
}

#endif /* dg_configRTC_CORRECTION == 1 */

/**
 * \brief RCX Calibration Task function.
 *
 * \param [in] pvParameters ignored.
 */
static void rcx_calibration_task( void *pvParameters )
{
        uint32_t ulNotifiedValue;
        OS_BASE_TYPE xResult __UNUSED;
        uint32_t cal_value;

        /* Initialize SYS RCX for RCX calibration */
        sys_rcx_calibrate_config(cm_notify_rcx_calibration_update);

#if (dg_configRTC_CORRECTION == 1)
        hw_rtc_register_cb(cm_rtc_callback);
#endif

        while (1) {
                // Wait for the internal notifications.
                xResult = OS_TASK_NOTIFY_WAIT(0x0, OS_TASK_NOTIFY_ALL_BITS, &ulNotifiedValue,
                                                                        OS_TASK_NOTIFY_FOREVER);
                OS_ASSERT(xResult == OS_OK);

                if (ulNotifiedValue & RCX_DO_CALIBRATION) {
                        uint64_t max_clk_count;

                        OS_ENTER_CRITICAL_SECTION();

                        cal_value        = hw_clk_get_calibration_data();
                        max_clk_count    = (uint64_t)dg_configXTAL32M_FREQ * RCX_CALIBRATION_CYCLES_WUP * RCX_ACCURACY_LEVEL;
                        rcx_clock_hz_acc = (max_clk_count + (cal_value >> 1)) / cal_value;
                        rcx_clock_hz     = rcx_clock_hz_acc / RCX_ACCURACY_LEVEL;
                        rcx_tick_rate_hz = get_optimum_tick_rate(rcx_clock_hz, &rcx_tick_period);
                        rcx_clock_period = (uint32_t)((rcx_period_dividend * RCX_ACCURACY_LEVEL) / rcx_clock_hz_acc);

#ifdef CONFIG_USE_BLE
#if (USE_BLE_SLEEP == 1)
                        /*
                         * Notify CMAC about the new values of:
                         *   rcx_clock_period
                         *   rcx_clock_hz_acc
                         */
                        ad_ble_update_rcx();
#endif /* (USE_BLE_SLEEP == 1) */
#endif /* CONFIG_USE_BLE */

#if (dg_configRTC_CORRECTION == 1)
                        // Run RTC compensation only if RTC time is running.
                        if (!HW_RTC_REG_GETF(RTC_CONTROL_REG, RTC_TIME_DISABLE)) {
                                cm_calculate_rtc_compensation_value();
                        }
#endif
                        OS_LEAVE_CRITICAL_SECTION();

#if (CPM_USE_RCX_DEBUG == 1)
                        log_printf(LOG_NOTICE, 1,
                                "clock_hz=%5d, tick_period=%3d, tick_rate_hz=%5d, clock_period=%10d\r\n",
                                rcx_clock_hz, rcx_tick_period, rcx_tick_rate_hz,
                                rcx_clock_period);
#endif
                }
        }
}
#endif
/**
 * \brief Start the timer and block sleep while the low power clock is settling.
 *
 * \details It starts the timer that blocks system from sleeping for
 *          dg_configXTAL32K_SETTLE_TIME. This is needed when the XTAL32K is used to make sure
 *          that the clock has settled properly before going back to sleep again.
 */
static void lp_clk_timer_start(void)
{
        /* Start the timer.  No block time is specified, and even if one was
         it would be ignored because the RTOS scheduler has not yet been
         started. */
        if (OS_TIMER_START(xLPSettleTimer, 0) != OS_TIMER_SUCCESS) {
                // The timer could not be set into the Active state.
                OS_ASSERT(0);
        }
}

#if (BSP_CFG_LP_CLOCK_SOURCE == BSP_CLOCKS_SOURCE_LP_CLK_RCX)
void cm_rcx_calibration_task_init(void) {

        /* Start the task that will handle the calibration calculations,
         * which require ~80usec@32MHz to complete. */
        // TODO: update the time in previous comment

        OS_BASE_TYPE status;

        // Create the RCX calibration task
        status = OS_TASK_CREATE("RCXcal",                       // The text name of the task.
                                rcx_calibration_task,           // The function that implements the task.
                                ( void * ) NULL,                // No parameter is passed to the task.
                                configMINIMAL_STACK_SIZE * OS_STACK_WORD_SIZE,  // The size of the stack to allocate.

                                ( tskIDLE_PRIORITY ),           // The priority assigned to the task.
                                xRCXCalibTaskHandle);           // The task handle is required.
        OS_ASSERT(status == pdPASS);

        (void) status;                                          // To satisfy the compiler
}
#endif

void cm_lp_clk_init(void)
{
        ASSERT_WARNING(xSemaphoreCM != NULL);

        bsp_prv_lpclk_xtal_on();
        CM_EVENT_WAIT();

        xLPSettleTimer = OS_TIMER_CREATE("LPSet",
#if (TEST_LP32K_FAST_DETECT == 1)
                                OS_MS_2_TICKS(5),
#else
                                OS_MS_2_TICKS(dg_configXTAL32K_SETTLE_TIME),
#endif
                                OS_TIMER_SUCCESS,          // Run once
                                (void *) 0,             // Timer id == none
                                vLPTimerCallback);      // Call-back
        OS_ASSERT(xLPSettleTimer != NULL);

        if (FSP_SUCCESS == bsp_prv_lpclk_select(LP_CLK_IS_XTAL32K))
        {
                /* Enable RTC freerunning counter mirror */
                bsp_prv_rtc_mirror_init();

#ifdef CONFIG_USE_BLE
                // Inform ble adapter about the availability of the LP clock.
                ad_ble_lpclock_available();
#endif
                /* Inform (blocked) Tasks about the availability of the LP clock. */
                OS_EVENT_GROUP_SET_BITS(xEventGroupCM_xtal, LP_CLK_AVAILABLE);
        } else {
                lp_clk_timer_start();
        }

        CM_EVENT_SIGNAL();
}

bool cm_lp_clk_is_avail(void)
{
        ASSERT_WARNING(xEventGroupCM_xtal != NULL);

        return (OS_EVENT_GROUP_GET_BITS(xEventGroupCM_xtal) & LP_CLK_AVAILABLE);
}

__RETAINED_CODE bool cm_lp_clk_is_avail_fromISR(void)
{
        ASSERT_WARNING(xEventGroupCM_xtal != NULL);

        return (OS_EVENT_GROUP_GET_BITS_FROM_ISR(xEventGroupCM_xtal) & LP_CLK_AVAILABLE);
}

void cm_wait_lp_clk_ready(void)
{
        ASSERT_WARNING(xEventGroupCM_xtal != NULL);

        OS_EVENT_GROUP_WAIT_BITS(xEventGroupCM_xtal,
                LP_CLK_AVAILABLE,
                OS_EVENT_GROUP_FAIL,                            // Don't clear bit after ret
                OS_EVENT_GROUP_OK,                              // Wait for all bits
                OS_EVENT_GROUP_FOREVER);                        // Block forever
}

void cm_lp_clk_wakeup(void)
{
        ASSERT_WARNING(xEventGroupCM_xtal != NULL);

        OS_EVENT_GROUP_CLEAR_BITS_FROM_ISR(xEventGroupCM_xtal, LP_CLK_AVAILABLE);
}
#endif /* OS_FREERTOS */

/*
 * Functions intended to be used only by the Clock and Power Manager or in hooks.
 */
__RETAINED_CODE static void apply_lowered_clocks(sys_clk_t new_sysclk, ahb_div_t new_ahbclk)
{
        /* TODO: Check disabling PLL will make a difference.
         * TODO: If PLL power-up is quite fast then we could turn it off and then back-on when restoring the clocks */

        // First the system clock
        if (new_sysclk != sysclk) {
                sys_clk_next = new_sysclk;

                adjust_peri_access_timings(false);
                // fast --> slow clock switch
                hw_clk_set_sysclk(SYS_CLK_IS_XTAL40M);                  // Set XTAL32 as sys_clk
                adjust_otp_access_timings();                         // Adjust OTP timings
        }
        // else cm_sysclk is RC32 as in all other cases it is set to XTAL32M.

        // Then the AHB clock
        if (new_ahbclk != ahbclk) {
                ahb_clk_next = new_ahbclk;

                if (ahbclk < new_ahbclk) {
                        // fast --> slow clock switch
                        hw_clk_set_hclk_div(new_ahbclk);
                        adjust_otp_access_timings();                 // Adjust OTP timings
                        adjust_peri_access_timings(true);
                } else {
                        // slow --> fast clock switch
                        adjust_otp_access_timings();                 // Adjust OTP timings
                        hw_clk_set_hclk_div(new_ahbclk);
                        adjust_peri_access_timings(true);
                }
        }
}

__RETAINED_CODE void cm_lower_all_clocks(void)
{
        DBG_SET_HIGH(CLK_MGR_USE_TIMING_DEBUG, CLKDBG_LOWER_CLOCKS);

        sys_clk_t new_sysclk;
        ahb_div_t new_ahbclk = ahb_div1;

#ifdef OS_FREERTOS
        // Cannot lower clocks if the first calibration has not been completed.
        if ((dg_configLP_CLK_SOURCE == LP_CLK_IS_ANALOG) && (BSP_CFG_LP_CLOCK_SOURCE == BSP_CLOCKS_SOURCE_LP_CLK_RCX) && !cm_lp_clk_is_avail_fromISR()) {
                return;
        }
#endif

        // Check which is the lowest system clock that can be used.
        do {
                new_sysclk = sysclk;

                // Check XTAL32 has settled.
                if (!xtal40m_settled) {
                        break;
                }

                switch (sysclk) {
                case sysclk_RC32:
                        // fall-through
                case sysclk_XTAL40M:
                        // unchanged: new_sysclk = cm_sysclk
                        break;
                case sysclk_PLL480:
                        new_sysclk = sysclk_XTAL40M;
                        break;

                case sysclk_LP:
                        // fall-through
                default:
                        // should never reach this point
                        ASSERT_WARNING(0);
                }
        } while (0);

        if (!xtal40m_settled) {
                new_ahbclk = ahb_div16;                               // Use 2MHz AHB clock.
        } else {
                new_ahbclk = ahb_div8;                                // Use 4Mhz AHB clock.
        }

        // Check if the SysTick is ON and if it is affected
        if ((dg_configABORT_IF_SYSTICK_CLK_ERR) && (SysTick->CTRL & SysTick_CTRL_ENABLE_Msk)) {
                if ((new_sysclk != sysclk) || (new_ahbclk != ahbclk)) {
                        /*
                         * This is an application error! The SysTick should not run with any of the
                         * sleep modes active! This is because the OS may decide to go to sleep
                         * because all tasks are blocked and nothing is pending, although the
                         * SysTick is running.
                         */
                        new_sysclk = sysclk;
                        new_ahbclk = ahbclk;
                }
        }

        apply_lowered_clocks(new_sysclk, new_ahbclk);
}

__RETAINED_CODE void cm_restore_all_clocks(void)
{
#if (DEVICE_FAMILY == DA1487X)
#else
#ifdef OS_FREERTOS
        if ((dg_configLP_CLK_SOURCE == LP_CLK_IS_ANALOG) && (BSP_CFG_LP_CLOCK_SOURCE == BSP_CLOCKS_SOURCE_LP_CLK_RCX) && !cm_lp_clk_is_avail_fromISR()) {
                return;
        }
#endif
        // Set the AMBA High speed Bus clock (slow --> fast clock switch)
        adjust_peri_access_timings(false);
        if (ahbclk != (ahb_div_t)hw_clk_get_hclk_div()) {
                ahb_clk_next = ahbclk;

                adjust_otp_access_timings();                         // Adjust OTP timings
                hw_clk_set_hclk_div(ahbclk);
        }

        // Set the system clock (slow --> fast clock switch)
        if (xtal40m_settled && (sysclk != sysclk_RC32)) {
                sys_clk_next = sysclk;

                adjust_otp_access_timings();                         // Adjust OTP timings
                if (sysclk >= sysclk_PLL480) {
                        hw_clk_set_sysclk(SYS_CLK_IS_PLL160M);           // Set PLL as sys_clk
                } else {
                        hw_clk_set_sysclk(SYS_CLK_IS_XTAL40M);       // Set XTAL32 as sys_clk
                }
        }

        adjust_peri_access_timings(true);
        DBG_SET_LOW(CLK_MGR_USE_TIMING_DEBUG, CLKDBG_LOWER_CLOCKS);
#endif
}

#ifdef OS_FREERTOS
void cm_wait_xtalm_ready_fromISR(void)
{
        if (!xtal40m_settled) {
#if     (DEVICE_FAMILY == DA1640X)
                // TODO: HOWTO?
#else
                while (NVIC_GetPendingIRQ(XTAL32M_RDY_IRQn) == 0);
#endif
                xtal40m_settled = true;
                cm_switch_to_xtalm_if_settled();
        }
}

#endif /* OS_FREERTOS */

__RETAINED_CODE bool cm_poll_xtalm_ready(void)
{
        return xtal40m_settled;
}

void cm_halt_until_xtalm_ready(void)
{
#ifdef OS_FREERTOS
        while (!xtal40m_settled) {
                GLOBAL_INT_DISABLE();
                /* System waking up. We ignore this PRIMASK set. */
                DBG_CONFIGURE_LOW(CMN_TIMING_DEBUG, CMNDBG_CRITICAL_SECTION);
                if (!xtal40m_settled) {
                        lower_amba_clocks();
                        __WFI();
                        restore_amba_clocks();
                }
                GLOBAL_INT_RESTORE();
        }
#else
        while (!xtal40m_settled) {
                lower_amba_clocks();
                __WFI();
                restore_amba_clocks();
        }
#endif /* OS_FREERTOS */
}

void cm_register_xtal_ready_callback(void (*cb)(void))
{
        xtal_ready_callback = cb;
}

void cm_halt_until_pll_locked(void)
{
        pll_locked = hw_clk_is_pll_locked();

#ifdef OS_FREERTOS
        ASSERT_WARNING(xEventGroupCM_xtal != NULL);

        while (!pll_locked) {
                GLOBAL_INT_DISABLE();
                /* System waking up. We ignore this PRIMASK set. */
                if (!pll_locked) {
                        lower_amba_clocks();
                        __WFI();
                        restore_amba_clocks();
                }
                GLOBAL_INT_RESTORE();
        }
#else
        while (!pll_locked) {
                lower_amba_clocks();
                __WFI();
                restore_amba_clocks();
        }
#endif /* OS_FREERTOS */
}

/**
 * \brief Switch to XTAL32M - Interrupt Safe version.
 *
 * \detail Waits until the XTAL32M has settled and sets it as the system clock.
 *
 * \warning It is called from Interrupt Context.
 */
__STATIC_INLINE void switch_to_xtal_safe(void)
{
        cm_halt_until_xtalm_ready();

        adjust_peri_access_timings(false);
        if (sys_clk_next > sysclk) {              // slow --> fast clock switch
                adjust_otp_access_timings();         // Adjust OTP timings
                hw_clk_set_sysclk(SYS_CLK_IS_XTAL40M);  // Set XTAL32 as sys_clk
        } else {                                        // fast --> slow clock switch
                hw_clk_set_sysclk(SYS_CLK_IS_XTAL40M);  // Set XTAL32 as sys_clk
                adjust_otp_access_timings();         // Adjust OTP timings
        }
        adjust_peri_access_timings(true);
}

__RETAINED_CODE void cm_sys_clk_sleep(bool entering_sleep, bool pll_off)
{
        ahb_clk_next = ahb_div1;

        if (entering_sleep == true) {
                if(dup_call_clk_sleep != false){
                        return;
                }
                dup_call_clk_sleep = true;

                // Sleep entry : No need to switch to RC32. PDC will do it.
                adjust_peri_access_timings(false);

                if (sysclk == sysclk_PLL480) {
                        if (
#if (HW_DESCOPED_CLOCK == 1)
                              (hw_clk_get_sysclk() == SYS_CLK_IS_PLL240M)
                           || (hw_clk_get_sysclk() == SYS_CLK_IS_PLL192M)
                           ||
#endif
                              (hw_clk_get_sysclk() == SYS_CLK_IS_PLL160M)
                           || (hw_clk_get_sysclk() == SYS_CLK_IS_PLL137M)
                           || (hw_clk_get_sysclk() == SYS_CLK_IS_PLL106M) ){
                                // Transition from PLL to RC32 is not allowed.
                                // Switch to XTAL32M first.
                                switch_to_xtal40m();
                        }
                        // No need to disable RC32M. It is already disabled.
                        if( pll_off == true ) {
                                disable_pll();
                        }
                }

                hw_clk_xtalm_compensate_amp();

#if (TEST_RTC_MR_ACCESS_UNDER_LP32K == 1)
                // DA1640x : Do not change the clock to 32KHz until goto_deepsleep().
                // If the clock is below 32KHz clock,
                // the system will run very slowly and can not be powered down efficiently.
                // The clock change into 32KHz will be applied by goto_deepsleep().

                if (sysclk != sysclk_RC32) {
                        switch_to_rc32();
                        hw_clk_disable_sysclk(SYS_CLK_IS_XTAL40M);
                }
#endif
                // Make sure that the AHB and APB busses are clocked at 32MHz.
                if (ahbclk != ahb_div1) {
                        // slow --> fast clock switch
                        adjust_otp_access_timings();                 // Adjust OTP timings
                        hw_clk_set_hclk_div(ahb_div1);                  // cm_ahbclk is not altered!
                }
                //hw_clk_set_pclk_div(apb_div1);                          // cm_apbclk is not altered!
                adjust_peri_access_timings(true);
        }
        else if(xtal40m_settled == true){
                if(dup_call_clk_sleep != true){
                        return;
                }
                dup_call_clk_sleep = false;
                /*
                 * XTAL32M ready: transition to the cm_sysclk, cm_ahbclk and cm_apbclk that were set
                 * by the user.
                 *
                 * Note that when the system wakes up the system clock is RC32 and the AHB / APB are
                 * clocked at highest frequency (because this is what the setting was just before
                 * sleep entry).
                 */

                sys_clk_t tmp_sys_clk;

#if (USE_BLE_SLEEP == 1)
                if (REG_GETF(CRG_XTAL, XTALRDY_CTRL_REG, XTALRDY_CLK_SEL) == 0) {
                        /*
                         * If normal XTAL32M start-up is used the XTAL32M settling time may have changed.
                         * Update CMAC wake-up time anyway. A recalculation may be required even if
                         * CMAC has updated the wake-up time.
                         */
                        ad_ble_update_wakeup_time();
                }
#endif
                adjust_peri_access_timings(false);

                if ((sysclk != sysclk_RC32) && xtal40m_settled) {
                        tmp_sys_clk = sysclk;

                        if (hw_clk_get_sysclk() == SYS_CLK_IS_LP) {
                                sys_clk_next = sysclk_XTAL40M;
                                sysclk = sysclk_RC32;               // Current clock is RC32
                                switch_to_xtal_safe();
                                sysclk = sys_clk_next;

                                sys_clk_next = tmp_sys_clk;
                        }

                        if (sys_clk_next == sysclk_PLL480) {
                                if (hw_clk_is_pll_locked() == false) {
                                        // System clock will be switched to PLL when PLL is locked
                                        enable_pll();
                                }
                                switch_to_pll();
                        }
                        sysclk = sys_clk_next;
                } else {
                        // If the user uses RC32 as the system clock then there's nothing to be done!
                }

                if ((cpuclk >= SYS_CLK_IS_PLL240M) && (cpuclk <= SYS_CLK_IS_PLL106M)) {
                        hw_clk_set_sysclk(cpuclk);
                }

                if (ahbclk != ahb_div1) {
                        ahb_clk_next = ahbclk;

                        // fast --> slow clock switch
                        hw_clk_set_hclk_div(ahbclk);                 // cm_ahbclk is not altered!
                        adjust_otp_access_timings();                 // Adjust OTP timings
                }
                // else cm_ahbclk == ahb_div1 and nothing has to be done!

                if (apbclk != apb_div1) {
                        //hw_clk_set_pclk_div(apbclk);
                }
                // else cm_apbclk == apb_div1 and nothing has to be done!

                adjust_peri_access_timings(true);
        }
}

__STATIC_INLINE void cm_sys_restore_sysclk(sys_clk_t prev_sysclk)
{
        ASSERT_ERROR(prev_sysclk == sysclk_PLL480);

        sys_enable_pll();
        sys_clk_next = prev_sysclk;
        switch_to_pll();
}

#ifdef OS_FREERTOS
__RETAINED_CODE void cm_sys_clk_wakeup(void)
{
        /*
         * Clear the "XTAL40_AVAILABLE" flag in the Event Group of the Clock Manager. It will be
         * set again to 1 when the XTAL32M has settled.
         * Note: this translates to a message in a queue that unblocks the Timer task in order to
         * service it. This will be done when the OS scheduler is resumed. Even if the
         * XTAL32M_RDY_IRQn hits while still in this function (pm_sys_wakeup_mode_is_XTAL32 is true), this
         * will result to a second message being added to the same queue. When the OS scheduler is
         * resumed, the first task that will be executed is the Timer task. This will first process
         * the first message of the queue (clear Event Group bits) and then the second (set Event
         * Group bits), which is the desired operation.
         */

        /* Timer task must have the highest priority so that it runs first
         * as soon as the OS scheduler is unblocked.
         * See caller (system_wake_up()) */
        ASSERT_WARNING(configTIMER_TASK_PRIORITY == (configMAX_PRIORITIES - 1));

        xtal40m_settled_notification = false;
        xtal40m_settled = hw_clk_is_xtalm_started();
        if (!xtal40m_settled) {
                OS_EVENT_GROUP_CLEAR_BITS_FROM_ISR(xEventGroupCM_xtal, XTAL40_AVAILABLE);
        }

        pll_locked = hw_clk_is_pll_locked();
        if (!pll_locked) {
                OS_EVENT_GROUP_CLEAR_BITS_FROM_ISR(xEventGroupCM_xtal, PLL_AVAILABLE);
        }
}

__RETAINED_CODE void cm_switch_to_xtalm_if_settled(void)
{
        if (xtal40m_settled) {
                // Restore system clocks
                cm_sys_clk_sleep(false, false);

                OS_BASE_TYPE xHigherPriorityTaskWoken;

                xtal40m_is_ready(&xHigherPriorityTaskWoken);
        }
}

#endif /* OS_FREERTOS */

#endif /* dg_configUSE_CLOCK_MGR */

#endif /* DEVICE_FAMILY */

/**
 \}
 \}
 \}
 */
