/**
 ****************************************************************************************
 *
 * @file net_network_main.c
 *
 * @brief Network initialize and handle
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

#include "FreeRTOS.h"
#include "osal.h"
#include "lwipopts.h"

#if CFG_WIFI                           /* Compile only with WiFi stack */

 #include <stdbool.h>

 #include "supp_def.h"                 // For feature __LIGHT_SUPPLICANT__

 #include "sys_feature.h"
 #include "net_network_main.h"
 #include "net_dns_client.h"
 #include "net_dhcp_client.h"
 #include "net_ip_handler.h"

 #include "lwip/api.h"
 #include "lwip/ip_addr.h"
 #include "lwip/tcpip.h"
 #include "lwip/udp.h"
 #include "lwip/dhcp.h"
 #include "lwip/dns.h"
 #include "lwip/sockets.h"
 #include "netif/etharp.h"
 #include "dhcpserver.h"

 #if CFG_PMGR
  #define DHCP_FINE_TIMER_RETRY_CNT    500
  #include "sleep_mgmt_regs.h"
  #include "rm_pmgr_w_instance.h"
 #else
  #include "defs.h"
 #endif                                /* CFG_PMGR */
 #include "common_def.h"

 #include "supp_config.h"
 #include "util_api.h"
 #if defined(__SUPPORT_IPV6__)
  #include "lwip/nd6.h"
 #endif

/* When rm_lwip_w is compiled alone
 * RM_LWIP_W flag is enabled the below code
 * helps to undefine the MACRO's which is defined
 * in custom_config_sdk.h*/
 #ifdef RM_LWIP_W
  #undef __SUPPORT_WPS_BTN__
 #endif

 #if defined(__SUPPORT_WPS_BTN__) && defined(__SUPPORT_WIFI_USER_GPIO__)
  #include "rm_wifi_user_app_gpio_handle.h"
 #endif
 #if defined(SIGMA_TEST_ENABLE)
  #include "rm_sigma.h"
 #endif                                // SIGMA_TEST_ENABLE

 #include "rm_lwip_w_helper.h"
 #include "rm_vee_flash_w_rrq_nvram.h"
 #include "rm_dhcp.h"
 #ifdef RM_MAP_PERSISTANT_W
  #include "rm_map_persistant_w.h"
  #include dg_configADNVPARAM_PROJ_FILE
 #endif

/* External functions */
 #if CFG_PMGR
extern int          rwnx_send_me_set_ps_mode(u8 ps_mode);
extern unsigned int RM_WIFI_dpm_ptim_event_get(void);

 #endif                                /* CFG_PMGR */
 #if (dg_configCODE_LOCATION == NON_VOLATILE_IS_FLASH)
extern int        start_system(char init_state, int net_chk_flag);
extern BaseType_t net_stack_init(void);

 #endif
 #if defined(__SUPPORT_IPV6__)
extern bool isIPv6Address(char * str, struct sockaddr_in6 * valid_ip);

 #endif
extern UINT  set_netInit_flag(UINT iface);
extern err_t ethernetif_init(struct netif * netif);

 #if CFG_PMGR
extern void RM_WIFI_dpm_arp_filter_set(unsigned long accept_addr, unsigned long subnet_addr);

 #endif                                /* CFG_PMGR */
extern UINT etharp_exist_defgw_addr(void);

 #if CFG_PMGR
extern int restore_arp_table(void);

 #endif                                /* CFG_PMGR */

extern unsigned char get_last_abnormal_cnt(void);
extern unsigned int  wait_supplicant_done(unsigned int timeout);

extern unsigned char fast_connection_sleep_flag;

/* Global variables */
TaskHandle_t lwip_init_task_handler = NULL;

extern int netmode[];

/* network init flag  wlan0=1, wlan1=2, wlan0+wlan1=3, eth0=4 */
static UCHAR netinit_flag = NO_INIT;
static char  dpm_wakeup_dhcp_client_renew_flag = pdFALSE;

void clear_netInit_flag(void);

unsigned int set_netInit_flag(unsigned int iface)
{
    unsigned int status = pdPASS;

 #if CFG_PMGR
    if (RM_PMGR_W_dpm_is_enabled() == pdTRUE)
    {
        if (WLAN0_IFACE != iface)
        {
            RM_PMGR_W_dpm_disable();
        }
    }
 #endif                                /* CFG_PMGR */

    /* wlan0=1, wlan1=2, wlan0+wlan1=3, eth0=4 */
    if (iface == WLAN0_IFACE)
    {
        netinit_flag = netinit_flag + 1;
    }
    else if (iface == WLAN1_IFACE)
    {
        netinit_flag = netinit_flag + 2;
    }

    /* Check the network initialization */
    status = check_net_init(iface);

    if (status != pdPASS)
    {
        if (iface == WLAN0_IFACE)
        {
            netinit_flag = netinit_flag - 1;
        }
        else if (iface == WLAN1_IFACE)
        {
            netinit_flag = netinit_flag - 2;
        }
    }

    return status;
}

/* Clear netinit_flag */
void clear_netInit_flag (void)
{
    netinit_flag = NO_INIT;
}

long ra6w1_network_main_get_timezone (void)
{
    int use = 0;
 #ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, NVR_KEY_TIMEZONE, &use);
 #endif
    if (use == -1)
    {
        use = 0;
    }

    return use;
}

unsigned int ra6w1_network_main_set_timezone (long timezone)
{
    if (timezone != 0)
    {
 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, NVR_KEY_TIMEZONE, timezone);
 #endif
    }
    else
    {
 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, NVR_KEY_TIMEZONE);
 #endif
    }

    return 0;
}

int ra6w1_network_main_get_netmode (int iface)
{
    return netmode[iface];
}

unsigned int ra6w1_network_main_set_netmode (unsigned char iface, unsigned char mode, unsigned char save)
{
    /* CONFIG NETMODE */
    netmode[iface] = mode;

    if (save)
    {
 #ifdef RM_MAP_PERSISTANT_W
        if (iface == 0)
        {
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                          ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_NETMODE_0,
                                          mode);
        }
        else if (iface == 1)
        {
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                          ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_NETMODE_1,
                                          mode);
        }
        return pdTRUE;
 #endif
    }

    return 0;
}

char get_enable_restart_dhcp_client (void)
{
    return dpm_wakeup_dhcp_client_renew_flag;
}

/* In DPM Wakeup state, it sets whether to restart DHCP Client after WiFi reconnection. */
char set_enable_restart_dhcp_client (char flag)
{
    return dpm_wakeup_dhcp_client_renew_flag = flag;
}

 #define NETIF_REPORT_TYPE_IPV4    0x01
 #define NETIF_REPORT_TYPE_IPV6    0x02
 #ifdef RM_LWIP_W_CLEANED
extern void rm_netif_issue_reports(struct netif * netif, u8_t report_type);

 #else
extern void netif_issue_reports(struct netif * netif, u8_t report_type);

 #endif                                /* RM_LWIP_W_CLEANED */
 #if CFG_PMGR
extern int RM_WIFI_dpm_supp_is_connected(void);

 #endif                                /* CFG_PMGR */

static void ra6w1_network_main_set_invalid_ipv6_state (int iface, struct netif * p_netif)
{
 #if LWIP_DHCP && defined(__SUPPORT_IPV6__)
    int idx = 1;                       // The index 0 is for Link-local IPv6 Address.

    if (p_netif && (get_netmode(iface) == DHCPCLIENT))
    {
        for (idx = 1; idx < LWIP_IPV6_NUM_ADDRESSES; idx++)
        {
            netif_ip6_addr_set_state(p_netif, idx, IP6_ADDR_INVALID);
        }
    }

 #else
    RA6W1_UNUSED_ARG(iface);
    RA6W1_UNUSED_ARG(p_netif);
 #endif
}

