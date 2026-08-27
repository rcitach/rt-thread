/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include "lwip/opt.h"
#include "lwip/sys.h"

#include "FreeRTOS.h"
#include "event_groups.h"

#if LWIP_IPV4 && LWIP_ARP /* don't build if not configured for use in lwipopts.h */

#include "lwip/etharp.h"
#include "lwip/prot/iana.h"
#include "netif/ethernet.h"
#include "net_arp.h"
#include "iface_defs.h"
#include "rm_etharp.h"

#include <string.h>

#define ETHARPING_RECV_EVT_BITS     (1)

#if CFG_PMGR
extern void *get_arp_table();
#endif /* CFG_PMGR */

extern err_t
dhcp_etharp_request_dst(struct netif *netif, const ip4_addr_t *ipaddr, const struct eth_addr *hw_dst_addr);

extern err_t (* rm_lwip_w_etharp_raw)(struct netif *,
                        const struct eth_addr *, const struct eth_addr *,
                        const struct eth_addr *, const ip4_addr_t *,
                        const struct eth_addr *, const ip4_addr_t *,
                        const u16_t);

/* Golbals */
static u32_t etharping_time;
static u16_t etharp_send_cnt;
static u16_t etharp_recv_cnt;
static u32_t etharping_total_time;
static u32_t etharping_min_time;
static u32_t etharping_max_time;
struct cmd_arping_param * etharping_param;
static TaskHandle_t etharpingTaskHandle;
static EventGroupHandle_t etharping_recv_evt_handle = NULL;
static bool etharpingThreadRunning = false;

/*
  * DHCP_ARP Packet send
  * MSG Data :
  *   Sender IP address 0.0.0.0
  *   Target IP address nx_interface_ip_address
  *   Target MAC address 00:00:00:00:00:00
  */
err_t
dhcp_etharp_request(struct netif *netif, const ip4_addr_t *ipaddr)
{
  LWIP_DEBUGF(ETHARP_DEBUG | LWIP_DBG_TRACE, ("etharp_request: sending DHCP_ARP request.\n"));

  return dhcp_etharp_request_dst(netif, ipaddr, &ethbroadcast);
}

err_t
etharp_send(struct netif *netif, const struct eth_addr *ethsrc_addr,
            const struct eth_addr *ethdst_addr,
            const struct eth_addr *hwsrc_addr, const ip4_addr_t *ipsrc_addr,
            const struct eth_addr *hwdst_addr, const ip4_addr_t *ipdst_addr,
            const u16_t opcode)
{
    return rm_lwip_w_etharp_raw(netif, ethsrc_addr, ethdst_addr, hwsrc_addr,
                              ipsrc_addr, hwdst_addr, ipdst_addr, opcode);
}

err_t
dhcp_etharp_request_dst(struct netif *netif, const ip4_addr_t *ipaddr, const struct eth_addr *hw_dst_addr)
{
    struct ip4_addr sip_addr;
    sip_addr.addr = 0;

    return rm_lwip_w_etharp_raw(netif, (struct eth_addr *)netif->hwaddr, hw_dst_addr,
                              (struct eth_addr *)netif->hwaddr, &sip_addr, &ethzero,
                              ipaddr, ARP_REQUEST);
}

