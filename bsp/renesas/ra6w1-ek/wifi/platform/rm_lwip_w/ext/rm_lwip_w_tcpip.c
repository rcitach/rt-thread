/*
 * rm_lwip_w_tcpip.c
 *
 *  Created on: 03-Oct-2024
 *      Author: Renesas
 */

#include "lwip/opt.h"

#ifdef RRQ61XX_CUSTOM_FIXES
#if !NO_SYS /* don't build if not configured for use in lwipopts.h */

#if defined(RRQ61XX_CUSTOM_FIXES) && defined(TCP_OUT_MUTEX)
sys_mutex_t lock_tcp_output;
sys_mutex_t lock_tcp_writemore;
#endif /* RRQ61XX_CUSTOM_FIXES && TCP_OUT_MUTEX */

#if defined(RRQ61XX_CUSTOM_FIXES) && defined(TCP_MULTI_SESS_PCB_MUTEX)
/* Case of TCP Multi session, for blocking hangup issue,
 * Lock processing of PCB Generation and deletion
 */
sys_mutex_t lock_tcp_multi_pcb;
#endif /* RRQ61XX_CUSTOM_FIXES && TCP_MULTI_SESS_PCB_MUTEX */

void rm_lwip_w_tcpip_init_action(void)
{
#if defined(RRQ61XX_CUSTOM_FIXES) && defined(TCP_OUT_MUTEX)
  /* Create lock_tcp_output */
  if (sys_mutex_new(&lock_tcp_output) != ERR_OK)
  {
    LWIP_ASSERT("failed to create lock_tcp_output", 0);
  }
  else
  {
    printf("[%s] Create lock_tcp_output\n", __func__);
  }

  /* Create lock_tcp_writemore */
  if (sys_mutex_new(&lock_tcp_writemore) != ERR_OK)
  {
    LWIP_ASSERT("failed to create lock_tcp_writemore", 0);
  }
  else
  {
    printf("[%s] Create lock_tcp_writemore\n", __func__);
  }
#endif /* RRQ61XX_CUSTOM_FIXES && TCP_OUT_MUTEX */

#if defined(RRQ61XX_CUSTOM_FIXES) && defined(TCP_MULTI_SESS_PCB_MUTEX)
  /* Lock processing of PCB Generation and deletion */
  if (sys_mutex_new(&lock_tcp_multi_pcb) != ERR_OK)
  {
	LWIP_ASSERT("failed to create lock_tcp_output", 0);
  }
#endif /* RRQ61XX_CUSTOM_FIXES && TCP_MULTI_SESS_PCB_MUTEX */
}

#endif /* !NO_SYS */
#endif /* RRQ61XX_CUSTOM_FIXES */
