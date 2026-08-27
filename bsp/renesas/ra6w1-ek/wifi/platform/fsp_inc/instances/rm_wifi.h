/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/*******************************************************************************************************************//**
 * @addtogroup WIFI
 * @{
 **********************************************************************************************************************/

#ifndef RM_WIFI_H
#define RM_WIFI_H

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

/* Register definitions, common services and error codes. */
#include "bsp_api.h"
#include "time.h"

#include "r_ioport_api.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "stream_buffer.h"
#include "sdk_defs.h"
#include "rm_wifi_api.h"
#include "rm_watchdog_service_w.h"
#include "rm_vee_flash_w.h"
#include "rm_wifi_config.h"
#include "r_rtc_api.h"
#ifdef RM_STDIO_W
#include "rm_stdio_w_cfg.h"
#endif
#ifdef RM_MAP_PERSISTANT_W
#include "rm_map_persistant_w.h"
#endif

/* Common macro for FSP header files. There is also a corresponding FSP_FOOTER macro at the end of this file. */
FSP_HEADER

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#define GAP_USER_CONFIGURE_MODE 1
#define GAP_USER_CONFIGURE_WIFI_MODE 1
#define GAP_USER_CONFIGURE_SECURITY 1
#define RM_WIFI_PIN_BUFF_SIZE 9

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/** User configuration structure, used in open function */
typedef struct st_wifi_coex_cfg
{
    uint32_t coex_asc1_pin;
    uint32_t coex_asc2_pin;
    uint32_t coex_bt_act_pin;
    uint32_t coex_asc1_reset_val;
    uint32_t coex_asc2_reset_val;
} wifi_coex_cfg_t;

typedef struct st_wifi_cfg
{
    const uint32_t num_uarts;           ///< Number of UART interfaces to use
    const uint32_t num_sockets;         ///< Number of sockets to initialize
    const bsp_io_port_pin_t reset_pin;  ///< Reset pin used for module
    watchdog_service_instance_t const * p_watchdog_service; ///< Pointer to Watchdog Service instance.
    rm_vee_instance_t const * p_vee_service; ///< Pointer to Watchdog Service instance.
    rtc_instance_t const * p_rtc_w_service;  ///< Pointer to RTC Service instance. 
    void const * p_context;             ///< User defined context passed into callback function.
    void const * p_extend;              ///< Pointer to extended configuration by instance of interface.
    uint32_t coex_enabled;              ///< Flag to indicate if Co-existence is enabled.
    wifi_coex_cfg_t coex_cfg;         ///< Coexistence struct include the relevent config for coex.
} wifi_cfg_t;

typedef struct
{
    uint32_t open;                       ///< Flag to indicate if wifi instance has been initialized
    wifi_cfg_t const * p_wifi_cfg;   ///< Pointer to initial configurations.
    bsp_io_port_pin_t reset_pin;         ///< Wifi module reset pin
    uint32_t num_uarts;                  ///< number of UARTS currently used for communication with module
    uint32_t tx_data_size;               ///< Size of the data to send
    uint32_t num_creatable_sockets;      ///< Number of simultaneous sockets supported
    uint32_t curr_cmd_port;              ///< Current UART instance index for AT commands
    uint32_t curr_data_port;             ///< Current UART instance index for data
    volatile uint32_t curr_socket_index; ///< Currently active socket instance
    uint32_t at_cmd_mode;                ///< Current command mode
    uint8_t curr_ipaddr[4];              ///< Current IP address of module
    uint8_t curr_subnetmask[4];          ///< Current Subnet Mask of module
    uint8_t curr_gateway[4];             ///< Current GAteway of module
} wifi_instance_ctrl_t;

typedef enum
{
    WIFI_DEVICE_MODE_EXT_STATION         = 0,    ///< Station mode
    WIFI_DEVICE_MODE_EXT_AP,                     ///< Access point mode
    WIFI_DEVICE_MODE_EXT_P2P,                    ///< P2P mode
    WIFI_DEVICE_MODE_EXT_P2P_GO,                 ///< P2P GO mode
    WIFI_DEVICE_MODE_EXT_AP_STATION,             ///< AP + Station mode
    WIFI_DEVICE_MODE_EXT_P2P_STATION,            ///< P2P + Station mode
    WIFI_DEVICE_MODE_EXT_MESH_POINT,             ///< Mesh point mode
    WIFI_DEVICE_MODE_EXT_MESH_PORTAL,            ///< Mesh portal mode
    WIFI_DEVICE_MODE_EXT_NOT_SUPPORTED,          ///< Unsupported mode
} e_wifi_device_mode_ext_t;

