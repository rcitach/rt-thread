/**
 ****************************************************************************************
 *
 * @file supp_def.h
 *
 * @brief Feature define for RA6W1/RA6W2 wpa_supplicant
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


#ifndef	__SUPP_DEF_H__
#define	__SUPP_DEF_H__

#include "FreeRTOS.h"
#include "custom_config_sdk.h"		// For Wi-Fi configuration features

/* Start - Package Version **************************************************/
#define	VER_NUM					"2.10"
#define	RELEASE_VER_STR			" - Sep/2023"
#define	RELEASE_VER		/* Enable release version */
/* End - Package Version ****************************************************/


/* Start - Supported Wi-Fi Mode (default: define all) ***********************/
#define	CONFIG_STA
#define	CONFIG_AP
#define CONFIG_WPS	/* Wi-Fi Protected Setup (WPS) (Main) */
#define CONFIG_WEP

#ifdef __LIGHT_SUPPLICANT__
	#undef  CONFIG_P2P
	#undef  CONFIG_CONCURRENT
#else /* __LIGHT_SUPPLICANT__ */

 #ifdef __SUPPORT_WIFI_CONCURRENT_CORE__
    #define CONFIG_CONCURRENT
  #else
    #undef  CONFIG_CONCURRENT
  #endif /* __SUPPORT_WIFI_CONCURRENT_CORE__ */

  #ifdef CONFIG_CONCURRENT
    #define CONFIG_AP

    #ifdef __SUPPORT_P2P__
        #define CONFIG_P2P
        #undef  CONFIG_P2P_CONCURRENT // currently, it's not suppotred. it should be deleted when suppoted.
    #else
        #undef  CONFIG_P2P
    #endif /* __SUPPORT_P2P__ */
  #endif /* CONFIG_CONCURRENT */

  #ifdef __SUPPORT_MESH__
    #ifdef __SUPPORT_MESH_PORTAL__
      #define CONFIG_MESH_PORTAL
    #else
      #undef  CONFIG_MESH_PORTAL
    #endif /* __SUPPORT_MESH_PORTAL__ */

    #define CONFIG_MESH
    #undef  CONFIG_P2P
    #undef  CONFIG_WPS
  #else
    #undef  CONFIG_MESH
  #endif /* __SUPPORT_MESH__ */

#endif	/* __LIGHT_SUPPLICANT__ */

/* End - Supported Wi-Fi Mode ***********************************************/

/****************************************************************************/
/* Start - Common features for STA, AP, P2P *********************************/
/****************************************************************************/

/* Start - Memory Optimize - - - - - - - - - - - - - - - - - - - - - - - -  */
#define	CONFIG_IMMEDIATE_SCAN
#define	CONFIG_DISALLOW_CONCURRENT_SCAN

/* Reduce memory for storage for scan result */
#define CONFIG_SCAN_RESULT_OPTIMIZE
#define UPDATE_REQUIRED_SSID_ACTIVATED_IN_SCAN_RESULTS
#define PROBE_REQ_WITH_SSID_FOR_ASSOC

#ifdef CONFIG_SCAN_RESULT_OPTIMIZE
	#define CONFIG_SCAN_REPLY_OPTIMIZE
	#define CONFIG_TOGGLE_SCAN_SORT_TYPE
#endif /* CONFIG_SCAN_RESULT_OPTIMIZE */


/* Remove ap_scan related codes (ap_scan = 1 by default) */
#define	FEATURE_USE_DEFAULT_AP_SCAN

/* End - Memory Optimize - - - - - - - - - - - - - - - - - - - - - - - - -  */

#define FEATURE_SCAN_FREQ_ORDER_TOGGLE /* toggle scan freq order by each scan */

/* Start - messages for printf, dump, dbg - - - - - - - - - - - - - - - - - */
#define	CONFIG_LOG_MASK		/* for en/disable debug msg for each module */

#ifndef CONFIG_LOG_MASK
	#define	ENABLE_NOTICE_DBG	/* ra6wx notice debug print */
	#define	ENABLE_ERROR_DBG
	#define	ENABLE_WARN_DBG
	#define	ENABLE_FATAL_DBG
	#define	ENABLE_DEBUG_DBG
