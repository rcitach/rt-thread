/**
 ****************************************************************************************
 *
 * @file app_start.c
 *
 * @brief Application start point
 *
 * Copyright (c) 2023-2024 Renesas Electronics. All rights reserved.
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
#include "custom_config_sdk.h"

#include "net_common.h"
#include "net_sntp_client.h"
#include "app_start.h"
#include "nvedit.h"
#include "iface_defs.h"
#include "common_def.h"
#include "lwip/dhcp.h"
#include "rm_sntp.h"
#if CFG_PMGR
#include "rm_pmgr_w_instance.h"
#endif /* CFG_PMGR */
#include "rm_vee_flash_w_rrq_nvram.h"

extern int wifi_netif_status(int intf);
#if defined ( __SUPPORT_SNTP_CLIENT__ )
extern unsigned int start_sntp(void);
#endif

/* Global variables */
extern int  interface_select;
extern int  sysmode_select;
TaskHandle_t user_task_handler = NULL;

/* Local variables */
static run_app_task_info_t *sys_app_task_info = NULL;
static run_app_task_info_t *user_app_task_info = NULL;

/* Local functions	 */

run_app_task_info_t *create_new_app(run_app_task_info_t *prev_task, const app_task_info_t *cur_info, int sys_mode);

static void create_sys_apps(int sysmode, UCHAR net_chk_flag);
static void create_user_apps(int sysmode, UCHAR net_chk_flag);
#ifdef __SUPPORT_FAST_CONN_SLEEP_2__
extern unsigned char fast_connection_sleep_flag; // from NVRAM ("N0_FST_CONNECT")
#endif /* __SUPPORT_FAST_CONN_SLEEP_2__ */

#if CFG_PMGR
static void dpm_reg_sys_apps(void)
{
    int    i;
    const app_task_info_t *cur_info;

    for (i = 0 ; sys_apps_table[i].name != NULL ; i++) {
        cur_info = &(sys_apps_table[i]);

#ifdef __SUPPORT_FAST_CONN_SLEEP_2__
        if (fast_connection_sleep_flag == pdFALSE
            && strcmp(cur_info->name, FAST_SLEEPMODE_MOD) == 0) {
            continue;
        }
#endif /* __SUPPORT_FAST_CONN_SLEEP_2__ */

        if ((cur_info->run_sys_mode & RUN_STA_MODE) && (cur_info->dpm_flag == TRUE)) {
            RM_PMGR_W_dpm_job_name_set(cur_info->name, cur_info->port_no);
        }
    }
}

static void dpm_reg_user_apps(void)
{
    int    i;
    const app_task_info_t *cur_info;

    for (i = 0 ; user_apps_table[i].name != NULL ; i++) {
        cur_info = &(user_apps_table[i]);

        if (   (cur_info->run_sys_mode & RUN_STA_MODE)
            && (cur_info->dpm_flag == TRUE)) {
            RM_PMGR_W_dpm_job_name_set(cur_info->name, cur_info->port_no);
        }
    }
}

int	dpm_reg_done = 0;
void dpm_reg_all_apps()
{
    if (dpm_reg_done) {
        return;
    }

    dpm_reg_sys_apps();
    dpm_reg_user_apps();

    dpm_reg_done = 1;
}
#endif /* CFG_PMGR */

/**
 ****************************************************************************************
 * @brief       Start system applications to run RA6W1/RA6W2 ( Registered in sys_apps_table[] )
 * @return      None
 ****************************************************************************************
 */
void start_sys_apps(int net_chk_flag)
{
    INT iface = WLAN0_IFACE;
    int sysmode = get_run_mode();

    if (!net_chk_flag) {
        /* Create network independent apps */
        create_sys_apps(sysmode, FALSE);
    } else {
#if defined ( __SUPPORT_SNTP_CLIENT__ )
        if (sntp_get_use())
            start_sntp();
#endif // __SUPPORT_SNTP_CLIENT__

#if CFG_PMGR
        if (RM_PMGR_W_dpm_is_enabled() == pdTRUE) {
            dpm_reg_all_apps();
        }
#endif /* CFG_PMGR */

        if (sysmode == WIFI_DEVICE_MODE_EXT_STATION) {
            iface = WLAN0_IFACE;
        } else if (sysmode == WIFI_DEVICE_MODE_EXT_AP) {
            iface = WLAN1_IFACE;
#ifdef __SUPPORT_P2P__
        } else if (sysmode == WIFI_DEVICE_MODE_EXT_P2P) {
            iface = WLAN1_IFACE;
        } else if (sysmode == WIFI_DEVICE_MODE_EXT_P2P_GO) {
            iface = WLAN1_IFACE;
#endif /* __SUPPORT_P2P__ */
#ifdef __SUPPORT_WIFI_CONCURRENT__
        } else if (sysmode == WIFI_DEVICE_MODE_EXT_AP_STATION) {
            iface = WLAN1_IFACE;
#ifdef __SUPPORT_P2P__
        } else if (sysmode == WIFI_DEVICE_MODE_EXT_P2P_STATION) {
            iface = WLAN1_IFACE;
#endif /* __SUPPORT_P2P__ */
#endif // __SUPPORT_WIFI_CONCURRENT__
        }
#ifdef __SUPPORT_MESH__
        else if (sysmode == WIFI_DEVICE_MODE_EXT_MESH_POINT || sysmode == WIFI_DEVICE_MODE_EXT_MESH_PORTAL) {
            iface = WLAN1_IFACE;
        }
#endif // __SUPPORT_MESH__
        else {
            printf("[%s] Unknown sysmode ??? \n", __func__);
        }

        interface_select = iface;
        sysmode_select = sysmode;

        /* Create network apps */
        create_sys_apps(sysmode, TRUE);
    }

    return;
}