typedef enum
{
    WIFI_MODE_AUTO_STA,  /* 0 */ /* Station 11b/g/n/ax/a/n(5G) */
    WIFI_MODE_BGN = 0,   /* 0 */ /* SoftAP */
    WIFI_MODE_GN,        /* 1 */
    WIFI_MODE_BG,        /* 2 */
    WIFI_MODE_N_ONLY,    /* 3 */
    WIFI_MODE_G_ONLY,    /* 4 */
    WIFI_MODE_B_ONLY,    /* 5 */
    // 5 GHz
    WIFI_MODE_AN,        /* 6 */
    WIFI_MODE_A_ONLY,    /* 7 */
    WIFI_MODE_N_ONLY_5G, /* 8 */
    WIFI_MODE_MAX
} e_wifi_phy_mode_ext_t;

typedef enum
{
    ENT_AUTH_TYPE_NONE               = 0,
    ENT_AUTH_TYPE_PEAP_TTLS_FAST,
    ENT_AUTH_TYPE_PEAP,
    ENT_AUTH_TYPE_TTLS,
    ENT_AUTH_TYPE_FAST,
    ENT_AUTH_TYPE_TLS,
    ENT_AUTH_TYPE_UNSUPPORTED,
} WIFIEntAuthTypeExt_t;

typedef enum
{
    ENT_AUTH_PROTO_NONE              = 0,
    ENT_AUTH_PROTO_MSCHAPv2_GTC,
    ENT_AUTH_PROTO_MSCHAPv2,
    ENT_AUTH_PROTO_GTC,
    ENT_AUTH_PROTO_TLS,
    ENT_AUTH_PROTO_UNSUPPORTED,
} WIFIEntAuthProtoExt_t;

/** Wi-Fi PMF (Protected Management Frames) modes */
typedef enum
{
    PMF_NONE,
    PMF_CAPABLE,
    PMF_REQUIRED,
    PMF_DEFAULT,
} WIFIPmf_t;

/** Wi-Fi Security extended types, backwards compatible with WIFISecurity_t */
typedef enum
{
    eWiFiSecurityOpen_ext         = eWiFiSecurityOpen,         ///< Open - No Security
    eWiFiSecurityWEP_ext          = eWiFiSecurityWEP,          ///< WEP Security
    eWiFiSecurityWPA_ext          = eWiFiSecurityWPA,          ///< WPA Security
    eWiFiSecurityWPA2_ext         = eWiFiSecurityWPA2,         ///< WPA2 Security
    eWiFiSecurityWPA2_ent_ext     = eWiFiSecurityWPA2_ent,     ///< WPA2 Enterprise Security
    eWiFiSecurityWPA3_ext         = eWiFiSecurityWPA3,         ///< WPA3 Security
    eWiFiSecurityNotSupported_ext = eWiFiSecurityNotSupported, ///< Unknown Security
    eWiFiSecurityWPA_ent_ext,                                  ///< WPA Enterprise Security
    eWiFiSecurityWPA_WPA2_ent_ext,                             ///< WPA + WPA2 Enterprise Security
    eWiFiSecurityWPA2_WPA3_ent_ext,                            ///< WPA2 + WPA3 Enterprise Security
    eWiFiSecurityWPA3_ent_ext,                                 ///< WPA3 128 Bits Enterprise Security
    eWiFiSecurityWPA3_192B_ent_ext,                            ///< WPA3 192 Bits Enterprise Security
    eWiFiSecurityWPA_WPA2_ext,                                 ///< WPA + WPA2 Security
    eWiFiSecurityWPA2_WPA3_ext,                                ///< WPA2 + WPA3 Security
    eWiFiSecurityWPA3_OWE_ext,                                 ///< WPA3-OWE Security
    eWiFiSecurityMax_ext
} WIFISecurityExt_t;

typedef enum
{
    eWiFiEncryptionNone = 0,
    eWiFiEncryptionTKIP = 1,
    eWiFiEncryptionAES = 2,
    eWiFiEncryptionTKIP_AES = 3
} WIFIEncryption_t;

typedef enum
{
    WIFI_TWT_EVENT_SESSION_SUCCESS,
    WIFI_TWT_EVENT_AP_REJECTED,
    WIFI_TWT_EVENT_AP_RESPONSE_PARAM_NOT_MATCHED,
    WIFI_TWT_EVENT_TEARDOWN_SUCCESS,
    WIFI_TWT_EVENT_AP_TEARDOWN_SUCCESS,
    WIFI_TWT_EVENT_MAX_RETRIES_REACHED,
    WIFI_TWT_EVENT_AP_NO_SUPPORT,
    WIFI_TWT_EVENT_TWT_REQ_ABORT,
    WIFI_TWT_EVENT_MAX,
} WIFI_TWTEvent_t;

