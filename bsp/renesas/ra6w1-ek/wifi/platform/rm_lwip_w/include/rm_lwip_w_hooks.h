/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/*
 * rm_lwip_w_hooks.h
 *
 *  Created on: 26-Sep-2024
 *      Author: Renesas
 */

#ifndef RRQ_FSP_SRC_RM_LWIP_W_INCLUDE_RM_LWIP_W_HOOKS_H_
#define RRQ_FSP_SRC_RM_LWIP_W_INCLUDE_RM_LWIP_W_HOOKS_H_


/*******************************************************/
/* Clean: sockets.c sockets.h                          */
/*******************************************************/
#include "lwip/api.h"

#if CFG_PMGR
int lwip_close_dpm(struct netconn *newconn);
int lwip_socket_dpm(char *name, int domain, int type, int protocol);
int alloc_socket_dpm(struct netconn *newconn, int accepted);
struct tcp_pcb * tcp_alloc_dpm(char *name, u8_t prio);
struct lwip_sock * get_lwip_sockets_pvt();
#endif /* CFG_PMGR */
#define TCP_GEN_CLI_NAME(name, idx, buf, buflen) \
    snprintf(buf, buflen, "%s_%d", name, idx);

#if CFG_PMGR
int tcp_reg_cli_name(void *arg, char *buf, size_t buflen);
int tcp_dereg_cli_name(void *arg, char *buf, size_t buflen);
int socket_dpm(char *name, int domain, int type, int protocol);
#endif /* CFG_PMGR */

int lwip_bind_if(int s, u8_t if_idx);

#if CFG_PMGR
#define socket_dpm(name,domain,type,protocol)     lwip_socket_dpm(name,domain,type,protocol)

extern int RM_PMGR_W_dpm_is_enabled(void);
extern int RM_PMGR_W_dpm_rcv_ready_set_by_port(unsigned int port);

#ifdef __SUPPORT_DPM_TCP_KEEPALIVE__
err_t lwip_send_tcp_keepalive_dpm(int s);
#define send_tcp_keepalive_dpm(s)                 lwip_send_tcp_keepalive_dpm(s)
#endif  // __SUPPORT_DPM_TCP_KEEPALIVE__
#endif /* CFG_PMGR */

#if LWIP_SOCKET
#if LWIP_COMPAT_SOCKETS
#define bind_if(s,if_idx)                         lwip_bind_if(s,if_idx);
#endif /* LWIP_COMPAT_SOCKETS */
#else
#define AF_UNSPEC       0
#define AF_INET         2
#if LWIP_IPV6
#define AF_INET6        10
#else /* LWIP_IPV6 */
#define AF_INET6        AF_UNSPEC
#endif /* LWIP_IPV6 */
#endif /* LWIP_SOCKET */

extern struct lwip_sock_name *socket_names;

/*******************************************************/
/*******************************************************/

#endif /* RRQ_FSP_SRC_RM_LWIP_W_INCLUDE_RM_LWIP_W_HOOKS_H_ */