void ra6w1_network_main_reconfig_net (int state)
{
 #ifdef __SUPPORT_IPV4__
  #if CFG_PMGR
    int dpm_sts, dpm_retry_cnt = 0;
  #endif                               /* CFG_PMGR */

    struct netif * netif = NULL;

    switch (state)
    {
        case INF_NET_STATION:
        {
            if ((check_net_init(WLAN0_IFACE) == pdPASS) &&
                (
  #if CFG_PMGR
                    !RM_PMGR_W_dpm_is_wakeup()
   #if !defined(__DISABLE_DPM_ABNORM__)
                    || get_last_abnormal_cnt() > 0
   #endif                                                 // !__DISABLE_DPM_ABNORM__
                    ||
  #endif                                                  /* CFG_PMGR */
                    get_enable_restart_dhcp_client() == pdTRUE ||
                    chk_ipaddress(WLAN0_IFACE) == pdFAIL) /* For: Power-on boot, abnormal boot */
                )
            {
                netif = netif_get_by_index(RM_WIFI_NETIF_INDEX(WLAN0_IFACE));

                if ((wifi_netif_status(WLAN0_IFACE) != pdPASS) || (chk_ipaddress(WLAN0_IFACE) == pdPASS))
                {
                    /* === WLAN0_IFACE is DOWN === */
  #if CFG_PMGR
                    if (RM_PMGR_W_dpm_is_enabled() == pdTRUE)
                    {
                        dpm_retry_cnt = 0;

                        do
                        {
                            dpm_sts = RM_PMGR_W_dpm_sleep_ready_clear(NET_IFCONFIG);
                            if (dpm_sts == DPM_SET_OK)
                            {
                                break;
                            }

                            vTaskDelay(portCONVERT_MS_2_TICKS(10));
                        } while (dpm_retry_cnt++ < 5);

                        if (RM_PMGR_W_dpm_timer_remaining_msec_get_by_tid(TID_U_DHCP_CLIENT) > 0)
                        {
                            // printf("[%s] %d DHCPC Cancel\n", __func__, __LINE__);
                            RM_PMGR_W_dpm_timer_delete_by_tid(TID_U_DHCP_CLIENT);
                        }
                    }
  #endif                                                                                                    /* CFG_PMGR */

  #if CFG_WIFI
   #if CFG_PMGR
                    if (RM_WIFI_dpm_supp_is_connected() && (chk_ipaddress(WLAN0_IFACE) == pdPASS))          /* For: cli reassoc */
                    {
   #else
                    if ((rm_wifi_is_wpa_state(WPA_COMPLETED,
                                              0) == FSP_SUCCESS) && (chk_ipaddress(WLAN0_IFACE) == pdPASS)) /* For: cli reassoc */
                    {
   #endif /* CFG_PMGR */
  #else /* CFG_WIFI */
                    if (chk_ipaddress(WLAN0_IFACE) == pdPASS)                                               /* For: cli reassoc */
                    {
  #endif /* CFG_WIFI */
                        if (get_netmode(WLAN0_IFACE) == DHCPCLIENT)
                        {
                            /* DHCPCLIENT */
  #if LWIP_DHCP
                            dhcp_renew(netif);
  #endif                               /* LWIP_DHCP */
                        }
                        else
                        {
  #ifdef RM_LWIP_W_CLEANED
                            rm_netif_issue_reports(netif, NETIF_REPORT_TYPE_IPV4 | NETIF_REPORT_TYPE_IPV6);
  #else
                            netif_issue_reports(netif, NETIF_REPORT_TYPE_IPV4 | NETIF_REPORT_TYPE_IPV6);
  #endif                               /* RM_LWIP_W_CLEANED */
                        }
                    }
                    else
                    {
                        ra6w1_network_main_set_invalid_ipv6_state(WLAN0_IFACE, netif);
                        arp_entry_delete(WLAN0_IFACE); /* ARP table delect all */

  #if LWIP_DHCP
                        if (get_netmode(WLAN0_IFACE) == DHCPCLIENT)
                        {
                            /* DHCPCLIENT */
                            dhcp_stop(netif);
                            dhcp_start(netif);
                        }
  #endif                               /* LWIP_DHCP */
                    }
                }
                else
                {
                    /* === WLAN0_IFACE is UP === */
  #if CFG_PMGR
                    if (RM_PMGR_W_dpm_timer_remaining_msec_get_by_tid(TID_U_DHCP_CLIENT) > 0)
                    {
                        // printf("[%s] %d DHCPC Cancel\n", __func__, __LINE__);
                        RM_PMGR_W_dpm_timer_delete_by_tid(TID_U_DHCP_CLIENT);
                    }

                    /* If you are connected after waking up with the "Wakeup Switch" */
                    RM_PMGR_W_dpm_timer_delete_by_tid(TID_U_ABNORMAL);
  #endif                               /* CFG_PMGR */

                    if (get_netmode(WLAN0_IFACE) == DHCPCLIENT)
                    {
  #if LWIP_DHCP
   #if CFG_PMGR
                        bool ps_mode = 0;

                        rm_wifi_ps_mode_get(&ps_mode);
                        if (ps_mode)
                        {
                            rwnx_send_me_set_ps_mode(0);

                            /* Give a chance for the mac to handle the ps disable */
                            vTaskDelay(portCONVERT_MS_2_TICKS(DHCP_FINE_TIMER_MSECS));
                        }
   #endif

                        /* DHCPCLIENT */
                        /* Send DHCP DISCOVER when reassoc. operation */
                        if (dhcp_get_state(netif) > DHCP_STATE_OFF
   #if CFG_PMGR
    #if !defined(__DISABLE_DPM_ABNORM__)
                            || chk_abnormal_wakeup()
    #endif                             // !__DISABLE_DPM_ABNORM__
   #endif                              /* CFG_PMGR */
                            )
                        {
                            ra6w1_network_main_set_invalid_ipv6_state(WLAN0_IFACE, netif);

                            arp_entry_delete(WLAN0_IFACE); /* ARP table delect all */
                            dhcp_stop(netif);
                        }

                        dhcp_start(netif);
   #if CFG_PMGR
                        if (ps_mode)
                        {
                            struct dhcp * dhcp  = netif_dhcp_data(netif);
                            int           retry = DHCP_FINE_TIMER_RETRY_CNT;

                            /* Retry up to 5 seconds */
                            while (retry && (dhcp->state != DHCP_STATE_BOUND))
                            {
                                dhcp_fine_tmr();
                                vTaskDelay(portCONVERT_MS_2_TICKS(DHCP_FINE_TIMER_MSECS));
                                if (--retry == 0)
                                {
                                    printf("%s:%d retry limit reached but dhcp->state=%d\n",
                                           __func__,
                                           __LINE__,
                                           dhcp->state);
                                }
                            }

                            rwnx_send_me_set_ps_mode(1);
                        }
   #endif
  #endif                               /* LWIP_DHCP */
                    }
                    else
                    {
  #ifdef RM_LWIP_W_CLEANED
                        rm_netif_issue_reports(netif, NETIF_REPORT_TYPE_IPV4 | NETIF_REPORT_TYPE_IPV6);
  #else
                        netif_issue_reports(netif, NETIF_REPORT_TYPE_IPV4 | NETIF_REPORT_TYPE_IPV6);
  #endif                               /* RM_LWIP_W_CLEANED */
                    }
                }
            }

            break;
        }

  #if !defined(__LIGHT_SUPPLICANT__)
        case P2P_NET_GO:
        {
            dhcps_cmd_param * dhcp_param = pvPortMalloc(sizeof(dhcps_cmd_param));
            ip_change(WLAN1_IFACE, P2P_GO_IP_ADDR, P2P_GO_NETMASK, IPADDR_ANY_STR, pdFALSE);
            memset(dhcp_param, 0, sizeof(dhcps_cmd_param));
            dhcp_param->cmd             = DHCP_SERVER_STATE_START;
            dhcp_param->dhcps_interface = WLAN1_IFACE;
            dhcps_run(dhcp_param);
            break;
        }

        case P2P_NET_CLIENT:
        {
            netif = netif_get_by_index(RM_WIFI_NETIF_INDEX(WLAN1_IFACE));
   #if LWIP_DHCP
            dhcp_start(netif);
   #endif                              /* LWIP_DHCP */
            break;
        }

        case P2P_NET_DEVICE:
        {
            unsigned char ret;

            ret = (unsigned char) wifi_netif_status(WLAN1_IFACE);
            if (((ret == 0xFF) || (ret != pdTRUE)) &&
                (chk_ipaddress(WLAN1_IFACE) == pdTRUE))
            {
                arp_entry_delete(WLAN1_IFACE); /* ARP table delect all */
            }

            if (get_netmode(WLAN1_IFACE) == DHCPCLIENT)
            {
                netif = netif_get_by_index(RM_WIFI_NETIF_INDEX(WLAN1_IFACE));
   #if LWIP_DHCP
                dhcp_release_and_stop(netif);
   #endif                              /* LWIP_DHCP */
            }
            else
            {
                dhcps_cmd_param * dhcp_param = pvPortMalloc(sizeof(dhcps_cmd_param));

                memset(dhcp_param, 0, sizeof(dhcps_cmd_param));
                dhcp_param->cmd             = DHCP_SERVER_STATE_STOP;
                dhcp_param->dhcps_interface = WLAN1_IFACE;

                dhcps_run(dhcp_param);
            }

            ip_change(WLAN1_IFACE, IPADDR_ANY_STR, IPADDR_ANY_STR, IPADDR_ANY_STR, pdFALSE);

            break;
        }
  #endif                               // ! __LIGHT_SUPPLICANT__
    }
 #endif                                // __SUPPORT_IPV4__
}

int ra6w1_network_main_get_sysmode (void)
{
    int sysmode = 0;

 #ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_SYS_MODE,
                                 &sysmode);
 #endif

    if (sysmode == -1)
    {
        sysmode = 0;
    }

    return sysmode;
}

int ra6w1_network_main_set_sysmode (int mode)
{
    if ((mode >= WIFI_DEVICE_MODE_EXT_STATION) && (mode <= WIFI_DEVICE_MODE_EXT_P2P_STATION))
    {
 #ifdef RM_MAP_PERSISTANT_W

        return RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                             ENV_GROUP_WIFIPROFILE,
                                             WIFI_PROFILE_SYS_MODE,
                                             mode);
 #endif
    }

    return -1;
}

