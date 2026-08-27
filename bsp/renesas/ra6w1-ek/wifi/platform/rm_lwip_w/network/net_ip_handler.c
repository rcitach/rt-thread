/**
 ****************************************************************************************
 *
 * @file net_ip_handler.c
 *
 * @brief Define for System Running Model
 *
 * Copyright (c) 2023 Renesas Electronics. All rights reserved.
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
#include "bsp_api.h"

#if CFG_WIFI /* Compile only with WiFi stack */

#include "FreeRTOS.h"
#include "lwipopts.h"

#include "sys_feature.h"

#include "lwip/ip_addr.h"
#include "lwip/netif.h"

#include "net_ip_handler.h"
#include "net_dns_client.h"
#include "net_dhcp_client.h"
#if CFG_PMGR
#include "rm_pmgr_w_instance.h"
#endif /* CFG_PMGR */
#include "rm_lwip_w_helper.h"
#include "rm_vee_flash_w_rrq_nvram.h"
#ifdef RM_MAP_PERSISTANT_W
#include "rm_map_persistant_w.h"
#endif

#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"

//extern functions
#if defined ( __SUPPORT_MULTI_IP_IF__ )
extern int get_netinit_state(void);
#endif	// __SUPPORT_MULTI_IP_IF__
#if CFG_PMGR
extern void RM_WIFI_dpm_arp_filter_set(unsigned long accept_addr, unsigned long subnet_addr);
#endif /* CFG_PMGR */
#if 0 // Need to add this function for next item...
extern void sys_app_ip_change_notify(void);
#endif // 0
#ifdef RRQ61XX_CUSTOM_FIXES
extern void dhcp_not_release_and_stop(struct netif * netif);
#endif /* RRQ61XX_CUSTOM_FIXES */

bool lwip_iface_is_up(int iface_flag);

bool lwip_iface_is_up(int iface_flag)
{
    struct netif * p_netif = netif_get_by_index(RM_WIFI_NETIF_INDEX(iface_flag));

    if (p_netif == NULL)
    {
        return pdFAIL;
    }

    return netif_is_up(p_netif);
}

