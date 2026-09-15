#include <stdint.h>

#include <rtthread.h>

#include "bsp_api.h"
#include "bsp_clocks.h"
#include "hal_data.h"
#include "sys_clock_mgr.h"

#include "wifi_hw_prepare.h"

#define WIFI_HW_CLOCK_TIMEOUT (5000000U)
#define WIFI_HW_CONSOLE_BAUD  (115200U)
#define WIFI_HW_CPU_CLOCK_HZ  (160000000U)
#define WIFI_HW_UART_CLOCK_HZ (80000000U)

extern uint32_t g_bsp_reset_stat_reg;

static void wifi_hw_freeze_system_watchdog(void)
{
    /*
     * RA6W1 starts SYS_WDOG after reset. The r_wdog_w object is linked into
     * this reduced image, so the BSP weak freeze hook is replaced by the
     * driver's no-op hook. This hardware-only test has no watchdog service
     * task, therefore freeze it explicitly.
     */
    CRG_TOP->SET_FREEZE_REG_b.FRZ_SYS_WDOG = 1;

    rt_kprintf("[wifi-hw] SYS_WDOG frozen: ctrl=0x%08x count=0x%08x\n",
               (unsigned int) SYS_WDOG->WATCHDOG_CTRL_REG,
               (unsigned int) SYS_WDOG->WATCHDOG_REG);
}

static void wifi_hw_print_reset_status(void)
{
    uint32_t reset_status = g_bsp_reset_stat_reg;

    rt_kprintf("[wifi-hw] reset status=0x%08x POR=%u HW=%u SW=%u SWD=%u M33_WDOG=%u\n",
               (unsigned int) reset_status,
               (unsigned int) ((reset_status >> 0) & 1U),
               (unsigned int) ((reset_status >> 1) & 1U),
               (unsigned int) ((reset_status >> 2) & 1U),
               (unsigned int) ((reset_status >> 3) & 1U),
               (unsigned int) ((reset_status >> 4) & 1U));
}

static int wifi_hw_wait_xtal_ready(void)
{
    uint32_t retry;

    for (retry = 0; retry < WIFI_HW_CLOCK_TIMEOUT; retry++)
    {
        /*
         * On RA6W1 the Wi-Fi clock manager tracks XTAL readiness through its
         * own XTAL-ready event state. That state is not present in the
         * RT-Thread port. Use the hardware status bits instead: RUNNING_AT_XTAL40M
         * is the clock mux status and XTAL40M_RDY is the oscillator lock status.
         */
        if (CRG_TOP->CLK_CTRL_REG_b.RUNNING_AT_XTAL40M ||
            CRG_COM->XTAL40M_CTRL_REG_b.XTAL40M_RDY)
        {
            return RT_EOK;
        }
    }

    return -RT_ETIMEOUT;
}

static int wifi_hw_select_xtal_safe(void)
{
    rt_base_t level;
    uint32_t retry;

    /* hw_clk_pll_sys_on() expects the CPU to be running from XTAL40M. */
    level = rt_hw_interrupt_disable();
    CRG_TOP->CLK_CTRL_REG_b.PLL_CLK_SEL = 2;
    CRG_TOP->CLK_CTRL_REG_b.SYS_CLK_SEL = 1;
    rt_hw_interrupt_enable(level);

    for (retry = 0; retry < WIFI_HW_CLOCK_TIMEOUT; retry++)
    {
        if (CRG_TOP->CLK_CTRL_REG_b.RUNNING_AT_XTAL40M)
        {
            return RT_EOK;
        }
    }

    return -RT_ETIMEOUT;
}

static int wifi_hw_wait_pll_lock(void)
{
    uint32_t retry;

    for (retry = 0; retry < WIFI_HW_CLOCK_TIMEOUT; retry++)
    {
        if (CRG_COM->PLL1_ARM_CTRL_REG_b.PLL_LOCK)
        {
            return RT_EOK;
        }
    }

    return -RT_ETIMEOUT;
}

static int wifi_hw_wait_pll_selected(void)
{
    uint32_t retry;

    for (retry = 0; retry < WIFI_HW_CLOCK_TIMEOUT; retry++)
    {
        if (CRG_TOP->CLK_CTRL_REG_b.RUNNING_AT_PLL)
        {
            return RT_EOK;
        }
    }

    return -RT_ETIMEOUT;
}

