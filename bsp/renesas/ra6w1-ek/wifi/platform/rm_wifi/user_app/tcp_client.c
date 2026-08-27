/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include "tcp_client.h"

#if (TCP_CLIENT_APP_START == 1)

#include "lwip/sockets.h"

#if CFG_PMGR
#include "r_pm_if.h"
#include "rm_pmgr_w_instance.h"
#endif /* CFG_PMGR */

#include "net_common.h"
#include "iface_defs.h"
#ifdef RM_MAP_PERSISTANT_W
#include "rm_map_persistant_w.h"
#endif

#define TCPC_REMAIN_DATA_INIT -1

#undef  ENABLE_TCPC_DBG_INFO
#define ENABLE_TCPC_DBG_ERR

#define TCPC_DBG  printf

#if defined (ENABLE_TCPC_DBG_INFO)
#define TCPC_DBG_INFO(fmt, ...)   \
    TCPC_DBG("[%s:%d]" fmt, __func__, __LINE__, ##__VA_ARGS__)
#else
#define TCPC_DBG_INFO(...)        do {} while (0)
#endif  // (ENABLE_TCPC_DBG_INFO)

#if defined (ENABLE_TCPC_DBG_ERR)
#define TCPC_DBG_ERR(fmt, ...)    \
    TCPC_DBG("[%s:%d]" fmt, __func__, __LINE__, ##__VA_ARGS__)
#else
#define TCPC_DBG_ERR(...)         do {} while (0)
#endif // (ENABLE_TCPC_DBG_ERR) 

#define TCPC_RECONN_WAIT_TIME       1000    // 1 sec

static tcpc_conf_t tcpc_conf = {0x00,};
static unsigned char g_tcpc_recv_buf[TCPC_DEF_BUF_SIZE] = {0x00,};
static unsigned char g_tcpc_send_buf[TCPC_DEF_BUF_SIZE] = {0x00,};
static int g_tcpc_send_cnt = 0;
static bool g_restart_tcp_client = false;

static EventGroupHandle_t tcpc_mgmt_evt = NULL;
static TaskHandle_t tcp_client_task_ptr = NULL;
#if CFG_PMGR
static bool g_restart_tcp_send_timer = false;
static TimerHandle_t tcpc_send_timer = NULL;
#endif

static void tcpc_cli_usage(void)
{
    TCPC_DBG("- USAGE: TCP Client Configuration\n");
    TCPC_DBG("* tcpc_conf %s <1|0>\n", TCPC_CLI_ACTIVE);
    TCPC_DBG("  : To active TCP client application. (Default: %d)\n", TCPC_DEF_ACTIVE);
    TCPC_DBG("* tcpc_conf %s <IP Address>\n", TCPC_CLI_PEER_IP_ADDR);
    TCPC_DBG("  : Peer IP Address should be IPv4 type(xxx.xxx.xxx.xxx)\n");
    TCPC_DBG("* tcpc_conf %s <Port Number>\n", TCPC_CLI_PEER_PORT);
    TCPC_DBG("* tcpc_conf %s <Period>\n", TCPC_CLI_SEND_PERIOD);
    TCPC_DBG("  : Period is from %d to %d, or 0 to disable. (Default: %d ms)\n",
            TCPC_MIN_SEND_PERIOD, TCPC_MAX_SEND_PERIOD, TCPC_DEF_SEND_PERIOD);
    TCPC_DBG("* tcpc_conf %s <data_size>\n", TCPC_CLI_SEND_DATA_SIZE);
    TCPC_DBG("  : Data size is from %d to %d. (Default: %d bytes)\n",
            TCPC_MIN_SEND_DATA_SIZE, TCPC_MAX_SEND_DATA_SIZE, TCPC_DEF_SEND_DATA_SIZE);
    TCPC_DBG("* tcpc_conf %s <1|0>\n", TCPC_CLI_AUTO_RESTART_AT_EXIT);
    TCPC_DBG("  : if set to 1, tcpc restarts when exits due to connecion failure (Default: %d)\n", TCPC_DEF_AUTO_RESTART_AT_EXIT);
    TCPC_DBG("* tcpc_conf %s <1|0> <idle_time> <intvl_time> <max_probes> (time values in seconds)\n", TCPC_CLI_KEEPALIVE);

    return ;
}

static void tcpc_print_conf(tcpc_conf_t *p_tcpc_conf)
{
    if (p_tcpc_conf) {
        TCPC_DBG("%-20s : %d\n", "Active", p_tcpc_conf->active);
        TCPC_DBG("%-20s : %s\n", "Peer IP Address", p_tcpc_conf->peer_ip_addr);
        TCPC_DBG("%-20s : %d\n", "Peer Port", p_tcpc_conf->peer_port);
        TCPC_DBG("%-20s : %d\n", "Period", p_tcpc_conf->send_period);
        TCPC_DBG("%-20s : %d\n", "Data size", p_tcpc_conf->send_data_size);
        TCPC_DBG("%-20s : %d\n", "autostart_atexit", p_tcpc_conf->autostart_atexit);
        TCPC_DBG("%-20s : %d\n", "KA enable", p_tcpc_conf->ka_enable);

        if (p_tcpc_conf->ka_enable) {
            TCPC_DBG("%-20s : %d (sec)\n", "KA idle time", p_tcpc_conf->ka_idle_time);
            TCPC_DBG("%-20s : %d (sec)\n", "KA interval time", p_tcpc_conf->ka_intvl_time);
            TCPC_DBG("%-20s : %d\n", "KA max probes", p_tcpc_conf->ka_max_probes);
        }
    }

    return ;
}

#if CFG_PMGR
static void tcpc_send_timer_cb(void *env)
{
    pmgr_instance_ctrl_t * p_instance_ctrl = RM_PMGR_W_get_ctrl();
    const char *timer_name = NULL;

    if (RM_PMGR_W_dpm_is_enabled()) {
        timer_name = (char *)env;
    } else {
        timer_name = pcTimerGetName((TimerHandle_t)env);
    }

    if (timer_name) {
        TCPC_DBG_INFO("TCP Timer(%s)\n", timer_name);
    }

    /* Constrain RAM till all TCP send is done */
    if (g_tcpc_send_cnt == 0) {
        RM_PMGR_W_add_sleep_constraint(RM_PMGR_W_get_ctrl(), PMGR_CONSTRAINT_POWER_RAM);
        if (p_instance_ctrl && p_instance_ctrl->debug_runtime_flag == PMGR_DEBUG_RAM_ENABLED)
        {
            p_instance_ctrl->ram_counter_cause[PMGR_DBG_RAM_CONSTRAINT_TCP_CLIENT]++;
        }
    }

    g_tcpc_send_cnt++;
}

static int tcpc_send_timer_register(int send_period)
{
    if (!send_period)
    {
        return 0;
    }

    /* Short period will result with effectivly no idle time */
    if (send_period < TCPC_MIN_SEND_PERIOD)
    {
        return -1;
    }

    if (RM_PMGR_W_dpm_is_enabled())
    {
        int tid = DPM_TIMER_ERR;

        /* Create DPM timer only once at POR, or if timer was deregistered */
        if (!RM_PMGR_W_dpm_is_wakeup() || g_restart_tcp_send_timer)
        {
            tid = RM_PMGR_W_dpm_timer_create(JOB_ID_SEND,
                                             TCPC_TIMER_NAME,
                                             (void (*)(char *))tcpc_send_timer_cb,
                                             send_period,
                                             send_period);
            if (tid == DPM_TIMER_ERR)
            {
                return -1;
            }

            g_restart_tcp_send_timer = false;
        }
    }
    else
    {
        /* Create FreeRTOS timer */
        tcpc_send_timer = xTimerCreate(TCPC_TIMER_NAME,
                                       pdMS_TO_TICKS(send_period),
                                       pdTRUE,
                                       NULL,
                                       (TimerCallbackFunction_t)tcpc_send_timer_cb);
        if (!tcpc_send_timer)
        {
            return -1;
        }

        if (xTimerStart(tcpc_send_timer, 0) != pdPASS)
        {
            return -1;
        }
    }

    g_tcpc_send_cnt = 0;
    RM_PMGR_W_dpm_job_name_set(JOB_ID_SEND, 0);

    return 0;
}

static int tcpc_send_timer_deregister()
{
    pmgr_instance_ctrl_t *p_instance_ctrl = RM_PMGR_W_get_ctrl();
    int ret = -1;

    if (tcpc_conf.send_period == 0) {
        return 0;
    }

    if (RM_PMGR_W_dpm_is_enabled()) {
        ret = RM_PMGR_W_dpm_timer_delete(JOB_ID_SEND, TCPC_TIMER_NAME);
    } else {
        ret = (xTimerDelete(tcpc_send_timer, 0) == pdPASS) ? 0 : -1;
    }

    if (ret < 0) {
        TCPC_DBG_ERR("Failed to deregister TCP client's timer(%d)\n", ret);
    }

    if (tcpc_conf.autostart_atexit) {
        g_restart_tcp_send_timer = true;
    }

    if (g_tcpc_send_cnt) {
        /* Remove the existing send constraint (the send will never happen) */
        RM_PMGR_W_remove_sleep_constraint(RM_PMGR_W_get_ctrl(), PMGR_CONSTRAINT_POWER_RAM);

        if (p_instance_ctrl && p_instance_ctrl->debug_runtime_flag == PMGR_DEBUG_RAM_ENABLED)
        {
            p_instance_ctrl->ram_counter_cause[PMGR_DBG_RAM_CONSTRAINT_TCP_CLIENT]--;
        }

        g_tcpc_send_cnt = 0;
    }

    return ret;
}
#endif /* CFG_PMGR */

static int tcpc_load_conf(tcpc_conf_t *p_tcpc_conf)
{
    int err = FSP_ERR_INVALID_DATA;
    tcpc_conf_t *p_conf = NULL;
    tcpc_conf_t conf = {0x00,};

    int is_exist_conf = pdFALSE;

    char *peer_ip_addr = NULL;
#ifdef RM_MAP_PERSISTANT_W
    uint16_t  data_length = 0;
#endif
#if CFG_PMGR
    unsigned int rtm_len = 0;
    unsigned int rtm_ret = 0;

    //Read configuration from RTM.
    if (RM_PMGR_W_dpm_is_enabled()) {
        rtm_len = RM_PMGR_W_user_rtm_get(TCPC_RTM_NAME, (unsigned char **)&p_conf);
        if (rtm_len == 0) {
            rtm_ret = (int)RM_PMGR_W_user_rtm_pool_alloc(TCPC_RTM_NAME, (void **)&p_conf, sizeof(tcpc_conf_t), 0);
            if (rtm_ret) {
                TCPC_DBG_ERR("Failed to allocate RTM for configuration(%d)\n", rtm_ret);
            } else {
                memset(p_conf, 0x00, sizeof(tcpc_conf_t));
            }
        } else {
            is_exist_conf = pdTRUE;
            TCPC_DBG_INFO("TCPC SESS restored ... \n");
        }
    }
#endif /* CFG_PMGR */

    if (!p_conf) {
        p_conf = &conf;
    }

    //Read configuration from NVRAM.
    if (!is_exist_conf) {

        TCPC_DBG_INFO("tcpc conf read from nvram ... \n");

        //Read start flag
#ifdef RM_MAP_PERSISTANT_W
        err = RM_MAP_PERSISTANT_W_Read_UINT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, TCPC_NVR_ACTIVE,
                                          &p_conf->active, &data_length);
#endif
        if (err != FSP_SUCCESS) {
            TCPC_DBG_INFO("Set Default active(%d)\n", TCPC_DEF_ACTIVE);
            p_conf->active = TCPC_DEF_ACTIVE;
        }

        //Read peer ip address
#ifdef RM_MAP_PERSISTANT_W
        err = RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, TCPC_NVR_PEER_IP_ADDR,
                                            &peer_ip_addr);
#endif
        if (err != FSP_SUCCESS) {
            TCPC_DBG_INFO("Set Default Peer IP address(%s)\n", TCPC_DEF_PEER_IP_ADDR);
            strcpy(p_conf->peer_ip_addr, TCPC_DEF_PEER_IP_ADDR);
        } else {
            strcpy(p_conf->peer_ip_addr, peer_ip_addr);
        }

        //Read peer port
#ifdef RM_MAP_PERSISTANT_W
        err = RM_MAP_PERSISTANT_W_Read_UINT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, TCPC_NVR_PEER_PORT,
                                          &p_conf->peer_port, &data_length);
#endif
        if (err != FSP_SUCCESS) {
            TCPC_DBG_INFO("Set Default Peer port(%d)\n", TCPC_DEF_PEER_PORT);
            p_conf->peer_port = TCPC_DEF_PEER_PORT;
        }

        //Read send period
#ifdef RM_MAP_PERSISTANT_W
        err = RM_MAP_PERSISTANT_W_Read_UINT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, TCPC_NVR_SEND_PERIOD,
                                          &p_conf->send_period, &data_length);
