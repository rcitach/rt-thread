/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef _RWNX_CFG_H
#define _RWNX_CFG_H

#include "rwnx_mac_common.h"
#include "rs_ieee802_11.h"
#include "rwnx_dev.h"
#include "fc80211_copy.h"
#include "rs_util.h"
#include "rwnx_cdlist.h"
#include "rwnx_cfg_if.h"
#include "rwnx_cfg_ap.h"


#define IEEE80211_HE_PPE_THRES_MAX_LEN          25
#define CFG80211_MAX_WEP_KEYS   4

struct vif_params {
        u32 flags;
        int use_4addr;
        u8 macaddr[ETH_ALEN];
        const u8 *vht_mumimo_groups;
        const u8 *vht_mumimo_follow_addr;
};

/**
 * enum wifi_inform_bss_frame_type - Frame types used when informing BSS data
 * @WIFI_INFORM_BSS_FTYPE_UNKNOWN: Frame type not identified
 * @WIFI_INFORM_BSS_FTYPE_BEACON: Beacon frame used for BSS discovery
 * @WIFI_INFORM_BSS_FTYPE_PROBE_RESPONSE: Probe Response frame used for BSS discovery
 * @WIFI_INFORM_BSS_FTYPE_MAX: Number of valid inform frame types
 */
enum wifi_inform_bss_frame_type {
    WIFI_INFORM_BSS_FTYPE_UNKNOWN = 0,
    WIFI_INFORM_BSS_FTYPE_BEACON,
    WIFI_INFORM_BSS_FTYPE_PROBE_RESPONSE,

    WIFI_INFORM_BSS_FTYPE_MAX,
};

enum ieee80211_privacy {
        IEEE80211_PRIVACY_ON,
        IEEE80211_PRIVACY_OFF,
        IEEE80211_PRIVACY_ANY
};

struct ieee80211_sta_he_cap {
        bool has_he;
        struct ieee80211_he_cap_elem he_cap_elem;
        struct ieee80211_he_mcs_nss_supp he_mcs_nss_supp;
        u8 ppe_thres[IEEE80211_HE_PPE_THRES_MAX_LEN];
};


struct ieee80211_sband_iftype_data {
        u16 types_mask;
        struct ieee80211_sta_he_cap he_cap;
        struct ieee80211_he_6ghz_capa he_6ghz_capa;
};

struct ieee80211_sta_s1g_cap {
        bool s1g;
        u8 cap[10]; /* use S1G_CAPAB_ */
        u8 nss_mcs[5];
};

struct ieee80211_sta_vht_cap {
        bool vht_supported;
        u32 cap; /* use IEEE80211_VHT_CAP_ */
        struct ieee80211_vht_mcs_info vht_mcs;
};


struct ieee80211_sta_ht_cap {
        u16 cap; /* use IEEE80211_HT_CAP_ */
        bool ht_supported;
        u8 ampdu_factor;
        u8 ampdu_density;
        struct ieee80211_mcs_info mcs;
};

struct ieee80211_rate {
        u32 flags;
        u16 bitrate;
        u16 hw_value, hw_value_short;
};

struct ieee80211_supported_band {
        struct ieee80211_channel *channels;
        struct ieee80211_rate *bitrates;
        enum fc80211_band band;
        int n_channels;
        int n_bitrates;
        struct ieee80211_sta_ht_cap ht_cap;
        struct ieee80211_sta_vht_cap vht_cap;
        struct ieee80211_sta_s1g_cap s1g_cap;
        struct ieee80211_edmg edmg_cap;
        u16 n_iftype_data;
        const struct ieee80211_sband_iftype_data *iftype_data;
};

struct ieee80211_iface_limit {
        u16 max;
        u16 types;
};

struct ieee80211_iface_combination {
        const struct ieee80211_iface_limit *limits;
        u32 num_different_channels;
        u16 max_interfaces;
        u8 n_limits;
        bool beacon_int_infra_match;
        u8 radar_detect_widths;
        u8 radar_detect_regions;
        u32 beacon_int_min_gcd;
};

struct ieee80211_txrx_stypes {
        u16 tx, rx;
};

struct regulatory_request;
enum wifi_signal_type {
     WIFI_SIGNAL_NONE,
     WIFI_SIGNAL_DBM,
     WIFI_SIGNAL_UNSPEC,

     WIFI_SIGNAL_MAX,
};

struct wiphy {
    bool rwnx_wiphy_dbg_en;
    u8 rwnx_wiphy_dbg_lvl;

    /* === Identification and capabilities === */
    u8 perm_addr[ETH_ALEN];

    /* === Scan configuration === */
    bool rwnx_debug_scan_en;
    u8 max_scan_ssids;
    u8 max_match_sets;
    u16 rwnx_scan_ie_length_limit;
    int bss_context_size;

