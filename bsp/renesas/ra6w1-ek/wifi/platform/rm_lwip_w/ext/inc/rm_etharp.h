/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef LWIP_RM_ETHARP_H
#define LWIP_RM_ETHARP_H
#include "lwip/opt.h"

#if LWIP_IPV4 && LWIP_ARP /* don't build if not configured for use in lwipopts.h */

#include "lwip/etharp.h"
#include "lwip/netif.h"
#include "lwip/prot/iana.h"
#include "netif/ethernet.h"
#include "net_arp.h"

#include <string.h>

err_t dhcp_etharp_request(struct netif *netif, const ip4_addr_t *ipaddr);
err_t etharp_send(struct netif *netif, const struct eth_addr *ethsrc_addr,
            const struct eth_addr *ethdst_addr,
            const struct eth_addr *hwsrc_addr, const ip4_addr_t *ipsrc_addr,
            const struct eth_addr *hwdst_addr, const ip4_addr_t *ipdst_addr,
            const u16_t opcode);
void etharp_print_arp_table(u32_t iface);
void etharping_init(void * param);

#endif /* LWIP_IPV4 && LWIP_ARP */
#endif /* LWIP_RM_ETHARP_H */
