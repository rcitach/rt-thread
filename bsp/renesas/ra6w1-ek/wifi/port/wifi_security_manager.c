#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <rtthread.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "co_int.h"

typedef unsigned long uint32;

#include "os.h"
#include "rwnx_cfg.h"
#include "wifi_security_manager.h"

#define WIFI_SECURITY_ETH_HDR_LEN 14U
#define WIFI_SECURITY_EAPOL_VERSION 2U
#define WIFI_SECURITY_EAPOL_PROTO 0x888eU
#define WIFI_SECURITY_MAX_IFNAME 15U

struct wifi_security_eapol_hdr
{
    u8 version;
    u8 type;
    u16 length;
} __attribute__((packed));

static const u8 s_wpa2_psk_rsn_ie[] = {
    0x30, 0x14,
    0x01, 0x00,
    0x00, 0x0f, 0xac, 0x04,
    0x01, 0x00,
    0x00, 0x0f, 0xac, 0x04,
    0x01, 0x00,
    0x00, 0x0f, 0xac, 0x02,
    0x00, 0x00
};

/* Dump the EAPOL-Key message 2/4 while debugging the 4-way handshake: the
 * frame contains the SNonce, the MIC and the RSN IE, which is everything
 * needed to check the MIC off-line against the PMK and both nonces. */
#define WIFI_SEC_DUMP_EAPOL (0)