#endif
        if (err != FSP_SUCCESS) {
            TCPC_DBG_INFO("Set Default period(%d)\n", TCPC_DEF_SEND_PERIOD);
            p_conf->send_period = TCPC_DEF_SEND_PERIOD;
        }

        //Read send data size
#ifdef RM_MAP_PERSISTANT_W
        err = RM_MAP_PERSISTANT_W_Read_UINT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, TCPC_NVR_SEND_DATA_SIZE,
                                          &p_conf->send_data_size, &data_length);
#endif
        if (err != FSP_SUCCESS) {
            TCPC_DBG_INFO("Set Default data size(%d)\n", TCPC_DEF_SEND_DATA_SIZE);
            p_conf->send_data_size = TCPC_DEF_SEND_DATA_SIZE;
        }

        // Read autostart_atexit flag
#ifdef RM_MAP_PERSISTANT_W
        err = RM_MAP_PERSISTANT_W_Read_UINT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG,
                                          TCPC_NVR_AUTO_RESTART_AT_EXIT, &p_conf->autostart_atexit, &data_length);
#endif
        if (err != FSP_SUCCESS) {
            TCPC_DBG_INFO("Set Default (%d)\n", TCPC_DEF_AUTO_RESTART_AT_EXIT);
            p_conf->autostart_atexit = TCPC_DEF_AUTO_RESTART_AT_EXIT;
        }

