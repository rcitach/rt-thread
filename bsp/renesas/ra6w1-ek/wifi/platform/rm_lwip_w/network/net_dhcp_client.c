/**
 ****************************************************************************************
 *
 * @file net_dhcp_client.c
 *
 * @brief DHCP Client handling module
 *
 * Copyright (c) 2016-2022 Renesas Electronics. All rights reserved.
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

#include "lwipopts.h"

#if LWIP_DHCP
#include "lwip/dhcp.h" /* For LWIP_DHCP of lwipopts_freertos.h */
#include "rm_lwip_w_cfg.h" /* For RM_LWIP_W_CLEANED */
#include "net_dhcp_client.h"
#if CFG_PMGR
#include "rm_pmgr_w_instance.h"
#include "rm_dhcp.h"
#else
#include "defs.h"
#endif /* CFG_PMGR */

#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"


extern u32_t dhcp_get_timeout_value(const struct netif *netif, int time_type);
#if CFG_PMGR
extern void RM_WIFI_dpm_arp_filter_set(unsigned long accept_addr, unsigned long subnet_addr);
#endif /* CFG_PMGR */
extern int ra6w1_network_main_get_netmode(int iface);
extern unsigned int ra6w1_network_main_check_ip_addr(unsigned char iface);
extern void dns_setserver(u8_t numdns, const ip_addr_t *dnsserver);
extern const ip_addr_t * dns_getserver(u8_t numdns);
extern int set_dns_addr(int iface, char *ip_addr);

#if CFG_PMGR
int dpm_clr_dhcpc_info(void)
{
#ifdef __SUPPORT_IPV4__
    dpm_supp_ip_info_t     *dpm_ip_info;

    if (RM_PMGR_W_rtm_exist() == pdFAIL) {
        return pdFAIL;
    }

    RM_PMGR_W_rtm_static_get(RTM_STATIC_KEY_SUPPL_IP_INFO_PTR, NULL, NULL, (void**)(&dpm_ip_info));

    memset(dpm_ip_info, 0, sizeof(dpm_supp_ip_info_t));
#endif // __SUPPORT_IPV4__
    return pdPASS;
}
#endif /* CFG_PMGR */

