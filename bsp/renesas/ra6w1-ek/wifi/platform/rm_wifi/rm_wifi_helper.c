/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

#include <string.h>
#include <stdio.h>
#include <strings.h>
#include "lwip/priv/tcp_priv.h"
#include "net_dhcp_server.h"
#include "dhcpserver.h"
#include "common_def.h"
#include "net_common.h"
#include "common_data.h"
#include "iface_defs.h"
#include "rm_wifi_helper.h"
#include "rm_wifi.h"
#include "rm_vee_flash_w_rrq_nvram.h"
#include "supp_config.h"
#ifdef RM_MAP_PERSISTANT_W
#include "rm_map_persistant_w.h"
#endif
#if defined (dg_configADNVPARAM_PROJ_FILE)
#include dg_configADNVPARAM_PROJ_FILE
#endif

#include "rm_cert.h"

#if defined (__SUPPORT_MQTT__)
#include "mqtt_client.h"
#endif

#if CFG_CLI
#include "rm_cli_w_usr_nvram.h"
#endif // CFG_CLI

#if defined ( __SUPPORT_WIFI_CONN_CB__ )
#include "event_groups.h"
#endif // __SUPPORT_WIFI_CONN_CB__

#if defined (__SUPPORT_MQTT__)
#include "mqtt_client.h"
#endif


/***********************************************************************************************************************
 * Externs
 **********************************************************************************************************************/

#if defined ( __SUPPORT_WIFI_CONN_CB__ )
extern void wifi_conn_notify_cb_register(void (*user_cb)(void));
extern void wifi_conn_fail_notify_cb_register(void (*user_cb)(short reason_code));
extern void wifi_disconn_notify_cb_register(void (*user_cb)(short reason_code));
extern void ap_sta_disconnected_notify_cb_register(void (*user_cb)(const unsigned char mac[6]));
#endif // __SUPPORT_WIFI_CONN_CB__

extern int get_sta_signal_poll(void);

extern char * strtok_r(char * str, const char * delim, char ** saveptr);
extern char * strcasestr(const char * haystack, const char * needle);

extern int  ra6w1_regdb_get_cty_idx_from_cc(char * cc);
extern int  ra6w1_regdb_get_ch_range_by_country_n_band(char * country, int band, int * min_ch, int * max_ch, 
                                                       unsigned int* ch_bitmap_5g, unsigned int exclude_flags);
extern void ra6w1_regdb_gen_5g_ch_range_string(char* str_buf, unsigned int ch_bitmap, char delimiter);

/***********************************************************************************************************************
 * Defines
 **********************************************************************************************************************/

#define MAX_SSID_LEN        32
#define DEFULT_MAC_MSW      0xEC9F
#define DEFULT_MAC_LSW      0x0D9FFFFE
#define NET_INFO_STR_LEN    18

/***********************************************************************************************************************
 * Global variables
 **********************************************************************************************************************/
extern const wifi_cfg_t g_wifi_cfg;
#if defined ( __SUPPORT_WIFI_CONN_CB__ )
EventGroupHandle_t   evt_grp_wifi_conn_notify = NULL;
short wifi_conn_fail_reason    = 0;
short wifi_disconn_reason      = 0;
short ap_wifi_conn_fail_reason = 0;
short ap_wifi_disconn_reason   = 0;

#if defined(__SUPPORT_MATTER_IOT__)
unsigned char matter_wifi_conn_status = FALSE;
#endif // (__SUPPORT_MATTER_IOT__)

#endif // __SUPPORT_WIFI_CONN_CB__

/// Global Define : Chipset Model
#undef CHIPSET_NAME
#if (TIN_SKU_BUILD_ID == TIN_SKU_WIFI6_B24_5_BLE || (defined (BSP_MCU_RRQ61051_208) || defined (BSP_MCU_RRQ61051_408)))
#define CHIPSET_NAME	"RA6W2"
#else
#define CHIPSET_NAME	"RA6W1"
#endif /* TIN_SKU_BUILD_ID == TIN_SKU_WIFI6_B24_5_BLE || (BSP_MCU_RRQ61051_208 || BSP_MCU_RRQ61051_408) */

/***********************************************************************************************************************
 * Private Function Definitions
 **********************************************************************************************************************/
#if CFG_WIFI
/**
 * Get SSID name.
 * 
 * @param[in]  prefix:      ssid name prefix.
 * @param[in]  iface:       interface index.
 * @param[in]  quotation:   output double quotes.
 * @param[in]  size:        maximal ssid string length to write to output ssid buffer 
 *                          (including terminating \0 character and quotes).
 * @param[out] ssid:        output buffer for writing ssid string to.
 * 
 * @return     zero if successful.
 */
int gen_ssid(char* prefix, int iface, int quotation, char* ssid, int size)
{
    unsigned long macmsw, maclsw;
    const char * qt = quotation ? "\"" : "";
    int prefix_len = strlen(prefix);
    int ssid_len = quotation ? size - 3 : size - 1; /* max ssid_len leaving space for quotes and terminating \0 character */

    ssid_len = ssid_len > MAX_SSID_LEN ? MAX_SSID_LEN : ssid_len;

    if (ssid_len < prefix_len)
    {
        return -1;
    }
    else if (ssid_len < prefix_len + 7)
    {
        sprintf(ssid, "%s%s%s", qt, prefix, qt);
    }
    else
    {
        getMacAddrMswLsw(iface, &macmsw, &maclsw);

        sprintf(ssid, "%s%s_%02lX%02lX%02lX%s",
            qt,
            prefix,
            (maclsw >> 16) & 0x0ff,
            (maclsw >> 8) & 0x0ff,
            (maclsw >> 0) & 0x0ff,
            qt);
    }
    return 0;
}

