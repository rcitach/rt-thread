/*
 * RT-Thread WLAN adapter for the standalone RA6W1 cfg80211 demo.
 *
 * The vendor ABI and WPA state machine remain in wifi_fc80211_demo.c.  This
 * file only translates RT-Thread WLAN objects and reports completed results.
 */

#include <rtthread.h>
#include <dev_wlan.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "co_int.h"

typedef unsigned long uint32;

#include "supp_types.h"
#include "rwnx_cfg.h"
#include "driver_fc80211.h"

#ifdef RT_USING_WIFI

#define WIFI_RT_WLAN_IF_INDEX  (0U)
#define WIFI_RT_WLAN_SCAN_MAX  (64U)

extern int wifi_fc80211_demo_ensure_init(void);
extern int wifi_fc80211_demo_scan_info(struct rt_wlan_info *infos,
                                       size_t max, size_t *count);
extern int wifi_fc80211_demo_join(const struct rt_sta_info *sta);
extern int wifi_fc80211_demo_get_link_info(struct rt_wlan_info *info);
extern int wifi_fc80211_demo_get_mac(rt_uint8_t mac[6]);
extern int wifi_fc80211_demo_disconnect(void);
extern int wifi_lwip_adapter_netif_start(void);
extern int wifi_lwip_adapter_netif_link_up(void);
extern int wifi_lwip_adapter_netif_link_down(void);
extern int wifi_lwip_adapter_dhcp_start(void);
extern int wifi_lwip_adapter_dhcp_stop(void);

static struct rt_wlan_device s_wifi_wlan;
static rt_bool_t s_wifi_wlan_registered;
static rt_bool_t s_wifi_wlan_initialized;
static int s_wifi_power_save;

static rt_err_t wifi_wlan_init(struct rt_wlan_device *wlan)
{
    (void) wlan;
    if (s_wifi_wlan_initialized)
    {
        return RT_EOK;
    }
    if (wifi_fc80211_demo_ensure_init() != RT_EOK)
    {
        return -RT_ERROR;
    }
    if (wifi_lwip_adapter_netif_start() != RT_EOK)
    {
        return -RT_ERROR;
    }
    s_wifi_wlan_initialized = RT_TRUE;
    return RT_EOK;
}

static rt_err_t wifi_wlan_mode(struct rt_wlan_device *wlan,
                               rt_wlan_mode_t mode)
{
    (void) wlan;
    if (mode == RT_WLAN_STATION)
    {
        return wifi_wlan_init(wlan);
    }
    /* The current standalone port has no AP beacon/crypto implementation. */
    return -RT_ENOSYS;
}

static rt_err_t wifi_wlan_scan(struct rt_wlan_device *wlan,
                               struct rt_scan_info *scan_info)
{
    struct rt_wlan_info *infos;
    struct rt_wlan_buff buff;
    size_t count = 0U;
    size_t index;
    int status;

    infos = (struct rt_wlan_info *) rt_malloc(
        sizeof(*infos) * WIFI_RT_WLAN_SCAN_MAX);
    if (infos == RT_NULL)
    {
        return -RT_ENOMEM;
    }

    status = wifi_wlan_init(wlan);
    if (status != RT_EOK)
    {
        rt_free(infos);
        return status;
    }
    status = wifi_fc80211_demo_scan_info(infos,
                                         WIFI_RT_WLAN_SCAN_MAX, &count);
    if (status != RT_EOK)
    {
        rt_free(infos);
        return status;
    }