#ifdef RM_MAP_PERSISTANT_W
        err = RM_MAP_PERSISTANT_W_Read_UINT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG,
                                         TCPC_NVR_KA_ENABLE, &p_conf->ka_enable, &data_length);
#endif
        if (err != FSP_SUCCESS) {
            TCPC_DBG_INFO("Set Default (%d)\n", TCPC_DEF_KA_ENABLE);
            p_conf->ka_enable = TCPC_DEF_KA_ENABLE;
        }

#ifdef RM_MAP_PERSISTANT_W
        err = RM_MAP_PERSISTANT_W_Read_UINT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG,
                                         TCPC_NVR_KA_IDLE_TIME, &p_conf->ka_idle_time, &data_length);
#endif
        if (err != FSP_SUCCESS) {
            TCPC_DBG_INFO("Set Default (%d)\n", TCPC_DEF_KA_IDLE_TIME);
            p_conf->ka_idle_time = TCPC_DEF_KA_IDLE_TIME;
        }

#ifdef RM_MAP_PERSISTANT_W
        err = RM_MAP_PERSISTANT_W_Read_UINT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG,
                                         TCPC_NVR_KA_INTVL_TIME, &p_conf->ka_intvl_time, &data_length);
#endif
        if (err != FSP_SUCCESS) {
            TCPC_DBG_INFO("Set Default (%d)\n", TCPC_DEF_KA_INTVL_TIME);
            p_conf->ka_intvl_time = TCPC_DEF_KA_INTVL_TIME;
        }

#ifdef RM_MAP_PERSISTANT_W
        err = RM_MAP_PERSISTANT_W_Read_UINT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG,
                                         TCPC_NVR_KA_MAX_PROBES, &p_conf->ka_max_probes, &data_length);
#endif
        if (err != FSP_SUCCESS) {
            TCPC_DBG_INFO("Set Default (%d)\n", TCPC_DEF_KA_MAX_PROBES);
            p_conf->ka_max_probes = TCPC_DEF_KA_MAX_PROBES;
        }
    }

    //Copy configuration
    memcpy(p_tcpc_conf, p_conf, sizeof(tcpc_conf_t));

    TCPC_DBG_INFO("%-20s : %d\n", "Active", p_tcpc_conf->active);
    TCPC_DBG_INFO("%-20s : %s\n", "Peer IP Address", p_tcpc_conf->peer_ip_addr);
    TCPC_DBG_INFO("%-20s : %d\n", "Peer Port", p_tcpc_conf->peer_port);
    TCPC_DBG_INFO("%-20s : %d\n", "Period", p_tcpc_conf->send_period);
    TCPC_DBG_INFO("%-20s : %d\n", "Data size", p_tcpc_conf->send_data_size);
    TCPC_DBG_INFO("%-20s : %d\n", "Auto-start at exit", p_tcpc_conf->autostart_atexit);
    TCPC_DBG_INFO("%-20s : %d\n", "KA enable", p_tcpc_conf->ka_enable);

    if (p_tcpc_conf->ka_enable) {
        TCPC_DBG_INFO("%-20s : %d (sec)\n", "KA idle time", p_tcpc_conf->ka_idle_time);
        TCPC_DBG_INFO("%-20s : %d (sec)\n", "KA interval time", p_tcpc_conf->ka_intvl_time);
        TCPC_DBG_INFO("%-20s : %d\n", "KA max probe cnt", p_tcpc_conf->ka_max_probes);
    }

    return 0;
}

