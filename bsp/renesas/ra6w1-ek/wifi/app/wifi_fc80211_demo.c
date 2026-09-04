/*
 * Standalone RA6W1 Wi-Fi bring-up and scan example.
 *
 * This file intentionally does not create a wpa_supplicant context and does
 * not call any WIFI_ or rm_wifi_ service API.  The only Wi-Fi control path is
 * the cfg80211 ABI exported by librwnx_drv.a.  The event queue is used only as
 * the transport for asynchronous driver completion notifications.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rtthread.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "co_int.h"

/* Older SDK headers use uint32 while co_int.h exposes uint32_t. */
typedef unsigned long uint32;

#include "rwnx_cfg.h"
#include "driver_fc80211.h"
#include "rwnx_hw.h"
#include "romac4rtos.h"
#include "supp_eloop.h"
#include "sys_feature.h"
#include "mbedtls/md.h"
#include "mbedtls/pkcs5.h"

#include "wifi_crypto_port.h"

#include "../platform/wifi_hw_prepare.h"

/* Symbols exported by the prebuilt Wi-Fi archives. */
extern BaseType_t rwnx_mac_task_initiailize(void);
extern BaseType_t rwnx_driver_task_initiailize(void);
extern int rwnx_hw_get_status(void);
extern void rwnx_cfg80211_initialize(void);
extern struct wiphy rwnx_wiphy;
extern void fc80211_enable_drv_event(void);
extern void fc80211_update_channel_info(void);
extern void ra6w1_regdb_data_init(char *country);
extern QueueHandle_t TO_SUPP_QUEUE;
extern QueueHandle_t CFG80211_semaphore;

/* These functions live in librwnx_drv.a but are not declared by rwnx_cfg.h. */
extern int fc80211_get_scan(ULONG wdev_id, int ifidx, void *arg);
extern int fc80211_abort_scan(ULONG wdev_id, int ifidx);
extern int fc80211_free_umac_scan_results(ULONG wdev_id, int ifidx);

#define WIFI_DEMO_IF_INDEX       (0U)
#define WIFI_DEMO_IF_NAME        "wlan0"
#define WIFI_DEMO_STATUS_ACTIVE  (1)
#define WIFI_DEMO_STATUS_RETRY   (1000U)
#define WIFI_DEMO_WIPHY_WAIT_MS  (1500U)
#define WIFI_DEMO_SCAN_WAIT_MS   (10000U)
#define WIFI_DEMO_SCAN_SETTLE_MS (200U)
#define WIFI_DEMO_CONNECT_WAIT_MS (15000U)
#define WIFI_DEMO_DISCONNECT_WAIT_MS (2000U)
#define WIFI_DEMO_POLL_MS        (20U)
#define WIFI_DEMO_MAX_CHANNELS   (42U)
#define WIFI_DEMO_MAX_SSID_LEN   (32U)
#define WIFI_DEMO_PSK_LEN        (32U)

#define WIFI_DEMO_STATUS_NOT_SUPPORTED_AUTH_ALG (13)
#define WIFI_DEMO_STATUS_AUTH_TIMEOUT (16)
#define WIFI_DEMO_STATUS_UNSPECIFIED_FAILURE (1)

/* The firmware reports WLAN_STATUS_SUCCESS as zero. */
#define WIFI_DEMO_STATUS_SUCCESS (0)

/* rwnx uses bit 0 for a disabled channel in its private channel flags. */
#define WIFI_DEMO_CHAN_DISABLED  (1U)

struct wifi_demo_state
{
    bool event_queue_ready;
    bool cfg80211_ready;
    bool crypto_ready;
    bool hardware_ready;
    bool wiphy_ready;
    bool iface_ready;
    bool scan_active;
    bool scan_done;
    bool scan_aborted;
    bool connect_active;
    bool connect_done;
    bool connect_timed_out;
    bool connected;
    int connect_status;
    u8 connect_bssid[ETH_ALEN];
    bool disconnect_done;
    struct wireless_dev *wdev;
};

static struct wifi_demo_state s_wifi_demo;

/* rsdev_cfg80211_associate() converts the supplicant ABI into a temporary
 * cfg80211_connect_params object and then calls rdev_connect().  Keep the
 * input objects static because this demo waits for the asynchronous result
 * after the call returns. */
static struct fc80211_global *s_connect_global;
static struct i802_bss *s_connect_bss;
static struct wpa_driver_fc80211_data *s_connect_driver;
static struct wpa_driver_associate_params s_connect_assoc;
static char s_connect_ssid[WIFI_DEMO_MAX_SSID_LEN + 1U];
static u8 s_connect_psk[WIFI_DEMO_PSK_LEN];
static u8 s_connect_target_bssid[ETH_ALEN];
static bool s_connect_target_bssid_set;
static unsigned int s_connect_target_freq;

static void wifi_demo_print_bssid(const u8 *bssid);

/* RSN IE for WPA2-Personal, CCMP, PSK.  cfg80211's crypto fields describe
 * the security policy, but the shipped rwnx driver copies the IE verbatim
 * into the association request when it is present. */
static const u8 s_connect_rsn_ie[] = {
    0x30, 0x14,
    0x01, 0x00,
    0x00, 0x0f, 0xac, 0x04,
    0x01, 0x00,
    0x00, 0x0f, 0xac, 0x04,
    0x01, 0x00,
    0x00, 0x0f, 0xac, 0x02,
    0x00, 0x00
};

_Static_assert(sizeof(s_connect_rsn_ie) == 22U,
               "WPA2-PSK RSN IE size changed");

/*
 * fc80211_get_scan() predates the public cfg80211 result API.  It receives an
 * internal result object whose first two members are an array and a count.
 * Keep this ABI-local copy here instead of importing supplicant's private
 * driver structures.
 */
