/**
 ****************************************************************************************
 *
 * @file net_wifi_monitor.h
 *
 * @brief Define for Wi-Fi status monitor Module
 *
 * Copyright (c) 2023-2024 Renesas Electronics. All rights reserved.
 *
 * This software ("Software") is owned by Renesas Electronics.
 *
 * By using this Software you agree that Renesas Electronics retains all
 * intellectual property and proprietary rights in and to this Software and any
 * use, reproduction, disclosure or distribution of the Software without express
 * written permission or a license agreement from Renesas Electronics is
 * strictly prohibited. This Software is solely for use on or in conjunction
 * with Renesas Electronics products.
 *
 * EXCEPT AS OTHERWISE PROVIDED IN A LICENSE AGREEMENT BETWEEN THE PARTIES, THE
 * SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. EXCEPT AS OTHERWISE
 * PROVIDED IN A LICENSE AGREEMENT BETWEEN THE PARTIES, IN NO EVENT SHALL
 * RENESAS ELECTRONICS BE LIABLE FOR ANY DIRECT, SPECIAL, INDIRECT, INCIDENTAL,
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THE SOFTWARE.
 *
 ****************************************************************************************
 */


#ifndef	__RA6WX_WIFI_MONITOR_H__
#define	__RA6WX_WIFI_MONITOR_H__

#include "FreeRTOS.h"
#include "lwipopts.h"
#include "iface_defs.h"

#include "net_common.h"

#define	WIFI_MONITOR_STACK_SIZE			(128*3)		/* 384 * sizeof(int) */
#define WIFI_MONITOR_THREAD_PRIORITY	17

/* Queue Size : 128, Msg size : 64 bytes , Max num : 2msgs */
#define	WIFI_MONITOR_QUEUE_SIZE		128

BaseType_t start_wifi_monitor(void);

#endif	/* __RA6WX_WIFI_MONITOR_H__ */

/* EOF */