    /* rt_wlan_dev_indicate_event_handle() invokes handlers synchronously, so
     * a stack item is valid for each report and does not escape this function. */
    for (index = 0U; index < count; index++)
    {
        rt_bool_t bssid_filter = RT_FALSE;
        rt_bool_t ssid_match = RT_TRUE;
        rt_bool_t channel_match = RT_TRUE;

        if (scan_info != RT_NULL)
        {
            size_t mac_index;
            for (mac_index = 0U; mac_index < 6U; mac_index++)
            {
                if (scan_info->bssid[mac_index] != 0U)
                {
                    bssid_filter = RT_TRUE;
                    break;
                }
            }
            if ((scan_info->ssid.len != 0U) &&
                ((scan_info->ssid.len != infos[index].ssid.len) ||
                 (memcmp(scan_info->ssid.val, infos[index].ssid.val,
                         scan_info->ssid.len) != 0)))
            {
                ssid_match = RT_FALSE;
            }
            if (bssid_filter &&
                (memcmp(scan_info->bssid, infos[index].bssid, 6U) != 0))
            {
                ssid_match = RT_FALSE;
            }
            if ((scan_info->channel_min > 0) &&
                (infos[index].channel < scan_info->channel_min))
            {
                channel_match = RT_FALSE;
            }
            if ((scan_info->channel_max > 0) &&
                (infos[index].channel > scan_info->channel_max))
            {
                channel_match = RT_FALSE;
            }
        }
        if (!ssid_match || !channel_match)
        {
            continue;
        }
        buff.data = &infos[index];
        buff.len = sizeof(infos[index]);
        rt_wlan_dev_indicate_event_handle(wlan,
                                          RT_WLAN_DEV_EVT_SCAN_REPORT,
                                          &buff);
    }
    rt_wlan_dev_indicate_event_handle(wlan, RT_WLAN_DEV_EVT_SCAN_DONE, RT_NULL);
    rt_free(infos);
    return RT_EOK;
}

static rt_err_t wifi_wlan_join(struct rt_wlan_device *wlan,
                               struct rt_sta_info *sta_info)
{
    int status;
    int dhcp_status;

    status = wifi_wlan_init(wlan);
    if (status == RT_EOK)
    {
        status = wifi_fc80211_demo_join(sta_info);
    }
    if (status == RT_EOK)
    {
        (void) wifi_lwip_adapter_netif_link_up();
        dhcp_status = wifi_lwip_adapter_dhcp_start();
        if (dhcp_status != RT_EOK)
        {
            rt_kprintf("[wifi-wlan] DHCP start failed: %d\n", dhcp_status);
        }
        rt_wlan_dev_indicate_event_handle(wlan, RT_WLAN_DEV_EVT_CONNECT,
                                          RT_NULL);
    }
    else
    {
        rt_wlan_dev_indicate_event_handle(wlan,
                                          RT_WLAN_DEV_EVT_CONNECT_FAIL,
                                          RT_NULL);
    }
    return status;
}

static rt_err_t wifi_wlan_disconnect(struct rt_wlan_device *wlan)
{
    int status;

    (void) wlan;
    status = wifi_fc80211_demo_disconnect();
    if (status == RT_EOK)
    {
        (void) wifi_lwip_adapter_dhcp_stop();
        (void) wifi_lwip_adapter_netif_link_down();
        rt_wlan_dev_indicate_event_handle(wlan, RT_WLAN_DEV_EVT_DISCONNECT,
                                          RT_NULL);
    }
    return status;
}

static rt_err_t wifi_wlan_scan_stop(struct rt_wlan_device *wlan)
{
    int status;

    (void) wlan;
    status = fc80211_abort_scan(0UL, WIFI_RT_WLAN_IF_INDEX);
    return (status == 0) ? RT_EOK : status;
}

static int wifi_wlan_get_rssi(struct rt_wlan_device *wlan)
{
    struct rt_wlan_info info;

    (void) wlan;
    if (wifi_fc80211_demo_get_link_info(&info) != RT_EOK)
    {
        return -1;
    }
    return info.rssi;
}

static int wifi_wlan_get_info(struct rt_wlan_device *wlan,
                              struct rt_wlan_info *info)
{
    (void) wlan;
    return wifi_fc80211_demo_get_link_info(info);
}

static rt_err_t wifi_wlan_set_powersave(struct rt_wlan_device *wlan,
                                        int level)
{
    int status;

    (void) wlan;
    status = fc80211_set_power_save(WIFI_RT_WLAN_IF_INDEX, level != 0, 0);
    if (status == 0)
    {
        s_wifi_power_save = (level != 0);
        return RT_EOK;
    }
    return status;
}

static int wifi_wlan_get_powersave(struct rt_wlan_device *wlan)
{
    (void) wlan;
    return s_wifi_power_save;
}