struct wifi_demo_scan_res
{
    unsigned int flags;
    u8 bssid[ETH_ALEN];
    int freq;
    u16 beacon_int;
    u16 caps;
    int qual;
    int noise;
    int level;
    u64 tsf;
    unsigned int age;
    unsigned int est_throughput;
    int snr;
    u64 parent_tsf;
    u8 tsf_bssid[ETH_ALEN];
    size_t ie_len;
    size_t beacon_ie_len;
    u8 *internal_bss;
    const u8 *ie;
    const u8 *beacon_ie;
};

struct wifi_demo_scan_results
{
    struct wifi_demo_scan_res **res;
    size_t num;
};

_Static_assert(sizeof(struct wifi_demo_scan_res) == 96U,
               "fc80211 scan result ABI changed");
_Static_assert(offsetof(struct wifi_demo_scan_res, ie) == 84U,
               "fc80211 scan result IE ABI changed");
_Static_assert(offsetof(struct wifi_demo_scan_res, beacon_ie) == 88U,
               "fc80211 scan result beacon IE ABI changed");

static void wifi_demo_free_driver_event(ra6wx_drv_msg_buf_t *msg)
{
    if (msg == NULL)
    {
        return;
    }

    if (msg->data != NULL)
    {
        vPortFree(msg->data);
        msg->data = NULL;
    }

    vPortFree(msg);
}

static bool wifi_demo_event_matches(ra6wx_drv_msg_buf_t *msg)
{
    if ((msg == NULL) || (msg->data == NULL))
    {
        return true;
    }

    /* The raw scan-done helper is called with only the registered device in
     * this archive, so its event attribute does not reliably carry if_index.
     * There is only one scan interface in this standalone demo; identify the
     * terminal event by command and avoid dropping a valid completion. */
    if ((msg->cmd == FC80211_CMD_NEW_SCAN_RESULTS) ||
        (msg->cmd == FC80211_CMD_SCAN_ABORTED) ||
        (msg->cmd == FC80211_CMD_CONNECT) ||
        (msg->cmd == FC80211_CMD_DISCONNECT))
    {
        return true;
    }

    return msg->data->attr.softmac_idx == (u16) WIFI_DEMO_IF_INDEX;
}

static void wifi_demo_handle_driver_event(ra6wx_drv_msg_buf_t *msg)
{
    if ((msg == NULL) || !wifi_demo_event_matches(msg))
    {
        return;
    }

    if (msg->cmd == FC80211_CMD_NEW_SCAN_RESULTS)
    {
        s_wifi_demo.scan_done = true;
    }
    else if (msg->cmd == FC80211_CMD_SCAN_ABORTED)
    {
        s_wifi_demo.scan_aborted = true;
        s_wifi_demo.scan_done = true;
    }
    else if (msg->cmd == FC80211_CMD_CONNECT)
    {
        int status = 0;
        int attr_status = -1;
        int response_status = -1;

        if (msg->data != NULL)
        {
            attr_status = (int) msg->data->attr.status;
            status = attr_status;

            /* CONNECT events normally contain this response object.  The
             * attribute status is retained as a fallback for older firmware
             * builds that only fill the event header. */
            if (msg->data->data_len >=
                (int) offsetof(struct cfg80211_connect_resp_params, payload))
            {
                struct cfg80211_connect_resp_params *response =
                    (struct cfg80211_connect_resp_params *) msg->data->data;

                response_status = response->status;
                status = response_status;
                memcpy(s_wifi_demo.connect_bssid, response->bssid,
                       sizeof(s_wifi_demo.connect_bssid));
                rt_kprintf("[wifi-demo] connect response: BSSID=");
                wifi_demo_print_bssid(response->bssid);
                rt_kprintf(" req_ie_len=%lu resp_ie_len=%lu "
                           "timeout_reason=%d\n",
                           (unsigned long) response->req_ie_len,
                           (unsigned long) response->resp_ie_len,
                           (int) response->timeout_reason);
            }

            s_wifi_demo.connect_timed_out =
                (msg->data->attr.timed_out != 0U);
        }

        s_wifi_demo.connect_status = status;
        s_wifi_demo.connected = (status == WIFI_DEMO_STATUS_SUCCESS);
        rt_kprintf("[wifi-demo] connect event: attr_status=%d "
                   "response_status=%d data_len=%d timed_out=%u\n",
                   attr_status, response_status,
                   msg->data != NULL ? msg->data->data_len : -1,
                   msg->data != NULL ? msg->data->attr.timed_out : 0U);
        if (status == WIFI_DEMO_STATUS_UNSPECIFIED_FAILURE)
        {
            rt_kprintf("[wifi-demo] status 1: unspecified failure; verify "
                       "that the BSSID and frequency belong to this AP\n");
        }
        else if (status == WIFI_DEMO_STATUS_NOT_SUPPORTED_AUTH_ALG)
        {
            rt_kprintf("[wifi-demo] status 13: AP rejected the "
                       "802.11 authentication algorithm\n");
        }
        else if (status == WIFI_DEMO_STATUS_AUTH_TIMEOUT)
        {
            rt_kprintf("[wifi-demo] status 16: authentication timed out; "
                       "check channel/BSSID and AP response\n");
        }
        if (s_wifi_demo.connect_active)
        {
            s_wifi_demo.connect_done = true;
        }
    }
    else if (msg->cmd == FC80211_CMD_DISCONNECT)
    {
        s_wifi_demo.connected = false;
        s_wifi_demo.disconnect_done = true;
    }
}

