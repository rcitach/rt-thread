/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include <stdio.h>
#include <string.h>

#include "rm_wifi.h"
#include "fsp_common_api.h"
#include "rm_wifi_dpm.h"

/* Socket and WiFi interface includes. */
#include "rm_wifi_api.h"
#include "rm_wifi_event.h"

/**********************************************************************************************************************
 * Exported global variables
 **********************************************************************************************************************/
extern const wifi_cfg_t g_wifi_cfg;

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/
static WIFIReturnCode_t WIFI_NetworkParams_check(const WIFINetworkParams_t * const pxNetworkParams)
{
    WIFISecurity_t security;
    if ((NULL == pxNetworkParams) || (0 == pxNetworkParams->ucSSIDLength))
    {
        return eWiFiFailure;
    }

    security = pxNetworkParams->xSecurity;
    if (security == eWiFiSecurityNotSupported ||
        (WIFISecurityExt_t) security >= eWiFiSecurityMax_ext)
    {
        return eWiFiFailure;
    }

    if ((0 == pxNetworkParams->xPassword.xWPA.ucLength) &&
        (eWiFiSecurityOpen != security) &&
        (eWiFiSecurityWEP != security) &&
        (eWiFiSecurityWPA_ent_ext != (WIFISecurityExt_t)security) &&
        (eWiFiSecurityWPA2_ent != security) &&
        (eWiFiSecurityWPA_WPA2_ent_ext != (WIFISecurityExt_t)security) &&
        (eWiFiSecurityWPA2_WPA3_ent_ext != (WIFISecurityExt_t)security) &&
        (eWiFiSecurityWPA3_ent_ext != (WIFISecurityExt_t)security) &&
        (eWiFiSecurityWPA3_192B_ent_ext != (WIFISecurityExt_t)security) &&
        (eWiFiSecurityWPA3_OWE_ext != (WIFISecurityExt_t)security))
    {
        return eWiFiFailure;
    }

    if (pxNetworkParams->ucSSIDLength > wificonfigMAX_SSID_LEN)
    {
        return eWiFiFailure;
    }

    if ((pxNetworkParams->xPassword.xWPA.ucLength > wificonfigMAX_PASSPHRASE_LEN) &&
        (eWiFiSecurityWEP != security))
    {
        return eWiFiFailure;
    }

    if (eWiFiSecurityWEP == security)
    {
        if (rm_wifi_wep_network_params_check(pxNetworkParams))
        {
            return eWiFiFailure;
        }
    }

    return eWiFiSuccess;
}