typedef struct
{
    WIFI_TWTEvent_t xEvent;
} WiFiEventExtInfoTWT_t;

typedef enum
{
    eWiFiEventExtMin = eWiFiEventMax,
    eWiFiEventExtTWT = eWiFiEventExtMin,   ///< TWT Event
    eWiFiEventExtMax
} WIFIEventExtType_t;

typedef struct
{
    WIFIEventExtType_t xEventExtType;
    union
    {
        WiFiEventExtInfoTWT_t xTWT;
    } xInfo;
} WIFIEventExt_t;

typedef struct
{
    unsigned char oper_chan;                            ///< p2p operational channel
    unsigned char listen_chan;                          ///< p2p listening channel
    unsigned char go_intent_chan;                       ///< p2p intent group owner channel
    char ssid_postfix[wificonfigSSID_POSTFIX_MAX_LEN];  ///< p2p intent group owner channel
} wifi_p2p_ext;

typedef struct
{
    WIFIScanConfig_t pxScanConfig; ///< Wi-Fi scan configuration
    WIFIBand_t       ucBand;       ///< Band to scan
} WIFIScanExtendedConfig_t;

typedef struct
{
    WIFIEntAuthTypeExt_t ucEntAuthType;
    WIFIEntAuthProtoExt_t ucEntAuthProto;
    char ucID[wificonfigMAX_ENT_IDENTITY_LEN];
    uint8_t ucIDLength;
    char ucPassword[wificonfigMAX_ENT_PASSWORD_LEN];
    uint8_t ucPasswordLength;
    char ucCert[wificonfigMAX_CERT_LEN];
    uint16_t ucCertLength;
    char ucPrivKey[wificonfigMAX_PRIV_KEY_LEN];
    uint16_t ucPrivKeyLength;
} WIFIEnterpriseNetParams_t;

typedef struct
{
    int  ap_max_inactivity;                  ///< Timeout in seconds to detect STA's inactivity
    WIFIEncryption_t ucEncMode;
} WIFIApNetParams_t;

/** Parameters passed to WIFI_ConfigureAPExt API for AP configuration */
typedef struct
{
    WIFINetworkParams_t xNetworkParams;      ///< Basic configuration
    WIFIEnterpriseNetParams_t xEntNetParams; ///< Enterprise security configuration
    WIFIApNetParams_t xApNetParams;          ///< AP configuration
    WIFIBand_t ucBand;                       ///< Band to configure
    e_wifi_phy_mode_ext_t ucWiFi_mode;       ///< Wi-Fi PHY mode to configure (a, b, g ,n, ...)
    bool hidden_ssid;                        ///< AP is using hidden SSID
    WIFIPmf_t pmf;                           ///< Protected Management Frame mode
    char sae_groups[wificonfigMAX_SAE_GROUPS_LEN]; ///< String contains all the WPA3 sae group ids
    uint8_t ucNumChannels;                   ///< Number of channels to scan
    uint32_t * pucChannelList;               ///< Pointer to list of channels to scan
} WIFINetworkParamsExt_t;

typedef struct
{
    WIFIConnectionInfo_t connection_info;
    uint8_t wifi_generation;
} WIFIConnectionInfoExt_t;

/* TWT Teardown request message structure */
struct twt_teardown_req
{
    /* TWT Negotiation type */
    uint8_t neg_type;
    /* All TWT */
    uint8_t all_twt;
    /* TWT flow ID */
    uint8_t id;
    /* VIF Index */
    uint8_t vif_idx;
};

/* TWT Flow configuration */
struct twt_conf_tag
{
    /* Flow Type (0: Announced, 1: Unannounced) */
    uint8_t flow_type;
    /* Wake interval Exponent */
    uint8_t wake_int_exp;
    /* Unit of measurement of TWT Minimum Wake Duration (0:256us, 1:tu) */
    uint8_t wake_dur_unit;
    /* Nominal Minimum TWT Wake Duration  (unit is defined by wake_dur_unit) */
    uint8_t min_twt_wake_dur;
    /* Trigger disable/enable (0: non-trigger, 1: Trigger enabled) */
    uint8_t trigger;
    /* Negotiation type (0: Individual, 1: wake TBTT, 2: BCAST, 3: BCAST MGMT) */
    uint8_t neg_type;
    /* TWT Wake Interval Mantissa */
    uint16_t wake_int_mantissa;
};

