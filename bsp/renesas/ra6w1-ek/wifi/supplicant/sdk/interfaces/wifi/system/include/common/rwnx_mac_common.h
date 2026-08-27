/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/**
 ****************************************************************************************
 *
 * @file rwnx_mac_common.h
 *
 * @brief Define for common variables for SDK
 *
 ****************************************************************************************
 */

#ifndef __RWNX_MAC_COMMON_H__
#define	__RWNX_MAC_COMMON_H__

#include "supp_common.h"
#include "fc80211_copy.h"
#include "rwnx_types.h"
//#include "mac_types.h"
#include "rwnx_le_util.h"

struct cfg80211_fils_resp_params {
    const u8 *kek;
    unsigned long kek_len;
    bool update_erp_next_seq_num;
    u16 erp_next_seq_num;
    const u8 *pmk;
    unsigned long pmk_len;
    const u8 *pmkid;
};

struct cfg80211_connect_resp_params {
#if 0 //cfg80211_connect
    int status;
    const u8 *bssid; 
    struct cfg80211_bss *bss;
    const u8 *req_ie;
    unsigned long req_ie_len;
    const u8 *resp_ie;
    unsigned long resp_ie_len;
    struct cfg80211_fils_resp_params fils;
    enum fc80211_timeout_reason timeout_reason;
#else
    int status;
    u8 bssid[ETH_ALEN];
    struct cfg80211_bss *bss;
    const u8 *req_ie;
    unsigned long req_ie_len;
    const u8 *resp_ie;
    unsigned long resp_ie_len;
    struct cfg80211_fils_resp_params fils;
    enum fc80211_timeout_reason timeout_reason;
    uint32 payload[0];
#endif
};

struct cfg80211_pmk_conf {
    const u8 *aa;
    u8 pmk_len;
    const u8 *pmk;
    const u8 *pmk_r0_name;
};

struct key_params {
    const u8 *key;
    const u8 *seq;
    int key_len;
    int seq_len;
    u16 vlan_id;
    u32 cipher;
    enum fc80211_key_mode mode;
};

struct key_parse {
    struct key_params p;
    int idx;
    int type;
    bool def, defmgmt, defbeacon;
    bool def_uni, def_multi;
};

#define IEEE80211_MAX_CHAINS    4

struct rate_info {
    u8 flags;
    u8 mcs;
    u16 legacy;
    u8 nss;
    u8 bw;
    u8 he_gi;
    u8 he_dcm;
    u8 he_ru_alloc;
    u8 n_bonded_ch;
};

struct sta_bss_parameters {
    u8 flags;
    u8 dtim_period;
    u16 beacon_interval;
};

struct station_info {
    // Bitmask indicating which fields are filled
    u64 filled;

    // Time (in seconds) the station has been connected
    u32 connected_time;

    // Time (in milliseconds) since last received frame
    u32 inactive_time;

    // Timestamp of association
    u64 assoc_at;

    // Total received bytes
    u64 rx_bytes;

    // Total transmitted bytes
    u64 tx_bytes;

    // Local Link ID (for mesh)
    u16 llid;

    // Peer Link ID (for mesh)
    u16 plid;

    // Peer link state (for mesh)
    u8 plink_state;

    // Most recent signal strength (dBm)
    s8 signal;

    // Average signal strength (dBm)
    s8 signal_avg;

    // Number of active antenna chains
    u8 chains;

    // Signal strength per antenna chain
    s8 chain_signal[IEEE80211_MAX_CHAINS];

    // Average signal per antenna chain
    s8 chain_signal_avg[IEEE80211_MAX_CHAINS];

    // Current TX rate information
    struct rate_info txrate;

    // Current RX rate information
    struct rate_info rxrate;

    // Total received packets
    u32 rx_packets;

    // Total transmitted packets
    u32 tx_packets;

    // Number of TX retries
    u32 tx_retries;

    // Number of failed TX attempts
    u32 tx_failed;

    // Dropped RX packets due to miscellaneous reasons
    u32 rx_dropped_misc;

    // BSS parameters (e.g., DTIM period)
    struct sta_bss_parameters bss_param;

    // Station flags (e.g., authorized)
    struct fc80211_sta_flag_update sta_flags;

    // Generation number for tracking updates
    int generation;

    // Association request IEs
    const u8 *assoc_req_ies;

    // Length of assoc_req_ies
    unsigned long assoc_req_ies_len;

    // Number of beacon losses
    u32 beacon_loss_count;

    // Time offset between local and peer clocks
    signed long long t_offset;

    // Local mesh power mode
    enum fc80211_mesh_power_mode local_pm;

    // Peer mesh power mode
    enum fc80211_mesh_power_mode peer_pm;

    // Non-peer mesh power mode
    enum fc80211_mesh_power_mode nonpeer_pm;

