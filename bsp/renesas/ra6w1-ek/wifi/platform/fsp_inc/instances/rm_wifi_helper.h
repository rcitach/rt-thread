/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef RM_WIFI_HELPER_H
#define RM_WIFI_HELPER_H

#define CC_STATUS_SUCCESS           0

/// Too long string value input
#define CC_FAILURE_STRING_LENGTH    1

/// No value input
#define CC_FAILURE_NO_VALUE         2

/// Range out
#define CC_FAILURE_RANGE_OUT        3

/// Not Supported input
#define CC_FAILURE_NOT_SUPPORTED    4

/// Invalid input
#define CC_FAILURE_INVALID          5

/// Memory Allocation Failure
#define CC_FAILURE_NO_ALLOCATION    6

/// Not Ready (Wi-Fi connection or Network setting)
#define CC_FAILURE_NOT_READY        7

/// Unknown Reason
#define CC_FAILURE_UNKNOWN          9

/// Enable/Disable values
typedef enum
{
    /// Not used
    CC_VAL_DISABLE,

    /// Used
    CC_VAL_ENABLE,
} cc_val_bool;

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "FreeRTOS.h"
#include "event_groups.h"
#if CFG_WIFI
 #include "util_api.h"
#endif                                 // CFG_WIFI

/**********************************************************************************************************************
 * External Functions Prototypes
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Defines
 **********************************************************************************************************************/

// atoi error policy
// default leading "0" / "+" / "-0" are not allowed
#define POL_1             1

// leading "+" / "-0" are not allowed
#define POL_2             2
#if CFG_WIFI

 #define CH_FLAG_NO_IR    (1 << 1)
 #define CH_FLAG_DFS      (1 << 3)

 #define rm_wifi_write_mac_address(...)              writeMACaddress(__VA_ARGS__)
 #define rm_wifi_get_mac_address_string(...)         getMACAddrStr(__VA_ARGS__)
 #define rm_wifi_util_sflash_write(...)              util_sflash_write(__VA_ARGS__)
 #define rm_wifi_util_sflash_read(...)               util_sflash_read(__VA_ARGS__)
 #define rm_wifi_util_sflash_erase(...)              util_sflash_erase(__VA_ARGS__)
 #define rm_wifi_is_in_softap_acs_mode(...)          is_in_softap_acs_mode(__VA_ARGS__)

/* wpa_supplicant functions */
 #define rm_wifi_chk_channel_by_country(...)         chk_channel_by_country(__VA_ARGS__)

/* supplicant functions */
 #define rm_wifi_select_ipaddr_type_from_str(...)    select_ipaddr_type_from_str(__VA_ARGS__)
#endif                                 // CFG_WIFI

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
#if CFG_WIFI

/* For select_ipaddr_type_from_str() */
struct sockaddr_storage;
#endif                                 // CFG_WIFI

/**********************************************************************************************************************
 * Helper functions used by other modules internally
 **********************************************************************************************************************/
#if CFG_WIFI
int  gen_ssid(char * prefix, int iface, int quotation, char * ssid, int size);
int  rm_wifi_atoi_custom(char * str);
int  rm_wifi_get_int_val_from_str(char * param, int * int_val, int policy);
void rm_wifi_register_wifi_notify_cb(void);
void factory_reset_sta_mode(void);
void factory_reset_ap_mode(void);
int  getMacAddrMswLsw(UINT iface, ULONG * macmsw, ULONG * maclsw);
int  getMACAddrStr(unsigned int iface, char * macstr, unsigned int separate);
UINT writeMACaddress(char * macaddr, int dst);

#endif                                 // CFG_WIFI
bool reset(void);
bool por_reset(void);

#if CFG_WIFI
int get_current_rssi(void);

#endif                                 // CFG_WIFI
void MemManage_Handler(void);
void BusFault_Handler(void);

#if CFG_WIFI
unsigned int      wait_supplicant_done(unsigned int timeout);
void              wifi_conn_fail_noti_to_atcmd_host(void);
void              print_sys_mode(unsigned int mode);
WIFISecurityExt_t rm_wifi_helper_security_str_to_type(char * security_str);
WIFISecurityExt_t rm_wifi_helper_security_type_get(UINT key_mgmt, UINT proto, UINT pairwise_cipher);

/* wpa_cli functions */
extern int  cc_set_network_str(const char * name, int iface, const char * val);
extern int  cc_set_network_int(char * name, int iface, int val);
extern int  ra6w1_cli_reply(char * cmdline, char * delimit, char * cli_reply);
extern void ra6w1_wpa_cli(int argc, char * argv[]);

/* wpa_supplicant functions */
extern int get_run_mode(void);
extern int select_ipaddr_type_from_str(char * str, struct sockaddr_storage * ipaddr);

extern bool twt_setup(struct twt_setup_req * req);
extern bool twt_teardown(struct twt_teardown_req * req);

#endif                                 // CFG_WIFI
#if defined(__SUPPORT_WIFI_CONCURRENT__)
 #if defined(__SUPPORT_FACTORY_RST_CONCURR_MODE__)
void factory_reset_concurrent_mode(void);

 #endif                                // __SUPPORT_FACTORY_RST_CONCURR_MODE__
int factory_reset_btn_onetouch(void);

#endif                                 // __SUPPORT_WIFI_CONCURRENT__

int rm_wifi_helper_country_code_is_valid(char * country_code);

int rm_wifi_helper_get_ch_range_by_country_n_band(char         * country,
                                                  int            band,
                                                  int          * min_ch,
                                                  int          * max_ch,
                                                  unsigned int * ch_bitmap_5g,
                                                  unsigned int   exclude_flags);

void rm_wifi_helper_gen_string_5g_ch_range(char * str_out, unsigned int ch_bitmap, char delimiter);

#endif                                 // RM_WIFI_HELPER_H