    enum wifi_signal_type signal_type;
    u16 interface_modes;
    u32 flags;
    u32 features;

    /* === Interface combinations & supported features === */
    const struct ieee80211_iface_combination *if_combo;
    int n_if_combos;
    const struct ieee80211_txrx_stypes *rwnx_mgmt_frames;
    u8 rwnx_ext_features[NUM_FC80211_EXT_FEATURES];

    /* === Band and cipher capabilities === */
    struct ieee80211_supported_band *bands[NUM_FC80211_BANDS];
    int n_cipher_suites;
    const u32 *cipher_suites;

    /* === Transmission and retry parameters === */
    u8 rwnx_short_retry_limit;
    u8 rwnx_long_retry_limit;
    u32 rwnx_fragmentation_threshold;
    u32 rwnx_rts_trigger_threshold;

    /* === Channel and timing === */
    u16 rwnx_duration_max;

    /* === Extended capabilities === */
    const u8 *rwnx_ext_capa;
    const u8 *rwnx_ext_capa_mask;
    u8 rwnx_ext_capa_len;

    /* === Device relationships === */
    struct cfg80211_registered_device *rdev;
    struct wireless_dev *wdev;
    struct wireless_dev *pwdev;

    /* === Multi-BSSID and coexistence support === */
    u8 rwnx_max_csa_count;
    u8 rwnx_mbssid_support : 1;
    u8 rwnx_he_only_mbssid_support : 1;
    u8 rwnx_max_adjacent_rssi_correction;

    /* === Misc configuration === */
    bool rwnx_enable_bridge;
    u32  reserved;
};

bool rwnx_cfg_handle_spurious_frame(int ifindex,
                                    const u8 *addr, gfp_t gfp);
void rwnx_cfg_notify_ready_on_channel(struct wireless_dev *wdev, u64 cookie,
                                      struct ieee80211_channel *chan,
                                      unsigned int duration, gfp_t gfp);
void rwnx_cfg_notify_roc_expired(struct wireless_dev *wdev, u64 cookie,
                                 struct ieee80211_channel *chan,
                                 gfp_t gfp);
void rwnx_cfg_notify_cqm_rssi_event(int ifindex, const u8 *peer,
                                    enum fc80211_cqm_rssi_threshold_event rssi_event,
                                    s32 rssi_level,gfp_t gfp);
void rwnx_cfg_pktloss_notify(int ifindex,
                             const u8 *peer, u32 num_packets, gfp_t gfp);
void rwnx_cfg_unref_bss(struct wiphy *wiphy, struct cfg80211_bss *bss);

void rwnx_cfg_tkip_mic_failure(int ifindex, const u8 *addr,
                               enum fc80211_key_type key_type, int key_id,
                               const u8 *tsc, gfp_t gfp);
void rwnx_cfg_disconnect_event(int ifindex, u16 reason,
                               const u8 *ie, unsigned long ie_len,
                               bool locally_generated, gfp_t gfp);
void rwnx_cfg_setup_chandef(struct cfg80211_chan_def *chandef,
                            struct ieee80211_channel *channel,
                            enum fc80211_channel_type chantype);
void rwnx_cfg_roam_event(int ifindex, struct cfg80211_roam_info *info,
                         gfp_t gfp);

void rwnx_cfg_notify_connect_result(int ifindex, const u8 *bssid,
                                    const u8 *req_ie, unsigned long req_ie_len,
                                    const u8 *resp_ie, unsigned long resp_ie_len,
                                    u16 status, gfp_t gfp);
int rwnx_cfg_external_auth(int ifindex,
                           struct cfg80211_external_auth_params *params,
                           gfp_t gfp);
void rwnx_cfg_probe_client_status(int ifindex, const u8 *addr,
                                  u64 cookie, bool acked, s32 ack_signal,
                                  bool is_valid_ack_signal, gfp_t gfp);
void rwnx_cfg_scan_done(struct cfg80211_scan_request *request, bool aborted);
void rwnx_cfg_radar_event(struct wiphy *wiphy,
                          struct cfg80211_chan_def *chandef, gfp_t gfp);
void rwnx_cfg_cac_event(int ifindex,
                        const struct cfg80211_chan_def *chandef,
                        enum fc80211_radar_event event, gfp_t gfp);

const u8 *rwnx_cfg_search_ie(u8 eid, const u8 *ies, int len);
struct cfg80211_bss *
rwnx_cfg_report_bss_width_frame(struct wiphy *wiphy,
                                struct ieee80211_channel *rx_channel,
                                enum fc80211_bss_scan_width scan_width,
                                struct ieee80211_mgmt *mgmt, size_t len,
                                s32 signal, gfp_t gfp);
void rwnx_cfg_channel_switch_begin_notify(int ifindex,
                                           struct cfg80211_chan_def *chandef,
                                           u8 count, bool quiet);
