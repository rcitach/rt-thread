/*
 * WPA Supplicant - Scanning
 * Copyright (c) 2003-2019, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 *
 *
 * Copyright (c) 2023 Modified by Renesas Electronics.
 *
 */

#include "utils/includes.h"

#include "utils/supp_common.h"
#include "utils/supp_eloop.h"
#include "common/ieee802_11_defs.h"
#include "common/wpa_ctrl.h"
#include "supp_config.h"
#include "wpa_supplicant_i.h"
#include "driver_i.h"
#include "wps_supplicant.h"
#include "ctrl_iface.h"
#if CFG_PMGR
#include "rm_pmgr_w_instance.h"
#endif /* CFG_PMGR */
#include "rm_wifi_reg_pwr_db.h"
#ifdef  CONFIG_P2P
#include "p2p_supplicant.h"
#include "p2p/supp_p2p.h"
#endif	// CONFIG_P2P
#ifdef CONFIG_HS20
#include "hs20_supplicant.h"
#endif	// CONFIG_HS20
#include "bss.h"
#include "supp_scan.h"
#ifdef CONFIG_MESH
#include "supp_mesh.h"
#endif	// CONFIG_MESH

extern void umac_heap_free(void *pointmem);
extern int i3ed11_freq_to_ch(int freq);
extern int fc80211_get_sta_rssi_value(int ifindex);
extern int get_run_mode(void);
extern char *ra6w1_regdb_create_freq_range_str(char* country);

#ifdef CONFIG_IMMEDIATE_SCAN
extern EventGroupHandle_t    ra6w1_sp_event_group;
#endif /* CONFIG_IMMEDIATE_SCAN */

#ifndef FEATURE_USE_DEFAULT_AP_SCAN
static void wpa_supplicant_gen_assoc_event(struct wpa_supplicant *wpa_s)
{
	struct wpa_ssid *ssid;
	union wpa_event_data data;

	ssid = wpa_supplicant_get_ssid(wpa_s);
	if (ssid == NULL)
		return;

	if (wpa_s->current_ssid == NULL) {
		wpa_s->current_ssid = ssid;
#ifdef CONFIG_NOTIFY
		wpas_notify_network_changed(wpa_s);
#endif	// CONFIG_NOTIFY
	}
#ifdef  IEEE8021X_EAPOL
	wpa_supplicant_initiate_eapol(wpa_s);
#endif	// IEEE8021X_EAPOL
	wpa_dbg(wpa_s, MSG_DEBUG, "Already associated with a configured "
		"network - generating associated event");
	os_memset(&data, 0, sizeof(data));
	wpa_supplicant_event(wpa_s, EVENT_ASSOC, &data);
}
#endif /* FEATURE_USE_DEFAULT_AP_SCAN */

#ifdef CONFIG_WPS
static int wpas_wps_in_use(struct wpa_supplicant *wpa_s,
			   enum wps_request_type *req_type)
{
	struct wpa_ssid *ssid;
	int wps = 0;

	for (ssid = wpa_s->conf->ssid; ssid; ssid = ssid->next) {
		if (!(ssid->key_mgmt & WPA_KEY_MGMT_WPS))
			continue;

		wps = 1;
		*req_type = wpas_wps_get_req_type(ssid);
		if (ssid->eap.phase1 && os_strstr(ssid->eap.phase1, "pbc=1"))
			return 2;
	}

#ifdef CONFIG_P2P
	if (!wpa_s->global->p2p_disabled && wpa_s->global->p2p &&
	    !wpa_s->conf->p2p_disabled) {
		wpa_s->wps->dev.p2p = 1;
		if (!wps) {
			wps = 1;
			*req_type = WPS_REQ_ENROLLEE_INFO;
		}
	}
#endif /* CONFIG_P2P */

	return wps;
}
#endif /* CONFIG_WPS */


#if defined ( CONFIG_SCHED_SCAN )
static int wpa_setup_mac_addr_rand_params(struct wpa_driver_scan_params *params,
					  const u8 *mac_addr)
{
	u8 *tmp;

	if (params->mac_addr) {
		params->mac_addr_mask = NULL;
		os_free(params->mac_addr);
		params->mac_addr = NULL;
	}

	params->mac_addr_rand = 1;

	if (!mac_addr)
		return 0;

	tmp = os_malloc(2 * ETH_ALEN);
	if (!tmp)
		return -1;

	os_memcpy(tmp, mac_addr, 2 * ETH_ALEN);
	params->mac_addr = tmp;
	params->mac_addr_mask = tmp + ETH_ALEN;
	return 0;
}
#endif	// defined ( CONFIG_SCHED_SCAN )


/**
 * wpa_supplicant_enabled_networks - Check whether there are enabled networks
 * @wpa_s: Pointer to wpa_supplicant data
 * Returns: 0 if no networks are enabled, >0 if networks are enabled
 *
 * This function is used to figure out whether any networks (or Interworking
 * with enabled credentials and auto_interworking) are present in the current
 * configuration.
 */
int wpa_supplicant_enabled_networks(struct wpa_supplicant *wpa_s)
{
	struct wpa_ssid *ssid = wpa_s->conf->ssid;
	int count = 0, disabled = 0;

#ifdef CONFIG_P2P
	if (wpa_s->p2p_mgmt)
		return 0; /* no normal network profiles on p2p_mgmt interface */
#endif	// CONFIG_P2P

	while (ssid) {
#ifdef CONFIG_AP		//MODIFY_SUPPLICANT_FOR_FREERTOS
		if (wpas_is_network_ap(ssid)) {
			disabled++;
		}
		else if (!wpas_network_disabled(wpa_s, ssid) &&
			(ssid->ssid != NULL || ssid->temporary == 1)) {
			count++;
		}
		else {
			disabled++;
		}
#else
		if (!wpas_network_disabled(wpa_s, ssid))
			count++;
		else
			disabled++;
#endif	// CONFIG_AP	//MODIFY_SUPPLICANT_FOR_FREERTOS
		ssid = ssid->next;
	}

#ifdef  CONFIG_INTERWORKING
	if (wpa_s->conf->cred && wpa_s->conf->interworking
#ifdef CONFIG_AP_WMM
	    && wpa_s->conf->auto_interworking
#endif	// CONFIG_AP_WMM
	   )
		count++;
#endif	// CONFIG_INTERWORKING
	if (count == 0 && disabled > 0) {
		wpa_dbg(wpa_s, MSG_DEBUG, "No enabled networks (%d disabled "
			"networks)", disabled);
	}
	return count;
}

#ifndef FEATURE_USE_DEFAULT_AP_SCAN
static void wpa_supplicant_assoc_try(struct wpa_supplicant *wpa_s,
				     struct wpa_ssid *ssid)
{
	int min_temp_disabled = 0;

	while (ssid) {
		if (!wpas_network_disabled(wpa_s, ssid)) {
			int temp_disabled = wpas_temp_disabled(wpa_s, ssid);

			if (temp_disabled <= 0)
				break;

			if (!min_temp_disabled ||
			    temp_disabled < min_temp_disabled)
				min_temp_disabled = temp_disabled;
		}
		ssid = ssid->next;
	}

	/* ap_scan=2 mode - try to associate with each SSID. */
	if (ssid == NULL) {
		wpa_dbg(wpa_s, MSG_DEBUG, "wpa_supplicant_assoc_try: Reached "
			"end of scan list - go back to beginning");
		wpa_s->prev_scan_ssid = WILDCARD_SSID_SCAN;

#ifdef PROBE_REQ_WITH_SSID_FOR_ASSOC	//MODIFY_SUPPLICANT_FOR_FREERTOS
		wpa_s->scan_for_connection = 1;
#endif /* PROBE_REQ_WITH_SSID_FOR_ASSOC */

		wpa_supplicant_req_scan(wpa_s, min_temp_disabled, 0);
		return;
	}
	if (ssid->next) {
		/* Continue from the next SSID on the next attempt. */
		wpa_s->prev_scan_ssid = ssid;
	} else {
		/* Start from the beginning of the SSID list. */
		wpa_s->prev_scan_ssid = WILDCARD_SSID_SCAN;
	}
	wpa_supplicant_associate(wpa_s, NULL, ssid);
}
#endif /* FEATURE_USE_DEFAULT_AP_SCAN */

//MODIFY_SUPPLICANT_FOR_FREERTOS
int wpa_supplicant_abort_scan(struct wpa_supplicant *wpa_s,
                struct wpa_driver_scan_params *params)
{
#if defined(CONFIG_SCAN_WITH_BSSID) || defined(CONFIG_SCAN_WITH_DIR_SSID)
	wpa_s->manual_scan_promisc = 0;
#endif /* CONFIG_SCAN_WITH_BSSID || CONFIG_SCAN_WITH_DIR_SSID */
	return 0;
}

#if defined ( CONFIG_SCHED_SCAN )
static void wpas_trigger_scan_cb(struct wpa_radio_work *work, int deinit)
{
	struct wpa_supplicant *wpa_s = work->wpa_s;
	struct wpa_driver_scan_params *params = work->ctx;
	int ret;

	if (deinit) {
		if (!work->started) {
			wpa_scan_free_params(params);
			return;
		}
		wpa_supplicant_notify_scanning(wpa_s, 0);
		wpas_notify_scan_done(wpa_s, 0);
		wpa_s->scan_work = NULL;
		return;
	}

	if ((wpa_s->mac_addr_rand_enable & MAC_ADDR_RAND_SCAN) &&
	    wpa_s->wpa_state <= WPA_SCANNING)
		wpa_setup_mac_addr_rand_params(params, wpa_s->mac_addr_scan);

	if (wpas_update_random_addr_disassoc(wpa_s) < 0) {
		wpa_msg(wpa_s, MSG_INFO,
			"Failed to assign random MAC address for a scan");
		wpa_scan_free_params(params);
		wpa_msg(wpa_s, MSG_INFO, WPA_EVENT_SCAN_FAILED "ret=-1");
		radio_work_done(work);
		return;
	}

	wpa_supplicant_notify_scanning(wpa_s, 1);

	if (wpa_s->clear_driver_scan_cache) {
		wpa_printf(MSG_DEBUG,
			   "Request driver to clear scan cache due to local BSS flush");
		params->only_new_results = 1;
	}
	ret = wpa_drv_scan(wpa_s, params);
	/*
	 * Store the obtained vendor scan cookie (if any) in wpa_s context.
	 * The current design is to allow only one scan request on each
	 * interface, hence having this scan cookie stored in wpa_s context is
	 * fine for now.
	 *
	 * Revisit this logic if concurrent scan operations per interface
	 * is supported.
	 */
	if (ret == 0)
		wpa_s->curr_scan_cookie = params->scan_cookie;
	wpa_scan_free_params(params);
	work->ctx = NULL;
	if (ret) {
		int retry = wpa_s->last_scan_req != MANUAL_SCAN_REQ &&
			!wpa_s->beacon_rep_data.token;

		if (wpa_s->disconnected)
			retry = 0;

		/* do not retry if operation is not supported */
		if (ret == -EOPNOTSUPP)
			retry = 0;

		wpa_supplicant_notify_scanning(wpa_s, 0);
		wpas_notify_scan_done(wpa_s, 0);
		if (wpa_s->wpa_state == WPA_SCANNING)
			wpa_supplicant_set_state(wpa_s,
						 wpa_s->scan_prev_wpa_state);
		wpa_msg(wpa_s, MSG_INFO, WPA_EVENT_SCAN_FAILED "ret=%d%s",
			ret, retry ? " retry=1" : "");
		radio_work_done(work);

		if (retry) {
			/* Restore scan_req since we will try to scan again */
			wpa_s->scan_req = wpa_s->last_scan_req;
			wpa_supplicant_req_scan(wpa_s, 1, 0);
		} else if (wpa_s->scan_res_handler) {
			/* Clear the scan_res_handler */
			wpa_s->scan_res_handler = NULL;
		}

		if (wpa_s->beacon_rep_data.token)
			wpas_rrm_refuse_request(wpa_s);

		return;
	}

	os_get_reltime(&wpa_s->scan_trigger_time);
	wpa_s->scan_runs++;
	wpa_s->normal_scans++;
	wpa_s->own_scan_requested = 1;
	wpa_s->clear_driver_scan_cache = 0;
	wpa_s->scan_work = work;
}
#endif	// defined ( CONFIG_SCHED_SCAN )

/**
 * wpa_supplicant_trigger_scan - Request driver to start a scan
 * @wpa_s: Pointer to wpa_supplicant data
 * @params: Scan parameters
 * Returns: 0 on success, -1 on failure
 */
int wpa_supplicant_trigger_scan(struct wpa_supplicant *wpa_s,
				struct wpa_driver_scan_params *params)
{ 	//MODIFY_SUPPLICANT_FOR_FREERTOS
	int ret;

	TX_FUNC_START("");

	if (wpa_s->scanning != 1)
		wpa_s->scanning = 1;

	if (wpa_s->clear_driver_scan_cache)
		params->only_new_results = 1;

	/* Call Init2() OPS */
	ret = wpa_drv_scan(wpa_s, params);
	if (ret) {
		if (wpa_s->scanning != 0)
			wpa_s->scanning = 0;
		printf(RED_COLOR " [%s] Failed wpa_drv_scan \n" CLEAR_COLOR, __func__);
		return ret;
	}

	os_get_reltime(&wpa_s->scan_trigger_time);
	wpa_s->scan_runs++;
	wpa_s->normal_scans++;
	wpa_s->own_scan_requested = 1;
	wpa_s->clear_driver_scan_cache = 0;
	if (DEBUG_SUPP_TMP)
        printf(" [%s] wpa_s:%p(%d) \n", __func__, wpa_s, wpa_s->own_scan_requested);

#ifdef PROBE_REQ_WITH_SSID_FOR_ASSOC
	wpa_s->scan_for_connection = 1;
#endif /* PROBE_REQ_WITH_SSID_FOR_ASSOC */

	TX_FUNC_END("");

	return ret;
}

#if defined ( CONFIG_SCHED_SCAN )
static void
wpa_supplicant_delayed_sched_scan_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;

	wpa_dbg(wpa_s, MSG_DEBUG, "Starting delayed sched scan");

	if (wpa_supplicant_req_sched_scan(wpa_s))
		wpa_supplicant_req_scan(wpa_s, 0, 0);
}


static void
wpa_supplicant_sched_scan_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;

	wpa_dbg(wpa_s, MSG_DEBUG, "Sched scan timeout - stopping it");

	wpa_s->sched_scan_timed_out = 1;
#if defined ( CONFIG_SCHED_SCAN )
	wpa_supplicant_cancel_sched_scan(wpa_s);
#endif	// defined ( CONFIG_SCHED_SCAN )
}


static int
wpa_supplicant_start_sched_scan(struct wpa_supplicant *wpa_s,
				struct wpa_driver_scan_params *params)
{
	int ret;

	wpa_supplicant_notify_scanning(wpa_s, 1);
	ret = wpa_drv_sched_scan(wpa_s, params);
	if (ret)
		wpa_supplicant_notify_scanning(wpa_s, 0);
	else
		wpa_s->sched_scanning = 1;

	return ret;
}


static int wpa_supplicant_stop_sched_scan(struct wpa_supplicant *wpa_s)
{
	int ret;

	ret = wpa_drv_stop_sched_scan(wpa_s);
	if (ret) {
		wpa_dbg(wpa_s, MSG_DEBUG, "stopping sched_scan failed!");
		/* TODO: what to do if stopping fails? */
		return -1;
	}

	return ret;
}
#endif  // CONFIG_SCHED_SCAN

static struct wpa_driver_scan_filter *
wpa_supplicant_build_filter_ssids(struct wpa_config *conf, size_t *num_ssids
#ifdef SUPPORT_SELECT_NETWORK_FILTER
                    , int set
#endif /* SUPPORT_SELECT_NETWORK_FILTER */
					)
{
	struct wpa_driver_scan_filter *ssids;
	struct wpa_ssid *ssid;
	size_t count;

	*num_ssids = 0;
#ifdef SUPPORT_SELECT_NETWORK_FILTER
	if (set != 1)
#endif /* SUPPORT_SELECT_NETWORK_FILTER */
	{
		if (!conf->filter_ssids)
			return NULL;
	}

	for (count = 0, ssid = conf->ssid; ssid; ssid = ssid->next) {
		if (ssid->ssid && ssid->ssid_len)
			count++;
	}
	if (count == 0)
		return NULL;
	ssids = os_calloc(count, sizeof(struct wpa_driver_scan_filter));
	if (ssids == NULL)
		return NULL;

	for (ssid = conf->ssid; ssid; ssid = ssid->next) {
		if (!ssid->ssid || !ssid->ssid_len)
			continue;
		os_memcpy(ssids[*num_ssids].ssid, ssid->ssid, ssid->ssid_len);
		ssids[*num_ssids].ssid_len = ssid->ssid_len;
		(*num_ssids)++;
	}

	return ssids;
}

#ifdef CONFIG_P2P_UNUSED_CMD
#ifdef CONFIG_P2P
static bool is_6ghz_supported(struct wpa_supplicant *wpa_s)
{
	struct hostapd_channel_data *chnl;
	int i, j;

	for (i = 0; i < wpa_s->hw.num_modes; i++) {
		if (wpa_s->hw.modes[i].mode == HOSTAPD_MODE_IEEE80211A) {
			chnl = wpa_s->hw.modes[i].channels;
			for (j = 0; j < wpa_s->hw.modes[i].num_channels; j++) {
				if (chnl[j].flag & HOSTAPD_CHAN_DISABLED)
					continue;
				if (is_6ghz_freq(chnl[j].freq))
					return true;
			}
		}
	}

	return false;
}
#endif /* CONFIG_P2P */
#endif // CONFIG_P2P_UNUSED_CMD

static void wpa_supplicant_optimize_freqs(
	struct wpa_supplicant *wpa_s, struct wpa_driver_scan_params *params)
{
#ifdef CONFIG_P2P
	if (params->freqs == NULL && wpa_s->p2p_in_provisioning &&
	    wpa_s->go_params) {
		/* Optimize provisioning state scan based on GO information */
		if (wpa_s->p2p_in_provisioning < 5 &&
		    wpa_s->go_params->freq > 0) {
			wpa_dbg(wpa_s, MSG_DEBUG, "P2P: Scan only GO "
				"preferred frequency %d MHz",
				wpa_s->go_params->freq);
			params->freqs = os_calloc(2, sizeof(int));
			if (params->freqs)
				params->freqs[0] = wpa_s->go_params->freq;
		} else if (wpa_s->p2p_in_provisioning < 8 &&
			   wpa_s->go_params->freq_list[0]) {
			wpa_dbg(wpa_s, MSG_DEBUG, "P2P: Scan only common "
				"channels");
			int_array_concat(&params->freqs,
					 wpa_s->go_params->freq_list);
			if (params->freqs)
				int_array_sort_unique(params->freqs);
		}
		wpa_s->p2p_in_provisioning++;
	}

#ifdef  CONFIG_P2P_OPTION
	if (params->freqs == NULL && wpa_s->p2p_in_invitation) {
		/*
		 * Optimize scan based on GO information during persistent
		 * group reinvocation
		 */
		if (wpa_s->p2p_in_invitation < 5 &&
		    wpa_s->p2p_invite_go_freq > 0) {
			wpa_dbg(wpa_s, MSG_DEBUG, "P2P: Scan only GO preferred frequency %d MHz during invitation",
				wpa_s->p2p_invite_go_freq);
			params->freqs = os_calloc(2, sizeof(int));
			if (params->freqs)
				params->freqs[0] = wpa_s->p2p_invite_go_freq;
		}
		wpa_s->p2p_in_invitation++;
		if (wpa_s->p2p_in_invitation > 20) {
			/*
			 * This should not really happen since the variable is
			 * cleared on group removal, but if it does happen, make
			 * sure we do not get stuck in special invitation scan
			 * mode.
			 */
			wpa_dbg(wpa_s, MSG_DEBUG, "P2P: Clear p2p_in_invitation");
			wpa_s->p2p_in_invitation = 0;
		}
	}
#endif	// CONFIG_P2P_OPTION
#endif /* CONFIG_P2P */

#ifdef CONFIG_WPS
	if (params->freqs == NULL && wpa_s->after_wps && wpa_s->wps_freq) {
		/*
		 * Optimize post-provisioning scan based on channel used
		 * during provisioning.
		 */
		wpa_dbg(wpa_s, MSG_DEBUG, "WPS: Scan only frequency %u MHz "
			"that was used during provisioning", wpa_s->wps_freq);
		params->freqs = os_calloc(2, sizeof(int));
		if (params->freqs)
			params->freqs[0] = wpa_s->wps_freq;
		wpa_s->after_wps--;
	} else if (wpa_s->after_wps)
		wpa_s->after_wps--;

	if (params->freqs == NULL && wpa_s->known_wps_freq && wpa_s->wps_freq)
	{
		/* Optimize provisioning scan based on already known channel */
		wpa_dbg(wpa_s, MSG_DEBUG, "WPS: Scan only frequency %u MHz",
			wpa_s->wps_freq);
		params->freqs = os_calloc(2, sizeof(int));
		if (params->freqs)
			params->freqs[0] = wpa_s->wps_freq;
		wpa_s->known_wps_freq = 0; /* only do this once */
	}
#endif /* CONFIG_WPS */
}


#ifdef CONFIG_INTERWORKING
static void wpas_add_interworking_elements(struct wpa_supplicant *wpa_s,
					   struct wpabuf *buf)
{
	wpabuf_put_u8(buf, WLAN_EID_INTERWORKING);
	wpabuf_put_u8(buf, is_zero_ether_addr(wpa_s->conf->hessid) ? 1 :
		      1 + ETH_ALEN);
	wpabuf_put_u8(buf, wpa_s->conf->access_network_type);
	/* No Venue Info */
	if (!is_zero_ether_addr(wpa_s->conf->hessid))
		wpabuf_put_data(buf, wpa_s->conf->hessid, ETH_ALEN);
}
#endif /* CONFIG_INTERWORKING */