#endif /* CONFIG_LOG_MASK */

#undef  ENABLE_WPA_STATE_DBG

/* For "info" log masking */
#undef  ENABLE_SCAN_DBG			/* ra6wx scan debug msg print		*/
#undef  ENABLE_ASSOC_DBG		/* ra6wx assoc/auth debug msg print	*/
#undef  ENABLE_EVENT_DBG		/* for event debug print			*/
#undef  ENABLE_P2P_DBG			/* for p2p sequence debug msg print	*/

#undef  TX_FUNC_INDICATE_DBG	/* for tx function start/end	    */
#undef  RX_FUNC_INDICATE_DBG	/* for rx function start/end	    */

#undef  ENABLE_WPA_DBG			/* for wpa debug msg print		    */
#undef  ENABLE_WPS_DBG			/* for wps debug msg print		    */
#undef  ENABLE_STATE_CHG_DBG	/* for state change display		    */
#undef  ENABLE_SM_ENTRY_DBG     /* for state machine entry debug	*/
#undef  ENABLE_ELOOP_DBG		/* for eloop debug msg print		*/
#undef  ENABLE_DRV_DBG			/* for drv debug msg print		    */
#undef  ENABLE_EAPOL_DBG		/* for eapol debug msg print		*/
#undef  ENABLE_IFACE_DBG		/* for interface debug msg print	*/
#undef  ENABLE_EAP_DBG			/* for wps/eap debug msg print		*/
#undef  ENABLE_AP_DBG			/* for ap sequence debug msg print	*/
#undef  ENABLE_AP_MGMT_DBG		/* for ap sequence debug msg print	*/
#undef  ENABLE_AP_WMM_DBG		/* for ap wmm debug msg print		*/
#undef  ENABLE_NVRAM_DBG		/* for nvram debugging			    */
#undef  ENABLE_CLI_DBG			/* ra6wx cli debug msg print		*/
#undef  ENABLE_DPM_DBG			/* for DPM debugging			    */
#undef  ENABLE_80211N_DBG		/* for WiFi 802.11n debugging	    */
#undef  ENABLE_WNM_DBG			/* for WNM debugging 			    */
#undef  ENABLE_CRYPTO_DBG

#undef  ENABLE_WPA_DUMP_DBG		/* for wpa debug msg dump		    */
#undef  ENABLE_BUF_DUMP_DBG		/* for dump buffer					*/

#undef  ENABLE_PRINT_TK_KEY_FOR_WIRESHARK_DBG   /* To decrypt encrypted packets in Wireshark... */


/* End - messages for printf, dump, dbg - - - - - - - - - - - - - - - - - - */

/* Start - WPS - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */
#ifdef	CONFIG_WPS
	#define	CONFIG_WPS_PIN		/* WPS Pin code			    */
	#define	CONFIG_WPS_REGISTRAR
	#undef  CONFIG_WPS_AP
	#undef  CONFIG_WPS_STRICT
#endif	/* CONFIG_WPS */

/* End - WPS - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */

/* Start - Configuration for EAPOL, WPA/WPA2 Authentication - - - - - - - - */
#define CONFIG_EAPOL
#define	IEEE8021X_EAPOL
#define	CONFIG_PMKSA		/* Pairwise Master Key Security Association */
/* End - Configuration for EAPOL, WPA/WPA2 Authentication - - - - - - - - - */

/* Start - EAP - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */
#define	CONFIG_EAP_METHOD
#define	CONFIG_EAP_PEER		/* RFC 4137 EAP Peer State Machine */
#define	EAP_WSC
/* End - EAP - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */

/* Start - MIC failure - - - - - - - - - - - - - - - - - - - - - - - - - -  */
#define	CONFIG_MIC_FAILURE_RSP		/* Support MIC Faulure response	    */
#define	CONFIG_DELAYED_MIC_ERROR_REPORT	/* Support MIC Faulure response     */
#undef  CONFIG_SIMULATED_MIC_FAILURE    /* MIC failure sim. after connection */
/* End - MIC failure - - - - - - - - - - - - - - - - - - - - - - - - - - -  */