static void wifi_demo_pump_events(void)
{
    if (TO_SUPP_QUEUE == NULL)
    {
        return;
    }

#ifdef CONFIG_SUPP_EVENT_DEPRECATED
    {
        ULONG queue_item[2];

        while (xQueueReceive(TO_SUPP_QUEUE, queue_item, 0) == pdTRUE)
        {
            if ((queue_item[0] == RA6WX_SP_DRV_FLAG) &&
                (queue_item[1] != 0U))
            {
                ra6wx_drv_msg_buf_t *msg =
                    (ra6wx_drv_msg_buf_t *)(uintptr_t) queue_item[1];

                wifi_demo_handle_driver_event(msg);

                wifi_demo_free_driver_event(msg);
            }
        }
    }
#else
    {
        ULONG queue_item[2];

        /* The current ABI queues a pointer to a two-word event packet. */
        while (xQueueReceive(TO_SUPP_QUEUE, queue_item, 0) == pdTRUE)
        {
            ULONG *packet = (ULONG *)(uintptr_t) queue_item[0];

            if (packet == NULL)
            {
                continue;
            }

            if ((packet[0] == RA6WX_SP_DRV_FLAG) && (packet[1] != 0U))
            {
                ra6wx_drv_msg_buf_t *msg =
                    (ra6wx_drv_msg_buf_t *)(uintptr_t) packet[1];

                wifi_demo_handle_driver_event(msg);

                /* packet aliases the driver message in this ABI. */
                wifi_demo_free_driver_event(msg);
            }
            else
            {
                /* No supplicant dispatcher exists in this demo. */
                vPortFree(packet);
            }
        }
    }
#endif
}

static int wifi_demo_wait_wiphy(void)
{
    unsigned int waited_ms;

    for (waited_ms = 0U; waited_ms < WIFI_DEMO_WIPHY_WAIT_MS;
         waited_ms += WIFI_DEMO_POLL_MS)
    {
        if ((rwnx_wiphy.rdev != NULL) &&
            ((rwnx_wiphy.bands[FC80211_BAND_2GHZ] != NULL) ||
             (rwnx_wiphy.bands[FC80211_BAND_5GHZ] != NULL)))
        {
            return RT_EOK;
        }

        rt_thread_mdelay(WIFI_DEMO_POLL_MS);
    }

    rt_kprintf("[wifi-demo] wiphy timeout: rdev=%p band2=%p band5=%p\n",
               rwnx_wiphy.rdev,
               rwnx_wiphy.bands[FC80211_BAND_2GHZ],
               rwnx_wiphy.bands[FC80211_BAND_5GHZ]);
    return -RT_ETIMEOUT;
}

static unsigned int wifi_demo_count_band_channels(unsigned int band_index)
{
    unsigned int count = 0U;

    struct ieee80211_supported_band *band = rwnx_wiphy.bands[band_index];
    int channel_index;

    if ((band == NULL) || (band->channels == NULL))
    {
        return 0U;
    }

    for (channel_index = 0; channel_index < band->n_channels;
         channel_index++)
    {
        if ((band->channels[channel_index].flags &
             WIFI_DEMO_CHAN_DISABLED) == 0U)
        {
            count++;
        }
    }

    if (count > WIFI_DEMO_MAX_CHANNELS)
    {
        count = WIFI_DEMO_MAX_CHANNELS;
    }

    return count;
}

static unsigned int wifi_demo_count_channels(void)
{
    unsigned int count = 0U;

    count += wifi_demo_count_band_channels(FC80211_BAND_2GHZ);
    count += wifi_demo_count_band_channels(FC80211_BAND_5GHZ);
    if (count > WIFI_DEMO_MAX_CHANNELS)
    {
        count = WIFI_DEMO_MAX_CHANNELS;
    }

    return count;
}

static unsigned int wifi_demo_fill_frequencies(int *freqs,
                                                unsigned int max_channels)
{
    unsigned int count = 0U;
    unsigned int band_index;

    for (band_index = FC80211_BAND_2GHZ;
         band_index <= FC80211_BAND_5GHZ;
         band_index++)
    {
        struct ieee80211_supported_band *band =
            rwnx_wiphy.bands[band_index];
        int channel_index;

        if ((band == NULL) || (band->channels == NULL))
        {
            continue;
        }

        for (channel_index = 0; channel_index < band->n_channels;
             channel_index++)
        {
            struct ieee80211_channel *channel = &band->channels[channel_index];

            if ((channel->flags & WIFI_DEMO_CHAN_DISABLED) != 0U)
            {
                continue;
            }

            if (count == max_channels)
            {
                freqs[count] = 0;
                return count;
            }

            freqs[count++] = (int) channel->center_freq;
        }
    }

    freqs[count] = 0;
    return count;
}

static void wifi_demo_print_ssid(const u8 *ies, size_t ie_len)
{
    size_t offset = 0U;
    bool printed = false;

    while ((ies != NULL) && (offset + 2U <= ie_len))
    {
        u8 element_id = ies[offset];
        u8 element_len = ies[offset + 1U];

        offset += 2U;
        if ((size_t) element_len > ie_len - offset)
        {
            break;
        }

        if (element_id == 0U)
        {
            size_t index;

            for (index = 0U; index < element_len; index++)
            {
                u8 value = ies[offset + index];

                if ((value >= 0x20U) && (value <= 0x7eU))
                {
                    rt_kprintf("%c", value);
                }
                else
                {
                    rt_kprintf(".");
                }
            }

            printed = (element_len != 0U);
            break;
        }

        offset += element_len;
    }

    if (!printed)
    {
        rt_kprintf("<hidden>");
    }
}

static void wifi_demo_free_scan_results(struct wifi_demo_scan_results *results)
{
    size_t index;

    if (results == NULL)
    {
        return;
    }

    for (index = 0U; index < results->num; index++)
    {
        if (results->res != NULL)
        {
            vPortFree(results->res[index]);
        }
    }

    vPortFree(results->res);
    results->res = NULL;
    results->num = 0U;
}

