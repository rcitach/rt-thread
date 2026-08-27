/*
 * WPA Supplicant - command line interface for wpa_supplicant daemon
 * Copyright (c) 2004-2013, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 *
 * Copyright (c) 2023 Modified by Renesas Electronics
 */


#include "includes.h"
#include "common/wpa_ctrl.h"
#include "supp_config.h"
#include "supp_eloop.h"
#include "common_utils.h"
#include "common_def.h"
#include "rm_wifi_helper.h"

#define __USE_HEAP_FOR_CLI_SCAN__    /* Use heap area to alloc rsp_buffer */

#undef  __DEBUG_CLI_REPLY__             /* DEBUG */
#undef  __DEBUG_WLANINIT_CMD_PARSER__   /* DEBUG */
#undef  CONFIG_MESH                     /* MESH Debug CMD Disabled */

#define MAX_WLANINIT_PARAMS             16
#define MAX_CLI_PARAMS                  DEFAULT_BSS_MAX_COUNT
#define CLI_DELIMIT                     " "

#define CLI_CMD_REQ_UNICODE_SIZE        384    /* for hexa unicode */
#define CLI_CMD_REQ_BUF_SIZE            256
#define CLI_RSP_SMALL_BUF_SIZE          1024
#define CLI_RSP_BUF_SIZE                CLI_SCAN_RSP_BUF_SIZE

#define WLANINIT_CMD_BUF_SIZE           CLI_CMD_REQ_UNICODE_SIZE

#define PROFILE_IDX                     0
#define MODE_IDX                        1
#define SSID_IDX                        2
#define KEY_MGMT_PROTO_IDX              3

/* Enterprise */
#define EAP_METHOD_IDX                  3
#define EAP_PHASE2_METHOD_IDX           4
#define EAP_ID_IDX                      5
#define EAP_PW_IDX                      6

#define KEY_MGMT_PAIRWISE_CIPHER_IDX    4
#define KEY_MGMT_PSK_KEY_IDX            5
#define KEY_MGMT_WEP_USE_INDEX_IDX      4
#define KEY_MGMT_WEP_KEY_IDX            5
#define CHANNEL_IDX                     6
#define COUNTRY_CODE_IDX                7
#define OPEN_MODE_CHANNEL_IDX           4
#define OPEN_MODE_COUNTRY_CODE_IDX      5

#define REPLY_LEN                       5
#define REPLY_OK                        "OK"
#define REPLY_FAIL                      "FAIL"

/* WLANINIT DEFINE */
#define MODE_STA                        '1'
#define MODE_AP                         '2'
#define MODE_MESH_POINT                 '5'

#define WEPKEY_INDEX_MIN                '0'
#define WEPKEY_INDEX_MAX                '3'

#define CMD_ENABLE_NETWORK              "enable_network"
#define CMD_DISABLE_NETWORK             "disable_network"
#define CMD_ADD_NETWORK                 "add_network"
#define CMD_REMOVE_NETWORK              "remove_network"
#define CMD_SET_NETWORK                 "set_network"
#define CMD_COUNTRY                     "country"
#define CMD_CLI_SAVE                    "save_config"

#define CMD2_SSID                       "ssid"
#define CMD2_FREQ                       "frequency"
#define CMD2_CHANNEL                    "channel"

#define CMD2_KEY_MGMT                   "key_mgmt"
#define VAL_KEY_MGMT_NONE               "NONE"
#define VAL_KEY_MGMT_WPA_PSK            "WPA-PSK"
#define VAL_KEY_MGMT_WPA_PSK_SHA256            "WPA-PSK-SHA256"
#define VAL_KEY_MGMT_WPA_PSK_WPA_PSK_SHA256    "WPA-PSK WPA-PSK-SHA256"
#define VAL_KEY_MGMT_SAE                "SAE"
#define VAL_KEY_MGMT_WPA_PSK_SAE        "SAE WPA-PSK"
#define VAL_KEY_MGMT_WPA_PSK_SHA256_SAE "WPA-PSK-SHA256 SAE"
#define VAL_KEY_MGMT_WPA_PSK_WPA_PSK_SHA256_SAE    "WPA-PSK WPA-PSK-SHA256 SAE"
#define VAL_KEY_MGMT_OWE                "OWE"
#ifdef __SUPPORT_WPA_ENTERPRISE_CORE__
#define VAL_KEY_MGMT_EAP                "WPA-EAP"
#define VAL_KEY_MGMT_EAPS256            "WPA-EAP-SHA256"
#define VAL_KEY_MGMT_EAPS256_EAP        "WPA-EAP-SHA256 WPA-EAP"
#define VAL_KEY_MGMT_EAP192B            "WPA-EAP-SUITE-B-192"
#endif // __SUPPORT_WPA_ENTERPRISE_CORE__

#define CMD2_PROTO                      "proto"
#define VAL_PROTO_WPA1                  "WPA"
#define VAL_PROTO_WPA2_RSN              "RSN"
#define VAL_PROTO_WPA1_2                "RSN WPA"

#define CMD2_WEPKEY_USE_INDEX           "wep_tx_keyidx"
#define CMD2_WEPKEYx                    "wep_key"

#define CMD2_PAIRWISE                   "pairwise"
#define VAL_PAIRWISE_TKIP               "TKIP"
#define VAL_PAIRWISE_AES_CCMP           "CCMP"
#define VAL_PAIRWISE_TKIP_CCMP          "TKIP CCMP"
#define VAL_PAIRWISE_GCMP_256           "GCMP-256"

#define CMD2_GROUP                      "group"
#define VAL_GROUP_GCMP_256              "GCMP-256"

#define CMD2_GROUP_MGMT                 "group_mgmt"
#define VAL_GROUP_MGMT_BIP_GMAC_256     "BIP-GMAC-256"


#define CMD2_PSK_KEY                    "psk"
#define CMD2_SAE_PASSWORD               "sae_password"

#define CMD2_WPAS_MODE                  "mode"
#define CMD2_PMF_80211W                 "ieee80211w"

#define RA6W_SCAN_RESULTS_RX_EV        0x00000020
#define RA6W_SCAN_RESULTS_TX_EV        0x00000040

//extern functions
#if defined ( SIGMA_TEST_ENABLE )
extern int PRINT_SIGMA_CMD(const char *fmt, ...);
#endif  // SIGMA_TEST_ENABLE

extern int i3ed11_freq_to_ch(int freq);

extern EventGroupHandle_t    ra6w1_sp_event_group;
#define     printf(...)    printf(__VA_ARGS__)
extern UINT is_supplicant_done();
#ifdef __SUPPORT_MESH__
extern int get_run_mode(void);
#endif /* __SUPPORT_MESH__ */

static void print_help(const char *cmd);
static int wpa_request(int argc, char *argv[], char* reply);

#ifdef __DEBUG_CLI_REPLY__
char    dbg_wpa_cli = 1;
#else
char    dbg_wpa_cli = 0;
#endif // __DEBUG_CLI_REPLY__

/**
 * ex)
 * print_separate_bar("=", 10, 2);
 *
 * "==========\n\n"
 *
 */
static void print_separate_bar(unsigned char text, unsigned char loop_count, unsigned char CR_loop_count)
{
    unsigned char prt_str[260];

    memset(prt_str, 0, 256);

    if ((loop_count + CR_loop_count) + 1 > 260) {
        loop_count = (unsigned char)(260 - (CR_loop_count - 1));
    }

    memset(prt_str, text, loop_count);

    if (CR_loop_count > 0) {
        memset(prt_str + loop_count, '\n', CR_loop_count);
    }
    printf("%s", prt_str);
}

static int str_starts(const char *src, const char *match)
{
    return strncmp(src, match, strlen(match)) == 0;
}

static void wpa_cli_msg_cb(char *msg)
{
    ra6w1_cli_prt("<%s> msg=[%s]\n", __func__, msg);
}

static int _wpa_ctrl_command(char *cmd, int print , char *reply)
{
#ifdef CONFIG_SCAN_REPLY_OPTIMIZE
    char *cli_rsp_buf_p = NULL;
#else
    char *cli_rsp_buf_p;
#endif /* CONFIG_SCAN_REPLY_OPTIMIZE */
    size_t len;
    int ret;
#ifdef __FIXED_SCAN_RESULT_BUF__
    char scan_result_cmd_flag = 0;
#endif /* __FIXED_SCAN_RESULT_BUF__ */

    ra6w1_cli_prt("<%s> cmd=[%s]\n", __func__, cmd);

#ifndef CONFIG_SCAN_REPLY_OPTIMIZE
    cli_rsp_buf_p = pvPortMalloc(CLI_RSP_BUF_SIZE);

    if (cli_rsp_buf_p == NULL) {
        ra6w1_cli_prt("<%s> cli_rsp_buf_p alloc fail\n", __func__);
        return -1;
    }
#endif /* CONFIG_SCAN_REPLY_OPTIMIZE */

    if (is_supplicant_done() == 0) {
        printf("[cli] Not SUPP init.(cmd=%s)\n", cmd);
        ret = -1;
        goto end;
    }

    len = CLI_RSP_BUF_SIZE - 1;

#ifdef    CONFIG_IMMEDIATE_SCAN
    if (strncmp(cmd, "SCAN", 4) == 0) {
        xEventGroupWaitBits(ra6w1_sp_event_group, RA6W_SCAN_RESULTS_RX_EV, pdTRUE, pdFALSE, OS_EVENT_NO_WAIT);

#ifdef    CONFIG_SCAN_RESULT_OPTIMIZE
        xEventGroupSetBits(ra6w1_sp_event_group, RA6W_SCAN_RESULTS_TX_EV);
#endif    /* CONFIG_SCAN_RESULT_OPTIMIZE */
    }
#endif    /* CONFIG_IMMEDIATE_SCAN */

#ifdef CONFIG_SCAN_REPLY_OPTIMIZE
    ret = wpa_ctrl_request(cmd, strlen(cmd), &cli_rsp_buf_p, &len, wpa_cli_msg_cb);
#else
    ret = wpa_ctrl_request(cmd, strlen(cmd), cli_rsp_buf_p, &len, wpa_cli_msg_cb);
#endif /* CONFIG_SCAN_REPLY_OPTIMIZE */

    if (ret != 0 || len == 0 ) {
        ra6w1_cli_prt("'%s' command failed(ret = %d).\n", cmd, ret);
        goto end;
    }

#ifdef    CONFIG_IMMEDIATE_SCAN
    if ((strncmp(cli_rsp_buf_p, "OK", 2) == 0) && (strncmp(cmd, "SCAN", 4) == 0)) {
#ifdef CONFIG_SCAN_REPLY_OPTIMIZE
        if (cli_rsp_buf_p) {
            vPortFree(cli_rsp_buf_p);
            cli_rsp_buf_p = NULL;
        }
        ret = wpa_ctrl_request("SCAN_RESULTS", 12, &cli_rsp_buf_p, &len, wpa_cli_msg_cb);
#ifdef __FIXED_SCAN_RESULT_BUF__
        scan_result_cmd_flag = 1;
#endif /* __FIXED_SCAN_RESULT_BUF__ */    
#else
        ret = wpa_ctrl_request("SCAN_RESULTS", 12, cli_rsp_buf_p, &len, wpa_cli_msg_cb);
#endif /* CONFIG_SCAN_REPLY_OPTIMIZE */
    } else {

        xEventGroupWaitBits(ra6w1_sp_event_group,
                       RA6W_SCAN_RESULTS_RX_EV,
                       pdTRUE,
                       pdFALSE,
                       OS_EVENT_NO_WAIT);
    }
#endif  /* CONFIG_IMMEDIATE_SCAN */

end:
    if (ret == -1) {
        strcpy(reply, "FAIL");
    } else {
        strcpy(reply, cli_rsp_buf_p);
    }

#ifdef CONFIG_SCAN_REPLY_OPTIMIZE
    if (cli_rsp_buf_p && ret == 0 && len != 0) {
        vPortFree(cli_rsp_buf_p);
    }
#else
    {
        if (cli_rsp_buf_p) {
            vPortFree(cli_rsp_buf_p);
        }
    }
#endif /* CONFIG_SCAN_REPLY_OPTIMIZE */
    cli_rsp_buf_p = NULL;

    return ret;
}

static int wpa_ctrl_command(char *cmd, char *reply)
{
    return _wpa_ctrl_command(cmd, 1, reply);
}

static int write_cmd(char *buf, size_t buflen, const char *cmd, int argc,
             char *argv[])
{
    int i, res;
    char *pos, *end;

    pos = buf;
    end = buf + buflen;

    res = snprintf(pos, end - pos, "%s", cmd);
    if (res < 0 || res >= end - pos)
        goto fail;
    pos += res;

    for (i = 0; i < argc; i++) {
        res = snprintf(pos, end - pos, " %s", argv[i]);
        if (res < 0 || res >= end - pos)
            goto fail;
        pos += res;
    }

    buf[buflen - 1] = '\0';

    return 0;

fail:
    ra6w1_cli_prt("<%s> Too long command\n", __func__);
    return -1;
}

static int wpa_cli_cmd(const char *cmd, int min_args, int argc, char *argv[], char *reply)
{
    char    buf[CLI_CMD_REQ_BUF_SIZE];
    int    ret;

    if (argc < min_args) {
        printf("Invalid %s - at least %d arg%s required.\n",
                cmd, min_args,
               min_args > 1 ? "s are" : " is");
        return -1;
    }

    memset(buf, 0, CLI_CMD_REQ_BUF_SIZE);
    if (write_cmd(buf, sizeof(buf), cmd, argc, argv) < 0) {
        return -1;
    }

    ra6w1_cli_prt("<%s> cmd=[%s],buf=[%s]\n", __func__, cmd, buf);

    ret = wpa_ctrl_command(buf, reply);

#if defined(SIGMA_TEST_ENABLE)
    if(!strstr(cmd, "SCAN") && !strstr(cmd, "scan"))
        PRINT_SIGMA_CMD("%s\n", reply);
#endif // SIGMA_TEST_ENABLE

    return ret;
}


/* WPA_CLI Commands */
static int wpa_cli_cmd_help(int argc, char *argv[], char *reply)
{
    print_help(argc == 0 ? argv[0] : NULL);
    return 0;
}

static int wpa_cli_cmd_status(int argc, char *argv[], char *reply)
{
    return wpa_ctrl_command("STATUS", reply);
}

#ifdef CONFIG_ENTERPRISE
static int wpa_cli_cmd_pmksa(int argc, char *argv[], char *reply)
{
    return wpa_ctrl_command("PMKSA", reply);
}

static int wpa_cli_cmd_pmksa_flush(int argc, char *argv[], char *reply)
{
    return wpa_ctrl_command("PMKSA_FLUSH", reply);
}
#endif    // CONFIG_ENTERPRISE

#ifdef CONFIG_PMKSA_CACHE_EXTERNAL
static int wpa_cli_cmd_pmksa_get(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("PMKSA_GET", 1, argc, argv, reply);
}


static int wpa_cli_cmd_pmksa_add(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("PMKSA_ADD", 8, argc, argv, reply);
}

#ifdef CONFIG_MESH
static int wpa_cli_mesh_cmd_pmksa_get(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("MESH_PMKSA_GET", 1, argc, argv, reply);
}

static int wpa_cli_mesh_cmd_pmksa_add(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("MESH_PMKSA_ADD", 4, argc, argv, reply);
}
#endif /* CONFIG_MESH */
#endif /* CONFIG_PMKSA_CACHE_EXTERNAL */

#ifdef    CONFIG_LOG_MASK
static int wpa_cli_cmd_set_log(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("SET_LOG", 1, argc, argv, reply);
}

static int wpa_cli_cmd_get_log(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("GET_LOG", 0, argc, argv, reply);
}
#endif    /* CONFIG_LOG_MASK */

static int wpa_cli_cmd_save_config(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("SAVE_CONFIG", 0, argc, argv, reply);
}

#ifdef __SUPPORT_WPA3_SAE__
static int wpa_cli_cmd_sae_groups(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("SAE_GROUPS", 0, argc, argv, reply);
}
#endif /* __SUPPORT_WPA3_SAE__ */



static int wpa_cli_cmd_select_network(int argc, char *argv[], char *reply)
{
    if (argc == 0)
        printf("Need <0(STA)|1(AP)"
#ifdef    CONFIG_P2P
                "|2(P2P)"
#endif    /* CONFIG_P2P */
                "> \n");
    return wpa_cli_cmd("SELECT_NETWORK", 1, argc, argv, reply);
}

static int wpa_cli_cmd_add_network(int argc, char *argv[], char *reply)
{
    if (argc == 0)
        printf("Need <0(STA)|1(AP)"
#ifdef    CONFIG_P2P
                "|2(P2P)"
#endif    /* CONFIG_P2P */
#ifdef __SUPPORT_MESH__
                "|3(MESH)"
#endif /* __SUPPORT_MESH__ */
                "> \n");
    return wpa_cli_cmd("ADD_NETWORK", 1, argc, argv, reply);
}

static int wpa_cli_cmd_remove_network(int argc, char *argv[], char *reply)
{
    if (argc == 0)
        printf("Need <0(STA)|1(AP)"
#ifdef    CONFIG_P2P
                "|2(P2P)"
#endif    /* CONFIG_P2P */
                ">\n");
    return wpa_cli_cmd("REMOVE_NETWORK", 1, argc, argv, reply);
}

static int wpa_cli_cmd_enable_network(int argc, char *argv[], char *reply)
{
    if (argc == 0)
        printf("Need <0(STA)|1(AP)"
#ifdef    CONFIG_P2P
                "|2(P2P)"
#endif    /* CONFIG_P2P */
                ">\n");
    return wpa_cli_cmd("ENABLE_NETWORK", 1, argc, argv, reply);
}
static int wpa_cli_cmd_disable_network(int argc, char *argv[], char *reply)
{
    if (argc == 0)
        printf("Need <0(STA)|1(AP)"
#ifdef    CONFIG_P2P
                "|2(P2P)"
#endif    /* CONFIG_P2P */
                ">\n");
    return wpa_cli_cmd("DISABLE_NETWORK", 1, argc, argv, reply);
}

static int wpa_cli_cmd_set_network(int argc, char *argv[], char *reply)
{
    if (argc < 3) {
        printf("Need <0(STA)|1(AP)"
#ifdef    CONFIG_P2P
                "|2(P2P)"
#endif    /* CONFIG_P2P */
#ifdef __SUPPORT_MESH__
                "|3(MESH)"
#endif /* __SUPPORT_MESH__ */
                ">, <field>, <value>\n");
        return -1;
    }

    return wpa_cli_cmd("SET_NETWORK", 3, argc, argv, reply);
}

static int wpa_cli_cmd_get_network(int argc, char *argv[], char *reply)
{
    if (argc != 2) {
        printf("Need <0(STA)|1(AP)"
#ifdef    CONFIG_P2P
                "|2(P2P)"
#endif    /* CONFIG_P2P */
#ifdef __SUPPORT_MESH__
                "|3(MESH)"
#endif /* __SUPPORT_MESH__ */
                ">, <field>\n");
        return -1;
    }

    return wpa_cli_cmd("GET_NETWORK", 2, argc, argv, reply);
}

static int wpa_cli_cmd_list_networks(int argc, char *argv[], char *reply)
{
    return wpa_ctrl_command("LIST_NETWORKS", reply);
}

static int wpa_cli_country(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("COUNTRY", 0, argc, argv, reply);
}

static int wpa_cli_cmd_scan(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("SCAN", 0, argc, argv, reply);
}

static int wpa_cli_cmd_disconnect(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("DISCONNECT", 0, argc, argv, reply);
}

static int wpa_cli_cmd_reassociate(int argc, char *argv[], char *reply)
{
    return wpa_ctrl_command("REASSOCIATE", reply);
}

static int wpa_cli_cmd_reconnect(int argc, char *argv[], char *reply)
{
    return wpa_ctrl_command("RECONNECT", reply);
}

#ifdef    CONFIG_SIMPLE_ROAMING
static int wpa_cli_cmd_roaming(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("ROAM", 0, argc, argv, reply);
}
static int wpa_cli_cmd_roam_threshold(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("ROAM_THRESHOLD", 1, argc, argv, reply);
}
#endif    /* CONFIG_SIMPLE_ROAMING */

static int wpa_cli_cmd_sta_autoconnect(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("STA_AUTOCONNECT", 1, argc, argv, reply);
}

#ifdef    CONFIG_WPS /* munchang.jung_20141108 */
static int wpa_cli_cmd_wps_pbc(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("WPS_PBC", 0, argc, argv, reply);
}

#ifdef    CONFIG_WPS_PIN
static int wpa_cli_cmd_wps_pin(int argc, char *argv[], char *reply)
{
    if (argc == 0) {
        printf("Need <BSSID(if necessary)>,<PIN>\n");
        return -1;
    }

    return wpa_cli_cmd("WPS_PIN", 1, argc, argv, reply);
}

static int wpa_cli_cmd_wps_cancel(int argc, char *argv[], char *reply)
{
    return wpa_ctrl_command("WPS_CANCEL", reply);
}
#endif    /* CONFIG_WPS_PIN */

#ifdef    CONFIG_AP
static int wpa_cli_cmd_wps_ap_pin(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("WPS_AP_PIN", 1, argc, argv, reply);
}
#endif    /* CONFIG_AP */

static int wpa_cli_cmd_wps_reg(int argc, char *argv[], char *reply)
{
    char cmd[256];
    int res;

    /* for Concurrent WPS */
    if (argc < 2) {
        printf("Need <BSSID> <AP PIN>\n");
        return -1;
    }

    if (argc == 2)
        res = snprintf(cmd, sizeof(cmd), "WPS_REG %s %s",
                  argv[0], argv[1]);
    else if (argc == 5 || argc == 6) {
        char ssid_hex[2 * 32 + 1];
        char key_hex[2 * 64 + 1];
        int i;

        ssid_hex[0] = '\0';
        for (i = 0; i < 32; i++) {
            if (argv[2][i] == '\0')
                break;
            snprintf(&ssid_hex[i * 2], 3, "%02x", argv[2][i]);
        }

        key_hex[0] = '\0';
        if (argc == 6) {
            for (i = 0; i < 64; i++) {
                if (argv[5][i] == '\0')
                    break;
                snprintf(&key_hex[i * 2], 3, "%02x",
                        argv[5][i]);
            }
        }

        res = snprintf(cmd, sizeof(cmd),
                  "WPS_REG %s %s %s %s %s %s",
                  argv[0], argv[1], ssid_hex, argv[3], argv[4],
                  key_hex);
    } else {
        ra6w1_cli_prt("Invalid WPS_REG command: need two arguments:\n"
                 "- BSSID of the target AP\n"
                 "- AP PIN\n");
        ra6w1_cli_prt("Alternatively, six arguments can be used to "
                 "reconfigure the AP:\n"
                 "- BSSID of the target AP\n"
                 "- AP PIN\n"
                 "- new SSID\n"
                 "- new auth (OPEN, WPAPSK, WPA2PSK)\n"
                 "- new encr (NONE, WEP, TKIP, CCMP)\n"
                 "- new key\n");
        return -1;
    }

    if (res < 0 || (size_t) res >= sizeof(cmd) - 1) {
        ra6w1_cli_prt("Too long WPS_REG command.\n");
        return -1;
    }
    return wpa_ctrl_command(cmd, reply);
}

static int wpa_cli_device_type(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("DEVICE_TYPE", 0, argc, argv, reply);
}

static int wpa_cli_device_name(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("DEVICE_NAME", 0, argc, argv, reply);
}

static int wpa_cli_model_name(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("MODEL_NAME", 0, argc, argv, reply);
}

static int wpa_cli_model_number(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("MODEL_NUMBER", 0, argc, argv, reply);
}

static int wpa_cli_manufacturer(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("MANUFACTURER", 0, argc, argv, reply);
}

static int wpa_cli_serial_number(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("SERIAL_NUMBER", 0, argc, argv, reply);
}

static int wpa_cli_config_methods(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("CONFIG_METHODS", 0, argc, argv, reply);
}

static int wpa_cli_cmd_set_config_default(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("CONFIG_DEFAULT", 0, argc, argv, reply);
}
#endif    /* CONFIG_WPS */

#ifdef    CONFIG_AP
static int wpa_cli_cmd_ap(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("AP", 1, argc, argv, reply);
}

