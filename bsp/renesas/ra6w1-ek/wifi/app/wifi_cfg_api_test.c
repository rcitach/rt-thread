/*
 * cfg80211 ABI/API probe for the standalone RA6W1 Wi-Fi port.
 *
 * This command deliberately separates three things which are easy to confuse
 * when working with the vendor archive:
 *
 *   1. link: every symbol declared by rwnx_cfg.h is referenced here;
 *   2. call: a small set of operations is called with a valid idle station;
 *   3. state: operations requiring a connection/AP/async event are reported
 *      as SKIP instead of being called with fake structures.
 *
 * Run:
 *   wifi_cfg_api_test          - link check + safe calls + pure helpers
 *   wifi_cfg_api_test scan     - also run the known-good scan adapter
 *
 * The command does not create a wpa_supplicant context and does not use the
 * WIFI_* or rm_wifi_* service entry points.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <rtthread.h>

#include "co_int.h"

/* Older SDK headers use uint32 while co_int.h exposes uint32_t. */
typedef unsigned long uint32;

#include "rwnx_cfg.h"
#include "rwnx_hw.h"

#define WIFI_CFG_TEST_IF_INDEX  (0U)
#define WIFI_CFG_TEST_IF_TYPE   (FC80211_IFTYPE_STATION)

extern int wifi_fc80211_demo_ensure_init(void);
extern struct wireless_dev *wifi_fc80211_demo_get_wdev(void);
extern int wifi_fc80211_demo_scan_once(void);
extern struct wiphy rwnx_wiphy;

enum wifi_cfg_test_status
{
    WIFI_CFG_TEST_PASS = 0,
    WIFI_CFG_TEST_ERROR,
    WIFI_CFG_TEST_SKIP
};

struct wifi_cfg_test_count
{
    unsigned int called;
    unsigned int passed;
    unsigned int errors;
    unsigned int skipped;
};

static struct wifi_cfg_test_count s_wifi_cfg_test_count;

static void wifi_cfg_test_reset_count(void)
{
    memset(&s_wifi_cfg_test_count, 0, sizeof(s_wifi_cfg_test_count));
}

static void wifi_cfg_test_report_call(const char *name, int ret)
{
    s_wifi_cfg_test_count.called++;
    if (ret == 0)
    {
        s_wifi_cfg_test_count.passed++;
        rt_kprintf("[wifi-cfg-test] PASS  %-38s ret=%d\n", name, ret);
    }
    else
    {
        s_wifi_cfg_test_count.errors++;
        rt_kprintf("[wifi-cfg-test] ERROR %-36s ret=%d\n", name, ret);
    }
}

static void wifi_cfg_test_report_skip(const char *name, const char *reason)
{
    s_wifi_cfg_test_count.skipped++;
    rt_kprintf("[wifi-cfg-test] SKIP  %-38s %s\n", name, reason);
}

static struct ieee80211_channel *wifi_cfg_test_first_channel(void)
{
    unsigned int band_index;

    for (band_index = FC80211_BAND_2GHZ;
         band_index <= FC80211_BAND_5GHZ;
         band_index++)
    {
        struct ieee80211_supported_band *band = rwnx_wiphy.bands[band_index];
        int channel_index;

        if ((band == NULL) || (band->channels == NULL))
        {
            continue;
        }

        for (channel_index = 0; channel_index < band->n_channels;
             channel_index++)
        {
            struct ieee80211_channel *channel = &band->channels[channel_index];

            if ((channel->flags & 1U) == 0U)
            {
                return channel;
            }
        }
    }

    return NULL;
}

static int wifi_cfg_test_change_iface(void)
{
    return rwnx_cfg80211_change_iface(WIFI_CFG_TEST_IF_INDEX,
                                      WIFI_CFG_TEST_IF_TYPE,
                                      WIFI_CFG_TEST_IF_TYPE, NULL);
}

static int wifi_cfg_test_set_wiphy_params(void)
{
    /* changed == 0 is the least invasive capability/configuration probe. */
    return rwnx_cfg80211_set_wiphy_params(WIFI_CFG_TEST_IF_INDEX,
                                          WIFI_CFG_TEST_IF_TYPE, 0U);
}

static int wifi_cfg_test_set_tx_power(void)
{
    return rwnx_cfg80211_set_tx_power(WIFI_CFG_TEST_IF_INDEX,
                                      WIFI_CFG_TEST_IF_TYPE,
                                      FC80211_TX_POWER_AUTOMATIC, 0);
}

