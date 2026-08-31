/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef _RWNX_CFG_IF_H
#define _RWNX_CFG_IF_H

#include "rwnx_cfg_common.h"

struct sta_txpwr {
        enum fc80211_tx_power_setting type;
        s16 power;
};

struct station_parameters {
        const u8 *supp_rates;
        u32 sta_modify_mask;
        u32 sta_flags_mask;
        u32 sta_flags_set;
        int listen_interval;
        u16 peer_aid;
        u8 plink_state;
        u8 len_supp_rates;
        u16 aid;
        const struct ieee80211_ht_cap *ht_capa;
        const struct ieee80211_vht_cap *vht_capa;
        u8 max_sp;
        enum fc80211_mesh_power_mode local_pm;
        u8 uapsd_queues;
        u16 capability;
        const u8 *ext_capab;
        const u8 *supported_channels;
        u8 len_supp_channels;
        u8 ext_capab_len;
        const u8 *supported_oper_classes;
        u8 len_supp_oper_classes;
        u8 opmode_notif;
        bool opmode_notif_used;
        int support_p2p_ps;
        const struct ieee80211_he_cap_elem *he_capa;
        u8 len_he_capa;
        const struct ieee80211_he_6ghz_capa *he_6ghz_capa;
        struct sta_txpwr txpwr;
};

struct cfg80211_bss_ies {
        u64 tsf;
        struct rcu_head rcu_head;
        int len;
        bool from_beacon;
        u8 data[];
};

struct cfg80211_bss {
        struct ieee80211_channel *channel;
        enum fc80211_bss_scan_width scan_width;

        const struct cfg80211_bss_ies *ies;
        const struct cfg80211_bss_ies *beacon_ies;
        const struct cfg80211_bss_ies *proberesp_ies;

        struct cfg80211_bss *hidden_beacon_bss;
        struct cfg80211_bss *transmitted_bss;
        struct cdlist_node nontrans_list;

        s32 signal;

        u16 beacon_interval;
        u16 capability;

        u8 bssid[ETH_ALEN];
        u8 chains;
        s8 chain_signal[IEEE80211_MAX_CHAINS];

        u8 bssid_index;
        u8 max_bssid_indicator;

        u8 priv[] __aligned(sizeof(void *));
};

struct cfg80211_roam_info {
        struct ieee80211_channel *channel;
        struct cfg80211_bss *bss;
        const u8 *bssid;
        const u8 *req_ie;
        unsigned long req_ie_len;
        const u8 *resp_ie;
        unsigned long resp_ie_len;
        struct cfg80211_fils_resp_params fils;
};

struct station_del_parameters {
        const u8 mac[ETH_ALEN];
        u8 subtype;
        u16 reason_code;
};

struct cfg80211_bss_select_adjust {
        enum fc80211_band band;
        s8 delta;
};

struct cfg80211_connect_params {
        struct ieee80211_channel *channel;
        struct ieee80211_channel *freq_hint;
        const u8 *bssid;
        const u8 *bssid_hint;
        const u8 *ssid;
        unsigned long ssid_len;
        enum fc80211_auth_type auth_type;
        const u8 *ie;
        unsigned long ie_len;
        bool private;
        enum fc80211_mfp mgmt_frame_protection;
        struct cfg80211_crypto_settings crypto;
        const u8 *key;
        u8 key_len, key_idx;
        u32 flags;
        struct ieee80211_ht_cap ht_capa;
        struct ieee80211_ht_cap ht_capa_mask;
        struct ieee80211_vht_cap vht_capa;
        struct ieee80211_vht_cap vht_capa_mask;
        bool pbss;
        const u8 *prev_bssid;
        const u8 *fils_erp_username;
        unsigned long fils_erp_username_len;
        const u8 *fils_erp_realm;
        unsigned long fils_erp_realm_len;
        u16 fils_erp_next_seq_num;
        const u8 *fils_erp_rrk;
        unsigned long fils_erp_rrk_len;
        bool want_1x;
        struct ieee80211_edmg edmg;
};

struct cfg80211_update_ft_ies_params {
        u16 md;
        const u8 *ie;
        unsigned long ie_len;
};

struct cfg80211_pmksa {
        const u8 *bssid;
        const u8 *pmkid;
        const u8 *pmk;
        unsigned long pmk_len;
        const u8 *ssid;
        unsigned long ssid_len;
        const u8 *cache_id;
        u32 pmk_lifetime;
        u8 pmk_reauth_threshold;
};

struct cfg80211_cqm_config {
        u32 rssi_hyst;
        s32 last_rssi_event_value;
        int n_rssi_thresholds;
        s32 rssi_thresholds[];
};

struct cfg80211_inform_bss {
        struct ieee80211_channel *chan;
        enum fc80211_bss_scan_width scan_width;
        s32 signal;
        u64 boottime_ns;
        u64 parent_tsf;
        u8 parent_bssid[ETH_ALEN] __aligned(2);
        u8 chains;
        s8 chain_signal[IEEE80211_MAX_CHAINS];
};

struct mgmt_frame_regs {
        u32 global_stypes, interface_stypes;
        u32 global_mcast_stypes, interface_mcast_stypes;
};

struct cfg80211_scan_6ghz_params {
        u32 short_ssid;
        u32 channel_idx;
        u8 bssid[ETH_ALEN];
        bool unsolicited_probe;
        bool short_ssid_valid;
        bool psc_no_listen;
};

struct ieee80211_scan_info {
        bool abort_scan;
};

struct ieee80211_txq_params {
        enum fc80211_ac ac;
        u16 txop;
        u16 cwmin;
        u16 cwmax;
        u8 aifs;
};

struct cfg80211_scan_request {
        struct wireless_dev *wdev;
        struct cfg80211_ssid *ssids;
        int n_ssids;
        u32 n_channels;
        enum fc80211_bss_scan_width scan_width;
        const u8 *ie;
        u16 period;
        bool duration_mandatory;
        u32 flags;
        unsigned long ie_len;

        u32 rates[NUM_FC80211_BANDS];

        /* internal */
        struct wiphy *wiphy;
        unsigned long scan_start;
        bool scan_6ghz;
        struct ieee80211_scan_info info;
        bool notified;
        bool no_cck;

        /* keep last */
        struct ieee80211_channel *channels[];
};

struct survey_info {
        struct ieee80211_channel *channel;
        u64 time;
        u64 time_busy;
        u64 time_ext_busy;
        u64 time_rx;
        u64 time_tx;
        u64 time_scan;
        u64 time_bss_rx;
        u32 filled;
        s8 noise;
};

struct cfg80211_mgmt_tx_params {
        struct ieee80211_channel *chan;
        bool offchan;
        unsigned int wait;
        const u8 *buf;
        unsigned long len;
        bool no_cck;
        bool dont_wait_for_ack;
        int n_csa_offsets;
        const u16 *csa_offsets;
};

#endif

