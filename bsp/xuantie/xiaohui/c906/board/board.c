/*
 * Copyright (c) 2006-2025, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-04-23     Wangshun     first version
 */

#include <board.h>
#include <rthw.h>
#include <rtthread.h>
#include <drv_usart.h>

extern unsigned long __heap_start;
extern unsigned long __heap_end;

extern unsigned int __bss_start;
extern unsigned int __bss_end;


#define RT_HEAP_SIZE        0x200
#define RT_HW_HEAP_BEGIN    ((void *)&__bss_end)
#define RT_HW_HEAP_END      ((void *)(((rt_size_t)RT_HW_HEAP_BEGIN) + RT_HEAP_SIZE ))

void init_bss(void)
{
    unsigned int *dst;

    dst = &__bss_start;
    while (dst < &__bss_end)
    {
        *dst++ = 0;
    }
}

void primary_cpu_entry(void)
{
    //初始化BSS
    init_bss();
    
    //启动RT-Thread Smart内核
    entry();
}

/**
 * This function will initialize your board.
 */
void rt_hw_board_init()
{
    rt_hw_interrupt_init();

#ifdef RT_USING_HEAP
    rt_system_heap_init(RT_HW_HEAP_BEGIN, RT_HW_HEAP_END);
#endif

#ifdef BSP_USING_UART
    rt_hw_usart_init();
#endif

#ifdef RT_USING_CONSOLE
    rt_console_set_device(RT_CONSOLE_DEVICE_NAME);
#endif

#ifdef RT_USING_COMPONENTS_INIT
    rt_components_board_init();
#endif
}