#if defined ( __SUPPORT_WIFI_CONN_CB__ )
static void wifi_conn_cb(void)
{
    if (evt_grp_wifi_conn_notify != NULL) {
        if (get_run_mode() == WIFI_DEVICE_MODE_EXT_AP) {
            xEventGroupSetBits(evt_grp_wifi_conn_notify, WIFI_CONN_SUCC_SOFTAP);
        } else {
            xEventGroupSetBits(evt_grp_wifi_conn_notify, WIFI_CONN_SUCC_STA);
#if defined(__SUPPORT_MATTER_IOT__)
        matter_wifi_conn_status = TRUE;
        wifi_noti_connected();
#endif // (__SUPPORT_MATTER_IOT__)
        }
    }
}

//
// void (*wifi_conn_fail_notify_cb)(int reason_code)
//
// reason_code :
//     WLAN_REASON_PEERKEY_MISMATCH    : Wrong password
//
static void wifi_conn_fail_cb(short reason_code)
{
    if (evt_grp_wifi_conn_notify != NULL) {
        if (get_run_mode() == WIFI_DEVICE_MODE_EXT_AP) {
            ap_wifi_conn_fail_reason = reason_code;
            xEventGroupSetBits(evt_grp_wifi_conn_notify, WIFI_CONN_FAIL_SOFTAP);
        } else {
            wifi_conn_fail_reason = reason_code;
            xEventGroupSetBits(evt_grp_wifi_conn_notify, WIFI_CONN_FAIL_STA);
#if defined(__SUPPORT_MATTER_IOT__)
            matter_wifi_conn_status = FALSE;
#endif // (__SUPPORT_MATTER_IOT__)
        }
    }
}

//
// void (*wifi_disconn_notify_cb)(int reason_code)
//
static void wifi_disconn_cb(short reason_code)
{
    if (evt_grp_wifi_conn_notify != NULL) {
        if (get_run_mode() == WIFI_DEVICE_MODE_EXT_AP) {
            ap_wifi_disconn_reason = reason_code;
            xEventGroupSetBits(evt_grp_wifi_conn_notify, WIFI_DISCONN_SOFTAP);
        } else {
#if defined(__SUPPORT_MATTER_IOT__)
        matter_wifi_conn_status = FALSE;
        wifi_noti_disconnected();
#endif // (__SUPPORT_MATTER_IOT__)
            wifi_disconn_reason = reason_code;
            xEventGroupSetBits(evt_grp_wifi_conn_notify, WIFI_DISCONN_STA);
        }
    }
}

#if defined (__SUPPORT_IPV4__)
extern void tcp_abandon_remote_ip(const ip_addr_t *addr);
static void ap_sta_disconnected_cb(const unsigned char mac[6])
{
    unsigned char mac_addr[6] = {0x00,};
    ip4_addr_t ip_addr = {0x00,};

    memcpy(mac_addr, mac, sizeof(mac_addr));

    if (is_dhcp_server_running()) {
        // Search IP address on MAC
        if (dhcps_search_ip_on_mac(mac_addr, &ip_addr)) {
            // Abandon tcp connection
            tcp_abandon_remote_ip((ip_addr_t*)&ip_addr);
        }
    }

    return ;
}
#endif // __SUPPORT_IPV4__
#endif // __SUPPORT_WIFI_CONN_CB__

void rm_wifi_register_wifi_notify_cb(void)
{
#if defined ( __SUPPORT_WIFI_CONN_CB__ )
    // Create sync-up event
    evt_grp_wifi_conn_notify = xEventGroupCreate();
    if (evt_grp_wifi_conn_notify == NULL) {
        printf("\n\n>>> Failed to create Wi-Fi connection notify-cb event !!!\n\n");
        return;
    }

    /* Wi-Fi connection call-back */
    wifi_conn_notify_cb_register(wifi_conn_cb);

    /* Wi-Fi connection call-back */
    wifi_conn_fail_notify_cb_register(wifi_conn_fail_cb);

    /* Wi-Fi disconnection call-back */
    wifi_disconn_notify_cb_register(wifi_disconn_cb);
#if defined (__SUPPORT_IPV4__)
    /* AP-STA-DISCONNECTED call-back */
    ap_sta_disconnected_notify_cb_register(ap_sta_disconnected_cb);
#endif // __SUPPORT_IPV4__
#endif // __SUPPORT_WIFI_CONN_CB__
}

int rm_wifi_atoi_custom (char* str)
{
    int res = 0, minus_sign = 0;

    if (str == NULL) {
        return 0;
    }

    for (int i = 0; str[i] != '\0'; ++i) {
        if (i == 0) {
             if (str[i] >= '0' && str[i] <= '9') {
                 res = res * 10 + str[i] - '0';
             } else if (str[i] == '-') {
                 minus_sign = 1;
             } else if (str[i] == '+') {
                 minus_sign = 0;
             } else {
                return 0;
             }
        } else {
            if (str[i] >= '0' && str[i] <= '9') {
                res = res * 10 + str[i] - '0';
            } else {
                return 0;
            }
        }
    }

    return (minus_sign?(res*(-1)):(res));
}

int rm_wifi_get_int_val_from_str(char* param, int* int_val, int policy)
{
    int result = -1, param_len, int_val_old;

    if (param == NULL || int_val == NULL) {
        return -1;
    }

    param_len = strlen(param);
    int_val_old = *int_val;

    if (param_len == 1) {
        if (param[0] == '0') {
            // "0" <- non error 0 return
            *int_val = 0;
            result = 0; /* SUCCESS */
        } else {
            // check if valid single digit 1 ~ 9
            *int_val = rm_wifi_atoi_custom(param);

            if (*int_val > 0 && *int_val < 10) {
                // valid value: 1~9
                result = 0; /* SUCCESS */
            } else {
                // error: e.g. == 0
                *int_val = int_val_old;
                result = -1;
            }
        }
    } else if (param_len == 0) {
        *int_val = int_val_old;
        result = -1;
    } else {
        //    param_len > 1

        if (policy == POL_1) {
            // leading "0" / "+" / "-0" are not allowed
            if (param[0] == '0' || param[0] == '+')
                return -1;

            if (param[0] == '-' && param[1] == '0')
                return -1;
        } else if (policy == POL_2) {
            // leading "+" / "-0" are not allowed
            if (param[0] == '+')
                return -1;

            if (param[0] == '-' && param[1] == '0')
                return -1;
        }

        *int_val = rm_wifi_atoi_custom(param);

        if (*int_val > -1 && *int_val < 10) {
            // considered error
            *int_val = int_val_old;
            result = -1;
        } else {
            result = 0; /* SUCCESS */
        }
    }

    return result;
}

