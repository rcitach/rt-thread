/*
 * Wi-Fi/lwIP adapter for the standalone cfg80211 path.
 *
 * The vendor archives use a small ABI around lwIP pbufs. This file provides
 * that ABI without pulling in the old RM_LWIP_W network-management layer.
 * The application remains responsible for creating/configuring the netif,
 * DHCP, routing and link state.
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "lwip/err.h"
#include "lwip/etharp.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "lwip/netifapi.h"
#include "lwip/pbuf.h"
#include "lwip/tcpip.h"

/* The vendor archive was built against a lwIP configuration where
 * ipaddr_aton was emitted as a function. In the IPv4-only lwIP build it is
 * normally a macro to ip4addr_aton, so export the expected symbol here. */
#ifdef ipaddr_aton
#undef ipaddr_aton
#endif
int ipaddr_aton(const char *cp, ip4_addr_t *addr)
{
    return ip4addr_aton(cp, addr);
}

#ifdef RT_USING_NETDEV
#include "netdev.h"
#endif

#ifndef WLAN0_IFACE
#define WLAN0_IFACE                 (0U)
#endif

#ifndef WLAN1_IFACE
#define WLAN1_IFACE                 (1U)
#endif

#ifndef RM_WIFI_NETIF_INDEX
#define RM_WIFI_NETIF_INDEX(iface)  ((iface) + 2U)
#endif

#ifndef WIFI_LWIP_IF_NAME
#define WIFI_LWIP_IF_NAME           "wlan0"
#endif

#define WIFI_LWIP_TASK              (20U)
#define WIFI_DRIVER_TASK            (17U)
#define WIFI_DRIVER_START_XMIT     (2U)

/* Values from the removed RM_LWIP_W net_ip_handler.h interface. */
#define WIFI_IP_INFO_MACADDR        (0)
#define WIFI_IP_INFO_IPADDR         (1)
#define WIFI_IP_INFO_SUBNET         (2)
#define WIFI_IP_INFO_GATEWAY        (3)
#define WIFI_IP_INFO_MTU            (4)
#define WIFI_IP_INFO_DNS            (5)
#define WIFI_IP_INFO_DNS_2ND        (6)
#define WIFI_NET_INFO_STR_LEN       (18U)

typedef unsigned long u32;

static struct netif s_wifi_netif;
static bool s_wifi_netif_ready;
static bool s_wifi_netif_link_up;
#ifdef RT_USING_NETDEV
static struct netdev s_wifi_netdev;
static bool s_wifi_netdev_ready;
#endif

extern int wifi_fc80211_demo_get_mac(uint8_t mac[6]);

err_t wifi_lwip_adapter_netif_init(struct netif *netif);

extern u32 rwnx_driver_task_send_event(u32 from, u32 to, u32 event,
                                       void *parameter);
extern int rwnx_free_txq_count(void);
extern int rwnx_empty_txq_item(void);
extern bool bsp_tcs_otp_read_mac(uint32_t *mac) __attribute__((weak));

static u8_t wifi_lwip_netif_index_from_driver(u8_t if_idx)
{
    /* Firmware builds use either VIF 0/1 or the legacy lwIP indices 2/3.
     * The standalone netif can have any actual index, so normalize all
     * WLAN0 representations to the netif created by this adapter. */
    if (s_wifi_netif_ready &&
        ((if_idx == WLAN0_IFACE) ||
         (if_idx == RM_WIFI_NETIF_INDEX(WLAN0_IFACE)) ||
         (if_idx == netif_get_index(&s_wifi_netif))))
    {
        return netif_get_index(&s_wifi_netif);
    }
    if (if_idx == WLAN0_IFACE || if_idx == WLAN1_IFACE)
    {
        return (u8_t) RM_WIFI_NETIF_INDEX(if_idx);
    }

    return if_idx;
}

#ifdef RT_USING_NETDEV
static struct netif *wifi_lwip_netif_from_netdev(struct netdev *netdev)
{
    return (netdev != NULL) ? (struct netif *) netdev->user_data : NULL;
}

static int wifi_netdev_set_up(struct netdev *netdev)
{
    struct netif *netif = wifi_lwip_netif_from_netdev(netdev);
    if (netif == NULL)
    {
        return -RT_ERROR;
    }
    return netifapi_netif_set_up(netif);
}

