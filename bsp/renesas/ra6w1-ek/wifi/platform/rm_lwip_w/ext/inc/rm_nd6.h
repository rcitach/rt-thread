/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef LWIP_RM_ND6_H
#define LWIP_RM_ND6_H

#include "lwip/opt.h"

#if LWIP_IPV6

#include "lwip/netif.h"


void rm_nd6_tmr(void);
char * ndp_state(char state);
void nd6_display_neighbor_cache_netif(struct netif * netif);

#endif /* LWIP_IPV6 */

#endif /* LWIP_RM_ND6_H */
