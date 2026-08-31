#include <rtthread.h>
#include <string.h>

#include "bsp_api.h"
#include "rm_wifi.h"
#include "rm_wifi_api.h"
#include "wifi_app.h"

#define WIFI_START_THREAD_STACK_SIZE  (4096)
#define WIFI_START_THREAD_PRIORITY    (12)
#define WIFI_SCAN_RESULT_COUNT        (100)

static WIFIScanResult_t g_wifi_scan_results[WIFI_SCAN_RESULT_COUNT];

static const char * wifi_security_name(WIFISecurity_t security)
{
    switch (security)
    {
        case eWiFiSecurityOpen:
            return "Open";
        case eWiFiSecurityWEP:
            return "WEP";
        case eWiFiSecurityWPA:
            return "WPA";
        case eWiFiSecurityWPA2:
            return "WPA2";
        case eWiFiSecurityWPA3:
            return "WPA3";
        default:
            return "Other";
    }
}

static void wifi_print_scan_results(void)
{
    rt_uint32_t i;

    rt_kprintf("Wi-Fi scan results:\n");
    rt_kprintf("No  SSID                             RSSI  CH  Security\n");

    for (i = 0; i < WIFI_SCAN_RESULT_COUNT; i++)
    {
        WIFIScanResult_t *result = &g_wifi_scan_results[i];
        rt_uint32_t ssid_length = result->ucSSIDLength;

        if ((result->cRSSI == 0) && (ssid_length == 0))
        {
            continue;
        }

        if (ssid_length > sizeof(result->ucSSID))
        {
            ssid_length = sizeof(result->ucSSID);
        }

        rt_kprintf("%2u  %.*s  %4d  %2u  %s\n",
                   (unsigned int) (i + 1),
                   (int) ssid_length,
                   result->ucSSID,
                   (int) result->cRSSI,
                   (unsigned int) result->ucChannel,
                   wifi_security_name((WIFISecurity_t) result->xSecurity));
    }
}

static void wifi_start_thread(void *parameter)
{
    WIFIReturnCode_t result;
    fsp_err_t watchdog_status;
    uint8_t mac[wificonfigMAX_BSSID_LEN];

    (void) parameter;
    rt_thread_mdelay(100);

    /* rm_wifi_init() registers tasks with this service during WIFI_On(). */
    watchdog_status = g_wifi_cfg.p_watchdog_service->p_api->open(
        g_wifi_cfg.p_watchdog_service->p_ctrl,
        g_wifi_cfg.p_watchdog_service->p_cfg);
    if ((FSP_SUCCESS != watchdog_status) &&
        (FSP_ERR_ALREADY_OPEN != watchdog_status))
    {
        rt_kprintf("Watchdog service open failed: %d\n", (int) watchdog_status);
        return;
    }

    result = WIFI_On();
    if (eWiFiSuccess != result)
    {
        rt_kprintf("WIFI_On failed: %d\n", (int) result);
        return;
    }

    rt_kprintf("Wi-Fi started\n");

    result = WIFI_GetMAC(mac);
    if (eWiFiSuccess == result)
    {
        rt_kprintf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                   mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
    else
    {
        rt_kprintf("WIFI_GetMAC failed: %d\n", (int) result);
    }

    result = WIFI_SetMode((WIFIDeviceMode_t) 0); /* eWiFiModeStation */
    if (eWiFiSuccess != result)
    {
        rt_kprintf("WIFI_SetMode failed: %d\n", (int) result);
        return;
    }

    result = WIFI_Disconnect();
    if (eWiFiSuccess != result)
    {
        rt_kprintf("WIFI_Disconnect failed: %d\n", (int) result);
    }

    rt_thread_mdelay(100);
    memset(g_wifi_scan_results, 0, sizeof(g_wifi_scan_results));

    result = WIFI_Scan(g_wifi_scan_results, WIFI_SCAN_RESULT_COUNT);
    if (eWiFiSuccess == result)
    {
        wifi_print_scan_results();
    }
    else
    {
        rt_kprintf("WIFI_Scan failed: %d\n", (int) result);
    }

    while (1)
    {
        rt_thread_mdelay(1000);
    }
}

int ra6w1_wifi_app_start(void)
{
    rt_thread_t thread;

    thread = rt_thread_create("wifi_start",
                              wifi_start_thread,
                              RT_NULL,
                              WIFI_START_THREAD_STACK_SIZE,
                              WIFI_START_THREAD_PRIORITY,
                              20);
    if (RT_NULL == thread)
    {
        return -RT_ENOMEM;
    }

    rt_thread_startup(thread);
    return RT_EOK;
}

INIT_APP_EXPORT(ra6w1_wifi_app_start);
