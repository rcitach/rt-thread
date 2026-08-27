/**
 ****************************************************************************************
 *
 * @file net_stack_init.c
 *
 * @brief Initialize Network Stack
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

/*
 ******************************************************************************
 *       INCLUDE
 ******************************************************************************
 */
#include "bsp_api.h"
#include "FreeRTOS.h"
#include "custom_config_sdk.h"
#if CFG_WIFI
 #include "utils/includes.h"
#endif                                 /* CFG_WIFI */
#include "bsp_clocks.h"
#include "iface_defs.h"
#include "common_def.h"

#include "lwip/dhcp.h"
#include "lwip/netif.h"
#include "lwip/ip_addr.h"
#include "lwip/tcpip.h"
#include "lwip/dns.h"
#include "dhcpserver.h"
#include "sys_feature.h"
#if CFG_PMGR
 #include "user_dpm.h"
 #include "rm_pmgr_w_instance.h"
#else
 #if CFG_WIFI
  #include "romac4rtos.h"
 #endif                                /* CFG_WIFI */
#endif                                 /* CFG_PMGR */
#if CFG_WIFI
 #include "rm_wifi_helper.h"
 #include "util_api.h"
#endif                                 /* CFG_WIFI */
#if CFG_CLI
 #include "rm_cli_w.h"
#endif                                 // CFG_CLI
#include "net_wifi_monitor.h"
#if CFG_WIFI
 #include "rm_wifi_reg_pwr_db.h"
 #include "supp_config.h"
#endif                                 /* CFG_WIFI */
#include "net_network_main.h"
#ifdef RM_VEE_FLASH_W_RRQ_NVRAM_H
 #include "rm_vee_flash_w_rrq_nvram.h"
#endif                                 /* RM_VEE_FLASH_W_RRQ_NVRAM_H */
#include "rm_dhcp.h"
#ifdef R_RTC_W_H
 #include "r_rtc_w.h"
#endif                                 /* R_RTC_W_H */

#if defined(__SUPPORT_IPV6__)
 #include "lwip/ip6_addr.h"
 #include "lwip/priv/nd6_priv.h"
 #ifdef RM_MAP_PERSISTANT_W
  #include "rm_map_persistant_w.h"
  #include dg_configADNVPARAM_PROJ_FILE
 #endif

 #if CFG_PMGR
extern s8_t dpm_nd6_new_router(const ip6_addr_t * router_addr, struct netif * netif);
extern s8_t dpm_nd6_new_onlink_prefix(const ip6_addr_t * prefix, struct netif * netif);

extern struct nd6_router_list_entry default_router_list[LWIP_ND6_NUM_ROUTERS];
extern s8_t nd6_new_neighbor_cache_entry(void);

 #endif                                /* CFG_PMGR */
#endif                                 // __SUPPORT_IPV6__

/*
 ******************************************************************************
 *       LOCAL DEFINES
 ******************************************************************************
 */
#if CFG_PMGR
 #define DPM_INIT_FEATURE
 #define USER_WAKEUP_TIMER_REG
#endif                                 /* CFG_PMGR */

/*
 ******************************************************************************
 *       LOCAL CONSTANTS
 ******************************************************************************
 */

/*
 ******************************************************************************
 *       LOCAL DATA TYPES
 ******************************************************************************
 */

/*
 ******************************************************************************
 *       GLOBAL VARIABLES
 ******************************************************************************
 */
struct netif wlan0_iface;              /* Station interface : wlan0 */
struct netif wlan1_iface;              /* AP/P2P interface : wlan1 */

ip_addr_t iface_ipaddr, iface_netmask, iface_gateway;
ip_addr_t dns_ip_addr;

/* For FreeRTOS lwip Network interface and WIFI Mac interface mapping as boot */
char * p_ipaddr_str;
char * p_netmask_str;
char * p_gateway_str;
char * p_dns_str;

#if CFG_PMGR
user_dpm_supp_ip_info_t  * dpm_ip_info;
user_dpm_supp_net_info_t * dpm_netinfo;

 #if defined(__SUPPORT_IPV6__)
dpm_supp_ipv6_info_t * dpm_ipv6_info;
 #endif                                // __SUPPORT_IPV6__
#endif                                 /* CFG_PMGR */

/*
 ******************************************************************************
 *       LOCAL VARIABLES
 ******************************************************************************
 */

extern EventGroupHandle_t ra6w1_sp_event_group;
#if 0
extern UCHAR fast_connection_sleep_flag;
#endif

int netmode[] = {DHCPCLIENT, STATIC_IP};

/*
 ******************************************************************************
 *       LOCAL FUNCTION PROTOTYPES
 ******************************************************************************
 */

extern err_t      ethernetif_init(struct netif * netif);
extern err_t      ethernet_input(struct pbuf * p, struct netif * netif);
extern void       rwnx_driver_initialize_wiphy(void);
extern BaseType_t rwnx_mac_task_initiailize(void);
extern int        rwnx_hw_get_status(void);
extern BaseType_t rwnx_driver_task_initiailize(void);
extern BaseType_t start_ra6w1_wpa_supplicant(void);
extern void       dpm_ops_bss_infos_changed(void);
extern sys_clk_t  cm_cpu_clk_get(void);

#if CFG_PMGR
extern void fc80211_connection_loss(void);
extern UINT is_dpm_supplicant_done(void);
extern void dpm_reg_all_apps();
extern void RM_WIFI_dpm_ptim_event_set(int event);
extern int  RM_PMGR_W_dpm_wakeup_src_get(void);
extern int  restore_arp_table(void);

extern struct rm_etharp_entry * arp_table;
extern void * get_arp_table();

#endif                                 /* CFG_PMGR */
extern UINT set_netInit_flag(UINT iface);
extern void ra6w1_regdb_data_init(char * country);
extern void wifi_netif_control(int intf, int flag);

BaseType_t     net_stack_init(void);
struct netif * net_get_netif(int iface_index);

/*
 ******************************************************************************
 *       FUNCTIONS
 ******************************************************************************
 */

