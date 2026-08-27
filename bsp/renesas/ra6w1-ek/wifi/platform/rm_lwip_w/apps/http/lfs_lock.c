/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

#include "lfs_lock.h"
#include "FreeRTOS.h"
#include "semphr.h"

/* Expose a global symbol `lfs_mutex` for existing code that checks
 *  `if (lfs_mutex)` before taking/giving. Keep accessor functions
 *  for new code. */
SemaphoreHandle_t lfs_mutex = NULL;

SemaphoreHandle_t lfs_mutex_get (void)
{
    if (!lfs_mutex)
    {
        lfs_mutex = xSemaphoreCreateMutex();
    }

    return lfs_mutex;
}

BaseType_t lfs_mutex_take (TickType_t ticks_to_wait)
{
    SemaphoreHandle_t m = lfs_mutex_get();
    if (!m)
    {
        return pdFALSE;
    }

    return xSemaphoreTake(m, ticks_to_wait);
}

void lfs_mutex_give (void)
{
    SemaphoreHandle_t m = lfs_mutex_get();
    if (m)
    {
        xSemaphoreGive(m);
    }
}
