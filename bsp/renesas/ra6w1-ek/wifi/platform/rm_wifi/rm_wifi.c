/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "limits.h"
#if defined(__SUPPORT_WIFI_USER_GPIO__)
 #include "r_gpio_w.h"
#endif
#include "rm_wifi.h"
#include "lwip/timeouts.h"
#include "stream_buffer.h"
#include "semphr.h"
#include "portmacro.h"
#include "defs.h"
#include "iface_defs.h"
#include "rm_wifi_helper.h"
#include "rm_wifi_event.h"
#include "common_def.h"
#include "supp_config.h"
#include "rm_wifi_api.h"
#include "fsp_common_api.h"
#include "rm_cert.h"
#include "rm_wifi_dpm.h"
#include "rm_vee_flash_w_rrq_nvram.h"

#if CFG_PMGR
#include "rm_pmgr_api.h"
#include "rm_pmgr_w_instance.h"
#endif
#ifdef RM_MAP_PERSISTANT_W
#include "rm_map_persistant_w.h"
#endif

/***********************************************************************************************************************
 * Externs
 **********************************************************************************************************************/
extern void phy_rf_enable(void);
extern void phy_rf_disable(void);
extern const ioport_instance_t g_gpio_w;
extern unsigned int wait_supplicant_done(unsigned int timeout);
extern unsigned int wait_supplicant_deinit(unsigned int timeout);
extern int request_stop_supplicant(void);
extern int request_start_supplicant(void);
extern void set_run_mode(int mode);
extern BaseType_t rwnx_driver_task_initiailize(void);
extern BaseType_t rwnx_mac_task_initiailize(void);
extern int ra6w1_cli_reply(char *cmdline, char *delimit, char *cli_reply);
extern int ra6w1_regdb_ch_to_freq(int ch);
extern int get_sta_signal_poll(void);
extern struct wpa_supplicant *get_wpa_supplicant(void );
extern enum wpa_states wpa_supplicant_get_state(struct wpa_supplicant *wpa_s);
extern const char * ap_get_state(struct wpa_supplicant *wpa_s);
extern int getMacAddrMswLsw(UINT iface, ULONG *macmsw, ULONG *maclsw);
extern int set_sys_mode(int mode);
extern int get_sys_mode(void);
extern int get_run_mode(void);
extern bool me_ps_on_get(void);
extern int rwnx_send_mm_set_ps_options(u8 if_index, u16 listen_interval, s8 dont_wait_bcmc);
extern int i3ed11_freq_to_ch(int);
extern int wpa_supplicant_set_atcmd_event_callback(void * const p_ctrl,
                                                   unsigned int (* p_callback)(void * const p_ctrl, int index,
                                                                               unsigned char * p_in, unsigned int inlen));
extern struct netif *net_get_netif(int iface_index);
#if (ATCMD_IF_SUPPORT == 1)
extern uint32_t atcmd_set_startup_atcmd_event_callback(void * const p_ctrl, unsigned int (* p_callback)(void * const p_ctrl, unsigned char * p_in, unsigned int inlen));
extern uint32_t atcmd_set_initdone_resp(char * p_out, size_t outlen, int is_startup);
#endif

extern char * strtok_r(char * str, const char * delim, char ** saveptr);
extern char * strcasestr(const char * haystack, const char * needle);
extern UINT chk_network_ready(UCHAR iface);

/***********************************************************************************************************************
 * Enumerations
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Defines
 **********************************************************************************************************************/

#define INVALID_RSSI -9999
#define DELAY_100MS  100
#define COUNTER_MAX 50
#define RM_WIFI_STATUS_PARAMS_MAX_LEN 19
#define RM_WIFI_AP_STATUS_VALUE_MAX_LEN 39
#define RM_WIFI_STA_STATUS_VALUE_MAX_LEN 160
#define RM_WIFI_COUNTRY_CODE_LEN 2
#define RM_WIFI_SET_COUNTRY_CMD_LEN 11
#define RM_WIFI_SCAN_BUF_SIZE 6144
#define RM_WIFI_STATUS_BUF_SIZE 1024
#define RM_WIFI_SET_COUNTRY_BUFF_SIZE 18
#define RM_WIFI_GET_COUNTRY_BUFF_SIZE 6
#define RM_WIFI_CHAR_MAC_LEN 13
#define RM_WIFI_BSSID_PARAMS_MAX_LEN 17
#define RM_WIFI_SECURITY_PARAMS_MAX_LEN 64
#define RM_WIFI_STATION_LIST_BUFF_SIZE 180

#define RM_WIFI_PS_LWIP_TIMEOUTS_ADDITION_MSEC 60000

#define STR_VALUE(arg)      #arg
#define STRINGIFY(arg)      STR_VALUE(arg)

/* OK/FAIL Only */
#define REPLY_SIZE 5
#define CLI_REPLY_AND_CHECK(cmd) cli_reply_and_check(cmd, __func__, __LINE__)
#define REPLY_SIZE_256 256

/***********************************************************************************************************************
 * Constants
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static Globals
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private Function Definitions
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 *  Call ra6w1_cli_reply and check its return
 *
 * @param[in]   cmd        wpa cli command
 * @param[in]   func       __func__
 * @param[in]   line       __LINE__
 * @param[out]  reply_buf  buffer to store the reply
 * @retval      The return value of ra6w1_cli_reply or FSP_ERR_WIFI_FAILED if FAIL
 **********************************************************************************************************************/
static inline int cli_reply_generic(char *cmd, const char *func, int line, char *reply_buf)
{
    int ret_val;

    ret_val = ra6w1_cli_reply(cmd, NULL, reply_buf);
    if (ret_val || strncmp(reply_buf, "FAIL", 4) == 0)
    {
        printf("%s:%d cli %s failed ret=%d, reply=%s\n", func, line, cmd, ret_val, reply_buf);

        if (ret_val == 0)
        {
            ret_val = FSP_ERR_WIFI_FAILED;
        }
    }

    return ret_val;
}

/*******************************************************************************************************************//**
 *  Allocate default reply buffer, and call cli_reply_generic.
 *
 * @param[in]   cmd        wpa cli command
 * @param[in]   func       __func__
 * @param[in]   line       __LINE__
 * @retval      The return value of ra6w1_cli_reply or FSP_ERR_WIFI_FAILED if FAIL
 * @note        This function should not be called from commands that require a reply larger than OK/FAIL (e.g scan) 
 **********************************************************************************************************************/
static inline int cli_reply_and_check(char *cmd, const char *func, int line)
{
    char reply[REPLY_SIZE] = {0};

    return cli_reply_generic(cmd, func, line, reply);
}

/*******************************************************************************************************************//**
 *  To remove the quotes from a single quoted string.
 *
 * @param[in]   len        Length of the input string.
 * @param[in]   input      Input string, potentially quoted (not null terminated).
 * @param[out]  output     The output buffer without the quotes (not null terminated).
 * @retval      The output pointer
 * @note        The length of the input string is modified if the string is quoted.
 **********************************************************************************************************************/
static char * unquote(uint8_t *len, char *output, const char *input)
{
    const char qt = '\'';

    if (*len > 2 && input[0] == qt && input[*len - 1] == qt)
    {
        memcpy(output, input + 1, *len - 2);
        *len -= 2;
    }
    else
    {
        memcpy(output, input, *len);
    }

    return output;
}

/*******************************************************************************************************************//**
 *  Ensure a string is quoted with a single quote.
 *
 * @param[in]   len        Length of the input string
 * @param[in]   input      Input string, potentially quoted, can be empty.
 * @param[out]  output     The output buffer, should be large enough to hold the quoted output string.
 * @retval      the output pointer
 **********************************************************************************************************************/
static char * quote(int len, char *output, const char *input)
{
    const char qt = '\'';
    const char qt2 = '\"';

    if (len >= 2 && input[0] == qt && input[len - 1] == qt)
    {
        memcpy(output, input, len);
        output[len] = '\0';
    }
    else if (len >= 2 && input[0] == qt2 && input[len - 1] == qt2)
    {
        memcpy(output + 1, input + 1, len - 2); // excluding double quotes
        output[0] = qt;
        output[len - 1] = qt;
        output[len] = '\0';
    }
    else
    {
        memcpy(output + 1, input, len);
        output[0] = qt;
        output[len + 1] = qt;
        output[len + 2] = '\0';
    }
    return output;
}

/*******************************************************************************************************************//**
 *  To escape special characters in a string by adding backslashes.
 *
 * @param[in]   len        Length of the input string.
 * @param[in]   input      Input string.
 * @param[out]  output     The output buffer with escaped special characters.
 * @param[in]   dst_size   Size of the output buffer.
 * @retval      The output pointer
 **********************************************************************************************************************/
static char * escape_special_characters(int len, char * output, char * input, size_t dst_size)
{
    size_t j = 0;
    int start = 0;
    int end = len;
    const char qt = '\"';

    if (len >= 2 && input[0] == qt && input[len - 1] == qt)
    {
        start = 1;
        end = len - 1;
    }

    for (int i = start; i < end && j + 1 < dst_size; i++)
    {
        char c = input[i];
        if ((c == '\\' || c == '"' || c == '/' || c == '\'') && j + 2 < dst_size)
        {
            output[j++] = '\\';
        }
        output[j++] = c;
    }
    output[j] = '\0';
    return output;
}

/*******************************************************************************************************************//**
 * Checks whether the WEP key is in hex format.
 *
 * @param[in]   cKey      Input WEP key.
 * @param[in]   key_len   Length of the WEP key.
 * @retval      true      The WEP key is in hex format
 * @retval      false     The WEP key is in ascii format
 **********************************************************************************************************************/
static bool is_wep_key_hex(const char * cKey, uint8_t key_len)
{
    uint8_t i;

    for (i = 0; i < key_len; i++)
    {
        if ((cKey[i] >= '0') && (cKey[i] <= '9'))
        {
            continue;
        }
        else if ((cKey[i] >= 'A') && (cKey[i] <= 'F'))
        {
            continue;
        }
        else if ((cKey[i] >= 'a') && (cKey[i] <= 'f'))
        {
            continue;
        }
        else
        {
            return false;
        }
    }

    return true;
}

