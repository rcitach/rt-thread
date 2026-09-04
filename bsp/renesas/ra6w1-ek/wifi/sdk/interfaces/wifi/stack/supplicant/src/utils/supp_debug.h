/**
 ****************************************************************************************
 *
 * @file supp_debug.h
 *
 * @brief supplicant features for RA6W1 supplicant
 *
 * Copyright (c) 2023 Renesas Electronics. All rights reserved.
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

#include "supp_def.h"
#include "common_def.h"

#define NO_PRINT        do { } while (0);
#define ra6w1_prt       printf
/* Debugging function - conditional printf and hex dump. Driver wrappers can
 * use these for debugging purposes. */
#ifndef __MSG_DEBUG__
#define __MSG_DEBUG__
enum {
    MSG_EXCESSIVE, MSG_MSGDUMP, MSG_DEBUG, MSG_USER_DEBUG, MSG_INFO, MSG_WARNING, MSG_ERROR
};

extern int wpa_debug_level;
extern int wpa_debug_show_keys;
extern int wpa_debug_timestamp;
#endif /* __MSG_DEBUG__ */


#ifdef ENABLE_BUF_DUMP_DBG
  void ra6wx_dump(const char *title, const u8 *buf, size_t len);
  void ra6wx_ascii_dump(const char *title, const void *buf, size_t len);
  void ra6wx_buf_dump(const char *title, const struct wpabuf *buf);
#else
  #define ra6wx_dump(x, y,z)          NO_PRINT
  #define ra6wx_ascii_dump(x, y,z)    NO_PRINT
  #define ra6wx_buf_dump(x, y)        NO_PRINT  
#endif    /* ENABLE_BUF_DUMP_DBG */

#ifdef CONFIG_LOG_MASK
void ra6wx_notice_prt(const char *fmt,...);
void ra6wx_warn_prt(const char *fmt,...);
void ra6wx_err_prt(const char *fmt,...);
void ra6wx_fatal_prt(const char *fmt,...);
void ra6wx_debug_prt(const char *fmt,...);
void ra6wx_info_prt(const char *fmt,...);

  #ifdef ENABLE_EVENT_DBG
    #define ra6wx_ev_prt             ra6w1_prt
  #else
    #define ra6wx_ev_prt(...)        NO_PRINT
  #endif /* ENABLE_EVENT_DBG */

  #ifdef ENABLE_SCAN_DBG
    #define ra6wx_scan_prt           ra6w1_prt
  #else
    #define ra6wx_scan_prt(...)      NO_PRINT
  #endif /* ENABLE_SCAN_DBG */

  #ifdef ENABLE_ASSOC_DBG
    #define ra6wx_assoc_prt          ra6w1_prt
  #else
    #define ra6wx_assoc_prt(...)     NO_PRINT
  #endif /* ENABLE_ASSOC_DBG */

  #ifdef ENABLE_P2P_DBG
    #define ra6wx_p2p_prt            ra6w1_prt
  #else
    #define ra6wx_p2p_prt(...)       NO_PRINT
  #endif /* ENABLE_P2P_DBG */