#ifdef CONFIG_MBO
static void wpas_fils_req_param_add_max_channel(struct wpa_supplicant *wpa_s,
						struct wpabuf **ie)
{
	if (wpabuf_resize(ie, 5)) {
		wpa_printf(MSG_DEBUG,
			   "Failed to allocate space for FILS Request Parameters element");
		return;
	}

	/* FILS Request Parameters element */
	wpabuf_put_u8(*ie, WLAN_EID_EXTENSION);
	wpabuf_put_u8(*ie, 3); /* FILS Request attribute length */
	wpabuf_put_u8(*ie, WLAN_EID_EXT_FILS_REQ_PARAMS);
	/* Parameter control bitmap */
	wpabuf_put_u8(*ie, 0);
	/* Max Channel Time field - contains the value of MaxChannelTime
	 * parameter of the MLME-SCAN.request primitive represented in units of
	 * TUs, as an unsigned integer. A Max Channel Time field value of 255
	 * is used to indicate any duration of more than 254 TUs, or an
	 * unspecified or unknown duration. (IEEE Std 802.11ai-2016, 9.4.2.178)
	 */
	wpabuf_put_u8(*ie, 255);
}
#endif /* CONFIG_MBO */


void wpa_supplicant_set_default_scan_ies(struct wpa_supplicant *wpa_s)
{
	struct wpabuf *default_ies = NULL;
	u8 ext_capab[18];
	int ext_capab_len, frame_id;
	enum wpa_driver_if_type type = WPA_IF_STATION;

#ifdef CONFIG_P2P
	if (wpa_s->p2p_group_interface == P2P_GROUP_INTERFACE_CLIENT)
		type = WPA_IF_P2P_CLIENT;
#endif /* CONFIG_P2P */

	wpa_drv_get_ext_capa(wpa_s, type);

	ext_capab_len = wpas_build_ext_capab(wpa_s, ext_capab,
					     sizeof(ext_capab));
	if (ext_capab_len > 0 &&
	    wpabuf_resize(&default_ies, ext_capab_len) == 0)
		wpabuf_put_data(default_ies, ext_capab, ext_capab_len);

#ifdef CONFIG_MBO
	if (wpa_s->enable_oce & OCE_STA)
		wpas_fils_req_param_add_max_channel(wpa_s, &default_ies);
	/* Send MBO and OCE capabilities */
	if (wpabuf_resize(&default_ies, 12) == 0)
		wpas_mbo_scan_ie(wpa_s, default_ies);
#endif /* CONFIG_MBO */

	if (type == WPA_IF_P2P_CLIENT)
		frame_id = VENDOR_ELEM_PROBE_REQ_P2P;
	else
		frame_id = VENDOR_ELEM_PROBE_REQ;

	if (wpa_s->vendor_elem[frame_id]) {
		size_t len;

		len = wpabuf_len(wpa_s->vendor_elem[frame_id]);
		if (len > 0 && wpabuf_resize(&default_ies, len) == 0)
			wpabuf_put_buf(default_ies,
				       wpa_s->vendor_elem[frame_id]);
	}

	if (default_ies)
		wpa_drv_set_default_scan_ies(wpa_s, wpabuf_head(default_ies),
					     wpabuf_len(default_ies));
	wpabuf_free(default_ies);
}

static struct wpabuf * wpa_supplicant_extra_ies(struct wpa_supplicant *wpa_s)
{
	struct wpabuf *extra_ie = NULL;
	u8 ext_capab[18];
	int ext_capab_len;
#ifdef CONFIG_WPS
	int wps = 0;
	enum wps_request_type req_type = WPS_REQ_ENROLLEE_INFO;
#endif /* CONFIG_WPS */

#ifdef CONFIG_P2P
	if (wpa_s->p2p_group_interface == P2P_GROUP_INTERFACE_CLIENT)
		wpa_drv_get_ext_capa(wpa_s, WPA_IF_P2P_CLIENT);
	else
#endif /* CONFIG_P2P */
		wpa_drv_get_ext_capa(wpa_s, WPA_IF_STATION);

	ext_capab_len = wpas_build_ext_capab(wpa_s, ext_capab,
					     sizeof(ext_capab));
	if (ext_capab_len > 0 &&
	    wpabuf_resize(&extra_ie, ext_capab_len) == 0)
		wpabuf_put_data(extra_ie, ext_capab, ext_capab_len);

#ifdef CONFIG_INTERWORKING
	if (wpa_s->conf->interworking &&
	    wpabuf_resize(&extra_ie, 100) == 0)
		wpas_add_interworking_elements(wpa_s, extra_ie);
#endif /* CONFIG_INTERWORKING */

#ifdef CONFIG_MBO
	if (wpa_s->enable_oce & OCE_STA)
		wpas_fils_req_param_add_max_channel(wpa_s, &extra_ie);
#endif /* CONFIG_MBO */

#ifdef CONFIG_WPS
	wps = wpas_wps_in_use(wpa_s, &req_type);

	if (wps) {
		struct wpabuf *wps_ie;
		wps_ie = wps_build_probe_req_ie(wps == 2 ? DEV_PW_PUSHBUTTON :
						DEV_PW_DEFAULT,
						&wpa_s->wps->dev,
						wpa_s->wps->uuid, req_type,
						0, NULL);
		if (wps_ie) {
			if (wpabuf_resize(&extra_ie, wpabuf_len(wps_ie)) == 0)
				wpabuf_put_buf(extra_ie, wps_ie);
			wpabuf_free(wps_ie);
		}
	}

#ifdef CONFIG_P2P
	if (wps) {
#ifdef  CONFIG_P2P_UNUSED_CMD	//MODIFY_SUPPLICANT_FOR_FREERTOS
		size_t ielen = p2p_scan_ie_buf_len(wpa_s->global->p2p);
#else
		size_t ielen = 100;
#endif  /* CONFIG_P2P_UNUSED_CMD */
		if (wpabuf_resize(&extra_ie, ielen) == 0)
			wpas_p2p_scan_ie(wpa_s, extra_ie);
	}
#endif /* CONFIG_P2P */

#ifdef  CONFIG_MESH
	wpa_supplicant_mesh_add_scan_ie(wpa_s, &extra_ie);
#endif	// CONFIG_MESH

#endif /* CONFIG_WPS */

#ifdef CONFIG_HS20
	if (wpa_s->conf->hs20 && wpabuf_resize(&extra_ie, 9) == 0)
		wpas_hs20_add_indication(extra_ie, -1, 0);
#endif /* CONFIG_HS20 */

#ifdef CONFIG_FST
	if (wpa_s->fst_ies &&
	    wpabuf_resize(&extra_ie, wpabuf_len(wpa_s->fst_ies)) == 0)
		wpabuf_put_buf(extra_ie, wpa_s->fst_ies);
#endif /* CONFIG_FST */

#ifdef CONFIG_MBO
	/* Send MBO and OCE capabilities */
	if (wpabuf_resize(&extra_ie, 12) == 0)
		wpas_mbo_scan_ie(wpa_s, extra_ie);
#endif /* CONFIG_MBO */

#if defined ( CONFIG_VENDOR_ELEM )
	if (wpa_s->vendor_elem[VENDOR_ELEM_PROBE_REQ]) {
		struct wpabuf *buf = wpa_s->vendor_elem[VENDOR_ELEM_PROBE_REQ];

		if (wpabuf_resize(&extra_ie, wpabuf_len(buf)) == 0)
			wpabuf_put_buf(extra_ie, buf);
	}
#endif	// defined ( CONFIG_VENDOR_ELEM )

	return extra_ie;
}


#ifdef CONFIG_P2P

/*
 * Check whether there are any enabled networks or credentials that could be
 * used for a non-P2P connection.
 */
static int non_p2p_network_enabled(struct wpa_supplicant *wpa_s)
{
	struct wpa_ssid *ssid;

	for (ssid = wpa_s->conf->ssid; ssid; ssid = ssid->next) {
		if (wpas_network_disabled(wpa_s, ssid))
			continue;
		if (!ssid->p2p_group)
			return 1;
	}

#ifdef  CONFIG_INTERWORKING
	if (wpa_s->conf->cred && wpa_s->conf->interworking &&
	    wpa_s->conf->auto_interworking)
		return 1;
#endif	// CONFIG_INTERWORKING

	return 0;
}

#endif /* CONFIG_P2P */


int wpa_add_scan_freqs_list(struct wpa_supplicant *wpa_s,
			    enum hostapd_hw_mode band,
			    struct wpa_driver_scan_params *params, bool is_6ghz)
{
	/* Include only supported channels for the specified band */
	struct hostapd_hw_modes *mode;
	int num_chans = 0;
	int *freqs, i;

	mode = get_mode(wpa_s->hw.modes, wpa_s->hw.num_modes, band, is_6ghz);
	if (!mode)
		return -1;

	if (params->freqs) {
		while (params->freqs[num_chans])
			num_chans++;
	}

	freqs = os_realloc(params->freqs,
			   (num_chans + mode->num_channels + 1) * sizeof(int));
	if (!freqs)
		return -1;

	params->freqs = freqs;
	for (i = 0; i < mode->num_channels; i++) {
		if (mode->channels[i].flag & HOSTAPD_CHAN_DISABLED)
			continue;
		params->freqs[num_chans++] = mode->channels[i].freq;
	}
	params->freqs[num_chans] = 0;

	return 0;
}


static void wpa_setband_scan_freqs(struct wpa_supplicant *wpa_s,
				   struct wpa_driver_scan_params *params)
{
	if (wpa_s->hw.modes == NULL)
		return; /* unknown what channels the driver supports */
	if (params->freqs) {
        wpa_printf(MSG_DEBUG, "[%s:%d] add no channel ... \n", __func__, __LINE__);
		return; /* already using a limited channel set */
    }
    if (wpa_s->scan_band > 0) {
        wpa_printf(MSG_DEBUG, "[%s:%d] scan with forced band, handled later \n", __func__, __LINE__);
        return;
    }

	if (wpa_s->setband_mask & WPA_SETBAND_2G) {
		wpa_add_scan_freqs_list(wpa_s, HOSTAPD_MODE_IEEE80211G, params,
					false);
        wpa_printf(MSG_DEBUG, "[%s:%d] add 2G (SETBAND=2G) \n", __func__, __LINE__);
    }

#ifdef CONFIG_5G_SUPPORT
	if (wpa_s->setband_mask & WPA_SETBAND_5G) {
		wpa_add_scan_freqs_list(wpa_s, HOSTAPD_MODE_IEEE80211A, params,
					false);
        wpa_printf(MSG_DEBUG, "[%s:%d] add 5G (SETBAND=5G) \n", __func__, __LINE__);
    }
#endif /* CONFIG_5G_SUPPORT */

#ifdef CONFIG_6G_SUPPORT	// for 11ax (Wi-Fi 6E)
	if (wpa_s->setband_mask & WPA_SETBAND_6G) {
		wpa_add_scan_freqs_list(wpa_s, HOSTAPD_MODE_IEEE80211A, params,
					true);
        wpa_printf(MSG_DEBUG, "[%s:%d] add 6G (SETBAND=6G) \n", __func__, __LINE__);
    }
#endif /* CONFIG_6G_SUPPORT */
}

#ifdef FEATURE_SCAN_FREQ_ORDER_TOGGLE /* FC9000 Only */	//MODIFY_SUPPLICANT_FOR_FREERTOS
static void update_freqs_order_reversed(int num_freq,  int *freqs)
{
	int *freq_temp = (int *)os_calloc(num_freq, sizeof(int));

	if(freq_temp == NULL)
		return;

	for(int i = 0; i < num_freq  ; i++) {
		freq_temp[i] = freqs[i];
	}

	for(int i = 0 ; i < num_freq ; i++) {
		freqs[num_freq - i - 1] = freq_temp[i];
	}

	freqs[num_freq] = 0;

	os_free(freq_temp);
}
#endif /* FEATURE_SCAN_FREQ_ORDER_TOGGLE */  /* FC9000 Only */

#ifdef CONFIG_STA_COUNTRY_CODE
/**
 * update_freqs_with_country_code - update freqs with country code
 * @wpa_s: Pointer to wpa_supplicant data
 * @freqs: Pointer to channel array. e.g. freqs[] = {2472, 5180, 0}
 * @band : applies only when freqs is empty (NULL)
 * Returns: void
 */
void update_freqs_with_country_code(struct wpa_supplicant *wpa_s, int **freqs, int band)
{
    int i, j, k;
    int *freqs_temp;
    struct wpa_freq_range_list ranges;
    
    int num_freq = 0;

    ranges = wpa_s->conf->country_range;

    if (*freqs) {
        // requested scan_freq exists, band is ignored
        wpa_printf(MSG_DEBUG, "[%s] Requested scan_freq exists \n", __func__);
        freqs_temp = *freqs;
        int packing_required = 0;
        int *freqs_temp2 = NULL;

        for (i = 0; freqs_temp[i]; i++) {
            // scan freq is allowed in country?
            if (!freq_range_list_includes(&ranges, freqs_temp[i])) {
                wpa_printf(MSG_DEBUG, "[%s] freq(%d) is excluded as "
                    "it is not supported under the current country's regdb \n", __func__, freqs_temp[i]);
                freqs_temp[i] = 0;
                packing_required = 1;
            } else {
                num_freq++;
            }          
        }

        if (packing_required) {
            wpa_printf(MSG_DEBUG, "[%s] packing ... \n", __func__);
            freqs_temp2 = (int *)os_malloc(sizeof(int) * (num_freq + 1));
            if (freqs_temp2 == NULL) {
                wpa_printf(MSG_ERROR, "[%s:%d] Memory allocation failed\n", __func__, __LINE__);
    			return;
            }
            memset(freqs_temp2, 0, (sizeof(int) * (num_freq + 1)));

            k = 0;
            for (j = 0; j < i; j++) {
                if (freqs_temp[j]) {
                    freqs_temp2[k++] = freqs_temp[j];
                }
            }
            memset(freqs_temp, 0x00, (sizeof(int) * (i + 1)));
            memcpy(freqs_temp, freqs_temp2, (sizeof(int) * (num_freq + 1)));
            
            vPortFree(freqs_temp2);
       }
    } else {
        // requested scan_freq NOT exist. This means full scan (scan policy needed later), band is respected
        wpa_printf(MSG_DEBUG, "[%s] Full scan with band (%d): 2=2.4GHz, 5=5GHz, 0=2.4GHz+5GHz ... \n", __func__, band);
        
        char* freq_list_str = ra6w1_regdb_create_freq_range_str(wpa_s->conf->country);

        wpa_printf(MSG_DEBUG, "[%s] INPUT: freq_list_str(%s), band (2=2.4GHz, 5=5GHz, 0=2.4G+5G) = %d\n", __func__, freq_list_str, band);

        extern int * freq_range_to_channel_list_by_mode(struct wpa_supplicant *wpa_s, 
                                                     char *val, enum hostapd_hw_mode mode);

        extern int * freq_range_to_channel_list(struct wpa_supplicant *wpa_s, char *val);
        
        if (band == 2
            || wpa_s->setband_mask == WPA_SETBAND_2G
            || (wpa_s->setband_mask &  WPA_SETBAND_2G && wpa_s->setband_mask & WPA_SETBAND_6G)) {
            /* 2GHz only scan */
            // freq_temp (zero terminated) : Caller should free after use 
            freqs_temp = freq_range_to_channel_list_by_mode(wpa_s, freq_list_str, HOSTAPD_MODE_IEEE80211G);

            /* JP Channel <-- keep legacy code (p2p) */
            if (os_strcmp(wpa_s->ifname, P2P_DEVICE_NAME) == 0) {
                // search elem 2484 (ch14) and replace with 2472 (ch13)
                for (i = 0; freqs_temp[i]; i++) {
                    if (freqs_temp[i] == 2484) {
                        freqs_temp[i] = 2472; /* Channel 14 selected on 11b only */
                        break;
                    }
                }
            }

            if (wpa_s->setband_mask == WPA_SETBAND_2G)
                wpa_s->scan_band = 2;
            else if (wpa_s->setband_mask == WPA_SETBAND_5G)
                wpa_s->scan_band = 5;
            else
                wpa_s->scan_band = 0;
        } else if (band == 5
            || wpa_s->setband_mask == WPA_SETBAND_5G
            || (wpa_s->setband_mask &  WPA_SETBAND_5G && wpa_s->setband_mask &  WPA_SETBAND_6G)) {
            /* 5GHz only scan */
            // freq_temp (zero terminated) : Caller should free after use
            freqs_temp = freq_range_to_channel_list_by_mode(wpa_s, freq_list_str, HOSTAPD_MODE_IEEE80211A);
            if (wpa_s->setband_mask == WPA_SETBAND_2G)
                wpa_s->scan_band = 2;
            else if (wpa_s->setband_mask == WPA_SETBAND_5G)
                wpa_s->scan_band = 5;
            else
                wpa_s->scan_band = 0;
        } else {
            /* 2.4GHz, 5GHz scan */
            freqs_temp = freq_range_to_channel_list(wpa_s, freq_list_str);
            wpa_s->scan_band = 0;
        }
        
        vPortFree(freq_list_str);
    }

    wpa_printf(MSG_DEBUG, "[%s] OUTPUT: freqs=", __func__);
    for (i = 0; freqs_temp[i]; i++) {
        wpa_printf(MSG_DEBUG, "%d(%d) ", freqs_temp[i], i3ed11_freq_to_ch(freqs_temp[i]));
    }
    num_freq = i;
    
    wpa_printf(MSG_DEBUG, "num_freq=%d\n", num_freq);

#ifdef FEATURE_SCAN_FREQ_ORDER_TOGGLE
    if(wpa_s->reverse_scan_freq) {
        update_freqs_order_reversed(num_freq, freqs_temp);
        wpa_s->reverse_scan_freq = 0;
    } else {
        wpa_s->reverse_scan_freq = 1;
    }
#endif /* FEATURE_SCAN_FREQ_ORDER_TOGGLE */

    *freqs = freqs_temp;
}
#endif /* CONFIG_STA_COUNTRY_CODE */

#ifdef CONFIG_OWE
static void wpa_add_scan_ssid(struct wpa_supplicant *wpa_s,
			      struct wpa_driver_scan_params *params,
			      size_t max_ssids, const u8 *ssid, size_t ssid_len)
{
	unsigned int j;

	for (j = 0; j < params->num_ssids; j++) {
		if (params->ssids[j].ssid_len == ssid_len &&
		    params->ssids[j].ssid &&
		    os_memcmp(params->ssids[j].ssid, ssid, ssid_len) == 0)
			return; /* already in the list */
	}

	if (params->num_ssids + 1 > max_ssids) {
		wpa_printf(MSG_DEBUG, "Over max scan SSIDs for manual request");
		return;
	}

	wpa_printf(MSG_DEBUG, "Scan SSID (manual request): %s",
		   wpa_ssid_txt(ssid, ssid_len));

	params->ssids[params->num_ssids].ssid = ssid;
	params->ssids[params->num_ssids].ssid_len = ssid_len;
	params->num_ssids++;
}


static void wpa_add_owe_scan_ssid(struct wpa_supplicant *wpa_s,
				  struct wpa_driver_scan_params *params,
				  struct wpa_ssid *ssid, size_t max_ssids)
{
	struct wpa_bss *bss;

	if (!(ssid->key_mgmt & WPA_KEY_MGMT_OWE))
		return;

	wpa_printf(MSG_DEBUG, "OWE: Look for transition mode AP. ssid=%s",
		   wpa_ssid_txt(ssid->ssid, ssid->ssid_len));

	dl_list_for_each(bss, &wpa_s->bss, struct wpa_bss, list) {
		const u8 *owe, *pos, *end;
		const u8 *owe_ssid;
		size_t owe_ssid_len;

		if (bss->ssid_len != ssid->ssid_len ||
		    os_memcmp(bss->ssid, ssid->ssid, ssid->ssid_len) != 0)
			continue;

		owe = wpa_bss_get_vendor_ie(bss, OWE_IE_VENDOR_TYPE);
		if (!owe || owe[1] < 4)
			continue;

		pos = owe + 6;
		end = owe + 2 + owe[1];

		/* Must include BSSID and ssid_len */
		if (end - pos < ETH_ALEN + 1)
			return;

		/* Skip BSSID */
		pos += ETH_ALEN;
		owe_ssid_len = *pos++;
		owe_ssid = pos;

		if ((size_t) (end - pos) < owe_ssid_len ||
		    owe_ssid_len > SSID_MAX_LEN)
			return;

		wpa_printf(MSG_DEBUG,
			   "OWE: scan_ssids: transition mode OWE ssid=%s",
			   wpa_ssid_txt(owe_ssid, owe_ssid_len));

		wpa_add_scan_ssid(wpa_s, params, max_ssids,
				  owe_ssid, owe_ssid_len);
		return;
	}
}
#endif /* CONFIG_OWE */