static int wifi_cfg_test_set_power_mgmt(void)
{
    return rwnx_cfg80211_set_power_mgmt(WIFI_CFG_TEST_IF_INDEX,
                                        WIFI_CFG_TEST_IF_TYPE, false, 0);
}

static int wifi_cfg_test_get_channel(void)
{
    struct cfg80211_chan_def chandef;

    memset(&chandef, 0, sizeof(chandef));
    return rwnx_cfg80211_get_channel(WIFI_CFG_TEST_IF_INDEX,
                                     WIFI_CFG_TEST_IF_TYPE, &chandef);
}

static int wifi_cfg_test_set_monitor_channel(void)
{
    struct ieee80211_channel *channel = wifi_cfg_test_first_channel();
    struct cfg80211_chan_def chandef;

    if (channel == NULL)
    {
        return -RT_ENOSYS;
    }

    memset(&chandef, 0, sizeof(chandef));
    rwnx_cfg_setup_chandef(&chandef, channel, FC80211_CHAN_HT20);
    return rwnx_cfg80211_set_monitor_channel(&chandef);
}

static int wifi_cfg_test_set_txq_params(void)
{
    struct ieee80211_txq_params params;

    memset(&params, 0, sizeof(params));
    params.ac = FC80211_AC_BE;
    params.cwmin = 15U;
    params.cwmax = 1023U;
    params.aifs = 3U;
    params.txop = 0U;
    return rwnx_cfg80211_set_txq_params(WIFI_CFG_TEST_IF_INDEX,
                                        WIFI_CFG_TEST_IF_TYPE, &params);
}

static int wifi_cfg_test_dump_survey(void)
{
    struct survey_info info;

    memset(&info, 0, sizeof(info));
    return rwnx_cfg80211_dump_survey(WIFI_CFG_TEST_IF_INDEX,
                                     WIFI_CFG_TEST_IF_TYPE, 0, &info);
}

static int wifi_cfg_test_get_station(void)
{
    u8 mac[ETH_ALEN] = {0};
    struct station_info info;

    memset(&info, 0, sizeof(info));
    return rwnx_cfg80211_get_station(WIFI_CFG_TEST_IF_INDEX,
                                     WIFI_CFG_TEST_IF_TYPE, mac, &info);
}

static int wifi_cfg_test_dump_station(void)
{
    u8 mac[ETH_ALEN] = {0};
    struct station_info info;

    memset(&info, 0, sizeof(info));
    return rwnx_cfg80211_dump_station(WIFI_CFG_TEST_IF_INDEX,
                                      WIFI_CFG_TEST_IF_TYPE, 0, mac, &info);
}

static int wifi_cfg_test_set_cqm(void)
{
    return rwnx_cfg80211_set_cqm_rssi_config(WIFI_CFG_TEST_IF_INDEX,
                                             WIFI_CFG_TEST_IF_TYPE,
                                             -127, 0U);
}

static int wifi_cfg_test_cancel_roc(void)
{
    /* Cookie zero must not cancel a real operation. */
    return rwnx_cfg80211_cancel_remain_on_channel(WIFI_CFG_TEST_IF_INDEX,
                                                  WIFI_CFG_TEST_IF_TYPE, 0U);
}

static int wifi_cfg_test_cancel_mgmt_tx(void)
{
    /* Cookie zero must not cancel a real operation. */
    return rwnx_cfg80211_mgmt_tx_cancel_wait(WIFI_CFG_TEST_IF_INDEX,
                                             WIFI_CFG_TEST_IF_TYPE, 0U);
}