#else /* CONFIG_LOG_MASK */

  #ifdef ENABLE_NOTICE_DBG
    #define ra6wx_notice_prt          ra6w1_prt
  #else
      #define ra6wx_notice_prt(...)   NO_PRINT
  #endif /* ENABLE_NOTICE_DBG */

  #ifdef ENABLE_WARN_DBG
    #define ra6wx_warn_prt            ra6w1_prt
  #else
    #define ra6wx_warn_prt(...)       NO_PRINT
  #endif /* ENABLE_WARN_DBG */

  #ifdef ENABLE_ERROR_DBG
    #define ra6wx_err_prt             ra6w1_prt
  #else
    #define ra6wx_err_prt(...)        NO_PRINT
  #endif /* ENABLE_ERROR_DBG */

  #ifdef ENABLE_FATAL_DBG
    #define ra6wx_fatal_prt           ra6w1_prt
  #else
    #define ra6wx_fatal_prt(...)      NO_PRINT
  #endif /* ENABLE_FATAL_DBG */

  #ifdef ENABLE_DEBUG_DBG
    #define ra6wx_debug_prt           ra6w1_prt
  #else
    #define ra6wx_debug_prt(...)      NO_PRINT
  #endif /* ENABLE_FATAL_DBG */

  #ifdef ENABLE_EVENT_DBG
    #define ra6wx_ev_prt              ra6w1_prt
  #else
      #define ra6wx_ev_prt(...)       NO_PRINT
  #endif /* ENABLE_EVENT_DBG */

  #ifdef ENABLE_SCAN_DBG
    #define ra6wx_scan_prt            ra6w1_prt
  #else
    #define ra6wx_scan_prt(...)       NO_PRINT
  #endif /* ENABLE_SCAN_DBG */

  #ifdef ENABLE_ASSOC_DBG
    #define ra6wx_assoc_prt           ra6w1_prt
  #else
    #define ra6wx_assoc_prt(...)      NO_PRINT
  #endif /* ENABLE_ASSOC_DBG */

  #ifdef ENABLE_P2P_DBG
    #define ra6wx_p2p_prt             ra6w1_prt
  #else
    #define ra6wx_p2p_prt(...)        NO_PRINT
  #endif /* ENABLE_P2P_DBG */

#endif /* CONFIG_LOG_MASK */

/* Tx function */
#ifdef    TX_FUNC_INDICATE_DBG
  #define TX_FUNC_START(x)            ra6w1_prt(ANSI_BCOLOR_MAGENTA "[%s] START" ANSI_BCOLOR_DEFULT "\n", __func__);
  #define TX_FUNC_END(x)              ra6w1_prt(ANSI_BCOLOR_MAGENTA "[%s] END" ANSI_BCOLOR_DEFULT "\n", __func__);
  #define TX_FUNC_PRE_END(x)          ra6w1_prt(ANSI_BCOLOR_MAGENTA "[%s] PRE_END" ANSI_BCOLOR_DEFULT "\n", __func__);
#else
  #define TX_FUNC_START(x)            NO_PRINT
  #define TX_FUNC_END(x)              NO_PRINT
  #define TX_FUNC_PRE_END(x)          NO_PRINT
#endif    /* TX_FUNC_INDICATE_DBG */

/* Rx function */
#ifdef    RX_FUNC_INDICATE_DBG
  #define RX_FUNC_START(x)            ra6w1_prt(ANSI_BCOLOR_CYAN "<%s> START" ANSI_BCOLOR_DEFULT "\n", __func__);
  #define RX_FUNC_END(x)              ra6w1_prt(ANSI_BCOLOR_CYAN "<%s> END" ANSI_BCOLOR_DEFULT "\n", __func__);
  #define RX_FUNC_PRE_END(x)          ra6w1_prt(ANSI_BCOLOR_CYAN "<%s> PRE_END" ANSI_BCOLOR_DEFULT "\n", __func__);
#else
  #define RX_FUNC_START(x)            NO_PRINT
  #define RX_FUNC_END(x)              NO_PRINT
  #define RX_FUNC_PRE_END(x)          NO_PRINT
#endif    /* RX_FUNC_INDICATE_DBG */

#ifdef    ENABLE_STATE_CHG_DBG
  #define ra6wx_state_chg_prt         ra6w1_prt
#else
  #define ra6wx_state_chg_prt(...)    NO_PRINT
#endif    /* ENABLE_STATE_CHG_DBG */

#ifdef    ENABLE_SM_ENTRY_DBG
  #define ra6wx_sm_entry_prt          ra6w1_prt
#else
  #define ra6wx_sm_entry_prt(...)     NO_PRINT
#endif    /* ENABLE_SM_ENTRY_DBG */

#ifdef    ENABLE_ELOOP_DBG
  #define ra6wx_eloop_prt             ra6w1_prt
#else
  #define ra6wx_eloop_prt(...)        NO_PRINT
