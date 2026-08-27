/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

// Simple LittleFS mutex helper for application code
#ifndef LFS_LOCK_H
 #define LFS_LOCK_H

 #include "FreeRTOS.h"
 #include "semphr.h"
 #include <stdint.h>

 #ifdef __cplusplus
extern "C" {
 #endif

SemaphoreHandle_t lfs_mutex_get(void);
BaseType_t        lfs_mutex_take(TickType_t ticks_to_wait);
void              lfs_mutex_give(void);

/* Legacy code sometimes checks `if (lfs_mutex)` directly; expose it here */
extern SemaphoreHandle_t lfs_mutex;

 #ifdef __cplusplus
}
 #endif

#endif                                 // LFS_LOCK_H
