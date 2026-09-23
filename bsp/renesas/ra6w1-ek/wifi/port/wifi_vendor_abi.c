/* Closed Wi-Fi archive ABI for the RT-Thread WLAN backend. */
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <rtthread.h>
#include <dev_wlan.h>
#include <lwip/ip_addr.h>
#include <netdev.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "event_groups.h"
#include "wifi_vendor.h"
#include "wifi_wlan_port.h"

#define DBG_TAG "wifi.abi"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#define WLAN1_IFACE 1U
extern bool bsp_tcs_otp_read_mac(uint32_t *mac) __attribute__((weak));
QueueHandle_t TO_SUPP_QUEUE;
QueueHandle_t TO_CLI_QUEUE;

/* Resolve the legacy supplicant object's dependencies here, so supp_eloop.o
 * is not pulled from the archive alongside our native eloop timeout API.
 * The CLI/P2P event consumers are not used by the WLAN backend. */
EventGroupHandle_t ra6w1_sp_event_group;
QueueHandle_t wifi_monitor_event_q;
#ifdef CONFIG_SCAN_REPLY_OPTIMIZE
char *cli_rx_ev_data;
#else
ra6wx_cli_rx_ev_data_t *cli_rx_ev_data;
#endif
static rt_bool_t eloop_stopped;

/* Only the ABI handle is a wrapper type; allocation and IPC use RT-Thread.
 * Dynamic descriptors also remain compatible with the wrapper's delete API. */
static QueueHandle_t vendor_queue_create(const char *name, rt_size_t depth)
{
    StaticQueue_t *descriptor = rt_calloc(1, sizeof(*descriptor));
    if (!descriptor)
        return RT_NULL;
    descriptor->rt_ipc = (struct rt_ipc_object *)rt_mq_create(name,
                              sizeof(ULONG) * 2, depth, RT_IPC_FLAG_PRIO);
    if (!descriptor->rt_ipc)
    {
        rt_free(descriptor);
        return RT_NULL;
    }
    return (QueueHandle_t)descriptor;
}

static void vendor_queue_delete(QueueHandle_t queue)
{
    StaticQueue_t *descriptor = (StaticQueue_t *)queue;
    if (descriptor)
    {
        rt_mq_delete((rt_mq_t)descriptor->rt_ipc);
        rt_free(descriptor);
    }
}

rt_mq_t wifi_vendor_event_queue_init(void)
{
    QueueHandle_t supp_queue, cli_queue;
    if (TO_SUPP_QUEUE)
        return (rt_mq_t)((StaticQueue_t *)TO_SUPP_QUEUE)->rt_ipc;

    supp_queue = vendor_queue_create("wpaevt", TO_SUPP_QUEUE_SIZE);
    if (!supp_queue)
        return RT_NULL;
    cli_queue = vendor_queue_create("wpacli", TO_CLI_QUEUE_SIZE);
    if (!cli_queue)
    {
        vendor_queue_delete(supp_queue);
        return RT_NULL;
    }
    TO_SUPP_QUEUE = supp_queue;
    TO_CLI_QUEUE = cli_queue;
    eloop_stopped = RT_FALSE;
    return (rt_mq_t)((StaticQueue_t *)supp_queue)->rt_ipc;
}

void wifi_vendor_event_queue_deinit(void)
{
    /* Startup rollback only: vendor producers and wifievt must be stopped. */
    vendor_queue_delete(TO_SUPP_QUEUE);
    vendor_queue_delete(TO_CLI_QUEUE);
    TO_SUPP_QUEUE = RT_NULL;
    TO_CLI_QUEUE = RT_NULL;
    eloop_stopped = RT_TRUE;
}

int ra6wx_eloop_init(void)
{
    return wifi_vendor_event_queue_init() ? 0 : -1;
}

void ra6wx_eloop_run(struct wpa_global *global, struct wpa_supplicant *wpa_s)
{
    (void)global;
    (void)wpa_s;
    /* WLAN owns the event worker; running the legacy loop would steal events. */
    LOG_E("standalone supplicant loop is unsupported; use WLAN management");
}

void eloop_terminate(void)
{
    eloop_stopped = RT_TRUE;
}

int eloop_terminated(void)
{
    return eloop_stopped;
}

#ifdef ipaddr_aton
#undef ipaddr_aton
#endif
int ipaddr_aton(const char *cp, ip4_addr_t *addr)
{
    return ip4addr_aton(cp, addr);
}

void rm_wifi_event_signal_connected(uint8_t *ssid, uint8_t ssid_len,
                                    uint8_t *bssid, int security,
                                    uint8_t chan_info)
{
    (void) ssid;
    (void) ssid_len;
    (void) bssid;
    (void) security;
    (void) chan_info;
}

void rm_wifi_event_signal_disconnected(uint16_t reason)
{
    (void) reason;
}

void rm_wifi_event_signal_ap_sta_disconnected(uint16_t reason,
                                              uint8_t *ucMac)
{
    (void) reason;
    (void) ucMac;
}

void rm_wifi_event_signal_twt(int twt_event)
{
    (void) twt_event;
}

