/**
 ****************************************************************************************
 *
 * @file romac4rtos.h
 *
 * @brief RoMAC Interface for RTOS
 *
 * Copyright (c) 2023-2024 Renesas Electronics. All rights reserved.
 *
 * This software ("Software") is owned by Renesas Electronics.
 *
 * By using this Software you agree that Renesas Electronics retains all
 * intellectual property and proprietary rights in and to this Software and any
 * use, reproduction, disclosure or distribution of the Software without express
 * written permission or a license agreement from Renesas Electronics is
 * strictly prohibited. This Software is solely for use on or in conjunction
 * with Renesas Electronics products.
 *
 * EXCEPT AS OTHERWISE PROVIDED IN A LICENSE AGREEMENT BETWEEN THE PARTIES, THE
 * SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. EXCEPT AS OTHERWISE
 * PROVIDED IN A LICENSE AGREEMENT BETWEEN THE PARTIES, IN NO EVENT SHALL
 * RENESAS ELECTRONICS BE LIABLE FOR ANY DIRECT, SPECIAL, INDIRECT, INCIDENTAL,
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THE SOFTWARE.
 *
 ****************************************************************************************
 */

/**
 * \addtogroup REFERENCE_APPS
 * \{
 */
#ifndef CLI_ROMAC4RTOS_H_
#define CLI_ROMAC4RTOS_H_

#include <stdint.h>
#include <stdbool.h>

#define CHK_PTIM_STATUS(status, mask) ((status & mask) == mask)
#define PTIM_STATUS_DIFF(status, mask) ((status & mask) == 0)
#define CHK_PTIM_STATUS_UNDEF(status) ((status & 0x00FFFFFF) == 0)
#define CHK_PTIM_STATUS_ERR(status) (status & 0xFF000000)

typedef enum {
    /// No mask
    RTOS4DPM_ST_UNDEFINED          = 0x00000000,
    /// PTIM received UC, BC & MC data, so FBOOT needs to process UC data.
    RTOS4DPM_ST_UPLOAD             = 0x00000001,
    /// The received beacon data has changed, so FBOOT needs to handle it.
    RTOS4DPM_ST_BCN_CHANGED        = 0x00000002,
    /// There are no beacons, so FBOOT needs to scan for the AP or reconnect
    /// to other AP.
    RTOS4DPM_ST_NOBCN              = 0x00000004,
    /// TX failed because there was no ACK.
    RTOS4DPM_ST_NOACK              = 0x00000008,
    /// The station received a deauthentication frame or a
    /// disassociation frame.
    RTOS4DPM_ST_DEAUTH             = 0x00000010,
    /// A flag which indicates that PTIM did not receive a expected UC data.
    RTOS4DPM_ST_NOUC               = 0x00000020,

    /// A flag that indicates that PTIM did not received ACK on TCP KA.
    RTOS4DPM_ST_TCP_KA_TIMEOUT     = 0x00000040,
    /// PTIM internal problem indication.
    RTOS4DPM_ST_INTERNAL_PTIM_ISSUE= 0x00000080,

    /// PTIM ran at least once before FBOOT wakes up.
    RTOS4DPM_ST_FROM_FAST          = 0x00001000,
    /// While FBOOT is waking up, there were no PTIMs waking up.
    RTOS4DPM_ST_FROM_FULL          = 0x00002000,
    RTOS4DPM_ST_FB0                = 0x00004000,
    RTOS4DPM_ST_FB1                = 0x00008000,

    RTOS4DPM_ST_WEAK_SIGNAL_0      = 0x00010000,
    RTOS4DPM_ST_WEAK_SIGNAL_1      = 0x00020000,
    RTOS4DPM_ST_WEAK_SIGNAL_0_1    = 0x00030000,
    RTOS4DPM_ST_TIMP_UPDATE_0      = 0x00040000,
    RTOS4DPM_ST_TIMP_UPDATE_1      = 0x00080000,
    RTOS4DPM_ST_TIMP_UPDATE_0_1    = 0x000C0000,

    /// BUFP is complete or changed.
    RTOS4DPM_ST_BUFP_DONE          = 0x00100000,

    // Error status from below
    // No error status
    // RTOS4DPM_STE_NO_ERROR       = 0x00000000,

    /// During PTIM boot, system fault was detected.
    RTOS4DPM_STE_FAULT_DETECTED    = 0x01000000,
    /// During PTIM boot, WDOG was detected. The WDOG can be caused by a
    /// PTIM or other boot image.
    RTOS4DPM_STE_WDOG_DETECTED     = 0x02000000,
    /// RTC_WDOG was caused by RTC HW
    RTOS4DPM_STE_RTC_WDOG_DETECTED = 0x03000000,
    RTOS4DPM_STE_PTIM_TIMEOUT      = 0x04000000,
    RTOS4DPM_STE_INVALID_PREAMBLE  = 0x05000000,
    RTOS4DPM_STE_INVALID_MAC       = 0x06000000,
    RTOS4DPM_STE_INVALID_BSSID     = 0x07000000
} RTOS4DPM_ST;