bool cmd_tcpc_conf(int argc, char *argv[])
{
    int idx = 0;
    int ret = 0;

#if defined(ENABLE_TCPC_DBG_INFO)
    for (idx = 0 ; idx < argc ; idx++) {
        TCPC_DBG_INFO("ARG #%d. %s\n", idx, argv[idx]);
    }
#endif // ENABLE_TCPC_DBG_INFO

    for (idx = 1 ; idx < argc ; idx++) {
        if ((strcmp(argv[idx], TCPC_CLI_ACTIVE) == 0) && (argc == 3)) {
            idx++;
            const int active = atoi(argv[idx]);

            if (active != 0 && active != 1) {
                goto err;
            }

#ifdef RM_MAP_PERSISTANT_W
	    ret = RM_MAP_PERSISTANT_W_Write_UINT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG,
                                               TCPC_NVR_ACTIVE, (void *)&active, 4);
#endif
            if (ret) {
                TCPC_DBG_ERR("Failed to set active(%d)\n", ret);
                goto err;
            }
        } else if ((strcmp(argv[idx], TCPC_CLI_PEER_IP_ADDR) == 0) && (argc == 3)) {
            idx++;
            const char *ip_addr = argv[idx];
            const size_t ip_addr_len = strlen(ip_addr);

            if (ip_addr_len <= 0 || ip_addr_len >= TCPC_PEER_IP_ADDR_LEN) {
                goto err;
            }

#ifdef RM_MAP_PERSISTANT_W
            ret = RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG,
                                                 TCPC_NVR_PEER_IP_ADDR, ip_addr);
#endif
            if (ret) {
                TCPC_DBG_ERR("Failed to set Peer ip_addr(%d)\n", ret);
                goto err;
            }
        } else if ((strcmp(argv[idx], TCPC_CLI_PEER_PORT) == 0) && (argc == 3)) {
            idx++;
            const int port = atoi(argv[idx]);

            if (port <= 0 || port >= 65535) {
                goto err;
            }
#ifdef RM_MAP_PERSISTANT_W
            ret = RM_MAP_PERSISTANT_W_Write_UINT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG,
                                               TCPC_NVR_PEER_PORT, (void *)&port, 4);
#endif
            if (ret) {
                TCPC_DBG_ERR("Failed to set Peer port number(%d)\n", ret);
                goto err;
            }
        } else if ((strcmp(argv[idx], TCPC_CLI_SEND_PERIOD) == 0) && (argc == 3)) {
            idx++;
            const int period = atoi(argv[idx]);

            if ((period) && ((period < TCPC_MIN_SEND_PERIOD) || (period > TCPC_MAX_SEND_PERIOD))) {
                goto err;
            } 

#ifdef RM_MAP_PERSISTANT_W
            ret = RM_MAP_PERSISTANT_W_Write_UINT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG,
                                               TCPC_NVR_SEND_PERIOD, (void *)&period, 4);
#endif
            if (ret) {
                TCPC_DBG_ERR("Failed to set period to send(%d)\n", ret);
                goto err;
            }
        } else if ((strcmp(argv[idx], TCPC_CLI_SEND_DATA_SIZE) == 0) && (argc == 3)) {
            idx++;
            const int data_size = atoi(argv[idx]);

            if ((data_size < TCPC_MIN_SEND_DATA_SIZE) || (data_size > TCPC_MAX_SEND_DATA_SIZE)) {
                goto err;
            }

#ifdef RM_MAP_PERSISTANT_W
            ret = RM_MAP_PERSISTANT_W_Write_UINT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG,
                                               TCPC_NVR_SEND_DATA_SIZE, (void *)&data_size, 4);
#endif
            if (ret) {
                TCPC_DBG_ERR("Failed to set data size to send(%d)\n", ret);
                goto err;
            }
        } else if ((strcmp(argv[idx], TCPC_CLI_STATUS) == 0) && (argc == 2)) {

            tcpc_load_conf(&tcpc_conf);

            tcpc_print_conf(&tcpc_conf);

        } else if ((strcmp(argv[idx], TCPC_CLI_AUTO_RESTART_AT_EXIT) == 0) && (argc == 3)) {

            idx++;
            const int auto_start_at_exit = atoi(argv[idx]);
            
            if (auto_start_at_exit != 0 && auto_start_at_exit != 1) {
                goto err;
            }
#ifdef RM_MAP_PERSISTANT_W
            ret = RM_MAP_PERSISTANT_W_Write_UINT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG,
                                               TCPC_NVR_AUTO_RESTART_AT_EXIT, (void *)&auto_start_at_exit, 4);
#endif
            if (ret) {
                TCPC_DBG_ERR("Failed to set (%d)\n", ret);
                goto err;
            }

        } else if ((strcmp(argv[idx], TCPC_CLI_KEEPALIVE) == 0) && (argc >= 3)) {
            const int ka_enable = atoi(argv[++idx]);

            if (ka_enable != 0 && ka_enable != 1) {
                goto err;
            }

            // When enabling must give more parameters
            if (ka_enable && argc != 6) {
                goto err;
            }
#ifdef RM_MAP_PERSISTANT_W
            ret = RM_MAP_PERSISTANT_W_Write_UINT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG,
                                               TCPC_NVR_KA_ENABLE, (void *)&ka_enable, 4);
#endif
            if (ret) {
                TCPC_DBG_ERR("Failed to set ka_enable (%d)\n", ret);
                goto err;
            }

            if (ka_enable) {
                const int ka_idle_time = atoi(argv[++idx]);
                const int ka_intvl_time = atoi(argv[++idx]);
                const int ka_max_probes = atoi(argv[++idx]);

                if (ka_idle_time <= 0 || ka_intvl_time <= 0 || ka_max_probes <= 0) {
                    goto err;
                }
#ifdef RM_MAP_PERSISTANT_W
                ret = RM_MAP_PERSISTANT_W_Write_UINT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG,
                                                   TCPC_NVR_KA_IDLE_TIME, (void *)&ka_idle_time, 4);
#endif
                if (ret) {
                    TCPC_DBG_ERR("Failed to set ka_idle_time (%d)\n", ret);
                    goto err;
                }
#ifdef RM_MAP_PERSISTANT_W
                ret = RM_MAP_PERSISTANT_W_Write_UINT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG,
                                                   TCPC_NVR_KA_INTVL_TIME, (void *)&ka_intvl_time, 4);
#endif
                if (ret) {
                    TCPC_DBG_ERR("Failed to set ka_intvl_time (%d)\n", ret);
                    goto err;
                }
#ifdef RM_MAP_PERSISTANT_W
                ret = RM_MAP_PERSISTANT_W_Write_UINT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG,
                                                   TCPC_NVR_KA_MAX_PROBES, (void *)&ka_max_probes, 4);