/**
 ****************************************************************************************
 * @brief       Start user applications to run RA6W1/RA6W2 ( Registered in user_apps_table[] )
 * @return      None
 ****************************************************************************************
 */
void start_user_apps(int net_chk_flag)
{
    /* Run user's apps */
    create_user_apps(get_run_mode(), net_chk_flag);

    return;
}


static void create_sys_apps(int sysmode, UCHAR net_chk_flag)
{

    run_app_task_info_t  *cur_task = sys_app_task_info;
    int i;

    for (i = 0 ; sys_apps_table[i].name != NULL ; i++) {
        /* Run matched apps with net_chk_flag */
        if (sys_apps_table[i].net_chk_flag == net_chk_flag) {
            cur_task = create_new_app(cur_task, &(sys_apps_table[i]), sysmode);
        }
    }

    sys_app_task_info = cur_task;

    return;
}

static void create_user_apps(int sysmode, UCHAR net_chk_flag)
{
    run_app_task_info_t  *cur_user_task = user_app_task_info;
    int    i;

    for (i = 0 ; user_apps_table[i].name != NULL ; i++) {
        if (user_apps_table[i].net_chk_flag == net_chk_flag) {
            cur_user_task = create_new_app(cur_user_task, &(user_apps_table[i]), sysmode);
            if ((strcmp(user_apps_table[i].name , "wifi_disconn") == 0)) {
                user_task_handler = cur_user_task->task_handler;
            }
        }
    }

    user_app_task_info = cur_user_task;

    return;
}

run_app_task_info_t *create_new_app(run_app_task_info_t *prev_task,
                                    const app_task_info_t *cur_info,
                                    int sys_mode)
{
    BaseType_t xRtn;
    run_app_task_info_t    *cur_task = NULL;

    switch (sys_mode) {
        case WIFI_DEVICE_MODE_EXT_STATION : {
            if (cur_info->run_sys_mode & RUN_STA_MODE) {
                break;
            } else {
                return NULL;
            }
        } break;

        case WIFI_DEVICE_MODE_EXT_AP : {
            if (cur_info->run_sys_mode & RUN_AP_MODE) {
                break;
            } else {
                return NULL;
            }
        } break;

#if defined ( __SUPPORT_WIFI_CONCURRENT__ )

#if defined ( __SUPPORT_P2P__ )
        case WIFI_DEVICE_MODE_EXT_P2P : {
            if (cur_info->run_sys_mode & RUN_P2P_MODE) {
                break;
            } else {
                return NULL;
            }
        } break;

        case WIFI_DEVICE_MODE_EXT_P2P_GO :
            return NULL;
#endif // __SUPPORT_P2P__

        case WIFI_DEVICE_MODE_EXT_AP_STATION :
            break;

#if defined ( __SUPPORT_P2P__ )
        case WIFI_DEVICE_MODE_EXT_P2P_STATION :
#endif // __SUPPORT_P2P__
            return NULL;

#endif    // __SUPPORT_WIFI_CONCURRENT__
    } // End of Switch

    cur_task = pvPortMalloc(sizeof(run_app_task_info_t));
    if (cur_task == NULL) {
        return NULL;
    }
    memset(cur_task, 0, sizeof(run_app_task_info_t));

    cur_task->prev = prev_task;

    // Create application task
    xRtn = xTaskCreate(cur_info->entry_func,
                    (const char *)cur_info->name,
                    cur_info->stksize,
                    (void *)&cur_info->port_no,
                    cur_info->priority,
                    &(cur_task->task_handler));

    if (xRtn != pdPASS) {
        printf("Failed to create task [%s] !!!\n", (const char *)cur_info->name);
    }

    return cur_task;
}

/* EOF */