#define NVR_STR_LEN 13
int ip_change(UINT iface, char * ipaddress, char * netmask, char * gateway, UCHAR save)
{
#ifdef __SUPPORT_IPV4__
    struct netif * p_netif = netif_get_by_index(RM_WIFI_NETIF_INDEX(iface));
    char tmp_str[NVR_STR_LEN];

    if (p_netif == NULL || check_net_init(iface) != pdPASS)
    {
        printf("[%s] Failed to get network interface (%d)\n", __func__, iface);
        return pdFAIL;
    }

    /* Check ip, subnet, gateway */
    if ((ip4_addr_netmask_valid(ipaddr_addr(netmask)) == pdFALSE)
            || (ipaddr_addr(ipaddress) == IPADDR_NONE)
            || (ipaddr_addr(gateway) == IPADDR_NONE  && strcmp(gateway, "0.0.0.0") != 0))
    {
        return pdFAIL;
    }
    else
    {
        ip_addr_t tmp_addr;
        unsigned int ip_val, subn_val, gw_val;

        ipaddr_aton(gateway, &tmp_addr);
        ip_val = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&tmp_addr)));
        memset(&tmp_addr, 0x00, sizeof(ip_addr_t));

        ipaddr_aton(ipaddress, &tmp_addr);
        gw_val = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&tmp_addr)));
        memset(&tmp_addr, 0x00, sizeof(ip_addr_t));

        ipaddr_aton(netmask, &tmp_addr);
        subn_val = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&tmp_addr)));

        if ((isvalidIPsubnetRange(gw_val, ip_val, subn_val) == pdFALSE)
                && (strcmp(gateway, "0.0.0.0") != 0))
        {
            return pdFAIL;
        }
    }

    /* Write nvram */
    if (save)
    {
        /* Static IP addres mode */
        memset(tmp_str, 0, NVR_STR_LEN);
        snprintf(tmp_str, sizeof(tmp_str), "%s_%d",
                 NETMODE_WIFI_PROFILE, iface == ETH0_IFACE ? 0 : iface);
        #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                    tmp_str, STATIC_IP);
        #endif

        /* IP Address */
        memset(tmp_str, 0, NVR_STR_LEN);
        snprintf(tmp_str, sizeof(tmp_str), "%s_%d",
                 STATIC_IP_ADDRESS, (iface == ETH0_IFACE ? 0 : iface));
        #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                       tmp_str, ipaddress);
        #endif

        /* Subnet */
        memset(tmp_str, 0, NVR_STR_LEN);
        snprintf(tmp_str, sizeof(tmp_str), "%s_%d",
                 STATIC_IP_NETMASK, (iface == ETH0_IFACE ? 0 : iface));
        #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                       tmp_str, netmask);
        #endif

        /* Gateway */
        memset(tmp_str, 0, NVR_STR_LEN);
        snprintf(tmp_str, sizeof(tmp_str), "%s_%d",
                 STATIC_IP_GATEWAY, (iface == ETH0_IFACE ? 0 : iface));
        #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                       tmp_str, gateway);
        #endif
    }

    /* Check the network initialization */
    if (check_net_init(iface) == pdPASS)
    {
        /* DHCP Client : STOP */
#ifdef __SUPPORT_DHCPC_IP_TO_STATIC_IP__
        extern unsigned char fast_connection_sleep_flag;

        if ((get_netmode(iface) == DHCPCLIENT) && (fast_connection_sleep_flag == pdTRUE))
        {
            set_netmode(iface, STATIC_IP, 0);
#if LWIP_DHCP &&  RRQ61XX_CUSTOM_FIXES
            dhcp_not_release_and_stop(p_netif);
#endif /* LWIP_DHCP  && RRQ61XX_CUSTOM_FIXES*/
        }
        else
#endif /* __SUPPORT_DHCPC_IP_TO_STATIC_IP__ */
        {
#if LWIP_DHCP
            dhcp_release(p_netif);
#endif /* LWIP_DHCP */
            set_netmode(iface, STATIC_IP, save);
        }

#if CFG_PMGR

        if (RM_PMGR_W_dpm_is_enabled() == 1)
        {
            user_dpm_supp_ip_info_t * p_dpm_ip_info = NULL;
            user_dpm_supp_net_info_t * p_dpm_netinfo = NULL;

            RM_PMGR_W_dpm_job_name_clear(NET_IFCONFIG);

            RM_PMGR_W_rtm_static_get(RTM_STATIC_KEY_SUPPL_NET_INFO_PTR, NULL, NULL, (void **)(&p_dpm_netinfo));

            if (p_dpm_netinfo)
            {
                p_dpm_netinfo->net_mode = (char)get_netmode(WLAN0_IFACE);
            }

            RM_PMGR_W_rtm_static_get(RTM_STATIC_KEY_SUPPL_IP_INFO_PTR, NULL, NULL, (void **)(&p_dpm_ip_info));

            if (p_dpm_ip_info)
            {
                p_dpm_ip_info->dpm_dhcp_xid = 0;

                ip_addr_t tmp_addr;
                unsigned int ip_val, subn_val, gw_val;

                ipaddr_aton(gateway, &tmp_addr);
                ip_val = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&tmp_addr)));
                memset(&tmp_addr, 0x00, sizeof(ip_addr_t));

                ipaddr_aton(ipaddress, &tmp_addr);
                gw_val = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&tmp_addr)));
                memset(&tmp_addr, 0x00, sizeof(ip_addr_t));

                ipaddr_aton(netmask, &tmp_addr);
                subn_val = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&tmp_addr)));

                p_dpm_ip_info->dpm_ip_addr = ip_val;
                p_dpm_ip_info->dpm_netmask = subn_val;
                p_dpm_ip_info->dpm_gateway = gw_val;
                p_dpm_ip_info->dpm_dhcp_server_ip = 0;

                RM_WIFI_dpm_arp_filter_set(p_dpm_ip_info->dpm_ip_addr, p_dpm_ip_info->dpm_netmask);
            }
        }

