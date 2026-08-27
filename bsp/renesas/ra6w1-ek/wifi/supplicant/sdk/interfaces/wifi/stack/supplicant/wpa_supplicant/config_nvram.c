/**
 *****************************************************************************************
 * @file	config_nvram.c
 * @brief	WPA Supplicant / Configuration backend: Windows registry from
 * wpa_supplicant-2.4
 *****************************************************************************************
 */

/*
 * WPA Supplicant / Configuration backend: Windows registry
 * Copyright (c) 2003-2008, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 *
 * This file implements a configuration backend for Windows registry. All the
 * configuration information is stored in the registry and the format for
 * network configuration fields is same as described in the sample
 * configuration file, wpa_supplicant.conf.
 *
 * Configuration data is in
 * \a HKEY_LOCAL_MACHINE\\SOFTWARE\\%wpa_supplicant\\configs
 * key. Each configuration profile has its own key under this. In terms of text
 * files, each profile would map to a separate text file with possibly multiple
 * networks. Under each profile, there is a networks key that lists all
 * networks as a subkey. Each network has set of values in the same way as
 * network block in the configuration file. In addition, blobs subkey has
 * possible blobs as values.
 *
 * Example network configuration block:
 * \verbatim
HKEY_LOCAL_MACHINE\SOFTWARE\wpa_supplicant\configs\test\networks\0000
   ssid="example"
   key_mgmt=WPA-PSK
\endverbatim
 *
 * Copyright (c) 2020-2022 Modified by Renesas Electronics.
 */

#include "includes.h"

#include "supp_common.h"
#include "supp_config.h"

#include "nvedit.h"

#include "supp_driver.h"
#include "wpa_supplicant_i.h"

#if CFG_PMGR
#include "rm_pmgr_w_instance.h"
#endif /* CFG_PMGR */

#include "uuid.h"
#include "ap_config.h"		//MODIFY_SUPPLICANT_FOR_FREERTOS
#include "rm_vee_flash_w_rrq_nvram.h"

#ifdef UNICODE
#define TSTR "%S"
#else /* UNICODE */
#define TSTR "%s"
#endif /* UNICODE */

#include "config_ssid.h"
#include "common_def.h"

extern int	get_run_mode(void);
#ifdef	EAP_PEAP
extern int	ra6w1_peap_version;
#endif	/* EAP_PEAP */

#if CFG_PMGR
extern dpm_supp_key_info_t	*dpm_supp_key_info;
extern dpm_supp_conn_info_t	*dpm_supp_conn_info;
extern dpm_supp_conn_ext_info_t	*dpm_supp_conn_ext_info;
#endif /* CFG_PMGR */
extern UCHAR	fast_connection_sleep_flag;

static int wpa_config_read_global(struct wpa_config *config, char *ifname)
{
	extern void fc80211_set_roaming_mode(int mode);
	extern int fc80211_set_threshold(int min_thold, int max_thold);

	TX_FUNC_START("");

	ra6w1_nvram_prt("[%s] START\n", __func__);

	wpa_config_set_global_defaults(config); /* FC9000 Only */

#ifdef CONFIG_SAE
	/* SAE_GROUPS */
	{
		char tmp_sae_groups[3 * MAX_SAE_GROUPS];
		memset(tmp_sae_groups, 0, 3 * MAX_SAE_GROUPS);
        

		if (config->sae_groups == NULL)
		{
			config->sae_groups = os_malloc(sizeof(int)*6);
		}

		if (!tmp_sae_groups[0])
		{
			extern const unsigned char support_sae_groups[];
			for (int i = 0; i < MAX_SAE_GROUPS; i++) {
				config->sae_groups[i] = support_sae_groups[i];
				if (config->sae_groups[i] == 0) {
					break;
				}
			}
		}
		else
		{
			config->sae_groups[0] = atoi(strtok(tmp_sae_groups, " "));

			if (config->sae_groups[0] >= 0) {
				for (int i = 1; i <= MAX_SAE_GROUPS; i++) {
					config->sae_groups[i] = atoi(strtok(NULL, " "));
					if (config->sae_groups[i] == 0) {
						break;
					}
				}
			}
		}
	}
#endif /* CONFIG_SAE */

#ifdef CONFIG_STA_COUNTRY_CODE
	 country_to_freq_range_list(&config->country_range, config->country);
#endif /* CONFIG_STA_COUNTRY_CODE */

	TX_FUNC_END("");

	return 0;
}

