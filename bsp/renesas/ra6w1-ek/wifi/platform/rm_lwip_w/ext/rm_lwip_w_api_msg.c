/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/*
 * rm_lwip_w_api_msg.c
 *
 *  Created on: 03-Oct-2024
 *      Author: Renesas
 */

#include "lwip/opt.h"

#if LWIP_NETCONN /* don't build if not configured for use in lwipopts.h */

#include "lwip/priv/api_msg.h"

#include "lwip/ip.h"
#include "lwip/ip_addr.h"
#include "lwip/udp.h"
#include "lwip/tcp.h"
#include "lwip/raw.h"

#ifdef RRQ61XX_CUSTOM_FIXES
#if LWIP_TCP
extern err_t (*lwip_pvt_handle_accept_function)(void *, struct tcp_pcb *, err_t);

bool rm_lwip_w_setp_tcp_server_action(void *arg)
{
  struct netconn *conn = (struct netconn *)arg;
  struct tcp_pcb *pcb = conn->pcb.tcp;

  if (pcb->state == LISTEN) {  //for tcp server
	conn->state = NETCONN_LISTEN;

	// delete the recvmbox and allocate the acceptmbox
	if (sys_mbox_valid(&conn->recvmbox)) {
	  sys_mbox_free(&conn->recvmbox);
	  sys_mbox_set_invalid(&conn->recvmbox);
	}

	// create the acceptmbox
	if (!sys_mbox_valid(&conn->acceptmbox)) {
	  if (sys_mbox_new(&conn->acceptmbox, DEFAULT_ACCEPTMBOX_SIZE) == ERR_OK) {
		  tcp_accept(pcb, lwip_pvt_handle_accept_function);
	  }
	}

	return true;
  }

  return false;
}

#endif /* LWIP_TCP */
#endif /* RRQ61XX_CUSTOM_FIXES */

#endif /* LWIP_NETCONN */
