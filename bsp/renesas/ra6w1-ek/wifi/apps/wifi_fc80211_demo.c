/*
 * Standalone RA6W1 Wi-Fi bring-up and scan example.
 *
 * This file intentionally does not create a wpa_supplicant interface and does
 * not call any WIFI_ or rm_wifi_ service API.  The Wi-Fi control path is the
 * cfg80211 ABI exported by librwnx_drv.a; the small WPA/RSN state machine is
 * driven directly from the same driver's EAPOL queue.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rtthread.h>
#include <dev_wlan.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "co_int.h"

/* Older SDK headers use uint32 while co_int.h exposes uint32_t. */
typedef unsigned long uint32;

#include "rwnx_cfg.h"
#include "driver_fc80211.h"
#include "rwnx_hw.h"
#include "romac4rtos.h"
#include "supp_eloop.h"
#include "os.h"
#include "sys_feature.h"
#include "wifi_wpa_sm_port.h"
#include "wifi_security_manager.h"
#include "lwip/pbuf.h"
#include "mbedtls/md.h"
#include "mbedtls/pkcs5.h"

#include "wifi_crypto_port.h"

#include "wifi_hw_prepare.h"

/* Symbols exported by the prebuilt Wi-Fi archives. */
extern BaseType_t rwnx_mac_task_initiailize(void);
extern BaseType_t rwnx_driver_task_initiailize(void);
extern int rwnx_hw_get_status(void);
extern void rwnx_cfg80211_initialize(void);
extern struct wiphy rwnx_wiphy;
extern void fc80211_enable_drv_event(void);
extern void fc80211_update_channel_info(void);
extern void ra6w1_regdb_data_init(char *country);
extern QueueHandle_t TO_SUPP_QUEUE;
extern QueueHandle_t CFG80211_semaphore;
extern void supplicant_done(void);

/* Vendor PSK derivation, exported by libsupplicant.a
 * (src/crypto/sha1-pbkdf2.c).  This is the exact primitive the SDK supplicant
 * uses for the passphrase -> PMK step: pbkdf2_sha1() -> hmac_sha1() -> HSU.
 * It is used here as the reference implementation for the local Mbed TLS
 * (CryptoCell SHA1 ALT) derivation. */
extern int pbkdf2_sha1(const char *passphrase, const u8 *ssid, size_t ssid_len,
                       int iterations, u8 *buf, size_t buflen);

/* These functions live in librwnx_drv.a but are not declared by rwnx_cfg.h. */
extern int fc80211_get_scan(ULONG wdev_id, int ifidx, void *arg);
extern int fc80211_abort_scan(ULONG wdev_id, int ifidx);
extern int fc80211_free_umac_scan_results(ULONG wdev_id, int ifidx);
extern int i3ed11_l2data_from_sp(int ifindex, unsigned char *data,
                                 unsigned int length, bool hasMacHeader,
                                 const char *dst);

#define WIFI_DEMO_IF_INDEX       (0U)
#define WIFI_DEMO_IF_NAME        "wlan0"
#define WIFI_DEMO_STATUS_ACTIVE  (1)
#define WIFI_DEMO_STATUS_RETRY   (1000U)
#define WIFI_DEMO_WIPHY_WAIT_MS  (1500U)
#define WIFI_DEMO_SCAN_WAIT_MS   (10000U)
#define WIFI_DEMO_SCAN_SETTLE_MS (200U)
#define WIFI_DEMO_CONNECT_WAIT_MS (15000U)
#define WIFI_DEMO_CONNECT_POLL_MS (2U)
#define WIFI_DEMO_DISCONNECT_WAIT_MS (2000U)
#define WIFI_DEMO_POLL_MS        (20U)

/* Every byte written to the console delays the EAPOL-Key response, and an AP
 * drops a message 2/4 whose replay counter is older than the message 1/4 it is
 * currently waiting for.  Keep the per-frame trace off unless it is needed. */
#define WIFI_DEMO_EAPOL_VERBOSE  (0)

/* Dump the association IEs and the EAPOL-Key message 2/4 so the MIC and the
 * RSN IE can be checked off-line against the PMK/nonces. */
#define WIFI_DEMO_DUMP_FRAMES    (0)
#define WIFI_DEMO_MAX_CHANNELS   (42U)
#define WIFI_DEMO_MAX_SSID_LEN   (32U)
#define WIFI_DEMO_PSK_LEN        (32U)
#define WIFI_DEMO_MAX_AP_RSN_IE_LEN (257U)

#define WIFI_DEMO_STATUS_NOT_SUPPORTED_AUTH_ALG (13)
#define WIFI_DEMO_STATUS_AUTH_TIMEOUT (16)
#define WIFI_DEMO_STATUS_UNSPECIFIED_FAILURE (1)

#define WIFI_DEMO_ETH_HDR_LEN       (14U)
#define WIFI_DEMO_EAPOL_ETHERTYPE   (0x888eU)

/* The firmware reports WLAN_STATUS_SUCCESS as zero. */
#define WIFI_DEMO_STATUS_SUCCESS (0)

/* rwnx uses bit 0 for a disabled channel in its private channel flags. */
#define WIFI_DEMO_CHAN_DISABLED  (1U)

struct wifi_demo_state
{
    bool event_queue_ready;
    bool cfg80211_ready;
    bool crypto_ready;
    bool hardware_ready;
    bool wiphy_ready;
    bool iface_ready;
    bool scan_active;
    bool scan_done;
    bool scan_aborted;
    bool connect_active;
    bool connect_done;
    bool connect_timed_out;
    bool connected;
    bool connect_secure;
    bool wpa_key_done;
    bool disconnect_in_progress;
    enum wpa_states wpa_state;
    int connect_status;
    u8 connect_bssid[ETH_ALEN];
    char connected_ssid[WIFI_DEMO_MAX_SSID_LEN + 1U];
    unsigned int connected_freq;
    int station_query_status;
    bool disconnect_done;
    struct wireless_dev *wdev;
    struct wifi_security_manager security;
    struct wpa_sm *wpa_sm;
    struct wpa_sm_ctx *wpa_ctx;
};

static struct wifi_demo_state s_wifi_demo;

/* rsdev_cfg80211_associate() converts the supplicant ABI into a temporary
 * cfg80211_connect_params object and then calls rdev_connect().  Keep the
 * input objects static because this demo waits for the asynchronous result
 * after the call returns. */
static struct fc80211_global *s_connect_global;
static struct i802_bss *s_connect_bss;
static struct wpa_driver_fc80211_data *s_connect_driver;
static struct wpa_driver_associate_params s_connect_assoc;
static char s_connect_ssid[WIFI_DEMO_MAX_SSID_LEN + 1U];
static u8 s_connect_psk[WIFI_DEMO_PSK_LEN];
static u8 s_connect_target_bssid[ETH_ALEN];
static bool s_connect_target_bssid_set;
static unsigned int s_connect_target_freq;
static bool s_connect_secure;
static bool s_connect_host_wpa;
static u8 s_connect_ap_rsn_ie[WIFI_DEMO_MAX_AP_RSN_IE_LEN];
static size_t s_connect_ap_rsn_ie_len;
static u8 s_connect_req_rsn_ie[WIFI_DEMO_MAX_AP_RSN_IE_LEN];
static size_t s_connect_req_rsn_ie_len;

/* The standalone event queue is shared by driver events and EAPOL packets. */
static void wifi_demo_pump_events(void);

static void wifi_demo_print_bssid(const u8 *bssid);
static bool wifi_demo_bssid_is_zero(const u8 *bssid);
static size_t wifi_demo_find_rsn_ie(const u8 *ies, size_t ie_len,
                                    u8 *out, size_t out_len);
static size_t wifi_demo_copy_rsn_ie(const u8 *ies, size_t ie_len);
static int wifi_demo_query_station(struct station_info *sinfo);
static void wifi_demo_print_station_info(const struct station_info *sinfo);

static rt_wlan_security_t wifi_demo_security_from_ies(const u8 *ies,
                                                       size_t ie_len);
static int wifi_demo_frequency_to_channel(int freq);

/*
 * fc80211_get_scan() predates the public cfg80211 result API.  It receives an
 * internal result object whose first two members are an array and a count.
 * Keep this ABI-local copy here instead of importing supplicant's private
 * driver structures.
 */
struct wifi_demo_scan_res
{
    unsigned int flags;
    u8 bssid[ETH_ALEN];
    int freq;
    u16 beacon_int;
    u16 caps;
    int qual;
    int noise;
    int level;
    u64 tsf;
    unsigned int age;
    unsigned int est_throughput;
    int snr;
    u64 parent_tsf;
    u8 tsf_bssid[ETH_ALEN];
    size_t ie_len;
    size_t beacon_ie_len;
    u8 *internal_bss;
    const u8 *ie;
    const u8 *beacon_ie;
};

struct wifi_demo_scan_results
{
    struct wifi_demo_scan_res **res;
    size_t num;
};

_Static_assert(sizeof(struct wifi_demo_scan_res) == 96U,
               "fc80211 scan result ABI changed");
_Static_assert(offsetof(struct wifi_demo_scan_res, ie) == 84U,
               "fc80211 scan result IE ABI changed");
_Static_assert(offsetof(struct wifi_demo_scan_res, beacon_ie) == 88U,
               "fc80211 scan result beacon IE ABI changed");

struct wifi_demo_eth_hdr
{
    u8 dest[ETH_ALEN];
    u8 src[ETH_ALEN];
    u16 ethertype;
} __attribute__((packed));

_Static_assert(sizeof(struct wifi_demo_eth_hdr) == WIFI_DEMO_ETH_HDR_LEN,
               "Ethernet header layout changed");

#if WIFI_DEMO_DUMP_FRAMES
static void wifi_demo_hex_dump(const char *tag, const u8 *data, size_t len)
{
    size_t index;

    if (data == NULL)
    {
        rt_kprintf("[wifi-demo] %s: <null>\n", tag);
        return;
    }

    rt_kprintf("[wifi-demo] %s (%u bytes):", tag, (unsigned int) len);
    for (index = 0U; index < len; index++)
    {
        if ((index % 16U) == 0U)
        {
            rt_kprintf("\n  %03u:", (unsigned int) index);
        }

        rt_kprintf(" %02x", data[index]);
    }
    rt_kprintf("\n");
}
#else
#define wifi_demo_hex_dump(tag, data, len)  ((void) (tag))
#endif /* WIFI_DEMO_DUMP_FRAMES */

static const char *wifi_demo_wpa_state_name(enum wpa_states state)
{
    switch (state)
    {
    case WPA_DISCONNECTED:
        return "DISCONNECTED";
    case WPA_ASSOCIATED:
        return "ASSOCIATED";
    case WPA_4WAY_HANDSHAKE:
        return "4WAY";
    case WPA_GROUP_HANDSHAKE:
        return "GROUP";
    case WPA_COMPLETED:
        return "COMPLETED";
    default:
        return "OTHER";
    }
}

static void wifi_demo_wpa_set_state(void *ctx, enum wpa_states state)
{
    struct wifi_demo_state *state_ctx = (struct wifi_demo_state *) ctx;

    if (state_ctx == NULL)
    {
        return;
    }

    state_ctx->wpa_state = state;
    if (state == WPA_COMPLETED)
    {
        state_ctx->wpa_key_done = true;
        state_ctx->connected = true;
        state_ctx->connect_done = true;
        rt_kprintf("[wifi-demo] WPA2 key negotiation completed\n");
    }
    else if ((state == WPA_DISCONNECTED) ||
             (state == WPA_AUTHENTICATING))
    {
        state_ctx->wpa_key_done = false;
        state_ctx->connected = false;
    }

}

static void wifi_demo_wpa_deauthenticate(void *ctx, u16 reason_code)
{
    struct wifi_demo_state *state_ctx = (struct wifi_demo_state *) ctx;

    (void) reason_code;
    if (state_ctx != NULL)
    {
        state_ctx->connected = false;
        state_ctx->wpa_key_done = false;
    }

    /* The state machine detected an authentication failure.  The firmware
     * must leave the associated state before the next connect request. */
    if ((s_connect_driver != NULL) &&
        !s_wifi_demo.disconnect_in_progress)
    {
        (void) rwnx_cfg80211_disconnect((u8) WIFI_DEMO_IF_INDEX,
                                        FC80211_IFTYPE_STATION,
                                        reason_code);
    }
}

static void wifi_demo_wpa_reconnect(void *ctx)
{
    (void) ctx;
}

static void wifi_demo_wpa_deinit(void)
{
    wifi_security_manager_deinit(&s_wifi_demo.security);
    s_wifi_demo.wpa_sm = NULL;
    s_wifi_demo.wpa_ctx = NULL;
    s_wifi_demo.wpa_state = WPA_DISCONNECTED;
    s_wifi_demo.wpa_key_done = false;
}