#if defined ( __BOOT_CONN_TIME_PRINT__ )
void log_boot_conn_with_run_time(char * string)
{
    unsigned long cur_tick, diff_tick;
    unsigned long diff_tick_MS;

    cur_tick = xTaskGetTickCount();

    diff_tick = cur_tick - _boot_start_tick;
    diff_tick_MS = diff_tick * 10;

    printf(CYAN_COLOR "[ ~ %d.%03d sec ] %s \n" CLEAR_COLOR,
                ((int)diff_tick_MS / 1000),
                (diff_tick_MS % 1000),
                string);
}
#endif // __BOOT_CONN_TIME_PRINT__


//
//// For Factory-Reset APIs ////

/* For Customer's configuration */
softap_config_t _ap_config_param = { 0, };
softap_config_t *ap_config_param = (softap_config_t *) &_ap_config_param;
extern int gen_ssid(char* prefix, int iface, int quotation, char* ssid, int size);

void factory_reset_sta_mode(void)
{
    printf("Set STA Mode ...\n");

#ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Erase_GROUP(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG);
#endif
#ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Erase_GROUP(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG);
#endif
#ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Erase_GROUP(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG);
#endif
#ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Erase_GROUP(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_TESTCFG);
#endif
#ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Erase_GROUP(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE);
#endif
    vTaskDelay(portCONVERT_MS_2_TICKS(100));
}

void factory_reset_ap_mode(void)
{
    char default_ssid[MAX_SSID_LEN + 3];

    printf(ANSI_COLOR_DEFULT "Set Soft-AP Mode ...\n");

    if (factory_reset(0) == pdFAIL) /* Factory reset */
    {
        return;
    }

    /* Default value set-up */
#ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                WIFI_PROFILE_SYS_MODE, (int) DFLT_SYSMODE);

    /* SSID+MACADDRESS */
    memset(default_ssid, 0, MAX_SSID_LEN + 3);

    if (gen_ssid(CHIPSET_NAME, WLAN1_IFACE, 1, default_ssid, sizeof(default_ssid)) == -1) {
        printf("SSID Error\n");
    }
    // wifi cfg
    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, (const char *) WIFI_PROFILE_SSID_1, default_ssid);
    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                WIFI_PROFILE_CHANNEL, (int) DFLT_AP_CHANNEL);
    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_COUNTRY_CODE, (const char *) DFLT_AP_COUNTRY_CODE);

    // ip network 
    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                WIFI_PROFILE_NETMODE_1, (int) DFLT_NETMODE_1);
    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_IPADDR_1, DFLT_AP_IP);
    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_NETMASK_1, DFLT_AP_SUBNET);
    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_GATEWAY_1, DFLT_AP_GW);
    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG,
                                NVR_KEY_DHCPD, (int) DFLT_AP_DHCP_S);
    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG,
                                NVR_KEY_DHCP_TIME, (int) DFLT_DHCP_S_LEASE_TIME);
    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, NVR_KEY_DHCP_S_IP, DFLT_DHCP_S_S_IP);
    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, NVR_KEY_DHCP_E_IP, DFLT_DHCP_S_E_IP);
    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_COMPLETE, 1);
    printf("\nOK\n");
#endif
}

#if defined ( __SUPPORT_WIFI_CONCURRENT__ )
#if defined ( __SUPPORT_FACTORY_RST_CONCURR_MODE__ )
void factory_reset_concurrent_mode(void)
{
    char default_ssid[MAX_SSID_LEN + 3];

    printf("Set Concurrentr-Mode (STA + Soft-AP) ...\n");

    if (factory_reset(0) == pdFAIL) /* Factory reset */
    {
        return;
    }
    vTaskDelay(portCONVERT_MS_2_TICKS(10));

#ifdef RM_MAP_PERSISTANT_W

    /* Default value set-up */
    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                (const char *) WIFI_PROFILE_SYS_MODE, (int) WIFI_DEVICE_MODE_EXT_AP_STATION);

    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, (const char *) WIFI_PROFILE_COUNTRY_CODE, (const char *) DFLT_AP_COUNTRY_CODE);
    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                (const char *) WIFI_PROFILE_CHANNEL, (int) DFLT_AP_CHANNEL);
    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                               WIFI_PROFILE_WIFI_MODE, WIFI_MODE_BGN);

    /* SSID+MACADDRESS */
    memset(default_ssid, 0, MAX_SSID_LEN + 3);
    if (gen_ssid(CHIPSET_NAME, WLAN1_IFACE, 1, default_ssid, sizeof(default_ssid)) == -1) {
        printf("SSID Error\n");
    }

    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, (const char *) WIFI_PROFILE_SSID_1, default_ssid);

    /* IP Address */
    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                (const char *) WIFI_PROFILE_NETMODE_1, (int) DFLT_NETMODE_1);
    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, (const char *) WIFI_PROFILE_IPADDR_1, DFLT_AP_IP);
    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, (const char *) WIFI_PROFILE_NETMASK_1, DFLT_AP_SUBNET);
    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, (const char *) WIFI_PROFILE_GATEWAY_1, DFLT_AP_GW);
    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, (const char *) WIFI_PROFILE_DNSSVR_1, DFLT_AP_DNS);
    
    /* DHCP Server */
    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG,
                                (const char *) NVR_KEY_DHCPD, (int) DFLT_AP_DHCP_S);
    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG,
                                (const char *) NVR_KEY_DHCP_TIME, (int) DFLT_DHCP_S_LEASE_TIME);
    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, (const char *) NVR_KEY_DHCP_S_IP, DFLT_DHCP_S_S_IP);
    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, (const char *) NVR_KEY_DHCP_E_IP, DFLT_DHCP_S_E_IP);
    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, (const char *) NVR_KEY_DHCP_DNS_IP, DFLT_DHCP_S_DNS_IP);
    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_COMPLETE, 1);
