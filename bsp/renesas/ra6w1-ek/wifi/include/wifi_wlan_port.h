#ifndef WIFI_WLAN_PORT_H
#define WIFI_WLAN_PORT_H

#include <rtthread.h>
struct rt_wlan_device;
struct rt_wlan_device *wifi_wlan_device(void);

struct wifi_wlan_stats
{
    rt_uint32_t rx_packets, rx_bytes, rx_dropped;
    rt_uint32_t rx_state_dropped, rx_input_rejected;
    rt_uint32_t tx_packets, tx_bytes, queue_rejected, event_rejected;
    rt_uint32_t tx_copied, min_free_txq;
};
void wifi_wlan_stats_reset(void);
void wifi_wlan_stats_get(struct wifi_wlan_stats *stats);

#endif
