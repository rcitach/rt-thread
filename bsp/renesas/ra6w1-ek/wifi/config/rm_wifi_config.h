/* generated configuration header file - do not edit */
#ifndef RM_WIFI_CONFIG_H_
#define RM_WIFI_CONFIG_H_
#ifdef __cplusplus
            extern "C" {
            #endif

#define dg_configUSE_TRACE_FOR_DEBUG               ( 0 )

#define WIFI_CFG_PARAM_CHECKING_ENABLE   (BSP_CFG_PARAM_CHECKING_ENABLE)
#define WIFI_CFG_WATCHDOG_SERVICE_ENABLE (1)
#define WIFI_CFG_BLE_CLI_ENABLE          (0)

#ifndef OS_FREERTOS
#define OS_FREERTOS
#endif
#define __DISABLE_DPM_ABNORM__
#define dg_configDEVICE DA16400_00
#define TEST_FSP_USE_MODULE_DEFINED_STDIO_RETARGET
#define DIRTY_LWIP  1
#if ((0 == 1) && (1 == (1)))
#ifndef __SUPPORT_MQTT__
#define __SUPPORT_MQTT__
#endif
#else
            #undef __SUPPORT_MQTT__
            #endif

#define SUPPORT_DPM_ABNORMAL_NOTIFICATION_ONLY 0
#ifndef TEST_APP_START
#define TEST_APP_START 0
#endif
#define wificonfigMAX_SOCKETS                   4
#define wificonfigNUM_CONNECTION_RETRY          3
#define wificonfigMAX_NETWORK_PROFILES          0
#define wificonfigMAX_SSID_LEN                  32
#define wificonfigSSID_POSTFIX_MAX_LEN          22
#define wificonfigMAX_SAE_GROUPS_LEN            20
#define wificonfigMAX_ENT_IDENTITY_LEN          32
#define wificonfigMAX_ENT_PASSWORD_LEN          32
#define wificonfigMAX_CERT_LEN                  (1024 * 4)
#define wificonfigMAX_PRIV_KEY_LEN              (1024 * 4)
#define wificonfigMAX_BSSID_LEN                 6
#define wificonfigMAX_WEPKEYS                   (4)
#define wificonfig128BIT_WEPKEY_LEN             (26)
#define wificonfig64BIT_WEPKEY_LEN              (10)
#define wificonfigMAX_WEPKEY_LEN                (wificonfig128BIT_WEPKEY_LEN)
#define wificonfigMAX_PASSPHRASE_LEN            128
#define wificonfigMAX_SEMAPHORE_WAIT_TIME_MS    60000
#define wificonfigMAX_CONNECTED_STATIONS        (4)
#define wificonfigACCESS_POINT_SSID_PREFIX      ("Enter SSID for Soft AP")
#define wificonfigACCESS_POINT_PASSKEY          ("Enter Password for Soft AP")
#define wificonfigACCESS_POINT_CHANNEL          (11)
#define wificonfigACCESS_POINT_SECURITY         (eWiFiSecurityWPA2)
#define wificonfigACCESS_POINT_MIN_INACTIVITY   (30)
#define wificonfigACCESS_POINT_MAX_INACTIVITY   (86400)
#define wificonfigMAX_CHANNEL_LIST              (20)

/* For TCP Client application */
#define TCP_CLIENT_APP_START                     (1)

/* LWIP dependency */
#ifndef __SUPPORT_IPV4__
#define __SUPPORT_IPV4__
#endif
#ifndef __SUPPORT_IPV6__
#define __SUPPORT_IPV6__
#endif
#define RRQ61XX_CUSTOM_FIXES_MANDATORY
#define RM_LWIP_W_CLEANED			(1)
#define HTTPD_ENABLE_HTTPS			(1)
#define ATCMD_IF_SUPPORT (0)
#if (1 == (0))
            #define __OTA_UPDATE_MCU_FW__
            #define __SUPPORT_ATCMD__
            #endif
#undef configCPU_CLOCK_HZ
#define configCPU_CLOCK_HZ                 (160000000)
#ifdef __cplusplus
            }
            #endif
#endif /* RM_WIFI_CONFIG_H_ */