#ifdef  UNUSED_CODE_DELETE
static void wpa_set_scan_ssids(struct wpa_supplicant *wpa_s,
			       struct wpa_driver_scan_params *params,
			       size_t max_ssids)
{
	unsigned int i;
#if !defined CONFIG_OWE
	unsigned int j;
#endif	// !CONFIG_OWE
	struct wpa_ssid *ssid;

	/*
	 * For devices with max_ssids greater than 1, leave the last slot empty
	 * for adding the wildcard scan entry.
	 */
	max_ssids = max_ssids > 1 ? max_ssids - 1 : max_ssids;

	for (i = 0; i < wpa_s->scan_id_count; i++) {
		ssid = wpa_config_get_network(wpa_s->conf, wpa_s->scan_id[i]);
		if (!ssid)
			continue;
#ifdef CONFIG_OWE	//MODIFY_SUPPLICANT_FOR_FREERTOS
		if (ssid->scan_ssid)
			wpa_add_scan_ssid(wpa_s, params, max_ssids,
					  ssid->ssid, ssid->ssid_len);
		/*
		 * Also add the SSID of the OWE BSS, to allow discovery of
		 * transition mode APs more quickly.
		 */
		wpa_add_owe_scan_ssid(wpa_s, params, ssid, max_ssids);
#else				//MODIFY_SUPPLICANT_FOR_FREERTOS
	for (j = 0; j < params->num_ssids; j++) {
		if (params->ssids[j].ssid_len == ssid->ssid_len &&
			params->ssids[j].ssid &&
			os_memcmp(params->ssids[j].ssid, ssid->ssid,
			ssid->ssid_len) == 0)
			break;
		}
		if (j < params->num_ssids)
			continue; /* already in the list */

		if (params->num_ssids + 1 > max_ssids) {
			wpa_printf_dbg(MSG_DEBUG,
				"Over max scan SSIDs for manual request");
			break;
		}

		wpa_printf_dbg(MSG_DEBUG, "Scan SSID (manual request): %s",
		wpa_ssid_txt(ssid->ssid, ssid->ssid_len));
		params->ssids[params->num_ssids].ssid = ssid->ssid;
		params->ssids[params->num_ssids].ssid_len = ssid->ssid_len;
		params->num_ssids++;
#endif				//MODIFY_SUPPLICANT_FOR_FREERTOS
	}

	wpa_s->scan_id_count = 0;
}

static int wpa_set_ssids_from_scan_req(struct wpa_supplicant *wpa_s,
				       struct wpa_driver_scan_params *params,
				       size_t max_ssids)
{
	unsigned int i;

	if (wpa_s->ssids_from_scan_req == NULL ||
	    wpa_s->num_ssids_from_scan_req == 0)
		return 0;

	if (wpa_s->num_ssids_from_scan_req > max_ssids) {
		wpa_s->num_ssids_from_scan_req = max_ssids;
		wpa_printf(MSG_DEBUG, "Over max scan SSIDs from scan req: %u",
			   (unsigned int) max_ssids);
	}

	for (i = 0; i < wpa_s->num_ssids_from_scan_req; i++) {
		params->ssids[i].ssid = wpa_s->ssids_from_scan_req[i].ssid;
		params->ssids[i].ssid_len =
			wpa_s->ssids_from_scan_req[i].ssid_len;
		wpa_hexdump_ascii(MSG_DEBUG, "specific SSID",
				  params->ssids[i].ssid,
				  params->ssids[i].ssid_len);
	}

	params->num_ssids = wpa_s->num_ssids_from_scan_req;
	wpa_s->num_ssids_from_scan_req = 0;
	return 1;
}
#endif  /* UNUSED_CODE_DELETE */

#ifdef CONFIG_RECONNECT_OPTIMIZE
u8 fast_reconnect_scan = 0;
#endif /* CONFIG_RECONNECT_OPTIMIZE */

// MODIFY_SUPPLICANT_FOR_FREERTOS
static void wpa_supplicant_scan(void *eloop_ctx, void *timeout_ctx) 
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	struct wpa_ssid *ssid = NULL;
	int ret;
	struct wpabuf *extra_ie = NULL;
	struct wpa_driver_scan_params params;
	struct wpa_driver_scan_params *scan_params = NULL;
	size_t max_ssids;
	enum wpa_states prev_state;
	int connect_without_scan = 0;

	wpa_s->ignore_post_flush_scan_res = 0;

	TX_FUNC_START("");

#if 0	/* by Shingu 20160901 (Concurrent) */
#if defined(CONFIG_CONCURRENT) && defined(CONFIG_DISALLOW_CONCURRENT_SCAN)
	if (get_run_mode() == WIFI_DEVICE_MODE_EXT_P2P_STATION) {
		if (is_other_iface_scan_ongoing(wpa_s->global, wpa_s)) {
			return;
		}
	}
#endif /* defined(CONFIG_CONCURRENT) && defined(CONFIG_DISALLOW_CONCURRENT_SCAN) */
#endif	/* 0 */

#ifdef CONFIG_REUSED_UMAC_BSS_LIST
	wpa_bss_flush(wpa_s);
#endif /* CONFIG_REUSED_UMAC_BSS_LIST */

#ifdef CONFIG_SCHED_SCAN
	if (wpa_s->pno || wpa_s->pno_sched_pending) {
		ra6wx_scan_prt("[%s] Skip scan - PNO is in progress\n", __func__);
		return;
	}
#endif /* CONFIG_SCHED_SCAN */

	if (wpa_s->disconnected && wpa_s->scan_req == NORMAL_SCAN_REQ) {
		ra6wx_debug_prt("[%s] Disconnected - do not scan\n", __func__);

		wpa_supplicant_set_state(wpa_s, WPA_DISCONNECTED);
		return;
	}

	ra6wx_scan_prt("[%s] wpa_s->scanning=%d\n", __func__, wpa_s->scanning);

	if (wpa_s->scanning) {
		/*
		 * If we are already in scanning state, we shall reschedule the
		 * the incoming scan request.
		 */
		ra6wx_scan_prt("[%s] Already scanning - "
			"Reschedule the incoming scan req\n", __func__);

		wpa_supplicant_req_scan(wpa_s, wpa_s->scan_interval, 0);

		ra6wx_scan_prt("[%s] FINISH : #1\n", __func__);
		return;
	}

	if (!wpa_supplicant_enabled_networks(wpa_s) &&
	    wpa_s->scan_req == NORMAL_SCAN_REQ) {
		ra6wx_debug_prt("[%s] No enabled networks - do not scan\n",
			 __func__);
		wpa_supplicant_set_state(wpa_s, WPA_INACTIVE);

		ra6wx_scan_prt("[%s] FINISH : #2\n", __func__);
		return;
	}

#ifndef FEATURE_USE_DEFAULT_AP_SCAN
	if (wpa_s->conf->ap_scan != 0 &&
	    (wpa_s->drv_flags & WPA_DRIVER_FLAGS_WIRED)) {
		ra6wx_scan_prt("            [%s] Using wired authentication - "
			"overriding ap_scan configuration\n", __func__);

		wpa_s->conf->ap_scan = 0;
	}

	if (wpa_s->conf->ap_scan == 0) {
		wpa_supplicant_gen_assoc_event(wpa_s);

		ra6wx_scan_prt("            [%s] FINISH : #3\n", __func__);
		return;
	}
#endif /* FEATURE_USE_DEFAULT_AP_SCAN */

	if (wpa_s->scan_req != MANUAL_SCAN_REQ &&
		wpa_s->connect_without_scan) {
		connect_without_scan = 1;
		for (ssid = wpa_s->conf->ssid; ssid; ssid = ssid->next) {
			if (ssid == wpa_s->connect_without_scan)
				break;
		}
	}

#ifdef	CONFIG_P2P
	if (wpas_p2p_in_progress(wpa_s)) {
		ra6wx_scan_prt("            [%s] Delay station mode scan "
			"while P2P operation is in progress\n", __func__);

		wpa_supplicant_req_scan(wpa_s, 5, 0);

		ra6wx_scan_prt("            [%s] FINISH : #4\n", __func__);

		return;
	}
#endif	/* CONFIG_P2P */

#ifndef FEATURE_USE_DEFAULT_AP_SCAN
	if (wpa_s->conf->ap_scan == 2)
		max_ssids = 1;
	else
#endif /* FEATURE_USE_DEFAULT_AP_SCAN */
	{
		max_ssids = wpa_s->max_scan_ssids;
		if (max_ssids > WPAS_MAX_SCAN_SSIDS)
			max_ssids = WPAS_MAX_SCAN_SSIDS;
	}

	wpa_s->last_scan_req = wpa_s->scan_req;
	wpa_s->scan_req = NORMAL_SCAN_REQ;

	if (connect_without_scan) {
		wpa_s->connect_without_scan = NULL;
		if (ssid) {
			ra6wx_scan_prt("[%s] Start a pre-selected network "
				   "without scan step\n", __func__);
			wpa_supplicant_associate(wpa_s, NULL, ssid);
			return;
		}
	}

	os_memset(&params, 0, sizeof(params));

#ifdef CONFIG_RECONNECT_OPTIMIZE
	fast_reconnect_scan = 0;
#endif /* CONFIG_RECONNECT_OPTIMIZE */

	prev_state = wpa_s->wpa_state;
	if (wpa_s->wpa_state == WPA_DISCONNECTED ||
	    wpa_s->wpa_state == WPA_INACTIVE)
		wpa_supplicant_set_state(wpa_s, WPA_SCANNING);

	/*
	 * If autoscan has set its own scanning parameters
	 */
	if (wpa_s->autoscan_params != NULL) {
		scan_params = wpa_s->autoscan_params;
		goto scan;
	}

	if (wpa_s->last_scan_req != MANUAL_SCAN_REQ &&
	    wpa_s->connect_without_scan) {
		for (ssid = wpa_s->conf->ssid; ssid; ssid = ssid->next) {
			if (ssid == wpa_s->connect_without_scan)
				break;
		}

		wpa_s->connect_without_scan = NULL;

		if (ssid) {
			ra6wx_scan_prt("[%s] Start a pre-selected "
				"network without scan step\n", __func__);

#ifdef CONFIG_REUSED_UMAC_BSS_LIST
			wpa_bss_flush(wpa_s);
#endif /* CONFIG_REUSED_UMAC_BSS_LIST */

			wpa_supplicant_associate(wpa_s, NULL, ssid);

			ra6wx_scan_prt("[%s] FINISH : #5\n", __func__);
			return;
		}
	}

#ifdef CONFIG_P2P
	if ((wpa_s->p2p_in_provisioning || wpa_s->show_group_started) &&
	    wpa_s->go_params) {
		ra6wx_scan_prt("[%s] P2P : Use specific SSID for scan during P2P "
			"group formation (p2p_in_provisioning=%d "
			"show_group_started=%d)\n",
				__func__,
				wpa_s->p2p_in_provisioning,
				wpa_s->show_group_started);

		params.ssids[0].ssid = wpa_s->go_params->ssid;
		params.ssids[0].ssid_len = wpa_s->go_params->ssid_len;
		params.num_ssids = 1;

		goto ssid_list_set;
	}

#ifdef	CONFIG_P2P_OPTION
	if (wpa_s->p2p_in_invitation) {
		if (wpa_s->current_ssid) {
			ra6wx_scan_prt("[%s] P2P: Use specific SSID for scan "
				"during invitation\n", __func__);

			params.ssids[0].ssid = wpa_s->current_ssid->ssid;
			params.ssids[0].ssid_len =
				wpa_s->current_ssid->ssid_len;
			params.num_ssids = 1;
		} else {
			ra6wx_scan_prt("[%s] P2P: No specific SSID known for scan "
				"during invitation\n", __func__);
		}
		goto ssid_list_set;
	}
#endif	/* CONFIG_P2P_OPTION */
#endif	/* CONFIG_P2P */

	/* Find the starting point from which to continue scanning */
	ssid = wpa_s->conf->ssid;
	if (wpa_s->prev_scan_ssid != WILDCARD_SSID_SCAN) {
		while (ssid) {
			if (ssid == wpa_s->prev_scan_ssid) {
				ssid = ssid->next;
				break;
			}
			ssid = ssid->next;
		}
	}

#ifndef FEATURE_USE_DEFAULT_AP_SCAN
	if (wpa_s->last_scan_req != MANUAL_SCAN_REQ && wpa_s->conf->ap_scan == 2) {
		wpa_s->connect_without_scan = NULL;
		wpa_s->prev_scan_wildcard = 0;
		wpa_supplicant_assoc_try(wpa_s, ssid);
		ra6wx_scan_prt("            [%s] FINISH : #6\n", __func__);
		return;
	} else if (wpa_s->conf->ap_scan == 2) {
		/*
		 * User-initiated scan request in ap_scan == 2; scan with
		 * wildcard SSID.
		 */
		ssid = NULL;
	} else
#endif /* FEATURE_USE_DEFAULT_AP_SCAN */

	if (wpa_s->reattach && wpa_s->current_ssid != NULL) {
		/*
		 * Perform single-channel single-SSID scan for
		 * reassociate-to-same-BSS operation.
		 */
		/* Setup SSID */
		ssid = wpa_s->current_ssid;

		ra6wx_ascii_dump("Scan SSID", ssid->ssid, ssid->ssid_len);

		params.ssids[0].ssid = ssid->ssid;
		params.ssids[0].ssid_len = ssid->ssid_len;
		params.num_ssids = 1;

		/*
		 * Allocate memory for frequency array, allocate one extra
		 * slot for the zero-terminator.
		 */
		params.freqs = (int *)os_malloc(sizeof(int) * 2);
		if (params.freqs == NULL) {
			ra6wx_scan_prt("[%s] Memory allocation failed\n", __func__);
			return;
		}
		params.freqs[0] = wpa_s->assoc_freq;
		params.freqs[1] = 0;

		/*
		 * Reset the reattach flag so that we fall back to full scan if
		 * this scan fails.
		 */
		wpa_s->reattach = 0;
	} else {
		struct wpa_ssid *start = ssid, *tssid;
		int freqs_set = 0;
		if (ssid == NULL && max_ssids > 1)
			ssid = wpa_s->conf->ssid;

		while (ssid) {
#if 0 /* Debug */
			printf(ANSI_BCOLOR_YELLOW "[%s] %d\n"
			"prev_scan_wildcard=%d\n"
			"scan_req=%d\n"
			"last_scan_req=%d\n"
			"scan_runs=%d\n"
			"reverse_scan_freq=%d\n"
			"manual_scan_use_id=%d\n"
			"manual_scan_only_new=%d\n"
			"manual_scan_promisc=%d\n"
			"own_scan_requested=%d\n"
			"own_scan_running=%d\n"
			"clear_driver_scan_cache=%d\n"
			"manual_scan_id=%d\n"
			"scan_interval=%d\n"
			"normal_scans=%d\n"
			"scan_for_connection=%d\n"
			"params.num_ssids=%d\n"
			 ANSI_BCOLOR_DEFULT"\n",
			 __func__, __LINE__,
			wpa_s->prev_scan_wildcard, 
			wpa_s->scan_req,
			wpa_s->last_scan_req,
			wpa_s->scan_runs,
			wpa_s->reverse_scan_freq,
			wpa_s->manual_scan_use_id,
			wpa_s->manual_scan_only_new,
			wpa_s->manual_scan_promisc,
			wpa_s->own_scan_requested,
			wpa_s->own_scan_running,
			wpa_s->clear_driver_scan_cache,
			wpa_s->manual_scan_id,
			wpa_s->scan_interval,
			wpa_s->normal_scans,
			wpa_s->scan_for_connection,
			params.num_ssids
			);
#endif /* Debug */

			if (!wpas_network_disabled(wpa_s, ssid) && ssid->mode < 2 /* Hidden ssid or reassoc_freq ??????*/
				&& (ssid->scan_ssid
#ifdef CONFIG_RECONNECT_OPTIMIZE
					|| (wpa_s->reassoc_freq && (wpa_s->reassoc_try <= 3))
#endif /* CONFIG_RECONNECT_OPTIMIZE */
				) && wpa_s->last_scan_req < MANUAL_SCAN_REQ
			) {
				ra6wx_ascii_dump("Scan SSID ",
						ssid->ssid, ssid->ssid_len);

				params.ssids[params.num_ssids].ssid =
					ssid->ssid;
				params.ssids[params.num_ssids].ssid_len =
					ssid->ssid_len;

#if 0 /* Debug */
				printf(ANSI_COLOR_LIGHT_GREEN "[%s] %d\n"
				"prev_scan_wildcard=%d\n"
				"scan_req=%d\n"
				"last_scan_req=%d\n"
				"scan_runs=%d\n"
				"reverse_scan_freq=%d\n"
				"manual_scan_use_id=%d\n"
				"manual_scan_only_new=%d\n"
				"manual_scan_promisc=%d\n"
				"own_scan_requested=%d\n"
				"own_scan_running=%d\n"
				"clear_driver_scan_cache=%d\n"
				"manual_scan_id=%d\n"
				"scan_interval=%d\n"
				"normal_scans=%d\n"
				"scan_for_connection=%d\n"
				"params.num_ssids=%d\n"
				 ANSI_BCOLOR_DEFULT"\n",
				 __func__, __LINE__,
				wpa_s->prev_scan_wildcard, 
				wpa_s->scan_req,
				wpa_s->last_scan_req,
				wpa_s->scan_runs,
				wpa_s->reverse_scan_freq,
				wpa_s->manual_scan_use_id,
				wpa_s->manual_scan_only_new,
				wpa_s->manual_scan_promisc,
				wpa_s->own_scan_requested,
				wpa_s->own_scan_running,
				wpa_s->clear_driver_scan_cache,
				wpa_s->manual_scan_id,
				wpa_s->scan_interval,
				wpa_s->normal_scans,
				wpa_s->scan_for_connection,
				params.num_ssids
				);
#endif /* Debug */

#ifdef PROBE_REQ_WITH_SSID_FOR_ASSOC
				if (wpa_s->scan_for_connection == 0 || wpa_s->last_scan_req > INITIAL_SCAN_REQ)
#endif /* PROBE_REQ_WITH_SSID_FOR_ASSOC */
				{
					params.num_ssids++;
				}

				if ((size_t)params.num_ssids + 1 >= max_ssids)
					break;
			}
#ifdef CONFIG_OWE
			if (!wpas_network_disabled(wpa_s, ssid)) {
				/*
				 * Also add the SSID of the OWE BSS, to allow
				 * discovery of transition mode APs more
				 * quickly.
				 */
				wpa_add_owe_scan_ssid(wpa_s, &params, ssid,
							  max_ssids);
			}
#endif
			ssid = ssid->next;
			if (ssid == start)
				break;
			if (ssid == NULL && max_ssids > 1 &&
			    start != wpa_s->conf->ssid)
				ssid = wpa_s->conf->ssid;
		}

		for (tssid = wpa_s->conf->ssid; tssid; tssid = tssid->next) {
			if (wpas_network_disabled(wpa_s, tssid))
				continue;
			if ((params.freqs || !freqs_set) && tssid->scan_freq) {
				int_array_concat(&params.freqs,
						 tssid->scan_freq);
			} else {
				os_free(params.freqs);
				params.freqs = NULL;
			}
			freqs_set = 1;
		}
		int_array_sort_unique(params.freqs);
	}

	if (ssid && max_ssids == 1) {
		/*
		 * If the driver is limited to 1 SSID at a time interleave
		 * wildcard SSID scans with specific SSID scans to avoid
		 * waiting a long time for a wildcard scan.
		 */
		if (!wpa_s->prev_scan_wildcard) {
			params.ssids[0].ssid = NULL;
			params.ssids[0].ssid_len = 0;
			wpa_s->prev_scan_wildcard = 1;

			ra6wx_scan_prt("            [%s] Starting AP scan for "
				"wildcard SSID (Interleave with specific)\n",
				__func__);
		} else {
			wpa_s->prev_scan_ssid = ssid;
			wpa_s->prev_scan_wildcard = 0;

			ra6wx_scan_prt("            [%s] Starting AP scan for "
				"specific SSID: %s\n",
				__func__,
				wpa_ssid_txt(ssid->ssid, ssid->ssid_len));
		}
	} else if (ssid) {
		/* max_ssids > 1 */

#ifdef CONFIG_RECONNECT_OPTIMIZE
		if (ssid->scan_ssid == 0 && !(wpa_s->reassoc_freq && wpa_s->reassoc_try <= 3)) {
			wpa_s->prev_scan_ssid = ssid;

			ra6wx_scan_prt("            [%s] Include wildcard SSID in "
				"the scan request\n", __func__);

#if 0 /* Debug */
			printf(ANSI_COLOR_LIGHT_MAGENTA"[%s] %d\n"
				"prev_scan_wildcard=%d\n"
				"scan_req=%d\n"
				"last_scan_req=%d\n"
				"scan_runs=%d\n"
				"reverse_scan_freq=%d\n"
				"manual_scan_use_id=%d\n"
				"manual_scan_only_new=%d\n"
				"manual_scan_promisc=%d\n"
				"own_scan_requested=%d\n"
				"own_scan_running=%d\n"
				"clear_driver_scan_cache=%d\n"
				"manual_scan_id=%d\n"
				"scan_interval=%d\n"
				"normal_scans=%d\n"
				"scan_for_connection=%d\n"
				"params.num_ssids=%d\n"
				 ANSI_BCOLOR_DEFULT"\n",
				 __func__, __LINE__,
				wpa_s->prev_scan_wildcard, 
				wpa_s->scan_req,
				wpa_s->last_scan_req,
				wpa_s->scan_runs,
				wpa_s->reverse_scan_freq,
				wpa_s->manual_scan_use_id,
				wpa_s->manual_scan_only_new,
				wpa_s->manual_scan_promisc,
				wpa_s->own_scan_requested,
				wpa_s->own_scan_running,
				wpa_s->clear_driver_scan_cache,
				wpa_s->manual_scan_id,
				wpa_s->scan_interval,
				wpa_s->normal_scans,
				wpa_s->scan_for_connection,
				params.num_ssids
				);
#endif /* Debug */


#ifdef PROBE_REQ_WITH_SSID_FOR_ASSOC
			if (wpa_s->scan_for_connection == 0 || wpa_s->last_scan_req > INITIAL_SCAN_REQ) /* Hidden SSID ??????????? ??? Scan????? */
#endif /* PROBE_REQ_WITH_SSID_FOR_ASSOC */
			{ 
				params.num_ssids++;
			}
		}
#endif /* CONFIG_RECONNECT_OPTIMIZE */
	} else {
#if 0 /* Debug */
		printf(ANSI_BCOLOR_CYAN "[%s] %d\n"
			"prev_scan_wildcard=%d\n"
			"scan_req=%d\n"
			"last_scan_req=%d\n"
			"scan_runs=%d\n"
			"reverse_scan_freq=%d\n"
			"manual_scan_use_id=%d\n"
			"manual_scan_only_new=%d\n"
			"manual_scan_promisc=%d\n"
			"own_scan_requested=%d\n"
			"own_scan_running=%d\n"
			"clear_driver_scan_cache=%d\n"
			"manual_scan_id=%d\n"
			"scan_interval=%d\n"
			"normal_scans=%d\n"
			"scan_for_connection=%d\n"
			"params.num_ssids=%d\n"
			 ANSI_BCOLOR_DEFULT"\n",
			 __func__, __LINE__,
			wpa_s->prev_scan_wildcard, 
			wpa_s->scan_req,
			wpa_s->last_scan_req,
			wpa_s->scan_runs,
			wpa_s->reverse_scan_freq,
			wpa_s->manual_scan_use_id,
			wpa_s->manual_scan_only_new,
			wpa_s->manual_scan_promisc,
			wpa_s->own_scan_requested,
			wpa_s->own_scan_running,
			wpa_s->clear_driver_scan_cache,
			wpa_s->manual_scan_id,
			wpa_s->scan_interval,
			wpa_s->normal_scans,
			wpa_s->scan_for_connection,
			params.num_ssids
			);
#endif /* Debug */


		wpa_s->prev_scan_ssid = WILDCARD_SSID_SCAN;
		params.num_ssids++;

		ra6wx_scan_prt("            [%s] Starting AP scan for wildcard\n",
			__func__);
	}
