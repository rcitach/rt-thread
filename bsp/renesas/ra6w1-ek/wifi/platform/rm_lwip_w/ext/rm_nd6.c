/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include "rm_lwip_w_cfg.h" /* For RM_LWIP_W_CLEANED */
#include "lwip/opt.h"

#if LWIP_IPV6  /* don't build if not configured for use in lwipopts.h */

#include "lwip/prot/ethernet.h"
#include "lwip/prot/ip.h"
#include "lwip/prot/ip6.h"

#include "lwip/netif.h"
#include "lwip/nd6.h"
#include "lwip/priv/nd6_priv.h"
#include "lwip/prot/nd6.h"
#include "lwip/prot/icmp6.h"
#include "lwip/pbuf.h"
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/ip6.h"
#include "lwip/ip6_addr.h"
#include "lwip/inet_chksum.h"
#include "lwip/icmp6.h"
#include "lwip/mld6.h"
#include "lwip/dhcp6.h"
#include "lwip/ip.h"
#include "lwip/stats.h"
#include "lwip/dns.h"
#include "rm_nd6.h"

#include <string.h>

/* Default values, can be updated by a RA message. */
extern u32_t reachable_time;
extern u32_t retrans_timer; /* @todo implement this value in timer */

#define RM_ND6_SEND_FLAG_ALLNODES_DEST 0x02
/** Router Advertisement are sent in Max intervals (see RFC 4861, ch. 10) */
#define MAX_RTR_ADVERT_INTERVAL 1800 // 4~1800
#define MIN_RTR_ADVERT_INTERVAL 3	 // 3 ~ MAX_RTR_ADVERT_INTERVAL * 0.74
#define INITIAL_RTR_ADVERT_INTERVAL 600

static u16_t nd6_tmr_ra_reduction;
/* Multicast address holder. */
static ip6_addr_t multicast_address;

#if LWIP_IPV6_SEND_ROUTER_ADVERT
static void nd6_send_ra(struct netif *netif, u8_t flags);
#endif /* LWIP_IPV6_SEND_ROUTER_ADVERT */
extern int get_run_mode(void);
extern void print_separate_bar(unsigned char text, unsigned char loop_count, unsigned char CR_loop_count);

#if LWIP_IPV6_SEND_ROUTER_ADVERT

#define SET_ND6_RA_PREFIX_OPTION 0
#define SET_ND6_RA_RDNSS_OPTION 0
/**
 * Send a router advertisement message
 *
 * @param netif the netif on which to send the message
 * @param flags one of ND6_SEND_FLAG_*
 */