static int wifi_wlan_get_channel(struct rt_wlan_device *wlan)
{
    struct rt_wlan_info info;

    (void) wlan;
    if (wifi_fc80211_demo_get_link_info(&info) != RT_EOK)
    {
        return -1;
    }
    return info.channel;
}

static rt_err_t wifi_wlan_get_mac(struct rt_wlan_device *wlan,
                                  rt_uint8_t mac[6])
{
    (void) wlan;
    return wifi_fc80211_demo_get_mac(mac);
}

/* These operations have no complete and tested equivalent in the current
 * prebuilt RA6W1 driver. Returning ENOSYS is safer than reporting success. */
static rt_err_t wifi_wlan_unsupported_err(struct rt_wlan_device *wlan)
{
    (void) wlan;
    return -RT_ENOSYS;
}

static rt_err_t wifi_wlan_unsupported_ap(struct rt_wlan_device *wlan,
                                         struct rt_ap_info *info)
{
    (void) info;
    return wifi_wlan_unsupported_err(wlan);
}

static rt_err_t wifi_wlan_unsupported_bool(struct rt_wlan_device *wlan,
                                           rt_bool_t enable)
{
    (void) enable;
    return wifi_wlan_unsupported_err(wlan);
}

static rt_err_t wifi_wlan_unsupported_int(struct rt_wlan_device *wlan,
                                          int value)
{
    (void) value;
    return wifi_wlan_unsupported_err(wlan);
}

static rt_country_code_t wifi_wlan_get_country(struct rt_wlan_device *wlan)
{
    (void) wlan;
    return RT_COUNTRY_UNKNOWN;
}

static rt_err_t wifi_wlan_unsupported_ap_deauth(struct rt_wlan_device *wlan,
                                                rt_uint8_t mac[])
{
    (void) mac;
    return wifi_wlan_unsupported_err(wlan);
}

static rt_err_t wifi_wlan_unsupported_filter(struct rt_wlan_device *wlan,
                                              struct rt_wlan_filter *filter)
{
    (void) filter;
    return wifi_wlan_unsupported_err(wlan);
}

static rt_err_t wifi_wlan_unsupported_set_country(struct rt_wlan_device *wlan,
                                                   rt_country_code_t country)
{
    (void) country;
    return wifi_wlan_unsupported_err(wlan);
}

static rt_err_t wifi_wlan_unsupported_set_mac(struct rt_wlan_device *wlan,
                                               rt_uint8_t mac[])
{
    (void) mac;
    return wifi_wlan_unsupported_err(wlan);
}

static int wifi_wlan_unsupported_get_info(struct rt_wlan_device *wlan,
                                          struct rt_wlan_info *info)
{
    (void) info;
    return wifi_wlan_unsupported_err(wlan);
}

static int wifi_wlan_unsupported_data(struct rt_wlan_device *wlan,
                                      void *buff, int len)
{
    (void) buff;
    (void) len;
    return wifi_wlan_unsupported_err(wlan);
}

static int wifi_wlan_unsupported_fast(void *data)
{
    (void) data;
    return -RT_ENOSYS;
}

static rt_err_t wifi_wlan_unsupported_fast_connect(void *data,
                                                    rt_int32_t len)
{
    (void) data;
    (void) len;
    return -RT_ENOSYS;
}