#endif
    printf("\nOK\n");
}
#endif // __SUPPORT_FACTORY_RST_CONCURR_MODE__

int factory_reset_btn_onetouch(void)
{
#ifdef __SUPPORT_FACTORY_RST_CONCURR_MODE__
    int cur_run_mode;
    int prev_run_mode;
    
    cur_run_mode = get_run_mode();

    // Change system running mode between current mode <-> concurrent-mode
    switch (cur_run_mode) {
        case WIFI_DEVICE_MODE_EXT_STATION :
            // Check if Soft-AP profile exists or not.
#ifdef RM_MAP_PERSISTANT_W
            if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                           WIFI_PROFILE_SSID_1, (int *)&prev_run_mode) != FSP_SUCCESS) {
#endif
                printf("\n!!! STA_ONLY mode : Does not change to concurrent-mode (STA+Soft-AP) !!!\n\n");
                return pdFALSE;
            }

            // Change to concurrent-mode : STA + Soft-AP
#ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG,
                                        (const char *) ENV_SWITCH_SYSMODE, (int) WIFI_DEVICE_MODE_EXT_STATION);
#endif
#ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                        (const char *) WIFI_PROFILE_SYS_MODE, (int) WIFI_DEVICE_MODE_EXT_AP_STATION);
#endif
            break;

        case WIFI_DEVICE_MODE_EXT_AP  :
            // Check if STA profile exists or not.
#ifdef RM_MAP_PERSISTANT_W
            if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                           WIFI_PROFILE_SSID_0, (int *)&prev_run_mode) != FSP_SUCCESS) {

#endif
                printf("\n!!! WIFI_DEVICE_MODE_EXT_AP mode : Does not change to concurrent-mode (STA+Soft-AP) !!!\n\n");
                return pdFALSE;
            }

            // Change to concurrent-mode : STA + Soft-AP
#ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG,
                                        (const char *)ENV_SWITCH_SYSMODE, (int)WIFI_DEVICE_MODE_EXT_AP);
#endif
#ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                        (const char *)WIFI_PROFILE_SYS_MODE, (int)WIFI_DEVICE_MODE_EXT_AP_STATION);
#endif
            break;

        case WIFI_DEVICE_MODE_EXT_AP_STATION :
            // Read saved previous sys_run_mode in NVRAM
#ifdef RM_MAP_PERSISTANT_W
            if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG,
                                        (const char *)ENV_SWITCH_SYSMODE, (int *)&prev_run_mode) != FSP_SUCCESS) {
#endif
                printf("\n!!! Not defined yet to switch sys_run_mode in NVRAM !!!\n\n");
                return pdFALSE;
            }

            // Change to concurrent-mode : Previous running mode
#ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                        (const char *)WIFI_PROFILE_SYS_MODE, prev_run_mode);
#endif
#ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG,
                                        (const char *)ENV_SWITCH_SYSMODE, (int)WIFI_DEVICE_MODE_EXT_AP_STATION);
#endif
            break;

        default :
            printf("!!! Unknown sys_run_mode (%d)\n\n", cur_run_mode);
            return pdFALSE;
    }
#endif // __SUPPORT_FACTORY_RST_CONCURR_MODE__

    return pdTRUE;
}
#endif    /* __SUPPORT_WIFI_CONCURRENT__ */
static bool stop_service(void)
{
    extern TaskHandle_t ra6wx_sp_thread;
    extern TaskHandle_t RWNX_DRIVER_TASK;
    extern TaskHandle_t RWNX_MAC_TASK;
    char reply[10]= {0, };

    R_WDOG_W_Refresh(g_wifi_cfg.p_watchdog_service->p_cfg->p_wdt->p_ctrl);

#if defined (__SUPPORT_MQTT__)
    mqtt_client_termination();
    #endif // __SUPPORT_MQTT__

    if (ra6wx_sp_thread) {
        if (ra6w1_cli_reply("flush", NULL, reply) > -1) {
            if (strcmp(reply, "OK") != 0)
            {
                return pdFAIL;
            }
            vTaskDelay(portCONVERT_MS_2_TICKS(200));
        }
    }

    /* The RT-Thread wrapper does not expose FreeRTOS task snapshots. The Wi-Fi
     * port owns these three tasks, so suspend those handles explicitly before
     * removing their queues and task objects. */
    if (ra6wx_sp_thread)
    {
        vTaskSuspend(ra6wx_sp_thread);
    }
    if (RWNX_DRIVER_TASK)
    {
        vTaskSuspend(RWNX_DRIVER_TASK);
    }
    if (RWNX_MAC_TASK)
    {
        vTaskSuspend(RWNX_MAC_TASK);
    }

    /* Queue Delete */
    extern void rwnx_mac_task_queue_del(void);
    extern void rwnx_driver_queue_del(void);
    rwnx_mac_task_queue_del();
    rwnx_driver_queue_del();

    /* Thread remove */
    if (ra6wx_sp_thread) {
        vTaskDelete(ra6wx_sp_thread);
        ra6wx_sp_thread = NULL;
    }

    if (RWNX_DRIVER_TASK) {
        vTaskDelete(RWNX_DRIVER_TASK);
        RWNX_DRIVER_TASK = NULL;
    }

    if (RWNX_MAC_TASK) {
        vTaskDelete(RWNX_MAC_TASK);
        RWNX_MAC_TASK = NULL;
    }

#if (dg_configUSE_TRACE_FOR_DEBUG == 1)
    clear_function_trace();
#endif	// dg_configUSE_TRACE_FOR_DEBUG

    R_WDOG_W_Refresh(g_wifi_cfg.p_watchdog_service->p_cfg->p_wdt->p_ctrl);

    return pdPASS;
}
#endif