#if CFG_PMGR
int dpm_save_dhcpc_info(void)
{
#if	defined (__SUPPORT_IPV4__)
    dpm_supp_ip_info_t     *dpm_ip_info;
    struct netif *netif;
    struct dhcp *dhcp_client;

    unsigned long    ip_addr, net_mask, gateway;
    unsigned long    dhcp_server_ip_addr, dns_server;
    int get_tid;

    // Network interface
    netif = netif_get_by_index(RM_WIFI_NETIF_INDEX(WLAN0_IFACE));
    if (netif == NULL) {
        printf("[%s] Wrong network_interface !!!\n", __func__);
        return -1;
    }

    // DHCP client
    dhcp_client = netif_dhcp_data(netif);
    if (dhcp_client == NULL) {
        printf("[%s] Wrong DHCP data !!!\n", __func__);
        return -1;
    }

    if (RM_PMGR_W_rtm_exist() == pdFAIL) {
        return -1;
    }

    RM_PMGR_W_rtm_static_get(RTM_STATIC_KEY_SUPPL_IP_INFO_PTR, NULL, NULL, (void**)(&dpm_ip_info));

    // IP address
    ip_addr = ip4_addr_get_u32(&dhcp_client->offered_ip_addr);
    if (ip_addr == IPADDR_ANY || ip_addr == IPADDR_NONE) {
        return -1;
    }
    
    dpm_ip_info->dpm_ip_addr = ip_addr;

    // Netmask
    net_mask = ip4_addr_get_u32(&dhcp_client->offered_sn_mask);
    dpm_ip_info->dpm_netmask = net_mask;

    romac4rtos_set_ipv4(ip_addr, net_mask);

    // Gateway
    gateway = ip4_addr_get_u32(&dhcp_client->offered_gw_addr);
    dpm_ip_info->dpm_gateway = gateway;
    
    // DNS Server address #0
#ifdef RM_LWIP_W_CLEANED
    const ip_addr_t *dns_ip_addr;
    dns_ip_addr = dns_getserver(0);
#if defined (__SUPPORT_IPV4__) && defined (__SUPPORT_IPV6__)
    dns_server = ip4_addr_get_u32(&dns_ip_addr->u_addr.ip4);   // TEMP_FOR_COMPILE
#elif defined (__SUPPORT_IPV4__)
    dns_server = ip4_addr_get_u32(dns_ip_addr);
#endif
#else
#if defined (__SUPPORT_IPV4__) && defined (__SUPPORT_IPV6__)
    dns_server = ip4_addr_get_u32(&dhcp_client->dns_ip_addr.u_addr.ip4);	// TEMP_FOR_COMPILE
#elif defined (__SUPPORT_IPV4__)
    dns_server = ip4_addr_get_u32(&dhcp_client->dns_ip_addr);
#endif
#endif /* RM_LWIP_W_CLEANED */
    dpm_ip_info->dpm_dns_addr[0] = dns_server;

    // DNS Server address #1
    dpm_ip_info->dpm_dns_addr[1] = 0;    

    dpm_ip_info->dpm_lease = (unsigned long)dhcp_get_timeout_value(netif, OFFERED_T0_LEASE);    // Lease Time
    dpm_ip_info->dpm_renewal = (unsigned long)dhcp_get_timeout_value(netif, OFFERED_T1_RENEW);    // Renew Time
    dpm_ip_info->dpm_timeout = (unsigned long)dhcp_get_timeout_value(netif, OFFERED_T2_REBIND);    // Rebind Time

    // DHCP Server address
    //dhcp_server_ip_addr = ip4_addr_get_u32(&dhcp_client->server_ip_addr);
#if defined (__SUPPORT_IPV4__) && defined (__SUPPORT_IPV6__)
    dhcp_server_ip_addr =  ip4_addr_get_u32(&dhcp_client->server_ip_addr.u_addr.ip4);	// TEMP_FOR_COMPILE
#elif defined (__SUPPORT_IPV4__)
    dhcp_server_ip_addr = ip4_addr_get_u32(&dhcp_client->server_ip_addr);
#endif
    dpm_ip_info->dpm_dhcp_server_ip = dhcp_server_ip_addr;

    /* It is judged whether it is infinite lease time.  */
    if (dpm_ip_info->dpm_lease < DHCP_INFINITE_LEASE) {
        if (RM_PMGR_W_dpm_timer_remaining_msec_get_by_tid(TID_U_DHCP_CLIENT) > 0) {
            printf("[%s] Cancel DHCP renew timer. Manual renew...\n", __func__);
            RM_PMGR_W_dpm_timer_delete_by_tid(TID_U_DHCP_CLIENT);
        }

        get_tid = RM_PMGR_W_dpm_timer_create_by_tid((unsigned int)dpm_ip_info->dpm_renewal*1000,
                                     NET_IFCONFIG,
                                     TID_U_DHCP_CLIENT,
                                     0,
                                     rtc_dhcp_renew_timeout);

    } else if (dpm_ip_info->dpm_lease == DHCP_INFINITE_LEASE) {
        get_tid = TID_U_DHCP_CLIENT;
    }

    if (dpm_ip_info->dpm_ip_addr == IPADDR_ANY || dpm_ip_info->dpm_ip_addr == IPADDR_NONE) {
        printf("[%s] DPM IP addr(%x) is wrong\n", __func__ , (unsigned int)(dpm_ip_info->dpm_ip_addr));
        return -1;
    }

    RM_WIFI_dpm_arp_filter_set(dpm_ip_info->dpm_ip_addr, dpm_ip_info->dpm_netmask);

    return get_tid;
#else
		//return 0;
#endif // __SUPPORT_IPV4__
}