typedef enum {
    RTOS4DPM_FUNC_NONE    = 0x00000000,
    RTOS4DPM_FUNC_TWT     = 0x00000001,
    RTOS4DPM_FUNC_TIM     = 0x00000002,
    RTOS4DPM_FUNC_BCMC    = 0x00000004,
    RTOS4DPM_FUNC_UC      = 0x00000008,
    RTOS4DPM_FUNC_KA      = 0x00000010,
    RTOS4DPM_FUNC_ACTRESP = 0x00000020,
    RTOS4DPM_FUNC_DEAUTH  = 0x00010000,
    RTOS4DPM_FUNC_APSYNC  = 0x00040000
} RTOS4DPM_FUNC;


typedef enum {
    RTOS4DPM_FEAT_NONE             = 0x00000000,
    RTOS4DPM_FEAT_NO_WFI           = 0x00000001,
    RTOS4DPM_FEAT_NO_PTIMOUT       = 0x00000002,
    RTOS4DPM_FEAT_NO_DEAUTH        = 0x00000004,
    RTOS4DPM_FEAT_NO_BCNCHG        = 0x00000008,
    RTOS4DPM_FEAT_NO_PBR           = 0x00000010,
    RTOS4DPM_FEAT_NO_LOSS          = 0x00000020,
    RTOS4DPM_FEAT_NO_PM            = 0x00000040,
    RTOS4DPM_FEAT_NO_ACTIVE        = 0x00000080,
    RTOS4DPM_FEAT_ACTIVE           = 0x00000100,
    RTOS4DPM_FEAT_WAIT_BCN         = 0x00000200,
    RTOS4DPM_FEAT_STANDALONE       = 0x00000400,
    RTOS4DPM_FEAT_STANDALONE_PHY   = 0x00000800,
    RTOS4DPM_FEAT_STANDALONE_UCODE = 0x00001000,
    RTOS4DPM_FEAT_TWTON_TIMOFF     = 0x00002000,
    RTOS4DPM_FEAT_HICURRENT        = 0x00004000,
} RTOS4DPM_FEAT;


// DBG
void romac4rtos_rtmsz(void);
void romac4rtos_reset_dpmst(void);

// TIMP Interfaces
void romac4rtos_timp_connection_loss(void);
void romac4rtos_timp_rxon_us(uint32_t monotonic_cnt);
void romac4rtos_timp_init(void);
void romac4rtos_timp_reset(uint32_t no, int64_t clk);
void romac4rtos_timp_period(uint32_t no, uint32_t tu);
uint32_t romac4rtos_timp_update(int64_t clk);
void romac4rtos_timp_stat(uint32_t s);

// MAC Interfaces
void romac4rtos_set_macaddr(uint8_t *macaddr);
void romac4rtos_set_aid(int32_t aid);
void romac4rtos_bcnint(int32_t bcnint);
int32_t romac4rtos_get_dtimp(void);
void romac4rtos_dtimp(int32_t dtimp);
void romac4rtos_set_bssid(uint8_t *bssid);
uint8_t romac4rtos_get_bssid_index(void);
void romac4rtos_set_bssid_index(uint8_t bssid_index);
void romac4rtos_set_freq(int32_t freq, int32_t chnum, int32_t band);
void romac4rtos_set_color(int32_t bss_color);
void romac4rtos_set_pwr(int8_t pwr);
void romac4rtos_set_usrpwr(int8_t usrpwr);
void romac4rtos_set_edca(int8_t ac, uint32_t ac_param);
void romac4rtos_set_pwrmgt(bool pwrmgt);
void romac4rtos_set_ampdu(uint32_t ampdu_size_max_he, uint8_t ampdu_spacing_min);
void romac4rtos_set_twtconf(uint8_t negty, bool wake_dur_unit, bool trigger,
                            bool implicit, bool flow_type, uint8_t id,
                            uint8_t wake_int_exp, uint8_t min_wake_dur,
                            uint16_t wake_int_mantissa, uint8_t channel);
