/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef RM_WIFI_EVENT_H
#define RM_WIFI_EVENT_H

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "rm_wifi_api.h"
#include "rm_wifi.h"
#include "osal.h"

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
typedef void (*rm_wifi_event_handler_t)(void *);

/**********************************************************************************************************************
 * Function declarations
 **********************************************************************************************************************/
fsp_err_t rm_wifi_event_register(int event_type, rm_wifi_event_handler_t handler);
void rm_wifi_event_handler_init(void);
void rm_wifi_event_handler_deinit(void);
void rm_wifi_event_signal_connected(u8 *ssid, u8 ssid_len, u8 *bssid, int security, u8 chan_info);
void rm_wifi_event_signal_disconnected(u16 reason);
void rm_wifi_event_signal_ap_sta_disconnected(u16 reason, u8 *ucMac);
void rm_wifi_event_signal_twt(WIFI_TWTEvent_t twt_event);

#endif // RM_WIFI_EVENT_H