static int wifi_demo_wpa_sm_init(void)
{
    struct wifi_security_config security_config;
    struct wifi_security_callbacks security_callbacks;
    const UCHAR *own_addr;

    wifi_demo_wpa_deinit();
    own_addr = wpa_driver_fc80211_get_macaddr(s_connect_bss);
    if (own_addr == NULL)
    {
        return -RT_ERROR;
    }

    /* The PTK (and therefore the EAPOL-Key MIC) is derived from this address:
     * the authenticator uses the STA MAC it saw in the (Re)Association
     * Request, so a zero or stale value here can never complete the 4-way
     * handshake. */
    if (wifi_demo_bssid_is_zero(own_addr))
    {
        rt_kprintf("[wifi-demo] fatal: driver reports a zero STA MAC; the "
                   "4-way PTK would not match the AP\n");
        return -RT_ERROR;
    }
    memset(&security_config, 0, sizeof(security_config));
    security_config.bss = s_connect_bss;
    security_config.ifname = WIFI_DEMO_IF_NAME;
    security_config.own_addr = own_addr;
    security_config.ssid = (const u8 *) s_connect_ssid;
    security_config.ssid_len = strlen(s_connect_ssid);
    security_config.pmk = s_connect_psk;
    security_config.pmk_len = sizeof(s_connect_psk);
    security_config.mode = WIFI_SECURITY_WPA2_PSK;
    security_config.user_ctx = &s_wifi_demo;
    memset(&security_callbacks, 0, sizeof(security_callbacks));
    security_callbacks.state_changed = wifi_demo_wpa_set_state;
    security_callbacks.deauthenticate = wifi_demo_wpa_deauthenticate;
    security_callbacks.reconnect = wifi_demo_wpa_reconnect;
    security_config.callbacks = security_callbacks;

    if (wifi_security_manager_init(&s_wifi_demo.security,
                                   &security_config) != RT_EOK)
    {
        rt_kprintf("[wifi-demo] security manager init failed\n");
        return -RT_ERROR;
    }
    s_wifi_demo.wpa_sm = s_wifi_demo.security.sm;
    s_wifi_demo.wpa_ctx = s_wifi_demo.security.sm_ctx;
    s_wifi_demo.wpa_state = WPA_ASSOCIATED;
    s_wifi_demo.wpa_key_done = false;
    return RT_EOK;
}

/* Print the EAPOL-Key header fields that show whether the AP is retransmitting
 * the *same* message 1/4 (identical replay counter and ANonce means the AP
 * either never received or rejected our message 2/4) or starting a fresh
 * handshake.  Offsets follow the IEEE 802.1X header + EAPOL-Key body. */
#if WIFI_DEMO_EAPOL_VERBOSE
static void wifi_demo_trace_eapol_key(const u8 *eapol, size_t len)
{
    const u8 *key;
    u16 key_info;
    unsigned int index;

    if ((eapol == NULL) || (len < 4U) || (eapol[1] != 3U))
    {
        return;
    }

    key = eapol + 4U;
    if (len < 4U + 17U)
    {
        return;
    }

    key_info = (u16) (((u16) key[1] << 8) | (u16) key[2]);
    rt_kprintf("[wifi-demo] EAPOL-Key key_info=0x%04x ver=%u mic=%u "
               "replay=",
               (unsigned int) key_info,
               (unsigned int) (key_info & 0x0007U),
               (unsigned int) ((key_info & 0x0100U) != 0U));
    for (index = 0U; index < 8U; index++)
    {
        rt_kprintf("%02x", key[5U + index]);
    }
    if (len < 4U + 49U)
    {
        rt_kprintf(" anonce=<short>\n");
        return;
    }
    rt_kprintf(" anonce=");
    for (index = 0U; index < 8U; index++)
    {
        rt_kprintf("%02x", key[13U + index]);
    }
    rt_kprintf("...\n");
}
#endif /* WIFI_DEMO_EAPOL_VERBOSE */

static void wifi_demo_handle_l2_packet(struct pbuf *packet)
{
    struct wifi_demo_eth_hdr eth;
    u8 *eapol = NULL;
    u16 ethertype;
    u16 payload_len;
    u16 copied;

    if (packet == NULL)
    {
        return;
    }

    if (packet->tot_len < WIFI_DEMO_ETH_HDR_LEN)
    {
        pbuf_free(packet);
        return;
    }

    copied = pbuf_copy_partial(packet, &eth, sizeof(eth), 0);
    if (copied != sizeof(eth))
    {
        pbuf_free(packet);
        return;
    }

    ethertype = (u16) (((u16) ((u8 *) &eth.ethertype)[0] << 8) |
                       ((u8 *) &eth.ethertype)[1]);
    if ((ethertype != WIFI_DEMO_EAPOL_ETHERTYPE) ||
        (s_wifi_demo.wpa_sm == NULL) ||
        (packet->tot_len <= WIFI_DEMO_ETH_HDR_LEN))
    {
        pbuf_free(packet);
        return;
    }

    payload_len = (u16) (packet->tot_len - WIFI_DEMO_ETH_HDR_LEN);
    eapol = os_malloc(payload_len);
    if (eapol != NULL)
    {
        copied = pbuf_copy_partial(packet, eapol, payload_len,
                                   WIFI_DEMO_ETH_HDR_LEN);
        if (copied == payload_len)
        {
            int status;

#if WIFI_DEMO_EAPOL_VERBOSE
            wifi_demo_trace_eapol_key(eapol, payload_len);
#endif
            status = wifi_security_manager_rx_eapol(
                &s_wifi_demo.security, eth.src, eapol, payload_len);
            if (status < 0)
            {
                rt_kprintf("[wifi-demo] EAPOL processing failed: %d\n", status);
            }
        }
        os_free(eapol);
    }

    pbuf_free(packet);
}

static void wifi_demo_free_driver_event(ra6wx_drv_msg_buf_t *msg)
{
    if (msg == NULL)
    {
        return;
    }

    if (msg->data != NULL)
    {
        vPortFree(msg->data);
        msg->data = NULL;
    }

    vPortFree(msg);
}

static bool wifi_demo_event_matches(ra6wx_drv_msg_buf_t *msg)
{
    if ((msg == NULL) || (msg->data == NULL))
    {
        return true;
    }

    /* The raw scan-done helper is called with only the registered device in
     * this archive, so its event attribute does not reliably carry if_index.
     * There is only one scan interface in this standalone demo; identify the
     * terminal event by command and avoid dropping a valid completion. */
    if ((msg->cmd == FC80211_CMD_NEW_SCAN_RESULTS) ||
        (msg->cmd == FC80211_CMD_SCAN_ABORTED) ||
        (msg->cmd == FC80211_CMD_CONNECT) ||
        (msg->cmd == FC80211_CMD_DISCONNECT))
    {
        return true;
    }

    return msg->data->attr.softmac_idx == (u16) WIFI_DEMO_IF_INDEX;
}

static void wifi_demo_handle_driver_event(ra6wx_drv_msg_buf_t *msg)
{
    if ((msg == NULL) || !wifi_demo_event_matches(msg))
    {
        return;
    }

    if (msg->cmd == FC80211_CMD_NEW_SCAN_RESULTS)
    {
        s_wifi_demo.scan_done = true;
    }
    else if (msg->cmd == FC80211_CMD_SCAN_ABORTED)
    {
        s_wifi_demo.scan_aborted = true;
        s_wifi_demo.scan_done = true;
    }
    else if (msg->cmd == FC80211_CMD_CONNECT)
    {
        int status = 0;
        int attr_status = -1;
        int response_status = -1;

        if (msg->data != NULL)
        {
            attr_status = (int) msg->data->attr.status;
            status = attr_status;
            s_connect_req_rsn_ie_len = 0U;

            /* CONNECT events normally contain this response object.  The
             * attribute status is retained as a fallback for older firmware
             * builds that only fill the event header. */
            if (msg->data->data_len >=
                (int) offsetof(struct cfg80211_connect_resp_params, payload))
            {
                struct cfg80211_connect_resp_params *response =
                    (struct cfg80211_connect_resp_params *) msg->data->data;
                size_t payload_len = (size_t) msg->data->data_len -
                    offsetof(struct cfg80211_connect_resp_params, payload);
                size_t req_ie_len = 0U;
                size_t resp_ie_len = 0U;
                const u8 *resp_ie = NULL;

                response_status = response->status;
                status = response_status;
                memcpy(s_wifi_demo.connect_bssid, response->bssid,
                       sizeof(s_wifi_demo.connect_bssid));

                /* The event is copied as a raw cfg80211 payload.  Pointer
                 * members in cfg80211_connect_resp_params are not valid in
                 * this address space; reconstruct the two IE ranges from
                 * the flexible payload exactly as driver_fc80211.c does. */
                if (response->req_ie_len <= (unsigned long) payload_len)
                {
                    req_ie_len = (size_t) response->req_ie_len;
                    if (response->resp_ie_len <=
                        (unsigned long) (payload_len - req_ie_len))
                    {
                        resp_ie_len = (size_t) response->resp_ie_len;
                        resp_ie = (const u8 *) response->payload + req_ie_len;
                        if ((response_status == WIFI_DEMO_STATUS_SUCCESS) &&
                            (resp_ie_len != 0U))
                        {
                            (void) wifi_demo_copy_rsn_ie(resp_ie,
                                                          resp_ie_len);
                        }
                    }

                    /* EAPOL-Key message 2/4 must carry the RSN IE that this
                     * station actually put into the (Re)Association Request.
                     * Recover it from req_ie instead of relying on a locally
                     * generated look-alike, which some APs reject. */
                    s_connect_req_rsn_ie_len = 0U;
                    if ((req_ie_len != 0U) &&
                        (response_status == WIFI_DEMO_STATUS_SUCCESS))
                    {
                        s_connect_req_rsn_ie_len = wifi_demo_find_rsn_ie(
                            (const u8 *) response->payload, req_ie_len,
                            s_connect_req_rsn_ie,
                            sizeof(s_connect_req_rsn_ie));
                    }
                }
            }

            s_wifi_demo.connect_timed_out =
                (msg->data->attr.timed_out != 0U);
        }

        s_wifi_demo.connect_status = status;
        s_wifi_demo.connected = false;
        if (status == WIFI_DEMO_STATUS_SUCCESS)
        {
            /* A successful response normally carries the selected BSSID.  A
             * few firmware revisions leave it empty, so retain the explicit
             * target or the driver's association context as a fallback. */
            if (wifi_demo_bssid_is_zero(s_wifi_demo.connect_bssid))
            {
                if (s_connect_target_bssid_set)
                {
                    memcpy(s_wifi_demo.connect_bssid,
                           s_connect_target_bssid,
                           sizeof(s_wifi_demo.connect_bssid));
                }
                else if ((s_connect_driver != NULL) &&
                         !wifi_demo_bssid_is_zero(s_connect_driver->bssid))
                {
                    memcpy(s_wifi_demo.connect_bssid,
                           s_connect_driver->bssid,
                           sizeof(s_wifi_demo.connect_bssid));
                }
            }

            memcpy(s_wifi_demo.connected_ssid, s_connect_ssid,
                   sizeof(s_wifi_demo.connected_ssid));
            s_wifi_demo.connected_ssid[WIFI_DEMO_MAX_SSID_LEN] = '\0';
            s_wifi_demo.connected_freq = s_connect_target_freq;

            /* The standalone event pump consumes the event before the
             * supplicant helper can update this context.  Keep the public
             * driver state coherent for later get_station/status calls. */
            if (s_connect_driver != NULL)
            {
                s_connect_driver->associated = 1;
                if (!wifi_demo_bssid_is_zero(s_wifi_demo.connect_bssid))
                {
                    memcpy(s_connect_driver->bssid,
                           s_wifi_demo.connect_bssid,
                           sizeof(s_connect_driver->bssid));
                }
                memcpy(s_connect_driver->ssid, s_connect_ssid,
                       s_connect_assoc.ssid_len >
                       sizeof(s_connect_driver->ssid) ?
                       sizeof(s_connect_driver->ssid) :
                       s_connect_assoc.ssid_len);
                s_connect_driver->ssid_len = s_connect_assoc.ssid_len;
                s_connect_driver->assoc_freq = s_connect_target_freq;
            }

            if (s_connect_host_wpa && (s_wifi_demo.wpa_sm != NULL))
            {
                s_wifi_demo.wpa_state = WPA_ASSOCIATED;
                s_wifi_demo.wpa_key_done = false;
                if (s_connect_req_rsn_ie_len != 0U)
                {
                    int ie_status = wifi_security_manager_set_assoc_ie(
                        &s_wifi_demo.security, s_connect_req_rsn_ie,
                        s_connect_req_rsn_ie_len);

                    if (ie_status != RT_EOK)
                    {
                        rt_kprintf("[wifi-demo] association IE setup failed: %d\n",
                                   ie_status);
                    }
                }
                if (wifi_security_manager_notify_assoc(
                        &s_wifi_demo.security, s_wifi_demo.connect_bssid,
                        s_connect_ap_rsn_ie_len != 0U ? s_connect_ap_rsn_ie : NULL,
                        s_connect_ap_rsn_ie_len) != RT_EOK)
                {
                    rt_kprintf("[wifi-demo] security manager association setup failed\n");
                    s_wifi_demo.connect_status = -RT_ERROR;
                    s_wifi_demo.connect_done = true;
                    return;
                }
            }
            else
            {
                s_wifi_demo.connected = true;
            }
        }
        else if (s_connect_driver != NULL)
        {
            s_connect_driver->associated = 0;
        }

        if (status != WIFI_DEMO_STATUS_SUCCESS)
        {
            wifi_security_manager_notify_disassoc(&s_wifi_demo.security);
            s_wifi_demo.wpa_state = WPA_DISCONNECTED;
            s_wifi_demo.wpa_key_done = false;
        }
        if (status == WIFI_DEMO_STATUS_UNSPECIFIED_FAILURE)
        {
            rt_kprintf("[wifi-demo] status 1: unspecified failure; verify "
                       "that the BSSID and frequency belong to this AP\n");
        }
        else if (status == WIFI_DEMO_STATUS_NOT_SUPPORTED_AUTH_ALG)
        {
            rt_kprintf("[wifi-demo] status 13: AP rejected the "
                       "802.11 authentication algorithm\n");
        }
        else if (status == WIFI_DEMO_STATUS_AUTH_TIMEOUT)
        {
            rt_kprintf("[wifi-demo] status 16: authentication timed out; "
                       "check channel/BSSID and AP response\n");
        }
        if (s_wifi_demo.connect_active &&
            ((status != WIFI_DEMO_STATUS_SUCCESS) || !s_connect_host_wpa))
        {
            s_wifi_demo.connect_done = true;
        }
    }
    else if (msg->cmd == FC80211_CMD_DISCONNECT)
    {
        wifi_security_manager_notify_disassoc(&s_wifi_demo.security);
        s_wifi_demo.wpa_state = WPA_DISCONNECTED;
        s_wifi_demo.wpa_key_done = false;
        s_wifi_demo.connected = false;
        s_wifi_demo.connected_ssid[0] = '\0';
        s_wifi_demo.connected_freq = 0U;
        memset(s_wifi_demo.connect_bssid, 0,
               sizeof(s_wifi_demo.connect_bssid));
        if (s_connect_driver != NULL)
        {
            s_connect_driver->associated = 0;
            memset(s_connect_driver->bssid, 0,
                   sizeof(s_connect_driver->bssid));
        }
        if (s_wifi_demo.connect_active)
        {
            s_wifi_demo.connect_status = -RT_ERROR;
            s_wifi_demo.connect_done = true;
        }
        s_wifi_demo.disconnect_done = true;
    }
}

