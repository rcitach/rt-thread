/*
 * wpa_supplicant/hostapd / common helper functions, etc.
 * Copyright (c) 2002-2007, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef __SUPP_COMMON_H
#define __SUPP_COMMON_H

#include "os.h"
#include "lwip/netif.h"

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__DragonFly__) || \
    defined(__OpenBSD__)
#include <sys/types.h>
#include <sys/endian.h>

#include "wpabuf.h"

#ifndef _BYTE_ORDER
#define _BYTE_ORDER _LITTLE_ENDIAN
#endif /* _BYTE_ORDER */


#define __BYTE_ORDER    _BYTE_ORDER
#define __LITTLE_ENDIAN _LITTLE_ENDIAN
#define __BIG_ENDIAN    _BIG_ENDIAN
#ifdef __OpenBSD__
#define bswap_16 swap16
#define bswap_32 swap32
#define bswap_64 swap64
#else /* __OpenBSD__ */
#define bswap_16 bswap16
#define bswap_32 bswap32
#define bswap_64 bswap64
#endif /* __OpenBSD__ */
#endif /* defined(__FreeBSD__) || defined(__NetBSD__) ||
    * defined(__DragonFly__) || defined(__OpenBSD__) */

#define __BYTE_ORDER    _BYTE_ORDER
#define __LITTLE_ENDIAN _LITTLE_ENDIAN
#define __BIG_ENDIAN    _BIG_ENDIAN
static inline unsigned short bswap_16(unsigned short v)
{
    return (unsigned short)(((v & 0xff) << 8) | (v >> 8));
}

static inline unsigned int bswap_32(unsigned int v)
{
    return ((v & 0xff) << 24) | ((v & 0xff00) << 8) |
        ((v & 0xff0000) >> 8) | (v >> 24);
}

static inline unsigned long long bswap_64(unsigned long long x)
{
    return ((x >> 56) & 0x00000000000000FFull) |
        ((x >> 40) & 0x000000000000FF00ull) |
        ((x >> 24) & 0x0000000000FF0000ull) |
        ((x >> 8) & 0x00000000FF000000ull) |
        ((x << 8) & 0x000000FF00000000ull) |
        ((x << 24) & 0x0000FF0000000000ull) |
        ((x << 40) & 0x00FF000000000000ull) |
        ((x << 56) & 0xFF00000000000000ull);
}

#define WPA_TYPES_DEFINED

#ifndef WPA_TYPES_DEFINED


#ifdef CONFIG_USE_INTTYPES_H
#include <inttypes.h>
#else
#include <stdint.h>
#endif

#define WPA_TYPES_DEFINED
#endif /* !WPA_TYPES_DEFINED */

#if 0
/* Define platform specific byte swapping macros */

#if defined(__CYGWIN__) || defined(CONFIG_NATIVE_WINDOWS)

static inline unsigned short wpa_swap_16(unsigned short v)
{
    return ((v & 0xff) << 8) | (v >> 8);
}

static inline unsigned int wpa_swap_32(unsigned int v)
{
    return ((v & 0xff) << 24) | ((v & 0xff00) << 8) |
        ((v & 0xff0000) >> 8) | (v >> 24);
}

#define le_to_host16(n) (n)
#define host_to_le16(n) (n)
#define be_to_host16(n) wpa_swap_16(n)
#define host_to_be16(n) wpa_swap_16(n)
#define le_to_host32(n) (n)
#define host_to_le32(n) (n)
#define be_to_host32(n) wpa_swap_32(n)
#define host_to_be32(n) wpa_swap_32(n)
#define host_to_le64(n) (n)

#define WPA_BYTE_SWAP_DEFINED

#endif /* __CYGWIN__ || CONFIG_NATIVE_WINDOWS */
#endif

#ifndef WPA_BYTE_SWAP_DEFINED

