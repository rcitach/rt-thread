/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include <stdio.h>
#include <string.h>
#include "rm_wifi.h"
#include "rm_wifi_helper.h"
#include "bsp_clocks.h"
#include "lwip/init.h"
#include "sys_clock_mgr.h"

#include "rm_wifi_event.h"
#include "rm_sntp.h"
#include "sntp.h"
#include "portable.h"
#include "net_network_main.h"
#include "util_api.h"
#ifdef RM_STDIO_W
#include "rm_stdio_w_cfg.h"
#endif
#ifdef CONFIG_RTT
#include "RTT/SEGGER_RTT.h"
#endif
#if (defined (__SUPPORT_WPS_BTN__) || defined (__SUPPORT_FACTORY_RESET_BTN__)) && defined(__SUPPORT_WIFI_USER_GPIO__)
#include "rm_wifi_user_app_gpio_handle.h"
#endif

#include "lwip/dhcp.h"
#if (TEST_APP_START == 1)
#include "app_start.h"
#else
#define FAST_SLEEPMODE_MOD          "sleepmode12"
#endif /* TEST_APP_START == 1 */
#include "common_def.h"
#include "rm_vee_flash_w_rrq_nvram.h"

#if CFG_PMGR
#include "rm_pmgr_api.h"
#include "rm_pmgr_w_instance.h"
#else
#include "defs.h"
#endif /* CFG_PMGR */

#include "r_rtc_w.h"
#ifdef RM_MAP_PERSISTANT_W
#include "rm_map_persistant_w.h"
#endif

#if defined ( __SUPPORT_FAST_CONN_SLEEP_2__ )
#define   FAST_CON_ARP_CHECK_MAX_CNT    5    // During N * 200ms = 1s checking
TaskHandle_t fast_sleepmode_init_handler = NULL;
#endif /* __SUPPORT_FAST_CONN_SLEEP_2__ */
int  interface_select = 0;
int  sysmode_select = 0;
extern void dpm_ext_wu_mon_start(void);

#define OTP_SKU_ID_DEF (TIN_SKU_BUILD_ID)
static uint32_t otp_sku_id = OTP_SKU_ID_DEF;
static int build_sku_id = OTP_SKU_ID_DEF;

#define WIFI_INIT_STACK_SIZE (configMINIMAL_STACK_SIZE * 8 * sizeof(StackType_t))

cm_clock_callback_t mycevt_callback;

#if (FSP_USE_HEAP_5 == 0)
/* Allocate the memory for the heap. */
PRIVILEGED_DATA static HeapRegion_t xHeapRegions[3];

#if( configAPPLICATION_ALLOCATED_HEAP == 1 )
/* The application writer has already defined the array used for the RTOS
 *  heap - probably so it can be placed in a special segment or address. */
extern uint8_t ucHeap1[ (configTOTAL_HEAP_SIZE >> 2) * 3 ];
extern uint8_t ucHeap2[ (configTOTAL_HEAP_SIZE >> 2) * 1 ];
#else
static uint8_t ucHeap1[ (configTOTAL_HEAP_SIZE >> 2) * 3 ] __attribute__((section("os_heap")));
static uint8_t ucHeap2[ (configTOTAL_HEAP_SIZE >> 2) * 1 ] __attribute__((section("os_heap")));
#endif /* configAPPLICATION_ALLOCATED_HEAP */

HeapRegion_t *get_heap_regions()
{
    return &xHeapRegions[0];
}
#endif

/* From NVRAM ("N0_FST_CONNECT") */
unsigned char fast_connection_sleep_flag = pdFALSE;

extern void set_run_mode(int mode);
extern void print_version(void);
extern uint32_t get_rwnx_sleep_period_time(uint64_t rtctime);
extern uint32_t rwnx_sku_id_read_otp(void);

static void prvSetupHardware(void)
{
    /* Init hardware */

    /* need enable at flash boot */
    REG_SETF(CRG_TOP, CLK_AMBA_REG, PERI_CLK_ENABLE, 1);

#if defined(__SUPPORT_WIFI_USER_GPIO__) && (defined (__SUPPORT_WPS_BTN__) || defined (__SUPPORT_FACTORY_RESET_BTN__))
    /* Set configuration for H/W button */
    rm_wifi_app_gpio_config_button();
#endif /* defined(__SUPPORT_WIFI_USER_GPIO__) && ( defined (__SUPPORT_WPS_BTN__) || defined (__SUPPORT_FACTORY_RESET_BTN__)) */
}

