/* Vendor types only. OS operations in the port use RT-Thread APIs. */
#ifndef WIFI_VENDOR_H
#define WIFI_VENDOR_H

#include <rtthread.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "co_int.h"
typedef unsigned long uint32;
#include "supp_def.h"
#include "supp_types.h"
#include "rwnx_cfg.h"
#include "driver_fc80211.h"

rt_mq_t wifi_vendor_event_queue_init(void);
void wifi_vendor_event_queue_deinit(void);
int getMacAddrMswLsw(unsigned int iface, unsigned long *macmsw,
                     unsigned long *maclsw);
int wifi_wpa_timers_init(rt_mq_t queue);
void wifi_wpa_timers_deinit(void);
void wifi_wpa_timers_run(void);

#endif
