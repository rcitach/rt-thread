/**
 ****************************************************************************************
 *
 * @file net_ip_handler.h
 *
 * @brief Define for IP Handler
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


#ifndef	__NET_IP_CONTROLLER_H__
#define	__NET_IP_CONTROLLER_H__

#include "iface_defs.h"
#include "common_def.h"

#ifdef DPM_PORT
#include "user_dpm.h"
#endif	/*DPM_PORT*/

#include "net_common.h"

#define NET_INFO_STR_LEN			18
#ifdef	__SUPPORT_IPV6__
  #define NET_INFO_LONG_STR_LEN		60
#endif

int		 ip_change(UINT iface, char  *ipaddress, char *netmask, char *gateway, UCHAR save);
UINT	 get_ip_info(int iface_flag, int info_flag, char *result_str);
#ifdef __SUPPORT_DHCPC_IP_TO_STATIC_IP__
int	 set_dhcpCientIP_to_staticIP(void);
#endif /* __SUPPORT_DHCPC_IP_TO_STATIC_IP__ */


#ifdef IP_STACK_PORT
VOID	_nx_ip_conflict(struct NX_IP_STRUCT *ip_ptr, NX_PACKET *packet);
#endif	/*IP_STACK_PORT*/

#if 0 //def	__SUPPORT_IPV6__
void	ipv6_auto_init(UINT intf_select, int dhcp);
int		ipv6_change(int iface, char *ipv6_str, int prefix);
#endif	/* __SUPPORT_IPV6__ */

#endif	/* __NET_IP_CONTROLLER_H__ */

/* EOF */