/*******************************************************************************************************************//**
 * Compares the value of wpa_state from the structure wpa_supplicant with the input parameter and return success 
 * when it matches, failure otherwise.
 *
 * @param[in]  state                          Input (enum wpa_states)state which is to be compared.
 * @param[in]  max_delay_counter              Max delay counter. 1 is 100ms
 *
 * @retval FSP_SUCCESS                           Function completed successfully.
 * @retval FSP_ERR_WIFI_FAILED                   Error occurred with command to Wifi module.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_is_wpa_state(uint8_t state, int max_delay_counter)
{
    struct wpa_supplicant *wpa_s;
    int counter = 0;

    wpa_s = get_wpa_supplicant();
    if (!wpa_s)
        return FSP_ERR_WIFI_FAILED;

    while (wpa_supplicant_get_state(wpa_s) != (enum wpa_states) state)
    {
        if (counter++ >= max_delay_counter) // Timeout
            return FSP_ERR_WIFI_FAILED;

        vTaskDelay(DELAY_100MS);
    }

    return FSP_SUCCESS;
}

static fsp_err_t rm_wifi_parse_status_and_match_ap_conf(const WIFINetworkParams_t *pxNetworkParams, char *buff)
{
    char key[RM_WIFI_STATUS_PARAMS_MAX_LEN + 1];
    char value[RM_WIFI_AP_STATUS_VALUE_MAX_LEN + 1];
    char ssid[wificonfigMAX_SSID_LEN + 2];
    char *ptr;
    int i;
    uint8_t match_count = 0;

    ptr = strtok(buff, "\n");
    if (ptr == NULL)
        return FSP_ERR_WIFI_FAILED;

    while (ptr != NULL) {
        if (sscanf(ptr, "%" STRINGIFY(RM_WIFI_STATUS_PARAMS_MAX_LEN) "[^=]=%" STRINGIFY(RM_WIFI_AP_STATUS_VALUE_MAX_LEN) "s", key, value) == 2)
        {
            if (pxNetworkParams != NULL)
            {
                if (strcmp(key, "ssid") == 0)
                {
                    match_count++;
                    for (i = 0; i < pxNetworkParams->ucSSIDLength + 2; i++)
                        ssid[i] = (char) pxNetworkParams->ucSSID[i];

                    if ((pxNetworkParams->ucSSIDLength != strlen(value)) || (strncmp(&ssid[0], value, strlen(value)) != 0))
                        return FSP_ERR_WIFI_FAILED;
                }
                else if (strcmp(key, "key_mgmt") == 0)
                {
                    match_count++;
                    if (strcmp(value, "WPA-PSK") == 0)
                    {
                        if (pxNetworkParams->xSecurity != eWiFiSecurityWPA)
                            return FSP_ERR_WIFI_FAILED;
                    }
                    else if (strcmp(value, "WPA2-PSK") == 0)
                    {
                        if (pxNetworkParams->xSecurity != eWiFiSecurityWPA2)
                            return FSP_ERR_WIFI_FAILED;
                    }
                    else if (strcmp(value, "WEP") == 0)
                    {
                        if (pxNetworkParams->xSecurity != eWiFiSecurityWEP)
                            return FSP_ERR_WIFI_FAILED;
                    }
                    else if (strcmp(value, "NONE") == 0)
                    {
                        if (pxNetworkParams->xSecurity != eWiFiSecurityOpen)
                            return FSP_ERR_WIFI_FAILED;
                    }
                    else if (strcmp(value, "WPA2-ENT") == 0)
                    {
                        if (pxNetworkParams->xSecurity != eWiFiSecurityWPA2_ent)
                            return FSP_ERR_WIFI_FAILED;
                    }
                    else if (strcmp(value, "WPA3") == 0)
                    {
                        if (pxNetworkParams->xSecurity != eWiFiSecurityWPA3)
                            return FSP_ERR_WIFI_FAILED;
                    }
                    else
                        return FSP_ERR_WIFI_FAILED;

                }
                else if (strcmp(key, "channel") == 0)
                {
                    match_count++;
                    if (pxNetworkParams->ucChannel != atoi(value) && pxNetworkParams->ucChannel)
                        return FSP_ERR_WIFI_FAILED;
                }
                else if (strcmp(key, "wpa_state") == 0)
                {
                    match_count++;
                    if (strcmp(value, "COMPLETED") != 0)
                        return FSP_ERR_WIFI_AP_NOT_CONNECTED;
                }
            }
            else
            {
                if (strcmp(value, "COMPLETED") == 0)
                    return FSP_SUCCESS;
                else
                    return FSP_ERR_WIFI_FAILED;
            }
        }

        ptr = strtok(NULL, "\n");
    }
    if (match_count == 4)
        return FSP_SUCCESS;
    else
        return FSP_ERR_WIFI_FAILED;
}

static fsp_err_t rm_wifi_parse_connection_info(WIFIConnectionInfoExt_t *pxConnectionInfoExt, char *buff, int iface_num)
{
    fsp_err_t ret = FSP_SUCCESS;
    char key[RM_WIFI_STATUS_PARAMS_MAX_LEN + 1];
    char * p_value;
    char *line;
    unsigned int bssid2[wificonfigMAX_BSSID_LEN];
    unsigned char iface_index = 0;

    memset(pxConnectionInfoExt, 0, sizeof(*pxConnectionInfoExt) * iface_num);

    p_value = pvPortMalloc(RM_WIFI_STA_STATUS_VALUE_MAX_LEN + 1);
    if (p_value == NULL)
    {
        return FSP_ERR_WIFI_FAILED;
    }

    line = strtok(buff, "\n");
    while (line != NULL && iface_index < iface_num)
    {
        if (strchr(line, '=') == NULL)
        {
            while ((line = strtok(NULL, "\n")))
            {
                if (strchr(line, '=') == NULL)
                {
                    break;
                }
                if (sscanf(line, "%" STRINGIFY(RM_WIFI_STATUS_PARAMS_MAX_LEN) "[^=]=%"
                          STRINGIFY(RM_WIFI_STA_STATUS_VALUE_MAX_LEN) "[^\n]", key, p_value) == 2)
                {
                    if (strcmp(key, "ssid") == 0)
                    {
                        pxConnectionInfoExt[iface_index].connection_info.ucSSIDLength = (uint8_t) strlen(p_value);
                        strncpy((char*)pxConnectionInfoExt[iface_index].connection_info.ucSSID, p_value,
                                pxConnectionInfoExt[iface_index].connection_info.ucSSIDLength);
                    }
                    else if (strcmp(key, "bssid") == 0)
                    {
                        sscanf(p_value, "%x:%x:%x:%x:%x:%x", &bssid2[0], &bssid2[1], &bssid2[2],
                                                             &bssid2[3], &bssid2[4], &bssid2[5]);
                        for (int i = 0; i < wificonfigMAX_BSSID_LEN; i++)
                        {
                            pxConnectionInfoExt[iface_index].connection_info.ucBSSID[i] = (uint8_t) bssid2[i];
                        }
                    }
                    else if (strcmp(key, "key_mgmt") == 0)
                    {
                        if (strcmp(p_value, "NONE") == 0)
                        {
                            if (pxConnectionInfoExt[iface_index].connection_info.xSecurity != eWiFiSecurityWEP)
                            {
                                pxConnectionInfoExt[iface_index].connection_info.xSecurity = eWiFiSecurityOpen;
                            }
                        }
                        else if (strcmp(p_value, "WPA-PSK") == 0)
                        {
                            pxConnectionInfoExt[iface_index].connection_info.xSecurity = eWiFiSecurityWPA;
                        }
                        else if (strcmp(p_value, "WPA2-PSK") == 0)
                        {
                            pxConnectionInfoExt[iface_index].connection_info.xSecurity = eWiFiSecurityWPA2;
                        }
                        else if (strcmp(p_value, "WPA2/IEEE 802.1X/EAP") == 0)
                        {
                            pxConnectionInfoExt[iface_index].connection_info.xSecurity = eWiFiSecurityWPA2_ent;
                        }
                        else if (strcmp(p_value, "SAE") == 0)
                        {
                            pxConnectionInfoExt[iface_index].connection_info.xSecurity = eWiFiSecurityWPA3;
                        }
                        else if (strcmp(p_value, "WPA/IEEE 802.1X/EAP") == 0)
                        {
                            pxConnectionInfoExt[iface_index].connection_info.xSecurity = (WIFISecurity_t) eWiFiSecurityWPA_ent_ext;
                        }
                        else if (strcmp(p_value, "WPA2+WPA/IEEE 802.1X/EAP") == 0)
                        {
                            pxConnectionInfoExt[iface_index].connection_info.xSecurity = (WIFISecurity_t) eWiFiSecurityWPA_WPA2_ent_ext;
                        }
                        else if (strcmp(p_value, "WPA2-EAP-SUITE-B") == 0)
                        {
                            pxConnectionInfoExt[iface_index].connection_info.xSecurity = (WIFISecurity_t) eWiFiSecurityWPA3_ent_ext;
                        }
                        else if (strcmp(p_value, "WPA2-EAP-SUITE-B-192") == 0)
                        {
                            pxConnectionInfoExt[iface_index].connection_info.xSecurity = (WIFISecurity_t) eWiFiSecurityWPA3_192B_ent_ext;
                        }
                        else if (strcmp(p_value, "WPA2-PSK+WPA-PSK") == 0)
                        {
                            pxConnectionInfoExt[iface_index].connection_info.xSecurity = (WIFISecurity_t) eWiFiSecurityWPA_WPA2_ext;
                        }
                        else if (strcmp(p_value, "OWE") == 0)
                        {
                            pxConnectionInfoExt[iface_index].connection_info.xSecurity = (WIFISecurity_t) eWiFiSecurityWPA3_OWE_ext;
                        }
                        else if (strcmp(p_value, "WPA2-PSK+SAE") == 0)
                        {
                            pxConnectionInfoExt[iface_index].connection_info.xSecurity = (WIFISecurity_t) eWiFiSecurityWPA2_WPA3_ext;
                        }
                        else if (strcmp(p_value, "WPA2-EAP-SHA256") == 0)
                        {
                            pxConnectionInfoExt[iface_index].connection_info.xSecurity = (WIFISecurity_t) eWiFiSecurityWPA2_WPA3_ent_ext;
                        }
                        else
                        {
                            pxConnectionInfoExt[iface_index].connection_info.xSecurity = (WIFISecurity_t) eWiFiSecurityNotSupported;
                        }
                    }
                    else if (strcmp(key, "channel") == 0)
                    {
                        pxConnectionInfoExt[iface_index].connection_info.ucChannel = (uint8_t) atoi(p_value);
                    }
                    else if (strcmp(key, "wpa_state") == 0)
                    {
                        if (strcmp(p_value, "COMPLETED") != 0)
                        {
                            ret = FSP_ERR_WIFI_FAILED;
                            goto end;
                        }
                    }
                    else if (strcmp(key, "pairwise_cipher") == 0)
                    {
                        if (strncmp(p_value, "WEP", 3) == 0)
                        {
                            pxConnectionInfoExt[iface_index].connection_info.xSecurity = eWiFiSecurityWEP;
                        }
                    }
                    else if (strcmp(key, "wifi_generation") == 0)
                    {
                        pxConnectionInfoExt[iface_index].wifi_generation = (uint8_t) atoi(p_value);
                    }
                }
            }

            if (pxConnectionInfoExt[iface_index].connection_info.ucSSIDLength != 0)
            {
                iface_index++;
            }
        }
        else
        {
            line = strtok(NULL, "\n");
        }
    }

end:

    if (p_value)
    {
        vPortFree(p_value);
        p_value = NULL;
    }

    if (iface_index == 0)
    {
        return FSP_ERR_WIFI_FAILED;
    }

    return ret;
}

static bool rm_wifi_network_params_get(char * network_str, WIFIScanResult_t * network)
{
    char security_str[RM_WIFI_SECURITY_PARAMS_MAX_LEN + 1];
    char bssid_str[RM_WIFI_BSSID_PARAMS_MAX_LEN + 1];
    char ssid[wificonfigMAX_SSID_LEN + 1];

    WIFIScanResult_t param = {};

    int freq, rssi;
    int num_of_params = sscanf(network_str, "%" STRINGIFY(RM_WIFI_BSSID_PARAMS_MAX_LEN) "s %u %d %"
        STRINGIFY(RM_WIFI_SECURITY_PARAMS_MAX_LEN) "s %" STRINGIFY(wificonfigMAX_SSID_LEN) "[^\n]",
        bssid_str, &freq, &rssi, security_str, ssid);

    if (num_of_params < 4)
    {
        return false;
    }
    else if  (num_of_params == 4)   /* Hidden SSID */
    {
        param.ucSSID[0] = HIDDEN_SSID_DETECTION_CHAR;
        param.ucSSIDLength = 1;
    }
    else
    {
        /* Check ' ' character in front of SSID */
        char * p_ssid = strstr(network_str, security_str);
        if (p_ssid)
        {
            p_ssid += strlen(security_str);
            if (*p_ssid == '\t') p_ssid++;
        }

        if (strlen(p_ssid) > strlen(ssid))
        {
            param.ucSSIDLength = (uint8_t) strlen(p_ssid);
        }
        else
        {
            param.ucSSIDLength = (uint8_t) strlen(ssid);
            p_ssid = ssid;
        }

        if (param.ucSSIDLength > wificonfigMAX_SSID_LEN)
        {
            param.ucSSIDLength = wificonfigMAX_SSID_LEN;
        }

        memcpy(param.ucSSID, p_ssid, param.ucSSIDLength);
    }

    param.xSecurity = (WIFISecurity_t) rm_wifi_helper_security_str_to_type(security_str);
    param.ucChannel = (uint8_t) i3ed11_freq_to_ch(freq);
    param.cRSSI = (int8_t) rssi;

    unsigned bssid[wificonfigMAX_BSSID_LEN];
    num_of_params = sscanf(bssid_str, "%x:%x:%x:%x:%x:%x",
        &bssid[0], &bssid[1], &bssid[2],
        &bssid[3], &bssid[4], &bssid[5]);

    if (num_of_params != 6)
    {
        return false;
    }

    for (unsigned i = 0; i < wificonfigMAX_BSSID_LEN; i++)
    {
        param.ucBSSID[i] = (uint8_t) bssid[i];
    }

    *network = param;
    return true;
}

static void rm_wifi_parse_scan_results(char * networks_str, unsigned max_networks, WIFIScanResult_t * networks)
{
    const char * newline_delimiters = "\r\n";

    char * per_line_saveptr = NULL;
    char * token = strtok_r(networks_str, newline_delimiters, &per_line_saveptr);

    while ((token != NULL) && (max_networks))
    {
        WIFIScanResult_t network = {};

        if (rm_wifi_network_params_get(token, &network))
        {
            *networks++ = network;
            max_networks--;
        }

        token = strtok_r(NULL, newline_delimiters, &per_line_saveptr);
    }

    if (max_networks)
    {
        networks->ucSSIDLength = 0;   /* Mark end of list */
    }
}

static fsp_err_t rm_wifi_is_softap_state(char * softap_state)
{
    struct wpa_supplicant * wpa_s;
    uint8_t counter = 0;

    wpa_s = get_wpa_supplicant();
    if (!wpa_s)
    {
        return FSP_ERR_WIFI_FAILED;
    }

    while (strncmp(ap_get_state(wpa_s), softap_state, strlen(softap_state)))
    {
        vTaskDelay(DELAY_100MS);
        if (++counter >= COUNTER_MAX) // Timeout
               return FSP_ERR_WIFI_FAILED;
    }

    return FSP_SUCCESS;
}