bool reset(void)
{
    bool status = pdFAIL;
#if CFG_WIFI
    status = stop_service();
#endif
    if (status == pdFAIL) {
        printf("Error Stop Service(%d)\n", status);
        return pdFAIL;
    }

    printf("\nRebooting...\n\n");
    vTaskDelay(portCONVERT_MS_2_TICKS(100));

    R_BSP_RetainedMemFlagClear();

    PORRESET;

    return pdPASS;
}

bool por_reset(void)
{
    PORRESET;

    return pdPASS;
}

int get_current_rssi(void)
{
    return get_sta_signal_poll();
}

static fsp_err_t factory_nvram_reset(void)
{
    int status = FSP_ERR_UNSUPPORTED;

#ifdef RM_MAP_PERSISTANT_W
    if (pdFAIL == stop_service())
    {
        return FSP_ERR_ASSERTION;
    }

    /* Factory reset */
    status = RM_MAP_PERSISTANT_W_Erase_GROUP(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG);
    if (status != FSP_SUCCESS)
    {
        printf("Error Erase WIFICFG\n");
        return pdFAIL; //ER_DELETE_ERROR;
    }
    status = RM_MAP_PERSISTANT_W_Erase_GROUP(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG);
    if (status != FSP_SUCCESS)
    {
        printf("Error Erase SYSCFG\n");
        return pdFAIL; //ER_DELETE_ERROR;
    }
    status = RM_MAP_PERSISTANT_W_Erase_GROUP(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG);
    if (status != FSP_SUCCESS)
    {
        printf("Error Erase APPCFG\n");
        return pdFAIL; //ER_DELETE_ERROR;
    }
    status = RM_MAP_PERSISTANT_W_Erase_GROUP(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_TESTCFG);
    if (status != FSP_SUCCESS)
    {
        printf("Error Erase TESTCFG\n");
        return pdFAIL; //ER_DELETE_ERROR;
    }
    status = RM_MAP_PERSISTANT_W_Erase_GROUP(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE);
    if (status != FSP_SUCCESS)
    {
        printf("Error Erase EASYSETUP\n");
        return status; //ER_DELETE_ERROR;
    }
 #ifdef BLE_CFG_DA14xxx_DEVICE
    status = RM_MAP_PERSISTANT_W_Erase_GROUP(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_BLESEC);
    if (status != FSP_SUCCESS)
    {
        printf("Error Erase BLESEC\n");
        return status; //ER_DELETE_ERROR;
    }
 #endif

    vTaskDelay(portCONVERT_MS_2_TICKS(100));
#endif

    return status;
}

// cpu clock save nvram
static int del_cpu_clock_nvram(void)
{
    int status = 0;

#ifdef RM_MAP_PERSISTANT_W
    /* TODO check the status value of already existing code with Korean team
     * it returns pdfalse which is 0 and in status we are checking only < '0' */
    status = RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_BOOTCFG, "clk.cpu");
#endif

    if (status != FSP_SUCCESS)
    {
        return FALSE;
    }

    return TRUE;
}

static bool rm_wifi_helper_is_wep(char * security_str)
{
    return strcasestr(security_str, "wep");
}

static bool rm_wifi_helper_is_wpa(char * security_str)
{
    return strcasestr(security_str, "wpa-");
}

static bool rm_wifi_helper_is_wpa2(char * security_str)
{
    return strcasestr(security_str, "wpa2-");
}

static bool rm_wifi_helper_is_wpa3(char * security_str)
{
    char * keywords[] = {
        "wpa3-",
        "sae-",
    };

    for (unsigned i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++)
    {
        if (strcasestr(security_str, keywords[i]))
        {
            return true;
        }
    }

    return false;
}

static bool rm_wifi_helper_is_ent(char * security_str)
{
    return strcasestr(security_str, "eap");
}

static bool rm_wifi_helper_is_wpa3_ent(char * security_str)
{
    return (strcasestr(security_str, "eap-sha256-ccmp"));
}

static bool rm_wifi_helper_is_wpa2wpa3_ent(char * security_str)
{
    return (strcasestr(security_str, "wpa2-eap+eap-sha256-ccmp"));
}

static bool rm_wifi_helper_is_wpa3_192b_ent(char * security_str)
{
    return (strcasestr(security_str, "suite-b-192-gcmp-256"));
}

static bool rm_wifi_helper_is_wpa2_wpa3(char * security_str)
{
    return (strcasestr(security_str, "wpa2-") && strcasestr(security_str, "+sae"));
}

static bool rm_wifi_helper_is_wpa3_owe(char * security_str)
{
    return strcasestr(security_str, "owe-");
}