int ra6w1_get_fast_connection_mode (void)
{
    int fast_connection = 0;

 #ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                 ENV_GROUP_WIFICFG,
                                 NVR_KEY_FST_CONNECT,
                                 &fast_connection);
 #endif                                // RM_MAP_PERSISTANT_W

    if (fast_connection == -1)
    {
        fast_connection = 0;
    }

    return fast_connection;
}

int ra6w1_set_fast_connection_mode (int mode)
{
 #ifdef RM_MAP_PERSISTANT_W
    if ((mode >= 0) && (mode <= 1))
    {
        if (mode == 0)
        {
            RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_FST_CONNECT);

            return 0;
        }
        else
        {
            return RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_WIFICFG,
                                                 NVR_KEY_FST_CONNECT,
                                                 mode);
        }
    }
 #endif                                // RM_MAP_PERSISTANT_W

    return -1;
}

void ra6w1_network_main_change_iface_updown (unsigned int iface, unsigned int flag)
{
 #if LWIP_DHCP

    /* dhcp_client run on just "wlan0" interface on ra6wx */
    struct netif * netif = netif_get_by_index(RM_WIFI_NETIF_INDEX(iface));

    RA6W1_UNUSED_ARG(flag);

    if ((iface == WLAN0_IFACE) && (get_netmode(iface) == DHCPCLIENT) && netif_is_up(netif))
    {
        if (netif != NULL)
        {
            if (dhcp_renew(netif) != ERR_OK)
            {
                printf("DHCP Client Renew Fail\n");
            }
        }
        else
        {
            printf("Network Interface is NULL!!\n");
        }
    }
 #endif                                /* LWIP_DHCP */
}

 #ifdef __SUPPORT_DHCPC_IP_TO_STATIC_IP__
void ra6w1_set_temp_staticip_mode (int mode, int save)
{
    if ((mode >= 0) && (mode <= 1))
    {
        if (mode == pdTRUE)
        {
            if (save == pdTRUE)
            {
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                              ENV_GROUP_SYSCFG,
                                              ENV_KEY_TEMP_STATIC_IP,
                                              pdTRUE);
  #endif
            }
            else
            {
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                              ENV_GROUP_SYSCFG,
                                              ENV_KEY_TEMP_STATIC_IP,
                                              pdTRUE);
  #endif
            }
        }
        else
        {
            if (save == pdTRUE)
            {
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, ENV_KEY_TEMP_STATIC_IP);
  #endif                               // Temporarily use a static IPaddress.
            }
            else
            {
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, ENV_KEY_TEMP_STATIC_IP);
  #endif
            }
        }
    }
}

int ra6w1_get_temp_staticip_mode (int iface_flag)
{
    int t_staticIP = pdFALSE;

    if (iface_flag == WLAN0_IFACE)
    {
  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                     ENV_GROUP_SYSCFG,
                                     ENV_KEY_TEMP_STATIC_IP,
                                     &t_staticIP);
  #endif
        if (t_staticIP < pdTRUE)
        {
            t_staticIP = pdFALSE;
        }
    }

    return t_staticIP;
}

 #endif                                // __SUPPORT_DHCPC_IP_TO_STATIC_IP__