#undef USE_WLAN1_DNS
static BaseType_t net_init (void)
{
    ip_addr_set_zero(&iface_ipaddr);
    ip_addr_set_zero(&iface_netmask);
    ip_addr_set_zero(&iface_gateway);

    memset(&wlan0_iface, 0, sizeof(wlan0_iface));
    memset(&wlan1_iface, 0, sizeof(wlan1_iface));

    /* RT-Thread starts lwIP and its tcpip_thread from lwip_system_init().
     * Calling tcpip_init() here would create a second stack instance. */
#ifndef RT_USING_LWIP
    tcpip_init(NULL, NULL);
#endif

#if CFG_PMGR
    int ip_condition = 0;
    arp_table = (struct rm_etharp_entry *) get_arp_table();

    /* [WLAN0] DPM mode lwip network stack initialization as connection */
    if (RM_PMGR_W_dpm_is_wakeup() == pdTRUE)
    {
        RM_PMGR_W_rtm_static_get(RTM_STATIC_KEY_SUPPL_IP_INFO_PTR, NULL, NULL, (void **) (&dpm_ip_info));
        RM_PMGR_W_rtm_static_get(RTM_STATIC_KEY_SUPPL_NET_INFO_PTR, NULL, NULL, (void **) (&dpm_netinfo));
 #if defined(__SUPPORT_IPV6__)
        RM_PMGR_W_rtm_static_get(RTM_STATIC_KEY_IPV6_INFO_PTR, NULL, NULL, (void **) (&dpm_ipv6_info));
 #endif                                // __SUPPORT_IPV6__

        /* Recover Network information from Retention Memory */
        ra6w1_network_main_set_netmode(WLAN0_IFACE, dpm_netinfo->net_mode, 0);

        // Restore DNS server info.
 #if defined(__SUPPORT_IPV4__) && defined(__SUPPORT_IPV6__)
        dns_ip_addr.u_addr.ip4.addr = dpm_ip_info->dpm_dns_addr[0];
 #elif defined(__SUPPORT_IPV4__)

        //
 #elif defined(__SUPPORT_IPV6__)

        //
 #endif
        dns_setserver(0, &dns_ip_addr);

        set_netInit_flag(WLAN0_IFACE);

        // Restore netowork interface
 #if defined(__SUPPORT_IPV4__) && defined(__SUPPORT_IPV6__)
        iface_ipaddr.u_addr.ip4.addr  = dpm_ip_info->dpm_ip_addr;
        iface_netmask.u_addr.ip4.addr = dpm_ip_info->dpm_netmask;
        iface_gateway.u_addr.ip4.addr = dpm_ip_info->dpm_gateway;
 #elif defined(__SUPPORT_IPV4__)

        //
 #elif defined(__SUPPORT_IPV6__)

        //
 #endif                                // ...

        netif_add(&wlan0_iface,
 #if defined(__SUPPORT_IPV4__) && defined(__SUPPORT_IPV6__)
                  &iface_ipaddr.u_addr.ip4,
                  &iface_netmask.u_addr.ip4,
                  &iface_gateway.u_addr.ip4,
 #elif defined(__SUPPORT_IPV4__)
                  &iface_ipaddr,
                  &iface_netmask,
                  &iface_gateway,
 #endif                                // __SUPPORT_IPV4__
                  NULL,
                  ethernetif_init,
                  ethernet_input);

        netif_set_default(&wlan0_iface);
 #if defined(__SUPPORT_IPV6__)
        ip6_addr_t addr;
        ip6_addr_t ip6_gateway;
        ip6_addr_t ip6_dns;
        ip6_addr_t prefix_addr;
        s8_t       prefix_idx;
        u32_t      valid_life, pref_life;
        s8_t       neighbor_index;

        wlan0_iface.ip6_autoconfig_enabled = 1;
        netif_create_ip6_linklocal_address(&wlan0_iface, 1);
        netif_ip6_addr_set_state(&wlan0_iface, 0, IP6_ADDR_PREFERRED);

        // Need to check how to set "the state"... ( Preferred / Tentative )
        // : Change state to IP6_ADDR_TENTATIVE after Autoconfig or DHCP client ...
        netif_ip6_addr_set_state(&wlan0_iface, 1, IP6_ADDR_PREFERRED); // Set global IPv6 as Preferred

        /* Restore IP Info */
        memcpy(addr.addr, dpm_ipv6_info->dpm_ipv6_addr, sizeof(addr.addr));
        memcpy(ip6_gateway.addr, dpm_ipv6_info->dpm_gateway, sizeof(addr.addr));
        memcpy(ip6_dns.addr, dpm_ipv6_info->dpm_dns_addr, sizeof(addr.addr));

        IP6_ADDR(&prefix_addr, addr.addr[0], addr.addr[1], 0, 0);
        valid_life = dpm_ipv6_info->dpm_ipv6_valid_life;
        pref_life  = dpm_ipv6_info->dpm_ipv6_pref_life;

        /* Set Info */
        netif_ip6_addr_set(&wlan0_iface, 1, &addr); // Set Global IPv6 Address
        netif_ip6_addr_set_valid_life(&wlan0_iface, 1, valid_life);
        netif_ip6_addr_set_pref_life(&wlan0_iface, 1, pref_life);
        dns_setserver(0, (ip_addr_t *) &ip6_dns);

        prefix_idx = dpm_nd6_new_onlink_prefix(&prefix_addr, &wlan0_iface);
        if (prefix_idx >= 0)
        {
            prefix_list[prefix_idx].invalidation_timer = 300;
        }

        /* Restore Neighbor & Router Info */
        for (int i = 0; i < DPM_NEIGHBOR_NUM; i++)
        {
            if (!is_zero_ether_addr(dpm_ipv6_info->dpm_neighbor_macaddr[i]))
            {
                neighbor_index = nd6_new_neighbor_cache_entry();
                if (neighbor_index < 0)
                {

                    /* Could not create neighbor entry for this router. */
                    return -1;
                }

                neighbor_cache[i].state = ND6_REACHABLE;
                neighbor_cache[i].netif = &wlan0_iface;
                neighbor_cache[i].q     = NULL;

                memcpy(neighbor_cache[i].lladdr, dpm_ipv6_info->dpm_neighbor_macaddr[i], 6); // Restore Neighbor Info
                ip6_addr_copy(neighbor_cache[i].next_hop_address, dpm_ipv6_info->dpm_next_hop_address[i]);
                neighbor_cache[i].counter.reachable_time = dpm_ipv6_info->dpm_reachable_time;
            }

            if (dpm_ipv6_info->dpm_router_idx == i) // Restore Router Info
            {
                default_router_list[0].neighbor_entry        = &(neighbor_cache[i]);
                default_router_list[0].invalidation_timer    = dpm_ipv6_info->dpm_router_lifetime;
                default_router_list[0].neighbor_entry->state = ND6_REACHABLE;
                default_router_list[0].neighbor_entry->counter.reachable_time = LWIP_ND6_REACHABLE_TIME;
            }
        }
 #endif                                // __SUPPORT_IPV6__

        // Make it active ... after WIFI Connection and Ready
        // First of all ... do netif as down,
        // After WIFI Connection and Ready, do as UP
        netif_set_down(&wlan0_iface);

 #if !defined(__DISABLE_DPM_ABNORM__)
        if (!chk_abnormal_wakeup())
 #endif                                // !__DISABLE_DPM_ABNORM__
        {
            restore_arp_table();
        }
    }
    else
#endif                                 /* CFG_PMGR */
    {
        /* [WLAN0] Default Network interface add */
#if CFG_WIFI
        if ((get_run_mode() == WIFI_DEVICE_MODE_EXT_STATION) ||
            get_run_mode() == WIFI_DEVICE_MODE_EXT_AP_STATION
 #if defined(__SUPPORT_P2P__)
            || get_run_mode() == WIFI_DEVICE_MODE_EXT_P2P_STATION
 #endif                                // __SUPPORT_P2P__
            )
#endif                                 /* CFG_WIFI */
        {
#ifdef RM_MAP_PERSISTANT_W
 #ifdef PLATFORM_NVPARAM_H_
            RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                         ENV_GROUP_WIFIPROFILE,
                                         WIFI_PROFILE_NETMODE_0,
                                         &netmode[WLAN0_IFACE]);
 #endif                                /* PLATFORM_NVPARAM_H_  */
#endif

            if (netmode[WLAN0_IFACE] < 1)
            {
                netmode[WLAN0_IFACE] = DEFAULT_NETMODE_WLAN0;
            }

            if (get_netmode(0) == STATIC_IP)
            {
                ip_addr_set_zero(&iface_ipaddr);
                ip_addr_set_zero(&iface_netmask);
                ip_addr_set_zero(&iface_gateway);
#ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                ENV_GROUP_WIFIPROFILE,
                                                WIFI_PROFILE_IPADDR_0,
                                                &p_ipaddr_str);
                RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                ENV_GROUP_WIFIPROFILE,
                                                WIFI_PROFILE_NETMASK_0,
                                                &p_netmask_str);
                RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                ENV_GROUP_WIFIPROFILE,
                                                WIFI_PROFILE_GATEWAY_0,
                                                &p_gateway_str);
