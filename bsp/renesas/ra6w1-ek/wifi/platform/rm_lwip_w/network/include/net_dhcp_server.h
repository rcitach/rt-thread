/**
 ****************************************************************************************
 *
 * @file rm_lwip_w/network/include/net_dhcp_server.h
 *
 * @brief Define for DHCP Server Module
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


#ifndef	__NET_DHCP_SERVER_H__
#define	__NET_DHCP_SERVER_H__

#include "FreeRTOS.h"
#include "lwipopts.h"

#include "sys_feature.h"
#include "iface_defs.h"
#include "common_def.h"

#ifdef DPM_PORT
#include "user_dpm.h"
#endif	/*DPM_PORT*/

#include "net_common.h"

/* Default Configuration */
#define	DHCP_SERVER_STACK_SIZE			1024
#define DHCP_SERVER_MIN_RESPONSE_DELAY	64

#define DHCP_SERVER_P2P_ULONG_START_IP	IP_ADDRESS(192,168,20,2)	// 0xC0A81402
#define DHCP_SERVER_P2P_ULONG_END_IP	IP_ADDRESS(192,168,20,11)	// 0xC0A8140B
#define DHCP_SERVER_P2P_LEASE_TIME		3600
#define DHCP_SERVER_P2P_STRING_DNS_IP	IP_ADDRESS(8,8,8,8)			// 0x08080808
#define	DHCP_MIN_LEASE_TIME				60
#define	DHCP_IP_ADDRESS_MAX_LIST_SIZE	10

//extern functions
#if defined ( __SUPPORT_ATCMD__ )
extern void PRINTF_ATCMD(const char *fmt, ...);
#endif  // __SUPPORT_ATCMD__



//
// Global functions
//
unsigned int run_thread_dhcp_server(unsigned int iface, unsigned int state, unsigned int ver);
unsigned int get_current_ip_info(const unsigned int intf);
unsigned int get_current_dns_ipaddr(ULONG *dns_ipaddr, const unsigned int intf);
unsigned int set_start_ip_address(char *value, int ver);
unsigned int set_end_ip_address(char *value, int ver);
unsigned int set_range_ip_address_list(char *start, char *end, int ver);
unsigned int get_range_ip_address_list(ULONG *start_ipaddr, ULONG *end_ipaddr);
unsigned int set_response_delay(int value);
unsigned int set_lease_time(int value, int ver);
unsigned int get_lease_time(int *value);
unsigned int set_dns_information(char *value, int ver);
unsigned int get_dns_information(ULONG *value, const unsigned int intf);

unsigned int is_dhcp_server_running(void);

void usage_dhcpd(void);
unsigned int dhcp_server_lease_table(unsigned int print_unassign);
int setDHCPServerBootStart(int flag);
int getDHCPServerBootStart(void);
unsigned int get_dhcp_server_state(unsigned int iface, unsigned int ver);

unsigned int get_current_dns_ipaddr(unsigned long *dns_ipaddr, const unsigned int iface);

#endif	/* __NET_DHCP_SERVER_H__ */

/* EOF */