/* TWT Setup request message structure */
struct twt_setup_req
{
    /* VIF Index */
    uint8_t vif_idx;

    /* Setup request type */
    uint8_t setup_type;

    /* TWT Setup configuration */
    struct twt_conf_tag conf;

    /* Auto TWT Setup at connection*/
    uint8_t auto_setup;
};

 /**
 * @brief Wi-Fi Extended event handler definition.
 *
 * @param[in] xEventExt - Wi-Fi Extended event data structure.
 *
 * @return None.
 */
typedef void (* WIFIEventExtHandler_t)(WIFIEventExt_t * xEventExt);

/*******************************************************************************************************************//**
 * @} (end addtogroup WIFI)
 **********************************************************************************************************************/

/**********************************************************************************************************************
 * Exported global variables
 **********************************************************************************************************************/
extern const wifi_cfg_t g_wifi_cfg;
/**********************************************************************************************************************
 * Public Function Prototypes
 **********************************************************************************************************************/
fsp_err_t rm_wifi_wep_network_params_check(const WIFINetworkParams_t * const pxNetworkParams);
fsp_err_t rm_wifi_enterprise_network_params_check(const WIFINetworkParamsExt_t * const pxNetworkParamsExt);
fsp_err_t rm_wifi_open(wifi_cfg_t const *const p_cfg);
fsp_err_t rm_wifi_open_connect(wifi_cfg_t const * const p_cfg);
fsp_err_t rm_wifi_close();
fsp_err_t rm_wifi_connect(const WIFINetworkParams_t * const pxNetworkParams, bool hidden_ssid,
                          WIFIPmf_t pmf, const char * sae_groups,
                          uint8_t ucNumChannels, uint32_t * pucChannelList);
fsp_err_t rm_wifi_connect_Ext(const WIFINetworkParamsExt_t * const pxNetworkParamsExt);
fsp_err_t rm_wifi_auto_config(const WIFINetworkParams_t * const pxNetworkParams);
fsp_err_t rm_wifi_disconnect();
fsp_err_t rm_wifi_p2p(unsigned char oper_chan, unsigned char listen_chan, unsigned char go_intent, const char * ssid_postfix);
fsp_err_t rm_wifi_p2p_find(void);
fsp_err_t rm_wifi_p2p_connect(const char *p2p_addr, const char *wps_method);
fsp_err_t rm_wifi_p2p_group_remove(void);
fsp_err_t rm_wifi_p2p_group_add(void);
fsp_err_t rm_wifi_p2p_peers(char * reply);
fsp_err_t rm_wifi_p2p_accept(void);
fsp_err_t rm_wifi_p2p_set_oper_chan(unsigned char channel);
fsp_err_t rm_wifi_p2p_set_listen_chan(unsigned char channel);
fsp_err_t rm_wifi_p2p_set_go_intent(unsigned char intent);
fsp_err_t rm_wifi_p2p_set_find_timeout(unsigned char timeout);
fsp_err_t rm_wifi_p2p_set_ssid_postfix(const char * ssid_postfix);
fsp_err_t rm_wifi_p2p_get(char *p2pconfig);
fsp_err_t rm_wifi_isolate(bool enable_isolation);
fsp_err_t rm_wifi_acl_mac(const char *mac_addr);
fsp_err_t rm_wifi_acl(const char *filter_func);
fsp_err_t rm_wifi_wmm(bool enable_wmm);
fsp_err_t rm_wifi_wmm_ps(bool enable_wmm_ps);
fsp_err_t rm_wifi_wps_pbc(const char *mac_addr);
fsp_err_t rm_wifi_wps_pin(const char *mac_addr, const char *pin, char *gen_pin);
fsp_err_t rm_wifi_wps_cancel(void);
fsp_err_t rm_wifi_wps_ap_pin(const char *state, const char *arg1, const char *arg2, char *gen_pin);
fsp_err_t rm_wifi_ps_mode_set(const bool enable);
fsp_err_t rm_wifi_ps_mode_get(bool *config);
fsp_err_t rm_wifi_set_listen_interval(const int listen_interval);
e_wifi_device_mode_ext_t WIFI_Convert_Mode_To_Ext_Mode(WIFIDeviceMode_t xDeviceMode);
WIFIDeviceMode_t WIFI_Convert_Ext_Mode_To_Mode(e_wifi_device_mode_ext_t xDeviceModeExt);
fsp_err_t rm_wifi_set_mode(e_wifi_device_mode_ext_t xDeviceModeExt);
fsp_err_t rm_wifi_get_mode(e_wifi_device_mode_ext_t *xDeviceModeExt);
fsp_err_t rm_wifi_network_add(const WIFINetworkProfile_t * const pxNetworkProfile, uint16_t index);
fsp_err_t rm_wifi_network_del(uint16_t index);
fsp_err_t rm_wifi_mac_addr_get(uint8_t *p_macaddr);
fsp_err_t rm_wifi_scan(WIFIScanResult_t *p_results, uint32_t maxNetworks);
fsp_err_t rm_wifi_scan_extended(WIFIScanResult_t *p_results, uint32_t maxNetworks, WIFIScanExtendedConfig_t * pxScanConfigExtended);
fsp_err_t rm_wifi_start_ap();
fsp_err_t rm_wifi_stop_ap();
fsp_err_t rm_wifi_configure_ap(WIFINetworkParamsExt_t *pxNetworkParamsExt);
fsp_err_t rm_wifi_is_connected(const WIFINetworkParams_t *pxNetworkParams);
fsp_err_t rm_wifi_get_connection_info(WIFIConnectionInfo_t *pxConnectionInfo, int iface_num);
fsp_err_t rm_wifi_get_connection_info_ext(WIFIConnectionInfoExt_t *pxConnectionInfoExt, int iface_num);
fsp_err_t rm_wifi_get_rssi(int8_t *pcRSSI);
fsp_err_t rm_wifi_start_disconnect_station(uint8_t * pucMac);
fsp_err_t rm_wifi_mac_addr_set(uint8_t *pucMac);
fsp_err_t rm_wifi_set_country_code(const char *pcCountryCode);
fsp_err_t rm_wifi_get_country_code(char *pcCountryCode);
fsp_err_t rm_wifi_set_atcmd_event_callback(void * const p_ctrl,
                                            unsigned int (* p_callback)(void * const p_ctrl, int index,
                                                                        unsigned char * p_in, unsigned int inlen));
