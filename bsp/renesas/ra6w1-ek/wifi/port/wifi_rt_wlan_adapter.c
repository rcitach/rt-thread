/* RA6W1 station driver for RT-Thread WLAN. The WLAN protocol owns lwIP/DHCP. */
#include <rtthread.h>
#include <dev_wlan.h>
#include <dev_wlan_mgnt.h>
#include <dev_wlan_prot.h>
#include <lwip/pbuf.h>
#include <string.h>
#include <stddef.h>
#include "wifi_vendor.h"
#include "wifi_hw_prepare.h"
#include "wifi_crypto_port.h"
#include "wifi_security_manager.h"
#include "wifi_wlan_port.h"
#include "romac4rtos.h"
#include "wpa_common.h"
#include "sys_feature.h"

#define DBG_TAG "wifi.wlan"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#if !defined(RT_WLAN_PROT_LWIP_ENABLE) || !defined(RT_WLAN_PROT_LWIP_PBUF_FORCE)
#error "RA6W1 requires WLAN lwIP protocol with PBUF_FORCE"
#endif
#if PBUF_LINK_ENCAPSULATION_HLEN < 456 || PBUF_PAYLOAD_MARGIN_LEN < 36
#error "RA6W1 firmware requires TX descriptor headroom and tailroom"
#endif

/* SDK channel flag bit 0 is disabled (see cfg80211 channel ABI). */
#define WIFI_CHANNEL_DISABLED 1U
#define WIFI_WDEV_ID       0UL
#define WIFI_IFINDEX       0
#define WIFI_AP_IFINDEX    1
#define WIFI_MAC_IFACE     0U
#define WIFI_AP_MAC_IFACE  1U
#define WIFI_SCAN_LIMIT    64
#define WIFI_CHANNEL_LIMIT 42
#define WIFI_OPERATION_MS  8000
#define WIFI_EVENT_STACK   4096
#define WIFI_EVENT_PRIO    14
#define WIFI_DRIVER_TASK   17U
#define WIFI_LWIP_TASK     20U
#define WIFI_TX_EVENT      2U

extern int rwnx_mac_task_initiailize(void);
extern int rwnx_driver_task_initiailize(void);
extern int rwnx_hw_get_status(void);
extern void rwnx_cfg80211_initialize(void);
extern void fc80211_enable_drv_event(void);
extern void fc80211_update_channel_info(void);
extern void supplicant_done(void);
extern struct wiphy rwnx_wiphy;
extern QueueHandle_t CFG80211_semaphore;
extern int rwnx_free_txq_count(void);
extern int rwnx_empty_txq_item(void);
extern unsigned long rwnx_driver_task_send_event(unsigned long src, unsigned long dst,
                                                unsigned long event, void *data);
extern int pbkdf2_sha1(const char *passphrase, const u8 *ssid, size_t ssid_len,
                      int iterations, u8 *buf, size_t buflen);

/* These structures are passed into the prebuilt SDK. Check the actual ARM ABI. */
_Static_assert(sizeof(struct pbuf) == 16 && offsetof(struct pbuf, if_idx) == 15,
               "vendor pbuf ABI mismatch (descriptor starts at pbuf + 16)");
_Static_assert(sizeof(unsigned long) == 4, "vendor event ABI requires 32-bit words");
_Static_assert(offsetof(struct wpa_scan_res, ie) == 84, "vendor scan ABI mismatch");

enum wifi_link_state { WIFI_IDLE, WIFI_JOINING, WIFI_LINKED, WIFI_STOPPING };
struct wifi_scan_entry
{
    struct rt_wlan_info info;
    u8 rsn[257];
    size_t rsn_len;
};

static struct
{
    struct rt_wlan_device device;
    struct rt_wlan_device ap_device;
    struct rt_mutex control;
    rt_mq_t queue;
    rt_thread_t worker;
    rt_thread_t init_worker;
    struct fc80211_global *global;
    struct i802_bss *bss;
    struct wifi_security_manager security;
    struct wpa_driver_associate_params assoc;
    struct rt_sta_info target;
    struct rt_ap_info ap_info;
    struct rt_scan_info scan_filter;
    struct wifi_scan_entry *scan_cache;
    size_t scan_count;
    int frequencies[WIFI_CHANNEL_LIMIT + 1];
    u8 ap_rsn[257];
    size_t ap_rsn_len;
    u8 pmk[32];
    u8 ap_pmk[32];
    u8 ap_beacon_head[36];
    u8 ap_beacon_tail[256];
    size_t ap_beacon_tail_len;
    enum wifi_link_state state;
    rt_uint32_t notifications;
    rt_uint16_t deauth_reason;
    int init_result;
    int powersave;
    rt_bool_t scan_busy;
    rt_bool_t scan_report;
    rt_bool_t enabled;
    rt_bool_t ap_enabled;
    rt_bool_t ap_active;
    rt_bool_t init_attempted;
    rt_bool_t init_scheduled;
    rt_bool_t ready;
} wifi;
static struct wifi_wlan_stats stats;

struct rt_wlan_device *wifi_wlan_device(void)
{
    return &wifi.device;
}

void wifi_wlan_stats_reset(void)
{
    rt_base_t level = rt_hw_interrupt_disable();
    memset(&stats, 0, sizeof(stats));
    stats.min_free_txq = 32;
    rt_hw_interrupt_enable(level);
}

void wifi_wlan_stats_get(struct wifi_wlan_stats *out)
{
    rt_base_t level = rt_hw_interrupt_disable();
    if (out)
        *out = stats;
    rt_hw_interrupt_enable(level);
}

static void notify(rt_wlan_dev_event_t event)
{
    if ((unsigned int)event < sizeof(wifi.notifications) * 8U)
        wifi.notifications |= 1UL << event;
}

static const u8 *find_ie(const u8 *ies, size_t length, u8 id, size_t *found_len)
{
    *found_len = 0;
    while (ies && length >= 2)
    {
        size_t size = (size_t)ies[1] + 2;
        if (size > length)
            break;
        if (ies[0] == id)
        {
            *found_len = size;
            return ies;
        }
        ies += size;
        length -= size;
    }
    return RT_NULL;
}

static int freq_channel(int frequency)
{
    if (frequency == 2484)
        return 14;
    if (frequency >= 2412 && frequency <= 2472)
        return (frequency - 2407) / 5;
    if (frequency >= 5000 && frequency < 5900)
        return (frequency - 5000) / 5;
    return 0;
}

static int channel_freq(int channel)
{
    if (channel == 14)
        return 2484;
    return channel <= 13 ? 2407 + channel * 5 : 5000 + channel * 5;
}

static int zero_mac(const u8 *mac)
{
    static const u8 zero[6];
    return memcmp(mac, zero, sizeof(zero)) == 0;
}

static rt_err_t get_mac_address(unsigned int iface, rt_uint8_t mac[6])
{
    const UCHAR *address = RT_NULL;
    unsigned long macmsw;
    unsigned long maclsw;

    if (!mac)
        return -RT_EINVAL;
    /* The protocol may query the address before the deferred vendor init has
     * created bss. Fall back to the same OTP/default source in that case. */
    if (wifi.bss)
        address = wpa_driver_fc80211_get_macaddr(wifi.bss);
    if (address && !zero_mac(address))
    {
        memcpy(mac, address, 6);
        return RT_EOK;
    }
    if (getMacAddrMswLsw(iface, &macmsw, &maclsw) < 0 ||
        ((macmsw & 0xffffUL) == 0UL && maclsw == 0UL))
        return -RT_ERROR;
    mac[0] = (rt_uint8_t)(macmsw >> 8);
    mac[1] = (rt_uint8_t)macmsw;
    mac[2] = (rt_uint8_t)(maclsw >> 24);
    mac[3] = (rt_uint8_t)(maclsw >> 16);
    mac[4] = (rt_uint8_t)(maclsw >> 8);
    mac[5] = (rt_uint8_t)maclsw;
    return RT_EOK;
}