static void wifi_hw_print_clock_state(const char *tag)
{
    uint32_t clk_ctrl;
    uint32_t pll_ctrl;
    uint32_t xtal_ctrl;

    clk_ctrl = CRG_TOP->CLK_CTRL_REG;
    pll_ctrl = CRG_COM->PLL1_ARM_CTRL_REG;
    xtal_ctrl = CRG_COM->XTAL40M_CTRL_REG;

    rt_kprintf("[wifi-hw] %s: CLK_AMBA=0x%08x CLK_CTRL=0x%08x "
               "PLL_CTRL=0x%08x XTAL_CTRL=0x%08x SYS_STATUS=0x%08x\n",
               tag,
               (unsigned int) CRG_TOP->CLK_AMBA_REG,
               (unsigned int) clk_ctrl,
               (unsigned int) pll_ctrl,
               (unsigned int) xtal_ctrl,
               (unsigned int) CRG_TOP->SYS_STATUS_REG);
    rt_kprintf("[wifi-hw] %s: SYS_SEL=%u PLL_SEL=%u PLL_EN=%u PLL_LOCK=%u "
               "XTAL_EN=%u XTAL_RDY=%u XTAL_DPLL_EN=%u RUN_XTAL=%u RUN_PLL=%u\n",
               tag,
               (unsigned int) (clk_ctrl & 0x3U),
               (unsigned int) ((clk_ctrl >> 2) & 0x7U),
               (unsigned int) ((pll_ctrl >> 8) & 0x1U),
               (unsigned int) ((pll_ctrl >> 13) & 0x1U),
               (unsigned int) (xtal_ctrl & 0x1U),
               (unsigned int) ((xtal_ctrl >> 23) & 0x1U),
               (unsigned int) ((xtal_ctrl >> 2) & 0x1U),
               (unsigned int) ((clk_ctrl >> 12) & 0x1U),
               (unsigned int) ((clk_ctrl >> 13) & 0x1U));
}

static int wifi_hw_restore_console_baud(void)
{
    uart_w_baud_setting_t baud_setting;
    fsp_err_t             fsp_status;
    uint64_t              divisor_denominator;
    uint64_t              divisor_x64;

    /*
     * The generated RA6W1 console setting is 43/26. The UART peripheral
     * clock in the PLL path is 80 MHz, while R_FSP_SystemClockHzGet() does
     * not account for PLL_CLK_SEL=2 and reports an invalid value here.
     */
    divisor_denominator = 16ULL * WIFI_HW_CONSOLE_BAUD;
    divisor_x64 = (((uint64_t) WIFI_HW_UART_CLOCK_HZ * 64ULL) +
                   (divisor_denominator / 2ULL)) /
                  divisor_denominator;
    baud_setting.int_baud = (uint32_t) (divisor_x64 / 64ULL);
    baud_setting.fra_baud = (uint32_t) (divisor_x64 % 64ULL);

    /* The clock switch must not leave the console channel gated. */
    hw_clk_enable_uart_w_clk(g_uart0_cfg.channel);
    fsp_status = g_uart0.p_api->baudSet(g_uart0.p_ctrl, &baud_setting);
    if (FSP_SUCCESS != fsp_status)
    {
        rt_kprintf("[wifi-hw] UART baud update failed: %d\n", fsp_status);
        return -RT_ERROR;
    }

    rt_kprintf("[wifi-hw] UART baud restored: clock=%u int=%u frac=%u\n",
               (unsigned int) WIFI_HW_UART_CLOCK_HZ,
               (unsigned int) baud_setting.int_baud,
               (unsigned int) baud_setting.fra_baud);
    return RT_EOK;
}

static int wifi_hw_refresh_runtime_timing(void)
{
    uint32_t systick_reload;

    /*
     * SystemCoreClockUpdate() uses the 480 MHz PLL source entry and ignores
     * PLL_CLK_SEL. This path selects PLL160, so record the actual core clock
     * explicitly before updating delay and RT-Thread tick timing.
     */
    SystemCoreClock = WIFI_HW_CPU_CLOCK_HZ;
    systick_reload = SystemCoreClock / RT_TICK_PER_SECOND;
    if ((systick_reload == 0U) || (systick_reload > SysTick_LOAD_RELOAD_Msk))
    {
        rt_kprintf("[wifi-hw] invalid SysTick reload: %u\n",
                   (unsigned int) systick_reload);
        return -RT_EINVAL;
    }

    if (wifi_hw_restore_console_baud() != RT_EOK)
    {
        return -RT_ERROR;
    }

    /* Do this after UART is repaired, because diagnostics use uart0. */
    if (SysTick_Config(systick_reload) != 0U)
    {
        rt_kprintf("[wifi-hw] SysTick configuration failed\n");
        return -RT_ERROR;
    }
    NVIC_SetPriority(SysTick_IRQn, 0xFF);

    rt_kprintf("[wifi-hw] runtime timing refreshed: core=%u uart=115200 tick=%u Hz\n",
               (unsigned int) SystemCoreClock,
               (unsigned int) RT_TICK_PER_SECOND);
    return RT_EOK;
}