fsp_err_t rm_wifi_set_startup_atcmd_event_callback(void * const p_ctrl,
                                                    unsigned int (* p_callback)(void * const p_ctrl,
                                                                                unsigned char * p_in,
                                                                                unsigned int inlen));
fsp_err_t rm_wifi_get_atcmd_hostinitdone_resp(char * p_out, size_t outlen);
fsp_err_t rm_wifi_get_station_list(WIFIStationInfo_t * pxStationList, uint8_t * pcStationListSize);
fsp_err_t rm_wifi_is_wpa_state(uint8_t state, int max_delay_counter);
fsp_err_t rm_wifi_twt_setup(struct twt_setup_req *req);
fsp_err_t rm_wifi_twt_teardown(struct twt_teardown_req *req);

/**
 * @brief Setup p2p.
 *
 * @param[in] p2pNetworkParams - Structure to hold the Wi-Fi P2P params.
 *
 * @return eWiFiSuccess if managed to set p2p successfully, failure code otherwise.
 */
WIFIReturnCode_t WIFI_SetupP2P(const wifi_p2p_ext *p2pNetworkParams);
/**
 * @brief find P2P Devices for up-to timeout seconds.
 *
 * @return eWiFiSuccess if managed to find P2P devices successfully, failure code otherwise.
 */
WIFIReturnCode_t WIFI_P2PFind(void);
/**
 * @brief connect to a P2P Device.
 *
 * @param[in] p2p_addr - p2p address of other device.
 * @param[in] wps_method - securiry method <"pbc"|PIN>.
 * 
 * @return eWiFiSuccess if managed to connect to a P2P device successfully, failure code otherwise.
 */
WIFIReturnCode_t WIFI_P2PConnect(const char *p2p_addr, const char *wps_method);
/**
 * @brief Remove P2P group.
 *
 * @return eWiFiSuccess if managed to remove the P2P group successfully, failure code otherwise.
 */
WIFIReturnCode_t WIFI_P2PGroupRemove(void);
/**
 * @brief Add a new P2P group.
 *
 * @return eWiFiSuccess if managed to add the P2P group successfully, failure code otherwise.
 */
WIFIReturnCode_t WIFI_P2PGroupAdd(void);
/**
 * @brief List known P2P peers.
 *
 * @return eWiFiSuccess if managed to list peers successfully, failure code otherwise.
 */