static rt_bool_t is_dhcp_frame(const struct pbuf *p)
{
    u8 header[38];
    if (!p || p->tot_len < sizeof(header) ||
        pbuf_copy_partial((struct pbuf *)p, header, sizeof(header), 0) != sizeof(header))
        return RT_FALSE;
    return header[12] == 0x08 && header[13] == 0x00 &&
           header[23] == 17 &&
           ((header[34] == 0 && header[35] == 67 && header[36] == 0 && header[37] == 68) ||
            (header[34] == 0 && header[35] == 68 && header[36] == 0 && header[37] == 67));
}

static void operation_timeout(void *ctx, void *user)
{
    (void)ctx;
    if (user)
    {
        /* Do not accept another scan until the abort completion drains. */
        if (wifi.scan_busy)
        {
            LOG_W("scan timed out; waiting for abort completion");
            (void)fc80211_abort_scan(WIFI_WDEV_ID, WIFI_IFINDEX);
        }
    }
    else if (wifi.state == WIFI_JOINING)
    {
        LOG_W("association/key exchange timed out");
        notify(RT_WLAN_DEV_EVT_CONNECT_FAIL);
        wifi.deauth_reason = 3;
    }
}

static void security_state(void *ctx, enum wpa_states state)
{
    struct fc80211_sta_flag_update flags = {0};
    struct hostapd_sta_add_params station = {0};
    (void)ctx;
    if (state != WPA_COMPLETED || wifi.state != WIFI_JOINING)
        return;
    /* Only authorize firmware here. Link and DHCP belong to WLAN, not the
     * vendor set_supp_port helper (which also alters its legacy netif). */
    flags.mask = flags.set = 1U << FC80211_STA_FLAG_AUTHORIZED;
    station.addr = wifi.bss->drv->bssid;
    if (rsdev_cfg80211_set_station(wifi.bss->drv, &station, &flags) != 0)
    {
        wifi.deauth_reason = 3;
        return;
    }
    eloop_cancel_timeout(operation_timeout, &wifi, RT_NULL);
    wifi.state = WIFI_LINKED;
    notify(RT_WLAN_DEV_EVT_CONNECT);
}

static void security_deauth(void *ctx, u16 reason)
{
    (void)ctx;
    /* A WPA callback must not destroy the WPA context currently on its stack. */
    wifi.deauth_reason = reason ? reason : 3;
}

static void security_reconnect(void *ctx)
{
    security_deauth(ctx, 3);
}

static void disconnect_cleanup(void)
{
    enum wifi_link_state previous = wifi.state;
    eloop_cancel_timeout(operation_timeout, &wifi, RT_NULL);
    wifi_security_manager_deinit(&wifi.security);
    memset(wifi.pmk, 0, sizeof(wifi.pmk));
    memset(&wifi.target.key, 0, sizeof(wifi.target.key));
    wifi.bss->drv->associated = 0;
    memset(wifi.bss->drv->bssid, 0, 6);
    wifi.state = WIFI_IDLE;
    wifi.deauth_reason = 0;
    if (previous == WIFI_JOINING)
        notify(RT_WLAN_DEV_EVT_CONNECT_FAIL);
    else if (previous != WIFI_IDLE)
        notify(RT_WLAN_DEV_EVT_DISCONNECT);
}

static void scan_complete(rt_bool_t aborted)
{
    struct wpa_scan_results *results = RT_NULL;
    struct wifi_scan_entry *cache = RT_NULL;
    size_t i, count = 0;
    if (!wifi.scan_busy)
        return;
    eloop_cancel_timeout(operation_timeout, &wifi, (void *)1);
    if (!aborted)
        results = fc80211_get_scan_results(wifi.bss->drv);
    if (results && results->num)
        cache = rt_calloc(results->num < WIFI_SCAN_LIMIT ? results->num : WIFI_SCAN_LIMIT,
                          sizeof(*cache));
    if (results && results->num && !cache)
        LOG_E("cannot allocate scan results");
    if (results && cache)
    {
        for (i = 0; i < results->num && count < WIFI_SCAN_LIMIT; i++)
        {
            const struct wpa_scan_res *r = results->res[i];
            struct wifi_scan_entry *entry = &cache[count];
            struct wpa_ie_data parsed;
            const u8 *ssid, *rsn;
            size_t ssid_len, rsn_len;
            if (!r)
                continue;
            ssid = find_ie(r->ie, r->ie_len, 0, &ssid_len);
            if (!ssid)
                ssid = find_ie(r->beacon_ie, r->beacon_ie_len, 0, &ssid_len);
            if (!ssid || ssid_len < 2 || ssid_len > 34)
                continue;
            entry->info.ssid.len = ssid_len - 2;
            memcpy(entry->info.ssid.val, ssid + 2, ssid_len - 2);
            memcpy(entry->info.bssid, r->bssid, 6);
            entry->info.channel = freq_channel(r->freq);
            entry->info.band = r->freq < 3000 ? RT_802_11_BAND_2_4GHZ : RT_802_11_BAND_5GHZ;
            entry->info.rssi = r->level;
            entry->info.security = (r->caps & 0x10) ? SECURITY_UNKNOWN : SECURITY_OPEN;
            rsn = find_ie(r->ie, r->ie_len, 48, &rsn_len);
            if (!rsn)
                rsn = find_ie(r->beacon_ie, r->beacon_ie_len, 48, &rsn_len);
            if (rsn)
            {
                memcpy(entry->rsn, rsn, rsn_len);
                entry->rsn_len = rsn_len;
                /* Host security supports WPA2-PSK/CCMP; reject SAE-only,
                 * mandatory-PMF, TKIP-group and enterprise networks. MFPR is
                 * bit 6 in the RSN capabilities field. */
                if (wpa_parse_wpa_ie_rsn(rsn, rsn_len, &parsed) == 0 &&
                    (parsed.key_mgmt & WPA_KEY_MGMT_PSK) &&
                    (parsed.pairwise_cipher & WPA_CIPHER_CCMP) &&
                    parsed.group_cipher == WPA_CIPHER_CCMP &&
                    /* MFPR (bit 6) requires protected management frames;
                     * MFPC (bit 7) only advertises optional support and is
                     * valid for this WPA2-CCMP station. */
                    !(parsed.capabilities & WPA_CAPABILITY_MFPR))
                    entry->info.security = SECURITY_WPA2_AES_PSK;
            }
            count++;
        }
    }
    if (results)
    {
        /* Entries borrow UMAC IE storage. Free only wrappers here; retain
         * the firmware BSS cache until the next scan so association can find
         * the selected BSSID. The snapshot itself owns copies of needed IEs. */
        for (i = 0; i < results->num; i++)
            rt_free(results->res[i]);
        rt_free(results->res);
        rt_free(results);
    }
    rt_free(wifi.scan_cache);
    wifi.scan_cache = cache;
    wifi.scan_count = count;
    wifi.scan_report = RT_TRUE;
}

static void connected_event(const ra6wx_drv_ev_data_t *data)
{
    const struct cfg80211_connect_resp_params *response;
    const u8 *ies, *rsn;
    size_t header = offsetof(struct cfg80211_connect_resp_params, payload);
    size_t rsn_len, available;
    if (wifi.state != WIFI_JOINING)
        return;
    if (!data || data->data_len < (int)header)
    {
        wifi.deauth_reason = 3;
        return;
    }
    response = (const void *)data->data;
    if (response->status != 0)
    {
        LOG_W("association failed: status=%d", response->status);
        disconnect_cleanup();
        return;
    }
    available = data->data_len - header;
    if (response->req_ie_len > available ||
        response->resp_ie_len > available - response->req_ie_len ||
        (!zero_mac(wifi.target.bssid) && memcmp(wifi.target.bssid, response->bssid, 6)))
    {
        wifi.deauth_reason = 3;
        return;
    }
    wifi.bss->drv->associated = 1;
    memcpy(wifi.bss->drv->bssid, response->bssid, 6);
    memcpy(wifi.target.bssid, response->bssid, 6);
    memcpy(wifi.bss->drv->ssid, wifi.target.ssid.val, wifi.target.ssid.len);
    wifi.bss->drv->ssid_len = wifi.target.ssid.len;
    wifi.bss->drv->assoc_freq = wifi.assoc.freq.freq;
    if (wifi.target.security == SECURITY_OPEN)
    {
        eloop_cancel_timeout(operation_timeout, &wifi, RT_NULL);
        wifi.state = WIFI_LINKED;
        notify(RT_WLAN_DEV_EVT_CONNECT);
        return;
    }
    /* The archive serializes IEs after the response; its pointer members are
     * not valid across the message queue and must never be dereferenced. */
    ies = (const u8 *)response->payload;
    rsn = find_ie(ies, response->req_ie_len, 48, &rsn_len);
    if (rsn && wifi_security_manager_set_assoc_ie(&wifi.security, rsn, rsn_len) != 0)
    {
        wifi.deauth_reason = 3;
        return;
    }
    rsn = find_ie(ies + response->req_ie_len, response->resp_ie_len, 48, &rsn_len);
    if (!rsn)
    {
        rsn = wifi.ap_rsn;
        rsn_len = wifi.ap_rsn_len;
    }
    if (!rsn_len || wifi_security_manager_notify_assoc(&wifi.security,
                    response->bssid, rsn, rsn_len) != RT_EOK)
        wifi.deauth_reason = 3;
}