#endif                                 /* RM_MAP_PERSISTANT_W */

                if ((p_ipaddr_str == NULL) || (p_netmask_str == NULL))
                {
                    p_ipaddr_str  = DEFAULT_IPADDR_WLAN0;
                    p_netmask_str = DEFAULT_SUBNET_WLAN0;

                    if (p_gateway_str == NULL)
                    {
                        p_gateway_str = DEFAULT_GATEWAY_WLAN0;
                    }

                    ipaddr_aton(p_ipaddr_str, &iface_ipaddr);
                    ipaddr_aton(p_netmask_str, &iface_netmask);
                    ipaddr_aton(p_gateway_str, &iface_gateway);
                }
                else
                {
                    ipaddr_aton(p_ipaddr_str, &iface_ipaddr);
                    ipaddr_aton(p_netmask_str, &iface_netmask);
                    ipaddr_aton(p_gateway_str, &iface_gateway);
                }

                // DNS 1st
                ip_addr_set_zero(&dns_ip_addr);
#ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                ENV_GROUP_WIFIPROFILE,
                                                WIFI_PROFILE_DNSSVR_0,
                                                &p_dns_str);
#endif                                 /* RM_MAP_PERSISTANT_W */

                if (p_dns_str == NULL)
                {
                    p_dns_str = DEFAULT_DNS_WLAN0;
                    ipaddr_aton(p_dns_str, &dns_ip_addr);
                }
                else
                {
                    ipaddr_aton(p_dns_str, &dns_ip_addr);
                }

                dns_setserver(0, &dns_ip_addr);

                // DNS 2nd
                ip_addr_set_zero(&dns_ip_addr);
#ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                ENV_GROUP_WIFIPROFILE,
                                                WIFI_PROFILE_DNSSVR_2ND_0,
                                                &p_dns_str);
#endif                                 /* RM_MAP_PERSISTANT_W */

                if (p_dns_str == NULL)
                {
                    p_dns_str = DEFAULT_DNS_2ND;
                    ipaddr_aton(p_dns_str, &dns_ip_addr);
                }
                else
                {
                    ipaddr_aton(p_dns_str, &dns_ip_addr);
                }

                dns_setserver(2, &dns_ip_addr);

                /* Case of DPM & Static IP, Save info */
#if CFG_PMGR
                if (RM_PMGR_W_dpm_is_enabled() == pdTRUE)
                {
                    RM_PMGR_W_rtm_static_get(RTM_STATIC_KEY_SUPPL_IP_INFO_PTR, NULL, NULL, (void **) (&dpm_ip_info));
                    RM_PMGR_W_rtm_static_get(RTM_STATIC_KEY_SUPPL_NET_INFO_PTR, NULL, NULL, (void **) (&dpm_netinfo));

                    dpm_netinfo->net_mode = STATIC_IP;

 #if defined(__SUPPORT_IPV4__) && defined(__SUPPORT_IPV6__)
                    dpm_ip_info->dpm_ip_addr = iface_ipaddr.u_addr.ip4.addr;
                    dpm_ip_info->dpm_netmask = iface_netmask.u_addr.ip4.addr;
                    dpm_ip_info->dpm_gateway = iface_gateway.u_addr.ip4.addr;
 #elif defined(__SUPPORT_IPV4__)

                    //
 #elif defined(__SUPPORT_IPV6__)

                    //
 #endif

 #if defined(__SUPPORT_IPV4__) && defined(__SUPPORT_IPV6__)

                    // dpm_ip_info->dpm_dns_addr[0] = dns_getserver(0)->addr;
                    // dpm_ip_info->dpm_dns_addr[1] = dns_getserver(1)->addr;
                    dpm_ip_info->dpm_dns_addr[0] = dns_ip_addr.u_addr.ip4.addr;
 #elif defined(__SUPPORT_IPV4__)

                    //
 #elif defined(__SUPPORT_IPV6__)

                    //
 #endif

                    RM_WIFI_dpm_arp_filter_set(dpm_ip_info->dpm_ip_addr, dpm_ip_info->dpm_netmask);
                }