void etharp_print_arp_table(u32_t iface)
{
	struct netif *netif;
#if CFG_PMGR
	struct rm_etharp_entry *arp_table;
#endif /* CFG_PMGR */

	netif = netif_get_by_index(RM_WIFI_NETIF_INDEX(iface));

	if(netif == NULL)
	{
		LWIP_DEBUGF(ETHARP_DEBUG, ("\n[WLAN%d] Network Interface is NULL\n", (u16_t) iface));
		return;
	}
#if CFG_PMGR

	arp_table = (struct rm_etharp_entry *)get_arp_table();
	printf("WLAN[%d]\n", iface != 1 ? 0 : 1);
	printf("-------------------------------------------------------------------\n");
	printf("arp_status\tarp_time\tMAC Address\t\tIP Address\n");
	printf("-------------------------------------------------------------------\n");

	for (int i = 0; i < ARP_TABLE_SIZE; i++)
	{
#if (LWIP_IPV6) && (LWIP_IPV4)
		if (arp_table[i].netif->ip_addr.u_addr.ip4.addr == netif->ip_addr.u_addr.ip4.addr)
#elif LWIP_IPV4
		if (arp_table[i].netif->ip_addr.addr == netif->ip_addr.addr)
#endif
		{

			if(arp_table[i].state == ETHARP_STATE_EMPTY)
			{
				//printf("Empty");
				continue;
			}
			else if(arp_table[i].state == ETHARP_STATE_PENDING)
			{
				printf("Pending");
			}
			else if(arp_table[i].state == ETHARP_STATE_STABLE)
			{
				printf("Stable");
			}
			else if(arp_table[i].state == ETHARP_STATE_STABLE_REREQUESTING_1
					|| arp_table[i].state == ETHARP_STATE_STABLE_REREQUESTING_2)
			{
				printf("Requesting");
			}

			printf("\t\t%d", arp_table[i].ctime);
			printf("\t\t%02X:%02X:%02X:%02X:%02X:%02X",
					arp_table[i].ethaddr.addr[0],
					arp_table[i].ethaddr.addr[1],
					arp_table[i].ethaddr.addr[2],
					arp_table[i].ethaddr.addr[3],
					arp_table[i].ethaddr.addr[4],
					arp_table[i].ethaddr.addr[5]);
			printf("\t%s\n", ipaddr_ntoa((const ip_addr_t *)&arp_table[i].ipaddr));
		}
	}
#endif /* CFG_PMGR */
}