    // Expected throughput in kbps
    u32 expected_throughput;

    // Total TX airtime in microseconds
    u64 tx_duration;

    // Total RX airtime in microseconds
    u64 rx_duration;

    // Number of received beacons
    u64 rx_beacon;

    // Average signal strength of received beacons
    u8 rx_beacon_signal_avg;

    // Whether connected to a mesh gate
    u8 connected_to_gate;

    // Per-TID statistics
    struct cfg80211_tid_stats *pertid;

    // Signal strength of last ACK
    s8 ack_signal;

    // Average signal strength of ACKs
    s8 avg_ack_signal;

    // Airtime scheduling weight
    u16 airtime_weight;

    // Number of received MPDUs
    u32 rx_mpdu_count;

    // Number of FCS errors
    u32 fcs_err_count;

    // Airtime link metric for mesh routing
    u32 airtime_link_metric;

    // Whether connected to an authentication server
    u8 connected_to_as;
};


#define ENOTSUPP    524 /* Operation is not supported */

enum rate_info_flags {
    RATE_INFO_FLAGS_MCS         = BIT(0),
    RATE_INFO_FLAGS_VHT_MCS         = BIT(1),
    RATE_INFO_FLAGS_SHORT_GI        = BIT(2),
    RATE_INFO_FLAGS_DMG         = BIT(3),
    RATE_INFO_FLAGS_HE_MCS          = BIT(4),
    RATE_INFO_FLAGS_EDMG            = BIT(5),
    RATE_INFO_FLAGS_EXTENDED_SC_DMG     = BIT(6),
};

struct ieee80211_edmg {
    u8 channels;
    uint16_t bw_config;
};

struct ieee80211_channel {
    enum fc80211_band band;
    u32 center_freq;
    u16 freq_offset;
    u16 hw_value;
    u32 flags;
    int max_antenna_gain;
    int max_power;
    int max_reg_power;
    bool beacon_found;
    u32 orig_flags;
    int orig_mag, orig_mpwr;
    enum fc80211_dfs_state dfs_state;
    unsigned long dfs_state_entered;
    unsigned int dfs_cac_ms;
};

struct cfg80211_chan_def {
    struct ieee80211_channel *chan;
    enum fc80211_chan_width width;
    u32 center_freq1;
    u32 center_freq2;
    struct ieee80211_edmg edmg;
    u16 freq1_offset;
    enum fc80211_channel_type chan_type;
};

#define IEEE80211_MAX_SSID_LEN      32


#define IS_FREQ_2P4GHZ(n) (n >= 2412 && n <= 2484)
#define IS_FREQ_5GHZ(n)   (n >= 5180 && n < 5895)


enum ieee80211_bss_type {
    IEEE80211_BSS_TYPE_ESS,
    IEEE80211_BSS_TYPE_PBSS,
    IEEE80211_BSS_TYPE_IBSS,
    IEEE80211_BSS_TYPE_MBSS,
    IEEE80211_BSS_TYPE_ANY
};

struct wireless_dev {
    struct wiphy *wiphy;
    enum fc80211_iftype iftype;
    u8  if_index;

    /* the remainder of this struct should be private to cfg80211 */
    struct cdlist_node mgmt_registrations;

    bool use_4addr, is_running, registered, registering,p2p_started ;

    /* currently used for IBSS and SME - might be rearranged later */
    u8 ssid[IEEE80211_MAX_SSID_LEN];
    u8 ssid_len, mesh_id_len, mesh_id_up_len;
    struct cfg80211_conn *conn;
    struct cfg80211_cached_keys *connect_keys;
    enum ieee80211_bss_type conn_bss_type;
    /** (private) Used by the internal configuration code */
    struct cfg80211_internal_bss *current_bss; /* associated / joined */

    bool ps;
    int ps_timeout;

    bool cac_started;
    unsigned long cac_start_time;
    unsigned int cac_time_ms;

    struct cfg80211_cqm_config *cqm_config;

    struct cfg80211_registered_device *rdev;
};

struct ieee80211_hdr {
    unsigned short __bitwise frame_control;
    unsigned short __bitwise duration_id;
    u8 addr1[ETH_ALEN];
    u8 addr2[ETH_ALEN];
    u8 addr3[ETH_ALEN];
    unsigned short __bitwise seq_ctrl;
    u8 addr4[ETH_ALEN];
} __packed __aligned(2);

struct cfg80211_ssid {
    u8 ssid[IEEE80211_MAX_SSID_LEN];
    u8 ssid_len;
};

struct cfg80211_external_auth_params {
    enum fc80211_external_auth_action action;
    u8 bssid[ETH_ALEN];
    struct cfg80211_ssid ssid;
    unsigned int key_mgmt_suite;
    u16 status;
    const u8 *pmkid;
};