WIFISecurityExt_t rm_wifi_helper_security_str_to_type(char * security_str)
{
    WIFISecurityExt_t security_type = eWiFiSecurityOpen_ext;

    const char * group_delimiters = "]";

    char * per_group_saveptr = NULL;
    char * token = strtok_r(security_str, group_delimiters, &per_group_saveptr);

    while (token != NULL) {
        char * group = token + 1;

        if (rm_wifi_helper_is_wep(group) && (security_type < eWiFiSecurityWEP_ext)) {
            security_type = eWiFiSecurityWEP_ext;
        }

        if (rm_wifi_helper_is_wpa(group) && (security_type < eWiFiSecurityWPA_ext)) {
            security_type = eWiFiSecurityWPA_ext;
        }

        if (rm_wifi_helper_is_wpa2(group) && (security_type != eWiFiSecurityWPA_ext &&  security_type < eWiFiSecurityWPA2_ext)) {
            security_type = eWiFiSecurityWPA2_ext;
        }

        if (rm_wifi_helper_is_wpa2(group) && rm_wifi_helper_is_ent(group) && (security_type != eWiFiSecurityWPA_ent_ext) &&
            (security_type < eWiFiSecurityWPA2_ent_ext)) {
            security_type = eWiFiSecurityWPA2_ent_ext;
        }

        if (rm_wifi_helper_is_wpa3(group) && (security_type < eWiFiSecurityWPA3_ext)) {
            security_type = eWiFiSecurityWPA3_ext;
        }

        if (rm_wifi_helper_is_wpa(group) && rm_wifi_helper_is_ent(group) &&
            (security_type != eWiFiSecurityWPA2_ent_ext && security_type < eWiFiSecurityWPA_ent_ext)) {
            security_type = eWiFiSecurityWPA_ent_ext;
        }

        if ((security_type == eWiFiSecurityWPA_ent_ext) && rm_wifi_helper_is_wpa2(group) && rm_wifi_helper_is_ent(group)) {
            security_type = eWiFiSecurityWPA_WPA2_ent_ext;
        }

        if (rm_wifi_helper_is_wpa2wpa3_ent(group) && rm_wifi_helper_is_ent(group) && (security_type < eWiFiSecurityWPA2_WPA3_ent_ext)) {
            security_type = eWiFiSecurityWPA2_WPA3_ent_ext;
        }

        if (rm_wifi_helper_is_wpa2(group) && rm_wifi_helper_is_ent(group) && rm_wifi_helper_is_wpa3_ent(group) &&
            (security_type != eWiFiSecurityWPA2_WPA3_ent_ext) &&(security_type < eWiFiSecurityWPA3_ent_ext)) {
            security_type = eWiFiSecurityWPA3_ent_ext;
        }

        if (rm_wifi_helper_is_wpa2(group) && rm_wifi_helper_is_ent(group) && rm_wifi_helper_is_wpa3_192b_ent(group) &&
            (security_type < eWiFiSecurityWPA3_192B_ent_ext)) {
            security_type = eWiFiSecurityWPA3_192B_ent_ext;
        }

        if (rm_wifi_helper_is_wpa2(group) && (security_type == eWiFiSecurityWPA_ext) && (security_type < eWiFiSecurityWPA_WPA2_ext)) {
            security_type = eWiFiSecurityWPA_WPA2_ext;
        }

        if (rm_wifi_helper_is_wpa2_wpa3(group) && (security_type < eWiFiSecurityWPA2_WPA3_ext)) {
            security_type = eWiFiSecurityWPA2_WPA3_ext;
        }

        if (rm_wifi_helper_is_wpa3_owe(group) && (security_type < eWiFiSecurityWPA3_OWE_ext)) {
            security_type = eWiFiSecurityWPA3_OWE_ext;
        }

        token = strtok_r(NULL, group_delimiters, &per_group_saveptr);
    }

    return security_type;
}

WIFISecurityExt_t rm_wifi_helper_security_type_get(UINT key_mgmt, UINT proto, UINT pairwise_cipher)
{
    char security_str[64] = {0};

    snprintf(security_str, 64, "[%s-%s]",
             wpa_key_mgmt_txt(key_mgmt, proto),
             wpa_cipher_txt(pairwise_cipher));

    return rm_wifi_helper_security_str_to_type(security_str);
}

