#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <rtthread.h>

#include "bsp_api.h"
#include "bsp_tcs.h"
#include "co_int.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

/* Older SDK headers use uint32 while co_int.h exposes uint32_t. */
typedef unsigned long uint32;

#include "rwnx_cfg.h"
#include "driver_fc80211.h"
#include "rwnx_hw.h"
#include "romac4rtos.h"
#include "sys_feature.h"

#include "../platform/wifi_hw_prepare.h"

/* These entry points are exported by the prebuilt Wi-Fi archives. */
extern BaseType_t rwnx_mac_task_initiailize(void);
extern BaseType_t rwnx_driver_task_initiailize(void);
extern struct wiphy rwnx_wiphy;
extern void fc80211_enable_drv_event(void);
extern void fc80211_update_channel_info(void);
extern QueueHandle_t TO_SUPP_QUEUE;

#define WIFI_HW_DRIVER_ACTIVE  (1)
#define WIFI_HW_STATUS_RETRIES (1024U)
#define WIFI_HW_SCAN_WAIT_MS       (8000U)
#define WIFI_HW_SCAN_POLL_MS       (20U)
#define WIFI_HW_SCAN_SETTLE_MS     (100U)
#define WIFI_HW_WIPHY_WAIT_MS      (1000U)
#define WIFI_HW_LOW_LEVEL_IFNAME   "wlan0"
#define WIFI_HW_SUPP_IFNAME        "sta0"

/*
 * Keep the same object relationship as the vendor supplicant:
 * global -> driver -> bss, with bss->wdev_id identifying the low-level wdev.
 */
struct wifi_hw_scan_context
{
    struct wireless_dev *wdev;
    struct fc80211_global *global;
    struct wpa_driver_fc80211_data *drv;
    struct i802_bss *bss;
    bool wiphy_ready;
    bool regdb_ready;
    bool event_queue_ready;
    bool scan_in_progress;
    bool scan_abort_pending;
    bool scan_terminal_event;
    bool scan_aborted;
};

static struct wifi_hw_scan_context s_wifi_hw_scan;

static void wifi_hw_scan_prepare_regdb(void);

static void wifi_hw_scan_release_event(ra6wx_drv_msg_buf_t *msg)
{
    bool event_for_context;
    bool is_scan_event;

    if (msg == NULL)
    {
        return;
    }

    is_scan_event = (msg->cmd == FC80211_CMD_TRIGGER_SCAN) ||
                    (msg->cmd == FC80211_CMD_NEW_SCAN_RESULTS) ||
                    (msg->cmd == FC80211_CMD_SCAN_ABORTED);
    event_for_context = is_scan_event &&
                        (msg->data != NULL) &&
                        (s_wifi_hw_scan.global != NULL) &&
                        (s_wifi_hw_scan.bss != NULL) &&
                        (s_wifi_hw_scan.wdev != NULL) &&
                        (msg->data->attr.softmac_idx ==
                         (u16) s_wifi_hw_scan.wdev->if_index);

    if (is_scan_event && (msg->data != NULL) &&
        (s_wifi_hw_scan.global != NULL) &&
        (s_wifi_hw_scan.bss != NULL))
    {
        if (event_for_context && (msg->cmd == FC80211_CMD_SCAN_ABORTED))
        {
            /* process_drv_event() has a fall-through bug for this command. */
            s_wifi_hw_scan.scan_aborted = true;
            s_wifi_hw_scan.scan_terminal_event = true;
        }
        else if (event_for_context &&
                 (msg->cmd == FC80211_CMD_NEW_SCAN_RESULTS))
        {
            s_wifi_hw_scan.scan_terminal_event = true;
        }

        /* There is no wpa_supplicant object behind this standalone context. */
        if ((s_wifi_hw_scan.drv != NULL) &&
            (s_wifi_hw_scan.drv->ctx == NULL))
        {
            s_wifi_hw_scan.drv->scan_for_auth = 1U;
        }

        /* This is the same dispatch path used by ra6wx_eloop_run(). */
        driver_fc80211_process_global_ev(msg);

        if ((msg->cmd == FC80211_CMD_NEW_SCAN_RESULTS) ||
            (msg->cmd == FC80211_CMD_SCAN_ABORTED))
        {
            /* A late abort/completion releases the standalone scan lock. */
            if (event_for_context && s_wifi_hw_scan.scan_abort_pending)
            {
                s_wifi_hw_scan.scan_abort_pending = false;
                s_wifi_hw_scan.scan_in_progress = false;
                if (s_wifi_hw_scan.drv != NULL)
                {
                    s_wifi_hw_scan.drv->scan_for_auth = 0U;
                }
            }
        }
    }
    else if (msg->data != NULL)
    {
        /* The global dispatcher owns and frees event data when it is used. */
        vPortFree(msg->data);
        msg->data = NULL;
    }

    vPortFree(msg);
}