void rwnx_cfg_channel_switch_notify(int ifindex,
                                    struct cfg80211_chan_def *chandef);
void rwnx_cfg_mgmt_tx_cfm_status(struct wireless_dev *wdev, u64 cookie,
                                 const u8 *buf, unsigned long len, bool ack, gfp_t gfp);
void rwnx_cfg_fast_transistion_event(int ifindex,
                                     struct cfg80211_ft_event_params *ft_event,
                                     int len);
void rwnx_cfg_update_new_peer_candidate(int ifindex,
                                        const u8 *macaddr, const u8 *ie, u8 ie_len,
                                        int sig_dbm, gfp_t gfp);

void rwnx_cfg_report_obss_beacon(struct wireless_dev *wdev, const u8 *frame, unsigned long len, int freq, int sig_dbm);
void rwnx_cfg_rx_unprot_mlme(int ifindex,
                                  const u8 *buf, unsigned int len);
bool rwnx_cfg_rx_mgmt_frame(struct wireless_dev *wdev, int freq, int sig_dbm, const u8 *buf, size_t len, u32 flags);
bool rwnx_cfg_rx_unknown_4addr(int ifindex,
                               const u8 *addr, gfp_t gfp);
void rwnx_cfg_update_dfs_state(struct wiphy *wiphy,
                               const struct cfg80211_chan_def *chandef,
                               enum fc80211_dfs_state dfs_state);
void rwnx_cfg_sched_dfs_update(struct cfg80211_registered_device *rdev);

/* static inline functions */
static inline struct cfg80211_bss * rwnx_cfg_report_bss_frame(struct wiphy *wiphy,
                          struct ieee80211_channel *rx_channel,
                          struct ieee80211_mgmt *mgmt, size_t len,
                          s32 signal, gfp_t gfp)
{
        return rwnx_cfg_report_bss_width_frame(wiphy, rx_channel,
                                               FC80211_BSS_CHAN_WIDTH_20,
                                               mgmt, len, signal, gfp);
}

static inline const struct element *
rwnx_cfg_search_elem(u8 eid, const u8 *ies, int len)
{
        const struct element *elem;
        for_each_element(elem, ies, len) {
                if (elem->id == (eid))
                        return elem;
        }
        return NULL;
}

const struct element *
rwnx_cfg_search_elem_match(u8 eid, const u8 *ies, unsigned int len,
                         const u8 *match, unsigned int match_len,
                         unsigned int match_offset);

static inline const u8 *
rwnx_cfg_search_ie_match(u8 eid, const u8 *ies, unsigned int len,
                       const u8 *match, unsigned int match_len,
                       unsigned int match_offset)
{
        if ((match_len && match_offset < 2) ||
                    (!match_len && match_offset))
                return NULL;

        return (const void *)rwnx_cfg_search_elem_match(eid, ies, len,
                                                        match, match_len,
                                                        match_offset ?
                                                        match_offset - 2 : 0);
}

static inline const u8 *rwnx_cfg_search_ext_ie(u8 ext_eid, const u8 *ies, int len)
{
        return rwnx_cfg_search_ie_match(WLAN_EID_EXTENSION, ies, len,
                                      &ext_eid, 1, 2);
}

static inline const struct element *
rwnx_cfg_search_extended_elem(u8 ext_eid, const u8 *ies, int len)
{
        return rwnx_cfg_search_elem_match(WLAN_EID_EXTENSION, ies, len,
                                        &ext_eid, 1, 0);
}

static inline bool
rwnx_cfg_chandef_identical(const struct cfg80211_chan_def *chandef1,
                           const struct cfg80211_chan_def *chandef2)
{
        return (chandef1->chan == chandef2->chan &&
                chandef1->width == chandef2->width &&
                chandef1->center_freq1 == chandef2->center_freq1 &&
                chandef1->freq1_offset == chandef2->freq1_offset &&
                chandef1->center_freq2 == chandef2->center_freq2);
}

static inline void rwnx_cfg_wiphy_extented_feature_set(struct wiphy *wiphy,
                                                       enum fc80211_ext_feature_index ftidx)
{
        u8 *ft_byte;

        ft_byte = &wiphy->rwnx_ext_features[ftidx / 8];
        *ft_byte |= BIT(ftidx % 8);
}

struct wireless_dev * rwnx_cfg80211_add_iface(u8 if_index       /* 0: for wlan0, 1: for wlan1 */
                            , const char *name
                            , unsigned char name_assign_type
                            , enum fc80211_iftype type          /* may use fc80211_iftype */
                            , struct vif_params *params);