#ifdef CONFIG_P2P
ssid_list_set:
#endif /* CONFIG_P2P */

	wpa_supplicant_optimize_freqs(wpa_s, &params);
	extra_ie = wpa_supplicant_extra_ies(wpa_s);

	if (wpa_s->last_scan_req == MANUAL_SCAN_REQ &&
	    wpa_s->manual_scan_only_new)
		params.only_new_results = 1;

	if (wpa_s->last_scan_req == MANUAL_SCAN_REQ && params.freqs == NULL &&
	    wpa_s->manual_scan_freqs) {
		ra6wx_scan_prt("            [%s] Limit manual scan to specified \n"
			"channels\n", __func__);

		params.freqs = wpa_s->manual_scan_freqs;
		wpa_s->manual_scan_freqs = NULL;
	}

	if (params.freqs == NULL && wpa_s->next_scan_freqs) {
		ra6wx_scan_prt("            [%s] Optimize scan based on previously "
			"generated frequency list\n", __func__);

		params.freqs = wpa_s->next_scan_freqs;
	} else
		os_free(wpa_s->next_scan_freqs);

	wpa_s->next_scan_freqs = NULL;
	wpa_setband_scan_freqs(wpa_s, &params);

	/* See if user specified frequencies. If so, scan only those. */
	if (wpa_s->conf->freq_list && !params.freqs) {
		ra6wx_scan_prt("            [%s] Optimize scan based on "
			"conf->freq_list\n", __func__);

		int_array_concat(&params.freqs, wpa_s->conf->freq_list);
	}

	/* Use current associated channel? */
	if (wpa_s->conf->scan_cur_freq && !params.freqs) {
		unsigned int num = wpa_s->num_multichan_concurrent;

		params.freqs = os_calloc(num + 1, sizeof(int));
		if (params.freqs) {
			num = get_shared_radio_freqs(wpa_s, params.freqs, num);
			if (num > 0) {
				ra6wx_scan_prt("            [%s] Scan only the "
					"current operating channels since "
					"scan_cur_freq is enabled\n", __func__);
			} else {
				os_free(params.freqs);
				params.freqs = NULL;
			}
		}
	}

#ifdef SUPPORT_SELECT_NETWORK_FILTER
	if(wpa_s->last_scan_req != MANUAL_SCAN_REQ)
#endif /* SUPPORT_SELECT_NETWORK_FILTER */
	params.filter_ssids = wpa_supplicant_build_filter_ssids(
					wpa_s->conf, &params.num_filter_ssids
#ifdef SUPPORT_SELECT_NETWORK_FILTER
					, 0
#endif /* SUPPORT_SELECT_NETWORK_FILTER */
					);

	if (extra_ie) {
		params.extra_ies = wpabuf_head(extra_ie);
		params.extra_ies_len = wpabuf_len(extra_ie);
	}

#ifdef CONFIG_P2P
#ifdef	CONFIG_P2P_OPTION
	if (wpa_s->p2p_in_provisioning || wpa_s->p2p_in_invitation ||
	    (wpa_s->show_group_started && wpa_s->go_params))
#else	/* CONFIG_P2P_OPTION */
	if (wpa_s->p2p_in_provisioning ||
	    (wpa_s->show_group_started && wpa_s->go_params))
#endif	/* CONFIG_P2P_OPTION */
	{
		/*
		 * The interface may not yet be in P2P mode, so we have to
		 * explicitly request P2P probe to disable CCK rates.
		 */
		params.p2p_probe = 1;
	}
#endif /* CONFIG_P2P */

	scan_params = &params;

scan:
#ifdef CONFIG_P2P
	/*
	 * If the driver does not support multi-channel concurrency and a
	 * virtual interface that shares the same radio with the wpa_s interface
	 * is operating there may not be need to scan other channels apart from
	 * the current operating channel on the other virtual interface. Filter
	 * out other channels in case we are trying to find a connection for a
	 * station interface when we are not configured to prefer station
	 * connection and a concurrent operation is already in process.
	 */
	if (wpa_s->scan_for_connection &&
	    wpa_s->last_scan_req == NORMAL_SCAN_REQ &&
	    !scan_params->freqs && !params.freqs &&
#ifdef	CONFIG_P2P_UNUSED_CMD
	    wpas_is_p2p_prioritized(wpa_s) &&
#endif	/* CONFIG_P2P_UNUSED_CMD */
	    wpa_s->p2p_group_interface == NOT_P2P_GROUP_INTERFACE &&
	    non_p2p_network_enabled(wpa_s)) {
		unsigned int num = wpa_s->num_multichan_concurrent;

		params.freqs = os_calloc(num + 1, sizeof(int));
		if (params.freqs) {
			num = get_shared_radio_freqs(wpa_s, params.freqs, num);
			if (num > 0 && num == wpa_s->num_multichan_concurrent) {
				ra6wx_scan_prt("            [%s] Scan only the "
					"current operating channels since "
					"all channels are already used\n",
					__func__);
			} else {
				os_free(params.freqs);
				params.freqs = NULL;
			}
		}
	}
#endif /* CONFIG_P2P */

#ifdef CONFIG_STA_COUNTRY_CODE
	update_freqs_with_country_code(wpa_s, &params.freqs, wpa_s->scan_band);
#endif /* CONFIG_STA_COUNTRY_CODE */

#ifdef	CONFIG_RECONNECT_OPTIMIZE  
	/** When performing a scan in the connected state by CLI Command & Setup  
	 * skip the fast scan & do normal scan. 
	 */
#ifdef	CONFIG_SIMPLE_ROAMING
	if (wpa_s->conf->roam_state != ROAM_SCANNING) {
#endif	/* CONFIG_SIMPLE_ROAMING */

    ra6wx_scan_prt("[%s] assoc_freq=%d reassoc_freq=%d, scan_for_connection=%d, scan_cur_freq=%d, num_ssids=%d\n",
            __func__, wpa_s->assoc_freq, wpa_s->reassoc_freq, wpa_s->scan_for_connection, wpa_s->conf->scan_cur_freq, params.num_ssids);

	if ((wpa_s->reassoc_freq && wpa_s->scan_for_connection)
#ifdef CONFIG_FAST_CONN_ASSOC_CH
#if CFG_PMGR
	    || (!RM_PMGR_W_dpm_is_wakeup() && wpa_s->conf->scan_cur_freq)
#else
		|| (wpa_s->conf->scan_cur_freq)
#endif /* CFG_PMGR */
#endif /* CONFIG_FAST_CONN_ASSOC_CH */
	) {
		wpa_s->reassoc_try++;

		if (wpa_s->reassoc_try <= 3) {

			if (params.freqs != NULL)
				os_free(params.freqs);

			params.freqs = (int *)os_malloc(sizeof(int) * 2);

			if (params.freqs == NULL) {
				ra6wx_scan_prt("[%s] Memory allocation failed\n",
						__func__);
				return;
			}

#ifdef CONFIG_FAST_CONN_ASSOC_CH
#if CFG_PMGR
            if (!RM_PMGR_W_dpm_is_wakeup() && wpa_s->conf->scan_cur_freq) {
#else
			if (wpa_s->conf->scan_cur_freq) {
#endif /* CFG_PMGR */
				params.num_ssids = 1;
                params.freqs[0] = wpa_s->reassoc_freq = wpa_s->conf->scan_cur_freq;
                ra6wx_scan_prt("[%s] freq=%d scan_cur_freq=%d\n", __func__, wpa_s->reassoc_freq, wpa_s->conf->scan_cur_freq);
            }
            else
#endif /* CONFIG_FAST_CONN_ASSOC_CH */
            {
    			params.freqs[0] = wpa_s->reassoc_freq;
			}

            params.freqs[1] = 0;
			fast_reconnect_scan = 1;
			ra6wx_notice_prt("Fast scan, freq=%d\n", wpa_s->reassoc_freq);
		}
	}
#ifdef	CONFIG_SIMPLE_ROAMING
	}
#endif	/* CONFIG_SIMPLE_ROAMING */

#endif	/* CONFIG_RECONNECT_OPTIMIZE */

#ifdef CONFIG_SCAN_WITH_BSSID
	if (wpa_s->manual_scan_promisc && !is_zero_ether_addr(wpa_s->scanbssid))
	{
		scan_params->bssid_scan_flag = 1;
		memcpy(scan_params->scanbssid , wpa_s->scanbssid, ETH_ALEN);
		//ra6wx_notice_prt("Scan with BSSID %x:%x:%x:%x:%x:%x\n",
		//				scan_params->scanbssid[0], scan_params->scanbssid[1], scan_params->scanbssid[2],
		//				scan_params->scanbssid[3], scan_params->scanbssid[4], scan_params->scanbssid[5]);			
	}
#endif

#ifdef CONFIG_SCAN_WITH_DIR_SSID
	if (wpa_s->manual_scan_promisc && wpa_s->ssids_from_scan_req && wpa_s->num_ssids_from_scan_req != 0)
	{
		int ssid_index;

		for (ssid_index = 0; ssid_index < wpa_s->num_ssids_from_scan_req; ssid_index++)
		{
			scan_params->ssids[ssid_index].ssid = wpa_s->ssids_from_scan_req[ssid_index].ssid;
			scan_params->ssids[ssid_index].ssid_len = wpa_s->ssids_from_scan_req[ssid_index].ssid_len;
			//ra6wx_notice_prt("[%s] %dth SSID scan, ssid = %s\n", __func__, ssid_index, scan_params->ssids[ssid_index].ssid);
		}

		scan_params->dir_ssid_scan_flag = 1;
		scan_params->num_ssids = wpa_s->num_ssids_from_scan_req;
		wpa_s->num_ssids_from_scan_req = 0;
	}
	else
	{
		scan_params->dir_ssid_scan_flag = 0;
	}
#endif

#ifdef CONFIG_MBO
        if (wpa_s->enable_oce & OCE_STA) {
            scan_params->oce_scan = 1;
        }
#endif /* CONFIG_MBO */

	ra6wx_scan_prt("[%s] call TRIGGER_SCAN\n", __func__);

	ret = wpa_supplicant_trigger_scan(wpa_s, scan_params);

	if ((ret) && wpa_s->last_scan_req == MANUAL_SCAN_REQ && params.freqs &&
	    !wpa_s->manual_scan_freqs) {
		/* Restore manual_scan_freqs for the next attempt */
		wpa_s->manual_scan_freqs = params.freqs;
		params.freqs = NULL;
	}

	wpabuf_free(extra_ie);
	os_free(params.freqs);
	os_free(params.filter_ssids);

	if (ret) {
		ra6wx_err_prt("[%s] Failed to init AP scan\n", __func__);

		if (prev_state != wpa_s->wpa_state)
			wpa_supplicant_set_state(wpa_s, prev_state);

		xEventGroupSetBits(ra6w1_sp_event_group, RA6WX_SCAN_RESULTS_FAIL_EV);

		/* Restore scan_req since we will try to scan again */
		wpa_s->scan_req = wpa_s->last_scan_req;
		wpa_supplicant_req_scan(wpa_s, 1, 0);
	} else {
		wpa_s->scan_for_connection = 0;
	}

	TX_FUNC_END("");
}

#if 0 /* unused for RA6Wx supp 2.7 */
void wpa_supplicant_update_scan_int(struct wpa_supplicant *wpa_s, int sec)
{
	struct os_reltime remaining, new_int;
	int cancelled;

	cancelled = eloop_cancel_timeout_one(wpa_supplicant_scan, wpa_s, NULL,
					     &remaining);

	new_int.sec = sec;
	new_int.usec = 0;
	if (cancelled && os_reltime_before(&remaining, &new_int)) {
		new_int.sec = remaining.sec;
		new_int.usec = remaining.usec;
	}

	if (cancelled) {
		eloop_register_timeout(new_int.sec, new_int.usec,
				       wpa_supplicant_scan, wpa_s, NULL);
	}
	wpa_s->scan_interval = sec;
}
#endif	// 0 /* unused for RA6Wx supp 2.7 */


/**
 * wpa_supplicant_req_scan - Schedule a scan for neighboring access points
 * @wpa_s: Pointer to wpa_supplicant data
 * @sec: Number of seconds after which to scan
 * @usec: Number of microseconds after which to scan
 *
 * This function is used to schedule a scan for neighboring access points after
 * the specified time.
 */
void wpa_supplicant_req_scan(struct wpa_supplicant *wpa_s, int sec, int usec)
{
	int res;

	TX_FUNC_START("");

#ifdef CONFIG_P2P
	if (wpa_s->p2p_mgmt) {
		wpa_dbg(wpa_s, MSG_DEBUG,
			"Ignore scan request (%d.%06d sec) on p2p_mgmt interface",
			sec, usec);
		return;
	}
#endif	// CONFIG_P2P

#ifndef ENABLE_SCAN_ON_AP_MODE	//MODIFY_SUPPLICANT_FOR_FREERTOS
	if (get_run_mode() == WIFI_DEVICE_MODE_EXT_AP) {
		ra6wx_scan_prt("[%s] STA interface is not enabled."
					"Do not scan\n", __func__, sec, usec);
		return;
	}
#endif /* ! ENABLE_SCAN_ON_AP_MODE */

	res = eloop_deplete_timeout(sec, usec, wpa_supplicant_scan, wpa_s, NULL);
	if (res == 1) {
		//MODIFY_SUPPLICANT_FOR_FREERTOS - chg
		ra6wx_scan_prt("[%s] Rescheduling scan request : " "%d.%06d sec\n", __func__, sec, usec);
	} else if (res == 0) {
		//MODIFY_SUPPLICANT_FOR_FREERTOS - chg
		ra6wx_scan_prt("[%s] Ignore new scan request for "
			"%d.%06d sec since an earlier request is "
			"scheduled to trigger sooner\n",
			__func__, sec, usec);
	} else {
		//MODIFY_SUPPLICANT_FOR_FREERTOS - chg
		ra6wx_scan_prt("[%s] Setting scan request: %d.%06d sec \n", __func__, sec, usec);
		eloop_register_timeout(sec, usec, wpa_supplicant_scan, wpa_s, NULL);
	}

	TX_FUNC_END("");
}


#if defined ( CONFIG_SCHED_SCAN )
/**
 * wpa_supplicant_delayed_sched_scan - Request a delayed scheduled scan
 * @wpa_s: Pointer to wpa_supplicant data
 * @sec: Number of seconds after which to scan
 * @usec: Number of microseconds after which to scan
 * Returns: 0 on success or -1 otherwise
 *
 * This function is used to schedule periodic scans for neighboring
 * access points after the specified time.
 */
int wpa_supplicant_delayed_sched_scan(struct wpa_supplicant *wpa_s,
				      int sec, int usec)
{
	if (!wpa_s->sched_scan_supported)
		return -1;

	eloop_register_timeout(sec, usec,
			       wpa_supplicant_delayed_sched_scan_timeout,
			       wpa_s, NULL);

	return 0;
}
#endif	// defined ( CONFIG_SCHED_SCAN )


#if defined ( CONFIG_SRP )
static void
wpa_scan_set_relative_rssi_params(struct wpa_supplicant *wpa_s,
				  struct wpa_driver_scan_params *params)
{
	if (wpa_s->wpa_state != WPA_COMPLETED ||
	    !(wpa_s->drv_flags & WPA_DRIVER_FLAGS_SCHED_SCAN_RELATIVE_RSSI) ||
	    wpa_s->srp.relative_rssi_set == 0)
		return;

	params->relative_rssi_set = 1;
	params->relative_rssi = wpa_s->srp.relative_rssi;

	if (wpa_s->srp.relative_adjust_rssi == 0)
		return;

#ifdef CONFIG_BAND_5GHZ
	params->relative_adjust_band = wpa_s->srp.relative_adjust_band;
#endif	//CONFIG_BAND_5GHZ
	params->relative_adjust_rssi = wpa_s->srp.relative_adjust_rssi;
}
#endif	// defined ( CONFIG_SRP )

#if defined ( CONFIG_SCHED_SCAN )
/**
 * wpa_supplicant_req_sched_scan - Start a periodic scheduled scan
 * @wpa_s: Pointer to wpa_supplicant data
 * Returns: 0 is sched_scan was started or -1 otherwise
 *
 * This function is used to schedule periodic scans for neighboring
 * access points repeating the scan continuously.
 */