static void wifi_hw_scan_pump_events(void)
{
    if (TO_SUPP_QUEUE == NULL)
    {
        return;
    }

#ifdef CONFIG_SUPP_EVENT_DEPRECATED
    {
        ULONG temp_pack[2];

        while (xQueueReceive(TO_SUPP_QUEUE, temp_pack, 0) == pdTRUE)
        {
            if (temp_pack[0] == RA6WX_SP_DRV_FLAG)
            {
                if (temp_pack[1] != 0U)
                {
                    wifi_hw_scan_release_event(
                        (ra6wx_drv_msg_buf_t *) (uintptr_t) temp_pack[1]);
                }
            }
        }
    }
#else
    {
        ULONG queue_item[2];
        ULONG *temp_pack;

        /* The non-deprecated ABI queues a pointer to an 8-byte packet. For
         * driver events the packet is the message itself and temp[1] repeats
         * that message address. */
        while (xQueueReceive(TO_SUPP_QUEUE, queue_item, 0) == pdTRUE)
        {
            temp_pack = (ULONG *) (uintptr_t) queue_item[0];
            if (temp_pack == NULL)
            {
                continue;
            }

            if (temp_pack[0] == RA6WX_SP_DRV_FLAG)
            {
                if (temp_pack[1] != 0U)
                {
                    wifi_hw_scan_release_event(
                        (ra6wx_drv_msg_buf_t *) (uintptr_t) temp_pack[1]);
                }
                else
                {
                    vPortFree(temp_pack);
                }
            }
            else
            {
                /* Standalone test mode has no supplicant loop for other events. */
                vPortFree(temp_pack);
            }
        }
    }
#endif /* CONFIG_SUPP_EVENT_DEPRECATED */
}

static int wifi_hw_scan_prepare_event_queue(void)
{
    int status;

    if (TO_SUPP_QUEUE == NULL)
    {
        status = ra6wx_eloop_init();
        if (status != 0)
        {
            rt_kprintf("[wifi-hw] supplicant event queue init failed: %d\n",
                       status);
            return -RT_ERROR;
        }
    }

    /* rs80211_send_event() is gated until the supplicant enables driver events. */
    fc80211_enable_drv_event();
    s_wifi_hw_scan.event_queue_ready = true;
    return RT_EOK;
}

/* Called by the BSP reset warm-start hook before the shell is available. */
void wifi_hw_early_watchdog_freeze(void)
{
    CRG_TOP->SET_FREEZE_REG_b.FRZ_SYS_WDOG = 1;
}

