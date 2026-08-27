/**
 ****************************************************************************************
 *
 * @file net_dhcp_client.h
 *
 * @brief Define for DHCP Client Module
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


#ifndef	__NET_DHCP_CLIENT_H__
#define	__NET_DHCP_CLIENT_H__

#include "FreeRTOS.h"
#include "lwipopts.h"

#include "iface_defs.h"
#include "common_def.h"

#include "net_ip_handler.h"
#include "lwip/netif.h"
#include "lwip/prot/dhcp.h"
#include "lwip/dhcp.h"

#if CFG_PMGR
#include "sleep_mgmt_regs.h"
#include "user_dpm.h"
#endif /* CFG_PMGR */

#include "net_common.h"

#define DHCP_INFINITE_LEASE  0xFFFFFFFFUL
void rtc_dhcp_renew_timeout(char *timer_name);

int dpm_get_dhcpc_ipinfo(void);

#endif	/* __NET_DHCP_CLIENT_H__ */

/* EOF */