static int wpa_cli_cmd_ap_chan_switch(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("AP_CHAN_SWITCH", 1, argc, argv, reply);
}

static int wpa_cli_cmd_ap_status(int argc, char *argv[], char *reply)
{
    if (wpa_ctrl_command("AP-STATUS", reply))
        return 0;
    return -1;
}

static int wpa_cli_cmd_sta(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("STA", 1, argc, argv, reply);
}

static int wpa_ctrl_command_sta(char *cmd, char *addr, size_t addr_len, char *reply)
{
    char *tmp_cli_rsp_buf;
#ifdef CONFIG_SCAN_REPLY_OPTIMIZE
    char *cli_rsp_buf_p = NULL;
#else
    char *cli_rsp_buf_p;
#endif /* CONFIG_SCAN_REPLY_OPTIMIZE */
    char *pos;
    size_t len;
    int ret;

#ifndef CONFIG_SCAN_REPLY_OPTIMIZE
    cli_rsp_buf_p = pvPortMalloc(CLI_RSP_SMALL_BUF_SIZE);

    if (cli_rsp_buf_p == NULL) {
        ra6w1_cli_prt("<%s> cli_rsp_buf_p alloc fail\n", __func__);
        return -1;
    }
#endif /* CONFIG_SCAN_REPLY_OPTIMIZE */

    len =  CLI_RSP_BUF_SIZE - 1;
#ifdef CONFIG_SCAN_REPLY_OPTIMIZE
    ret = wpa_ctrl_request(cmd, strlen(cmd), &cli_rsp_buf_p, &len, wpa_cli_msg_cb);
#else
    ret = wpa_ctrl_request(cmd, strlen(cmd), cli_rsp_buf_p, &len, wpa_cli_msg_cb);
#endif /* CONFIG_SCAN_REPLY_OPTIMIZE */

    if (ret == -2) {
        ra6w1_cli_prt("'%s' command timed out.\n", cmd);
#ifdef CONFIG_SCAN_REPLY_OPTIMIZE
        if (cli_rsp_buf_p) {
            vPortFree(cli_rsp_buf_p);
        }
#else
        vPortFree(cli_rsp_buf_p);
#endif /* CONFIG_SCAN_REPLY_OPTIMIZE */


        return -2;
    } else if (ret < 0) {
        ra6w1_cli_prt("'%s' command failed.\n", cmd);
#ifdef CONFIG_SCAN_REPLY_OPTIMIZE
        if (cli_rsp_buf_p) {
            vPortFree(cli_rsp_buf_p);
        }
#else
        vPortFree(cli_rsp_buf_p);
#endif    /* CONFIG_SCAN_REPLY_OPTIMIZE */

        return -1;
    } else if (len == 0) {
        ra6w1_cli_prt("'%s' command finished.\n", cmd);
#ifdef CONFIG_SCAN_REPLY_OPTIMIZE
        if (cli_rsp_buf_p) {
            vPortFree(cli_rsp_buf_p);
        }
#else
        vPortFree(cli_rsp_buf_p);
#endif /* CONFIG_SCAN_REPLY_OPTIMIZE */

        return -1;
    }

    cli_rsp_buf_p[len] = '\0';

    if (memcmp(cli_rsp_buf_p, "FAIL", 4) == 0) {
#ifdef CONFIG_SCAN_REPLY_OPTIMIZE
        if (cli_rsp_buf_p) {
            vPortFree(cli_rsp_buf_p);
        }
#else
        vPortFree(cli_rsp_buf_p);
#endif /* CONFIG_SCAN_REPLY_OPTIMIZE */

        return -1;
    }

    strncpy(reply, cli_rsp_buf_p, CLI_RSP_SMALL_BUF_SIZE);

    ra6w1_cli_prt("%s", cli_rsp_buf_p);

    tmp_cli_rsp_buf = pvPortMalloc(CLI_RSP_SMALL_BUF_SIZE);
    memset(tmp_cli_rsp_buf, 0, CLI_RSP_SMALL_BUF_SIZE);
    strcpy(tmp_cli_rsp_buf, cli_rsp_buf_p);
    pos = tmp_cli_rsp_buf;

    while (*pos != '\0' && *pos != '\n')
        pos++;
    *pos = '\0';

    strncpy(addr, tmp_cli_rsp_buf, addr_len);
#ifdef CONFIG_SCAN_REPLY_OPTIMIZE
    if (cli_rsp_buf_p) {
        vPortFree(cli_rsp_buf_p);
        vPortFree(tmp_cli_rsp_buf);
    }
#else
    vPortFree(cli_rsp_buf_p);
    vPortFree(tmp_cli_rsp_buf);
#endif /* CONFIG_SCAN_REPLY_OPTIMIZE */

    return 0;
}

static int wpa_cli_cmd_all_sta(int argc, char *argv[], char *reply)
{
    char addr[32], cmd[64];
    char *tmp_reply;
    unsigned char connected_sta = 0;
    unsigned char all_sta = 0;
    unsigned char all_flag = 0;

    if (argc == 1 && strncmp(argv[1],"all", 3))
    {
        all_flag = pdTRUE;
    }

    tmp_reply = pvPortMalloc(CLI_RSP_SMALL_BUF_SIZE);
    memset(tmp_reply, 0, CLI_RSP_SMALL_BUF_SIZE);

    if (wpa_ctrl_command_sta("STA-FIRST", addr, sizeof(addr), tmp_reply)) {
        strcpy(reply, "NOT_FOUND");

        if (tmp_reply)
        {
            vPortFree(tmp_reply);
        }
        return 0;
    }

    do {
        snprintf(cmd, sizeof(cmd), "STA-NEXT %s", addr);

        if (all_flag == pdFALSE && (!strstr(tmp_reply, "[AUTHORIZED]")
#ifdef __SUPPORT_MESH__
            && get_run_mode() != WIFI_DEVICE_MODE_EXT_MESH_POINT
            && get_run_mode() != WIFI_DEVICE_MODE_EXT_MESH_PORTAL
#endif /* __SUPPORT_MESH__ */
        ))
        {
            continue;
        }
        else
        {
            all_sta++;
            if (strstr(tmp_reply, "[AUTHORIZED]"))
            {
                connected_sta++;
            }
        }

        if ((strlen(reply) + strlen(tmp_reply)+1) <= CLI_RSP_BUF_SIZE) {
            strcat(reply, tmp_reply);
            strcat(reply, "\n");
        } else {
            printf("%s\n", tmp_reply);
        }
    } while (wpa_ctrl_command_sta(cmd, addr, sizeof(addr), tmp_reply) == 0);

    if (tmp_reply)
    {
        vPortFree(tmp_reply);
    }

    if ((connected_sta == 0 && all_flag == pdFALSE) || all_sta == 0) {
        strcpy(reply, "NOT_FOUND");
        return 0;
    } else {
        char num_tmp[26];
        if (all_flag == pdTRUE)
        {
            sprintf(num_tmp, "sta_count=%d\nall_sta=%d", connected_sta, all_sta);
        }
        else
        {
            sprintf(num_tmp, "sta_count=%d\n", connected_sta);
        }
        
        strcat(reply, num_tmp);
    }

    return -1;
}

static int wpa_cli_cmd_list_sta(int argc, char *argv[], char *reply)
{
    char addr[32], cmd[64];
    char *tmp_reply;

    tmp_reply = pvPortMalloc(CLI_RSP_SMALL_BUF_SIZE);
    memset(tmp_reply, 0, CLI_RSP_SMALL_BUF_SIZE);

    if (wpa_ctrl_command_sta("STA-FIRST", addr, sizeof(addr), tmp_reply)) {
        strcpy(reply, "NOT_FOUND");

        if (tmp_reply)
        {
            vPortFree(tmp_reply);
        }
        return 0;
    }

    do {
        snprintf(cmd, sizeof(cmd), "STA-NEXT %s", addr);

        if (os_strcmp(addr, "") != 0) {    
            strcat(reply, addr);
            strcat(reply, "\n");
        }

    } while (wpa_ctrl_command_sta(cmd, addr, sizeof(addr), tmp_reply) == 0);

    if (tmp_reply)
    {
        vPortFree(tmp_reply);
    }
    return -1;
}

#ifdef    CONFIG_AP_MANAGE_CLIENT
static int wpa_cli_cmd_deauthenticate(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("DEAUTHENTICATE", 1, argc, argv, reply);
}


static int wpa_cli_cmd_disassociate(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("DISASSOCIATE", 1, argc, argv, reply);
}
#endif    /* CONFIG_AP_MANAGE_CLIENT */

#ifdef    CONFIG_AP_WMM
static int wpa_cli_cmd_wmm_enabled(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("WMM_ENABLED", 0, argc, argv, reply);
}

static int wpa_cli_cmd_wmm_ps_enabled(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("WMM_PS_ENABLED", 0, argc, argv, reply);
}

static int wpa_cli_cmd_set_wmm_params(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("WMM_PARAMS", 0, argc, argv, reply);
}

static int wpa_cli_cmd_get_wmm_params(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("ALL_WMM", 0, argc, argv, reply);
}
#endif    /* CONFIG_AP_WMM */

#ifdef    CONFIG_ACL
static int wpa_cli_cmd_ap_acl_mac(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("ACL_MAC", 1, argc, argv, reply);
}

static int wpa_cli_cmd_ap_acl(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("ACL", 0, argc, argv, reply);
}
#endif    /* CONFIG_ACL */

static int wpa_cli_cmd_ap_max_inactivity(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("AP_MAX_INACTIVITY", 0, argc, argv, reply);
}

#ifdef    CONFIG_AP /* by Shingu 20161010 (Keep-alive) */
static int wpa_cli_cmd_ap_send_ka(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("AP_SEND_KEEP_ALIVE", 0, argc, argv, reply);
}
#endif    /* CONFIG_AP */

#ifdef __SUPPORT_SIGMA_TEST__
static int wpa_cli_cmd_addba_reject(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("ADDBA_REJECT", 0, argc, argv, reply);
}
#endif    /* __SUPPORT_SIGMA_TEST__ */

#ifdef    CONFIG_AP_PARAMETERS
static int wpa_cli_cmd_ap_rts(int argc, char *argv[], char *reply)
{
    if (argc > 1) {
        strcpy(reply, "FAIL");
        return -1;
    }
    return wpa_cli_cmd("AP_RTS", 0, argc, argv, reply);
}

static int wpa_cli_cmd_set_greenfield(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("GREENFIELD", 0, argc, argv, reply);
}

#ifdef __SUPPORT_SIGMA_TEST__
static int wpa_cli_cmd_ht_protection(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("HT_PROTECTION", 0, argc, argv, reply);
}
#endif    /* __SUPPORT_SIGMA_TEST__ */

#endif    /* CONFIG_AP_PARAMETERS */
#endif    /* CONFIG_AP */


#ifdef CONFIG_MESH
static int wpa_cli_cmd_mesh_interface_add(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("MESH_INTERFACE_ADD", 0, argc, argv, reply);
}

static int wpa_cli_cmd_mesh_group_add(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("MESH_GROUP_ADD", 1, argc, argv, reply);
}

static int wpa_cli_cmd_mesh_group_remove(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("MESH_GROUP_REMOVE", 1, argc, argv, reply);
}

static int wpa_cli_cmd_mesh_peer_remove(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("MESH_PEER_REMOVE", 1, argc, argv, reply);
}

static int wpa_cli_cmd_mesh_peer_add(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("MESH_PEER_ADD", 1, argc, argv, reply);
}

static int wpa_cli_cmd_mesh_all_sta_remove(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("MESH_ALL_STA_REMOVE", 0, argc, argv, reply);
}

static int wpa_cli_cmd_mesh_sta_remove(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("MESH_STA_REMOVE", 1, argc, argv, reply);
}
#endif /* CONFIG_MESH */

#ifdef    CONFIG_P2P
static int wpa_cli_cmd_p2p_find(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("P2P_FIND", 0, argc, argv, reply);
}

static int wpa_cli_cmd_p2p_stop_find(int argc, char *argv[], char *reply)
{
    return wpa_ctrl_command("P2P_STOP_FIND", reply);
}

static int wpa_cli_cmd_p2p_connect(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("P2P_CONNECT", 2, argc, argv, reply);
}

static int wpa_cli_cmd_p2p_group_remove(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("P2P_GROUP_REMOVE", 0, argc, argv, reply);
}

static int wpa_cli_cmd_p2p_group_add(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("P2P_GROUP_ADD", 0, argc, argv, reply);
}

static int wpa_ctrl_command_p2p_peer(char *cmd,
                     char *addr, size_t addr_len,
                     int discovered, char *reply)
{
#ifdef CONFIG_SCAN_REPLY_OPTIMIZE
    char *cli_rsp_buf_p = NULL;
#else
    char *cli_rsp_buf_p;
#endif /* CONFIG_SCAN_REPLY_OPTIMIZE */
    char *pos;
    size_t len;
    int ret;

#ifndef CONFIG_SCAN_REPLY_OPTIMIZE
    cli_rsp_buf_p = pvPortMalloc(CLI_RSP_BUF_SIZE);

    if (cli_rsp_buf_p == NULL) {
        ra6w1_cli_prt("<%s> cli_rsp_buf_p alloc fail\n", __func__);
        return -1;
    }
#endif /* CONFIG_SCAN_REPLY_OPTIMIZE */

    len = CLI_RSP_BUF_SIZE - 1;
#ifdef CONFIG_SCAN_REPLY_OPTIMIZE
    ret = wpa_ctrl_request(cmd, strlen(cmd), &cli_rsp_buf_p, &len,
                  wpa_cli_msg_cb);
#else
    ret = wpa_ctrl_request(cmd, strlen(cmd), cli_rsp_buf_p, &len,
                  wpa_cli_msg_cb);
#endif /* CONFIG_SCAN_REPLY_OPTIMIZE */

    if (ret == -2) {
        ra6w1_cli_prt("'%s' command timed out.\n", cmd);
#ifdef CONFIG_SCAN_REPLY_OPTIMIZE
        if (cli_rsp_buf_p) {
            vPortFree(cli_rsp_buf_p);
        }
#else
        vPortFree(cli_rsp_buf_p);
#endif /* CONFIG_SCAN_REPLY_OPTIMIZE */

        return -2;
    } else if (ret < 0) {
        ra6w1_cli_prt("'%s' command failed.\n", cmd);
#ifdef CONFIG_SCAN_REPLY_OPTIMIZE
        if (cli_rsp_buf_p) {
            vPortFree(cli_rsp_buf_p);
        }
#else
        vPortFree(cli_rsp_buf_p);
#endif /* CONFIG_SCAN_REPLY_OPTIMIZE */

        return -1;
    } else if (len == 0) {
        ra6w1_cli_prt("'%s' command finished.\n", cmd);
#ifdef CONFIG_SCAN_REPLY_OPTIMIZE
        if (cli_rsp_buf_p) {
            vPortFree(cli_rsp_buf_p);
        }
#else
        vPortFree(cli_rsp_buf_p);
#endif /* CONFIG_SCAN_REPLY_OPTIMIZE */

        return -1;
    }

    cli_rsp_buf_p[len] = '\0';

    if (memcmp(cli_rsp_buf_p, "FAIL", 4) == 0) {
#ifdef CONFIG_SCAN_REPLY_OPTIMIZE
        if (cli_rsp_buf_p) {
            vPortFree(cli_rsp_buf_p);
        }
#else
        vPortFree(cli_rsp_buf_p);
#endif /* CONFIG_SCAN_REPLY_OPTIMIZE */

        return -1;
    }

    strncpy(reply, cli_rsp_buf_p, CLI_RSP_SMALL_BUF_SIZE);

    ra6w1_cli_prt("%s", cli_rsp_buf_p);

    {
        char tmp_cli_rsp_buf[CLI_RSP_SMALL_BUF_SIZE];

        memset(tmp_cli_rsp_buf, 0, CLI_RSP_SMALL_BUF_SIZE);
        strcpy(tmp_cli_rsp_buf, cli_rsp_buf_p);
        pos = tmp_cli_rsp_buf;

        while (*pos != '\0' && *pos != '\n')
            pos++;
        *pos++ = '\0';

        strncpy(addr, tmp_cli_rsp_buf, addr_len);
#ifdef CONFIG_SCAN_REPLY_OPTIMIZE
        if (cli_rsp_buf_p) {
            vPortFree(cli_rsp_buf_p);
        }
#else
        vPortFree(cli_rsp_buf_p);
#endif /* CONFIG_SCAN_REPLY_OPTIMIZE */

        if (!discovered || strstr(pos, "[PROBE_REQ_ONLY]") == NULL)
            ra6w1_cli_prt("%s\n", addr);
    }

    return 0;
}

static int wpa_cli_cmd_p2p_peers(int argc, char *argv[], char *reply)
{
    char addr[32], cmd[64];
    int discovered;

    char *tmp_reply;

    tmp_reply = pvPortMalloc(CLI_RSP_SMALL_BUF_SIZE);
    memset(tmp_reply, 0, CLI_RSP_SMALL_BUF_SIZE);

    discovered = argc > 0 && strcmp(argv[0], "discovered") == 0;

    if (wpa_ctrl_command_p2p_peer("P2P_PEER FIRST", addr, sizeof(addr), discovered, tmp_reply)) {
        strcpy(reply, "NOT_FOUND");

        if (tmp_reply)
        {
            vPortFree(tmp_reply);
        }
        return -1;
    }

    do {
        snprintf(cmd, sizeof(cmd), "P2P_PEER NEXT-%s", addr);
        if ((strlen(reply) + strlen(tmp_reply)+1) <= CLI_RSP_BUF_SIZE) {
            strcat(reply, tmp_reply);
            strcat(reply,"\n");
        }
    } while (wpa_ctrl_command_p2p_peer(cmd, addr, sizeof(addr), discovered, tmp_reply) == 0);

    return 0;
}

static int wpa_cli_cmd_p2p_accept(int argc, char *argv[], char *reply)
{
    return wpa_ctrl_command("P2P_ACCEPT", reply);
}

static int wpa_cli_cmd_p2p_set(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("P2P_SET", 1, argc, argv, reply);
}

static int wpa_cli_cmd_p2p_get(int argc, char *argv[], char *reply)
{
    return wpa_ctrl_command("P2P_GET", reply);
}

#ifdef __SUPPORT_SIGMA_TEST__
static int wpa_cli_cmd_p2p_read_ssid_psk(int argc, char *argv[], char *reply)
{
    return wpa_ctrl_command("P2P_READ_SSID_PSK", reply);
}

static int wpa_cli_cmd_p2p_read_pin(int argc, char *argv[], char *reply)
{
    return wpa_ctrl_command("P2P_READ_PIN", reply);
}

static int wpa_cli_cmd_p2p_read_ip(int argc, char *argv[], char *reply)
{
    return wpa_ctrl_command("P2P_READ_IP", reply);
}

static int wpa_cli_cmd_p2p_read_gid(int argc, char *argv[], char *reply)
{
    return wpa_ctrl_command("P2P_READ_GID", reply);
}
#endif    /* __SUPPORT_SIGMA_TEST__ */

#ifdef    __WPA_CLI_FOR_TEST__
#ifdef  CONFIG_P2P_POWER_SAVE
static int wpa_cli_cmd_p2p_presence_req(int argc, char *argv[], char *reply)
{
    if (argc != 0 && argc != 2 && argc != 4) {
        return -1;
    }

    return wpa_cli_cmd("P2P_PRESENCE_REQ", 0, argc, argv, reply);
}
#endif  /* CONFIG_P2P_POWER_SAVE */

static int wpa_cli_cmd_p2p_dev_disc(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("P2P_DEVICE_DISCOVERABILITY", 2, argc, argv, reply);
}
#endif    /* __WPA_CLI_FOR_TEST__ */
#endif    /* CONFIG_P2P */

#ifdef    __WPA_CLI_FOR_TEST__
static int wpa_cli_cmd_retry_l(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("RETRY_L", 0, argc, argv, reply);
}
static int wpa_cli_cmd_retry_s(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("RETRY_S", 0, argc, argv, reply);
}
#endif    /* __WPA_CLI_FOR_TEST__ */

static int wpa_cli_cmd_ampdu(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("AMPDU", 1, argc, argv, reply);
}

static int wpa_cli_cmd_flush(int argc, char *argv[], char *reply)
{
    return wpa_ctrl_command("FLUSH", reply);
}

#ifdef    CONFIG_TWT
static int wpa_cli_cmd_twt_setup(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("TWT_SETUP", 0, argc, argv, reply);
}

static int wpa_cli_cmd_twt_teardown(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("TWT_TEARDOWN", 0, argc, argv, reply);
}
#endif    // CONFIG_TWT

#ifdef EAP_PEAP
static int wpa_cli_cmd_peap_ver(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("PEAP_VER", 0, argc, argv, reply);
}
#endif // EAP_PEAP

#ifdef CONFIG_INTERWORKING
static char ** wpa_cli_complete_bss(const char *str, int pos)
{
#if 0
    int arg = get_cmd_arg_num(str, pos);
    char **res = NULL;

    switch (arg) {
    case 1:
        res = cli_txt_list_array(&bsses);
        break;
    }

    return res;
#else
    return NULL;
#endif
}
#endif /* CONFIG_INTERWORKING */

#ifndef    CONFIG_IMMEDIATE_SCAN
static int wpa_cli_cmd_scan_results(int argc, char *argv[], char *reply)
{
    return wpa_ctrl_command("SCAN_RESULTS");
}

static int wpa_cli_cmd_bss(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("BSS", 1, argc, argv, reply);
}

static int wpa_cli_cmd_bss_expire_age(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("BSS_EXPIRE_AGE", 1, argc, argv, reply);
}

static int wpa_cli_cmd_bss_expire_count(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("BSS_EXPIRE_COUNT", 1, argc, argv, reply);
}

static int wpa_cli_cmd_bss_flush(int argc, char *argv[], char *reply)
{
    char cmd[256];
    int res;

    if (argc < 1)
        res = snprintf(cmd, sizeof(cmd), "BSS_FLUSH 0");
    else
        res = snprintf(cmd, sizeof(cmd), "BSS_FLUSH %s", argv[0]);
    if (res < 0 || (size_t) res >= sizeof(cmd) - 1) {
        printf("Too long BSS_FLUSH cmd\n");
        return -1;
    }
    return wpa_ctrl_command(cmd, reply);
}
#endif    /* CONFIG_IMMEDIATE_SCAN */

#ifdef    IEEE8021X_EAPOL /* munchang.jung_20141108 */
static int wpa_cli_cmd_logon(int argc, char *argv[], char *reply)
{
    return wpa_ctrl_command("LOGON", reply);
}

static int wpa_cli_cmd_logoff(int argc, char *argv[], char *reply)
{
    return wpa_ctrl_command("LOGOFF", reply);
}

static int wpa_cli_cmd_reauthenticate(int argc, char *argv[], char *reply)
{
    return wpa_ctrl_command("REAUTHENTICATE", reply);
}

#ifdef    CONFIG_INTERWORKING
static int wpa_cli_cmd_identity(int argc, char *argv[], char *reply)
{
    char cmd[256], *pos, *end;
    int i, ret;

    if (argc < 2) {
        printf("Invalid IDENTITY: needs two arguments <iface><identity>\n");
        return -1;
    }

    end = cmd + sizeof(cmd);
    pos = cmd;
    ret = snprintf(pos, end - pos, WPA_CTRL_RSP "IDENTITY-%s:%s",
              argv[0], argv[1]);
    if (ret < 0 || ret >= end - pos) {
        printf("Too long IDENTITY\n");
        return -1;
    }
    pos += ret;
    for (i = 2; i < argc; i++) {
        ret = snprintf(pos, end - pos, " %s", argv[i]);
        if (ret < 0 || ret >= end - pos) {
            printf("Too long IDENTITY\n");
            return -1;
        }
        pos += ret;
    }

    return wpa_ctrl_command(cmd, reply);
}

static int wpa_cli_cmd_password(int argc, char *argv[], char *reply)
{
    char cmd[256], *pos, *end;
    int i, ret;

    if (argc < 2) {
        printf("Invalid PASSWORD: needs two arguments <iface><password>\n");
        return -1;
    }

    end = cmd + sizeof(cmd);
    pos = cmd;
    ret = snprintf(pos, end - pos, WPA_CTRL_RSP "PASSWORD-%s:%s",
              argv[0], argv[1]);
    if (ret < 0 || ret >= end - pos) {
        printf("Too long PASSWORD.\n");
        return -1;
    }
    pos += ret;
    for (i = 2; i < argc; i++) {
        ret = snprintf(pos, end - pos, " %s", argv[i]);
        if (ret < 0 || ret >= end - pos) {
            printf("Too long PASSWORD.\n");
            return -1;
        }
        pos += ret;
    }

    return wpa_ctrl_command(cmd, reply);
}

