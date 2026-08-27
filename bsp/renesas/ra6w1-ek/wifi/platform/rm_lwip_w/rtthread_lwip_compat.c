/* RT-Thread lwIP compatibility for the WiFi network-management glue. */

#include "lwip/opt.h"
#include "lwip/netif.h"
#include "lwip/dhcp.h"
#include "lwip/prot/dhcp.h"

/* The original SDK exported this wrapper around lwIP's private report helper.
 * RT-Thread lwIP already emits reports from the public netif setters. */
void rm_netif_issue_reports(struct netif *netif, u8_t report_type)
{
    LWIP_UNUSED_ARG(netif);
    LWIP_UNUSED_ARG(report_type);
}

/* The WiFi manager only needs to inspect the current public DHCP state. */
u8_t dhcp_get_state(const struct netif *netif)
{
    const struct dhcp *dhcp = netif_dhcp_data(netif);

    return dhcp != NULL ? dhcp->state : DHCP_STATE_OFF;
}