int wifi_hw_platform_prepare(void)
{
    rt_base_t level;
    uint32_t pll_result;
    int status;

    rt_kprintf("[wifi-hw] prepare clocks and Wi-Fi platform\n");
    wifi_hw_print_reset_status();
    wifi_hw_freeze_system_watchdog();
    wifi_hw_print_clock_state("before prepare");

    /* Match prvSetupHardware() in the working FSP Wi-Fi project. */
    CRG_TOP->CLK_AMBA_REG_b.PERI_CLK_ENABLE = 1;
    CRG_TOP->CLK_AMBA_REG_b.TIMER_CLK_ENABLE = 1;

    /* The working Wi-Fi path changes OQSPI to a safe divider before PLL use. */
    CRG_TOP->CLK_AMBA_REG_b.OQSPIF_DIV = 2;

    /* Keep the middleware-visible clock state consistent with the hardware. */
    cm_sys_clk_init(sysclk_PLL480);

    rt_kprintf("[wifi-hw] enable XTAL40M\n");
    bsp_clock_xtalm_enable(true);
    status = wifi_hw_wait_xtal_ready();
    if (status != RT_EOK)
    {
        rt_kprintf("[wifi-hw] XTAL40M is not ready\n");
        wifi_hw_print_clock_state("XTAL timeout");
        return status;
    }

    /*
     * The RT-Thread BSP normally leaves the chip on the configured PLL. If it
     * does not, use the real RA6W1 FSP PLL routine before touching the Wi-Fi
     * AHB window. This is deliberately bounded so a clock fault is reported.
     */
    if (!CRG_TOP->CLK_CTRL_REG_b.RUNNING_AT_PLL)
    {
        status = wifi_hw_select_xtal_safe();
        if (status != RT_EOK)
        {
            rt_kprintf("[wifi-hw] cannot select XTAL40M before PLL startup\n");
            wifi_hw_print_clock_state("XTAL select timeout");
            return status;
        }

        wifi_hw_print_clock_state("before PLL startup");
        rt_kprintf("[wifi-hw] start PLL480\n");
        pll_result = hw_clk_pll_sys_on();
        rt_kprintf("[wifi-hw] PLL startup returned 0x%08x\n",
                   (unsigned int) pll_result);
        wifi_hw_print_clock_state("after PLL startup");

        status = wifi_hw_wait_pll_lock();
        if (status != RT_EOK)
        {
            rt_kprintf("[wifi-hw] PLL did not lock\n");
            wifi_hw_print_clock_state("PLL timeout");
            return status;
        }
    }

    /* Configure the same 160 MHz CPU / peripheral clock used by the demo. */
    level = rt_hw_interrupt_disable();
    CRG_TOP->CLK_AMBA_REG_b.HCLK_DIV = 0;
    CRG_TOP->CLK_CTRL_REG_b.PLL_CLK_SEL = 2;
    CRG_TOP->CLK_CTRL_REG_b.PLL_PERI_ENABLE = 1;
    CRG_TOP->CLK_CTRL_REG_b.PLL_CPU_ENABLE = 1;
    CRG_TOP->CLK_CTRL_REG_b.SYS_CLK_SEL = 3;
    rt_hw_interrupt_enable(level);

    status = wifi_hw_wait_pll_selected();
    if (status != RT_EOK)
    {
        rt_kprintf("[wifi-hw] CPU clock did not switch to PLL\n");
        wifi_hw_print_clock_state("clock switch timeout");
        return status;
    }

    (void) cm_cpu_clk_set(cpuclk_160M);

    status = wifi_hw_refresh_runtime_timing();
    if (status != RT_EOK)
    {
        return status;
    }

    rt_kprintf("[wifi-hw] CPU clock set to 160 MHz\n");
    rt_kprintf("wifi_hw_refresh_runtime_timing returned %d\n", status);
    wifi_hw_print_clock_state("after prepare");
    rt_kprintf("[wifi-hw] SystemCoreClock=%u\n", (unsigned int) SystemCoreClock);
    rt_kprintf("[wifi-hw] Wi-Fi memory is owned by RT-Thread lwIP; skip ra6w1_mem_init\n");

    return RT_EOK;
}