static int wpa_cli_cmd_new_password(int argc, char *argv[], char *reply)
{
    char cmd[256], *pos, *end;
    int i, ret;

    if (argc < 2) {
        printf("Invalid NEW_PASSWORD : needs two arguments (iface and password)\n");
        return -1;
    }

    end = cmd + sizeof(cmd);
    pos = cmd;
    ret = snprintf(pos, end - pos, WPA_CTRL_RSP "NEW_PASSWORD-%s:%s",
              argv[0], argv[1]);
    if (ret < 0 || ret >= end - pos) {
        printf("Too long NEW_PASSWORD.\n");
        return -1;
    }
    pos += ret;
    for (i = 2; i < argc; i++) {
        ret = snprintf(pos, end - pos, " %s", argv[i]);
        if (ret < 0 || ret >= end - pos) {
            printf("Too long NEW_PASSWORD.\n");
            return -1;
        }
        pos += ret;
    }

    return wpa_ctrl_command(cmd, reply);
}

static int wpa_cli_cmd_pin(int argc, char *argv[], char *reply)
{
    char cmd[256], *pos, *end;
    int i, ret;

    if (argc < 2) {
        printf("Invalid PIN: needs two arguments <iface><pin>\n");
        return -1;
    }

    end = cmd + sizeof(cmd);
    pos = cmd;
    ret = snprintf(pos, end - pos, WPA_CTRL_RSP "PIN-%s:%s",
              argv[0], argv[1]);
    if (ret < 0 || ret >= end - pos) {
        printf("Too long PIN.\n");
        return -1;
    }
    pos += ret;
    for (i = 2; i < argc; i++) {
        ret = snprintf(pos, end - pos, " %s", argv[i]);
        if (ret < 0 || ret >= end - pos) {
            printf("Too long PIN.\n");
            return -1;
        }
        pos += ret;
    }
    return wpa_ctrl_command(cmd, reply);
}

static int wpa_cli_cmd_otp(int argc, char *argv[], char *reply)
{
    char cmd[256], *pos, *end;
    int i, ret;

    if (argc < 2) {
        printf("Invalid OTP: needs two arguments <iface><password>\n");
        return -1;
    }

    end = cmd + sizeof(cmd);
    pos = cmd;
    ret = snprintf(pos, end - pos, WPA_CTRL_RSP "OTP-%s:%s",
              argv[0], argv[1]);
    if (ret < 0 || ret >= end - pos) {
        printf("Too long OTP.\n");
        return -1;
    }
    pos += ret;
    for (i = 2; i < argc; i++) {
        ret = snprintf(pos, end - pos, " %s", argv[i]);
        if (ret < 0 || ret >= end - pos) {
            printf("Too long OTP.\n");
            return -1;
        }
        pos += ret;
    }

    return wpa_ctrl_command(cmd, reply);
}

static int wpa_cli_cmd_list_creds(int argc, char *argv[], char *reply)
{
    return wpa_ctrl_command("LIST_CREDS", reply);
}

static int wpa_cli_cmd_add_cred(int argc, char *argv[], char *reply)
{
    return wpa_ctrl_command("ADD_CRED", reply);
}

static int wpa_cli_cmd_remove_cred(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("REMOVE_CRED", 1, argc, argv, reply);
}

static int wpa_cli_cmd_set_cred(int argc, char *argv[], char *reply)
{
    if (argc != 3) {
        printf("Invalid SET_CRED : needs three arguments\n"
               "<cred id> <var_name> <value>\n");
        return -1;
    }

    return wpa_cli_cmd("SET_CRED", 3, argc, argv, reply);
}

static int wpa_cli_cmd_get_cred(int argc, char *argv[], char *reply)
{
    if (argc != 2) {
        printf("Invalid GET_CRED : needs two arguments\n"
               "<cred id> <var_name>\n");
        return -1;
    }

    return wpa_cli_cmd("GET_CRED", 2, argc, argv, reply);
}
#endif    /* CONFIG_INTERWORKING */
#endif    /* IEEE8021X_EAPOL */

#ifdef    CONFIG_WPS
#ifdef    CONFIG_WPS_ER
static int wpa_cli_cmd_wps_er_start(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("WPS_ER_START", 0, argc, argv, reply);
}

static int wpa_cli_cmd_wps_er_stop(int argc, char *argv[], char *reply)
{
    return wpa_ctrl_command("WPS_ER_STOP", reply);
}

static int wpa_cli_cmd_wps_er_pin(int argc, char *argv[], char *reply)
{
    if (argc < 2) {
        printf("Invalid WPS_ER_PIN : need at least two arguments:\n"
               "- UUID: use 'any' to select any\n"
               "- PIN: Enrollee PIN\n"
               "optional: - Enrollee MAC_addr\n");
        return -1;
    }

    return wpa_cli_cmd("WPS_ER_PIN", 2, argc, argv, reply);
}

static int wpa_cli_cmd_wps_er_pbc(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("WPS_ER_PBC", 1, argc, argv, reply);
}

static int wpa_cli_cmd_wps_er_learn(int argc, char *argv[], char *reply)
{
    if (argc != 2) {
        printf("Invalid WPS_ER_LEARN : need two arguments:\n"
               "- UUID: specify which AP to use\n"
               "- PIN: AP PIN\n");
        return -1;
    }

    return wpa_cli_cmd("WPS_ER_LEARN", 2, argc, argv, reply);
}

static int wpa_cli_cmd_wps_er_set_config(int argc, char *argv[], char *reply)
{
    if (argc != 2) {
        printf("Invalid WPS_ER_SET_CONFIG : need two "
               "arguments:\n"
               "- UUID: specify which AP to use\n"
               "- Network configuration id\n");
        return -1;
    }

    return wpa_cli_cmd("WPS_ER_SET_CONFIG", 2, argc, argv, reply);
}

static int wpa_cli_cmd_wps_er_config(int argc, char *argv[], char *reply)
{
    char cmd[256];
    int res;

    if (argc == 5 || argc == 6) {
        char ssid_hex[2 * 32 + 1];
        char key_hex[2 * 64 + 1];
        int i;

        ssid_hex[0] = '\0';
        for (i = 0; i < 32; i++) {
            if (argv[2][i] == '\0')
                break;
            snprintf(&ssid_hex[i * 2], 3, "%02x", argv[2][i]);
        }

        key_hex[0] = '\0';
        if (argc == 6) {
            for (i = 0; i < 64; i++) {
                if (argv[5][i] == '\0')
                    break;
                snprintf(&key_hex[i * 2], 3, "%02x",
                        argv[5][i]);
            }
        }

        res = snprintf(cmd, sizeof(cmd),
                  "WPS_ER_CONFIG %s %s %s %s %s %s",
                  argv[0], argv[1], ssid_hex, argv[3], argv[4],
                  key_hex);
    } else {
        printf("Invalid WPS_ER_CONFIG : need six arguments:\n"
               "- AP UUID\n"
               "- AP PIN\n"
               "- new SSID\n"
               "- new auth (OPEN, WPAPSK, WPA2PSK)\n"
               "- new encr (NONE, WEP, TKIP, CCMP)\n"
               "- new key\n");
        return -1;
    }

    if (res < 0 || (size_t) res >= sizeof(cmd) - 1) {
        printf("Too long WPS_ER_CONFIG.\n");
        return -1;
    }
    return wpa_ctrl_command(cmd, reply);
}
#endif    /* CONFIG_WPS_ER */
#endif    /* CONFIG_WPS */

#ifdef    CONFIG_AP
#ifdef    __FRAG_ENABLE__
static int wpa_cli_cmd_ap_frag(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("AP_FRAG", 0, argc, argv, reply);
}
#endif    /* __FRAG_ENABLE__ */

#ifdef    CONFIG_PRE_AUTH
static int wpa_cli_cmd_preauthenticate(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("PREAUTH", 1, argc, argv, reply);
}
#endif    /* CONFIG_PRE_AUTH */

#ifdef    CONFIG_PEERKEY
static int wpa_cli_cmd_stkstart(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("STKSTART", 1, argc, argv, reply);
}
#endif    /* CONFIG_PEERKEY */

#ifdef    CONFIG_IEEE80211R
static int wpa_cli_cmd_ft_ds(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("FT_DS", 1, argc, argv, reply);
}
#endif    /* CONFIG_IEEE80211R */
#endif    /* CONFIG_AP */

#ifdef    CONFIG_P2P
#ifdef    CONFIG_P2P_OPTION
static int wpa_cli_cmd_p2p_serv_disc_req(int argc, char *argv[], char *reply)
{
    char cmd[CLI_CMD_REQ_BUF_SIZE];

    if (argc != 2 && argc != 4) {
        printf("Invalid P2P_SERV_DISC_REQ : needs two "
               "arguments (address and TLVs) or four arguments "
               "(address, \"upnp\", version, search target "
               "(SSDP ST:)\n");
        return -1;
    }

    if (write_cmd(cmd, sizeof(cmd), "P2P_SERV_DISC_REQ", argc, argv) < 0)
        return -1;
    return wpa_ctrl_command(cmd, reply);
}

static int wpa_cli_cmd_p2p_serv_disc_cancel_req(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("P2P_SERV_DISC_CANCEL_REQ", 1, argc, argv, reply);
}

static int wpa_cli_cmd_p2p_serv_disc_resp(int argc, char *argv[], char *reply)
{
    char cmd[CLI_CMD_REQ_BUF_SIZE];
    int res;

    if (argc != 4) {
        printf("Invalid P2P_SERV_DISC_RESP : needs four "
               "arguments (freq, address, dialog token, and TLVs)\n");
        return -1;
    }

    res = snprintf(cmd, sizeof(cmd), "P2P_SERV_DISC_RESP %s %s %s %s",
              argv[0], argv[1], argv[2], argv[3]);
    if (res < 0 || (size_t) res >= sizeof(cmd))
        return -1;
    cmd[sizeof(cmd) - 1] = '\0';
    return wpa_ctrl_command(cmd);
}

static int wpa_cli_cmd_p2p_service_update(int argc, char *argv[], char *reply)
{
    return wpa_ctrl_command("P2P_SERVICE_UPDATE", reply);
}

static int wpa_cli_cmd_p2p_serv_disc_external(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("P2P_SERV_DISC_EXTERNAL", 1, argc, argv, reply);
}


static int wpa_cli_cmd_p2p_service_flush(int argc, char *argv[], char *reply)
{
    return wpa_ctrl_command("P2P_SERVICE_FLUSH", reply);
}


static int wpa_cli_cmd_p2p_service_add(int argc, char *argv[], char *reply)
{
    char cmd[CLI_CMD_REQ_BUF_SIZE];
    int res;

    if (argc != 3 && argc != 4) {
        printf("Invalid P2P_SERVICE_ADD command: needs three or four "
               "arguments\n");
        return -1;
    }

    if (argc == 4)
        res = snprintf(cmd, sizeof(cmd),
                  "P2P_SERVICE_ADD %s %s %s %s",
                  argv[0], argv[1], argv[2], argv[3]);
    else
        res = snprintf(cmd, sizeof(cmd),
                  "P2P_SERVICE_ADD %s %s %s",
                  argv[0], argv[1], argv[2]);
    if (res < 0 || (size_t) res >= sizeof(cmd))
        return -1;
    cmd[sizeof(cmd) - 1] = '\0';
    return wpa_ctrl_command(cmd, reply);
}

static int wpa_cli_cmd_p2p_service_del(int argc, char *argv[], char *reply)
{
    char cmd[CLI_CMD_REQ_BUF_SIZE];
    int res;

    if (argc != 2 && argc != 3) {
        printf("Invalid P2P_SERVICE_DEL : needs two or three "
               "arguments\n");
        return -1;
    }

    if (argc == 3)
        res = snprintf(cmd, sizeof(cmd), "P2P_SERVICE_DEL %s %s %s", argv[0], argv[1], argv[2]);
    else
        res = snprintf(cmd, sizeof(cmd), "P2P_SERVICE_DEL %s %s", argv[0], argv[1]);
    if (res < 0 || (size_t) res >= sizeof(cmd))
        return -1;
    cmd[sizeof(cmd) - 1] = '\0';
    return wpa_ctrl_command(cmd, reply);
}

static int wpa_cli_cmd_p2p_invite(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("P2P_INVITE", 1, argc, argv, reply);
}

static int wpa_cli_cmd_p2p_reject(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("P2P_REJECT", 1, argc, argv, reply);
}
#endif    /* CONFIG_P2P_OPTION */

#ifdef    CONFIG_P2P_UNUSED_CMD
static int wpa_cli_cmd_p2p_listen(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("P2P_LISTEN", 0, argc, argv, reply);
}

static int wpa_cli_cmd_p2p_prov_disc(int argc, char *argv[], char *reply)
{
    if (argc != 2 && argc != 3) {
        printf("Invalid P2P_PROV_DISC : needs at least "
               "two arguments, address and config method\n"
               "(display, keypad, or pbc) and an optional join\n");
        return -1;
    }

    return wpa_cli_cmd("P2P_PROV_DISC", 2, argc, argv, reply);
}

static int wpa_cli_cmd_p2p_results(int argc, char *argv[], char *reply)
{
    return wpa_ctrl_command("P2P_RESULTS");
}

static int wpa_cli_cmd_p2p_get_passphrase(int argc, char *argv[], char *reply)
{
    return wpa_ctrl_command("P2P_GET_PASSPHRASE");
}

static int wpa_cli_cmd_p2p_flush(int argc, char *argv[], char *reply)
{
    return wpa_ctrl_command("P2P_FLUSH", reply);
}

static int wpa_cli_cmd_p2p_unauthorize(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("P2P_UNAUTHORIZE", 1, argc, argv, reply);
}

static int wpa_cli_cmd_p2p_cancel(int argc, char *argv[], char *reply)
{
    return wpa_ctrl_command("P2P_CANCEL", reply);
}

static int wpa_cli_cmd_p2p_ext_listen(int argc, char *argv[], char *reply)
{
    if (argc != 0 && argc != 2) {
        printf("Invalid P2P_EXT_LISTEN : needs two arguments "
               "(availability period, availability interval; in "
               "millisecods).\n"
               "Extended Listen Timing can be cancelled with this "
               "command when used without parameters.\n");
        return -1;
    }

    return wpa_cli_cmd("P2P_EXT_LISTEN", 0, argc, argv, reply);
}

static int wpa_cli_cmd_p2p_remove_client(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("P2P_REMOVE_CLIENT", 1, argc, argv, reply);
}

static int wpa_cli_p2p_group_idle(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("P2P_GROUP_IDLE", 0, argc, argv, reply);
}
#endif    /* CONFIG_P2P_UNUSED_CMD */

#ifdef    CONFIG_WIFI_DISPLAY
static int wpa_cli_cmd_wfd_subelem_set(int argc, char *argv[], char *reply)
{
    char cmd[100];
    int res;

    if (argc != 1 && argc != 2) {
        printf("Invalid WFD_SUBELEM_SET : needs one or two "
               "arguments (subelem, hexdump)\n");
        return -1;
    }

    res = snprintf(cmd, sizeof(cmd), "WFD_SUBELEM_SET %s %s",
              argv[0], argc > 1 ? argv[1] : "");
    if (res < 0 || (size_t) res >= sizeof(cmd))
        return -1;
    cmd[sizeof(cmd) - 1] = '\0';
    return wpa_ctrl_command(cmd);
}


static int wpa_cli_cmd_wfd_subelem_get(int argc, char *argv[], char *reply)
{
    char cmd[100];
    int res;

    if (argc != 1) {
        printf("Invalid WFD_SUBELEM_GET : needs one argument (subelem)\n");
        return -1;
    }

    res = snprintf(cmd, sizeof(cmd), "WFD_SUBELEM_GET %s", argv[0]);
    if (res < 0 || (size_t) res >= sizeof(cmd))
        return -1;
    cmd[sizeof(cmd) - 1] = '\0';
    return wpa_ctrl_command(cmd);
}
#endif    /* CONFIG_WIFI_DISPLAY */
#endif    /* CONFIG_P2P */

#ifdef    CONFIG_IBSS_RSN /* munchang.jung_20141108 */
static int wpa_cli_cmd_ibss_rsn(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("IBSS_RSN", 1, argc, argv, reply);
}
#endif    /* CONFIG_IBSS_RSN */
#ifdef CONFIG_MBO
static int wpa_cli_cmd_non_pref_chan(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("SET non_pref_chan", 0, argc, argv, reply);
}

static int wpa_cli_cmd_mbo_cell_capa(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("SET mbo_cell_capa", 0, argc, argv, reply);
}

static int wpa_cli_cmd_disassoc_imminent_rssi_threshold(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("SET disassoc_imminent_rssi_threshold", 0, argc, argv, reply);
}

static int wpa_cli_cmd_oce(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("SET oce", 0, argc, argv, reply);
}
#endif /* CONFIG_MBO */
#ifdef    CONFIG_INTERWORKING
static int wpa_cli_cmd_fetch_anqp(int argc, char *argv[], char *reply)
{
    return wpa_ctrl_command("FETCH_ANQP", reply);
}

static int wpa_cli_cmd_stop_fetch_anqp(int argc, char *argv[], char *reply)
{
    return wpa_ctrl_command("STOP_FETCH_ANQP", reply);
}

static int wpa_cli_cmd_interworking_select(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("INTERWORKING_SELECT", 0, argc, argv, reply);
}

static int wpa_cli_cmd_interworking_connect(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("INTERWORKING_CONNECT", 1, argc, argv, reply);
}

static int wpa_cli_cmd_interworking_add_network(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("INTERWORKING_ADD_NETWORK", 1, argc, argv, reply);
}

static int wpa_cli_cmd_anqp_get(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("ANQP_GET", 2, argc, argv, reply);
}

#ifdef    CONFIG_GAS
static int wpa_cli_cmd_gas_request(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("GAS_REQUEST", 2, argc, argv, reply);
}

static int wpa_cli_cmd_gas_response_get(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("GAS_RESPONSE_GET", 2, argc, argv, reply);
}
#endif    /* CONFIG_GAS */
#endif    /* CONFIG_INTERWORKING */

#ifdef    CONFIG_AUTOSCAN
static int wpa_cli_cmd_autoscan(int argc, char *argv[], char *reply)
{
    if (argc == 0)
        return wpa_ctrl_command("AUTOSCAN ", reply);

    return wpa_cli_cmd("AUTOSCAN", 0, argc, argv, reply);
}
#endif    /* CONFIG_AUTOSCAN */

#ifdef    CONFIG_WNM_SLEEP_MODE
static int wpa_cli_cmd_wnm_sleep(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("WNM_SLEEP", 0, argc, argv, reply);
}
#endif    /* CONFIG_WNM_SLEEP_MODE */

#ifdef    CONFIG_WNM_BSS_TRANS_MGMT
static int wpa_cli_cmd_wnm_bss_query(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("WNM_BSS_QUERY", 1, argc, argv, reply);
}
#endif    /* CONFIG_WNM_BSS_TRANS_MGMT */

#ifdef    CONFIG_TESTING_OPTIONS
static int wpa_cli_cmd_printconf(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("PRINTCONF", 0, argc, argv, reply);
}
#endif    /* CONFIG_TESTING_OPTIONS */


static int wpa_cli_cmd_set(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("SET", 0, argc, argv, reply);
}

static int wpa_cli_cmd_get(int argc, char *argv[], char *reply)
{
    return wpa_cli_cmd("GET", 0, argc, argv, reply);
}

enum wpa_cli_cmd_flags {
    cli_cmd_flag_none            = 0x00,
    cli_cmd_flag_sensitive        = 0x01
};

struct wpa_cli_cmd {
    const char    *cmd;
    int        (*handler)(int argc, char *argv[], char *reply);
    char    ** (*completion)(const char *str, int pos);
    enum    wpa_cli_cmd_flags flags;
    const char    *usage;
};

static struct wpa_cli_cmd wpa_cli_commands[] = {
    { "help",                     wpa_cli_cmd_help,                     NULL,  cli_cmd_flag_none,        "[command] = show usage help"                                        },
/*[COMMON]****************************************************************/
    { "\n[COMMON]",    NULL,    NULL,    0, NULL },

    { "status",                   wpa_cli_cmd_status,                   NULL,  cli_cmd_flag_none,        "= get current status"                                               },
#ifdef    CONFIG_LOG_MASK
    { "set_log",                  wpa_cli_cmd_set_log,                  NULL,  cli_cmd_flag_sensitive,    "<notice|warn|err|fatal|all> <0/1> or \"default\"= disable/enable specified module"    },
    { "get_log",                  wpa_cli_cmd_get_log,                  NULL,  cli_cmd_flag_none,        "= get enabled log mask"                                             },
#endif    /* CONFIG_LOG_MASK */
#ifdef  __SUPPORT_WPA3_SAE__
    { "sae_groups",               wpa_cli_cmd_sae_groups,               NULL,  cli_cmd_flag_none,        "= config SAE groups"                                                },
#endif /* __SUPPORT_WPA3_SAE__ */
    { "save_config",              wpa_cli_cmd_save_config,              NULL,  cli_cmd_flag_none,        "= save the current profile"                                         },

/*[NETWORK PROFILES]*******************************************************/
    { "select_network",           wpa_cli_cmd_select_network,           NULL,  cli_cmd_flag_none,        "<if_id> = select a network (disable others)"                        },
#ifdef    CONFIG_P2P
    { "add_network",              wpa_cli_cmd_add_network,              NULL,  cli_cmd_flag_none,        "<if_id> = add a network [STA:0, AP/P2P:1] "                        },
#else
    { "add_network",              wpa_cli_cmd_add_network,              NULL,  cli_cmd_flag_none,        "<if_id> = add a network [STA:0/AP:1] "                              },
#endif    /* CONFIG_P2P */
    { "remove_network",           wpa_cli_cmd_remove_network,           NULL,  cli_cmd_flag_none,        "<if_id> = remove a network"                                         },
    { "set_network",              wpa_cli_cmd_set_network,              NULL,  cli_cmd_flag_sensitive,    "<if_id> <var> <value> = set network variables"                     },
    { "get_network",              wpa_cli_cmd_get_network,              NULL,  cli_cmd_flag_none,        "<if_id> <var> = get network variables"                              },
    { "list_networks",            wpa_cli_cmd_list_networks,            NULL,  cli_cmd_flag_none,        "= list configured networks"                                         },
    { "country",                  wpa_cli_country,                      NULL,  cli_cmd_flag_none,        "<Country code> = set country code"                                  },
    { "enable_network",           wpa_cli_cmd_enable_network,           NULL,  cli_cmd_flag_none,        "<if_id> = enable a network"                                         },
    { "disable_network",          wpa_cli_cmd_disable_network,          NULL,  cli_cmd_flag_none,        "<if_id> = disable a network"                                        },

/* [STATION] ****************************************************************/
    { "\n[STATION]",    NULL,    NULL,    0, NULL },