static int wifi_demo_start(void)
{
    BaseType_t task_status;
    unsigned int retry;
    int status;

    if (s_wifi_demo.hardware_ready)
    {
        return RT_EOK;
    }

    status = wifi_hw_platform_prepare();
    if (status != RT_EOK)
    {
        rt_kprintf("[wifi-demo] platform prepare failed: %d\n", status);
        return status;
    }

    if (!s_wifi_demo.event_queue_ready)
    {
        status = ra6wx_eloop_init();
        if (status != 0)
        {
            rt_kprintf("[wifi-demo] event queue init failed: %d\n", status);
            return -RT_ERROR;
        }

        fc80211_enable_drv_event();
        s_wifi_demo.event_queue_ready = true;
    }

    romac4rtos_initialize(true, dg_configPTIMG_HDR_ADDR);
    romac4rtos_config(0, 0);

    /* The scan wrapper filters channels against the regulatory table. */
    {
        char country[] = COUNTRY_CODE_DEFAULT;
        ra6w1_regdb_data_init(country);
    }

    task_status = rwnx_mac_task_initiailize();
    if (task_status != pdPASS)
    {
        rt_kprintf("[wifi-demo] MAC task init failed: %d\n",
                   (int) task_status);
        return -RT_ERROR;
    }

    task_status = rwnx_driver_task_initiailize();
    if (task_status != pdPASS)
    {
        rt_kprintf("[wifi-demo] driver task init failed: %d\n",
                   (int) task_status);
        return -RT_ERROR;
    }

    /* Public cfg80211 operations wait on this vendor-created response queue.
     * The original supplicant startup performs this step indirectly; the
     * standalone demo must initialize it explicitly. */
    if (CFG80211_semaphore == NULL)
    {
        rwnx_cfg80211_initialize();
    }
    if (CFG80211_semaphore == NULL)
    {
        rt_kprintf("[wifi-demo] cfg80211 response queue init failed\n");
        return -RT_ERROR;
    }
    s_wifi_demo.cfg80211_ready = true;
    rt_kprintf("[wifi-demo] cfg80211 response queue ready: %p\n",
               CFG80211_semaphore);

    for (retry = 0U; retry < WIFI_DEMO_STATUS_RETRY; retry++)
    {
        if (rwnx_hw_get_status() == WIFI_DEMO_STATUS_ACTIVE)
        {
            break;
        }

        vTaskDelay(1);
    }

    if (retry == WIFI_DEMO_STATUS_RETRY)
    {
        rt_kprintf("[wifi-demo] driver status=%d, timeout\n",
                   rwnx_hw_get_status());
        return -RT_ETIMEOUT;
    }

    status = wifi_demo_wait_wiphy();
    if (status != RT_EOK)
    {
        return status;
    }

    /* Apply the regulatory channel table already provided by the driver. */
    fc80211_update_channel_info();

    s_wifi_demo.wdev = fc80211_wdev_from_if_idx(WIFI_DEMO_IF_INDEX);
    if (s_wifi_demo.wdev == NULL)
    {
        s_wifi_demo.wdev = rwnx_cfg80211_add_iface(
            WIFI_DEMO_IF_INDEX, WIFI_DEMO_IF_NAME, 0U,
            FC80211_IFTYPE_STATION, NULL);
    }

    if ((s_wifi_demo.wdev == NULL) ||
        (s_wifi_demo.wdev->wiphy != &rwnx_wiphy) ||
        (s_wifi_demo.wdev->if_index != WIFI_DEMO_IF_INDEX))
    {
        rt_kprintf("[wifi-demo] station interface creation failed\n");
        s_wifi_demo.wdev = NULL;
        return -RT_ERROR;
    }

    s_wifi_demo.iface_ready = true;
    s_wifi_demo.hardware_ready = true;
    rt_kprintf("[wifi-demo] initialized: %s, %u channels available "
               "(2G=%u, 5G=%u)\n",
               WIFI_DEMO_IF_NAME,
               wifi_demo_count_channels(),
               wifi_demo_count_band_channels(FC80211_BAND_2GHZ),
               wifi_demo_count_band_channels(FC80211_BAND_5GHZ));
    return RT_EOK;
}

