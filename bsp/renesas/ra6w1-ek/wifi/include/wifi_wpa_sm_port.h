/*
 * Minimal ABI for the prebuilt RSN supplicant state machine.
 *
 * This is deliberately limited to the WPA/RSN lower state machine.  It does
 * not expose or create a wpa_supplicant interface, configuration database, or
 * control socket.
 */
#ifndef WIFI_WPA_SM_PORT_H
#define WIFI_WPA_SM_PORT_H

#include <stdbool.h>
#include <stddef.h>

#include "common/defs.h"
#include "common/wpa_common.h"
#include "common/ieee802_11_defs.h"

struct eapol_sm;
struct wpa_sm;
struct wpa_config_blob;
struct hostapd_freq_params;
struct wpa_channel_info;

/* Keep this layout in lockstep with rsn_supp/wpa.h used to build libsupplicant.
 * The prebuilt wpa_sm implementation dereferences callbacks by offset. */
struct wpa_sm_ctx {
    void *ctx;
    void *msg_ctx;

    void (*set_state)(void *ctx, enum wpa_states state);
    enum wpa_states (*get_state)(void *ctx);
    void (*deauthenticate)(void *ctx, u16 reason_code);
    void (*reconnect)(void *ctx);
    int (*set_key)(void *ctx, enum wpa_alg alg,
                   const u8 *addr, int key_idx, int set_tx,
                   const u8 *seq, size_t seq_len,
                   const u8 *key, size_t key_len,
                   enum key_flag key_flag);
    void *(*get_network_ctx)(void *ctx);
    int (*get_bssid)(void *ctx, u8 *bssid);
    int (*ether_send)(void *ctx, const u8 *dest, u16 proto,
                      const u8 *buf, size_t len);
    int (*get_beacon_ie)(void *ctx);
    void (*cancel_auth_timeout)(void *ctx);
    u8 *(*alloc_eapol)(void *ctx, u8 type, const void *data, u16 data_len,
                       size_t *msg_len, void **data_pos);
    int (*add_pmkid)(void *ctx, void *network_ctx, const u8 *bssid,
                     const u8 *pmkid, const u8 *fils_cache_id,
                     const u8 *pmk, size_t pmk_len, u32 pmk_lifetime,
                     u8 pmk_reauth_threshold, int akmp);
    int (*remove_pmkid)(void *ctx, void *network_ctx, const u8 *bssid,
                        const u8 *pmkid, const u8 *fils_cache_id);
    void (*set_config_blob)(void *ctx, struct wpa_config_blob *blob);
    const struct wpa_config_blob *(*get_config_blob)(void *ctx,
                                                       const char *name);
    int (*mlme_setprotection)(void *ctx, const u8 *addr,
                              int protection_type, int key_type);
    int (*update_ft_ies)(void *ctx, const u8 *md, const u8 *ies,
                         size_t ies_len);
    int (*send_ft_action)(void *ctx, u8 action, const u8 *target_ap,
                          const u8 *ies, size_t ies_len);
    int (*mark_authenticated)(void *ctx, const u8 *target_ap);
#ifdef CONFIG_TDLS
    int (*tdls_get_capa)(void *ctx, int *tdls_supported,
                         int *tdls_ext_setup, int *tdls_chan_switch);
    int (*send_tdls_mgmt)(void *ctx, const u8 *dst, u8 action_code,
                          u8 dialog_token, u16 status_code, u32 peer_capab,
                          int initiator, const u8 *buf, size_t len);
    int (*tdls_oper)(void *ctx, int oper, const u8 *peer);
    int (*tdls_peer_addset)(void *ctx, const u8 *addr, int add, u16 aid,
                            u16 capability, const u8 *supp_rates,
                            size_t supp_rates_len,
                            const struct ieee80211_ht_capabilities *ht_capab,
                            const struct ieee80211_vht_capabilities *vht_capab,
                            const struct ieee80211_he_capabilities *he_capab,
                            size_t he_capab_len,
                            const struct ieee80211_he_6ghz_band_cap *he_6ghz_capab,
                            u8 qosinfo, int wmm, const u8 *ext_capab,
                            size_t ext_capab_len, const u8 *supp_channels,
                            size_t supp_channels_len,
                            const u8 *supp_oper_classes,
                            size_t supp_oper_classes_len);
    int (*tdls_enable_channel_switch)(
        void *ctx, const u8 *addr, u8 oper_class,
        const struct hostapd_freq_params *params);
    int (*tdls_disable_channel_switch)(void *ctx, const u8 *addr);
#endif /* CONFIG_TDLS */
    void (*set_rekey_offload)(void *ctx, const u8 *kek, size_t kek_len,
                              const u8 *kck, size_t kck_len,
                              const u8 *replay_ctr);
    int (*key_mgmt_set_pmk)(void *ctx, const u8 *pmk, size_t pmk_len);
    void (*fils_hlp_rx)(void *ctx, const u8 *dst, const u8 *src,
                        const u8 *pkt, size_t pkt_len);
    int (*channel_info)(void *ctx, struct wpa_channel_info *ci);
    void (*transition_disable)(void *ctx, u8 bitmap);
    void (*store_ptk)(void *ctx, u8 *addr, int cipher,
                      u32 life_time, const struct wpa_ptk *ptk);
};