static void handle_driver_event(ra6wx_drv_msg_buf_t *message)
{
    if (!message)
        return;
    switch (message->cmd)
    {
    case FC80211_CMD_NEW_SCAN_RESULTS: scan_complete(RT_FALSE); break;
    case FC80211_CMD_SCAN_ABORTED: scan_complete(RT_TRUE); break;
    case FC80211_CMD_CONNECT: connected_event(message->data); break;
    case FC80211_CMD_DISCONNECT:
        if (wifi.bss)
            disconnect_cleanup();
        break;
    default: break;
    }
    rt_free(message->data);
    rt_free(message);
}

static void handle_eapol(struct pbuf *p)
{
    u8 *frame, *copy = RT_NULL;
    if (!p)
        return;
    if (p->tot_len < 18 || p->tot_len > 2304 || !wifi.security.sm ||
        (wifi.state != WIFI_JOINING && wifi.state != WIFI_LINKED))
        goto out;
    frame = p->payload;
    if (p->len != p->tot_len)
    {
        copy = rt_malloc(p->tot_len);
        if (!copy || pbuf_copy_partial(p, copy, p->tot_len, 0) != p->tot_len)
            goto out;
        frame = copy;
    }
    if (frame[12] == 0x88 && frame[13] == 0x8e &&
        memcmp(frame + 6, wifi.target.bssid, 6) == 0)
        wifi_security_manager_rx_eapol(&wifi.security, frame + 6, frame + 14, p->tot_len - 14);
out:
    rt_free(copy);
    pbuf_free(p);
}

static int scan_matches(const struct rt_wlan_info *info)
{
    const struct rt_scan_info *f = &wifi.scan_filter;
    return (!f->ssid.len || (f->ssid.len == info->ssid.len &&
            !memcmp(f->ssid.val, info->ssid.val, f->ssid.len))) &&
           (zero_mac(f->bssid) || !memcmp(f->bssid, info->bssid, 6)) &&
           (f->channel_min <= 0 || info->channel >= f->channel_min) &&
           (f->channel_max <= 0 || info->channel <= f->channel_max);
}

/* Never invoke WLAN management/user callbacks with our control mutex held:
 * management calls into driver ops while holding its own mutex. */
static void dispatch_notifications(rt_uint32_t events, rt_bool_t scan)
{
    unsigned int event;
    if (scan)
    {
        size_t i;
        for (i = 0; i < wifi.scan_count; i++)
        {
            struct rt_wlan_buff buff = { .data = &wifi.scan_cache[i].info,
                                         .len = sizeof(struct rt_wlan_info) };
            if (scan_matches(buff.data))
                rt_wlan_dev_indicate_event_handle(&wifi.device, RT_WLAN_DEV_EVT_SCAN_REPORT, &buff);
        }
        rt_mutex_take(&wifi.control, RT_WAITING_FOREVER);
        wifi.scan_busy = RT_FALSE;
        rt_mutex_release(&wifi.control);
        rt_wlan_dev_indicate_event_handle(&wifi.device, RT_WLAN_DEV_EVT_SCAN_DONE, RT_NULL);
    }
    for (event = 0; event < RT_WLAN_DEV_EVT_MAX; event++)
        if (events & (1UL << event))
            rt_wlan_dev_indicate_event_handle(
                (event >= RT_WLAN_DEV_EVT_AP_START &&
                 event <= RT_WLAN_DEV_EVT_AP_ASSOCIATE_FAILED) ?
                &wifi.ap_device : &wifi.device, event, RT_NULL);
}

static void event_worker(void *parameter)
{
    ULONG item[2];
    (void)parameter;
    for (;;)
    {
        rt_uint32_t events;
        rt_bool_t scan;
        if (rt_mq_recv(wifi.queue, item, sizeof(item), RT_WAITING_FOREVER) < 0)
            continue;
        rt_mutex_take(&wifi.control, RT_WAITING_FOREVER);
#ifdef CONFIG_SUPP_EVENT_DEPRECATED
        if (item[0] == RA6WX_SP_DRV_FLAG)
            handle_driver_event((void *)item[1]);
        else if (item[0] == RA6WX_L2_PKT_RX_EV)
            handle_eapol((void *)item[1]);
#else
        /* Current firmware posts a pointer. Driver events embed their two
         * words; EAPOL posts an independently allocated two-word envelope. */
        if (item[0])
        {
            ULONG *packet = (void *)item[0];
            if (packet[0] == RA6WX_SP_DRV_FLAG)
                handle_driver_event((void *)packet);
            else
            {
                if (packet[0] == RA6WX_L2_PKT_RX_EV)
                    handle_eapol((void *)packet[1]);
                rt_free(packet);
            }
        }
#endif
        wifi_wpa_timers_run();
        if (wifi.deauth_reason && wifi.bss)
        {
            u16 reason = wifi.deauth_reason;
            wifi.deauth_reason = 0;
            if (wifi.state == WIFI_JOINING)
                notify(RT_WLAN_DEV_EVT_CONNECT_FAIL);
            wifi.state = WIFI_STOPPING;
            (void)rwnx_cfg80211_disconnect(WIFI_IFINDEX, FC80211_IFTYPE_STATION, reason);
            wifi_security_manager_deinit(&wifi.security);
        }
        events = wifi.notifications;
        wifi.notifications = 0;
        scan = wifi.scan_report;
        wifi.scan_report = RT_FALSE;
        rt_mutex_release(&wifi.control);
        dispatch_notifications(events, scan);
    }
}