#endif    /* ENABLE_ELOOP_DBG */

#ifdef    ENABLE_DRV_DBG
  #define ra6wx_drv_prt               ra6w1_prt
#else
  #define ra6wx_drv_prt(...)          NO_PRINT
#endif    /* ENABLE_DRV_DBG */

#ifdef    ENABLE_AP_DBG
  #define ra6wx_ap_prt                ra6w1_prt
#else
  #define ra6wx_ap_prt(...)           NO_PRINT
#endif

#ifdef    ENABLE_EAPOL_DBG
  #define ra6wx_eapol_prt             ra6w1_prt
#else
  #define ra6wx_eapol_prt(...)        NO_PRINT
#endif    /* ENABLE_EAPOL_DBG */

#ifdef    ENABLE_IFACE_DBG
  #define ra6wx_iface_prt             ra6w1_prt
#else
  #define ra6wx_iface_prt(...)        NO_PRINT
#endif    /* ENABLE_IFACE_DBG */

#ifdef    ENABLE_WPA_DBG
  #define ra6wx_wpa_prt               ra6w1_prt
#else
  #define ra6wx_wpa_prt(...)          NO_PRINT
#endif    /* ENABLE_WPA_DBG */

#ifdef    ENABLE_WPS_DBG
  #define ra6wx_wps_prt               ra6w1_prt
#else
  #define ra6wx_wps_prt(...)          NO_PRINT
#endif    /* ENABLE_WPS_DBG */

#ifdef    ENABLE_WPS_VDBG
  #define ra6wx_wps_vprt              ra6w1_prt
#else
  #define ra6wx_wps_vprt(...)         NO_PRINT
#endif    /* ENABLE_WPS_DBG */

#ifdef    ENABLE_EAP_DBG
  #define ra6wx_eap_prt               ra6w1_prt
#else
  #define ra6wx_eap_prt(...)          NO_PRINT
#endif    /* ENABLE_EAP_DBG */

#ifdef    ENABLE_AP_DBG
  #define ra6wx_ap_prt                ra6w1_prt
#else
  #define ra6wx_ap_prt(...)           NO_PRINT
#endif    /* ENABLE_AP_DBG */

#ifdef    ENABLE_AP_MGMT_DBG
  #define ra6wx_ap_mgmt_prt           ra6w1_prt
#else
  #define ra6wx_ap_mgmt_prt(...)      NO_PRINT
#endif    /* ENABLE_AP_MGMT_DBG */

#ifdef    ENABLE_AP_WMM_DBG
  #define ra6wx_ap_wmm_prt            ra6w1_prt
#else
  #define ra6wx_ap_wmm_prt(...)       NO_PRINT
#endif    /* ENABLE_AP_WMM_DBG */

#ifdef    ENABLE_NVRAM_DBG
  #define ra6w1_nvram_prt            ra6w1_prt
#else
  #define ra6w1_nvram_prt(...)       NO_PRINT
#endif    /* ENABLE_NVRAM_DBG */

#ifdef    ENABLE_CLI_DBG
  #define ra6w1_cli_prt              ra6w1_prt
#else
  #define ra6w1_cli_prt(...)         NO_PRINT
#endif    /* ENABLE_CLI_DBG */
  
#ifdef    ENABLE_80211N_DBG
  #define ra6wx_11n_prt               ra6w1_prt
#else
  #define ra6wx_11n_prt(...)          NO_PRINT
#endif    /* ENABLE_80211N_DBG */

#ifdef    ENABLE_WNM_DBG
  #define ra6wx_wnm_prt               ra6w1_prt
#else
  #define ra6wx_wnm_prt(...)          NO_PRINT
#endif    /* ENABLE_WNM_DBG */

#ifdef    ENABLE_DPM_DBG
  #define ra6wx_dpm_prt               ra6w1_prt
#else
  #define ra6wx_dpm_prt(...)          NO_PRINT
#endif    /* ENABLE_DPM_DBG */