void ra6w1_network_main_check_network (int iface_flag, int simple)
{
    struct netif * netif = netif_get_by_index(RM_WIFI_NETIF_INDEX(iface_flag));

 #if defined(__SUPPORT_IPV4__) || defined(__SUPPORT_IPV6__)
    char * p_str_ip_addr = NULL;
    size_t idx           = 0;
 #endif

 #if defined(__SUPPORT_IPV6__)
    char   str_ip_addr[IP6ADDR_STRLEN_MAX + 1] = {0x00, };
    size_t str_ip_addr_len = 0;
 #elif defined(__SUPPORT_IPV4__)
    char str_ip_addr[IP4ADDR_STRLEN_MAX + 1] = {0x00, };
 #endif

    /* Check the network initialization */
    if (check_net_init(iface_flag) != pdPASS)
    {
        return;
    }

    if (netif == NULL)
    {
        if (!simple)
        {
            printf("Interface(%s%d) NULL \n",
                   iface_flag == ETH0_IFACE ? "eth" : "wlan",
                   iface_flag == WLAN1_IFACE ? WLAN1_IFACE : WLAN0_IFACE);
        }

        return;
    }

    printf("\nWLAN%d:\n\n", iface_flag);

    /* MAC Address */
    printf("   MAC Address . . . . . . . . : %02X:%02X:%02X:%02X:%02X:%02X\n",
           netif->hwaddr[0],
           netif->hwaddr[1],
           netif->hwaddr[2],
           netif->hwaddr[3],
           netif->hwaddr[4],
           netif->hwaddr[5]);

 #ifdef __SUPPORT_IPV4__
  #ifdef __SUPPORT_DHCPC_IP_TO_STATIC_IP__
    printf("   NetMode . . . . . . . . . . : %s%s\n",
           ((get_netmode(iface_flag) == STATIC_IP) ? "Static IP" : "DHCP Client"),
           ((ra6w1_get_temp_staticip_mode(iface_flag) == pdTRUE) ? "(TEMP)" : ""));
  #else
    printf("   NetMode . . . . . . . . . . : %s\n",
           ((get_netmode(iface_flag) == STATIC_IP) ? "Static IP" : "DHCP Client"));
  #endif                               // __SUPPORT_DHCPC_IP_TO_STATIC_IP__
 #endif                                // __SUPPORT_IPV4__

 #if defined(SIGMA_TEST_ENABLE)
    PRINT_SIGMA_CMD("\nWLAN%d:\n\n", iface_flag);

    /* MAC Address */
    PRINT_SIGMA_CMD("   MAC Address . . . . . . . . : %02X:%02X:%02X:%02X:%02X:%02X\n", netif->hwaddr[0],
                    netif->hwaddr[1], netif->hwaddr[2], netif->hwaddr[3], netif->hwaddr[4], netif->hwaddr[5]);

  #ifdef __SUPPORT_IPV4__
   #ifdef __SUPPORT_DHCPC_IP_TO_STATIC_IP__
    PRINT_SIGMA_CMD("   NetMode . . . . . . . . . . : %s%s\n",
                    get_netmode(iface_flag) == STATIC_IP ? "Static IP" : "DHCP Client",
                    ra6w1_get_temp_staticip_mode(iface_flag) == pdTRUE ? "(TEMP)" : "");
   #else
    PRINT_SIGMA_CMD("   NetMode . . . . . . . . . . : %s\n",
                    get_netmode(iface_flag) == STATIC_IP ? "Static IP" : "DHCP Client");
   #endif                              // __SUPPORT_DHCPC_IP_TO_STATIC_IP__
  #endif                               // __SUPPORT_IPV4__
 #endif                                // SIGMA_TEST_ENABLE

 #if defined(__SUPPORT_IPV6__)         // __SUPPORT_IPV6__

    /* IPv6 Information */

    memset(str_ip_addr, 0x00, sizeof(str_ip_addr));

    if (!ip_addr_isany_val(netif->ip6_addr[0]))
    {
        p_str_ip_addr = ipaddr_ntoa_r((ip_addr_t *) &netif->ip6_addr, str_ip_addr, sizeof(str_ip_addr));
        if ((p_str_ip_addr == str_ip_addr) && (str_ip_addr_len = strlen(str_ip_addr)))
        {
            for (idx = 0; idx < str_ip_addr_len; idx++)
            {
                if ((str_ip_addr[idx] >= 0x41) && (str_ip_addr[idx] <= 0x46))
                {
                    str_ip_addr[idx] += 0x20;
                }
            }

            printf("   Link-local IPv6 Address . . : %s\n", str_ip_addr);
  #if defined(SIGMA_TEST_ENABLE)
            PRINT_SIGMA_CMD("   Link-local IPv6 Address . . : %s\n", str_ip_addr);
  #endif                               // SIGMA_TEST_ENABLE
        }
    }

    /* Prefix Address */
    for (idx = 1; idx < LWIP_IPV6_NUM_ADDRESSES; idx++)
    {
        memset(str_ip_addr, 0x00, sizeof(str_ip_addr));
        p_str_ip_addr = ipaddr_ntoa_r((ip_addr_t *) &netif->ip6_addr[idx], str_ip_addr, sizeof(str_ip_addr));
        if ((p_str_ip_addr == str_ip_addr) && (str_ip_addr_len = strlen(str_ip_addr)))
        {
            if ((str_ip_addr[0] != '0') && (str_ip_addr[0] != ':'))
            {
                for (size_t i = 0; i < str_ip_addr_len; i++)
                {
                    if ((str_ip_addr[i] >= 0x41) && (str_ip_addr[i] <= 0x46))
                    {
                        str_ip_addr[i] += 0x20;
                    }
                }

                printf("   IPv6 Address[%d] . . . . . . : %s\n", idx, str_ip_addr);
  #if defined(SIGMA_TEST_ENABLE)
                PRINT_SIGMA_CMD("   IPv6 Address[%d] . . . . . . : %s\n", idx, str_ip_addr);
  #endif                               // SIGMA_TEST_ENABLE
            }
        }
    }

  #if defined(__SUPPORT_IPV4__)        // __SUPPORT_IPV4__ && __SUPPORT_IPV6__

    /* IP Address */
    memset(str_ip_addr, 0x00, sizeof(str_ip_addr));
    p_str_ip_addr = ipaddr_ntoa_r((ip_addr_t *) &netif->ip_addr.u_addr.ip4, str_ip_addr, sizeof(str_ip_addr));
    if ((p_str_ip_addr == str_ip_addr) && (str_ip_addr_len = strlen(str_ip_addr)))
    {
        printf("   IPv4 Address. . . . . . . . : %s\n", str_ip_addr);
    }

    /* Subnet */
    memset(str_ip_addr, 0x00, sizeof(str_ip_addr));
    p_str_ip_addr = ipaddr_ntoa_r((ip_addr_t *) &netif->netmask.u_addr.ip4, str_ip_addr, sizeof(str_ip_addr));
    if ((p_str_ip_addr == str_ip_addr) && (str_ip_addr_len = strlen(str_ip_addr)))
    {
        printf("   Subnet Mask . . . . . . . . : %s\n", str_ip_addr);
    }

    /* Gateway */
    memset(str_ip_addr, 0x00, sizeof(str_ip_addr));
    p_str_ip_addr = ipaddr_ntoa_r((ip_addr_t *) &netif->gw.u_addr.ip4, str_ip_addr, sizeof(str_ip_addr));
    if ((p_str_ip_addr == str_ip_addr) && (str_ip_addr_len = strlen(str_ip_addr)))
    {
        printf("   Default Gateway . . . . . . : %s\n", str_ip_addr);
    }

    /* MTU */
    printf("   MTU . . . . . . . . . . . . : %d\n", netif->mtu);

   #if defined(SIGMA_TEST_ENABLE)

    /* IP Address */
    PRINT_SIGMA_CMD("   IPv4 Address. . . . . . . . : %s\n", ipaddr_ntoa((ip_addr_t *) &netif->ip_addr.u_addr.ip4));

    /* Subnet */
    PRINT_SIGMA_CMD("   Subnet Mask . . . . . . . . : %s\n", ipaddr_ntoa((ip_addr_t *) &netif->netmask.u_addr.ip4));

    /* Gateway */
    PRINT_SIGMA_CMD("   Default Gateway . . . . . . : %s\n", ipaddr_ntoa((ip_addr_t *) &netif->gw.u_addr.ip4));

    /* MTU */
    PRINT_SIGMA_CMD("   MTU . . . . . . . . . . . . : %d\n", netif->mtu);
   #endif                              // __SUPPORT_IPV4__
  #endif                               // SIGMA_TEST_ENABLE
 #elif defined(__SUPPORT_IPV4__)       // __SUPPORT_IPV4__

    /* IP Address */
    memset(str_ip_addr, 0x00, sizeof(str_ip_addr));
    p_str_ip_addr = ipaddr_ntoa_r((ip_addr_t *) &netif->ip_addr, str_ip_addr, sizeof(str_ip_addr));
    if ((p_str_ip_addr == str_ip_addr) && (str_ip_addr_len = strlen(str_ip_addr)))
    {
        printf("   IP Address. . . . . . . . . : %s\n", str_ip_addr);
    }

    /* Subnet */
    memset(str_ip_addr, 0x00, sizeof(str_ip_addr));
    p_str_ip_addr = ipaddr_ntoa_r((ip_addr_t *) &netif->netmask, str_ip_addr, sizeof(str_ip_addr));
    if ((p_str_ip_addr == str_ip_addr) && (str_ip_addr_len = strlen(str_ip_addr)))
    {
        printf("   Mask. . . . . . . . . . . . : %s\n", str_ip_addr);
    }

    /* Gateway */
    memset(str_ip_addr, 0x00, sizeof(str_ip_addr));
    p_str_ip_addr = ipaddr_ntoa_r((ip_addr_t *) &netif->gw, str_ip_addr, sizeof(str_ip_addr));
    if ((p_str_ip_addr == str_ip_addr) && (str_ip_addr_len = strlen(str_ip_addr)))
    {
        printf("   Gateway . . . . . . . . . . : %s\n", );
    }

    /* MTU */
    printf("   MTU . . . . . . . . . . . . : %d\n", netif->mtu);

  #if defined(SIGMA_TEST_ENABLE)

    /* IP Address */
    PRINT_SIGMA_CMD("   IP Address. . . . . . . . . : %s\n", ipaddr_ntoa(&netif->ip_addr));

    /* Subnet */
    PRINT_SIGMA_CMD("   Mask. . . . . . . . . . . . : %s\n", ipaddr_ntoa(&netif->netmask));

    /* Gateway */
    PRINT_SIGMA_CMD("   Gateway . . . . . . . . . . : %s\n", ipaddr_ntoa(&netif->gw));

    /* MTU */
    PRINT_SIGMA_CMD("   MTU . . . . . . . . . . . . : %d\n", netif->mtu);
  #endif                               // SIGMA_TEST_ENABLE
 #endif                                // __SUPPORT_IPV6__ && __SUPPORT_IPV4__

    if (netif_is_up(netif))
    {
        int is_printed = pdFALSE;
 #if defined(__SUPPORT_IPV4__)
  #if LWIP_DHCP
        if (get_netmode(((iface_flag == 1) ? WLAN1_IFACE : WLAN0_IFACE)) == DHCPCLIENT)
        {
            struct dhcp * dhcp = netif_dhcp_data(netif);

            if (dhcp->offered_t0_lease == 0xFFFFFFFFUL)
            {
                printf("   IP Lease Time . . . . . . . : Forever\n");
            }
            else if (dhcp->offered_t0_lease)
            {
                printf("   IP Lease Time . . . . . . . : %02luh %02lum %02lus\n",
                       dhcp->offered_t0_lease / 3600,                            /* Hour */
                       dhcp->offered_t0_lease / 60 % 60,                         /* Minute */
                       dhcp->offered_t0_lease % 60);                             /* Second */

                printf("   IP Renewal Time . . . . . . : %02luh %02lum %02lus\n",
                       dhcp->offered_t1_renew / 3600,                            /* Hour */
                       dhcp->offered_t1_renew / 60 % 60,                         /* Minute */
                       dhcp->offered_t1_renew % 60);                             /* Second */

                printf("   Timeout . . . . . . . . . . : %02uh %02um %02us\n",
                       (dhcp->t1_renew_time * DHCP_COARSE_TIMER_SECS) / 3600,    /* Hour */
                       (dhcp->t1_renew_time * DHCP_COARSE_TIMER_SECS) / 60 % 60, /* Minute */
                       (dhcp->t1_renew_time * DHCP_COARSE_TIMER_SECS) % 60);     /* Sec. */
            }

   #if defined(SIGMA_TEST_ENABLE)
            if (dhcp->offered_t0_lease == 0xFFFFFFFFUL)
            {
                PRINT_SIGMA_CMD("   IP Lease Time . . . . . . . : Forever\n");
            }
            else if (dhcp->offered_t0_lease)
            {
                PRINT_SIGMA_CMD("   IP Lease Time . . . . . . . : %02uh %02um %02us\n",
                                dhcp->offered_t0_lease / 3600,                            /* Hour */
                                dhcp->offered_t0_lease / 60 % 60,                         /* Minute */
                                dhcp->offered_t0_lease % 60);                             /* Second */

                PRINT_SIGMA_CMD("   IP Renewal Time . . . . . . : %02uh %02um %02us\n",
                                dhcp->offered_t1_renew / 3600,                            /* Hour */
                                dhcp->offered_t1_renew / 60 % 60,                         /* Minute */
                                dhcp->offered_t1_renew % 60);                             /* Second */

                PRINT_SIGMA_CMD("   Timeout . . . . . . . . . . : %02uh %02um %02us\n",
                                (dhcp->t1_renew_time * DHCP_COARSE_TIMER_SECS) / 3600,    /* Hour */
                                (dhcp->t1_renew_time * DHCP_COARSE_TIMER_SECS) / 60 % 60, /* Minute */
                                (dhcp->t1_renew_time * DHCP_COARSE_TIMER_SECS) % 60);     /* Sec. */
            }
   #endif // SIGMA_TEST_ENABLE
        }
  #endif /* LWIP_DHCP */

        if (iface_flag == WLAN0_IFACE)
        {
            /* DNS */
            for (idx = 0; idx < DNS_MAX_SERVERS; idx++)
            {
                memset(str_ip_addr, 0x00, sizeof(str_ip_addr));
                p_str_ip_addr = ipaddr_ntoa_r((ip_addr_t *) dns_getserver(idx), str_ip_addr, sizeof(str_ip_addr));
                if ((p_str_ip_addr == str_ip_addr) && (str_ip_addr_len = strlen(str_ip_addr)))
                {
                    if (is_in_valid_ip_class(str_ip_addr))
                    {
                        if (!is_printed)
                        {
                            printf("   DNS Servers . . . . . . . . : %s\n", str_ip_addr);
  #if defined(SIGMA_TEST_ENABLE)
                            PRINT_SIGMA_CMD("   DNS . . . . . . . . . . . . : %s\n", str_ip_addr);
  #endif                               // SIGMA_TEST_ENABLE
                            is_printed = pdTRUE;
                        }
                        else
                        {
                            printf("                                 %s\n", str_ip_addr);
  #if defined(SIGMA_TEST_ENABLE)
                            PRINT_SIGMA_CMD("                                 %s\n", str_ip_addr);
  #endif                               // SIGMA_TEST_ENABLE
                        }
                    }
                }
            }
        }
 #endif                                // __SUPPORT_IPV4__

 #if defined(__SUPPORT_IPV6__)
        if (iface_flag == WLAN0_IFACE)
        {
  #ifdef RRQ61XX_CUSTOM_FIXES

            /* Gateway */
            if (nd6_get_default_gateway())
            {
                printf("   Default IPv6 Gateway. . . . : %s\n", nd6_get_default_gateway());
            }
  #endif                               /* RRQ61XX_CUSTOM_FIXES */

            /* DNS Server */
            for (idx = 0; idx < DNS_MAX_SERVERS; idx++)
            {
                memset(str_ip_addr, 0x00, sizeof(str_ip_addr));
                p_str_ip_addr = ipaddr_ntoa_r((ip_addr_t *) dns_getserver(idx), str_ip_addr, sizeof(str_ip_addr));
                if ((p_str_ip_addr == str_ip_addr) && (str_ip_addr_len = strlen(str_ip_addr)))
                {
                    if ((isIPv6Address(str_ip_addr, NULL)) && (str_ip_addr[0] != '0'))
                    {
                        if (!is_printed)
                        {
                            printf("   DNS Servers . . . . . . . . : %s\n", str_ip_addr);
                            is_printed = pdTRUE;
                        }
                        else
                        {
                            printf("                                 %s\n", str_ip_addr);
                        }
                    }
                }
            }
        }
 #endif                                /* __SUPPORT_IPV6__ */

        // netif->mib2_counters->ifoutnucastpkts
        printf("\n");
    }

    /* Restore preemption.  */
}