static rt_err_t wlan_hw_init(struct rt_wlan_device *device)
{
    rt_tick_t start;
    struct wireless_dev *wdev;
    struct wireless_dev *ap_wdev;
    char country[] = COUNTRY_CODE_DEFAULT;
    int result = -RT_ERROR;
    (void)device;
    rt_mutex_take(&wifi.control, RT_WAITING_FOREVER);
    if (wifi.init_attempted)
    {
        result = wifi.init_result;
        goto out;
    }
    wifi.init_attempted = RT_TRUE;
    wifi.init_result = -RT_ERROR;
    result = wifi_hw_platform_prepare();
    if (result != RT_EOK)
    {
        LOG_E("hardware preparation failed: %d", result);
        goto out;
    }
    result = wifi_crypto_init();
    if ((result != 1) && (result != 2))
    {
        LOG_E("crypto initialization failed: %d", result);
        result = -RT_ERROR;
        goto out;
    }
    wifi.queue = wifi_vendor_event_queue_init();
    if (!wifi.queue)
    {
        LOG_E("vendor event queue creation failed");
        result = -RT_ENOMEM;
        goto out;
    }
    result = wifi_wpa_timers_init(wifi.queue);
    if (result != RT_EOK)
    {
        LOG_E("WPA timer initialization failed: %d", result);
        wifi_vendor_event_queue_deinit();
        goto out;
    }
    wifi.worker = rt_thread_create("wifievt", event_worker, RT_NULL,
                                    WIFI_EVENT_STACK, WIFI_EVENT_PRIO, 10);
    if (!wifi.worker)
    {
        LOG_E("driver event worker creation failed");
        wifi_wpa_timers_deinit();
        wifi_vendor_event_queue_deinit();
        result = -RT_ENOMEM;
        goto out;
    }
    rt_thread_startup(wifi.worker);
    /* Once vendor tasks have started they cannot be unwound by this API.
     * Keep their queue/worker alive on a later failure; don't duplicate tasks
     * on retry or free memory still reachable by an interrupt or firmware. */
    fc80211_enable_drv_event();
    romac4rtos_initialize(true, dg_configPTIMG_HDR_ADDR);
    romac4rtos_config(0, 0);
    ra6w1_regdb_data_init(country);
    result = rwnx_mac_task_initiailize();
    if (result != pdPASS)
    {
        LOG_E("MAC task initialization failed: %d", result);
        result = -RT_ERROR;
        goto out;
    }
    result = rwnx_driver_task_initiailize();
    if (result != pdPASS)
    {
        LOG_E("driver task initialization failed: %d", result);
        result = -RT_ERROR;
        goto out;
    }
    if (!CFG80211_semaphore)
        rwnx_cfg80211_initialize();
    if (!CFG80211_semaphore)
    {
        LOG_E("cfg80211 response semaphore was not created");
        result = -RT_ERROR;
        goto out;
    }
    start = rt_tick_get();
    while (rwnx_hw_get_status() != 1 || !rwnx_wiphy.bands[0] ||
           !rwnx_wiphy.bands[0]->channels || !rwnx_wiphy.bands[0]->n_channels)
    {
        if (rt_tick_get() - start >= rt_tick_from_millisecond(3000))
        {
            result = -RT_ETIMEOUT;
            goto out;
        }
        rt_thread_mdelay(1);
    }
    fc80211_update_channel_info();
    wdev = fc80211_wdev_from_if_idx(WIFI_IFINDEX);
    if (!wdev)
        wdev = rwnx_cfg80211_add_iface(WIFI_IFINDEX, RT_WLAN_DEVICE_STA_NAME, 0,
                                       FC80211_IFTYPE_STATION, RT_NULL);
    if (!wdev || wdev->wiphy != &rwnx_wiphy)
    {
        LOG_E("station interface creation failed: wdev=%p wiphy=%p expected=%p",
              wdev, wdev ? wdev->wiphy : RT_NULL, &rwnx_wiphy);
        result = -RT_ERROR;
        goto out;
    }
    ap_wdev = fc80211_wdev_from_if_idx(WIFI_AP_IFINDEX);
    if (!ap_wdev)
        ap_wdev = rwnx_cfg80211_add_iface(WIFI_AP_IFINDEX, RT_WLAN_DEVICE_AP_NAME, 0,
                                          FC80211_IFTYPE_AP, RT_NULL);
    if (!ap_wdev || ap_wdev->wiphy != &rwnx_wiphy ||
        ap_wdev->iftype != FC80211_IFTYPE_AP)
    {
        LOG_E("AP interface creation failed: wdev=%p wiphy=%p expected=%p",
              ap_wdev, ap_wdev ? ap_wdev->wiphy : RT_NULL, &rwnx_wiphy);
        result = -RT_ERROR;
        goto out;
    }
    wifi.global = fc80211_global_init();
    if (!wifi.global)
    {
        LOG_E("cfg80211 global context allocation failed");
        result = -RT_ENOMEM;
        goto out;
    }
    wifi.bss = wpa_driver_fc80211_init(RT_NULL, RT_WLAN_DEVICE_STA_NAME, wifi.global);
    if (!wifi.bss)
    {
        LOG_E("fc80211 station context initialization failed");
        fc80211_global_deinit(wifi.global);
        wifi.global = RT_NULL;
        result = -RT_ERROR;
        goto out;
    }
    if (!wifi.bss->drv || wifi.bss->ifindex != WIFI_IFINDEX)
    {
        LOG_E("invalid station context: bss=%p drv=%p ifindex=%d",
              wifi.bss, wifi.bss ? wifi.bss->drv : RT_NULL,
              wifi.bss ? wifi.bss->ifindex : -1);
        result = -RT_ERROR;
        goto out;
    }
    result = rwnx_cfg80211_set_power_mgmt(WIFI_IFINDEX, FC80211_IFTYPE_STATION, false, 0);
    if (result)
    {
        wifi.powersave = -1;
        LOG_W("cannot disable power saving during initialization: %d", result);
    }
    wifi.ready = RT_TRUE;
    supplicant_done();
    result = RT_EOK;
    LOG_I("%s ready, WLAN protocol, powersave=%d", RT_WLAN_DEVICE_STA_NAME, wifi.powersave);
out:
    wifi.init_result = result;
    rt_mutex_release(&wifi.control);
    if (result != RT_EOK)
        LOG_E("initialization failed: %d", result);
    return result;
}

static void wifi_init_worker_entry(void *parameter)
{
    int result;
    (void)parameter;
    result = wlan_hw_init(&wifi.device);
    if (result != RT_EOK)
        LOG_E("deferred Wi-Fi initialization failed: %d", result);
    rt_mutex_take(&wifi.control, RT_WAITING_FOREVER);
    wifi.init_scheduled = RT_FALSE;
    rt_mutex_release(&wifi.control);
}

/* Mode binding must be quick. Hardware startup can wait for firmware/clock
 * readiness and is therefore performed after WLAN management has recorded the
 * station device and attached the lwIP protocol. */
static rt_err_t wlan_init(struct rt_wlan_device *device)
{
    (void)device;
    rt_mutex_take(&wifi.control, RT_WAITING_FOREVER);
    if (wifi.ready || wifi.init_scheduled)
    {
        rt_mutex_release(&wifi.control);
        return RT_EOK;
    }
    wifi.init_scheduled = RT_TRUE;
    wifi.init_worker = rt_thread_create("wifiinit", wifi_init_worker_entry,
                                        RT_NULL, WIFI_EVENT_STACK, 13, 10);
    if (!wifi.init_worker)
    {
        wifi.init_scheduled = RT_FALSE;
        rt_mutex_release(&wifi.control);
        LOG_E("cannot create deferred Wi-Fi initialization thread");
        return -RT_ENOMEM;
    }
    rt_thread_startup(wifi.init_worker);
    rt_mutex_release(&wifi.control);
    return RT_EOK;
}

static rt_err_t wlan_mode(struct rt_wlan_device *device, rt_wlan_mode_t mode)
{
    rt_err_t result = RT_EOK;
    rt_bool_t is_ap = device == &wifi.ap_device;
    if ((is_ap && mode != RT_WLAN_AP && mode != RT_WLAN_NONE) ||
        (!is_ap && mode != RT_WLAN_STATION && mode != RT_WLAN_NONE))
        return -RT_ENOSYS;
    rt_mutex_take(&wifi.control, RT_WAITING_FOREVER);
    if (mode == RT_WLAN_NONE &&
        (is_ap ? wifi.ap_active : (wifi.state != WIFI_IDLE || wifi.scan_busy)))
    {
        rt_mutex_release(&wifi.control);
        return -RT_EBUSY;
    }
    if (is_ap)
        wifi.ap_enabled = mode == RT_WLAN_AP;
    else
        wifi.enabled = mode == RT_WLAN_STATION;
    if (mode == RT_WLAN_NONE)
    {
        struct rt_wlan_device *wlan = is_ap ? &wifi.ap_device : &wifi.device;
        if (!is_ap)
        {
            (void)fc80211_free_umac_scan_results(WIFI_WDEV_ID, WIFI_IFINDEX);
            rt_free(wifi.scan_cache);
            wifi.scan_cache = RT_NULL;
            wifi.scan_count = 0;
        }
        /* Remove the lwIP netdev when a mode is explicitly disabled. */
        if (wlan->prot != RT_NULL)
            result = rt_wlan_prot_detach_dev(wlan);
    }
    rt_mutex_release(&wifi.control);
    return result;
}

