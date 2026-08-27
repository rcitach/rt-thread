/**
 ****************************************************************************************
 *
 * @file net_network_main.h
 *
 * @brief Define for Network initialize and handle
 *
 * Copyright (c) 2023-2024 Renesas Electronics. All rights reserved.
 *
 * This software ("Software") is owned by Renesas Electronics.
 *
 * By using this Software you agree that Renesas Electronics retains all
 * intellectual property and proprietary rights in and to this Software and any
 * use, reproduction, disclosure or distribution of the Software without express
 * written permission or a license agreement from Renesas Electronics is
 * strictly prohibited. This Software is solely for use on or in conjunction
 * with Renesas Electronics products.
 *
 * EXCEPT AS OTHERWISE PROVIDED IN A LICENSE AGREEMENT BETWEEN THE PARTIES, THE
 * SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. EXCEPT AS OTHERWISE
 * PROVIDED IN A LICENSE AGREEMENT BETWEEN THE PARTIES, IN NO EVENT SHALL
 * RENESAS ELECTRONICS BE LIABLE FOR ANY DIRECT, SPECIAL, INDIRECT, INCIDENTAL,
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THE SOFTWARE.
 *
 ****************************************************************************************
 */


#ifndef    __NET_NETWORK_MAIN_H__
#define    __NET_NETWORK_MAIN_H__

#include "FreeRTOS.h"
#include "lwipopts.h"

#include "iface_defs.h"
#include "common_def.h"
#include "net_arp.h"

#define RA6WX_PKT_SIZE         640
#define RA6WX_MAX_PACKETS      64

#define IP_PACKET_POOL_SIZE    (RA6WX_PKT_SIZE + sizeof(NX_PACKET)) * RA6WX_MAX_PACKETS

/* 1K is enouph for IP layer stack and thread stack */
#define RA6WX_IP_STACK         ((1 * KBYTE) + 384 )    // (1 * KBYTE)
#define RA6WX_ARP_STACK        (512) /* (1*KBYTE) */
#define RA6WX_BSD_SOCK_STACK   (2 * KBYTE)

//
// Extern functions
//
extern int wifi_netif_status(int intf);
int get_netmode(int iface);
unsigned int set_netmode(UCHAR iface, UCHAR mode, UCHAR save);
int getSysMode(void);
int setSysMode(int mode);

#ifdef    __TICKTIME__
time_t tick_time(time_t *p);
#endif    /* __TICKTIME__ */
long get_time_zone(void);
unsigned long set_time_zone(long timezone);


long ra6w1_network_main_get_timezone(void);
unsigned int ra6w1_network_main_set_timezone(long timezone);
int ra6w1_network_main_get_netmode(int iface);
unsigned int ra6w1_network_main_set_netmode(unsigned char iface, unsigned char mode, unsigned char save);
void ra6w1_network_main_reconfig_net(int state);
int ra6w1_network_main_get_sysmode(void);
int ra6w1_network_main_set_sysmode(int mode);
int ra6w1_get_fast_connection_mode(void);
int ra6w1_set_fast_connection_mode(int mode);
void ra6w1_network_main_change_iface_updown(unsigned int iface, unsigned int flag);
void ra6w1_network_main_check_network(int iface_flag, int simple);
int ra6w1_network_main_check_net_init(int iface);
unsigned int ra6w1_network_main_check_ip_addr(unsigned char iface);
#if defined ( __SUPPORT_IPV6__ )
unsigned int ra6w1_network_main_check_global_unicast_ip6_addr(unsigned char iface);
#endif // __SUPPORT_IPV6__ 
unsigned char ra6w1_network_main_check_dhcp_state(unsigned char iface);
unsigned int ra6w1_network_main_check_network_ready(unsigned char iface);
int ra6w1_network_main_check_net_ip_status(int iface);
int ra6w1_network_main_check_net_link_status(int iface);

void ra6w1_network_main_print_wlaninit_mode(void);
int ra6w1_network_main_set_wlaninit_mode(int flag);
int ra6w1_network_main_get_wlaninit_mode(void);

int ra6w1_network_main_is_wlaninit(void);
int ra6w1_network_main_init_wlan_with_task(void);
int ra6w1_network_main_init_wlan(void);
void ra6w1_network_main_init_system_apps(int net_chk_flag);
#if defined (__SUPPORT_IPV6__)		// UDP_DPM_TEST
int ra6w1_network_main_check_net_ipv6_status(int iface);
#endif // __SUPPORT_IPV6__

char get_enable_restart_dhcp_client(void);
char set_enable_restart_dhcp_client(char flag);
#ifdef __SUPPORT_DHCPC_IP_TO_STATIC_IP__
void ra6w1_set_temp_staticip_mode(int mode, int save);
int ra6w1_get_temp_staticip_mode(int iface_flag);
#endif // __SUPPORT_DHCPC_IP_TO_STATIC_IP__
void polling_state_check(void *pvParameters);
#endif    /* __NET_NETWORK_MAIN_H__ */

/* EOF */
