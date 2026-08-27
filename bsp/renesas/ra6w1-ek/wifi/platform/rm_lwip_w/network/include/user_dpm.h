/**
 ****************************************************************************************
 *
 * @file user_dpm.h
 *
 * @brief Definition for User DPM feature.
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

#ifndef __USER_DPM_H__
#define __USER_DPM_H__

#include "bsp_api.h"

#if CFG_PMGR
 #include "FreeRTOS.h"
 #include "custom_config_sdk.h"

enum USER_DPM_TID
{
    TID_U_USER_WAKEUP = 2,
    TID_U_DHCP_CLIENT,
    TID_U_ABNORMAL
};

/////////////////////////////////////////////////////////////////////////////
/// RA6WX DPM Structures   ///////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

 #define DPM_ETH_ALEN           6
 #define DPM_MAX_SSID_LEN       32
 #define DPM_MAX_WEP_KEY_LEN    16
 #define DPM_PSK_LEN            32

/// Global network information for DPM operation
typedef  struct
{
    /* for Network Instances */
    char net_mode;

    /* for ra6wx Supplicant */
    char wifi_mode;
    char country[4];

    char reserve_1[2];
} user_dpm_supp_net_info_t;

/// Global IP address information for DPM operation
typedef  struct
{
    /// DPM dhcp xid
    long dpm_dhcp_xid;

    /// DPM IP address
    uint32 dpm_ip_addr;

    /// DPM IP address Netmask
    uint32 dpm_netmask;

    /// DPM Gateway address
    uint32 dpm_gateway;

    /// DPM DNS server address
    uint32 dpm_dns_addr[2];

    /// DPM DHCP Client Lease timeout
    int32 dpm_lease;

    /// DPM DHCP Client renew timeout
    int32 dpm_renewal;

    /// DPM DHCP Client operation timeout
    uint32 dpm_timeout;                // dpm_rebind

    /// DPM DHCP Client renew Server IP address
    uint32 dpm_dhcp_server_ip;
} user_dpm_supp_ip_info_t;

typedef  struct
{
    int mode;
    int disabled;

    int           id;
    int           ssid_len;
    int           scan_ssid;
    int           psk_set;
    int           auth_alg;
    unsigned char bssid[DPM_ETH_ALEN];    // 6
    unsigned char reserved[2];            // Padding 2bytes
    unsigned char ssid[DPM_MAX_SSID_LEN]; // 32
    unsigned char psk[DPM_PSK_LEN];       // 32

 #if defined(DEF_SAVE_DPM_WIFI_MODE)
    int wifi_mode;
    int dpm_opt;

  #ifdef __SUPPORT_IEEE80211W__
    unsigned char pmf;
    unsigned char reserved_2[3];       // Padding 3bytes
  #else
    unsigned char reserved_2[4];       // 4bytes
  #endif // __SUPPORT_IEEE80211W__
 #else                                 /////////////////////////////////
  #ifdef __SUPPORT_IEEE80211W__
    unsigned char pmf;
    unsigned char reserved_2[11];      // Padding 3bytes + 9bytes
  #else
    unsigned char reserved_2[12];      // 12bytes
  #endif // __SUPPORT_IEEE80211W__
 #endif                                // DEF_SAVE_DPM_WIFI_MODE
} user_dpm_supp_conn_info_t;

 #define WPA_KCK_MAX_LEN    32
 #define WPA_KEK_MAX_LEN    64
 #define WPA_TK_MAX_LEN     32

typedef  struct
{
    int   wpa_alg;
    int   key_idx;
    int   set_tx;
    uint8 seq[6];
    int   seq_len;
    uint8 ptk_kck[WPA_KCK_MAX_LEN];    // EAPOL-Key Key Confirmation Key (KCK)
    uint8 ptk_kek[WPA_KEK_MAX_LEN];    // EAPOL-Key Key Encryption Key (KEK)
    uint8 ptk_tk1[WPA_TK_MAX_LEN];     // Temporal Key 1 (TK1)
    int   key_len;
} user_cipher_ptk_t;

typedef  struct
{
    int   wpa_alg;
    int   key_idx;
    int   set_tx;
    uint8 seq[6];
    int   seq_len;
    uint8 gtk[32];
    int   key_len;
} user_cipher_gtk_t;

typedef  struct
{
    int proto;
    int key_mgmt;
    int pairwise_cipher;
    int group_cipher;

    uint8 wep_key_len;
    uint8 wep_tx_keyidx;
    uint8 wep_key[DPM_MAX_WEP_KEY_LEN];

    user_cipher_ptk_t ptk;
    user_cipher_gtk_t gtk;

    uint8 reserve_1[8];
} user_dpm_supp_key_info_t;

// DPM RTC Timer defines
 #define DPM_MODE_NOT_ENABLED       -1
 #define DPM_TIMER_SEC_OVERFLOW     -2
 #define DPM_TIMER_ALREADY_EXIST    -3
 #define DPM_TIMER_NAME_ERROR       -4
 #define DPM_UNSUPPORTED_RTM        -6
 #define DPM_TIMER_REGISTER_FAIL    -7

 #define DPM_TIMER_MAX_ERR          16 // TIMER_ERR

extern uint8 get_last_abnormal_act(void);

/**
 *********************************************************************************
 * @brief     Display User RTM (Retention Memory) usage
 * @return    None
 *********************************************************************************
 */
void show_rtm_for_app(void);

/**
 *********************************************************************************
 * @brief     Check wakeup state is Abnormal-Wakeup when DPM wakeup
 * @return    0 when Abnormal-wakeup, 1 other wakeup
 *********************************************************************************
 */
int chk_abnormal_wakeup(void);

#endif                                 /* CFG_PMGR */

//
//////////////////////////////////////////////////////////////////////

#endif                                 /* __USER_DPM_H__ */

/* EOF */