#ifndef __BYTE_ORDER
#ifndef __LITTLE_ENDIAN
#ifndef __BIG_ENDIAN
#define __LITTLE_ENDIAN 1234
#define __BIG_ENDIAN 4321
#if defined(sparc)
#define __BYTE_ORDER __BIG_ENDIAN
#endif
#endif /* __BIG_ENDIAN */
#endif /* __LITTLE_ENDIAN */
#endif /* __BYTE_ORDER */

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define le_to_host16(n) ((__force u16) (le16) (n))
#define host_to_le16(n) ((__force le16) (u16) (n))
#define be_to_host16(n) bswap_16((__force u16) (be16) (n))
#define host_to_be16(n) ((__force be16) bswap_16((n)))
#define le_to_host32(n) ((__force u32) (le32) (n))
#define host_to_le32(n) ((__force le32) (u32) (n))
#define be_to_host32(n) bswap_32((__force u32) (be32) (n))
#define host_to_be32(n) ((__force be32) bswap_32((n)))
#define le_to_host64(n) ((__force u64) (le64) (n))
#define host_to_le64(n) ((__force le64) (u64) (n))
#define be_to_host64(n) bswap_64((__force u64) (be64) (n))
#define host_to_be64(n) ((__force be64) bswap_64((n)))
#elif __BYTE_ORDER == __BIG_ENDIAN
#define le_to_host16(n) bswap_16(n)
#define host_to_le16(n) bswap_16(n)
#define be_to_host16(n) (n)
#define host_to_be16(n) (n)
#define le_to_host32(n) bswap_32(n)
#define host_to_le32(n) bswap_32(n)
#define be_to_host32(n) (n)
#define host_to_be32(n) (n)
#define le_to_host64(n) bswap_64(n)
#define host_to_le64(n) bswap_64(n)
#define be_to_host64(n) (n)
#define host_to_be64(n) (n)
#ifndef WORDS_BIGENDIAN
#define WORDS_BIGENDIAN
#endif
#else
/* #error Could not determine CPU byte order */
#endif

#define WPA_BYTE_SWAP_DEFINED
#endif /* !WPA_BYTE_SWAP_DEFINED */


/* Macros for handling unaligned memory accesses */

static inline u16 WPA_GET_BE16(const u8 *a)
{
    return (u16) ((a[0] << 8) | a[1]);
}

static inline void WPA_PUT_BE16(u8 *a, u16 val)
{
    a[0] = (u8) (val >> 8);
    a[1] = (u8) (val & 0xff);
}

static inline u16 WPA_GET_LE16(const u8 *a)
{
    return (u16) ((a[1] << 8) | a[0]);
}

static inline void WPA_PUT_LE16(u8 *a, u16 val)
{
    a[1] = (u8) (val >> 8);
    a[0] = (u8) (val & 0xff);
}

static inline u32 WPA_GET_BE24(const u8 *a)
{
    return (u32) ((a[0] << 16) | (a[1] << 8) | a[2]);
}

static inline void WPA_PUT_BE24(u8 *a, u32 val)
{
    a[0] = (val >> 16) & 0xff;
    a[1] = (val >> 8) & 0xff;
    a[2] = val & 0xff;
}

static inline u32 WPA_GET_BE32(const u8 *a)
{
    return ((u32) a[0] << 24) | (a[1] << 16) | (a[2] << 8) | a[3];
}

static inline void WPA_PUT_BE32(u8 *a, u32 val)
{
    a[0] = (u8) ((val >> 24) & 0xff);
    a[1] = (u8) ((val >> 16) & 0xff);
    a[2] = (u8) ((val >> 8) & 0xff);
    a[3] = (u8) (val & 0xff);
}

static inline u32 WPA_GET_LE32(const u8 *a)
{
    return ((u32) a[3] << 24) | (a[2] << 16) | (a[1] << 8) | a[0];
}