struct wpa_config * wpa_config_read(const char *name, char *ifname)
{
	struct wpa_config *config;

	TX_FUNC_START("");

	config = wpa_config_alloc_empty(NULL, NULL);

	if (config == NULL) {
		ra6w1_nvram_prt("[%s] config == NULL\n", __func__);
		return NULL;
	}

	//ra6w1_nvram_prt("[%s] Reading configuration profile(%s)\n", __func__, ifname);
	ra6w1_nvram_prt(" Read profile \n");	// Timing issue
#if CFG_PMGR
	if (RM_PMGR_W_dpm_is_wakeup() == pdTRUE) {
		if (wpa_supp_dpm_restore_config(config)) {
			ra6wx_dpm_prt("[DPM] %s : Failed to restore network "
					"infomation from Retention Memory\n",
					__func__);

			wpa_config_free(config);
			return NULL;
		}
	} 
	else
#endif /* CFG_PMGR */
	{
		wpa_config_read_global(config, ifname);
	}
	
#ifdef	CONFIG_CONCURRENT
	if ((get_run_mode() == WIFI_DEVICE_MODE_EXT_AP_STATION
#ifdef CONFIG_MESH
		|| get_run_mode() == WIFI_DEVICE_MODE_EXT_MESH_PORTAL
#endif /* CONFIG_MESH */
		) && os_strcmp(ifname, SOFTAP_DEVICE_NAME) == 0) {

		if (config->ssid == NULL)
			return config;

		config->ssid->frequency = FREQUENCE_DEFAULT;

#ifdef	CONFIG_P2P
	} else if (get_run_mode() == WIFI_DEVICE_MODE_EXT_P2P_STATION &&
		   os_strcmp(ifname, P2P_DEVICE_NAME) == 0) {
		config->p2p_oper_reg_class = 81;
		config->p2p_oper_channel = 1;
		config->p2p_listen_reg_class = 81;
		config->p2p_listen_channel = 1;
#endif	/* CONFIG_P2P */
	}
#endif	/* CONFIG_CONCURRENT */

	TX_FUNC_END("");

	return config;
}

int wpa_config_write(char *confname, struct wpa_config *config, char *ifname)
{
	struct wpa_ssid *ssid;
#ifdef	CONFIG_CONCURRENT
	int mode_id = 0;	// 0: STA, 1: Soft-AP, 2: P2P, 3: MESH Point
#endif	/* CONFIG_CONCURRENT */

	TX_FUNC_START("");

#ifdef	CONFIG_CONCURRENT
	if (os_strcmp(ifname, STA_DEVICE_NAME) == 0)
		mode_id = FIXED_NETWORK_ID_STA;
	else if (os_strcmp(ifname, SOFTAP_DEVICE_NAME) == 0)
		mode_id = FIXED_NETWORK_ID_AP;
	else if (os_strcmp(ifname, P2P_DEVICE_NAME) == 0)
		mode_id = FIXED_NETWORK_ID_P2P;
#ifdef __SUPPORT_MESH__
	else if (os_strcmp(ifname, MESH_POINT_DEVICE_NAME) == 0)
		mode_id = FIXED_NETWORK_ID_MESH_POINT;
#endif /* __SUPPORT_MESH__ */
#endif	/* CONFIG_CONCURRENT */

	ra6w1_nvram_prt("[%s] Writing configuration NVRAM\n", __func__);

#ifdef	CONFIG_CONCURRENT
	if (    mode_id == FIXED_NETWORK_ID_STA
		&& (   get_run_mode() == WIFI_DEVICE_MODE_EXT_AP_STATION
#if defined ( __SUPPORT_P2P__ )
			|| get_run_mode() == WIFI_DEVICE_MODE_EXT_P2P_STATION
#endif // __SUPPORT_P2P__
#ifdef __SUPPORT_MESH__
			|| get_run_mode() == WIFI_DEVICE_MODE_EXT_MESH_PORTAL
#endif /* __SUPPORT_MESH__ */
	    	)) {
		ra6w1_nvram_prt("[%s] Skip writing sta0 configuration to NVRAM "
			       "on Concurrent mode\n", __func__);
	} else
#endif	/* CONFIG_CONCURRENT */

	for (ssid = config->ssid; ssid; ssid = ssid->next) {
		ra6w1_nvram_prt("[%s] ssid->id=%d\n", __func__, ssid->id);

		if (ssid->key_mgmt == WPA_KEY_MGMT_WPS)
			continue; /* do not save temporary WPS networks */

#ifdef	CONFIG_AP_WIFI_MODE
        if (ssid->wifi_mode <= WIFI_MODE_B_ONLY) {
    		if (ssid->wifi_mode == WIFI_MODE_BG ||
    			ssid->wifi_mode == WIFI_MODE_G_ONLY ||
    			ssid->wifi_mode == WIFI_MODE_B_ONLY) {
    			ssid->disable_ht = 1;
    		} else {
    			ssid->disable_ht = 0;
    		}
        } else {
            if (ssid->wifi_mode == WIFI_MODE_A_ONLY) {
                ssid->disable_ht = 1;
            } else {
                ssid->disable_ht = 0;
            }            
        }
#endif	/* CONFIG_AP_WIFI_MODE */

	}

	return 0;
}