WIFIReturnCode_t WIFI_P2PPeers(char * reply);
/**
 * @brief Accept P2P connection request.
 *
 * @return eWiFiSuccess if managed to accept the request successfully, failure code otherwise.
 */
WIFIReturnCode_t WIFI_P2PAccept(void);
/**
 * @brief Set P2P operating channel.
 *
 * @param[in] channel - The operating channel to set.
 *
 * @return eWiFiSuccess if managed to set the operating channel successfully, failure code otherwise.
 */
WIFIReturnCode_t WIFI_P2PSetOperChan(const uint8_t channel);
/**
 * @brief Set P2P listen channel.
 *
 * @param[in] channel - The listen channel to set.
 *
 * @return eWiFiSuccess if managed to set the listen channel successfully, failure code otherwise.
 */
WIFIReturnCode_t WIFI_P2PSetListenChan(const uint8_t channel);
/**
 * @brief Set P2P Group Owner (GO) intent.
 *
 * @param[in] intent - The GO intent value to set.
 *
 * @return eWiFiSuccess if managed to set the GO intent successfully, failure code otherwise.
 */
WIFIReturnCode_t WIFI_P2PSetGoIntent(const uint8_t intent);
/**
 * @brief Set P2P find timeout.
 *
 * @param[in] timeout - the timeout for the find.
 *
 * @return eWiFiSuccess if managed to set the find timeout successfully, failure code otherwise.
 */
WIFIReturnCode_t WIFI_P2PSetFindTimeout(const uint8_t timeout);
/**
 * @brief Set P2P ssid postfix.
 *
 * @param[in] ssid_postfix - The ssid postfix to set.
 *
 * @return eWiFiSuccess if managed to set the ssid postfix successfully, failure code otherwise.
 */
WIFIReturnCode_t WIFI_P2PSetSsidPostfix(const char * ssid_postfix);
/**
 * @brief P2P get paramaters.
 *
 * @param[out] p2pconfig - The current configuration of p2p.
 * 
 * @return eWiFiSuccess if managed to list peers successfully, failure code otherwise.
 */
WIFIReturnCode_t WIFI_P2PGet(char *p2pconfig);
/**
 * @brief Isolate STAs connected to the AP from each other (or cancel isolation).
 *
 * @param[out] enable_isolation - The desired isolation mode (on/off).
 * 
 * @return eWiFiSuccess if managed to isolate successfully, failure code otherwise.
 */
WIFIReturnCode_t WIFI_Isolate(bool enable_isolation);
/**
 * @brief Register the MAC Address of a Station in the ACL
 *
 * @param[in] mac_addr - The MAC address to add to the list
 * 
 * @return eWiFiSuccess if managed to add successfully, failure code otherwise.
 */
WIFIReturnCode_t WIFI_AclMac(const char *mac_addr);
/**
 * @brief Set ACL to allow/deny/clear"
 *
 * @param[in] filter_mode - The mode to set the MAC Filtering
 * 
 * @return eWiFiSuccess if managed to set successfully, failure code otherwise.
 */
WIFIReturnCode_t WIFI_Acl(const char *filter_mode);
/**
 * @brief Set WMM function to enable/disable
 *
 * @param[in] enable_wmm - The mode to set the WMM functionality
 * 
 * @return eWiFiSuccess if managed to set successfully, failure code otherwise.
 */
WIFIReturnCode_t WIFI_WMM(bool enable_wmm);
/**
 * @brief Set WMM-PS function to enable/disable
 *
 * @param[in] enable_wmm_ps - The mode to set the WMM-PS functionality
 * 
 * @return eWiFiSuccess if managed to set successfully, failure code otherwise.
 */
WIFIReturnCode_t WIFI_WMM_PS(bool enable_wmm_ps);
/**
 * @brief Initiate WPS Push Button Configuration (PBC) with a specific AP or any available one
 *
 * @param[in] mac_addr - The BSSID of the target access point, or "any" to any available AP
 *
 * @return eWiFiSuccess if WPS PBC was initiated successfully, or a failure code otherwise.
 */
WIFIReturnCode_t WIFI_WpsPbc(const char *mac_addr);
/**
 * @brief Initiate WPS PIN method with a specific AP or any available one
 *
 * @param[in] mac_addr - The BSSID of the target access point, or "any" to any available AP
 * @param[in] pin - (Optional) An 8-digit WPS PIN code used for authentication.
 *                  Pass NULL to generate and use a random PIN.
 * @param[out] gen_pin - (Optional) Buffer (must be at least 9 bytes long) to store the generated PIN.
 *
 * @return eWiFiSuccess if WPS PIN was initiated successfully, or a failure code otherwise.
 */