void romac4rtos_set_twtactive(bool en);
void romac4rtos_reset_twt_en();
void romac4rtos_set_next_sp(uint64_t local_current, uint64_t next);
void romac4rtos_set_next_sp_ap_tst(uint64_t next_sp_ap_tst);
uint64_t romac4rtos_get_next_sp_ap_tst();
void romac4rtos_set_twt_individual_tim_interval(uint64_t twt_individual_tim_interval);
uint64_t romac4rtos_get_twt_individual_tim_interval();
void romac4rtos_set_twt_guard_us(uint32_t twt_guard_us);
uint32_t romac4rtos_get_twt_guard_us();
void romac4rtos_set_next_tim_start_c(uint64_t next_tim_start_c);
uint64_t romac4rtos_get_next_tim_start_c(void);
void romac4rtos_set_next_bcn(uint64_t local_current, uint64_t next);
void romac4rtos_set_rtos_clk_drift(uint32_t rtos_clk_drift);
int64_t romac4rtos_get_next_bcn();
uint64_t romac4rtos_get_next_bcn_rtclk();
int64_t romac4rtos_get_start_tsf_local();
int64_t romac4rtos_get_start_tsf_rtclk();
void romac4rtos_set_st_bcnst_dtim_cnt(uint8_t interval);
uint8_t romac4rtos_get_st_bcnst_dtim_cnt();
int64_t romac4rtos_get_st_tw_dtim();
uint16_t romac4rtos_get_mctrl_bcn_int();
uint8_t romac4rtos_get_mctrl_dtim_p();

uint16_t romac4rtos_get_apcap(uint16_t ap_cap);
bool     romac4rtos_check_ele(uint8_t ie_id);
void     romac4rtos_set_ele_crc(uint32_t ele_crc);
void     romac4rtos_update_bcn(uint16_t len, uint8_t *ssid, int32_t ssid_len,
                               int32_t dsss_ch, bool ofdm_en, uint16_t tim_us,
                               int16_t ts_us);
void     romac4rtos_update_start_tsf(int64_t rtclk, int64_t tsf, int64_t local);

void romac4rtos_ksr_reset(void);
void romac4rtos_ksr_store(uint8_t keyidx, uint32_t enc_cntrl, uint8_t *macaddr,
                          uint32_t *enckey, uint32_t *encwpi, uint8_t keyty);
void romac4rtos_ksr_rem(uint8_t keyidx);
/// All KSRs are restored at once. However when KSR is restored on RA6W1,
/// each KSR is restored one by one rather than all at once.
void romac4rtos_ksr_rec(void);
void romac4rtos_set_accept_oui(int32_t no, uint32_t oui);
void romac4rtos_set_he(bool he);
void romac4rtos_set_wmm(bool wmm);
void romac4rtos_set_uapsd(bool uapsd);
void romac4rtos_set_qos(bool qos);

// PTIM function features for customer
void romac4rtos_set_tim_timeout(int32_t timeout_tu);
void romac4rtos_set_deauth_timeout(int32_t timeout_tu);
void romac4rtos_set_deauth_period(uint32_t period);

// SEQN
void      romac4rtos_set_seqn(uint8_t tid, uint16_t seqn);
uint16_t  romac4rtos_get_seqn(uint8_t tid);
uint16_t *romac4rtos_get_seqn_ptr(void);
void      romac4rtos_set_nseqn(uint16_t seqn);
uint16_t  romac4rtos_get_nseqn(void);

// TXIV
void     romac4rtos_set_mic(uint8_t *mickey, uint8_t miclen);
void     romac4rtos_set_wepkey_len(uint8_t wepkey_len);
void     romac4rtos_get_txiv(uint16_t *cipher_suite, uint16_t *keyidx,
                             uint8_t *iv, int32_t *iv_len);