int ra6w1_network_main_check_net_init (int iface)
{
    if (iface == NONE_IFACE)
    {
        iface = WLAN0_IFACE;
    }

    if (((iface == WLAN0_IFACE) && ((netinit_flag == WLAN0_INIT) || (netinit_flag == CONCURRENT_INIT))) ||
        ((iface == WLAN1_IFACE) && ((netinit_flag == WLAN1_INIT) || (netinit_flag == CONCURRENT_INIT))))
    {
        return pdPASS;
    }

    return pdFAIL;
}

unsigned int ra6w1_network_main_check_ip_addr (unsigned char iface)
{
    struct netif * netif = netif_get_by_index(RM_WIFI_NETIF_INDEX(iface));

 #if defined(__SUPPORT_IPV4__) && defined(__SUPPORT_IPV6__)
    int           chk_ip4_addr = 0, chk_ip6_addr = 0;
    unsigned long ip_address;
    ip_address = netif->ip_addr.u_addr.ip4.addr;

    if ((ip_address != IPADDR_ANY) && (ip_address != IPADDR_NONE)) // Check IPv4
    {
        chk_ip4_addr = 1;
    }

    for (int idx = 0; idx < LWIP_IPV6_NUM_ADDRESSES; idx++) // Check IPv6
    {
        if ((netif->ip6_addr[idx].u_addr.ip6.addr[0]) &&
            (idx >= 1) &&
            (!ip6_addr_isinvalid(netif_ip6_addr_state(netif, idx))))
        {
            chk_ip6_addr = 1;
        }
    }

    if (chk_ip4_addr)
    {
        return pdTRUE;
    }

    if (chk_ip6_addr)
    {
        return pdTRUE;
    }

 #elif defined(__SUPPORT_IPV4__)
    unsigned long ip_address;

    ip_address = ip4_addr_get_u32(&netif->ip_addr);
    if ((ip_address != IPADDR_ANY) && (ip_address != IPADDR_NONE))
    {
        return pdTRUE;
    }

 #elif defined(__SUPPORT_IPV6__)
    for (int idx = 0; idx < LWIP_IPV6_NUM_ADDRESSES; idx++)
    {
        if ((netif->ip6_addr[idx].u_addr.ip6.addr[0]) &&
            (idx >= 1) &&
            (!ip6_addr_isinvalid(netif_ip6_addr_state(netif, idx))))
        {
            return pdTRUE;
        }
    }
 #endif                                // __SUPPORT_IPV6__

    return pdFALSE;
}

 #if defined(__SUPPORT_IPV6__)