static int wifi_low_scan(int argc, char **argv)
{
    unsigned int channel_count;
    unsigned int waited_ms;
    int frequencies[WIFI_DEMO_MAX_CHANNELS + 1U];
    struct wpa_driver_scan_params scan_params;
    struct wpa_driver_fc80211_data driver_context;
    struct wifi_demo_scan_results results;
    int status;

    (void) argv;

    if (argc != 1)
    {
        rt_kprintf("usage: wifi_low_scan\n");
        return -RT_EINVAL;
    }

    status = wifi_demo_start();
    if (status != RT_EOK)
    {
        return status;
    }

    if (s_wifi_demo.scan_active)
    {
        rt_kprintf("[wifi-demo] scan is already active\n");
        return -RT_EBUSY;
    }

    channel_count = wifi_demo_count_channels();
    if (channel_count == 0U)
    {
        rt_kprintf("[wifi-demo] no enabled channels\n");
        return -RT_ERROR;
    }

    memset(&scan_params, 0, sizeof(scan_params));
    memset(&driver_context, 0, sizeof(driver_context));
    memset(&results, 0, sizeof(results));
    channel_count = wifi_demo_fill_frequencies(frequencies, channel_count);

    /* The vendor wrapper converts the supplicant scan ABI into the firmware
     * request and owns the converted request lifetime. */
    scan_params.ssids[0].ssid = NULL;
    scan_params.ssids[0].ssid_len = 0U;
    scan_params.num_ssids = 1U;
    scan_params.freqs = frequencies;
    driver_context.ifindex = (int) WIFI_DEMO_IF_INDEX;
    driver_context.nlmode = FC80211_IFTYPE_STATION;

    /* Discard a completion left by a previous request before arming this
     * request's state.  Otherwise a late event can complete the new scan. */
    wifi_demo_pump_events();
    s_wifi_demo.scan_active = true;
    s_wifi_demo.scan_done = false;
    s_wifi_demo.scan_aborted = false;

    status = rsdev_cfg80211_scan((unsigned long) WIFI_DEMO_IF_INDEX,
                                  &driver_context, &scan_params);
    if (status != 0)
    {
        s_wifi_demo.scan_active = false;
        rt_kprintf("[wifi-demo] scan request failed: %d\n", status);
        return status;
    }

    for (waited_ms = 0U; waited_ms < WIFI_DEMO_SCAN_WAIT_MS;
         waited_ms += WIFI_DEMO_POLL_MS)
    {
        wifi_demo_pump_events();
        if (s_wifi_demo.scan_done)
        {
            break;
        }

        rt_thread_mdelay(WIFI_DEMO_POLL_MS);
    }

    if (!s_wifi_demo.scan_done)
    {
        status = fc80211_abort_scan(0UL, WIFI_DEMO_IF_INDEX);
        rt_kprintf("[wifi-demo] scan timeout, abort=%d\n", status);

        /* The request can be cleared by the driver just before the terminal
         * event is queued.  Give that event a bounded window to arrive. */
        for (waited_ms = 0U; waited_ms < WIFI_DEMO_SCAN_SETTLE_MS;
             waited_ms += WIFI_DEMO_POLL_MS)
        {
            wifi_demo_pump_events();
            if (s_wifi_demo.scan_done)
            {
                break;
            }

            rt_thread_mdelay(WIFI_DEMO_POLL_MS);
        }

        wifi_demo_pump_events();
        if (!s_wifi_demo.scan_done)
        {
            s_wifi_demo.scan_active = false;
            return -RT_ETIMEOUT;
        }
    }

    s_wifi_demo.scan_active = false;
    if (s_wifi_demo.scan_aborted)
    {
        rt_kprintf("[wifi-demo] scan aborted\n");
        return -RT_ERROR;
    }

    /* The first argument is unused by the archive implementation for this
     * target; interface index is resolved through fc80211_wdev_from_if_idx(). */
    {
        struct
        {
            void *drv;
            struct wifi_demo_scan_results *res;
            unsigned int assoc_freq;
            unsigned int ibss_freq;
            u8 assoc_bssid[ETH_ALEN];
        } scan_arg;

        scan_arg.drv = &driver_context;
        scan_arg.res = &results;
        scan_arg.assoc_freq = 0U;
        scan_arg.ibss_freq = 0U;
        memset(scan_arg.assoc_bssid, 0, sizeof(scan_arg.assoc_bssid));
        status = fc80211_get_scan(0UL, WIFI_DEMO_IF_INDEX, &scan_arg);
    }

    if (status != 0)
    {
        rt_kprintf("[wifi-demo] get scan results failed: %d\n", status);
        wifi_demo_free_scan_results(&results);
        (void) fc80211_free_umac_scan_results(0UL, WIFI_DEMO_IF_INDEX);
        return status;
    }

    rt_kprintf("[wifi-demo] scan complete: %u AP(s)\n",
               (unsigned int) results.num);
    rt_kprintf("No  BSSID              Freq   RSSI   SSID\n");
    for (size_t index = 0U; index < results.num; index++)
    {
        struct wifi_demo_scan_res *result = results.res[index];

        if (result == NULL)
        {
            continue;
        }

        rt_kprintf("%-3u %02X:%02X:%02X:%02X:%02X:%02X %-6d %-6d ",
                   (unsigned int) index,
                   result->bssid[0], result->bssid[1], result->bssid[2],
                   result->bssid[3], result->bssid[4], result->bssid[5],
                   result->freq, result->level);
        wifi_demo_print_ssid(result->ie, result->ie_len);
        rt_kprintf("\n");
    }

    wifi_demo_free_scan_results(&results);
    (void) fc80211_free_umac_scan_results(0UL, WIFI_DEMO_IF_INDEX);
    return RT_EOK;
}

static int wifi_demo_hex_value(char value)
{
    if ((value >= '0') && (value <= '9'))
    {
        return value - '0';
    }

    if ((value >= 'a') && (value <= 'f'))
    {
        return value - 'a' + 10;
    }

    if ((value >= 'A') && (value <= 'F'))
    {
        return value - 'A' + 10;
    }

    return -1;
}

static int wifi_demo_parse_psk(const char *text, u8 *psk)
{
    size_t index;

    if ((text == NULL) || (psk == NULL) || (strlen(text) != 64U))
    {
        return -RT_EINVAL;
    }

    for (index = 0U; index < WIFI_DEMO_PSK_LEN; index++)
    {
        int high = wifi_demo_hex_value(text[index * 2U]);
        int low = wifi_demo_hex_value(text[index * 2U + 1U]);

        if ((high < 0) || (low < 0))
        {
            return -RT_EINVAL;
        }

        psk[index] = (u8) ((high << 4) | low);
    }

    return RT_EOK;
}

static int wifi_demo_prepare_psk(const char *credential, const char *ssid,
                                 u8 *psk)
{
    size_t credential_len;
    int status;

    if ((credential == NULL) || (ssid == NULL) || (psk == NULL))
    {
        return -RT_EINVAL;
    }

    credential_len = strlen(credential);
    if ((credential_len == 64U) &&
        (wifi_demo_parse_psk(credential, psk) == RT_EOK))
    {
        return RT_EOK;
    }

    if ((credential_len < 8U) || (credential_len > 63U))
    {
        rt_kprintf("[wifi-demo] password must be 8..63 bytes, or a 64-digit raw PSK\n");
        return -RT_EINVAL;
    }

    if (!s_wifi_demo.crypto_ready)
    {
        status = wifi_demo_crypto_init();
        if (status == 0)
        {
            return -RT_ERROR;
        }

        s_wifi_demo.crypto_ready = true;
        rt_kprintf("[wifi-demo] CryptoCell runtime ready (%d)\n", status);
    }

    /* WPA-Personal's passphrase form is PBKDF2-HMAC-SHA1(password, SSID,
     * 4096), producing the 32-byte PMK consumed by cfg80211. This is a
     * crypto primitive only; no supplicant state machine is created. */
    status = mbedtls_pkcs5_pbkdf2_hmac_ext(
        MBEDTLS_MD_SHA1,
        (const unsigned char *) credential, credential_len,
        (const unsigned char *) ssid, strlen(ssid),
        4096U, WIFI_DEMO_PSK_LEN, psk);
    if (status != 0)
    {
        memset(psk, 0, WIFI_DEMO_PSK_LEN);
        rt_kprintf("[wifi-demo] PBKDF2 failed: %d\n", status);
        return -RT_ERROR;
    }

    return RT_EOK;
}