struct rsn_supp_config {
    void *network_ctx;
    int allowed_pairwise_cipher;
    int proactive_key_caching;
    int eap_workaround;
    void *eap_conf_ctx;
    const u8 *ssid;
    size_t ssid_len;
    int wpa_ptk_rekey;
    int wpa_deny_ptk0_rekey;
    int p2p;
    int wpa_rsc_relaxation;
    int owe_ptk_workaround;
    const u8 *fils_cache_id;
    int beacon_prot;
    bool force_kdk_derivation;
};

/* These values are normally supplied by wpa_supplicant's network setup
 * code.  The standalone demo has no network configuration layer, so it must
 * set the same fields directly on the lower state machine. */
enum wpa_sm_conf_params {
    RSNA_PMK_LIFETIME,
    RSNA_PMK_REAUTH_THRESHOLD,
    RSNA_SA_TIMEOUT,
    WPA_PARAM_PROTO,
    WPA_PARAM_PAIRWISE,
    WPA_PARAM_GROUP,
    WPA_PARAM_KEY_MGMT,
    WPA_PARAM_MGMT_GROUP,
    WPA_PARAM_RSN_ENABLED,
    WPA_PARAM_MFP,
    WPA_PARAM_OCV,
    WPA_PARAM_SAE_PWE,
    WPA_PARAM_SAE_PK,
    WPA_PARAM_DENY_PTK0_REKEY,
    WPA_PARAM_EXT_KEY_ID,
    WPA_PARAM_USE_EXT_KEY_ID,
    WPA_PARAM_FT_RSNXE_USED,
    WPA_PARAM_DPP_PFS,
    WPA_PARAM_OCI_FREQ_EAPOL,
    WPA_PARAM_OCI_FREQ_EAPOL_G2,
    WPA_PARAM_OCI_FREQ_FT_ASSOC,
    WPA_PARAM_OCI_FREQ_FILS_ASSOC,
};

struct wpa_sm *wpa_sm_init(struct wpa_sm_ctx *ctx);
void wpa_sm_deinit(struct wpa_sm *sm);
void wpa_sm_notify_assoc(struct wpa_sm *sm, const u8 *bssid);
void wpa_sm_notify_disassoc(struct wpa_sm *sm);
void wpa_sm_set_pmk(struct wpa_sm *sm, const u8 *pmk, size_t pmk_len,
                    const u8 *pmkid, const u8 *bssid);
void wpa_sm_set_config(struct wpa_sm *sm, struct rsn_supp_config *config);
int wpa_sm_set_param(struct wpa_sm *sm, enum wpa_sm_conf_params param,
                     unsigned int value);
void wpa_sm_set_own_addr(struct wpa_sm *sm, const u8 *addr);
void wpa_sm_set_ifname(struct wpa_sm *sm, const char *ifname);
void wpa_sm_set_eapol(struct wpa_sm *sm, struct eapol_sm *eapol);
int wpa_sm_set_assoc_wpa_ie(struct wpa_sm *sm, const u8 *ie, size_t len);
int wpa_sm_set_ap_rsn_ie(struct wpa_sm *sm, const u8 *ie, size_t len);
int wpa_sm_rx_eapol(struct wpa_sm *sm, const u8 *src_addr,
                    const u8 *buf, size_t len);
int wpa_sm_has_ptk_installed(struct wpa_sm *sm);
unsigned int wpa_sm_get_pairwise_cipher(struct wpa_sm *sm);

#endif /* WIFI_WPA_SM_PORT_H */