struct cfg80211_cqm_send_params {
    //NL80211_ATTR_WIPHY
    //NL80211_ATTR_IFINDEX
    int ifindex;
    //NL80211_ATTR_MAC
    u8 addr[ETH_ALEN];

    enum fc80211_attr_cqm cqm;
    //cfg80211_cqm_rssi_notify
        //FC80211_ATTR_CQM_RSSI_THRESHOLD_EVENT
        //NL80211_ATTR_CQM_RSSI_LEVEL
    enum fc80211_cqm_rssi_threshold_event rssi_event;
    s32 rssi_level;

    //cfg80211_cqm_txe_notify
        //NL80211_ATTR_CQM_TXE_PKTS
        //NL80211_ATTR_CQM_TXE_RATE
        //NL80211_ATTR_CQM_TXE_INTVL

    //cfg80211_cqm_pktloss_notify
        //FC80211_ATTR_CQM_PKT_LOSS_EVENT
        //num_packets
    u32 num_packets;

    //cfg80211_cqm_beacon_loss_notify
        //FC80211_ATTR_CQM_BEACON_LOSS_EVENT
};

struct cfg80211_probe_status_params {
    u8 addr[ETH_ALEN];
    u64 cookie;
    bool acked;
    s32 ack_signal;
};

struct cfg80211_ft_event_params {
    const u8 *ies;
    unsigned long ies_len;
    const u8 target_ap[ETH_ALEN];
    const u8 *ric_ies;
    unsigned long ric_ies_len;
    uint32 payload[0];
};

struct cfg80211_rada_detect_noti {
    enum fc80211_radar_event event;
    const struct cfg80211_chan_def chandef;
};

#define MAC_ADDR_LEN	6
struct mac_addr
{
    /// Array of 16-bit words that make up the MAC address.
    uint16_t array[MAC_ADDR_LEN / 2];
};
#define TX_MAX_DATA_LENG    200
struct RFTX {
    uint32_t freq; ///< freq channel frequency, if 0 then current setting frequency used.
    uint32_t numFrames; ///
    uint32_t frameLen;
    uint32_t txRate;
    uint32_t txPower;
    struct mac_addr destAddr;
    struct mac_addr bssid;
    uint8_t htEnable;
    uint8_t GI;
    uint8_t greenField;
    uint8_t preambleType;
    uint8_t qosEnable;
    uint8_t ackPolicy;
    uint8_t scrambler;
    uint8_t aifsnval;
    uint8_t ant;
    uint8_t BW;
    uint32_t tx_timeout;
    uint32_t data_length;
    uint8_t data[TX_MAX_DATA_LENG];
    bool high_rate;
};

/**
 ****************************************************************************************
 * @brief Return LMAC version
 *
 * @return version of LMAC
 ****************************************************************************************
 */
char *lmac_cmd_get_version(void);
/**
 ****************************************************************************************
 * @brief Return RF version
 *
 * @return version of RF
 ****************************************************************************************
 */
char *lmac_cmd_get_rf_version(void);
/**
 ****************************************************************************************
 * @brief Return LMAC build date
 *
 * @return  build date of LMAC
 ****************************************************************************************
 */
char *lmac_cmd_get_build_date(void);

/**
 ****************************************************************************************
 * @brief Return LMAC build options
 *
 * @return  build options of LMAC
 ****************************************************************************************
 */
char *lmac_cmd_get_build_options(void);


/**
 ****************************************************************************************
 * @brief Print and set LMAC state.
 *
 * @return state of HW LMAC
 ****************************************************************************************
 */
uint8_t lmac_cmd_state_get(void);

/**
 ****************************************************************************************
 * @brief Print and set LMAC state.
 *
 * @param[in]  state  LMAC HW state
 * @return  change state success or not
 ****************************************************************************************
 */
bool lmac_cmd_state_set(uint8_t state);

/**
 ****************************************************************************************
 * @brief Print MIB information.
 *
 * @param[in] reset  if reset then reset MIB count
 * @return pointer of struct machw_mib_tag machw_mib
 ****************************************************************************************
 */
void *lmac_cmd_mib(bool reset);

/**
 ****************************************************************************************
 * @brief RFTX function
 *
 * @param[in] struct RFTX rftx_param for TX
 ****************************************************************************************
 */
void lmac_cmd_rftx(struct RFTX *rftx_param);

/**
 ****************************************************************************************
 * @brief RFCW TX Start
 *
 * @param[in] freq  Frequency
 * @param[in] txPower TX power
 ****************************************************************************************
 */
void lmac_cmd_rfcw_start(uint32_t freq, uint32_t txPower);