static void wifi_demo_print_bssid(const u8 *bssid)
{
    rt_kprintf("%02X:%02X:%02X:%02X:%02X:%02X",
               bssid[0], bssid[1], bssid[2],
               bssid[3], bssid[4], bssid[5]);
}

static int wifi_demo_parse_bssid(const char *text, u8 *bssid)
{
    unsigned int index;

    if ((text == NULL) || (bssid == NULL) || (strlen(text) != 17U))
    {
        return -RT_EINVAL;
    }

    for (index = 0U; index < ETH_ALEN; index++)
    {
        int high;
        int low;

        high = wifi_demo_hex_value(text[index * 3U]);
        low = wifi_demo_hex_value(text[index * 3U + 1U]);
        if ((high < 0) || (low < 0))
        {
            return -RT_EINVAL;
        }

        bssid[index] = (u8) ((high << 4) | low);
        if ((index + 1U < ETH_ALEN) && (text[index * 3U + 2U] != ':'))
        {
            return -RT_EINVAL;
        }
    }

    if (text[17U] != '\0')
    {
        return -RT_EINVAL;
    }

    return RT_EOK;
}

static int wifi_demo_parse_frequency(const char *text, unsigned int *freq)
{
    char *end;
    unsigned long value;

    if ((text == NULL) || (freq == NULL) || (text[0] == '\0'))
    {
        return -RT_EINVAL;
    }

    end = NULL;
    value = strtoul(text, &end, 10);
    if ((*end != '\0') || (value < 2000UL) || (value > 7000UL))
    {
        return -RT_EINVAL;
    }

    *freq = (unsigned int) value;
    return RT_EOK;
}

/* The normal SDK creates this complete context before association.  In
 * particular, rsdev_cfg80211_associate() relies on the driver object having a
 * valid first_bss/capability relationship even though its immediate lookup
 * uses only ifindex. */
static int wifi_demo_prepare_connect_driver(void)
{
    struct wireless_dev *active_wdev;

    if ((s_connect_driver != NULL) && (s_connect_bss != NULL) &&
        (s_connect_global != NULL))
    {
        return RT_EOK;
    }

    if (s_connect_global == NULL)
    {
        s_connect_global = fc80211_global_init();
        if (s_connect_global == NULL)
        {
            rt_kprintf("[wifi-demo] fc80211 global init failed\n");
            return -RT_ENOMEM;
        }
    }

    s_connect_bss = (struct i802_bss *)
        wpa_driver_fc80211_init(NULL, WIFI_DEMO_IF_NAME, s_connect_global);
    if ((s_connect_bss == NULL) || (s_connect_bss->drv == NULL))
    {
        rt_kprintf("[wifi-demo] fc80211 driver context init failed\n");
        s_connect_bss = NULL;
        s_connect_driver = NULL;
        return -RT_ERROR;
    }

    s_connect_driver = s_connect_bss->drv;
    active_wdev = fc80211_wdev_from_if_idx(WIFI_DEMO_IF_INDEX);
    if ((active_wdev == NULL) ||
        (active_wdev->wiphy != &rwnx_wiphy) ||
        (s_connect_bss->ifindex != (int) WIFI_DEMO_IF_INDEX) ||
        (s_connect_driver->ifindex != (int) WIFI_DEMO_IF_INDEX) ||
        (s_connect_driver->first_bss != s_connect_bss))
    {
        rt_kprintf("[wifi-demo] invalid connect context: wdev=%p bss=%p "
                   "drv=%p first_bss=%p ifindex=%d/%d\n",
                   active_wdev, s_connect_bss, s_connect_driver,
                   s_connect_driver->first_bss,
                   s_connect_bss->ifindex, s_connect_driver->ifindex);
        return -RT_ERROR;
    }

    s_wifi_demo.wdev = active_wdev;
    rt_kprintf("[wifi-demo] connect context ready: global=%p bss=%p "
               "drv=%p wdev_id=%lu capa=0x%08x\n",
               s_connect_global, s_connect_bss, s_connect_driver,
               (unsigned long) s_connect_bss->wdev_id,
               (unsigned int) s_connect_driver->capa.flags);
    return RT_EOK;
}