static rt_err_t wifi_wait_ready(rt_int32_t timeout_ms)
{
    rt_tick_t deadline = rt_tick_get() + rt_tick_from_millisecond(timeout_ms);
    for (;;)
    {
        rt_bool_t ready, scheduled;
        int result;
        rt_mutex_take(&wifi.control, RT_WAITING_FOREVER);
        ready = wifi.ready;
        scheduled = wifi.init_scheduled;
        result = wifi.init_result;
        rt_mutex_release(&wifi.control);
        if (ready)
            return RT_EOK;
        if (!scheduled && result != RT_EOK)
            return result;
        if ((rt_int32_t)(rt_tick_get() - deadline) >= 0)
            return -RT_ETIMEOUT;
        rt_thread_mdelay(20);
    }
}

static rt_err_t wlan_scan(struct rt_wlan_device *device, struct rt_scan_info *filter)
{
    struct wpa_driver_scan_params params = {0};
    int band, i, count = 0, result = -RT_EBUSY;
    if (device != &wifi.device)
        return -RT_EINVAL;
    result = wifi_wait_ready(5000);
    if (result != RT_EOK)
        return result;
    rt_mutex_take(&wifi.control, RT_WAITING_FOREVER);
    if (!wifi.ready || !wifi.enabled || wifi.scan_busy ||
        wifi.state == WIFI_JOINING || wifi.state == WIFI_STOPPING)
        goto out;
    memset(&wifi.scan_filter, 0, sizeof(wifi.scan_filter));
    if (filter)
        wifi.scan_filter = *filter;
    if (wifi.scan_filter.ssid.len > 32 || wifi.scan_filter.passive)
    {
        result = -RT_ENOSYS;
        goto out;
    }
    for (band = 0; band < NUM_FC80211_BANDS; band++)
    {
        struct ieee80211_supported_band *b = rwnx_wiphy.bands[band];
        if (!b)
            continue;
        for (i = 0; i < b->n_channels && count < WIFI_CHANNEL_LIMIT; i++)
        {
            int channel = freq_channel(b->channels[i].center_freq);
            if (!(b->channels[i].flags & WIFI_CHANNEL_DISABLED) &&
                (wifi.scan_filter.channel_min <= 0 || channel >= wifi.scan_filter.channel_min) &&
                (wifi.scan_filter.channel_max <= 0 || channel <= wifi.scan_filter.channel_max))
                wifi.frequencies[count++] = b->channels[i].center_freq;
        }
    }
    if (!count)
    {
        result = -RT_EINVAL;
        goto out;
    }
    (void)fc80211_free_umac_scan_results(WIFI_WDEV_ID, WIFI_IFINDEX);
    wifi.frequencies[count] = 0;
    params.freqs = wifi.frequencies;
    params.num_ssids = 1;
    params.ssids[0].ssid = wifi.scan_filter.ssid.len ? wifi.scan_filter.ssid.val : RT_NULL;
    params.ssids[0].ssid_len = wifi.scan_filter.ssid.len;
    rt_free(wifi.scan_cache);
    wifi.scan_cache = RT_NULL;
    wifi.scan_count = 0;
    wifi.scan_busy = RT_TRUE;
    result = eloop_register_timeout(WIFI_OPERATION_MS / 1000, 0, operation_timeout, &wifi, (void *)1);
    if (!result)
        result = rsdev_cfg80211_scan(WIFI_WDEV_ID, wifi.bss->drv, &params);
    if (result)
    {
        eloop_cancel_timeout(operation_timeout, &wifi, (void *)1);
        wifi.scan_busy = RT_FALSE;
        result = -RT_ERROR;
    }
out:
    rt_mutex_release(&wifi.control);
    return result;
}

static int derive_pmk(const u8 *ssid, size_t ssid_len,
                      const u8 *key, size_t key_len, u8 pmk[32])
{
    char password[65];
    int result, i;
    if (key_len == 64)
    {
        for (i = 0; i < 32; i++)
        {
            int j, byte = 0;
            for (j = 0; j < 2; j++)
            {
                unsigned char c = key[i * 2 + j];
                int nibble = c >= '0' && c <= '9' ? c - '0' :
                             c >= 'a' && c <= 'f' ? c - 'a' + 10 :
                             c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
                if (nibble < 0)
                    return -RT_EINVAL;
                byte = byte * 16 + nibble;
            }
            pmk[i] = byte;
        }
        return RT_EOK;
    }
    if (key_len < 8 || key_len > 63)
        return -RT_EINVAL;
    memcpy(password, key, key_len);
    password[key_len] = 0;
    result = pbkdf2_sha1(password, ssid, ssid_len, 4096, pmk, 32);
    memset(password, 0, sizeof(password));
    return result ? -RT_ERROR : RT_EOK;
}

static int parse_pmk(const struct rt_sta_info *sta)
{
    return derive_pmk(sta->ssid.val, sta->ssid.len,
                      sta->key.val, sta->key.len, wifi.pmk);
}

static rt_err_t wifi_wait_ready(rt_int32_t timeout_ms);

static struct ieee80211_channel *ap_channel(rt_uint16_t channel)
{
    int band, i;
    int frequency = channel_freq(channel);

    for (band = 0; band < NUM_FC80211_BANDS; band++)
    {
        struct ieee80211_supported_band *supported = rwnx_wiphy.bands[band];
        if (!supported)
            continue;
        for (i = 0; i < supported->n_channels; i++)
            if (supported->channels[i].center_freq == frequency &&
                !(supported->channels[i].flags & WIFI_CHANNEL_DISABLED))
                return &supported->channels[i];
    }
    return RT_NULL;
}

static size_t append_ie(u8 *buffer, size_t offset, size_t capacity,
                        u8 id, const u8 *data, size_t length)
{
    if (length > 255 || offset > capacity || capacity - offset < length + 2)
        return 0;
    buffer[offset++] = id;
    buffer[offset++] = (u8)length;
    memcpy(buffer + offset, data, length);
    return offset + length;
}

static int build_ap_settings(const struct rt_ap_info *info,
                             struct cfg80211_ap_settings *settings)
{
    static const u8 rates[] = {0x82, 0x84, 0x8b, 0x96, 0x24, 0x30, 0x48, 0x6c};
    struct ieee80211_channel *channel;
    rt_uint8_t mac[6];
    size_t offset;
    u8 channel_number;
    rt_bool_t secure;

    if (!info || !settings || !info->ssid.len || info->ssid.len > 32)
        return -RT_EINVAL;
    if (info->channel < 1 || info->channel > 14)
        return -RT_EINVAL;
    secure = info->security == SECURITY_WPA2_AES_PSK;
    if (info->security != SECURITY_OPEN && !secure)
        return -RT_ENOSYS;
    if (secure && (info->key.len < 8 || info->key.len > 63))
        return -RT_EINVAL;
    channel = ap_channel(info->channel);
    if (!channel || get_mac_address(WIFI_AP_MAC_IFACE, mac) != RT_EOK)
        return -RT_EINVAL;
    if (secure && derive_pmk(info->ssid.val, info->ssid.len,
                             info->key.val, info->key.len, wifi.ap_pmk) != RT_EOK)
        return -RT_EINVAL;

    memset(settings, 0, sizeof(*settings));
    settings->ssid = info->ssid.val;
    settings->ssid_len = info->ssid.len;
    settings->hidden_ssid = info->hidden ? FC80211_HIDDEN_SSID_ZERO_LEN :
                                           FC80211_HIDDEN_SSID_NOT_IN_USE;
    settings->auth_type = FC80211_AUTHTYPE_OPEN_SYSTEM;
    settings->privacy = secure;
    settings->beacon_interval = 100;
    settings->dtim_period = 2;
    settings->chandef.chan = channel;
    settings->chandef.width = FC80211_CHAN_WIDTH_20;
    settings->chandef.center_freq1 = channel->center_freq;
    settings->chandef.chan_type = FC80211_CHAN_HT20;

    memset(wifi.ap_beacon_head, 0, sizeof(wifi.ap_beacon_head));
    wifi.ap_beacon_head[0] = 0x80;
    memcpy(wifi.ap_beacon_head + 4, mac, 6);
    memcpy(wifi.ap_beacon_head + 10, mac, 6);
    memcpy(wifi.ap_beacon_head + 16, mac, 6);
    wifi.ap_beacon_head[32] = 100;
    wifi.ap_beacon_head[34] = (u8)(secure ? 0x31 : 0x21);
    wifi.ap_beacon_head[35] = 0x04;
    settings->beacon.head = wifi.ap_beacon_head;
    settings->beacon.head_len = sizeof(wifi.ap_beacon_head);