unsigned int ra6w1_network_main_check_global_unicast_ip6_addr (unsigned char iface)
{
    struct netif * netif = netif_get_by_index(RM_WIFI_NETIF_INDEX(iface));

    for (int idx = 1; idx < LWIP_IPV6_NUM_ADDRESSES; idx++)
    {
        if (ip6_addr_isglobal((ip6_addr_t *) &netif->ip6_addr[idx]))
        {
            return pdTRUE;
        }
    }

    return pdFALSE;
}

 #endif                                // __SUPPORT_IPV6__

unsigned char ra6w1_network_main_check_dhcp_state (unsigned char iface)
{
 #if LWIP_DHCP
    struct netif * netif = netif_get_by_index(RM_WIFI_NETIF_INDEX(iface));
  #ifdef __SUPPORT_IPV4__
    unsigned char dhcp_state;

    dhcp_state = dhcp_get_state(netif);

    return dhcp_state;
  #else

    return 0;
  #endif                               // __SUPPORT_IPV4__
 #else /* LWIP_DHCP */
    return 0;
 #endif                                /* LWIP_DHCP */
}

unsigned int ra6w1_network_main_check_network_ready (unsigned char iface)
{
    if (iface == NONE_IFACE)
    {
        iface = WLAN0_IFACE;
    }

    if (ra6w1_network_main_check_net_init(iface) == pdPASS)
    {
 #if CFG_PMGR
        if (RM_PMGR_W_dpm_is_wakeup() == pdTRUE)
        {
            if (is_supplicant_done() == 1)
            {
                if (RM_WIFI_dpm_supp_is_connected() == pdPASS)
                {
                    return pdTRUE;
                }
            }
        }
        else
 #endif                                /* CFG_PMGR */
        {
 #if CFG_WIFI
  #if CFG_PMGR
            if ((RM_WIFI_dpm_supp_is_connected() == pdPASS) || (get_run_mode() != WIFI_DEVICE_MODE_EXT_STATION))
  #else
            if ((rm_wifi_is_wpa_state(WPA_COMPLETED,
                                      0) == FSP_SUCCESS) || (get_run_mode() != WIFI_DEVICE_MODE_EXT_STATION))
  #endif                               /* CFG_PMGR */
 #endif                                /* CFG_WIFI */
            {
                if (((iface == WLAN0_IFACE) && ra6w1_network_main_check_net_link_status(iface)) ||
                    ((iface == WLAN1_IFACE) && ra6w1_network_main_check_net_link_status(iface)))
                {
                    return chk_ipaddress(iface);
                }
            }
        }
    }

    return pdFALSE;
}

int ra6w1_network_main_check_net_ip_status (int iface)
{
    return !ra6w1_network_main_check_network_ready(iface);
}

 #if defined(__SUPPORT_IPV6__)         // UDP_DPM_TEST
int ra6w1_network_main_check_net_ipv6_status (int iface)
{
    return !ra6w1_network_main_check_global_unicast_ip6_addr(iface);
}

 #endif                                // __SUPPORT_IPV6__

int ra6w1_network_main_check_net_link_status (int iface)
{
    struct netif * wlan0_netif = netif_get_by_index(RM_WIFI_NETIF_INDEX(WLAN0_IFACE));
    struct netif * wlan1_netif = netif_get_by_index(RM_WIFI_NETIF_INDEX(WLAN1_IFACE));

    if (((iface == WLAN0_IFACE) && (netif_is_up(wlan0_netif))) ||
        ((iface == WLAN1_IFACE) && (netif_is_up(wlan1_netif))))
    {
        return pdTRUE;
    }

    return pdFALSE;
}

// pdFALSE: WLAN init X, pdTRUE WLAN init O
static int   ra6w1_network_initwlan_mode        = -1;
const char * ra6w1_network_initwlan_mode_nvname = "INITWLAN";

void ra6w1_network_main_print_wlaninit_mode (void)
{
    // Update status
    ra6w1_network_main_get_wlaninit_mode();

    // Print status
    if (ra6w1_network_initwlan_mode == pdTRUE)
    {
        printf("\nEnabled WLAN init.\n");
    }
    else
    {
        printf("\nDisabled WLAN init.\n");
    }
}

int ra6w1_network_main_set_wlaninit_mode (int flag)
{
    int ret = 0;

    switch (flag)
    {
        case pdFALSE:
        {
            /* Don't run WLAN init when boot */
 #ifdef RM_MAP_PERSISTANT_W
            ret = RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                ENV_GROUP_SYSCFG,
                                                ra6w1_network_initwlan_mode_nvname,
                                                pdFALSE);
 #endif
            if (ret)
            {
                printf("[%s] Failed to set %s(%d)\n", __func__, ra6w1_network_initwlan_mode_nvname, ret);
                break;
            }

            ra6w1_network_initwlan_mode = pdFALSE;
            break;
        }

        case pdTRUE:
        {
            /* Run WLAN init when boot */
 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(),
                                      ENV_GROUP_SYSCFG,
                                      ra6w1_network_initwlan_mode_nvname);
 #endif
            ra6w1_network_initwlan_mode = pdTRUE;
            break;
        }

        default:
        {
            ret = -1;
            break;
        }
    }

    return ret;
}