static inline void WPA_PUT_LE32(u8 *a, u32 val)
{
    a[3] = (u8) ((val >> 24) & 0xff);
    a[2] = (u8) ((val >> 16) & 0xff);
    a[1] = (u8) ((val >> 8) & 0xff);
    a[0] = (u8) (val & 0xff);
}

static inline u64 WPA_GET_BE64(const u8 *a)
{
    return (((u64) a[0]) << 56) | (((u64) a[1]) << 48) |
        (((u64) a[2]) << 40) | (((u64) a[3]) << 32) |
        (((u64) a[4]) << 24) | (((u64) a[5]) << 16) |
        (((u64) a[6]) << 8) | ((u64) a[7]);
}

static inline void WPA_PUT_BE64(u8 *a, u64 val)
{
    a[0] = (u8) (val >> 56);
    a[1] = (u8) (val >> 48);
    a[2] = (u8) (val >> 40);
    a[3] = (u8) (val >> 32);
    a[4] = (u8) (val >> 24);
    a[5] = (u8) (val >> 16);
    a[6] = (u8) (val >> 8);
    a[7] = (u8) (val & 0xff);
}

static inline u64 WPA_GET_LE64(const u8 *a)
{
    return (((u64) a[7]) << 56) | (((u64) a[6]) << 48) |
        (((u64) a[5]) << 40) | (((u64) a[4]) << 32) |
        (((u64) a[3]) << 24) | (((u64) a[2]) << 16) |
        (((u64) a[1]) << 8) | ((u64) a[0]);
}

static inline void WPA_PUT_LE64(u8 *a, u64 val)
{
    a[7] = (u8) (val >> 56);
    a[6] = (u8) (val >> 48);
    a[5] = (u8) (val >> 40);
    a[4] = (u8) (val >> 32);
    a[3] = (u8) (val >> 24);
    a[2] = (u8) (val >> 16);
    a[1] = (u8) (val >> 8);
    a[0] = (u8) (val & 0xff);
}


#ifndef ETH_ALEN
#define ETH_ALEN 6
#endif
#ifndef ETH_HLEN
#define ETH_HLEN 14
#endif
#ifndef IFNAMSIZ
#define IFNAMSIZ NETIF_NAMESIZE
#endif
#ifndef ETH_P_ALL
#define ETH_P_ALL 0x0003
#endif
#ifndef ETH_P_IP
#define ETH_P_IP 0x0800
#endif
#ifndef ETH_P_80211_ENCAP
#define ETH_P_80211_ENCAP 0x890d /* TDLS comes under this category */
#endif
#ifndef ETH_P_PAE
#define ETH_P_PAE 0x888E /* Port Access Entity (IEEE 802.1X) */
#endif /* ETH_P_PAE */
#ifndef ETH_P_EAPOL
#define ETH_P_EAPOL ETH_P_PAE
#endif /* ETH_P_EAPOL */
#ifndef ETH_P_RSN_PREAUTH
#define ETH_P_RSN_PREAUTH 0x88c7
#endif /* ETH_P_RSN_PREAUTH */
#ifndef ETH_P_RRB
#define ETH_P_RRB 0x890D
#endif /* ETH_P_RRB */
#ifndef ETH_P_OUI
#define ETH_P_OUI 0x88B7
#endif /* ETH_P_OUI */


#ifdef __GNUC__
#define PRINTF_FORMAT(a,b) __attribute__ ((format (printf, (a), (b))))
#define STRUCT_PACKED __attribute__ ((packed))
#else
#define PRINTF_FORMAT(a,b)
#define STRUCT_PACKED
#endif


#if 1 // add by munchang
int getopt(int argc, char *const argv[], const char *optstring);
extern char *optarg;
extern int optind;
#endif

#ifdef CONFIG_ANSI_C_EXTRA

#if !defined(_MSC_VER) || _MSC_VER < 1400
/* snprintf - used in number of places; sprintf() is _not_ a good replacement
 * due to possible buffer overflow; see, e.g.,
 * http://www.ijs.si/software/snprintf/ for portable implementation of
 * snprintf. */
