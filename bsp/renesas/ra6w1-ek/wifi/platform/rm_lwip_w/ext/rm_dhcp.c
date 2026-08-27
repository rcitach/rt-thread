/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/


#include "lwip/opt.h"

#if LWIP_IPV4 && LWIP_DHCP /* don't build if not configured for use in lwipopts.h */

#include "lwip/stats.h"
#include "lwip/mem.h"
#include "lwip/udp.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "lwip/def.h"
#include "lwip/dhcp.h"
#include "lwip/autoip.h"
#include "lwip/dns.h"
#include "lwip/etharp.h"
#include "lwip/prot/dhcp.h"
#include "lwip/prot/iana.h"

#include "lwip/dhcp.h"
#include "rm_dhcp.h"
#include "FreeRTOS.h"
#include "net_common.h"
#include "iface_defs.h"
#include "common_def.h"
#include "net_network_main.h"
#include "lwip/dns.h"

#if CFG_PMGR
#include "rm_pmgr_w_instance.h"
#endif /* CFG_PMGR */

#ifndef LWIP_DHCP_INPUT_ERROR
#define LWIP_DHCP_INPUT_ERROR(message, expression, handler) do { if (!(expression)) { \
  handler;} } while(0)
#endif

#if defined ( __SUPPORT_IPV6__ )
unsigned int ra6w1_network_main_check_global_unicast_ip6_addr(unsigned char iface);
#endif // __SUPPORT_IPV6__

#define DHCP_HOSTNAME_ONLY 1

#if DHCP_DEBUG
static u8_t debug_dhcp_client = 2; /* Debug */
#else
static u8_t debug_dhcp_client = 1; /* defaut 1 */
#endif /* DHCP_DEBUG */

static char const *dhcp_state_list[]
	= {"STOP", "REQ", "INIT", "REBOOT", "REBINDING", "RENEWING","SEL",
	   "INFO", "CHK", "PERMANENT","BOUND", "RELEASING","BACKING_OFF"};

#define DHCP_INFINITE_LEASE  0xFFFFFFFFUL

void set_debug_dhcpc(u8_t flag)
{
	debug_dhcp_client = flag;
}

u8_t get_debug_dhcpc(void)
{
	return debug_dhcp_client;
}

#if !defined (__LIGHT_SUPPLICANT__)
extern void set_supp_ip_str(char *src_str);
#endif // !(__LIGHT_SUPPLICANT__)
extern int ra6w1_network_main_get_netmode(int iface);
#ifdef __SUPPORT_DHCPC_IP_TO_STATIC_IP__
extern int ra6w1_get_temp_staticip_mode(int iface_flag);
extern int set_dhcpCientIP_to_staticIP(void);
#endif // __SUPPORT_DHCPC_IP_TO_STATIC_IP__

#ifdef __SUPPORT_DHCP_OPTION_LEASE_TIME__
static u16_t dhcp_option_lease_time(u16_t options_out_len, u8_t *options, u32 lease_time);
#endif // __SUPPORT_DHCP_OPTION_LEASE_TIME__

