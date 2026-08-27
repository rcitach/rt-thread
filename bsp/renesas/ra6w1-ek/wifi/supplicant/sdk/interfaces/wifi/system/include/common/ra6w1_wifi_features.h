/**
 ****************************************************************************************
 *
 * @file ra6w1_wifi_features.h
 *
 * @brief Define for common system features
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

#if !defined ( __RRQ61X_WIFI_FEATURES_H__ )
#define __RRQ61X_WIFI_FEATURES_H__

///
/// Wi-Fi features for RRQ61000 ///
///


// For Wi-Fi feature ...

//
#if defined (__SUPPORT_MESH_PORTAL__) || defined (__SUPPORT_MESH_POINT_ONLY__)
    // STA, Soft-AP, Wi-Fi Direct, Concurrent, MESH
    #undef  __TINY_MEM_MODEL__
    #undef  __LIGHT_SUPPLICANT__

#else

  //
  // Enable Wi-Fi concurrent mode in Wi-Fi core
  //
  #define __SUPPORT_WIFI_CONCURRENT_CORE__

  #if defined ( __SUPPORT_WIFI_CONCURRENT_CORE__ )
    // STA, Soft-AP, Wi-Fi Direct (Option), Concurrent-mode
    #undef  __TINY_MEM_MODEL__
    #undef  __LIGHT_SUPPLICANT__

    #define __SUPPORT_P2P__                  // It must be sync with CONFIG_P2P in supp_def.
    #define __SUPPORT_CONCURRENT__           // It must be sync with CONFIG_CONCURRENT in supp_def.h

  #else
      // Only STA, Soft-AP
      #define __TINY_MEM_MODEL__
      #define __LIGHT_SUPPLICANT__
      #undef  __SUPPORT_P2P__                // It must be sync with CONFIG_P2P in supp_def.
  #endif //  __SUPPORT_WIFI_CONCURRENT_CORE__

#endif // !__SUPPORT_MESH_PORTAL__ && !__SUPPORT_MESH_POINT_ONLY__


// Features for Wi-Fi configuration

/* WIFI config based on SKU build flag --- begin */
#if TIN_SKU_BUILD_ID == TIN_SKU_WIFI4_B24
// both 5GHz band and 11ax wifi mode undefined
#elif TIN_SKU_BUILD_ID == TIN_SKU_WIFI6_B24
#define  __SUPPORT_11AX__                      // support 802.11AX
#elif (TIN_SKU_BUILD_ID == TIN_SKU_WIFI6_B24_5 || TIN_SKU_BUILD_ID == TIN_SKU_WIFI6_B24_5_BLE)
#define  __SUPPORT_SETBAND_5GHZ__              // support 5GHz in setband
#define  __SUPPORT_11AX__                      // support 802.11AX
#endif
/* WIFI config based on SKU build flag --- end */

#define __SCAN_ON_AP_MODE__                   // Support SCAN on AP mode

#if defined ( __SUPPORT_WPA3_PERSONAL__ )
  #define __SUPPORT_WPA3_PERSONAL_CORE__      // WPA3-Personal SAE, OWE
#else
  #undef  __SUPPORT_WPA3_PERSONAL_CORE__
#endif // __SUPPORT_WPA3_PERSONAL__

#if defined ( __SUPPORT_WPA_ENTERPRISE__ )
  #define __SUPPORT_WPA_ENTERPRISE_CORE__     // WPA Enterprise
#else
  #undef   __SUPPORT_WPA_ENTERPRISE_CORE__
#endif // __SUPPORT_WPA_ENTERPRISE__

#undef  __SUPPORT_AP_PS_MODE                  // Support AP Power-Save mode
#undef  __SUPPORT_LEGACY_WIFI_PS__            // Legacy Wi-Fi power-save mode
#undef  __SUPPORT_WIFI_CONN_NOTIFY_CB__       // Wi-Fi Connection User Callback
#undef  __SUPPORT_BSSID_SCAN__                // Scan with BSSID 
#undef  __SUPPORT_DIR_SSID_SCAN__             // Scan with User SSID directly
#undef  __DISABLE_NO_SUTIABLE_NETWORK_MSG__   // Disable
                                              // "No suitable network found" console message