static void wifi_demo_pump_events(void)
{
    if (TO_SUPP_QUEUE == NULL)
    {
        return;
    }

#ifdef CONFIG_SUPP_EVENT_DEPRECATED
    {
        ULONG queue_item[2];

        while (xQueueReceive(TO_SUPP_QUEUE, queue_item, 0) == pdTRUE)
        {
            if ((queue_item[0] == RA6WX_SP_DRV_FLAG) &&
                (queue_item[1] != 0U))
            {
                ra6wx_drv_msg_buf_t *msg =
                    (ra6wx_drv_msg_buf_t *)(uintptr_t) queue_item[1];

                wifi_demo_handle_driver_event(msg);

                wifi_demo_free_driver_event(msg);
            }
            else if ((queue_item[0] == RA6WX_L2_PKT_RX_EV) &&
                     (queue_item[1] != 0U))
            {
                wifi_demo_handle_l2_packet(
                    (struct pbuf *) (uintptr_t) queue_item[1]);
            }
        }
    }
#else
    {
        ULONG queue_item[2];

        /* The current ABI queues a pointer to a two-word event packet. */
        while (xQueueReceive(TO_SUPP_QUEUE, queue_item, 0) == pdTRUE)
        {
            ULONG *packet = (ULONG *)(uintptr_t) queue_item[0];

            if (packet == NULL)
            {
                continue;
            }

            if ((packet[0] == RA6WX_SP_DRV_FLAG) && (packet[1] != 0U))
            {
                ra6wx_drv_msg_buf_t *msg =
                    (ra6wx_drv_msg_buf_t *)(uintptr_t) packet;

                wifi_demo_handle_driver_event(msg);

                /* In this ABI packet[1] aliases the queue message itself;
                 * wifi_demo_free_driver_event() releases both the payload and
                 * that message buffer. */
                wifi_demo_free_driver_event(msg);
            }
            else if ((packet[0] == RA6WX_L2_PKT_RX_EV) &&
                     (packet[1] != 0U))
            {
                wifi_demo_handle_l2_packet(
                    (struct pbuf *) (uintptr_t) packet[1]);
                vPortFree(packet);
            }
            else
            {
                vPortFree(packet);
            }
        }
    }
#endif
}

/* Bring the host and firmware back to a clean station state. A failed WPA
 * exchange can leave EAPOL packets in TO_SUPP_QUEUE and the driver context
 * marked associated; starting another association without clearing both is
 * what caused repeated-connect queue/assertion failures. */
static int wifi_demo_disconnect_internal(u16 reason_code)
{
    unsigned int waited_ms;
    int request_status = 0;

    if (s_wifi_demo.disconnect_in_progress)
    {
        return -RT_EBUSY;
    }

    s_wifi_demo.disconnect_in_progress = true;
    s_wifi_demo.disconnect_done = false;

    /* Stop WPA first so late EAPOL packets are discarded by the pump. */
    wifi_demo_wpa_deinit();
    wifi_demo_pump_events();

    if ((s_connect_driver != NULL) &&
        ((s_connect_driver->associated != 0) ||
         !wifi_demo_bssid_is_zero(s_connect_driver->bssid) ||
         s_wifi_demo.connected))
    {
        (void) wpa_driver_fc80211_set_supp_port(s_connect_bss, 0);
        request_status = rwnx_cfg80211_disconnect(
            (u8) WIFI_DEMO_IF_INDEX, FC80211_IFTYPE_STATION, reason_code);

        if (request_status == 0)
        {
            for (waited_ms = 0U;
                 waited_ms < WIFI_DEMO_DISCONNECT_WAIT_MS;
                 waited_ms += WIFI_DEMO_POLL_MS)
            {
                wifi_demo_pump_events();
                if (s_wifi_demo.disconnect_done)
                {
                    break;
                }
                rt_thread_mdelay(WIFI_DEMO_POLL_MS);
            }
        }
    }

    /* Do not retain driver-side association data if firmware omitted the
     * asynchronous disconnect event. */
    if (s_connect_driver != NULL)
    {
        fc80211_mark_disconnected(s_connect_driver);
    }
    wifi_demo_pump_events();

    s_wifi_demo.connected = false;
    s_wifi_demo.connect_active = false;
    s_wifi_demo.connect_done = false;
    s_wifi_demo.connect_timed_out = false;
    s_wifi_demo.connect_status = -RT_ERROR;
    s_wifi_demo.connected_ssid[0] = '\0';
    s_wifi_demo.connected_freq = 0U;
    memset(s_wifi_demo.connect_bssid, 0,
           sizeof(s_wifi_demo.connect_bssid));
    s_connect_ap_rsn_ie_len = 0U;
    s_connect_req_rsn_ie_len = 0U;
    s_connect_target_bssid_set = false;
    memset(s_connect_target_bssid, 0, sizeof(s_connect_target_bssid));
    s_connect_target_freq = 0U;
    s_connect_host_wpa = false;
    s_wifi_demo.disconnect_done = true;
    s_wifi_demo.disconnect_in_progress = false;

    return request_status;
}

static int wifi_demo_wait_wiphy(void)
{
    unsigned int waited_ms;

    for (waited_ms = 0U; waited_ms < WIFI_DEMO_WIPHY_WAIT_MS;
         waited_ms += WIFI_DEMO_POLL_MS)
    {
        if ((rwnx_wiphy.rdev != NULL) &&
            ((rwnx_wiphy.bands[FC80211_BAND_2GHZ] != NULL) ||
             (rwnx_wiphy.bands[FC80211_BAND_5GHZ] != NULL)))
        {
            return RT_EOK;
        }

        rt_thread_mdelay(WIFI_DEMO_POLL_MS);
    }

    rt_kprintf("[wifi-demo] wiphy timeout: rdev=%p band2=%p band5=%p\n",
               rwnx_wiphy.rdev,
               rwnx_wiphy.bands[FC80211_BAND_2GHZ],
               rwnx_wiphy.bands[FC80211_BAND_5GHZ]);
    return -RT_ETIMEOUT;
}

static unsigned int wifi_demo_count_band_channels(unsigned int band_index)
{
    unsigned int count = 0U;

    struct ieee80211_supported_band *band = rwnx_wiphy.bands[band_index];
    int channel_index;

    if ((band == NULL) || (band->channels == NULL))
    {
        return 0U;
    }

    for (channel_index = 0; channel_index < band->n_channels;
         channel_index++)
    {
        if ((band->channels[channel_index].flags &
             WIFI_DEMO_CHAN_DISABLED) == 0U)
        {
            count++;
        }
    }

    if (count > WIFI_DEMO_MAX_CHANNELS)
    {
        count = WIFI_DEMO_MAX_CHANNELS;
    }

    return count;
}

static unsigned int wifi_demo_count_channels(void)
{
    unsigned int count = 0U;

    count += wifi_demo_count_band_channels(FC80211_BAND_2GHZ);
    count += wifi_demo_count_band_channels(FC80211_BAND_5GHZ);
    if (count > WIFI_DEMO_MAX_CHANNELS)
    {
        count = WIFI_DEMO_MAX_CHANNELS;
    }

    return count;
}

static unsigned int wifi_demo_fill_frequencies(int *freqs,
                                                unsigned int max_channels)
{
    unsigned int count = 0U;
    unsigned int band_index;

    for (band_index = FC80211_BAND_2GHZ;
         band_index <= FC80211_BAND_5GHZ;
         band_index++)
    {
        struct ieee80211_supported_band *band =
            rwnx_wiphy.bands[band_index];
        int channel_index;

        if ((band == NULL) || (band->channels == NULL))
        {
            continue;
        }

        for (channel_index = 0; channel_index < band->n_channels;
             channel_index++)
        {
            struct ieee80211_channel *channel = &band->channels[channel_index];

            if ((channel->flags & WIFI_DEMO_CHAN_DISABLED) != 0U)
            {
                continue;
            }

            if (count == max_channels)
            {
                freqs[count] = 0;
                return count;
            }

            freqs[count++] = (int) channel->center_freq;
        }
    }

    freqs[count] = 0;
    return count;
}

static void wifi_demo_print_ssid(const u8 *ies, size_t ie_len)
{
    size_t offset = 0U;
    bool printed = false;

    while ((ies != NULL) && (offset + 2U <= ie_len))
    {
        u8 element_id = ies[offset];
        u8 element_len = ies[offset + 1U];

        offset += 2U;
        if ((size_t) element_len > ie_len - offset)
        {
            break;
        }

        if (element_id == 0U)
        {
            size_t index;

            for (index = 0U; index < element_len; index++)
            {
                u8 value = ies[offset + index];

                if ((value >= 0x20U) && (value <= 0x7eU))
                {
                    rt_kprintf("%c", value);
                }
                else
                {
                    rt_kprintf(".");
                }
            }

            printed = (element_len != 0U);
            break;
        }

        offset += element_len;
    }

    if (!printed)
    {
        rt_kprintf("<hidden>");
    }
}

static void wifi_demo_free_scan_results(struct wifi_demo_scan_results *results)
{
    size_t index;

    if (results == NULL)
    {
        return;
    }

    for (index = 0U; index < results->num; index++)
    {
        if (results->res != NULL)
        {
            vPortFree(results->res[index]);
        }
    }

    vPortFree(results->res);
    results->res = NULL;
    results->num = 0U;
}