int snprintf(char *str, size_t size, const char *format, ...);

/* vsnprintf - only used for wpa_msg() in wpa_supplicant.c */
int vsnprintf(char *str, size_t size, const char *format, va_list ap);
#endif /* !defined(_MSC_VER) || _MSC_VER < 1400 */

/* getopt - only used in main.c */
int getopt(int argc, char *const argv[], const char *optstring);
extern char *optarg;
extern int optind;

#ifndef CONFIG_NO_SOCKLEN_T_TYPEDEF
#ifndef __socklen_t_defined
typedef int socklen_t;
#endif /* __socklen_t_defined */
#endif /* CONFIG_NO_SOCKLEN_T_TYPEDEF */

/* inline - define as __inline or just define it to be empty, if needed */
#ifdef CONFIG_NO_INLINE
#define inline
#else
#define inline __inline
#endif /* CONFIG_NO_INLINE */

#ifndef __func__
#define __func__ "__func__ not defined"
#endif

#ifndef bswap_16
#define bswap_16(a) ((((u16) (a) << 8) & 0xff00) | (((u16) (a) >> 8) & 0xff))
#endif

#ifndef bswap_32
#define bswap_32(a) ((((u32) (a) << 24) & 0xff000000) | \
             (((u32) (a) << 8) & 0xff0000) | \
                 (((u32) (a) >> 8) & 0xff00) | \
                 (((u32) (a) >> 24) & 0xff))
#endif

#ifndef MSG_DONTWAIT
#define MSG_DONTWAIT 0
#endif

#ifdef _WIN32_WCE
void perror(const char *s);
#endif /* _WIN32_WCE */

#endif /* CONFIG_ANSI_C_EXTRA */

#ifndef MAC2STR
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#define MACSTR2 "%02X:%02X:%02X:%02X:%02X:%02X"

/*
 * Compact form for string representation of MAC address
 * To be used, e.g., for constructing dbus paths for P2P Devices
 */
#define COMPACT_MACSTR "%02x%02x%02x%02x%02x%02x"
#endif /* MAC2STR */

#ifndef BIT
#define BIT(x) (1U << (x))
#endif

#ifndef BIT_ULL
#define BIT_ULL(x) (1ULL << (x))
#endif

/*
 * Definitions for sparse validation
 * (http://kernel.org/pub/linux/kernel/people/josh/sparse/)
 */
#ifdef __CHECKER__
#define __force __attribute__((force))
#undef __bitwise
#define __bitwise __attribute__((bitwise))
#else
#define __force
#undef __bitwise
#define __bitwise
#endif

typedef u16 __bitwise be16;
typedef u16 __bitwise le16;
typedef u32 __bitwise be32;
typedef u32 __bitwise le32;
typedef u64 __bitwise be64;
typedef u64 __bitwise le64;

#ifndef __must_check
#if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)
#define __must_check __attribute__((__warn_unused_result__))
#else
#define __must_check
#endif /* __GNUC__ */
#endif /* __must_check */

#ifndef __maybe_unused
#if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)
#define __maybe_unused __attribute__((unused))
#else
#define __maybe_unused
#endif /* __GNUC__ */
#endif /* __must_check */

#define SSID_MAX_LEN 32

struct wpa_ssid_value {
    u8 ssid[SSID_MAX_LEN];
    size_t ssid_len;
};

int hwaddr_aton(const char *txt, u8 *addr);
int hwaddr_masked_aton(const char *txt, u8 *addr, u8 *mask, u8 maskable);
int hwaddr_compact_aton(const char *txt, u8 *addr);
int hwaddr_aton2(const char *txt, u8 *addr);
int hex2byte(const char *hex);
int hexstr2bin(const char *hex, u8 *buf, size_t len);
void inc_byte_array(u8 *counter, size_t len);
void buf_shift_right(u8 *buf, size_t len, size_t bits);
#ifdef CONFIG_OWE_TRANS
void disc_byte_array(u8 *counter, size_t len);
#endif  // CONFIG_OWE_TRANS
void wpa_get_ntp_timestamp(u8 *buf);
int wpa_scnprintf(char *buf, size_t size, const char *fmt, ...);
int wpa_snprintf_hex_sep(char *buf, size_t buf_size, const u8 *data, size_t len,
             char sep);