int wpa_supplicant_req_sched_scan(struct wpa_supplicant *wpa_s)
{
	struct wpa_driver_scan_params params;
	struct wpa_driver_scan_params *scan_params;
	enum wpa_states prev_state;
	struct wpa_ssid *ssid = NULL;
	struct wpabuf *extra_ie = NULL;
	int ret;
	unsigned int max_sched_scan_ssids;
	int wildcard = 0;
	int need_ssids;
	struct sched_scan_plan scan_plan;

	if (!wpa_s->sched_scan_supported)
		return -1;

	if (wpa_s->max_sched_scan_ssids > WPAS_MAX_SCAN_SSIDS)
		max_sched_scan_ssids = WPAS_MAX_SCAN_SSIDS;
	else
		max_sched_scan_ssids = wpa_s->max_sched_scan_ssids;

	if (max_sched_scan_ssids < 1
#ifdef CONFIG_SCAN_OFFLOAD
			|| wpa_s->conf->disable_scan_offload
#endif /* CONFIG_SCAN_OFFLOAD */
		)
		return -1;

	wpa_s->sched_scan_stop_req = 0;

	if (wpa_s->sched_scanning) {
		wpa_dbg(wpa_s, MSG_DEBUG, "Already sched scanning");
		return 0;
	}

	need_ssids = 0;
	for (ssid = wpa_s->conf->ssid; ssid; ssid = ssid->next) {
		if (!wpas_network_disabled(wpa_s, ssid) && !ssid->scan_ssid) {
			/* Use wildcard SSID to find this network */
			wildcard = 1;
		} else if (!wpas_network_disabled(wpa_s, ssid) &&
			   ssid->ssid_len)
			need_ssids++;

#ifdef CONFIG_WPS
		if (!wpas_network_disabled(wpa_s, ssid) &&
		    ssid->key_mgmt == WPA_KEY_MGMT_WPS) {
			/*
			 * Normal scan is more reliable and faster for WPS
			 * operations and since these are for short periods of
			 * time, the benefit of trying to use sched_scan would
			 * be limited.
			 */
			wpa_dbg(wpa_s, MSG_DEBUG, "Use normal scan instead of "
				"sched_scan for WPS");
			return -1;
		}
#endif /* CONFIG_WPS */
	}
	if (wildcard)
		need_ssids++;

	if (wpa_s->normal_scans < 3 &&
	    (need_ssids <= wpa_s->max_scan_ssids ||
	     wpa_s->max_scan_ssids >= (int) max_sched_scan_ssids)) {
		/*
		 * When normal scan can speed up operations, use that for the
		 * first operations before starting the sched_scan to allow
		 * user space sleep more. We do this only if the normal scan
		 * has functionality that is suitable for this or if the
		 * sched_scan does not have better support for multiple SSIDs.
		 */
		wpa_dbg(wpa_s, MSG_DEBUG, "Use normal scan instead of "
			"sched_scan for initial scans (normal_scans=%d)",
			wpa_s->normal_scans);
		return -1;
	}

	os_memset(&params, 0, sizeof(params));

	/* If we can't allocate space for the filters, we just don't filter */
	params.filter_ssids = os_calloc(wpa_s->max_match_sets,
					sizeof(struct wpa_driver_scan_filter));

	prev_state = wpa_s->wpa_state;
	if (wpa_s->wpa_state == WPA_DISCONNECTED ||
	    wpa_s->wpa_state == WPA_INACTIVE)
		wpa_supplicant_set_state(wpa_s, WPA_SCANNING);

	if (wpa_s->autoscan_params != NULL) {
		scan_params = wpa_s->autoscan_params;
		goto scan;
	}

	/* Find the starting point from which to continue scanning */
	ssid = wpa_s->conf->ssid;
	if (wpa_s->prev_sched_ssid) {
		while (ssid) {
			if (ssid == wpa_s->prev_sched_ssid) {
				ssid = ssid->next;
				break;
			}
			ssid = ssid->next;
		}
	}

	if (!ssid || !wpa_s->prev_sched_ssid) {
		wpa_dbg(wpa_s, MSG_DEBUG, "Beginning of SSID list");
		wpa_s->sched_scan_timeout = max_sched_scan_ssids * 2;
		wpa_s->first_sched_scan = 1;
		ssid = wpa_s->conf->ssid;
		wpa_s->prev_sched_ssid = ssid;
	}

	if (wildcard) {
		wpa_dbg(wpa_s, MSG_DEBUG, "Add wildcard SSID to sched_scan");
		params.num_ssids++;
	}

	while (ssid) {
		if (wpas_network_disabled(wpa_s, ssid))
			goto next;

		if (params.num_filter_ssids < wpa_s->max_match_sets &&
		    params.filter_ssids && ssid->ssid && ssid->ssid_len) {
			wpa_dbg(wpa_s, MSG_DEBUG, "add to filter ssid: %s",
				wpa_ssid_txt(ssid->ssid, ssid->ssid_len));
			os_memcpy(params.filter_ssids[params.num_filter_ssids].ssid,
				  ssid->ssid, ssid->ssid_len);
			params.filter_ssids[params.num_filter_ssids].ssid_len =
				ssid->ssid_len;
			params.num_filter_ssids++;
		} else if (params.filter_ssids && ssid->ssid && ssid->ssid_len)
		{
			wpa_dbg(wpa_s, MSG_DEBUG, "Not enough room for SSID "
				"filter for sched_scan - drop filter");
			os_free(params.filter_ssids);
			params.filter_ssids = NULL;
			params.num_filter_ssids = 0;
		}

		if (ssid->scan_ssid && ssid->ssid && ssid->ssid_len) {
			if (params.num_ssids == max_sched_scan_ssids)
				break; /* only room for broadcast SSID */
			wpa_dbg(wpa_s, MSG_DEBUG,
				"add to active scan ssid: %s",
				wpa_ssid_txt(ssid->ssid, ssid->ssid_len));
			params.ssids[params.num_ssids].ssid =
				ssid->ssid;
			params.ssids[params.num_ssids].ssid_len =
				ssid->ssid_len;
			params.num_ssids++;
			if (params.num_ssids >= max_sched_scan_ssids) {
				wpa_s->prev_sched_ssid = ssid;
				do {
					ssid = ssid->next;
				} while (ssid &&
					 (wpas_network_disabled(wpa_s, ssid) ||
					  !ssid->scan_ssid));
				break;
			}
		}

	next:
		wpa_s->prev_sched_ssid = ssid;
		ssid = ssid->next;
	}

	if (params.num_filter_ssids == 0) {
		os_free(params.filter_ssids);
		params.filter_ssids = NULL;
	}

	extra_ie = wpa_supplicant_extra_ies(wpa_s);
	if (extra_ie) {
		params.extra_ies = wpabuf_head(extra_ie);
		params.extra_ies_len = wpabuf_len(extra_ie);
	}

#ifdef CONFIG_SCAN_FILTER_RSSI
	if (wpa_s->conf->filter_rssi)
		params.filter_rssi = wpa_s->conf->filter_rssi;
#endif	// CONFIG_SCAN_FILTER_RSSI

	/* See if user specified frequencies. If so, scan only those. */
	if (wpa_s->conf->freq_list && !params.freqs) {
		wpa_dbg(wpa_s, MSG_DEBUG,
			"Optimize scan based on conf->freq_list");
		int_array_concat(&params.freqs, wpa_s->conf->freq_list);
	}

#ifdef CONFIG_MBO
	if (wpa_s->enable_oce & OCE_STA)
		params.oce_scan = 1;
#endif /* CONFIG_MBO */

	scan_params = &params;

scan:
	wpa_s->sched_scan_timed_out = 0;

	/*
	 * We cannot support multiple scan plans if the scan request includes
	 * too many SSID's, so in this case use only the last scan plan and make
	 * it run infinitely. It will be stopped by the timeout.
	 */
	if (wpa_s->sched_scan_plans_num == 1 ||
	    (wpa_s->sched_scan_plans_num && !ssid && wpa_s->first_sched_scan)) {
		params.sched_scan_plans = wpa_s->sched_scan_plans;
		params.sched_scan_plans_num = wpa_s->sched_scan_plans_num;
	} else if (wpa_s->sched_scan_plans_num > 1) {
		wpa_dbg(wpa_s, MSG_DEBUG,
			"Too many SSIDs. Default to using single scheduled_scan plan");
		params.sched_scan_plans =
			&wpa_s->sched_scan_plans[wpa_s->sched_scan_plans_num -
						 1];
		params.sched_scan_plans_num = 1;
	} else {
		if (wpa_s->conf->sched_scan_interval)
			scan_plan.interval = wpa_s->conf->sched_scan_interval;
		else
			scan_plan.interval = 10;

		if (scan_plan.interval > wpa_s->max_sched_scan_plan_interval) {
			wpa_printf(MSG_WARNING,
				   "Scan interval too long(%u), use the maximum allowed(%u)",
				   scan_plan.interval,
				   wpa_s->max_sched_scan_plan_interval);
			scan_plan.interval =
				wpa_s->max_sched_scan_plan_interval;
		}

		scan_plan.iterations = 0;
		params.sched_scan_plans = &scan_plan;
		params.sched_scan_plans_num = 1;
	}

	params.sched_scan_start_delay = wpa_s->conf->sched_scan_start_delay;

	if (ssid || !wpa_s->first_sched_scan) {
		wpa_dbg(wpa_s, MSG_DEBUG,
			"Starting sched scan after %u seconds: interval %u timeout %d",
			params.sched_scan_start_delay,
			params.sched_scan_plans[0].interval,
			wpa_s->sched_scan_timeout);
	} else {
		wpa_dbg(wpa_s, MSG_DEBUG,
			"Starting sched scan after %u seconds (no timeout)",
			params.sched_scan_start_delay);
	}

	wpa_setband_scan_freqs(wpa_s, scan_params);

#if defined ( CONFIG_SCHED_SCAN )
	if ((wpa_s->mac_addr_rand_enable & MAC_ADDR_RAND_SCHED_SCAN) &&
	    wpa_s->wpa_state <= WPA_SCANNING)
		wpa_setup_mac_addr_rand_params(&params,
					       wpa_s->mac_addr_sched_scan);
#endif	// defined ( CONFIG_SCHED_SCAN )

#if defined ( CONFIG_SRP )
	wpa_scan_set_relative_rssi_params(wpa_s, scan_params);
#endif	// defined ( CONFIG_SRP )

	ret = wpa_supplicant_start_sched_scan(wpa_s, scan_params);
	wpabuf_free(extra_ie);
	os_free(params.filter_ssids);
	os_free(params.mac_addr);
	if (ret) {
		wpa_msg(wpa_s, MSG_WARNING, "Failed to initiate sched scan");
		if (prev_state != wpa_s->wpa_state)
			wpa_supplicant_set_state(wpa_s, prev_state);
		return ret;
	}

	/* If we have more SSIDs to scan, add a timeout so we scan them too */
	if (ssid || !wpa_s->first_sched_scan) {
		wpa_s->sched_scan_timed_out = 0;
		eloop_register_timeout(wpa_s->sched_scan_timeout, 0,
				       wpa_supplicant_sched_scan_timeout,
				       wpa_s, NULL);
		wpa_s->first_sched_scan = 0;
		wpa_s->sched_scan_timeout /= 2;
		params.sched_scan_plans[0].interval *= 2;
		if ((unsigned int) wpa_s->sched_scan_timeout <
		    params.sched_scan_plans[0].interval ||
		    params.sched_scan_plans[0].interval >
		    wpa_s->max_sched_scan_plan_interval) {
			params.sched_scan_plans[0].interval = 10;
			wpa_s->sched_scan_timeout = max_sched_scan_ssids * 2;
		}
	}

	/* If there is no more ssids, start next time from the beginning */
	if (!ssid)
		wpa_s->prev_sched_ssid = NULL;

	return 0;
}
#endif // CONFIG_SCHED_SCAN

/**
 * wpa_supplicant_cancel_scan - Cancel a scheduled scan request
 * @wpa_s: Pointer to wpa_supplicant data
 *
 * This function is used to cancel a scan request scheduled with
 * wpa_supplicant_req_scan().
 */
void wpa_supplicant_cancel_scan(struct wpa_supplicant *wpa_s)
{
	RX_FUNC_START("");
	wpa_dbg(wpa_s, MSG_DEBUG, "Cancelling scan request");
	eloop_cancel_timeout(wpa_supplicant_scan, wpa_s, NULL);
	RX_FUNC_END("");
}

#if defined ( CONFIG_SCHED_SCAN )
/**
 * wpa_supplicant_cancel_delayed_sched_scan - Stop a delayed scheduled scan
 * @wpa_s: Pointer to wpa_supplicant data
 *
 * This function is used to stop a delayed scheduled scan.
 */
void wpa_supplicant_cancel_delayed_sched_scan(struct wpa_supplicant *wpa_s)
{
	RX_FUNC_START("");
	if (!wpa_s->sched_scan_supported)
		return;

	wpa_dbg(wpa_s, MSG_DEBUG, "Cancelling delayed sched scan");
	eloop_cancel_timeout(wpa_supplicant_delayed_sched_scan_timeout,
			     wpa_s, NULL);
	RX_FUNC_END("");
}


/**
 * wpa_supplicant_cancel_sched_scan - Stop running scheduled scans
 * @wpa_s: Pointer to wpa_supplicant data
 *
 * This function is used to stop a periodic scheduled scan.
 */
void wpa_supplicant_cancel_sched_scan(struct wpa_supplicant *wpa_s)
{
	RX_FUNC_START("");
	if (!wpa_s->sched_scanning)
		return;

	if (wpa_s->sched_scanning)
		wpa_s->sched_scan_stop_req = 1;

	wpa_dbg(wpa_s, MSG_DEBUG, "Cancelling sched scan");
	eloop_cancel_timeout(wpa_supplicant_sched_scan_timeout, wpa_s, NULL);
	wpa_supplicant_stop_sched_scan(wpa_s);
	RX_FUNC_END("");
}
#endif // CONFIG_SCHED_SCAN

/**
 * wpa_supplicant_notify_scanning - Indicate possible scan state change
 * @wpa_s: Pointer to wpa_supplicant data
 * @scanning: Whether scanning is currently in progress
 *
 * This function is to generate scanning notifycations. It is called whenever
 * there may have been a change in scanning (scan started, completed, stopped).
 * wpas_notify_scanning() is called whenever the scanning state changed from the
 * previously notified state.
 */
void wpa_supplicant_notify_scanning(struct wpa_supplicant *wpa_s,
				    int scanning)
{
	if (wpa_s->scanning != scanning) {
		wpa_s->scanning = scanning;
#if 0   /* by Bright : Merge 2.7 */	//MODIFY_SUPPLICANT_FOR_FREERTOS
		wpas_notify_scanning(wpa_s);
#endif	// 0
	}
}

static int wpa_scan_get_max_rate(const struct wpa_scan_res *res)
{
	int rate = 0;
	const u8 *ie;
	int i;

	ie = wpa_scan_get_ie(res, WLAN_EID_SUPP_RATES);
	for (i = 0; ie && i < ie[1]; i++) {
		if ((ie[i + 2] & 0x7f) > rate)
			rate = ie[i + 2] & 0x7f;
	}

	ie = wpa_scan_get_ie(res, WLAN_EID_EXT_SUPP_RATES);
	for (i = 0; ie && i < ie[1]; i++) {
		if ((ie[i + 2] & 0x7f) > rate)
			rate = ie[i + 2] & 0x7f;
	}

	return rate;
}


/**
 * wpa_scan_get_ie - Fetch a specified information element from a scan result
 * @res: Scan result entry
 * @ie: Information element identitifier (WLAN_EID_*)
 * Returns: Pointer to the information element (id field) or %NULL if not found
 *
 * This function returns the first matching information element in the scan
 * result.
 */
const u8 * wpa_scan_get_ie(const struct wpa_scan_res *res, u8 ie)
{	/* FC9000 supplicant 2.6 */	//MODIFY_SUPPLICANT_FOR_FREERTOS
	const u8 *end=NULL, *pos=NULL;
#ifndef CONFIG_SCAN_UMAC_HEAP_ALLOC
#ifdef CONFIG_SCAN_RESULT_OPTIMIZE
	pos = (const u8 *) (res + 1);
#else /* CONFIG_SCAN_RESULT_OPTIMIZE */
	pos = (const u8 *) (res + 1);
#endif /* CONFIG_SCAN_RESULT_OPTIMIZE */
#else	//CONFIG_SCAN_UMAC_HEAP_ALLOC
	if(res->ie == NULL)
		return NULL;
	pos = (const u8 *)res->ie;
#endif	//CONFIG_SCAN_UMAC_HEAP_ALLOC
	end = pos + res->ie_len;

	while (pos + 1 < end) {
		if (pos + 2 + pos[1] > end)
			break;
		if (pos[0] == ie)
			return pos;
		pos += 2 + pos[1];
	}

	return (const u8*)NULL;
}


/**
 * wpa_scan_get_vendor_ie - Fetch vendor information element from a scan result
 * @res: Scan result entry
 * @vendor_type: Vendor type (four octets starting the IE payload)
 * Returns: Pointer to the information element (id field) or %NULL if not found
 *
 * This function returns the first matching information element in the scan
 * result.
 */
const u8 * wpa_scan_get_vendor_ie(const struct wpa_scan_res *res,
				  u32 vendor_type)
{	/* FC9000 supplicant 2.6 */	//MODIFY_SUPPLICANT_FOR_FREERTOS
	const u8 *end=NULL, *pos=NULL;

#ifndef CONFIG_SCAN_UMAC_HEAP_ALLOC
#ifdef CONFIG_SCAN_RESULT_OPTIMIZE
	pos = (const u8 *) (res + 1);
#else /* CONFIG_SCAN_RESULT_OPTIMIZE */
	pos = (const u8 *) (res + 1);
#endif /* CONFIG_SCAN_RESULT_OPTIMIZE */
#else
	if(res->ie == NULL)
		return (const u8*)NULL;
	pos = (const u8 *) res->ie;
#endif
	end = pos + res->ie_len;

	while (pos + 1 < end) {
		if (pos + 2 + pos[1] > end)
			break;
		if (pos[0] == WLAN_EID_VENDOR_SPECIFIC && pos[1] >= 4 &&
					vendor_type == WPA_GET_BE32(&pos[2]))
			return pos;
		pos += 2 + pos[1];
	}

	return NULL;
}


/**
 * wpa_scan_get_vendor_ie_beacon - Fetch vendor information from a scan result
 * @res: Scan result entry
 * @vendor_type: Vendor type (four octets starting the IE payload)
 * Returns: Pointer to the information element (id field) or %NULL if not found
 *
 * This function returns the first matching information element in the scan
 * result.
 *
 * This function is like wpa_scan_get_vendor_ie(), but uses IE buffer only
 * from Beacon frames instead of either Beacon or Probe Response frames.
 */
const u8 * wpa_scan_get_vendor_ie_beacon(const struct wpa_scan_res *res,
					 u32 vendor_type)
{	/* FC9000 supplicant 2.6 */	//MODIFY_SUPPLICANT_FOR_FREERTOS
	const u8 *end, *pos;

	if (res->beacon_ie_len == 0)
		return NULL;

#ifndef CONFIG_SCAN_UMAC_HEAP_ALLOC
#ifdef CONFIG_SCAN_RESULT_OPTIMIZE
	pos = (const u8 *) (res + 1);
	pos += res->ie_len;
#else /* CONFIG_SCAN_RESULT_OPTIMIZE */
	pos = (const u8 *) (res + 1);
	pos += res->ie_len;
#endif /* CONFIG_SCAN_RESULT_OPTIMIZE */
#else
	if (res->beacon_ie == NULL)
		return NULL;

	pos = (const u8 *) res->beacon_ie;
#endif
	end = pos + res->beacon_ie_len;

	while (pos + 1 < end) {
		if (pos + 2 + pos[1] > end)
			break;
		if (pos[0] == WLAN_EID_VENDOR_SPECIFIC && pos[1] >= 4 &&
						vendor_type == WPA_GET_BE32(&pos[2]))
			return pos;
		pos += 2 + pos[1];
	}

	return NULL;
}


/**
 * wpa_scan_get_vendor_ie_multi - Fetch vendor IE data from a scan result
 * @res: Scan result entry
 * @vendor_type: Vendor type (four octets starting the IE payload)
 * Returns: Pointer to the information element payload or %NULL if not found
 *
 * This function returns concatenated payload of possibly fragmented vendor
 * specific information elements in the scan result. The caller is responsible
 * for freeing the returned buffer.
 */
struct wpabuf * wpa_scan_get_vendor_ie_multi(const struct wpa_scan_res *res,
					     u32 vendor_type)
{
	struct wpabuf *buf;
	const u8 *end, *pos;

	buf = wpabuf_alloc(res->ie_len);
	if (buf == NULL)
		return NULL;

#ifndef CONFIG_SCAN_UMAC_HEAP_ALLOC
	pos = (const u8 *) (res + 1);
#else
	if(res->ie == NULL)
	{
		wpabuf_free(buf);
		return NULL;
	}
	pos = (const u8 *)res->ie;
#endif	// CONFIG_SCAN_UMAC_HEAP_ALLOC
	end = pos + res->ie_len;

	/* FC9000 supplicant 2.6 */	//MODIFY_SUPPLICANT_FOR_FREERTOS
	while (pos + 1 < end) {
		if (pos + 2 + pos[1] > end)
			break;
		if (pos[0] == WLAN_EID_VENDOR_SPECIFIC && pos[1] >= 4 &&
							vendor_type == WPA_GET_BE32(&pos[2]))
			wpabuf_put_data(buf, pos + 2 + 4, pos[1] - 4);
		pos += 2 + pos[1];
	}

	if (wpabuf_len(buf) == 0) {
		wpabuf_free(buf);
		buf = NULL;
	}

	return buf;
}

/*
 * Channels with a great SNR can operate at full rate. What is a great SNR?
 * This doc https://supportforums.cisco.com/docs/DOC-12954 says, "the general
 * rule of thumb is that any SNR above 20 is good." This one
 * http://www.cisco.com/en/US/tech/tk722/tk809/technologies_q_and_a_item09186a00805e9a96.shtml#qa23
 * recommends 25 as a minimum SNR for 54 Mbps data rate. The estimates used in
 * scan_est_throughput() allow even smaller SNR values for the maximum rates
 * (21 for 54 Mbps, 22 for VHT80 MCS9, 24 for HT40 and HT20 MCS7). Use 25 as a
 * somewhat conservative value here.
 */
#define GREAT_SNR 25

#ifdef CONFIG_TOGGLE_SCAN_SORT_TYPE
enum compar_type {
	COMPAR_SECURITY,
	//COMPAR_RATE,
	COMPAR_SNR
};

static int wpa_scan_compar(int wpa_a, int wpa_b, int snr_a, int snr_b, struct wpa_scan_res * wa, struct wpa_scan_res * wb, enum compar_type type)
{
	switch(type) {
		case COMPAR_SECURITY:
			if (wpa_b && !wpa_a)
				return 1;
			if (!wpa_b && wpa_a)
				return -1;

			/* privacy support preferred */
			if ((wa->caps & IEEE80211_CAP_PRIVACY) == 0 &&
				(wb->caps & IEEE80211_CAP_PRIVACY))
				return 1;
			if ((wa->caps & IEEE80211_CAP_PRIVACY) &&
				(wb->caps & IEEE80211_CAP_PRIVACY) == 0)
				return -1;
#if 0 //[[ trinity_0170811_BEGIN -- test
		case COMPAR_RATE:
			/* best/max rate preferred if SNR close enough */
			if ((snr_a && snr_b && abs(snr_b - snr_a) < 5) ||
				(wa->qual && wb->qual && abs(wb->qual - wa->qual) < 10)) {
				maxrate_a = wpa_scan_get_max_rate(wa);
				maxrate_b = wpa_scan_get_max_rate(wb);
				if (maxrate_a != maxrate_b)
					return maxrate_b - maxrate_a;
			}
#endif //]] trinity_0170811_END
			/* fall through */

		case COMPAR_SNR:
			if (snr_b == snr_a)
				return wb->qual - wa->qual;
			return snr_b - snr_a;
	}

	return 0;
}

static enum compar_type current_compar_type = COMPAR_SECURITY;

static int wpa_scan_result_compar_advanced(const void *a, const void *b)
{
	struct wpa_scan_res **_wa = (void *) a;
	struct wpa_scan_res **_wb = (void *) b;
	struct wpa_scan_res *wa = *_wa;
	struct wpa_scan_res *wb = *_wb;
	int wpa_a, wpa_b;
	int snr_a, snr_b;
	enum compar_type type1, type2;
	int temp_res = 0;

	type1 = current_compar_type;
	if(current_compar_type == COMPAR_SECURITY) {
		type2 = COMPAR_SNR;
	} else {
		type2 = COMPAR_SECURITY;
	}

	/* WPA/WPA2 support preferred */
	wpa_a = wpa_scan_get_vendor_ie(wa, WPA_IE_VENDOR_TYPE) != NULL ||
		wpa_scan_get_ie(wa, WLAN_EID_RSN) != NULL;
	wpa_b = wpa_scan_get_vendor_ie(wb, WPA_IE_VENDOR_TYPE) != NULL ||
		wpa_scan_get_ie(wb, WLAN_EID_RSN) != NULL;

	if ((wa->flags & wb->flags & WPA_SCAN_LEVEL_DBM) &&
		!((wa->flags | wb->flags) & WPA_SCAN_NOISE_INVALID)) {
		snr_a = MIN(wa->level - wa->noise, GREAT_SNR);
		snr_b = MIN(wb->level - wb->noise, GREAT_SNR);
	} else {
		/* Not suitable information to calculate SNR, so use level */
		snr_a = wa->level;
		snr_b = wb->level;
	}

	temp_res = wpa_scan_compar(wpa_a, wpa_b, snr_a, snr_b, wa, wb, type1);
	if(temp_res)
		return temp_res;

	temp_res = wpa_scan_compar(wpa_a, wpa_b, snr_a, snr_b, wa, wb, type2);
	if(temp_res)
		return temp_res;

#if 0 //[[ trinity_0170811_BEGIN -- test
	temp_res = wpa_scan_compar(wpa_a, wpa_b, snr_a, snr_b, wa, wb, type3);
	if(temp_res)
		return temp_res;
#endif //]] trinity_0170811_END

	return 0;
#undef MIN
}

#else  /* CONFIG_TOGGLE_SCAN_SORT_TYPE */

/* Compare function for sorting scan results. Return >0 if @b is considered
 * better. */