static fsp_err_t rm_wifi_enterprise_connect_Ext(const WIFINetworkParamsExt_t * const pxNetworkParamsExt)
{
    fsp_err_t ret;
    char rep[REPLY_SIZE] = {0};
    char cli_cmd[50 + MAX(wificonfigMAX_ENT_IDENTITY_LEN, MAX(wificonfigMAX_PASSPHRASE_LEN, wificonfigMAX_SSID_LEN))];
    char str_id[wificonfigMAX_ENT_IDENTITY_LEN + 3];   // +3 for quotes and terminating '\0'
    char str_ssid[wificonfigMAX_SSID_LEN + 3];         // +3 for quotes and terminating '\0'
    char str_pass[wificonfigMAX_ENT_PASSWORD_LEN + 3]; // +3 for quotes and terminating '\0'

    /* Intentionally ignore the reply */
    ra6w1_cli_reply("remove_network 0", NULL, rep);

    CLI_REPLY_AND_CHECK("add_network 0");
    sprintf(cli_cmd, "set_network 0 ssid %s", quote(pxNetworkParamsExt->xNetworkParams.ucSSIDLength,
            str_ssid, (char *) pxNetworkParamsExt->xNetworkParams.ucSSID));
    CLI_REPLY_AND_CHECK(cli_cmd);

    /* Check if multiple channels are specified for scanning */
    if (pxNetworkParamsExt != NULL &&
        pxNetworkParamsExt->ucNumChannels > 0 &&
        pxNetworkParamsExt->pucChannelList != NULL)
    {
        char scan_freq_cmd[256];
        int freq;
        int offset = 0;

        /* Use local pointers for cleaner access */
        uint8_t  ucNumChannels  = pxNetworkParamsExt->ucNumChannels;
        uint32_t * pucChannelList = pxNetworkParamsExt->pucChannelList;

        /* Prepare the command: set_network 0 scan_freq <freq1> <freq2> ... */
        offset += snprintf(scan_freq_cmd + offset, sizeof(scan_freq_cmd) - offset, "set_network 0 freq_list");

        for (uint8_t idx = 0; idx < ucNumChannels; idx++)
        {
            /* Convert channel to frequency using ra6w1_regdb_ch_to_freq */
            freq = ra6w1_regdb_ch_to_freq((int) pucChannelList[idx]);
            if (freq > 0)
            {
                offset += snprintf(scan_freq_cmd + offset, sizeof(scan_freq_cmd) - offset, " %d", freq);
                if (offset >= (int) sizeof(scan_freq_cmd))
                {
                    break; /* Prevent overflow */
                }
            }
        }

        /* Send the command */
        CLI_REPLY_AND_CHECK(scan_freq_cmd);
    }

    switch ((WIFISecurityExt_t) pxNetworkParamsExt->xNetworkParams.xSecurity)
    {
    case eWiFiSecurityWPA_ent_ext:
    case eWiFiSecurityWPA2_ent_ext:
    case eWiFiSecurityWPA_WPA2_ent_ext:
        CLI_REPLY_AND_CHECK("set_network 0 proto RSN WPA");
        CLI_REPLY_AND_CHECK("set_network 0 key_mgmt WPA-EAP");
        CLI_REPLY_AND_CHECK("set_network 0 pairwise TKIP CCMP");
        CLI_REPLY_AND_CHECK("set_network 0 ieee80211w 0");
        break;
    case eWiFiSecurityWPA2_WPA3_ent_ext:
        CLI_REPLY_AND_CHECK("set_network 0 proto RSN");
        CLI_REPLY_AND_CHECK("set_network 0 key_mgmt WPA-EAP-SHA256 WPA-EAP");
        CLI_REPLY_AND_CHECK("set_network 0 pairwise CCMP");
        CLI_REPLY_AND_CHECK("set_network 0 ieee80211w 1");
        break;
    case eWiFiSecurityWPA3_ent_ext:
        CLI_REPLY_AND_CHECK("set_network 0 proto RSN");
        CLI_REPLY_AND_CHECK("set_network 0 key_mgmt WPA-EAP-SHA256");
        CLI_REPLY_AND_CHECK("set_network 0 pairwise CCMP");
        CLI_REPLY_AND_CHECK("set_network 0 ieee80211w 2");
        break;
    case eWiFiSecurityWPA3_192B_ent_ext:
        CLI_REPLY_AND_CHECK("set_network 0 proto RSN");
        CLI_REPLY_AND_CHECK("set_network 0 key_mgmt WPA-EAP-SUITE-B-192");
        CLI_REPLY_AND_CHECK("set_network 0 pairwise GCMP-256");
        CLI_REPLY_AND_CHECK("set_network 0 group GCMP-256");
        CLI_REPLY_AND_CHECK("set_network 0 group_mgmt BIP-GMAC-256");
        CLI_REPLY_AND_CHECK("set_network 0 ieee80211w 2");
        break;
    default:
        break;
    }

    switch (pxNetworkParamsExt->xEntNetParams.ucEntAuthType)
    {
    case ENT_AUTH_TYPE_PEAP_TTLS_FAST:
        CLI_REPLY_AND_CHECK("set_network 0 eap PEAP TTLS FAST");
        CLI_REPLY_AND_CHECK("set_network 0 phase1  'fast_provisioning=2'");
        goto phase2;
    case ENT_AUTH_TYPE_PEAP:
        CLI_REPLY_AND_CHECK("set_network 0 eap PEAP");
        goto phase2;
    case ENT_AUTH_TYPE_TTLS:
        CLI_REPLY_AND_CHECK("set_network 0 eap TTLS");
        goto phase2;
    case ENT_AUTH_TYPE_FAST:
        CLI_REPLY_AND_CHECK("set_network 0 eap FAST");
        CLI_REPLY_AND_CHECK("set_network 0 phase1  'fast_provisioning=2'");
        goto password;
    case ENT_AUTH_TYPE_TLS:
        CLI_REPLY_AND_CHECK("set_network 0 eap TLS");
        goto identity;
    default:
        return FSP_ERR_WIFI_CONFIG_FAILED;
    }

phase2:
    switch (pxNetworkParamsExt->xEntNetParams.ucEntAuthProto)
    {
    case ENT_AUTH_PROTO_MSCHAPv2_GTC:
        CLI_REPLY_AND_CHECK("set_network 0 phase2 'auth=MSCHAPV2 GTC'");
        break;
    case ENT_AUTH_PROTO_MSCHAPv2:
        CLI_REPLY_AND_CHECK("set_network 0 phase2 'auth=MSCHAPV2'");
        break;
    case ENT_AUTH_PROTO_GTC:
        CLI_REPLY_AND_CHECK("set_network 0 phase2 'auth=GTC'");
        break;
    case ENT_AUTH_PROTO_TLS:
        if (pxNetworkParamsExt->xEntNetParams.ucEntAuthType == ENT_AUTH_TYPE_TTLS)
        {
            CLI_REPLY_AND_CHECK("set_network 0 phase2 'autheap=TLS'");
        }
        else
        {
            CLI_REPLY_AND_CHECK("set_network 0 phase2 'auth=TLS'");
        }
        break;
    default:
        return FSP_ERR_WIFI_CONFIG_FAILED;
    }

password:
    sprintf(cli_cmd, "set_network 0 password %s", quote(pxNetworkParamsExt->xEntNetParams.ucPasswordLength,
            str_pass, pxNetworkParamsExt->xEntNetParams.ucPassword));
    CLI_REPLY_AND_CHECK(cli_cmd);

identity:
    sprintf(cli_cmd, "set_network 0 identity %s",
            quote(pxNetworkParamsExt->xEntNetParams.ucIDLength, str_id, pxNetworkParamsExt->xEntNetParams.ucID));
    CLI_REPLY_AND_CHECK(cli_cmd);
    
    if (pxNetworkParamsExt->hidden_ssid)
    {
        CLI_REPLY_AND_CHECK("set_network 0 scan_ssid 1");
    }
    
    ret = CLI_REPLY_AND_CHECK("select_network 0");
    if (ret)
        return FSP_ERR_WIFI_AP_NOT_CONNECTED;

    return (rm_wifi_is_wpa_state(WPA_COMPLETED, COUNTER_MAX));
}

static fsp_err_t rm_wifi_write_cert_privkey(const WIFINetworkParamsExt_t * pxNetworkParamsExt)
{
    int status;

    if ((RM_CERT_IsPemFormat(pxNetworkParamsExt->xEntNetParams.ucCert) == pdFALSE) ||
        (RM_CERT_IsPemFormat(pxNetworkParamsExt->xEntNetParams.ucPrivKey) == pdFALSE))
    {
        return FSP_ERR_WIFI_CONFIG_FAILED;
    }
    else
    {
        status = RM_CERT_Write(RM_CERT_GetModule(SF_TLS_CERT_WPA_ENT_CERTIFICATE_ADDR),
                               RM_CERT_GetType(SF_TLS_CERT_WPA_ENT_CERTIFICATE_ADDR),
                               RM_CERT_FORMAT_PEM,
                               (unsigned char *)pxNetworkParamsExt->xEntNetParams.ucCert,
                               pxNetworkParamsExt->xEntNetParams.ucCertLength);
        if (status)
        {
            return FSP_ERR_WIFI_CONFIG_FAILED;
        }

        status = RM_CERT_Write(RM_CERT_GetModule(SF_TLS_CERT_WPA_ENT_PRIVATE_KEY_ADDR),
                               RM_CERT_GetType(SF_TLS_CERT_WPA_ENT_PRIVATE_KEY_ADDR),
                               RM_CERT_FORMAT_PEM,
                               (unsigned char *)pxNetworkParamsExt->xEntNetParams.ucPrivKey,
                               pxNetworkParamsExt->xEntNetParams.ucPrivKeyLength);
        if (status)
        {
            return FSP_ERR_WIFI_CONFIG_FAILED;
        }
    }

    return FSP_SUCCESS;
}