static WIFIReturnCode_t WIFI_ApNetParams_check(const WIFIApNetParams_t * const pxApNetParams)
{
    if ((pxApNetParams->ap_max_inactivity != 0 &&
                (pxApNetParams->ap_max_inactivity < wificonfigACCESS_POINT_MIN_INACTIVITY ||
                 pxApNetParams->ap_max_inactivity > wificonfigACCESS_POINT_MAX_INACTIVITY)) ||
        ((pxApNetParams->ap_max_inactivity % 10) != 0))
    {
        return eWiFiFailure;
    }

    if ((pxApNetParams->ucEncMode < eWiFiEncryptionNone) &&
        (pxApNetParams->ucEncMode > eWiFiEncryptionTKIP_AES))
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_NetworkParams_check_Ext(const WIFINetworkParamsExt_t * const pxNetworkParamsExt)
{
    WIFISecurityExt_t xSecurity = (WIFISecurityExt_t) pxNetworkParamsExt->xNetworkParams.xSecurity;

    if (eWiFiSuccess != WIFI_NetworkParams_check(&pxNetworkParamsExt->xNetworkParams))
    {
        return eWiFiFailure;
    }

    if (eWiFiSuccess != WIFI_ApNetParams_check(&pxNetworkParamsExt->xApNetParams))
    {
        return eWiFiFailure;
    }

    if ((eWiFiSecurityWPA_ent_ext == xSecurity) ||
        (eWiFiSecurityWPA2_ent_ext == xSecurity) ||
        (eWiFiSecurityWPA_WPA2_ent_ext == xSecurity) ||
        (eWiFiSecurityWPA2_WPA3_ent_ext == xSecurity) ||
        (eWiFiSecurityWPA3_ent_ext == xSecurity) ||
        (eWiFiSecurityWPA3_192B_ent_ext == xSecurity))
    {
        if (FSP_SUCCESS != rm_wifi_enterprise_network_params_check(pxNetworkParamsExt))
        {
            return eWiFiFailure;
        }
    }

    if (pxNetworkParamsExt->ucBand >= eWiFiBandMax)
    {
        return eWiFiFailure;
    }

    if (pxNetworkParamsExt->ucWiFi_mode >= WIFI_MODE_MAX)
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

static WIFIReturnCode_t p2p_network_params_check(const wifi_p2p_ext * const p2pNetworkParams)
{
    fsp_err_t ret = FSP_ERR_WIFI_FAILED;
    e_wifi_device_mode_ext_t pxDeviceModeExt;

    ret = rm_wifi_get_mode(&pxDeviceModeExt);

    if (ret != FSP_SUCCESS)
    {
        return eWiFiFailure;
    }

    if (NULL == p2pNetworkParams)
    {
        return eWiFiFailure;
    }

    if (pxDeviceModeExt != WIFI_DEVICE_MODE_EXT_P2P_STATION)
    {
        if (p2pNetworkParams->oper_chan > 48)
        {
            return eWiFiFailure;
        }
    }

    if (pxDeviceModeExt != WIFI_DEVICE_MODE_EXT_P2P_GO)
    {
        if(p2pNetworkParams->listen_chan > 11)
        {
            return eWiFiFailure;
        }
    }

    if (pxDeviceModeExt != WIFI_DEVICE_MODE_EXT_P2P)
    {
        if(p2pNetworkParams->go_intent_chan > 15)
        {
            return eWiFiFailure;
        }
    }

    if(strlen(p2pNetworkParams->ssid_postfix) > wificonfigSSID_POSTFIX_MAX_LEN)
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_On(void)
{
    if (rm_wifi_open(&g_wifi_cfg))
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_OnAuto(void)
{
    if (rm_wifi_open_connect(&g_wifi_cfg))
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_Off(void)
{
    if (rm_wifi_close())
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_ConnectAP(const WIFINetworkParams_t * const pxNetworkParams)
{
    if (eWiFiSuccess != WIFI_NetworkParams_check(pxNetworkParams))
    {
        return eWiFiFailure;
    }

    if (rm_wifi_connect(pxNetworkParams, false, PMF_DEFAULT, NULL, 0, NULL))
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_ConnectAPExt(const WIFINetworkParamsExt_t * const pxNetworkParamsExt)
{
    if (eWiFiSuccess != WIFI_NetworkParams_check_Ext(pxNetworkParamsExt))
    {
        return eWiFiFailure;
    }

    if (rm_wifi_connect_Ext(pxNetworkParamsExt))
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_AutoConfig(const WIFINetworkParams_t * const pxNetworkParams)
{
    if (eWiFiSuccess == WIFI_NetworkParams_check(pxNetworkParams))
    {
        if (FSP_SUCCESS == rm_wifi_auto_config(pxNetworkParams))
        {
            return eWiFiSuccess;
        }
    }

    return eWiFiFailure;
}

WIFIReturnCode_t WIFI_Disconnect(void)
{
    if (rm_wifi_disconnect())
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_Reset(void)
{
    if (WIFI_Off())
    {
        return eWiFiFailure;
    }

    if (WIFI_On())
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_SetMode(WIFIDeviceMode_t xDeviceMode)
{
    fsp_err_t ret = FSP_ERR_WIFI_FAILED;

    if (xDeviceMode != eWiFiModeStation &&
        xDeviceMode != eWiFiModeAP)
        return eWiFiFailure;

    ret = rm_wifi_set_mode(WIFI_Convert_Mode_To_Ext_Mode(xDeviceMode));
    if (ret)
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_GetMode(WIFIDeviceMode_t *pxDeviceMode)
{
    fsp_err_t ret = FSP_ERR_WIFI_FAILED;
    e_wifi_device_mode_ext_t pxDeviceModeExt;

    ret = rm_wifi_get_mode(&pxDeviceModeExt);
    if (ret)
    {
        return eWiFiFailure;
    }

    *pxDeviceMode = WIFI_Convert_Ext_Mode_To_Mode(pxDeviceModeExt);

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_SetupP2P(const wifi_p2p_ext * const p2pNetworkParams)
{
    fsp_err_t ret = FSP_ERR_WIFI_FAILED;

    if (eWiFiSuccess != p2p_network_params_check(p2pNetworkParams))
    {
        return eWiFiFailure;
    }

    ret = rm_wifi_p2p(p2pNetworkParams->oper_chan,
                       p2pNetworkParams->listen_chan,
                       p2pNetworkParams->go_intent_chan,
                       p2pNetworkParams->ssid_postfix);

    if (ret)
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_P2PFind(void)
{
    fsp_err_t ret = FSP_ERR_WIFI_FAILED;

    ret = rm_wifi_p2p_find();

    if (ret)
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_P2PConnect(const char *p2p_addr, const char *wps_method)
{
    fsp_err_t ret = FSP_ERR_WIFI_FAILED;

    ret = rm_wifi_p2p_connect(p2p_addr, wps_method);

    if (ret)
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_P2PGroupRemove(void)
{
    fsp_err_t ret = FSP_ERR_WIFI_FAILED;

    ret = rm_wifi_p2p_group_remove();

    if (ret)
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_P2PGroupAdd(void)
{
    fsp_err_t ret = FSP_ERR_WIFI_FAILED;

    ret = rm_wifi_p2p_group_add();

    if (ret)
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_P2PPeers(char * reply)
{
    fsp_err_t ret = FSP_ERR_WIFI_FAILED;

    ret = rm_wifi_p2p_peers(reply);

    if (ret)
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_P2PAccept(void)
{
    fsp_err_t ret = FSP_ERR_WIFI_FAILED;

    ret = rm_wifi_p2p_accept();

    if (ret)
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_P2PSetOperChan(const unsigned char channel)
{
    fsp_err_t ret = FSP_ERR_WIFI_FAILED;

    ret = rm_wifi_p2p_set_oper_chan(channel);

    if (ret)
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_P2PSetListenChan(const unsigned char channel)
{
    fsp_err_t ret = FSP_ERR_WIFI_FAILED;

    ret = rm_wifi_p2p_set_listen_chan(channel);

    if (ret)
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_P2PSetGoIntent(const unsigned char intent)
{
    fsp_err_t ret = FSP_ERR_WIFI_FAILED;

    ret = rm_wifi_p2p_set_go_intent(intent);

    if (ret)
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_P2PSetFindTimeout(const unsigned char timeout)
{
    fsp_err_t ret = FSP_ERR_WIFI_FAILED;

    ret = rm_wifi_p2p_set_find_timeout(timeout);

    if (ret)
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_P2PSetSsidPostfix(const char * ssid_postfix)
{
    fsp_err_t ret = FSP_ERR_WIFI_FAILED;

    ret = rm_wifi_p2p_set_ssid_postfix(ssid_postfix);

    if (ret)
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_P2PGet(char *p2pconfig)
{
    fsp_err_t ret = FSP_ERR_WIFI_FAILED;

    ret = rm_wifi_p2p_get(p2pconfig);
    
    if (ret)
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_Isolate(bool enable_isolation)
{
    fsp_err_t ret = FSP_ERR_WIFI_FAILED;

    ret = rm_wifi_isolate(enable_isolation);

    if (ret)
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_AclMac(const char *addr)
{
    fsp_err_t ret = FSP_ERR_WIFI_FAILED;

    ret = rm_wifi_acl_mac(addr);
    
    if (ret)
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_Acl(const char *filter_mode)
{
    fsp_err_t ret = FSP_ERR_WIFI_FAILED;

    ret = rm_wifi_acl(filter_mode);
    
    if (ret)
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_WMM(bool enable_wmm)
{
    fsp_err_t ret = FSP_ERR_WIFI_FAILED;

    ret = rm_wifi_wmm(enable_wmm);

    if (ret)
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_WMM_PS(bool enable_wmm_ps)
{
    fsp_err_t ret = FSP_ERR_WIFI_FAILED;

    ret = rm_wifi_wmm_ps(enable_wmm_ps);

    if (ret)
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_SetPsMode(const bool enable)
{
    fsp_err_t ret = FSP_ERR_WIFI_FAILED;

    ret = rm_wifi_ps_mode_set(enable);

    if (ret)
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_GetPsMode(bool *config)
{
    fsp_err_t ret = FSP_ERR_WIFI_FAILED;

    ret =  rm_wifi_ps_mode_get(config);

    if (ret)
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_SetListenInterval(const int listen_interval)
{
    fsp_err_t ret = FSP_ERR_WIFI_FAILED;

    ret = rm_wifi_set_listen_interval(listen_interval);

    if (ret)
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_WpsPbc(const char *mac_addr)
{
    fsp_err_t ret = FSP_ERR_WIFI_FAILED;

    ret = rm_wifi_wps_pbc(mac_addr);

    if (ret)
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_WpsPin(const char *mac_addr, const char *pin, char *gen_pin)
{
    fsp_err_t ret = FSP_ERR_WIFI_FAILED;

    ret = rm_wifi_wps_pin(mac_addr, pin, gen_pin);

    if (ret)
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_WpsCancel(void)
{
    fsp_err_t ret = FSP_ERR_WIFI_FAILED;

    ret = rm_wifi_wps_cancel();
    
    if (ret)
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_WpsApPin(const char *state, const char *arg1, const char *arg2, char *gen_pin)
{
    fsp_err_t ret = FSP_ERR_WIFI_FAILED;

    ret = rm_wifi_wps_ap_pin(state, arg1, arg2, gen_pin);
    
    if (ret)
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;  
}

WIFIReturnCode_t WIFI_TwtSetup(struct twt_setup_req *req)
{
    fsp_err_t ret = FSP_ERR_WIFI_FAILED;

    ret = rm_wifi_twt_setup(req);

    if (ret)
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_TwtTeardown(struct twt_teardown_req *req)
{
    fsp_err_t ret = FSP_ERR_WIFI_FAILED;

    ret = rm_wifi_twt_teardown(req);

    if (ret)
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_NetworkAdd(const WIFINetworkProfile_t * const pxNetworkProfile, uint16_t *pusIndex)
{
    if ((NULL == pxNetworkProfile) || (0 == pxNetworkProfile->ucSSIDLength))
    {
        return eWiFiFailure;
    }

    if (pxNetworkProfile->xSecurity >= eWiFiSecurityNotSupported)
    {
        return eWiFiFailure;
    }

    if ((pxNetworkProfile->xSecurity != eWiFiSecurityOpen) && (pxNetworkProfile->ucPasswordLength == 0))
    {
        return eWiFiFailure;
    }

    if (rm_wifi_network_add(pxNetworkProfile, *pusIndex))
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_NetworkGet(WIFINetworkProfile_t *pxNetworkProfile, uint16_t usIndex)
{
    FSP_PARAMETER_NOT_USED(pxNetworkProfile);
    FSP_PARAMETER_NOT_USED(usIndex);

    return eWiFiNotSupported;
}

WIFIReturnCode_t WIFI_NetworkDelete(uint16_t usIndex)
{
    if (rm_wifi_network_del(usIndex))
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_Ping(uint8_t *pucIPAddr, uint16_t usCount, uint32_t ulIntervalMS)
{
    (void) pucIPAddr;
    (void) usCount;
    (void) ulIntervalMS;

    return eWiFiNotSupported;
}

WIFIReturnCode_t WIFI_GetIPInfo(WIFIIPConfiguration_t *pxIPInfo)
{
    (void) pxIPInfo;

    return eWiFiNotSupported;
}

WIFIReturnCode_t WIFI_GetMAC(uint8_t *pucMac)
{
    if (rm_wifi_mac_addr_get(pucMac))
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_GetHostIP(char *pcHost, uint8_t *pucIPAddr)
{
    (void) pcHost;
    (void) pucIPAddr;

    return eWiFiNotSupported;
}

WIFIReturnCode_t WIFI_Scan(WIFIScanResult_t *pxBuffer, uint8_t ucNumNetworks)
{
    if (rm_wifi_scan(pxBuffer, ucNumNetworks))
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_ScanExtended(WIFIScanResult_t * pxBuffer, uint8_t ucNumNetworks, WIFIScanExtendedConfig_t * pxScanConfigExtended)
{
    fsp_err_t ret = FSP_ERR_WIFI_FAILED;

    ret = rm_wifi_scan_extended(pxBuffer, ucNumNetworks, pxScanConfigExtended);

    if (ret)
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_StartAP(void)
{
    if (rm_wifi_start_ap())
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_StopAP(void)
{
    if (rm_wifi_stop_ap())
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_ConfigureAP(const WIFINetworkParams_t * const pxNetworkParams)
{
    WIFINetworkParamsExt_t xNetworkParamsExt = {0};

    if (eWiFiSuccess != WIFI_NetworkParams_check(pxNetworkParams))
    {
        return eWiFiFailure;
    }

    xNetworkParamsExt.xNetworkParams = *pxNetworkParams;

    if (rm_wifi_configure_ap(&xNetworkParamsExt))
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_SetPMMode(WIFIPMMode_t xPMModeType, const void * pvOptionValue)
{
    FSP_PARAMETER_NOT_USED(xPMModeType);
    FSP_PARAMETER_NOT_USED(pvOptionValue);

    return eWiFiNotSupported;
}

WIFIReturnCode_t WIFI_GetPMMode(WIFIPMMode_t * pxPMModeType, void * pvOptionValue)
{
    FSP_PARAMETER_NOT_USED(pxPMModeType);
    FSP_PARAMETER_NOT_USED(pvOptionValue);

    return eWiFiNotSupported;
}

WIFIReturnCode_t WIFI_RegisterEvent(WIFIEventType_t xEventType, WIFIEventHandler_t xHandler)
{
    if (rm_wifi_event_register(xEventType, (rm_wifi_event_handler_t)xHandler))
    {
        return eWiFiNotSupported;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_IsConnected(const WIFINetworkParams_t * pxNetworkParams)
{
    if (eWiFiSuccess != WIFI_NetworkParams_check(pxNetworkParams))
    {
        return eWiFiFailure;
    }

    if (rm_wifi_is_connected(pxNetworkParams))
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_StartScan(WIFIScanConfig_t *pxScanConfig)
{
    FSP_PARAMETER_NOT_USED(pxScanConfig);

    return eWiFiNotSupported;
}

WIFIReturnCode_t WIFI_GetScanResults(const WIFIScanResult_t **pxBuffer, uint16_t *ucNumNetworks)
{
    FSP_PARAMETER_NOT_USED(pxBuffer);
    FSP_PARAMETER_NOT_USED(ucNumNetworks);

    return eWiFiNotSupported;
}

WIFIReturnCode_t WIFI_StartConnectAP(const WIFINetworkParams_t * pxNetworkParams)
{
    FSP_PARAMETER_NOT_USED(pxNetworkParams);

    return eWiFiNotSupported;
}

WIFIReturnCode_t WIFI_StartDisconnect(void)
{
    return eWiFiNotSupported;
}

WIFIReturnCode_t WIFI_GetConnectionInfo(WIFIConnectionInfo_t *pxConnectionInfo)
{
    if (rm_wifi_get_connection_info(pxConnectionInfo, 1))
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_GetConnectionInfoExt(WIFIConnectionInfoExt_t *pxConnectionInfoExt, int iface_num)
{
    if (rm_wifi_get_connection_info_ext(pxConnectionInfoExt, iface_num))
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_GetRSSI(int8_t *pcRSSI)
{
    if (rm_wifi_get_rssi(pcRSSI))
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_GetStationList(WIFIStationInfo_t * pxStationList, uint8_t * pcStationListSize)
{
    e_wifi_device_mode_ext_t mode;

    if (FSP_SUCCESS != rm_wifi_get_mode(&mode))
    {
        printf("get_mode failed\n");
        return eWiFiFailure;
    }

    if (WIFI_DEVICE_MODE_EXT_STATION == mode)
    {
        /* mode is STATION - exit */
        printf("STATION mode:%d, exiting ..\n", mode);
        return eWiFiFailure;
    }

    if (rm_wifi_get_station_list(pxStationList, pcStationListSize))
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_StartDisconnectStation(uint8_t *pucMac)
{
    fsp_err_t ret = FSP_ERR_WIFI_FAILED;

    if (pucMac == NULL)
    {
        return eWiFiFailure;
    }

    ret = rm_wifi_start_disconnect_station(pucMac);
    
    if (ret)
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_SetSpoofingMAC(uint8_t *pucMac)
{
    if (rm_wifi_mac_addr_set(pucMac))
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_SetCountryCode(const char *pcCountryCode)
{
    if (rm_wifi_set_country_code(pcCountryCode))
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_GetCountryCode(char *pcCountryCode)
{
    if (rm_wifi_get_country_code(pcCountryCode))
    {
        return eWiFiFailure;
    }

    return eWiFiSuccess;
}

WIFIReturnCode_t WIFI_GetStatistic(WIFIStatisticInfo_t *pxStats)
{
    FSP_PARAMETER_NOT_USED(pxStats);

    return eWiFiNotSupported;
}

WIFIReturnCode_t WIFI_GetCapability(WIFICapabilityInfo_t *pxCaps)
{
    FSP_PARAMETER_NOT_USED(pxCaps);

    return eWiFiNotSupported;
}

struct netif *WIFI_GetNetIf(int iface_index)
{
    return rm_wifi_get_netif(iface_index);
}
