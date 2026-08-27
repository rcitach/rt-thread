/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/


#ifndef LWIP_DPM_MISC_H
#define LWIP_DPM_MISC_H

#include "lwip/priv/sockets_priv.h"

#if CFG_PMGR
void * get_arp_table(void);
s8_t dpm_nd6_new_router(const ip6_addr_t * router_addr, struct netif * netif);
s8_t dpm_nd6_new_onlink_prefix(const ip6_addr_t *prefix, struct netif *netif);
struct lwip_sock * get_socket_dpm(int fd);
void done_socket_dpm(struct lwip_sock * sock);
#endif

#if LWIP_NETIF_EXT_STATUS_CALLBACK
#ifdef RRQ61XX_CUSTOM_FIXES_MANDATORY
void rm_netif_issue_reports(struct netif * netif, u8_t report_type);
#endif /* RRQ61XX_CUSTOM_FIXES_MANDATORY */
#endif /* LWIP_NETIF_EXT_STATUS_CALLBACK */

#endif /* LWIP_DPM_MISC_H */