void debug_dhcp_msg(struct dhcp *dhcp)
{
    if (dhcp == NULL) {
        printf("no DHCP client attached yet!!!\n");
        return;
    }

#if DHCP_DEBUG
    printf("\n[DHCP struct info]\n\n");
    printf("xid=0x%lx\n", dhcp->xid);
    printf("pcb_allocated=%u\n", dhcp->pcb_allocated);
    printf("state : %s(%d)\n", dhcp_state_list[dhcp->state], dhcp->state);
    printf("tries=%u\n", dhcp->tries);
#if LWIP_DHCP_AUTOIP_COOP
    printf("autoip_coop_state=%u\n", dhcp->autoip_coop_state);
#endif
    printf("subnet_mask_given=%u\n", dhcp->subnet_mask_given);
    printf("request_timeout=%u\n", dhcp->request_timeout);
    printf("t1_timeout=%u\n", dhcp->t1_timeout);
    printf("t2_timeout=%u\n", dhcp->t2_timeout);
    printf("t1_renew_time=%u\n", dhcp->t1_renew_time);
    printf("t2_rebind_time=%u\n", dhcp->t2_rebind_time);
    printf("lease_used=%u\n", dhcp->lease_used);
    printf("t0_timeout=%u\n", dhcp->t0_timeout);

    printf("server_ip_addr=%s\n", ipaddr_ntoa(&dhcp->server_ip_addr));
    // LWIP_DHCP_PROVIDE_DNS_SERVERS
    printf("dns_ip_addr=%s\n", ipaddr_ntoa(dns_getserver(0)));
    printf("offered_ip_addr=%s\n", ipaddr_ntoa((const ip_addr_t*) &dhcp->offered_ip_addr));
    printf("offered_sn_mask=%s\n", ipaddr_ntoa((const ip_addr_t*) &dhcp->offered_sn_mask));
    printf("offered_gw_addr=%s\n", ipaddr_ntoa((const ip_addr_t*) &dhcp->offered_gw_addr));
    printf("offered_t0_lease=%lu\n", dhcp->offered_t0_lease);
    printf("offered_t1_renew=%lu\n", dhcp->offered_t1_renew);
    printf("offered_t2_rebind=%lu\n", dhcp->offered_t2_rebind);
#if LWIP_DHCP_BOOTP_FILE
    printf("offered_si_addr=%s\n", ipaddr_ntoa(&dhcp->offered_si_addr));
    printf("boot_file_name=%s\n", dhcp->boot_file_name);
#endif /* LWIP_DHCP_BOOTPFILE */
#else
    printf("\n<DHCP state : %s(%d)>\n",     dhcp_state_list[dhcp->state], dhcp->state);
    printf("\t%-19s: %5d\n",        "tries",            dhcp->tries);
    printf("\t%-19s: %5d sec\n",    "request_timeout",  dhcp->request_timeout * DHCP_COARSE_TIMER_SECS);
    printf("\t%-19s: %5d sec\n",    "t0_timeout",       dhcp->t0_timeout * DHCP_COARSE_TIMER_SECS);
    printf("\t%-19s: %5d sec\n",    "t1_timeout",       dhcp->t1_timeout * DHCP_COARSE_TIMER_SECS);
    printf("\t%-19s: %5d sec\n",    "t2_timeout",       dhcp->t2_timeout * DHCP_COARSE_TIMER_SECS);
    printf("\t%-19s: %5d sec\n",    "t1_renew_time",    dhcp->t1_renew_time * DHCP_COARSE_TIMER_SECS);
    printf("\t%-19s: %5d sec\n",    "t2_rebind_time",   dhcp->t2_rebind_time * DHCP_COARSE_TIMER_SECS);
    printf("\t%-19s: %5d sec\n\n",  "lease_used",       dhcp->lease_used * DHCP_COARSE_TIMER_SECS);
#endif /* DHCP_DEBUG */ 
}

void debug_dhcp_info(const struct netif *netif)
{
    struct dhcp *dhcp = netif_dhcp_data(netif);
    debug_dhcp_msg(dhcp);
}

/** Get current DHCP informations
 *
 * @param netif the netif to check
 * @return timeout_value
 */
u32_t
dhcp_get_timeout_value(const struct netif *netif, int time_type)
{
	struct dhcp *dhcp = netif_dhcp_data(netif);
	u32_t	time_val;

	switch (time_type) {
		case REQUEST_TIMEOUT	:
  			time_val = (u32_t)dhcp->request_timeout * DHCP_COARSE_TIMER_SECS;
			break;

		case T0_TIMEOUT			:
  			time_val = (u32_t)dhcp->t0_timeout * DHCP_COARSE_TIMER_SECS;
			break;

		case T1_TIMEOUT			:
  			time_val = (u32_t)dhcp->t1_timeout * DHCP_COARSE_TIMER_SECS;
			break;

		case T2_TIMEOUT			:
  			time_val = (u32_t)dhcp->t2_timeout * DHCP_COARSE_TIMER_SECS;
			break;

		case T1_RENEW_TIME		:
  			time_val = (u32_t)dhcp->t1_renew_time * DHCP_COARSE_TIMER_SECS;
			break;

		case T1_REBIND_TIME		:
  			time_val = (u32_t)dhcp->t2_rebind_time * DHCP_COARSE_TIMER_SECS;
			break;

		case LEASE_USED			:
  			time_val = (u32_t)dhcp->lease_used * DHCP_COARSE_TIMER_SECS;
			break;

		case OFFERED_T0_LEASE	:
			time_val = dhcp->offered_t0_lease * DHCP_COARSE_TIMER_SECS; /* lease period (in seconds) */
			break;

		case OFFERED_T1_RENEW	:
			time_val = dhcp->offered_t1_renew * DHCP_COARSE_TIMER_SECS; /* recommended renew time (usually 50% of lease period) */
			break;

		case OFFERED_T2_REBIND	:
			time_val = dhcp->offered_t2_rebind * DHCP_COARSE_TIMER_SECS; /* recommended rebind time (usually 87.5 of lease period)  */
			break;

		default :
			printf("[%s] type flag error (0x%d)\n", __func__, time_type);
			time_val = 0xffffffff;
	}	

	return time_val;
}