#endif                                 /* CFG_PMGR */
            }
            else
            {
#if CFG_PMGR
                if (RM_PMGR_W_dpm_is_enabled() == pdTRUE)
                {
                    RM_PMGR_W_rtm_static_get(RTM_STATIC_KEY_SUPPL_NET_INFO_PTR, NULL, NULL, (void **) (&dpm_netinfo));
                    dpm_netinfo->net_mode = DHCPCLIENT;
                }
#endif                                 /* CFG_PMGR */
            }

            set_netInit_flag(WLAN0_IFACE);
        }

        // [WLAN0] Add a network interface to the list of lwIP netifs.
        if (netif_add(&wlan0_iface,
#if defined(__SUPPORT_IPV4__) && defined(__SUPPORT_IPV6__)
                      &iface_ipaddr.u_addr.ip4,
                      &iface_netmask.u_addr.ip4,
                      &iface_gateway.u_addr.ip4,
#elif defined(__SUPPORT_IPV4__)
                      &iface_ipaddr,
                      &iface_netmask,
                      &iface_gateway,
#endif                                 // __SUPPORT_IPV4__
                      NULL,
                      ethernetif_init,
                      ethernet_input) == NULL)
        {
            return pdFAIL;
        }

#if defined(__USER_DHCP_HOSTNAME__)
        char * dhcp_hostname = NULL;

 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                        ENV_GROUP_SYSCFG,
                                        NVR_DHCPC_HOSTNAME,
                                        &dhcp_hostname);
 #endif                                // RM_MAP_PERSISTANT_W

        if ((dhcp_hostname != NULL) && (strlen(dhcp_hostname) != 0))
        {
            wlan0_iface.hostname = dhcp_hostname;
        }
#endif                                 // __USER_DHCP_HOSTNAME__

#ifdef __SUPPORT_IPV6__
        {
 #if CFG_PMGR
            uint8_t ipv6l[16];         // Link Local Address
 #endif /* CFG_PMGR */

            // Add IPv6 Address
            // 1st Address (linklocal_address)
            wlan0_iface.ip6_autoconfig_enabled = 1;
            netif_create_ip6_linklocal_address(&wlan0_iface, 1);

 #if CFG_PMGR
            memcpy(ipv6l, netif_ip_addr6(&wlan0_iface, 0), sizeof(ipv6l));
            romac4rtos_set_ipv6(ipv6l, NULL, NULL);
 #endif                                /* CFG_PMGR */

 #if CFG_PMGR
  #ifdef RM_MAP_PERSISTANT_W
            if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG,
                                             NVR_KEY_DPM_IP_CONDITION, &ip_condition))
            {
                ip_condition = PMGR_CONDITION_IPV4_MANDATORY;
            }
  #endif
            if (ip_condition & PMGR_CONDITION_IPV6_MANDATORY)
            {
                RM_PMGR_W_dpm_job_name_set(REG_NAME_DPM_LWIP_IPV6, 0);
                RM_PMGR_W_dpm_sleep_ready_clear(REG_NAME_DPM_LWIP_IPV6);
            }
 #endif                                /* CFG_PMGR */

            // Need to check how to set "the state"... ( Preferred / Tentative )
            // : Change state to IP6_ADDR_TENTATIVE after Autoconfig or DHCP client ...
        }
#endif                                 // __SUPPORT_IPV6__

        /* [WLAN1] AP or P2P, Secondary Network interface add */
#if CFG_WIFI
        if (get_run_mode() == WIFI_DEVICE_MODE_EXT_AP
 #if defined(__SUPPORT_P2P__)
            || get_run_mode() == WIFI_DEVICE_MODE_EXT_P2P ||
            get_run_mode() == WIFI_DEVICE_MODE_EXT_P2P_GO
 #endif                                // __SUPPORT_P2P__
            || get_run_mode() == WIFI_DEVICE_MODE_EXT_AP_STATION
 #if defined(__SUPPORT_P2P__)
            || get_run_mode() == WIFI_DEVICE_MODE_EXT_P2P_STATION
 #endif                                // __SUPPORT_P2P__
            )
#endif                                 /* CFG_WIFI */
        {
#ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                            ENV_GROUP_WIFIPROFILE,
                                            WIFI_PROFILE_IPADDR_1,
                                            &p_ipaddr_str);
            RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                            ENV_GROUP_WIFIPROFILE,
                                            WIFI_PROFILE_NETMASK_1,
                                            &p_netmask_str);
            RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                            ENV_GROUP_WIFIPROFILE,
                                            WIFI_PROFILE_GATEWAY_1,
                                            &p_gateway_str);
#endif                                 /* RM_MAP_PERSISTANT_W */
#ifdef USE_WLAN1_DNS
 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                            ENV_GROUP_WIFIPROFILE,
                                            WIFI_PROFILE_DNSSVR_1,
                                            &p_dns_str);
 #endif
#endif                                 // USE_WLAN1_DNS

            ip_addr_set_zero(&iface_ipaddr);
            ip_addr_set_zero(&iface_netmask);
            ip_addr_set_zero(&iface_gateway);

            if ((p_ipaddr_str == NULL) || (p_netmask_str == NULL))
            {
#if defined(__SUPPORT_P2P__)
 #if CFG_WIFI
                if ((get_run_mode() == WIFI_DEVICE_MODE_EXT_P2P_GO) ||
                    (get_run_mode() == WIFI_DEVICE_MODE_EXT_P2P) ||
                    (get_run_mode() == WIFI_DEVICE_MODE_EXT_P2P_STATION))
 #endif                                /* CFG_WIFI */
                {
                    p_ipaddr_str  = IPADDR_ANY_STR;
                    p_netmask_str = IPADDR_ANY_STR;
                }
                else
#endif                                 // __SUPPORT_P2P__
                {
                    p_ipaddr_str  = DEFAULT_IPADDR_WLAN1;
                    p_netmask_str = DEFAULT_SUBNET_WLAN1;

                    if (p_gateway_str == NULL)
                    {
                        p_gateway_str = DEFAULT_GATEWAY_WLAN1;
                    }
                }

                ipaddr_aton(p_ipaddr_str, &iface_ipaddr);
                ipaddr_aton(p_netmask_str, &iface_netmask);
                ipaddr_aton(p_gateway_str, &iface_gateway);
            }
            else
            {
                // read ok
                ipaddr_aton(p_ipaddr_str, &iface_ipaddr);
                ipaddr_aton(p_netmask_str, &iface_netmask);
                ipaddr_aton(p_gateway_str, &iface_gateway);
            }

            ipaddr_aton(p_ipaddr_str, &iface_ipaddr);
            ipaddr_aton(p_netmask_str, &iface_netmask);
            ipaddr_aton(p_gateway_str, &iface_gateway);

