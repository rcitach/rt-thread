/**
 ****************************************************************************************
 *
 * @file net_common.c
 *
 * @brief Common defines, structures and variable declaration
 *
 * Copyright (c) 2023 Renesas Electronics. All rights reserved.
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
#include "net_common.h"
#include "net_network_main.h"

UINT	discovery_ip_state = 0;
ULONG	gateway_ip_addr = 0;

void ra6w1_network_common_set_discovery_ip_state(UINT state)
{
	discovery_ip_state = state;
}

UINT ra6w1_network_common_get_discovery_ip_state(void)
{
	return discovery_ip_state;
}

void ra6w1_network_common_set_gateway_ip_addr(ULONG ip_addr)
{
	gateway_ip_addr = ip_addr;
}

ULONG ra6w1_network_common_get_gateway_ip_addr(void)
{
	return gateway_ip_addr;
}

int check_net_init(int iface)
{
	return ra6w1_network_main_check_net_init(iface);
}

int check_net_ip_status(int iface)
{
	return ra6w1_network_main_check_net_ip_status(iface);
}

#if defined (__SUPPORT_IPV6__)		// UDP_DPM_TEST
int check_net_ipv6_status(int iface)
{
	return ra6w1_network_main_check_net_ipv6_status(iface);
}
#endif	// __SUPPORT_IPV6__

UINT chk_ipaddress(UCHAR iface)
{
	return ra6w1_network_main_check_ip_addr(iface);
}

UINT chk_network_ready(UCHAR iface)
{
	return ra6w1_network_main_check_network_ready(iface);
}

void ra6w1_net_check(int iface_flag, int simple)
{
	ra6w1_network_main_check_network(iface_flag, simple);
}

int get_netmode(int iface)
{
	return ra6w1_network_main_get_netmode(iface);
}

UINT set_netmode(UCHAR iface, UCHAR mode, UCHAR save)
{
	return ra6w1_network_main_set_netmode(iface, mode, save);
}

int get_sys_mode(void)
{
	return ra6w1_network_main_get_sysmode();
}

int set_sys_mode(int mode)
{
	return ra6w1_network_main_set_sysmode(mode);
}

void iface_updown(UINT iface, UINT flag)
{
	ra6w1_network_main_change_iface_updown(iface, flag);
}

void reconfig_net(int state)
{
	ra6w1_network_main_reconfig_net(state);
}

#ifdef	__TICKTIME__
time_t tick_time(time_t *p)
{
	return ra6w1_network_main_get_ticktime(p);
}
#endif	/* __TICKTIME__ */

long get_time_zone(void)
{
	return ra6w1_network_main_get_timezone();
}

unsigned long set_time_zone(long timezone)
{
	return ra6w1_network_main_set_timezone(timezone);
}

/* EOF */