static int wifi_cfg_test_search_helpers(void)
{
    static const u8 ies[] = {
        0U, 3U, 'a', 'b', 'c',
        221U, 2U, 0x12U, 0x34U,
        WLAN_EID_EXTENSION, 2U, 0x99U, 0x55U
    };
    static const u8 match[] = {0x12U, 0x34U};
    const u8 *ie;
    const struct element *elem;
    struct cfg80211_chan_def a;
    struct cfg80211_chan_def b;
    struct ieee80211_channel *channel;
    struct wiphy test_wiphy;

    ie = rwnx_cfg_search_ie(0U, ies, (int) sizeof(ies));
    if (ie == NULL)
    {
        return -RT_ERROR;
    }

    elem = rwnx_cfg_search_elem(0U, ies, (int) sizeof(ies));
    if ((elem == NULL) || (elem->id != 0U))
    {
        return -RT_ERROR;
    }

    elem = rwnx_cfg_search_elem_match(221U, ies, sizeof(ies),
                                      match, sizeof(match), 0U);
    if ((elem == NULL) || (elem->id != 221U))
    {
        return -RT_ERROR;
    }

    if ((rwnx_cfg_search_ext_ie(0x99U, ies, sizeof(ies)) == NULL) ||
        (rwnx_cfg_search_extended_elem(0x99U, ies, sizeof(ies)) == NULL))
    {
        return -RT_ERROR;
    }

    channel = wifi_cfg_test_first_channel();
    if (channel == NULL)
    {
        return -RT_ENOSYS;
    }

    rwnx_cfg_setup_chandef(&a, channel, FC80211_CHAN_HT20);
    rwnx_cfg_setup_chandef(&b, channel, FC80211_CHAN_HT20);
    if (!rwnx_cfg_chandef_identical(&a, &b))
    {
        return -RT_ERROR;
    }

    memset(&test_wiphy, 0, sizeof(test_wiphy));
    rwnx_cfg_wiphy_extented_feature_set(
        &test_wiphy, (enum fc80211_ext_feature_index) 0U);
    if ((test_wiphy.rwnx_ext_features[0] & 1U) == 0U)
    {
        return -RT_ERROR;
    }

    return RT_EOK;
}

/*
 * A link anchor for every non-inline declaration in rwnx_cfg.h.  The volatile
 * address load prevents section garbage collection from dropping an otherwise
 * uncalled API. If a prebuilt archive does not export one symbol, the image
 * fails at link time instead of silently omitting the API from this report.
 */