static int wifi_hw_up_test(int argc, char **argv)
{
    unsigned int retry;
    BaseType_t status;
    int hw_status;

    (void) argc;
    (void) argv;

    rt_kprintf("[wifi-hw] start hardware bring-up\n");

    status = wifi_hw_platform_prepare();
    if (status != RT_EOK)
    {
        rt_kprintf("[wifi-hw] platform prepare failed: %d\n", status);
        return -RT_ERROR;
    }

    /* This is the reset-time sequence used before the MAC/driver tasks. */
    rt_kprintf("[wifi-hw] initialize ROMAC/PTIM\n");
    romac4rtos_initialize(true, dg_configPTIMG_HDR_ADDR);
    romac4rtos_config(0, 0);
    wifi_hw_scan_prepare_regdb();

    rt_kprintf("[wifi-hw] initialize MAC task\n");
    status = rwnx_mac_task_initiailize();
    if (status != pdPASS)
    {
        rt_kprintf("[wifi-hw] MAC task init failed: %d\n", (int) status);
        return -RT_ERROR;
    }

    rt_kprintf("[wifi-hw] initialize driver task\n");
    status = rwnx_driver_task_initiailize();
    if (status != pdPASS)
    {
        rt_kprintf("[wifi-hw] driver task init failed: %d\n", (int) status);
        return -RT_ERROR;
    }

    for (retry = 0; retry < WIFI_HW_STATUS_RETRIES; retry++)
    {
        hw_status = rwnx_hw_get_status();
        if (hw_status == WIFI_HW_DRIVER_ACTIVE)
        {
            rt_kprintf("[wifi-hw] PASS: driver active after %u ticks\n", retry);
            return RT_EOK;
        }

        vTaskDelay(1);
    }

    rt_kprintf("[wifi-hw] FAIL: status=%d after %u ticks\n",
               rwnx_hw_get_status(), WIFI_HW_STATUS_RETRIES);
    return -RT_ERROR;
}
MSH_CMD_EXPORT(wifi_hw_up_test, bring up Wi-Fi hardware and check DRIVER_ACTIVE);

