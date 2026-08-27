
/*
 * File Name : main_fc9000.c
 *      main entry point of FC9000 supplicant
 *
 * Copyright (c) 2014-2018, FCI, Inc.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 *
 * $Revision    : #1
 * $Date        : Sep 01, 2014
 * $Writer      : Bright Jeong,  FCI.
 *
 */

#include "FreeRTOS.h"
#include "custom_config_sdk.h"  // After app_features.h

#include "includes.h"

#include "supp_driver.h"
#include "wpa_supplicant_i.h"
#include "driver_i.h"
#include "p2p_supplicant.h"

#include "supp_eloop.h"
#include "supp_config.h"
#if CFG_PMGR
#include "sleep_mgmt_regs.h"
#include "rm_pmgr_w_instance.h"
#else
#include "defs.h"
#endif /* CFG_PMGR */
#include "osal.h"
#include "common_def.h"
#include "rm_wifi.h"

extern const wifi_cfg_t g_wifi_cfg;
static int	ra6wx_run_mode = WIFI_DEVICE_MODE_EXT_STATION;
#ifdef CONFIG_FAST_RECONNECT_V2
u8	fast_reconnect_tries;
u8	ra6wx_is_fast_reconnect = 0;
#endif
char *ra6wx_driver = "fc80211";
#ifdef	CONFIG_BRIDGE_IFACE
char *bridge_ifname = "br0";
#endif	/* CONFIG_BRIDGE_IFACE */

/* Define FreeRTOS thread control block */
TaskHandle_t ra6wx_sp_thread = NULL;

int get_run_mode(void)
{
	return ra6wx_run_mode;
}

void set_run_mode(int mode)
{
	ra6wx_run_mode = mode;
}

struct wpa_supplicant *WPA_S = NULL;
struct wpa_supplicant *get_wpa_supplicant(void)
{
	return WPA_S;	
}