    offset = 0;
    offset = append_ie(wifi.ap_beacon_tail, offset, sizeof(wifi.ap_beacon_tail),
                       0, info->ssid.val, info->ssid.len);
    if (!offset)
        return -RT_ENOMEM;
    offset = append_ie(wifi.ap_beacon_tail, offset, sizeof(wifi.ap_beacon_tail),
                       1, rates, sizeof(rates));
    if (!offset)
        return -RT_ENOMEM;
    channel_number = (u8)info->channel;
    offset = append_ie(wifi.ap_beacon_tail, offset, sizeof(wifi.ap_beacon_tail),
                       3, &channel_number, 1);
    if (!offset)
        return -RT_ENOMEM;
    if (secure)
    {
        static const u8 rsn[] = {
            0x01, 0x00, 0x00, 0x0f, 0xac, 0x04,
            0x01, 0x00, 0x00, 0x0f, 0xac, 0x04,
            0x01, 0x00, 0x00, 0x0f, 0xac, 0x02, 0x00, 0x00
        };
        offset = append_ie(wifi.ap_beacon_tail, offset, sizeof(wifi.ap_beacon_tail),
                           48, rsn, sizeof(rsn));
        if (!offset)
            return -RT_ENOMEM;
        settings->crypto.wpa_versions = FC80211_WPA_VERSION_2;
        settings->crypto.cipher_group = WLAN_CIPHER_SUITE_CCMP;
        settings->crypto.n_ciphers_pairwise = 1;
        settings->crypto.ciphers_pairwise[0] = WLAN_CIPHER_SUITE_CCMP;
        settings->crypto.n_akm_suites = 1;
        settings->crypto.akm_suites[0] = WLAN_AKM_SUITE_PSK;
        settings->crypto.psk = wifi.ap_pmk;
    }
    wifi.ap_beacon_tail_len = offset;
    settings->beacon.tail = wifi.ap_beacon_tail;
    settings->beacon.tail_len = offset;
    return RT_EOK;
}

static rt_err_t wlan_softap(struct rt_wlan_device *device, struct rt_ap_info *info)
{
    struct cfg80211_ap_settings settings;
    int result;
    if (device != &wifi.ap_device || !info)
        return -RT_EINVAL;
    result = wifi_wait_ready(5000);
    if (result != RT_EOK)
        return result;
    rt_mutex_take(&wifi.control, RT_WAITING_FOREVER);
    if (!wifi.ap_enabled || wifi.ap_active)
    {
        rt_mutex_release(&wifi.control);
        return -RT_EBUSY;
    }
    result = build_ap_settings(info, &settings);
    if (!result)
        result = rwnx_cfg80211_start_ap(WIFI_AP_IFINDEX, FC80211_IFTYPE_AP, &settings);
    if (!result)
    {
        wifi.ap_info = *info;
        wifi.ap_active = RT_TRUE;
        notify(RT_WLAN_DEV_EVT_AP_START);
        LOG_I("SoftAP started ssid=%.*s channel=%d security=%d",
              info->ssid.len, info->ssid.val, info->channel, info->security);
    }
    else
    {
        memset(wifi.ap_pmk, 0, sizeof(wifi.ap_pmk));
        LOG_E("SoftAP start failed: %d", result);
    }
    rt_mutex_release(&wifi.control);
    return result ? -RT_ERROR : RT_EOK;
}

static rt_err_t wlan_join(struct rt_wlan_device *device, struct rt_sta_info *sta)
{
    struct wifi_security_config config = {0};
    size_t i;
    int result = -RT_EBUSY;
    (void)device;
    if (device != &wifi.device || !sta || !sta->ssid.len || sta->ssid.len > 32)
        return -RT_EINVAL;
    result = wifi_wait_ready(5000);
    if (result != RT_EOK)
        return result;
    if (sta->security != SECURITY_OPEN && sta->security != SECURITY_WPA2_AES_PSK)
        return -RT_ENOSYS;
    rt_mutex_take(&wifi.control, RT_WAITING_FOREVER);
    if (!wifi.ready || !wifi.enabled || wifi.scan_busy || wifi.state != WIFI_IDLE)
        goto out;
    wifi.target = *sta;
    wifi.ap_rsn_len = 0;
    memset(&wifi.assoc, 0, sizeof(wifi.assoc));
    for (i = 0; i < wifi.scan_count; i++)
    {
        struct wifi_scan_entry *entry = &wifi.scan_cache[i];
        if (entry->info.ssid.len == sta->ssid.len &&
            !memcmp(entry->info.ssid.val, sta->ssid.val, sta->ssid.len) &&
            (zero_mac(sta->bssid) || !memcmp(entry->info.bssid, sta->bssid, 6)) &&
            entry->info.security == sta->security)
        {
            memcpy(wifi.target.bssid, entry->info.bssid, 6);
            wifi.target.channel = entry->info.channel;
            memcpy(wifi.ap_rsn, entry->rsn, entry->rsn_len);
            wifi.ap_rsn_len = entry->rsn_len;
            break;
        }
    }
    /* The standard management join scans first. A direct low-level join must
     * also identify a scanned BSS so its RSN/cipher policy can be validated. */
    if (zero_mac(wifi.target.bssid) || !wifi.target.channel ||
        (sta->security != SECURITY_OPEN && !wifi.ap_rsn_len))
    {
        result = -RT_EINVAL;
        goto clear_key;
    }
    wifi.assoc.ssid = wifi.target.ssid.val;
    wifi.assoc.ssid_len = wifi.target.ssid.len;
    wifi.assoc.bssid = wifi.target.bssid;
    wifi.assoc.freq.freq = channel_freq(wifi.target.channel);
    wifi.assoc.auth_alg = WPA_AUTH_ALG_OPEN;
    wifi.assoc.mode = IEEE80211_MODE_INFRA;
    wifi.assoc.bg_scan_period = -1;
    wifi.assoc.key_mgmt_suite = WPA_KEY_MGMT_NONE;
    wifi.assoc.pairwise_suite = wifi.assoc.group_suite = WPA_CIPHER_NONE;
    if (sta->security != SECURITY_OPEN)
    {
        result = parse_pmk(sta);
        if (result)
            goto clear_key;
        config.bss = wifi.bss;
        config.ifname = RT_WLAN_DEVICE_STA_NAME;
        config.own_addr = wpa_driver_fc80211_get_macaddr(wifi.bss);
        config.ssid = wifi.target.ssid.val;
        config.ssid_len = wifi.target.ssid.len;
        config.pmk = wifi.pmk;
        config.pmk_len = sizeof(wifi.pmk);
        config.mode = WIFI_SECURITY_WPA2_PSK;
        config.user_ctx = &wifi;
        config.callbacks.state_changed = security_state;
        config.callbacks.deauthenticate = security_deauth;
        config.callbacks.reconnect = security_reconnect;
        result = wifi_security_manager_init(&wifi.security, &config);
        if (result)
            goto clear_key;
        wifi.assoc.wpa_ie = wifi_security_manager_assoc_ie(&wifi.assoc.wpa_ie_len);
        wifi.assoc.wpa_proto = WPA_PROTO_RSN;
        wifi.assoc.key_mgmt_suite = WPA_KEY_MGMT_PSK;
        wifi.assoc.pairwise_suite = wifi.assoc.group_suite = WPA_CIPHER_CCMP;
        wifi.assoc.drop_unencrypted = 1;
        /* Keys are installed by the host WPA state machine, no firmware PSK offload. */
    }
    wifi.state = WIFI_JOINING;
    result = eloop_register_timeout(WIFI_OPERATION_MS / 1000, 0, operation_timeout, &wifi, RT_NULL);
    if (!result)
        result = rsdev_cfg80211_associate(wifi.bss->drv, &wifi.assoc);
    if (result)
    {
        eloop_cancel_timeout(operation_timeout, &wifi, RT_NULL);
        wifi_security_manager_deinit(&wifi.security);
        wifi.state = WIFI_IDLE;
        result = -RT_ERROR;
    }
clear_key:
    memset(&wifi.target.key, 0, sizeof(wifi.target.key));
    memset(wifi.pmk, 0, sizeof(wifi.pmk));
out:
    rt_mutex_release(&wifi.control);
    return result;
}

