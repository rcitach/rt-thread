/*
 * WPA Supplicant - Basic AP mode support routines
 * Copyright (c) 2003-2009, Jouni Malinen <j@w1.fi>
 * Copyright (c) 2009, Atheros Communications
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef AP_H
#define AP_H

int wpa_supplicant_create_ap(struct wpa_supplicant *wpa_s,
			     struct wpa_ssid *ssid);
void wpa_supplicant_ap_deinit(struct wpa_supplicant *wpa_s);
void wpa_supplicant_ap_rx_eapol(struct wpa_supplicant *wpa_s,
				const u8 *src_addr, const u8 *buf, size_t len);
int wpa_supplicant_ap_wps_pbc(struct wpa_supplicant *wpa_s, const u8 *bssid,
			      const u8 *p2p_dev_addr);
int wpa_supplicant_ap_wps_pin(struct wpa_supplicant *wpa_s, const u8 *bssid,
			      const char *pin, char *buf, size_t buflen,
			      int timeout);
int wpa_supplicant_ap_wps_cancel(struct wpa_supplicant *wpa_s);
void wpas_wps_ap_pin_disable(struct wpa_supplicant *wpa_s);
const char * wpas_wps_ap_pin_random(struct wpa_supplicant *wpa_s, int timeout);
const char * wpas_wps_ap_pin_get(struct wpa_supplicant *wpa_s);
int wpas_wps_ap_pin_set(struct wpa_supplicant *wpa_s, const char *pin,
			int timeout);
int ap_ctrl_iface_sta_first(struct wpa_supplicant *wpa_s,
			    char *buf, size_t buflen);
int ap_ctrl_iface_sta(struct wpa_supplicant *wpa_s, const char *txtaddr,
		      char *buf, size_t buflen);
int ap_ctrl_iface_sta_next(struct wpa_supplicant *wpa_s, const char *txtaddr,
			   char *buf, size_t buflen);
int ap_ctrl_iface_sta_deauthenticate(struct wpa_supplicant *wpa_s,
				     const char *txtaddr);
int ap_ctrl_iface_sta_disassociate(struct wpa_supplicant *wpa_s,
				   const char *txtaddr);
int ap_ctrl_iface_wpa_get_status(struct wpa_supplicant *wpa_s, char *buf,
				 size_t buflen, int verbose);
void ap_tx_status(void *ctx, const u8 *addr,
		  const u8 *buf, size_t len, int ack);
void ap_eapol_tx_status(void *ctx, const u8 *dst,
			const u8 *data, size_t len, int ack);
void ap_client_poll_ok(void *ctx, const u8 *addr);
void ap_rx_from_unknown_sta(void *ctx, const u8 *addr, int wds);
int ap_mgmt_rx(void *ctx, struct rx_mgmt *rx_mgmt);
void ap_mgmt_tx_cb(void *ctx, const u8 *buf, size_t len, u16 stype, int ok);
int wpa_supplicant_ap_update_beacon(struct wpa_supplicant *wpa_s);
int wpa_supplicant_ap_mac_addr_filter(struct wpa_supplicant *wpa_s,
				      const u8 *addr);
void wpa_supplicant_ap_pwd_auth_fail(struct wpa_supplicant *wpa_s);
int ap_switch_channel(struct wpa_supplicant *wpa_s,
		      struct csa_settings *settings);
int ap_ctrl_iface_chanswitch(struct wpa_supplicant *wpa_s, const char *txtaddr);
void wpas_ap_ch_switch(struct wpa_supplicant *wpa_s, int freq, int ht,
		       int offset, int width, int cf1, int cf2, int finished);
int ap_ctrl_iface_status(struct wpa_supplicant *wpa_s, char *buf, size_t buflen);
const char * ap_get_state(struct wpa_supplicant *wpa_s);
int wpas_ap_pwrsave(struct wpa_supplicant *wpa_s, int ps_state, int timeout);
int wpas_ap_rts(struct wpa_supplicant *wpa_s, int rts_threshold);
int wpas_addba_reject(struct wpa_supplicant *wpa_s, u8 addba_reject);
int wpas_ap_set_greenfield(struct wpa_supplicant *wpa_s, u16 set_greenfield);
int wpas_ap_ht_protection(struct wpa_supplicant *wpa_s, int ht_protection);
struct wpabuf * wpas_ap_wps_nfc_config_token(struct wpa_supplicant *wpa_s,
					     int ndef);

#ifdef CONFIG_AP_ISOLATION
int ap_ctrl_iface_isolate(struct wpa_supplicant *wpa_s,const int isolate);
#endif /* CONFIG_AP_ISOLATION */