WIFIReturnCode_t WIFI_WpsPin(const char *mac_addr, const char *pin, char *gen_pin);
/**
 * @brief Cancel any ongoing WPS operation (PIN or PBC)
 *
 * @return eWiFiSuccess if the WPS session was canceled successfully, or a failure code otherwise.
 */
WIFIReturnCode_t WIFI_WpsCancel(void);
/**
 * @brief Manage the WPS AP PIN state or value
 *
 * @param[in] state - A string indicating the desired operation:
 *                    "disable" – Deactivate AP PIN mode.
 *                    "random" – Generate a new random AP PIN.
 *                                Optionally, pass a timeout value in seconds as arg1.
 *                    "get" – Retrieve the current AP PIN. arg1 and arg2 are ignored.
 *                    "set" – Set a specific AP PIN.
 *                             arg1 must contain the desired 8-digit PIN.
 *                             Optionally, pass a timeout value in seconds as arg2.
 *
 * @param[in] arg1 - (Optional) Depends on the operation:
 *                   - For "random": timeout in seconds (as string).
 *                   - For "set": the 8-digit PIN to assign.
 *
 * @param[in] arg2 - (Optional) Only used with "set": timeout in seconds (as string).
 *
 * @param[out] gen_pin - (Optional) Buffer (must be at least 9 bytes long) to store the generated PIN.
 *
 * @return eWiFiSuccess if the operation completed successfully, or a failure code otherwise.
 */
WIFIReturnCode_t WIFI_WpsApPin(const char *state, const char *arg1, const char *arg2, char *gen_pin);
/**
 * @brief Set wifi ps function to enable/disable
 *
 * @param[in] enable - The mode to set the PS_MODE functionality
 * 
 * @return eWiFiSuccess if managed to set successfully, failure code otherwise.
 */
WIFIReturnCode_t WIFI_SetPsMode(const bool enable);
/**
 * @brief Get wifi ps config status
 * 
 * @param[out] config - The ps mode configuration that currently configured
 *
 * @return eWiFiSuccess if managed to Get successfully, failure code otherwise.
 */
WIFIReturnCode_t WIFI_GetPsMode(bool *config);
/**
 * @brief Set listen interval for wifi power save
 *
 * @param[in] listen_interval - The interval to set in beacon interval units

 * @return eWiFiSuccess if managed to set successfully, failure code otherwise.
 */
 WIFIReturnCode_t WIFI_SetListenInterval(const int listen_interval);
/**
 * @brief Send Target Wake Time (TWT) Setup request.
 *
 * @param req Pointer to the request parameters struct.
 *
 * @return eWiFiSuccess if message was created and sent (eWiFiFailure otherwise).
 */
WIFIReturnCode_t WIFI_TwtSetup(struct twt_setup_req *req);
/**
 * @brief Send Target Wake Time (TWT) Teardown request.
 *
 * @param req Pointer to the request parameters struct.
 *
 * @return eWiFiSuccess if message was created and sent (eWiFiFailure otherwise).
 */
WIFIReturnCode_t WIFI_TwtTeardown(struct twt_teardown_req *req);

/**
 * @brief Register a Wi-Fi Extended event Handler.
 *
 * @param[in] xEventType - Wi-Fi Extended event type.
 * @param[in] xHandler - Wi-Fi Extended event handler.
 *
 * @return eWiFiSuccess if registration is successful, failure code otherwise.
 */
WIFIReturnCode_t WIFI_RegisterEventExt(WIFIEventExtType_t xEventType, WIFIEventExtHandler_t xHandler);

/**
 * @brief Perform a Wi-Fi network Scan.
 *
 * @param[in] pxBuffer - Buffer for scan results.
 * @param[in] ucNumNetworks - Number of networks to retrieve in scan result.
 * @param[in] pxScanConfigExtended - Wi-Fi scan extended configuration
 *
 * @return @ref eWiFiSuccess if the Wi-Fi network scan was successful, failure code otherwise.
 *
 * @note The input buffer will have the results of the scan.
 *
 * **Example**
 * @code
 * const uint8_t ucNumNetworks = 10; //Get 10 scan results
 * WIFIScanResult_t xScanResults[ ucNumNetworks ];
 * WIFIScanExtendedConfig_t pxScanConfigExtended;
 * WIFI_ScanExtended( xScanResults, ucNumNetworks , &pxScanConfigExtended );
 * @endcode
 */