#ifdef USE_WLAN1_DNS
 #if CFG_WIFI
            if ((p_dns_str == NULL) &&
                get_run_mode() != WIFI_DEVICE_MODE_EXT_AP
  #if defined(__SUPPORT_P2P__)
                && get_run_mode() != WIFI_DEVICE_MODE_EXT_P2P_GO &&
                get_run_mode() != WIFI_DEVICE_MODE_EXT_P2P &&
                get_run_mode() != WIFI_DEVICE_MODE_EXT_P2P_STATION
  #endif                               // __SUPPORT_P2P__
                )
 #endif                                /* CFG_WIFI */
            {
                p_dns_str = DEFAULT_DNS_WLAN1;
                ipaddr_aton(p_dns_str, &dns_ip_addr);
            }
            else
            {
                ipaddr_aton(p_dns_str, &dns_ip_addr);
            }

 #if CFG_WIFI
            if (get_run_mode() != WIFI_DEVICE_MODE_EXT_AP
  #if defined(__SUPPORT_P2P__)
                && get_run_mode() != WIFI_DEVICE_MODE_EXT_P2P_GO &&
                get_run_mode() != WIFI_DEVICE_MODE_EXT_P2P &&
                get_run_mode() != WIFI_DEVICE_MODE_EXT_P2P_STATION
  #endif                               // __SUPPORT_P2P__
                )
 #endif                                /* CFG_WIFI */
            {
                dns_setserver(0, &dns_ip_addr);
            }
#endif                                 // USE_WLAN1_DNS

            // [WLAN1] Add a network interface to the list of lwIP netifs.
            if (netif_add(&wlan1_iface,
#if defined(__SUPPORT_IPV4__) && defined(__SUPPORT_IPV6__)
                          &iface_ipaddr.u_addr.ip4,
                          &iface_netmask.u_addr.ip4,
                          &iface_gateway.u_addr.ip4,
#elif defined(__SUPPORT_IPV4__)
                          &iface_ipaddr,
                          &iface_netmask,
                          &iface_gateway,
#endif                                             // __SUPPORT_IPV4__
                          NULL,
                          ethernetif_init,
                          ethernet_input) == NULL) // There is no device driver
            {
                return pdFAIL;
            }

#ifdef __SUPPORT_IPV6__
            {
                // Add IPv6 Address
                // 1st Address (linklocal_address)
                wlan1_iface.ip6_autoconfig_enabled = 1;
                netif_create_ip6_linklocal_address(&wlan1_iface, 1);
                netif_ip6_addr_set_state(&wlan1_iface, 0, IP6_ADDR_TENTATIVE);
            }
#endif                                 // __SUPPORT_IPV6__

            set_netInit_flag(WLAN1_IFACE);
        }

        /* 3. Station mode Netif Setup as default */
#if CFG_WIFI
        if ((get_run_mode() == WIFI_DEVICE_MODE_EXT_STATION) ||
            get_run_mode() == WIFI_DEVICE_MODE_EXT_AP_STATION
 #if defined(__SUPPORT_P2P__)
            || get_run_mode() == WIFI_DEVICE_MODE_EXT_P2P_STATION
 #endif                                // __SUPPORT_P2P__
            )
#endif                                 /* CFG_WIFI */
        {
            // Set the network interface as the default network interface.
            netif_set_default(&wlan0_iface);

            // Make it active ... after WIFI Connection and Ready
            // First of all ... do netif as down,
            // After WIFI Connection and Ready, do as UP
            netif_set_down(&wlan0_iface);
        }

        /* 4. If working mode is AP/P2P mode, Seconday Network if as default */
#if CFG_WIFI
        if (get_run_mode() == WIFI_DEVICE_MODE_EXT_AP
 #if defined(__SUPPORT_P2P__)
            || get_run_mode() == WIFI_DEVICE_MODE_EXT_P2P ||
            get_run_mode() == WIFI_DEVICE_MODE_EXT_P2P_GO
 #endif                                // __SUPPORT_P2P__
            || get_run_mode() == WIFI_DEVICE_MODE_EXT_AP_STATION)
#endif                                 /* CFG_WIFI */
        {
#if CFG_WIFI
            if (get_run_mode() == WIFI_DEVICE_MODE_EXT_AP_STATION
 #if defined(__SUPPORT_P2P__)
                || get_run_mode() == WIFI_DEVICE_MODE_EXT_P2P_STATION
 #endif                                // __SUPPORT_P2P__
                )
#endif                                 /* CFG_WIFI */
            {
                // Set the network interface as the default network interface.
                netif_set_default(&wlan1_iface);
            }

            // Make it active ...after WIFI Connection and Ready
            // First of all ... do netif as down,
            // After WIFI Connection and Ready, do as UP
            netif_set_down(&wlan1_iface);
        }
    }

    return pdPASS;
}

static int umac_lmac_init (int dpmmode)
{
    BaseType_t   status = pdFAIL;
    unsigned int retry_cnt;
    unsigned int retry_limit = 1024;

    status = rwnx_mac_task_initiailize(); // rwnx_mac_task call;
    if (status == pdFAIL)
    {
        printf("Fail\n");

        return pdFAIL;
    }

    status = rwnx_driver_task_initiailize(); // rwnx_driver_task_main call ;
    if (status == pdPASS)
    {
        ;
    }
    else
    {
        printf("Fail\n");

        return pdFAIL;
    }

    for (retry_cnt = 0; retry_cnt < retry_limit; retry_cnt++)
    {
        if (rwnx_hw_get_status() != 1) /* DRIVER_ACTIVE */
        {
            vTaskDelay(1);
        }
        else
        {
            break;
        }
    }

    if (retry_cnt >= retry_limit)
    {
        printf("Error rwnx_hw_get_status cnt=%d\n", retry_cnt);

        return pdFAIL;
    }

    return pdTRUE;
}