int ra6w1_network_main_get_wlaninit_mode (void)
{
    if (ra6w1_network_initwlan_mode == -1)
    {
 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                     ENV_GROUP_SYSCFG,
                                     ra6w1_network_initwlan_mode_nvname,
                                     &ra6w1_network_initwlan_mode);
 #endif

        if ((ra6w1_network_initwlan_mode == pdTRUE) || (ra6w1_network_initwlan_mode == -1))
        {
            ra6w1_network_initwlan_mode = pdTRUE;

            return pdTRUE;
        }
        else
        {
            ra6w1_network_initwlan_mode = pdFALSE;

            return pdFALSE;
        }
    }
    else if (ra6w1_network_initwlan_mode == pdFALSE)
    {
        return pdFALSE;
    }
    else if (ra6w1_network_initwlan_mode == pdTRUE)
    {
        return pdTRUE;
    }

    return pdFALSE;
}

static int ra6w1_network_main_wlaninit = pdFALSE;

int ra6w1_network_main_is_wlaninit ()
{
    return ra6w1_network_main_wlaninit;
}

static void ra6w1_network_main_enable_wps_btn (void)
{
 #if defined(__SUPPORT_WPS_BTN__)
    unsigned int ret = 0;

    if (!ra6w1_network_main_get_wlaninit_mode())
    {
        return;
    }

#if defined(__SUPPORT_WIFI_USER_GPIO__) && defined(BTN_WPS)
    /* Setup WPS button */
    ret = rm_wifi_app_gpio_check_wps_button(BTN_WPS_PORT, BTN_WPS_PIN,

 #if defined(__SUPPORT_EVK_LED__)
                                             FR_WPS_LED_PORT, FR_WPS_LED_PIN,
 #endif // __SUPPORT_EVK_LED__
                                             BTN_WPS_CHK_TIME);
#endif
    if (ret == pdTRUE) {
        char reply[10];
        memset(reply, 0, 10);
        ra6w1_cli_reply("wps_pbc any", NULL, reply);
    }
#endif // __SUPPORT_WIFI_USER_GPIO__ && BTN_WPS

    return ;
}

void ra6w1_network_main_init_system_apps (int net_chk_flag)
{
 #if (dg_configCODE_LOCATION == NON_VOLATILE_IS_FLASH)

    /* Start RRQ61000 system tasks */
    start_system(pdTRUE, net_chk_flag);
 #endif                                /* (dg_configCODE_LOCATION == NON_VOLATILE_IS_FLASH) */
}

static void ra6w1_network_main_init_wlan_task (void * params)
{
    int ret = 0;

    RA6W1_UNUSED_ARG(params);

    if (ra6w1_network_main_is_wlaninit())
    {
        goto end;
    }

    if (!ra6w1_network_main_get_wlaninit_mode())
    {
        ra6w1_network_main_set_wlaninit_mode(pdTRUE);
    }

    ret = ra6w1_network_main_init_wlan();
    if (ret)
    {
        goto end;
    }

    ra6w1_network_main_init_system_apps(pdTRUE);

end:

    vTaskDelete(NULL);
}

int ra6w1_network_main_init_wlan_with_task (void)
{
    OS_BASE_TYPE ret     = 0;
    OS_TASK      xHandle = NULL;

    size_t stack_size = (1024);

    if (ra6w1_network_main_is_wlaninit())
    {
        printf("Wi-Fi initialization already has been done.\n");
    }
    else
    {
        ret =
            OS_TASK_CREATE("Run_WlanInit", ra6w1_network_main_init_wlan_task, (void *) 0,
                           (stack_size * OS_STACK_WORD_SIZE), OS_TASK_PRIORITY_USER_MAX, xHandle);

        OS_ASSERT(ret == OS_TASK_CREATE_SUCCESS);
    }

    return ret;
}

int ra6w1_network_main_init_wlan (void)
{
    int ret = 0;
 #if CFG_PMGR
    int wakeup_type;
 #endif                                /* CFG_PMGR */

    if (!ra6w1_network_main_get_wlaninit_mode() || ra6w1_network_main_is_wlaninit())
    {
        return -1;
    }

 #if CFG_PMGR

    /* start the dpm sleep daemon , in case of STA Mode */
    if (get_run_mode() == WIFI_DEVICE_MODE_EXT_STATION)
    {
        if (RM_PMGR_W_dpm_is_enabled() == pdTRUE)
        {
            // Create mendatory resources
            RM_PMGR_W_dpm_lld_task_init();
        }
    }
 #endif                                /* CFG_PMGR */

 #if (dg_configCODE_LOCATION == NON_VOLATILE_IS_FLASH)

    // Initialize Wi-Fi
    ret = net_stack_init();
    if (ret == pdFALSE)
    {
        return -1;
    }
 #endif                                /* (dg_configCODE_LOCATION == NON_VOLATILE_IS_FLASH) */

    ra6w1_network_main_enable_wps_btn();

    ra6w1_network_main_wlaninit = pdTRUE;

 #if CFG_PMGR
    wakeup_type = RM_WIFI_dpm_ptim_event_get();

    if (get_run_mode() == WIFI_DEVICE_MODE_EXT_STATION)
    {
        RM_PMGR_W_dpm_lld_task_start(wakeup_type);
    }
 #endif                                /* CFG_PMGR */

    return 0;
}

 #if CFG_PMGR
extern unsigned char dpm_slp_time_reduce_flag;
 #endif                                    /* CFG_PMGR */

static unsigned int arp_req_send_flag = 1; // Need Initialize value
 #define APP_POLL_STATE    "poll_state"
 #define DPM_REG_ERR       -1
 #define WAIT_DPM_SLEEP    0
unsigned int get_current_arp_req_status (void)
{
    return arp_req_send_flag;
}