#endif
                if (ret) {
                    TCPC_DBG_ERR("Failed to set ka_max_probes (%d)\n", ret);
                    goto err;
                }
            }
        } else {
            goto err;
        }
    }

    return pdTRUE;

err:

    tcpc_cli_usage();

    return pdFALSE;
}

static int tcp_client_keepalive_set(int socket_fd, int enable, int keep_idle, int keep_intvl, int keep_cnt)
{
    int ret = 0;

    setsockopt(socket_fd, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(enable));
    if (ret) {
        TCPC_DBG_ERR("Failed to set SO_KEEPALIVE (%d)\n", ret);
    }

#if LWIP_TCP_KEEPALIVE
    if (keep_idle) {
        ret = setsockopt(socket_fd, IPPROTO_TCP, TCP_KEEPIDLE, &keep_idle, sizeof(keep_idle));
        if (ret) {
            TCPC_DBG_ERR("Failed to set TCP_KEEPIDLE (%d)\n", ret);
        }
    }

    if (keep_intvl) {
        ret = setsockopt(socket_fd, IPPROTO_TCP, TCP_KEEPINTVL, &keep_intvl, sizeof(keep_intvl));
        if (ret) {
            TCPC_DBG_ERR("Failed to set TCP_KEEPINTVL (%d)\n", ret);
        }
    }

    if (keep_cnt) {
        ret = setsockopt(socket_fd, IPPROTO_TCP, TCP_KEEPCNT, &keep_cnt, sizeof(keep_cnt));
        if (ret) {
            TCPC_DBG_ERR("Failed to set TCP_KEEPCNT (%d)\n", ret);
        }
    }
#else
    if (keep_idle) {
        keep_idle *= 1000; // Convert to ms
        ret = setsockopt(socket_fd, IPPROTO_TCP, TCP_KEEPALIVE, &keep_idle, sizeof(keep_idle));
        if (ret) {
            TCPC_DBG_ERR("Failed to set TCP_KEEPALIVE\n");
        }
    }
#endif

    return ret;
}

#if defined ( __SUPPORT_IPV6__ )
static void wait_ready_connect_ipv6(char *peer_tcp)
{
    extern struct netif wlan0_iface;
    struct in6_addr peer_in6addr;
    ip6_addr_t peer_ip6addr;
    int wait_cnt = 0;

    /* Convert string to workable IPv6 */
    inet_pton(AF_INET6, peer_tcp, &peer_in6addr);
    inet6_addr_to_ip6addr(&peer_ip6addr, &peer_in6addr)

    if (ip6_addr_isglobal(&peer_ip6addr)) {
        /* Wait till our own global address becomes valid */
        while (!ip6_addr_isvalid(netif_ip6_addr_state(&wlan0_iface, 1))) {
            vTaskDelay(portCONVERT_MS_2_TICKS(100));
            wait_cnt++;
            /* Wait max 10 seconds */
            if (wait_cnt > 100) {
                TCPC_DBG_ERR("Global IPv6 is not valid, attempting to connect with Link-local IPv6 address\n");
                break;
            }
        }
    }
}
#endif

