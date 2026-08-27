#include <rtthread.h>

#include "bsp_api.h"
#include "rm_wifi_api.h"
#include "rm_map_persistant_w.h"
#include "wifi_app.h"

extern const map_persistant_w_instance_t g_map_persistant_w;

#define WIFI_START_THREAD_STACK_SIZE  (4096)
#define WIFI_START_THREAD_PRIORITY    (12)

static void wifi_start_thread(void *parameter)
{
    fsp_err_t map_status;
    WIFIReturnCode_t result;

    (void) parameter;
    rt_thread_mdelay(100);

    map_status = g_map_persistant_w.p_api->open(g_map_persistant_w.p_ctrl);
    if ((FSP_SUCCESS != map_status) && (FSP_ERR_ALREADY_OPEN != map_status))
    {
        rt_kprintf("Wi-Fi NVRAM start failed: %d\n", (int) map_status);
        return;
    }

    result = WIFI_On();
    if (eWiFiSuccess == result)
    {
        rt_kprintf("Wi-Fi driver started\n");
    }
    else
    {
        rt_kprintf("Wi-Fi driver start failed: %d\n", (int) result);
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
