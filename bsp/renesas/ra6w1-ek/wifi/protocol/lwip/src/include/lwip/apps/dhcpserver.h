/**
 * Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Copyright (C) 2023-2024 Modified by Renesas Electronics
 *
**/

#ifndef __DHCPS_H__
#define __DHCPS_H__

#include <stdbool.h>
#include "lwipopts.h"
#include "lwip/ip_addr.h"

#if LWIP_IPV4

typedef struct dhcps_state{
	s16_t state;
} dhcps_state;

typedef struct dhcps_msg {
	u8_t op, htype, hlen, hops;
	u8_t xid[4];
	u16_t secs, flags;
	u8_t ciaddr[4];
	u8_t yiaddr[4];
	u8_t siaddr[4];
	u8_t giaddr[4];
	u8_t chaddr[16];
	u8_t sname[64];
	u8_t file[128];
	u8_t options[312];
}dhcps_msg;

typedef struct {
	bool enable;
	ip4_addr_t start_ip;
	ip4_addr_t end_ip;
} dhcps_lease_t;

enum dhcps_offer_option{
	OFFER_START = 0x00,
	OFFER_ROUTER = 0x01,
	OFFER_DNS = 0x02,
	OFFER_END
};

#define DHCPS_COARSE_TIMER_SECS			1
#define DHCPS_COARSE_TIMER_MSECS		(DHCPS_COARSE_TIMER_SECS * 1000UL)
#define DHCPS_MAX_LEASE					0x0A
#define DHCPS_LEASE_TIME_DEF			(120)
#define DHCPS_LEASE_UNIT CONFIG_LWIP_DHCPS_LEASE_UNIT

#define DHCP_SERVER_P2P_START_IP				"192,168,20,2"
#define DHCP_SERVER_P2P_END_IP					"192,168,20,11"
#define DHCP_SERVER_P2P_DEFAULT_LEASE_TIME		3600
#define DHCP_SERVER_P2P_DNS_IP					"8,8,8,8"

#define	DHCP_CLIENT_HOSTNAME_MAX			32

struct dhcps_pool {
	ip4_addr_t ip;
	u8_t mac[6];
	u32_t lease_timer;
	u8_t client_name[DHCP_CLIENT_HOSTNAME_MAX];
};

typedef u32_t dhcps_time_t;
typedef u8_t dhcps_offer_t;

typedef struct {
        dhcps_offer_t dhcps_offer;
        dhcps_offer_t dhcps_dns;
        dhcps_time_t  dhcps_time;
        dhcps_lease_t dhcps_poll;
} dhcps_options_t;

typedef struct {
	u32_t	cmd;
	ip4_addr_t ipaddr_ip;
	u32_t lease_time;
	ip4_addr_t dns;
	int	dhcps_interface;
} dhcps_cmd_param;

typedef void (*dhcps_cb_t)(u8_t client_ip[4]);

static inline bool dhcps_router_enabled (dhcps_offer_t offer) 
{
    return (offer & OFFER_ROUTER) != 0;
}

static inline bool dhcps_dns_enabled (dhcps_offer_t offer) 
{
    return (offer & OFFER_DNS) != 0;
}

void dhcps_start(struct netif *netif, ip4_addr_t ip);
void dhcps_stop(struct netif *netif);
void *dhcps_option_info(u8_t op_id, u32_t opt_len);
void dhcps_set_option_info(u8_t op_id, void *opt_info, u32_t opt_len);
bool dhcps_search_ip_on_mac(u8_t *mac, ip4_addr_t *ip);
struct dhcps_pool * dhcps_search_ip_pool(ip4_addr_t ip);
void dhcps_dns_setserver(const ip_addr_t *dnsserver);
ip4_addr_t dhcps_dns_getserver(void);
void dhcps_set_new_lease_cb(dhcps_cb_t cb);
void dhcps_run(void * param);
void dhcps_coarse_tmr(void);
void dhcps_print_lease_pool(u8_t is_print_assigned);
void dhcps_set_lease_time(dhcps_time_t ltime);
void dhcps_set_ip_range(char * sip, char * eip);
void dhcps_get_info(void);
dhcps_lease_t dhcps_get_ip_range(void);

#endif // LWIP_IPV4
#endif

/* EOF */