#ifdef    ENABLE_MEM_DBG
  #define ra6wx_mem_prt               ra6w1_prt
#else
  #define ra6wx_mem_prt(...)          NO_PRINT
#endif    /* ENABLE_DPM_DBG */

#ifdef ENABLE_CRYPTO_DBG
  #define ra6wx_crypto_prt            ra6w1_prt
#else
  #define ra6wx_crypto_prt(...)       NO_PRINT
#endif /* ENABLE_CRYPTO_DBG */


#define CONFIG_WPA_PRINTF
#define CONFIG_WPA_PRINTF_DBG        /* wpa_printf(MSG_DEBUG, ...) */
#define CONFIG_WPA_MSG
#define CONFIG_WPA_DBG               /* wpa_dbg ==> wpa_msg */

  #ifdef CONFIG_WPA_PRINTF
    #define CONFIG_WPA_MSG_CTRL
    #define CONFIG_WPA_MSG_GLOBAL
    #define CONFIG_WPA_MSG_GLOBAL_CTRL
    #define CONFIG_WPA_MSG_NO_GLOBAL
    #define CONFIG_WPA_MSG_GLOBAL_ONLY
  #endif /* CONFIG_WPA_PRINTF */
#undef    CONFIG_WPA_ASSERT

#define ENABLE_HOSTAPD_LOGGER_DBG
#define ENABLE_HEXDUMP_DBG

#ifdef ENABLE_HEXDUMP_DBG
    #define ENABLE_HEXDUMP_BUF_DBG

    #define ENABLE_HEXDUMP_ASCII_DBG
        #ifdef ENABLE_HEXDUMP_ASCII_DBG
            #define ENABLE_HEXDUMP_ASCII_KEY_DBG
        #endif /* ENABLE_HEXDUMP_ASCII_DBG */

    #define ENABLE_HEXDUMP_KEY_DBG
        #ifdef ENABLE_HEXDUMP_KEY_DBG
            #define ENABLE_HEXDUMP_BUF_KEY_DBG
        #endif /* ENABLE_HEXDUMP_KEY_DBG */
#endif /* ENABLE_HEXDUMP_DBG */

//=============================================================

#ifdef CONFIG_WPA_DBG
/*
 * wpa_dbg() behaves like wpa_msg(), but it can be removed from build to reduce
 * binary size. As such, it should be used with debugging messages that are not
 * needed in the control interface while wpa_msg() has to be used for anything
 * that needs to shown to control interface monitors.
 */
#define wpa_dbg(args...)    wpa_msg(args)
#else
#define wpa_dbg(args...)    NO_PRINT
#endif /* CONFIG_WPA_DBG */

#ifdef CONFIG_WPA_PRINTF
void wpa_printf(int level, const char *fmt, ...);
//#define wpa_printf(args...)    _wpa_printf(args...)

#else
#define wpa_printf(...)    NO_PRINT
#endif /* CONFIG_WPA_PRINTF */

#ifdef CONFIG_WPA_PRINTF_DBG
#define wpa_printf_dbg(args...) wpa_printf(args) /* wpa_printf(MSG_DEBUG, ...) */
#else
#define wpa_printf_dbg(args...) NO_PRINT
#endif /* CONFIG_WPA_PRINTF_DBG */


#define HOSTAPD_MODULE_IEEE80211    0x00000001
#define HOSTAPD_MODULE_IEEE8021X    0x00000002
#define HOSTAPD_MODULE_RADIUS        0x00000004
#define HOSTAPD_MODULE_WPA            0x00000008
#define HOSTAPD_MODULE_DRIVER        0x00000010
#define HOSTAPD_MODULE_IAPP            0x00000020
#define HOSTAPD_MODULE_MLME            0x00000040


#ifdef ENABLE_HOSTAPD_LOGGER_DBG
void hostapd_logger(void *ctx, const u8 *addr, unsigned int module, int level,
              const char *fmt, ...) PRINTF_FORMAT(5, 6);
#else
#define hostapd_logger(...)    NO_PRINT
#endif /* ENABLE_HOSTAPD_LOGGER_DBG */