/* Start - WNM Power Save - - - - - - - - - - - - - - - - - - - - - - - - - */
#define	CONFIG_WNM
#define	CONFIG_WNM_BSS_MAX_IDLE_PERIOD
#undef  CONFIG_WNM_BSS_MAX_IDLE_PERIOD_TEST

#ifdef	CONFIG_WNM_BSS_MAX_IDLE_PERIOD_TEST
	#define	ENABLE_WNM_DBG
#endif	/* CONFIG_WNM_BSS_MAX_IDLE_PERIOD_TEST */

#if defined (__SUPPORT_11AX__)
#define	CONFIG_WNM_ACTIONS
#undef  CONFIG_WNM_SLEEP_MODE
#undef  CONFIG_WNM_TFS
#define	CONFIG_WNM_BSS_TRANS_MGMT		// IEEE80211V
#define	CONFIG_WNM_SSID_LIST
#define	CONFIG_WNM_NOTIFICATION
#endif /* __SUPPORT_11AX__ */
/* End  - WNM Power Save - - - - - - - - - - - - - - - - - - - - - - - - -  */

/* Start - WPA3 - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#if defined ( __SUPPORT_WPA3_PERSONAL_CORE__ )
	#define	CONFIG_SAE
	#define	CONFIG_OWE
	#define	CONFIG_OWE_AP
	#define	CONFIG_OWE_BEACON
	#undef  TEST_OWE_GROUP_FIXED

	#undef  CONFIG_DPP					// unsupported

	#ifdef __SUPPORT_OWE_TRANS__		// For MESH
		#define CONFIG_OWE_TRANS
	#endif // __SUPPORT_OWE_TRANS__
#else
	#undef  CONFIG_OWE
	#undef  CONFIG_OWE_AP
	#undef  CONFIG_OWE_BEACON
#endif // __SUPPORT_WPA3_PERSONAL_CORE__
/* End - WPA3 - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#define SUPPLICANT_PLAIN_TEXT_SSID

#define WPA2_PATCH_201701

#if defined ( __SUPPORT_SETBAND_5GHZ__ )
  #define CONFIG_5G_SUPPORT
#else
  #undef  CONFIG_5G_SUPPORT
#endif	// __SUPPORT_SETBAND_5GHZ__

#undef  CONFIG_6G_SUPPORT

#define SUPPORT_SELECT_NETWORK_FILTER
#ifdef CONFIG_5G_SUPPORT
	#ifndef SUPPORT_SELECT_NETWORK_FILTER
		#define SUPPORT_SELECT_NETWORK_FILTER
	#endif
#endif

/****************************************************************************/
/* End - Common features for STA, AP, P2P ***********************************/
/****************************************************************************/

/* Start - STA Features *****************************************************/
#ifdef CONFIG_STA

#define	CONFIG_ALLOW_ANY_OPEN_AP    /* Allow connection to any open mode AP */
#undef  CONFIG_SIMPLE_ROAMING       /* MBO feature replaces simple roaming. */
#define	FIXED_ISSUES_LOCAL_DEAUTH	/* ignore_next_local_deauth clear   */
#define	CONFIG_STA_POWER_SAVE
#undef  CONFIG_DPM_OPT_WIFI_MODE	/* Unsupport 2.5/5GHz Dual Band Mode */

#define	DEF_SAVE_DPM_WIFI_MODE	/* save/restore wifi connection mode in dpm reconnection */

#ifdef	__SUPPORT_WPA_ENTERPRISE_CORE__
#define	CONFIG_ENTERPRISE		/* Need CONFIG_TLS		    */
	#ifdef __SUPPORT_WPA3_ENTERPRISE_192B_CORE__
		// WPA3-Enterprise
		#define CONFIG_SUITEB
		#define CONFIG_SUITEB192
		#define CONFIG_SHA384
	#endif /* __SUPPORT_WPA3_ENTERPRISE_192B_CORE__ */
#else
	#undef  CONFIG_ENTERPRISE	/* Need CONFIG_TLS          */
	// WPA3-Enterprise
	#undef CONFIG_SUITEB
	#undef CONFIG_SUITEB192
	#undef CONFIG_SHA384
#endif	/* __SUPPORT_WPA_ENTERPRISE_CORE__ */