int rm_wifi_get_int_val_from_str(char *param, int *int_val, int policy)
{
    char *end;
    long value;

    (void) policy;
    if ((param == NULL) || (int_val == NULL))
    {
        return -1;
    }

    value = strtol(param, &end, 0);
    if ((end == param) || (*end != '\0') ||
        (value < INT32_MIN) || (value > INT32_MAX))
    {
        return -1;
    }

    *int_val = (int) value;
    return 0;
}

int rm_wifi_helper_security_type_get(unsigned int key_mgmt,
                                     unsigned int proto,
                                     unsigned int pairwise_cipher)
{
    (void) key_mgmt;
    (void) proto;
    (void) pairwise_cipher;
    return 6;
}

uint32_t rm_wifi_build_sku_id_get(void)
{
    return 0U;
}

int rm_wifi_ps_mode_get(bool *config)
{
    if (config != NULL)
    {
        *config = false;
    }

    return 6;
}

int rm_wifi_is_wpa_state(uint8_t state, int max_delay_counter)
{
    (void) state;
    (void) max_delay_counter;
    return 6;
}

/*
 * The closed driver asks for the MAC in a split 16/32-bit representation
 * during startup.  Keep the board's historical default when the persistent
 * RM helper is not linked; WLAN1 receives the next address.
 */
int getMacAddrMswLsw(unsigned int iface, unsigned long *macmsw,
                    unsigned long *maclsw)
{
    unsigned long default_msw = 0xEC9FUL;
    unsigned long default_lsw = 0x0D9FFFFEUL;
    uint32_t otp_mac[2];

    if ((macmsw == NULL) || (maclsw == NULL))
    {
        return -1;
    }

    if ((bsp_tcs_otp_read_mac != NULL) && bsp_tcs_otp_read_mac(otp_mac))
    {
        default_msw = otp_mac[1];
        default_lsw = otp_mac[0];
    }

    if (iface == WLAN1_IFACE)
    {
        default_lsw++;
    }

    *macmsw = default_msw;
    *maclsw = default_lsw;
    return 0;
}

/* DPM is intentionally outside the WLAN adapter. */
int ra6w1_get_fast_connection_mode(void)
{
    return 0;
}

/* Resolve by the WLAN framework's netdev, never by an assumed netif index. */
unsigned int get_ip_info(int iface, int field, char *result)
{
    struct netdev *netdev = wifi_wlan_device()->netdev;
    if (!result)
        return 0;
    result[0] = '\0';
    if (iface != 0 || !netdev)
        return 0;
    switch (field)
    {
    case 0:
        rt_snprintf(result, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
                    netdev->hwaddr[0], netdev->hwaddr[1], netdev->hwaddr[2],
                    netdev->hwaddr[3], netdev->hwaddr[4], netdev->hwaddr[5]);
        break;
    case 1: ipaddr_ntoa_r(&netdev->ip_addr, result, 18); break;
    case 2: ipaddr_ntoa_r(&netdev->netmask, result, 18); break;
    case 3: ipaddr_ntoa_r(&netdev->gw, result, 18); break;
    case 4: rt_snprintf(result, 18, "%u", netdev->mtu); break;
    case 5: ipaddr_ntoa_r(&netdev->dns_servers[0], result, 18); break;
    case 6: ipaddr_ntoa_r(&netdev->dns_servers[1], result, 18); break;
    default: return 0;
    }
    return 1;
}

/* The SDK sometimes toggles its legacy netif from cfg80211 helpers. WLAN's
 * connect/disconnect indications are the sole link-state authority here. */
void wifi_netif_control(int interface, int flag)
{
    (void)interface;
    (void)flag;
}

/* Required companions of wifi_netif_control in the vendor rs_util.o. Provide
 * these locally as well, otherwise extracting that object duplicates the
 * network-control symbol and brings back the legacy netif index assumptions. */
struct wifi_vendor_debug_info
{
    uint16_t mask;
    uint8_t level;
    uint8_t reserved;
} um_dbg_info;
_Static_assert(sizeof(um_dbg_info) == 4, "vendor debug ABI mismatch");

void umac_set_debug(uint8_t level, uint16_t mask, int print)
{
    um_dbg_info.mask = mask;
    um_dbg_info.level = level;
    if (print)
        LOG_I("driver debug level=%u mask=0x%04x", level, mask);
}

int umac_dpm_check_init(void)
{
    /* This backend boots normally and does not support vendor DPM resume. */
    return 0;
}

int ieee80211_frequency_to_channel(int frequency)
{
    int base, spacing = 5;
    if (frequency <= 0)
        return 0;
    if (frequency == 2484)
        return 14;
    if (frequency < 2484)
        base = 2407;
    else if (frequency >= 4910 && frequency <= 4980)
        base = 4000;
    else if (frequency < 5925)
        base = 5000;
    else if (frequency == 5935)
        return 2;
    else if (frequency <= 45000)
        base = 5950;
    else if (frequency >= 58320 && frequency <= 70200)
    {
        base = 56160;
        spacing = 2160;
    }
    else
        return 0;
    return frequency >= base && (frequency - base) % spacing == 0 ?
           (frequency - base) / spacing : 0;
}