// For NAT feature : Not supported on lwIP of the RRQ61x0
#undef  __SUPPORT_MULTI_IP_IF__


// ..............................................................................
//
// Sub-features for upper defined feature
//
#if defined ( __SUPPORT_WPA_ENTERPRISE_CORE__ )
  #define __SUPPORT_WPA3_ENTERPRISE_CORE__         // WPA3-Enaterpise
  #ifdef __SUPPORT_WPA3_ENTERPRISE_CORE__
    #define __SUPPORT_WPA3_ENTERPRISE_192B_CORE__  // UMAC, LMAC not supported yet
  #endif // __SUPPORT_WPA3_ENTERPRISE_CORE__
#else
    #undef  __SUPPORT_WPA3_ENTERPRISE_CORE__
#endif /* __SUPPORT_WPA_ENTERPRISE_CORE__ */

// Defined in custom_config_sdk.h
#if defined __SUPPORT_FAST_CONN_SLEEP_2__
  #define __SUPPORT_ASSOC_CHANNEL__
#endif // __SUPPORT_FAST_CONN_SLEEP_2__

#if defined (__SUPPORT_SETBAND_5GHZ__)
  #define WPA_SETBAND_DEF    0                     // WPA_SETBAND_AUTO
#else
  #define WPA_SETBAND_DEF    2                     // WPA_SETBAND_2GH
#endif // __SUPPORT_SETBAND_5GHZ__


// ..............................................................................
//
// Start for MESH configuration ////////////////////////////////////////////////////////
//
#if defined ( __SUPPORT_MESH_PORTAL__ ) || defined ( __SUPPORT_MESH_POINT_ONLY__ )
  #define __SUPPORT_WPA3_PERSONAL_CORE__

  #if defined __SUPPORT_WPA3_PERSONAL_CORE__
    #define __SUPPORT_IEEE80211W__                 // PMF for WPA3-Personal
    #if !defined __SUPPORT_WPA3_SAE__              // for MESH
      #define __SUPPORT_WPA3_SAE__
    #endif // __SUPPORT_WPA3_SAE__

    #undef  __SUPPORT_WPA3_OWE__

    #if defined __SUPPORT_WPA3_OWE__
      #undef __SUPPORT_OWE_TRANS__
    #endif // __SUPPORT_WPA3_OWE__
  #endif // __SUPPORT_WPA3_PERSONAL_CORE__

  #undef  __SUPPORT_SOFTAP_DFLT_GW__
#else
  #define __SUPPORT_IEEE80211W__                   // PMF for WPA2-Personal

  #if defined __SUPPORT_WPA3_PERSONAL_CORE__
    #define __SUPPORT_WPA3_SAE__
    #define __SUPPORT_WPA3_OWE__

    #if defined __SUPPORT_WPA3_SAE__ || defined __SUPPORT_WPA3_OWE__
      #if !defined __SUPPORT_IEEE80211W__
        #define __SUPPORT_IEEE80211W__             // PMF for WPA3-Personal
      #endif // __SUPPORT_IEEE80211W__
    #endif // __SUPPORT_WPA3_SAE__ || __SUPPORT_WPA3_OWE__

    #if defined __SUPPORT_WPA3_OWE__
      #undef  __SUPPORT_OWE_TRANS__
    #endif // __SUPPORT_WPA3_OWE__
  #endif // __SUPPORT_WPA3_PERSONAL_CORE__    

  #if defined __SUPPORT_WPA3_ENTERPRISE_CORE__
    #if !defined __SUPPORT_IEEE80211W__
      #define __SUPPORT_IEEE80211W__               // PMF for WPA3-Enterprise
    #endif // __SUPPORT_IEEE80211W__
  #endif // __SUPPORT_WPA3_ENTERPRISE_CORE__

  #define __APPLE_DEVICE_PATCH__
  #define __SUPPORT_SOFTAP_DFLT_GW__
#endif    /* __SUPPORT_MESH_PORTAL__ || __SUPPORT_MESH_POINT_ONLY__ */
//
// End for MESH configuration ////////////////////////////////////////////////////////
//

#endif  // __RRQ61X_WIFI_FEATURES_H__

/* EOF */