static unsigned int wifi_cfg_test_link_anchor(void)
{
    unsigned int missing = 0U;

#define WIFI_CFG_LINK_CHECK(fn) \
    do { \
        volatile uintptr_t wifi_cfg_link_addr = (uintptr_t) (fn); \
        if (wifi_cfg_link_addr == 0U) { missing++; } \
    } while (0)

    WIFI_CFG_LINK_CHECK(rwnx_cfg_handle_spurious_frame);
    WIFI_CFG_LINK_CHECK(rwnx_cfg_notify_ready_on_channel);
    WIFI_CFG_LINK_CHECK(rwnx_cfg_notify_roc_expired);
    WIFI_CFG_LINK_CHECK(rwnx_cfg_notify_cqm_rssi_event);
    WIFI_CFG_LINK_CHECK(rwnx_cfg_pktloss_notify);
    WIFI_CFG_LINK_CHECK(rwnx_cfg_unref_bss);
    WIFI_CFG_LINK_CHECK(rwnx_cfg_tkip_mic_failure);
    WIFI_CFG_LINK_CHECK(rwnx_cfg_disconnect_event);
    WIFI_CFG_LINK_CHECK(rwnx_cfg_setup_chandef);
    WIFI_CFG_LINK_CHECK(rwnx_cfg_roam_event);
    WIFI_CFG_LINK_CHECK(rwnx_cfg_notify_connect_result);
    WIFI_CFG_LINK_CHECK(rwnx_cfg_external_auth);
    WIFI_CFG_LINK_CHECK(rwnx_cfg_probe_client_status);
    WIFI_CFG_LINK_CHECK(rwnx_cfg_scan_done);
    WIFI_CFG_LINK_CHECK(rwnx_cfg_radar_event);
    WIFI_CFG_LINK_CHECK(rwnx_cfg_cac_event);
    WIFI_CFG_LINK_CHECK(rwnx_cfg_search_ie);
    WIFI_CFG_LINK_CHECK(rwnx_cfg_report_bss_width_frame);
    WIFI_CFG_LINK_CHECK(rwnx_cfg_channel_switch_begin_notify);
    WIFI_CFG_LINK_CHECK(rwnx_cfg_channel_switch_notify);
    WIFI_CFG_LINK_CHECK(rwnx_cfg_mgmt_tx_cfm_status);
    WIFI_CFG_LINK_CHECK(rwnx_cfg_fast_transistion_event);
    WIFI_CFG_LINK_CHECK(rwnx_cfg_update_new_peer_candidate);
    WIFI_CFG_LINK_CHECK(rwnx_cfg_report_obss_beacon);
    WIFI_CFG_LINK_CHECK(rwnx_cfg_rx_unprot_mlme);
    WIFI_CFG_LINK_CHECK(rwnx_cfg_rx_mgmt_frame);
    WIFI_CFG_LINK_CHECK(rwnx_cfg_rx_unknown_4addr);
    WIFI_CFG_LINK_CHECK(rwnx_cfg_update_dfs_state);
    WIFI_CFG_LINK_CHECK(rwnx_cfg_sched_dfs_update);

    WIFI_CFG_LINK_CHECK(rwnx_cfg80211_add_iface);
    WIFI_CFG_LINK_CHECK(rwnx_cfg80211_del_iface);
    WIFI_CFG_LINK_CHECK(rwnx_cfg80211_change_iface);
    WIFI_CFG_LINK_CHECK(rwnx_cfg80211_add_key);
    WIFI_CFG_LINK_CHECK(rwnx_cfg80211_get_key);
    WIFI_CFG_LINK_CHECK(rwnx_cfg80211_del_key);
    WIFI_CFG_LINK_CHECK(rwnx_cfg80211_set_default_key);
    WIFI_CFG_LINK_CHECK(rwnx_cfg80211_set_default_mgmt_key);
    WIFI_CFG_LINK_CHECK(rwnx_cfg80211_start_ap);
    WIFI_CFG_LINK_CHECK(rwnx_cfg80211_change_beacon);
    WIFI_CFG_LINK_CHECK(rwnx_cfg80211_stop_ap);
    WIFI_CFG_LINK_CHECK(rwnx_cfg80211_add_station);
    WIFI_CFG_LINK_CHECK(rwnx_cfg80211_del_station);
    WIFI_CFG_LINK_CHECK(rwnx_cfg80211_change_station);
    WIFI_CFG_LINK_CHECK(rwnx_cfg80211_get_station);
    WIFI_CFG_LINK_CHECK(rwnx_cfg80211_dump_station);
    WIFI_CFG_LINK_CHECK(rwnx_cfg80211_change_bss);
    WIFI_CFG_LINK_CHECK(rwnx_cfg80211_set_txq_params);
    WIFI_CFG_LINK_CHECK(rwnx_cfg80211_set_monitor_channel);
    WIFI_CFG_LINK_CHECK(rwnx_cfg80211_scan);
    WIFI_CFG_LINK_CHECK(rwnx_cfg80211_connect);
    WIFI_CFG_LINK_CHECK(rwnx_cfg80211_disconnect);
    WIFI_CFG_LINK_CHECK(rwnx_cfg80211_set_wiphy_params);
    WIFI_CFG_LINK_CHECK(rwnx_cfg80211_set_tx_power);
    WIFI_CFG_LINK_CHECK(rwnx_cfg80211_dump_survey);
    WIFI_CFG_LINK_CHECK(rwnx_cfg80211_remain_on_channel);
    WIFI_CFG_LINK_CHECK(rwnx_cfg80211_cancel_remain_on_channel);
    WIFI_CFG_LINK_CHECK(rwnx_cfg80211_mgmt_tx);
    WIFI_CFG_LINK_CHECK(rwnx_cfg80211_mgmt_tx_cancel_wait);
    WIFI_CFG_LINK_CHECK(rwnx_cfg80211_set_power_mgmt);
    WIFI_CFG_LINK_CHECK(rwnx_cfg80211_set_cqm_rssi_config);
    WIFI_CFG_LINK_CHECK(rwnx_cfg80211_probe_client);
    WIFI_CFG_LINK_CHECK(rwnx_cfg80211_get_channel);
    WIFI_CFG_LINK_CHECK(rwnx_cfg80211_update_ft_ies);
    WIFI_CFG_LINK_CHECK(rwnx_cfg80211_channel_switch);
    WIFI_CFG_LINK_CHECK(rwnx_cfg80211_start_radar_detection);
    WIFI_CFG_LINK_CHECK(rwnx_cfg80211_external_auth);
    WIFI_CFG_LINK_CHECK(rwnx_cfg80211_send_null_data);

#undef WIFI_CFG_LINK_CHECK
    return missing;
}