/** During DPM Wakeup, Initialize processing is like below.
 * mac80211 Init() / UMAC LMAC Init --> ops start(dpm_start_req_handler) --> bss_info_changed(lmac connection status) -->
 * umac_dpm_init(umac connection status) --> Network stack create & start --> Supplicant create & start --> tim status_handler()
 **/
#if CFG_PMGR
static UINT dpm_full_wakeup_wlaninit ()
{
 #ifdef  R_RTC_W_HELPER_H
    __time64_t now;
 #endif                                /* R_RTC_W_HELPER_H */

    /* Restore rtc timezone */
    long timezone_offset = 0;
    RM_PMGR_W_rtm_static_get(RTM_STATIC_KEY_TIMEZONE, &timezone_offset, NULL, NULL);
 #ifdef R_RTC_W_H
    R_RTC_W_CalendarTimeZoneSet(R_RTC_W_GetCtrl(), &timezone_offset);
 #endif                                /* R_RTC_W_H */

 #if 0                                 // init to RM_PMGR_W_dpm_lld_task_start

    /* Init the resource of power daemon */
    RM_PMGR_W_dpm_lld_task_init();
 #endif

    if (umac_lmac_init(1) == pdFAIL)
    {
        return pdFAIL;
    }

 #if 0                                 // must check by wyyang

    /* set handler function of LMAC */
    dpm_ops_bss_infos_changed();
 #endif

    /* set wifi as connection */
 #if 0                                 // old DPM
    umac_dpm_func_init();
 #endif

    /* Init the resource of power daemon */
    /* already created event and mutex in start_dpm_sleep_daemon() run */
 #if 0                                 // already done by sean lee
    init_dpm_sleep_daemon();
 #endif

    /** 1. First of all, wait until LMAC Initial is done.
     * 2. MAC IDLE Mode --> Active Mode
     * 3. do start receiving packet
     */                                // TODO: should we?

    /* network stack(lwip) should be initialized with DPM */
    /* Init network resource */
    if (net_init() == pdPASS)
    {
 #ifdef FOR_DEBUG
        printf("OK\n");
 #endif
    }
    else
    {
        printf("Fail\n");

        return pdFAIL;
    }

    /* Network interface UP in DPM */
    wifi_netif_control(WLAN0_IFACE, 1);

    /* In case of DPM Full booting, Need link up connection as true */
 #if 0                                 // OLD DPM
    if (umac_dpm_check_init())
    {
        umac_dpm_init_done();
    }
 #endif

    /* Check overflow */
 #ifdef  R_RTC_W_HELPER_H
    ra6w1_time64(NULL, &now);
 #endif                                /* R_RTC_W_HELPER_H */

    return ERR_OK;
}

#endif                                 /* CFG_PMGR */

void reboot_func (unsigned int flag)   // Temporary
{
    printf("\n > Reboot....\n");

    if (flag == 0)
    {
        SWRESET;
    }
    else if (flag == 1)
    {
        PORRESET;
    }
    else if (flag == 2)
    {
        PORRESET;
    }
    else
    {
        printf(" Unknown flag\n");
    }
}

BaseType_t net_stack_init (void)
{
#ifdef DPM_INIT_FEATURE
    int  BootScenario;
    bool abnormal_dpm_boot = false;
#endif                                 /* DPM_INIT_FEATURE */
#if CFG_PMGR
    bool                 reconnect_try = false;
    enum DPM_WAKEUP_TYPE wakeuptype    = DPM_UNKNOWN_WAKEUP;
#endif                                 /* CFG_PMGR */
    char * result_ptr = NULL;

    if ((int) cm_cpu_clk_get() < cpuclk_26M)
    {
        printf("WLAN does not work at CPU clock retes below %dMhz.\n", cpuclk_26M);
#if CFG_CLI
        print_prompt();
#endif
        vTaskDelay(portCONVERT_MS_2_TICKS(500));

        return pdFALSE;
    }

#ifdef  DPM_INIT_FEATURE

    /* Booting Scenario and Checkin */
    BootScenario = RM_PMGR_W_dpm_wakeup_src_get();
    if (!((BootScenario == BSP_WAKEUP_SOURCE_GPIO) ||                         // External wakeup - None DPM
          (BootScenario == BSP_WAKEUP_SOURCE_WAKEUP_COUNTER) ||               // Counter wakeup
          (BootScenario == BSP_WAKEUP_SOURCE_POR) ||                          // POR
          (BootScenario == BSP_WAKEUP_SOURCE_WATCHDOG) ||                     // Reboot with retention
          (BootScenario == BSP_WAKEUP_RESET) /* reboot without retention */)) // cmd "reboot"
    {
        /* Check RF-Test mode */
        if (ra6w1_network_main_get_wlaninit_mode() == pdFALSE)
        {
            // Cannot support DPM in RF-Test mode.
 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, WIFI_PROFILE_ENABLE_DPM,
                                          0);
 #endif                                // RM_MAP_PERSISTANT_W
 #if 0                                 // ??
  #ifdef BUILD_OPT_RA6W1_ASIC
            extern void dpm_power_up_step4_patch(unsigned long prep_bit);

            dpm_power_up_step4_patch(0xf);
  #endif
 #endif
            printf("Fail to initialize WLAN. (step 0)\n!!! TEST MODE !!!\n");

            return pdFALSE;
        }

        /* Case of Wakeup Reset, clear the umac connection info */
        if ((BootScenario == BSP_WAKEUP_RESET_WITH_RETENTION) && RM_PMGR_W_dpm_mode_get())
        {
            // OLD DPM fc80211_dpm_info_clear();
        }
    }

#else

    /* Check the status of WLAN initialization */
    if (ra6w1_network_main_get_wlaninit_mode() == pdFALSE)
    {
        printf("\n\n!!! TEST MODE !!!\nWLAN not initialized.\n\n");
 #if CFG_CLI
        print_prompt();
 #endif
        vTaskDelay(portCONVERT_MS_2_TICKS(500));

        return pdFAIL;
    }
#endif