static void
nd6_send_ra(struct netif *netif, u8_t flags)
{
  struct ra_header *ra_hdr;
  struct lladdr_option *lladdr_opt;
#if SET_ND6_RA_PREFIX_OPTION
  struct prefix_option *prefix_opt;
#endif
#if SET_ND6_RA_RDNSS_OPTION
  struct rdnss_option *dns_server_opt;
#endif

  struct pbuf *p;
  const ip6_addr_t *src_addr;
  const ip6_addr_t *dest_addr;
  u16_t lladdr_opt_len;

  /* Use link-local address as source address. */
  src_addr = netif_ip6_addr(netif, 0);

  /* Allocate a packet. */
  lladdr_opt_len = ((netif->hwaddr_len + 2) >> 3) + (((netif->hwaddr_len + 2) & 0x07) ? 1 : 0);

  p = pbuf_alloc(PBUF_IP, sizeof(struct ra_header)
					  + (lladdr_opt_len << 3)		  //ICMPv6 Option (Source link-layer address)
#if SET_ND6_RA_PREFIX_OPTION
					  + sizeof(struct prefix_option)  //ICMPv6 Option (Prefix information)
#endif /* SET_PREFIX_OPTION */
#if SET_ND6_RA_RDNSS_OPTION
					  + sizeof(struct rdnss_option)   //ICMPv6 Option (Recursive DNS Server)
#endif /* SET_RDNSS_OPTION */
					  , PBUF_RAM);


  if (p == NULL) {
    ND6_STATS_INC(nd6.memerr);
    return;
  }

  /* Set fields. */
  ra_hdr = (struct ra_header *)p->payload;
  ra_hdr->type = ICMP6_TYPE_RA;
  ra_hdr->code = 0;
  ra_hdr->chksum = 0;
  ra_hdr->flags = (u8_t)(!ND6_RA_FLAG_MANAGED_ADDR_CONFIG	// Statefull (DHCPv6) On/Off
  					   | !ND6_RA_FLAG_OTHER_CONFIG			// Stateless (DHCPv6) On/Off
  					   | !ND6_RA_FLAG_HOME_AGENT
  					   | ND6_RA_PREFERENCE_MEDIUM);
  ra_hdr->router_lifetime = PP_HTONS(1800);
  ra_hdr->reachable_time = PP_HTONL(0);
  ra_hdr->retrans_timer = PP_HTONL(0);

  //ICMPv6 Option (Source link-layer address)
  lladdr_opt = (struct lladdr_option *)((u8_t*)p->payload + sizeof(struct ra_header));
  lladdr_opt->type = ND6_OPTION_TYPE_SOURCE_LLADDR;
  lladdr_opt->length = (u8_t)lladdr_opt_len;
  SMEMCPY(lladdr_opt->addr, netif->hwaddr, netif->hwaddr_len);
  
#if SET_ND6_RA_PREFIX_OPTION
  //ICMPv6 Option (Prefix information)
  prefix_opt = (struct prefix_option *)((u8_t*)lladdr_opt + sizeof(struct lladdr_option));
  prefix_opt->type = ND6_OPTION_TYPE_PREFIX_INFO;
  prefix_opt->length = (u8_t)4; //32 Bytes;
  prefix_opt->prefix_length = (u8_t)0x40; // 64
  prefix_opt->flags = (u8_t)(ND6_PREFIX_FLAG_AUTONOMOUS // for SLAAC
  							| ND6_PREFIX_FLAG_ON_LINK);

  prefix_opt->valid_lifetime = PP_NTOHL(2592000);
  prefix_opt->preferred_lifetime = PP_NTOHL(604800);
  prefix_opt->site_prefix_length = 0;

  //Temp: 2001:2:0:aab1::
  prefix_opt->prefix.addr[0] = PP_HTONL(0x20010002);
  prefix_opt->prefix.addr[1] = PP_HTONL(0x0000aab1);
  prefix_opt->prefix.addr[2] = PP_HTONL(0x00000000);
  prefix_opt->prefix.addr[3] = PP_HTONL(0x00000000);
#endif /* SET_PREFIX_OPTION */
	
#if SET_ND6_RA_RDNSS_OPTION
  //ICMPv6 Option (Recursive DNS Server)
  dns_server_opt = (struct rdnss_option *)((u8_t*)prefix_opt + sizeof(struct prefix_option));
  dns_server_opt->type = ND6_OPTION_TYPE_RDNSS;
  dns_server_opt->length = (u8_t)3; //24 Bytes
  dns_server_opt->reserved = 0;
  dns_server_opt->lifetime = PP_HTONL(270);
  //Temp: 2001:2:0:aab1::1
  dns_server_opt->rdnss_address->addr[0] = PP_HTONL(0x20010002);
  dns_server_opt->rdnss_address->addr[1] = PP_HTONL(0x0000aab1);
  dns_server_opt->rdnss_address->addr[2] = PP_HTONL(0x00000000);
  dns_server_opt->rdnss_address->addr[3] = PP_HTONL(0x00000001);
#endif /* SET_RDNSS_OPTION */

  /* Generate the solicited node address for the target address. */
  if (flags & RM_ND6_SEND_FLAG_ALLNODES_DEST) {
    ip6_addr_set_allnodes_linklocal(&multicast_address);
    ip6_addr_assign_zone(&multicast_address, IP6_MULTICAST, netif);
    dest_addr = &multicast_address;
  } else {
    dest_addr = ip6_current_src_addr();
  }

#if CHECKSUM_GEN_ICMP6
  IF__NETIF_CHECKSUM_ENABLED(netif, NETIF_CHECKSUM_GEN_ICMP6) {
    ra_hdr->chksum = ip6_chksum_pseudo(p, IP6_NEXTH_ICMP6, p->len, src_addr,
      dest_addr);
  }
#endif /* CHECKSUM_GEN_ICMP6 */

  /* Send the packet out. */
  ND6_STATS_INC(nd6.xmit);
  ip6_output_if(p, src_addr, dest_addr,
      ND6_HOPLIM, 0, IP6_NEXTH_ICMP6, netif);
  pbuf_free(p);
}
#endif /* LWIP_IPV6_SEND_ROUTER_ADVERT */