    { "scan",                     wpa_cli_cmd_scan,                     NULL,  cli_cmd_flag_none,        "= request new BSS scan"                                             },
    { "disconnect",               wpa_cli_cmd_disconnect,               NULL,  cli_cmd_flag_none,        "= disconn and wait for reassoc/reconn cmd before connecting"        },
    { "reassociate",              wpa_cli_cmd_reassociate,              NULL,  cli_cmd_flag_none,        "= force reassoc"                                                    },
    { "reconnect",                wpa_cli_cmd_reconnect,                NULL,  cli_cmd_flag_none,        "= like reassoc (When already disconnected)"                         },
#ifdef    CONFIG_SIMPLE_ROAMING
    { "roam",                     wpa_cli_cmd_roaming,                  NULL,  cli_cmd_flag_none,        "<run/stop> = run or stop roaming"                                   },
    { "roam_threshold",           wpa_cli_cmd_roam_threshold,           NULL,  cli_cmd_flag_none,        "<value> = set the roaming threshold RSSI"                           },
#endif    /* CONFIG_SIMPLE_ROAMING */

#ifndef    CONFIG_IMMEDIATE_SCAN
    { "scan_results",             wpa_cli_cmd_scan_results,             NULL,  cli_cmd_flag_none,        "= get latest scan results"                                          },
    { "bss",                      wpa_cli_cmd_bss,                      NULL,  cli_cmd_flag_none,        "[<idx> | <bssid>] = get detailed scan result info"                  },
    { "bss_expire_age",           wpa_cli_cmd_bss_expire_age,           NULL,  cli_cmd_flag_none,        "<value> = set BSS expiration age parameter"                         },
    { "bss_expire_count",         wpa_cli_cmd_bss_expire_count,         NULL,  cli_cmd_flag_none,        "<value> = set BSS expiration scan count parameter"                  },
    { "bss_flush",                wpa_cli_cmd_bss_flush,                NULL,  cli_cmd_flag_none,        "<value> = set BSS flush age (0 by default)"                         },
#endif    /* CONFIG_IMMEDIATE_SCAN */
    { "sta_autoconnect",          wpa_cli_cmd_sta_autoconnect,          NULL,  cli_cmd_flag_none,        "<0/1> = off/on auto-reconnect"                                      },

#ifdef    IEEE8021X_EAPOL
    { "logon",                    wpa_cli_cmd_logon,                    NULL,  cli_cmd_flag_none,        "= IEEE 802.1X EAPOL state machine logon"                            },
    { "logoff",                   wpa_cli_cmd_logoff,                   NULL,  cli_cmd_flag_none,        "= IEEE 802.1X EAPOL state machine logoff"                           },
    { "reauthenticate",           wpa_cli_cmd_reauthenticate,           NULL,  cli_cmd_flag_none,        "= trigger IEEE 802.1X/EAPOL reauthentication"                       },
#ifdef CONFIG_MBO
    { "non_pref_chan",            wpa_cli_cmd_non_pref_chan,            NULL,  cli_cmd_flag_none,      "non_pref_chan=<oper_class>:<chan>:<preference>:<reason> "             },
    { "mbo_cell_capa",            wpa_cli_cmd_mbo_cell_capa,            NULL,  cli_cmd_flag_none,      "mbo_cell_capa "          },
    { "disassoc_imminent_rssi_threshold", wpa_cli_cmd_disassoc_imminent_rssi_threshold,          NULL,   cli_cmd_flag_none,      "disassoc_imminent_rssi_threshold "          },
    { "oce",                      wpa_cli_cmd_oce,                      NULL,  cli_cmd_flag_none,      "oce "          },
#endif /* CONFIG_MBO */
#ifdef    CONFIG_INTERWORKING    
    { "identity",                 wpa_cli_cmd_identity,                 NULL,  cli_cmd_flag_none,        "<network id> <identity> = configure identity for an SSID"           },
    { "password",                 wpa_cli_cmd_password,                 NULL,  cli_cmd_flag_sensitive,    "<network id> <password> = configure password for an SSID"          },
    { "new_password",             wpa_cli_cmd_new_password,             NULL,  cli_cmd_flag_sensitive,    "<network id> <password> = change password for an SSID"             },
    { "pin",                      wpa_cli_cmd_pin,                      NULL,  cli_cmd_flag_sensitive,    "<network id> <pin> = configure pin for an SSID"                    },
    { "otp",                      wpa_cli_cmd_otp,                      NULL,  cli_cmd_flag_sensitive,    "<network id> <password> = configure one-time-password for an SSID" },
    { "list_creds",               wpa_cli_cmd_list_creds,               NULL,  cli_cmd_flag_none,        "= list configured credentials"                                      },
    { "add_cred",                 wpa_cli_cmd_add_cred,                 NULL,  cli_cmd_flag_none,        "= add a credential"                                                 },
    { "remove_cred",              wpa_cli_cmd_remove_cred,              NULL,  cli_cmd_flag_none,        "<cred id> = remove a credential"                                    },
    { "set_cred",                 wpa_cli_cmd_set_cred,                 NULL,  cli_cmd_flag_sensitive,    "<cred id> <variable> <value> = set credential variables"           },
    { "get_cred",                 wpa_cli_cmd_get_cred,                 NULL,  cli_cmd_flag_none,        "<cred id> <variable> = get credential variables"                    },
#endif    /* CONFIG_INTERWORKING */
#endif    /* IEEE8021X_EAPOL */

#ifdef    IEEE8021X_EAPOL
#ifdef CONFIG_ENTERPRISE
    { "pmksa",                    wpa_cli_cmd_pmksa,                    NULL,  cli_cmd_flag_none,        "= show PMKSA cache"                                                 },
    { "pmksa_flush",              wpa_cli_cmd_pmksa_flush,              NULL,  cli_cmd_flag_none,        "= flush PMKSA cache entries"                                        },
#endif // CONFIG_ENTERPRISE
#endif    // IEEE8021X_EAPOL
#ifdef CONFIG_PMKSA_CACHE_EXTERNAL
    { "pmksa_get",                wpa_cli_cmd_pmksa_get,                NULL,  cli_cmd_flag_none,        "<if_id = fetch all stored PMKSA cache entries"                      },
    { "pmksa_add",                wpa_cli_cmd_pmksa_add,                NULL,  cli_cmd_flag_sensitive,    "<if_id> <BSSID> <PMKID> <PMK> <reauth_time in seconds> <expiration in seconds> <akmp> <opportunistic> = store PMKSA cache entry from external storage"    },
#ifdef CONFIG_MESH
    { "mesh_pmksa_get",           wpa_cli_mesh_cmd_pmksa_get,           NULL,  cli_cmd_flag_none,        "<peer MAC address | any> = fetch all stored mesh PMKSA cache entries"    },
    { "mesh_pmksa_add",           wpa_cli_mesh_cmd_pmksa_add,           NULL,  cli_cmd_flag_sensitive,    "<BSSID> <PMKID> <PMK> <expiration in seconds> = store mesh PMKSA cache entry from external storage"    },
#endif /* CONFIG_MESH */
#endif /* CONFIG_PMKSA_CACHE_EXTERNAL */

/*[WPS]********************************************************************/
#ifdef    CONFIG_WPS
    { "\n[WPS]",    NULL,    NULL,    0, NULL },
    { "wps_pbc",                  wpa_cli_cmd_wps_pbc,                  NULL,  cli_cmd_flag_none,        "[BSSID] = start WPS : Push Button Configuration"                    },
#ifdef    CONFIG_WPS_PIN
    { "wps_pin",                  wpa_cli_cmd_wps_pin,                  NULL,  cli_cmd_flag_sensitive,    "<BSSID> [PIN] = start WPS PIN : Returns PIN"                       },
    { "wps_cancel",               wpa_cli_cmd_wps_cancel,               NULL,  cli_cmd_flag_none,        "Cancel the pending WPS operation"                                   },
#endif    /* CONFIG_WPS_PIN */
#ifdef CONFIG_WPS_NFC
    { "wps_nfc",                  wpa_cli_cmd_wps_nfc,                  wpa_cli_complete_bss,    cli_cmd_flag_none,    "[BSSID] = start Wi-Fi Protected Setup: NFC"           },
    { "wps_nfc_config_token",     wpa_cli_cmd_wps_nfc_config_token,     NULL,  cli_cmd_flag_none,        "<WPS|NDEF> = build configuration token"                             },
    { "wps_nfc_token",            wpa_cli_cmd_wps_nfc_token,            NULL,  cli_cmd_flag_none,        "<WPS|NDEF> = create password token"                                 },
    { "wps_nfc_tag_read",         wpa_cli_cmd_wps_nfc_tag_read,         NULL,  cli_cmd_flag_sensitive,    "<hexdump of payload> = report read NFC tag with WPS data"          },
    { "nfc_get_handover_req",     wpa_cli_cmd_nfc_get_handover_req,     NULL,  cli_cmd_flag_none,        "<NDEF> <WPS> = create NFC handover request"                         },
    { "nfc_get_handover_sel",     wpa_cli_cmd_nfc_get_handover_sel,     NULL,  cli_cmd_flag_none,        "<NDEF> <WPS> = create NFC handover select"                          },
    { "nfc_report_handover",      wpa_cli_cmd_nfc_report_handover,      NULL,  cli_cmd_flag_none,        "<role> <type> <hexdump of req> <hexdump of sel> = report completed NFC handover"    },
#endif /* CONFIG_WPS_NFC */

#ifdef    CONFIG_AP
    { "wps_ap_pin",               wpa_cli_cmd_wps_ap_pin,               NULL,  cli_cmd_flag_sensitive,    "[params..] = enable/disable AP PIN"                                },
#endif /* CONFIG_AP */
    { "wps_reg",                  wpa_cli_cmd_wps_reg,                  NULL,  cli_cmd_flag_sensitive,    "<BSSID> <AP PIN> = start WPS Registrar to configure an AP"         },
    { "device_type",              wpa_cli_device_type,                  NULL,  cli_cmd_flag_none,        "<Device type> = set device type (#-########-#)"                     },
    { "device_name",              wpa_cli_device_name,                  NULL,  cli_cmd_flag_none,        "<Device name> = set device name"                                    },
    { "model_name",               wpa_cli_model_name,                   NULL,  cli_cmd_flag_none,        "<Model name> = set model name"                                      },
      { "model_number",           wpa_cli_model_number,                 NULL,  cli_cmd_flag_none,        "<Model number> = set model number"                                  },
    { "manufacturer",             wpa_cli_manufacturer,                 NULL,  cli_cmd_flag_none,        "<Manufacturer> = set manufacturer"                                  },
    { "serial_number",            wpa_cli_serial_number,                NULL,  cli_cmd_flag_none,        "<Serial number> = set serial number"                                },
    { "config_methods",           wpa_cli_config_methods,               NULL,  cli_cmd_flag_none,        "<Config methods> = set config methods"                              },
    { "config_default",           wpa_cli_cmd_set_config_default,       NULL,  cli_cmd_flag_none,        "= set default configuration of WPS"                                 },
#ifdef    CONFIG_WPS_ER
    { "wps_er_start",             wpa_cli_cmd_wps_er_start,             NULL,  cli_cmd_flag_none,        "[IP address] = start Wi-Fi Protected Setup External Registrar"      },
    { "wps_er_stop",              wpa_cli_cmd_wps_er_stop,              NULL,  cli_cmd_flag_none,        "= stop Wi-Fi Protected Setup External Registrar"                    },
    { "wps_er_pin",               wpa_cli_cmd_wps_er_pin,               NULL,  cli_cmd_flag_sensitive,    "<UUID> <PIN> = add an Enrollee PIN to External Registrar"          },
    { "wps_er_pbc",               wpa_cli_cmd_wps_er_pbc,               NULL,  cli_cmd_flag_none,        "<UUID> = accept an Enrollee PBC using External Registrar"           },
    { "wps_er_learn",             wpa_cli_cmd_wps_er_learn,             NULL,  cli_cmd_flag_sensitive,    "<UUID> <PIN> = learn AP configuration"                             },
    { "wps_er_set_config",        wpa_cli_cmd_wps_er_set_config,        NULL,  cli_cmd_flag_none,        "<UUID> <network id> = set AP configuration for enrolling"           },
    { "wps_er_config",            wpa_cli_cmd_wps_er_config,            NULL,  cli_cmd_flag_sensitive,    "<UUID> <PIN> <SSID> <auth> <encr> <key> = configure AP"            },
#endif    /* CONFIG_WPS_ER */

#ifdef CONFIG_WPS_NFC
    { "wps_er_nfc_config_token",  wpa_cli_cmd_wps_er_nfc_config_token,  NULL,  cli_cmd_flag_none,        "<WPS/NDEF> <UUID> = build NFC configuration token"                  },
#endif /* CONFIG_WPS_NFC */
#endif    /* CONFIG_WPS */

#ifdef CONFIG_IBSS_RSN
    { "ibss_rsn",                 wpa_cli_cmd_ibss_rsn,                 NULL,  cli_cmd_flag_none,        "<addr> = request RSN auth. with <addr> in IBSS"                     },
#endif /* CONFIG_IBSS_RSN */

/*[Soft-AP]****************************************************************/
#ifdef CONFIG_AP
    { "\n[Soft-AP]",    NULL,    NULL,    0, NULL },

    { "ap",                       wpa_cli_cmd_ap,                       NULL,  cli_cmd_flag_none,        "<start|stop|restart> = Soft-AP mode"                                },
    { "ap_chan_switch",           wpa_cli_cmd_ap_chan_switch,           NULL,  cli_cmd_flag_none,        "<freq|channel> = switch Soft-AP operation channel"                  },
    { "ap_status",                wpa_cli_cmd_ap_status,                NULL,  cli_cmd_flag_none,        "= get AP status"                                                    },
    { "all_sta",                  wpa_cli_cmd_all_sta,                  NULL,  cli_cmd_flag_none,        "= get info. about all associated STAs"                              },
    { "list_sta",                 wpa_cli_cmd_list_sta,                 NULL,  cli_cmd_flag_none,        "= list all STAs (AP)"                                               },
    { "sta",                      wpa_cli_cmd_sta,                      NULL,  cli_cmd_flag_none,        "<addr> = get info. about an associated STA"                         },
#ifdef    CONFIG_AP_MANAGE_CLIENT
    { "deauthenticate",           wpa_cli_cmd_deauthenticate,           NULL,  cli_cmd_flag_none,        "<addr> = deauth. a STA"                                             },
    { "disassociate",             wpa_cli_cmd_disassociate,             NULL,  cli_cmd_flag_none,        "<addr> = disassoc. a STA"                                           },
#endif    /* CONFIG_AP_MANAGE_CLIENT */
#ifdef    CONFIG_AP_WMM
    { "wmm_enabled",              wpa_cli_cmd_wmm_enabled,              NULL,  cli_cmd_flag_none,        "<0/1> = disable/enable WMM"                                         },
    { "wmm_ps_enabled",           wpa_cli_cmd_wmm_ps_enabled,           NULL,  cli_cmd_flag_none,        "<0/1> = disable/enable WMM-PS"                                      },
    { "wmm_params",               wpa_cli_cmd_set_wmm_params,           NULL,  cli_cmd_flag_none,        "<ap|sta> <be/bk/vi/vo> <AIFS> <CWmin> <CWmax> <Burst(AP) or TxOP Limit(STA)> = set WMM params"    },
    { "all_wmm",                  wpa_cli_cmd_get_wmm_params,           NULL,  cli_cmd_flag_none,        "= read all WMM params"                                              },
#endif    /* CONFIG_AP_WMM */
#ifdef    CONFIG_ACL
    { "acl_mac",                  wpa_cli_cmd_ap_acl_mac,               NULL,  cli_cmd_flag_none,        "<addr> = Add ACL (Access Control List)"                             },
    { "acl",                      wpa_cli_cmd_ap_acl,                   NULL,  cli_cmd_flag_none,        "<allow|deny|clear|delete [addr]> = set ACL to allow/deny/clear"     },
#endif    /* CONFIG_ACL */
    { "ap_max_inactivity",        wpa_cli_cmd_ap_max_inactivity,        NULL,  cli_cmd_flag_none,        "<timeout> = set user inactivity timeout"                            },
#ifdef    CONFIG_AP
    { "ap_send_ka",               wpa_cli_cmd_ap_send_ka,               NULL,  cli_cmd_flag_none,        "<0/1> = disable/enable sending keep-alive"                          },
#endif    /* CONFIG_AP */
#ifdef __SUPPORT_SIGMA_TEST__
    { "addba_reject",             wpa_cli_cmd_addba_reject,             NULL,  cli_cmd_flag_none,        "<0/1> = 0: AMPDU RX Normal / 1: enable decline response"            },
#endif    /* __SUPPORT_SIGMA_TEST__ */
#ifdef    CONFIG_AP_PARAMETERS
    { "ap_rts",                   wpa_cli_cmd_ap_rts,                   NULL,  cli_cmd_flag_none,        "<threshold> set RTS threshold"                                      },
    { "greenfield",               wpa_cli_cmd_set_greenfield,           NULL,  cli_cmd_flag_none,        "<0/1> = off/on greenfield"                                          },
#ifdef __SUPPORT_SIGMA_TEST__
    { "ht_protection",            wpa_cli_cmd_ht_protection,            NULL,  cli_cmd_flag_none,        "<0/1> = off/on HT protection"                                       },
#endif    /* __SUPPORT_SIGMA_TEST__ */
#endif    /* CONFIG_AP_PARAMETERS */
#ifdef    __FRAG_ENABLE__
    { "ap_frag",                  wpa_cli_cmd_ap_frag,                  NULL,  cli_cmd_flag_none,        "<threshold> = set fragmentation threshold"                          },
#endif    /* __FRAG_ENABLE__ */
#ifdef    CONFIG_PRE_AUTH
    { "preauthenticate",          wpa_cli_cmd_preauthenticate,          NULL,  cli_cmd_flag_none,        "<BSSID> = force preauth."                                           },
#endif    /* CONFIG_PRE_AUTH */
#ifdef    CONFIG_IEEE80211R
    { "ft_ds",                    wpa_cli_cmd_ft_ds,                    NULL,  cli_cmd_flag_none,        "<addr> = request over-the-DS FT with <addr>"                        },
#endif    /* CONFIG_IEEE80211R */

#endif /* CONFIG_AP */
#ifdef CONFIG_TESTING_OPTIONS
    { "drop_sa",                  wpa_cli_cmd_drop_sa,                  NULL,  cli_cmd_flag_none,        "= drop SA without deauth/disassoc (test command)"                   },
#endif /* CONFIG_TESTING_OPTIONS */

/*[MESH]****************************************************************/
#ifdef CONFIG_MESH
    { "\n[MESH]" },
    { "mesh_interface_add",       wpa_cli_cmd_mesh_interface_add,       NULL,  cli_cmd_flag_none,        "[ifname] = Create a new mesh interface"                             },
    { "mesh_group_add",           wpa_cli_cmd_mesh_group_add,           NULL,  cli_cmd_flag_none,        "<network id> = join a mesh network (disable others)"                },
    { "mesh_group_remove",        wpa_cli_cmd_mesh_group_remove,        NULL,  cli_cmd_flag_none,        "<ifname> = Remove mesh group interface"                             },
    { "mesh_peer_remove",         wpa_cli_cmd_mesh_peer_remove,         NULL,  cli_cmd_flag_none,        "<addr> = Remove a mesh peer"                                        },
    { "mesh_peer_add",            wpa_cli_cmd_mesh_peer_add,            NULL,  cli_cmd_flag_none,        "<addr> [duration=<seconds>] = Add a mesh peer"                      },
    { "mesh_all_sta_remove",      wpa_cli_cmd_mesh_all_sta_remove,      NULL,  cli_cmd_flag_none,        "= Remove mesh all sta"                                              },
    { "mesh_sta_remove",          wpa_cli_cmd_mesh_all_sta_remove,      NULL,  cli_cmd_flag_none,        "<addr> = Remove a mesh sta"                                         },    
#endif /* CONFIG_MESH */

/* [P2P] ****************************************************************/
#ifdef    CONFIG_P2P
    { "\n[P2P]",    NULL,    NULL,    0, NULL },

    { "p2p_find",                 wpa_cli_cmd_p2p_find,                 NULL,  cli_cmd_flag_none,        "= find P2P Devices for up-to timeout seconds"                       },
    { "p2p_stop_find",            wpa_cli_cmd_p2p_stop_find,            NULL,  cli_cmd_flag_none,        "= stop P2P Devices search"                                          },
    { "p2p_connect",              wpa_cli_cmd_p2p_connect,              NULL,  cli_cmd_flag_none,        "<addr> <\"pbc\"|PIN> = connect to a P2P Device"                     },
    { "p2p_group_remove",         wpa_cli_cmd_p2p_group_remove,         NULL,  cli_cmd_flag_none,        "= remove P2P group interface (terminate group if GO)"               },
    { "p2p_group_add",            wpa_cli_cmd_p2p_group_add,            NULL,  cli_cmd_flag_none,        "= add a new P2P group (local end as GO)"                            },
    { "p2p_peers",                wpa_cli_cmd_p2p_peers,                NULL,  cli_cmd_flag_none,        "= list known (optionally, only fully discovered) P2P peers"         },
    { "p2p_accept",               wpa_cli_cmd_p2p_accept,               NULL,  cli_cmd_flag_none,        "= connect to a P2P Device who requests lastly"                      },
    { "p2p_set",                  wpa_cli_cmd_p2p_set,                  NULL,  cli_cmd_flag_none,        "<field> <value> = set a P2P parameter"                              },
    { "p2p_get",                  wpa_cli_cmd_p2p_get,                  NULL,  cli_cmd_flag_none,        "= get P2P parameters"                                               },
#ifdef    CONFIG_P2P_OPTION
    { "p2p_serv_disc_req",        wpa_cli_cmd_p2p_serv_disc_req,        NULL,  cli_cmd_flag_none,        "<addr> <TLVs> = schedule service discovery request"                 },
    { "p2p_serv_disc_cancel_req", wpa_cli_cmd_p2p_serv_disc_cancel_req, NULL,  cli_cmd_flag_none,    "<id> = cancel pending service discovery request"                        },
    { "p2p_serv_disc_resp",       wpa_cli_cmd_p2p_serv_disc_resp,       NULL,  cli_cmd_flag_none,        "<freq> <addr> <dialog token> <TLVs> = service discovery response"   },
    { "p2p_service_update",       wpa_cli_cmd_p2p_service_update,       NULL,  cli_cmd_flag_none,        "= indicate change in local services"                                },
    { "p2p_serv_disc_external",   wpa_cli_cmd_p2p_serv_disc_external,   NULL,  cli_cmd_flag_none,        "<external> = set external processing of service discovery"          },
    { "p2p_service_flush",        wpa_cli_cmd_p2p_service_flush,        NULL,  cli_cmd_flag_none,        "= remove all stored service entries"                                },
    { "p2p_service_add",          wpa_cli_cmd_p2p_service_add,          NULL,  cli_cmd_flag_none,        "<bonjour|upnp> <query|version> <response|service> = add a local service" },
    { "p2p_service_del",          wpa_cli_cmd_p2p_service_del,          NULL,  cli_cmd_flag_none,        "<bonjour|upnp> <query|version> [|service] = remove a local service" },
    { "p2p_invite",               wpa_cli_cmd_p2p_invite,               NULL,  cli_cmd_flag_none,        "<cmd> <peer=addr> = invite peer"                                    },
    { "p2p_reject",               wpa_cli_cmd_p2p_reject,               NULL,  cli_cmd_flag_none,        "<addr> = reject connection atWPA_CLI Optimizets from a specific peer"    },
#endif    /* CONFIG_P2P_OPTION */
#ifdef    CONFIG_WIFI_DISPLAY
    { "wfd_subelem_set",          wpa_cli_cmd_wfd_subelem_set,          NULL,  cli_cmd_flag_none,        "<subelem> [contents] = set Wi-Fi Display subelement"                },
    { "wfd_subelem_get",          wpa_cli_cmd_wfd_subelem_get,          NULL,  cli_cmd_flag_none,        "<subelem> = get Wi-Fi Display subelement"                           },
#endif    /* CONFIG_WIFI_DISPLAY */

#ifdef    CONFIG_P2P_UNUSED_CMD
    { "p2p_listen",               wpa_cli_cmd_p2p_listen,               NULL,  cli_cmd_flag_none,        "<timeout> = listen for P2P Devices for up-to timeout seconds"       },
    { "p2p_prov_disc",            wpa_cli_cmd_p2p_prov_disc,            NULL,  cli_cmd_flag_none,        "<addr> <method> = request provisioning discovery"                   },
    { "p2p_results",              wpa_cli_cmd_p2p_results,              NULL,  cli_cmd_flag_none,        "= get latest scan results of p2p_find"                              },
    { "p2p_get_passphrase",       wpa_cli_cmd_p2p_get_passphrase,       NULL,  cli_cmd_flag_none,        "= get the passphrase for a group (GO only)"                         },
    { "p2p_flush",                wpa_cli_cmd_p2p_flush,                NULL,  cli_cmd_flag_none,        "= flush P2P state"                                                  },
    { "p2p_unauthorize",          wpa_cli_cmd_p2p_unauthorize,          NULL,  cli_cmd_flag_none,        "<addr> = unauthorize a peer"                                        },
    { "p2p_cancel",               wpa_cli_cmd_p2p_cancel,               NULL,  cli_cmd_flag_none,        "= cancel P2P group formation"                                       },
    { "p2p_ext_listen",           wpa_cli_cmd_p2p_ext_listen,           NULL,  cli_cmd_flag_none,        "<period> <interval> = set extended listen timing"                   },
    { "p2p_remove_client",        wpa_cli_cmd_p2p_remove_client,        NULL,  cli_cmd_flag_none,        "<address|iface=address> = remove a peer from all groups"            },
    { "p2p_group_idle",           wpa_cli_p2p_group_idle,               NULL,  cli_cmd_flag_none,        "<idle time> = p2p group idle time"                                  },
#endif    /* CONFIG_P2P_UNUSED_CMD */

#ifdef __SUPPORT_SIGMA_TEST__
    { "p2p_read_ap",              wpa_cli_cmd_p2p_read_ssid_psk,        NULL,  cli_cmd_flag_none,        "= send/get SSID & Passphrase to UART for SIGMA test"                },
    { "p2p_read_pin",             wpa_cli_cmd_p2p_read_pin,             NULL,  cli_cmd_flag_none,        "= send/get PIN to UART for SIGMA test"                              },
    { "p2p_read_ip",              wpa_cli_cmd_p2p_read_ip,              NULL,  cli_cmd_flag_none,        "= send/get IP/NETMASK/DNS to UART for SIGMA test"                   },
    { "p2p_read_gid",             wpa_cli_cmd_p2p_read_gid,             NULL,  cli_cmd_flag_none,        "= send/get Group ID (MAC Address & SSID) to UART for SIGMA test"    },
#endif    /* __SUPPORT_SIGMA_TEST__ */

#ifdef    __WPA_CLI_FOR_TEST__
#ifdef  CONFIG_P2P_POWER_SAVE
    { "p2p_presence_req",         wpa_cli_cmd_p2p_presence_req,         NULL,  cli_cmd_flag_none,        "[<duration> <interval>]... = request GO presence"                   },
#endif  /* CONFIG_P2P_POWER_SAVE */
    { "p2p_dev_discover_ability", wpa_cli_cmd_p2p_dev_disc,             NULL,  cli_cmd_flag_none,        "<GO addr> <Client addr> = Send a Device Discoverability for test"   },
#endif    /* __WPA_CLI_FOR_TEST__ */

#endif    /* CONFIG_P2P */


/* [ETC] ****************************************************************/
    { "\n[ETC]",    NULL,    NULL,    0, NULL },