static const struct rt_wlan_dev_ops s_wifi_wlan_ops =
{
    .wlan_init = wifi_wlan_init,
    .wlan_mode = wifi_wlan_mode,
    .wlan_scan = wifi_wlan_scan,
    .wlan_join = wifi_wlan_join,
    .wlan_softap = wifi_wlan_unsupported_ap,
    .wlan_disconnect = wifi_wlan_disconnect,
    .wlan_ap_stop = wifi_wlan_unsupported_err,
    .wlan_ap_deauth = wifi_wlan_unsupported_ap_deauth,
    .wlan_scan_stop = wifi_wlan_scan_stop,
    .wlan_get_rssi = wifi_wlan_get_rssi,
    .wlan_get_info = wifi_wlan_get_info,
    .wlan_ap_get_info = wifi_wlan_unsupported_get_info,
    .wlan_set_powersave = wifi_wlan_set_powersave,
    .wlan_get_powersave = wifi_wlan_get_powersave,
    .wlan_cfg_promisc = wifi_wlan_unsupported_bool,
    .wlan_cfg_filter = wifi_wlan_unsupported_filter,
    .wlan_cfg_mgnt_filter = wifi_wlan_unsupported_bool,
    .wlan_set_channel = wifi_wlan_unsupported_int,
    .wlan_get_channel = wifi_wlan_get_channel,
    .wlan_set_country = wifi_wlan_unsupported_set_country,
    .wlan_get_country = wifi_wlan_get_country,
    .wlan_set_mac = wifi_wlan_unsupported_set_mac,
    .wlan_get_mac = wifi_wlan_get_mac,
    .wlan_recv = wifi_wlan_unsupported_data,
    .wlan_send = wifi_wlan_unsupported_data,
    .wlan_send_raw_frame = wifi_wlan_unsupported_data,
    .wlan_get_fast_info = wifi_wlan_unsupported_fast,
    .wlan_fast_connect = wifi_wlan_unsupported_fast_connect,
};

static void wifi_wlan_test_event(struct rt_wlan_device *wlan,
                                 rt_wlan_dev_event_t event,
                                 struct rt_wlan_buff *buff, void *parameter)
{
    (void) wlan;
    (void) parameter;
    if (event == RT_WLAN_DEV_EVT_SCAN_REPORT &&
        (buff == NULL || buff->data == NULL ||
         buff->len != sizeof(struct rt_wlan_info)))
    {
        return;
    }
    if (event == RT_WLAN_DEV_EVT_SCAN_REPORT)
    {
        const struct rt_wlan_info *info = buff->data;
        rt_kprintf("[wifi-wlan] scan ssid=%.*s bssid=%02X:%02X:%02X:%02X:%02X:%02X "
                   "ch=%d rssi=%d security=0x%08x\n",
                   info->ssid.len, info->ssid.val,
                   info->bssid[0], info->bssid[1], info->bssid[2],
                   info->bssid[3], info->bssid[4], info->bssid[5],
                   info->channel, info->rssi, (unsigned int) info->security);
    }
    else
    {
        rt_kprintf("[wifi-wlan] event=%d\n", (int) event);
    }
}

static int wifi_wlan_register(void)
{
    rt_err_t status;

    if (s_wifi_wlan_registered)
    {
        return RT_EOK;
    }
    status = rt_wlan_dev_register(&s_wifi_wlan, "wlan0", &s_wifi_wlan_ops,
                                  RT_WLAN_FLAG_STA_ONLY, NULL);
    if (status == RT_EOK)
    {
        s_wifi_wlan_registered = RT_TRUE;
        rt_wlan_dev_register_event_handler(&s_wifi_wlan,
                                           RT_WLAN_DEV_EVT_SCAN_REPORT,
                                           wifi_wlan_test_event, NULL);
        rt_wlan_dev_register_event_handler(&s_wifi_wlan,
                                           RT_WLAN_DEV_EVT_SCAN_DONE,
                                           wifi_wlan_test_event, NULL);
        rt_wlan_dev_register_event_handler(&s_wifi_wlan,
                                           RT_WLAN_DEV_EVT_CONNECT,
                                           wifi_wlan_test_event, NULL);
        rt_wlan_dev_register_event_handler(&s_wifi_wlan,
                                           RT_WLAN_DEV_EVT_CONNECT_FAIL,
                                           wifi_wlan_test_event, NULL);
        rt_wlan_dev_register_event_handler(&s_wifi_wlan,
                                           RT_WLAN_DEV_EVT_DISCONNECT,
                                           wifi_wlan_test_event, NULL);
    }
    return status;
}
INIT_APP_EXPORT(wifi_wlan_register);