static fsp_err_t rm_wifi_parse_get_station_list(WIFIStationInfo_t * pxStationList, uint8_t * pcStationListSize, char * buff)
{
    char * line = strtok(buff, "\n");
    int count = 0;
    uint8_t sta_list_sz_in = *pcStationListSize;
    int ret = 0;
    unsigned int tmp_mac[wificonfigMAX_BSSID_LEN];
    uint8_t i = 0;

    if (line == NULL) {
        return FSP_ERR_WIFI_FAILED;
    }

    while (line != NULL && count < sta_list_sz_in) {
        if (strcmp(line, "NOT_FOUND") == 0) /* No stations connected */
            break;

        ret = sscanf(line, "%x:%x:%x:%x:%x:%x",
                &(tmp_mac[0]),
                &(tmp_mac[1]),
                &(tmp_mac[2]),
                &(tmp_mac[3]),
                &(tmp_mac[4]),
                &(tmp_mac[5]));

        if (ret != 6) {
            printf("Invalid MAC address format\n");
        }
        
        for(i = 0; i < wificonfigMAX_BSSID_LEN; i++)
        {
            pxStationList[count].ucMAC[i] = (uint8_t)tmp_mac[i];
        }
        
        count++;

        // Get the next line
        line = strtok(NULL, "\n");
    }

    *pcStationListSize = count;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @addtogroup WIFI
 * @{
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 *  Checks the parameters of WEP security.
 *
 *  @param[in] pxNetworkParams       Network parameters.
 *
 *  @retval FSP_SUCCESS              WIFI successfully configured.
 *  @retval FSP_ERR_WIFI_FAILED      The check of WEP network parameters have failed.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_wep_network_params_check(const WIFINetworkParams_t * const pxNetworkParams)
{
    uint8_t i;
    bool wep_key_found = false;

    for (i = 0; i < wificonfigMAX_WEPKEYS; i++)
    {
        if (pxNetworkParams->xPassword.xWEP[i].ucLength == 0)
        {
            continue;
        }

        wep_key_found = true;
        if (pxNetworkParams->xPassword.xWEP[i].ucLength > wificonfigMAX_WEPKEY_LEN)
        {
            return FSP_ERR_WIFI_FAILED;
        }
        else if ((pxNetworkParams->xPassword.xWEP[i].ucLength == wificonfig64BIT_WEPKEY_LEN) ||
                 (pxNetworkParams->xPassword.xWEP[i].ucLength == wificonfig128BIT_WEPKEY_LEN))
        {
            if (!is_wep_key_hex(pxNetworkParams->xPassword.xWEP[i].cKey,
                 pxNetworkParams->xPassword.xWEP[i].ucLength))
            {
                return FSP_ERR_WIFI_FAILED;
            }
        }
        else if ((pxNetworkParams->xPassword.xWEP[i].ucLength != (wificonfig64BIT_WEPKEY_LEN / 2)) &&
                 (pxNetworkParams->xPassword.xWEP[i].ucLength != (wificonfig128BIT_WEPKEY_LEN / 2)))
        {
            return FSP_ERR_WIFI_FAILED;
        }
        else
        {
            continue;
        }
    }

    if (!wep_key_found)
    {
        return FSP_ERR_WIFI_FAILED;
    }

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 *  Checks the parameters of Enterprise WPA/WPA2/WAP3 security.
 *
 *  @param[in] pxNetworkParamsExt    Extended Network parameters.
 *
 *  @retval FSP_SUCCESS              WIFI successfully configured.
 *  @retval FSP_ERR_WIFI_FAILED      The check of Enterprise network parameters have failed.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_enterprise_network_params_check(const WIFINetworkParamsExt_t * const pxNetworkParamsExt)
{
    if ((NULL == pxNetworkParamsExt) || (0 == pxNetworkParamsExt->xEntNetParams.ucIDLength))
    {
        return FSP_ERR_WIFI_FAILED;
    }

    if (pxNetworkParamsExt->xEntNetParams.ucIDLength > wificonfigMAX_ENT_IDENTITY_LEN ||
        pxNetworkParamsExt->xEntNetParams.ucPasswordLength > wificonfigMAX_ENT_PASSWORD_LEN)
    {
        return FSP_ERR_WIFI_FAILED;
    }

    if (pxNetworkParamsExt->xEntNetParams.ucPasswordLength == 0)
    {
        if (((pxNetworkParamsExt->xEntNetParams.ucEntAuthProto == ENT_AUTH_PROTO_MSCHAPv2_GTC) &&
            (pxNetworkParamsExt->xEntNetParams.ucEntAuthType != ENT_AUTH_TYPE_TLS) &&
            (pxNetworkParamsExt->xEntNetParams.ucEntAuthType != ENT_AUTH_TYPE_FAST))||
            (pxNetworkParamsExt->xEntNetParams.ucEntAuthProto == ENT_AUTH_PROTO_MSCHAPv2) ||
            (pxNetworkParamsExt->xEntNetParams.ucEntAuthProto == ENT_AUTH_PROTO_GTC) ||
            (pxNetworkParamsExt->xEntNetParams.ucEntAuthType == ENT_AUTH_TYPE_FAST))
        {
            return FSP_ERR_WIFI_FAILED;
        }
    }

    if (((pxNetworkParamsExt->xEntNetParams.ucEntAuthType == ENT_AUTH_TYPE_TLS) ||
        (pxNetworkParamsExt->xEntNetParams.ucEntAuthProto == ENT_AUTH_PROTO_TLS)) &&
        ((pxNetworkParamsExt->xEntNetParams.ucCertLength == 0) ||
        (pxNetworkParamsExt->xEntNetParams.ucPrivKeyLength == 0) ||
        (pxNetworkParamsExt->xEntNetParams.ucCertLength > wificonfigMAX_CERT_LEN) ||
        (pxNetworkParamsExt->xEntNetParams.ucPrivKeyLength > wificonfigMAX_PRIV_KEY_LEN)))
    {
        return FSP_ERR_WIFI_FAILED;
    }

    if (pxNetworkParamsExt->xEntNetParams.ucEntAuthType >= ENT_AUTH_TYPE_UNSUPPORTED)
    {
        return FSP_ERR_WIFI_FAILED;
    }

    if (pxNetworkParamsExt->xEntNetParams.ucEntAuthProto >= ENT_AUTH_PROTO_UNSUPPORTED)
    {
        return FSP_ERR_WIFI_FAILED;
    }

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 *  Opens and configures the WIFI module.
 *
 *  @param[in]  p_cfg        Pointer to configuration structure.
 *
 *  @retval FSP_SUCCESS              WIFI successfully configured.
 *  @retval FSP_ERR_ASSERTION        NULL pointer to configuration or watchdog service instance.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_open(wifi_cfg_t const * const p_cfg)
{
#if WIFI_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_cfg);
    FSP_ASSERT(NULL != p_cfg->p_watchdog_service);
#endif

#if CFG_PMGR
    RM_PMGR_W_add_sleep_constraint(RM_PMGR_W_get_ctrl(), PMGR_CONSTRAINT_SLEEP_PROHIBITED);
    RM_PMGR_W_add_sleep_constraint(RM_PMGR_W_get_ctrl(), PMGR_CONSTRAINT_POWER_RETENTION);
    RM_PMGR_W_set_wake_source(RM_PMGR_W_get_ctrl(), PMGR_WAKE_SOURCE_WIFI);
#endif

    rm_wifi_event_handler_init();

    if (is_supplicant_done()
#if CFG_PMGR
        && !RM_PMGR_W_dpm_is_wakeup()
#endif
    ) {
        phy_rf_enable();
        return FSP_SUCCESS;
    }

    rm_wifi_init();

#if CFG_PMGR
    RM_PMGR_W_remove_sleep_constraint(RM_PMGR_W_get_ctrl(), PMGR_CONSTRAINT_SLEEP_PROHIBITED);
#endif

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 *  Opens and configures the WIFI module, and connects to previously configured AP using @ref rm_wifi_auto_config.
 *
 *  @param[in]  p_cfg        Pointer to configuration structure.
 *
 *  @retval FSP_SUCCESS                   Function completed successfully.
 *  @retval FSP_ERR_WIFI_FAILED           Error occurred with retrieving AP parameters from NVRAM or with connection.
 *  @retval FSP_ERR_WIFI_CONFIG_FAILED    Error occurred with save_config.
 *  @retval FSP_ERR_WIFI_AP_NOT_CONNECTED Error occurred with select_network
 **********************************************************************************************************************/
fsp_err_t rm_wifi_open_connect(wifi_cfg_t const * const p_cfg)
{
#ifdef RM_MAP_PERSISTANT_W
#define READ_NVRAM_STR(key, val) if (FSP_ERR_NOT_OPEN == RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, key, &val)) return FSP_ERR_WIFI_FAILED
#else
#define READ_NVRAM_STR(key, val) do { \
        (val) = read_nvram_string(ENV_GROUP_WIFICFG, (key)); \
        if (NULL == (val)) return FSP_ERR_WIFI_FAILED; \
    } while (0)
#endif

    e_wifi_device_mode_ext_t mode;
    const int id = 0;
    char field[30];
    char *profile, *ssid, *passphrase = NULL, *security;
    WIFISecurity_t security_val;
    WIFINetworkParams_t net_params = {0};

    fsp_err_t ret = rm_wifi_open(p_cfg);
    if (FSP_SUCCESS != ret)
    {
        return ret;
    }

    /* if mode is not AP, try to connect to previously configured AP */
    if (FSP_SUCCESS != rm_wifi_get_mode(&mode))
    {
        return FSP_ERR_WIFI_FAILED;
    }
    if ((WIFI_DEVICE_MODE_EXT_AP == mode)
#if CFG_PMGR
        || (RM_PMGR_W_dpm_is_wakeup())
#endif /* CFG_PMGR */
        )
    {
        /* mode is AP or DPM wakeup - don't try to connect */
        return FSP_SUCCESS;
    }

    /* load NVRAM configuration previously set with rm_wifi_auto_config */
    sprintf(field, PREFIX_NETWORK_PROFILE, id, "Profile");
    READ_NVRAM_STR(field, profile);
    if (!profile || strcmp(profile, "1"))
    {
        return FSP_ERR_WIFI_FAILED;
    }

    sprintf(field, PREFIX_NETWORK_PROFILE, id, "ssid");
    READ_NVRAM_STR(field, ssid);
    if (!ssid)
    {
        return FSP_ERR_WIFI_FAILED;
    }

    /* TODO: Support other security modes */
    sprintf(field, PREFIX_NETWORK_PROFILE, id, ENV_KEY_MGMT);
    READ_NVRAM_STR(field, security);
    if (!security)
    {
        security_val = eWiFiSecurityOpen;
    }
    else if (!strcmp("WPA-PSK", security))
    {
        security_val = eWiFiSecurityWPA2;
        sprintf(field, PREFIX_NETWORK_PROFILE, id, ENV_PSK);
        READ_NVRAM_STR(field, passphrase);
        if (!passphrase)
        {
            return FSP_ERR_WIFI_FAILED;
        }
    }
    else
    {
        /* TODO: Support other security modes */
        return FSP_ERR_WIFI_FAILED;
    }

    net_params.ucSSIDLength = strlen(ssid);
    net_params.xPassword.xWPA.ucLength = strlen(passphrase);

    unquote(&net_params.ucSSIDLength, (char *) net_params.ucSSID, ssid); // unquote the ssid
    unquote(&net_params.xPassword.xWPA.ucLength,
            net_params.xPassword.xWPA.cPassphrase, passphrase); // unquote the passphrase

    net_params.xSecurity = security_val;

    return rm_wifi_connect(&net_params, false, PMF_DEFAULT, NULL, 0, NULL);
}

/*******************************************************************************************************************//**
 *  Close WIFI module.
 *
 *  @retval FSP_SUCCESS              WIFI closed successfully.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_close()
{
    CLI_REPLY_AND_CHECK("flush");

    rm_wifi_event_handler_deinit();

    phy_rf_disable();

#if CFG_PMGR
    RM_PMGR_W_remove_sleep_constraint(RM_PMGR_W_get_ctrl(), PMGR_CONSTRAINT_POWER_RETENTION);
    RM_PMGR_W_clr_wake_source(RM_PMGR_W_get_ctrl(), PMGR_WAKE_SOURCE_WIFI);
#endif

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 *  Connects to the specified Wifi Access Point.
 *
 * @param[in]  pxNetworkParams           Network parameters validated by WIFI_NetworkParams_check().
 * @param[in]  hidden_ssid               Connect to AP with hidden SSID.
 * @param[in]  pmf                       Specify Protected Management Frames mode (only valid in some WIFISecurity modes)
 * @param[in]  sae_groups                Specify the SAE groups IDs (valid on WPA3 SAE security mode only)
 * @param[in]  ucNumChannels             Number of channels to scan (0 = scan all)
 * @param[in]  pucChannelList            Pointer to list of channels to scan (NULL = scan all)
 *
 * @retval FSP_SUCCESS                   Function completed successfully.
 * @retval FSP_ERR_WIFI_CONFIG_FAILED    Error occurred with save_config.
 * @retval FSP_ERR_WIFI_AP_NOT_CONNECTED Error occurred with select_network
 **********************************************************************************************************************/
fsp_err_t rm_wifi_connect(const WIFINetworkParams_t * const pxNetworkParams,
                          bool hidden_ssid, WIFIPmf_t pmf, const char * sae_groups,
                          uint8_t ucNumChannels, uint32_t * pucChannelList)
{
    int ret;
    char rep[REPLY_SIZE] = {0};
    uint8_t i = 0;
    char cli_cmd[50 + MAX(wificonfigMAX_PASSPHRASE_LEN, wificonfigMAX_SSID_LEN)];
    char str_ssid[wificonfigMAX_SSID_LEN + 3];      // +3 for quotes and terminating '\0'
    char str_psk[wificonfigMAX_PASSPHRASE_LEN + 3]; // +3 for quotes and terminating '\0'
    char esc_ssid[wificonfigMAX_SSID_LEN * 2 + 3];

    /* Intentionally ignore the reply */
    ra6w1_cli_reply("remove_network 0", NULL, rep);

    CLI_REPLY_AND_CHECK("add_network 0");

    /* Check if multiple channels are specified for scanning */
    if (ucNumChannels > 0 && pucChannelList != NULL)
    {
        char scan_freq_cmd[256];
        int freq;
        int offset = 0;

        /* Prepare the command: set_network 0 scan_freq <freq1> <freq2> ... */
        offset += snprintf(scan_freq_cmd + offset, sizeof(scan_freq_cmd) - offset, "set_network 0 freq_list");

        for (uint8_t idx = 0; idx < ucNumChannels; idx++)
        {
             /* Convert channel to frequency using ra6w1_regdb_ch_to_freq */
             freq = ra6w1_regdb_ch_to_freq((int) pucChannelList[idx]);
             if (freq > 0)
             {
                 offset += snprintf(scan_freq_cmd + offset, sizeof(scan_freq_cmd) - offset, " %d", freq);
                 if (offset >= (int) sizeof(scan_freq_cmd))
                 {
                    break; /* Prevent overflow */
                 }
             }
        }

        /* Send the command */
        CLI_REPLY_AND_CHECK(scan_freq_cmd);
    }

    escape_special_characters(pxNetworkParams->ucSSIDLength, esc_ssid,
                              (char *) pxNetworkParams->ucSSID, sizeof(esc_ssid));
    sprintf(cli_cmd, "set_network 0 ssid %s",
            quote(strlen(esc_ssid), str_ssid, esc_ssid));
    CLI_REPLY_AND_CHECK(cli_cmd);

    switch ((WIFISecurityExt_t) pxNetworkParams->xSecurity)
    {
    case eWiFiSecurityWEP:
        CLI_REPLY_AND_CHECK("set_network 0 key_mgmt NONE");
        for (i = 0; i < wificonfigMAX_WEPKEYS; i++)
        {
            if (pxNetworkParams->xPassword.xWEP[i].ucLength == 0)
            {
                continue;
            }

            if ((pxNetworkParams->xPassword.xWEP[i].ucLength == wificonfig64BIT_WEPKEY_LEN) ||
                (pxNetworkParams->xPassword.xWEP[i].ucLength == wificonfig128BIT_WEPKEY_LEN))
            {
                snprintf(cli_cmd, sizeof(cli_cmd), "set_network 0 wep_key%d %.*s", i,
                        pxNetworkParams->xPassword.xWEP[i].ucLength, pxNetworkParams->xPassword.xWEP[i].cKey);
            }
            else if ((pxNetworkParams->xPassword.xWEP[i].ucLength == (wificonfig64BIT_WEPKEY_LEN / 2)) ||
                        (pxNetworkParams->xPassword.xWEP[i].ucLength == (wificonfig128BIT_WEPKEY_LEN / 2)))
            {
                sprintf(cli_cmd, "set_network 0 wep_key%d %s", i,
                        quote(pxNetworkParams->xPassword.xWEP[i].ucLength, str_psk,
                        pxNetworkParams->xPassword.xWEP[i].cKey));
            }

            CLI_REPLY_AND_CHECK(cli_cmd);
        }
        sprintf(cli_cmd, "set_network 0 wep_tx_keyidx %d", pxNetworkParams->ucDefaultWEPKeyIndex);
        CLI_REPLY_AND_CHECK(cli_cmd);
        break;

    case eWiFiSecurityWPA_ext:
        CLI_REPLY_AND_CHECK("set_network 0 proto WPA");
        CLI_REPLY_AND_CHECK("set_network 0 pairwise TKIP");
        CLI_REPLY_AND_CHECK("set_network 0 key_mgmt WPA-PSK");
        sprintf(cli_cmd, "set_network 0 psk %s", quote(pxNetworkParams->xPassword.xWPA.ucLength,
                str_psk, pxNetworkParams->xPassword.xWPA.cPassphrase));
        CLI_REPLY_AND_CHECK(cli_cmd);
        break;

    case eWiFiSecurityWPA2_ext:
        CLI_REPLY_AND_CHECK("set_network 0 proto RSN");
        CLI_REPLY_AND_CHECK("set_network 0 pairwise CCMP");
        sprintf(cli_cmd, "set_network 0 key_mgmt WPA-PSK%s", (pmf == PMF_REQUIRED || pmf == PMF_CAPABLE) ? " WPA-PSK-SHA256" : "");
        CLI_REPLY_AND_CHECK(cli_cmd);
        memset(cli_cmd, 0x0, strlen(cli_cmd) + 1);
        sprintf(cli_cmd, "set_network 0 ieee80211w %d", pmf == PMF_DEFAULT ? PMF_NONE : pmf);
        CLI_REPLY_AND_CHECK(cli_cmd);
        sprintf(cli_cmd, "set_network 0 psk %s", quote(pxNetworkParams->xPassword.xWPA.ucLength,
                str_psk, pxNetworkParams->xPassword.xWPA.cPassphrase));
        CLI_REPLY_AND_CHECK(cli_cmd);
        break;

    case eWiFiSecurityWPA3_ext:
        CLI_REPLY_AND_CHECK("set_network 0 proto RSN");
        CLI_REPLY_AND_CHECK("set_network 0 key_mgmt SAE");
        CLI_REPLY_AND_CHECK("set_network 0 pairwise CCMP");
        CLI_REPLY_AND_CHECK("set_network 0 ieee80211w 2");
        sprintf(cli_cmd, "set_network 0 sae_password %s", quote(pxNetworkParams->xPassword.xWPA.ucLength,
                str_psk, pxNetworkParams->xPassword.xWPA.cPassphrase));
        CLI_REPLY_AND_CHECK(cli_cmd);

        /* If SAE groups configured by the user */
        if (sae_groups != NULL && strlen(sae_groups) != 0)
        {
            sprintf(cli_cmd, "sae_groups %s", sae_groups);
            CLI_REPLY_AND_CHECK(cli_cmd);
        }
        break;

    case eWiFiSecurityWPA_WPA2_ext:
        CLI_REPLY_AND_CHECK("set_network 0 proto WPA RSN");
        CLI_REPLY_AND_CHECK("set_network 0 key_mgmt WPA-PSK");
        CLI_REPLY_AND_CHECK("set_network 0 pairwise CCMP TKIP");
        sprintf(cli_cmd, "set_network 0 ieee80211w %d", pmf == PMF_DEFAULT ? PMF_NONE : pmf);
        CLI_REPLY_AND_CHECK(cli_cmd);
        sprintf(cli_cmd, "set_network 0 psk %s", quote(pxNetworkParams->xPassword.xWPA.ucLength,
                str_psk, pxNetworkParams->xPassword.xWPA.cPassphrase));
        CLI_REPLY_AND_CHECK(cli_cmd);
        break;

    case eWiFiSecurityWPA2_WPA3_ext:
        CLI_REPLY_AND_CHECK("set_network 0 proto RSN");
        CLI_REPLY_AND_CHECK("set_network 0 key_mgmt WPA-PSK SAE");
        CLI_REPLY_AND_CHECK("set_network 0 pairwise CCMP GCMP");
        sprintf(cli_cmd, "set_network 0 ieee80211w %d", pmf == PMF_DEFAULT ? PMF_CAPABLE : pmf);
        CLI_REPLY_AND_CHECK(cli_cmd);
        sprintf(cli_cmd, "set_network 0 psk %s", quote(pxNetworkParams->xPassword.xWPA.ucLength,
                str_psk, pxNetworkParams->xPassword.xWPA.cPassphrase));
        CLI_REPLY_AND_CHECK(cli_cmd);

        /* If SAE groups configured by the user */
        if (sae_groups != NULL && strlen(sae_groups) != 0)
        {
            sprintf(cli_cmd, "sae_groups %s", sae_groups);
            CLI_REPLY_AND_CHECK(cli_cmd);
        }
        break;

    case eWiFiSecurityOpen:
        CLI_REPLY_AND_CHECK("set_network 0 key_mgmt NONE");
        break;

    case eWiFiSecurityWPA3_OWE_ext:
        CLI_REPLY_AND_CHECK("set_network 0 proto RSN");
        CLI_REPLY_AND_CHECK("set_network 0 key_mgmt OWE");
        CLI_REPLY_AND_CHECK("set_network 0 pairwise CCMP");
        CLI_REPLY_AND_CHECK("set_network 0 ieee80211w 2");
        break;
    default:
        return FSP_ERR_WIFI_CONFIG_FAILED;
    }

    if (hidden_ssid)
    {
        CLI_REPLY_AND_CHECK("set_network 0 scan_ssid 1");
    }

    ret = CLI_REPLY_AND_CHECK("select_network 0");
    if (ret)
        return FSP_ERR_WIFI_AP_NOT_CONNECTED;

    return (rm_wifi_is_wpa_state(WPA_COMPLETED, COUNTER_MAX));
}


/*******************************************************************************************************************//**
 *  Connects to the specified Wifi Access Point.
 *
 * @param[in]  pxNetworkParamsExt        Extended network parameters.
 *
 * @retval FSP_SUCCESS                   Function completed successfully.
 * @retval FSP_ERR_WIFI_CONFIG_FAILED    Error occurred with save_config.
 * @retval FSP_ERR_WIFI_AP_NOT_CONNECTED Error occurred with select_network
 **********************************************************************************************************************/
fsp_err_t rm_wifi_connect_Ext(const WIFINetworkParamsExt_t * const pxNetworkParamsExt)
{
    fsp_err_t ret;
    WIFIBand_t band = pxNetworkParamsExt->ucBand;
    char band_cmd[32] = {0};
    WIFISecurityExt_t xSecurity = (WIFISecurityExt_t) pxNetworkParamsExt->xNetworkParams.xSecurity;

    if (band == eWiFiBand2G)
        sprintf(band_cmd, "set setband %s", "2G");
    else if (band == eWiFiBand5G)
        sprintf(band_cmd, "set setband %s", "5G");
    else
        sprintf(band_cmd, "set setband AUTO");

    CLI_REPLY_AND_CHECK(band_cmd);

    if ((xSecurity == eWiFiSecurityWPA_ent_ext) ||
        (xSecurity == eWiFiSecurityWPA2_ent_ext) ||
        (xSecurity == eWiFiSecurityWPA_WPA2_ent_ext) ||
        (xSecurity == eWiFiSecurityWPA2_WPA3_ent_ext) ||
        (xSecurity == eWiFiSecurityWPA3_ent_ext) ||
        (xSecurity == eWiFiSecurityWPA3_192B_ent_ext))
    {
        if (pxNetworkParamsExt->xEntNetParams.ucEntAuthType == ENT_AUTH_TYPE_TLS ||
            pxNetworkParamsExt->xEntNetParams.ucEntAuthProto == ENT_AUTH_PROTO_TLS)
        {
            ret = rm_wifi_write_cert_privkey(pxNetworkParamsExt);
            if (FSP_SUCCESS != ret)
            {
                return ret;
            }
        }

        ret = rm_wifi_enterprise_connect_Ext(pxNetworkParamsExt);
        return ret;
    }
    ret = rm_wifi_connect(&pxNetworkParamsExt->xNetworkParams, pxNetworkParamsExt->hidden_ssid,
                          pxNetworkParamsExt->pmf, pxNetworkParamsExt->sae_groups,
                          pxNetworkParamsExt->ucNumChannels, pxNetworkParamsExt->pucChannelList);
    return ret;
}

/*******************************************************************************************************************//**
 *  Sets the Wi-Fi Access Point (AP) parameters to be used when connecting using @ref rm_wifi_open_connect.
 *
 * @param[in]  pxNetworkParams          Network parameters.
 *
 * @retval FSP_SUCCESS                  Function completed successfully.
 * @retval FSP_ERR_WIFI_FAILED          Error occurred.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_auto_config(const WIFINetworkParams_t * const pxNetworkParams)
{
#ifdef RM_MAP_PERSISTANT_W
#define WRITE_NVRAM_STR(key, val) RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, key, val)
#else
#define WRITE_NVRAM_STR(key, val) write_nvram_string(ENV_GROUP_WIFICFG, (key), (val))
#endif

    int err = 0;
    const int id = 0;
    char field[30];
    char str_ssid[wificonfigMAX_SSID_LEN + 3];      // +3 for quotes and terminating '\0'
    char str_psk[wificonfigMAX_PASSPHRASE_LEN + 3]; // +3 for quotes and terminating '\0'

    sprintf(field, PREFIX_NETWORK_PROFILE, id, "Profile");
    err |= WRITE_NVRAM_STR(field, "1");

    sprintf(field, PREFIX_NETWORK_PROFILE, id, "ssid");
    err |= WRITE_NVRAM_STR(field, quote(pxNetworkParams->ucSSIDLength, str_ssid, (char *) pxNetworkParams->ucSSID));

    /* TODO: Support other security modes */
    switch(pxNetworkParams->xSecurity)
    {
    case eWiFiSecurityWPA2:
        sprintf(field, PREFIX_NETWORK_PROFILE, id, ENV_PSK);
        err |= WRITE_NVRAM_STR(field, quote(pxNetworkParams->xPassword.xWPA.ucLength,
                               str_psk, pxNetworkParams->xPassword.xWPA.cPassphrase));

        sprintf(field, PREFIX_NETWORK_PROFILE, id, ENV_KEY_MGMT);
        err |= WRITE_NVRAM_STR(field, "WPA-PSK");
        break;

    case eWiFiSecurityOpen:
        break;
    default:
        return FSP_ERR_WIFI_FAILED;
    }
    return err ? FSP_ERR_WIFI_FAILED : FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 *  Disconnects from connected AP.
 *
 *  @retval FSP_SUCCESS              WIFI disconnected successfully.
 *  @retval FSP_ERR_WIFI_FAILED      Error occurred with command to Wifi module.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_disconnect()
{
    int ret = CLI_REPLY_AND_CHECK("disconnect");

    if (ret)
        return FSP_ERR_WIFI_FAILED;

    return (rm_wifi_is_wpa_state(WPA_DISCONNECTED, COUNTER_MAX));
}

e_wifi_device_mode_ext_t WIFI_Convert_Mode_To_Ext_Mode(WIFIDeviceMode_t xDeviceMode)
{
    switch (xDeviceMode) {
        case eWiFiModeStation:
            return WIFI_DEVICE_MODE_EXT_STATION;
        case eWiFiModeAP:
            return WIFI_DEVICE_MODE_EXT_AP;
        case eWiFiModeP2P:
            return WIFI_DEVICE_MODE_EXT_P2P;
        case eWiFiModeAPStation:
            return WIFI_DEVICE_MODE_EXT_AP_STATION;
        default:
            return WIFI_DEVICE_MODE_EXT_NOT_SUPPORTED;
    }
}

WIFIDeviceMode_t WIFI_Convert_Ext_Mode_To_Mode(e_wifi_device_mode_ext_t xDeviceModeExt)
{
    switch (xDeviceModeExt) {
        case WIFI_DEVICE_MODE_EXT_STATION:
            return eWiFiModeStation;
        case WIFI_DEVICE_MODE_EXT_AP:
            return eWiFiModeAP;
        case WIFI_DEVICE_MODE_EXT_P2P:
            return eWiFiModeP2P;
        case WIFI_DEVICE_MODE_EXT_AP_STATION:
            return eWiFiModeAPStation;
        default:
            return eWiFiModeNotSupported;
    }
}

/*******************************************************************************************************************//**
 *  Set WIFI Mode
 *
 *  @retval FSP_SUCCESS              Set Mode successfully.
 *  @retval FSP_ERR_WIFI_FAILED      Error occurred with command to Wifi module.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_set_mode(e_wifi_device_mode_ext_t xDeviceMode)
{
    int ret;
    int current_sysmode = get_sys_mode();

    if (current_sysmode == xDeviceMode)
        return FSP_SUCCESS;

    ret = request_stop_supplicant();
    if (ret)
        return FSP_ERR_WIFI_FAILED;

    ret = set_sys_mode(xDeviceMode);
    if (ret)
        return FSP_ERR_WIFI_FAILED;

    set_run_mode(xDeviceMode);
    
    if (!wait_supplicant_deinit(500))
    {
        printf(">>> Supplicant deinit failed\n");
    }

    ret = request_start_supplicant();

    if (ret)
        return FSP_ERR_WIFI_FAILED;

    /* Wait for supplicant initialization (up to 5 sec) */
    if (!wait_supplicant_done(500))
    {
        printf(">>> Supplicant start failed\n");
    }

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 *  Get WIFI Mode
 *
 *  @retval FSP_SUCCESS              Get Mode successfully.
 *  @retval FSP_ERR_WIFI_FAILED      Error occurred with command to Wifi module.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_get_mode(e_wifi_device_mode_ext_t  *xDeviceModeExt)
{
    int ret;

    ret = get_sys_mode();
    if ((ret < WIFI_DEVICE_MODE_EXT_STATION) || (ret > WIFI_DEVICE_MODE_EXT_P2P_STATION))
        return FSP_ERR_WIFI_FAILED;

    *xDeviceModeExt = ret;
    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 *  WIFI P2P
 *
 *  @retval FSP_SUCCESS              Set P2P successfully.
 *  @retval FSP_ERR_WIFI_FAILED      Error occurred with command to Wifi module.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_p2p(unsigned char oper_chan, unsigned char listen_chan, unsigned char go_intent, const char * ssid_postfix)
{
    char oper_chan_cmd[64] = {0};
    char listen_chan_cmd[64] = {0};
    char go_intent_cmd[64] = {0};
    char ssid_postfix_cmd[64] = {0};
    char rep[REPLY_SIZE_256] = {0};
    
    e_wifi_device_mode_ext_t p2p_mode;

    if (FSP_SUCCESS != rm_wifi_get_mode(&p2p_mode))
    {
        printf("\n%d \n",p2p_mode);
        return FSP_ERR_WIFI_FAILED;
    }

    cli_reply_generic("config_default", __func__, __LINE__, rep); // for WPS

    if (p2p_mode != WIFI_DEVICE_MODE_EXT_P2P_STATION)
    {
        sprintf(ssid_postfix_cmd, "p2p_set ssid_postfix -%s", ssid_postfix);
        CLI_REPLY_AND_CHECK(ssid_postfix_cmd);

        sprintf(oper_chan_cmd, "p2p_set oper_channel %d", oper_chan);
        CLI_REPLY_AND_CHECK(oper_chan_cmd);
    }

    if (p2p_mode != WIFI_DEVICE_MODE_EXT_P2P_GO)
    {
        sprintf(listen_chan_cmd, "p2p_set listen_channel %d", listen_chan);
        CLI_REPLY_AND_CHECK(listen_chan_cmd);
    }

    if (p2p_mode == WIFI_DEVICE_MODE_EXT_P2P)
    {
        sprintf(go_intent_cmd, "p2p_set go_intent %d", go_intent);
        CLI_REPLY_AND_CHECK(go_intent_cmd);
    }

    return FSP_SUCCESS;
}
/*******************************************************************************************************************//**
 *  WIFI P2P find
 *
 *  @retval FSP_SUCCESS              Set P2P successfully.
 *  @retval FSP_ERR_WIFI_FAILED      Error occurred with command to Wifi module.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_p2p_find(void)
{
    int ret;

    ret = CLI_REPLY_AND_CHECK("p2p_find");
    
    if (ret)
        return FSP_ERR_WIFI_FAILED;
    
    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 *  WIFI P2P connect
 *
 *  @retval FSP_SUCCESS              Connected to P2P device successfully.
 *  @retval FSP_ERR_WIFI_FAILED      Error occurred with command to Wifi module.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_p2p_connect(const char *p2p_addr, const char *wps_method)
{
    char cmd[64] = {0};
    int ret;

    if(strncmp("pbc",wps_method,3) == 0)
    {

        sprintf(cmd, "p2p_connect %s pbc", p2p_addr);
    }
    else 
    {
        sprintf(cmd, "p2p_connect %s %s", p2p_addr, wps_method);
    }

   ret = CLI_REPLY_AND_CHECK(cmd);
    
    if (ret)
        return FSP_ERR_WIFI_FAILED;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 *  WIFI P2P group remove
 *
 *  @retval FSP_SUCCESS              Removed P2P group successfully.
 *  @retval FSP_ERR_WIFI_FAILED      Error occurred with command to Wifi module.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_p2p_group_remove(void)
{
    int ret;

    ret = CLI_REPLY_AND_CHECK("p2p_group_remove");
    
    if (ret)
        return FSP_ERR_WIFI_FAILED;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 *  WIFI P2P group add
 *
 *  @retval FSP_SUCCESS              Added P2P group successfully.
 *  @retval FSP_ERR_WIFI_FAILED      Error occurred with command to Wifi module.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_p2p_group_add(void)
{
    int ret;
    
    ret = CLI_REPLY_AND_CHECK("p2p_group_add");

    if (ret)
        return FSP_ERR_WIFI_FAILED;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 *  WIFI P2P peers
 *
 *  @retval FSP_SUCCESS              Listed known P2P peers successfully.
 *  @retval FSP_ERR_WIFI_FAILED      Error occurred with command to Wifi module.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_p2p_peers(char * reply)
{
    int ret;

    ret =  ra6w1_cli_reply("p2p_peers", NULL, reply);

    if (ret)
        return FSP_ERR_WIFI_FAILED;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 *  WIFI P2P accept
 *
 *  @retval FSP_SUCCESS              Accepted P2P connection successfully.
 *  @retval FSP_ERR_WIFI_FAILED      Error occurred with command to Wifi module.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_p2p_accept(void)
{
    int ret;

    ret = CLI_REPLY_AND_CHECK("p2p_accept");

    if (ret)
        return FSP_ERR_WIFI_FAILED;
        
    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 *  WIFI P2P Set Operating Channel
 *
 *  @retval FSP_SUCCESS              Set operating channel successfully.
 *  @retval FSP_ERR_WIFI_FAILED      Error occurred with command to Wifi module.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_p2p_set_oper_chan(unsigned char channel)
{
    char cmd[64] = {0};
    int ret;

    sprintf(cmd, "p2p_set oper_channel %d", channel);
    ret = CLI_REPLY_AND_CHECK(cmd);
    
    if (ret)
        return FSP_ERR_WIFI_FAILED;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 *  WIFI P2P Set Listen Channel
 *
 *  @retval FSP_SUCCESS              Set listen channel successfully.
 *  @retval FSP_ERR_WIFI_FAILED      Error occurred with command to Wifi module.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_p2p_set_listen_chan(unsigned char channel)
{
    char cmd[64] = {0};
    int ret;

    sprintf(cmd, "p2p_set listen_channel %d", channel);
    ret = CLI_REPLY_AND_CHECK(cmd);
    
    if (ret)
        return FSP_ERR_WIFI_FAILED;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 *  WIFI P2P Set GO Intent
 *
 *  @retval FSP_SUCCESS              Set GO Intent successfully.
 *  @retval FSP_ERR_WIFI_FAILED      Error occurred with command to Wifi module.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_p2p_set_go_intent(unsigned char intent)
{
    char cmd[64] = {0};
    int ret;

    sprintf(cmd, "p2p_set go_intent %d", intent);
    ret = CLI_REPLY_AND_CHECK(cmd);

    if (ret)
        return FSP_ERR_WIFI_FAILED;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 *  WIFI P2P Set Find Timeout
 *
 *  @retval FSP_SUCCESS              Set Find Timeout successfully.
 *  @retval FSP_ERR_WIFI_FAILED      Error occurred with command to Wifi module.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_p2p_set_find_timeout(unsigned char intent)
{
    char cmd[64] = {0};
    int ret;

    sprintf(cmd, "p2p_set find_timeout %d", intent);
    ret = CLI_REPLY_AND_CHECK(cmd);

    if (ret)
        return FSP_ERR_WIFI_FAILED;

    return FSP_SUCCESS;
}
/*******************************************************************************************************************//**
 *  WIFI P2P Set SSID Postfix
 *
 *  @retval FSP_SUCCESS              Set SSID Postfix successfully.
 *  @retval FSP_ERR_WIFI_FAILED      Error occurred with command to Wifi module.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_p2p_set_ssid_postfix(const char * ssid_postfix)
{
    char cmd[64] = {0};
    int ret;

    sprintf(cmd, "p2p_set ssid_postfix %s", ssid_postfix);
    ret = CLI_REPLY_AND_CHECK(cmd);

    if (ret)
        return FSP_ERR_WIFI_FAILED;

    return FSP_SUCCESS;
}
/*******************************************************************************************************************//**
 *  WIFI P2P Get Parameter
 *
 *  @retval FSP_SUCCESS              Retrieved P2P parameter successfully.
 *  @retval FSP_ERR_WIFI_FAILED      Error occurred with command to Wifi module.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_p2p_get(char *p2pconfig)
{
    if (p2pconfig)
    {
        ra6w1_cli_reply("p2p_get", NULL, p2pconfig);
        if (!p2pconfig || strncmp(p2pconfig, "FAIL", 4) == 0) 
        {
            return FSP_ERR_WIFI_FAILED;
        }                                                                                                   
    }

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 *  Isolate STAs connected to the AP from each other (or cancel isolation).
 *
 *  @retval FSP_SUCCESS              Isolate/connect the STAs successfully.
 *  @retval FSP_ERR_WIFI_FAILED      Error occurred with command to Wifi module.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_isolate(bool enable_isolation)
{
    char cmd[64] = {0};
    int ret;

    sprintf(cmd, "set_network 1 isolate %d", enable_isolation);
    ret = CLI_REPLY_AND_CHECK(cmd);

    if (ret)
    {
        return FSP_ERR_WIFI_FAILED;
    }

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Register the MAC Address of a Station in the ACL
 *
 *  @retval FSP_SUCCESS              mac_addr added successfully.
 *  @retval FSP_ERR_WIFI_FAILED      Error occurred with adding to ACL.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_acl_mac(const char *mac_addr)
{
    char cmd[64] = {0};
    int ret;

    sprintf(cmd, "acl_mac %s", mac_addr);
    ret = CLI_REPLY_AND_CHECK(cmd);

    if (ret)
        return FSP_ERR_WIFI_FAILED;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 *  Set ACL to allow/deny/clear"
 *
 *  @retval FSP_SUCCESS              Set the MAC Filtering to [filter_mode] successfully.
 *  @retval FSP_ERR_WIFI_FAILED      Error occurred with setting MAC Filtering to the mode.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_acl(const char *filter_mode)
{
    char cmd[64] = {0};
    int ret;

    sprintf(cmd, "acl %s", filter_mode);
    ret = CLI_REPLY_AND_CHECK(cmd);

    if (ret)
        return FSP_ERR_WIFI_FAILED;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Set WMM function to enable/disable
 *
 *  @retval FSP_SUCCESS              WMM function set successfully.
 *  @retval FSP_ERR_WIFI_FAILED      Error occurred with setting WMM.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_wmm(bool enable_wmm)
{
    char cmd[64] = {0};
    int ret;

    sprintf(cmd, "wmm_enabled %d", enable_wmm);
    ret = CLI_REPLY_AND_CHECK(cmd);

    if (ret)
    {
        return FSP_ERR_WIFI_FAILED;
    }

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Set WMM-PS function to enable/disable
 *
 *  @retval FSP_SUCCESS              WMM-PS function set successfully.
 *  @retval FSP_ERR_WIFI_FAILED      Error occurred with setting WMM-PS.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_wmm_ps(bool enable_wmm_ps)
{
    char cmd[64] = {0};
    int ret;

    sprintf(cmd, "wmm_ps_enabled %d", enable_wmm_ps);
    ret = CLI_REPLY_AND_CHECK(cmd);

    if (ret)
    {
        return FSP_ERR_WIFI_FAILED;
    }

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Initiate WPS Push Button Configuration (PBC)
 *
 *  @retval FSP_SUCCESS              WPS PBC was initiated successfully.
 *  @retval FSP_ERR_WIFI_FAILED      Error occurred with initiating WPS PBC.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_wps_pbc(const char *mac_addr)
{
    char cmd[64] = {0};
    int ret;

    sprintf(cmd, "wps_pbc %s", mac_addr);
    ret = CLI_REPLY_AND_CHECK(cmd);

    if (ret)
    {
        return FSP_ERR_WIFI_FAILED;
    }

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Initiate WPS PIN method
 *
 *  @retval FSP_SUCCESS              WPS PIN was initiated successfully.
 *  @retval FSP_ERR_WIFI_FAILED      Error occurred with initiating WPS PIN.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_wps_pin(const char *mac_addr, const char *pin, char *gen_pin)
{
    char cmd[64] = {0};
    char reply[RM_WIFI_PIN_BUFF_SIZE] = {0};
    int ret_val;

    if (pin == NULL)
    {
        sprintf(cmd, "wps_pin %s", mac_addr);
    }
    else
    {
        sprintf(cmd, "wps_pin %s %s", mac_addr, pin);
    }

    ret_val = cli_reply_generic(cmd, __func__, __LINE__, reply);

    if (pin == NULL && gen_pin != NULL)
    {
        sprintf(gen_pin, reply);
    }

    return ret_val;
}

/*******************************************************************************************************************//**
 * Cancel any ongoing WPS operation 
 *
 *  @retval FSP_SUCCESS              WPS session was canceled successfully.
 *  @retval FSP_ERR_WIFI_FAILED      Error occurred with cancelling WPS.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_wps_cancel(void)
{
    char cmd[64] = {0};
    int ret;

    sprintf(cmd, "wps_cancel");
    ret = CLI_REPLY_AND_CHECK(cmd);

    if (ret)
    {
        return FSP_ERR_WIFI_FAILED;
    }

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Manage the WPS AP PIN state or value
 *
 *  @retval FSP_SUCCESS              Operation was completed successfully.
 *  @retval FSP_ERR_WIFI_FAILED      Error occurred with enabling/disabling/generating.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_wps_ap_pin(const char *state, const char *arg1, const char *arg2, char *gen_pin)
{
    char cmd[64] = {0};
    char reply[RM_WIFI_PIN_BUFF_SIZE] = {0};
    int ret;

    if (arg1 == NULL)
    {
        sprintf(cmd, "wps_ap_pin %s", state);
    }
    else if (arg2 == NULL)
    {
        sprintf(cmd, "wps_ap_pin %s %s", state, arg1);
    }
    else
    {
        sprintf(cmd, "wps_ap_pin %s %s %s", state, arg1, arg2);
    }

    ret = cli_reply_generic(cmd, __func__, __LINE__, reply);

    if (ret)
    {
        return FSP_ERR_WIFI_FAILED;
    }

    if (gen_pin != NULL)
    {
        sprintf(gen_pin, reply);
    }

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Add a network with the specified index.
 *
 * @param[in]  pxNetworkProfile          Pointer to a structure which holds the Wi-Fi network parameters.
 * @param[in]  index                     network index.
 *
 * @retval FSP_SUCCESS                   Function completed successfully.
 * @retval FSP_ERR_WIFI_CONFIG_FAILED    Error occurred with save_config.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_network_add(const WIFINetworkProfile_t * const pxNetworkProfile, uint16_t index)
{
    char cmd[64] = {0};
    char str_ssid[wificonfigMAX_SSID_LEN + 3];       // +3 for quotes and terminating '\0'
    char str_pass[wificonfigMAX_PASSPHRASE_LEN + 3]; // +3 for quotes and terminating '\0'

    sprintf(cmd, "add_network %u", index);
    CLI_REPLY_AND_CHECK(cmd);

    sprintf(cmd, "set_network %u ssid %s", index, quote(pxNetworkProfile->ucSSIDLength,
            str_ssid, (char *) pxNetworkProfile->ucSSID));
    CLI_REPLY_AND_CHECK(cmd);

    /* TODO: Support other security modes */
    if (pxNetworkProfile->xSecurity == eWiFiSecurityWPA2)
    {
        sprintf(cmd, "set_network %u proto RSN", index);
        CLI_REPLY_AND_CHECK(cmd);

        sprintf(cmd, "set_network %u pairwise CCMP", index);
        CLI_REPLY_AND_CHECK(cmd);

        sprintf(cmd, "set_network %u key_mgmt WPA-PSK", index);
        CLI_REPLY_AND_CHECK(cmd);

        sprintf(cmd, "set_network %u psk %s", index, quote(pxNetworkProfile->ucPasswordLength,
            str_pass, (char *) pxNetworkProfile->cPassword));
        CLI_REPLY_AND_CHECK(cmd);
    }
    else
    {
        return FSP_ERR_WIFI_CONFIG_FAILED;
    }

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Delete a network with specified index.
 *
 * @param[in]  index    network index.
 *
 * @retval FSP_SUCCESS         Function completed successfully.
 * @retval FSP_ERR_WIFI_FAILED Error occurred with remove_network.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_network_del(uint16_t index)
{
    int ret;
    char cmd[64] = {0};

    sprintf(cmd, "remove_network %u", index);
    ret = CLI_REPLY_AND_CHECK(cmd);
    if (ret)
        return FSP_ERR_WIFI_FAILED;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 *  Get MAC address.
 *
 * @param[out] p_macaddr     Pointer array to hold mac address.
 *
 * @retval FSP_SUCCESS              Function completed successfully.
 * @retval FSP_ERR_WIFI_FAILED      Error occurred with command to Wifi module.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_mac_addr_get(uint8_t *p_macaddr)
{
    unsigned long macmsw, maclsw;

    getMacAddrMswLsw(WLAN0_IFACE, &macmsw,  &maclsw);
    if ((macmsw == 0) && (maclsw == 0))
        return FSP_ERR_WIFI_FAILED;

    p_macaddr[0] = ((macmsw >> 8) & 0Xff);
    p_macaddr[1] = (macmsw & 0xff);
    p_macaddr[2] = (uint8_t) ((maclsw >> 24) & 0xff);
    p_macaddr[3] = (maclsw >> 16 & 0xff);
    p_macaddr[4] = (maclsw >> 8  & 0xff);
    p_macaddr[5] = (maclsw & 0xff);

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 *  Get the information about local Wifi Access Points.
 *
 * @param[out]  p_results     Pointer to a structure array holding scanned Access Points.
 * @param[in]   maxNetworks  Size of the structure array for holding APs.
 *
 * @retval FSP_SUCCESS                Function completed successfully.
 * @retval FSP_ERR_WIFI_FAILED        Error occurred with command to Wifi module.
 * @retval FSP_ERR_OUT_OF_MEMORY      There is no more heap memory available.
 * @retval FSP_ERR_WIFI_SCAN_COMPLETE Wifi scan has completed.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_scan(WIFIScanResult_t *p_results, uint32_t maxNetworks)
{
    char *buff = pvPortMalloc(RM_WIFI_SCAN_BUF_SIZE);
    int status;
    fsp_err_t ret = FSP_SUCCESS;

    if (!buff)
        return FSP_ERR_OUT_OF_MEMORY;

    memset(buff, 0, RM_WIFI_SCAN_BUF_SIZE);
    status = ra6w1_cli_reply("scan", NULL, buff);

    if (status < 0 || strncmp(buff, "FAIL", 4) == 0)
    {
        ret = FSP_ERR_WIFI_FAILED;
        goto failure;
    }

    rm_wifi_parse_scan_results(buff, maxNetworks, p_results);

failure:
    vPortFree(buff);

    return ret;
}

/*******************************************************************************************************************//**
 *  Get the information about local Wifi Access Points.
 *
 * @param[out]  p_results     Pointer to a structure array holding scanned Access Points.
 * @param[in]   maxNetworks  Size of the structure array for holding APs.
 * @param[in]   pxScanConfigExtended  Wi-Fi scan extended configuration
 *
 * @retval FSP_SUCCESS                Function completed successfully.
 * @retval FSP_ERR_WIFI_FAILED        Error occurred with command to Wifi module.
 * @retval FSP_ERR_OUT_OF_MEMORY      There is no more heap memory available.
 * @retval FSP_ERR_WIFI_SCAN_COMPLETE Wifi scan has completed.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_scan_extended(WIFIScanResult_t *p_results, uint32_t maxNetworks, WIFIScanExtendedConfig_t * pxScanConfigExtended)
{
    char *buff = pvPortMalloc(RM_WIFI_SCAN_BUF_SIZE);
    int status;
    fsp_err_t ret = FSP_SUCCESS;
    char ssid_cmd[64] = {0};
    if (!buff)
        return FSP_ERR_OUT_OF_MEMORY;

    memset(buff, 0, RM_WIFI_SCAN_BUF_SIZE);

    if (pxScanConfigExtended)
    {
        if (pxScanConfigExtended->pxScanConfig.ucChannel)
        {
            int freq = ra6w1_regdb_ch_to_freq((int) pxScanConfigExtended->pxScanConfig.ucChannel);
            sprintf(ssid_cmd, "scan freq=%d", freq);
            status = ra6w1_cli_reply(ssid_cmd, NULL, buff);
        }
        else
        {
            if (pxScanConfigExtended->ucBand == eWiFiBand5G)
	    {
                status = ra6w1_cli_reply("scan band=5", NULL, buff);
            }
            else if (pxScanConfigExtended->ucBand == eWiFiBand2G)
            {
                status = ra6w1_cli_reply("scan band=2", NULL, buff);
            }
            else
            {
                status = ra6w1_cli_reply("scan", NULL, buff);
            }
        }
    }
    else
    {
        status = ra6w1_cli_reply("scan", NULL, buff);
    }

    if (status < 0 || strncmp(buff, "FAIL", 4) == 0)
    {
        ret = FSP_ERR_WIFI_FAILED;
        goto failure;
    }

    rm_wifi_parse_scan_results(buff, maxNetworks, p_results);

failure:
    vPortFree(buff);

    return ret;
}

/*******************************************************************************************************************//**
 *  Start SoftAP mode.
 *
 *  @retval FSP_SUCCESS              Start SoftAP mode successfully.
 *  @retval FSP_ERR_WIFI_FAILED      Error occurred with command to Wifi module.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_start_ap()
{
    int ret;

    ret = CLI_REPLY_AND_CHECK("ap start");
    if (ret)
        return FSP_ERR_WIFI_FAILED;

    return (rm_wifi_is_softap_state("ENABLED"));
}

/*******************************************************************************************************************//**
 *  Stop SoftAP mode.
 *
 *  @retval FSP_SUCCESS              Start SoftAP mode successfully.
 *  @retval FSP_ERR_WIFI_FAILED      Error occurred with command to Wifi module.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_stop_ap()
{
    int ret;

    ret = CLI_REPLY_AND_CHECK("ap stop");
    if (ret)
        return FSP_ERR_WIFI_FAILED;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 *  Configure SoftAP.
 *
 *  param[in] pxNetworkParams        Network parameters.
 *
 *  @retval FSP_SUCCESS              Configure SoftAP successfully.
 *  @retval FSP_ERR_WIFI_FAILED      Error occurred with command to Wifi module.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_configure_ap(WIFINetworkParamsExt_t *pxNetworkParamsExt)
{
    int ret;
    char rep[REPLY_SIZE_256] = {0};
    uint8_t *p_ssid = pxNetworkParamsExt->xNetworkParams.ucSSID;
    WIFISecurityExt_t security = (WIFISecurityExt_t) pxNetworkParamsExt->xNetworkParams.xSecurity;
    char *p_passphrase = pxNetworkParamsExt->xNetworkParams.xPassword.xWPA.cPassphrase;
    uint8_t channel = pxNetworkParamsExt->xNetworkParams.ucChannel;
    WIFIBand_t band = pxNetworkParamsExt->ucBand;
    e_wifi_phy_mode_ext_t wifi_mode = pxNetworkParamsExt->ucWiFi_mode;
    WIFIPmf_t pmf = pxNetworkParamsExt->pmf;
    char cli_cmd[50 + MAX(wificonfigMAX_PASSPHRASE_LEN, wificonfigMAX_SSID_LEN)];
    char str_ssid[wificonfigMAX_SSID_LEN + 3];      // +3 for quotes and terminating '\0'
    char str_psk[wificonfigMAX_PASSPHRASE_LEN + 3]; // +3 for quotes and terminating '\0'
    size_t len_ssid = pxNetworkParamsExt->xNetworkParams.ucSSIDLength;
    size_t len_pass = pxNetworkParamsExt->xNetworkParams.xPassword.xWPA.ucLength;
    int ap_max_inactivity = pxNetworkParamsExt->xApNetParams.ap_max_inactivity;
    WIFIEncryption_t encryption = (WIFIEncryption_t) pxNetworkParamsExt->xApNetParams.ucEncMode;

    if (band == eWiFiBand2G)
        sprintf(cli_cmd, "set setband %s", "2G");
    else if (band == eWiFiBand5G)
        sprintf(cli_cmd, "set setband %s", "5G");
    else
        sprintf(cli_cmd, "set setband AUTO");

    ret = CLI_REPLY_AND_CHECK(cli_cmd);
    if (ret)
    {
        return FSP_ERR_WIFI_CONFIG_FAILED;
    }

    /* Intentionally ignore the reply */
    ra6w1_cli_reply("remove_network 1", NULL, rep);

    ret = CLI_REPLY_AND_CHECK("add_network 1");
    if (ret)
    {
        return FSP_ERR_WIFI_CONFIG_FAILED;
    }

    cli_reply_generic("config_default", __func__, __LINE__, rep); // for WPS

    /* Mode */
    sprintf(cli_cmd, "set_network 1 mode %d", WIFI_DEVICE_MODE_EXT_AP + GAP_USER_CONFIGURE_MODE);
    ret = CLI_REPLY_AND_CHECK(cli_cmd);
    if (ret)
    {
        return FSP_ERR_WIFI_CONFIG_FAILED;
    }

    /* WiFi Mode */
    if ((wifi_mode < WIFI_MODE_MAX) &&
        (get_run_mode() < WIFI_DEVICE_MODE_EXT_AP_STATION))
    {
        sprintf(cli_cmd, "set_network 1 wifi_mode %d", wifi_mode);
        ret = CLI_REPLY_AND_CHECK(cli_cmd);
        if (ret)
        {
            return FSP_ERR_WIFI_CONFIG_FAILED;
        }
    }

    sprintf(cli_cmd, "set_network 1 ssid %s", quote(len_ssid, str_ssid, (char *) p_ssid));
    ret = CLI_REPLY_AND_CHECK(cli_cmd);
    if (ret)
    {
        return FSP_ERR_WIFI_CONFIG_FAILED;
    }

    switch (security)
    {
        case eWiFiSecurityWPA3:
            ret = CLI_REPLY_AND_CHECK("set_network 1 proto RSN");
            if (ret)
            {
                return FSP_ERR_WIFI_CONFIG_FAILED;
            }

            ret = CLI_REPLY_AND_CHECK("set_network 1 pairwise CCMP");
            if (ret)
            {
                return FSP_ERR_WIFI_CONFIG_FAILED;
            }

            ret = CLI_REPLY_AND_CHECK("set_network 1 key_mgmt SAE");
            if (ret)
            {
                return FSP_ERR_WIFI_CONFIG_FAILED;
            }

            ret = CLI_REPLY_AND_CHECK("set_network 1 ieee80211w 2");
            if (ret)
            {
                return FSP_ERR_WIFI_CONFIG_FAILED;
            }

            sprintf(cli_cmd, "set_network 1 psk %s", quote(len_pass, str_psk, p_passphrase));
            ret = CLI_REPLY_AND_CHECK(cli_cmd);
            if (ret)
            {
                return FSP_ERR_WIFI_CONFIG_FAILED;
            }

            /* If SAE groups configured by the user */
            if (strlen(pxNetworkParamsExt->sae_groups) != 0)
            {
                sprintf(cli_cmd, "sae_groups %s", pxNetworkParamsExt->sae_groups);
                ret = CLI_REPLY_AND_CHECK(cli_cmd);
                if (ret)
                {
                    return FSP_ERR_WIFI_CONFIG_FAILED;
                }
            }
            break;

        case eWiFiSecurityWPA2:
            ret = CLI_REPLY_AND_CHECK("set_network 1 proto RSN");
            if (ret)
            {
                return FSP_ERR_WIFI_CONFIG_FAILED;
            }

            if ((encryption == eWiFiEncryptionNone) ||
                (encryption == eWiFiEncryptionAES))
            {
                ret = CLI_REPLY_AND_CHECK("set_network 1 pairwise CCMP");
                if (ret)
                {
                    return FSP_ERR_WIFI_CONFIG_FAILED;
                }
            }
            else if (encryption == eWiFiEncryptionTKIP)
            {
                ret = CLI_REPLY_AND_CHECK("set_network 1 pairwise TKIP");
                if (ret)
                {
                    return FSP_ERR_WIFI_CONFIG_FAILED;
                }
            }
            else if (encryption == eWiFiEncryptionTKIP_AES)
            {
                ret = CLI_REPLY_AND_CHECK("set_network 1 pairwise TKIP CCMP");
                if (ret)
                {
                    return FSP_ERR_WIFI_CONFIG_FAILED;
                }
            }
            else
            {
                return FSP_ERR_WIFI_CONFIG_FAILED;
            }

            ret = CLI_REPLY_AND_CHECK("set_network 1 key_mgmt WPA-PSK");
            if (ret)
            {
                return FSP_ERR_WIFI_CONFIG_FAILED;
            }

            sprintf(cli_cmd, "set_network 1 ieee80211w %d", pmf == PMF_DEFAULT ? PMF_NONE : pmf);
            ret = CLI_REPLY_AND_CHECK(cli_cmd);
            if (ret)
            {
                return FSP_ERR_WIFI_CONFIG_FAILED;
            }

            sprintf(cli_cmd, "set_network 1 psk %s", quote(len_pass, str_psk, p_passphrase));
            ret = CLI_REPLY_AND_CHECK(cli_cmd);
            if (ret)
            {
                return FSP_ERR_WIFI_CONFIG_FAILED;
            }
            break;

        case eWiFiSecurityWPA:
            ret = CLI_REPLY_AND_CHECK("set_network 1 proto WPA");
            if (ret)
            {
                return FSP_ERR_WIFI_CONFIG_FAILED;
            }

            if ((encryption == eWiFiEncryptionNone) ||
                (encryption == eWiFiEncryptionTKIP))
            {
                ret = CLI_REPLY_AND_CHECK("set_network 1 pairwise TKIP");
                if (ret)
                {
                    return FSP_ERR_WIFI_CONFIG_FAILED;
                }
            }
            else if (encryption == eWiFiEncryptionAES)
            {
                ret = CLI_REPLY_AND_CHECK("set_network 1 pairwise CCMP");
                if (ret)
                {
                    return FSP_ERR_WIFI_CONFIG_FAILED;
                }
            }
            else if (encryption == eWiFiEncryptionTKIP_AES)
            {
                ret = CLI_REPLY_AND_CHECK("set_network 1 pairwise TKIP CCMP");
                if (ret)
                {
                    return FSP_ERR_WIFI_CONFIG_FAILED;
                }
            }
            else
            {
                return FSP_ERR_WIFI_CONFIG_FAILED;
            }

            ret = CLI_REPLY_AND_CHECK("set_network 1 key_mgmt WPA-PSK");
            if (ret)
            {
                return FSP_ERR_WIFI_CONFIG_FAILED;
            }

            sprintf(cli_cmd, "set_network 1 psk %s", quote(len_pass, str_psk, p_passphrase));
            ret = CLI_REPLY_AND_CHECK(cli_cmd);
            if (ret)
            {
                return FSP_ERR_WIFI_CONFIG_FAILED;
            }
            break;

        case eWiFiSecurityWPA_WPA2_ext:
            ret = CLI_REPLY_AND_CHECK("set_network 1 proto WPA RSN");
            if (ret)
            {
                return FSP_ERR_WIFI_CONFIG_FAILED;
            }

            ret = CLI_REPLY_AND_CHECK("set_network 1 key_mgmt WPA-PSK");
            if (ret)
            {
                return FSP_ERR_WIFI_CONFIG_FAILED;
            }

            if ((encryption == eWiFiEncryptionNone) ||
                (encryption == eWiFiEncryptionTKIP_AES))
            {
                ret = CLI_REPLY_AND_CHECK("set_network 1 pairwise TKIP CCMP");
                if (ret)
                {
                    return FSP_ERR_WIFI_CONFIG_FAILED;
                }
            }
            else if (encryption == eWiFiEncryptionTKIP)
            {
                ret = CLI_REPLY_AND_CHECK("set_network 1 pairwise TKIP");
                if (ret)
                {
                    return FSP_ERR_WIFI_CONFIG_FAILED;
                }
            }
            else if (encryption == eWiFiEncryptionAES)
            {
                ret = CLI_REPLY_AND_CHECK("set_network 1 pairwise CCMP");
                if (ret)
                {
                    return FSP_ERR_WIFI_CONFIG_FAILED;
                }
            }
            else
            {
                return FSP_ERR_WIFI_CONFIG_FAILED;
            }

            sprintf(cli_cmd, "set_network 1 ieee80211w %d", pmf == PMF_DEFAULT ? PMF_NONE : pmf);
            ret = CLI_REPLY_AND_CHECK(cli_cmd);
            if (ret)
            {
                return FSP_ERR_WIFI_CONFIG_FAILED;
            }

            sprintf(cli_cmd, "set_network 1 psk %s", quote(len_pass, str_psk, p_passphrase));
            ret = CLI_REPLY_AND_CHECK(cli_cmd);
            if (ret)
            {
                return FSP_ERR_WIFI_CONFIG_FAILED;
            }
            break;

        case eWiFiSecurityWPA2_WPA3_ext:
            ret = CLI_REPLY_AND_CHECK("set_network 1 proto RSN");
            if (ret)
            {
                return FSP_ERR_WIFI_CONFIG_FAILED;
            }

            ret = CLI_REPLY_AND_CHECK("set_network 1 key_mgmt WPA-PSK SAE");
            if (ret)
            {
                return FSP_ERR_WIFI_CONFIG_FAILED;
            }

            ret = CLI_REPLY_AND_CHECK("set_network 1 pairwise CCMP GCMP");
            if (ret)
            {
                return FSP_ERR_WIFI_CONFIG_FAILED;
            }

            sprintf(cli_cmd, "set_network 1 ieee80211w %d", pmf == PMF_DEFAULT ? PMF_CAPABLE : pmf);
            ret = CLI_REPLY_AND_CHECK(cli_cmd);
            if (ret)
            {
                return FSP_ERR_WIFI_CONFIG_FAILED;
            }

            sprintf(cli_cmd, "set_network 1 psk %s", quote(len_pass, str_psk, p_passphrase));
            ret = CLI_REPLY_AND_CHECK(cli_cmd);
            if (ret)
            {
                return FSP_ERR_WIFI_CONFIG_FAILED;
            }

            /* If SAE groups configured by the user */
            if (strlen(pxNetworkParamsExt->sae_groups) != 0)
            {
                sprintf(cli_cmd, "sae_groups %s", pxNetworkParamsExt->sae_groups);
                ret = CLI_REPLY_AND_CHECK(cli_cmd);
                if (ret)
                {
                    return FSP_ERR_WIFI_CONFIG_FAILED;
                }
            }
            break;

        case eWiFiSecurityOpen:
            ret = CLI_REPLY_AND_CHECK("set_network 1 key_mgmt NONE");
            if (ret)
            {
                return FSP_ERR_WIFI_CONFIG_FAILED;
            }
            break;

        case eWiFiSecurityWEP: // WEP is not supported in SoftAP
        case eWiFiSecurityWPA2_ent: // WPA2 Enterprise is not supported in SoftAP
        case eWiFiSecurityWPA3_OWE_ext:
            ret = CLI_REPLY_AND_CHECK("set_network 1 proto RSN");
            if (ret)
            {
                return FSP_ERR_WIFI_CONFIG_FAILED;
            }

            ret = CLI_REPLY_AND_CHECK("set_network 1 pairwise CCMP");
            if (ret)
            {
                return FSP_ERR_WIFI_CONFIG_FAILED;
            }

            ret = CLI_REPLY_AND_CHECK("set_network 1 key_mgmt OWE");
            if (ret)
            {
                return FSP_ERR_WIFI_CONFIG_FAILED;
            }

            ret = CLI_REPLY_AND_CHECK("set_network 1 ieee80211w 2");
            if (ret)
            {
                return FSP_ERR_WIFI_CONFIG_FAILED;
            }
            break;

        default:
            return FSP_ERR_WIFI_CONFIG_FAILED;
    }

    sprintf(cli_cmd, "ap_max_inactivity %d", ap_max_inactivity);
    ret = CLI_REPLY_AND_CHECK(cli_cmd);
    if (ret)
    {
        return FSP_ERR_WIFI_CONFIG_FAILED;
    }

    sprintf(cli_cmd, "set_network 1 channel %d", channel);
    ret = CLI_REPLY_AND_CHECK(cli_cmd);
    if (ret)
    {
        return FSP_ERR_WIFI_CONFIG_FAILED;
    }

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 *  Check if the Wi-Fi is connected and the AP configuration matches the query.
 *
 *  param[in] pxNetworkParams - Network parameters to query, if NULL then just check the Wi-Fi link status.
 *
 *  @retval FSP_SUCCESS                      Wi-Fi is connected and the AP configuration matches the query.
 *  @retval FSP_ERR_OUT_OF_MEMORY            There is no more heap memory available.
 *  @retval FSP_ERR_WIFI_FAILED              Error occurred with command to Wifi module.
 *  @retval FSP_ERR_WIFI_AP_NOT_CONNECTED    No connection to access point has happened.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_is_connected(const WIFINetworkParams_t *pxNetworkParams)
{
    int ret;
    fsp_err_t ret_err = FSP_ERR_WIFI_FAILED;
    char *buff = pvPortMalloc(RM_WIFI_STATUS_BUF_SIZE);
    char *new_buff;

    if (!buff)
        return FSP_ERR_OUT_OF_MEMORY;

    ret = ra6w1_cli_reply("status", NULL, buff);
    if (ret < 0 || strncmp(buff, "FAIL", 4) == 0)
    {
        ret_err = FSP_ERR_WIFI_FAILED;
        goto failure;
    }

    ret_err = rm_wifi_parse_status_and_match_ap_conf(pxNetworkParams, buff);
    if (!ret_err)
    {
        memset(buff, 0, RM_WIFI_STATUS_BUF_SIZE);
        ret = ra6w1_cli_reply("get_network 0 psk", NULL, buff);
        if (ret < 0 || strncmp(buff, "FAIL", 4) == 0)
        {
            ret_err = FSP_ERR_WIFI_FAILED;
            goto failure;
        }

        new_buff = buff + 1;

        if (strncmp(new_buff, &pxNetworkParams->xPassword.xWPA.cPassphrase[0], strlen(new_buff)-1) != 0)
        {
            ret_err = FSP_ERR_WIFI_FAILED;
            goto failure;
        }

        if ((strlen(new_buff) - 1) != pxNetworkParams->xPassword.xWPA.ucLength)
        {
            ret_err = FSP_ERR_WIFI_FAILED;
            goto failure;
        }

        ret_err = FSP_SUCCESS;
    }

failure:
    vPortFree(buff);

    return ret_err;
}

/*******************************************************************************************************************//**
 *  Get extended Wi-Fi info of the interfaces.
 *
 *  @retval FSP_SUCCESS              Get info successfully.
 *  @retval FSP_ERR_OUT_OF_MEMORY    There is no more heap memory available.
 *  @retval FSP_ERR_WIFI_FAILED      Error occurred with command to Wifi module.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_get_connection_info_ext(WIFIConnectionInfoExt_t *pxConnectionInfoExt, int iface_num)
{
    int status;
    fsp_err_t ret;
    char *buff = pvPortMalloc(RM_WIFI_STATUS_BUF_SIZE);

    if (!buff)
    {
        return FSP_ERR_OUT_OF_MEMORY;
    }

    status = ra6w1_cli_reply("status", NULL, buff);
    if (status < 0 || strncmp(buff, "FAIL", 4) == 0)
    {
        ret = FSP_ERR_WIFI_FAILED;
        goto failure;
    }

    ret = rm_wifi_parse_connection_info(pxConnectionInfoExt, buff, iface_num);

failure:
    vPortFree(buff);

    return ret;
}

/*******************************************************************************************************************//**
 *  Get Wi-Fi info of the interfaces.
 *
 *  @retval FSP_SUCCESS              Get info successfully.
 *  @retval FSP_ERR_OUT_OF_MEMORY    There is no more heap memory available.
 *  @retval FSP_ERR_WIFI_FAILED      Error occurred with command to Wifi module.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_get_connection_info(WIFIConnectionInfo_t *pxConnectionInfo, int iface_num)
{
    fsp_err_t ret = FSP_SUCCESS;
    WIFIConnectionInfoExt_t * conn_info_ext;

    conn_info_ext = pvPortMalloc(sizeof(*conn_info_ext) * iface_num);

    if (!conn_info_ext)
    {
        ret = FSP_ERR_OUT_OF_MEMORY;
        goto failure;
    }

    ret = rm_wifi_get_connection_info_ext(conn_info_ext, iface_num);
    *pxConnectionInfo = conn_info_ext->connection_info;

failure:
    vPortFree(conn_info_ext);

    return ret;
}

/*******************************************************************************************************************//**
 * Return the RSSI of the connected AP.
 *
 * @param[out] pcRSSI                       RSSI of the connected AP.
 *
 * @retval FSP_SUCCESS                      Function completed successfully.
 * @retval FSP_ERR_WIFI_AP_NOT_CONNECTED    No connection to access point has happened.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_get_rssi(int8_t *pcRSSI)
{
    int16_t rssi = (int16_t) get_sta_signal_poll();

    if (rssi == INVALID_RSSI)
        return FSP_ERR_WIFI_AP_NOT_CONNECTED;

    *pcRSSI = (int8_t)rssi;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 *  AP mode disconnecting a station.
 * 
 *  @param[in] pucMac                Station MAC address do be disconnected.
 *
 *  @retval FSP_SUCCESS              Disconnection was started successfully.
 *  @retval FSP_ERR_WIFI_FAILED      Error occurred.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_start_disconnect_station(uint8_t * pucMac)
{
    #define MAC_LEN 18

    char cmd[64] = {0};
    char macStr[MAC_LEN];
    int ret;

    snprintf(macStr, MAC_LEN, "%02X:%02X:%02X:%02X:%02X:%02X",
             pucMac[0], pucMac[1], pucMac[2],
             pucMac[3], pucMac[4], pucMac[5]);

    sprintf(cmd, "disassociate %s", macStr);
    printf("cmd is %s", cmd);
    ret = CLI_REPLY_AND_CHECK(cmd);

    if (ret)
        return FSP_ERR_WIFI_FAILED;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Set Wi-Fi MAC addresses.
 *
 * @param[in] pucMac                Station MAC address.
 *
 * @retval FSP_SUCCESS              Function completed successfully.
 * @retval FSP_ERR_WIFI_FAILED      Error occurred with command to Wifi module.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_mac_addr_set(uint8_t *pucMac)
{
    int ret;
    char mac_addr[RM_WIFI_CHAR_MAC_LEN];

    if (get_run_mode() == WIFI_DEVICE_MODE_EXT_STATION)
    {
        snprintf(mac_addr, RM_WIFI_CHAR_MAC_LEN, "%02X%02X%02X%02X%02X%02X",
                 pucMac[0], pucMac[1], pucMac[2], pucMac[3], pucMac[4], pucMac[5]);
        ret = rm_wifi_write_mac_address(mac_addr, MAC_SPOOFING);
        if (ret != E_WRITE_OK)
            return FSP_ERR_WIFI_FAILED;
    }
    else
    {
        return FSP_ERR_WIFI_FAILED;
    }

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Set country code.
 *
 * @param[in] pcCountryCode         Country code (null-terminated string, e.g. "US", "CN". See ISO-3166).
 *
 * @retval FSP_SUCCESS              Function completed successfully.
 * @retval FSP_ERR_WIFI_FAILED      Error occurred with command to Wifi module.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_set_country_code(const char *pcCountryCode)
{
    int ret;
    char buff[RM_WIFI_SET_COUNTRY_BUFF_SIZE];
    char set_country_cmd[RM_WIFI_SET_COUNTRY_CMD_LEN] = {0};

    if ((pcCountryCode == NULL) || (strlen(pcCountryCode) > RM_WIFI_COUNTRY_CODE_LEN))
        return FSP_ERR_WIFI_FAILED;

    sprintf(set_country_cmd, "country %s", pcCountryCode);
    ret = ra6w1_cli_reply(set_country_cmd, NULL, buff);
    if (ret < 0 || (strncmp(buff, "FAIL", 4) == 0) || (strcmp(buff, "Incorrect param!!") == 0))
        return FSP_ERR_WIFI_FAILED;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Return the country code.
 *
 * @param[out] pcCountryCode              Null-terminated string to hold the country code (ISO-3166).
 *                                        Must be at least 4 bytes.
 *
 * @retval FSP_SUCCESS                    Function completed successfully.
 * @retval FSP_ERR_WIFI_AP_NOT_CONNECTED  No connection to access point has happened.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_get_country_code(char *pcCountryCode)
{
    int ret;
    char buff[RM_WIFI_GET_COUNTRY_BUFF_SIZE];

    ret = ra6w1_cli_reply("country", NULL, buff);
    if (ret < 0 || strncmp(buff, "FAIL", 4) == 0)
        return FSP_ERR_WIFI_FAILED;

    strncpy(pcCountryCode, buff, RM_WIFI_COUNTRY_CODE_LEN);
    pcCountryCode[RM_WIFI_COUNTRY_CODE_LEN] = '\0';

    return FSP_SUCCESS;
}

fsp_err_t rm_wifi_set_atcmd_event_callback(void * const p_ctrl,
                                            unsigned int (* p_callback)(void * const p_ctrl, int index,
                                                                        unsigned char * p_in, unsigned int inlen))
{
    #if (ATCMD_IF_SUPPORT == 1)
    int ret = 0;

    ret = wpa_supplicant_set_atcmd_event_callback(p_ctrl, p_callback);
    if (ret)
        return FSP_ERR_WIFI_FAILED;

    return FSP_SUCCESS;
    #else
    return FSP_ERR_UNSUPPORTED;
    #endif
}

fsp_err_t rm_wifi_set_startup_atcmd_event_callback(void * const p_ctrl,
                                                    unsigned int (* p_callback)(void * const p_ctrl,
                                                                                unsigned char * p_in,
                                                                                unsigned int inlen))
{
    #if (ATCMD_IF_SUPPORT == 1)
    unsigned int ret = 0;

    ret = atcmd_set_startup_atcmd_event_callback(p_ctrl, p_callback);
    if (ret)
        return FSP_ERR_WIFI_FAILED;

    return FSP_SUCCESS;
    #else
    return FSP_ERR_UNSUPPORTED;
    #endif
}

fsp_err_t rm_wifi_get_atcmd_hostinitdone_resp(char * p_out, size_t outlen)
{
    #if (ATCMD_IF_SUPPORT == 1)
    unsigned int ret = 0;

    ret = atcmd_set_initdone_resp(p_out, outlen, 0);
    if (ret)
        return FSP_ERR_WIFI_FAILED;

    return FSP_SUCCESS;
    #else
    return FSP_ERR_UNSUPPORTED;
    #endif
}

/**
 * @brief Wrapper function to retrieve network interface.
 *
 * @param[in]   iface_index Index of the desired interface.
 * @retval      The output pointer.
 */
struct netif *rm_wifi_get_netif(int iface_index)
{
    return net_get_netif(iface_index);
}

WIFIReturnCode_t WIFI_SetModeExt(e_wifi_device_mode_ext_t xDeviceModeExt)
{
    fsp_err_t ret = FSP_ERR_WIFI_FAILED;

    if (xDeviceModeExt > WIFI_DEVICE_MODE_EXT_AP_STATION)
        return eWiFiFailure;

    ret = rm_wifi_set_mode(xDeviceModeExt);
    if (ret)
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_GetModeExt(e_wifi_device_mode_ext_t *xDeviceModeExt)
{
    fsp_err_t ret = FSP_ERR_WIFI_FAILED;

    ret = rm_wifi_get_mode(xDeviceModeExt);
    if (ret)
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_ConfigureAPExt(WIFINetworkParamsExt_t *pxNetworkParamsExt)
{
    if (eWiFiSuccess != WIFI_NetworkParams_check_Ext(pxNetworkParamsExt))
    {
        return eWiFiFailure;
    }

    if (FSP_SUCCESS != rm_wifi_configure_ap(pxNetworkParamsExt))
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

/*******************************************************************************************************************//**
 *  Get connected station list in softAP mode.
 *
 *  @retval FSP_SUCCESS              Get connected station list successfully.
 *  @retval FSP_ERR_OUT_OF_MEMORY    There is no more heap memory available.
 *  @retval FSP_ERR_WIFI_FAILED      Error occurred with command to Wifi module.
 **********************************************************************************************************************/
fsp_err_t rm_wifi_get_station_list(WIFIStationInfo_t * pxStationList, uint8_t * pcStationListSize)
{
    fsp_err_t ret;
    char * buff = pvPortMalloc(RM_WIFI_STATION_LIST_BUFF_SIZE);

    if (!buff) {
        return FSP_ERR_OUT_OF_MEMORY;
    }

    memset(buff, 0, RM_WIFI_STATION_LIST_BUFF_SIZE);
    ra6w1_cli_reply("list_sta", NULL, buff);
    if (strncmp(buff, "FAIL", 4) == 0)
    {
        ret = FSP_ERR_WIFI_FAILED;
        goto failure;
    }

    ret = rm_wifi_parse_get_station_list(pxStationList, pcStationListSize, buff);

failure:
    vPortFree(buff);

    return ret;
}

fsp_err_t rm_wifi_twt_setup(struct twt_setup_req *req)
{
    fsp_err_t ret = FSP_SUCCESS;

    if (chk_network_ready(WLAN0_IFACE) == pdFALSE)
    {
        return FSP_ERR_WIFI_FAILED;
    }

    if (!twt_setup(req))
    {
        ret = FSP_ERR_WIFI_FAILED;
    }

    return ret;
}

fsp_err_t rm_wifi_twt_teardown(struct twt_teardown_req *req)
{
    fsp_err_t ret = FSP_SUCCESS;

    if (!twt_teardown(req))
    {
        ret = FSP_ERR_WIFI_FAILED;
    }

    return ret;
}

WIFIReturnCode_t WIFI_RegisterEventExt(WIFIEventExtType_t xEventExtType, WIFIEventExtHandler_t xHandler)
{
    if (rm_wifi_event_register(xEventExtType, (rm_wifi_event_handler_t)xHandler))
    {
        return eWiFiNotSupported;
    }

    return eWiFiSuccess;
}

fsp_err_t rm_wifi_ps_mode_set(const bool enable)
{
    char cmd[64] = {0};
    int ret;
    bool update_needed = false;

#if CFG_PMGR
    if (RM_PMGR_W_dpm_is_enabled() == true && enable)
    {
        // Wifi PS and dpm cannot coexist!
        RM_PMGR_W_dpm_disable();
    }
#endif

    if (enable != me_ps_on_get())
        update_needed = true;

    sprintf(cmd, "set ps %d", enable);
    ret = CLI_REPLY_AND_CHECK(cmd);
    if (ret)
    {
        return FSP_ERR_WIFI_FAILED;
    }

    if (update_needed)
    {
        /* RT-Thread owns lwIP's timeout scheduler. The original SDK changed
         * its private timeout list here when entering WiFi power-save mode;
         * that private API is intentionally not part of RT-Thread lwIP. */
    }

    return FSP_SUCCESS;
}

fsp_err_t rm_wifi_ps_mode_get(bool *config)
{
    *config = me_ps_on_get();

    return FSP_SUCCESS;
}

fsp_err_t rm_wifi_set_listen_interval(const int listen_interval)
{
    if (rwnx_send_mm_set_ps_options(0, listen_interval, -1))
    {
        return FSP_ERR_WIFI_FAILED;
    }

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @} (end addtogroup WIFI)
 **********************************************************************************************************************/