static void tcpc_task_entry(void *param)
{
    extern int select_ipaddr_type_from_str(char* str, struct sockaddr_storage *ipaddr);

    FSP_PARAMETER_NOT_USED(param);

    int ret = 0;
    int ip_type = 0;
    int socket_fd = -1;
    int remaining_data = TCPC_REMAIN_DATA_INIT;
#if CFG_PMGR
    pmgr_instance_ctrl_t *p_instance_ctrl = RM_PMGR_W_get_ctrl();
#endif /* CFG_PMGR */

    //socket option
    int sockopt_reuse = 1;
    struct timeval sockopt_timeout = {0x00,};
    struct sockaddr_storage valid_ip = {0x00,};

#if defined ( __SUPPORT_IPV4__ )
    struct sockaddr_in local_addr4;
    struct sockaddr_in *srv_addr4 = (struct sockaddr_in *)&valid_ip;
#endif // __SUPPORT_IPV4__

#if defined ( __SUPPORT_IPV6__ )
    struct sockaddr_in6 local_addr6;
    struct sockaddr_in6 *srv_addr6 = (struct sockaddr_in6 *)&valid_ip;
#endif // __SUPPORT_IPV6__

    int len = 0;

    unsigned char *p_recv_buf = g_tcpc_recv_buf;

    strcpy((char*)g_tcpc_send_buf, "hello_there!!");
    unsigned char *p_send_buf = g_tcpc_send_buf;
    size_t recv_buflen = sizeof(g_tcpc_recv_buf);
    //size_t send_buflen = sizeof(g_tcpc_send_buf);
    
    ret = tcpc_load_conf(&tcpc_conf);
    if (ret) {
        TCPC_DBG_ERR("Failed to read TCP client's configuration\n");
        xEventGroupSetBits(tcpc_mgmt_evt, EVT_TCPC_EXIT);
        vTaskDelete(NULL);
    }

    if (!tcpc_conf.active) {
        TCPC_DBG_INFO("TCP client is not active\n");
        xEventGroupSetBits(tcpc_mgmt_evt, EVT_TCPC_EXIT);
        vTaskDelete(NULL);
        return;
    }

#if CFG_PMGR
    ret = tcpc_send_timer_register(tcpc_conf.send_period);
    if (ret) {
        TCPC_DBG_ERR("Failed to set TCP send timer\n");
        xEventGroupSetBits(tcpc_mgmt_evt, EVT_TCPC_EXIT);
        vTaskDelete(NULL);
        return;
    }

    /* JOB_ID_RECV - INIT with TCP port */
    RM_PMGR_W_dpm_job_name_set(JOB_ID_RECV, TCPC_DEF_LOCAL_PORT);

    /* JOB_ID_RECV - Set TCP port filter */
    RM_WIFI_dpm_tcp_port_filter_set(TCPC_DEF_LOCAL_PORT);

    if (!g_restart_tcp_client) {
        /* Constrain RAM till TCP connection done */
        RM_PMGR_W_add_sleep_constraint(RM_PMGR_W_get_ctrl(), PMGR_CONSTRAINT_POWER_RAM);
        if (p_instance_ctrl && p_instance_ctrl->debug_runtime_flag == PMGR_DEBUG_RAM_ENABLED)
        {
            p_instance_ctrl->ram_counter_cause[PMGR_DBG_RAM_CONSTRAINT_TCP_CLIENT]++;
        }
    }
#endif /* CFG_PMGR */

    TCPC_DBG("Start of TCP Client application\n");

    ip_type = select_ipaddr_type_from_str(tcpc_conf.peer_ip_addr, &valid_ip);

    /* Common */
    if ((strcmp(tcpc_conf.peer_ip_addr, "0") != 0)
#if defined ( __SUPPORT_IPV4__ )
            && (ip_type != IPADDR_TYPE_V4)
#endif // __SUPPORT_IPV4__
#if defined ( __SUPPORT_IPV6__ )
            && (ip_type != IPADDR_TYPE_V6)
#endif // __SUPPORT_IPV6__
       )
    {
        TCPC_DBG_ERR("IP address is invalid\n");
        goto end_of_task;
    }

#if defined ( __SUPPORT_IPV4__ )
    if (ip_type == IPADDR_TYPE_V4) {
        memset(&local_addr4, 0x00, sizeof(struct sockaddr_in));
        memset(srv_addr4, 0x00, sizeof(struct sockaddr_in));

#if CFG_PMGR
        socket_fd = socket_dpm(JOB_ID_RECV, PF_INET, SOCK_STREAM, 0);
#else
        socket_fd = socket(PF_INET, SOCK_STREAM, 0);
#endif /* CFG_PMGR */

        if (socket_fd < 0) {
            TCPC_DBG_ERR("Failed to assign socket descriptor\n");
            goto end_of_task;
        }
    }
#endif // __SUPPORT_IPV4__

#if defined ( __SUPPORT_IPV6__ )
    if (ip_type == IPADDR_TYPE_V6) {
        memset(&local_addr6, 0x00, sizeof(struct sockaddr_in6));
        memset(srv_addr6, 0x00, sizeof(struct sockaddr_in6));
#if CFG_PMGR
        socket_fd = socket_dpm(JOB_ID_RECV, PF_INET6, SOCK_STREAM, 0);
#else
        socket_fd = socket(PF_INET, SOCK_STREAM, 0);
#endif /* CFG_PMGR */

        if (socket_fd < 0) {
            TCPC_DBG_ERR("Failed to assign socket descriptor\n");
            goto end_of_task;
        }
    }
#endif // __SUPPORT_IPV6__
    ret = setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &sockopt_reuse, sizeof(sockopt_reuse));
    if (ret) {
        TCPC_DBG_ERR("Failed to set socket option - SO_REUSEADDR(%d)\n", ret);
    }

    sockopt_timeout.tv_sec = 0;
    sockopt_timeout.tv_usec = TCPC_DEF_RECV_TIMEOUT * 1000;

    ret = setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &sockopt_timeout, sizeof(sockopt_timeout));
    if (ret) {
        TCPC_DBG_ERR("Failed to set socket option - SO_RCVTIMEOUT(%d:%d)\n", ret, errno);
    }

    sockopt_timeout.tv_sec = 0;
    sockopt_timeout.tv_usec = TCPC_DEF_SEND_TIMEOUT * 1000;

    ret = setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, &sockopt_timeout, sizeof(sockopt_timeout));
    if (ret) {
        TCPC_DBG_ERR("Failed to set socket option - SO_SNDTIMEO(%d)\n", TCPC_DEF_SEND_TIMEOUT);
    }

    /* Set TCP keepalive socket options */
    tcp_client_keepalive_set(socket_fd, tcpc_conf.ka_enable, tcpc_conf.ka_idle_time,
                             tcpc_conf.ka_intvl_time, tcpc_conf.ka_max_probes);

#if defined ( __SUPPORT_IPV4__ )
    if (ip_type == IPADDR_TYPE_V4) {
        local_addr4.sin_family = AF_INET;
        local_addr4.sin_addr.s_addr = htonl(INADDR_ANY);
        local_addr4.sin_port = htons(TCPC_DEF_LOCAL_PORT);

        ret = bind(socket_fd, (struct sockaddr *)&local_addr4, sizeof(struct sockaddr_in));
        if (ret == -1) {
            TCPC_DBG_ERR("Failed to bind socket\n");
            close(socket_fd);
            goto end_of_task;
        }
        srv_addr4->sin_family = AF_INET;
        srv_addr4->sin_addr.s_addr = inet_addr(tcpc_conf.peer_ip_addr);
        srv_addr4->sin_port = htons(tcpc_conf.peer_port);

        TCPC_DBG("Connecting(%s:%d)\n", tcpc_conf.peer_ip_addr, tcpc_conf.peer_port);
        ret = connect(socket_fd, (struct sockaddr *)srv_addr4, sizeof(struct sockaddr_in));
    }
#endif // __SUPPORT_IPV4__

#if defined ( __SUPPORT_IPV6__ )
    if (ip_type == IPADDR_TYPE_V6) {
        local_addr6.sin6_family = AF_INET6;
        local_addr6.sin6_addr = in6addr_any;
        local_addr6.sin6_port = htons(TCPC_DEF_LOCAL_PORT);
        ret = bind(socket_fd, (struct sockaddr *)&local_addr6, sizeof(struct sockaddr_in6));
        if (ret == -1) {
            TCPC_DBG_ERR("Failed to bind socket\n");
            close(socket_fd);
            goto end_of_task;
        }
        srv_addr6->sin6_family = AF_INET6;
        inet_pton(AF_INET6, tcpc_conf.peer_ip_addr, &srv_addr6->sin6_addr);
        srv_addr6->sin6_port = htons(tcpc_conf.peer_port);
        TCPC_DBG("Connecting(%s:%d)\n", tcpc_conf.peer_ip_addr, tcpc_conf.peer_port);
        wait_ready_connect_ipv6(tcpc_conf.peer_ip_addr);
        ret = connect(socket_fd, (struct sockaddr *)srv_addr6, sizeof(struct sockaddr_in6));
    }