#if DHCP_DOES_ARP_CHECK
/**
 * Match an ARP reply with the offered IP address:
 * check whether the offered IP address is not in use using ARP
 *
 * @param netif the network interface on which the reply was received
 * @param addr The IP address we received a reply from
 * @param ethaddr The hw address we received a reply from
 */
void
rm_dhcp_arp_reply(struct netif *netif, const ip4_addr_t *addr, struct eth_addr *ethaddr)
{
  struct dhcp *dhcp;

  LWIP_ERROR("netif != NULL", (netif != NULL), return;);
  dhcp = netif_dhcp_data(netif);
  LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE, ("dhcp_arp_reply()\n"));
  /* is a DHCP client doing an ARP check? */
  /* In case of delayed response of ARP Reply, check also when DHCP Client State is Bound. */
  if ((dhcp != NULL) && (dhcp->state == DHCP_STATE_CHECKING || dhcp->state == DHCP_STATE_BOUND))
 {
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE, ("dhcp_arp_reply(): CHECKING, arp reply for 0x%08"X32_F"\n",
                ip4_addr_get_u32(addr)));
    /* did a host respond with the address we
       were offered by the DHCP server? */
    if (ip4_addr_cmp(addr, &dhcp->offered_ip_addr)) {
      /* we will not accept the offered address */
      LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE | LWIP_DBG_LEVEL_WARNING,
                  ("dhcp_arp_reply(): arp reply matched with offered address, declining\n"));

      printf(">>> Duplicate IP Address detected from %02x:%02x:%02x:%02x:%02x:%02x (%s)\n",
              (u16_t)ethaddr->addr[0], (u16_t)ethaddr->addr[1], (u16_t)ethaddr->addr[2],
              (u16_t)ethaddr->addr[3], (u16_t)ethaddr->addr[4], (u16_t)ethaddr->addr[5],
              ipaddr_ntoa((ip_addr_t *)addr));
      /* Set DHCP state to CHECKING because dhcp_arp_reply does not handle DHCP_STATE_BOUND state */
      dhcp->state = DHCP_STATE_CHECKING;
      dhcp_arp_reply(netif, addr);
    }
  }
#ifdef __SUPPORT_DHCPC_IP_TO_STATIC_IP__
  else if (ip4_addr_cmp(addr, netif_ip4_addr(netif)))
  {
    if (ra6w1_get_temp_staticip_mode(WLAN0_IFACE) == pdTRUE)
    {
      printf(">>> IP Address Collision from %02x:%02x:%02x:%02x:%02x:%02x (%s)\n",
              (u16_t)ethaddr->addr[0], (u16_t)ethaddr->addr[1], (u16_t)ethaddr->addr[2],
              (u16_t)ethaddr->addr[3], (u16_t)ethaddr->addr[4], (u16_t)ethaddr->addr[5],
              ipaddr_ntoa((ip_addr_t *)addr));

      printf(">>> Start DHCP_Client (Temp StaticIP)\n");
      set_netmode(WLAN0_IFACE, DHCPCLIENT, pdTRUE);
	  dhcp_start(netif);
    }
  }
#endif // __SUPPORT_DHCPC_IP_TO_STATIC_IP__
}
#endif /* DHCP_DOES_ARP_CHECK */

#endif /* LWIP_IPV4 && LWIP_DHCP */
