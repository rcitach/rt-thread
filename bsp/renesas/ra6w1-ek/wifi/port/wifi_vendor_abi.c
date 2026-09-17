/* Closed Wi-Fi archive ABI; no netif or DHCP ownership belongs here. */
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <rtthread.h>
#include <dev_wlan.h>
#include <lwip/ip_addr.h>
#include <netdev.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "wifi_wlan_port.h"

#define WLAN1_IFACE 1U
extern bool bsp_tcs_otp_read_mac(uint32_t *mac) __attribute__((weak));
extern QueueHandle_t TO_SUPP_QUEUE;
extern QueueHandle_t TO_CLI_QUEUE;
extern int ra6wx_eloop_init(void);
static QueueHandle_t supp_queue;

/* The supplicant owns both message queues.  Its closed archive creates them
 * through the FreeRTOS wrapper, which maps each queue to a native RT-Thread
 * message queue.  Calling ra6wx_eloop_init() is required even when RT-Thread
 * supplies the event worker: it creates TO_SUPP_QUEUE and TO_CLI_QUEUE and
 * initializes the vendor eloop state used by the driver tasks. */
rt_mq_t wifi_vendor_event_queue_init(void)
{
    StaticQueue_t *descriptor;

    if (TO_SUPP_QUEUE == RT_NULL)
    {
        if (ra6wx_eloop_init() != 0)
            return RT_NULL;
    }
    if (TO_SUPP_QUEUE == RT_NULL)
        return RT_NULL;

    /* QueueDefinition and StaticQueue_t intentionally share rt_ipc as their
     * first ABI member.  The wrapper exposes the native message queue there. */
    supp_queue = TO_SUPP_QUEUE;
    descriptor = (StaticQueue_t *)supp_queue;
    return (rt_mq_t)descriptor->rt_ipc;
}

void wifi_vendor_event_queue_deinit(void)
{
    QueueHandle_t event_queue = supp_queue;
    QueueHandle_t cli_queue = TO_CLI_QUEUE;

    if (event_queue)
    {
        vQueueDelete(event_queue);
        supp_queue = RT_NULL;
    }
    if (cli_queue && (cli_queue != event_queue))
        vQueueDelete(cli_queue);
    TO_SUPP_QUEUE = RT_NULL;
    TO_CLI_QUEUE = RT_NULL;
}

#ifdef ipaddr_aton
#undef ipaddr_aton
#endif
int ipaddr_aton(const char *cp, ip4_addr_t *addr)
{
    return ip4addr_aton(cp, addr);
}

void rm_wifi_event_signal_connected(uint8_t *ssid, uint8_t ssid_len,
                                    uint8_t *bssid, int security,
                                    uint8_t chan_info)
{
    (void) ssid;
    (void) ssid_len;
    (void) bssid;
    (void) security;
    (void) chan_info;
}

void rm_wifi_event_signal_disconnected(uint16_t reason)
{
    (void) reason;
}

void rm_wifi_event_signal_ap_sta_disconnected(uint16_t reason,
                                              uint8_t *ucMac)
{
    (void) reason;
    (void) ucMac;
}

void rm_wifi_event_signal_twt(int twt_event)
{
    (void) twt_event;
}

int rm_wifi_get_int_val_from_str(char *param, int *int_val, int policy)
{
    char *end;
    long value;

    (void) policy;
    if ((param == NULL) || (int_val == NULL))
    {
        return -1;
    }

    value = strtol(param, &end, 0);
    if ((end == param) || (*end != '\0') ||
        (value < INT32_MIN) || (value > INT32_MAX))
    {
        return -1;
    }

    *int_val = (int) value;
    return 0;
}

int rm_wifi_helper_security_type_get(unsigned int key_mgmt,
                                     unsigned int proto,
                                     unsigned int pairwise_cipher)
{
    (void) key_mgmt;
    (void) proto;
    (void) pairwise_cipher;
    return 6;
}

uint32_t rm_wifi_build_sku_id_get(void)
{
    return 0U;
}

int rm_wifi_ps_mode_get(bool *config)
{
    if (config != NULL)
    {
        *config = false;
    }

    return 6;
}

int rm_wifi_is_wpa_state(uint8_t state, int max_delay_counter)
{
    (void) state;
    (void) max_delay_counter;
    return 6;
}

/*
 * The closed driver asks for the MAC in a split 16/32-bit representation
 * during startup.  Keep the board's historical default when the persistent
 * RM helper is not linked; WLAN1 receives the next address.
 */
int getMacAddrMswLsw(unsigned int iface, unsigned long *macmsw,
                    unsigned long *maclsw)
{
    unsigned long default_msw = 0xEC9FUL;
    unsigned long default_lsw = 0x0D9FFFFEUL;
    uint32_t otp_mac[2];

    if ((macmsw == NULL) || (maclsw == NULL))
    {
        return -1;
    }

    if ((bsp_tcs_otp_read_mac != NULL) && bsp_tcs_otp_read_mac(otp_mac))
    {
        default_msw = otp_mac[1];
        default_lsw = otp_mac[0];
    }

    if (iface == WLAN1_IFACE)
    {
        default_lsw++;
    }

    *macmsw = default_msw;
    *maclsw = default_lsw;
    return 0;
}

/* DPM is intentionally outside the standalone adapter. */
int ra6w1_get_fast_connection_mode(void)
{
    return 0;
}

/* Resolve by the WLAN framework's netdev, never by an assumed netif index. */
unsigned int get_ip_info(int iface, int field, char *result)
{
    struct netdev *netdev = wifi_wlan_device()->netdev;
    if (!result)
        return 0;
    result[0] = '\0';
    if (iface != 0 || !netdev)
        return 0;
    switch (field)
    {
    case 0:
        rt_snprintf(result, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
                    netdev->hwaddr[0], netdev->hwaddr[1], netdev->hwaddr[2],
                    netdev->hwaddr[3], netdev->hwaddr[4], netdev->hwaddr[5]);
        break;
    case 1: ipaddr_ntoa_r(&netdev->ip_addr, result, 18); break;
    case 2: ipaddr_ntoa_r(&netdev->netmask, result, 18); break;
    case 3: ipaddr_ntoa_r(&netdev->gw, result, 18); break;
    case 4: rt_snprintf(result, 18, "%u", netdev->mtu); break;
    case 5: ipaddr_ntoa_r(&netdev->dns_servers[0], result, 18); break;
    case 6: ipaddr_ntoa_r(&netdev->dns_servers[1], result, 18); break;
    default: return 0;
    }
    return 1;
}

/* The SDK sometimes toggles its legacy netif from cfg80211 helpers. WLAN's
 * connect/disconnect indications are the sole link-state authority here. */
void __wrap_wifi_netif_control(int interface, int flag)
{
    (void)interface;
    (void)flag;
}