WIFIReturnCode_t WIFI_ScanExtended(WIFIScanResult_t * pxBuffer, uint8_t ucNumNetworks, WIFIScanExtendedConfig_t * pxScanConfigExtended);
WIFIReturnCode_t WIFI_ConnectAPExt(const WIFINetworkParamsExt_t * const pxNetworkParamsExt);
WIFIReturnCode_t WIFI_NetworkParams_check_Ext(const WIFINetworkParamsExt_t * const pxNetworkParamsExt);

WIFIReturnCode_t WIFI_SetModeExt(e_wifi_device_mode_ext_t xDeviceModeExt);
WIFIReturnCode_t WIFI_GetModeExt(e_wifi_device_mode_ext_t *xDeviceModeExt);

/**
 * @brief Get extended Wi-Fi info of the interfaces.
 *
 * This is a synchronous call.
 *
 * @param[out] pxConnectionInfoExt - pointer to array of info for the interfaces.
 * @param[in] iface_num - number of interfaces.
 *
 * @return eWiFiSuccess if connection info was got successfully, failure code otherwise.
 */
WIFIReturnCode_t WIFI_GetConnectionInfoExt(WIFIConnectionInfoExt_t *pxConnectionInfoExt, int iface_num);

void rm_wifi_init(void);
unsigned int rm_wifi_get_fault_pc(void);
void rm_wifi_set_fault_pc(unsigned int pc);
unsigned char rm_wifi_get_fault_count(void);
void rm_wifi_set_fault_count(char cnt);

const char * rm_wifi_otp_sku_id_get_str(void);
uint32_t rm_wifi_otp_sku_id_get(void);
uint32_t rm_wifi_build_sku_id_get(void);

/**
 * @brief Turns on Wi-Fi, and connects to previously configured AP.
 *
 * This function turns on Wi-Fi module, initializes the drivers and connects
 * to previously configured AP using WIFI_AutoConfig.
 * WIFI_On or WIFI_OnAuto must be called before calling any other Wi-Fi API.
 *
 * @return @ref eWiFiSuccess if Wi-Fi module was successfully turned on and associated,
 * failure code otherwise.
 */
WIFIReturnCode_t WIFI_OnAuto(void);

/**
 * @brief Sets the Wi-Fi Access Point (AP) parameters to be used when connecting using WIFI_OnAuto.
 *
 * @param[in] pxNetworkParams Configuration to join AP.
 *
 * @return @ref eWiFiSuccess if successful, failure code otherwise.
 */
WIFIReturnCode_t WIFI_AutoConfig(const WIFINetworkParams_t * const pxNetworkParams);

/**
 * @brief High-level function to retrieve a network interface by index.
 *
 * @param iface_index Index of the desired interface.
 * @return Pointer to the network interface, or NULL if index is invalid.
 */
struct netif *WIFI_GetNetIf(int iface_index);

/**
 * @brief Configure SoftAP.
 *
 * @param[in] pxNetworkParamsExt - Network parameters to configure AP.
 *
 * @return @ref eWiFiSuccess if SoftAP was successfully configured, failure code otherwise.
 *
 * **Example**
 * @code
 * WIFINetworkParamsExt_t xNetworkParamsExt;
 * xNetworkParamsExt.xNetworkParams.ucSSID = "SSID_Name";
 * xNetworkParamsExt.xNetworkParams.xPassword.xWPA.cPassphrase = "PASSWORD";
 * xNetworkParamsExt.xNetworkParams.xSecurity = eWiFiSecurityWPA2;
 * xNetworkParamsExt.xNetworkParams.ucChannel = ChannelNum;
 * xNetworkParamsExt.ucBand = eWiFiBand5G;
 * WIFI_ConfigureAPExt(&xNetworkParamsExt);
 * @endcode
 */
WIFIReturnCode_t WIFI_ConfigureAPExt(WIFINetworkParamsExt_t *pxNetworkParamsExt);

#if configUSE_IDLE_HOOK
void vApplicationIdleHookSub(void);
#endif

struct netif *rm_wifi_get_netif(int iface_index);

/**
 * @brief Initialize CC312 HW Engine and Link mbedTLS's callback function.
 */
void RM_WIFI_mbedtls_setup_psa_crypto(void);


/* Common macro for FSP header files. There is also a corresponding FSP_HEADER macro at the top of this file. */
FSP_FOOTER

#endif // RM_WIFI_H

/*******************************************************************************************************************//**
 * @} (end addtogroup WIFI)
 **********************************************************************************************************************/