/* Start - EAP Methods for WPA/WPA2-Enterprise - - - - - - - - - - - - - -  */
#ifdef	CONFIG_ENTERPRISE
	#undef  EAP_MD5
	#define	EAP_TLS			/* Mandatory in ra6wx */
	#define	EAP_PEAP		/* Mandatory in ra6wx */
	#define	EAP_TTLS		/* Mandatory in ra6wx */
	#define	EAP_FAST		/* Mandatory in ra6wx */
	#define	EAP_MSCHAPv2	/* Mandatory in ra6wx */
	#define	EAP_GTC			/* Mandatory in ra6wx */
#endif	/* CONFIG_ENTERPRISE */
/* End - EAP Methods for WPA/WPA2-Enterprise - - - - - - - - - - - - - - -  */

/* Start - Auto Scan features - - - - - - - - - - - - - - - - - - - - - - - */
#undef  CONFIG_AUTOSCAN
#undef  CONFIG_AUTOSCAN_PERIODIC
#undef  CONFIG_AUTOSCAN_EXPONENTIAL
/* End - Auto Scan features - - - - - - - - - - - - - - - - - - - - - - - - */

/* Try scanning previous defined connection channel */
#define	CONFIG_RECONNECT_OPTIMIZE /* fast reconnect */

/* Fast Connection in Sleep mode 1,2 */
/* IN SLEEP MODE 1 & FAST CONNECTION */
#ifdef __SUPPORT_ASSOC_CHANNEL__
#define CONFIG_FAST_CONN_ASSOC_CH
#endif /* __SUPPORT_ASSOC_CHANNEL__ */

#ifdef __SUPPORT_BSSID_SCAN__ /* Scan with BSSID */
#define CONFIG_SCAN_WITH_BSSID
#else
#undef CONFIG_SCAN_WITH_BSSID
#endif

#ifdef __SUPPORT_DIR_SSID_SCAN__ /* Scan with SSID */
#define CONFIG_SCAN_WITH_DIR_SSID
#else
#undef CONFIG_SCAN_WITH_DIR_SSID
#endif

#define CONFIG_FAST_CONNECTION 
#define CONFIG_STA_COUNTRY_CODE	/* Set country code, modify scan parameter */
#undef  CONFIG_STA_BSSID_FILTER

#define CONFIG_RECEIVE_DHCP_EVENT

#undef AUTO_WEPKEY_INDEX /* Automatic selection of wep key index */

#ifdef CONFIG_IMMEDIATE_SCAN
#undef CONFIG_FAST_RECONNECT
#undef CONFIG_FAST_RECONNECT_V2
#endif /* CONFIG_IMMEDIATE_SCAN */
#endif /* CONFIG_STA */

/* End - STA Features *******************************************************/

/* Start - AP Features ******************************************************/
#ifdef	CONFIG_AP

#define	NEED_AP_MLME
#define	CONFIG_AP_HW_FEATURE
#define	CONFIG_8021X		/* for ieee 802.1x authentication	    */
#undef  CONFIG_NO_HOSTAPD_LOGGER
#define	CONFIG_DBG_LOG_MSG
#define	CONFIG_AP_WMM		/* for Soft-AP WMM			    */

#define	CONFIG_EAP_SERVER

#ifdef	CONFIG_WPS
	#define	EAP_SERVER_WSC				/* for WPS	*/
	#define	EAP_SERVER_IDENTITY			/* for WPS	*/
	#define	CONFIG_AP_PLAIN_TEXT_SEC	/* for WPS	*/
#endif	/* CONFIG_WPS */

#define	CONFIG_AP_MANAGE_CLIENT /* cli commands for managing client	    */

#ifdef	__SCAN_ON_AP_MODE__
	#define	ENABLE_SCAN_ON_AP_MODE	/* Enable scan in AP mode	    */
#else
	#undef  ENABLE_SCAN_ON_AP_MODE	/* Disable scan in AP mode	    */
#endif	/* __SCAN_ON_AP_MODE__ */