int factory_reset(int reboot_flag)
{
    int status = pdFAIL;
#if CFG_WIFI
    status = stop_service();
#endif
    if (status == pdFAIL) {
        printf("Error Stop Service(%d)\n", status);
        return pdFAIL;
    }

    status = RM_CERT_DeleteAll();

    if (status > 0) {
        printf("Error Cert Delete(%d)\n", status);
        return pdFAIL;
    }

    /* Factory reset */
    status = factory_nvram_reset();
    if (FSP_SUCCESS != status) {
        printf("Error Factory NVRAM(%d)\n", status);
        return pdFAIL;
    }

#if defined (__SUPPORT_AWS_IOT_W__) && defined ( RM_MAP_PERSISTANT_W )
    RM_MAP_PERSISTANT_W_Erase_GROUP(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG);
#endif
#if defined (__SUPPORT_AWS_IOT_W__) && defined (__SUPPORT_ATCMD__)
    RM_CERT_Delete(RM_CERT_MODULE_ATCMD, RM_CERT_TYPE_CA_CERT);
    RM_CERT_Delete(RM_CERT_MODULE_ATCMD, RM_CERT_TYPE_CERT);
    RM_CERT_Delete(RM_CERT_MODULE_ATCMD, RM_CERT_TYPE_PRIVATE_KEY);
#endif

    /* Remove saved running CPU clock by user */
    status = del_cpu_clock_nvram();

    vTaskDelay(portCONVERT_MS_2_TICKS(400));

    if (reboot_flag)
    {
        reset();
    }

    return status;
}
#if CFG_WIFI
int getMacAddrMswLsw(UINT iface, ULONG * macmsw, ULONG * maclsw)
{
    char * macaddr_str = NULL;
    int idx;
    int type = 0;

    /* MAC Spoofing for Station Only */
    if (get_run_mode() == 0 && iface == WLAN0_IFACE)
    {
#ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, "0:MAC_SP", &macaddr_str);
#endif
        type = MAC_SPOOFING;
    }

    /* NVRAM MAC Address */
    if (macaddr_str == NULL)
    {
#ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_DEVCFG, "WLANMAC", &macaddr_str);
#endif
        type = NVRAM_MAC;
    }

    if (macaddr_str != NULL && strlen(macaddr_str) == 12)
    {
        char tmp_macstr[9];

        for (idx = 0; idx < 12; idx++)
        {
            if (!isxdigit((int) (macaddr_str[idx])))
            {
                goto error;
            }
        }

        /* MSW */
        memset(tmp_macstr, 0, 9);
        memcpy(tmp_macstr, macaddr_str, 4);
        *macmsw = strtoul(tmp_macstr, NULL, 16);

        /* LSW */
        memset(tmp_macstr, 0, 9);
        memcpy(tmp_macstr, macaddr_str + 4, 8);
        *maclsw = strtoul(tmp_macstr, NULL, 16);
    }

    /* OTP MAC Address*/
    if (macaddr_str == NULL)
    {
        uint32_t mac_address[2] = {0, };

        if (bsp_tcs_otp_read_mac(mac_address))
        {
            *macmsw = mac_address[1];
            *maclsw = mac_address[0];
#if 0 //for Debug
            char    macaddr_otp_str[13] = {0, };

            printf("MAC: %04x%08x\n", (unsigned int)(mac_address[1] & 0xffff), (unsigned int)mac_address[0]);
            sprintf(macaddr_otp_str, "%04x%08x", (unsigned int)(mac_address[1] & 0xffff), (unsigned int)mac_address[0]);
            printf("\tMAC Addr: %c%c:%c%c:%c%c:%c%c:%c%c:%c%c\n",
                   macaddr_otp_str[0], macaddr_otp_str[1],
                   macaddr_otp_str[2], macaddr_otp_str[3],
                   macaddr_otp_str[4], macaddr_otp_str[5],
                   macaddr_otp_str[6], macaddr_otp_str[7],
                   macaddr_otp_str[8], macaddr_otp_str[9],
                   macaddr_otp_str[10], macaddr_otp_str[11]);
#endif //for Debug
            type = OTP_MAC;
        }
        else
        {
            goto error;
        }
    }

    if (macmsw != NULL && maclsw != NULL)
    {
        if (iface == WLAN1_IFACE)
        {
#if SUPPORT_WLAN1_LOCAL_MACADDRESS
            /* locally MAC Address */
            *macmsw = *macmsw | BIT(1 + 8);
#else
            (*maclsw)++;
#endif /* SUPPORT_WLAN1_LOCAL_MACADDRESS */
        }
    }
    else
    {
error:
        *macmsw = DEFULT_MAC_MSW;
        *maclsw = DEFULT_MAC_LSW;

        printf("\33[41m<<Please check MAC address.>>\33[49m\n");
        return -1;
    }

    return type;
}

static unsigned char toint(char c)
{
    unsigned char rslt;

    if ((c >= '0') && (c <= '9'))
    {
        rslt = (c - '0');
    }
    else if ((c >= 'a') && (c <= 'f'))
    {
        rslt = (c - 'a' + (unsigned char) 10);
    }
    else if ((c >= 'A') && (c <= 'F'))
    {
        rslt = (c - 'A' + (unsigned char) 10);
    }
    else
    {
        rslt = (unsigned char) 0;
    }

    return rslt;
}