static int wifi_demo_start(void)
{
    BaseType_t task_status;
    unsigned int retry;
    int status;

    if (s_wifi_demo.hardware_ready)
    {
        return RT_EOK;
    }

    status = wifi_hw_platform_prepare();
    if (status != RT_EOK)
    {
        rt_kprintf("[wifi-demo] platform prepare failed: %d\n", status);
        return status;
    }

    if (!s_wifi_demo.event_queue_ready)
    {
        status = ra6wx_eloop_init();
        if (status != 0)
        {
            rt_kprintf("[wifi-demo] event queue init failed: %d\n", status);
            return -RT_ERROR;
        }

        fc80211_enable_drv_event();
        s_wifi_demo.event_queue_ready = true;
    }

    romac4rtos_initialize(true, dg_configPTIMG_HDR_ADDR);
    romac4rtos_config(0, 0);

    /* The scan wrapper filters channels against the regulatory table. */
    {
        char country[] = COUNTRY_CODE_DEFAULT;
        ra6w1_regdb_data_init(country);
    }

    task_status = rwnx_mac_task_initiailize();
    if (task_status != pdPASS)
    {
        rt_kprintf("[wifi-demo] MAC task init failed: %d\n",
                   (int) task_status);
        return -RT_ERROR;
    }

    task_status = rwnx_driver_task_initiailize();
    if (task_status != pdPASS)
    {
        rt_kprintf("[wifi-demo] driver task init failed: %d\n",
                   (int) task_status);
        return -RT_ERROR;
    }

    /* Public cfg80211 operations wait on this vendor-created response queue.
     * The original supplicant startup performs this step indirectly; the
     * standalone demo must initialize it explicitly. */
    if (CFG80211_semaphore == NULL)
    {
        rwnx_cfg80211_initialize();
    }
    if (CFG80211_semaphore == NULL)
    {
        rt_kprintf("[wifi-demo] cfg80211 response queue init failed\n");
        return -RT_ERROR;
    }
    s_wifi_demo.cfg80211_ready = true;

    for (retry = 0U; retry < WIFI_DEMO_STATUS_RETRY; retry++)
    {
        if (rwnx_hw_get_status() == WIFI_DEMO_STATUS_ACTIVE)
        {
            break;
        }

        vTaskDelay(1);
    }

    if (retry == WIFI_DEMO_STATUS_RETRY)
    {
        rt_kprintf("[wifi-demo] driver status=%d, timeout\n",
                   rwnx_hw_get_status());
        return -RT_ETIMEOUT;
    }

    status = wifi_demo_wait_wiphy();
    if (status != RT_EOK)
    {
        return status;
    }

    /* Apply the regulatory channel table already provided by the driver. */
    fc80211_update_channel_info();

    s_wifi_demo.wdev = fc80211_wdev_from_if_idx(WIFI_DEMO_IF_INDEX);
    if (s_wifi_demo.wdev == NULL)
    {
        s_wifi_demo.wdev = rwnx_cfg80211_add_iface(
            WIFI_DEMO_IF_INDEX, WIFI_DEMO_IF_NAME, 0U,
            FC80211_IFTYPE_STATION, NULL);
    }

    if ((s_wifi_demo.wdev == NULL) ||
        (s_wifi_demo.wdev->wiphy != &rwnx_wiphy) ||
        (s_wifi_demo.wdev->if_index != WIFI_DEMO_IF_INDEX))
    {
        rt_kprintf("[wifi-demo] station interface creation failed\n");
        s_wifi_demo.wdev = NULL;
        return -RT_ERROR;
    }

    s_wifi_demo.iface_ready = true;
    s_wifi_demo.hardware_ready = true;
    /* rwnx_rx_handle_eap_packet() uses the SDK's readiness flag as a guard
     * before it queues EAPOL frames.  Set only that transport gate; no
     * wpa_supplicant interface or upper-layer task is started here. */
    supplicant_done();
    rt_kprintf("[wifi-demo] ready: %s (%u channels)\n",
               WIFI_DEMO_IF_NAME, wifi_demo_count_channels());
    return RT_EOK;
}

/* Run one full scan and leave the raw results in *results.  The caller owns
 * them and must release them with wifi_demo_free_scan_results() followed by
 * fc80211_free_umac_scan_results(). */
static int wifi_demo_scan_collect(struct wifi_demo_scan_results *results)
{
    unsigned int channel_count;
    unsigned int waited_ms;
    int frequencies[WIFI_DEMO_MAX_CHANNELS + 1U];
    struct wpa_driver_scan_params scan_params;
    struct wpa_driver_fc80211_data driver_context;
    int status;

    if (results == NULL)
    {
        return -RT_EINVAL;
    }

    if (s_wifi_demo.scan_active)
    {
        rt_kprintf("[wifi-demo] scan is already active\n");
        return -RT_EBUSY;
    }

    channel_count = wifi_demo_count_channels();
    if (channel_count == 0U)
    {
        rt_kprintf("[wifi-demo] no enabled channels\n");
        return -RT_ERROR;
    }

    memset(&scan_params, 0, sizeof(scan_params));
    memset(&driver_context, 0, sizeof(driver_context));
    memset(results, 0, sizeof(*results));
    channel_count = wifi_demo_fill_frequencies(frequencies, channel_count);

    /* The vendor wrapper converts the supplicant scan ABI into the firmware
     * request and owns the converted request lifetime. */
    scan_params.ssids[0].ssid = NULL;
    scan_params.ssids[0].ssid_len = 0U;
    scan_params.num_ssids = 1U;
    scan_params.freqs = frequencies;
    driver_context.ifindex = (int) WIFI_DEMO_IF_INDEX;
    driver_context.nlmode = FC80211_IFTYPE_STATION;

    /* Discard a completion left by a previous request before arming this
     * request's state.  Otherwise a late event can complete the new scan. */
    wifi_demo_pump_events();
    s_wifi_demo.scan_active = true;
    s_wifi_demo.scan_done = false;
    s_wifi_demo.scan_aborted = false;

    status = rsdev_cfg80211_scan((unsigned long) WIFI_DEMO_IF_INDEX,
                                  &driver_context, &scan_params);
    if (status != 0)
    {
        s_wifi_demo.scan_active = false;
        rt_kprintf("[wifi-demo] scan request failed: %d\n", status);
        return status;
    }

    for (waited_ms = 0U; waited_ms < WIFI_DEMO_SCAN_WAIT_MS;
         waited_ms += WIFI_DEMO_POLL_MS)
    {
        wifi_demo_pump_events();
        if (s_wifi_demo.scan_done)
        {
            break;
        }

        rt_thread_mdelay(WIFI_DEMO_POLL_MS);
    }

    if (!s_wifi_demo.scan_done)
    {
        status = fc80211_abort_scan(0UL, WIFI_DEMO_IF_INDEX);
        rt_kprintf("[wifi-demo] scan timeout, abort=%d\n", status);

        /* The request can be cleared by the driver just before the terminal
         * event is queued.  Give that event a bounded window to arrive. */
        for (waited_ms = 0U; waited_ms < WIFI_DEMO_SCAN_SETTLE_MS;
             waited_ms += WIFI_DEMO_POLL_MS)
        {
            wifi_demo_pump_events();
            if (s_wifi_demo.scan_done)
            {
                break;
            }

            rt_thread_mdelay(WIFI_DEMO_POLL_MS);
        }

        wifi_demo_pump_events();
        if (!s_wifi_demo.scan_done)
        {
            s_wifi_demo.scan_active = false;
            return -RT_ETIMEOUT;
        }
    }

    s_wifi_demo.scan_active = false;
    if (s_wifi_demo.scan_aborted)
    {
        rt_kprintf("[wifi-demo] scan aborted\n");
        return -RT_ERROR;
    }

    /* The first argument is unused by the archive implementation for this
     * target; interface index is resolved through fc80211_wdev_from_if_idx(). */
    {
        struct
        {
            void *drv;
            struct wifi_demo_scan_results *res;
            unsigned int assoc_freq;
            unsigned int ibss_freq;
            u8 assoc_bssid[ETH_ALEN];
        } scan_arg;

        scan_arg.drv = &driver_context;
        scan_arg.res = results;
        scan_arg.assoc_freq = 0U;
        scan_arg.ibss_freq = 0U;
        memset(scan_arg.assoc_bssid, 0, sizeof(scan_arg.assoc_bssid));
        status = fc80211_get_scan(0UL, WIFI_DEMO_IF_INDEX, &scan_arg);
    }

    if (status != 0)
    {
        rt_kprintf("[wifi-demo] get scan results failed: %d\n", status);
        wifi_demo_free_scan_results(results);
        (void) fc80211_free_umac_scan_results(0UL, WIFI_DEMO_IF_INDEX);
        return status;
    }

    rt_kprintf("[wifi-demo] scan complete: %u AP(s)\n",
               (unsigned int) results->num);
    return RT_EOK;
}

static int wifi_low_scan(int argc, char **argv)
{
    struct wifi_demo_scan_results results;
    int status;

    (void) argv;

    if (argc != 1)
    {
        rt_kprintf("usage: wifi_low_scan\n");
        return -RT_EINVAL;
    }

    status = wifi_demo_start();
    if (status != RT_EOK)
    {
        return status;
    }

    status = wifi_demo_scan_collect(&results);
    if (status != RT_EOK)
    {
        return status;
    }

    rt_kprintf("No  BSSID              Freq   RSSI   SSID\n");
    for (size_t index = 0U; index < results.num; index++)
    {
        struct wifi_demo_scan_res *result = results.res[index];

        if (result == NULL)
        {
            continue;
        }

        rt_kprintf("%-3u %02X:%02X:%02X:%02X:%02X:%02X %-6d %-6d ",
                   (unsigned int) index,
                   result->bssid[0], result->bssid[1], result->bssid[2],
                   result->bssid[3], result->bssid[4], result->bssid[5],
                   result->freq, result->level);
        wifi_demo_print_ssid(result->ie, result->ie_len);
        rt_kprintf("\n");
    }

    wifi_demo_free_scan_results(&results);
    (void) fc80211_free_umac_scan_results(0UL, WIFI_DEMO_IF_INDEX);
    return RT_EOK;
}

static bool wifi_demo_ie_ssid_equal(const u8 *ies, size_t ie_len,
                                    const char *ssid)
{
    size_t ssid_len;
    size_t offset = 0U;

    if ((ies == NULL) || (ssid == NULL))
    {
        return false;
    }

    ssid_len = strlen(ssid);
    while (offset + 2U <= ie_len)
    {
        size_t element_len = ies[offset + 1U];
        size_t total_len = element_len + 2U;

        if (total_len > ie_len - offset)
        {
            break;
        }

        if (ies[offset] == 0U)
        {
            return (element_len == ssid_len) &&
                   (memcmp(ies + offset + 2U, ssid, ssid_len) == 0);
        }

        offset += total_len;
    }

    return false;
}

/* A phone hotspot is free to pick a new BSSID (and channel) every time it is
 * switched on, and the firmware's "auto" connect only works when it already
 * knows a BSS for the SSID.  Resolve the SSID with a scan so the connect
 * always targets the BSS that is on the air right now. */
static int wifi_demo_lookup_bss(const char *ssid, unsigned int *freq,
                                u8 *bssid)
{
    struct wifi_demo_scan_results results;
    struct wifi_demo_scan_res *best = NULL;
    size_t index;
    int status;

    if ((ssid == NULL) || (freq == NULL) || (bssid == NULL))
    {
        return -RT_EINVAL;
    }

    status = wifi_demo_scan_collect(&results);
    if (status != RT_EOK)
    {
        return status;
    }

    for (index = 0U; index < results.num; index++)
    {
        struct wifi_demo_scan_res *result = results.res[index];

        if (result == NULL)
        {
            continue;
        }

        if (!wifi_demo_ie_ssid_equal(result->ie, result->ie_len, ssid))
        {
            continue;
        }

        if ((best == NULL) || (result->level > best->level))
        {
            best = result;
        }
    }

    if (best == NULL)
    {
        wifi_demo_free_scan_results(&results);
        (void) fc80211_free_umac_scan_results(0UL, WIFI_DEMO_IF_INDEX);
        rt_kprintf("[wifi-demo] scan found no BSS for SSID \"%s\"\n",
                   ssid);
        return -RT_ERROR;
    }

    memcpy(bssid, best->bssid, ETH_ALEN);
    *freq = (unsigned int) best->freq;
    wifi_demo_free_scan_results(&results);
    (void) fc80211_free_umac_scan_results(0UL, WIFI_DEMO_IF_INDEX);
    return RT_EOK;
}

/* Offsets that must match libsupplicant.a: struct i802_bss embeds
 * char ifname[IFNAMSIZ + 1], so a different IFNAMSIZ shifts addr/freq and
 * every direct access to those fields would read the wrong bytes.  The
 * library writes bss->addr at offset 34, which is what these values are
 * checked against at run time (see wifi_demo_crypto_selftest). */
#define WIFI_DEMO_LIB_BSS_ADDR_OFFSET (34U)

static int wifi_demo_hex_value(char value)
{
    if ((value >= '0') && (value <= '9'))
    {
        return value - '0';
    }

    if ((value >= 'a') && (value <= 'f'))
    {
        return value - 'a' + 10;
    }

    if ((value >= 'A') && (value <= 'F'))
    {
        return value - 'A' + 10;
    }

    return -1;
}

static int wifi_demo_parse_psk(const char *text, u8 *psk)
{
    size_t index;

    if ((text == NULL) || (psk == NULL) || (strlen(text) != 64U))
    {
        return -RT_EINVAL;
    }

    for (index = 0U; index < WIFI_DEMO_PSK_LEN; index++)
    {
        int high = wifi_demo_hex_value(text[index * 2U]);
        int low = wifi_demo_hex_value(text[index * 2U + 1U]);

        if ((high < 0) || (low < 0))
        {
            return -RT_EINVAL;
        }

        psk[index] = (u8) ((high << 4) | low);
    }

    return RT_EOK;
}

/* Verify the crypto primitives of the prebuilt supplicant against a vector
 * that was reconstructed off-line from a real capture of this AP:
 * SSID "xiaomi", password "12345678", AP 02:5C:AD:40:32:C7,
 * STA 7C:15:2D:00:B4:E0, and the ANonce/SNonce pair of one handshake.
 * The reference values come from an independent implementation of
 * PBKDF2-HMAC-SHA1, sha1_prf and HMAC-SHA1.  Any mismatch means the library
 * derives a different PTK/MIC than the AP does, so the 4-way handshake can
 * never complete and the AP keeps retransmitting message 1/4. */
#define WIFI_DEMO_CRYPTO_SELFTEST 0

#if WIFI_DEMO_CRYPTO_SELFTEST
extern int sha1_prf(const u8 *key, size_t key_len, const char *label,
                    const u8 *data, size_t data_len, u8 *buf, size_t buf_len);
extern int hmac_sha1(const u8 *key, size_t key_len, const u8 *data,
                     size_t data_len, u8 *mac);