static int wpa_scan_result_compar(const void *a, const void *b)
{
#define MIN(a,b) a < b ? a : b
	struct wpa_scan_res **_wa = (void *) a;
	struct wpa_scan_res **_wb = (void *) b;
	struct wpa_scan_res *wa = *_wa;
	struct wpa_scan_res *wb = *_wb;
	int wpa_a, wpa_b;
	int snr_a, snr_b, snr_a_full, snr_b_full;

	/* WPA/WPA2 support preferred */
	wpa_a = wpa_scan_get_vendor_ie(wa, WPA_IE_VENDOR_TYPE) != NULL ||
		wpa_scan_get_ie(wa, WLAN_EID_RSN) != NULL;
	wpa_b = wpa_scan_get_vendor_ie(wb, WPA_IE_VENDOR_TYPE) != NULL ||
		wpa_scan_get_ie(wb, WLAN_EID_RSN) != NULL;

	if (wpa_b && !wpa_a)
		return 1;
	if (!wpa_b && wpa_a)
		return -1;

	/* privacy support preferred */
	if ((wa->caps & IEEE80211_CAP_PRIVACY) == 0 &&
	    (wb->caps & IEEE80211_CAP_PRIVACY))
		return 1;
	if ((wa->caps & IEEE80211_CAP_PRIVACY) &&
	    (wb->caps & IEEE80211_CAP_PRIVACY) == 0)
		return -1;

	if (wa->flags & wb->flags & WPA_SCAN_LEVEL_DBM) {
		snr_a_full = wa->snr;
		snr_a = MIN(wa->snr, GREAT_SNR);
		snr_b_full = wb->snr;
		snr_b = MIN(wb->snr, GREAT_SNR);
	} else {
		/* Level is not in dBm, so we can't calculate
		 * SNR. Just use raw level (units unknown). */
		snr_a = snr_a_full = wa->level;
		snr_b = snr_b_full = wb->level;
	}

	/* If SNR is close, decide by max rate or frequency band. For cases
	 * involving the 6 GHz band, use the throughput estimate irrespective
	 * of the SNR difference since the LPI/VLP rules may result in
	 * significant differences in SNR for cases where the estimated
	 * throughput can be considerably higher with the lower SNR. */
	if (snr_a && snr_b && (abs(snr_b - snr_a) < 7 ||
			       is_6ghz_freq(wa->freq) ||
			       is_6ghz_freq(wb->freq))) {
		if (wa->est_throughput != wb->est_throughput)
			return (int) wb->est_throughput -
				(int) wa->est_throughput;
	}
	if ((snr_a && snr_b && abs(snr_b - snr_a) < 5) ||
	    (wa->qual && wb->qual && abs(wb->qual - wa->qual) < 10)) {
		if (is_6ghz_freq(wa->freq) ^ is_6ghz_freq(wb->freq))
			return is_6ghz_freq(wa->freq) ? -1 : 1;
		if (IS_5GHZ(wa->freq) ^ IS_5GHZ(wb->freq))
			return IS_5GHZ(wa->freq) ? -1 : 1;
	}

	/* all things being equal, use SNR; if SNRs are
	 * identical, use quality values since some drivers may only report
	 * that value and leave the signal level zero */
	if (snr_b_full == snr_a_full)
		return wb->qual - wa->qual;
	return snr_b_full - snr_a_full;
#undef MIN
}
#endif /* CONFIG_TOGGLE_SCAN_SORT_TYPE */


#ifdef CONFIG_WPS
/* Compare function for sorting scan results when searching a WPS AP for
 * provisioning. Return >0 if @b is considered better. */
static int wpa_scan_result_wps_compar(const void *a, const void *b)
{
	struct wpa_scan_res **_wa = (void *) a;
	struct wpa_scan_res **_wb = (void *) b;
	struct wpa_scan_res *wa = *_wa;
	struct wpa_scan_res *wb = *_wb;
	int uses_wps_a, uses_wps_b;
	struct wpabuf *wps_a, *wps_b;
	int res;

	/* Optimization - check WPS IE existence before allocated memory and
	 * doing full reassembly. */
	uses_wps_a = wpa_scan_get_vendor_ie(wa, WPS_IE_VENDOR_TYPE) != NULL;
	uses_wps_b = wpa_scan_get_vendor_ie(wb, WPS_IE_VENDOR_TYPE) != NULL;
	if (uses_wps_a && !uses_wps_b)
		return -1;
	if (!uses_wps_a && uses_wps_b)
		return 1;

	if (uses_wps_a && uses_wps_b) {
		wps_a = wpa_scan_get_vendor_ie_multi(wa, WPS_IE_VENDOR_TYPE);
		wps_b = wpa_scan_get_vendor_ie_multi(wb, WPS_IE_VENDOR_TYPE);
		res = wps_ap_priority_compar(wps_a, wps_b);
		wpabuf_free(wps_a);
		wpabuf_free(wps_b);
		if (res)
			return res;
	}

	/*
	 * Do not use current AP security policy as a sorting criteria during
	 * WPS provisioning step since the AP may get reconfigured at the
	 * completion of provisioning.
	 */

	/* all things being equal, use signal level; if signal levels are
	 * identical, use quality values since some drivers may only report
	 * that value and leave the signal level zero */
	if (wb->level == wa->level)
		return wb->qual - wa->qual;
	return wb->level - wa->level;
}
#endif /* CONFIG_WPS */