#ifdef CONFIG_AP
struct wpabuf * wpas_ap_wps_nfc_handover_sel(struct wpa_supplicant *wpa_s,
					     int ndef);
#else /* CONFIG_AP */
static inline struct wpabuf *
wpas_ap_wps_nfc_handover_sel(struct wpa_supplicant *wpa_s,
			     int ndef)
{
	return NULL;
}
#endif /* CONFIG_AP */

int wpas_ap_wps_nfc_report_handover(struct wpa_supplicant *wpa_s,
				    const struct wpabuf *req,
				    const struct wpabuf *sel);
int wpas_ap_wps_add_nfc_pw(struct wpa_supplicant *wpa_s, u16 pw_id,
			   const struct wpabuf *pw, const u8 *pubkey_hash);

struct hostapd_config;
int wpa_supplicant_conf_ap_ht(struct wpa_supplicant *wpa_s,
			      struct wpa_ssid *ssid,
			      struct hostapd_config *conf);

int wpas_ap_stop_ap(struct wpa_supplicant *wpa_s);

int wpas_ap_pmksa_cache_list(struct wpa_supplicant *wpa_s, char *buf,
			     size_t len);
void wpas_ap_pmksa_cache_flush(struct wpa_supplicant *wpa_s);
int wpas_ap_pmksa_cache_list_mesh(struct wpa_supplicant *wpa_s, const u8 *addr,
				  char *buf, size_t len);
int wpas_ap_pmksa_cache_add_external(struct wpa_supplicant *wpa_s, char *cmd);
int wpas_ap_update_beacon(struct wpa_supplicant *wpa_s);

void wpas_ap_event_dfs_radar_detected(struct wpa_supplicant *wpa_s,
				      struct dfs_event *radar);
void wpas_ap_event_dfs_pre_cac_expired(struct wpa_supplicant *wpa_s,
					      struct dfs_event *radar);
void wpas_ap_event_dfs_cac_started(struct wpa_supplicant *wpa_s,
				   struct dfs_event *radar);
void wpas_ap_event_dfs_cac_finished(struct wpa_supplicant *wpa_s,
				    struct dfs_event *radar);
void wpas_ap_event_dfs_cac_aborted(struct wpa_supplicant *wpa_s,
				   struct dfs_event *radar);
void wpas_ap_event_dfs_cac_nop_finished(struct wpa_supplicant *wpa_s,
					struct dfs_event *radar);

void ap_periodic(struct wpa_supplicant *wpa_s);

#ifdef CONFIG_AP_POWER
int ap_ctrl_iface_set_ap_power(struct wpa_supplicant *wpa_s, char *ap_pwr);
int ap_ctrl_iface_get_ap_power(struct wpa_supplicant *wpa_s, char *ap_pwr);
#endif /* CONFIG_AP_POWER */

char *ap_ctrl_iface_get_country(struct wpa_supplicant *wpa_s);
int ap_ctrl_iface_ap(struct wpa_supplicant *wpa_s, char *cmd, char *buf, size_t buflen);
int ap_ctrl_iface_set_country(struct wpa_supplicant *wpa_s, char *country);

#ifdef CONFIG_AP_WMM
int ap_ctrl_iface_wmm_enabled(struct wpa_supplicant *wpa_s, char *pos);
int ap_ctrl_iface_get_wmm_enabled(struct wpa_supplicant *wpa_s, char *buf, size_t buflen);
int ap_ctrl_iface_wmm_ps_enabled(struct wpa_supplicant *wpa_s, char *pos);
int ap_ctrl_iface_get_wmm_ps_enabled(struct wpa_supplicant *wpa_s, char *buf, size_t buflen);
int ap_ctrl_iface_wmm_params(struct wpa_supplicant *wpa_s, char *pos);
int ap_ctrl_iface_get_wmm_params(struct wpa_supplicant *wpa_s);
#endif /* CONFIG_AP_WMM */

int ap_ctrl_iface_ap_max_inactivity(struct wpa_supplicant *wpa_s, char *pos);
int ap_ctrl_iface_get_ap_max_inactivity(struct wpa_supplicant *wpa_s, char *buf, size_t buflen);
int ap_ctrl_iface_ap_send_ka(struct wpa_supplicant *wpa_s, char *pos);
int ap_ctrl_iface_get_ap_send_ka(struct wpa_supplicant *wpa_s, char *buf, size_t buflen);
int ap_ctrl_iface_save_config(struct wpa_supplicant *wpa_s);


#ifdef CONFIG_ACL
extern int hostapd_ap_acl_init(struct wpa_supplicant *wpa_s);
extern int hostapd_ap_acl_set(struct wpa_supplicant *wpa_s);
#endif // CONFIG_ACL

#endif /* AP_H */

/* EOF */
