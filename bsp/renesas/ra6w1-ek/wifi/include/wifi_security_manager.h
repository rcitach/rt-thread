#ifndef WIFI_SECURITY_MANAGER_H
#define WIFI_SECURITY_MANAGER_H

#include <stdbool.h>
#include <stddef.h>

#include "driver_fc80211.h"
#include "wifi_wpa_sm_port.h"

enum wifi_security_mode
{
    WIFI_SECURITY_OPEN = 0,
    WIFI_SECURITY_WPA2_PSK,
};

struct wifi_security_manager;

struct wifi_security_callbacks
{
    void (*state_changed)(void *ctx, enum wpa_states state);
    void (*deauthenticate)(void *ctx, u16 reason_code);
    void (*reconnect)(void *ctx);
};

struct wifi_security_manager
{
    struct wpa_sm *sm;
    struct wpa_sm_ctx *sm_ctx;
    struct i802_bss *bss;
    void *user_ctx;
    struct wifi_security_callbacks callbacks;
    enum wifi_security_mode mode;
    enum wpa_states state;
    bool key_done;
    char ifname[16];
    u8 ssid[32];
    size_t ssid_len;
    u8 pmk[32];
    size_t pmk_len;
};

struct wifi_security_config
{
    struct i802_bss *bss;
    const char *ifname;
    const u8 *own_addr;
    const u8 *ssid;
    size_t ssid_len;
    const u8 *pmk;
    size_t pmk_len;
    enum wifi_security_mode mode;
    void *user_ctx;
    struct wifi_security_callbacks callbacks;
};

int wifi_security_manager_init(struct wifi_security_manager *manager,
                               const struct wifi_security_config *config);
void wifi_security_manager_deinit(struct wifi_security_manager *manager);

int wifi_security_manager_notify_assoc(struct wifi_security_manager *manager,
                                       const u8 *bssid,
                                       const u8 *ap_rsn_ie,
                                       size_t ap_rsn_ie_len);
void wifi_security_manager_notify_disassoc(
    struct wifi_security_manager *manager);
int wifi_security_manager_rx_eapol(struct wifi_security_manager *manager,
                                   const u8 *src_addr,
                                   const u8 *buf,
                                   size_t len);

/* Replace the RSN IE that EAPOL-Key message 2/4 reports as the station's own
 * IE.  The authenticator compares that element against the RSN IE it received
 * in the (Re)Association Request, so the caller should hand over the request
 * IE captured from the driver's CONNECT response when one is available. */
int wifi_security_manager_set_assoc_ie(struct wifi_security_manager *manager,
                                       const u8 *ie, size_t ie_len);

bool wifi_security_manager_is_key_done(
    const struct wifi_security_manager *manager);
enum wpa_states wifi_security_manager_get_state(
    const struct wifi_security_manager *manager);
const u8 *wifi_security_manager_assoc_ie(size_t *len);

#endif /* WIFI_SECURITY_MANAGER_H */
