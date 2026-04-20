/*
 * Copyright (c) 2011-2024, Shanghai Real-Thread Electronic Technology Co.,Ltd
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-08-29     RT-Thread    first version
 */

#include <rthw.h>
#include <rtthread.h>

int rt_hw_cpu_id(void)
{
    int cpu_id;
    __asm volatile (
            "mrc p15, 0, %0, c0, c0, 5"
            :"=r"(cpu_id)
    );
    cpu_id &= 0xf;
    return cpu_id;
}

rt_uint64_t get_main_cpu_affval(void)
{
    rt_uint32_t mpidr;

    __asm volatile (
            "mrc p15, 0, %0, c0, c0, 5"
            : "=r"(mpidr)
    );

    /*
     * GICv3 IROUTER uses Aff3:Aff2:Aff1:Aff0. On this AArch32 port we only
     * need the MPIDR affinity fields already provided by the core.
     */
    return (rt_uint64_t)(mpidr & 0x00FFFFFFU);
}

/**
 * @addtogroup ARM CPU
 */
/*@{*/

/** shutdown CPU */
void rt_hw_cpu_shutdown()
{
    rt_uint32_t level;

    rt_kprintf("shutdown...\n");

    level = rt_hw_interrupt_disable();
    while (level)
    {
        RT_ASSERT(0);
    }
}

/*@}*/
