#include <stdint.h>

#include <rtthread.h>

#define DBG_TAG "wifi.hw"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#include "bsp_api.h"
#include "bsp_clocks.h"
#include "hal_data.h"
#include "sys_clock_mgr.h"

#include "wifi_hw_prepare.h"

#define WIFI_HW_CLOCK_TIMEOUT (5000000U)
#define WIFI_HW_CONSOLE_BAUD  (115200U)
#define WIFI_HW_CPU_CLOCK_HZ  (160000000U)
#define WIFI_HW_UART_CLOCK_HZ (80000000U)

static void wifi_hw_freeze_system_watchdog(void)
{
    CRG_TOP->SET_FREEZE_REG_b.FRZ_SYS_WDOG = 1;
}


static int wifi_hw_wait_xtal_ready(void)
{
    uint32_t retry;

    for (retry = 0; retry < WIFI_HW_CLOCK_TIMEOUT; retry++)
    {
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

static int wifi_hw_restore_console_baud(void)
{
    uart_w_baud_setting_t baud_setting;
    fsp_err_t             fsp_status;
    uint64_t              divisor_denominator;
    uint64_t              divisor_x64;

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
        return -RT_ERROR;
    }

    return RT_EOK;
}

static int wifi_hw_refresh_runtime_timing(void)
{
    uint32_t systick_reload;

    SystemCoreClock = WIFI_HW_CPU_CLOCK_HZ;
    systick_reload = SystemCoreClock / RT_TICK_PER_SECOND;
    if ((systick_reload == 0U) || (systick_reload > SysTick_LOAD_RELOAD_Msk))
    {
        return -RT_EINVAL;
    }

    if (wifi_hw_restore_console_baud() != RT_EOK)
    {
        return -RT_ERROR;
    }

    /* Do this after UART is repaired, because diagnostics use uart0. */
    if (SysTick_Config(systick_reload) != 0U)
    {
        return -RT_ERROR;
    }
    NVIC_SetPriority(SysTick_IRQn, 0xFF);

    return RT_EOK;
}

int wifi_hw_platform_prepare(void)
{
    int status;
    rt_base_t level;

    wifi_hw_freeze_system_watchdog();

    /* Match prvSetupHardware() in the working FSP Wi-Fi project. */
    CRG_TOP->CLK_AMBA_REG_b.PERI_CLK_ENABLE = 1;
    CRG_TOP->CLK_AMBA_REG_b.TIMER_CLK_ENABLE = 1;

    /* The working Wi-Fi path changes OQSPI to a safe divider before PLL use. */
    CRG_TOP->CLK_AMBA_REG_b.OQSPIF_DIV = 2;

    cm_sys_clk_init(sysclk_PLL480);
    bsp_clock_xtalm_enable(true);
    status = wifi_hw_wait_xtal_ready();
    if (status != RT_EOK)
        goto out;

    if (!CRG_TOP->CLK_CTRL_REG_b.RUNNING_AT_PLL)
    {
        status = wifi_hw_select_xtal_safe();
        if (status != RT_EOK)
            goto out;

        (void) hw_clk_pll_sys_on();

        status = wifi_hw_wait_pll_lock();
        if (status != RT_EOK)
            goto out;
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
        goto out;

    (void) cm_cpu_clk_set(cpuclk_160M);

    status = wifi_hw_refresh_runtime_timing();
out:
    return status;
}