static void wifi_cfg_test_report_event_skips(void)
{
    static const char *const event_names[] = {
        "rwnx_cfg_handle_spurious_frame",
        "rwnx_cfg_notify_ready_on_channel",
        "rwnx_cfg_notify_roc_expired",
        "rwnx_cfg_notify_cqm_rssi_event",
        "rwnx_cfg_pktloss_notify",
        "rwnx_cfg_unref_bss",
        "rwnx_cfg_tkip_mic_failure",
        "rwnx_cfg_disconnect_event",
        "rwnx_cfg_roam_event",
        "rwnx_cfg_notify_connect_result",
        "rwnx_cfg_external_auth",
        "rwnx_cfg_probe_client_status",
        "rwnx_cfg_scan_done",
        "rwnx_cfg_radar_event",
        "rwnx_cfg_cac_event",
        "rwnx_cfg_report_bss_frame",
        "rwnx_cfg_report_bss_width_frame",
        "rwnx_cfg_channel_switch_begin_notify",
        "rwnx_cfg_channel_switch_notify",
        "rwnx_cfg_mgmt_tx_cfm_status",
        "rwnx_cfg_fast_transistion_event",
        "rwnx_cfg_update_new_peer_candidate",
        "rwnx_cfg_report_obss_beacon",
        "rwnx_cfg_rx_unprot_mlme",
        "rwnx_cfg_rx_mgmt_frame",
        "rwnx_cfg_rx_unknown_4addr",
        "rwnx_cfg_update_dfs_state",
        "rwnx_cfg_sched_dfs_update"
    };
    size_t index;

    for (index = 0U; index < sizeof(event_names) / sizeof(event_names[0]);
         index++)
    {
        wifi_cfg_test_report_skip(event_names[index],
                                  "driver event ingress; wait for real firmware event");
    }
}

static void wifi_cfg_test_report_state_skips(void)
{
    wifi_cfg_test_report_skip("rwnx_cfg80211_add_iface",
                              "wlan0 already exists; destructive lifecycle test not run");
    wifi_cfg_test_report_skip("rwnx_cfg80211_del_iface",
                              "would remove the test interface");
    wifi_cfg_test_report_skip("rwnx_cfg80211_scan",
                              "current archive consumes private scan ABI; use scan adapter");
    wifi_cfg_test_report_skip("rwnx_cfg80211_connect",
                              "requires real SSID/security and asynchronous result");
    wifi_cfg_test_report_skip("rwnx_cfg80211_disconnect",
                              "would tear down an existing connection");
    wifi_cfg_test_report_skip("rwnx_cfg80211_add_key",
                              "requires negotiated cipher/key material");
    wifi_cfg_test_report_skip("rwnx_cfg80211_get_key",
                              "requires key callback and existing key");
    wifi_cfg_test_report_skip("rwnx_cfg80211_del_key",
                              "requires an existing key and may alter security state");
    wifi_cfg_test_report_skip("rwnx_cfg80211_set_default_key",
                              "requires an existing key");
    wifi_cfg_test_report_skip("rwnx_cfg80211_set_default_mgmt_key",
                              "requires an existing management key");
    wifi_cfg_test_report_skip("rwnx_cfg80211_start_ap",
                              "starts an RF beacon/AP; requires complete settings");
    wifi_cfg_test_report_skip("rwnx_cfg80211_change_beacon",
                              "requires an active AP and valid beacon data");
    wifi_cfg_test_report_skip("rwnx_cfg80211_stop_ap",
                              "AP lifecycle test not requested");
    wifi_cfg_test_report_skip("rwnx_cfg80211_add_station",
                              "requires active AP and valid station capabilities");
    wifi_cfg_test_report_skip("rwnx_cfg80211_del_station",
                              "requires active AP and an associated station");
    wifi_cfg_test_report_skip("rwnx_cfg80211_change_station",
                              "requires active AP and an associated station");
    wifi_cfg_test_report_skip("rwnx_cfg80211_change_bss",
                              "changes BSS operation parameters");
    wifi_cfg_test_report_skip("rwnx_cfg80211_remain_on_channel",
                              "asynchronous channel reservation; cookie/event required");
    wifi_cfg_test_report_skip("rwnx_cfg80211_mgmt_tx",
                              "transmits a management frame; frame/cookie/event required");
    wifi_cfg_test_report_skip("rwnx_cfg80211_probe_client",
                              "sends a peer probe; connected/AP state required");
    wifi_cfg_test_report_skip("rwnx_cfg80211_update_ft_ies",
                              "requires an FT connection and valid IEs");
    wifi_cfg_test_report_skip("rwnx_cfg80211_channel_switch",
                              "changes operating channel and beacon state");
    wifi_cfg_test_report_skip("rwnx_cfg80211_start_radar_detection",
                              "DFS/CAC is regulatory and asynchronous");
    wifi_cfg_test_report_skip("rwnx_cfg80211_external_auth",
                              "requires an external-auth transaction");
    wifi_cfg_test_report_skip("rwnx_cfg80211_send_null_data",
                              "transmits a frame to a peer; valid peer state required");
}