int dpm_restore_dhcpc_info(void)
{
#if	defined (__SUPPORT_IPV4__)
    dpm_supp_ip_info_t     *dpm_ip_info;
    struct netif *netif;
    struct dhcp *dhcp_client;

    // Network interface
    netif = netif_get_by_index(RM_WIFI_NETIF_INDEX(WLAN0_IFACE));
    if (netif == NULL) {
        printf("[%s] Wrong network_interface !!!\n", __func__);
        return pdFAIL;
    }

    // DHCP client
    dhcp_client = netif_dhcp_data(netif);
    if (dhcp_client == NULL) {
        printf("[%s] Wrong DHCP data !!!\n", __func__);
        return pdFAIL;
    }

    if (RM_PMGR_W_rtm_exist() == pdFAIL) {
        return pdFAIL;
    }

    RM_PMGR_W_rtm_static_get(RTM_STATIC_KEY_SUPPL_IP_INFO_PTR, NULL, NULL, (void**)(&dpm_ip_info));

    // IP address
    if (dpm_ip_info->dpm_ip_addr == IPADDR_ANY || dpm_ip_info->dpm_ip_addr == IPADDR_NONE) {
        //printf("[%s] IP addr(%x) is wrong\n", __func__ , dpm_ip_info->dpm_ip_addr);
        return pdFAIL;
    }

    dhcp_client->offered_ip_addr.addr = dpm_ip_info->dpm_ip_addr;

    // DHCP Server address
    if (dpm_ip_info->dpm_dhcp_server_ip == IPADDR_ANY || dpm_ip_info->dpm_dhcp_server_ip == IPADDR_NONE) {
        printf("[%s] DHCP Server IP addr(%x) is wrong\n", __func__ , (unsigned int)(dpm_ip_info->dpm_dhcp_server_ip));
        return pdFAIL;
    }

#if defined (__SUPPORT_IPV4__) && defined (__SUPPORT_IPV6__)
    dhcp_client->server_ip_addr.u_addr.ip4.addr = dpm_ip_info->dpm_dhcp_server_ip;
#elif defined (__SUPPORT_IPV4__)
    dhcp_client->server_ip_addr.addr = dpm_ip_info->dpm_dhcp_server_ip;
#endif // __SUPPORT_IPV4__

#endif // __SUPPORT_IPV4__
    return pdTRUE;
}

void rtc_dhcp_renew_timeout(char *timer_name)
{
    RA6W1_UNUSED_ARG(timer_name);

#ifdef __SUPPORT_IPV4__
    int dpm_sts, dpm_retry_cnt = 0;
    int iface = WLAN0_IFACE;    //iface_select
    struct netif *netif = NULL;

    netif = netif_get_by_index(RM_WIFI_NETIF_INDEX(iface));

    if (   (DHCPCLIENT != get_netmode(iface))
        || (RM_WIFI_dpm_supp_is_connected() != 1)
        || (RM_PMGR_W_dpm_is_wakeup() != 1)) {
        return;
    }

    if (dhcp_get_state(netif) > DHCP_STATE_OFF) {
        return;
    }

dpm_dhcp_clr_retry :
    if (dpm_retry_cnt++ < 5) {
        dpm_sts = RM_PMGR_W_dpm_sleep_ready_clear(timer_name);

        switch (dpm_sts) {
        case DPM_SET_ERR :
            vTaskDelay(portCONVERT_MS_2_TICKS(10));
                
            goto dpm_dhcp_clr_retry;
            break;

        case DPM_SET_ERR_BLOCK :
            /* Don't need try continues */
            break;

        case DPM_SET_OK :
            break;
        }
    }

    dhcp_start(netif);
#endif // __SUPPORT_IPV4__
}

int dpm_get_dhcpc_ipinfo(void)
{
#if	defined (__SUPPORT_IPV4__)
    struct netif *netif;
    struct dhcp *dhcp_client;

    unsigned long    ip_addr;

    // Network interface
    netif = netif_get_by_index(RM_WIFI_NETIF_INDEX(WLAN0_IFACE));
    if (netif == NULL) {
        return 0;
    }

    // DHCP client
    dhcp_client = netif_dhcp_data(netif);
    if (dhcp_client == NULL) {
        return 0;
    }

    // IP address
    ip_addr = ip4_addr_get_u32(&dhcp_client->offered_ip_addr);
    if (ip_addr == IPADDR_ANY || ip_addr == IPADDR_NONE) {
        //printf("[%s] IP addr(%x) is wrong\n", __func__ , ip_addr);
        return 0;
    }
#endif // __SUPPORT_IPV4__

    return 1;
}
#endif /* CFG_PMGR */