int wpa_snprintf_hex(char *buf, size_t buf_size, const u8 *data, size_t len);
int wpa_snprintf_hex_uppercase(char *buf, size_t buf_size, const u8 *data,
                   size_t len);

int hwaddr_mask_txt(char *buf, size_t len, const u8 *addr, const u8 *mask);
int ssid_parse(const char *buf, struct wpa_ssid_value *ssid);

#ifdef CONFIG_NATIVE_WINDOWS
void wpa_unicode2ascii_inplace(TCHAR *str);
TCHAR * wpa_strdup_tchar(const char *str);
#else /* CONFIG_NATIVE_WINDOWS */
#define wpa_unicode2ascii_inplace(s) do { } while (0)
#define wpa_strdup_tchar(s) strdup((s))
#endif /* CONFIG_NATIVE_WINDOWS */

void printf_encode(char *txt, size_t maxlen, const u8 *data, size_t len);
size_t printf_decode(u8 *buf, size_t maxlen, const char *str);

const char * wpa_ssid_txt(const u8 *ssid, size_t ssid_len);
const char * wpa_ssid_plain_txt(const u8 *ssid, size_t ssid_len);


char * wpa_config_parse_string(const char *value, size_t *len);
int is_hex(const u8 *data, size_t len);
int has_ctrl_char(const u8 *data, size_t len);
int has_newline(const char *str);
int find_first_bit2(u32 value);
size_t merge_byte_arrays(u8 *res, size_t res_len,
             const u8 *src1, size_t src1_len,
             const u8 *src2, size_t src2_len);
char * dup_binstr(const void *src, size_t len);

extern bool is_broadcast_ether_addr(const u8 *a);
extern bool is_multicast_ether_addr(const u8 *a);
#if 0
static inline int is_zero_ether_addr(const u8 *a)
{
    return !(a[0] | a[1] | a[2] | a[3] | a[4] | a[5]);
}

static inline int is_broadcast_ether_addr(const u8 *a)
{
    return (a[0] & a[1] & a[2] & a[3] & a[4] & a[5]) == 0xff;
}

static inline int is_multicast_ether_addr(const u8 *a)
{
    return a[0] & 0x01;
}
#endif 

#define broadcast_ether_addr (const u8 *) "\xff\xff\xff\xff\xff\xff"

#if 0
#include "wpa_debug.h"
#endif

struct wpa_freq_range_list {
    struct wpa_freq_range {
        unsigned int min;
        unsigned int max;
    } *range;
    unsigned int num;
};

int freq_range_list_parse(struct wpa_freq_range_list *res, const char *value);
int freq_range_list_includes(const struct wpa_freq_range_list *list,
                 unsigned int freq);
char * freq_range_list_str(const struct wpa_freq_range_list *list);
#ifdef CONFIG_STA_COUNTRY_CODE
int country_to_freq_range_list(struct wpa_freq_range_list * freq_range, char *country);
#endif /* CONFIG_STA_COUNTRY_CODE */
int int_array_len(const int *a);
void int_array_concat(int **res, const int *a);
void int_array_sort_unique(int *a);
void int_array_add_unique(int **res, int a);
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif
void str_clear_free(char *str);
void bin_clear_free(void *bin, size_t len);
char * get_param(const char *cmd, const char *param);
void set_log_mask_whole(unsigned long mask);
void set_log_mask_whole_wpa(int mask);

int random_mac_addr(u8 *addr);
int random_mac_addr_keep_oui(u8 *addr);