void rm_nd6_tmr(void)
{
  struct netif *netif;

  nd6_tmr();
#if LWIP_IPV6_SEND_ROUTER_ADVERT
#if CFG_WIFI
  // Add by Renesas */
  #include "common_def.h"
  /* Send router advertisement messages, if necessary. rfc4861#section-6.2.4 */
  if (get_run_mode() == WIFI_DEVICE_MODE_EXT_AP) // SoftAP Only
  {
#endif /* CFG_WIFI */
      if (!nd6_tmr_ra_reduction) {
        nd6_tmr_ra_reduction = (rand() % MAX_RTR_ADVERT_INTERVAL) - 1;
        if (nd6_tmr_ra_reduction < MIN_RTR_ADVERT_INTERVAL || nd6_tmr_ra_reduction > MAX_RTR_ADVERT_INTERVAL)
        {
            nd6_tmr_ra_reduction = INITIAL_RTR_ADVERT_INTERVAL - 1;
        }

        NETIF_FOREACH(netif) {
          if (netif_is_up(netif) &&
              netif_is_link_up(netif) &&
              !ip6_addr_isinvalid(netif_ip6_addr_state(netif, 0)) &&
              !ip6_addr_isduplicated(netif_ip6_addr_state(netif, 0))) {
              nd6_send_ra(netif, RM_ND6_SEND_FLAG_ALLNODES_DEST);
          }
        }
      } else {
        nd6_tmr_ra_reduction--;
      }
#if CFG_WIFI
  }
#endif /* CFG_WIFI */
#endif /* LWIP_IPV6_SEND_ROUTER_ADVERT */
}

/**
 * Find the cached entry for default gateway(router)
 *
 * @return default gateway address.
 * add by renesas
 */
char* nd6_get_default_gateway(void)
{
  s8_t router_index;
  
  for (router_index = 0; router_index < LWIP_ND6_NUM_ROUTERS; router_index++) {
    if (default_router_list[router_index].neighbor_entry) {
  	return ip6addr_ntoa(&default_router_list[router_index].neighbor_entry->next_hop_address);
    }
  }

  return NULL;
}

char* ndp_state(char state)
{
  switch(state)
  {
    case ND6_NO_ENTRY:
        return "NO_ENTRY";

    case ND6_INCOMPLETE:
        return "INCOMPLETE";

    case ND6_REACHABLE:
        return "REACHABLE";

    case ND6_STALE:
        return "STALE";

    case ND6_DELAY:
        return "DELAY";

    case ND6_PROBE:
        return "PROBE";

    default:
        return "";
  }
}

/**
 * Display all neighbor_cache of the specified netif.
 *
 * @param netif points to a network interface
 */
void nd6_display_neighbor_cache_netif(struct netif *netif)
{
  u8_t i;
  s8_t router_index;

  printf("%-39s %-17s ", "IPv6 Address", "MAC Address");
  printf("%-10s %-4s %6s %6s%6s\n", "State", "Time", "Router", "RTTime", " RTFlag");
  print_separate_bar('=', 94, 1);

  for (i = 0; i < LWIP_ND6_NUM_NEIGHBORS; i++) {
    if (neighbor_cache[i].netif == netif) {
      printf("%-39s %02X:%02X:%02X:%02X:%02X:%02X %-10s ",
           ip6addr_ntoa(&neighbor_cache[i].next_hop_address),
           neighbor_cache[i].lladdr[0], neighbor_cache[i].lladdr[1], neighbor_cache[i].lladdr[2],
           neighbor_cache[i].lladdr[3], neighbor_cache[i].lladdr[4], neighbor_cache[i].lladdr[5],
           ndp_state(neighbor_cache[i].state));

       // State
       switch(neighbor_cache[i].state)
       {
          case ND6_REACHABLE:
            printf("%4ld", neighbor_cache[i].counter.reachable_time/ND6_TMR_INTERVAL);
            break;

          case ND6_STALE:
            printf("%4ld", neighbor_cache[i].counter.stale_time);
            break;

          case ND6_DELAY:
            printf("%4ld", neighbor_cache[i].counter.delay_time);
            break;

          case ND6_PROBE:
            printf("%4ld", neighbor_cache[i].counter.probes_sent);
            break;

          default:
            printf("    ");     
        }

        // router info
        printf(" %6d", neighbor_cache[i].isrouter);
        for (router_index = 0; router_index < LWIP_ND6_NUM_ROUTERS; router_index++) {
          if (default_router_list[router_index].neighbor_entry == &neighbor_cache[i]) {
            printf(" %6ld %6d", default_router_list[router_index].invalidation_timer,
          		  default_router_list[router_index].flags);
          }
        }
        printf("\n");
    }
  }

  print_separate_bar('-', 94, 1);
}
#endif /* LWIP_IPV6 */