static int wifi_demo_connect_request(const char *ssid, const u8 *psk,
                                     bool secure, unsigned int freq,
                                     const u8 *bssid)
{
    size_t ssid_len;
    unsigned int waited_ms;
    int request_status;
    int status;

    if (ssid == NULL)
    {
        return -RT_EINVAL;
    }

    ssid_len = strlen(ssid);
    if ((ssid_len == 0U) || (ssid_len > WIFI_DEMO_MAX_SSID_LEN))
    {
        rt_kprintf("[wifi-demo] SSID length must be 1..32 bytes\n");
        return -RT_EINVAL;
    }

    if (secure && (psk == NULL))
    {
        return -RT_EINVAL;
    }

    if ((freq != 0U) != (bssid != NULL))
    {
        rt_kprintf("[wifi-demo] freq and BSSID must be supplied together\n");
        return -RT_EINVAL;
    }

    status = wifi_demo_start();
    if (status != RT_EOK)
    {
        return status;
    }

    if (s_wifi_demo.scan_active || s_wifi_demo.connect_active)
    {
        rt_kprintf("[wifi-demo] another Wi-Fi operation is active\n");
        return -RT_EBUSY;
    }

    if (s_wifi_demo.connected)
    {
        rt_kprintf("[wifi-demo] already connected; run wifi_low_disconnect first\n");
        return -RT_EBUSY;
    }

    status = wifi_demo_prepare_connect_driver();
    if (status != RT_EOK)
    {
        return status;
    }

    /* Copy command-line data into storage that outlives this msh command.
     * The converted cfg80211 request is asynchronous after rsdev returns. */
    memcpy(s_connect_ssid, ssid, ssid_len);
    s_connect_ssid[ssid_len] = '\0';
    if (secure)
    {
        memcpy(s_connect_psk, psk, sizeof(s_connect_psk));
    }
    else
    {
        memset(s_connect_psk, 0, sizeof(s_connect_psk));
    }

    s_connect_target_freq = freq;
    s_connect_target_bssid_set = (bssid != NULL);
    if (s_connect_target_bssid_set)
    {
        memcpy(s_connect_target_bssid, bssid,
               sizeof(s_connect_target_bssid));
    }
    else
    {
        memset(s_connect_target_bssid, 0,
               sizeof(s_connect_target_bssid));
    }

    memset(&s_connect_assoc, 0, sizeof(s_connect_assoc));

    /* These are the values supplied by wpa_supplicant for a WPA2-PSK
     * infrastructure association.  The vendor conversion maps them to the
     * cfg80211 crypto suite selectors used by rwnx. */
    s_connect_assoc.ssid = (const u8 *) s_connect_ssid;
    s_connect_assoc.ssid_len = ssid_len;
    s_connect_assoc.bssid = s_connect_target_bssid_set ?
        s_connect_target_bssid : NULL;
    s_connect_assoc.freq.freq = (int) s_connect_target_freq;
    s_connect_assoc.wpa_ie = NULL;
    s_connect_assoc.wpa_ie_len = 0U;
    s_connect_assoc.wpa_proto = WPA_PROTO_RSN;
    s_connect_assoc.pairwise_suite = WPA_CIPHER_CCMP;
    s_connect_assoc.group_suite = WPA_CIPHER_CCMP;
    s_connect_assoc.mgmt_group_suite = 0U;
    s_connect_assoc.key_mgmt_suite = WPA_KEY_MGMT_PSK;
    s_connect_assoc.auth_alg = WPA_AUTH_ALG_OPEN;
    s_connect_assoc.mode = IEEE80211_MODE_INFRA;
    s_connect_assoc.mgmt_frame_protection = NO_MGMT_FRAME_PROTECTION;

    if (secure)
    {
        s_connect_assoc.wpa_ie = s_connect_rsn_ie;
        s_connect_assoc.wpa_ie_len = sizeof(s_connect_rsn_ie);
        s_connect_assoc.psk = s_connect_psk;
    }

    wifi_demo_pump_events();
    s_wifi_demo.connect_active = true;
    s_wifi_demo.connect_done = false;
    s_wifi_demo.connect_timed_out = false;
    s_wifi_demo.connect_status = -RT_ETIMEOUT;
    memset(s_wifi_demo.connect_bssid, 0,
           sizeof(s_wifi_demo.connect_bssid));

    /* This is the state update performed by
     * wpa_driver_fc80211_associate() immediately before its rsdev call. */
    fc80211_mark_disconnected(s_connect_driver);
    s_connect_driver->assoc_freq = s_connect_assoc.freq.freq > 0 ?
        (unsigned int) s_connect_assoc.freq.freq : 0U;
    memcpy(s_connect_driver->ssid, s_connect_assoc.ssid,
           s_connect_assoc.ssid_len);
    s_connect_driver->ssid_len = s_connect_assoc.ssid_len;

    rt_kprintf("[wifi-demo] associate target: freq=%u BSSID=",
               s_connect_target_freq);
    if (s_connect_target_bssid_set)
    {
        wifi_demo_print_bssid(s_connect_target_bssid);
    }
    else
    {
        rt_kprintf("auto");
    }
    rt_kprintf("\n");

    request_status = rsdev_cfg80211_associate(s_connect_driver,
                                              &s_connect_assoc);
    rt_kprintf("[wifi-demo] rsdev_cfg80211_associate: ssid=\"%s\" "
               "mode=%s return=%d (completion is asynchronous)\n",
               s_connect_ssid, secure ? "WPA2-PSK" : "OPEN",
               request_status);

    for (waited_ms = 0U; waited_ms < WIFI_DEMO_CONNECT_WAIT_MS;
         waited_ms += WIFI_DEMO_POLL_MS)
    {
        wifi_demo_pump_events();
        if (s_wifi_demo.connect_done)
        {
            break;
        }

        rt_thread_mdelay(WIFI_DEMO_POLL_MS);
    }

    wifi_demo_pump_events();
    s_wifi_demo.connect_active = false;
    if (!s_wifi_demo.connect_done)
    {
        rt_kprintf("[wifi-demo] connect timeout after %u ms\n",
                   WIFI_DEMO_CONNECT_WAIT_MS);
        return -RT_ETIMEOUT;
    }

    if (s_wifi_demo.connected)
    {
        rt_kprintf("[wifi-demo] connected: BSSID=");
        wifi_demo_print_bssid(s_wifi_demo.connect_bssid);
        rt_kprintf("\n");
        return RT_EOK;
    }

    rt_kprintf("[wifi-demo] connect failed: status=%d%s\n",
               s_wifi_demo.connect_status,
               s_wifi_demo.connect_timed_out ? " (timed out)" : "");
    return -RT_ERROR;
}

static int wifi_low_connect(int argc, char **argv)
{
    u8 psk[WIFI_DEMO_PSK_LEN];
    u8 bssid[ETH_ALEN];
    unsigned int freq = 0U;
    const u8 *bssid_arg = NULL;
    int status;

    if ((argc != 3) && (argc != 5))
    {
        rt_kprintf("usage: wifi_low_connect <ssid> <password|psk_hex64> "
                   "[freq_mhz bssid]\n");
        rt_kprintf("  use a normal 8..63 byte password or a 64-digit raw PSK/PMK\n");
        rt_kprintf("  no-scan mode requires both freq_mhz and bssid, e.g. "
                   "wifi_low_connect xiaomi password 2437 12:34:56:78:9A:BC\n");
        return -RT_EINVAL;
    }

    if (argc == 5)
    {
        status = wifi_demo_parse_frequency(argv[3], &freq);
        if (status != RT_EOK)
        {
            rt_kprintf("[wifi-demo] invalid frequency: %s\n", argv[3]);
            return status;
        }

        status = wifi_demo_parse_bssid(argv[4], bssid);
        if (status != RT_EOK)
        {
            rt_kprintf("[wifi-demo] invalid BSSID: %s\n", argv[4]);
            return status;
        }
        bssid_arg = bssid;
    }

    status = wifi_demo_prepare_psk(argv[2], argv[1], psk);
    if (status != RT_EOK)
    {
        return status;
    }

    return wifi_demo_connect_request(argv[1], psk, true, freq, bssid_arg);
}