void     romac4rtos_set_txiv(uint16_t cipher_suite, uint16_t keyidx,
                             uint8_t *iv, int32_t iv_len);
uint64_t romac4rtos_get_pn(void);

// IP
void romac4rtos_get_ipv4(uint32_t *ip, uint32_t *subnet);
void romac4rtos_set_ipv4(uint32_t ip, uint32_t subnet);
uint32_t romac4rtos_get_mcip(int32_t no);
uint16_t *romac4rtos_get_mcipv6(int32_t no);
void romac4rtos_set_mcip(int32_t no, uint32_t mcip);
void romac4rtos_set_mcipv6(int32_t no, uint16_t *mcipv6);
void romac4rtos_set_ip_identification(uint16_t identication);
void romac4rtos_get_ipv6(uint8_t *ipv6l, uint8_t *ipv6g, uint8_t *prefix);
void romac4rtos_set_ipv6(uint8_t *ipv6l, uint8_t *ipv6g, uint8_t *prefix);

// AP
void romac4rtos_set_ap_ip(uint32_t apip);
void romac4rtos_set_ap_ipv6(uint16_t *ipv6);

// Server -> Deprecated
void romac4rtos_set_svripv4(uint32_t ip, uint8_t *target_mac);
void romac4rtos_set_svripv6(uint8_t *ipv6l, uint8_t *ipv6g,
                            uint8_t *target_mac);

// ARP
bool romac4rtos_get_autoarp(void);
void romac4rtos_set_autoarp(bool en);
void romac4rtos_set_autoarp_period(int32_t period);
void romac4rtos_set_arp(uint8_t *ta, uint32_t tip); // Deprecated
void romac4rtos_set_arp_ta(uint8_t *ta);
void romac4rtos_set_arp_tip(uint32_t tip);
void romac4rtos_set_ndp(uint8_t *na4host_mac, uint8_t *ipv6); // Deprecated
void romac4rtos_set_na4host_ta(uint8_t *nata);
void romac4rtos_set_na4host_tip(uint8_t *tip);
void romac4rtos_set_arpreq(uint8_t *da, uint32_t tip); // Deprecated
void romac4rtos_set_arpresp_en(bool en);
void romac4rtos_set_arpreq_en(bool en);
void romac4rtos_set_arpreq_ta(uint8_t *da);
void romac4rtos_set_arpreq_tip(uint32_t tip);


// UDP
void romac4rtos_set_udphen(bool en);
// Deprecated. Use romac4rtos_set_udph_period_n instead.
void romac4rtos_set_udph_period(int32_t period);
void romac4rtos_set_udph_period_n(int32_t session_n, int32_t period);
void romac4rtos_set_udpen(bool en);
// Deprecated. Use romac4rtos_set_udp_n instead.
void romac4rtos_set_udp(uint16_t sport, uint16_t dport);
void romac4rtos_set_udp_n(int32_t session_n, uint16_t sport, uint16_t dport);
// Deprecated. Use romac4rtos_set_udp_period_n instead.
void romac4rtos_set_udp_period(int32_t period);
void romac4rtos_set_udp_period_n(int32_t session_n, int32_t period);
void romac4rtos_set_udpdata(int32_t paylen, uint8_t *payl);
void romac4rtos_set_udport(int32_t no, uint16_t dport);
uint16_t romac4rtos_get_udport(int32_t no);
// Deprecated. Use romac4rtos_set_udp_target_mac_n instead.
void romac4rtos_set_udp_target_mac(uint8_t *target_mac);
void romac4rtos_set_udp_target_mac_n(int32_t session_n, uint8_t *target_mac);
// Deprecated. Use romac4rtos_set_udp_ipv4_n instead.
void romac4rtos_set_udp_ipv4(uint32_t ip);
void romac4rtos_set_udp_ipv4_n(int32_t session_n, uint32_t ip);
// Deprecated. Use romac4rtos_set_udp_ipv6_n instead.
void romac4rtos_set_udp_ipv6(uint8_t *ipv6);
void romac4rtos_set_udp_ipv6_n(int32_t session_n, uint8_t *ipv6);