static void dump_scan_res(struct wpa_scan_results *scan_res)
{	//MODIFY_SUPPLICANT_FOR_FREERTOS
	size_t i;

	ra6wx_scan_prt("<%s> res=0x%p, num=%d\n", __func__, scan_res->res, scan_res->num);

	if (scan_res->res == NULL || scan_res->num == 0) {
		ra6wx_scan_prt("<%s> None...\n", __func__);
		return;
	}

	ra6wx_scan_prt("<%s> Sorted Scan Lists ------\n", __func__);

	for (i = 0; i < scan_res->num; i++) {
		struct wpa_scan_res *r = scan_res->res[i];
#ifdef ENABLE_SCAN_DBG
		int maxrate = wpa_scan_get_max_rate(r);
		int wpa = (int)(wpa_scan_get_vendor_ie(r, WPA_IE_VENDOR_TYPE) != NULL);
		int wpa2 = (int)(wpa_scan_get_ie(r, WLAN_EID_RSN) != NULL);
#endif /* ENABLE_SCAN_DBG */

		if ((r->flags & (WPA_SCAN_LEVEL_DBM | WPA_SCAN_NOISE_INVALID)) == WPA_SCAN_LEVEL_DBM) {
#ifdef	ENABLE_SCAN_DBG
			int snr = r->level - r->noise;
#endif	/* ENABLE_SCAN_DBG */

#ifdef	ENABLE_SCAN_DBG
			ra6wx_scan_prt(MACSTR " freq=%d level=%d snr=%d%s "
#else
			ra6wx_scan_prt(MACSTR " freq=%d level=%d "
#endif	/* ENABLE_SCAN_DBG */
				"flags=0x%x age=%3u caps=0x%03x\n",
				   MAC2STR(r->bssid), r->freq,
				   r->level,
#ifdef	ENABLE_SCAN_DBG
				   snr,
				   snr >= GREAT_SNR ? "*" : "", r->flags,
#endif	/* ENABLE_SCAN_DBG */
				   r->age, r->caps);
		} else {
#if 1
			ra6wx_scan_prt(" 		%02d) " MACSTR
				" freq=%d level=%d flags=0x%x age=%3u, maxrate=%3d, caps=0x%03x, %s%s%s%s\n",
				i, MAC2STR(r->bssid),
				r->freq, r->level, r->flags, r->age, maxrate, r->caps,
				wpa ? "WPA " : "",
				wpa2 ? "WPA2 " : "",
				(!wpa && !wpa2 && r->caps & IEEE80211_CAP_PRIVACY) ? "WEP":"",
				(!wpa && !wpa2 && !(r->caps & IEEE80211_CAP_PRIVACY)) ? "OPEN":""
				);
#else
			ra6wx_scan_prt("                 %d) " MACSTR
				" freq=%d level=%d flags=0x%x age=%u, wpa=%d, maxrate=%d\n",
				i, MAC2STR(r->bssid),
				r->freq, r->level, r->flags, r->age, wpa, maxrate);
#endif /* 1 */
		}
	}
}

#ifdef CONFIG_STA_BSSID_FILTER
/**
 * wpa_supplicant_filter_bssid_match - Is the specified BSSID allowed
 * @wpa_s: Pointer to wpa_supplicant data
 * @bssid: BSSID to check
 * Returns: 0 if the BSSID is filtered or 1 if not
 *
 * This function is used to filter out specific BSSIDs from scan reslts mainly
 * for testing purposes (SET bssid_filter ctrl_iface command).
 */
int wpa_supplicant_filter_bssid_match(struct wpa_supplicant *wpa_s,
				      const u8 *bssid)
{
	size_t i;

	if (wpa_s->bssid_filter == NULL)
		return 1;

	for (i = 0; i < wpa_s->bssid_filter_count; i++) {
		if (os_memcmp(wpa_s->bssid_filter + i * ETH_ALEN, bssid,
			      ETH_ALEN) == 0)
			return 1;
	}

	return 0;
}
#endif	// CONFIG_STA_BSSID_FILTER

#ifdef CONFIG_STA_COUNTRY_CODE		//MODIFY_SUPPLICANT_FOR_FREERTOS
static int wpa_supplicant_filter_country_match(struct wpa_freq_range_list ranges,
                       int freq)
{
	if(freq_range_list_includes(&ranges, freq)) {
		return 1;
	}

	ra6wx_scan_prt("[%s] freq %d is not included in current country\n", __func__, freq);
	return 0;
}
#endif /* CONFIG_STA_COUNTRY_CODE */

#ifdef CONFIG_REUSED_UMAC_BSS_LIST		//MODIFY_SUPPLICANT_FOR_FREERTOS
#if 0
extern void rdev_bss_lock();
extern void rdev_bss_unlock();
#endif
#endif

#define UMAC_MEM_USE_TX_MEM		//MODIFY_SUPPLICANT_FOR_FREERTOS

void filter_scan_res(struct wpa_supplicant *wpa_s,
		     struct wpa_scan_results *res)
{	//MODIFY_SUPPLICANT_FOR_FREERTOS

#ifdef CONFIG_STA_BSSID_FILTER
	size_t i, j;
#endif
#ifdef CONFIG_STA_COUNTRY_CODE
	struct wpa_freq_range_list ranges;
    ranges = wpa_s->conf->country_range;
#endif /* CONFIG_STA_COUNTRY_CODE */

#ifdef CONFIG_STA_BSSID_FILTER
	if (wpa_s->bssid_filter == NULL)
		return;

	for (i = 0, j = 0; i < res->num; i++) {
		if (wpa_supplicant_filter_bssid_match(wpa_s,
						      res->res[i]->bssid)
#ifdef CONFIG_STA_COUNTRY_CODE
		     && wpa_supplicant_filter_country_match(ranges,
						      res->res[i]->freq)
#endif /* CONFIG_STA_COUNTRY_CODE */
		) {
			res->res[j++] = res->res[i];
		} else {
			os_free(res->res[i]);
			res->res[i] = NULL;
		}
	}

	if (res->num != j) {
		wpa_printf_dbg(MSG_DEBUG, "Filtered out %d scan results",
			   (int) (res->num - j));
		res->num = j;
	}
#else /* CONFIG_STA_BSSID_FILTER */
#ifdef CONFIG_STA_COUNTRY_CODE
#ifdef CONFIG_REUSED_UMAC_BSS_LIST
#if 0
	rdev_bss_lock();
#endif
	for( int i = 0 ; i < (int) res->num; i++)
	{
		if (!wpa_supplicant_filter_country_match(ranges, res->res[i]->freq)) 
		{
			struct wpa_scan_res *temp= NULL;

			temp = res->res[i];
			temp->internal_bss = NULL;
			temp->ie = NULL;
			temp->beacon_ie = NULL;
			umac_heap_free(res->res[i]);
			res->res[i] = NULL;
				
			os_memmove(&res->res[i], &res->res[i + 1],
				   (res->num - i - 1)* sizeof(struct wpa_bss *));
			res->res[res->num-1] = NULL;
			res->num--;
			i--;
		}
	}
//	rdev_bss_unlock();
#else	//CONFIG_REUSED_UMAC_BSS_LIST
	for (i = 0, j = 0; i < res->num; i++) {
		if (wpa_supplicant_filter_country_match(ranges,
											  res->res[i]->freq)) {
			res->res[j++] = res->res[i];
		} else {
			umac_heap_free(res->res[i]);
			res->res[i] = NULL;
		}
	}

	if (res->num != j) {
		ra6wx_scan_prt("[%s] Filtered out %d scan results\n",
			   __func__, (int) (res->num - j));
		res->num = j;
	}
#endif	//CONFIG_REUSED_UMAC_BSS_LIST
#endif /* CONFIG_STA_COUNTRY_CODE */
#endif /* CONFIG_STA_BSSID_FILTER */
}

#ifdef  CONFIG_SUPP27_SCAN

/*
 * Noise floor values to use when we have signal strength
 * measurements, but no noise floor measurements. These values were
 * measured in an office environment with many APs.
 */
#define DEFAULT_NOISE_FLOOR_2GHZ (-89)		//MODIFY_SUPPLICANT_FOR_FREERTOS
#define DEFAULT_NOISE_FLOOR_5GHZ (-92)		//MODIFY_SUPPLICANT_FOR_FREERTOS

void scan_snr(struct wpa_scan_res *res)
{
	if (res->flags & WPA_SCAN_NOISE_INVALID) {
		res->noise = is_6ghz_freq(res->freq) ?
			DEFAULT_NOISE_FLOOR_6GHZ :
			(IS_5GHZ(res->freq) ?
			 DEFAULT_NOISE_FLOOR_5GHZ : DEFAULT_NOISE_FLOOR_2GHZ);
	}

#if 0   //[[ trinity_0171215_BEGIN -- test
	if (res->flags & WPA_SCAN_LEVEL_DBM) {
		res->snr = res->level - res->noise;
	} else {
		/* Level is not in dBm, so we can't calculate
		 * SNR. Just use raw level (units unknown). */
		res->snr = res->level;
	}
#endif //0   //[[ trinity_0171215_BEGIN -- test
}


/* Minimum SNR required to achieve a certain bitrate. */
struct minsnr_bitrate_entry {
	int minsnr;
	unsigned int bitrate; /* in Mbps */
};

/* VHT needs to be enabled in order to achieve MCS8 and MCS9 rates. */
static const int vht_mcs = 8;

static const struct minsnr_bitrate_entry vht20_table[] = {
	{ 0, 0 },
	{ 2, 6500 },   /* HT20 MCS0 */
	{ 5, 13000 },  /* HT20 MCS1 */
	{ 9, 19500 },  /* HT20 MCS2 */
	{ 11, 26000 }, /* HT20 MCS3 */
	{ 15, 39000 }, /* HT20 MCS4 */
	{ 18, 52000 }, /* HT20 MCS5 */
	{ 20, 58500 }, /* HT20 MCS6 */
	{ 25, 65000 }, /* HT20 MCS7 */
	{ 29, 78000 }, /* VHT20 MCS8 */
	{ -1, 78000 }  /* SNR > 29 */
};

static const struct minsnr_bitrate_entry vht40_table[] = {
	{ 0, 0 },
	{ 5, 13500 },   /* HT40 MCS0 */
	{ 8, 27000 },   /* HT40 MCS1 */
	{ 12, 40500 },  /* HT40 MCS2 */
	{ 14, 54000 },  /* HT40 MCS3 */
	{ 18, 81000 },  /* HT40 MCS4 */
	{ 21, 108000 }, /* HT40 MCS5 */
	{ 23, 121500 }, /* HT40 MCS6 */
	{ 28, 135000 }, /* HT40 MCS7 */
	{ 32, 162000 }, /* VHT40 MCS8 */
	{ 34, 180000 }, /* VHT40 MCS9 */
	{ -1, 180000 }  /* SNR > 34 */
};

static const struct minsnr_bitrate_entry vht80_table[] = {
	{ 0, 0 },
	{ 8, 29300 },   /* VHT80 MCS0 */
	{ 11, 58500 },  /* VHT80 MCS1 */
	{ 15, 87800 },  /* VHT80 MCS2 */
	{ 17, 117000 }, /* VHT80 MCS3 */
	{ 21, 175500 }, /* VHT80 MCS4 */
	{ 24, 234000 }, /* VHT80 MCS5 */
	{ 26, 263300 }, /* VHT80 MCS6 */
	{ 31, 292500 }, /* VHT80 MCS7 */
	{ 35, 351000 }, /* VHT80 MCS8 */
	{ 37, 390000 }, /* VHT80 MCS9 */
	{ -1, 390000 }  /* SNR > 37 */
};


static const struct minsnr_bitrate_entry vht160_table[] = {
	{ 0, 0 },
	{ 11, 58500 },  /* VHT160 MCS0 */
	{ 14, 117000 }, /* VHT160 MCS1 */
	{ 18, 175500 }, /* VHT160 MCS2 */
	{ 20, 234000 }, /* VHT160 MCS3 */
	{ 24, 351000 }, /* VHT160 MCS4 */
	{ 27, 468000 }, /* VHT160 MCS5 */
	{ 29, 526500 }, /* VHT160 MCS6 */
	{ 34, 585000 }, /* VHT160 MCS7 */
	{ 38, 702000 }, /* VHT160 MCS8 */
	{ 40, 780000 }, /* VHT160 MCS9 */
	{ -1, 780000 }  /* SNR > 37 */
};


static const struct minsnr_bitrate_entry he20_table[] = {
	{ 0, 0 },
	{ 2, 8600 },    /* HE20 MCS0 */
	{ 5, 17200 },   /* HE20 MCS1 */
	{ 9, 25800 },   /* HE20 MCS2 */
	{ 11, 34400 },  /* HE20 MCS3 */
	{ 15, 51600 },  /* HE20 MCS4 */
	{ 18, 68800 },  /* HE20 MCS5 */
	{ 20, 77400 },  /* HE20 MCS6 */
	{ 25, 86000 },  /* HE20 MCS7 */
	{ 29, 103200 }, /* HE20 MCS8 */
	{ 31, 114700 }, /* HE20 MCS9 */
	{ 34, 129000 }, /* HE20 MCS10 */
	{ 36, 143400 }, /* HE20 MCS11 */
	{ -1, 143400 }  /* SNR > 29 */
};

static const struct minsnr_bitrate_entry he40_table[] = {
	{ 0, 0 },
	{ 5, 17200 },   /* HE40 MCS0 */
	{ 8, 34400 },   /* HE40 MCS1 */
	{ 12, 51600 },  /* HE40 MCS2 */
	{ 14, 68800 },  /* HE40 MCS3 */
	{ 18, 103200 }, /* HE40 MCS4 */
	{ 21, 137600 }, /* HE40 MCS5 */
	{ 23, 154900 }, /* HE40 MCS6 */
	{ 28, 172100 }, /* HE40 MCS7 */
	{ 32, 206500 }, /* HE40 MCS8 */
	{ 34, 229400 }, /* HE40 MCS9 */
	{ 37, 258100 }, /* HE40 MCS10 */
	{ 39, 286800 }, /* HE40 MCS11 */
	{ -1, 286800 }  /* SNR > 34 */
};

static const struct minsnr_bitrate_entry he80_table[] = {
	{ 0, 0 },
	{ 8, 36000 },   /* HE80 MCS0 */
	{ 11, 72100 },  /* HE80 MCS1 */
	{ 15, 108100 }, /* HE80 MCS2 */
	{ 17, 144100 }, /* HE80 MCS3 */
	{ 21, 216200 }, /* HE80 MCS4 */
	{ 24, 288200 }, /* HE80 MCS5 */
	{ 26, 324300 }, /* HE80 MCS6 */
	{ 31, 360300 }, /* HE80 MCS7 */
	{ 35, 432400 }, /* HE80 MCS8 */
	{ 37, 480400 }, /* HE80 MCS9 */
	{ 40, 540400 }, /* HE80 MCS10 */
	{ 42, 600500 }, /* HE80 MCS11 */
	{ -1, 600500 }  /* SNR > 37 */
};


static const struct minsnr_bitrate_entry he160_table[] = {
	{ 0, 0 },
	{ 11, 72100 },   /* HE160 MCS0 */
	{ 14, 144100 },  /* HE160 MCS1 */
	{ 18, 216200 },  /* HE160 MCS2 */
	{ 20, 288200 },  /* HE160 MCS3 */
	{ 24, 432400 },  /* HE160 MCS4 */
	{ 27, 576500 },  /* HE160 MCS5 */
	{ 29, 648500 },  /* HE160 MCS6 */
	{ 34, 720600 },  /* HE160 MCS7 */
	{ 38, 864700 },  /* HE160 MCS8 */
	{ 40, 960800 },  /* HE160 MCS9 */
	{ 43, 1080900 }, /* HE160 MCS10 */
	{ 45, 1201000 }, /* HE160 MCS11 */
	{ -1, 1201000 }  /* SNR > 37 */
};


static unsigned int interpolate_rate(int snr, int snr0, int snr1,
				     int rate0, int rate1)
{
	return rate0 + (snr - snr0) * (rate1 - rate0) / (snr1 - snr0);
}


static unsigned int max_rate(const struct minsnr_bitrate_entry table[],
			     int snr, bool vht)
{
	const struct minsnr_bitrate_entry *prev, *entry = table;

	while ((entry->minsnr != -1) &&
	       (snr >= entry->minsnr) &&
	       (vht || entry - table <= vht_mcs))
		entry++;
	if (entry == table)
		return entry->bitrate;
	prev = entry - 1;
	if (entry->minsnr == -1 || (!vht && entry - table > vht_mcs))
		return prev->bitrate;
	return interpolate_rate(snr, prev->minsnr, entry->minsnr, prev->bitrate,
				entry->bitrate);
}


static unsigned int max_ht20_rate(int snr, bool vht)
{
	return max_rate(vht20_table, snr, vht);
}


static unsigned int max_ht40_rate(int snr, bool vht)
{
	return max_rate(vht40_table, snr, vht);
}


static unsigned int max_vht80_rate(int snr)
{
	return max_rate(vht80_table, snr, 1);
}


static unsigned int max_vht160_rate(int snr)
{
	return max_rate(vht160_table, snr, 1);
}


static unsigned int max_he_rate(const struct minsnr_bitrate_entry table[],
				int snr)
{
	const struct minsnr_bitrate_entry *prev, *entry = table;

	while (entry->minsnr != -1 && snr >= entry->minsnr)
		entry++;
	if (entry == table)
		return 0;
	prev = entry - 1;
	if (entry->minsnr == -1)
		return prev->bitrate;
	return interpolate_rate(snr, prev->minsnr, entry->minsnr,
				prev->bitrate, entry->bitrate);
}


unsigned int wpas_get_est_tpt(const struct wpa_supplicant *wpa_s,
			      const u8 *ies, size_t ies_len, int rate,
			      int snr, int freq)
{
	struct hostapd_hw_modes *hw_mode;
	unsigned int est, tmp;
	const u8 *ie;

	/* Limit based on estimated SNR */
	if (rate > 1 * 2 && snr < 1)
		rate = 1 * 2;
	else if (rate > 2 * 2 && snr < 4)
		rate = 2 * 2;
	else if (rate > 6 * 2 && snr < 5)
		rate = 6 * 2;
	else if (rate > 9 * 2 && snr < 6)
		rate = 9 * 2;
	else if (rate > 12 * 2 && snr < 7)
		rate = 12 * 2;
	else if (rate > 12 * 2 && snr < 8)
		rate = 14 * 2;
	else if (rate > 12 * 2 && snr < 9)
		rate = 16 * 2;
	else if (rate > 18 * 2 && snr < 10)
		rate = 18 * 2;
	else if (rate > 24 * 2 && snr < 11)
		rate = 24 * 2;
	else if (rate > 24 * 2 && snr < 12)
		rate = 27 * 2;
	else if (rate > 24 * 2 && snr < 13)
		rate = 30 * 2;
	else if (rate > 24 * 2 && snr < 14)
		rate = 33 * 2;
	else if (rate > 36 * 2 && snr < 15)
		rate = 36 * 2;
	else if (rate > 36 * 2 && snr < 16)
		rate = 39 * 2;
	else if (rate > 36 * 2 && snr < 17)
		rate = 42 * 2;
	else if (rate > 36 * 2 && snr < 18)
		rate = 45 * 2;
	else if (rate > 48 * 2 && snr < 19)
		rate = 48 * 2;
	else if (rate > 48 * 2 && snr < 20)
		rate = 51 * 2;
	else if (rate > 54 * 2 && snr < 21)
		rate = 54 * 2;
	est = rate * 500;

	hw_mode = get_mode_with_freq(wpa_s->hw.modes, wpa_s->hw.num_modes,
				     freq);

	if (hw_mode && hw_mode->ht_capab) {
		ie = get_ie(ies, ies_len, WLAN_EID_HT_CAP);
		if (ie) {
			tmp = max_ht20_rate(snr, false);
			if (tmp > est)
				est = tmp;
		}
	}

	if (hw_mode &&
	    (hw_mode->ht_capab & HT_CAP_INFO_SUPP_CHANNEL_WIDTH_SET)) {
		ie = get_ie(ies, ies_len, WLAN_EID_HT_OPERATION);
		if (ie && ie[1] >= 2 &&
		    (ie[3] & HT_INFO_HT_PARAM_SECONDARY_CHNL_OFF_MASK)) {
			tmp = max_ht40_rate(snr, false);
			if (tmp > est)
				est = tmp;
		}
	}

	if (hw_mode && hw_mode->vht_capab) {
		/* Use +1 to assume VHT is always faster than HT */
		ie = get_ie(ies, ies_len, WLAN_EID_VHT_CAP);
		if (ie) {
			bool vht80 = false, vht160 = false;

			tmp = max_ht20_rate(snr, true) + 1;
			if (tmp > est)
				est = tmp;

			ie = get_ie(ies, ies_len, WLAN_EID_HT_OPERATION);
			if (ie && ie[1] >= 2 &&
			    (ie[3] &
			     HT_INFO_HT_PARAM_SECONDARY_CHNL_OFF_MASK)) {
				tmp = max_ht40_rate(snr, true) + 1;
				if (tmp > est)
					est = tmp;
			}

			/* Determine VHT BSS bandwidth based on IEEE Std
			 * 802.11-2020, Table 11-23 (VHT BSs bandwidth) */
			ie = get_ie(ies, ies_len, WLAN_EID_VHT_OPERATION);
			if (ie && ie[1] >= 3) {
				u8 cw = ie[2] & VHT_OPMODE_CHANNEL_WIDTH_MASK;
				u8 seg0 = ie[3];
				u8 seg1 = ie[4];

				if (cw)
					vht80 = true;
				if (cw == 2 ||
				    (cw == 3 &&
				     (seg1 > 0 && abs(seg1 - seg0) == 16)))
					vht160 = true;
				if (cw == 1 &&
				    ((seg1 > 0 && abs(seg1 - seg0) == 8) ||
				     (seg1 > 0 && abs(seg1 - seg0) == 16)))
					vht160 = true;
			}

			if (vht80) {
				tmp = max_vht80_rate(snr) + 1;
				if (tmp > est)
					est = tmp;
			}

			if (vht160 &&
			    (hw_mode->vht_capab &
			     (VHT_CAP_SUPP_CHAN_WIDTH_160MHZ |
			      VHT_CAP_SUPP_CHAN_WIDTH_160_80PLUS80MHZ))) {
				tmp = max_vht160_rate(snr) + 1;
				if (tmp > est)
					est = tmp;
			}
		}
	}

	if (hw_mode && hw_mode->he_capab[IEEE80211_MODE_INFRA].he_supported) {
		/* Use +2 to assume HE is always faster than HT/VHT */
		struct ieee80211_he_capabilities *he;
		struct he_capabilities *own_he;
		u8 cw;

		ie = get_ie_ext(ies, ies_len, WLAN_EID_EXT_HE_CAPABILITIES);
		if (!ie || (ie[1] < 1 + IEEE80211_HE_CAPAB_MIN_LEN))
			return est;
		he = (struct ieee80211_he_capabilities *) &ie[3];
		own_he = &hw_mode->he_capab[IEEE80211_MODE_INFRA];

		tmp = max_he_rate(he20_table, snr) + 2;
		if (tmp > est)
			est = tmp;

		cw = he->he_phy_capab_info[HE_PHYCAP_CHANNEL_WIDTH_SET_IDX] &
			own_he->phy_cap[HE_PHYCAP_CHANNEL_WIDTH_SET_IDX];
		if (cw &
		    (IS_2P4GHZ(freq) ? HE_PHYCAP_CHANNEL_WIDTH_SET_40MHZ_IN_2G :
		     HE_PHYCAP_CHANNEL_WIDTH_SET_40MHZ_80MHZ_IN_5G)) {
			tmp = max_he_rate(he40_table, snr) + 2;
			if (tmp > est)
				est = tmp;
		}

		if (!IS_2P4GHZ(freq) &&
		    (cw & HE_PHYCAP_CHANNEL_WIDTH_SET_40MHZ_80MHZ_IN_5G)) {
			tmp = max_he_rate(he80_table, snr) + 2;
			if (tmp > est)
				est = tmp;
		}

		if (!IS_2P4GHZ(freq) &&
		    (cw & (HE_PHYCAP_CHANNEL_WIDTH_SET_160MHZ_IN_5G |
			   HE_PHYCAP_CHANNEL_WIDTH_SET_80PLUS80MHZ_IN_5G))) {
			tmp = max_he_rate(he160_table, snr) + 2;
			if (tmp > est)
				est = tmp;
		}
	}

	return est;
}


void scan_est_throughput(struct wpa_supplicant *wpa_s,
			 struct wpa_scan_res *res)
{
	int rate; /* max legacy rate in 500 kb/s units */
	int snr = res->snr;
	const u8 *ies = (const void *) (res + 1);
	size_t ie_len = res->ie_len;

	if (res->est_throughput)
		return;

	/* Get maximum legacy rate */
	rate = wpa_scan_get_max_rate(res);

	if (!ie_len)
		ie_len = res->beacon_ie_len;
	res->est_throughput =
		wpas_get_est_tpt(wpa_s, ies, ie_len, rate, snr, res->freq);

	/* TODO: channel utilization and AP load (e.g., from AP Beacon) */
}
#endif  /* CONFIG_SUPP27_SCAN */

#ifdef CONFIG_SCAN_RESULT_OPTIMIZE
extern char *scan_reply;
extern int scan_reply_size;
extern int scan_reply_len;
extern size_t scan_resp_len;
extern int wpa_supplicant_ctrl_iface_scan_results(
    struct wpa_supplicant *wpa_s, char *buf, size_t buflen);
#endif /* CONFIG_SCAN_RESULT_OPTIMIZE */

/**
 * wpa_supplicant_get_scan_results - Get scan results
 * @wpa_s: Pointer to wpa_supplicant data
 * @info: Information about what was scanned or %NULL if not available
 * @new_scan: Whether a new scan was performed
 * Returns: Scan results, %NULL on failure
 *
 * This function request the current scan results from the driver and updates
 * the local BSS list wpa_s->bss. The caller is responsible for freeing the
 * results with wpa_scan_results_free().
 */
struct wpa_scan_results *
wpa_supplicant_get_scan_results(struct wpa_supplicant *wpa_s,
				struct scan_info *info, int new_scan)
{	//MODIFY_SUPPLICANT_FOR_FREERTOS

	struct wpa_scan_results *scan_res=NULL;
	size_t i;
#ifdef CONFIG_REUSED_UMAC_BSS_LIST
#ifdef ENABLE_SCAN_DBG
	int iface = 0;
#endif
#endif //CONFIG_REUSED_UMAC_BSS_LIST

	RX_FUNC_START("");

#ifdef CONFIG_TOGGLE_SCAN_SORT_TYPE
	int (*compar)(const void *, const void *) = wpa_scan_result_compar_advanced;
#else
	int (*compar)(const void *, const void *) = wpa_scan_result_compar;
#endif /* CONFIG_TOGGLE_SCAN_SORT_TYPE */

#ifdef CONFIG_IMMEDIATE_SCAN
#ifdef CONFIG_SCAN_RESULT_OPTIMIZE
#ifdef ENABLE_SCAN_DBG
	unsigned int	status = 0;
#endif
	ULONG	ra6w1_events;
#endif /* CONFIG_SCAN_RESULT_OPTIMIZE */
#endif /* CONFIG_IMMEDIATE_SCAN */

	scan_res = wpa_drv_get_scan_results2(wpa_s);
	if (scan_res == NULL) {
		wpa_dbg(wpa_s, MSG_DEBUG, "Failed to get scan results");
		return NULL;
	} else {
		ra6wx_scan_prt("[%s] Fetched %d scan results\n",
			      __func__, scan_res->num);
	}

	if (scan_res->fetch_time.sec == 0) {
		/*
		 * Make sure we have a valid timestamp if the driver wrapper
		 * does not set this.
		 */
		os_get_reltime(&scan_res->fetch_time);
	}
	filter_scan_res(wpa_s, scan_res);

#ifdef	CONFIG_SUPP27_SCAN
	for (i = 0; i < scan_res->num; i++) {
		struct wpa_scan_res *scan_res_item = scan_res->res[i];

		scan_snr(scan_res_item);
		scan_est_throughput(wpa_s, scan_res_item);
	}
#endif	/* CONFIG_SUPP27_SCAN */

#ifdef CONFIG_WPS
	if (wpas_wps_searching(wpa_s)) {
		wpa_dbg(wpa_s, MSG_DEBUG, "WPS: Order scan results with WPS "
			"provisioning rules");
		compar = wpa_scan_result_wps_compar;
	}
#endif /* CONFIG_WPS */

#ifdef CONFIG_REUSED_UMAC_BSS_LIST
/* Insert for simple roaming - 2022-11-14
  The bss of the currently connected AP is not updated by CONFIG_REUSED_UMAC_BSS_LIST.
  Therefore, update the level value with beacon */

#ifdef ENABLE_SCAN_DBG
	switch (get_run_mode()) {
		case WIFI_DEVICE_MODE_EXT_STATION:
		case WIFI_DEVICE_MODE_EXT_AP_STATION:
#if defined ( CONFIG_P2P )
#ifdef CONFIG_P2P_CONCURRENT
		case WIFI_DEVICE_MODE_EXT_P2P_STATION:
#endif // CONFIG_P2P_CONCURRENT
#endif /* ( CONFIG_P2P ) */
			iface = 0;	// WLAN0_IFACE
			break;
#if defined ( CONFIG_P2P )
		case WIFI_DEVICE_MODE_EXT_P2P:
			iface = 1;	// WLAN1_IFACE
			break;
#endif /* ( CONFIG_P2P ) */
	}
#endif /* ENABLE_SCAN_DBG */

	/* Update the level of the current connected AP */
    if (wpa_s->current_bss != NULL
        && !(wpa_s->current_bss->bssid[0] == 0 && wpa_s->current_bss->bssid[1] == 0 &&
             wpa_s->current_bss->bssid[2] == 0 && wpa_s->current_bss->bssid[3] == 0 &&
             wpa_s->current_bss->bssid[4] == 0 && wpa_s->current_bss->bssid[5] == 0)) {

    	for (i = 0; i < scan_res->num; i++) {
    	    ra6wx_scan_prt("[%s] %2d scan->bssid=%02x:%02x:%02x:%02x:%02x:%02x rssi=%d, cur->bss=%02x:%02x:%02x:%02x:%02x:%02x\n", __func__, i,
            	    scan_res->res[i]->bssid[0], scan_res->res[i]->bssid[1], scan_res->res[i]->bssid[2],
            	    scan_res->res[i]->bssid[3], scan_res->res[i]->bssid[4], scan_res->res[i]->bssid[5],
            	    scan_res->res[i]->level,
            	    wpa_s->current_bss->bssid[0], wpa_s->current_bss->bssid[1], wpa_s->current_bss->bssid[2],
            	    wpa_s->current_bss->bssid[3], wpa_s->current_bss->bssid[4], wpa_s->current_bss->bssid[5]);

    		if (os_memcmp(scan_res->res[i]->bssid, wpa_s->current_bss->bssid, ETH_ALEN) == 0) {
                struct wpa_signal_info si;
                int ret;
                
                ret = wpa_drv_signal_poll(wpa_s, &si);
                if (ret) {
                    break;
                }

                ra6wx_scan_prt("si->current_signal=%d get_sta_rssi=%d\n", si.current_signal, fc80211_get_sta_rssi_value(iface));
                scan_res->res[i]->level = si.current_signal;
    			scan_res->res[i]->age = 0;
                ra6wx_scan_prt(ANSI_COLOR_LIGHT_GREEN "scan_res: Current AP RSSI(%d) Update OK\n" ANSI_COLOR_DEFULT, scan_res->res[i]->level);
    			break;
    		}
    	}

    	if (i == scan_res->num) {
    	    ra6wx_notice_prt(ANSI_COLOR_LIGHT_YELLOW "scan_res: Current AP Not found (%02x:%02x:%02x:%02x:%02x:%02x)\n" ANSI_COLOR_DEFULT,
            	    wpa_s->current_bss->bssid[0], wpa_s->current_bss->bssid[1], wpa_s->current_bss->bssid[2],
            	    wpa_s->current_bss->bssid[3], wpa_s->current_bss->bssid[4], wpa_s->current_bss->bssid[5]);
    	}
	}
#endif //CONFIG_REUSED_UMAC_BSS_LIST

	if (scan_res->res) {
		qsort(scan_res->res, scan_res->num,
		      sizeof(struct wpa_scan_res *), compar);
	}

	dump_scan_res(scan_res);

#ifdef	CONFIG_SUPP27_SCAN
	if (wpa_s->ignore_post_flush_scan_res) {
		/* FLUSH command aborted an ongoing scan and these are the
		 * results from the aborted scan. Do not process the results to
		 * maintain flushed state. */
		wpa_dbg(wpa_s, MSG_DEBUG,
			"Do not update BSS table based on pending post-FLUSH scan results");
		wpa_s->ignore_post_flush_scan_res = 0;
		return scan_res;
	}
#endif	/* CONFIG_SUPP27_SCAN */

	wpa_bss_update_start(wpa_s);
	if (scan_res->num > 0) {
    	ra6wx_scan_prt("\n[No] [Action func] [Signal] [Beacon] [Capability] [Quality] [Noise] [SNR] [Flags] [Age]     [Timestamp] [Ch] [bssid]           [ssid]\n");
   	}

	for (i = 0; i < scan_res->num; i++) {
	    ra6wx_scan_prt("%4d ", i + 1);
		wpa_bss_update_scan_res(wpa_s, scan_res->res[i], &scan_res->fetch_time);
	}
	wpa_bss_update_end(wpa_s, info, new_scan);

#ifdef	CONFIG_P2P /* by Shingu 20170418 (P2P ACS) */
	if ((get_run_mode() == WIFI_DEVICE_MODE_EXT_P2P)
#ifdef CONFIG_P2P_CONCURRENT
      || get_run_mode() == WIFI_DEVICE_MODE_EXT_P2P_STATION) 
#endif // CONFIG_P2P_CONCURRENT
      && (!wpa_s->conf->p2p_oper_channel || !wpa_s->conf->p2p_listen_channel)
      && os_strcmp(wpa_s->ifname, P2P_DEVICE_NAME) == 0) {
         wpas_p2p_get_best_social(wpa_s, scan_res);
	}
#endif	/* CONFIG_P2P */

#ifdef CONFIG_IMMEDIATE_SCAN
#ifdef CONFIG_SCAN_RESULT_OPTIMIZE
	ra6w1_events = xEventGroupWaitBits(ra6w1_sp_event_group,
						RA6W_SCAN_RESULTS_TX_EV,
						pdTRUE,
						pdFALSE,
						OS_EVENT_NO_WAIT);

	 if (ra6w1_events & RA6W_SCAN_RESULTS_TX_EV) {
		ra6wx_scan_prt("<%s> Scan result command received from cli"
			"(status=%d)\n", __func__, status);

		if (!scan_reply) {
			scan_reply = pvPortMalloc(scan_reply_size);
		}

		if (scan_reply == NULL) {
			scan_resp_len = 1;
			xEventGroupSetBits(ra6w1_sp_event_group, RA6WX_SCAN_RESULTS_FAIL_EV);
			ra6wx_info_prt("[%s] Malloc Error for scan_reply(%d)\n", __func__, scan_reply_size);
		} else {
			memset(scan_reply, 0, scan_reply_size);
			os_memcpy(scan_reply, "OK", 2);
			scan_reply_len = 2;
			scan_reply_len = wpa_supplicant_ctrl_iface_scan_results(
				select_sta0(wpa_s), scan_reply, scan_reply_size);
			xEventGroupSetBits(ra6w1_sp_event_group, RA6W_SCAN_RESULTS_RX_EV);
		}
	}
#else /* CONFIG_SCAN_RESULT_OPTIMIZE */
	xEventGroupSetBits(ra6w1_sp_event_group, RA6W_SCAN_RESULTS_RX_EV);
#endif /* CONFIG_SCAN_RESULT_OPTIMIZE */
#endif /* CONFIG_IMMEDIATE_SCAN */

	ra6wx_scan_prt("Scan result sorted by: ");

	if (current_compar_type == COMPAR_SECURITY) {
		ra6wx_scan_prt("Security -> SNR \n");
	} else {
		ra6wx_scan_prt("SNR -> Security \n");
	}

	if (current_compar_type == COMPAR_SECURITY) {
		current_compar_type = COMPAR_SNR;
	} else {
		current_compar_type = COMPAR_SECURITY;
	}

	RX_FUNC_END("");

	return scan_res;
}


/**
 * wpa_supplicant_update_scan_results - Update scan results from the driver
 * @wpa_s: Pointer to wpa_supplicant data
 * Returns: 0 on success, -1 on failure
 *
 * This function updates the BSS table within wpa_supplicant based on the
 * currently available scan results from the driver without requesting a new
 * scan. This is used in cases where the driver indicates an association
 * (including roaming within ESS) and wpa_supplicant does not yet have the
 * needed information to complete the connection (e.g., to perform validation
 * steps in 4-way handshake).
 */
int wpa_supplicant_update_scan_results(struct wpa_supplicant *wpa_s)
{
	struct wpa_scan_results *scan_res;
	scan_res = wpa_supplicant_get_scan_results(wpa_s, NULL, 0);
	if (scan_res == NULL)
		return -1;
	wpa_scan_results_free(scan_res);

	return 0;
}


/**
 * scan_only_handler - Reports scan results
 */
void scan_only_handler(struct wpa_supplicant *wpa_s,
		       struct wpa_scan_results *scan_res)
{
	wpa_dbg(wpa_s, MSG_DEBUG, "Scan-only results received");
	if (wpa_s->last_scan_req == MANUAL_SCAN_REQ &&
	    wpa_s->manual_scan_use_id && wpa_s->own_scan_running) {
		wpa_msg_ctrl(wpa_s, MSG_INFO, WPA_EVENT_SCAN_RESULTS "id=%u",
			     wpa_s->manual_scan_id);
		wpa_s->manual_scan_use_id = 0;
	} else {
		wpa_msg_ctrl(wpa_s, MSG_INFO, WPA_EVENT_SCAN_RESULTS);
	}

#if 0	/* by Bright : Merge 2.7 */
	wpas_notify_scan_results(wpa_s);
	wpas_notify_scan_done(wpa_s, 1);
#endif	/* by Bright : Merge 2.7 */

#if defined ( CONFIG_SCAN_WORK )
	if (wpa_s->scan_work) {
		struct wpa_radio_work *work = wpa_s->scan_work;
		wpa_s->scan_work = NULL;
#if 0	/* by Bright : Merge 2.7 */
		radio_work_done(work);
#endif	/* by Bright : Merge 2.7 */
	}
#endif	// defined ( CONFIG_SCAN_WORK )