#define	CONFIG_AP_ISOLATION			/* Isolation */
#define	CONFIG_ACL					/* ACL (Access Control List)		    */
#define	CONFIG_AP_NONE_STA			/* Check - No Station left				*/
#define	CONFIG_AP_POWER				/* Set Power							*/
#define	CONFIG_IEEE80211N			/* Enable IEEE 802.11n Support for AP	*/
#define	CONFIG_AP_HT				/* AP High Throughput					*/
#define	CONFIG_HT_OVERRIDES			/* 802.11n enable/disable				*/
#define	CONFIG_AP_WIFI_MODE			/* Wi-Fi mode set : bgn/bg/n/g/b/an/a/n(5g) */
#define	CONFIG_ACS					/* Enable Automatic Channel Selection	*/
#define	CONFIG_AP_PARAMETERS
#define	CONFIG_AP_REASSOC_OPTIMIZE	/* Prevent memory free/realloc on reassociation */

#undef  CONFIG_AP_TEST_SKIP_TX_STATUS
#define	CONFIG_IEEE80211D			/* ieee 802.11d geographical regulations*/
#undef  CONFIG_AP_WDS				/* for AP WDS 							*/
#ifdef CONFIG_5G_SUPPORT
	#define	CONFIG_AP_DFS				/* for Dynamic Channel Selection : 5GHz	*/
#endif	
#undef  CONFIG_AP_SECURITY_WEP		/* for AP security : WEP				*/
#undef  CONFIG_AP_VLAN				/* for AP VLAN function					*/
#define CONFIG_NO_VLAN

#undef  CONFIG_FULL_DYNAMIC_VLAN
#undef  CONFIG_RADIUS				/* for RADIUS							*/
#define	CONFIG_NO_RADIUS			/* for RADIUS							*/
#undef  RADIUS_SERVER

#ifdef CONFIG_5G_SUPPORT
  #define	CONFIG_IEEE80211H		/* ieee 802.11h DFS						*/
#else
  #undef 	CONFIG_IEEE80211H		/* ieee 802.11h DFS						*/
#endif // CONFIG_5G_SUPPORT

#undef  CONFIG_IEEE80211F			/* ieee 802.11f IAPP					*/
#undef  CONFIG_IEEE80211R			/* ieee 802.11r Fast Secure Roaming		*/
#undef  CONFIG_IEEE80211R_AP		/* ieee 802.11r Fast Secure Roaming		*/
#define	CONFIG_CTRL_IFACE

#endif	/* CONFIG_AP */
/* End - AP Features ********************************************************/


/* Start - P2P (Wi-Fi Direct) ***********************************************/
#ifdef	CONFIG_P2P

#define	CONFIG_P2P_POWER_SAVE		/* p2p Power Save (OpPS & NoA)		    */
#undef  CONFIG_P2P_OPTION			/* p2p option func. (invite, SD)	    */
#undef  ENABLE_EXTRA_CONF
#undef  CONFIG_BRIDGE_IFACE
#undef  CONFIG_WIFI_DISPLAY
#undef  CONFIG_ACL_P2P				/* P2P GO ACL (Access Control List)	    */

#endif	/* CONFIG_P2P */
/* End - P2P (Wi-Fi Direct) *************************************************/


/* Start - Not supported, might be used in future ***************************/

#undef  CONFIG_MACSEC				/*  MACsec secure session				*/
#undef  CONFIG_NO_PBKDF2

#define	CONFIG_IEEE80211AC			/* IEEE802.11AC						    */

#if defined (__SUPPORT_11AX__)
#define	CONFIG_IEEE80211AX			/* IEEE802.11AX						    */
#define	CONFIG_TWT
#define	CONFIG_INTERWORKING
#endif /* __SUPPORT_11AX__ */