int rwnx_cfg80211_del_iface(u8 if_index, u8 if_type);
int rwnx_cfg80211_change_iface(u8 if_index, u8 if_type, enum fc80211_iftype type, struct vif_params *params);
int rwnx_cfg80211_add_key(u8 if_index, u8 if_type, u8 key_index, bool pairwise, const u8 *mac_addr, struct key_params *params);
int rwnx_cfg80211_get_key(u8 if_index, u8 if_type, u8 key_index, bool pairwise, const u8 *mac_addr,
                          void *cookie, void (*callback)(void *cookie, struct key_params*));
int rwnx_cfg80211_del_key(u8 if_index, u8 if_type, u8 key_index, bool pairwise, const u8 *mac_addr);
int rwnx_cfg80211_set_default_key(u8 if_index, u8 if_type, u8 key_index, bool unicast, bool multicast);
int rwnx_cfg80211_set_default_mgmt_key(u8 if_index, u8 if_type, u8 key_index);
int rwnx_cfg80211_start_ap(u8 if_index, u8 if_type, struct cfg80211_ap_settings *settings);
int rwnx_cfg80211_change_beacon(u8 if_index, u8 if_type, struct cfg80211_beacon_data *info);
int rwnx_cfg80211_stop_ap(u8 if_index, u8 if_type);
int rwnx_cfg80211_add_station(u8 if_index, u8 if_type, const u8 *mac, struct station_parameters *params);
int rwnx_cfg80211_del_station(u8 if_index, u8 if_type, struct station_del_parameters *params);
int rwnx_cfg80211_change_station(u8 if_index, u8 if_type, const u8 *mac, struct station_parameters *params);
int rwnx_cfg80211_get_station(u8 if_index, u8 if_type, const u8 *mac, struct station_info *sinfo);
int rwnx_cfg80211_dump_station(u8 if_index, u8 if_type, int idx, u8 *mac, struct station_info *sinfo);
int rwnx_cfg80211_change_bss(u8 if_index, u8 if_type, struct bss_parameters *params);
int rwnx_cfg80211_set_txq_params(u8 if_index, u8 if_type, struct ieee80211_txq_params *params);
int rwnx_cfg80211_set_monitor_channel(struct cfg80211_chan_def *chandef);
int rwnx_cfg80211_scan(u8 if_index, u8 if_type, struct cfg80211_scan_request *request);
int rwnx_cfg80211_connect(u8 if_index, u8 if_type, struct cfg80211_connect_params *sme);
int rwnx_cfg80211_disconnect(u8 if_index, u8 if_type, u16 reason_code);
int rwnx_cfg80211_set_wiphy_params(u8 if_index, u8 if_type, u32 changed);
int rwnx_cfg80211_set_tx_power(u8 if_index, u8 if_type, enum fc80211_tx_power_setting type, int mbm);
int rwnx_cfg80211_dump_survey(u8 if_index, u8 if_type, int idx, struct survey_info *info);
int rwnx_cfg80211_remain_on_channel(u8 if_index, u8 if_type, struct ieee80211_channel *chan, unsigned int duration, u64 *cookie);
int rwnx_cfg80211_cancel_remain_on_channel(u8 if_index, u8 if_type, u64 cookie);
int rwnx_cfg80211_mgmt_tx(u8 if_index, u8 if_type, struct cfg80211_mgmt_tx_params *params, u64 *cookie);
int rwnx_cfg80211_mgmt_tx_cancel_wait(u8 if_index, u8 if_type, u64 cookie);
int rwnx_cfg80211_set_power_mgmt(u8 if_index, u8 if_type, bool enabled, int timeout);
int rwnx_cfg80211_set_cqm_rssi_config(u8 if_index, u8 if_type, s32 rssi_thold, u32 rssi_hyst);
int rwnx_cfg80211_probe_client(u8 if_index, u8 if_type, const u8 *peer, u64 *cookie);
int rwnx_cfg80211_set_cqm_rssi_config(u8 if_index, u8 if_type, s32 rssi_thold, u32 rssi_hyst);
int rwnx_cfg80211_probe_client(u8 if_index, u8 if_type, const u8 *peer, u64 *cookie);
int rwnx_cfg80211_get_channel(u8 if_index, u8 if_type, struct cfg80211_chan_def *chandef);
int rwnx_cfg80211_update_ft_ies(u8 if_index, u8 if_type, struct cfg80211_update_ft_ies_params *ftie);
int rwnx_cfg80211_channel_switch(u8 if_index, u8 if_type, struct cfg80211_csa_settings *params);
int rwnx_cfg80211_start_radar_detection(u8 if_index, u8 if_type, struct cfg80211_chan_def *chandef, u32 cac_time_ms);
int rwnx_cfg80211_external_auth(u8 if_index, u8 if_type, struct cfg80211_external_auth_params *params);
int rwnx_cfg80211_send_null_data(u8 if_index, u8 if_type,u8 *mac_addr);

#endif /* _RWNX_CFG_H */