    { "ampdu",                     wpa_cli_cmd_ampdu,                   NULL,  cli_cmd_flag_none,        "<tx|rx> <0/1> = off/on AMPDU Tx or Rx"                              },
    { "flush",                     wpa_cli_cmd_flush,                   NULL,  cli_cmd_flag_none,        "= flush supplicant state"                                           },

#ifdef    CONFIG_TWT
    { "twt_setup",                 wpa_cli_cmd_twt_setup,               NULL,  cli_cmd_flag_none,        "[dialog=<token>] [exponent=<exponent>] [mantissa=<mantissa>] [min_twt=<Min TWT>] [setup_cmd=<setup-cmd>] [twt=<u64>] [requestor=0|1] [trigger=0|1] [implicit=0|1] [flow_type=0|1] [flow_id=<3-bit-id>] [protection=0|1] [twt_channel=<twt chanel id>] [control=<control-u8>] = Send TWT Setup frame" },
    { "twt_teardown",              wpa_cli_cmd_twt_teardown,            NULL,  cli_cmd_flag_none,        "[flags=<value>] = Send TWT Teardown frame"                          },
#endif    // CONFIG_TWT

#ifdef CONFIG_ENTERPRISE
#ifdef EAP_PEAP
    { "peap_ver",                  wpa_cli_cmd_peap_ver,                NULL,  cli_cmd_flag_none,        "<0/1> = set PEAP Version"                                           },
#endif // EAP_PEAP
#endif // CONFIG_ENTERPRISE
#ifdef    CONFIG_IBSS_RSN
    { "ibss_rsn",                  wpa_cli_cmd_ibss_rsn,                NULL,  cli_cmd_flag_none,        "<addr> = request RSN auth. with <addr> in IBSS"                     },
#endif    /* CONFIG_IBSS_RSN */
#ifdef    CONFIG_INTERWORKING
    { "fetch_anqp",                wpa_cli_cmd_fetch_anqp,              NULL,  cli_cmd_flag_none,        "= fetch ANQP info for all APs"                                      },
    { "stop_fetch_anqp",           wpa_cli_cmd_stop_fetch_anqp,         NULL,  cli_cmd_flag_none,        "= stop fetch_anqp operation"                                        },
    { "interworking_select",       wpa_cli_cmd_interworking_select,     NULL,  cli_cmd_flag_none,        "[auto] = perform Interworking network selection"                    },
    { "interworking_connect",      wpa_cli_cmd_interworking_connect,    NULL,  cli_cmd_flag_none,        "<BSSID> = connect using Interworking credentials"                   },
    { "interworking_add_network",  wpa_cli_cmd_interworking_add_network,wpa_cli_complete_bss,    cli_cmd_flag_none,    "<BSSID> = connect using Interworking credentials"     },
    { "anqp_get",                  wpa_cli_cmd_anqp_get,                NULL,  cli_cmd_flag_none,        "<addr> <info id>[,<info id>]... = request ANQP information"         },
#ifdef    CONFIG_GAS
    { "gas_request",               wpa_cli_cmd_gas_request,             NULL,  cli_cmd_flag_none,        "<addr> <AdvProtoID> [QueryReq] = GAS request"                       },
    { "gas_response_get",          wpa_cli_cmd_gas_response_get,        NULL,  cli_cmd_flag_none,        "<addr> <dialog token> [start,len] = Fetch last GAS response"        },
#endif    /* CONFIG_GAS */
#endif /* CONFIG_INTERWORKING */
#ifdef CONFIG_HS20
    { "\n[HS20]" },
    { "hs20_anqp_get",             wpa_cli_cmd_hs20_anqp_get,           wpa_cli_complete_bss,    cli_cmd_flag_none,    "<addr> <subtype>[,<subtype>]... = request HS 2.0 ANQP information"    },
    { "nai_home_realm_list",       wpa_cli_cmd_get_nai_home_realm_list, wpa_cli_complete_bss,    cli_cmd_flag_none,    "<addr> <home realm> = get HS20 nai home realm list"   },
    { "hs20_icon_request",         wpa_cli_cmd_hs20_icon_request,       wpa_cli_complete_bss,    cli_cmd_flag_none,    "<addr> <icon name> = get Hotspot 2.0 OSU icon"        },
    { "fetch_osu",                 wpa_cli_cmd_fetch_osu,               NULL,  cli_cmd_flag_none,        "= fetch OSU provider information from all APs"                      },
    { "cancel_fetch_osu",          wpa_cli_cmd_cancel_fetch_osu    ,    NULL,  cli_cmd_flag_none,        "= cancel fetch_osu command"                                         },
#endif /* CONFIG_HS20 */

#ifdef CONFIG_AUTOSCAN
    { "autoscan",                  wpa_cli_cmd_autoscan,                NULL,  cli_cmd_flag_none,        "[params] = Set or unset (if none) autoscan parameters"              },
#endif /* CONFIG_AUTOSCAN */
#ifdef CONFIG_WNM
#ifdef    CONFIG_WNM_SLEEP_MODE
    { "wnm_sleep",                 wpa_cli_cmd_wnm_sleep,               NULL,  cli_cmd_flag_none,        "<enter/exit> [interval=#] = enter/exit WNM-Sleep mode"              },
#endif    /* CONFIG_WNM_SLEEP_MODE */
#ifdef    CONFIG_WNM_BSS_TRANS_MGMT
    { "wnm_bss_query",             wpa_cli_cmd_wnm_bss_query,           NULL,  cli_cmd_flag_none,        "<query reason> [list] [neighbor=<BSSID>,<BSSID information>,<operating class>,<channel number>,<PHY type>[,<hexdump of optional subelements>] = Send BSS Transition Management Query"    },
#endif    /* CONFIG_WNM_BSS_TRANS_MGMT */
#endif /* CONFIG_WNM */

#ifdef CONFIG_DPP
    { "\n[DPP]" },
    { "dpp_qr_code",               wpa_cli_cmd_dpp_qr_code,             NULL,  cli_cmd_flag_none,        "report a scanned DPP URI from a QR Code"                            },
    { "dpp_bootstrap_gen",         wpa_cli_cmd_dpp_bootstrap_gen,       NULL,  cli_cmd_flag_sensitive,    "type=<qrcode> [chan=..] [mac=..] [info=..] [curve=..] [key=..] = generate DPP bootstrap information"    },
    { "dpp_bootstrap_remove",      wpa_cli_cmd_dpp_bootstrap_remove,    NULL,  cli_cmd_flag_none,        "*|<id> = remove DPP bootstrap information"                          },
    { "dpp_bootstrap_get_uri",     wpa_cli_cmd_dpp_bootstrap_get_uri,   NULL,  cli_cmd_flag_none,        "<id> = get DPP bootstrap URI"                                       },
    { "dpp_bootstrap_info",        wpa_cli_cmd_dpp_bootstrap_info,      NULL,  cli_cmd_flag_none,        "<id> = show DPP bootstrap information"                              },
    { "dpp_auth_init",             wpa_cli_cmd_dpp_auth_init,           NULL,  cli_cmd_flag_none,        "peer=<id> [own=<id>] = initiate DPP bootstrapping"                  },
    { "dpp_listen",                wpa_cli_cmd_dpp_listen,              NULL,  cli_cmd_flag_none,        "<freq in MHz> = start DPP listen"                                   },
    { "dpp_stop_listen",           wpa_cli_cmd_dpp_stop_listen,         NULL,  cli_cmd_flag_none,        "= stop DPP listen"                                                  },
    { "dpp_configurator_add",      wpa_cli_cmd_dpp_configurator_add,    NULL,  cli_cmd_flag_sensitive,    "[curve=..] [key=..] = add DPP configurator"                        },
    { "dpp_configurator_remove",   wpa_cli_cmd_dpp_configurator_remove, NULL,  cli_cmd_flag_none,        "*|<id> = remove DPP configurator"                                   },
    { "dpp_configurator_get_key",  wpa_cli_cmd_dpp_configurator_get_key,NULL,  cli_cmd_flag_none,        "<id> = Get DPP configurator's private key"                          },
    { "dpp_pkex_add",              wpa_cli_cmd_dpp_pkex_add,            NULL,  cli_cmd_flag_sensitive,   "add PKEX code"                                                      },
    { "dpp_pkex_remove",           wpa_cli_cmd_dpp_pkex_remove,         NULL,  cli_cmd_flag_none,        "*|<id> = remove DPP pkex information"                               },
#endif /* CONFIG_DPP */
#ifdef    CONFIG_TESTING_OPTIONS
    { "printconf",                 wpa_cli_cmd_printconf,               NULL,  cli_cmd_flag_none,        "= read WPS environment"                                             },
#endif    /* CONFIG_TESTING_OPTIONS */
    { "set",                       wpa_cli_cmd_set,                     NULL,  cli_cmd_flag_none,        "set variables"                                                      },
    { "get",                       wpa_cli_cmd_get,                     NULL,  cli_cmd_flag_none,        "get variables"                                                      },
#ifdef    __WPA_CLI_FOR_TEST__
    { "retry_long",                wpa_cli_cmd_retry_l,                 NULL,  cli_cmd_flag_none,        "<value> = set retry long value"                                     },
    { "retry_short",               wpa_cli_cmd_retry_s,                 NULL,  cli_cmd_flag_none,        "<value> = set retry short value"                                    },
#endif    /* __WPA_CLI_FOR_TEST__ */

    { NULL,    NULL,    NULL,      cli_cmd_flag_none,    NULL }
};


/*
 * Prints command usage, lines are padded with the specified string.
 */
static void print_cmd_help(struct wpa_cli_cmd *cmd, const char *pad)
{
    printf("%s%s %s\n", pad, cmd->cmd, ((cmd->usage != NULL) ? cmd->usage:""));
}


static void print_help(const char *cmd)
{
    int n;
    printf("\ncommands:\n");
    for (n = 0; wpa_cli_commands[n].cmd; n++) {
        if (cmd == NULL || str_starts(wpa_cli_commands[n].cmd, cmd))
            print_cmd_help(&wpa_cli_commands[n], "  ");
        vTaskDelay(1);
    }
}


static int wpa_request(int argc, char *argv[], char* reply)
{
    int ret = 0;
    struct wpa_cli_cmd *cmd, *match = NULL;
    int count;

    ra6w1_cli_prt("<%s> START : [%s] \n", __func__, argv[0]);

    if (dbg_wpa_cli) {
        printf("[%s] argc=%d ", __func__, argc);
        for (int ii = 0; ii < argc;) {
            printf("%s ", argv[ii]);
            ii++;
        }
        printf("\n");
    }

    count = 0;
    cmd = wpa_cli_commands;

    while (cmd->cmd) {
        if (os_strncasecmp(cmd->cmd, argv[0], strlen(argv[0])) == 0) {
            match = cmd;
            if (os_strcasecmp(cmd->cmd, argv[0]) == 0) {
                /* we have an exact match */
                count = 1;
                break;
            }
            count++;
        }
        cmd++;
    }

    if (count > 1) {
        printf("\tAmbiguous cmd '%s';\n", argv[0]);

        cmd = wpa_cli_commands;

        while (cmd->cmd) {
            if (os_strncasecmp(cmd->cmd, argv[0],
                       strlen(argv[0])) == 0) {
                printf("\t%s\n", cmd->cmd);
            }
            cmd++;
        }
        printf("\n");
        ret = 1;
    } else if (count == 0) {
        printf("\tUnknown command: %s\n", argv[0]);
        ret = 1;
    } else {
        ret = match->handler(argc - 1, &argv[1], reply);        // wpa_cli_cmd_scan : case of scan
    }

    return ret;
}

#define SUB_STRING_SIZE 512
static void printf_sub_strings(const char *str, size_t sub_size) {
    size_t i;
    size_t len = strlen(str);

    for (i = 0; i < len; i += sub_size) {
        printf("%.*s", (int)sub_size, str + i);
    }
}

static int cc_set_network(char *cmd, int iface)
{
    char return_str[16] = {0, };
    int ret = ra6w1_cli_reply(cmd, CLI_DELIMIT_TAB, return_str);

    if (ret < 0) {
        return CC_FAILURE_UNKNOWN;
    } else if (strncmp(return_str, "FAIL", 4) == 0) {
        char cmd_tmp[30] = {0, };

        sprintf(cmd_tmp, "add_network"CLI_DELIMIT_TAB"%d", iface);
        ra6w1_cli_reply(cmd_tmp, CLI_DELIMIT_TAB, NULL);

        if (iface == 1) {
            ra6w1_cli_reply("set_network 1 mode 2", NULL, NULL);
        }
        
        ret = ra6w1_cli_reply(cmd, CLI_DELIMIT_TAB, return_str);
        if (ret < 0 || strncmp(return_str, "FAIL", 4) == 0) {
            return CC_FAILURE_UNKNOWN;
        }
    }
    
    return CC_STATUS_SUCCESS;
}