#undef  CONFIG_IEEE80211R			/* Fast BSS Transition;FT (FT-PSK)	    */
#undef  INTERWORKING_3GPP
#undef  CONFIG_ROAMING_PARTNER
#undef  CONFIG_BGSCAN				/* background scan and roaming interface	*/
#undef  CONFIG_BGSCAN_LEARN			/* bg scan and roaming module: learn	*/
#undef  CONFIG_EXCLUDE_SSID
#undef  CONFIG_PRIO_GROUP			/* To disable priority list				*/
#undef  CONFIG_PRE_AUTH				/* Preauth. for IEEE 802.11r		    */
#undef	CONFIG_FAST_REAUTH			// Fast reauth
#undef  CONFIG_NOTIFY
#if defined (__SUPPORT_11AX__)
#define	CONFIG_RRM					// radio resource management --- ieee80211k
#endif /* __SUPPORT_11AX__ */
#define	CONFIG_NO_CONFIG_BLOBS
#if defined (__SUPPORT_SETBAND_5GHZ__)
#define	CONFIG_BAND_5GHZ
#define	CONFIG_BAND
#define	CONFIG_AP_SUPPORT_5GHZ
#define	CONFIG_IEEE80211AC_WMM
#define	CONFIG_WMM_ACTIONS
#endif /* __SUPPORT_SETBAND_5GHZ__ */
#define	CONFIG_LAST_SEQ_CTRL
#undef  CONFIG_EAP_PASSWORD
#undef  CONFIG_MODULE_TESTS
#undef  CONFIG_PTKSA_CACHE

#define	CONFIG_SME

#if defined(CONFIG_STA) || defined(CONFIG_AP)
#define CONFIG_SCAN_UMAC_HEAP_ALLOC
#endif // defined(CONFIG_STA) || defined(CONFIG_AP)

#if defined(CONFIG_P2P)
#define CONFIG_SCAN_UMAC_HEAP_ALLOC
#endif // defined(CONFIG_P2P)

#ifdef CONFIG_SCAN_UMAC_HEAP_ALLOC
#define CONFIG_REUSED_UMAC_BSS_LIST
#else
#undef CONFIG_REUSED_UMAC_BSS_LIST
#endif

#ifdef __SUPPORT_IEEE80211W__
	#define	CONFIG_IEEE80211W			/* Protected Management Frame		    */
	#undef  CONFIG_IEEE80211W_SIGMA		/* Only define for sigma test			*/
#endif

/* Diabled SAE FFC */
#ifdef	CONFIG_SAE
	#define	__DISABLE_SAE_FFC__
#endif // CONFIG_SAE

#define CONFIG_MONITOR_THREAD_EVENT_CHANGE

/* Start - Code Optimize *****************************************************/

#undef  CONFIG_FST				// Don't enable this feature
#undef  CONFIG_FILS				// Don't enable this feature
#undef  CONFIG_EXT_PASSWORD		// Don't enable this feature
#undef  CONFIG_IBSS_RSN			// Don't enable this feature
#undef  CONFIG_ERP				// Don't enable this feature

#define	CONFIG_RIC_ELEMENT
#define	CONFIG_LCI					// Use in rrm
#undef  CONFIG_SCHED_SCAN
#undef  CONFIG_SCAN_WORK
#undef  CONFIG_SCAN_OFFLOAD
#undef  CONFIG_SCAN_FILTER_RSSI
#undef  CONFIG_SRP
#undef  CONFIG_IBSS
#undef  CONFIG_BSS_DMG
#define	CONFIG_VENDOR_ELEM
#undef  CONFIG_AP_VENDOR_ELEM
#undef  CONFIG_WOWLAN
#undef  CONFIG_NEIGHBOR_AP_DB
#undef  CONFIG_RANDOM_ADDR
#undef  CONFIG_M2U_BC_DEAUTH
#undef  CONFIG_ASSOC_CB
#undef  CONFIG_RADIO_WORK
#undef  CONFIG_IGNOR_OLD_SCAN
#undef  CONFIG_OPENSSL_MOD
#undef  CONFIG_SUPP27_EAPOL
#undef  CONFIG_SUPP27_IFACE
#undef  CONFIG_AP_BSS_LOAD_UPDATE
#undef  CONFIG_CHANNEL_UTILIZATION
#undef  CONFIG_MAC_RAND_SCAN
#undef  CONFIG_CTRL_PNO
#undef  CONFIG_DISALLOW_BSSID
#undef  CONFIG_DISALLOW_SSID
#undef  CONFIG_AP_VHT               /* Very High Throughput */
#undef  CONFIG_MSCS                 /* Mirrored Stream Classification Service */
#undef  CONFIG_SCS                  /* Stream Classification Service */
#undef  CONFIG_DSCP                 /* Differentiated Service Code Point */
#if defined (__SUPPORT_11AX__)
#define	CONFIG_MBO					/* Multi Band Operation */
#define	CONFIG_WPA_CRED
#define CONFIG_RADIO_MEASUREMENTS	// radio measurements
#endif /* __SUPPORT_11AX__ */
#define	CONFIG_GAS                  /* Group Advertisments Service */