static int wifi_hw_get_mac(int argc, char **argv)
{
    uint32_t mac_words[2] = {0U, 0U};
    uint8_t mac[6];

    (void) argv;

    if (argc != 1)
    {
        rt_kprintf("usage: wifi_hw_get_mac\n");
        return -RT_ERROR;
    }

    /* This is the same OTP source used by the Renesas Wi-Fi MAC resolver. */
    if (!bsp_tcs_otp_read_mac(mac_words))
    {
        rt_kprintf("[wifi-hw] failed to read MAC from OTP\n");
        return -RT_ERROR;
    }

    /* mac_words[0] is the low word and mac_words[1] is the high word. */
    mac[0] = (uint8_t) ((mac_words[1] >> 8) & 0xffU);
    mac[1] = (uint8_t) (mac_words[1] & 0xffU);
    mac[2] = (uint8_t) ((mac_words[0] >> 24) & 0xffU);
    mac[3] = (uint8_t) ((mac_words[0] >> 16) & 0xffU);
    mac[4] = (uint8_t) ((mac_words[0] >> 8) & 0xffU);
    mac[5] = (uint8_t) (mac_words[0] & 0xffU);

    rt_kprintf("[wifi-hw] OTP MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
               mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return RT_EOK;
}
MSH_CMD_EXPORT(wifi_hw_get_mac, read the permanent Wi-Fi MAC from OTP);

static int wifi_hw_scan_extract_ssid(const uint8_t *ies,
                                     size_t ie_len,
                                     char *ssid,
                                     size_t ssid_size)
{
    size_t offset = 0U;

    if ((ssid == NULL) || (ssid_size == 0U))
    {
        return 0;
    }

    ssid[0] = '\0';

    if ((ies == NULL) || (ie_len == 0U))
    {
        return 0;
    }

    while ((offset + 2U) <= ie_len)
    {
        uint8_t element_id = ies[offset];
        uint8_t element_len = ies[offset + 1U];

        offset += 2U;
        if (((size_t) element_len) > (ie_len - offset))
        {
            break;
        }

        if (element_id == 0U)
        {
            size_t copy_len = element_len;

            if (copy_len >= ssid_size)
            {
                copy_len = ssid_size - 1U;
            }

            memcpy(ssid, &ies[offset], copy_len);
            ssid[copy_len] = '\0';
            return (int) copy_len;
        }

        offset += element_len;
    }

    return 0;
}

static int wifi_hw_scan_wait_wiphy(void)
{
    unsigned int waited_ms;

    /* rwnx_driver_task_main() performs wiphy registration asynchronously. */
    for (waited_ms = 0U; waited_ms < WIFI_HW_WIPHY_WAIT_MS;
         waited_ms += WIFI_HW_SCAN_POLL_MS)
    {
        if ((rwnx_wiphy.rdev != NULL) &&
            ((rwnx_wiphy.bands[FC80211_BAND_2GHZ] != NULL) ||
             (rwnx_wiphy.bands[FC80211_BAND_5GHZ] != NULL)))
        {
            return RT_EOK;
        }

        rt_thread_mdelay(WIFI_HW_SCAN_POLL_MS);
    }

    rt_kprintf("[wifi-hw] wiphy init timeout: rdev=%p band2=%p band5=%p\n",
               rwnx_wiphy.rdev,
               rwnx_wiphy.bands[FC80211_BAND_2GHZ],
               rwnx_wiphy.bands[FC80211_BAND_5GHZ]);
    return -RT_ETIMEOUT;
}

static void wifi_hw_scan_prepare_regdb(void)
{
    char country[4] = {0};

    if (s_wifi_hw_scan.regdb_ready)
    {
        return;
    }

    /* This is the same default-country setup used by net_stack_init.c. */
    strncpy(country, COUNTRY_CODE_DEFAULT, sizeof(country) - 1U);
    ra6w1_regdb_data_init(country);
    s_wifi_hw_scan.regdb_ready = true;
    rt_kprintf("[wifi-hw] regulatory database: %s\n", country);
}

static int wifi_hw_scan_finish(int status)
{
    if (s_wifi_hw_scan.drv != NULL)
    {
        s_wifi_hw_scan.drv->scan_for_auth = 0U;
    }

    s_wifi_hw_scan.scan_in_progress = false;
    s_wifi_hw_scan.scan_abort_pending = false;
    s_wifi_hw_scan.scan_terminal_event = false;
    s_wifi_hw_scan.scan_aborted = false;
    return status;
}

static int wifi_hw_scan_prepare_driver(void)
{
    int status;
    struct wireless_dev *active_wdev;

    if (!s_wifi_hw_scan.wiphy_ready)
    {
        status = wifi_hw_scan_wait_wiphy();
        if (status != RT_EOK)
        {
            return status;
        }

        wifi_hw_scan_prepare_regdb();

        /* Apply the country power table after the task has created the wiphy. */
        rt_kprintf("[wifi-hw] update channel information\n");
        fc80211_update_channel_info();

        if ((rwnx_wiphy.bands[FC80211_BAND_2GHZ] == NULL) &&
            (rwnx_wiphy.bands[FC80211_BAND_5GHZ] == NULL))
        {
            rt_kprintf("[wifi-hw] wiphy has no 2.4/5 GHz band\n");
            return -RT_ERROR;
        }

        s_wifi_hw_scan.wiphy_ready = true;
    }

    if (!s_wifi_hw_scan.event_queue_ready)
    {
        status = wifi_hw_scan_prepare_event_queue();
        if (status != RT_EOK)
        {
            return status;
        }
    }

    if (s_wifi_hw_scan.global == NULL)
    {
        rt_kprintf("[wifi-hw] create fc80211 global context\n");
        s_wifi_hw_scan.global = fc80211_global_init();
        if (s_wifi_hw_scan.global == NULL)
        {
            rt_kprintf("[wifi-hw] fc80211 global init failed\n");
            return -RT_ENOMEM;
        }
    }

    if (s_wifi_hw_scan.wdev == NULL)
    {
        /* Reuse the primary interface if the driver task already created it. */
        s_wifi_hw_scan.wdev = fc80211_wdev_from_if_idx(0);
    }

    if (s_wifi_hw_scan.wdev == NULL)
    {
        rt_kprintf("[wifi-hw] add low-level %s station interface\n",
                   WIFI_HW_LOW_LEVEL_IFNAME);
        s_wifi_hw_scan.wdev = rwnx_cfg80211_add_iface(
            0U, WIFI_HW_LOW_LEVEL_IFNAME, 0U,
            FC80211_IFTYPE_STATION, NULL);
        if (s_wifi_hw_scan.wdev == NULL)
        {
            rt_kprintf("[wifi-hw] add wlan0 failed\n");
            return -RT_ERROR;
        }
    }

    active_wdev = fc80211_wdev_from_if_idx(0);
    if ((active_wdev != s_wifi_hw_scan.wdev) ||
        (s_wifi_hw_scan.wdev->rdev == NULL) ||
        (s_wifi_hw_scan.wdev->wiphy != &rwnx_wiphy) ||
        (s_wifi_hw_scan.wdev->if_index != 0U))
    {
        rt_kprintf("[wifi-hw] wlan0/wiphy relationship is invalid: "
                   "wdev=%p active=%p rdev=%p wiphy=%p if_index=%u\n",
                   s_wifi_hw_scan.wdev,
                   active_wdev,
                   s_wifi_hw_scan.wdev->rdev,
                   s_wifi_hw_scan.wdev->wiphy,
                   (unsigned int) s_wifi_hw_scan.wdev->if_index);
        return -RT_ERROR;
    }

    if (s_wifi_hw_scan.bss == NULL)
    {
        rt_kprintf("[wifi-hw] create fc80211 driver context for %s\n",
                   WIFI_HW_SUPP_IFNAME);
        s_wifi_hw_scan.bss = (struct i802_bss *)
            wpa_driver_fc80211_init(NULL, WIFI_HW_SUPP_IFNAME,
                                     s_wifi_hw_scan.global);
        if ((s_wifi_hw_scan.bss == NULL) ||
            (s_wifi_hw_scan.bss->drv == NULL))
        {
            rt_kprintf("[wifi-hw] fc80211 driver init failed\n");
            s_wifi_hw_scan.bss = NULL;
            s_wifi_hw_scan.drv = NULL;
            return -RT_ERROR;
        }

        s_wifi_hw_scan.drv = s_wifi_hw_scan.bss->drv;
    }

    /* Driver init may refresh the primary vif while it configures station mode. */
    active_wdev = fc80211_wdev_from_if_idx(0);
    if ((s_wifi_hw_scan.drv == NULL) ||
        (active_wdev != s_wifi_hw_scan.wdev) ||
        (s_wifi_hw_scan.drv->global != s_wifi_hw_scan.global) ||
        (s_wifi_hw_scan.drv->first_bss != s_wifi_hw_scan.bss) ||
        (s_wifi_hw_scan.bss->drv != s_wifi_hw_scan.drv) ||
        (s_wifi_hw_scan.bss->ifindex != s_wifi_hw_scan.drv->ifindex) ||
        (s_wifi_hw_scan.bss->ifindex != (int) s_wifi_hw_scan.wdev->if_index))
    {
        rt_kprintf("[wifi-hw] scan context relationship is invalid: "
                   "global=%p drv=%p drv_global=%p first_bss=%p bss=%p "
                   "bss_drv=%p ifindex=%d/%d wdev_if=%u\n",
                   s_wifi_hw_scan.global,
                   s_wifi_hw_scan.drv,
                   (s_wifi_hw_scan.drv != NULL) ?
                       s_wifi_hw_scan.drv->global : NULL,
                   (s_wifi_hw_scan.drv != NULL) ?
                       s_wifi_hw_scan.drv->first_bss : NULL,
                   s_wifi_hw_scan.bss,
                   (s_wifi_hw_scan.bss != NULL) ?
                       s_wifi_hw_scan.bss->drv : NULL,
                   (s_wifi_hw_scan.bss != NULL) ?
                       s_wifi_hw_scan.bss->ifindex : -1,
                   (s_wifi_hw_scan.drv != NULL) ?
                       s_wifi_hw_scan.drv->ifindex : -1,
                   (unsigned int) s_wifi_hw_scan.wdev->if_index);
        return -RT_ERROR;
    }

    rt_kprintf("[wifi-hw] scan context: global=%p drv=%p bss=%p "
               "wdev=%p rdev=%p wiphy=%p wdev_id=%lu ifindex=%d\n",
               s_wifi_hw_scan.global,
               s_wifi_hw_scan.drv,
               s_wifi_hw_scan.bss,
               s_wifi_hw_scan.wdev,
               s_wifi_hw_scan.wdev->rdev,
               s_wifi_hw_scan.wdev->wiphy,
               (unsigned long) s_wifi_hw_scan.bss->wdev_id,
               s_wifi_hw_scan.drv->ifindex);

    /* Discard setup-time messages now that the complete context exists. */
    wifi_hw_scan_pump_events();

    return RT_EOK;
}

static int wifi_hw_scan(int argc, char **argv)
{
    struct wpa_driver_scan_params params;
    struct wpa_scan_results *results;
    size_t index;
    int status;
    unsigned int waited_ms;
    unsigned int settle_ms;

    (void) argv;

    if (argc != 1)
    {
        rt_kprintf("usage: wifi_hw_scan\n");
        return -RT_ERROR;
    }

    if (rwnx_hw_get_status() != WIFI_HW_DRIVER_ACTIVE)
    {
        rt_kprintf("[wifi-hw] driver is not active, run wifi_hw_up_test first\n");
        return -RT_ERROR;
    }

    /* A previous timeout may have left an abort event in the queue. */
    if (s_wifi_hw_scan.event_queue_ready)
    {
        wifi_hw_scan_pump_events();
    }

    if (s_wifi_hw_scan.scan_in_progress)
    {
        rt_kprintf("[wifi-hw] scan is already in progress\n");
        return -RT_ERROR;
    }

    status = wifi_hw_scan_prepare_driver();
    if (status != RT_EOK)
    {
        return status;
    }

    /*
     * rsdev_cfg80211_scan() owns the conversion to cfg80211_scan_request,
     * including channel enumeration, allocation, and async slot ownership.
     * A NULL SSID with num_ssids=1 is the vendor's wildcard active scan.
     */
    memset(&params, 0, sizeof(params));
    params.ssids[0].ssid = NULL;
    params.ssids[0].ssid_len = 0U;
    params.num_ssids = 1U;
    params.freqs = NULL;
    params.only_new_results = 0U;

    wifi_hw_scan_pump_events();
    s_wifi_hw_scan.drv->scan_for_auth = 1U;
    s_wifi_hw_scan.drv->scan_state = NO_SCAN;
    s_wifi_hw_scan.drv->scan_complete_events = 0;
    s_wifi_hw_scan.scan_in_progress = true;
    s_wifi_hw_scan.scan_terminal_event = false;
    s_wifi_hw_scan.scan_aborted = false;

    rt_kprintf("[wifi-hw] request low-level scan: all enabled channels "
               "wait=%u ms\n", WIFI_HW_SCAN_WAIT_MS);
    status = rsdev_cfg80211_scan(s_wifi_hw_scan.bss->wdev_id,
                                  s_wifi_hw_scan.drv,
                                  &params);
    rt_kprintf("[wifi-hw] rsdev_cfg80211_scan returned %d\n", status);

    if (status != 0)
    {
        wifi_hw_scan_pump_events();
        return wifi_hw_scan_finish(-RT_ERROR);
    }

    /* Match wpa_driver_fc80211_scan() after the low-level request succeeds. */
    if (!s_wifi_hw_scan.scan_terminal_event)
    {
        s_wifi_hw_scan.drv->scan_state = SCAN_REQUESTED;
    }

    /* Completion is delivered through the driver event queue, not by polling a private slot. */
    for (waited_ms = 0U;
         waited_ms < WIFI_HW_SCAN_WAIT_MS;
         waited_ms += WIFI_HW_SCAN_POLL_MS)
    {
        wifi_hw_scan_pump_events();
        if (s_wifi_hw_scan.scan_terminal_event)
        {
            break;
        }

        rt_thread_mdelay(WIFI_HW_SCAN_POLL_MS);
    }

    wifi_hw_scan_pump_events();
    if (!s_wifi_hw_scan.scan_terminal_event)
    {
        int abort_status;

        rt_kprintf("[wifi-hw] scan timeout; state=%d, abort request\n",
                   s_wifi_hw_scan.drv->scan_state);
        abort_status = fc80211_abort_scan(s_wifi_hw_scan.bss->wdev_id,
                                          s_wifi_hw_scan.drv->ifindex);
        rt_kprintf("[wifi-hw] fc80211_abort_scan returned %d\n",
                   abort_status);

        for (settle_ms = 0U;
             settle_ms < WIFI_HW_SCAN_SETTLE_MS;
             settle_ms += WIFI_HW_SCAN_POLL_MS)
        {
            wifi_hw_scan_pump_events();
            if (s_wifi_hw_scan.scan_terminal_event)
            {
                break;
            }

            rt_thread_mdelay(WIFI_HW_SCAN_POLL_MS);
        }

        wifi_hw_scan_pump_events();
        if (!s_wifi_hw_scan.scan_terminal_event)
        {
            rt_kprintf("[wifi-hw] abort completion event not received; "
                       "keep context pending\n");
            /* Do not clear scan_for_auth: a late event must remain suppressed. */
            s_wifi_hw_scan.scan_abort_pending = true;
            return -RT_ETIMEOUT;
        }

        return wifi_hw_scan_finish(-RT_ETIMEOUT);
    }

    if (s_wifi_hw_scan.scan_aborted)
    {
        rt_kprintf("[wifi-hw] scan completed as aborted after %u ms\n",
                   waited_ms);
        return wifi_hw_scan_finish(-RT_ERROR);
    }

    rt_kprintf("[wifi-hw] scan completion event after %u ms\n", waited_ms);

    results = fc80211_get_scan_results(s_wifi_hw_scan.drv);
    if (results == NULL)
    {
        rt_kprintf("[wifi-hw] fc80211_get_scan_results returned NULL\n");
        return wifi_hw_scan_finish(-RT_ERROR);
    }

    if ((results->num != 0U) && (results->res == NULL))
    {
        rt_kprintf("[wifi-hw] scan result array is NULL (num=%u)\n",
                   (unsigned int) results->num);
        wpa_scan_results_free(results);
        return wifi_hw_scan_finish(-RT_ERROR);
    }

    rt_kprintf("[wifi-hw] scan results: %u AP(s)\n", (unsigned int) results->num);
    rt_kprintf("%-3s %-17s %-6s %-6s %s\n",
               "No", "BSSID", "Freq", "RSSI", "SSID");

    for (index = 0U; index < results->num; index++)
    {
        struct wpa_scan_res *result = results->res[index];
        char ssid[IEEE80211_MAX_SSID_LEN + 1U];
        int ssid_len;

        if (result == NULL)
        {
            continue;
        }

        ssid_len = wifi_hw_scan_extract_ssid(result->ie,
                                             result->ie_len,
                                             ssid,
                                             sizeof(ssid));
        rt_kprintf("%-3u %02X:%02X:%02X:%02X:%02X:%02X %-6d %-6d ",
                   (unsigned int) index,
                   result->bssid[0], result->bssid[1], result->bssid[2],
                   result->bssid[3], result->bssid[4], result->bssid[5],
                   result->freq,
                   result->level);
        if (ssid_len > 0)
        {
            rt_kprintf("%.*s\n", ssid_len, ssid);
        }
        else
        {
            rt_kprintf("<hidden>\n");
        }
    }

    wpa_scan_results_free(results);
    return wifi_hw_scan_finish(RT_EOK);
}
MSH_CMD_EXPORT(wifi_hw_scan, scan Wi-Fi using the low-level rwnx cfg80211 path);