extern int hmac_sha1_vector(const u8 *key, size_t key_len, size_t num_elem,
                            const u8 *addr[], const size_t *len, u8 *mac);

static void wifi_demo_selftest_dump(const char *tag, const u8 *value,
                                    size_t len)
{
    size_t index;

    rt_kprintf("[selftest] %s=", tag);
    for (index = 0U; index < len; index++)
    {
        rt_kprintf("%02x", value[index]);
    }
    rt_kprintf("\n");
}

static void wifi_demo_crypto_selftest(void)
{
    static const u8 pmk[WIFI_DEMO_PSK_LEN] = {
        0x28U, 0x0dU, 0xa9U, 0xd8U, 0xd4U, 0x49U, 0x90U, 0x54U,
        0x4bU, 0xbaU, 0xedU, 0x60U, 0xccU, 0x17U, 0xdcU, 0x26U,
        0xc9U, 0x7eU, 0xa9U, 0x2dU, 0xddU, 0xd9U, 0x63U, 0x67U,
        0x8bU, 0x77U, 0xffU, 0x93U, 0x31U, 0xbeU, 0x8aU, 0xfcU
    };
    static const u8 sta[ETH_ALEN] = {0x7cU, 0x15U, 0x2dU, 0x00U, 0xb4U, 0xe0U};
    static const u8 ap[ETH_ALEN] = {0x02U, 0x5cU, 0xadU, 0x40U, 0x32U, 0xc7U};
    static const u8 anonce[32] = {
        0x54U, 0xb8U, 0xb9U, 0xedU, 0x17U, 0x7cU, 0x42U, 0x8bU,
        0x4dU, 0xe8U, 0x24U, 0xa3U, 0x6bU, 0x12U, 0x88U, 0x13U,
        0x21U, 0xc4U, 0xc9U, 0x92U, 0x73U, 0xf4U, 0x22U, 0x8aU,
        0x26U, 0x5bU, 0xe3U, 0x54U, 0xf1U, 0x44U, 0x17U, 0x43U
    };
    static const u8 snonce[32] = {
        0x04U, 0x03U, 0xa2U, 0xcbU, 0x21U, 0x5cU, 0xccU, 0x8dU,
        0x82U, 0xd7U, 0x16U, 0xccU, 0xdcU, 0x2dU, 0x73U, 0xbbU,
        0xecU, 0xafU, 0x98U, 0x9dU, 0x3bU, 0xd8U, 0x72U, 0xd8U,
        0x0aU, 0x33U, 0xb4U, 0x4eU, 0xeeU, 0x3eU, 0xe4U, 0xfeU
    };
    static const u8 frame[121] = {
        0x02U, 0x03U, 0x00U, 0x75U, 0x02U, 0x01U, 0x0aU, 0x00U,
        0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
        0x01U, 0x04U, 0x03U, 0xa2U, 0xcbU, 0x21U, 0x5cU, 0xccU,
        0x8dU, 0x82U, 0xd7U, 0x16U, 0xccU, 0xdcU, 0x2dU, 0x73U,
        0xbbU, 0xecU, 0xafU, 0x98U, 0x9dU, 0x3bU, 0xd8U, 0x72U,
        0xd8U, 0x0aU, 0x33U, 0xb4U, 0x4eU, 0xeeU, 0x3eU, 0xe4U,
        0xfeU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
        0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
        0x00U, 0x00U, 0x00U, 0x00U, 0x80U, 0x00U, 0x00U, 0x00U,
        0x00U, 0x00U, 0x00U, 0x00U, 0xa8U, 0xa3U, 0x00U, 0x00U,
        0x58U, 0xdcU, 0xf1U, 0x1cU, 0xfaU, 0x0fU, 0x14U, 0x3aU,
        0x91U, 0x9aU, 0x52U, 0xf6U, 0x17U, 0x6eU, 0x24U, 0xa3U,
        0x2bU, 0x00U, 0x16U, 0x30U, 0x14U, 0x01U, 0x00U, 0x00U,
        0x0fU, 0xacU, 0x04U, 0x01U, 0x00U, 0x00U, 0x0fU, 0xacU,
        0x04U, 0x01U, 0x00U, 0x00U, 0x0fU, 0xacU, 0x02U, 0x00U,
        0x00U
    };
    /* Independent reference values (host implementation).
     * kck_ref is the KCK produced by the exact PRF the library implements:
     * HMAC-SHA1(pmk, "Pairwise key expansion" + NUL + data + counter) with the
     * counter starting at 0.  kck_alt is the same PRF without the NUL byte. */
    static const u8 kck_ref[16] = {
        0xb9U, 0x4fU, 0x03U, 0x85U, 0x94U, 0xc1U, 0x7dU, 0x79U,
        0x28U, 0x7dU, 0x15U, 0x11U, 0xb1U, 0x35U, 0x6fU, 0xd6U
    };
    static const u8 kck_alt[16] = {
        0x85U, 0x53U, 0xa1U, 0x02U, 0xa1U, 0xa1U, 0x03U, 0xd7U,
        0x52U, 0xceU, 0x09U, 0x21U, 0xc5U, 0xb0U, 0x6bU, 0x54U
    };
    static const u8 mic_ref[16] = {
        0x3dU, 0xcfU, 0x19U, 0x2cU, 0xf7U, 0xaeU, 0x5bU, 0x43U,
        0xf9U, 0xb5U, 0xb6U, 0x26U, 0x16U, 0xf4U, 0x1bU, 0x57U
    };
    static const u8 mic_alt[16] = {
        0x4aU, 0x04U, 0x85U, 0xb9U, 0x38U, 0x15U, 0x86U, 0x56U,
        0xc3U, 0x51U, 0x43U, 0x7bU, 0x99U, 0x0dU, 0x7dU, 0x51U
    };
    /* The frame on the air carried this MIC, which matches neither of the
     * values above: if the reference is reproduced below, the library used a
     * different KCK (different PTK inputs) to compute it. */
    static const u8 mic_on_air[16] = {
        0xdcU, 0xf1U, 0x1cU, 0xfaU, 0x0fU, 0x14U, 0x3aU, 0x91U,
        0x9aU, 0x52U, 0xf6U, 0x17U, 0x6eU, 0x24U, 0xa3U, 0x2bU
    };
    static const char s_prf_label[] = "Pairwise key expansion";
    u8 data[2U * ETH_ALEN + 2U * 32U];
    u8 prf_out[64];
    u8 mic[32];
    u8 vec[128];
    u8 mac_multi[20];
    u8 mac_single[20];
    const u8 *prf_addr[3];
    size_t prf_len[3];
    struct wpa_ptk ptk;
    u8 buf[121];
    const u8 *first;
    const u8 *second;
    u8 counter = 0U;

    /* PTK derivation input: Min(AA,SPA) || Max(AA,SPA) ||
     * Min(ANonce,SNonce) || Max(ANonce,SNonce). */
    if (memcmp(sta, ap, ETH_ALEN) < 0)
    {
        memcpy(data, sta, ETH_ALEN);
        memcpy(data + ETH_ALEN, ap, ETH_ALEN);
    }
    else
    {
        memcpy(data, ap, ETH_ALEN);
        memcpy(data + ETH_ALEN, sta, ETH_ALEN);
    }
    if (memcmp(anonce, snonce, 32U) < 0)
    {
        first = anonce;
        second = snonce;
    }
    else
    {
        first = snonce;
        second = anonce;
    }
    memcpy(data + 2U * ETH_ALEN, first, 32U);
    memcpy(data + 2U * ETH_ALEN + 32U, second, 32U);

    rt_kprintf("[selftest] running crypto vector checks\n");
    rt_kprintf("[selftest] ABI: IFNAMSIZ=%u i802_bss.addr@%u (library %u) "
               "sizeof(i802_bss)=%u assoc_params=%u\n",
               (unsigned int) IFNAMSIZ,
               (unsigned int) offsetof(struct i802_bss, addr),
               (unsigned int) WIFI_DEMO_LIB_BSS_ADDR_OFFSET,
               (unsigned int) sizeof(struct i802_bss),
               (unsigned int) sizeof(struct wpa_driver_associate_params));

    if (sha1_prf(pmk, sizeof(pmk), s_prf_label, data,
                 sizeof(data), prf_out, sizeof(prf_out)) != 0)
    {
        rt_kprintf("[selftest] sha1_prf failed\n");
    }
    else
    {
        wifi_demo_selftest_dump("sha1_prf  KCK", prf_out, 16U);
        rt_kprintf("[selftest] expect    KCK=%s (label without NUL: %s)\n",
                   "b94f038594c17d79287d1511b1356fd6",
                   "8553a102a1a103d752ce0921c5b06b54");
        rt_kprintf("[selftest] sha1_prf  %s\n",
                   (memcmp(prf_out, kck_ref, 16U) == 0) ? "OK" : "WRONG");
    }

    /* sha1_prf feeds three separate elements to hmac_sha1_vector(); the HSU
     * backend of hmac_sha1_vector() must join them exactly like the
     * single-buffer form.  Compare both paths against the same reference. */
    memcpy(vec, s_prf_label, sizeof(s_prf_label));   /* 23 bytes incl. NUL */
    memcpy(vec + sizeof(s_prf_label), data, sizeof(data));
    vec[sizeof(s_prf_label) + sizeof(data)] = counter;
    prf_addr[0] = (const u8 *) s_prf_label;
    prf_len[0] = sizeof(s_prf_label);
    prf_addr[1] = data;
    prf_len[1] = sizeof(data);
    prf_addr[2] = &counter;
    prf_len[2] = 1U;
    memset(mac_multi, 0, sizeof(mac_multi));
    memset(mac_single, 0, sizeof(mac_single));
    if (hmac_sha1_vector(pmk, sizeof(pmk), 3U, prf_addr, prf_len,
                         mac_multi) != 0)
    {
        rt_kprintf("[selftest] hmac_sha1_vector(3) failed\n");
    }
    if (hmac_sha1(pmk, sizeof(pmk), vec,
                  sizeof(s_prf_label) + sizeof(data) + 1U,
                  mac_single) != 0)
    {
        rt_kprintf("[selftest] hmac_sha1(1) failed\n");
    }
    wifi_demo_selftest_dump("hmac 3 elements", mac_multi, sizeof(mac_multi));
    wifi_demo_selftest_dump("hmac 1 element ", mac_single, sizeof(mac_single));
    rt_kprintf("[selftest] expect         %s\n",
               "b94f038594c17d79287d1511b1356fd634db3602");

    memset(&ptk, 0, sizeof(ptk));
    if (wpa_pmk_to_ptk(pmk, sizeof(pmk), s_prf_label,
                       sta, ap, snonce, anonce, &ptk,
                       (int) WPA_KEY_MGMT_PSK, (int) WPA_CIPHER_CCMP,
                       NULL, 0U, 0U) != 0)
    {
        rt_kprintf("[selftest] wpa_pmk_to_ptk failed\n");
    }
    else
    {
        wifi_demo_selftest_dump("pmk_to_ptk KCK", ptk.kck, ptk.kck_len);
        wifi_demo_selftest_dump("pmk_to_ptk KEK", ptk.kek, ptk.kek_len);
        wifi_demo_selftest_dump("pmk_to_ptk TK ", ptk.tk, ptk.tk_len);
        rt_kprintf("[selftest] pmk_to_ptk %s (kck_len=%u tk_len=%u)\n",
                   (memcmp(ptk.kck, kck_ref, 16U) == 0) ? "OK" : "WRONG",
                   (unsigned int) ptk.kck_len, (unsigned int) ptk.tk_len);
    }

    memcpy(buf, frame, sizeof(buf));
    memset(buf + 81U, 0, 16U);
    if (wpa_eapol_key_mic(kck_ref, sizeof(kck_ref),
                          (int) WPA_KEY_MGMT_PSK, 2, buf, sizeof(buf),
                          mic) != 0)
    {
        rt_kprintf("[selftest] wpa_eapol_key_mic failed\n");
    }
    else
    {
        wifi_demo_selftest_dump("eapol_key_mic", mic, 16U);
        rt_kprintf("[selftest] expect        %s (true KCK)\n",
                   "3dcf192cf7ae5b43f9b5b62616f41b57");
        rt_kprintf("[selftest] mic %s, KCK %s, on-air %s\n",
                   (memcmp(mic, mic_ref, 16U) == 0) ? "OK" : "WRONG",
                   "b94f038594c17d79287d1511b1356fd6",
                   "dcf11cfa0f143a919a52f6176e24a32b");
    }
    (void) mic_alt;
    (void) mic_on_air;
    (void) kck_alt;
}
#endif /* WIFI_DEMO_CRYPTO_SELFTEST */