#endif // __SUPPORT_IPV6__
    if (ret < 0) {
        TCPC_DBG_ERR("Failed to establish TCP session(%d,%d)\n", ret, errno);
        close(socket_fd);
        goto end_of_task;
    }

    TCPC_DBG("Connected(%s:%d)\n", tcpc_conf.peer_ip_addr, tcpc_conf.peer_port);

    if (tcpc_mgmt_evt) {
        /* Notify the starter task that TCP connection is done */
        xEventGroupSetBits(tcpc_mgmt_evt, EVT_TCPC_CONN);
    }

#if CFG_PMGR
    if (tcpc_conf.send_period)
    {
        /* TCP send timer callback may now execute and schedule Tx */
        RM_PMGR_W_dpm_wakeup_done(JOB_ID_SEND);
        /* For callback to be called */
        vTaskDelay(portCONVERT_MS_2_TICKS(50));
    }
#endif /* CFG_PMGR */

    remaining_data = TCPC_REMAIN_DATA_INIT;
    while (1) {
        memset(p_recv_buf, 0x00, recv_buflen);

        if (g_tcpc_send_cnt > 0) {
            g_tcpc_send_cnt--;
            len = send(socket_fd, p_send_buf, tcpc_conf.send_data_size, 0);
            if (len <= 0) {
                TCPC_DBG("Failed to send data(%d:%d)\n", len, errno);
                if (errno == EHOSTUNREACH) {
#if CFG_PMGR
                    /* TCP send failure - Remove constraint */
                    RM_PMGR_W_remove_sleep_constraint(RM_PMGR_W_get_ctrl(), PMGR_CONSTRAINT_POWER_RAM);
                    if (p_instance_ctrl && p_instance_ctrl->debug_runtime_flag == PMGR_DEBUG_RAM_ENABLED)
                    {
                        p_instance_ctrl->ram_counter_cause[PMGR_DBG_RAM_CONSTRAINT_TCP_CLIENT]--;
                    }
#endif /* CFG_PMGR */
                    /* Either TCPC is disconnected or WI-Fi is disconnected */
                    break;
                }
            } else {
                TCPC_DBG("Sending tcp data (%s) done !!\n", p_send_buf);
            }

#if CFG_PMGR
            if (g_tcpc_send_cnt == 0) {
                /* All TCP send are success - Remove constraint */
                RM_PMGR_W_remove_sleep_constraint(RM_PMGR_W_get_ctrl(), PMGR_CONSTRAINT_POWER_RAM);
                if (p_instance_ctrl && p_instance_ctrl->debug_runtime_flag == PMGR_DEBUG_RAM_ENABLED)
                {
                    p_instance_ctrl->ram_counter_cause[PMGR_DBG_RAM_CONSTRAINT_TCP_CLIENT]--;
                }
            }
#endif /* CFG_PMGR */

            /* NOTE: user application logic for 'DONE (or failure)' case - 
               user may want to call send() again if reply does not arrive, if then, add exception handler is required  */
        }

#if CFG_PMGR
        /* JOB_ID_RECV - ready to receive data */
        RM_PMGR_W_dpm_rcv_ready_set(JOB_ID_RECV);
#endif /* CFG_PMGR */

        if (remaining_data == TCPC_REMAIN_DATA_INIT) {
#if CFG_PMGR
            /* Constrain RAM till TCP recv done */
            RM_PMGR_W_add_sleep_constraint(RM_PMGR_W_get_ctrl(), PMGR_CONSTRAINT_POWER_RAM);
            if (p_instance_ctrl && p_instance_ctrl->debug_runtime_flag == PMGR_DEBUG_RAM_ENABLED)
            {
                p_instance_ctrl->ram_counter_cause[PMGR_DBG_RAM_CONSTRAINT_TCP_CLIENT]++;
            }
#endif /* CFG_PMGR */
        }

        len = recv(socket_fd, p_recv_buf, recv_buflen, 0);

        if (len > 0) {
            p_recv_buf[len] = '\0';
            TCPC_DBG("< Read from server: (%d) bytes read\n", len);
        } else {
            if (errno != EAGAIN) {
                TCPC_DBG("Failed to receive data(%d,%d), closing tcp client ... \n", len, errno);
#if CFG_PMGR
                /* TCP recv failure - Remove constraint */
                RM_PMGR_W_remove_sleep_constraint(RM_PMGR_W_get_ctrl(), PMGR_CONSTRAINT_POWER_RAM);
                if (p_instance_ctrl && p_instance_ctrl->debug_runtime_flag == PMGR_DEBUG_RAM_ENABLED)
                {
                    p_instance_ctrl->ram_counter_cause[PMGR_DBG_RAM_CONSTRAINT_TCP_CLIENT]--;
                }
#endif /* CFG_PMGR */
                /* Either TCPC is disconnected or WI-Fi is disconnected */ 
                break;
            }
        }

#if CFG_PMGR
        remaining_data = RM_PMGR_W_socket_rx_data_is_remaining(socket_fd);

        if (remaining_data == 0) {
            remaining_data = TCPC_REMAIN_DATA_INIT;
            /* TCP recv done - Remove constraint */
            RM_PMGR_W_remove_sleep_constraint(RM_PMGR_W_get_ctrl(), PMGR_CONSTRAINT_POWER_RAM);
            if (p_instance_ctrl && p_instance_ctrl->debug_runtime_flag == PMGR_DEBUG_RAM_ENABLED)
            {
                p_instance_ctrl->ram_counter_cause[PMGR_DBG_RAM_CONSTRAINT_TCP_CLIENT]--;
            }
        }
#endif /* CFG_PMGR */

        vTaskDelay(portCONVERT_MS_2_TICKS(500));
    }

    close(socket_fd);

    if (tcpc_conf.autostart_atexit == pdTRUE) {
#if CFG_PMGR
        /* Constrain RAM till TCP connection done */
        RM_PMGR_W_add_sleep_constraint(RM_PMGR_W_get_ctrl(), PMGR_CONSTRAINT_POWER_RAM);
        if (p_instance_ctrl && p_instance_ctrl->debug_runtime_flag == PMGR_DEBUG_RAM_ENABLED)
        {
            p_instance_ctrl->ram_counter_cause[PMGR_DBG_RAM_CONSTRAINT_TCP_CLIENT]++;
        }
#endif /* CFG_PMGR */
    }