const char * cstr_token(const char *str, const char *delim, const char **last);
char * str_token(char *str, const char *delim, char **context);
size_t utf8_escape(const char *inp, size_t in_size,
           char *outp, size_t out_size);
size_t utf8_unescape(const char *inp, size_t in_size,
             char *outp, size_t out_size);
int is_ctrl_char(char c);

u8 rssi_to_rcpi(int rssi);

void swap_buf(const u8 *orig, u8 *swap, int len);

void forced_memzero(void *ptr, size_t len);

/*
 * gcc 4.4 ends up generating strict-aliasing warnings about some very common
 * networking socket uses that do not really result in a real problem and
 * cannot be easily avoided with union-based type-punning due to struct
 * definitions including another struct in system header files. To avoid having
 * to fully disable strict-aliasing warnings, provide a mechanism to hide the
 * typecast from aliasing for now. A cleaner solution will hopefully be found
 * in the future to handle these cases.
 */
void * __hide_aliasing_typecast(void *foo);
#define aliasing_hide_typecast(a,t) (t *) __hide_aliasing_typecast((a))

#ifdef __SUPPORT_ATCMD__
extern void atcmd_asynchony_event(int index, char *buf);
#endif

#if (ATCMD_IF_SUPPORT == 1)
extern void * gp_wifi_atcmd_ctrl;
extern unsigned int (* gp_wifi_atcmd_event_cb)(void * const p_ctrl, int index, unsigned char * p_in, unsigned int inlen);
#endif

#ifdef CONFIG_VALGRIND
#include <valgrind/memcheck.h>
#define WPA_MEM_DEFINED(ptr, len) VALGRIND_MAKE_MEM_DEFINED((ptr), (len))
#else /* CONFIG_VALGRIND */
#define WPA_MEM_DEFINED(ptr, len) do { } while (0)
#endif /* CONFIG_VALGRIND */

#ifdef CONFIG_LOG_MASK
#define SUPP_LOG_NOTICE         BIT(0)
#define SUPP_LOG_WARN           BIT(1)
#define SUPP_LOG_ERR            BIT(2)
#define SUPP_LOG_FATAL          BIT(3)
#define SUPP_LOG_INFO           BIT(4)
#define SUPP_LOG_DEBUG          BIT(10)
#define SUPP_LOG_EVENT          0x10000000
#define SUPP_LOG_ALL            0x7FFFFFFF
#define SUPP_LOG_DEFAULT        0x0000001F /* NOTICE, WARN, ERR, FATAL INFO */

void set_log_mask(unsigned long mask, unsigned long enable);
int get_log_mask(void);
#endif /* CONFIG_LOG_MASK */

#define HOSTAPD_MODULE_IEEE80211    0x00000001
#define HOSTAPD_MODULE_IEEE8021X    0x00000002
#define HOSTAPD_MODULE_RADIUS       0x00000004
#define HOSTAPD_MODULE_WPA      0x00000008
#define HOSTAPD_MODULE_DRIVER       0x00000010
#define HOSTAPD_MODULE_MLME     0x00000040

enum hostapd_logger_level {
    HOSTAPD_LEVEL_DEBUG_VERBOSE = 0,
    HOSTAPD_LEVEL_DEBUG = 1,
    HOSTAPD_LEVEL_INFO = 2,
    HOSTAPD_LEVEL_NOTICE = 3,
    HOSTAPD_LEVEL_WARNING = 4
};

void reboot_func_cb(void *eloop_ctx, void *timeout_ctx);

/*
 * Customer's callback function to notify Wi-Fi connection state.
 */
extern void (*wifi_conn_notify_cb)(void);
extern void (*wifi_conn_fail_notify_cb)(short reason_code);
extern void (*wifi_disconn_notify_cb)(short reason_code);

//MODIFY_SUPPLICANT_FOR_FREERTOS
#define false   0
#define true    1

#endif /* COMMON_H */