static int wifi_demo_prepare_psk(const char *credential, const char *ssid,
                                 u8 *psk)
{
    size_t credential_len;
    u8 sdk_psk[WIFI_DEMO_PSK_LEN];
    int status;

    if ((credential == NULL) || (ssid == NULL) || (psk == NULL))
    {
        return -RT_EINVAL;
    }

    credential_len = strlen(credential);

    /* The CryptoCell runtime must be up before either the Mbed TLS PBKDF2 or
     * the supplicant's own HMAC/PTK derivation runs, so bring it up even when
     * the 64-digit raw PSK form skips PBKDF2. */
    if (!s_wifi_demo.crypto_ready)
    {
        status = wifi_demo_crypto_init();
        if (status == 0)
        {
            return -RT_ERROR;
        }

        s_wifi_demo.crypto_ready = true;
#if WIFI_DEMO_CRYPTO_SELFTEST
        wifi_demo_crypto_selftest();
#endif
    }

    if ((credential_len == 64U) &&
        (wifi_demo_parse_psk(credential, psk) == RT_EOK))
    {
        rt_kprintf("[wifi-demo] using the supplied 64-digit raw PSK/PMK\n");
        return RT_EOK;
    }

    if ((credential_len < 8U) || (credential_len > 63U))
    {
        rt_kprintf("[wifi-demo] password must be 8..63 bytes, or a 64-digit raw PSK\n");
        return -RT_EINVAL;
    }

    /* WPA-Personal's passphrase form is PBKDF2-HMAC-SHA1(password, SSID,
     * 4096), producing the 32-byte PMK consumed by cfg80211.  Two independent
     * implementations are evaluated here: Mbed TLS (which this port routes
     * through the CryptoCell SHA1 ALT) and the vendor supplicant primitive
     * pbkdf2_sha1() (which the SDK supplicant uses for the PSK).  The vendor
     * result is treated as the reference: a mismatch means the local Mbed
     * TLS/CryptoCell SHA1 path is not producing a valid WPA PSK, and the AP
     * would then reject EAPOL-Key message 2/4 with a MIC failure. */
    status = mbedtls_pkcs5_pbkdf2_hmac_ext(
        MBEDTLS_MD_SHA1,
        (const unsigned char *) credential, credential_len,
        (const unsigned char *) ssid, strlen(ssid),
        4096U, WIFI_DEMO_PSK_LEN, psk);
    if (status != 0)
    {
        memset(psk, 0, WIFI_DEMO_PSK_LEN);
        rt_kprintf("[wifi-demo] PBKDF2 failed: %d\n", status);
        return -RT_ERROR;
    }

    memset(sdk_psk, 0, sizeof(sdk_psk));
    if (pbkdf2_sha1(credential, (const u8 *) ssid, strlen(ssid), 4096,
                    sdk_psk, sizeof(sdk_psk)) != 0)
    {
        rt_kprintf("[wifi-demo] warning: vendor pbkdf2_sha1 failed; "
                   "keeping the Mbed TLS PMK\n");
        return RT_EOK;
    }

    memcpy(psk, sdk_psk, WIFI_DEMO_PSK_LEN);

    return RT_EOK;
}

static void wifi_demo_print_bssid(const u8 *bssid)
{
    rt_kprintf("%02X:%02X:%02X:%02X:%02X:%02X",
               bssid[0], bssid[1], bssid[2],
               bssid[3], bssid[4], bssid[5]);
}

static size_t wifi_demo_find_rsn_ie(const u8 *ies, size_t ie_len,
                                    u8 *out, size_t out_len)
{
    size_t offset = 0U;

    if ((ies == NULL) || (out == NULL))
    {
        return 0U;
    }

    while (offset + 2U <= ie_len)
    {
        size_t element_len = ies[offset + 1U];
        size_t total_len = element_len + 2U;

        if (total_len > ie_len - offset)
        {
            break;
        }

        if ((ies[offset] == WLAN_EID_RSN) && (total_len <= out_len))
        {
            memcpy(out, ies + offset, total_len);
            return total_len;
        }

        offset += total_len;
    }

    return 0U;
}

static size_t wifi_demo_copy_rsn_ie(const u8 *ies, size_t ie_len)
{
    s_connect_ap_rsn_ie_len = 0U;
    if (ies == NULL)
    {
        return 0U;
    }

    s_connect_ap_rsn_ie_len = wifi_demo_find_rsn_ie(ies, ie_len,
                                                    s_connect_ap_rsn_ie,
                                                    sizeof(s_connect_ap_rsn_ie));
    return s_connect_ap_rsn_ie_len;
}

static int wifi_demo_parse_bssid(const char *text, u8 *bssid)
{
    unsigned int index;

    if ((text == NULL) || (bssid == NULL) || (strlen(text) != 17U))
    {
        return -RT_EINVAL;
    }

    for (index = 0U; index < ETH_ALEN; index++)
    {
        int high;
        int low;

        high = wifi_demo_hex_value(text[index * 3U]);
        low = wifi_demo_hex_value(text[index * 3U + 1U]);
        if ((high < 0) || (low < 0))
        {
            return -RT_EINVAL;
        }

        bssid[index] = (u8) ((high << 4) | low);
        if ((index + 1U < ETH_ALEN) && (text[index * 3U + 2U] != ':'))
        {
            return -RT_EINVAL;
        }
    }

    if (text[17U] != '\0')
    {
        return -RT_EINVAL;
    }

    return RT_EOK;
}

static int wifi_demo_parse_frequency(const char *text, unsigned int *freq)
{
    char *end;
    unsigned long value;

    if ((text == NULL) || (freq == NULL) || (text[0] == '\0'))
    {
        return -RT_EINVAL;
    }

    end = NULL;
    value = strtoul(text, &end, 10);
    if ((*end != '\0') || (value < 2000UL) || (value > 7000UL))
    {
        return -RT_EINVAL;
    }

    *freq = (unsigned int) value;
    return RT_EOK;
}

/* The normal SDK creates this complete context before association.  In
 * particular, rsdev_cfg80211_associate() relies on the driver object having a
 * valid first_bss/capability relationship even though its immediate lookup
 * uses only ifindex. */
static int wifi_demo_prepare_connect_driver(void)
{
    struct wireless_dev *active_wdev;

    if ((s_connect_driver != NULL) && (s_connect_bss != NULL) &&
        (s_connect_global != NULL))
    {
        return RT_EOK;
    }

    if (s_connect_global == NULL)
    {
        s_connect_global = fc80211_global_init();
        if (s_connect_global == NULL)
        {
            rt_kprintf("[wifi-demo] fc80211 global init failed\n");
            return -RT_ENOMEM;
        }
    }

    s_connect_bss = (struct i802_bss *)
        wpa_driver_fc80211_init(NULL, WIFI_DEMO_IF_NAME, s_connect_global);
    if ((s_connect_bss == NULL) || (s_connect_bss->drv == NULL))
    {
        rt_kprintf("[wifi-demo] fc80211 driver context init failed\n");
        s_connect_bss = NULL;
        s_connect_driver = NULL;
        return -RT_ERROR;
    }

    s_connect_driver = s_connect_bss->drv;
    active_wdev = fc80211_wdev_from_if_idx(WIFI_DEMO_IF_INDEX);
    if ((active_wdev == NULL) ||
        (active_wdev->wiphy != &rwnx_wiphy) ||
        (s_connect_bss->ifindex != (int) WIFI_DEMO_IF_INDEX) ||
        (s_connect_driver->ifindex != (int) WIFI_DEMO_IF_INDEX) ||
        (s_connect_driver->first_bss != s_connect_bss))
    {
        rt_kprintf("[wifi-demo] invalid connect context: wdev=%p bss=%p "
                   "drv=%p first_bss=%p ifindex=%d/%d\n",
                   active_wdev, s_connect_bss, s_connect_driver,
                   s_connect_driver->first_bss,
                   s_connect_bss->ifindex, s_connect_driver->ifindex);
        return -RT_ERROR;
    }

    s_wifi_demo.wdev = active_wdev;
    return RT_EOK;
}

/* The vendor supplicant opens the 802.1X controlled port as soon as the
 * connection is usable: wpa_driver_fc80211_set_supp_port(bss, 1) marks the STA
 * as authorized in the firmware (FC80211_STA_FLAG_AUTHORIZED) and tells the
 * netif layer that the interface is up.  Without that step the firmware keeps
 * the STA port closed, so an associated WPA2 client carries no traffic and the
 * AP/router never lists it as a real client - while an open (no RSNA)
 * connection is not gated by the 802.1X port at all and therefore shows up.
 *
 * The driver helper is NULL-safe about the netif: wifi_netif_control() only
 * calls netif_set_up() when the wlan0 netif already exists. */
static void wifi_demo_authorize_port(void)
{
    int status;

    if ((s_connect_bss == NULL) || (s_connect_driver == NULL) ||
        (s_connect_driver->associated == 0) ||
        wifi_demo_bssid_is_zero(s_connect_driver->bssid))
    {
        return;
    }

    status = wpa_driver_fc80211_set_supp_port(s_connect_bss, 1);
    if (status != 0)
    {
        rt_kprintf("[wifi-demo] port authorization failed: %d\n", status);
    }
}

