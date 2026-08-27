/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef __TCP_CLIENT_H__
#define __TCP_CLIENT_H__

#include "rm_wifi.h"

#if (TCP_CLIENT_APP_START == 1)

#include "FreeRTOS.h"
#include "custom_config_sdk.h"
#include "common_def.h"
#include "sys_app_defs.h"
#include "util_api.h"
#include "rm_vee_flash_w_rrq_nvram.h"

#define TCPC_DEF_ACTIVE                 0
#define TCPC_DEF_LOCAL_PORT             10192
#define TCPC_DEF_BUF_SIZE               (1024 * 4)
#define TCPC_DEF_PEER_IP_ADDR           "192.168.1.138"
#define TCPC_DEF_PEER_PORT              TCPC_DEF_LOCAL_PORT
#define TCPC_DEF_SEND_PERIOD            10000   // ms
#define TCPC_MIN_SEND_PERIOD            1000    // ms
#define TCPC_MAX_SEND_PERIOD            3600000   // ms
#define TCPC_DEF_SEND_DATA_SIZE         64      // bytes
#define TCPC_MIN_SEND_DATA_SIZE         64      // bytes
#define TCPC_MAX_SEND_DATA_SIZE         1400    // bytes
#define TCPC_DEF_AUTO_RESTART_AT_EXIT   0       // don't restart tcpc after exit due to connection failure
#define TCPC_DEF_KA_ENABLE              1
#define TCPC_DEF_KA_IDLE_TIME           120
#define TCPC_DEF_KA_INTVL_TIME          30
#define TCPC_DEF_KA_MAX_PROBES          9


#define TCPC_DEF_RECV_TIMEOUT   100     //ms
#define TCPC_DEF_SEND_TIMEOUT   3000    //ms

#define TCPC_PEER_IP_ADDR_LEN   64

#define TCPC_NVR_ACTIVE                 "TCPC_ACTIVE"
#define TCPC_NVR_PEER_IP_ADDR           "TCPC_PEER_IP_ADDR"
#define TCPC_NVR_PEER_PORT              "TCPC_PEER_PORT"
#define TCPC_NVR_SEND_PERIOD            "TCPC_SEND_PERIOD"
#define TCPC_NVR_SEND_DATA_SIZE         "TCPC_SEND_DATA_SIZE"
#define TCPC_NVR_AUTO_RESTART_AT_EXIT   "TCPC_AUTO_REST_EXIT"
#define TCPC_NVR_KA_ENABLE              "TCPC_KA_ENABLE"
#define TCPC_NVR_KA_IDLE_TIME           "TCPC_KA_IDLE_TIME"
#define TCPC_NVR_KA_INTVL_TIME          "TCPC_KA_INTVL_TIME"
#define TCPC_NVR_KA_MAX_PROBES          "TCPC_KA_MAX_PROBES"

#define TCPC_RTM_NAME           "TCPC_CONF"

/// Define the jobs of TCP_Client that join PMGR_DPM system
#define JOB_ID_SEND       "JOB_SEND"
#define JOB_ID_RECV       "JOB_RECV"
#define JOB_ID_TCPC_CONN  "JOB_TCPC_CONN"

#define TCPC_TIMER_NAME         "TCPC_T"
#define TCPC_TASK_SIZE          (1024)  //word

#define TCPC_CLI_ACTIVE                 "active"
#define TCPC_CLI_PEER_IP_ADDR           "peer_ip_addr"
#define TCPC_CLI_PEER_PORT              "peer_port"
#define TCPC_CLI_SEND_PERIOD            "send_period"
#define TCPC_CLI_SEND_DATA_SIZE         "send_data_size"
#define TCPC_CLI_USAGE                  "help"
#define TCPC_CLI_STATUS                 "status"
#define TCPC_CLI_AUTO_RESTART_AT_EXIT   "autostart_atexit"
#define TCPC_CLI_KEEPALIVE              "keepalive"

typedef struct {
    int active;
    char peer_ip_addr[TCPC_PEER_IP_ADDR_LEN];
    int peer_port;

    int send_period;
    int send_data_size;
    int autostart_atexit;

    // TCP Keepalive params
    int ka_enable;
    int ka_idle_time;
    int ka_intvl_time;
    int ka_max_probes;
} tcpc_conf_t;

bool cmd_tcpc_conf(int argc, char *argv[]);
void tcpc_task_starter(void *param);

/// Define events for feature "TCPC_CLI_AUTO_RESTART_AT_EXIT"
#define EVT_TCPC_DISCONN          (1UL << (0))
#define EVT_TCPC_CONN             (1UL << (1))
#define EVT_TCPC_EXIT             (1UL << (2))
#define EVT_TCPC_ANY              (EVT_TCPC_DISCONN | EVT_TCPC_CONN | EVT_TCPC_EXIT)

#endif

#endif // __TCP_CLIENT_H__