int cc_set_network_str(const char *name, int iface, const char *val)
{
    char cmd[256] = {0, };

    sprintf(cmd, "set_network"CLI_DELIMIT_TAB"%d"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s", iface, name, val);
    return cc_set_network(cmd, iface);
}

int cc_set_network_int(char *name, int iface, int val)
{
    char cmd[256] = {0, };

    sprintf(cmd, "set_network"CLI_DELIMIT_TAB"%d"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%d", iface, name, val);
    return cc_set_network(cmd, iface);
}

int ra6w1_cli_reply(char *cmdline, char *delimit, char *cli_reply)
{
    int ret = 0;

    char *tmp_reply;
    char tmp_cmdline[CLI_CMD_REQ_UNICODE_SIZE];
    char *tmp_delimit;
    char *params[MAX_WLANINIT_PARAMS];
    int  param_cnt = 0;

    memset(tmp_cmdline, 0, CLI_CMD_REQ_UNICODE_SIZE);

    strncpy(tmp_cmdline, cmdline, CLI_CMD_REQ_UNICODE_SIZE);

    if (delimit == NULL) {
        tmp_delimit = " ";
    } else {
        tmp_delimit = delimit;
    }

    if (cli_reply == NULL) {
        tmp_reply = pvPortMalloc(CLI_RSP_BUF_SIZE);

        if (tmp_reply == NULL) {
            printf("[%s] Failed to malloc\n", __func__);
            ret = -1;
            goto CMD_ERROR;
        }

        memset(tmp_reply, 0, CLI_RSP_BUF_SIZE);
    } else {
        tmp_reply = cli_reply;
    }


    /* hexstr Decode */
    {
        unsigned char *tmp_decode;
        extern size_t printf_decode(u8 *, size_t, const char *);
        int len = 0;

        tmp_decode = pvPortMalloc(CLI_CMD_REQ_BUF_SIZE);

        if (tmp_decode == NULL) {
            printf("[%s] Failed to malloc\n", __func__);
            ret = -1;
            goto CMD_ERROR;
        }

#ifdef __DEBUG_CLI_REPLY__ /* DEBUG */
        printf("[%s] cmdline=%s\n", __func__, tmp_cmdline);
#endif /* __DEBUG_CLI_REPLY__ */

        len = printf_decode(tmp_decode, CLI_CMD_REQ_UNICODE_SIZE, tmp_cmdline);
        if (len > CLI_CMD_REQ_BUF_SIZE) {
            if (tmp_decode)
            {
                vPortFree(tmp_decode);
            }
            printf("[%s] Error hex_decode\n", __func__);
            goto CMD_ERROR;
        }

        memset(tmp_cmdline, 0, CLI_CMD_REQ_UNICODE_SIZE);
        memcpy(tmp_cmdline, tmp_decode, CLI_CMD_REQ_BUF_SIZE);

        if (tmp_decode)
        {
            vPortFree(tmp_decode);
        }
    }

#ifdef __DEBUG_CLI_REPLY__ /* DEBUG */
    printf("[%s:%d] cmdline=%s\n", __func__, __LINE__, tmp_cmdline);
#endif /* __DEBUG_CLI_REPLY__ */

    /*  [START] ======= Separate  param ======= */
    /* parse arguments */
    params[param_cnt++] = strtok(tmp_cmdline, tmp_delimit);

    while (((params[param_cnt] = strtok(NULL, tmp_delimit)) != NULL)) {
        if ((params[param_cnt][0] == '\'')
            && (params[param_cnt][strlen(params[param_cnt])-1] != '\'')) { /* Check Start String ' */
            char* check_point;
restore_delimit:
            params[param_cnt][strlen(params[param_cnt])] = tmp_delimit[0]; /* Recover delimit charactor */
            check_point = strtok(NULL, tmp_delimit);

            if (check_point == NULL) {
                ret = -1;
                strcpy(tmp_reply, "FAIL");
                goto CMD_ERROR;
            } else if (check_point[strlen(check_point)-1] != '\'') { /* Check End String ' */
                goto restore_delimit;
            }
        }

        param_cnt++;

        if (param_cnt > (MAX_WLANINIT_PARAMS - 1)) {
            break;
        }
    }
    /*  [END] ======= Separate param ======= */

#ifdef __DEBUG_CLI_REPLY__ /* DEBUG */
    for (int aaa = 0; aaa < param_cnt;) {
        printf("params[%d]= %s\n", aaa, params[aaa]);
        aaa++;
    }
#endif /* __DEBUG_CLI_REPLY__ */

    ret = wpa_request(param_cnt, params, tmp_reply);

CMD_ERROR:

    if (cli_reply == NULL && tmp_reply != NULL) {
        if (strlen(tmp_reply) > SUB_STRING_SIZE) {
            printf("\n");
            printf_sub_strings(tmp_reply, SUB_STRING_SIZE);
        } else {
            printf("\n%s", tmp_reply);
        }

        if (tmp_reply) {
            vPortFree(tmp_reply);
        }
    }

    return ret;
}


#define __CONSOLE_HEXA_ENCODING_FOR_SSID__


#define SWAP(a, b) { long t = *(long *)(a); *(long *)(a) = *(long *)(b); *(long *)(b) = t; }
extern void print_separate_bar(unsigned char text, unsigned char loop_count, unsigned char CR_loop_count);
/*****************************************************************************
 *
 * Function : ra6w1_wpa_cli
 *
 * - arguments
 *    argc : argeument count
 *    argv : "cli command with arguments" string
 *
 * - return :
 *    int status : running result
 *
 * - Discription :
 *      ra6wx cli api function.
 *
 ****************************************************************************/
void ra6w1_wpa_cli(int argc, char *argv[])
{
    char* tmp_argv;

    /*  [START] ======= Merge argv[] ======= */
    if (argc > 2) {
        int argv_len = 0;

        for (int ii = 1; ii < argc;) {
            argv_len = argv_len + (strlen(argv[ii]) + 1);
             ii++;
        }

        tmp_argv = pvPortMalloc(argv_len);

        if (tmp_argv == NULL) {
            printf("[%s] Error Malloc(%d)\n", __func__, argv_len);
            return;
        }

        memset(tmp_argv, 0, argv_len);
        strcpy(tmp_argv, argv[1]);

        for (int ii = 2; ii < argc;) {
            strcat(tmp_argv, " ");
            strcat(tmp_argv, argv[ii]);
            ii++;
        }
    /* [END] ======= Merge argv[] ======= */

    } else {
        tmp_argv = argv[1];
    }


    if (strncmp(tmp_argv, "scan", 4) == 0 || strcmp(tmp_argv, "sc") == 0) {
        char    *scan_result;
        char*    params[MAX_CLI_PARAMS][5]; /* BSSID, SIGNAL, */
        int    param_cnt = 0;
#if 0
        int sort_key = SCAN_SIGNAL_IDX;
#endif /* 0 */

        scan_result = pvPortMalloc(CLI_SCAN_RSP_BUF_SIZE);

        if (scan_result == NULL) {
            printf("[%s] Failed to malloc(%d : CLI_SCAN_RSP_BUF_SIZE)\n", __func__, CLI_SCAN_RSP_BUF_SIZE);
            if (argc > 2) {
                vPortFree(tmp_argv);
            }
            return;
        }

        memset(scan_result, 0, CLI_SCAN_RSP_BUF_SIZE);
        memset(params, 0, sizeof(params));
        param_cnt = 0;
        ra6w1_cli_reply(tmp_argv, NULL, scan_result);

        if (strlen(scan_result) < 30) {
            printf("Scan: %s\n", scan_result);

            vPortFree(scan_result);
            if (argc > 2) {
                if (tmp_argv)
                {
                    vPortFree(tmp_argv);
                }
            }
            return;
        }

        if (strcmp(tmp_argv, "sc") == 0) { // Scan result as supplicant original type.
            printf("%s\n", scan_result);
            vPortFree(scan_result);

            if (argc > 2) {
                if (tmp_argv)
                {
                    vPortFree(tmp_argv);
                }
            }
            return;
        }

        /*  [START] ======= Separate  param ======= */
        /* parse arguments */
        params[param_cnt++][0] = strtok(scan_result, "\n"); /* Title */

        if (params[0][0] == NULL) {
            printf("\33[31mNo Scan Resutls\33[39m\n");
#if defined(SIGMA_TEST_ENABLE)
            PRINT_SIGMA_CMD("\33[31mNo Scan Resutls\33[39m\n");
#endif
        }
        else {
            print_separate_bar('=', 100, 1);
            print_separate_bar('=', 100, 1);
            printf("[NO] %-45s %6s [CH] %-17s [SECURITY]\n",
                        "[SSID]", "[RSSI]", "[BSSID]");
#if defined(SIGMA_TEST_ENABLE)
            PRINT_SIGMA_CMD("[NO] %-45s %6s [CH] %-17s [SECURITY]\n",
                        "[SSID]", "[RSSI]", "[BSSID]");
#endif
            print_separate_bar('-', 100, 1);

            while ((params[param_cnt][SCAN_BSSID_IDX] = strtok(NULL, "\t")))            /* BSSID */
            {
                if ((params[param_cnt][SCAN_FREQ_IDX] = strtok(NULL, "\t")))            /* Freq. */
                {
                    if ((params[param_cnt][SCAN_SIGNAL_IDX] = strtok(NULL, "\t")))    /* Signal */
                    {
                        if ((params[param_cnt][SCAN_FLGA_IDX] = strtok(NULL, "\t")))    /* Flag */
                        {
                            params[param_cnt][SCAN_SSID_IDX] = strtok(NULL, "\n");    /* SSID */
                        }
                    }
                }

                param_cnt++;
                if (param_cnt > (MAX_CLI_PARAMS - 1)) {
                    break;
                }
            }

#if 0 /* Lists are already sorted by Supplicant */
            /* sort */
            for (int i = 1; i < param_cnt-1; ++i ) {

                for (int j = 1; j < param_cnt-i; ++j) {
                    if (atoi(params[j][sort_key]) < atoi(params[j+1][sort_key])) {
                        SWAP(&params[j][SCAN_BSSID_IDX],    &params[j+1][SCAN_BSSID_IDX]);
                        SWAP(&params[j][SCAN_FREQ_IDX],        &params[j+1][SCAN_FREQ_IDX]);
                        SWAP(&params[j][SCAN_SIGNAL_IDX],    &params[j+1][SCAN_SIGNAL_IDX]);
                        SWAP(&params[j][SCAN_FLGA_IDX],        &params[j+1][SCAN_FLGA_IDX]);
                        SWAP(&params[j][SCAN_SSID_IDX],        &params[j+1][SCAN_SSID_IDX]);
                    }
                }
            }
#endif /* 0 */

            for  (int idx = 1; idx < param_cnt; idx++) {
#ifdef __CONSOLE_HEXA_ENCODING_FOR_SSID__
                char ssid_txt[32 * 4 + 1];
#endif /* __CONSOLE_HEXA_ENCODING_FOR_SSID__ */
                vTaskDelay(1);

                if (idx % 2 == 1) {
                    printf("\33[30m\33[47m");
                }

                printf("[%02d]", idx);    /* No */
#if defined(SIGMA_TEST_ENABLE)
                PRINT_SIGMA_CMD("[%02d]", idx);    /* No */
#endif // SIGMA_TEST_ENABLE
#ifdef __CONSOLE_HEXA_ENCODING_FOR_SSID__
                if (params[idx][SCAN_SSID_IDX][0] == HIDDEN_SSID_DETECTION_CHAR) { /* Hidden SSID */
                    printf(" \33[31m\33[1m%-47s\33[0m\33[3%cm%s",
                        "[Hidden]",
                        (idx % 2 == 1) ? '0':'9',
                        (idx % 2 == 1) ? "\33[47m":"");
#if defined(SIGMA_TEST_ENABLE)
		    PRINT_SIGMA_CMD(" %-47s", "[Hidden]");
#endif // SIGMA_TEST_ENABLE
                } else {
                    printf_encode(ssid_txt, sizeof(ssid_txt),
                        (unsigned char *)params[idx][SCAN_SSID_IDX], strlen((char*)params[idx][SCAN_SSID_IDX]));

                    if (strlen(ssid_txt) <= 50) {
                        printf(" %-47s", ssid_txt);
#if defined(SIGMA_TEST_ENABLE)
                        PRINT_SIGMA_CMD(" %-47s", ssid_txt);
#endif // SIGMA_TEST_ENABLE
                    } else {
                        printf(" %s\33[49m\33[39m\n%s%52s",
                            ssid_txt,
                            (idx % 2 == 1) ? "\33[30m\33[47m":"",
                            " ");
#if defined(SIGMA_TEST_ENABLE)
			PRINT_SIGMA_CMD(" %s\n", ssid_txt);
#endif // SIGMA_TEST_ENABLE
                    } 
                }
#else
                if (params[idx][SCAN_SSID_IDX][0] == HIDDEN_SSID_DETECTION_CHAR) { /* Hidden SSID */
                    printf(" \33[31m\33[1m%-47s\33[0m\33[3%cm%s",
                        "[Hidden]",
                        (idx % 2 == 1) ? '0':'9',
                        (idx % 2 == 1) ? "\33[47m":"");
#if defined(SIGMA_TEST_ENABLE)
		    PRINT_SIGMA_CMD(" %-47s", "[Hidden]");
#endif // SIGMA_TEST_ENABLE
                } else if (strlen(params[idx][SCAN_SSID_IDX]) <= 50) {
                    printf(" %-47s", params[idx][SCAN_SSID_IDX]);
#if defined(SIGMA_TEST_ENABLE)
                    PRINT_SIGMA_CMD(" %-47s", params[idx][SCAN_SSID_IDX]);
#endif // SIGMA_TEST_ENABLE
                } else {
                    printf(" %s\33[49m\33[39m\n%s%52s",
                        params[idx][SCAN_SSID_IDX],
                        (idx % 2 == 1) ? "\33[30m\33[47m":"",
                        " ");
#if defined(SIGMA_TEST_ENABLE)
		    PRINT_SIGMA_CMD(" %s\n", params[idx][SCAN_SSID_IDX]);
#endif // SIGMA_TEST_ENABLE
                }
#endif /* __CONSOLE_HEXA_ENCODING_FOR_SSID__ */

                printf(    "%3s  "                    /* RSSI */
                        " %3d "                 /* CH */
                        " %16s\33[49m\33[39m"    /* BSSID */
                        " %s"                    /* Flag */
                        "\n",
                        params[idx][SCAN_SIGNAL_IDX],
                        i3ed11_freq_to_ch(atoi(params[idx][SCAN_FREQ_IDX])),
                        params[idx][SCAN_BSSID_IDX],
                        params[idx][SCAN_FLGA_IDX]);
#if defined(SIGMA_TEST_ENABLE)
                PRINT_SIGMA_CMD(    "%3s  "     /* RSSI */
                        " %3d "                 /* CH */
                        " %16s "                /* BSSID */
                        " %s"                   /* Flag */
                        "\n",
                        params[idx][SCAN_SIGNAL_IDX],
                        i3ed11_freq_to_ch(atoi(params[idx][SCAN_FREQ_IDX])),
                        params[idx][SCAN_BSSID_IDX],
                        params[idx][SCAN_FLGA_IDX]);
#endif // SIGMA_TEST_ENABLE
#ifdef __CONSOLE_HEXA_ENCODING_FOR_SSID__
                if (params[idx][SCAN_SSID_IDX][0] != HIDDEN_SSID_DETECTION_CHAR && strstr(ssid_txt, "\\x") != 0)
                {
                    printf("     %s\n", params[idx][SCAN_SSID_IDX]);
#if defined(SIGMA_TEST_ENABLE)
                    PRINT_SIGMA_CMD("     %s\n", params[idx][SCAN_SSID_IDX]);
#endif
                }
#endif /* __CONSOLE_HEXA_ENCODING_FOR_SSID__ */
            }

            /*  [END] ======= Separate param ======= */
            print_separate_bar('=', 100, 1);
        }
        vPortFree(scan_result);
    }
    else {
        ra6w1_cli_reply(tmp_argv, NULL, NULL);
    }

    if (argc > 2) {
#ifdef __DEBUG_CLI_REPLY__ /* DEBUG */
        printf("[%s] tmp_argv=%s\n", __func__, tmp_argv);
#endif /* __DEBUG_CLI_REPLY__ */

        if (tmp_argv) {
            vPortFree(tmp_argv);
        }
    }
}


/*
 * Function : ra6w1_cli_eloop_run
 *
 * - arguments    : reply
 * - return    : status
 *
 * - Discription :
 *    Asyncronous Response event from supplicant to cli.
 */
#ifdef CONFIG_SCAN_REPLY_OPTIMIZE
UINT ra6w1_cli_eloop_run(char **reply)
#else
UINT ra6w1_cli_eloop_run(char *reply)
#endif /* CONFIG_SCAN_REPLY_OPTIMIZE */

{
    UINT    status;
#ifndef CONFIG_SCAN_REPLY_OPTIMIZE
    ra6w1_cli_rsp_buf_t *ra6w1_cli_rsp_buf_p;
#endif /* CONFIG_SCAN_REPLY_OPTIMIZE */

    ULONG temp_pack[2];

    extern QueueHandle_t TO_CLI_QUEUE;

    TX_FUNC_START(" ");
    while (1) {
        vTaskDelay(portCONVERT_MS_2_TICKS(50));

        /* Get supplicant -> cli response event : 100msec */
        status = xQueueReceive(TO_CLI_QUEUE, &temp_pack, 10);

        if (status == pdTRUE) {
            ra6w1_cli_prt("<%s> Wait receive_msg (0x%02x)\n", __func__, status);
        }

        if (status != pdTRUE) {
            ra6w1_cli_prt("<%s> Wait receive_msg (0x%02x)\n", __func__, status);
        } else if (temp_pack[0] == RA6WX_SP_CLI_RX_FLAG) {

#ifdef CONFIG_SCAN_REPLY_OPTIMIZE
            *reply = (char *)temp_pack[1];
#else
            ra6w1_cli_rsp_buf_p = (ra6w1_cli_rsp_buf_t *)temp_pack[1];
            strcpy(reply, ra6w1_cli_rsp_buf_p->event_data->data);
#endif /* CONFIG_SCAN_REPLY_OPTIMIZE */
            TX_FUNC_END(" ");
            return status;
        }
    }
}


/* TEST wlaninit_cmd_parser */
/**************************
[OPEN]
wlaninit_cmd_parser [profileID] [wpas_mode: 1:STA, 2: AP] [ssid] [OPEN] [CHANNEL] [COUNTRY_CODE]
wlaninit_cmd_parser 0 1 'SSID'                               <== OPEN Station
wlaninit_cmd_parser 0 1 'SSID AAAA 1234'                     <== OPEN Station
wlaninit_cmd_parser 0 1 'SSID' OPEN                          <== OPEN Station
wlaninit_cmd_parser 1 2 'SSID' OPEN 13                       <== OPEN AP (CH. 13 Country default)
wlaninit_cmd_parser 1 2 'SSID' OPEN 13 KR                    <== OPEN AP (CH. 13 Country KR)

[WEP]
wlaninit_cmd_parser [profileID] [wpas_mode: 1:STA, 2: AP] [ssid] [WEP] [WEP_Key_INDEX] [WEP_Key] [ap Channel] [Country Code]
wlaninit_cmd_parser 0 1 'SSID' WEP 0 '12345'                               <== Station WEP (keyindx 0, 64bit ascii key)
wlaninit_cmd_parser 0 1 'SSID' WEP 1 '1234567890123'                       <== Station WEP (keyindx 1, 128bit ascii key)
wlaninit_cmd_parser 0 1 'SSID' WEP 2 0x1234567890                          <== Station WEP (keyindx 3, 64bit hexa key)
wlaninit_cmd_parser 0 1 'SSID' WEP 3 0x12345678901234567890123456          <== Station WEP (keyindx 3, 128bit hexa key)
wlaninit_cmd_parser 1 2 'SSID' WEP 0 '12345'                               <== AP WEP (keyindx 0, 64bit ascii key)
wlaninit_cmd_parser 1 2 'SSID' WEP 1 '1234567890' 13                       <== AP WEP (keyindx 1, 128bit ASCII key CH.13 Country default)
wlaninit_cmd_parser 1 2 'SSID' WEP 2 '1234567890' 13 KR                    <== AP WEP (keyindx 2, 128bit ASCII key CH.13 Country KR)
wlaninit_cmd_parser 1 2 'SSID' WEP 3 0x12345678901234567890123456 13 KR    <== AP WEP (keyindx 3, 128bit hexa key CH.13 Country KR)

[WPA|RSN(WPA2)]
wlaninit_cmd_parser [profileID] [wpas_mode: 1:STA, 2: AP] [ssid] [WPA|RSN|WPA_RSN|RSN_PMF1|RSN_PMF2|RSN_PMF3|RSN_PMF1_SAE|RSN_PMF2_SAE] [TKIP|CCMP|TKIP_CCMP] [psk_key] [ap Channel] [Country Code]
=====================================================================================================================
wlaninit_cmd_parser [profileID] [Mode: 1:STA, 2: AP] [ssid] [WPA] [TKIP] [PSK-KEY] [ap Channel] [Country Code]
wlaninit_cmd_parser 0 1 'SSID' WPA TKIP '1234567890'                       <== Station wpa-psk TKIP
wlaninit_cmd_parser 1 2 'SSID' WPA TKIP '1234567890'                       <== AP wpa-psk TKIP (CH. auto, Country defult)
wlaninit_cmd_parser 1 2 'SSID' WPA TKIP '1234567890'    11                 <== AP wpa-psk TKIP (CH. 11, Country defult)
wlaninit_cmd_parser 1 2 'SSID' WPA TKIP '1234567890'    13 KR              <== AP wpa-psk TKIP (CH. 13, Country KR)

wlaninit_cmd_parser [profileID] [wpas_mode: 1:STA, 2: AP] [ssid] [WPA] [CCMP] [PSK-KEY] [ap Channel] [Country Code]
wlaninit_cmd_parser 0 1 'SSID' WPA CCMP '1234567890'                       <== Station wpa-psk CCMP (CH. auto, Country defult)
wlaninit_cmd_parser 1 2 'SSID' WPA CCMP '1234567890' 11                    <== AP wpa-psk CCMP (CH. 11, Country defult)
wlaninit_cmd_parser 1 2 'SSID' WPA CCMP '1234567890' 13 KR                 <== AP wpa-psk CCMP (CH. 13, Country KR)

wlaninit_cmd_parser [profileID] [wpas_mode: 1:STA, 2: AP] [ssid] [RSN] [TKIP] [PSK-KEY] [ap Channel] [Country Code]
wlaninit_cmd_parser 0 1 'SSID' RSN TKIP '1234567890'                       <== Station wpa2-psk TKIP (CH. auto, Country defult)
wlaninit_cmd_parser 1 2 'SSID' RSN TKIP '1234567890'    11                 <== AP wpa2-psk TKIP (CH. 11, Country defult)
wlaninit_cmd_parser 1 2 'SSID' RSN TKIP '1234567890' 13 KR                 <== AP wpa2-psk TKIP (CH. 13, Country KR)

wlaninit_cmd_parser [profileID] [wpas_mode: 1:STA, 2: AP] [ssid] [RSN] [CCMP] [PSK-KEY] [ap Channel] [Country Code]
wlaninit_cmd_parser 0 1 'SSID' RSN CCMP '1234567890'                       <== Station wpa2-psk CCMP
wlaninit_cmd_parser 1 2 'SSID' RSN CCMP '1234567890' 11                    <== AP wpa2-psk CCMP (CH. 11, Country defult)
wlaninit_cmd_parser 1 2 'SSID' RSN CCMP '1234567890' 13 KR                 <== AP wpa2-psk CCMP (CH. 13, Country KR)

wlaninit_cmd_parser [profileID] [wpas_mode: 1:STA, 2: AP] [ssid] [RSN+PMF] [CCMP] [PSK-KEY] [ap Channel] [Country Code]
wlaninit_cmd_parser 0 1 'SSID' RSN_PMF1 CCMP '1234567890'                  <== Station wpa2-psk CCMP PMF-OPTIONAL 
wlaninit_cmd_parser 0 1 'SSID' RSN_PMF2 CCMP '1234567890'                  <== Station WPA-PSK-SHA256 CCMP PMF-REQUIRED 
wlaninit_cmd_parser 0 1 'SSID' RSN_PMF3 CCMP '1234567890'                  <== Station WPA-PSK WPA-PSK-SHA256 CCMP PMF-OPTIONAL 
wlaninit_cmd_parser 1 2 'SSID' RSN_PMF1 CCMP '1234567890'                  <== AP wpa2-psk CCMP PMF-OPTIONAL 
wlaninit_cmd_parser 1 2 'SSID' RSN_PMF2 CCMP '1234567890'                  <== AP WPA-PSK-SHA256 CCMP PMF-REQUIRED 

wlaninit_cmd_parser [profileID] [wpas_mode: 1:STA, 2: AP] [ssid] [WPA_RSN] [TKIP] [PSK-KEY] [ap Channel] [Country Code]
wlaninit_cmd_parser 0 1 ssid WPA_RSN TKIP '1234567890'                     <== Station wpa1/2-psk TKIP
wlaninit_cmd_parser 1 2 ssid WPA_RSN TKIP '1234567890'    11               <== AP wpa1/2-psk TKIP (CH. 11, Country defult)
wlaninit_cmd_parser 1 2 ssid WPA_RSN TKIP '1234567890' 13 KR               <== AP wpa1/2-psk TKIP (CH. 13, Country KR)

wlaninit_cmd_parser [profileID] [wpas_mode: 1:STA, 2: AP] [ssid] [WPA_RSN] [CCMP] [PSK-KEY] [ap Channel] [Country Code]
wlaninit_cmd_parser 0 1 'SSID' WPA_RSN CCMP '1234567890'                   <== Station wpa1/2-psk CCMP
wlaninit_cmd_parser 1 2 'SSID' WPA_RSN CCMP '1234567890'    11             <== AP wpa1/2-psk CCMP (CH. 11, Country defult)
wlaninit_cmd_parser 1 2 'SSID' WPA_RSN CCMP '1234567890' 13 KR             <== AP wpa1/2-psk CCMP (CH. 13, Country KR)

wlaninit_cmd_parser [profileID] [wpas_mode: 1:STA, 2: AP] [ssid] [WPA_RSN] [TKIP_CCMP] [PSK-KEY] [ap Channel] [Country Code]
wlaninit_cmd_parser 0 1 'SSID' WPA_RSN TKIP_CCMP '1234567890'              <== Station wpa1/2-psk TKIP/CCMP
wlaninit_cmd_parser 1 2 'SSID' WPA_RSN TKIP_CCMP '1234567890' 11           <== AP wpa1/2-psk TKIP/CCMP (CH. 11, Country defult)
wlaninit_cmd_parser 1 2 'SSID' WPA_RSN TKIP_CCMP '1234567890' 13 KR        <== AP wpa1/2-psk TKIP/CCMP (CH. 13, Country KR)

wlaninit_cmd_parser 1 2 'SSID' WPA_RSN TKIP_CCMP '123456789012345678901234567890123456789012345678901234567890123' 13 KR    <== AP wpa1/2-psk TKIP/CCMP (CH. 13, Country KR)
wlaninit_cmd_parser 1 2 'SSID' WPA_RSN TKIP_CCMP '123456789 123456789 123456789 123456789 123456789 123456789 123' 13 KR    <== AP wpa1/2-psk TKIP/CCMP (CH. 13, Country KR)

[Mesh]
wlaninit_cmd_parser 3 5 'RA6WX_MESH' OPEN 11 KR
wlaninit_cmd_parser 3 5 'RA6WX_MESH' SAE CCMP 'N12345678' 11 KR

[wpas_mode]
WPAS_MODE_INFRA = 0
WPAS_MODE_IBSS = 1
WPAS_MODE_AP = 2
WPAS_MODE_P2P_GO = 3
WPAS_MODE_P2P_GROUP_FORMATION = 4
WPAS_MODE_MESH_POINT = 5

**************************/

int wlaninit_cmd_parser (char *cmdline, char *delimit)
{
    int ret = 0;

    char    reply[REPLY_LEN];
    char    tmp_cmdline[WLANINIT_CMD_BUF_SIZE];
    char    *tmp_delimit;
    char    *params[MAX_CLI_PARAMS];
    int     param_cnt = 0;
    char    ra6w1_cli_cmd[WLANINIT_CMD_BUF_SIZE];
    char    tmp_wep_key[32];

    memset(tmp_cmdline, 0, WLANINIT_CMD_BUF_SIZE);

    strcpy(tmp_cmdline, cmdline);

    if (delimit == NULL) {
        tmp_delimit = CLI_DELIMIT;
    } else {
        tmp_delimit = delimit;
    }

    if (dbg_wpa_cli) {
        printf("[%s] %s\n", __func__, tmp_cmdline);
    }

    /*  [START] ======= Separate  param ======= */
    /* parse arguments */
    params[param_cnt++] = strtok(tmp_cmdline, tmp_delimit);
    while (((params[param_cnt] = strtok(NULL, tmp_delimit)) != NULL)) {
        if ((params[param_cnt][0] == '\'')
            && (params[param_cnt][strlen(params[param_cnt])-1] != '\'')) { /* Check Start String ' */
            char * check_point;
restore_delimit:
            params[param_cnt][strlen(params[param_cnt])] = tmp_delimit[0]; /* Recover delimit charactor */
            check_point = strtok(NULL, tmp_delimit);
            if (check_point == NULL) {
                strcpy(reply, "FAIL");
                strcpy(ra6w1_cli_cmd, cmdline);
                ret = -1;
                goto CMD_ERROR;
            } else if (check_point[strlen(check_point)-1] != '\'') { /* Check End String ' */
                goto restore_delimit;
            }
        }
        param_cnt++;

        if (param_cnt > (MAX_CLI_PARAMS - 1)) {
            break;
        }
    }
    /*  [END] ======= Separate param ======= */

#ifdef __DEBUG_WLANINIT_CMD_PARSER__
    printf("[wlaninit_cmd_parser]\n");
    for (int cc = 0; cc < param_cnt;) {
        printf("%d. [%s]\n", cc, params[cc]);
        cc++;
    }
#endif /* __DEBUG_WLANINIT_CMD_PARSER__ */

    /* SETUP */
    if (!((params[PROFILE_IDX][0] == '0' && params[MODE_IDX][0] == MODE_STA)    /* Station) */
        || (params[PROFILE_IDX][0] == '1' && params[MODE_IDX][0] == MODE_AP)    /* AP */
#ifdef __SUPPORT_MESH__
        || (params[PROFILE_IDX][0] == '3' && params[MODE_IDX][0] == MODE_MESH_POINT)    /* MESH_POINT*/
#endif /* __SUPPORT_MESH__ */
    )) {
        strcpy(reply, "FAIL");
        sprintf(ra6w1_cli_cmd, "PROFILE ID & MODE: %s %s", params[PROFILE_IDX], params[MODE_IDX]);
        goto CMD_ERROR;
    }

    /* Remove Network(Remove Profile) *********************************************/
    memset(ra6w1_cli_cmd, 0, WLANINIT_CMD_BUF_SIZE);
    sprintf(ra6w1_cli_cmd, "%s"CLI_DELIMIT_TAB"%s", CMD_REMOVE_NETWORK, params[PROFILE_IDX]);

#ifdef __DEBUG_WLANINIT_CMD_PARSER__
    printf("[%s:%d] ra6w1_cli_cmd=%s\n", __FUNCTION__, __LINE__, ra6w1_cli_cmd);
#endif /* __DEBUG_WLANINIT_CMD_PARSER__ */
    ra6w1_cli_reply(ra6w1_cli_cmd, CLI_DELIMIT_TAB, reply); /* cli remove_network 0 */

    /* Add Network(Profile add) ***************************************************/
    memset(ra6w1_cli_cmd, 0, WLANINIT_CMD_BUF_SIZE);
    sprintf(ra6w1_cli_cmd, "%s"CLI_DELIMIT_TAB"%s", CMD_ADD_NETWORK, params[PROFILE_IDX]);

    memset(reply, 0, REPLY_LEN);
#ifdef __DEBUG_WLANINIT_CMD_PARSER__
    printf("[%s:%d] ra6w1_cli_cmd=%s\n", __FUNCTION__, __LINE__, ra6w1_cli_cmd);
#endif /* __DEBUG_WLANINIT_CMD_PARSER__ */
    ret = ra6w1_cli_reply(ra6w1_cli_cmd, CLI_DELIMIT_TAB, reply); /* cli add_network */
    if (ret == -1 || !strcmp(reply, REPLY_FAIL)) {
        goto CMD_ERROR;
    }

    /* SSID ************************************************************************/
    memset(ra6w1_cli_cmd, 0, WLANINIT_CMD_BUF_SIZE);
    sprintf(ra6w1_cli_cmd, "%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s",
        CMD_SET_NETWORK,
        params[PROFILE_IDX],
        CMD2_SSID,
        params[SSID_IDX]);

    memset(reply, 0, REPLY_LEN);
#ifdef __DEBUG_WLANINIT_CMD_PARSER__
    printf("[%s:%d] ra6w1_cli_cmd=%s\n", __FUNCTION__, __LINE__, ra6w1_cli_cmd);
#endif /* __DEBUG_WLANINIT_CMD_PARSER__ */
    ret = ra6w1_cli_reply(ra6w1_cli_cmd, CLI_DELIMIT_TAB, reply); /* cli set_network 0 ssid TEST_SSID */
    if (ret == -1 || !strcmp(reply, REPLY_FAIL)) {
        goto CMD_ERROR;
    }

    /* WPAS_MODE  *************************************************************/
    /* wpa_s->ssid->mode : struct wpa_supplicant -> struct wpa_ssid -> enum wpas_mode(config_ssid.h) */
    memset(ra6w1_cli_cmd, 0, WLANINIT_CMD_BUF_SIZE);
    sprintf(ra6w1_cli_cmd, "%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s",
        CMD_SET_NETWORK,
        params[PROFILE_IDX],
        CMD2_WPAS_MODE,
        params[MODE_IDX][0] == MODE_STA ? "0" : params[MODE_IDX]);

    memset(reply, 0, REPLY_LEN);
#ifdef __DEBUG_WLANINIT_CMD_PARSER__
    printf("[%s:%d] ra6w1_cli_cmd=%s\n", __FUNCTION__, __LINE__, ra6w1_cli_cmd);
#endif /* __DEBUG_WLANINIT_CMD_PARSER__ */
    ret = ra6w1_cli_reply(ra6w1_cli_cmd, CLI_DELIMIT_TAB, reply); /* cli set_network 0 mode params[2] */
    if (ret == -1 || !strcmp(reply, REPLY_FAIL)) {
        goto CMD_ERROR;
    }

    /***************************************************************************
     Start Security Mode
    ***************************************************************************/
    
    /* OPEN MODE **************************************************************/
    if ((param_cnt == 3) || strcmp(params[KEY_MGMT_PROTO_IDX], "OPEN") == 0) {
        memset(ra6w1_cli_cmd, 0, WLANINIT_CMD_BUF_SIZE);
        sprintf(ra6w1_cli_cmd, "%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s",
            CMD_SET_NETWORK,
            params[PROFILE_IDX],
            CMD2_KEY_MGMT,
            VAL_KEY_MGMT_NONE);

        memset(reply, 0, REPLY_LEN);
#ifdef __DEBUG_WLANINIT_CMD_PARSER__
        printf("[%s:%d] ra6w1_cli_cmd=%s\n", __FUNCTION__, __LINE__, ra6w1_cli_cmd);
#endif /* __DEBUG_WLANINIT_CMD_PARSER__ */
        ret = ra6w1_cli_reply(ra6w1_cli_cmd, CLI_DELIMIT_TAB, reply); /* cli set_network 0 key_mgmt NONE */
        if (ret == -1 || !strcmp(reply, REPLY_FAIL)) {
            goto CMD_ERROR;
        }
    }
    /* OWE  *******************************************************************/
    else if (strcmp(params[KEY_MGMT_PROTO_IDX], "OWE") == 0) {
        /* proto */
        memset(ra6w1_cli_cmd, 0, WLANINIT_CMD_BUF_SIZE);
        sprintf(ra6w1_cli_cmd, "%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s",
            CMD_SET_NETWORK,
            params[PROFILE_IDX],
            CMD2_PROTO,
            VAL_PROTO_WPA2_RSN); /* cli set_network 0 proto  RSN */

        memset(reply, 0, REPLY_LEN);
#ifdef __DEBUG_WLANINIT_CMD_PARSER__
        printf("[%s:%d] ra6w1_cli_cmd=%s\n", __FUNCTION__, __LINE__, ra6w1_cli_cmd);
#endif /* __DEBUG_WLANINIT_CMD_PARSER__ */
        ret = ra6w1_cli_reply(ra6w1_cli_cmd, CLI_DELIMIT_TAB, reply);

        /* pairwise */
        memset(ra6w1_cli_cmd, 0, WLANINIT_CMD_BUF_SIZE);
        sprintf(ra6w1_cli_cmd, "%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s",
            CMD_SET_NETWORK,
            params[PROFILE_IDX],
            CMD2_PAIRWISE,
            VAL_PAIRWISE_AES_CCMP); /* cli set_network 0 pairwise  CCMP */

        memset(reply, 0, REPLY_LEN);
#ifdef __DEBUG_WLANINIT_CMD_PARSER__
        printf("[%s:%d] ra6w1_cli_cmd=%s\n", __FUNCTION__, __LINE__, ra6w1_cli_cmd);
#endif /* __DEBUG_WLANINIT_CMD_PARSER__ */
        ret = ra6w1_cli_reply(ra6w1_cli_cmd, CLI_DELIMIT_TAB, reply);

        /* key_mgmt */    
        memset(ra6w1_cli_cmd, 0, WLANINIT_CMD_BUF_SIZE);
        sprintf(ra6w1_cli_cmd, "%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s",
            CMD_SET_NETWORK,
            params[PROFILE_IDX],
            CMD2_KEY_MGMT,
            VAL_KEY_MGMT_OWE);
                
        memset(reply, 0, REPLY_LEN);
#ifdef __DEBUG_WLANINIT_CMD_PARSER__
        printf("[%s:%d] ra6w1_cli_cmd=%s\n", __FUNCTION__, __LINE__, ra6w1_cli_cmd);
#endif /* __DEBUG_WLANINIT_CMD_PARSER__ */
        ret = ra6w1_cli_reply(ra6w1_cli_cmd, CLI_DELIMIT_TAB, reply); /* cli set_network 0 key_mgmt OWE */
        if (ret == -1 || !strcmp(reply, REPLY_FAIL)) {
            goto CMD_ERROR;
        }

        /* PMF(ieee80211w) */
        memset(ra6w1_cli_cmd, 0, WLANINIT_CMD_BUF_SIZE);
        sprintf(ra6w1_cli_cmd, "%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%d",
            CMD_SET_NETWORK,
            params[PROFILE_IDX],
            CMD2_PMF_80211W, 2); /* cli set_network 0 ieee80211w 2 */

        memset(reply, 0, REPLY_LEN);
#ifdef __DEBUG_WLANINIT_CMD_PARSER__
            printf("[%s:%d] ra6w1_cli_cmd=%s\n", __FUNCTION__, __LINE__, ra6w1_cli_cmd);
#endif /* __DEBUG_WLANINIT_CMD_PARSER__ */
        ret = ra6w1_cli_reply(ra6w1_cli_cmd, CLI_DELIMIT_TAB, reply);
        if (ret == -1 || !strcmp(reply, REPLY_FAIL)) {
            goto CMD_ERROR;
        }

    } 

    /* WEP ********************************************************************/
    else if (strcmp(params[KEY_MGMT_PROTO_IDX], "WEP") == 0) {    /* WEP STA */
        memset(ra6w1_cli_cmd, 0, WLANINIT_CMD_BUF_SIZE);
        sprintf(ra6w1_cli_cmd, "%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s",
            CMD_SET_NETWORK,
            params[PROFILE_IDX],
            CMD2_KEY_MGMT,
            VAL_KEY_MGMT_NONE);

        memset(reply, 0, REPLY_LEN);
#ifdef __DEBUG_WLANINIT_CMD_PARSER__
        printf("[%s:%d] ra6w1_cli_cmd=%s\n", __FUNCTION__, __LINE__, ra6w1_cli_cmd);
#endif /* __DEBUG_WLANINIT_CMD_PARSER__ */
        ret = ra6w1_cli_reply(ra6w1_cli_cmd, CLI_DELIMIT_TAB, reply); /* cli set_network 0 key_mgmt wep */
        if (ret == -1 || !strcmp(reply, REPLY_FAIL)) {
            goto CMD_ERROR;
        }

        /* WEP_KEY X */
        memset(ra6w1_cli_cmd, 0, WLANINIT_CMD_BUF_SIZE);
        memset(tmp_wep_key, 0, 32);

        if (params[KEY_MGMT_WEP_KEY_IDX][0] == '0' && params[KEY_MGMT_WEP_KEY_IDX][1] == 'x'  \
            && ((strlen(params[KEY_MGMT_WEP_KEY_IDX]) == 10+2)
                || (strlen(params[KEY_MGMT_WEP_KEY_IDX]) == 26+2))) { /* Check if the key type is Hexa.*/
            sprintf(tmp_wep_key, "%s", params[KEY_MGMT_WEP_KEY_IDX]+2);
        } else /* if (strlen(params[6]) == 5 || strlen(params[6]) == 13) */ { /* Check key size */
            sprintf(tmp_wep_key, "%s", params[KEY_MGMT_WEP_KEY_IDX]);
        }

        sprintf(ra6w1_cli_cmd, "%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s%s"CLI_DELIMIT_TAB"%s",
            CMD_SET_NETWORK,
            params[PROFILE_IDX],
            CMD2_WEPKEYx, params[KEY_MGMT_WEP_USE_INDEX_IDX],    /* key index */
            tmp_wep_key);

        memset(reply, 0, REPLY_LEN);
#ifdef __DEBUG_WLANINIT_CMD_PARSER__
        printf("[%s:%d] ra6w1_cli_cmd=%s\n", __FUNCTION__, __LINE__, ra6w1_cli_cmd);
#endif /* __DEBUG_WLANINIT_CMD_PARSER__ */

        ret = ra6w1_cli_reply(ra6w1_cli_cmd, CLI_DELIMIT_TAB, reply); /* cli set_network 0 key_keyX 'params[4] ' */
        if (ret == -1 || !strcmp(reply, REPLY_FAIL)) {
            goto CMD_ERROR;
        }

        /* KEY INDEX */
        sprintf(ra6w1_cli_cmd, "%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s",
            CMD_SET_NETWORK,
            params[PROFILE_IDX],
            CMD2_WEPKEY_USE_INDEX,
            params[KEY_MGMT_WEP_USE_INDEX_IDX]);

        memset(reply, 0, REPLY_LEN);
#ifdef __DEBUG_WLANINIT_CMD_PARSER__
        printf("[%s:%d] ra6w1_cli_cmd=%s\n", __FUNCTION__, __LINE__, ra6w1_cli_cmd);
#endif /* __DEBUG_WLANINIT_CMD_PARSER__ */
        ret = ra6w1_cli_reply(ra6w1_cli_cmd, CLI_DELIMIT_TAB, reply); /* cli set_network 0 wep_tx_keyidx  params[4] */
        if (ret == -1 || !strcmp(reply, REPLY_FAIL)) {
            goto CMD_ERROR;
        }
    }

    /* WPA1 or WPA2-PSK or WPA-PSK-SHA256 or SAE ******************************/
    else if (strstr(params[KEY_MGMT_PROTO_IDX], "EAP") == 0) { // WPA1,2 WPA-PSK, SAE ... (Not Enterprise)
        const char *config_key_mgmt;

        /* === Proto : WPA Version (WPA, RSN=WPA2, WPA RSN) === */
        if (strcmp(params[KEY_MGMT_PROTO_IDX], "WPA_RSN") != 0
            && strncmp(params[KEY_MGMT_PROTO_IDX], "RSN_PMF", 7) != 0
            && strcmp(params[KEY_MGMT_PROTO_IDX], "WPA_RSN_PMF") != 0) { /* wpa1/wpa2/SAE */
            /* proto */
            memset(ra6w1_cli_cmd, 0, WLANINIT_CMD_BUF_SIZE);
            sprintf(ra6w1_cli_cmd, "%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB,
                CMD_SET_NETWORK, params[PROFILE_IDX], CMD2_PROTO);

            if (strcmp(params[KEY_MGMT_PROTO_IDX], "RSN") == 0
                || strcmp(params[KEY_MGMT_PROTO_IDX], "RSN_SAE") == 0
                || strcmp(params[KEY_MGMT_PROTO_IDX], "SAE") == 0) {
                strcat(ra6w1_cli_cmd, VAL_PROTO_WPA2_RSN); // WPA2(RSN)
            } else if (strcmp(params[KEY_MGMT_PROTO_IDX], "WPA") == 0) {
                strcat(ra6w1_cli_cmd, VAL_PROTO_WPA1);    // WPA1
            } else {
                strcat(ra6w1_cli_cmd, VAL_PROTO_WPA1_2);  // WPA1,2
            }

            memset(reply, 0, REPLY_LEN);
#ifdef __DEBUG_WLANINIT_CMD_PARSER__
            printf("[%s:%d] ra6w1_cli_cmd=%s\n", __FUNCTION__, __LINE__, ra6w1_cli_cmd);
#endif /* __DEBUG_WLANINIT_CMD_PARSER__ */
            ret = ra6w1_cli_reply(ra6w1_cli_cmd, CLI_DELIMIT_TAB, reply);
            if (ret == -1 || !strcmp(reply, REPLY_FAIL)) {
                goto CMD_ERROR;
            }
        } else if (strncmp(params[KEY_MGMT_PROTO_IDX], "RSN_PMF", 7) == 0) { /* Proto RSN(WPA2) */
            /* proto */
            memset(ra6w1_cli_cmd, 0, WLANINIT_CMD_BUF_SIZE);
            sprintf(ra6w1_cli_cmd, "%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s",
                CMD_SET_NETWORK,
                params[PROFILE_IDX],
                CMD2_PROTO,
                VAL_PROTO_WPA2_RSN); /* cli set_network 0 proto RSN */

            memset(reply, 0, REPLY_LEN);
#ifdef __DEBUG_WLANINIT_CMD_PARSER__
            printf("[%s:%d] ra6w1_cli_cmd=%s\n", __FUNCTION__, __LINE__, ra6w1_cli_cmd);
#endif /* __DEBUG_WLANINIT_CMD_PARSER__ */
            ret = ra6w1_cli_reply(ra6w1_cli_cmd, CLI_DELIMIT_TAB, reply);
            if (ret == -1 || !strcmp(reply, REPLY_FAIL)) {
                goto CMD_ERROR;
            }
        } else if (strcmp(params[KEY_MGMT_PROTO_IDX], "WPA_RSN_PMF1") == 0) { /* Proto WAP1/2 PMF 1/ WPA_RSN_PMF */
            /* proto */
            memset(ra6w1_cli_cmd, 0, WLANINIT_CMD_BUF_SIZE);
            sprintf(ra6w1_cli_cmd, "%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s",
                CMD_SET_NETWORK,
                params[PROFILE_IDX],
                CMD2_PROTO,
                VAL_PROTO_WPA1,
                VAL_PROTO_WPA2_RSN); /* cli set_network 0 proto WPA RSN */

            memset(reply, 0, REPLY_LEN);
#ifdef __DEBUG_WLANINIT_CMD_PARSER__
            printf("[%s:%d] ra6w1_cli_cmd=%s\n", __FUNCTION__, __LINE__, ra6w1_cli_cmd);
#endif /* __DEBUG_WLANINIT_CMD_PARSER__ */
            ret = ra6w1_cli_reply(ra6w1_cli_cmd, CLI_DELIMIT_TAB, reply);
            if (ret == -1 || !strcmp(reply, REPLY_FAIL)) {
                goto CMD_ERROR;
            }
        }

        /* key_mgmt: ======================================================== */
#ifdef __SUPPORT_WPA3_SAE__
        if (strcmp(params[KEY_MGMT_PROTO_IDX], "RSN_SAE") == 0
            || strstr(params[KEY_MGMT_PROTO_IDX], "RSN_PMF1_SAE") != 0) { /* WPA1, WPA2 PMF-Optional */
            config_key_mgmt = VAL_KEY_MGMT_WPA_PSK_SAE; // WPA-PSK SAE
        } else if (strstr(params[KEY_MGMT_PROTO_IDX], "RSN_PMF2_SAE") != 0) { /* SAE, WPA2 PMF-Mandatory */
            config_key_mgmt = VAL_KEY_MGMT_WPA_PSK_SHA256_SAE; // WPA-PSK-SHA265 SAE
        } else if (params[MODE_IDX][0] == MODE_STA
            && strstr(params[KEY_MGMT_PROTO_IDX], "RSN_PMF3_SAE") != 0) { // Station Only : WPA-PSK PMF Optinal SAE
            config_key_mgmt = VAL_KEY_MGMT_WPA_PSK_WPA_PSK_SHA256_SAE;
        } else if (strcmp(params[KEY_MGMT_PROTO_IDX], "SAE") == 0) {
            config_key_mgmt = VAL_KEY_MGMT_SAE;
        } else
#endif /* __SUPPORT_WPA3_SAE__ */
#ifdef __SUPPORT_IEEE80211W__
        if (strstr(params[KEY_MGMT_PROTO_IDX], "RSN_PMF2") != 0) { /* WPA2 PMF 2 */
            config_key_mgmt = VAL_KEY_MGMT_WPA_PSK_SHA256;
        } else if (strstr(params[KEY_MGMT_PROTO_IDX], "RSN_PMF1") != 0) { /* WPA2 PMF 1 */
            config_key_mgmt = VAL_KEY_MGMT_WPA_PSK;

        } else if (params[MODE_IDX][0] == MODE_STA
            && strstr(params[KEY_MGMT_PROTO_IDX], "RSN_PMF3") != 0) { // Station Only : WPA-PSK WPA-PSK-SHA256 PMF Optinal
            config_key_mgmt = VAL_KEY_MGMT_WPA_PSK_WPA_PSK_SHA256;
        } else
#endif /* __SUPPORT_IEEE80211W__ */
        {
            config_key_mgmt = VAL_KEY_MGMT_WPA_PSK;
        }

        /* key_mgmt */
        memset(ra6w1_cli_cmd, 0, WLANINIT_CMD_BUF_SIZE);
        sprintf(ra6w1_cli_cmd, "%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s",
            CMD_SET_NETWORK,
            params[PROFILE_IDX],
            CMD2_KEY_MGMT,
            config_key_mgmt); /* cli set_network 0 key_mgmt WPA-PSK or SAE */

        memset(reply, 0, REPLY_LEN);
#ifdef __DEBUG_WLANINIT_CMD_PARSER__
        printf("[%s:%d] ra6w1_cli_cmd=%s\n", __FUNCTION__, __LINE__, ra6w1_cli_cmd);
#endif /* __DEBUG_WLANINIT_CMD_PARSER__ */
        ret = ra6w1_cli_reply(ra6w1_cli_cmd, CLI_DELIMIT_TAB, reply);
        if (ret == -1 || !strcmp(reply, REPLY_FAIL)) {
            goto CMD_ERROR;
        }

        /*  PMF(ieee80211w): Disable, Optional, Mandatory ==================== */
#if defined __SUPPORT_IEEE80211W__
        {
            int pmf_flag = NO_MGMT_FRAME_PROTECTION;
#if defined __SUPPORT_WPA3_SAE__
            if (strcmp(params[KEY_MGMT_PROTO_IDX], "RSN_SAE") == 0
                || strstr(params[KEY_MGMT_PROTO_IDX], "RSN_PMF1") != 0
                || strstr(params[KEY_MGMT_PROTO_IDX], "RSN_PMF3") != 0) { /* WPA1, WPA2 PMF-Optional */
                pmf_flag = MGMT_FRAME_PROTECTION_OPTIONAL;
            } else if (strcmp(params[KEY_MGMT_PROTO_IDX], "SAE") == 0
                || strstr(params[KEY_MGMT_PROTO_IDX], "RSN_PMF2") != 0) { /* SAE, WPA2 PMF-Mandatory */ // RSN_PMF2_SAE, SAE
                pmf_flag = MGMT_FRAME_PROTECTION_REQUIRED;
            }
#else
            if (strstr(params[KEY_MGMT_PROTO_IDX], "RSN_PMF1") != 0) { /* WPA1, WPA2 PMF-Optional */
                pmf_flag = MGMT_FRAME_PROTECTION_OPTIONAL;
            } else if (strstr(params[KEY_MGMT_PROTO_IDX], "RSN_PMF2") != 0) { /* WPA2 PMF-Mandatory*/
                pmf_flag = MGMT_FRAME_PROTECTION_REQUIRED;
            }
#endif /* __SUPPORT_WPA3_SAE__ */

            if (pmf_flag > 0) {
                /* ieee80211w */
                memset(ra6w1_cli_cmd, 0, WLANINIT_CMD_BUF_SIZE);
                sprintf(ra6w1_cli_cmd, "%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%d",
                    CMD_SET_NETWORK,
                    params[PROFILE_IDX],
                    CMD2_PMF_80211W,
                    pmf_flag); /* cli set_network 0 ieee80211w x */

                memset(reply, 0, REPLY_LEN);
#ifdef __DEBUG_WLANINIT_CMD_PARSER__
                    printf("[%s:%d] ra6w1_cli_cmd=%s\n", __FUNCTION__, __LINE__, ra6w1_cli_cmd);
#endif /* __DEBUG_WLANINIT_CMD_PARSER__ */
                ret = ra6w1_cli_reply(ra6w1_cli_cmd, CLI_DELIMIT_TAB, reply);
                if (ret == -1 || !strcmp(reply, REPLY_FAIL)) {
                    goto CMD_ERROR;
                }
            }
        }
#endif /* __SUPPORT_IEEE80211W__ */

        /* Pairwise Cipher (TKIP, CCMP, CCMP TKIP) ========================== */
        if (strcmp(params[KEY_MGMT_PAIRWISE_CIPHER_IDX], "TKIP_CCMP") == 0) {
            // TKIP CCMP
            /* pairwise */
            memset(ra6w1_cli_cmd, 0, WLANINIT_CMD_BUF_SIZE);
            sprintf(ra6w1_cli_cmd, "%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s",
                CMD_SET_NETWORK,
                params[PROFILE_IDX],
                CMD2_PAIRWISE,
                VAL_PAIRWISE_TKIP,
                VAL_PAIRWISE_AES_CCMP); /* cli set_network 0 pairwise WPA RSN */

            memset(reply, 0, REPLY_LEN);
#ifdef __DEBUG_WLANINIT_CMD_PARSER__
            printf("[%s:%d] ra6w1_cli_cmd=%s\n", __FUNCTION__, __LINE__, ra6w1_cli_cmd);
#endif /* __DEBUG_WLANINIT_CMD_PARSER__ */
            ret = ra6w1_cli_reply(ra6w1_cli_cmd, CLI_DELIMIT_TAB, reply);
            if (ret == -1 || !strcmp(reply, REPLY_FAIL)) {
                goto CMD_ERROR;
            }
        } else  {
            /* pairwise */
            memset(ra6w1_cli_cmd, 0, WLANINIT_CMD_BUF_SIZE);
            sprintf(ra6w1_cli_cmd, "%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s",
                CMD_SET_NETWORK,
                params[PROFILE_IDX],
                CMD2_PAIRWISE,
                params[KEY_MGMT_PAIRWISE_CIPHER_IDX]); /* cli set_network 0 pairwise  params[5] */

            memset(reply, 0, REPLY_LEN);
#ifdef __DEBUG_WLANINIT_CMD_PARSER__
            printf("[%s:%d] ra6w1_cli_cmd=%s\n", __FUNCTION__, __LINE__, ra6w1_cli_cmd);
#endif /* __DEBUG_WLANINIT_CMD_PARSER__ */
            ret = ra6w1_cli_reply(ra6w1_cli_cmd, CLI_DELIMIT_TAB, reply);
        }

        /* PSK Key or SAE_Password ========================================== */
        /* psk */
        memset(ra6w1_cli_cmd, 0, WLANINIT_CMD_BUF_SIZE);
        sprintf(ra6w1_cli_cmd, "%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s",
            CMD_SET_NETWORK,
            params[PROFILE_IDX],
#ifdef __SUPPORT_WPA3_SAE__
            strcmp(params[KEY_MGMT_PROTO_IDX], "SAE") == 0 ? CMD2_SAE_PASSWORD : CMD2_PSK_KEY,
#else
            CMD2_PSK_KEY,
#endif /* __SUPPORT_WPA3_SAE__ */
            params[KEY_MGMT_PSK_KEY_IDX]); /* cli set_network 0 psk    params[KEY_MGMT_PSK_KEY_IDX] */

        memset(reply, 0, REPLY_LEN);
#ifdef __DEBUG_WLANINIT_CMD_PARSER__
        printf("[%s:%d] ra6w1_cli_cmd=%s\n", __FUNCTION__, __LINE__, ra6w1_cli_cmd);
#endif /* __DEBUG_WLANINIT_CMD_PARSER__ */
        ret = ra6w1_cli_reply(ra6w1_cli_cmd, CLI_DELIMIT_TAB, reply);
        if (ret == -1 || !strcmp(reply, REPLY_FAIL)) {
            goto CMD_ERROR;
        }

#ifdef __SUPPORT_WPA3_SAE__
        if (config_key_mgmt && strstr(config_key_mgmt, "SAE")) {
            memset(ra6w1_cli_cmd, 0, WLANINIT_CMD_BUF_SIZE);
            sprintf(ra6w1_cli_cmd, "%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s",
                    CMD_SET_NETWORK,
                    params[PROFILE_IDX],
                    CMD2_SAE_PASSWORD,
                    params[KEY_MGMT_PSK_KEY_IDX]); /* cli set_network 0 sea_password    params[KEY_MGMT_PSK_KEY_IDX] */

            memset(reply, 0, REPLY_LEN);
#ifdef __DEBUG_WLANINIT_CMD_PARSER__
            printf("[%s:%d] ra6w1_cli_cmd=%s\n", __FUNCTION__, __LINE__, ra6w1_cli_cmd);
#endif /* __DEBUG_WLANINIT_CMD_PARSER__ */
            ret = ra6w1_cli_reply(ra6w1_cli_cmd, CLI_DELIMIT_TAB, reply);
            if (ret == -1 || !strcmp(reply, REPLY_FAIL)) {
                goto CMD_ERROR;
            }
        }
#endif /* __SUPPORT_WPA3_SAE__ */
    }
#ifdef __SUPPORT_WPA_ENTERPRISE_CORE__
    /* WPA Enterprise, Station Only *******************************************/
    else if (params[PROFILE_IDX][0] == '0' && params[MODE_IDX][0] == MODE_STA
            && strstr(params[KEY_MGMT_PROTO_IDX], "EAP") != 0) {
        enum
        {
            WPA1_2,
            WPA2_3_128B,
            WPA3_128B,
            WPA3_192B
        };

        char tmp_cmd[32];
        unsigned char wpa_mode = WPA1_2;

       /*
            cli set_network 0 proto RSN WPA
            cli set_network 0 pairwise CCMP TKIP
            cli set_network 0 key_mgmt WPA-EAP
            cli set_network 0 eap PEAP TTLS
            cli set_network 0 phase2 'auth=MSCHAPV2 GTC'
            cli set_network 0 identity 'xxx'
            cli set_network 0 password 'xxx'
            cli select_network 0
            */

       /* WPA3-ENT-192Bits
            cli set_network 0 proto RSN
            cli set_network 0 key_mgmt WPA-EAP-SUITE-B-192
            cli set_network 0 pairwise GCMP-256
            cli set_network 0 group GCMP-256
            cli set_network 0 group_mgmt BIP-GMAC-256
            cli set_network 0 ieee80211w 2
            cli set_network 0 eap PEAP TTLS
            cli set_network 0 phase2 'auth=MSCHAPV2'
            cli set_network 0 identity 'xxxx'
            cli set_network 0 password 'xxxx'
            */

        // WPA3-Enterprise 192Bits // (key_mgmt WPA-EAP-SUITE-B-192, proto RSN, ieee80211w 2)
        if (strstr(params[KEY_MGMT_PROTO_IDX], "EAP-WPA3-192B") != 0) {
            wpa_mode = WPA3_192B;
        }

        // WPA3-Enterprise 128Bits // WPA3-ENT (key_mgmt WPA-EAP-SHA256, proto RSN, pairwise CCMP ieee80211w 2,)
        else if (strstr(params[KEY_MGMT_PROTO_IDX], "EAP-WPA3_") != 0) {
            wpa_mode = WPA3_128B;
        }

        // WPA2/3-Enterprise 128Bits // WPA2/3-ENT (key_mgmt WPA-EAP-SHA256 WPA-EAP, proto RSN, pairwise CCMP, ieee80211w 1)
        else if (strstr(params[KEY_MGMT_PROTO_IDX], "EAP-WPA23") != 0) {
            wpa_mode = WPA2_3_128B;
        }

        // WPA1/2-Enterprise // WPA1/2-ENT (key_mgmt WPA-EAP, proto RSN WPA, pairwise CCMP TKIP, ieee80211w 0)
        else if (strstr(params[KEY_MGMT_PROTO_IDX], "EAP-WPA12") != 0) {
            wpa_mode = WPA1_2;
        }
        else
        {
            wpa_mode = WPA1_2;
        }

        // key_mgmt
        memset(ra6w1_cli_cmd, 0, WLANINIT_CMD_BUF_SIZE);
        memset(tmp_cmd, 0, 32);

        switch (wpa_mode)
        {
            case WPA3_192B:
                strcpy(tmp_cmd, VAL_KEY_MGMT_EAP192B);
                break;

            case WPA3_128B:
                strcpy(tmp_cmd, VAL_KEY_MGMT_EAPS256);
                break;

            case WPA2_3_128B:
                strcpy(tmp_cmd, VAL_KEY_MGMT_EAPS256_EAP);
                break;

            case WPA1_2:
            default:
                strcpy(tmp_cmd, VAL_KEY_MGMT_EAP);
                break;
        }

        sprintf(ra6w1_cli_cmd, "%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s",
            CMD_SET_NETWORK,
            params[PROFILE_IDX],
            CMD2_KEY_MGMT, tmp_cmd); /* cli set_network 0 key_mgmt eap */

        memset(reply, 0, REPLY_LEN);
#ifdef __DEBUG_WLANINIT_CMD_PARSER__
        printf("[%s:%d] ra6w1_cli_cmd=%s\n", __FUNCTION__, __LINE__, ra6w1_cli_cmd);
#endif /* __DEBUG_WLANINIT_CMD_PARSER__ */
        ret = ra6w1_cli_reply(ra6w1_cli_cmd, CLI_DELIMIT_TAB, reply);
        if (ret == -1 || !strcmp(reply, REPLY_FAIL)) {
            goto CMD_ERROR;
        }


        // proto : RSN WPA
        memset(ra6w1_cli_cmd, 0, WLANINIT_CMD_BUF_SIZE);
        memset(tmp_cmd, 0, 32);

        switch (wpa_mode)
        {
            case WPA3_192B:
            case WPA3_128B:
            case WPA2_3_128B:
                strcpy(tmp_cmd, VAL_PROTO_WPA2_RSN); // RSN Only
                break;

            case WPA1_2:
            default:
                strcpy(tmp_cmd, VAL_PROTO_WPA1_2); // RSN WPA
                break;
        }

        sprintf(ra6w1_cli_cmd, "%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s",
            CMD_SET_NETWORK,
            params[PROFILE_IDX],
            CMD2_PROTO,
            tmp_cmd); /* cli set_network 0 proto RSN WPA */

        memset(reply, 0, REPLY_LEN);
#ifdef __DEBUG_WLANINIT_CMD_PARSER__
        printf("[%s:%d] ra6w1_cli_cmd=%s\n", __FUNCTION__, __LINE__, ra6w1_cli_cmd);
#endif /* __DEBUG_WLANINIT_CMD_PARSER__ */
        ret = ra6w1_cli_reply(ra6w1_cli_cmd, CLI_DELIMIT_TAB, reply);
        if (ret == -1 || !strcmp(reply, REPLY_FAIL)) {
            goto CMD_ERROR;
        }

        // pairwise: TKIP CCMP
        memset(ra6w1_cli_cmd, 0, WLANINIT_CMD_BUF_SIZE);

        memset(tmp_cmd, 0, 32);

        switch (wpa_mode)
        {
            case WPA3_192B:
                strcpy(tmp_cmd, VAL_PAIRWISE_GCMP_256); // GCMP-256
                break;

            case WPA3_128B:
            case WPA2_3_128B:
                strcpy(tmp_cmd, VAL_PAIRWISE_AES_CCMP); // CCMP Only
                break;

            case WPA1_2:
            default:
                strcpy(tmp_cmd, VAL_PAIRWISE_TKIP_CCMP); // CCMP TKIP
                break;
        }

        sprintf(ra6w1_cli_cmd, "%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s",
            CMD_SET_NETWORK,
            params[PROFILE_IDX],
            CMD2_PAIRWISE,
            tmp_cmd); /* cli set_network 0 pairwise CCMP TKIP */

        memset(reply, 0, REPLY_LEN);
#ifdef __DEBUG_WLANINIT_CMD_PARSER__
        printf("[%s:%d] ra6w1_cli_cmd=%s\n", __FUNCTION__, __LINE__, ra6w1_cli_cmd);
#endif /* __DEBUG_WLANINIT_CMD_PARSER__ */
        ret = ra6w1_cli_reply(ra6w1_cli_cmd, CLI_DELIMIT_TAB, reply);
        if (ret == -1 || !strcmp(reply, REPLY_FAIL)) {
            goto CMD_ERROR;
        }

        // group
        if (wpa_mode == WPA3_192B)         // WPA3-ENT 192Bits Only
        {
            // group : GCMP-256
            memset(ra6w1_cli_cmd, 0, WLANINIT_CMD_BUF_SIZE);
            sprintf(ra6w1_cli_cmd, "%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s",
                CMD_SET_NETWORK,
                params[PROFILE_IDX],
                CMD2_GROUP,
                VAL_GROUP_GCMP_256); /* cli set_network 0 key_mgmt eap */

            memset(reply, 0, REPLY_LEN);
#ifdef __DEBUG_WLANINIT_CMD_PARSER__
            printf("[%s:%d] ra6w1_cli_cmd=%s\n", __FUNCTION__, __LINE__, ra6w1_cli_cmd);
#endif /* __DEBUG_WLANINIT_CMD_PARSER__ */
            ret = ra6w1_cli_reply(ra6w1_cli_cmd, CLI_DELIMIT_TAB, reply);
            if (ret == -1 || !strcmp(reply, REPLY_FAIL)) {
                goto CMD_ERROR;
            }
            
            // group_mgmt BIP-GMAC-256
            memset(ra6w1_cli_cmd, 0, WLANINIT_CMD_BUF_SIZE);
            sprintf(ra6w1_cli_cmd, "%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s",
                CMD_SET_NETWORK,
                params[PROFILE_IDX],
                CMD2_GROUP_MGMT,
                VAL_GROUP_MGMT_BIP_GMAC_256); /* cli set_network 0 key_mgmt eap */


            memset(reply, 0, REPLY_LEN);
#ifdef __DEBUG_WLANINIT_CMD_PARSER__
            printf("[%s:%d] ra6w1_cli_cmd=%s\n", __FUNCTION__, __LINE__, ra6w1_cli_cmd);
#endif /* __DEBUG_WLANINIT_CMD_PARSER__ */
            ret = ra6w1_cli_reply(ra6w1_cli_cmd, CLI_DELIMIT_TAB, reply);
            if (ret == -1 || !strcmp(reply, REPLY_FAIL)) {
                goto CMD_ERROR;
            }
        }

        // ieee80211w(PMF) 0 1 2 
        memset(ra6w1_cli_cmd, 0, WLANINIT_CMD_BUF_SIZE);

        memset(tmp_cmd, 0, 32);

        switch (wpa_mode)
        {
            case WPA3_192B:
            case WPA3_128B:
                strcpy(tmp_cmd, "2"); // 2  PMF Required 
                break;

            case WPA2_3_128B:
                strcpy(tmp_cmd, "1"); // 1 PMF Capable 
                break;

            case WPA1_2:
            default:
                strcpy(tmp_cmd, "0"); // 0 PMF Disable 
                break;
        }

        sprintf(ra6w1_cli_cmd, "%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s",
            CMD_SET_NETWORK,
            params[PROFILE_IDX],
            CMD2_PMF_80211W, tmp_cmd); /* cli set_network 0 ieee80211w 1 */


        memset(reply, 0, REPLY_LEN);
#ifdef __DEBUG_WLANINIT_CMD_PARSER__
            printf("[%s:%d] ra6w1_cli_cmd=%s\n", __FUNCTION__, __LINE__, ra6w1_cli_cmd);
#endif /* __DEBUG_WLANINIT_CMD_PARSER__ */
        ret = ra6w1_cli_reply(ra6w1_cli_cmd, CLI_DELIMIT_TAB, reply);
        if (ret == -1 || !strcmp(reply, REPLY_FAIL)) {
            goto CMD_ERROR;
        }


        // eap PEAP TTLS FAST
        memset(tmp_cmd, 0, 32);

        if (strstr(params[EAP_METHOD_IDX], "PEAP"))
        {
            strcat(tmp_cmd, "PEAP");
        }

        if (strstr(params[EAP_METHOD_IDX], "TTLS"))
        {
            if (strlen(tmp_cmd) > 0)
            {
                strcat(tmp_cmd, " ");
            }
            strcat(tmp_cmd, "TTLS");        
        }

        if (strstr(params[EAP_METHOD_IDX], "FAST"))
        {
            if (strlen(tmp_cmd) > 0)
            {
                strcat(tmp_cmd, " ");
            }
            strcat(tmp_cmd, "FAST");
        }

        if (strstr(params[EAP_METHOD_IDX], "_TLS") != 0)
        {
            strcpy(tmp_cmd, "TLS");
        }
        
        memset(ra6w1_cli_cmd, 0, WLANINIT_CMD_BUF_SIZE);
        sprintf(ra6w1_cli_cmd, "%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s",
            CMD_SET_NETWORK,
            params[PROFILE_IDX],
            "eap",
            tmp_cmd); /* cli set_network 0 eap PEAP TTLS FAST */

        memset(reply, 0, REPLY_LEN);
#ifdef __DEBUG_WLANINIT_CMD_PARSER__
        printf("[%s:%d] ra6w1_cli_cmd=%s\n", __FUNCTION__, __LINE__, ra6w1_cli_cmd);
#endif /* __DEBUG_WLANINIT_CMD_PARSER__ */
        ret = ra6w1_cli_reply(ra6w1_cli_cmd, CLI_DELIMIT_TAB, reply);
        if (ret == -1 || !strcmp(reply, REPLY_FAIL)) {
            goto CMD_ERROR;
        }

        if (strstr(params[EAP_METHOD_IDX], "_TLS") == 0)
        {
            if (strstr(params[EAP_METHOD_IDX], "FAST"))
            {
                //phase1
                memset(ra6w1_cli_cmd, 0, WLANINIT_CMD_BUF_SIZE);
                sprintf(ra6w1_cli_cmd, "%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"\'%s\'",
                    CMD_SET_NETWORK,
                    params[PROFILE_IDX],
                    "phase1",
                    "fast_provisioning=2"); /* cli set_network 0 phase1 fast_provisioning=2 */

                memset(reply, 0, REPLY_LEN);
#ifdef __DEBUG_WLANINIT_CMD_PARSER__
                printf("[%s:%d] ra6w1_cli_cmd=%s\n", __FUNCTION__, __LINE__, ra6w1_cli_cmd);
#endif /* __DEBUG_WLANINIT_CMD_PARSER__ */
                ret = ra6w1_cli_reply(ra6w1_cli_cmd, CLI_DELIMIT_TAB, reply);
                if (ret == -1 || !strcmp(reply, REPLY_FAIL)) {
                    goto CMD_ERROR;
                }
            }

            if (strstr(params[EAP_METHOD_IDX], "PEAP") || strstr(params[EAP_METHOD_IDX], "TTLS"))
            {
				char phase2_cmd[10];

                memset(tmp_cmd, 0, 32);
				memset(phase2_cmd, 0, 10);
				strcpy(phase2_cmd, "auth");

				if (strstr(params[EAP_PHASE2_METHOD_IDX], "TLS"))
				{
					strcat(tmp_cmd, "TLS");
					if(strstr(params[EAP_METHOD_IDX], "TTLS"))
					{
						strcpy(phase2_cmd, "autheap");
					}
				}

                if (strstr(params[EAP_PHASE2_METHOD_IDX], "MSCHAPV2"))
                {
                    strcat(tmp_cmd, "MSCHAPV2");
                }

                if (strstr(params[EAP_PHASE2_METHOD_IDX], "GTC"))
                {
                    if (strlen(tmp_cmd) > 0)
                    {
                        strcat(tmp_cmd, " ");
                    }
                    strcat(tmp_cmd, "GTC");        
                }

                //phase2
                memset(ra6w1_cli_cmd, 0, WLANINIT_CMD_BUF_SIZE);
                sprintf(ra6w1_cli_cmd, "%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"\'%s=%s\'",
                    CMD_SET_NETWORK,
                    params[PROFILE_IDX],
                    "phase2",
                    phase2_cmd,
                    tmp_cmd); /* cli set_network 0 phase2 auth=MSCHAPV2 GTC */

                memset(reply, 0, REPLY_LEN);
#ifdef __DEBUG_WLANINIT_CMD_PARSER__
                printf("[%s:%d] ra6w1_cli_cmd=%s\n", __FUNCTION__, __LINE__, ra6w1_cli_cmd);
#endif /* __DEBUG_WLANINIT_CMD_PARSER__ */
                ret = ra6w1_cli_reply(ra6w1_cli_cmd, CLI_DELIMIT_TAB, reply);
                if (ret == -1 || !strcmp(reply, REPLY_FAIL)) {
                    goto CMD_ERROR;
                }
            }
        }

        // id
        memset(ra6w1_cli_cmd, 0, WLANINIT_CMD_BUF_SIZE);
        sprintf(ra6w1_cli_cmd, "%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s",
            CMD_SET_NETWORK,
            params[PROFILE_IDX],
            "identity",
            params[EAP_ID_IDX]); /* cli set_network 0 pairwise WPA RSN */

        memset(reply, 0, REPLY_LEN);
#ifdef __DEBUG_WLANINIT_CMD_PARSER__
        printf("[%s:%d] ra6w1_cli_cmd=%s\n", __FUNCTION__, __LINE__, ra6w1_cli_cmd);
#endif /* __DEBUG_WLANINIT_CMD_PARSER__ */
        ret = ra6w1_cli_reply(ra6w1_cli_cmd, CLI_DELIMIT_TAB, reply);
        if (ret == -1 || !strcmp(reply, REPLY_FAIL)) {
            goto CMD_ERROR;
        }

        // password
        if (strstr(params[EAP_METHOD_IDX], "_TLS") == 0)
        {
            memset(ra6w1_cli_cmd, 0, WLANINIT_CMD_BUF_SIZE);
            sprintf(ra6w1_cli_cmd, "%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s",
                CMD_SET_NETWORK,
                params[PROFILE_IDX],
                "password",
                params[EAP_PW_IDX]); /* cli set_network 0 password '   ' */

            memset(reply, 0, REPLY_LEN);
#ifdef __DEBUG_WLANINIT_CMD_PARSER__
            printf("[%s:%d] ra6w1_cli_cmd=%s\n", __FUNCTION__, __LINE__, ra6w1_cli_cmd);
#endif /* __DEBUG_WLANINIT_CMD_PARSER__ */
            ret = ra6w1_cli_reply(ra6w1_cli_cmd, CLI_DELIMIT_TAB, reply);
            if (ret == -1 || !strcmp(reply, REPLY_FAIL)) {
                goto CMD_ERROR;
            }
        }
    }
#endif // __SUPPORT_WPA_ENTERPRISE_CORE__
    else {
        strcpy(reply, "FAIL");
        //strcpy(ra6w1_cli_cmd, "Security Mode");
        sprintf(ra6w1_cli_cmd, "Security Mode: %s", params[KEY_MGMT_PROTO_IDX]);
        goto CMD_ERROR;
    }
    /* END Security Mode ******************************************************/

    /* COUNTRY CODE & CHANNEL *************************************************/
    if ((params[MODE_IDX][0] == MODE_AP
#ifdef __SUPPORT_MESH__
        || params[MODE_IDX][0] == MODE_MESH_POINT
#endif /* __SUPPORT_MESH__ */
        )
        && (param_cnt >= CHANNEL_IDX
        || strcmp(params[KEY_MGMT_PROTO_IDX], "OPEN") == 0
        || strcmp(params[KEY_MGMT_PROTO_IDX], "OWE") == 0)) {

        /* OPEN MODE : COUNTRY CODE & CHANNEL */
        if (((strcmp(params[KEY_MGMT_PROTO_IDX], "OPEN") == 0) || (strcmp(params[KEY_MGMT_PROTO_IDX], "OWE") == 0))
            && (param_cnt >= OPEN_MODE_CHANNEL_IDX)) {

#if 0 // setup_apply_preset() already done.
            /* COUNTRY CODE */
            if (param_cnt >= OPEN_MODE_COUNTRY_CODE_IDX) {

                memset(ra6w1_cli_cmd, 0, WLANINIT_CMD_BUF_SIZE);
                sprintf(ra6w1_cli_cmd, "%s"CLI_DELIMIT_TAB"%s",
                    CMD_COUNTRY,
                    params[OPEN_MODE_COUNTRY_CODE_IDX]); /* cli country [country code] */

                memset(reply, 0, REPLY_LEN);
#ifdef __DEBUG_WLANINIT_CMD_PARSER__
                printf("[%s:%d] ra6w1_cli_cmd=%s\n", __FUNCTION__, __LINE__, ra6w1_cli_cmd);
#endif /* __DEBUG_WLANINIT_CMD_PARSER__ */
                ret = ra6w1_cli_reply(ra6w1_cli_cmd, CLI_DELIMIT_TAB, reply);
                if (ret == -1 || !strcmp(reply, REPLY_FAIL)) {
                    goto CMD_ERROR;
                }
            }
#endif    /*0*/

            /* CHANNEL */
            memset(ra6w1_cli_cmd, 0, WLANINIT_CMD_BUF_SIZE);
            sprintf(ra6w1_cli_cmd, "%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s",
                CMD_SET_NETWORK,
                params[PROFILE_IDX],
                CMD2_CHANNEL,
                params[OPEN_MODE_CHANNEL_IDX]);

            memset(reply, 0, REPLY_LEN);
#ifdef __DEBUG_WLANINIT_CMD_PARSER__
            printf("[%s:%d] ra6w1_cli_cmd=%s\n", __FUNCTION__, __LINE__, ra6w1_cli_cmd);
#endif /* __DEBUG_WLANINIT_CMD_PARSER__ */
            ret = ra6w1_cli_reply(ra6w1_cli_cmd, CLI_DELIMIT_TAB, reply);
            if (ret == -1 || !strcmp(reply, REPLY_FAIL)) {
                goto CMD_ERROR;
            }
        } else {
            /* SECURITY MODE : COUNTRY CODE & CHANNEL */
#if 0 // setup_apply_preset() already did
            /* COUNTRY CODE */
            if (param_cnt >= OPEN_MODE_COUNTRY_CODE_IDX) {

                memset(ra6w1_cli_cmd, 0, WLANINIT_CMD_BUF_SIZE);
                sprintf(ra6w1_cli_cmd, "%s"CLI_DELIMIT_TAB"%s",
                    CMD_COUNTRY,
                    params[COUNTRY_CODE_IDX]); /* cli country [country code] */

                memset(reply, 0, REPLY_LEN);
#ifdef __DEBUG_WLANINIT_CMD_PARSER__
                printf("[%s:%d] ra6w1_cli_cmd=%s\n", __FUNCTION__, __LINE__, ra6w1_cli_cmd);
#endif /* __DEBUG_WLANINIT_CMD_PARSER__ */
                ret = ra6w1_cli_reply(ra6w1_cli_cmd, CLI_DELIMIT_TAB, reply);
                if (ret == -1 || !strcmp(reply, REPLY_FAIL)) {
                    goto CMD_ERROR;
                }
            }
#endif    /*0*/
            /* CHANNEL */
            memset(ra6w1_cli_cmd, 0, WLANINIT_CMD_BUF_SIZE);
            sprintf(ra6w1_cli_cmd, "%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s"CLI_DELIMIT_TAB"%s",
                CMD_SET_NETWORK,
                params[PROFILE_IDX],
                CMD2_CHANNEL,
                params[CHANNEL_IDX]);

            memset(reply, 0, REPLY_LEN);
#ifdef __DEBUG_WLANINIT_CMD_PARSER__
            printf("[%s:%d] ra6w1_cli_cmd=%s\n", __FUNCTION__, __LINE__, ra6w1_cli_cmd);
#endif /* __DEBUG_WLANINIT_CMD_PARSER__ */
            ret = ra6w1_cli_reply(ra6w1_cli_cmd, CLI_DELIMIT_TAB, reply);
            if (ret == -1 || !strcmp(reply, REPLY_FAIL)) {
                goto CMD_ERROR;
            }
        }
    }

    return 0;

CMD_ERROR:

    if (ret != 0 || !strcmp(reply, REPLY_FAIL)) {
        printf("%s: %s\n", reply, ra6w1_cli_cmd);
    }

    return ret;
}

/* EOF */