#undef  CONFIG_VENUE_NAME
#undef  CONFIG_NETWORK_AUTH_TYPE
#undef  CONFIG_ROAM_CONSORTIUM
#undef  CONFIG_IP_ADDR_AUTH_TYPE
#undef  CONFIG_NAI_REALM
#undef  CONFIG_DOMAIN_NAME
#undef  CONFIG_3GPP
#undef  CONFIG_PCSC_FUNCS
#undef  CONFIG_EXT_SIM
#undef  CONFIG_GAS_SERVER
#undef  CONFIG_HS20

#undef  CONFIG_SUPP27_PROBE_REQ
#if	defined ( CONFIG_SUPP27_PROBE_REQ )
	#undef  CONFIG_NO_AUTH_IF_SEEN_ON
	#undef  CONFIG_NO_PROBE_RESP_IF_SEEN_ON
	#undef  CONFIG_NO_PROBE_RESP_IF_MAX_STA
#endif	// CONFIG_SUPP27_PROBE_REQ

#undef  CONFIG_SUPP27_KEY_MGMT
#undef  CONFIG_SUPP27_RADIUS
#undef  CONFIG_SUPP27_CIPHER
#undef  CONFIG_SUPP27_STA_SM
#undef  CONFIG_SUPP27_AUTH
#undef  CONFIG_SUPP27_MIC_LEN
#undef  CONFIG_SUPP27_CONFIG_NVRAM
#undef  CONFIG_SUPP27_CONFIG
#define	CONFIG_SUPP27_SCAN
#undef  CONFIG_SUPP27_AP_DRV_CB
#undef  CONFIG_SUPP27_AP_NOTIF_ASSOC
#undef  CONFIG_SUPP27_ROAM_CONSORTIUM
#undef  CONFIG_SUPP27_EVENTS
#define	CONFIG_SUPP27_STA_INFO
#define	CONFIG_STA_EXT_CAPAB
#undef  CONFIG_SHA384_WPS

#if defined ( CONFIG_ENTERPRISE )
#define CONFIG_SUPP27_BIN_CLR_FREE
#else
#undef  CONFIG_SUPP27_BIN_CLR_FREE
#endif	// CONFIG_ENTERPRISE

#undef  CONFIG_SUPP27_STR_CLR_FREE
#undef  CONFIG_SUPP27_DRV_80211
#undef  CONFIG_SUPP27_BEACON
#undef  CONFIG_SUPP27_WPA_DRV_SMPS_MODE
#undef  CONFIG_SUPP27_STA_SEEN
#undef  CONFIG_SUPP27_STA_TRACK
#undef  CONFIG_SUPP27_DFS_DOMAIN

#undef  CONFIG_SUPP27_WPS_NFC
#undef  CONFIG_SUPP27_WPS_2ND_DEV
#undef  CONFIG_SUPP27_WPS_DUALBAND

#undef  UNUSED_CODE_EAP_SIM_DB          // EAP_SIM_DB
#undef  UNUSED_CODE_SUPP_CONFIG
#undef  UNUSED_CODE_SUPP_CONFIG_NVRAM
#undef  UNUSED_CODE_DELETE              // FOR SAVE MEMORY

#if (defined ( __SUPPORT_IPV4__ ) && defined ( __SUPPORT_IPV6__ ))
  #define CONFIG_IPV6
  #define CONFIG_IPV4
#elif defined ( __SUPPORT_IPV4__ )
  #undef  CONFIG_IPV6
  #define CONFIG_IPV4
#elif defined ( __SUPPORT_IPV6__ )
  #define CONFIG_IPV6
  #undef  CONFIG_IPV4
#endif // __SUPPORT_IPV6__

// ..........................................................................
// For temporary debugging during initial implementation
//
#define DEBUG_SUPP_TMP    0    // 0:Disable, 1:Enable
// ..........................................................................

/* End - Code Optimize *******************************************************/

/* End - Not supported, might be used in future *****************************/

#endif /*__SUPP_DEF_H__*/
/* EOF */