static int wifi_demo_connect_request(const char *ssid, const u8 *psk,
                                     bool secure, unsigned int freq,
                                     const u8 *bssid)
{
    size_t ssid_len;
    unsigned int waited_ms;
    int request_status;
    int status;

    if (ssid == NULL)
    {
        return -RT_EINVAL;
    }

    ssid_len = strlen(ssid);
    if ((ssid_len == 0U) || (ssid_len > WIFI_DEMO_MAX_SSID_LEN))
    {
        rt_kprintf("[wifi-demo] SSID length must be 1..32 bytes\n");
        return -RT_EINVAL;
    }

    if (secure && (psk == NULL))
    {
        return -RT_EINVAL;
    }

    if ((freq != 0U) != (bssid != NULL))
    {
        rt_kprintf("[wifi-demo] freq and BSSID must be supplied together\n");
        return -RT_EINVAL;
    }

    status = wifi_demo_start();
    if (status != RT_EOK)
    {
        return status;
    }

    if (s_wifi_demo.scan_active || s_wifi_demo.connect_active)
    {
        rt_kprintf("[wifi-demo] another Wi-Fi operation is active\n");
        return -RT_EBUSY;
    }

    /* Always clear a previous association and any queued EAPOL before a new
     * request. This also handles a handshake that timed out without a final
     * driver event. */
    if ((s_connect_driver != NULL) &&
        ((s_connect_driver->associated != 0) ||
         !wifi_demo_bssid_is_zero(s_connect_driver->bssid) ||
         s_wifi_demo.connected || (s_wifi_demo.wpa_sm != NULL)))
    {
        status = wifi_demo_disconnect_internal(3U);
        if (status != 0)
        {
            return status;
        }
    }

    /* A phone hotspot picks a new BSSID/channel whenever it is switched on, so
     * the command-line freq/BSSID can be stale and the driver's "auto"
     * connect then fails with status 1.  When no explicit target was given,
     * look the SSID up on the air and use the BSS that is really there. */
    if (bssid == NULL)
    {
        unsigned int resolved_freq = 0U;
        u8 resolved_bssid[ETH_ALEN];

        if (wifi_demo_lookup_bss(ssid, &resolved_freq, resolved_bssid) ==
            RT_EOK)
        {
            freq = resolved_freq;
            memcpy(s_connect_target_bssid, resolved_bssid,
                   sizeof(s_connect_target_bssid));
            s_connect_target_bssid_set = true;
        }
        else
        {
            /* The driver may still use its cached BSS if the scan missed it. */
        }
    }

    status = wifi_demo_prepare_connect_driver();
    if (status != RT_EOK)
    {
        return status;
    }

    if (secure &&
        ((s_connect_driver->capa.flags &
          WPA_DRIVER_FLAGS_4WAY_HANDSHAKE_PSK) == 0ULL))
    {
        /* A CONNECT/ASSOCIATE success is only an 802.11 MLME result here.
         * Without firmware 4-way offload, a host WPA state machine must
         * consume EAPOL and install PTK/GTK before this is a usable link. */
        rt_kprintf("[wifi-demo] warning: driver has no WPA2-PSK 4-way "
                   "offload (capa_flags=0x%08x%08x); association status "
                   "will not prove secure connection\n",
                   (unsigned int) (s_connect_driver->capa.flags >> 32),
                   (unsigned int) s_connect_driver->capa.flags);
    }

    /* Copy command-line data into storage that outlives this msh command.
     * The converted cfg80211 request is asynchronous after rsdev returns. */
    memcpy(s_connect_ssid, ssid, ssid_len);
    s_connect_ssid[ssid_len] = '\0';
    if (secure)
    {
        memcpy(s_connect_psk, psk, sizeof(s_connect_psk));
    }
    else
    {
        memset(s_connect_psk, 0, sizeof(s_connect_psk));
    }
    s_connect_secure = secure;
    s_wifi_demo.connect_secure = secure;
    s_connect_host_wpa = secure &&
        ((s_connect_driver->capa.flags &
          WPA_DRIVER_FLAGS_4WAY_HANDSHAKE_PSK) == 0ULL);

    s_connect_target_freq = freq;
    if (bssid != NULL)
    {
        s_connect_target_bssid_set = true;
        memcpy(s_connect_target_bssid, bssid,
               sizeof(s_connect_target_bssid));
    }
    else if (!s_connect_target_bssid_set)
    {
        /* No explicit target and the scan did not resolve one: let the driver
         * pick a BSS from its own cached scan results. */
        memset(s_connect_target_bssid, 0,
               sizeof(s_connect_target_bssid));
    }

    s_connect_ap_rsn_ie_len = 0U;

    memset(&s_connect_assoc, 0, sizeof(s_connect_assoc));

    /* These are the values supplied by wpa_supplicant for a WPA2-PSK
     * infrastructure association.  The vendor conversion maps them to the
     * cfg80211 crypto suite selectors used by rwnx. */
    s_connect_assoc.ssid = (const u8 *) s_connect_ssid;
    s_connect_assoc.ssid_len = ssid_len;
    s_connect_assoc.bssid = s_connect_target_bssid_set ?
        s_connect_target_bssid : NULL;
    s_connect_assoc.freq.freq = (int) s_connect_target_freq;
    s_connect_assoc.wpa_ie = NULL;
    s_connect_assoc.wpa_ie_len = 0U;
    s_connect_assoc.wpa_proto = WPA_PROTO_RSN;
    s_connect_assoc.pairwise_suite = WPA_CIPHER_CCMP;
    s_connect_assoc.group_suite = WPA_CIPHER_CCMP;
    s_connect_assoc.mgmt_group_suite = 0U;
    s_connect_assoc.key_mgmt_suite = WPA_KEY_MGMT_PSK;
    s_connect_assoc.auth_alg = WPA_AUTH_ALG_OPEN;
    s_connect_assoc.mode = IEEE80211_MODE_INFRA;
    s_connect_assoc.mgmt_frame_protection = NO_MGMT_FRAME_PROTECTION;

    if (secure)
    {
        s_connect_assoc.wpa_ie = wifi_security_manager_assoc_ie(
            &s_connect_assoc.wpa_ie_len);
        s_connect_assoc.psk = s_connect_psk;
    }

    if (s_connect_host_wpa)
    {
        status = wifi_demo_wpa_sm_init();
        if (status != RT_EOK)
        {
            rt_kprintf("[wifi-demo] WPA2 state machine init failed: %d\n",
                       status);
            return status;
        }
    }
    else
    {
        wifi_demo_wpa_deinit();
    }

    wifi_demo_pump_events();
    s_wifi_demo.connect_active = true;
    s_wifi_demo.connect_done = false;
    s_wifi_demo.connect_timed_out = false;
    s_wifi_demo.connect_status = -RT_ETIMEOUT;
    memset(s_wifi_demo.connect_bssid, 0,
           sizeof(s_wifi_demo.connect_bssid));
    s_wifi_demo.connected_ssid[0] = '\0';
    s_wifi_demo.connected_freq = 0U;
    s_wifi_demo.station_query_status = -RT_ERROR;

    /* This is the state update performed by
     * wpa_driver_fc80211_associate() immediately before its rsdev call. */
    fc80211_mark_disconnected(s_connect_driver);
    s_connect_driver->assoc_freq = s_connect_assoc.freq.freq > 0 ?
        (unsigned int) s_connect_assoc.freq.freq : 0U;
    memcpy(s_connect_driver->ssid, s_connect_assoc.ssid,
           s_connect_assoc.ssid_len);
    s_connect_driver->ssid_len = s_connect_assoc.ssid_len;

    request_status = rsdev_cfg80211_associate(s_connect_driver,
                                              &s_connect_assoc);
    if (request_status != 0)
    {
        s_wifi_demo.connect_active = false;
        (void) wifi_demo_disconnect_internal(3U);
        return request_status;
    }

    for (waited_ms = 0U; waited_ms < WIFI_DEMO_CONNECT_WAIT_MS;
         waited_ms += WIFI_DEMO_CONNECT_POLL_MS)
    {
        wifi_demo_pump_events();
        if (s_wifi_demo.connect_done)
        {
            break;
        }

        rt_thread_mdelay(WIFI_DEMO_CONNECT_POLL_MS);
    }

    wifi_demo_pump_events();
    s_wifi_demo.connect_active = false;
    if (!s_wifi_demo.connect_done)
    {
        rt_kprintf("[wifi-demo] connect timeout after %u ms\n",
                   WIFI_DEMO_CONNECT_WAIT_MS);
        (void) wifi_demo_disconnect_internal(3U);
        return -RT_ETIMEOUT;
    }

    if (s_wifi_demo.connected)
    {
        struct station_info sinfo;
        int station_status;

        /* Same step the vendor supplicant performs when the handshake (or, for
         * an open network, the association) completes. */
        wifi_demo_authorize_port();

        station_status = wifi_demo_query_station(&sinfo);

        rt_kprintf("[wifi-demo] connected: BSSID=");
        wifi_demo_print_bssid(s_wifi_demo.connect_bssid);
        rt_kprintf("\n");
        if (station_status == RT_EOK)
        {
            rt_kprintf("[wifi-demo] associated station: ");
            wifi_demo_print_station_info(&sinfo);
        }
        else
        {
            rt_kprintf("[wifi-demo] station query after WPA completion failed: %d\n",
                       station_status);
        }
        return RT_EOK;
    }

    rt_kprintf("[wifi-demo] connect failed: status=%d%s\n",
               s_wifi_demo.connect_status,
               s_wifi_demo.connect_timed_out ? " (timed out)" : "");
    (void) wifi_demo_disconnect_internal(3U);
    return -RT_ERROR;
}

static bool wifi_demo_bssid_is_zero(const u8 *bssid)
{
    size_t index;

    if (bssid == NULL)
    {
        return true;
    }

    for (index = 0U; index < ETH_ALEN; index++)
    {
        if (bssid[index] != 0U)
        {
            return false;
        }
    }

    return true;
}

static const u8 *wifi_demo_status_bssid(void)
{
    if (!wifi_demo_bssid_is_zero(s_wifi_demo.connect_bssid))
    {
        return s_wifi_demo.connect_bssid;
    }

    if ((s_connect_driver != NULL) &&
        !wifi_demo_bssid_is_zero(s_connect_driver->bssid))
    {
        return s_connect_driver->bssid;
    }

    if (s_connect_target_bssid_set &&
        !wifi_demo_bssid_is_zero(s_connect_target_bssid))
    {
        return s_connect_target_bssid;
    }

    return NULL;
}

static int wifi_demo_query_station(struct station_info *sinfo)
{
    const u8 *bssid;
    int status;

    if (sinfo == NULL)
    {
        return -RT_EINVAL;
    }

    memset(sinfo, 0, sizeof(*sinfo));
    bssid = wifi_demo_status_bssid();
    if ((s_wifi_demo.wdev == NULL) || (bssid == NULL) ||
        (CFG80211_semaphore == NULL))
    {
        s_wifi_demo.station_query_status = -RT_ERROR;
        return s_wifi_demo.station_query_status;
    }

    status = rwnx_cfg80211_get_station(
        (u8) WIFI_DEMO_IF_INDEX, FC80211_IFTYPE_STATION, bssid, sinfo);
    s_wifi_demo.station_query_status = status;
    if (status == RT_EOK)
    {
        if (!s_wifi_demo.connect_secure || s_wifi_demo.wpa_key_done)
        {
            s_wifi_demo.connected = true;
        }
        if (wifi_demo_bssid_is_zero(s_wifi_demo.connect_bssid))
        {
            memcpy(s_wifi_demo.connect_bssid, bssid,
                   sizeof(s_wifi_demo.connect_bssid));
        }
        if ((s_wifi_demo.connected_ssid[0] == '\0') &&
            (s_connect_driver != NULL) &&
            (s_connect_driver->ssid_len != 0U) &&
            (s_connect_driver->ssid_len <= WIFI_DEMO_MAX_SSID_LEN))
        {
            memcpy(s_wifi_demo.connected_ssid, s_connect_driver->ssid,
                   s_connect_driver->ssid_len);
            s_wifi_demo.connected_ssid[s_connect_driver->ssid_len] = '\0';
        }
        if ((s_wifi_demo.connected_ssid[0] == '\0') &&
            (s_wifi_demo.wdev != NULL) &&
            (s_wifi_demo.wdev->ssid_len != 0U) &&
            (s_wifi_demo.wdev->ssid_len <= WIFI_DEMO_MAX_SSID_LEN))
        {
            memcpy(s_wifi_demo.connected_ssid, s_wifi_demo.wdev->ssid,
                   s_wifi_demo.wdev->ssid_len);
            s_wifi_demo.connected_ssid[s_wifi_demo.wdev->ssid_len] = '\0';
        }
        if ((s_wifi_demo.connected_freq == 0U) &&
            (s_connect_driver != NULL))
        {
            s_wifi_demo.connected_freq = s_connect_driver->assoc_freq;
        }
    }

    return status;
}

static void wifi_demo_print_station_info(const struct station_info *sinfo)
{
    u64 filled;

    if (sinfo == NULL)
    {
        return;
    }

    filled = sinfo->filled;
    /* rt_kprintf() in this BSP does not implement the "ll" length modifier,
     * so 64-bit values are printed as two 32-bit halves. */
    rt_kprintf(" station_filled=0x%08x%08x",
               (unsigned int) (filled >> 32), (unsigned int) filled);
    if ((filled & (1ULL << FC80211_STA_INFO_SIGNAL)) != 0ULL)
    {
        rt_kprintf(" signal=%d dBm", (int) sinfo->signal);
    }
    if ((filled & (1ULL << FC80211_STA_INFO_SIGNAL_AVG)) != 0ULL)
    {
        rt_kprintf(" avg=%d dBm", (int) sinfo->signal_avg);
    }
    if ((filled & (1ULL << FC80211_STA_INFO_TX_BITRATE)) != 0ULL)
    {
        rt_kprintf(" tx_legacy=%u00kbps tx_flags=0x%02x tx_mcs=%u tx_nss=%u",
                   (unsigned int) sinfo->txrate.legacy,
                   (unsigned int) sinfo->txrate.flags,
                   (unsigned int) sinfo->txrate.mcs,
                   (unsigned int) sinfo->txrate.nss);
    }
    if ((filled & (1ULL << FC80211_STA_INFO_CONNECTED_TIME)) != 0ULL)
    {
        rt_kprintf(" connected_time=%lu s",
                   (unsigned long) sinfo->connected_time);
    }
    if ((filled & (1ULL << FC80211_STA_INFO_RX_BYTES64)) != 0ULL)
    {
        rt_kprintf(" rx_bytes=0x%08x%08x",
                   (unsigned int) (sinfo->rx_bytes >> 32),
                   (unsigned int) sinfo->rx_bytes);
    }
    if ((filled & (1ULL << FC80211_STA_INFO_TX_BYTES64)) != 0ULL)
    {
        rt_kprintf(" tx_bytes=0x%08x%08x",
                   (unsigned int) (sinfo->tx_bytes >> 32),
                   (unsigned int) sinfo->tx_bytes);
    }
    rt_kprintf("\n");
}

static int wifi_demo_frequency_to_channel(int freq)
{
    if ((freq >= 2412) && (freq <= 2484))
    {
        return (freq == 2484) ? 14 : ((freq - 2407) / 5);
    }
    if ((freq >= 5000) && (freq <= 5900))
    {
        return (freq - 5000) / 5;
    }
    return -1;
}

static rt_wlan_security_t wifi_demo_security_from_ies(const u8 *ies,
                                                       size_t ie_len)
{
    size_t offset = 0U;
    bool has_rsn = false;
    bool has_wpa = false;

    while ((ies != NULL) && (offset + 2U <= ie_len))
    {
        size_t element_len = ies[offset + 1U];
        size_t total_len = element_len + 2U;

        if (total_len > ie_len - offset)
        {
            break;
        }
        if (ies[offset] == WLAN_EID_RSN)
        {
            has_rsn = true;
        }
        else if ((ies[offset] == 221U) && (element_len >= 4U) &&
                 (ies[offset + 2U] == 0x00U) &&
                 (ies[offset + 3U] == 0x50U) &&
                 (ies[offset + 4U] == 0xf2U) &&
                 (ies[offset + 5U] == 0x01U))
        {
            has_wpa = true;
        }
        offset += total_len;
    }

    if (has_rsn)
    {
        return SECURITY_WPA2_AES_PSK;
    }
    if (has_wpa)
    {
        return SECURITY_WPA_AES_PSK;
    }
    return SECURITY_OPEN;
}

/* Public conversion entry points used by the RT-Thread WLAN adapter.  The
 * adapter never exposes the vendor scan-result pointers outside this file. */