//=============================================================

#ifdef ENABLE_HEXDUMP_KEY_DBG
void wpa_hexdump_key(int level, const char *title, const void *buf, size_t len);
#else
#define wpa_hexdump_key(l,t,b,le)    NO_PRINT
#endif /* ENABLE_HEXDUMP_KEY_DBG */

#ifdef ENABLE_HEXDUMP_ASCII_KEY_DBG
void wpa_hexdump_ascii_key(int level, const char *title, const void *buf, size_t len);
#else
#define wpa_hexdump_ascii_key(l,t,b,le)    NO_PRINT
#endif /* ENABLE_HEXDUMP_ASCII_KEY_DBG */

#ifdef ENABLE_HEXDUMP_BUF_KEY_DBG
void wpa_hexdump_buf_key(int level, const char *title, const struct wpabuf *buf);
#else
#define wpa_hexdump_buf_key(l,t,b)    NO_PRINT
#endif /* ENABLE_HEXDUMP_BUF_KEY_DBG */

#ifdef ENABLE_HEXDUMP_ASCII_DBG
void wpa_hexdump_ascii(int level, const char *title, const void *buf, size_t len);
#else
#define wpa_hexdump_ascii(l,t,b,le)    NO_PRINT
#endif /* ENABLE_HEXDUMP_ASCII_DBG */

#ifdef ENABLE_HEXDUMP_DBG
void wpa_hexdump(int level, const char *title, const void *buf, size_t len);
#else
#define wpa_hexdump(l,t,b,le)    NO_PRINT
#endif /* ENABLE_HEXDUMP_DBG */

#ifdef ENABLE_HEXDUMP_BUF_DBG
void wpa_hexdump_buf(int level, const char *title, const struct wpabuf *buf);
#else
#define wpa_hexdump_buf(l,t,b)    NO_PRINT
#endif /* ENABLE_HEXDUMP_BUF_DBG */

#ifdef CONFIG_WPA_MSG
/**
 * wpa_msg - Conditional printf for default target and ctrl_iface monitors
 * @ctx: Pointer to context data; this is the ctx variable registered
 *    with struct wpa_driver_ops::init()
 * @level: priority level (MSG_*) of the message
 * @fmt: printf format string, followed by optional arguments
 *
 * This function is used to print conditional debugging and error messages. The
 * output may be directed to stdout, stderr, and/or syslog based on
 * configuration. This function is like wpa_printf(), but it also sends the
 * same message to all attached ctrl_iface monitors.
 *
 * Note: New line '\n' is added to the end of the text when printing to stdout.
 */
void wpa_msg(void *ctx, int level, const char *fmt, ...) PRINTF_FORMAT(3, 4);
#else
#define wpa_msg(args...)        NO_PRINT
#endif /* CONFIG_WPA_MSG */


#ifdef CONFIG_WPA_MSG_CTRL
/**
 * wpa_msg_ctrl - Conditional printf for ctrl_iface monitors
 * @ctx: Pointer to context data; this is the ctx variable registered
 *    with struct wpa_driver_ops::init()
 * @level: priority level (MSG_*) of the message
 * @fmt: printf format string, followed by optional arguments
 *
 * This function is used to print conditional debugging and error messages.
 * This function is like wpa_msg(), but it sends the output only to the
 * attached ctrl_iface monitors. In other words, it can be used for frequent
 * events that do not need to be sent to syslog.
 */
void wpa_msg_ctrl(void *ctx, int level, const char *fmt, ...)
PRINTF_FORMAT(3, 4);
#else
#define wpa_msg_ctrl(args...)        NO_PRINT
#endif /* CONFIG_WPA_MSG_CTRL */