static void etharping_thread(void *arg)
{
	LWIP_UNUSED_ARG(arg);

	LWIP_DEBUGF(ETHARP_DEBUG, ("\nEtharping Thread Start\n"));

	struct netif *netif;
	UINT count = 1;

	if (etharping_param == NULL) {
		printf("ARPing Param is Null!!\n");

		if (etharping_recv_evt_handle != NULL) {
			vEventGroupDelete(etharping_recv_evt_handle);
			etharping_recv_evt_handle = NULL;
		}

		if (etharpingTaskHandle != NULL) {
			vTaskDelete(etharpingTaskHandle);
			etharpingTaskHandle = NULL;
		}

		etharpingThreadRunning = false;

		return;
	} else {
		LWIP_DEBUGF(ETHARP_DEBUG, ("ARPing Network Interface:WLAN[%d]\n", etharping_param->ping_interface));
	}

	etharpingThreadRunning = true;
	printf("ARPing %s\n", ipaddr_ntoa((const ip_addr_t *)&(etharping_param->ipaddr)));

	netif = netif_get_by_index(RM_WIFI_NETIF_INDEX(etharping_param->ping_interface));

	if (netif == NULL) {
		printf("Network Interface is NULL\n");

		if (etharping_param != NULL) {
            vPortFree(etharping_param);
            etharping_param = NULL;
		}

		if (etharping_recv_evt_handle != NULL) {
			vEventGroupDelete(etharping_recv_evt_handle);
			etharping_recv_evt_handle = NULL;
		}

		if (etharpingTaskHandle != NULL) {
			vTaskDelete(etharpingTaskHandle);
			etharpingTaskHandle = NULL;
		}

		etharpingThreadRunning = false;

		return;
	}

	while (1) {
		if (((etharping_param->count == 0 && count > 5)
		    || (etharping_param->count != 0 && count > etharping_param->count))
		    && etharpingTaskHandle != NULL) {
			printf("\n--- %s arping statistics ---\n", ipaddr_ntoa((const ip_addr_t *)&etharping_param->ipaddr));
			printf("%u packets transmitted, %u received,  %u%% unanswered (%u extra)\n",
														etharp_send_cnt,
														etharp_recv_cnt,
														((etharp_send_cnt-etharp_recv_cnt) * 100) / etharp_send_cnt,
														(etharp_recv_cnt - etharp_send_cnt));

			printf("rtt min/avg/max = %lu/%lu/%lu ms\n", etharping_min_time,
														etharping_total_time / etharp_recv_cnt,
														etharping_max_time);

			if (etharping_param != NULL) {
                vPortFree(etharping_param);
                etharping_param = NULL;
			}

			if (etharping_recv_evt_handle != NULL) {
				vEventGroupDelete(etharping_recv_evt_handle);
				etharping_recv_evt_handle = NULL;
			}

			if (etharpingTaskHandle != NULL) {
				etharpingThreadRunning = false;
				vTaskDelete(etharpingTaskHandle);
				etharpingTaskHandle = NULL;
			}
			break;
		}
		else
		{
			count++;
		}

		//send arp
		etharp_send_cnt++;

		if (etharping_recv_evt_handle != NULL) {
			xEventGroupClearBits(etharping_recv_evt_handle, ETHARPING_RECV_EVT_BITS);
		}

		int result = arp_request(etharping_param->ipaddr, etharping_param->ping_interface);

		if (result == pdFAIL) {
			printf("ARP request Fail!!");
			continue;
		}

		etharping_time = sys_now();

		//wait receive packet or timeout
		if (etharping_recv_evt_handle != NULL) {
			EventBits_t ret;

			if (etharping_param->wait != 0) {
				ret = xEventGroupWaitBits(etharping_recv_evt_handle, ETHARPING_RECV_EVT_BITS, pdTRUE, pdFALSE, portCONVERT_MS_2_TICKS(etharping_param->wait));
			} else {
				ret = xEventGroupWaitBits(etharping_recv_evt_handle, ETHARPING_RECV_EVT_BITS, pdTRUE, pdFALSE, portCONVERT_MS_2_TICKS(ETHARPING_RCV_TIMEO));
			}

			if (ret & ETHARPING_RECV_EVT_BITS) {
				u32_t response_time = sys_now() - etharping_time;

				etharping_total_time += response_time;

				if (etharping_min_time == 0) {
					etharping_min_time = response_time;
					etharping_max_time = response_time;
				} else if(etharping_min_time > response_time) {
					etharping_min_time = response_time;
				} else if(etharping_max_time < response_time) {
					etharping_max_time = response_time;
				}

				etharp_recv_cnt++;
				printf("60 bytes from %s index=%u time%c%lums\n", ipaddr_ntoa((const ip_addr_t *)&etharping_param->ipaddr), etharp_send_cnt,
                        response_time < portTICK_PERIOD_MS ? '<':'=',
                        response_time < portTICK_PERIOD_MS ? portTICK_PERIOD_MS : response_time);
			} else {
				printf("From %s Timeout\n", ipaddr_ntoa((const ip_addr_t *)&etharping_param->ipaddr));
			}
		}

		//set interval
		if (etharping_param->interval != 0) {
			vTaskDelay(portCONVERT_MS_2_TICKS(etharping_param->interval));
		} else {
			vTaskDelay(portCONVERT_MS_2_TICKS(ETHARPING_DELAY));
		}
	}
}

void etharping_init(void * param){

	etharp_send_cnt = 0;
	etharp_recv_cnt = 0;
	etharping_total_time = 0;
	etharping_min_time = 0;
	etharping_max_time = 0;

	if (!etharpingThreadRunning) {
		etharping_param = (struct cmd_arping_param *)param;
		etharping_recv_evt_handle = xEventGroupCreate();
		etharpingTaskHandle = (TaskHandle_t)sys_thread_new("arping_thread",
									etharping_thread,
									param,
									DEFAULT_THREAD_STACKSIZE,
									DEFAULT_THREAD_PRIO+1);
	} else {
		printf("%s %d: arping_thread already running\n", __func__, __LINE__);
	}

}

#endif /* LWIP_IPV4 && LWIP_ARP */