static int wifi_netdev_set_down(struct netdev *netdev)
{
    struct netif *netif = wifi_lwip_netif_from_netdev(netdev);
    if (netif == NULL)
    {
        return -RT_ERROR;
    }
    return netifapi_netif_set_down(netif);
}

static int wifi_netdev_set_addr_info(struct netdev *netdev,
                                     ip_addr_t *ip_addr,
                                     ip_addr_t *netmask, ip_addr_t *gw)
{
    ip4_addr_t current_ip;
    ip4_addr_t current_netmask;
    ip4_addr_t current_gw;
    struct netif *netif = wifi_lwip_netif_from_netdev(netdev);
    if (netif == NULL)
    {
        return -RT_EINVAL;
    }

    current_ip = *netif_ip4_addr(netif);
    current_netmask = *netif_ip4_netmask(netif);
    current_gw = *netif_ip4_gw(netif);
    if (ip_addr != NULL)
    {
        current_ip = *ip_2_ip4(ip_addr);
    }
    if (netmask != NULL)
    {
        current_netmask = *ip_2_ip4(netmask);
    }
    if (gw != NULL)
    {
        current_gw = *ip_2_ip4(gw);
    }
    return netifapi_netif_set_addr(netif, &current_ip,
                                   &current_netmask, &current_gw);
}

static int wifi_netdev_set_dns_server(struct netdev *netdev,
                                      uint8_t dns_num,
                                      ip_addr_t *dns_server)
{
    if ((netdev == NULL) || (dns_server == NULL) ||
        (dns_num >= NETDEV_DNS_SERVERS_NUM))
    {
        return -RT_EINVAL;
    }

    /* lwIP 2.1.2 uses the registered default netdev DNS table when
     * RT_USING_NETDEV is enabled. Do not call dns_setserver() here: its
     * implementation dispatches back to every netdev callback. */
    netdev_low_level_set_dns_server(netdev, dns_num, dns_server);
    return RT_EOK;
}

static int wifi_netdev_set_dhcp(struct netdev *netdev, rt_bool_t enabled)
{
    struct netif *netif = wifi_lwip_netif_from_netdev(netdev);
    if (netif == NULL)
    {
        return -RT_ERROR;
    }
    if (enabled)
    {
        int result = netifapi_dhcp_start(netif);
        if (result == ERR_OK)
        {
            netdev_low_level_set_dhcp_status(netdev, RT_TRUE);
        }
        return result;
    }

    {
        int result = netifapi_dhcp_stop(netif);
        if (result == ERR_OK)
        {
            netdev_low_level_set_dhcp_status(netdev, RT_FALSE);
        }
        return result;
    }
}

#ifdef RT_USING_FINSH
extern int lwip_netdev_ping(struct netdev *netdev, const char *host,
                            size_t data_len, uint32_t timeout,
                            struct netdev_ping_resp *ping_resp,
                            rt_bool_t isbind);
#endif

static int wifi_netdev_set_default(struct netdev *netdev)
{
    struct netif *netif = wifi_lwip_netif_from_netdev(netdev);
    if (netif == NULL)
    {
        return -RT_ERROR;
    }
    netif_set_default(netif);
    return RT_EOK;
}

static const struct netdev_ops s_wifi_netdev_ops =
{
    .set_up = wifi_netdev_set_up,
    .set_down = wifi_netdev_set_down,
    .set_addr_info = wifi_netdev_set_addr_info,
    .set_dns_server = wifi_netdev_set_dns_server,
    .set_dhcp = wifi_netdev_set_dhcp,
#ifdef RT_USING_FINSH
    .ping = lwip_netdev_ping,
    .netstat = NULL,
#endif
    .set_default = wifi_netdev_set_default,
};
#endif