/**
 ****************************************************************************************
 * @brief RFCW TX Stop
 *
 * @param[in] rftx_param for TX
 ****************************************************************************************
 */

void lmac_cmd_rfcw_stop(void);

/**
 ****************************************************************************************
 * @brief Setting RF CW PLL Reset
 *
 ****************************************************************************************
 */
void lmac_cmd_rf_cw_pll_reset(void);

/**
 ****************************************************************************************
 * @brief Setting RF CW PLL Set
 *
 ****************************************************************************************
 */
void lmac_cmd_rf_cw_pll_set(void);

/**
 ****************************************************************************************
 * @brief Get TX power.
 *
 * @param[out]  ofdmminpwrlevel   OFDM MIN Power level
 * @param[out]  dsssmaxpwrlevel   DSSS MAX Power level
 * @param[out]  ofdmmaxpwrlevel   OFDM MAX Power level
 ****************************************************************************************
 */
void lmac_cmd_power_get(uint8_t *ofdmminpwrlevel, uint8_t *dsssmaxpwrlevel, uint8_t *ofdmmaxpwrlevel);

/**
 ****************************************************************************************
 * @brief Set TX power.
 *
 * @param[in]  ofdmminpwrlevel   OFDM MIN Power level
 * @param[in]  dsssmaxpwrlevel   DSSS MAX Power level
 * @param[in]  ofdmmaxpwrlevel   OFDM MAX Power level
 ****************************************************************************************
 */
void lmac_cmd_power_set(uint8_t ofdmminpwrlevel, uint8_t dsssmaxpwrlevel, uint8_t ofdmmaxpwrlevel);

/**
 ****************************************************************************************
 * @brief Get RF channel.
 *
 * @param[out]  ch  channel index
 * @param[out]  freq  frequency
 ****************************************************************************************
 */
void lmac_cmd_channel_get(uint16_t *ch, uint16_t *freq);

/**
 ****************************************************************************************
 * @brief Set RF channel.
 *
 * @param[in]  band 2 for 2G, band 5 for 5G and 0 is don't care
 * @param[in]  ch  channel index
 * @return  channel frequency
 ****************************************************************************************
 */
uint32_t lmac_cmd_channel_set(uint32_t band, uint32_t ch);

/**
 ****************************************************************************************
 * @brief Set RF frequency.
 *
 * @param[in]  channel frequency
 * @return  channel band
 ****************************************************************************************
 */
uint32_t lmac_cmd_channel_frequncy_set(uint32_t freq);

/**
 ****************************************************************************************
 * @brief Set lmac debug filter.
 *
 * @param[in]  severity
 * @param[in]  module
 ****************************************************************************************
 */
void lmac_cmd_dbg(int severity, int module);

/**
 ****************************************************************************************
 * @brief Set lmac trace level.
 *
 * @param[in]  argc
 * @param[in]  argv
 * @return  true if trace supported
 ****************************************************************************************
 */
bool lmac_cmd_trace(int argc, const char **argv);

/**
 ****************************************************************************************
 * @brief Set TX Scale register
 *
 * @param[in]  argc
 * @param[in]  argv
 * @return  true if command success.
 ****************************************************************************************
 */
bool lmac_cmd_tx_scale(int argc, const char **argv);

/**
 ****************************************************************************************
 * @brief Set TX Scale mode
 *
 * @param[in]  argc
 * @param[in]  argv
 * @return  true if command success.
 ****************************************************************************************
 */
int lmac_cmd_scale_mode(int argc, const char **argv);

/**
 ****************************************************************************************
 * @brief Set TX LDPC mode on/off
 *
 * @param[in]  argc
 * @param[in]  argv
 * @return  true if command success.
 ****************************************************************************************
 */
bool cmd_lmac_ldpc(int argc, const char *argv[]);

uint32_t lmac_cmd_get_rwnx_sleep_cnt(void);
uint32_t lmac_cmd_get_rwnx_wakeup_cnt(void);
uint32_t lmac_cmd_get_rwnx_total_sleep_time(void);
uint32_t lmac_cmd_get_rwnx_total_wake_time(void);
void lmac_cmd_clear_ps_stats(void);
#define RWNX_SLEEP_DBG_EN 1

#define ___constant_swab16(z) ((unsigned short)(         								\
                               (((unsigned short)(z) & (unsigned short)0x00ffU) << 8) |	\
                               (((unsigned short)(z) & (unsigned short)0xff00U) >> 8)))
#define __swab16(z)     ___constant_swab16(z)
//#define cpu_to_be16(x)	((unsigned short __bitwise)__swab16((x)))



extern bool _wiphy_ext_feature_isset(int ifindex);
extern struct wireless_dev *fc80211_wdev_from_if_idx(int ifidx);


#endif /* __RWNX_MAC_COMMON_H__ */

/* EOF */