#ifdef __SUPPORT_DHCPC_IP_TO_STATIC_IP__
extern int ra6w1_get_temp_staticip_mode(int iface_flag);

int set_dhcpCientIP_to_staticIP(void)
{
#ifdef __SUPPORT_IPV4__
    struct netif *netif = netif_get_by_index(RM_WIFI_NETIF_INDEX(WLAN0_IFACE));

    /* Check the network initialization */
    if (check_net_init(WLAN0_IFACE) != pdPASS || netif == NULL) {
        return pdFAIL;
    }
    
    if (ra6w1_network_main_get_netmode(WLAN0_IFACE) == DHCPCLIENT
        && ra6w1_get_temp_staticip_mode(WLAN0_IFACE)) {

        if (ra6w1_network_main_check_ip_addr(WLAN0_IFACE) == pdTRUE) {
            int        status;
            char    ipaddr_str[IPADDR_LEN];
            char    subnet_str[IPADDR_LEN];
            char    gateway_str[IPADDR_LEN];
#if CFG_WIFI
            unsigned char wait_cnt = 0;
#endif /* CFG_WIFI */

            // DHCP client
            struct dhcp    *dhcp_client = netif_dhcp_data(netif);
            
            while (pdTRUE) {
                if (dhcp_client->state == DHCP_STATE_BOUND && dhcp_client->offered_gw_addr.addr > 0) {
                    strcpy(ipaddr_str, ipaddr_ntoa((ip_addr_t *)(&dhcp_client->offered_ip_addr)));
                    strcpy(subnet_str, ipaddr_ntoa((ip_addr_t *)(&dhcp_client->offered_sn_mask)));
                    strcpy(gateway_str, ipaddr_ntoa((ip_addr_t *)(&dhcp_client->offered_gw_addr)));
                    break;
                } else {
#if CFG_WIFI
#if CFG_PMGR
                    if ((RM_WIFI_dpm_supp_is_connected() == pdPASS) && (wait_cnt % 300 == 0)) {    // 3 sec.
#else
                    if ((rm_wifi_is_wpa_state(WPA_COMPLETED, 0) == FSP_SUCCESS) && (wait_cnt % 300 == 0)) {
#endif /* CFG_PMGR */
                        printf("[%s] Waiting for DHCP_Client's BOUND.\n", __func__);
                        wait_cnt = 0;
                    }

                    // DHCP_Dcline
#if CFG_PMGR
                    if ((RM_WIFI_dpm_supp_is_connected() == pdFAIL)
#else
                    if ((rm_wifi_is_wpa_state(WPA_COMPLETED, 0) != FSP_SUCCESS)
#endif /* CFG_PMGR */
						|| (dhcp_client->state == DHCP_STATE_INIT)
                        || (dhcp_client->state == DHCP_STATE_BACKING_OFF)) {
                        wait_cnt = 0;
                    }

                    vTaskDelay(portCONVERT_MS_2_TICKS(10));
                    wait_cnt++;
#else
                    vTaskDelay(portCONVERT_MS_2_TICKS(10));
#endif /* CFG_WIFI */
                }
            }

            status = ip_change(WLAN0_IFACE, ipaddr_str, subnet_str, gateway_str, pdTRUE);

            if (status == pdPASS) {
                if (dns_getserver(0) != (ip_addr_t *)NULL) {
                    dns_setserver(0, dns_getserver(0));
                }

#ifdef RM_LWIP_W_CLEANED
                strcpy(ipaddr_str, ipaddr_ntoa(dns_getserver(0)));
#else
                strcpy(ipaddr_str, ipaddr_ntoa(&dhcp_client->dns_ip_addr));
#endif
                set_dns_addr(WLAN0_IFACE, ipaddr_str);

                printf(">>> Change to Static-IP mode with assigned IP address from DHCP mode.\n");
            } else {
                printf("[%s] Error: Set Temporarily static IPaddress.:%s ", __func__, ipaddr_str);
            }

            return status;
        }
    }
#endif // __SUPPORT_IPV4__
    return pdFAIL;
}

#endif /* __SUPPORT_DHCPC_IP_TO_STATIC_IP__ */
#endif /* LWIP_DHCP */

/* EOF */