int wifi_lwip_adapter_netif_start(void)
{
    ip4_addr_t any;
    uint8_t mac[6];
    err_t result;

    if (s_wifi_netif_ready)
    {
        return RT_EOK;
    }

    ip4_addr_set_zero(&any);
    result = netifapi_netif_add(&s_wifi_netif, &any, &any, &any, NULL,
                                wifi_lwip_adapter_netif_init, tcpip_input);
    if (result != ERR_OK)
    {
        return -RT_ERROR;
    }

    memset(mac, 0, sizeof(mac));
    if (wifi_fc80211_demo_get_mac(mac) == RT_EOK)
    {
        s_wifi_netif.hwaddr_len = 6U;
        memcpy(s_wifi_netif.hwaddr, mac, sizeof(mac));
    }
    netifapi_netif_set_down(&s_wifi_netif);
    netifapi_netif_set_link_down(&s_wifi_netif);
    s_wifi_netif_ready = true;

#ifdef RT_USING_NETDEV
    memset(&s_wifi_netdev, 0, sizeof(s_wifi_netdev));
    s_wifi_netdev.ops = &s_wifi_netdev_ops;
    s_wifi_netdev.user_data = &s_wifi_netif;
    s_wifi_netdev.hwaddr_len = s_wifi_netif.hwaddr_len;
    memcpy(s_wifi_netdev.hwaddr, s_wifi_netif.hwaddr,
           s_wifi_netif.hwaddr_len);
    s_wifi_netdev.mtu = s_wifi_netif.mtu;
    s_wifi_netdev.flags = NETDEV_FLAG_BROADCAST | NETDEV_FLAG_ETHARP;
    if (netdev_register(&s_wifi_netdev, WIFI_LWIP_IF_NAME,
                        &s_wifi_netif) != 0)
    {
        s_wifi_netif_ready = false;
        netifapi_netif_remove(&s_wifi_netif);
        return -RT_ERROR;
    }
    s_wifi_netdev_ready = true;
    netdev_low_level_set_dhcp_status(&s_wifi_netdev, RT_TRUE);
#endif
    return RT_EOK;
}

int wifi_lwip_adapter_netif_link_up(void)
{
    if (!s_wifi_netif_ready)
    {
        return -RT_ERROR;
    }
    netifapi_netif_set_link_up(&s_wifi_netif);
    netifapi_netif_set_up(&s_wifi_netif);
    s_wifi_netif_link_up = true;
#ifdef RT_USING_NETDEV
    if (s_wifi_netdev_ready)
    {
        netdev_low_level_set_link_status(&s_wifi_netdev, RT_TRUE);
        netdev_low_level_set_status(&s_wifi_netdev, RT_TRUE);
    }
#endif
    return RT_EOK;
}

int wifi_lwip_adapter_netif_link_down(void)
{
    if (!s_wifi_netif_ready)
    {
        return RT_EOK;
    }
#ifdef RT_USING_NETDEV
    if (s_wifi_netdev_ready)
    {
        (void) netifapi_dhcp_stop(&s_wifi_netif);
        netdev_low_level_set_link_status(&s_wifi_netdev, RT_FALSE);
        netdev_low_level_set_status(&s_wifi_netdev, RT_FALSE);
    }
#endif
    netifapi_netif_set_link_down(&s_wifi_netif);
    netifapi_netif_set_down(&s_wifi_netif);
    s_wifi_netif_link_up = false;
    return RT_EOK;
}

int wifi_lwip_adapter_dhcp_start(void)
{
    if (!s_wifi_netif_ready || !s_wifi_netif_link_up)
    {
        return -RT_ERROR;
    }
    return netifapi_dhcp_start(&s_wifi_netif);
}

int wifi_lwip_adapter_dhcp_stop(void)
{
    if (!s_wifi_netif_ready)
    {
        return RT_EOK;
    }
    return netifapi_dhcp_stop(&s_wifi_netif);
}

/* Called by librwnx_drv when a frame has been received from the firmware. */
void rrq16x_lwip_mbox_rx(struct pbuf *p)
{
    struct netif *netif;

    if (p == NULL)
    {
        return;
    }

    p->if_idx = wifi_lwip_netif_index_from_driver(p->if_idx);
    netif = netif_get_by_index(p->if_idx);

    if ((netif == NULL) || (netif->input == NULL) ||
        !netif_is_up(netif) || !netif_is_link_up(netif))
    {
        pbuf_free(p);
        return;
    }

    if (netif->input(p, netif) != ERR_OK)
    {
        pbuf_free(p);
    }
}