UINT writeMACaddress(char * mac_addr, int dst)
{
    UINT status = E_WRITE_OK;
    UINT idx, len;
    char tmp_macstr[13];
    char tmpstr[3];

    memset(tmp_macstr, 0, 13);
    len = strlen(mac_addr);

    if (len == 17 || len == 12)
    {
        if (len == 12)
        {
            sprintf(tmp_macstr, "%c%c%c%c%c%c%c%c%c%c%c%c",
                    tolower(mac_addr[0]),  tolower(mac_addr[1]),
                    tolower(mac_addr[2]),  tolower(mac_addr[3]),
                    tolower(mac_addr[4]),  tolower(mac_addr[5]),
                    tolower(mac_addr[6]),  tolower(mac_addr[7]),
                    tolower(mac_addr[8]),  tolower(mac_addr[9]),
                    tolower(mac_addr[10]), tolower(mac_addr[11]));
        }
        else
        {
            sprintf(tmp_macstr, "%c%c%c%c%c%c%c%c%c%c%c%c",
                    tolower(mac_addr[0]),  tolower(mac_addr[1]),
                    tolower(mac_addr[3]),  tolower(mac_addr[4]),
                    tolower(mac_addr[6]),  tolower(mac_addr[7]),
                    tolower(mac_addr[9]),  tolower(mac_addr[10]),
                    tolower(mac_addr[12]), tolower(mac_addr[13]),
                    tolower(mac_addr[15]), tolower(mac_addr[16]));
        }

        for (idx = 0; idx < 12; idx++)
        {
            if (!isxdigit((int) (tmp_macstr[idx])))
            {
                status = E_DIGIT_ERROR;
                goto error;
            }

            if (idx == 1)
            {
                /* Check mulicast address */
                if (toint(mac_addr[idx]) & BIT(0))
                {
                    status = E_MCAST_ERROR;
                    goto error;
                }

                /* Check locally address */
                if (dst != MAC_SPOOFING && (toint(mac_addr[idx]) & BIT(1)))
                {
                    status = E_LOCAL_ERROR;
                    goto error;
                }
            }
        }

        strncpy(tmpstr, tmp_macstr, 2);
        if (!strncmp(tmp_macstr, "000000000000", 12) ||
            !strncmp(tmp_macstr, "ffffffffffff", 12) ||
            strtoul(tmpstr, NULL, 16) % 2 == 1)
        {
            status = E_INVALID_ERROR;
            goto error;
        }
    }
    else
    {
        if (dst == MAC_SPOOFING
            && strncasecmp(mac_addr, "erase", 5) == 0)
        {
#ifdef RM_MAP_PERSISTANT_W
            if (RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, "0:MAC_SP") != FSP_SUCCESS)
            {
#endif
                status = E_ERASE_ERROR;
            }
            else
            {
                status = E_ERASE_OK;
            }
        }
        else if (dst == NVRAM_MAC && strncasecmp(mac_addr, "erase", 5) == 0)
        {
#ifdef RM_MAP_PERSISTANT_W
            if (RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_DEVCFG, "WLANMAC") != FSP_SUCCESS)
            {
#endif
                status = E_ERASE_ERROR;
            }
            else
            {
                status = E_ERASE_OK;
            }
        }
        else
        {
            status = E_DIGIT_ERROR;
        }
    }

    if (status == E_WRITE_OK)
    {
#ifndef SUPPORT_WLAN1_LOCAL_MACADDRESS
        /* Check  last digit Even(Exception: Mac Spoofing) */
        if  (strtoul(tmp_macstr + 9, NULL, 16) % 2 == 0 || dst == MAC_SPOOFING)
#endif /* SUPPORT_WLAN1_LOCAL_MACADDRESS */
        {
            status = E_WRITE_ERROR;

            if (dst == MAC_SPOOFING)
            {
#ifdef RM_MAP_PERSISTANT_W
                if (RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                   ENV_GROUP_SYSCFG, "0:MAC_SP", tmp_macstr) == 0)
                {
#endif
                    status = E_WRITE_OK;
                }
                else
                {
                    status = E_WRITE_ERROR;
                }
            }
            else if (dst == NVRAM_MAC)
            {
#ifdef RM_MAP_PERSISTANT_W
                if (RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                   ENV_GROUP_DEVCFG, "WLANMAC", tmp_macstr) == 0)
                {
#endif
                    status = E_WRITE_OK;
                }
                else
                {
                    status = E_WRITE_ERROR;
                }
            }
            else if (dst == OTP_MAC)
            {
                uint32_t mac_address[2] = {0, };
                char macaddr_otp_str[13] = {0,};
                int i;
                uint8_t tmp_mac[6] = {0, };

                if (bsp_tcs_otp_read_mac(mac_address))
                {
                    sprintf(macaddr_otp_str, "%04x%08x", (unsigned int) (mac_address[1] & 0xffff), (unsigned int) mac_address[0]);
                    if (strcmp(macaddr_otp_str, tmp_macstr) == 0)
                    {
                        return E_SAME_ERROR;
                    }
                }

                for (i = 0; i < 6; i++)
                {
                    tmp_mac[i] = toint(tmp_macstr[i * 2]) << 4 | toint(tmp_macstr[(i * 2) + 1]);
                }

                memcpy(&mac_address[0], tmp_mac + 2, 4);
                memcpy(((uint8_t *) (&mac_address[1])) + 2, tmp_mac, 2);

                mac_address[0] = (tmp_mac[2] << 24 & 0xff000000) | (tmp_mac[3] << 16 & 0xff0000) | (tmp_mac[4] << 8 & 0xff00) | (tmp_mac[5] & 0xff);
                mac_address[1] = (tmp_mac[0] << 8 & 0xff00) | (tmp_mac[1] & 0xff);

#if 0 // for Debug
                printf("mac_addr raw: %04lx%08lx\n", mac_address[1], mac_address[0]);
                return E_WRITE_OK;
#endif // for Debug

                if (bsp_tcs_otp_write_mac(mac_address, CHECK_OPTION_DIFFERENT))
                {
                    status = E_WRITE_OK;
                }
                else
                {
                    status = E_WRITE_ERROR;
                }
            }
#ifndef SUPPORT_WLAN1_LOCAL_MACADDRESS
        }
        else
        {
            status = E_DIGIT_ERROR; // Error Odd
#endif // SUPPORT_WLAN1_LOCAL_MACADDRESS
        }
    }

error:
    return status;
}

int getMACAddrStr(unsigned int iface, char * macstr, unsigned int separate)
{
    int status = 0;
    unsigned long macmsw, maclsw;

    /* Get MAC Address */
    status = getMacAddrMswLsw(iface, &macmsw,  &maclsw);

    if (((int) status) >= 0)
    {
        if (separate)
        {
            snprintf(macstr, NET_INFO_STR_LEN, "%02lX:%02lX:%02lX:%02lX:%02lX:%02lX",
                     ((macmsw >> 8) & 0Xff),
                     (macmsw & 0xff),
                     ((maclsw >> 24) & 0xff),
                     (maclsw >> 16 & 0xff),
                     (maclsw >> 8  & 0xff),
                     (maclsw & 0xff));
        }
        else
        {
            snprintf(macstr, 13, "%02lX%02lX%02lX%02lX%02lX%02lX",
                     ((macmsw >> 8) & 0xff),
                     (macmsw & 0xff),
                     ((maclsw >> 24) & 0xff),
                     (maclsw >> 16 & 0xff),
                     (maclsw >> 8  & 0xff),
                     (maclsw & 0xff));
        }
    }

    return status;
}

int rm_wifi_helper_country_code_is_valid(char * country_code)
{
    if (ra6w1_regdb_get_cty_idx_from_cc(country_code) == -1)
    {
        return pdFALSE;
    }
    else
    {
        return pdTRUE;
    }
}

int rm_wifi_helper_get_ch_range_by_country_n_band(char * country,
                                                     int band,
                                                   int * min_ch,
                                                   int * max_ch,
                                          unsigned int * ch_bitmap_5g,
                                            unsigned int exclude_flags)
{
    return ra6w1_regdb_get_ch_range_by_country_n_band(country, band, min_ch, max_ch, ch_bitmap_5g, exclude_flags);
}

void rm_wifi_helper_gen_string_5g_ch_range(char * str_out, unsigned int ch_bitmap, char delimiter)
{
    ra6w1_regdb_gen_5g_ch_range_string(str_out, ch_bitmap, delimiter);
    
    return;
}
#endif
/*******************************************************************************************************************//**
 * @} (end addtogroup WIFI)
 **********************************************************************************************************************/

