/**
 ****************************************************************************************
 *
 * @file rm_wifi/user_app/sys_app_defs.h
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

#ifndef __SYS_APP_DEFS_H__
#define __SYS_APP_DEFS_H__

#include "FreeRTOS.h"
#include "custom_config_sdk.h"

#include <stdio.h>


/*****************************************************************************
 *    DEVICE
 *****************************************************************************/


/******************************************************************************
 *    NET
 ******************************************************************************/


/******************************************************************************
 * Application threads
 ******************************************************************************/



/* For System and applications */
#define RUN_STA_MODE         0x1
#define RUN_AP_MODE          0x2

#if defined ( __SUPPORT_P2P__ )
#define RUN_P2P_MODE         0x4
#else
#define RUN_P2P_MODE         0x0
#endif // __SUPPORT_P2P__

#if defined ( __SUPPORT_WIFI_CONCURRENT__ )
#define RUN_STA_SOFTAP_MODE  0x8
#else
#define RUN_STA_SOFTAP_MODE  0x0
#endif // __SUPPORT_WIFI_CONCURRENT__

#define RUN_ALL_MODE_WO_P2P  (RUN_STA_MODE | RUN_AP_MODE | RUN_STA_SOFTAP_MODE)
#define RUN_ALL_MODE_W_P2P   (RUN_STA_MODE | RUN_AP_MODE | RUN_P2P_MODE | RUN_STA_SOFTAP_MODE)

#define RUN_BASIC_MODE       (RUN_STA_MODE | RUN_AP_MODE)
#define RUN_ALL_MODE         RUN_ALL_MODE_WO_P2P

typedef    struct    _app_task_info {
    /// Thread Name
    char    *name;

    /// Funtion Entry_point
    void     (*entry_func)(void *);

    /// Thread Stack Size
    uint16    stksize;

    /// Thread Priority
    uint16    priority;

    /// Flag to check network initializing
    uint8     net_chk_flag;

    /// Usage flag for DPM running
    uint8     dpm_flag;

    /// Port number for network communitation
    uint16    port_no;

    /// Running mode of RA6Wx
    int    run_sys_mode;
} app_task_info_t;

typedef    struct    _user_run_task_list_ {
    TaskHandle_t       task_handler;
    app_task_info_t    *info;
    struct _user_run_task_list_    *prev;
} run_app_task_info_t;


/*
 * Application Names ...
 */

/* sys_app_lists[] */
#define APP_INIT_SECURE_MODULE      "secure_module"
#define APP_EXT_WU_MON              "mon_ext_wakeup"
#define APP_ATCMD                   "at_cmd"
#define APP_UART2_MON               "uart2_mon"
#define APP_MQTT_SUB                "mqtt_sub"
#define APP_MQTT_PUB                "mqtt_pub"
#define APP_HTTP_CLIENT             "auto_Http_c"
#define APP_HTTP_SVR                "auto_http_s"
#define APP_HTTPS_SVR               "auto_https_s"
#define APP_AP_TIMER                "SoftAP_timer"
#define APP_WEBSOCKET_CLIENT        "websocket_c"


/* customer_app_tbl[] */
#define HELLO_WORLD_1               "helloWorld_1"
#define HELLO_WORLD_2               "helloWorld_2"

#define DPM_TIMER_TEST              "dpm_timer_test"
#define WIFI_CONN                   "conn_notify"

#define CUSTOMER_PROVISION          "custom_provision"

#define APP_POLL_STATE              "poll_state"
#define APP_MONITOR                 "monitor_svc"

/* For OTA Update sample */
#define APP_OTA_UPDATE              "ota_update"
#define OTA_UPDATE_MQTT_PORT        1884

#define FAST_SLEEPMODE_MOD          "sleepmode12"


/*
 * Defines for customer's applications
 */

#define TCP_SV_PORT                  5000
#define TCP_SESS_WIN_SZ              4096
#define TCP_PAYLOAD_SZ               640
#define PKT_POOL_CNT                 12
#define TCP_POOL_SIZE                (TCP_PAYLOAD_SZ * PKT_POOL_CNT)

#define TCP_RX_SZ                    4096
#define TCP_LISTEN_MAX_PEND          5


/******************************************************************************
 * External global functions
 ******************************************************************************/
extern int  get_run_mode(void);
#if defined (__SUPPORT_ATCMD__)
extern void start_atcmd(void);
#endif

#if defined ( __SUPPORT_DPM_EXT_WU_MON__ )
extern void dpm_ext_wu_mon(void *pvParameters);
#endif // __SUPPORT_DPM_EXT_WU_MON__

#if defined ( __SUPPORT_APMODE_SHUTDOWN_TIMER__ )
static void ap_mode_shutdown_tm(void *pvParameters);
#endif // __SUPPORT_APMODE_SHUTDOWN_TIMER__

#if defined ( __SUPPORT_SIGMA_TEST__ )
extern void sigma_host_init(void);
#endif    // __SUPPORT_SIGMA_TEST__

#if defined ( __SUPPORT_FATFS__ )
extern void init_fs_partition(void);
#endif    // __SUPPORT_FATFS__

#if defined ( __HTTP_SVR_AUTO_START__ )
extern void auto_run_http_svr(void *pvParameters);
#endif // __HTTP_SVR_AUTO_START__

#if defined (__ENABLE_SAMPLE_APP__)
extern void regist_sample_cb(void);
extern void sample_preconfig(void);
#endif    // __ENABLE_SAMPLE_APP__

#endif /* __SYS_APP_DEFS_H__ */

/* EOF */