	if (wpa_s->wpa_state == WPA_SCANNING)
		wpa_supplicant_set_state(wpa_s, wpa_s->scan_prev_wpa_state);
}

#ifdef CONFIG_SCHED_SCAN
int wpas_scan_scheduled(struct wpa_supplicant *wpa_s)
{
	return eloop_is_timeout_registered(wpa_supplicant_scan, wpa_s, NULL);
}


struct wpa_driver_scan_params *
wpa_scan_clone_params(const struct wpa_driver_scan_params *src)
{
	struct wpa_driver_scan_params *params;
	size_t i;
	u8 *n;

	params = os_zalloc(sizeof(*params));
	if (params == NULL)
		return NULL;

	for (i = 0; i < src->num_ssids; i++) {
		if (src->ssids[i].ssid) {
			n = os_memdup(src->ssids[i].ssid,
				      src->ssids[i].ssid_len);
			if (n == NULL)
				goto failed;
			params->ssids[i].ssid = n;
			params->ssids[i].ssid_len = src->ssids[i].ssid_len;
		}
	}
	params->num_ssids = src->num_ssids;

	if (src->extra_ies) {
		n = os_memdup(src->extra_ies, src->extra_ies_len);
		if (n == NULL)
			goto failed;
		params->extra_ies = n;
		params->extra_ies_len = src->extra_ies_len;
	}

	if (src->freqs) {
		int len = int_array_len(src->freqs);
		params->freqs = os_memdup(src->freqs, (len + 1) * sizeof(int));
		if (params->freqs == NULL)
			goto failed;
	}

	if (src->filter_ssids) {
		params->filter_ssids = os_memdup(src->filter_ssids,
						 sizeof(*params->filter_ssids) *
						 src->num_filter_ssids);
		if (params->filter_ssids == NULL)
			goto failed;
		params->num_filter_ssids = src->num_filter_ssids;
	}

#ifdef CONFIG_SCAN_FILTER_RSSI
	params->filter_rssi = src->filter_rssi;
#endif	// CONFIG_SCAN_FILTER_RSSI
#ifdef CONFIG_P2P
	params->p2p_probe = src->p2p_probe;
#endif	// CONFIG_P2P
	params->only_new_results = src->only_new_results;
	params->low_priority = src->low_priority;
	params->duration = src->duration;
	params->duration_mandatory = src->duration_mandatory;
	params->oce_scan = src->oce_scan;

	if (src->sched_scan_plans_num > 0) {
		params->sched_scan_plans =
			os_memdup(src->sched_scan_plans,
				  sizeof(*src->sched_scan_plans) *
				  src->sched_scan_plans_num);
		if (!params->sched_scan_plans)
			goto failed;

		params->sched_scan_plans_num = src->sched_scan_plans_num;
	}

#if defined ( CONFIG_SCHED_SCAN )
	if (src->mac_addr_rand &&
	    wpa_setup_mac_addr_rand_params(params, src->mac_addr))
		goto failed;
#endif	// defined ( CONFIG_SCHED_SCAN )

	if (src->bssid) {
		u8 *bssid;

		bssid = os_memdup(src->bssid, ETH_ALEN);
		if (!bssid)
			goto failed;
		params->bssid = bssid;
	}

	params->relative_rssi_set = src->relative_rssi_set;
	params->relative_rssi = src->relative_rssi;
#ifdef CONFIG_BAND_5GHZ
	params->relative_adjust_band = src->relative_adjust_band;
#endif	// CONFIG_BAND_5GHZ
	params->relative_adjust_rssi = src->relative_adjust_rssi;
	params->p2p_include_6ghz = src->p2p_include_6ghz;
	return params;

failed:
	wpa_scan_free_params(params);
	return NULL;
}
#endif /* CONFIG_SCHED_SCAN */

void wpa_scan_free_params(struct wpa_driver_scan_params *params)
{
	size_t i;

	if (params == NULL)
		return;

	for (i = 0; i < params->num_ssids; i++)
		os_free((u8 *) params->ssids[i].ssid);
	os_free((u8 *) params->extra_ies);
	os_free(params->freqs);
	os_free(params->filter_ssids);
#ifdef CONFIG_SCHED_SCAN
	os_free(params->sched_scan_plans);

	/*
	 * Note: params->mac_addr_mask points to same memory allocation and
	 * must not be freed separately.
	 */
	os_free((u8 *) params->mac_addr);
	os_free((u8 *) params->bssid);
#endif	// CONFIG_SCHED_SCAN

	os_free(params);
}

/*
 * Function : ra6wx_scan_results_timer
 *
 * - arguments  :
 *      : timeout_ctx   - wpa_s structure pointer
 *      : timeout   - timout sleep time (sec)
 * - return     : status    - treadX timer create result
 *
 * - Discription :
 *      Tmeout function after sending TRIGGER_SCAN
 */
int ra6wx_scan_results_timer(void *timeout_ctx, int timeout)	//MODIFY_SUPPLICANT_FOR_FREERTOS
{
	UINT    status = 0;

	RX_FUNC_START("");

	eloop_register_timeout(timeout, 0,
                supp_scan_result_event,
                (void *)timeout_ctx, NULL);

	RX_FUNC_END("");

	return status;
}


#ifdef CONFIG_RECONNECT_OPTIMIZE	//MODIFY_SUPPLICANT_FOR_FREERTOS
	u8 wpa_scan_is_fast_reconnect(void) {
		if(fast_reconnect_scan == 1)
			return 1;
		else
			return 0;
	}
#else
u8 wpa_scan_is_fast_reconnect(void) {
	return 0;
}
#endif /* CONFIG_RECONNECT_OPTIMIZE */

#ifdef CONFIG_SCHED_SCAN
int wpas_start_pno(struct wpa_supplicant *wpa_s)
{
	int ret;
	size_t prio, i, num_ssid, num_match_ssid;
	struct wpa_ssid *ssid;
	struct wpa_driver_scan_params params;
	struct sched_scan_plan scan_plan;
	unsigned int max_sched_scan_ssids;

#if defined ( CONFIG_SCHED_SCAN )
	if (!wpa_s->sched_scan_supported)
		return -1;

	if (wpa_s->max_sched_scan_ssids > WPAS_MAX_SCAN_SSIDS)
		max_sched_scan_ssids = WPAS_MAX_SCAN_SSIDS;
	else
		max_sched_scan_ssids = wpa_s->max_sched_scan_ssids;
	if (max_sched_scan_ssids < 1)
		return -1;
#endif	// defined ( CONFIG_SCHED_SCAN )

	if (wpa_s->pno || wpa_s->pno_sched_pending)
		return 0;

	if ((wpa_s->wpa_state > WPA_SCANNING) &&
	    (wpa_s->wpa_state < WPA_COMPLETED)) {
		wpa_printf(MSG_ERROR, "PNO: In assoc process");
		return -EAGAIN;
	}

	if (wpa_s->wpa_state == WPA_SCANNING) {
		wpa_supplicant_cancel_scan(wpa_s);
#if defined ( CONFIG_SCHED_SCAN )
		if (wpa_s->sched_scanning) {
			wpa_printf(MSG_DEBUG, "Schedule PNO on completion of "
				   "ongoing sched scan");
			wpa_supplicant_cancel_sched_scan(wpa_s);
			wpa_s->pno_sched_pending = 1;
			return 0;
		}
#endif	// defined ( CONFIG_SCHED_SCAN )
	}

#if defined ( CONFIG_SCHED_SCAN )
	if (wpa_s->sched_scan_stop_req) {
		wpa_printf(MSG_DEBUG,
			   "Schedule PNO after previous sched scan has stopped");
		wpa_s->pno_sched_pending = 1;
		return 0;
	}
#endif // defined ( CONFIG_SCHED_SCAN )

	os_memset(&params, 0, sizeof(params));

	num_ssid = num_match_ssid = 0;
	ssid = wpa_s->conf->ssid;
	while (ssid) {
		if (!wpas_network_disabled(wpa_s, ssid)) {
			num_match_ssid++;
			if (ssid->scan_ssid)
				num_ssid++;
		}
		ssid = ssid->next;
	}

	if (num_match_ssid == 0) {
		wpa_printf(MSG_DEBUG, "PNO: No configured SSIDs");
		return -1;
	}

	if (num_match_ssid > num_ssid) {
		params.num_ssids++; /* wildcard */
		num_ssid++;
	}

	if (num_ssid > max_sched_scan_ssids) {
		wpa_printf(MSG_DEBUG, "PNO: Use only the first %u SSIDs from "
			   "%u", max_sched_scan_ssids, (unsigned int) num_ssid);
		num_ssid = max_sched_scan_ssids;
	}

	if (num_match_ssid > wpa_s->max_match_sets) {
		num_match_ssid = wpa_s->max_match_sets;
		wpa_dbg(wpa_s, MSG_DEBUG, "PNO: Too many SSIDs to match");
	}
	params.filter_ssids = os_calloc(num_match_ssid,
					sizeof(struct wpa_driver_scan_filter));
	if (params.filter_ssids == NULL)
		return -1;

#ifdef CONFIG_PRIO_GROUP
	i = 0;
	prio = 0;
	ssid = wpa_s->conf->pssid[prio];
	while (ssid) {
		if (!wpas_network_disabled(wpa_s, ssid)) {
			if (ssid->scan_ssid && params.num_ssids < num_ssid) {
				params.ssids[params.num_ssids].ssid =
					ssid->ssid;
				params.ssids[params.num_ssids].ssid_len =
					 ssid->ssid_len;
				params.num_ssids++;
			}
			os_memcpy(params.filter_ssids[i].ssid, ssid->ssid,
				  ssid->ssid_len);
			params.filter_ssids[i].ssid_len = ssid->ssid_len;
			params.num_filter_ssids++;
			i++;
			if (i == num_match_ssid)
				break;
		}
		if (ssid->pnext)
			ssid = ssid->pnext;
		else if (prio + 1 == wpa_s->conf->num_prio)
			break;
		else
			ssid = wpa_s->conf->pssid[++prio];
	}
#endif	// CONFIG_PRIO_GROUP

#ifdef CONFIG_SCAN_FILTER_RSSI
	if (wpa_s->conf->filter_rssi)
		params.filter_rssi = wpa_s->conf->filter_rssi;
#endif	// CONFIG_SCAN_FILTER_RSSI

#ifdef CONFIG_SCHED_SCAN
	if (wpa_s->sched_scan_plans_num) {
		params.sched_scan_plans = wpa_s->sched_scan_plans;
		params.sched_scan_plans_num = wpa_s->sched_scan_plans_num;
	} else {
		/* Set one scan plan that will run infinitely */
		if (wpa_s->conf->sched_scan_interval)
			scan_plan.interval = wpa_s->conf->sched_scan_interval;
		else
			scan_plan.interval = 10;

		scan_plan.iterations = 0;
		params.sched_scan_plans = &scan_plan;
		params.sched_scan_plans_num = 1;
	}

	params.sched_scan_start_delay = wpa_s->conf->sched_scan_start_delay;
#endif	// CONFIG_SCHED_SCAN

	if (params.freqs == NULL && wpa_s->manual_sched_scan_freqs) {
		wpa_dbg(wpa_s, MSG_DEBUG, "Limit sched scan to specified channels");
		params.freqs = wpa_s->manual_sched_scan_freqs;
	}

#if defined ( CONFIG_SCHED_SCAN )
	if ((wpa_s->mac_addr_rand_enable & MAC_ADDR_RAND_PNO) &&
	    wpa_s->wpa_state <= WPA_SCANNING)
		wpa_setup_mac_addr_rand_params(&params, wpa_s->mac_addr_pno);
#endif	// defined ( CONFIG_SCHED_SCAN )

	wpa_scan_set_relative_rssi_params(wpa_s, &params);

	ret = wpa_supplicant_start_sched_scan(wpa_s, &params);
	os_free(params.filter_ssids);
	os_free(params.mac_addr);
	if (ret == 0)
		wpa_s->pno = 1;
	else
		wpa_msg(wpa_s, MSG_ERROR, "Failed to schedule PNO");
	return ret;
}


int wpas_stop_pno(struct wpa_supplicant *wpa_s)
{
	int ret = 0;

	if (!wpa_s->pno)
		return 0;

#if defined ( CONFIG_SCHED_SCAN )
	ret = wpa_supplicant_stop_sched_scan(wpa_s);
	wpa_s->sched_scan_stop_req = 1;
#endif	// defined ( CONFIG_SCHED_SCAN )

	wpa_s->pno = 0;
	wpa_s->pno_sched_pending = 0;

	if (wpa_s->wpa_state == WPA_SCANNING)
		wpa_supplicant_req_scan(wpa_s, 0, 0);

	return ret;
}
#endif /* CONFIG_SCHED_SCAN */

#ifdef  CONFIG_MAC_RAND_SCAN
void wpas_mac_addr_rand_scan_clear(struct wpa_supplicant *wpa_s,
				    unsigned int type)
{
	type &= MAC_ADDR_RAND_ALL;
	wpa_s->mac_addr_rand_enable &= ~type;

	if (type & MAC_ADDR_RAND_SCAN) {
		os_free(wpa_s->mac_addr_scan);
		wpa_s->mac_addr_scan = NULL;
	}

	if (type & MAC_ADDR_RAND_SCHED_SCAN) {
		os_free(wpa_s->mac_addr_sched_scan);
		wpa_s->mac_addr_sched_scan = NULL;
	}

	if (type & MAC_ADDR_RAND_PNO) {
		os_free(wpa_s->mac_addr_pno);
		wpa_s->mac_addr_pno = NULL;
	}
}


int wpas_mac_addr_rand_scan_set(struct wpa_supplicant *wpa_s,
				unsigned int type, const u8 *addr,
				const u8 *mask)
{
	u8 *tmp = NULL;

	if ((wpa_s->mac_addr_rand_supported & type) != type ) {
		wpa_printf(MSG_INFO,
			   "scan: MAC randomization type %u != supported=%u",
			   type, wpa_s->mac_addr_rand_supported);
		return -1;
	}

	wpas_mac_addr_rand_scan_clear(wpa_s, type);

	if (addr) {
		tmp = os_malloc(2 * ETH_ALEN);
		if (!tmp)
			return -1;
		os_memcpy(tmp, addr, ETH_ALEN);
		os_memcpy(tmp + ETH_ALEN, mask, ETH_ALEN);
	}

	if (type == MAC_ADDR_RAND_SCAN) {
		wpa_s->mac_addr_scan = tmp;
	} else if (type == MAC_ADDR_RAND_SCHED_SCAN) {
		wpa_s->mac_addr_sched_scan = tmp;
	} else if (type == MAC_ADDR_RAND_PNO) {
		wpa_s->mac_addr_pno = tmp;
	} else {
		wpa_printf(MSG_INFO,
			   "scan: Invalid MAC randomization type=0x%x",
			   type);
		os_free(tmp);
		return -1;
	}

	wpa_s->mac_addr_rand_enable |= type;
	return 0;
}

int wpas_mac_addr_rand_scan_get_mask(struct wpa_supplicant *wpa_s,
				     unsigned int type, u8 *mask)
{
	const u8 *to_copy;

	if ((wpa_s->mac_addr_rand_enable & type) != type)
		return -1;

	if (type == MAC_ADDR_RAND_SCAN) {
		to_copy = wpa_s->mac_addr_scan;
	} else if (type == MAC_ADDR_RAND_SCHED_SCAN) {
		to_copy = wpa_s->mac_addr_sched_scan;
	} else if (type == MAC_ADDR_RAND_PNO) {
		to_copy = wpa_s->mac_addr_pno;
	} else {
		wpa_printf(MSG_DEBUG,
			   "scan: Invalid MAC randomization type=0x%x",
			   type);
		return -1;
	}

	os_memcpy(mask, to_copy + ETH_ALEN, ETH_ALEN);
	return 0;
}
#endif  /* CONFIG_MAC_RAND_SCAN */

#if defined ( CONFIG_RADIO_WORK )
int wpas_abort_ongoing_scan(struct wpa_supplicant *wpa_s)
{
	struct wpa_radio_work *work;
	struct wpa_radio *radio = wpa_s->radio;

	dl_list_for_each(work, &radio->work, struct wpa_radio_work, list) {
		if (work->wpa_s != wpa_s || !work->started ||
		    (os_strcmp(work->type, "scan") != 0 &&
		     os_strcmp(work->type, "p2p-scan") != 0))
			continue;
		wpa_dbg(wpa_s, MSG_DEBUG, "Abort an ongoing scan");
#ifdef  __SUPPORT_VIRTUAL_ONELINK__
		return wpa_drv_abort_scan(wpa_s, wpa_s->curr_scan_cookie);
#else
		return 0;
#endif	//  __SUPPORT_VIRTUAL_ONELINK__
	}

	wpa_dbg(wpa_s, MSG_DEBUG, "No ongoing scan/p2p-scan found to abort");
	return -1;
}
#endif	// defined ( CONFIG_RADIO_WORK )

#if defined ( CONFIG_SCHED_SCAN )
int wpas_sched_scan_plans_set(struct wpa_supplicant *wpa_s, const char *cmd)
{
	struct sched_scan_plan *scan_plans = NULL;
	const char *token, *context = NULL;
	unsigned int num = 0;

	if (!cmd)
		return -1;

	if (!cmd[0]) {
		wpa_printf(MSG_DEBUG, "Clear sched scan plans");
		os_free(wpa_s->sched_scan_plans);
		wpa_s->sched_scan_plans = NULL;
		wpa_s->sched_scan_plans_num = 0;
		return 0;
	}

	while ((token = cstr_token(cmd, " ", &context))) {
		int ret;
		struct sched_scan_plan *scan_plan, *n;

		n = os_realloc_array(scan_plans, num + 1, sizeof(*scan_plans));
		if (!n)
			goto fail;

		scan_plans = n;
		scan_plan = &scan_plans[num];
		num++;

		ret = sscanf(token, "%u:%u", &scan_plan->interval,
			     &scan_plan->iterations);
		if (ret <= 0 || ret > 2 || !scan_plan->interval) {
			wpa_printf(MSG_ERROR,
				   "Invalid sched scan plan input: %s", token);
			goto fail;
		}

		if (scan_plan->interval > wpa_s->max_sched_scan_plan_interval) {
			wpa_printf(MSG_WARNING,
				   "scan plan %u: Scan interval too long(%u), use the maximum allowed(%u)",
				   num, scan_plan->interval,
				   wpa_s->max_sched_scan_plan_interval);
			scan_plan->interval =
				wpa_s->max_sched_scan_plan_interval;
		}

		if (ret == 1) {
			scan_plan->iterations = 0;
			break;
		}

		if (!scan_plan->iterations) {
			wpa_printf(MSG_ERROR,
				   "scan plan %u: Number of iterations cannot be zero",
				   num);
			goto fail;
		}

		if (scan_plan->iterations >
		    wpa_s->max_sched_scan_plan_iterations) {
			wpa_printf(MSG_WARNING,
				   "scan plan %u: Too many iterations(%u), use the maximum allowed(%u)",
				   num, scan_plan->iterations,
				   wpa_s->max_sched_scan_plan_iterations);
			scan_plan->iterations =
				wpa_s->max_sched_scan_plan_iterations;
		}

		wpa_printf(MSG_DEBUG,
			   "scan plan %u: interval=%u iterations=%u",
			   num, scan_plan->interval, scan_plan->iterations);
	}

	if (!scan_plans) {
		wpa_printf(MSG_ERROR, "Invalid scan plans entry");
		goto fail;
	}

	if (cstr_token(cmd, " ", &context) || scan_plans[num - 1].iterations) {
		wpa_printf(MSG_ERROR,
			   "All scan plans but the last must specify a number of iterations");
		goto fail;
	}

	wpa_printf(MSG_DEBUG, "scan plan %u (last plan): interval=%u",
		   num, scan_plans[num - 1].interval);

	if (num > wpa_s->max_sched_scan_plans) {
		wpa_printf(MSG_WARNING,
			   "Too many scheduled scan plans (only %u supported)",
			   wpa_s->max_sched_scan_plans);
		wpa_printf(MSG_WARNING,
			   "Use only the first %u scan plans, and the last one (in infinite loop)",
			   wpa_s->max_sched_scan_plans - 1);
		os_memcpy(&scan_plans[wpa_s->max_sched_scan_plans - 1],
			  &scan_plans[num - 1], sizeof(*scan_plans));
		num = wpa_s->max_sched_scan_plans;
	}

	os_free(wpa_s->sched_scan_plans);
	wpa_s->sched_scan_plans = scan_plans;
	wpa_s->sched_scan_plans_num = num;

	return 0;

fail:
	os_free(scan_plans);
	wpa_printf(MSG_ERROR, "invalid scan plans list");
	return -1;
}


/**
 * wpas_scan_reset_sched_scan - Reset sched_scan state
 * @wpa_s: Pointer to wpa_supplicant data
 *
 * This function is used to cancel a running scheduled scan and to reset an
 * internal scan state to continue with a regular scan on the following
 * wpa_supplicant_req_scan() calls.
 */
void wpas_scan_reset_sched_scan(struct wpa_supplicant *wpa_s)
{
	wpa_s->normal_scans = 0;
	if (wpa_s->sched_scanning) {
		wpa_s->sched_scan_timed_out = 0;
		wpa_s->prev_sched_ssid = NULL;
		wpa_supplicant_cancel_sched_scan(wpa_s);
	}
}


void wpas_scan_restart_sched_scan(struct wpa_supplicant *wpa_s)
{
	/* simulate timeout to restart the sched scan */
	wpa_s->sched_scan_timed_out = 1;
	wpa_s->prev_sched_ssid = NULL;
	wpa_supplicant_cancel_sched_scan(wpa_s);
}
#endif	// defined ( CONFIG_SCHED_SCAN )

/* EOF */