static int wifi_cfg_api_test(int argc, char **argv)
{
    unsigned int missing;
    int status;
    bool run_scan = false;
    struct wireless_dev *wdev;

    if ((argc != 1) && (argc != 2))
    {
        rt_kprintf("usage: wifi_cfg_api_test [scan]\n");
        return -RT_EINVAL;
    }

    if ((argc == 2) && (strcmp(argv[1], "scan") == 0))
    {
        run_scan = true;
    }
    else if (argc == 2)
    {
        rt_kprintf("usage: wifi_cfg_api_test [scan]\n");
        return -RT_EINVAL;
    }

    status = wifi_fc80211_demo_ensure_init();
    if (status != RT_EOK)
    {
        rt_kprintf("[wifi-cfg-test] init failed: %d\n", status);
        return status;
    }

    wdev = wifi_fc80211_demo_get_wdev();
    if ((wdev == NULL) || (wdev->if_index != WIFI_CFG_TEST_IF_INDEX) ||
        (wdev->iftype != WIFI_CFG_TEST_IF_TYPE))
    {
        rt_kprintf("[wifi-cfg-test] invalid station context: wdev=%p\n", wdev);
        return -RT_ERROR;
    }

    wifi_cfg_test_reset_count();
    missing = wifi_cfg_test_link_anchor();
    rt_kprintf("[wifi-cfg-test] link anchor: %s (%u missing)\n",
               (missing == 0U) ? "PASS" : "ERROR", missing);

    status = wifi_cfg_test_search_helpers();
    wifi_cfg_test_report_call("rwnx_cfg_* pure helper set", status);

    wifi_cfg_test_report_call("rwnx_cfg80211_change_iface",
                              wifi_cfg_test_change_iface());
    wifi_cfg_test_report_call("rwnx_cfg80211_set_wiphy_params",
                              wifi_cfg_test_set_wiphy_params());
    wifi_cfg_test_report_call("rwnx_cfg80211_set_tx_power",
                              wifi_cfg_test_set_tx_power());
    wifi_cfg_test_report_call("rwnx_cfg80211_set_power_mgmt",
                              wifi_cfg_test_set_power_mgmt());
    wifi_cfg_test_report_call("rwnx_cfg80211_get_channel",
                              wifi_cfg_test_get_channel());
    wifi_cfg_test_report_call("rwnx_cfg80211_set_monitor_channel",
                              wifi_cfg_test_set_monitor_channel());
    wifi_cfg_test_report_call("rwnx_cfg80211_set_txq_params",
                              wifi_cfg_test_set_txq_params());
    wifi_cfg_test_report_call("rwnx_cfg80211_dump_survey",
                              wifi_cfg_test_dump_survey());
    wifi_cfg_test_report_call("rwnx_cfg80211_get_station",
                              wifi_cfg_test_get_station());
    wifi_cfg_test_report_call("rwnx_cfg80211_dump_station",
                              wifi_cfg_test_dump_station());
    wifi_cfg_test_report_call("rwnx_cfg80211_set_cqm_rssi_config",
                              wifi_cfg_test_set_cqm());
    wifi_cfg_test_report_call("rwnx_cfg80211_cancel_remain_on_channel",
                              wifi_cfg_test_cancel_roc());
    wifi_cfg_test_report_call("rwnx_cfg80211_mgmt_tx_cancel_wait",
                              wifi_cfg_test_cancel_mgmt_tx());

    wifi_cfg_test_report_state_skips();
    wifi_cfg_test_report_event_skips();

    if (run_scan)
    {
        status = wifi_fc80211_demo_scan_once();
        wifi_cfg_test_report_call("scan adapter (rsdev -> rwnx)", status);
    }
    else
    {
        wifi_cfg_test_report_skip("scan adapter (rsdev -> rwnx)",
                                  "run `wifi_cfg_api_test scan` to exercise asynchronous scan");
    }

    rt_kprintf("[wifi-cfg-test] summary: called=%u pass=%u error=%u skip=%u\n",
               s_wifi_cfg_test_count.called,
               s_wifi_cfg_test_count.passed,
               s_wifi_cfg_test_count.errors,
               s_wifi_cfg_test_count.skipped);

    /* An API returning -EOPNOTSUPP/-EINVAL is useful diagnostic data; the
     * command itself completed unless the link anchor or context failed. */
    return (missing == 0U) ? RT_EOK : -RT_ERROR;
}

MSH_CMD_EXPORT(wifi_cfg_api_test,
               probe every rwnx_cfg.h API and report safe/state-dependent calls);