static rt_err_t wlan_disconnect(struct rt_wlan_device *device)
{
    int result = RT_EOK;
    if (device != &wifi.device)
        return -RT_EINVAL;
    rt_mutex_take(&wifi.control, RT_WAITING_FOREVER);
    if (wifi.state != WIFI_IDLE && wifi.state != WIFI_STOPPING)
    {
        result = rwnx_cfg80211_disconnect(WIFI_IFINDEX, FC80211_IFTYPE_STATION, 3);
        if (!result)
        {
            eloop_cancel_timeout(operation_timeout, &wifi, RT_NULL);
            wifi.state = WIFI_STOPPING;
        }
        else
            result = -RT_ERROR;
    }
    rt_mutex_release(&wifi.control);
    return result;
}

static rt_err_t wlan_ap_stop(struct rt_wlan_device *device)
{
    int result;
    if (device != &wifi.ap_device)
        return -RT_EINVAL;
    rt_mutex_take(&wifi.control, RT_WAITING_FOREVER);
    if (!wifi.ap_active)
    {
        rt_mutex_release(&wifi.control);
        return RT_EOK;
    }
    result = rwnx_cfg80211_stop_ap(WIFI_AP_IFINDEX, FC80211_IFTYPE_AP);
    if (!result)
    {
        wifi.ap_active = RT_FALSE;
        memset(&wifi.ap_info, 0, sizeof(wifi.ap_info));
        memset(wifi.ap_pmk, 0, sizeof(wifi.ap_pmk));
        notify(RT_WLAN_DEV_EVT_AP_STOP);
        LOG_I("SoftAP stopped");
    }
    rt_mutex_release(&wifi.control);
    return result ? -RT_ERROR : RT_EOK;
}

static rt_err_t wlan_ap_deauth(struct rt_wlan_device *device, rt_uint8_t mac[6])
{
    struct station_del_parameters params;
    if (device != &wifi.ap_device || !mac)
        return -RT_EINVAL;
    rt_mutex_take(&wifi.control, RT_WAITING_FOREVER);
    if (!wifi.ap_active)
    {
        rt_mutex_release(&wifi.control);
        return -RT_ERROR;
    }
    memset(&params, 0, sizeof(params));
    memcpy((void *)params.mac, mac, 6);
    params.reason_code = 3;
    rt_mutex_release(&wifi.control);
    return rwnx_cfg80211_del_station(WIFI_AP_IFINDEX, FC80211_IFTYPE_AP, &params) ?
           -RT_ERROR : RT_EOK;
}

static rt_err_t wlan_scan_stop(struct rt_wlan_device *device)
{
    int result = RT_EOK;
    if (device != &wifi.device)
        return -RT_EINVAL;
    rt_mutex_take(&wifi.control, RT_WAITING_FOREVER);
    if (wifi.scan_busy)
        result = fc80211_abort_scan(WIFI_WDEV_ID, WIFI_IFINDEX) ? -RT_ERROR : RT_EOK;
    rt_mutex_release(&wifi.control);
    return result;
}

static int wlan_get_info(struct rt_wlan_device *device, struct rt_wlan_info *info)
{
    struct station_info station = {0};
    int result = -RT_ERROR;
    if (device != &wifi.device || !info)
        return -RT_EINVAL;
    rt_mutex_take(&wifi.control, RT_WAITING_FOREVER);
    if (wifi.state == WIFI_LINKED)
    {
        memset(info, 0, sizeof(*info));
        info->ssid = wifi.target.ssid;
        memcpy(info->bssid, wifi.target.bssid, 6);
        info->security = wifi.target.security;
        info->channel = wifi.target.channel;
        info->band = info->channel <= 14 ? RT_802_11_BAND_2_4GHZ : RT_802_11_BAND_5GHZ;
        result = rwnx_cfg80211_get_station(WIFI_IFINDEX, FC80211_IFTYPE_STATION,
                                            wifi.target.bssid, &station);
        if (!result)
            info->rssi = station.signal;
        else
            result = -RT_ERROR;
    }
    rt_mutex_release(&wifi.control);
    return result;
}

static int wlan_ap_get_info(struct rt_wlan_device *device, struct rt_wlan_info *info)
{
    if (device != &wifi.ap_device || !info)
        return -RT_EINVAL;
    rt_mutex_take(&wifi.control, RT_WAITING_FOREVER);
    if (!wifi.ap_active)
    {
        rt_mutex_release(&wifi.control);
        return -RT_ERROR;
    }
    memset(info, 0, sizeof(*info));
    info->ssid = wifi.ap_info.ssid;
    info->channel = wifi.ap_info.channel;
    info->band = info->channel <= 14 ? RT_802_11_BAND_2_4GHZ : RT_802_11_BAND_5GHZ;
    info->security = wifi.ap_info.security;
    if (get_mac_address(WIFI_AP_MAC_IFACE, info->bssid) != RT_EOK)
    {
        rt_mutex_release(&wifi.control);
        return -RT_ERROR;
    }
    rt_mutex_release(&wifi.control);
    return RT_EOK;
}

static int wlan_rssi(struct rt_wlan_device *device)
{
    struct rt_wlan_info info;
    return wlan_get_info(device, &info) == RT_EOK ? info.rssi : -127;
}

static int wlan_channel(struct rt_wlan_device *device)
{
    struct rt_wlan_info info;
    return wlan_get_info(device, &info) == RT_EOK ? info.channel : -RT_ERROR;
}

static rt_err_t wlan_power_save(struct rt_wlan_device *device, int level)
{
    int result;
    (void)device;
    if (level != 0 && level != 1)
        return -RT_EINVAL;
    rt_mutex_take(&wifi.control, RT_WAITING_FOREVER);
    result = wifi.ready ? rwnx_cfg80211_set_power_mgmt(WIFI_IFINDEX,
                             FC80211_IFTYPE_STATION, level != 0, 0) : -1;
    if (!result)
        wifi.powersave = level;
    rt_mutex_release(&wifi.control);
    return result ? -RT_ERROR : RT_EOK;
}

static int wlan_get_power_save(struct rt_wlan_device *device)
{
    int powersave;
    (void)device;
    rt_mutex_take(&wifi.control, RT_WAITING_FOREVER);
    powersave = wifi.powersave;
    rt_mutex_release(&wifi.control);
    return powersave;
}

/* These controls require firmware interfaces that are not present in the
 * standalone RA6W1 archive. Return an explicit error instead of allowing the
 * WLAN core's NULL-operation default (RT_EOK) to report a false success. */
static rt_err_t wlan_unsupported_promisc(struct rt_wlan_device *device,
                                         rt_bool_t start)
{
    (void)device;
    (void)start;
    return -RT_ENOSYS;
}

static rt_err_t wlan_unsupported_filter(struct rt_wlan_device *device,
                                        struct rt_wlan_filter *filter)
{
    (void)device;
    (void)filter;
    return -RT_ENOSYS;
}

static rt_err_t wlan_unsupported_channel(struct rt_wlan_device *device,
                                         int channel)
{
    (void)device;
    (void)channel;
    return -RT_ENOSYS;
}

static rt_err_t wlan_unsupported_country(struct rt_wlan_device *device,
                                         rt_country_code_t country)
{
    (void)device;
    (void)country;
    return -RT_ENOSYS;
}

static rt_err_t wlan_unsupported_mac(struct rt_wlan_device *device,
                                     rt_uint8_t mac[6])
{
    (void)device;
    (void)mac;
    return -RT_ENOSYS;
}

static int wlan_unsupported_raw_frame(struct rt_wlan_device *device,
                                      void *buffer, int length)
{
    (void)device;
    (void)buffer;
    (void)length;
    return -RT_ENOSYS;
}

