/**
 * @file
 * SNTP client API
 */

/*
 * Copyright (c) 2007-2009 Frédéric Bernon, Simon Goldschmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Frédéric Bernon, Simon Goldschmidt
 *
 */
#ifndef LWIP_HDR_APPS_RM_SNTP_H
#define LWIP_HDR_APPS_RM_SNTP_H

#include "rm_sntp_opts.h"
#include "lwip/ip_addr.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	u32_t	cmd;
	char * sntp_svr_addr;
	u32_t period;
} sntp_cmd_param;

enum sntp_cmd {
	SNTP_START,
	SNTP_SYNC,
	SNTP_STOP,
	SNTP_STATUS,
	SNTP_SERVER_ADDR,
	SNTP_SERVER_ADDR_1,
	SNTP_SERVER_ADDR_2,
	SNTP_PERIOD
};

typedef enum _sntp_svr_t {
	SNTP_SVR_0,
	SNTP_SVR_1,
	SNTP_SVR_2,
} sntp_svr_t;

/* SNTP operating modes: default is to poll using unicast.
   The mode has to be set before calling sntp_init(). */
#define SNTP_OPMODE_POLL            0
#define SNTP_OPMODE_LISTENONLY      1
void sntp_setoperatingmode(u8_t operating_mode);
u8_t sntp_getoperatingmode(void);

void sntp_init(void);
void sntp_stop(void);
u8_t sntp_enabled(void);
void sntp_run(void * arg);
void sntp_finsh(void);

int sntp_get_period(void);
u8_t sntp_set_period(u32_t period);	/*sec*/
u8_t sntp_get_use(void);
u8_t sntp_set_use(u8_t use);
char * sntp_get_server(sntp_svr_t svr_idx);
u8_t sntp_set_server(char *svraddr, sntp_svr_t svr_idx);
void sntp_sync(void);
void sntp_thread(void * arg);
void sntp_sync_now(void);

void sntp_setserver(u8_t idx, const ip_addr_t *addr);
const ip_addr_t* sntp_getserver(u8_t idx);

#if SNTP_MONITOR_SERVER_REACHABILITY
u8_t sntp_getreachability(u8_t idx);
#endif /* SNTP_MONITOR_SERVER_REACHABILITY */

#if SNTP_SERVER_DNS
void sntp_setservername(u8_t idx, const char *server);
const char *sntp_getservername(u8_t idx);
#endif /* SNTP_SERVER_DNS */

#if SNTP_GET_SERVERS_FROM_DHCP || SNTP_GET_SERVERS_FROM_DHCPV6
void sntp_servermode_dhcp(int set_servers_from_dhcp);
#else /* SNTP_GET_SERVERS_FROM_DHCP || SNTP_GET_SERVERS_FROM_DHCPV6 */
#define sntp_servermode_dhcp(x)
#endif /* SNTP_GET_SERVERS_FROM_DHCP || SNTP_GET_SERVERS_FROM_DHCPV6 */

#ifdef __cplusplus
}
#endif

#endif /* LWIP_HDR_APPS_SNTP_H */