#if WIFI_SEC_DUMP_EAPOL
static void security_dump_frame(const char *tag, const u8 *data, size_t len)
{
    size_t index;

    rt_kprintf("[wifi-sec] %s (%u bytes):", tag, (unsigned int) len);
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
#endif /* WIFI_SEC_DUMP_EAPOL */

/* Supplied by libsupplicant.a (src/crypto/sha1-prf.c): the PRF that turns
 * PMK + both MAC addresses + both nonces into the PTK. */
extern int sha1_prf(const u8 *key, size_t key_len, const char *label,
                    const u8 *data, size_t data_len, u8 *buf, size_t buf_len);

/* The AP checks a message 2/4 by hashing the received frame with the MIC field
 * treated as zero, using KCK = PRF(PMK, "Pairwise key expansion",
 * Min(AA,SPA)||Max(AA,SPA)||Min(ANonce,SNonce)||Max(ANonce,SNonce)).
 * Reproducing that here converts "the handshake silently times out" into an
 * immediate verdict: OK means the AP will accept the frame. */
static u8 s_last_anonce[32];
static bool s_last_anonce_valid;

static u32 security_be16(const u8 *value)
{
    return ((u32) value[0] << 8) | (u32) value[1];
}

static void security_store_anonce(const u8 *eapol, size_t len)
{
    u32 key_info;

    /* IEEE 802.1X header (4) + EAPOL-Key body. */
    if ((eapol == NULL) || (len < (4U + 49U)) || (eapol[1] != 3U))
    {
        return;
    }

    key_info = security_be16(eapol + 5U);
    /* Message 1/4: pairwise + ACK set, MIC clear. */
    if (((key_info & 0x0008U) != 0U) && ((key_info & 0x0080U) != 0U) &&
        ((key_info & 0x0100U) == 0U))
    {
        memcpy(s_last_anonce, eapol + 4U + 13U, sizeof(s_last_anonce));
        s_last_anonce_valid = true;
    }
}

static void security_verify_msg2(const struct wifi_security_manager *manager,
                                 const u8 *dest, const u8 *frame,
                                 size_t len)
{
    u8 data[2U * ETH_ALEN + 2U * 32U];
    u8 prf_out[64];
    u8 mic[32];
    u8 body[121];
    const u8 *own_addr;
    const u8 *first;
    const u8 *second;
    bool matches;

    if ((manager == NULL) || (dest == NULL) || (frame == NULL) ||
        (len != sizeof(body)) || !s_last_anonce_valid)
    {
        return;
    }

    own_addr = wpa_driver_fc80211_get_macaddr(manager->bss);
    if (own_addr == NULL)
    {
        return;
    }

    if (memcmp(own_addr, dest, ETH_ALEN) < 0)
    {
        memcpy(data, own_addr, ETH_ALEN);
        memcpy(data + ETH_ALEN, dest, ETH_ALEN);
    }
    else
    {
        memcpy(data, dest, ETH_ALEN);
        memcpy(data + ETH_ALEN, own_addr, ETH_ALEN);
    }

    /* The SNonce is the nonce the library just wrote into message 2/4. */
    if (memcmp(s_last_anonce, frame + 17U, 32U) < 0)
    {
        first = s_last_anonce;
        second = frame + 17U;
    }
    else
    {
        first = frame + 17U;
        second = s_last_anonce;
    }
    memcpy(data + 2U * ETH_ALEN, first, 32U);
    memcpy(data + 2U * ETH_ALEN + 32U, second, 32U);

    if (sha1_prf(manager->pmk, manager->pmk_len, "Pairwise key expansion",
                 data, sizeof(data), prf_out, sizeof(prf_out)) != 0)
    {
        return;
    }

    memcpy(body, frame, sizeof(body));
    memset(body + 81U, 0, 16U);
    if (wpa_eapol_key_mic(prf_out, 16U, (int) WPA_KEY_MGMT_PSK, 2,
                          body, sizeof(body), mic) != 0)
    {
        return;
    }

    matches = (memcmp(mic, frame + 81U, 16U) == 0);
    if (!matches)
    {
        rt_kprintf("[wifi-sec] msg2 MIC verification failed\n");
    }
}

static void security_set_state(void *ctx, enum wpa_states state)
{
    struct wifi_security_manager *manager = ctx;

    if (manager == NULL)
    {
        return;
    }

    manager->state = state;
    manager->key_done = (state == WPA_COMPLETED);
    if (manager->callbacks.state_changed != NULL)
    {
        manager->callbacks.state_changed(manager->user_ctx, state);
    }
}

static enum wpa_states security_get_state(void *ctx)
{
    const struct wifi_security_manager *manager = ctx;

    return manager != NULL ? manager->state : WPA_DISCONNECTED;
}

static void security_deauthenticate(void *ctx, u16 reason_code)
{
    struct wifi_security_manager *manager = ctx;

    if (manager == NULL)
    {
        return;
    }

    manager->key_done = false;
    if (manager->callbacks.deauthenticate != NULL)
    {
        manager->callbacks.deauthenticate(manager->user_ctx, reason_code);
    }
}

static void security_reconnect(void *ctx)
{
    struct wifi_security_manager *manager = ctx;

    if ((manager != NULL) && (manager->callbacks.reconnect != NULL))
    {
        manager->callbacks.reconnect(manager->user_ctx);
    }
}

static int security_set_key(void *ctx, enum wpa_alg alg, const u8 *addr,
                            int key_idx, int set_tx, const u8 *seq,
                            size_t seq_len, const u8 *key, size_t key_len,
                            enum key_flag key_flag)
{
    struct wifi_security_manager *manager = ctx;
    struct wpa_driver_set_key_params params;
    int status;

    if ((manager == NULL) || (manager->bss == NULL))
    {
        return -1;
    }

    memset(&params, 0, sizeof(params));
    params.ifname = manager->ifname;
    params.alg = alg;
    params.addr = addr;
    params.key_idx = key_idx;
    params.set_tx = set_tx;
    params.seq = seq;
    params.seq_len = seq_len;
    params.key = key;
    params.key_len = key_len;
    params.key_flag = key_flag;

    status = wpa_driver_fc80211_set_key(manager->bss, &params);
    if (status < 0)
    {
        rt_kprintf("[wifi-sec] set_key failed: %d\n", status);
    }
    return status;
}

static void *security_get_network_ctx(void *ctx)
{
    return ctx;
}

static int security_get_bssid(void *ctx, u8 *bssid)
{
    struct wifi_security_manager *manager = ctx;

    if ((manager == NULL) || (manager->bss == NULL) || (bssid == NULL))
    {
        return -1;
    }

    return wpa_driver_fc80211_get_bssid(manager->bss, bssid);
}

static int security_ether_send(void *ctx, const u8 *dest, u16 proto,
                               const u8 *buf, size_t len)
{
    struct wifi_security_manager *manager = ctx;
    extern int i3ed11_l2data_from_sp(int ifindex, unsigned char *data,
                                     unsigned int length, bool hasMacHeader,
                                     const char *dst);
    int status;

    if ((manager == NULL) || (manager->bss == NULL) || (dest == NULL) ||
        (buf == NULL) || (len == 0U))
    {
        return -1;
    }

    (void) proto;

    status = i3ed11_l2data_from_sp(manager->bss->ifindex,
                                   (unsigned char *) buf, (unsigned int) len,
                                   false, (const char *) dest);
    /* Nothing is written to the console before the transmit: an AP discards a
     * message 2/4 whose replay counter is older than the message 1/4 it is
     * waiting on, so the frame has to leave as early as possible. */
    if (status < 0)
    {
        rt_kprintf("[wifi-sec] EAPOL TX failed: %d\n", status);
    }

#if WIFI_SEC_DUMP_EAPOL
    /* Message 2/4 is 121 bytes: 4 (EAPOL) + 77 (key header) + 16 (MIC) +
     * 2 (key data length) + 22 (RSN IE). */
    if ((len == 121U) && (buf[1] == 3U))
    {
        security_dump_frame("EAPOL-Key 2/4", buf, len);
    }
#endif /* WIFI_SEC_DUMP_EAPOL */

    /* Recompute what the AP will compute, so a MIC mismatch is reported here
     * instead of only showing up as an AP retransmission. */
    if ((len == 121U) && (buf[1] == 3U))
    {
        security_verify_msg2(manager, dest, buf, len);
    }

    return status < 0 ? status : 0;
}

static u8 *security_alloc_eapol(void *ctx, u8 type, const void *data,
                                u16 data_len, size_t *msg_len,
                                void **data_pos)
{
    struct wifi_security_eapol_hdr *hdr;
    u8 *msg;
    size_t total_len = sizeof(*hdr) + data_len;

    (void) ctx;
    msg = os_malloc(total_len);
    if (msg == NULL)
    {
        return NULL;
    }

    hdr = (struct wifi_security_eapol_hdr *) msg;
    hdr->version = WIFI_SECURITY_EAPOL_VERSION;
    hdr->type = type;
    hdr->length = host_to_be16(data_len);

    /* The vendor implementation (wpa_supplicant/wpas_glue.c:
     * wpa_alloc_eapol()) zeroes the whole payload area when the caller passes
     * no initial data: message 2/4 is built that way and the library only
     * fills the fields it sets, leaving the EAPOL-Key IV/RSC/Key ID and - most
     * importantly - the MIC field untouched.  wpa_eapol_key_mic() then hashes
     * the buffer as-is, so an uninitialised MIC field would make the AP reject
     * every message 2/4 (it hashes the frame with that field treated as zero). */
    if ((data != NULL) && (data_len != 0U))
    {
        memcpy(hdr + 1, data, data_len);
    }
    else if (data_len != 0U)
    {
        memset(hdr + 1, 0, data_len);
    }

    if (msg_len != NULL)
    {
        *msg_len = total_len;
    }
    if (data_pos != NULL)
    {
        *data_pos = hdr + 1;
    }

    return msg;
}

static int security_add_pmkid(void *ctx, void *network_ctx,
                              const u8 *bssid, const u8 *pmkid,
                              const u8 *fils_cache_id, const u8 *pmk,
                              size_t pmk_len, u32 pmk_lifetime,
                              u8 pmk_reauth_threshold, int akmp)
{
    (void) ctx;
    (void) network_ctx;
    (void) bssid;
    (void) pmkid;
    (void) fils_cache_id;
    (void) pmk;
    (void) pmk_len;
    (void) pmk_lifetime;
    (void) pmk_reauth_threshold;
    (void) akmp;
    return 0;
}

static int security_remove_pmkid(void *ctx, void *network_ctx,
                                 const u8 *bssid, const u8 *pmkid,
                                 const u8 *fils_cache_id)
{
    (void) ctx;
    (void) network_ctx;
    (void) bssid;
    (void) pmkid;
    (void) fils_cache_id;
    return 0;
}

static int security_setprotection(void *ctx, const u8 *addr,
                                  int protection_type, int key_type)
{
    (void) ctx;
    (void) addr;
    (void) protection_type;
    (void) key_type;
    return 0;
}

static int security_get_beacon_ie(void *ctx)
{
    (void) ctx;
    return 0;
}

static void security_cancel_auth_timeout(void *ctx)
{
    (void) ctx;
}

const u8 *wifi_security_manager_assoc_ie(size_t *len)
{
    if (len != NULL)
    {
        *len = sizeof(s_wpa2_psk_rsn_ie);
    }
    return s_wpa2_psk_rsn_ie;
}

int wifi_security_manager_init(struct wifi_security_manager *manager,
                               const struct wifi_security_config *config)
{
    struct rsn_supp_config rsn_config;
    const UCHAR *mac_addr;
    size_t name_len;

    if ((manager == NULL) || (config == NULL) || (config->bss == NULL) ||
        (config->ifname == NULL) || (config->ssid == NULL) ||
        (config->ssid_len == 0U) || (config->ssid_len > sizeof(manager->ssid)) ||
        (config->own_addr == NULL) || (config->mode != WIFI_SECURITY_WPA2_PSK) ||
        (config->pmk == NULL) || (config->pmk_len != sizeof(manager->pmk)))
    {
        return -RT_EINVAL;
    }

    wifi_security_manager_deinit(manager);
    memset(manager, 0, sizeof(*manager));
    manager->bss = config->bss;
    manager->user_ctx = config->user_ctx;
    manager->callbacks = config->callbacks;
    manager->mode = config->mode;
    manager->state = WPA_DISCONNECTED;
    manager->ssid_len = config->ssid_len;
    manager->pmk_len = config->pmk_len;
    memcpy(manager->ssid, config->ssid, config->ssid_len);
    memcpy(manager->pmk, config->pmk, config->pmk_len);

    name_len = strlen(config->ifname);
    if (name_len > WIFI_SECURITY_MAX_IFNAME)
    {
        name_len = WIFI_SECURITY_MAX_IFNAME;
    }
    memcpy(manager->ifname, config->ifname, name_len);
    manager->ifname[name_len] = '\0';

    manager->sm_ctx = os_zalloc(sizeof(*manager->sm_ctx));
    if (manager->sm_ctx == NULL)
    {
        return -RT_ENOMEM;
    }

    manager->sm_ctx->ctx = manager;
    manager->sm_ctx->msg_ctx = manager;
    manager->sm_ctx->set_state = security_set_state;
    manager->sm_ctx->get_state = security_get_state;
    manager->sm_ctx->deauthenticate = security_deauthenticate;
    manager->sm_ctx->reconnect = security_reconnect;
    manager->sm_ctx->set_key = security_set_key;
    manager->sm_ctx->get_network_ctx = security_get_network_ctx;
    manager->sm_ctx->get_bssid = security_get_bssid;
    manager->sm_ctx->ether_send = security_ether_send;
    manager->sm_ctx->get_beacon_ie = security_get_beacon_ie;
    manager->sm_ctx->cancel_auth_timeout = security_cancel_auth_timeout;
    manager->sm_ctx->alloc_eapol = security_alloc_eapol;
    manager->sm_ctx->add_pmkid = security_add_pmkid;
    manager->sm_ctx->remove_pmkid = security_remove_pmkid;
    manager->sm_ctx->mlme_setprotection = security_setprotection;

    manager->sm = wpa_sm_init(manager->sm_ctx);
    if (manager->sm == NULL)
    {
        wifi_security_manager_deinit(manager);
        return -RT_ENOMEM;
    }

    memset(&rsn_config, 0, sizeof(rsn_config));
    rsn_config.network_ctx = manager;
    rsn_config.allowed_pairwise_cipher = WPA_CIPHER_CCMP;
    rsn_config.ssid = manager->ssid;
    rsn_config.ssid_len = manager->ssid_len;
    wpa_sm_set_config(manager->sm, &rsn_config);

    if ((wpa_sm_set_param(manager->sm, WPA_PARAM_PROTO, WPA_PROTO_RSN) != 0) ||
        (wpa_sm_set_param(manager->sm, WPA_PARAM_PAIRWISE, WPA_CIPHER_CCMP) != 0) ||
        (wpa_sm_set_param(manager->sm, WPA_PARAM_GROUP, WPA_CIPHER_CCMP) != 0) ||
        (wpa_sm_set_param(manager->sm, WPA_PARAM_KEY_MGMT, WPA_KEY_MGMT_PSK) != 0) ||
        (wpa_sm_set_param(manager->sm, WPA_PARAM_RSN_ENABLED, 1U) != 0))
    {
        wifi_security_manager_deinit(manager);
        return -RT_ERROR;
    }

    mac_addr = wpa_driver_fc80211_get_macaddr(manager->bss);
    if (mac_addr == NULL)
    {
        wifi_security_manager_deinit(manager);
        return -RT_ERROR;
    }
    wpa_sm_set_own_addr(manager->sm, mac_addr);
    wpa_sm_set_ifname(manager->sm, manager->ifname);
    wpa_sm_set_eapol(manager->sm, NULL);
    if ((wpa_sm_set_assoc_wpa_ie(manager->sm, s_wpa2_psk_rsn_ie,
                                 sizeof(s_wpa2_psk_rsn_ie)) != 0) ||
        (wpa_sm_set_ap_rsn_ie(manager->sm, NULL, 0U) != 0))
    {
        wifi_security_manager_deinit(manager);
        return -RT_ERROR;
    }
    wpa_sm_set_pmk(manager->sm, manager->pmk, manager->pmk_len, NULL, NULL);
    return RT_EOK;
}

void wifi_security_manager_deinit(struct wifi_security_manager *manager)
{
    if (manager == NULL)
    {
        return;
    }
    if (manager->sm != NULL)
    {
        wpa_sm_notify_disassoc(manager->sm);
        wpa_sm_deinit(manager->sm);
        /* wpa_sm_deinit() owns and frees sm->ctx. */
        manager->sm = NULL;
        manager->sm_ctx = NULL;
    }
    if (manager->sm_ctx != NULL)
    {
        /* This is only reached when wpa_sm_init() failed before taking
         * ownership of the context. */
        os_free(manager->sm_ctx);
        manager->sm_ctx = NULL;
    }
    memset(manager, 0, sizeof(*manager));
    manager->state = WPA_DISCONNECTED;
}

int wifi_security_manager_notify_assoc(struct wifi_security_manager *manager,
                                       const u8 *bssid,
                                       const u8 *ap_rsn_ie,
                                       size_t ap_rsn_ie_len)
{
    int status;

    if ((manager == NULL) || (manager->sm == NULL) || (bssid == NULL))
    {
        return -RT_EINVAL;
    }
    status = wpa_sm_set_ap_rsn_ie(manager->sm, ap_rsn_ie, ap_rsn_ie_len);
    if (status != 0)
    {
        return status;
    }
    manager->key_done = false;
    manager->state = WPA_ASSOCIATED;
    wpa_sm_notify_assoc(manager->sm, bssid);
    return RT_EOK;
}

void wifi_security_manager_notify_disassoc(
    struct wifi_security_manager *manager)
{
    if ((manager != NULL) && (manager->sm != NULL))
    {
        wpa_sm_notify_disassoc(manager->sm);
        manager->state = WPA_DISCONNECTED;
        manager->key_done = false;
    }
}

int wifi_security_manager_rx_eapol(struct wifi_security_manager *manager,
                                   const u8 *src_addr,
                                   const u8 *buf, size_t len)
{
    if ((manager == NULL) || (manager->sm == NULL) || (src_addr == NULL) ||
        (buf == NULL) || (len == 0U))
    {
        return -RT_EINVAL;
    }
    security_store_anonce(buf, len);
    return wpa_sm_rx_eapol(manager->sm, src_addr, buf, len);
}

int wifi_security_manager_set_assoc_ie(struct wifi_security_manager *manager,
                                       const u8 *ie, size_t ie_len)
{
    if ((manager == NULL) || (manager->sm == NULL) || (ie == NULL) ||
        (ie_len == 0U))
    {
        return -RT_EINVAL;
    }
    return wpa_sm_set_assoc_wpa_ie(manager->sm, ie, ie_len);
}

bool wifi_security_manager_is_key_done(
    const struct wifi_security_manager *manager)
{
    return (manager != NULL) && manager->key_done;
}

enum wpa_states wifi_security_manager_get_state(
    const struct wifi_security_manager *manager)
{
    return manager != NULL ? manager->state : WPA_DISCONNECTED;
}
