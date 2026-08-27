/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef LWIP_RM_TCP_H
#define LWIP_RM_TCP_H

#include "lwip/opt.h"
#include "lwip/tcp.h"
#include "lwip/priv/tcp_priv.h"
#include "lwip/debug.h"

#include "rm_lwip_w_hooks.h"


void tcp_abandon_remote_ip(const ip_addr_t * addr);

#endif /* LWIP_RM_TCP_H */
