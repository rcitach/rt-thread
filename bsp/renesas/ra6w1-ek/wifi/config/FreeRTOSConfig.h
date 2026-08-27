#ifndef WIFI_FREERTOS_CONFIG_H
#define WIFI_FREERTOS_CONFIG_H

#include "bsp_api.h"

/* The Wi-Fi SDK keeps its native FreeRTOS names. The wrapper maps these calls
 * to RT-Thread objects, so only SDK-specific configuration is kept here. */
#define FSP_USE_HEAP_5                      (1)
#define configAPPLICATION_ALLOCATED_HEAP    (0)
#define configSUPPORT_DYNAMIC_ALLOCATION    (1)
#define configSUPPORT_STATIC_ALLOCATION     (0)
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS (1)

#ifndef CFG_WIFI
#define CFG_WIFI                            (1)
#endif

#ifndef CFG_MBEDTLS
#define CFG_MBEDTLS                         (1)
#endif

#ifndef OS_TASK_PRIORITY_USER
#define OS_TASK_PRIORITY_USER               (tskIDLE_PRIORITY + 1)
#endif

#ifndef OS_TASK_PRIORITY_USER_MAX
#define OS_TASK_PRIORITY_USER_MAX           (configMAX_PRIORITIES - 1)
#endif

#ifndef configPRINTF
#define configPRINTF(...)                   ((void) 0)
#endif

#ifndef portCONVERT_MS_2_TICKS
#define portCONVERT_MS_2_TICKS(x)           pdMS_TO_TICKS(x)
#endif

#ifndef portCONVERT_TICKS_2_MS
#define portCONVERT_TICKS_2_MS(x)           ((uint32_t) ((((uint64_t) (x)) * 1000U) / RT_TICK_PER_SECOND))
#endif

#ifndef portEND_SWITCHING_ISR
#define portEND_SWITCHING_ISR(x)            portYIELD_FROM_ISR(x)
#endif

#endif