int wifi_fc80211_demo_scan_info(struct rt_wlan_info *infos, size_t max,
                                size_t *count)
{
    struct wifi_demo_scan_results results;
    size_t index;
    size_t copied = 0U;
    int status;

    if ((infos == NULL) || (count == NULL) || (max == 0U))
    {
        return -RT_EINVAL;
    }
    *count = 0U;
    status = wifi_demo_start();
    if (status != RT_EOK)
    {
        return status;
    }
    status = wifi_demo_scan_collect(&results);
    if (status != RT_EOK)
    {
        return status;
    }

    for (index = 0U; (index < results.num) && (copied < max); index++)
    {
        const struct wifi_demo_scan_res *result = results.res[index];
        int channel;

        if (result == NULL)
        {
            continue;
        }
        memset(&infos[copied], 0, sizeof(infos[copied]));
        infos[copied].security = wifi_demo_security_from_ies(
            result->ie, result->ie_len);
        infos[copied].band = (result->freq >= 5000) ?
            RT_802_11_BAND_5GHZ : RT_802_11_BAND_2_4GHZ;
        infos[copied].datarate = result->est_throughput;
        infos[copied].rssi = (rt_int16_t) result->level;
        memcpy(infos[copied].bssid, result->bssid,
               RT_WLAN_BSSID_MAX_LENGTH);
        channel = wifi_demo_frequency_to_channel(result->freq);
        infos[copied].channel = (rt_int16_t) channel;
        /* The scan result contains the SSID IE; reuse the bounded parser used
         * by the standalone display path and copy only valid bytes. */
        {
            size_t ie_offset = 0U;
            while ((result->ie != NULL) &&
                   (ie_offset + 2U <= result->ie_len))
            {
                size_t element_len = result->ie[ie_offset + 1U];
                size_t total_len = element_len + 2U;
                if (total_len > result->ie_len - ie_offset)
                {
                    break;
                }
                if ((result->ie[ie_offset] == 0U) &&
                    (element_len <= RT_WLAN_SSID_MAX_LENGTH))
                {
                    infos[copied].ssid.len = (rt_uint8_t) element_len;
                    memcpy(infos[copied].ssid.val,
                           result->ie + ie_offset + 2U, element_len);
                    infos[copied].ssid.val[element_len] = '\0';
                    break;
                }
                ie_offset += total_len;
            }
        }
        copied++;
    }

    wifi_demo_free_scan_results(&results);
    (void) fc80211_free_umac_scan_results(0UL, WIFI_DEMO_IF_INDEX);
    *count = copied;
    return RT_EOK;
}

int wifi_fc80211_demo_join(const struct rt_sta_info *sta)
{
    char ssid[RT_WLAN_SSID_MAX_LENGTH + 1U];
    u8 psk[WIFI_DEMO_PSK_LEN];
    unsigned int freq = 0U;
    bool secure;
    const u8 *bssid = NULL;
    int status;

    if ((sta == NULL) || (sta->ssid.len == 0U) ||
        (sta->ssid.len > RT_WLAN_SSID_MAX_LENGTH) ||
        (sta->key.len > RT_WLAN_PASSWORD_MAX_LENGTH))
    {
        return -RT_EINVAL;
    }
    if ((sta->security != SECURITY_OPEN) &&
        (sta->security != SECURITY_WPA2_AES_PSK) &&
        (sta->security != SECURITY_WPA2_TKIP_PSK) &&
        (sta->security != SECURITY_WPA2_MIXED_PSK) &&
        (sta->security != SECURITY_WPA_AES_PSK) &&
        (sta->security != SECURITY_WPA_TKIP_PSK))
    {
        return -RT_ENOSYS;
    }

    memcpy(ssid, sta->ssid.val, sta->ssid.len);
    ssid[sta->ssid.len] = '\0';
    if ((sta->channel > 0) && (sta->channel <= 14))
    {
        freq = (sta->channel == 14) ? 2484U :
               (2407U + ((unsigned int) sta->channel * 5U));
    }
    else if (sta->channel > 14)
    {
        freq = 5000U + ((unsigned int) sta->channel * 5U);
    }
    if (!wifi_demo_bssid_is_zero(sta->bssid))
    {
        bssid = sta->bssid;
    }
    secure = (sta->security != SECURITY_OPEN);
    if (secure)
    {
        status = wifi_demo_prepare_psk((const char *) sta->key.val,
                                       ssid, psk);
        if (status != RT_EOK)
        {
            return status;
        }
    }
    return wifi_demo_connect_request(ssid, secure ? psk : NULL, secure,
                                     freq, bssid);
}

int wifi_fc80211_demo_get_link_info(struct rt_wlan_info *info)
{
    struct station_info sinfo;
    const u8 *bssid;
    int status;
    int freq;

    if (info == NULL)
    {
        return -RT_EINVAL;
    }
    status = wifi_demo_start();
    if (status != RT_EOK)
    {
        return status;
    }
    wifi_demo_pump_events();
    status = wifi_demo_query_station(&sinfo);
    if (status != RT_EOK)
    {
        return status;
    }
    memset(info, 0, sizeof(*info));
    info->security = s_wifi_demo.connect_secure ?
                     SECURITY_WPA2_AES_PSK : SECURITY_OPEN;
    freq = (s_wifi_demo.connected_freq != 0U) ?
           (int) s_wifi_demo.connected_freq :
           ((s_connect_driver != NULL) ? (int) s_connect_driver->assoc_freq : 0);
    info->band = (freq >= 5000) ? RT_802_11_BAND_5GHZ :
                 RT_802_11_BAND_2_4GHZ;
    info->channel = (rt_int16_t) wifi_demo_frequency_to_channel(freq);
    info->rssi = ((sinfo.filled & (1ULL << FC80211_STA_INFO_SIGNAL)) != 0ULL) ?
                 (rt_int16_t) sinfo.signal : -127;
    bssid = wifi_demo_status_bssid();
    if (bssid != NULL)
    {
        memcpy(info->bssid, bssid, RT_WLAN_BSSID_MAX_LENGTH);
    }
    if (s_wifi_demo.connected_ssid[0] != '\0')
    {
        info->ssid.len = (rt_uint8_t) strlen(s_wifi_demo.connected_ssid);
        memcpy(info->ssid.val, s_wifi_demo.connected_ssid, info->ssid.len);
    }
    return RT_EOK;
}

int wifi_fc80211_demo_get_mac(rt_uint8_t mac[RT_WLAN_BSSID_MAX_LENGTH])
{
    UCHAR *addr;

    if (mac == NULL)
    {
        return -RT_EINVAL;
    }
    if (wifi_demo_start() != RT_EOK)
    {
        return -RT_ERROR;
    }
    addr = fc80211_get_interface_macaddr(0UL, WIFI_DEMO_IF_INDEX);
    if (addr == NULL)
    {
        return -RT_ERROR;
    }
    memcpy(mac, addr, RT_WLAN_BSSID_MAX_LENGTH);
    return RT_EOK;
}

int wifi_fc80211_demo_disconnect(void)
{
    int status = wifi_demo_start();

    if (status != RT_EOK)
    {
        return status;
    }
    return wifi_demo_disconnect_internal(3U);
}

static int wifi_low_connect(int argc, char **argv)
{
    u8 psk[WIFI_DEMO_PSK_LEN];
    u8 bssid[ETH_ALEN];
    unsigned int freq = 0U;
    const u8 *bssid_arg = NULL;
    int status;

    if ((argc != 3) && (argc != 5))
    {
        rt_kprintf("usage: wifi_low_connect <ssid> <password|psk_hex64> "
                   "[freq_mhz bssid]\n");
        rt_kprintf("  use a normal 8..63 byte password or a 64-digit raw PSK/PMK\n");
        rt_kprintf("  no-scan mode requires both freq_mhz and bssid, e.g. "
                   "wifi_low_connect xiaomi password 2437 12:34:56:78:9A:BC\n");
        return -RT_EINVAL;
    }

    if (argc == 5)
    {
        status = wifi_demo_parse_frequency(argv[3], &freq);
        if (status != RT_EOK)
        {
            rt_kprintf("[wifi-demo] invalid frequency: %s\n", argv[3]);
            return status;
        }

        status = wifi_demo_parse_bssid(argv[4], bssid);
        if (status != RT_EOK)
        {
            rt_kprintf("[wifi-demo] invalid BSSID: %s\n", argv[4]);
            return status;
        }
        bssid_arg = bssid;
    }

    status = wifi_demo_prepare_psk(argv[2], argv[1], psk);
    if (status != RT_EOK)
    {
        return status;
    }

    return wifi_demo_connect_request(argv[1], psk, true, freq, bssid_arg);
}

static int wifi_low_connect_open(int argc, char **argv)
{
    u8 bssid[ETH_ALEN];
    unsigned int freq = 0U;
    const u8 *bssid_arg = NULL;
    int status;

    if ((argc != 2) && (argc != 4))
    {
        rt_kprintf("usage: wifi_low_connect_open <ssid> [freq_mhz bssid]\n");
        return -RT_EINVAL;
    }

    if (argc == 4)
    {
        status = wifi_demo_parse_frequency(argv[2], &freq);
        if (status != RT_EOK)
        {
            rt_kprintf("[wifi-demo] invalid frequency: %s\n", argv[2]);
            return status;
        }

        status = wifi_demo_parse_bssid(argv[3], bssid);
        if (status != RT_EOK)
        {
            rt_kprintf("[wifi-demo] invalid BSSID: %s\n", argv[3]);
            return status;
        }
        bssid_arg = bssid;
    }

    return wifi_demo_connect_request(argv[1], NULL, false, freq, bssid_arg);
}

static int wifi_low_disconnect(int argc, char **argv)
{
    int status;

    (void) argv;

    if (argc != 1)
    {
        rt_kprintf("usage: wifi_low_disconnect\n");
        return -RT_EINVAL;
    }

    status = wifi_demo_start();
    if (status != RT_EOK)
    {
        return status;
    }

    status = wifi_demo_disconnect_internal(3U);
    if (status != 0)
    {
        rt_kprintf("[wifi-demo] disconnect failed: %d\n", status);
        return status;
    }

    rt_kprintf("[wifi-demo] disconnected\n");
    return RT_EOK;
}

static int wifi_low_status(int argc, char **argv)
{
    struct station_info sinfo;
    const u8 *bssid;
    bool current_bss;
    int station_status;

    (void) argv;

    if (argc != 1)
    {
        rt_kprintf("usage: wifi_low_status\n");
        return -RT_EINVAL;
    }

    wifi_demo_pump_events();
    current_bss = (s_wifi_demo.wdev != NULL) &&
                  (s_wifi_demo.wdev->current_bss != NULL);
    if (!s_wifi_demo.connected && current_bss &&
        (!s_wifi_demo.connect_secure || s_wifi_demo.wpa_key_done))
    {
        /* The event may have been consumed by another task.  cfg80211's
         * current_bss is the driver's associated-state fallback. */
        s_wifi_demo.connected = true;
        s_wifi_demo.connect_status = WIFI_DEMO_STATUS_SUCCESS;
    }

    station_status = -RT_ERROR;
    if (s_wifi_demo.connected)
    {
        station_status = wifi_demo_query_station(&sinfo);
    }

    rt_kprintf("[wifi-demo] initialized=%s interface=%s link=%s wpa=%s",
               s_wifi_demo.hardware_ready ? "yes" : "no",
               s_wifi_demo.iface_ready ? WIFI_DEMO_IF_NAME : "none",
               s_wifi_demo.connected ? "connected" : "disconnected",
               wifi_demo_wpa_state_name(s_wifi_demo.wpa_state));
    if (s_wifi_demo.connected)
    {
        rt_kprintf(" ssid=\"%s\"",
                   s_wifi_demo.connected_ssid[0] != '\0' ?
                   s_wifi_demo.connected_ssid : "<unknown>");
        if (s_wifi_demo.connected_freq != 0U)
        {
            rt_kprintf(" freq=%uMHz", s_wifi_demo.connected_freq);
        }
        rt_kprintf(" bssid=");
        bssid = wifi_demo_status_bssid();
        if (bssid != NULL)
        {
            wifi_demo_print_bssid(bssid);
        }
        else
        {
            rt_kprintf("<unknown>");
        }
        rt_kprintf(" current_bss=%s", current_bss ? "yes" : "no");
        if (station_status == RT_EOK)
        {
            wifi_demo_print_station_info(&sinfo);
        }
        else
        {
            rt_kprintf(" station_query=%d\n", station_status);
        }
    }
    else
    {
        rt_kprintf(" current_bss=%s last_connect_status=%d\n",
                   current_bss ? "yes" : "no", s_wifi_demo.connect_status);
    }
    return RT_EOK;
}

static int wifi_low_init(int argc, char **argv)
{
    (void) argv;

    if (argc != 1)
    {
        rt_kprintf("usage: wifi_low_init\n");
        return -RT_EINVAL;
    }

    return wifi_demo_start();
}

void wifi_hw_early_watchdog_freeze(void)
{
    CRG_TOP->SET_FREEZE_REG_b.FRZ_SYS_WDOG = 1;
}

MSH_CMD_EXPORT(wifi_low_init, initialize Wi-Fi through direct cfg80211 ABI);
MSH_CMD_EXPORT(wifi_low_scan, scan Wi-Fi through direct cfg80211 ABI);
MSH_CMD_EXPORT(wifi_low_connect,
               connect to WPA2-PSK AP through direct cfg80211 ABI);
MSH_CMD_EXPORT(wifi_low_connect_open,
               connect to open AP through direct cfg80211 ABI);
MSH_CMD_EXPORT(wifi_low_disconnect,
               disconnect through direct cfg80211 ABI);
MSH_CMD_EXPORT(wifi_low_status, show standalone Wi-Fi link state);

/* Entry points used by the standalone cfg80211 API probe. */
int wifi_fc80211_demo_ensure_init(void)
{
    return wifi_demo_start();
}

struct wireless_dev *wifi_fc80211_demo_get_wdev(void)
{
    return s_wifi_demo.wdev;
}

int wifi_fc80211_demo_scan_once(void)
{
    return wifi_low_scan(1, NULL);
}