#endif /* CFG_PMGR */

#if defined __SUPPORT_MULTI_IP_IF__

        if (get_run_mode() < WIFI_DEVICE_MODE_EXT_AP_STATION
#if defined __SUPPORT_MESH__
                || get_run_mode() == WIFI_DEVICE_MODE_EXT_MESH_POINT
#endif // __SUPPORT_MESH__
           )
#endif // __SUPPORT_MULTI_IP_IF__
        {
            ip4_addr_t addr;

            ip4addr_aton(ipaddress, &addr);
            netif_set_ipaddr(p_netif, &addr);

            ip4addr_aton(netmask, &addr);
            netif_set_netmask(p_netif, &addr);

            if (strcmp(gateway, "0.0.0.0") == 0)
            {
                netif_set_gw(p_netif, IPADDR_ANY);
            }
            else
            {
                ip4addr_aton(gateway, &addr);

                if (ip4_addr_get_u32(&addr) != IPADDR_ANY)
                {
                    netif_set_gw(p_netif, &addr);
                }
            }
        }

#if defined __SUPPORT_MULTI_IP_IF__
        else
        {
            netif_set_ipaddr(p_netif, ipaddr_addr(ipaddress));
        }

#endif // __SUPPORT_MULTI_IP_IF__

#if 0 // Need to add this function for next item...
        sys_app_ip_change_notify();
#endif // 0
    }

#endif // __SUPPORT_IPV4__

    return pdPASS;
}

UINT get_ip_info(int iface_flag, int info_flag, char * result_str)
{
    struct netif * p_netif = netif_get_by_index(RM_WIFI_NETIF_INDEX(iface_flag));

    if (p_netif == NULL)
    {
        return pdFAIL;
    }

    switch (info_flag)
    {
        case GET_MACADDR:
            snprintf(result_str, NET_INFO_STR_LEN,
                     "%02X:%02X:%02X:%02X:%02X:%02X",
                     p_netif->hwaddr[0],
                     p_netif->hwaddr[1],
                     p_netif->hwaddr[2],
                     p_netif->hwaddr[3],
                     p_netif->hwaddr[4],
                     p_netif->hwaddr[5]);
            break;

#ifdef __SUPPORT_IPV4__

        case GET_IPADDR:
            snprintf(result_str, NET_INFO_STR_LEN, "%s", ipaddr_ntoa(&p_netif->ip_addr));
            break;

        case GET_SUBNET:
            snprintf(result_str, NET_INFO_STR_LEN, "%s", ipaddr_ntoa(&p_netif->netmask));
            break;

        case GET_GATEWAY:
            snprintf(result_str, NET_INFO_STR_LEN, "%s", ipaddr_ntoa(&p_netif->gw));
            break;

        case GET_MTU:
            snprintf(result_str, NET_INFO_STR_LEN, "%d", p_netif->mtu);
            break;

        case GET_DNS:
            /* DNS Server primary */
            snprintf(result_str, NET_INFO_STR_LEN, "%s", ipaddr_ntoa((ip_addr_t *)dns_getserver(0)));
            break;

        case GET_DNS_2ND:
            /* DNS Server 2ndary */
            snprintf(result_str, NET_INFO_STR_LEN, "%s", ipaddr_ntoa((ip_addr_t *)dns_getserver(2)));
            break;
#endif // __SUPPORT_IPV4__

        case GET_IPV6:
#ifdef	__SUPPORT_IPV6__
            printf("=== [%s] Need to write FreeRTOS code for GET_IPV6 ...\n", __func__);
#endif	/* __SUPPORT_IPV6__ */
            break;
    }

    return pdPASS;
}
#endif /* CFG_WIFI */

/* EOF */