static void ra6w1_supplicant_thread_entry(void * pvPraram)
{
    struct wpa_interface *ifaces;
    struct wpa_params params;
    struct wpa_global *global;
    struct wpa_supplicant *wpa_s;
#ifdef CONFIG_AP
    struct wpa_supplicant *wpa_s_new;
#endif /* CONFIG_AP */

#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
    uint8_t task_wdog_id = WATCHDOG_SERVICE_W_NOT_REGISTERED_ID;
    g_wifi_cfg.p_watchdog_service->p_api->registerTask(g_wifi_cfg.p_watchdog_service->p_ctrl, g_wifi_cfg.p_watchdog_service->p_cfg, &task_wdog_id);
    g_wifi_cfg.p_watchdog_service->p_api->notify(g_wifi_cfg.p_watchdog_service->p_ctrl, g_wifi_cfg.p_watchdog_service->p_cfg, task_wdog_id);
#endif

#if CFG_PMGR
    /* Currently,,, DPM will be operated on STA mode */
    if (get_run_mode() != WIFI_DEVICE_MODE_EXT_STATION) {
        RM_PMGR_W_dpm_disable();
    }

    if (RM_PMGR_W_dpm_is_enabled() == pdTRUE) {
        if (RM_PMGR_W_dpm_job_name_set(REG_NAME_DPM_SUPPLICANT, 0) != FSP_SUCCESS) {
            ra6wx_err_prt("[%s] RM_PMGR_W_dpm_job_name_set(REG_NAME_DPM_SUPPLICANT) "
                    "error\n", __func__);
        }
    }
#endif /* CFG_PMGR */

    os_memset(&params, 0, sizeof(params));

    ifaces = os_malloc(sizeof(struct wpa_interface));

    if (ifaces == NULL) {
#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
        g_wifi_cfg.p_watchdog_service->p_api->unregisterTask(g_wifi_cfg.p_watchdog_service->p_ctrl, task_wdog_id);
        task_wdog_id = WATCHDOG_SERVICE_W_NOT_REGISTERED_ID;
#endif
        return;
    }

    memset(ifaces, 0, sizeof(struct wpa_interface));

    ifaces->ifname = STA_DEVICE_NAME;

    ifaces->driver = ra6wx_driver;
#ifdef	CONFIG_BRIDGE_IFACE
    memset((char *)ifaces->bridge_ifname, 0, sizeof(ifaces->bridge_ifname));
    sprintf(ifaces->bridge_ifname, "%s", bridge_ifname);
#endif	/* CONFIG_BRIDGE_IFACE */

    global = wpa_supplicant_init(&params);
    if (global == NULL) {
        ra6wx_fatal_prt(" Init Fail...\n\n");
        goto out;
    }

    wpa_s = wpa_supplicant_add_iface(global, ifaces, NULL);
    if (wpa_s == NULL) {
        ra6wx_err_prt(">>> Failed to add SUPP interface ...\n");
        goto supp_init_fail;
    }

    WPA_S = wpa_s;	// save wpa_s

    /* Run RA6WX supplicant as defined mode */
    switch (get_run_mode()) {

#ifdef CONFIG_MESH
        case WIFI_DEVICE_MODE_EXT_MESH_POINT :
        case WIFI_DEVICE_MODE_EXT_MESH_PORTAL:
            /* create new interface for soft-ap */
            if (!wpas_mesh_add_interface(wpa_s)) {
                ra6wx_err_prt(">>> Failed to add MP interface ...\n");
                goto supp_init_fail;
            }

            if (get_run_mode() == WIFI_DEVICE_MODE_EXT_MESH_POINT)
            {
                if (wpa_drv_if_remove(wpa_s, WPA_IF_STATION, "wlan0")){
                    ra6wx_err_prt(">>> Supplicant Failed to remove "
                            "STA Device interface...\n");
                    goto supp_init_fail;
                }

                wpa_supplicant_remove_iface(wpa_s->global, wpa_s, 0);

                wpa_s_new->parent = wpa_s_new;
            }

            ra6wx_notice_prt("\n>>> Start MESH Point Mode...\n");
            break;

#endif /* CONFIG_MESH */


#ifdef  CONFIG_AP
        case WIFI_DEVICE_MODE_EXT_AP :
            /* create new interface for soft-ap */
            wpa_s_new = wpas_add_softap_interface(wpa_s);
            WPA_S = wpa_s_new;	// save wpa_s
            if (!wpa_s_new) {
                ra6wx_err_prt(">>> Failed to add SUPP Soft-AP interface...\n");
                goto supp_init_fail;
            }
#ifdef CONFIG_OWE_TRANS
            if (wpa_s_new->conf->owe_transition_mode == 1) {
                break;
            }
#endif	// CONFIG_OWE_TRANS
            if (wpa_drv_if_remove(wpa_s, WPA_IF_STATION, "wlan0")) {
                ra6wx_err_prt(">>> Failed to remove SUPP STA interface...\n");
                goto supp_init_fail;
            }

            wpa_supplicant_remove_iface(wpa_s->global, wpa_s, 0);
            wpa_s_new->parent = wpa_s_new;

            //ra6wx_notice_prt("\n>>> Start Soft-AP mode...\n");
            break;
#endif  /* CONFIG_AP */

#if defined(CONFIG_P2P) && defined(CONFIG_AP)
        case WIFI_DEVICE_MODE_EXT_P2P :
            //MODIFY_SUPPLICANT_FOR_FREERTOS
            wpa_s_new = wpas_p2p_add_p2pdev_interface(wpa_s, wpa_s->global->params.conf_p2p_dev);
            if (!wpa_s_new) {
                ra6wx_notice_prt(RED_COLOR ">>> Failed to add SUPP P2P interface ...\n" CLEAR_COLOR);
                goto supp_init_fail;
            }

            if (wpa_drv_if_remove(wpa_s, WPA_IF_STATION, "wlan0")) {
                ra6wx_notice_prt(">>> Failed to remove SUPP interface...\n");
                goto supp_init_fail;
            }

            wpa_supplicant_remove_iface(wpa_s->global, wpa_s, 0);
            wpa_s_new->parent = wpa_s_new;

            WPA_S = wpa_s_new;	// save wpa_s

            ra6wx_notice_prt("\n>>> Start Wi-Fi Direct mode...\n");

            eloop_register_timeout(0, 100000, wpas_p2p_find_cb, wpa_s_new, NULL);
            break;

        case WIFI_DEVICE_MODE_EXT_P2P_GO :
            //MODIFY_SUPPLICANT_FOR_FREERTOS
            wpa_s_new = wpas_p2p_add_p2pdev_interface(wpa_s, wpa_s->global->params.conf_p2p_dev);
            if (!wpa_s_new) {
                ra6wx_notice_prt(RED_COLOR ">>> Failed to add SUPP P2P interface...\n" CLEAR_COLOR);
                goto supp_init_fail;
            }

            if (wpa_drv_if_remove(wpa_s, WPA_IF_STATION, "wlan0")){
                ra6wx_notice_prt(">>> Failed to remove SUPP STA interface...\n");
                goto supp_init_fail;
            }
            wpa_supplicant_remove_iface(wpa_s->global, wpa_s, 0);
            wpa_s_new->parent = wpa_s_new;

            WPA_S = wpa_s_new;	// save wpa_s

            ra6wx_notice_prt("\n>>> Start P2P GO fixed mode...\n");

            eloop_register_timeout(0, 100000, wpas_p2p_group_add_cb, wpa_s_new, NULL);

            break;
#endif	/* defined(CONFIG_P2P) && defined(CONFIG_AP) */

#ifdef	CONFIG_CONCURRENT
#if defined(CONFIG_STA) && defined(CONFIG_AP)
        case WIFI_DEVICE_MODE_EXT_AP_STATION :
            /* create new interface for soft-ap */
            wpa_s_new = wpas_add_softap_interface(wpa_s);
            if (!wpa_s_new) {
                ra6wx_notice_prt(">>> Supplicant fail to add "
                        "Soft-AP interface...\n");
                goto supp_init_fail;
            }
            WPA_S = wpa_s_new;	// save wpa_s

            ra6wx_notice_prt("\n>>> Start STA & SOFT-AP Concurrent mode...\n");
            break;
#endif /* defined(CONFIG_STA) && defined(CONFIG_AP) */

#if defined(CONFIG_STA) && defined(CONFIG_P2P) && defined(CONFIG_AP)
#ifdef CONFIG_P2P_CONCURRENT
        case WIFI_DEVICE_MODE_EXT_P2P_STATION :
            /* Insert proper code for sta:p2p concurrent mode if needed */
            //MODIFY_SUPPLICANT_FOR_FREERTOS
            wpa_s_new = wpas_p2p_add_p2pdev_interface(wpa_s, wpa_s->global->params.conf_p2p_dev);
            if (!wpa_s_new) {
                ra6wx_notice_prt(">>> Supplicant fail to add P2P interface...\n");
                goto supp_init_fail;
            }

            WPA_S = wpa_s_new;	// save wpa_s

            ra6wx_notice_prt("\n>>> Start STA & P2P Concurrent mode...\n");
            os_sleep(0, 100000);
            eloop_register_timeout(5, 0, wpas_p2p_find_cb, wpa_s_new, NULL);
            break;
#endif // CONFIG_P2P_CONCURRENT
#endif /* defined(CONFIG_STA) && defined(CONFIG_P2P) && defined(CONFIG_AP) */
#endif	/* CONFIG_CONCURRENT */

#ifdef CONFIG_STA
        case WIFI_DEVICE_MODE_EXT_STATION :
#if CFG_PMGR
            if (RM_PMGR_W_dpm_is_wakeup() == pdFALSE) {
                ra6wx_notice_prt(">>> Start STA mode...\n");
            }
#else
            ra6wx_notice_prt(">>> Start STA mode...\n");
#endif /* CFG_PMGR */

            break;
#endif /* CONFIG_STA */

        default :
            ra6wx_notice_prt(">>> Unsupported mode(%d)...\n", get_run_mode());
            wpa_drv_if_remove(wpa_s, WPA_IF_STATION, "wlan0");
            goto supp_init_fail;
    }

#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
    g_wifi_cfg.p_watchdog_service->p_api->suspend(g_wifi_cfg.p_watchdog_service->p_ctrl, task_wdog_id);
#endif

    wpa_supplicant_run(global, global->ifaces);

#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
    g_wifi_cfg.p_watchdog_service->p_api->resumeAndNotify(g_wifi_cfg.p_watchdog_service->p_ctrl, g_wifi_cfg.p_watchdog_service->p_cfg, task_wdog_id);
#endif

supp_init_fail :
    ra6wx_err_prt(RED_COLOR ">>> wpa_supplicant_deinit ...\n" CLEAR_COLOR);
    wpa_supplicant_deinit(global);
out:
    os_free(ifaces);

#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
    g_wifi_cfg.p_watchdog_service->p_api->unregisterTask(g_wifi_cfg.p_watchdog_service->p_ctrl, task_wdog_id);
    task_wdog_id = WATCHDOG_SERVICE_W_NOT_REGISTERED_ID;
#endif

    ra6wx_sp_thread = NULL;
    vTaskDelete(NULL);
}

