#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>

#include <rtthread.h>

#include "bsp_clocks.h"
#include "sys_clock_mgr.h"
#include "FreeRTOS.h"
#include "timers.h"

#ifdef pvPortMalloc
#undef pvPortMalloc
#endif
#ifdef pvPortCalloc
#undef pvPortCalloc
#endif
#ifdef vPortFree
#undef vPortFree
#endif
#ifdef xPortGetFreeHeapSize
#undef xPortGetFreeHeapSize
#endif
#ifdef xPortGetMinimumEverFreeHeapSize
#undef xPortGetMinimumEverFreeHeapSize
#endif

/* The vendor archives use the FreeRTOS heap ABI directly.  The RT-Thread
 * wrapper maps these names for C sources, but that mapping cannot affect an
 * already-built archive. */
void * pvPortMalloc(size_t size)
{
    return rt_malloc(size);
}

void * pvPortCalloc(size_t count, size_t size)
{
    return rt_calloc(count, size);
}

void vPortFree(void * pointer)
{
    rt_free(pointer);
}

size_t xPortGetFreeHeapSize(void)
{
    rt_size_t total;
    rt_size_t used;
    rt_size_t max_used;

    rt_memory_info(&total, &used, &max_used);
    return (size_t) (total - used);
}

size_t xPortGetMinimumEverFreeHeapSize(void)
{
    return xPortGetFreeHeapSize();
}

/* Older vendor objects call the internal FreeRTOS timer entry point. */
BaseType_t xTimerGenericCommandFromTask(TimerHandle_t timer,
                                        BaseType_t command,
                                        TickType_t optional_value,
                                        BaseType_t * higher_priority_task_woken,
                                        TickType_t ticks_to_wait)
{
    return xTimerGenericCommand(timer, command, optional_value,
                                higher_priority_task_woken, ticks_to_wait);
}

/* Preserve the ABI used by the original Cortex-M FreeRTOS port. */
uint32_t ulSetInterruptMask(void)
{
    return (uint32_t) rt_hw_interrupt_disable();
}

void vClearInterruptMask(uint32_t mask)
{
    rt_hw_interrupt_enable((rt_base_t) mask);
}

void vPortYield(void)
{
    rt_thread_yield();
}

int SEGGER_RTT_vprintf(unsigned buffer_index, const char * format, va_list * arguments)
{
    char buffer[256];
    int length;

    (void) buffer_index;
    length = vsnprintf(buffer, sizeof(buffer), format, *arguments);
    if (length > 0)
    {
        rt_kprintf("%s", buffer);
    }

    return length;
}

/* The optional EVK buttons are outside this RT-Thread Wi-Fi port. */
void rm_wifi_app_gpio_config_button(void)
{
}

/* DA1640 clock-manager calls are part of the middleware API, but the actual
 * DA1640 hardware clock implementation is excluded for an RA6W1 build.  The
 * RA BSP has already configured the MCU clock, so retain the requested value
 * as the middleware-visible clock state and leave hardware switching to FSP.
 */
static cpu_clk_t s_wifi_cpu_clock = cpuclk_160M;
static sys_clk_t s_wifi_system_clock = sysclk_PLL480;

void cm_sys_clk_init(sys_clk_t type)
{
    s_wifi_system_clock = type;
}

cm_sys_clk_set_status_t cm_sys_clk_set(sys_clk_t type)
{
    s_wifi_system_clock = type;
    return cm_sysclk_success;
}

bool cm_cpu_clk_set(cpu_clk_t clock)
{
    s_wifi_cpu_clock = clock;
    return true;
}

void cm_cpu_clk_set_fromISR(sys_clk_t type, ahb_div_t divider)
{
    (void) type;
    (void) divider;
}

sys_clk_t cm_sys_clk_get(void)
{
    return s_wifi_system_clock;
}

sys_clk_t cm_sys_clk_get_fromISR(void)
{
    return s_wifi_system_clock;
}

apb_div_t cm_apb_get_clock_divider(void)
{
    return apb_div1;
}

ahb_div_t cm_ahb_get_clock_divider(void)
{
    return ahb_div1;
}

cpu_clk_t cm_cpu_clk_get(void)
{
    return s_wifi_cpu_clock;
}

cpu_clk_t cm_cpu_clk_get_fromISR(void)
{
    return s_wifi_cpu_clock;
}

void cm_halt_until_xtalm_ready(void)
{
}

bool cm_register_clock_callback(cm_clock_callback_t * callback)
{
    (void) callback;
    return true;
}

bool cm_deregister_clock_callback(cm_clock_callback_t * callback)
{
    (void) callback;
    return true;
}