static rt_err_t wlan_mac(struct rt_wlan_device *device, rt_uint8_t mac[6])
{
    unsigned int iface = device == &wifi.ap_device ? WIFI_AP_MAC_IFACE : WIFI_MAC_IFACE;
    return get_mac_address(iface, mac);
}

/* The current archive schedules this callback on tcpip_thread. RX uses the
 * legacy VIF+2 index; TX uses VIF. WLAN owns the actual netif numbering.
 * Success transfers the reference to WLAN; on rejection we free it here. */
void rrq16x_lwip_mbox_rx(struct pbuf *p)
{
    struct rt_wlan_device *device = &wifi.device;
    rt_uint8_t ifindex;
    rt_base_t level;
    if (!p)
        return;
    level = rt_hw_interrupt_disable();
    stats.rx_packets++;
    stats.rx_bytes += p->tot_len;
    rt_hw_interrupt_enable(level);
    if (is_dhcp_frame(p))
        LOG_I("DHCP RX len=%d idx=%u state=%d", p->tot_len, p->if_idx, wifi.state);
    /* Firmware reports VIF indices directly on some builds and VIF + 2 on
     * older builds. Normalize both forms before handing the frame to the
     * corresponding WLAN protocol instance. */
    if (p->if_idx == WIFI_AP_IFINDEX || p->if_idx == WIFI_AP_IFINDEX + 2)
        device = &wifi.ap_device;
    else if (wifi.ap_active && wifi.state != WIFI_LINKED)
        device = &wifi.ap_device;
    if (device == &wifi.ap_device ? !wifi.ap_active : wifi.state != WIFI_LINKED)
        goto drop;
    ifindex = device == &wifi.ap_device ? WIFI_AP_IFINDEX : WIFI_IFINDEX;
    p->if_idx = ifindex;
    if (rt_wlan_dev_transfer_prot(device, p, p->tot_len) == RT_EOK)
        return;
drop:
    level = rt_hw_interrupt_disable();
    stats.rx_dropped++;
    rt_hw_interrupt_enable(level);
    pbuf_free(p);
}

static int wlan_send(struct rt_wlan_device *device, void *buffer, int length)
{
    struct pbuf *p = buffer, *tx;
    rt_base_t level;
    rt_bool_t copy;
    int free_slots;
    unsigned long result;
    rt_bool_t dhcp;
    rt_bool_t is_ap = device == &wifi.ap_device;
    rt_uint8_t ifindex = is_ap ? WIFI_AP_IFINDEX : WIFI_IFINDEX;
    if (!p || length != p->tot_len || length < 14 || length > 1514)
        return -RT_EINVAL;
    dhcp = is_dhcp_frame(p);
    if (is_ap ? !wifi.ap_active : wifi.state != WIFI_LINKED)
        return -RT_ERROR;
    free_slots = rwnx_free_txq_count();
    level = rt_hw_interrupt_disable();
    if ((rt_uint32_t)(free_slots > 0 ? free_slots : 0) < stats.min_free_txq)
        stats.min_free_txq = free_slots > 0 ? free_slots : 0;
    rt_hw_interrupt_enable(level);
    if (free_slots <= 0 || rwnx_empty_txq_item())
    {
        level = rt_hw_interrupt_disable();
        stats.queue_rejected++;
        rt_hw_interrupt_enable(level);
        if (dhcp)
            LOG_W("DHCP TX queue rejected free=%d", free_slots);
        return -RT_ENOMEM;
    }
    /* Firmware writes descriptors before the Ethernet header. Only privately
     * owned, contiguous PBUF_RAM with the reserved headroom is safe to share.
     * Copy RX/custom/reference chains and TCP buffers already in flight. */
    copy = p->next || p->len != p->tot_len || p->ref != 1 ||
           !pbuf_match_type(p, PBUF_RAM) || (p->flags & PBUF_FLAG_IS_CUSTOM) ||
           (uintptr_t)p->payload < (uintptr_t)p + LWIP_MEM_ALIGN_SIZE(sizeof(*p)) + 456;
    tx = p;
    if (copy)
    {
        tx = pbuf_alloc(PBUF_RAW_TX, p->tot_len, PBUF_RAM);
        if (!tx)
            return -RT_ENOMEM;
        if (pbuf_copy(tx, p) != ERR_OK)
        {
            pbuf_free(tx);
            return -RT_ERROR;
        }
    }
    else
        pbuf_ref(tx);
    tx->if_idx = ifindex;
    result = rwnx_driver_task_send_event(WIFI_LWIP_TASK, WIFI_DRIVER_TASK, WIFI_TX_EVENT, tx);
    if (!result)
    {
        /* Return only the reference acquired here; caller retains its own. */
        pbuf_free(tx);
        level = rt_hw_interrupt_disable();
        stats.event_rejected++;
        rt_hw_interrupt_enable(level);
        if (dhcp)
            LOG_W("DHCP TX event rejected");
        return -RT_ENOMEM;
    }
    /* Driver may already have freed tx after a successful queue operation. */
    level = rt_hw_interrupt_disable();
    stats.tx_packets++;
    stats.tx_bytes += length;
    stats.tx_copied += copy;
    rt_hw_interrupt_enable(level);
    if (dhcp)
        LOG_I("DHCP TX queued len=%d copied=%d", length, copy);
    return RT_EOK;
}

static const struct rt_wlan_dev_ops wlan_ops =
{
    .wlan_init = wlan_init,
    .wlan_mode = wlan_mode,
    .wlan_scan = wlan_scan,
    .wlan_join = wlan_join,
    .wlan_softap = wlan_softap,
    .wlan_disconnect = wlan_disconnect,
    .wlan_ap_stop = wlan_ap_stop,
    .wlan_ap_deauth = wlan_ap_deauth,
    .wlan_scan_stop = wlan_scan_stop,
    .wlan_get_rssi = wlan_rssi,
    .wlan_get_info = wlan_get_info,
    .wlan_ap_get_info = wlan_ap_get_info,
    .wlan_set_powersave = wlan_power_save,
    .wlan_get_powersave = wlan_get_power_save,
    .wlan_get_channel = wlan_channel,
    .wlan_cfg_promisc = wlan_unsupported_promisc,
    .wlan_cfg_filter = wlan_unsupported_filter,
    .wlan_cfg_mgnt_filter = wlan_unsupported_promisc,
    .wlan_set_channel = wlan_unsupported_channel,
    .wlan_set_country = wlan_unsupported_country,
    .wlan_set_mac = wlan_unsupported_mac,
    .wlan_get_mac = wlan_mac,
    .wlan_send = wlan_send,
    .wlan_send_raw_frame = wlan_unsupported_raw_frame,
};

static int wifi_wlan_register(void)
{
    int result;
    rt_mutex_init(&wifi.control, "wifictl", RT_IPC_FLAG_PRIO);
    wifi_wlan_stats_reset();
    result = rt_wlan_dev_register(&wifi.device, RT_WLAN_DEVICE_STA_NAME, &wlan_ops,
                                   RT_WLAN_FLAG_STA_ONLY, RT_NULL);
    if (result != RT_EOK)
    {
        rt_mutex_detach(&wifi.control);
        LOG_E("WLAN device registration failed: %d", result);
        return result;
    }
    result = rt_wlan_dev_register(&wifi.ap_device, RT_WLAN_DEVICE_AP_NAME, &wlan_ops,
                                  RT_WLAN_FLAG_AP_ONLY, RT_NULL);
    if (result != RT_EOK)
    {
        LOG_E("WLAN AP device registration failed: %d", result);
        return result;
    }
    result = rt_wlan_set_mode(RT_WLAN_DEVICE_STA_NAME, RT_WLAN_STATION);
    if (result != RT_EOK)
    {
        LOG_E("STA mode binding failed: %d", result);
        return result;
    }
    LOG_I("STA mode bound: %s", RT_WLAN_DEVICE_STA_NAME);
    result = rt_wlan_set_mode(RT_WLAN_DEVICE_AP_NAME, RT_WLAN_AP);
    if (result != RT_EOK)
        LOG_E("AP mode binding failed: %d", result);
    else
        LOG_I("AP mode bound: %s", RT_WLAN_DEVICE_AP_NAME);
    return result;
}
INIT_APP_EXPORT(wifi_wlan_register);