/**************************************************/
/* Supp mem pool space */
#define RA6WX_SP_POOL_SIZE	((1024*5)/2) // 5 Kbytes
#define RA6WX_SP_POOL_MESH_PORTAL_SIZE (RA6WX_SP_POOL_SIZE + (1024*1/2)) // RA6WX_SP_POOL_SIZE + 1 Kbytes
#define RA6WX_SP_PRIORITY   19

BaseType_t start_ra6w1_wpa_supplicant(void)
{
	BaseType_t status;

#ifdef FOR_DEBUG
	printf(">>> Start RA6W1/RA6W2 wpa_supplicant ...\n");
#endif

	status = xTaskCreate(ra6w1_supplicant_thread_entry,
                                "ra6w1_supp",
                                RA6WX_SP_POOL_SIZE,
                                (void *) NULL,
                                RA6WX_SP_PRIORITY,
                                &ra6wx_sp_thread);

	return status;
}

void get_supp_ver(char *buf)
{
	if (buf == NULL) {
		ra6wx_notice_prt("[%s] NULL pointer...\n", __func__);
		return;
	}

	sprintf(buf, "%s %s", VER_NUM, RELEASE_VER_STR);
}

int get_sta_signal_poll(void)
{
    if ((rm_wifi_is_wpa_state(WPA_COMPLETED, 0) == FSP_SUCCESS)) {
    	struct wpa_supplicant *wpa_s;
        struct wpa_signal_info si;
        int ret;

        wpa_s = get_wpa_supplicant();
        
        ret = wpa_drv_signal_poll(wpa_s, &si);
        if (ret) {
            return -WPA_INVALID_NOISE;
        }
        return si.current_signal;
    } 
    else 
    {
        return -WPA_INVALID_NOISE;
    }
}

/* EOF */
