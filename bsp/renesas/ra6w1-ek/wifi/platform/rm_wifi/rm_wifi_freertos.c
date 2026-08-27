/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "rm_wifi.h"
#include "FreeRTOSConfig.h"

#if configUSE_IDLE_HOOK

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @brief Application idle task hook for Wi-Fi
 **********************************************************************************************************************/
void vApplicationIdleHookSub(void)
{
 #if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
    g_wifi_cfg.p_watchdog_service->p_api->notifyFromIdle(g_wifi_cfg.p_watchdog_service->p_ctrl,
                                                             g_wifi_cfg.p_watchdog_service->p_cfg);
 #endif
}

#endif