/* Link-output callback for an application-created RT-Thread netif. */
err_t wifi_lwip_adapter_linkoutput(struct netif *netif, struct pbuf *p)
{
    u32 status;

    if ((netif == NULL) || (p == NULL))
    {
        return ERR_ARG;
    }

    if (!netif_is_up(netif) || !netif_is_link_up(netif))
    {
        pbuf_free(p);
        return ERR_IF;
    }

    if ((rwnx_free_txq_count() <= 0) || (rwnx_empty_txq_item() != 0))
    {
        return ERR_MEM;
    }

    if (p->if_idx == 0U)
    {
        p->if_idx = netif_get_index(netif);
    }

    /* The driver task consumes the VIF index, not the lwIP netif index.
     * The standalone netif is commonly index 1, while older RM builds use
     * index 2 for WLAN0; accept both forms. */
    if ((p->if_idx == netif_get_index(netif)) ||
        (p->if_idx == RM_WIFI_NETIF_INDEX(WLAN0_IFACE)))
    {
        p->if_idx = WLAN0_IFACE;
    }
    else if ((p->if_idx == RM_WIFI_NETIF_INDEX(WLAN1_IFACE)) &&
             (WLAN1_IFACE != WLAN0_IFACE))
    {
        p->if_idx = WLAN1_IFACE;
    }

    pbuf_ref(p);
    status = rwnx_driver_task_send_event(WIFI_LWIP_TASK, WIFI_DRIVER_TASK,
                                         WIFI_DRIVER_START_XMIT, p);
    if (status == 0U)
    {
        pbuf_free(p);
        return ERR_MEM;
    }

    return ERR_OK;
}

/* Convenience initializer for a netif created by the application. */
err_t wifi_lwip_adapter_netif_init(struct netif *netif)
{
    if (netif == NULL)
    {
        return ERR_ARG;
    }

    strncpy(netif->name, WIFI_LWIP_IF_NAME, NETIF_NAMESIZE - 1U);
    netif->name[NETIF_NAMESIZE - 1U] = '\0';
    netif->mtu = 1500U;
    netif->flags |= NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP |
                    NETIF_FLAG_LINK_UP;
    netif->output = etharp_output;
    netif->linkoutput = wifi_lwip_adapter_linkoutput;

    return ERR_OK;
}

/*
 * Compatibility symbols referenced by the vendor archives. The standalone
 * application owns connection state and events, so these legacy RM_WIFI
 * notifications intentionally do not start the old service layer.
 */
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

/*
 * Legacy IP query ABI retained for archive compatibility.  DHCP, DNS and
 * address assignment remain owned by the application's RT-Thread lwIP code.
 */
unsigned int get_ip_info(int iface_flag, int info_flag, char *result_str)
{
    struct netif *netif;

    if (result_str == NULL)
    {
        return 0U;
    }

    result_str[0] = '\0';
    netif = netif_get_by_index((u8_t)(iface_flag + RM_WIFI_NETIF_INDEX(0U)));
    if (netif == NULL)
    {
        return 0U;
    }

    switch (info_flag)
    {
        case WIFI_IP_INFO_MACADDR:
            (void)snprintf(result_str, WIFI_NET_INFO_STR_LEN,
                           "%02X:%02X:%02X:%02X:%02X:%02X",
                           netif->hwaddr[0], netif->hwaddr[1],
                           netif->hwaddr[2], netif->hwaddr[3],
                           netif->hwaddr[4], netif->hwaddr[5]);
            break;
        case WIFI_IP_INFO_IPADDR:
            (void)snprintf(result_str, WIFI_NET_INFO_STR_LEN, "%s",
                           ipaddr_ntoa(&netif->ip_addr));
            break;
        case WIFI_IP_INFO_SUBNET:
            (void)snprintf(result_str, WIFI_NET_INFO_STR_LEN, "%s",
                           ipaddr_ntoa(&netif->netmask));
            break;
        case WIFI_IP_INFO_GATEWAY:
            (void)snprintf(result_str, WIFI_NET_INFO_STR_LEN, "%s",
                           ipaddr_ntoa(&netif->gw));
            break;
        case WIFI_IP_INFO_MTU:
            (void)snprintf(result_str, WIFI_NET_INFO_STR_LEN, "%u",
                           (unsigned int)netif->mtu);
            break;
        default:
            return 0U;
    }

    return 1U;
}