__RETAINED_CODE static void cm_change_clock_callback(sys_clk_is_t clksrc, uint32_t freq, void *param)
{
    (void) param;

    if ( freq != 0) {
        if (clksrc == SYS_CLK_IS_NONE) {
            ;
        } else if(clksrc == SYS_CLK_IS_LP) {
            REG_SETF(CRG_TOP, CLK_CTRL_REG, PLL_PERI_ENABLE, 0);    // peri pll clock
            REG_SETF(CRG_TOP, CLK_AMBA_REG, OQSPIF_DIV, 0);         // OQSPI 1/1 speed
            REG_SETF(CRG_PER, SET_CLK_COM_REG, I2C_CLK_SEL, 0); /* change to DIVN */
            REG_SETF(CRG_PER, SET_CLK_COM_REG, I2C2_CLK_SEL, 0); /* change to DIVN */
        } else if(clksrc == SYS_CLK_IS_XTAL40M) {
            REG_SETF(CRG_TOP, CLK_CTRL_REG, PLL_PERI_ENABLE, 0);    // peri pll clock
            REG_SETF(CRG_TOP, CLK_AMBA_REG, OQSPIF_DIV, 0);         // OQSPI 1/1 speed
            REG_SETF(CRG_PER, SET_CLK_COM_REG, I2C_CLK_SEL, 0); /* change to DIVN */
            REG_SETF(CRG_PER, SET_CLK_COM_REG, I2C2_CLK_SEL, 0); /* change to DIVN */
        } else {
            REG_SETF(CRG_TOP, CLK_CTRL_REG, PLL_PERI_ENABLE, 1);    // peri pll clock
            REG_SETF(CRG_TOP, CLK_AMBA_REG, OQSPIF_DIV, 1);         // OQSPI 1/2 speed
            // Using PLL then set clock selection to 1
            REG_SETF(CRG_PER, SET_CLK_COM_REG, I2C_CLK_SEL, 1); /* change to DIVC */
            REG_SETF(CRG_PER, SET_CLK_COM_REG, I2C2_CLK_SEL, 1); /* change to DIVC */
        }
    }
    else
    {
            uint32_t curdiv ;

            curdiv = REG_GETF(CRG_TOP, CLK_AMBA_REG, OQSPIF_DIV);
            curdiv = curdiv + 1;
            REG_SETF(CRG_TOP, CLK_AMBA_REG, OQSPIF_DIV, curdiv);         // speed-down
    }
}

#if CFG_PMGR
static void cfg_dpm_mode_by_boot_scenario(int wakeup_type)
{
    int wakeup_type_sleep2 = (BSP_WAKEUP_SOURCE_GPIO | BSP_WAKEUP_SOURCE_WAKEUP_COUNTER | 
                              BSP_WAKEUP_SOURCE_POR  | BSP_WAKEUP_SOURCE_WATCHDOG | BSP_WAKEUP_SOURCE_SENSOR);
    
    // sleep mode2 wakeup
    if ( ((wakeup_type & BSP_WAKEUP_RESET_WITH_RETENTION) == 0) && ((wakeup_type & wakeup_type_sleep2) != 0)) {
        if (RM_PMGR_W_dpm_mode_get_from_nvram()) {
            RM_PMGR_W_rtm_static_set(RTM_STATIC_KEY_DPM_WAKEUP_FLAG, 0, 0);
        }
    // reset
    } else if (wakeup_type == BSP_WAKEUP_RESET_WITH_RETENTION || wakeup_type == BSP_WAKEUP_RESET) {    // CMD Reboot
        if (RM_PMGR_W_dpm_mode_get_from_nvram()) {
            RM_PMGR_W_rtm_static_set(RTM_STATIC_KEY_DPM_WAKEUP_FLAG, 0, 0);
        }
    // sleep mode3 wakeup
    } else {
        /* DPM mode (run or stop) check!! */
        /* Wakeup source 0x0a by TIM & RTC, Wakeup source 0x09 by External Interrupt */
        if (RM_PMGR_W_dpm_mode_get() == 0) {
            RM_PMGR_W_dpm_disable();
            RM_PMGR_W_rtm_static_set(RTM_STATIC_KEY_DPM_WAKEUP_FLAG, 0, 0);
        } else {
            if (RM_WIFI_dpm_ptim_event_get_pre() == RTOS4DPM_ST_UNDEFINED) {
                /* Wakeup from sleep3 (not DPM_sleep) */
                RM_PMGR_W_rtm_static_set(RTM_STATIC_KEY_DPM_WAKEUP_FLAG, 0, 0);
            } else {
                /* Have to set DPM Wakeup state flag
                 *    when wakeup as mode 0xa or DPM wakeup with external interrupt.
                 */
                if (wakeup_type & BSP_WAKEUP_RESET_WITH_RETENTION) {
                    RM_PMGR_W_rtm_static_set(RTM_STATIC_KEY_DPM_WAKEUP_FLAG, 1, 0);
                }
            }
        }
    }
}
#endif /* CFG_PMGR */