static int wifi_wlan_test(int argc, char **argv)
{
    struct rt_wlan_info info;
    struct rt_sta_info sta;
    int status;

    if (argc < 2)
    {
        rt_kprintf("usage: wifi_wlan_test init|scan|connect|open|status|disconnect|mac|ps on|ps off\n");
        return -RT_EINVAL;
    }
    status = wifi_wlan_register();
    if (status != RT_EOK)
    {
        return status;
    }
    if (strcmp(argv[1], "init") == 0)
    {
        return rt_wlan_dev_init(&s_wifi_wlan, RT_WLAN_STATION);
    }

    /* rt_wlan_dev_register() only registers the device.  The WLAN device
     * mutex is initialized by _rt_wlan_dev_init(), so every command that
     * enters rt_device_control() must pass through device initialization. */
    status = rt_wlan_dev_init(&s_wifi_wlan, RT_WLAN_STATION);
    if (status != RT_EOK)
    {
        return status;
    }

    if (strcmp(argv[1], "scan") == 0)
    {
        return rt_wlan_dev_scan(&s_wifi_wlan, NULL);
    }
    if ((strcmp(argv[1], "connect") == 0) ||
        (strcmp(argv[1], "open") == 0))
    {
        if (((strcmp(argv[1], "connect") == 0) && (argc < 4)) ||
            ((strcmp(argv[1], "open") == 0) && (argc < 3)))
        {
            rt_kprintf("usage: wifi_wlan_test connect <ssid> <password> [channel]\n");
            return -RT_EINVAL;
        }
        memset(&sta, 0, sizeof(sta));
        sta.ssid.len = (rt_uint8_t) strlen(argv[2]);
        if (sta.ssid.len > RT_WLAN_SSID_MAX_LENGTH)
        {
            return -RT_EINVAL;
        }
        memcpy(sta.ssid.val, argv[2], sta.ssid.len);
        sta.security = (strcmp(argv[1], "open") == 0) ?
                       SECURITY_OPEN : SECURITY_WPA2_AES_PSK;
        if (sta.security != SECURITY_OPEN)
        {
            sta.key.len = (rt_uint8_t) strlen(argv[3]);
            if (sta.key.len > RT_WLAN_PASSWORD_MAX_LENGTH)
            {
                return -RT_EINVAL;
            }
            memcpy(sta.key.val, argv[3], sta.key.len);
        }
        if (((sta.security == SECURITY_OPEN) && (argc >= 4)) ||
            ((sta.security != SECURITY_OPEN) && (argc >= 5)))
        {
            sta.channel = (rt_uint16_t) strtoul(
                argv[sta.security == SECURITY_OPEN ? 3 : 4], NULL, 10);
        }
        return rt_device_control((rt_device_t) &s_wifi_wlan,
                                 RT_WLAN_CMD_JOIN, &sta);
    }
    if (strcmp(argv[1], "status") == 0)
    {
        memset(&info, 0, sizeof(info));
        status = rt_wlan_dev_get_info(&s_wifi_wlan, &info);
        rt_kprintf("[wifi-wlan] status=%d ssid=%.*s channel=%d rssi=%d\n",
                   status, info.ssid.len, info.ssid.val, info.channel,
                   info.rssi);
        return status;
    }
    if (strcmp(argv[1], "disconnect") == 0)
    {
        return rt_wlan_dev_disconnect(&s_wifi_wlan);
    }
    if (strcmp(argv[1], "mac") == 0)
    {
        rt_uint8_t mac[6];
        memset(mac, 0, sizeof(mac));
        status = rt_wlan_dev_get_mac(&s_wifi_wlan, mac);
        rt_kprintf("[wifi-wlan] mac=%02X:%02X:%02X:%02X:%02X:%02X\n",
                   mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        return status;
    }
    if ((strcmp(argv[1], "ps") == 0) && (argc >= 3))
    {
        return rt_wlan_dev_set_powersave(&s_wifi_wlan,
                                         strcmp(argv[2], "on") == 0);
    }
    return -RT_EINVAL;
}
MSH_CMD_EXPORT(wifi_wlan_test, test RT-Thread WLAN adapter callbacks);

#else

/* Keep a discoverable command when the WLAN framework is disabled in rtconfig.h. */
static int wifi_wlan_test(int argc, char **argv)
{
    (void) argc;
    (void) argv;
    rt_kprintf("[wifi-wlan] RT_USING_WIFI is disabled; enable it in rtconfig.h\n");
    return -RT_ENOSYS;
}
MSH_CMD_EXPORT(wifi_wlan_test, test RT-Thread WLAN adapter callbacks);

#endif
