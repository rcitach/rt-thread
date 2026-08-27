/* generated configuration header file - do not edit */
#ifndef RM_STDIO_W_CFG_H_
#define RM_STDIO_W_CFG_H_
#ifdef __cplusplus
extern "C" {
#endif
#define CONFIG_RETARGET
#define CONFIG_RA6W1_PRINTF
#define CONFIG_RTT

#ifndef dg_configUSE_CONSOLE
#define dg_configUSE_CONSOLE   1
#endif

#ifndef dg_configUSE_UART2
#define dg_configUSE_UART2   1
#endif

#ifndef dg_configUSE_UART3
#define dg_configUSE_UART3   0  
#endif

#if (dg_configUSE_CONSOLE == 1)
#define CONFIG_CONSOLE_RINGBUF_SIZE             ( 2048 )

#ifndef dg_configUART_ADAPTER
#define dg_configUART_ADAPTER                   ( 1 )
#endif

#ifndef dg_configUART_SOFTWARE_FIFO
#define dg_configUART_SOFTWARE_FIFO             ( 1 )
#endif

#ifndef dg_configUART1_SOFTWARE_FIFO_SIZE
#define dg_configUART1_SOFTWARE_FIFO_SIZE       ( 160*2 )
#endif
#endif

#if (dg_configUSE_UART2 == 1 || dg_configUSE_UART3 == 1)
#ifndef dg_configUART_SOFTWARE_FIFO 
       #define dg_configUART_SOFTWARE_FIFO         ( 1 )
   #endif 

#ifndef dg_configUART2_SOFTWARE_FIFO_SIZE
#define dg_configUART2_SOFTWARE_FIFO_SIZE       ( 160*2 )
#endif

#ifndef dg_configUART3_SOFTWARE_FIFO_SIZE
#define dg_configUART3_SOFTWARE_FIFO_SIZE       ( 160*2 )
#endif
#endif 

#ifdef __cplusplus
}
#endif
#endif /* RM_STDIO_W_CFG_H_ */