end_of_task:

    //TCPC_DBG("End of TCP Client application\n");

#if CFG_PMGR
    /* JOB_ID_SEND - delete DPM timer */
    tcpc_send_timer_deregister();

    /* JOB_ID_RECV - delete port filter */
    RM_WIFI_dpm_tcp_port_delete(TCPC_DEF_LOCAL_PORT);
#endif /* CFG_PMGR */

    if (tcpc_conf.autostart_atexit == pdTRUE) {
        if (tcpc_mgmt_evt) {
            xEventGroupSetBits(tcpc_mgmt_evt, EVT_TCPC_DISCONN);
        }
    }
    
    vTaskDelete(NULL);

    return ;
}

static BaseType_t tcp_client_task_start(void)
{
    if (chk_network_ready(WLAN0_IFACE) == pdFALSE) {
        while (chk_network_ready(WLAN0_IFACE) == pdFALSE) {
            vTaskDelay(portCONVERT_MS_2_TICKS(100));
        }
    }

    return xTaskCreate(tcpc_task_entry,
                       JOB_ID_RECV,
                       (TCPC_TASK_SIZE),
                       NULL,
                       (OS_TASK_PRIORITY_USER + 6),
                       &tcp_client_task_ptr);
}

static int tcpc_is_active(void)
{
    int err = FSP_ERR_INVALID_DATA;
    unsigned int conf_existing = pdFALSE;
    int is_active = pdFALSE;
    
#ifdef RM_MAP_PERSISTANT_W
    uint16_t  data_length = 0;
#endif /* RM_MAP_PERSISTANT_W */

#if CFG_PMGR
    tcpc_conf_t *p_conf = NULL;

    if (RM_PMGR_W_dpm_is_enabled()) {
        conf_existing = RM_PMGR_W_user_rtm_get(TCPC_RTM_NAME, (unsigned char **)&p_conf);
        if (conf_existing != pdFALSE) {
            is_active = p_conf->active;
        }
    }
#endif /* CFG_PMGR */

    if (conf_existing == pdFALSE) {

#ifdef RM_MAP_PERSISTANT_W
        err = RM_MAP_PERSISTANT_W_Read_UINT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, TCPC_NVR_ACTIVE,
                                          &is_active, &data_length);
#endif /* RM_MAP_PERSISTANT_W */
        if (err != FSP_SUCCESS) {
            TCPC_DBG_INFO("Set Default active(%d)\n", TCPC_DEF_ACTIVE);
            is_active = TCPC_DEF_ACTIVE;
        }
    }

    return is_active;
}

void tcpc_task_starter(void *param)
{
    FSP_PARAMETER_NOT_USED(param);
#if !defined (__SUPPORT_MATTER_IOT__)
    EventBits_t events = 0;
    BaseType_t status = pdPASS;
    pmgr_instance_ctrl_t *p_instance_ctrl = NULL;

    if (!tcpc_is_active()) {
        vTaskDelete(NULL);
        return;
    }

#if CFG_PMGR
    p_instance_ctrl = RM_PMGR_W_get_ctrl();
    RM_PMGR_W_add_sleep_constraint(RM_PMGR_W_get_ctrl(), PMGR_CONSTRAINT_POWER_RAM);
    if (p_instance_ctrl && p_instance_ctrl->debug_runtime_flag == PMGR_DEBUG_RAM_ENABLED)
    {
        p_instance_ctrl->ram_counter_cause[PMGR_DBG_RAM_CONSTRAINT_TCP_CLIENT]++;
    }
#endif /* CFG_PMGR */

    // Event flag group init
    if (tcpc_mgmt_evt == NULL) {
        tcpc_mgmt_evt = xEventGroupCreate();
        if (tcpc_mgmt_evt == NULL) {
            TCPC_DBG_ERR(RED_COLOR "Event group Create Error!");
            goto    TCPC_STARTER_END;
        }
    }

    g_restart_tcp_client = true;
    status = tcp_client_task_start();
    if (status != pdPASS) {
        TCPC_DBG_ERR("[%s:%d] tcp_client_task_start failed %ld\n", __func__, __LINE__, status);
        goto TCPC_STARTER_END;
    }

    while (1) {
        events = xEventGroupWaitBits(tcpc_mgmt_evt,
                                     EVT_TCPC_ANY,
                                     pdTRUE,
                                     pdFALSE,
                                     portMAX_DELAY);
        
        if (events & EVT_TCPC_DISCONN) {
            TCPC_DBG("[%s:%d] TCPC restarting ...\n", __func__, __LINE__);

            vTaskDelay(portCONVERT_MS_2_TICKS(TCPC_RECONN_WAIT_TIME));

            g_restart_tcp_client = true;
            status = tcp_client_task_start();

            if (status != pdPASS) {
                TCPC_DBG("[%s:%d] tcp_client_task_start failed %ld\n", __func__, __LINE__, status);
                goto TCPC_STARTER_END;
            }
        } else if (events & EVT_TCPC_CONN) {
#if CFG_PMGR
            p_instance_ctrl = RM_PMGR_W_get_ctrl();
            /* TCP connection done - Remove constraint */
            RM_PMGR_W_remove_sleep_constraint(p_instance_ctrl, PMGR_CONSTRAINT_POWER_RAM);
            if (p_instance_ctrl && p_instance_ctrl->debug_runtime_flag == PMGR_DEBUG_RAM_ENABLED)
            {
                p_instance_ctrl->ram_counter_cause[PMGR_DBG_RAM_CONSTRAINT_TCP_CLIENT]--;
            }
#endif /* CFG_PMGR */
        } else if (events & EVT_TCPC_EXIT) {
            goto TCPC_STARTER_END;
        } else {
            TCPC_DBG_ERR("Unknown event! %lu \n", events);
        }
    }

TCPC_STARTER_END:
#if CFG_PMGR
    RM_PMGR_W_dpm_job_name_clear(JOB_ID_TCPC_CONN);
#endif

    if (tcpc_mgmt_evt) {
        vEventGroupDelete(tcpc_mgmt_evt);
        tcpc_mgmt_evt = NULL;
    }

#endif	// !__SUPPORT_MATTER_IOT__
    vTaskDelete(NULL);
    return;
}

#endif
