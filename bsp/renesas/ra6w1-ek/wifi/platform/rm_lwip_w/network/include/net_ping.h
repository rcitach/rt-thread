/**
 ****************************************************************************************
 *
 * @file rm_lwip_w/network/include/net_ping.h
 *
 * @brief Define for PING Client
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


#ifndef	__NET_PING_H__
#define	__NET_PING_H__

#include "FreeRTOS.h"
#include "lwipopts.h"

#include "iface_defs.h"

#include "net_network_main.h"
#include "net_dns_client.h"
#if !defined ( __SUPPORT_IPV6__ )
#include "net_arp.h"
#endif // __SUPPORT_IPV6__

#include "ip_addr.h"

#undef RA6WX_PING_DEBUG

#ifdef RA6WX_PING_DEBUG
  #define PING_DEBUG_PRINT(...)	printf(__VA_ARGS__)
#else
  #define PING_DEBUG_PRINT(...)
#endif	/*RA6WX_PING_DEBUG*/

#define PING_DATA		"ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890-"
#define PING_DATA_LEN		27 /* (sizeof(PING_DATA)-1) */
#define PING_DOMAIN_LEN		64

#ifdef	__SUPPORT_SIGMA_TEST__
  #define MAX_PING_SIZE		10000
#else
  #define MAX_PING_SIZE		2048
#endif	/* __SUPPORT_SIGMA_TEST__ */

#define DEFAULT_PING_SIZE	32
#define DEFAULT_PING_WAIT	4000 /* Default time-out is 4000 (4 seconds). */
#define DEFAULT_PING_COUNT	4

#define MIN_INTERVAL		10
#define DEFAULT_INTERVAL	1000 /* ms */

#define POOL_USE_SIZE		1480

/* Define transmit packet pool. */
#define	ICMP_PKT_PAYLOAD_SZ			1536
#define	ICMP_PKT_PAYLOAD_SZ_IPV6	1556

#define ICMP_PKT_POOL_SZ		((ICMP_PKT_PAYLOAD_SZ + sizeof(NX_PACKET)) * 8)
#define ICMP_PKT_POOL_SZ_IPV6	((ICMP_PKT_PAYLOAD_SZ_IPV6 + sizeof(NX_PACKET)) * 8)


bool cmd_ping_client(int argc, char *argv[]);
#if !defined ( __SUPPORT_IPV6__ )
bool cmd_arping(int argc, char *argv[]);
#endif // __SUPPORT_IPV6__
void ping_dns_found(const char* domain, const ip_addr_t *ipaddr, void *arg);
unsigned int ra6w1_ping_client(int iface,
                            char *domain_str,
                            unsigned long ipaddr,
                              unsigned long *ipv6dst,
                            unsigned long *ipv6src,
                            int len,
                            uint64_t max_count,
                            int wait,
                            int interval,
                            int nodisplay,
                            char *ping_result);

struct cmd_ping_param{
	unsigned int	status;
	char	        domain_str[PING_DOMAIN_LEN + 1];
	ip_addr_t       ipaddr;
	uint64_t	    max_count;
	unsigned int	len;
	unsigned int	wait;
	unsigned int	interval;
	int		        ping_interface;
	unsigned int	dns_query_wait_option;
#ifdef	__SUPPORT_IPV6__
	int 	        is_v6;
	int 	        is_ipv6;
	unsigned long	ipv6_dst[4];
	unsigned long	ipv6_src[4];
#endif	/* __SUPPORT_IPV6__ */
    char no_display;
};

#endif	/* __NET_PING_H__ */

/* EOF */