static int wifi_low_connect_open(int argc, char **argv)
{
    u8 bssid[ETH_ALEN];
    unsigned int freq = 0U;
    const u8 *bssid_arg = NULL;
    int status;

    if ((argc != 2) && (argc != 4))
    {
        rt_kprintf("usage: wifi_low_connect_open <ssid> [freq_mhz bssid]\n");
        return -RT_EINVAL;
    }

    if (argc == 4)
    {
        status = wifi_demo_parse_frequency(argv[2], &freq);
        if (status != RT_EOK)
        {
            rt_kprintf("[wifi-demo] invalid frequency: %s\n", argv[2]);
            return status;
        }

        status = wifi_demo_parse_bssid(argv[3], bssid);
        if (status != RT_EOK)
        {
            rt_kprintf("[wifi-demo] invalid BSSID: %s\n", argv[3]);
            return status;
        }
        bssid_arg = bssid;
    }

    return wifi_demo_connect_request(argv[1], NULL, false, freq, bssid_arg);
}

static int wifi_low_disconnect(int argc, char **argv)
{
    unsigned int waited_ms;
    int request_status;
    int status;

    (void) argv;

    if (argc != 1)
    {
        rt_kprintf("usage: wifi_low_disconnect\n");
        return -RT_EINVAL;
    }

    status = wifi_demo_start();
    if (status != RT_EOK)
    {
        return status;
    }

    wifi_demo_pump_events();
    s_wifi_demo.disconnect_done = false;
    request_status = rwnx_cfg80211_disconnect(
        (u8) WIFI_DEMO_IF_INDEX, FC80211_IFTYPE_STATION, 3U);
    rt_kprintf("[wifi-demo] disconnect request queued: return=%d\n",
               request_status);

    for (waited_ms = 0U; waited_ms < WIFI_DEMO_DISCONNECT_WAIT_MS;
         waited_ms += WIFI_DEMO_POLL_MS)
    {
        wifi_demo_pump_events();
        if (s_wifi_demo.disconnect_done)
        {
            break;
        }

        rt_thread_mdelay(WIFI_DEMO_POLL_MS);
    }

    wifi_demo_pump_events();
    if (!s_wifi_demo.disconnect_done)
    {
        rt_kprintf("[wifi-demo] disconnect completion timeout\n");
        return -RT_ETIMEOUT;
    }

    rt_kprintf("[wifi-demo] disconnected\n");
    return RT_EOK;
}

static int wifi_low_status(int argc, char **argv)
{
    (void) argv;

    if (argc != 1)
    {
        rt_kprintf("usage: wifi_low_status\n");
        return -RT_EINVAL;
    }

    wifi_demo_pump_events();
    rt_kprintf("[wifi-demo] initialized=%s interface=%s link=%s",
               s_wifi_demo.hardware_ready ? "yes" : "no",
               s_wifi_demo.iface_ready ? WIFI_DEMO_IF_NAME : "none",
               s_wifi_demo.connected ? "connected" : "disconnected");
    if (s_wifi_demo.connected)
    {
        rt_kprintf(" bssid=");
        wifi_demo_print_bssid(s_wifi_demo.connect_bssid);
    }
    rt_kprintf("\n");
    return RT_EOK;
}

static int wifi_low_init(int argc, char **argv)
{
    (void) argv;

    if (argc != 1)
    {
        rt_kprintf("usage: wifi_low_init\n");
        return -RT_EINVAL;
    }

    return wifi_demo_start();
}

void wifi_hw_early_watchdog_freeze(void)
{
    CRG_TOP->SET_FREEZE_REG_b.FRZ_SYS_WDOG = 1;
}

MSH_CMD_EXPORT(wifi_low_init, initialize Wi-Fi through direct cfg80211 ABI);
MSH_CMD_EXPORT(wifi_low_scan, scan Wi-Fi through direct cfg80211 ABI);
MSH_CMD_EXPORT(wifi_low_connect,
               connect to WPA2-PSK AP through direct cfg80211 ABI);
MSH_CMD_EXPORT(wifi_low_connect_open,
               connect to open AP through direct cfg80211 ABI);
MSH_CMD_EXPORT(wifi_low_disconnect,
               disconnect through direct cfg80211 ABI);
MSH_CMD_EXPORT(wifi_low_status, show standalone Wi-Fi link state);
MSH_CMD_EXPORT_ALIAS(wifi_low_init, wifi_demo_init,
                     initialize Wi-Fi through direct cfg80211 ABI);
MSH_CMD_EXPORT_ALIAS(wifi_low_scan, wifi_demo_scan,
                     scan Wi-Fi through direct cfg80211 ABI);
MSH_CMD_EXPORT_ALIAS(wifi_low_connect, wifi_demo_connect,
                     connect WPA2-PSK AP through direct cfg80211 ABI);
MSH_CMD_EXPORT_ALIAS(wifi_low_connect_open, wifi_demo_connect_open,
                     connect open AP through direct cfg80211 ABI);
MSH_CMD_EXPORT_ALIAS(wifi_low_disconnect, wifi_demo_disconnect,
                     disconnect through direct cfg80211 ABI);
MSH_CMD_EXPORT_ALIAS(wifi_low_status, wifi_demo_status,
                     show standalone Wi-Fi link state);

/* Entry points used by the standalone cfg80211 API probe. */
int wifi_fc80211_demo_ensure_init(void)
{
    return wifi_demo_start();
}

struct wireless_dev *wifi_fc80211_demo_get_wdev(void)
{
    return s_wifi_demo.wdev;
}

int wifi_fc80211_demo_scan_once(void)
{
    return wifi_low_scan(1, NULL);
}