void polling_state_check (void * pvParameters)
{
 #undef FOR_DEBUG
 #ifdef __SUPPORT_IPV4__
  #ifdef __ENABLE_UNUSED__
    unsigned int status;
  #endif                               // __ENABLE_UNUSED__
    int                exist_gw;
    unsigned int       arp_req_wait_cnt = 0;
    const ip4_addr_t * gw_addr;
    unsigned long      gw_ulongaddr;
  #ifdef __ENABLE_UNUSED__
    const ip4_addr_t * net_mask;
    unsigned long      netmask_ulong;
    unsigned long      ip_ulongaddr = 0;
  #endif                               // __ENABLE_UNUSED__

  #if CFG_PMGR
    int ip_condition = 0;
    pmgr_instance_ctrl_t * p_instance_ctrl = RM_PMGR_W_get_ctrl();
    int job_done = pdTRUE;
  #endif                               /* CFG_PMGR */
    struct netif * netif;

    RA6W1_UNUSED_ARG(pvParameters);

    netif = netif_get_by_index(RM_WIFI_NETIF_INDEX(WLAN0_IFACE));

    if (netif == NULL)
    {
        while (1)
        {
            netif = netif_get_by_index(RM_WIFI_NETIF_INDEX(WLAN0_IFACE));
            if (netif)
            {
                break;
            }

            vTaskDelay(portCONVERT_MS_2_TICKS(20));
        }
    }

  #if CFG_PMGR
   #ifdef RM_MAP_PERSISTANT_W
    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_DPM_IP_CONDITION,
                                     &ip_condition))
    {
        ip_condition = PMGR_CONDITION_IPV4_MANDATORY;
    }
   #endif

    // printf("[%s] Starting ...\n", __func__);
  #endif                               /* CFG_PMGR */

    /* Initialize the flags */
    arp_req_send_flag = 1;

    while (1)
    {
        /* Do nothing anymore duing DPM sleep operation ... */
  #if CFG_PMGR
        if (RM_PMGR_W_dpm_sleep_is_started() != WAIT_DPM_SLEEP)
        {
            goto next_turn;
        }
  #endif                               /* CFG_PMGR */

  #ifdef FOR_DEBUG
        printf("[%s] arp_req_send_flag is %d \n", __func__, arp_req_send_flag);
  #endif
        if (arp_req_send_flag == 1)
        {
  #if CFG_PMGR
            if (ip_condition & PMGR_CONDITION_IPV4_MANDATORY)
            {
                /* Prohibit to enter DPM sleep mode */
                if (RM_PMGR_W_dpm_is_enabled())
                {
                    if (job_done == pdTRUE)
                    {
                        RM_PMGR_W_add_sleep_constraint(p_instance_ctrl, PMGR_CONSTRAINT_POWER_RAM);
                        if (p_instance_ctrl->debug_runtime_flag == PMGR_DEBUG_RAM_ENABLED)
                        {
                            p_instance_ctrl->ram_counter_cause[PMGR_DBG_RAM_CONSTRAINT_APP_POLL_STATE]++;
                        }

                        job_done = pdFALSE;
                    }
                }
            }
  #endif                               /* CFG_PMGR */
        }
        else
        {
  #if CFG_PMGR
            if (ip_condition & PMGR_CONDITION_IPV4_MANDATORY)
            {
                if (RM_PMGR_W_dpm_is_enabled())
                {
                    /* Permit to enter DPM sleep mode */
                    if (job_done == pdFALSE)
                    {
                        RM_PMGR_W_remove_sleep_constraint(p_instance_ctrl, PMGR_CONSTRAINT_POWER_RAM);
                        if (p_instance_ctrl->debug_runtime_flag == PMGR_DEBUG_RAM_ENABLED)
                        {
                            p_instance_ctrl->ram_counter_cause[PMGR_DBG_RAM_CONSTRAINT_APP_POLL_STATE]--;
                        }

                        job_done = pdTRUE;
                    }
                }
            }
  #endif                               /* CFG_PMGR */
            goto next_turn;
        }

        // stop_sys_apps_completed();
        // stop_user_apps_completed();

        /* Check if Connection & ip address assignment */
  #if CFG_WIFI
   #if CFG_PMGR
        if ((RM_WIFI_dpm_supp_is_connected() == 1) && (ra6w1_network_main_check_ip_addr(0) == pdTRUE))
        {
   #else
        if ((rm_wifi_is_wpa_state(WPA_COMPLETED, 0) == FSP_SUCCESS) && (ra6w1_network_main_check_ip_addr(0) == pdTRUE))
        {
   #endif                              /* CFG_PMGR */
  #else /* CFG_WIFI */
        if (ra6w1_network_main_check_ip_addr(0) == pdTRUE)
        {
  #endif                               /* CFG_WIFI */
            /* checking the default gw address */
            exist_gw = etharp_exist_defgw_addr();
  #ifdef FOR_DEBUG
            printf("[%s] DPM Default gw is %d \n", __func__, exist_gw);
  #endif

            if (exist_gw == 0)
            {
                /* Send ARP reqeuset if arp table is empty. */
                if (arp_req_wait_cnt == 0)
                {
  #if CFG_PMGR

                    /* In case of DPM Wakeup, After supplicant is done(End of key seting), we have to send packet */
                    if (RM_PMGR_W_dpm_is_wakeup() == pdTRUE)
                    {
                        wait_supplicant_done(UINT_MAX);
                    }
  #endif                               /* CFG_PMGR */

                    /* check the default gw address */
                    gw_addr      = netif_ip4_gw(netif);
                    gw_ulongaddr = ip4_addr_get_u32(gw_addr);
                    if ((gw_ulongaddr == IPADDR_ANY) || (gw_ulongaddr == IPADDR_NONE))
                    {
  #ifdef FOR_DEBUG
                        printf("[%s] gw ipaddr(%x) is wrong\n", __func__, ip_ulongaddr);
  #endif
                        arp_req_wait_cnt  = 0;
                        arp_req_send_flag = 1;
                        goto next_turn;
                    }

/* Fix me lator */
  #ifdef __ENABLE_UNUSED__             /* Temporary Code for DPM PTIM IP and subnet */
                    /* check the default gw address */
                    ip_addr      = netif_ip4_addr(netif);
                    ip_ulongaddr = ip4_addr_get_u32(ip_addr);

                    net_mask      = netif_ip4_netmask(netif);
                    netmask_ulong = ip4_addr_get_u32(net_mask);
                    if ((ip_ulongaddr == IPADDR_ANY) || (ip_ulongaddr == IPADDR_NONE))
                    {
                        printf("[%s] IP addr(%x) is wrong\n", __func__, ip_ulongaddr);
                        goto next_turn;
                    }

   #if CFG_PMGR
                    RM_WIFI_dpm_arp_filter_set(ip_ulongaddr, netmask_ulong);
   #endif                              /* CFG_PMGR */
  #endif                               // __ENABLE_UNUSED__

                    arp_req_send_flag = 1;

                    // arp_request(gw_ulongaddr, 0);
  #ifdef __ENABLE_UNUSED__             /* ip config is ok, so just do ARP Req sending 1 times per 10, don't need it */
                    status = etharp_request(netif, gw_addr);
                    if (status != ERR_OK)
                    {
                        arp_req_wait_cnt  = 0;
                        arp_req_send_flag = 1;
                        goto next_turn;
                    }

  #else
                    etharp_request(netif, gw_addr);
  #endif                               // __ENABLE_UNUSED__
                    // printf("[%s] Send ARP IP addr(%x) , GW(%x)\n", __func__ , ip_ulongaddr, gw_ulongaddr);
                }

                /* Wait until to receive response during 100 msec */
                if (arp_req_wait_cnt++ >= 10)
                {
                    arp_req_wait_cnt = 0;
                }

  #if CFG_PMGR
                if (dpm_slp_time_reduce_flag == pdTRUE)
                {
                    if ((arp_req_wait_cnt > 1) && RM_PMGR_W_dpm_is_wakeup())
                    {
                        arp_req_wait_cnt  = 0;
                        arp_req_send_flag = 0;
                        if (ip_condition & PMGR_CONDITION_IPV4_MANDATORY)
                        {
                            if (job_done == pdFALSE)
                            {
                                RM_PMGR_W_remove_sleep_constraint(p_instance_ctrl, PMGR_CONSTRAINT_POWER_RAM);
                                if (p_instance_ctrl->debug_runtime_flag == PMGR_DEBUG_RAM_ENABLED)
                                {
                                    p_instance_ctrl->ram_counter_cause[PMGR_DBG_RAM_CONSTRAINT_APP_POLL_STATE]--;
                                }

                                job_done = pdTRUE;
                            }
                        }

                        goto next_turn;
                    }
                }
  #endif                               /* CFG_PMGR */

                if (fast_connection_sleep_flag == pdTRUE)
                {
                    /* In Sleep mode 1,2 Fast connection, we don't need to make default ARP Table */
                    if (arp_req_wait_cnt > 1)
                    {
                        arp_req_wait_cnt  = 0;
                        arp_req_send_flag = pdTRUE;
                    }
                }
            }
            else if (exist_gw == 1)    // Have ARP table for default_gw
            {
  #ifdef FOR_DEBUG
                printf("[%s] DPM Default gw is exist\n", __func__);
  #endif
                arp_req_wait_cnt  = 0;
                arp_req_send_flag = 0;
            }

  #ifdef __ENABLE_UNUSED__             /* Not used */
            else if (exist_gw == -1)   // Error case
            {
                arp_req_wait_cnt  = 0;
                arp_req_send_flag = 1;
            }
  #endif                               // __ENABLE_UNUSED__
        }
        else                           // In case of no connection ...
        {
            arp_req_wait_cnt  = 0;
            arp_req_send_flag = 1;
        }

next_turn:
        vTaskDelay(portCONVERT_MS_2_TICKS(10));
    }
 #endif                                // __SUPPORT_IPV4__
}

#endif                                 /* CFG_WIFI */

/* EOF */