// TCP
void romac4rtos_set_tcpacken(bool en);
void romac4rtos_set_tcpchken(bool en);
void romac4rtos_set_tcpen(bool en);
void romac4rtos_set_tcpkaen(bool en);
void romac4rtos_set_tcpka_n(int32_t session_n, bool en, uint32_t keep_idle,
                            uint32_t keep_intvl, uint16_t keep_cnt);
void romac4rtos_set_tcpchk_period(int32_t period);
void romac4rtos_set_tcpdata_period(int32_t period);
void romac4rtos_set_tcp_n(int32_t session_n, uint16_t sport, uint16_t dport,
                          uint32_t initial_seqn, uint32_t initial_ackn,
                          uint16_t window_size);
uint16_t romac4rtos_get_tcpka_probes_sent_n(int32_t session_n);
// void romac4rtos_set_tcpack(int32_t tcpack_len, uint8_t *tcpack_payl);
void romac4rtos_set_tcpdata(int32_t paylen, uint8_t *payl);
void romac4rtos_set_tcpseq_n(int32_t session_n, uint32_t seqn, uint32_t ackn,
                             uint16_t window_size);
void romac4rtos_set_tcport(int32_t no, uint16_t dport);
uint16_t romac4rtos_get_tcport(int32_t no);
void romac4rtos_set_tcp_target_mac_n(int32_t session_n, uint8_t *target_mac);
void romac4rtos_set_tcp_ipv4_n(int32_t session_n, uint32_t ip);
void romac4rtos_set_tcp_ipv6_n(int32_t session_n, uint8_t *ipv6);

// ROMAC Interfaces
void romac4rtos_initialize(bool reset_en, const uint32_t img4ptim);
void romac4rtos_upload(uint32_t reserved);
void romac4rtos_config(uint32_t timfunc, uint32_t timfeat);
void romac4rtos_req_period(int32_t req_ptim, int32_t req_ka);
void romac4rtos_ready(bool forced, uint32_t reserved);
void romac4rtos_download(uint32_t reserved);
void romac4rtos_ready_sleep(bool reset_en);
void romac4rtos_finalize(void);
/// It retreives PTIM event. See RTOS4DPM_ST.
uint32_t romac4rtos_event(void);
uint32_t romac4rtos_rxenv(void);
uint64_t romac4rtos_calc_sleep_time(void);
uint64_t romac4rtos_calc_dpm_sleep_time(void);

// BUFP
void     romac4rtos_set_bufp_en(bool en);
uint16_t romac4rtos_get_bufp_chk_period(void);
void     romac4rtos_set_bufp_chk_period(uint8_t pty, int32_t chk_period_ms);
void     romac4rtos_set_bufpconf(bool done_en, bool change_en, bool autoset_en,
                                 bool nopsp, uint8_t retry_cnt);
bool     romac4rtos_get_bufp_arp_status(void);
uint16_t romac4rtos_get_bufp_done_cnt(void);
uint16_t romac4rtos_get_bufp_max_cnt(void);
bool     romac4rtos_get_bufp_probe_status(void);
uint8_t  romac4rtos_get_bufp_status(void);

// Deprecated Interfaces
void romac4rtos_get_info(void);
void romac4rtos_bufp_info(void);
void romac4rtos_ksr_info(void);
void romac4rtos_set_clk(uint8_t maclk, uint8_t sysclk);
void romac4rtos_set_feat(uint32_t feat);
void romac4rtos_set_dbgfeat(uint32_t dbgfeat);
void romac4rtos_set_rxcntrl(uint32_t rxcntrl);
void romac4rtos_set_dtf(uint32_t dtf); ///< Data filter
void romac4rtos_set_psty(uint32_t psty);

bool romac4rtos_check_twt(void);
uint8_t romac4rtos_get_twt_negty(void);
uint8_t romac4rtos_get_twt_min_wake_dur(void);
uint64_t romac4rtos_calc_twt_sleep_dur(uint64_t c, uint32_t ready_c);

void romac4rtos_reset_st_rxst_pending_arp_req();
void romac4rtos_mark_arp_request_pending(uint32_t ip);
void romac4rtos_handle_arp_reply(uint32_t ip);

int32_t romac4rtos_get_rtos_drift_us_from_rtm(void);

#endif /* CLI_ROMAC4RTOS_H_ */

/**
 * \}
 */