/*** Setting a country-specific power table   ***/
    {
        char c_code[4] = {0, };
#if CFG_PMGR
        if (RM_PMGR_W_dpm_is_wakeup())
        {
            char * c_code_tmp = NULL;
            RM_PMGR_W_rtm_static_get(RTM_STATIC_KEY_COUNTRY_CODE, NULL, NULL, (void **) (&c_code_tmp));
            strcpy(c_code, c_code_tmp);
        }
        else
#endif                                 /* CFG_PMGR */
        {
#ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                            ENV_GROUP_WIFIPROFILE,
                                            "country_code",
                                            &result_ptr);
#endif                                 /* RM_MAP_PERSISTANT_W */
            if (result_ptr && strlen(result_ptr))
            {
                strcpy(c_code, result_ptr);
            }
            else
            {
                strncpy(c_code, COUNTRY_CODE_DEFAULT, 4);
            }
        }

        ra6w1_regdb_data_init(c_code);
    }

    /* Initialize Supplicant stack */
    ra6w1_sp_event_group = xEventGroupCreate();

#if CFG_PMGR
 #if !defined(__DISABLE_DPM_ABNORM__)
    extern bool umac_dpm_latest_status(void);

    if (RM_PMGR_W_dpm_wakeup_is_abnormal())
    {
        abnormal_dpm_boot = true;
        RM_PMGR_W_dpm_ptim_abnormal_wakeup_set();
        RM_WIFI_dpm_conn_info_clear();
        RM_PMGR_W_rtm_static_set(RTM_STATIC_KEY_DPM_WAKEUP_FLAG, 0, 0);
    }
 #endif                                // !__DISABLE_DPM_ABNORM__

    if (RM_PMGR_W_dpm_is_wakeup() == pdTRUE)
    {
        romac4rtos_initialize(false, dg_configPTIMG_HDR_ADDR);
        romac4rtos_upload(0);

        RM_WIFI_dpm_ptim_event_set(romac4rtos_event());
 #ifdef FOR_DEBUG
        printf("ptim_event:0x%x \n", RM_WIFI_dpm_ptim_event_get());
 #endif                                // FOR_DEBUG

        /* DPM Wakeup */
        dpm_full_wakeup_wlaninit();
    }
    else
#endif                                 /* CFG_PMGR */
    {
#if CFG_WIFI
        romac4rtos_initialize(true, dg_configPTIMG_HDR_ADDR);

        // Call when RTM is reset
        romac4rtos_config(0, 0);
#endif                                 /* CFG_WIFI */

#if 0

        /* do skip for sleep mode 1/2 fast connect , Factory Reset button*/
        if (fast_connection_sleep_flag == pdFALSE)
        {
            if (ra6w1_network_main_get_wlaninit_mode() == pdFALSE)
            {
 #ifdef BUILD_OPT_RA6W1_ASIC
                extern void dpm_power_up_step4_patch(unsigned long prep_bit);

                dpm_power_up_step4_patch(0xf);
 #endif                                // BUILD_OPT_RA6W1_ASIC
                PRINTF("Fail to initialize WLAN. (step 1)\n!!! TEST MODE !!!\n");

                return pdFALSE;
            }
        }
#endif

        /* Channel info update by country for suppl to get the correct ch info from mac */
        extern void fc80211_update_channel_info(void);

        fc80211_update_channel_info();

        if (umac_lmac_init(0) == pdFAIL)
        {
            return pdFAIL;
        }

        printf("Network init...");

        if (net_init() == pdPASS)
        {
            printf("OK\n");
        }
        else
        {
            printf("Fail\n");

            return pdFAIL;
        }

        switch (get_run_mode())
        {
#if CFG_WIFI
            case WIFI_DEVICE_MODE_EXT_AP:
 #if defined(__SUPPORT_WIFI_CONCURRENT__)
            case WIFI_DEVICE_MODE_EXT_AP_STATION:
  #ifdef __SUPPORT_P2P__
            case WIFI_DEVICE_MODE_EXT_P2P:
            case WIFI_DEVICE_MODE_EXT_P2P_GO:
            case WIFI_DEVICE_MODE_EXT_P2P_STATION:
  #endif                               /* __SUPPORT_P2P__ */
 #endif                                /* __SUPPORT_WIFI_CONCURRENT__ */
                {
 #if defined(__SUPPORT_IPV4__)
  #ifdef __SUPPORT_DHCP_SVR__
                    int               use_dhcpd = NONE_IFACE;
                    dhcps_cmd_param * dhcp_param;
   #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_SYSCFG,
                                                 "USEDHCPD",
                                                 &use_dhcpd);
   #endif
                    if (use_dhcpd == -1)
                    {
                        use_dhcpd = pdFALSE;
                    }

                    if (use_dhcpd == pdTRUE)
                    {
                        dhcp_param = pvPortMalloc(sizeof(dhcps_cmd_param));
                        memset(dhcp_param, 0, sizeof(dhcps_cmd_param));
                        dhcp_param->cmd             = DHCP_SERVER_STATE_START;
                        dhcp_param->dhcps_interface = WLAN1_IFACE;

                        printf("Start DHCP Server\n");
                        dhcps_run(dhcp_param);
                    }
  #endif
 #endif                                // __SUPPORT_IPV4__
                }
                break;
#endif                                 /* CFG_WIFI */
        }
    }

#if CFG_PMGR
    if (get_netmode(WLAN0_IFACE) == DHCPCLIENT)
    {
        int dpm_retry_cnt = 0;
        int dpm_sts;

        // Register for DPM operation
        RM_PMGR_W_dpm_job_name_set(NET_IFCONFIG, 0);
        RM_PMGR_W_dpm_wakeup_done(NET_IFCONFIG);

        if ((RM_PMGR_W_dpm_is_wakeup() == 1) && (RM_WIFI_dpm_supp_is_connected() == 1))
        {
dpm_dhcpc_set_r:

            if (dpm_retry_cnt++ < 5)
            {
                dpm_sts = RM_PMGR_W_dpm_sleep_ready_set(NET_IFCONFIG);

                switch (dpm_sts)
                {
                    case DPM_SET_ERR:
                    {
                        vTaskDelay(portCONVERT_MS_2_TICKS(10));
                        goto dpm_dhcpc_set_r;
                        break;
                    }

                    case DPM_SET_OK:
                    {
                        break;
                    }

                    default:
                    {
                        printf(RED_COLOR " [%s] Error set dpm \n" CLEAR_COLOR, __func__);
                        break;
                    }
                }
            }
        }
    }