#ifdef CONFIG_WPA_MSG_GLOBAL
/**
 * wpa_msg_global - Global printf for ctrl_iface monitors
 * @ctx: Pointer to context data; this is the ctx variable registered
 *    with struct wpa_driver_ops::init()
 * @level: priority level (MSG_*) of the message
 * @fmt: printf format string, followed by optional arguments
 *
 * This function is used to print conditional debugging and error messages.
 * This function is like wpa_msg(), but it sends the output as a global event,
 * i.e., without being specific to an interface. For backwards compatibility,
 * an old style event is also delivered on one of the interfaces (the one
 * specified by the context data).
 */
void wpa_msg_global(void *ctx, int level, const char *fmt, ...)
PRINTF_FORMAT(3, 4);
#else
#define wpa_msg_global(args...)        NO_PRINT
#endif /* CONFIG_WPA_MSG_GLOBAL */


#ifdef CONFIG_WPA_MSG_GLOBAL_CTRL
/**
 * wpa_msg_global_ctrl - Conditional global printf for ctrl_iface monitors
 * @ctx: Pointer to context data; this is the ctx variable registered
 *    with struct wpa_driver_ops::init()
 * @level: priority level (MSG_*) of the message
 * @fmt: printf format string, followed by optional arguments
 *
 * This function is used to print conditional debugging and error messages.
 * This function is like wpa_msg_global(), but it sends the output only to the
 * attached global ctrl_iface monitors. In other words, it can be used for
 * frequent events that do not need to be sent to syslog.
 */
void wpa_msg_global_ctrl(void *ctx, int level, const char *fmt, ...)
PRINTF_FORMAT(3, 4);
#else
#define wpa_msg_global_ctrl(args...)    NO_PRINT
#endif /* CONFIG_WPA_MSG_GLOBAL_CTRL */


#ifdef CONFIG_WPA_MSG_NO_GLOBAL
/**
 * wpa_msg_no_global - Conditional printf for ctrl_iface monitors
 * @ctx: Pointer to context data; this is the ctx variable registered
 *    with struct wpa_driver_ops::init()
 * @level: priority level (MSG_*) of the message
 * @fmt: printf format string, followed by optional arguments
 *
 * This function is used to print conditional debugging and error messages.
 * This function is like wpa_msg(), but it does not send the output as a global
 * event.
 */
void wpa_msg_no_global(void *ctx, int level, const char *fmt, ...)
PRINTF_FORMAT(3, 4);
#else
#define wpa_msg_no_global(args...)    NO_PRINT
#endif /* CONFIG_WPA_MSG_NO_GLOBAL */


#ifdef CONFIG_WPA_MSG_GLOBAL_ONLY
/**
 * wpa_msg_global_only - Conditional printf for ctrl_iface monitors
 * @ctx: Pointer to context data; this is the ctx variable registered
 *    with struct wpa_driver_ops::init()
 * @level: priority level (MSG_*) of the message
 * @fmt: printf format string, followed by optional arguments
 *
 * This function is used to print conditional debugging and error messages.
 * This function is like wpa_msg_global(), but it sends the output only as a
 * global event.
 */
void wpa_msg_global_only(void *ctx, int level, const char *fmt, ...)
PRINTF_FORMAT(3, 4);
#else
#define wpa_msg_global_only(args...)    NO_PRINT
#endif /* CONFIG_WPA_MSG_GLOBAL_ONLY */



#ifndef WPA_MSG_TYPE
#define WPA_MSG_TYPE
enum wpa_msg_type {
    WPA_MSG_PER_INTERFACE,
    WPA_MSG_GLOBAL,
    WPA_MSG_NO_GLOBAL,
    WPA_MSG_ONLY_GLOBAL,
};
#endif /* WPA_MSG_TYPE */

//=============================================================

#ifdef    CONFIG_WPA_ASSERT
  #define WPA_ASSERT(a)                                        \
    do {                                                    \
        if (!(a)) {                                            \
            ra6w1_prt("[%s] WPA_ASSERT FAILED '" #a "'\n",    \
                __func__);                                    \
            exit(1);                                        \
        }                                                    \
    } while (0)

#else
  #define WPA_ASSERT(a)            NO_PRINT
#endif    /* CONFIG_WPA_ASSERT */


/* EOF */