#if CFG_PMGR
int wpa_supp_dpm_restore_config(struct wpa_config *config)
{
	struct wpa_ssid *ssid;
	int id;
#ifdef CONFIG_STA_COUNTRY_CODE
	const char *country;
#endif /* CONFIG_STA_COUNTRY_CODE */

	ssid = config->ssid;

	if (ssid == NULL) {
		if (get_run_mode() == WIFI_DEVICE_MODE_EXT_STATION) {
			id = 0;
#if defined ( __SUPPORT_P2P__ )
		} else if (get_run_mode() == WIFI_DEVICE_MODE_EXT_P2P) {
			id = 2;
#endif // __SUPPORT_P2P__
		} else {
			ra6wx_err_prt("[DPM] %s : DPM mode not supported "
					"in this mode\n", __func__);
			return -1;
		}

		ssid = os_zalloc(sizeof(*ssid));
		if (ssid == NULL) {
			ra6wx_mem_prt("[DPM] %s: mem alloc fail...\n", __func__);
			return -1;
		}

		dl_list_init(&ssid->psk_list);
		wpa_config_set_network_defaults(ssid);

		// setband_mask
		config->setband_mask = (int)dpm_supp_conn_info->setband_mask;
		wpa_printf(MSG_DEBUG, "[DPM] dpm_supp_conn_info->setband_mask = %d\n", (int)dpm_supp_conn_info->setband_mask);

#ifdef CONFIG_STA_COUNTRY_CODE
		RM_PMGR_W_rtm_static_get(RTM_STATIC_KEY_COUNTRY_CODE, NULL, NULL, (void**)(&country));
		if(country != NULL) {
			memset(config->country, 0, 3);
			memcpy(config->country, country , 3);
			country_to_freq_range_list(&config->country_range, config->country);
		} else {
			strcpy(config->country, "US");
		}
#endif /* CONFIG_STA_COUNTRY_CODE */

		if (dpm_supp_conn_info->id == id) {
			/* connection data */
			ssid->mode = (enum wpas_mode)dpm_supp_conn_info->mode;
			ssid->disabled = dpm_supp_conn_info->disabled;

#ifdef DEF_SAVE_DPM_WIFI_MODE
			/* wifi mode addition, 180528 */
#ifdef CONFIG_AP_WIFI_MODE
			ssid->wifi_mode = dpm_supp_conn_info->wifi_mode;
#ifdef  CONFIG_DPM_OPT_WIFI_MODE
			ssid->dpm_opt_wifi_mode = dpm_supp_conn_info->dpm_opt;
#endif  /* CONFIG_DPM_OPT_WIFI_MODE */
			/** In DPM Wakeup, If WiFi mode is B/G , let's set the disable HT **/
			if (ssid->wifi_mode == WIFI_MODE_BG) {
				ssid->disable_ht = 1;
				ssid->ht = 0;
			}
#endif /* CONFIG_AP_WIFI_MODE */
#endif			
			ssid->id = dpm_supp_conn_info->id;
			ssid->ssid_len = dpm_supp_conn_info->ssid_len;
			ssid->scan_ssid = dpm_supp_conn_info->scan_ssid;
			ssid->psk_set = dpm_supp_conn_info->psk_set;
			ssid->auth_alg = dpm_supp_conn_info->auth_alg;

			os_memcpy(ssid->bssid, dpm_supp_conn_info->bssid, ETH_ALEN);

			ssid->ssid = os_calloc(32, sizeof(char));
			if(ssid->ssid == NULL) {
				ra6wx_err_prt("[DPM] %s :Failed to restore ssid.\n", __func__);
				return -1;
			}
			
			os_memcpy(ssid->ssid, dpm_supp_conn_info->ssid, dpm_supp_conn_info->ssid_len);
			if (strlen(dpm_supp_conn_ext_info->passphrase)) {
				ssid->passphrase = os_malloc(wificonfigMAX_PASSPHRASE_LEN + 1);
				if (ssid->passphrase == NULL)
					return -1;
				strcpy(ssid->passphrase, dpm_supp_conn_ext_info->passphrase);
			}

			/* key data */
			ssid->proto = dpm_supp_key_info->proto;
			ssid->key_mgmt = dpm_supp_key_info->key_mgmt;

#ifdef CONFIG_IEEE80211W
			ssid->ieee80211w = dpm_supp_conn_ext_info->ieee80211w;
#endif // CONFIG_IEEE80211W

#ifdef CONFIG_SAE
			size_t sae_password_len = strlen(dpm_supp_conn_ext_info->sae_password);
			if (sae_password_len > 0) {
				ssid->sae_password = os_malloc(sae_password_len + 1);
				if (ssid->sae_password) {
					strcpy(ssid->sae_password, dpm_supp_conn_ext_info->sae_password);
				}
			}

			if (dpm_supp_conn_ext_info->sae_groups[0]) {
				config->sae_groups = os_malloc(sizeof(int) * MAX_SAE_GROUPS);
				if (config->sae_groups) {
					os_memcpy(config->sae_groups, dpm_supp_conn_ext_info->sae_groups, sizeof(dpm_supp_conn_ext_info->sae_groups));
				}
			}
#endif // CONFIG_SAE

#ifdef CONFIG_WEP
			if (dpm_supp_key_info->wep_key_len > 0)
			{
				ssid->wep_key_len[dpm_supp_key_info->wep_tx_keyidx] = dpm_supp_key_info->wep_key_len;
				ssid->wep_tx_keyidx = dpm_supp_key_info->wep_tx_keyidx;

				os_memcpy(ssid->wep_key[dpm_supp_key_info->wep_tx_keyidx],
				dpm_supp_key_info->wep_key, MAX_WEP_KEY_LEN);
			}
			else
#endif	// CONFIG_WEP
			{
				os_memcpy(ssid->psk, dpm_supp_conn_info->psk, 32);
			}

			ssid->pairwise_cipher = dpm_supp_key_info->pairwise_cipher;
			ssid->group_cipher = dpm_supp_key_info->group_cipher;

#ifdef CONFIG_WEP
			ssid->wep_key_len[dpm_supp_key_info->wep_tx_keyidx]
							= dpm_supp_key_info->wep_key_len;
			ssid->wep_tx_keyidx = dpm_supp_key_info->wep_tx_keyidx;
			os_memcpy(ssid->wep_key[dpm_supp_key_info->wep_tx_keyidx],
					dpm_supp_key_info->wep_key, MAX_WEP_KEY_LEN);
#endif /* CONFIG_WEP */

#ifdef IEEE8021X_EAPOL
			if ((dpm_supp_conn_ext_info->identity_len > 0) && !(ssid->eap.identity)) {
				ssid->eap.identity = os_malloc(dpm_supp_conn_ext_info->identity_len);
				if (ssid->eap.identity) {
					memcpy(ssid->eap.identity, dpm_supp_conn_ext_info->identity, dpm_supp_conn_ext_info->identity_len);
					ssid->eap.identity_len = dpm_supp_conn_ext_info->identity_len;
				}
			}

			if ((dpm_supp_conn_ext_info->password_len > 0) && !(ssid->eap.password)) {
				ssid->eap.password = os_malloc(dpm_supp_conn_ext_info->password_len);
				if (ssid->eap.password) {
					memcpy(ssid->eap.password, dpm_supp_conn_ext_info->password, dpm_supp_conn_ext_info->password_len);
					ssid->eap.password_len = dpm_supp_conn_ext_info->password_len;
					ssid->eap.flags &= ~EAP_CONFIG_FLAGS_PASSWORD_NTHASH;
					ssid->eap.flags &= ~EAP_CONFIG_FLAGS_EXT_PASSWORD;
				}
			}
#endif // IEEE8021X_EAPOL
		}

		config->ssid = ssid;
	}

	ra6wx_dpm_prt("[DPM] Done restore Config Information\n");

	return 0;
}
#endif /* CFG_PMGR */

/* EOF */