#endif                                 /* CFG_PMGR */

#ifndef __SUPPORT_SIGMA_TEST__
 #if CFG_PMGR
    if ((get_run_mode() == WIFI_DEVICE_MODE_EXT_STATION) && RM_PMGR_W_dpm_is_enabled())
    {
        ra6w1_arp_create_polling_state_check(WLAN0_IFACE);
    }
 #endif                                /* CFG_PMGR */
#endif                                 // __SUPPORT_SIGMA_TEST__

#ifdef  USER_WAKEUP_TIMER_REG
    if (RM_PMGR_W_dpm_is_enabled())
    {
        RM_PMGR_W_dpm_user_wakeup_timer_init();
    }
#endif                                 // USER_WAKEUP_TIMER_REG

    /* Start Wifi monitor thread to receive events from Supplicant */
    if (start_wifi_monitor() == pdFAIL)
    {
        printf("Error creating task wifi_ev_mon\n");

        return pdFAIL;
    }

    /* Register Wi-Fi connect/disconnect status notify call-back functions */
#if CFG_WIFI
    rm_wifi_register_wifi_notify_cb();
#endif                                 /* CFG_WIFI */

    /* Start Wi-Fi wpa_supplicant */
    if (start_ra6w1_wpa_supplicant() == pdFAIL)
    {
        printf(">>> Failed to start wpa_supplicant !!!\n");

        return pdFAIL;
    }

#if CFG_PMGR
    if (RM_PMGR_W_dpm_is_wakeup())
    {
        reconnect_try = abnormal_dpm_boot;

        // printf(YELLOW_COLOR " [%s] reconnect_try:%d \n" CLEAR_COLOR, __func__, reconnect_try);

        /* !!! Caution !!!
         *    Don't run this function before netInit().
         */
        wakeuptype = (enum DPM_WAKEUP_TYPE) RM_PMGR_W_dpm_wakeup_type_get(1);

        /* Case of UC, MB/BC, NO-BCN, let's do have enough time to process by Application */
        /* Don't check the wakeup condition ... */
        {
            /* Network interface UP in DPM */
            wifi_netif_control(WLAN0_IFACE, 1);

            /* User DPM module should be registered as ASAP */
 #ifdef __CHECK_ABNORMAL_WAKEUP__
            if (!(wakeuptype == DPM_NOACK_WAKEUP))
 #endif                                /* __CHECK_ABNORMAL_WAKEUP__ */
            {
                dpm_reg_all_apps();
            }

            /* TIM Status is UC, BC, BCN_CHG, DPM_USER_0, DPM_USER_1 , let's wait supplicant ready(key set, eloop run.) */
            if ((wakeuptype == DPM_PACKET_WAKEUP) || (wakeuptype == DPM_USER_WAKEUP))
            {
                int cnt = 0;
                while (1)
                {
                    if (is_dpm_supplicant_done())
                    {
                        break;
                    }

                    vTaskDelay(portCONVERT_MS_2_TICKS(10));
                    if (cnt++ > 100)
                    {
                        printf(RED_COLOR "please supplicant check (supp init fail) \n" CLEAR_COLOR);
                        break;
                    }
                }

 #if 0

                // In case DPM_USER_0, DPM_USER_1, Beacon Packet of TIM Processing Start
                // Other(UC, BC/MC) case , DPM sleep daemon would be start the RX Packet
                dpm_ops_tim_rx_handler();
 #endif
            }
            else if (wakeuptype == DPM_NOACK_WAKEUP) /* In case of No ACK, No BCN */
            {
                unsigned char rx_bcn_check_count = 0;
 #define MAX_NO_ACK_BCN_CNT_SLEEP    10

                /* We have to process the connection loss by Null keepavlie */
                /* So we should wait enoughly */
                /* Normally, maximum DTIM Count of APs  is 3 , for sync so le't do wait 300msec */
                while (1)
                {
                    /* If currently bcn is received, break */
                    if (RM_WIFI_dpm_get_cur_bcn_count() > 0)
                    {
                        break;
                    }
                    /* total 100ms sleep */
                    else if (rx_bcn_check_count++ > MAX_NO_ACK_BCN_CNT_SLEEP)
                    {
                        break;
                    }
                    else
                    {
                        vTaskDelay(portCONVERT_MS_2_TICKS(100));
                    }
                }
            }
            else if (wakeuptype == DPM_DEAUTH_WAKEUP) /* TIM status is Deauth */
            {
                /* It means, TIM received the Deauth packet */
                /* So need reconnect */

                // OAL_MSLEEP(10);    /* Sleep for Supplicant initialize */
                while (1)
                {
                    if (is_dpm_supplicant_done())
                    {
                        break;
                    }

                    vTaskDelay(portCONVERT_MS_2_TICKS(10));
                }

 #if 0

                /* For Fast reconnect processing */
                if (dpm_ops_tim_rx_handler() == false)
                {
                    /* If Deauth/Deassoc mgmt packet is not exist, run the connection lost event */
                    reconnect_try = true;
                }

 #else

                /* If Deauth/Deassoc mgmt packet is not exist, run the connection lost event */
                reconnect_try = true;
 #endif
            }
            else if (wakeuptype == DPM_TIM_ERR_WAKEUP) /* TIM status is NO DPM */
            {
                /* It means, TIM has something wrong and fault */
                /* So need reboot */
                reboot_func(SYS_REBOOT);
            }
            else if (wakeuptype == DPM_TCP_KA_TIMEOUT_WAKEUP) /* TIM status is TCP_KA_TIMEOUT */
            {
                RM_PMGR_W_socket_dpm_tcpka_update_pcb();
            }
        }

        // printf(YELLOW_COLOR " [%s] reconnect_try:%d \n" CLEAR_COLOR, __func__, reconnect_try);

        /* abnormal dpm full booting --> connection lost --> reconnect */
        if (reconnect_try)
        {
            vTaskDelay(portCONVERT_MS_2_TICKS(30));
            fc80211_connection_loss();
        }
    }
#endif                                 /* CFG_PMGR */

    return pdPASS;
}

struct netif * net_get_netif (int iface_index)
{
    if (iface_index == WLAN0_IFACE)
    {
        return &wlan0_iface;
    }
    else if (iface_index == WLAN1_IFACE)
    {
        return &wlan1_iface;
    }

    return NULL;
}

/* EOF */
