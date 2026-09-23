/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef _RWNX_CFG_AP_H
#define _RWNX_CFG_AP_H

#include "rwnx_cfg_common.h"

struct cfg80211_mbssid_elems {
        u8 cnt;
        struct {
                const u8 *data;
                long len;
        } elem[];
};

struct cfg80211_beacon_data {
        const u8 *head, *tail;
        const u8 *beacon_ies;
        const u8 *proberesp_ies;
        const u8 *assocresp_ies;
        const u8 *probe_resp;
        const u8 *lci;
        const u8 *civicloc;
        struct cfg80211_mbssid_elems *mbssid_ies;
        s8 ftm_responder;

        unsigned long head_len, tail_len;
        unsigned long beacon_ies_len;
        unsigned long proberesp_ies_len;
        unsigned long assocresp_ies_len;
        unsigned long probe_resp_len;
        unsigned long lci_len;
        unsigned long civicloc_len;
};

struct mac_address {
        u8 addr[ETH_ALEN];
};

struct cfg80211_acl_data {
        enum fc80211_acl_policy acl_policy;
        int n_acl_entries;

        /* Keep it last */
        struct mac_address mac_addrs[];
};

struct cfg80211_bitrate_mask {
        struct {
                u32 legacy;
                u8 ht_mcs[IEEE80211_HT_MCS_MASK_LEN];
                u16 vht_mcs[FC80211_VHT_NSS_MAX];
                u16 he_mcs[FC80211_HE_NSS_MAX];
                enum fc80211_txrate_gi gi;
                enum fc80211_he_gi he_gi;
                enum fc80211_he_ltf he_ltf;
        } control[NUM_FC80211_BANDS];
};

struct ieee80211_he_obss_pd {
        bool enable;
        u8 sr_ctrl;
        u8 non_srg_max_offset;
        u8 min_offset;
        u8 max_offset;
        u8 bss_color_bitmap[8];
        u8 partial_bssid_bitmap[8];
};

struct cfg80211_he_bss_color {
        u8 color;
        bool enabled;
        bool partial;
};

struct cfg80211_fils_discovery {
        u32 min_interval;
        u32 max_interval;
        unsigned long tmpl_len;
        const u8 *tmpl;
};

struct cfg80211_ap_settings {

        struct cfg80211_beacon_data beacon;
        struct cfg80211_chan_def chandef;
        struct cfg80211_crypto_settings crypto;

        u8 p2p_ctwindow;
        const u8 *ssid;
        int inactivity_timeout;
        int beacon_interval, dtim_period;
        unsigned long ssid_len;
        enum fc80211_hidden_ssid hidden_ssid;
        enum fc80211_auth_type auth_type;
        bool privacy;
        bool p2p_opp_ps;
        bool pbss;

        u32 flags;
        const struct ieee80211_ht_cap *ht_cap;
        const struct ieee80211_vht_cap *vht_cap;
        const struct ieee80211_he_cap_elem *he_cap;
        bool ht_required, vht_required, he_required, sae_h2e_required;
};

struct bss_parameters {
        u8 basic_rates_len;
        const u8 *basic_rates;
        int ap_isolate;
        int ht_opmode;
        int use_cts_prot;
        int use_short_slot_time;
        int use_short_preamble;
        s8 p2p_ctwindow, p2p_opp_ps;
};

struct cfg80211_csa_settings {
        struct cfg80211_chan_def chandef;
        struct cfg80211_beacon_data beacon_csa;
        struct cfg80211_beacon_data beacon_after;
        u8 count;
        const u16 *counter_offsets_presp;
        const u16 *counter_offsets_beacon;
        unsigned int n_counter_offsets_beacon;
        unsigned int n_counter_offsets_presp;
        bool radar_required;
        bool block_tx;
};

#endif