static void boot_scenario(void)
{
    /* Wakeup source should be saved */
    bsp_wakeup_source_mask_t wakeup_src;

    /* wake up source read */
    wakeup_src = R_BSP_WakeupSourceGet();

    extern void dpm_mac_set_wakeup_source(bsp_wakeup_source_mask_t wakeup_source);
#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
    if (0xDEADBEEFU == g_watchdog_service_w_nmi_event_data[0]) {
        printf("\nWakeup source is (0x%x --> 0x0)\n", wakeup_src);
        if(wakeup_src & BSP_WAKEUP_SOURCE_GPIO)
        {
            printf("\n  --Wakeup Pin 0x%x\n", R_BSP_WakeupSourcePinGet());
        }
        wakeup_src = 0x0;
    }
    else {
#else
    {
#endif
        printf("\nWakeup source is 0x%x\n", wakeup_src);
        if(wakeup_src & BSP_WAKEUP_SOURCE_GPIO)
        {
            printf("  --Wakeup Pin 0x%x\n\n", R_BSP_WakeupSourcePinGet());
        }
    }

#if CFG_PMGR
    RM_PMGR_W_dpm_wakeup_src_save(wakeup_src);
#endif /* CFG_PMGR */
    dpm_mac_set_wakeup_source(wakeup_src);

#if CFG_PMGR
    RM_PMGR_W_rtm_dpm_area_init(wakeup_src);
#ifdef BUILD_OPT_RA6W1_MAC
    cfg_dpm_mode_by_boot_scenario(wakeup_src);
#endif //BUILD_OPT_RA6W1_MAC
#endif /* CFG_PMGR */
}

#if defined ( __SUPPORT_FAST_CONN_SLEEP_2__ )
static void fast_sleepmode_init(void *arg)
{
    RA6W1_UNUSED_ARG(arg);

    int iface = interface_select;
#if defined ( __SUPPORT_DHCPC_IP_TO_STATIC_IP__ )
    int t_static_ip = 0;
#endif // __SUPPORT_DHCPC_IP_TO_STATIC_IP__

    if (fast_connection_sleep_flag == pdFALSE || get_run_mode() != WIFI_DEVICE_MODE_EXT_STATION) {
        vTaskDelete(NULL);
        return;
    }

#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
    uint8_t sys_wdog_id = WATCHDOG_SERVICE_W_NOT_REGISTERED_ID;
    g_wifi_cfg.p_watchdog_service->p_api->registerTask(g_wifi_cfg.p_watchdog_service->p_ctrl, g_wifi_cfg.p_watchdog_service->p_cfg, &sys_wdog_id);
#endif

#if defined ( __SUPPORT_DHCPC_IP_TO_STATIC_IP__ )
    extern int ra6w1_get_temp_staticip_mode(int iface_flag);
    
    t_static_ip = ra6w1_get_temp_staticip_mode(WLAN0_IFACE);

    if (get_netmode(WLAN0_IFACE) == STATIC_IP) {
        UINT arp_req_wait_cnt = 0;

        arp_req_wait_cnt = 0;

        // Check communication status.
        while (pdTRUE) {
#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
            g_wifi_cfg.p_watchdog_service->p_api->notify(g_wifi_cfg.p_watchdog_service->p_ctrl, g_wifi_cfg.p_watchdog_service->p_cfg, sys_wdog_id);
#endif

#if CFG_PMGR
            if (RM_WIFI_dpm_supp_is_connected() == pdPASS) {
#else
            if(rm_wifi_is_wpa_state(WPA_COMPLETED, 0) == FSP_SUCCESS) {
#endif /* CFG_PMGR */
                if (do_autoarp_check() == 0) { // GW ARP Found
                    //printf("[%s] GW ARP Found\n", __func__);
                    if (arp_req_wait_cnt > 0)
                        printf("Gateway ARP Check Retry %d - OK\n", arp_req_wait_cnt);

                    arp_req_wait_cnt = 0;

#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
                    g_wifi_cfg.p_watchdog_service->p_api->suspend(g_wifi_cfg.p_watchdog_service->p_ctrl, sys_wdog_id);
                    vTaskDelay(portCONVERT_MS_2_TICKS(100)); // 100ms
                    g_wifi_cfg.p_watchdog_service->p_api->resumeAndNotify(g_wifi_cfg.p_watchdog_service->p_ctrl, g_wifi_cfg.p_watchdog_service->p_cfg, sys_wdog_id);
#endif
                    continue;
                } else if (arp_req_wait_cnt < FAST_CON_ARP_CHECK_MAX_CNT) {    /* ARP Checking for 1000ms */
                    //printf(">>> Gateway ARP Checking Wait %dms\n", arp_req_wait_cnt * 200);
                    arp_req_wait_cnt++;
#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
                    g_wifi_cfg.p_watchdog_service->p_api->suspend(g_wifi_cfg.p_watchdog_service->p_ctrl, sys_wdog_id);
                    vTaskDelay(portCONVERT_MS_2_TICKS(200)); // 200ms
                    g_wifi_cfg.p_watchdog_service->p_api->resumeAndNotify(g_wifi_cfg.p_watchdog_service->p_ctrl, g_wifi_cfg.p_watchdog_service->p_cfg, sys_wdog_id);
#endif
                    continue;
                } else {    /* Communication error(Gateway IP ARP No response): DHCP is started  */
#if LWIP_DHCP
                    struct netif *netif = (struct netif *)netif_get_by_index(RM_WIFI_NETIF_INDEX(iface));
#endif /* LWIP_DHCP */
                    arp_req_wait_cnt = 0;

                    printf(">>> Gateway is no ARP response, Start DHCP_Client\n");

                    if (t_static_ip) {
                        /* Set DHCP Client as netmode */
                        set_netmode(iface, DHCPCLIENT, pdTRUE);
#if LWIP_DHCP
                        dhcp_start(netif);
#endif /* LWIP_DHCP */

#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
                        g_wifi_cfg.p_watchdog_service->p_api->suspend(g_wifi_cfg.p_watchdog_service->p_ctrl, sys_wdog_id);
                        vTaskDelay(portCONVERT_MS_2_TICKS(1000));        //1000ms sleep
                        g_wifi_cfg.p_watchdog_service->p_api->resumeAndNotify(g_wifi_cfg.p_watchdog_service->p_ctrl, g_wifi_cfg.p_watchdog_service->p_cfg, sys_wdog_id);
#endif

                        break;
                    } else if (get_netmode(iface) == DHCPCLIENT) {
#if LWIP_DHCP
                        dhcp_renew(netif);
#endif /* LWIP_DHCP */

                        break;
                    }

                    continue;
                }

                break;
            } else {
#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
                g_wifi_cfg.p_watchdog_service->p_api->suspend(g_wifi_cfg.p_watchdog_service->p_ctrl, sys_wdog_id);
                vTaskDelay(portCONVERT_MS_2_TICKS(10)); // 10ms
                g_wifi_cfg.p_watchdog_service->p_api->resumeAndNotify(g_wifi_cfg.p_watchdog_service->p_ctrl, g_wifi_cfg.p_watchdog_service->p_cfg, sys_wdog_id);
#endif
            }
        }
    }
#endif // __SUPPORT_DHCPC_IP_TO_STATIC_IP__

#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
    g_wifi_cfg.p_watchdog_service->p_api->unregisterTask(g_wifi_cfg.p_watchdog_service->p_ctrl, sys_wdog_id);
#endif

    vTaskDelete(NULL);

    return;
}

static void fast_sleepmode_init_start(void)
{
	int	ret;
	ret = xTaskCreate(fast_sleepmode_init,
	                  FAST_SLEEPMODE_MOD,
	                  (256),
	                  (void *)NULL,
	                  (OS_TASK_PRIORITY_USER + 6),
	                  &fast_sleepmode_init_handler);
	if (ret != pdPASS) {
	    printf("[%s] ERROR(%d)\n", __func__, ret);
	}

#if CFG_PMGR
	if (get_run_mode() == WIFI_DEVICE_MODE_EXT_STATION && RM_PMGR_W_dpm_is_enabled() && fast_connection_sleep_flag) {
		RM_PMGR_W_dpm_job_name_set(FAST_SLEEPMODE_MOD, 0);
	}
#endif /* CFG_PMGR */

	return;
}
#endif /* __SUPPORT_FAST_CONN_SLEEP_2__ */

static bool chk_support_cpu_clk(int cpu_clock)
{
    switch (cpu_clock) {
        case cpuclk_2M:          //!< 2.5 MHz, divided by 16 in XTAL
        case cpuclk_5M:          //!< 5  MHz, divided by 8 in XTAL
        case cpuclk_10M:         //!< 10 MHz, divided by 4 in XTAL
        case cpuclk_20M:         //!< 20 MHz, divided by 2 in XTAL

        case cpuclk_26M:         //!< 26 MHz, divided by 4 in SYSCLK 106MHz
        case cpuclk_34M:         //!< 34 MHz, divided by 4 in SYSCLK 137MHz
        case cpuclk_40M:         //!< 40 MHz, divided by 4 in SYSCLK 160MHz

        case cpuclk_53M:         //!< 53 MHz, divided by 2 in SYSCLK 106MHz
        case cpuclk_68M:         //!< 68 MHz, divided by 2 in SYSCLK 137MHz
        case cpuclk_80M:         //!< 80 MHz, divided by 2 in SYSCLK 160MHz

        case cpuclk_106M:        //!< 106 MHz
        case cpuclk_137M:        //!< 137 MHz
        case cpuclk_160M:        //!< 160 MHz
#if (HW_DESCOPED_CLOCK == 1)
        case cpuclk_192M:      //!< 192 MHz, descoped
        case cpuclk_240M:      //!< 240 MHz, descoped
#endif // HW_DESCOPED_CLOCK
           return TRUE;

        default:
            return FALSE;
    }
}

static int get_cpu_clock_nvram(void)
{
    int cpu_clock = 0;

#ifdef RM_MAP_PERSISTANT_W
    uint16_t data_length;
    RM_MAP_PERSISTANT_W_Read_UINT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_BOOTCFG, "clk.cpu", &cpu_clock, &data_length);
#endif

    if (chk_support_cpu_clk(cpu_clock)) {
        return cpu_clock;
    }

    return 0;
}

void print_sys_mode(UINT mode)
{
    char temp_str[30];

    memset(temp_str, 0, sizeof(temp_str));

    switch (mode) {
    case WIFI_DEVICE_MODE_EXT_STATION   :
        strcpy(temp_str, "Station Only");
        break;

    case WIFI_DEVICE_MODE_EXT_AP    :
        strcpy(temp_str, "Soft-AP");
        break;

#ifdef __SUPPORT_P2P__
    case WIFI_DEVICE_MODE_EXT_P2P   :
        strcpy(temp_str, "WiFi Direct");
        break;

    case WIFI_DEVICE_MODE_EXT_P2P_GO  :
        strcpy(temp_str, "WiFi Direct P2P GO fixed");
        break;
#endif /* __SUPPORT_P2P__ */

    case WIFI_DEVICE_MODE_EXT_AP_STATION   :
        strcpy(temp_str, "Station & Soft-AP");
        break;

#ifdef __SUPPORT_P2P__
    case WIFI_DEVICE_MODE_EXT_P2P_STATION  :
        strcpy(temp_str, "Station & WiFi Direct");
        break;
#endif /* __SUPPORT_P2P__ */

    default:
        strcpy(temp_str, "Unknown Mode ...");

    }

    printf("\nSystem Mode : %s (%d)\r\n", temp_str, mode);
}

static void wifi_init(void *pvParameters)
{
    long __timezone_offset;
    TaskHandle_t xHandle = NULL;

    RA6W1_UNUSED_ARG(pvParameters);

    /*
     * In special cases, such as "BLE-combo + WIFI",
     * this task will occupy CPU for a long time (over 4 sec?).
     * Therefore, we need to set watchdog service suspend to prevent the watchdog reset.
     */
    xHandle = xTaskGetCurrentTaskHandle();

    mycevt_callback.func = &cm_change_clock_callback;
    mycevt_callback.priority = cm_cb_priority_coupled_hyper_tightly;
    mycevt_callback.param = NULL;

    cm_register_clock_callback(&(mycevt_callback));

    // If PLL-Clock is selected, the clock source of OQSPIC is supplied at 160 MHz.
    // So the divider of OQSPIC must be set as below.
    /* No problem at room temperature, but at high temperature(85C) it may cause boot failure without specific oqspi configuration.
       So increase oqspi clock to 80Mhz after init_environ()call. in the init_environ() it sets the oqspi spcific registers for high speed. */
    REG_SETF(CRG_TOP, CLK_AMBA_REG, OQSPIF_DIV, 2);

    cm_sys_clk_init(sysclk_PLL480);

    // System clock will not be changed to SYS_CLK_IS_XTAL40M if XTAL40M is not ready.
    // In this case the system clock will be changed when the XTAL32M_RDY interrupt occurs.
    cm_halt_until_xtalm_ready();

    cm_cpu_clk_set(cpuclk_160M);

    REG_SETF(CRG_TOP, CLK_CTRL_REG, PLL_PERI_ENABLE, 1);    // peri pll clock

    /* Set the desired wakeup mode. */

#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
    /* Register the Idle task first. */
    uint8_t idle_task_wdog_id = WATCHDOG_SERVICE_W_NOT_REGISTERED_ID;
    g_wifi_cfg.p_watchdog_service->p_api->registerTask(g_wifi_cfg.p_watchdog_service->p_ctrl, g_wifi_cfg.p_watchdog_service->p_cfg, &idle_task_wdog_id);
    ASSERT_WARNING(WATCHDOG_SERVICE_W_NOT_REGISTERED_ID != idle_task_wdog_id);
    g_wifi_cfg.p_watchdog_service->p_api->setIdleId(g_wifi_cfg.p_watchdog_service->p_ctrl, idle_task_wdog_id);

    uint8_t sys_wdog_id = WATCHDOG_SERVICE_W_NOT_REGISTERED_ID;
    g_wifi_cfg.p_watchdog_service->p_api->registerTask(g_wifi_cfg.p_watchdog_service->p_ctrl, g_wifi_cfg.p_watchdog_service->p_cfg, &sys_wdog_id);
    g_wifi_cfg.p_watchdog_service->p_api->notify(g_wifi_cfg.p_watchdog_service->p_ctrl, g_wifi_cfg.p_watchdog_service->p_cfg, sys_wdog_id);
    g_wifi_cfg.p_watchdog_service->p_api->suspend(g_wifi_cfg.p_watchdog_service->p_ctrl, sys_wdog_id);
#endif

    /* Prepare the hardware to run this demo. */
    prvSetupHardware();

#ifndef RT_USING_LWIP
    ra6w1_mem_init();
#endif

#if (dg_configCODE_LOCATION == NON_VOLATILE_IS_FLASH)
    /* init_environ() is added to improve the speed of vee_nvparam. */
    REG_SETF(CRG_TOP, CLK_AMBA_REG, OQSPIF_DIV, 1);

    /* Wakeup source should be saved */
    //==============================================
    // Boot Scenario
    //==============================================

    /* Read SKU_ID from OTP */
    otp_sku_id = rwnx_sku_id_read_otp();

    boot_scenario();

    // TODO: dpm wakeup check
    if (get_cpu_clock_nvram() > 0)
        cm_cpu_clk_set(get_cpu_clock_nvram());
    else if (cm_cpu_clk_get() != cpuclk_160M)
        cm_cpu_clk_set(cpuclk_160M); // Default CPU Clock

#if CFG_PMGR
    if (RM_PMGR_W_dpm_wakeup_src_get() == BSP_WAKEUP_SOURCE_POR
         || RM_PMGR_W_dpm_wakeup_src_get() == BSP_WAKEUP_RESET
         || RM_PMGR_W_dpm_wakeup_src_get() == BSP_WAKEUP_RESET_WITH_RETENTION)
#endif /* CFG_PMGR */
#if CFG_CLI
        print_version();
#endif
#else
    boot_scenario();
#endif /* (dg_configCODE_LOCATION == NON_VOLATILE_IS_FLASH) */

#if defined (__SUPPORT_PRODTEST__)
    start_prodtest_task();
#endif

#if (dg_configCODE_LOCATION == NON_VOLATILE_IS_FLASH)
#if CFG_PMGR
    if (RM_PMGR_W_dpm_wakeup_src_get() == BSP_WAKEUP_SOURCE_POR
        || RM_PMGR_W_dpm_wakeup_src_get() == BSP_WAKEUP_RESET
        || RM_PMGR_W_dpm_wakeup_src_get() == BSP_WAKEUP_RESET_WITH_RETENTION
        || RM_PMGR_W_dpm_wakeup_src_get() == BSP_WAKEUP_SOURCE_WAKEUP_COUNTER) {
#endif /* CFG_PMGR */
        set_run_mode(get_sys_mode());      // Set Wi-Fi running mode
        print_sys_mode(get_run_mode());
#if CFG_PMGR
    }
#endif /* CFG_PMGR */

#ifdef __SUPPORT_FAST_CONN_SLEEP_2__
#if CFG_PMGR
    extern UCHAR dpm_slp_time_reduce_flag;
    fast_connection_sleep_flag = ra6w1_get_fast_connection_mode();

    /* Let's reduce the booting time in Fast connection mode */
    if (fast_connection_sleep_flag == pdTRUE) {
        PRINTF(YELLOW_COLOR "\n>>> Wi-Fi Fast_Connection mode ...\n" CLEAR_COLOR);
        dpm_slp_time_reduce_flag = pdTRUE;
    } else {
        dpm_slp_time_reduce_flag = pdFALSE;
    }
#endif /* CFG_PMGR */
#endif /* __SUPPORT_FAST_CONN_SLEEP_2__ */
#endif /* (dg_configCODE_LOCATION == NON_VOLATILE_IS_FLASH) */

#if CFG_PMGR
    vPortEnterCritical();
    RM_PMGR_W_dpm_timer_task_create();
    R_BSP_WakeupSourcePinClear();
    vPortExitCritical();
#endif

    bsp_rtc_disable_bod();

#if CFG_PMGR
    RM_PMGR_W_dpm_environ_init();

    if (RM_PMGR_W_dpm_is_wakeup() == pdFALSE) {
#endif /* CFG_PMGR */
#if CFG_PMGR
        /* get time zone from nvram and update rtm with it */
        RM_PMGR_W_rtm_static_set(RTM_STATIC_KEY_TIMEZONE, get_time_zone(), 0);
#if defined ( __SUPPORT_SNTP_CLIENT__ )
        RM_PMGR_W_rtm_static_set(RTM_STATIC_KEY_SNTP_PERIOD, sntp_get_period(), 0);
#endif // __SUPPORT_SNTP_CLIENT__
    }
#endif /* CFG_PMGR */

    /* init RTC_W Calendar time service */
    g_wifi_cfg.p_rtc_w_service->p_api->open(g_wifi_cfg.p_rtc_w_service->p_ctrl,
                                                g_wifi_cfg.p_rtc_w_service->p_cfg);
#if CFG_PMGR
    if (RM_PMGR_W_dpm_is_wakeup() == pdFALSE)
#endif /* CFG_PMGR */
    {
        __timezone_offset = get_time_zone();
        R_RTC_W_CalendarTimeZoneSet(R_RTC_W_GetCtrl(), &__timezone_offset);
    }

    ra6w1_network_main_init_wlan();

#if (TEST_APP_START == 1)
    ra6w1_network_main_init_system_apps(pdFALSE); // just starts

    if (ra6w1_network_main_get_wlaninit_mode()) {
        ra6w1_network_main_init_system_apps(pdTRUE); // regi_dpm -> wait until net -> starts

#if defined ( __SUPPORT_FAST_CONN_SLEEP_2__ )
        fast_sleepmode_init_start();
#endif /* __SUPPORT_FAST_CONN_SLEEP_2__ */
  	}
#else
#if defined ( __SUPPORT_FAST_CONN_SLEEP_2__ )
    if (ra6w1_network_main_get_wlaninit_mode()) {
#if defined ( __SUPPORT_SNTP_CLIENT__ )
        extern unsigned int start_sntp(void);
        if (sntp_get_use()) {
            start_sntp();
        }
#endif // __SUPPORT_SNTP_CLIENT__

#if defined ( __SUPPORT_FAST_CONN_SLEEP_2__ )
        fast_sleepmode_init_start();
#endif /* __SUPPORT_FAST_CONN_SLEEP_2__ */
    }
#endif /* defined (__SUPPORT_WEBSOCKET_CLIENT_FOR_ATCMD__) || ( __SUPPORT_FAST_CONN_SLEEP_2__ ) */
#endif /* (TEST_APP_START == 1) */

#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
    g_wifi_cfg.p_watchdog_service->p_api->resumeAndNotify(g_wifi_cfg.p_watchdog_service->p_ctrl, g_wifi_cfg.p_watchdog_service->p_cfg, sys_wdog_id);
    g_wifi_cfg.p_watchdog_service->p_api->unregisterTask(g_wifi_cfg.p_watchdog_service->p_ctrl, sys_wdog_id);
#endif

    vTaskDelete(xHandle);
}

void rm_wifi_init(void)
{
    BaseType_t status;
    TaskHandle_t xHandle;

#ifdef CONFIG_RTT
    SEGGER_RTT_ConfigUpBuffer(0, NULL, NULL, 0, SEGGER_RTT_MODE_NO_BLOCK_SKIP);
#endif

#if (FSP_USE_HEAP_5 == 0)
    {
        HeapRegion_t	*p_xHeapRegions = xHeapRegions;

        if ( (uint32_t)ucHeap1 < (uint32_t)ucHeap2 ) {
            /* region 0 : ucHeap1 */
            p_xHeapRegions->pucStartAddress = ucHeap1;
            p_xHeapRegions->xSizeInBytes = (size_t)((configTOTAL_HEAP_SIZE >> 2) * 3) ;

            /* region 1 : ucHeap2 */
            ++p_xHeapRegions;
            p_xHeapRegions->pucStartAddress = ucHeap2;
            p_xHeapRegions->xSizeInBytes = (size_t)((configTOTAL_HEAP_SIZE >> 2) * 1) ;

        } else {
            /* region 0 : ucHeap2 */
            p_xHeapRegions->pucStartAddress = ucHeap2;
            p_xHeapRegions->xSizeInBytes = (size_t)((configTOTAL_HEAP_SIZE >> 2) * 1) ;

            /* region 1 : ucHeap1 */
            ++p_xHeapRegions;
            p_xHeapRegions->pucStartAddress = ucHeap1;
            p_xHeapRegions->xSizeInBytes = (size_t)((configTOTAL_HEAP_SIZE >> 2) * 3) ;

        }
        /* End mark */
        ++p_xHeapRegions;
        p_xHeapRegions->pucStartAddress = NULL;
        p_xHeapRegions->xSizeInBytes = 0;

        vPortDefineHeapRegions(xHeapRegions);
    }
#endif

    status = xTaskCreate(wifi_init, "WifiInit",
                         WIFI_INIT_STACK_SIZE, NULL,
                         OS_TASK_PRIORITY_HIGHEST, &xHandle);
    ASSERT_ERROR_UNINIT(status == pdPASS);

#if (BSP_CFG_RTOS != 2)
    /* Start the tasks and timer running. */
    vTaskStartScheduler();
#endif

    if (ra6w1_network_main_get_wlaninit_mode())
    {
        /* Wait for supplicant initialization (up to 1 sec) */
        if (!wait_supplicant_done(100))
        {
            printf(">>> Supplicant start failed\n");
        }
    }
}

const char * rm_wifi_otp_sku_id_get_str(void)
{
    switch (otp_sku_id) {
        case TIN_SKU_WIFI6_B24_5:
            return "Dual Band (2.4 + 5 GHz) WI-FI 6";
        case TIN_SKU_WIFI6_B24_5_BLE:
            return "Dual Band (2.4 + 5 GHz) WI-FI 6 w/ BLE";
        case TIN_SKU_WIFI6_B24:
            return "Single Band (2.4 GHz) WI-FI 6";
        case TIN_SKU_WIFI4_B24:
            return "Single Band (2.4 GHz) WI-FI 4";
		default:
			return "UNKNOWN";
    }
}

uint32_t rm_wifi_otp_sku_id_get(void)
{
    return otp_sku_id;
}

uint32_t rm_wifi_build_sku_id_get(void)
{
    return build_sku_id;
}


