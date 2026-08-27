/**
 *  Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Copyright (C) 2023-2024 Modified by Renesas Electronics
 *
**/

#include "lwipopts.h"

#if defined ( __SUPPORT_DHCP_SVR__ )

/* These values were supplied by the vendor lwIP configuration.  Keep the
 * DHCP server self-contained while the protocol stack is RT-Thread lwIP. */
#ifndef CONFIG_LWIP_DHCPS_MAX_STATION_NUM
#define CONFIG_LWIP_DHCPS_MAX_STATION_NUM 10
#endif
#ifndef CONFIG_LWIP_DHCPS_LEASE_UNIT
#define CONFIG_LWIP_DHCPS_LEASE_UNIT 1
#endif

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include "lwip/inet.h"
#include "lwip/err.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include "lwip/mem.h"
#include "arch/sys_arch.h"

#include "dhcpserver.h"
#include "dhcpserver_options.h"

#include "net_dhcp_server.h"
#ifdef RM_MAP_PERSISTANT_W
#include "rm_map_persistant_w.h"
#endif

#pragma GCC diagnostic ignored "-Waggregate-return"

#if LWIP_IPV4
#define BOOTP_BROADCAST				0x8000

#define DHCP_REQUEST				1
#define DHCP_REPLY					2
#define DHCP_HTYPE_ETHERNET			1
#define DHCP_HLEN_ETHERNET			6
#define DHCP_MSG_LEN				236

#define DHCPS_SERVER_PORT			67
#define DHCPS_CLIENT_PORT			68

#define DHCPDISCOVER				1
#define DHCPOFFER					2
#define DHCPREQUEST					3
#define DHCPDECLINE					4
#define DHCPACK						5
#define DHCPNAK						6
#define DHCPRELEASE					7

#define DHCP_OPTION_SUBNET_MASK						1
#define DHCP_OPTION_ROUTER							3
#define DHCP_OPTION_DNS_SERVER						6
#define DHCP_OPTION_CLIENT_HOST_NAME				12
#define DHCP_OPTION_REQ_IPADDR						50
#define DHCP_OPTION_LEASE_TIME						51
#define DHCP_OPTION_MSG_TYPE						53
#define DHCP_OPTION_SERVER_ID						54
#define DHCP_OPTION_INTERFACE_MTU					26
#define DHCP_OPTION_PERFORM_ROUTER_DISCOVERY		31
#define DHCP_OPTION_BROADCAST_ADDRESS				28
#define DHCP_OPTION_REQ_LIST						55
#define DHCP_OPTION_END								255

#if DHCPS_DEBUG
#define DHCPS_LOG	printf
#else
#define DHCPS_LOG(...)
#endif

#undef DHCPS_DEBUG_DUMP

#define MAX_STATION_NUM CONFIG_LWIP_DHCPS_MAX_STATION_NUM

#define DHCPS_STATE_OFFER 1
#define DHCPS_STATE_DECLINE 2
#define DHCPS_STATE_ACK 3
#define DHCPS_STATE_NAK 4
#define DHCPS_STATE_IDLE 5
#define DHCPS_STATE_RELEASE 6

#define	DHCPS_THREAD_STACKSIZE	1024//(1024 * 2) // 2048 <- DEFAULT_THREAD_STACKSIZE 

typedef struct _list_node {
	void *pnode;
	struct _list_node *pnext;
} list_node;

////////////////////////////////////////////////////////////////////////////////////

static const u32_t magic_cookie  = 0x63538263;

struct netif   *dhcps_netif = NULL;
static struct udp_pcb *dhcps_pcb = NULL;
static ip4_addr_t broadcast_dhcps;
static ip4_addr_t server_address;
static ip4_addr_t dns_server = {0};
static ip4_addr_t client_address;        //added
static ip4_addr_t client_address_plus;
static ip4_addr_t s_dhcps_mask = {
#ifdef USE_CLASS_B_NET
        .addr = PP_HTONL(LWIP_MAKEU32(255, 240, 0, 0))
#else
        .addr = PP_HTONL(LWIP_MAKEU32(255, 255, 255, 0))
#endif
    };

/* For System configuration ... */

static list_node *plist = NULL;
static bool renew = false;

static dhcps_lease_t dhcps_poll;
static dhcps_time_t dhcps_lease_time = DHCPS_LEASE_TIME_DEF;  //minute
static dhcps_offer_t dhcps_offer = 0xFF;
static dhcps_offer_t dhcps_dns = 0x00;
static dhcps_cb_t dhcps_cb;

static TaskHandle_t dhcpsTaskHandle = NULL;

static int ip_lease_tbl_entry_cnt = 0;

/**
 * ex)
 * print_separate_bar("=", 10, 2);
 *
 * "==========\n\n"
 *
 */
static void print_separate_bar(unsigned char text, unsigned char loop_count, unsigned char CR_loop_count)
{
    unsigned char prt_str[260];

    memset(prt_str, 0, 256);

    if ((loop_count + CR_loop_count) + 1 > 260) {
        loop_count = (unsigned char)(260 - (CR_loop_count - 1));
    }

    memset(prt_str, text, loop_count);

    if (CR_loop_count > 0) {
        memset(prt_str + loop_count, '\n', CR_loop_count);
    }
    printf("%s", prt_str);
}

/******************************************************************************
 * FunctionName : dhcps_option_info
 * Description  : get the DHCP message option info
 * Parameters   : op_id -- DHCP message option id
 *                opt_len -- DHCP message option length
 * Returns      : DHCP message option addr
*******************************************************************************/
void *dhcps_option_info(u8_t op_id, u32_t opt_len)
{
    void *option_arg = NULL;

    switch (op_id) {
        case IP_ADDRESS_LEASE_TIME:
            if (opt_len == sizeof(dhcps_time_t)) {
                option_arg = &dhcps_lease_time;
            }

            break;

        case REQUESTED_IP_ADDRESS:
            if (opt_len == sizeof(dhcps_lease_t)) {
                option_arg = &dhcps_poll;
            }

            break;

        case ROUTER_SOLICITATION_ADDRESS:
            if (opt_len == sizeof(dhcps_offer_t)) {
                option_arg = &dhcps_offer;
            }

            break;

        case DOMAIN_NAME_SERVER:
            if (opt_len == sizeof(dhcps_offer_t)) {
                option_arg = &dhcps_dns;
            }

            break;
        case SUBNET_MASK:
            if (opt_len == sizeof(s_dhcps_mask)) {
                option_arg = &s_dhcps_mask;
            }

            break;

		case CLIENT_POOL:
			option_arg = plist;
		
			break;
			
#if 0
		case HOST_NAME :
			option_arg = 
			break;
#endif	// 0

        default:
            break;
    }

    return option_arg;
}

/******************************************************************************
 * FunctionName : dhcps_set_option_info
 * Description  : set the DHCP message option info
 * Parameters   : op_id -- DHCP message option id
 *                opt_info -- DHCP message option info
 *                opt_len -- DHCP message option length
 * Returns      : none
*******************************************************************************/
void dhcps_set_option_info(u8_t op_id, void *opt_info, u32_t opt_len)
{
    if (opt_info == NULL) {
        return;
    }

    switch (op_id) {
        case IP_ADDRESS_LEASE_TIME:
            if (opt_len == sizeof(dhcps_time_t)) {
				dhcps_lease_time = *(dhcps_time_t *)opt_info;
            }

            break;

        case REQUESTED_IP_ADDRESS:
            if (opt_len == sizeof(dhcps_lease_t)) {
                dhcps_poll = *(dhcps_lease_t *)opt_info;
                vTaskDelay (500);
            }

            break;

        case ROUTER_SOLICITATION_ADDRESS:
            if (opt_len == sizeof(dhcps_offer_t)) {
                dhcps_offer = *(dhcps_offer_t *)opt_info;
            }

            break;

        case DOMAIN_NAME_SERVER:
            if (opt_len == sizeof(dhcps_offer_t)) {
                dhcps_dns = *(dhcps_offer_t *)opt_info;
            }
            break;

        case SUBNET_MASK:
            if (opt_len == sizeof(s_dhcps_mask)) {
                s_dhcps_mask = *(ip4_addr_t *)opt_info;
            }
            break;

        default:
            break;
    }

    return;
}

/******************************************************************************
 * FunctionName : node_insert_to_list
 * Description  : insert the node to the list
 * Parameters   : phead -- the head node of the list
 *                pinsert -- the insert node of the list
 * Returns      : none
*******************************************************************************/
static void node_insert_to_list(list_node **phead, list_node *pinsert)
{
    list_node *p = NULL;
    struct dhcps_pool *pdhcps_pool = NULL;
    struct dhcps_pool *pdhcps_node = NULL;

    if (*phead == NULL) {
        *phead = pinsert;
    } else {
        p = *phead;
        pdhcps_node = pinsert->pnode;
        pdhcps_pool = p->pnode;

        if (pdhcps_node->ip.addr < pdhcps_pool->ip.addr) {
            pinsert->pnext = p;
            *phead = pinsert;
        } else {
            while (p->pnext != NULL) {
                pdhcps_pool = p->pnext->pnode;

                if (pdhcps_node->ip.addr < pdhcps_pool->ip.addr) {
                    pinsert->pnext = p->pnext;
                    p->pnext = pinsert;
                    break;
                }

                p = p->pnext;
            }

            if (p->pnext == NULL) {
                p->pnext = pinsert;
            }
        }
    }
	ip_lease_tbl_entry_cnt++;
	DHCPS_LOG("dhcps: an entry added (total_cnt=%d) \n", ip_lease_tbl_entry_cnt);
}

/******************************************************************************
 * FunctionName : node_delete_from_list
 * Description  : remove the node from list
 * Parameters   : phead -- the head node of the list
 *                pdelete -- the remove node of the list
 * Returns      : none
*******************************************************************************/
void node_remove_from_list(list_node **phead, list_node *pdelete);

void node_remove_from_list(list_node **phead, list_node *pdelete)
{
    list_node *p = NULL;

    p = *phead;

    if (p == NULL) {
        *phead = NULL;
    } else {
        if (p == pdelete) {
            // Note: Ignoring the "use after free" warnings, as it could only happen
            // if the linked list contains loops
            *phead = p->pnext; // NOLINT(clang-analyzer-unix.Malloc)
            pdelete->pnext = NULL;
        } else {
            while (p!= NULL) {
                if (p->pnext == pdelete) { // NOLINT(clang-analyzer-unix.Malloc)
                    p->pnext = pdelete->pnext;
                    pdelete->pnext = NULL;
                }

                p = p->pnext;
            }
        }
    }
	ip_lease_tbl_entry_cnt--;
	DHCPS_LOG("dhcps: an entry removed (total_cnt=%d) \n", ip_lease_tbl_entry_cnt);
}

/******************************************************************************
 * FunctionName : add_msg_type
 * Description  : add TYPE option of DHCP message
 * Parameters   : optptr -- the addr of DHCP message option
 * Returns      : the addr of DHCP message option
*******************************************************************************/
static u8_t *add_msg_type(u8_t *optptr, u8_t type)
{
    *optptr++ = DHCP_OPTION_MSG_TYPE;
    *optptr++ = 1;
    *optptr++ = type;
    return optptr;
}

/******************************************************************************
 * FunctionName : add_offer_options
 * Description  : add OFFER option of DHCP message
 * Parameters   : optptr -- the addr of DHCP message option
 * Returns      : the addr of DHCP message option
*******************************************************************************/
static u8_t *add_offer_options(u8_t *optptr)
{
    ip4_addr_t ipadd;

    *optptr++ = DHCP_OPTION_SUBNET_MASK;
    *optptr++ = 4;
    *optptr++ = ip4_addr1(&s_dhcps_mask);
    *optptr++ = ip4_addr2(&s_dhcps_mask);
    *optptr++ = ip4_addr3(&s_dhcps_mask);
    *optptr++ = ip4_addr4(&s_dhcps_mask);

    *optptr++ = DHCP_OPTION_LEASE_TIME;
    *optptr++ = 4;
    *optptr++ = (u8_t)(((dhcps_lease_time * DHCPS_LEASE_UNIT) >> 24) & 0xFF);
    *optptr++ = ((dhcps_lease_time * DHCPS_LEASE_UNIT) >> 16) & 0xFF;
    *optptr++ = ((dhcps_lease_time * DHCPS_LEASE_UNIT) >> 8) & 0xFF;
    *optptr++ = ((dhcps_lease_time * DHCPS_LEASE_UNIT) >> 0) & 0xFF;

    *optptr++ = DHCP_OPTION_SERVER_ID;
    *optptr++ = 4;
    ipadd.addr = *((u32_t *) &server_address);
    *optptr++ = ip4_addr1(&ipadd);
    *optptr++ = ip4_addr2(&ipadd);
    *optptr++ = ip4_addr3(&ipadd);
    *optptr++ = ip4_addr4(&ipadd);

    if (dhcps_router_enabled(dhcps_offer)) {
#if LWIP_IPV4 && LWIP_IPV6
    	ipadd.addr = *((u32_t *) &(dhcps_netif->gw.u_addr.ip4.addr));
#elif LWIP_IPV4
    	ipadd.addr = *((u32_t *) &(ip_2_ip4(dhcps_netif->gw).addr));
#endif
        if (!ip4_addr_isany_val(ipadd)) {
            *optptr++ = DHCP_OPTION_ROUTER;
            *optptr++ = 4;
            *optptr++ = ip4_addr1(&ipadd);
            *optptr++ = ip4_addr2(&ipadd);
            *optptr++ = ip4_addr3(&ipadd);
            *optptr++ = ip4_addr4(&ipadd);
        }
    }

#ifdef __SUPPORT_MESH__
	if (get_run_mode() == WIFI_DEVICE_MODE_EXT_MESH_POINT
		|| get_run_mode() == WIFI_DEVICE_MODE_EXT_MESH_PORTAL)
	{
	    *optptr++ = DHCP_OPTION_DNS_SERVER;
	    *optptr++ = 4;
	    if (dhcps_dns_enabled(dhcps_dns)) {
	        *optptr++ = ip4_addr1(&dns_server);
	        *optptr++ = ip4_addr2(&dns_server);
	        *optptr++ = ip4_addr3(&dns_server);
	        *optptr++ = ip4_addr4(&dns_server);
	    } else {
	        *optptr++ = ip4_addr1(&ipadd);
	        *optptr++ = ip4_addr2(&ipadd);
	        *optptr++ = ip4_addr3(&ipadd);
	        *optptr++ = ip4_addr4(&ipadd);
	    }
    }
#endif // __SUPPORT_MESH__

    *optptr++ = DHCP_OPTION_BROADCAST_ADDRESS;
    *optptr++ = 4;
    ip4_addr_t broadcast_addr = { .addr = (ipadd.addr & s_dhcps_mask.addr) | ~s_dhcps_mask.addr };
    *optptr++ = ip4_addr1(&broadcast_addr);
    *optptr++ = ip4_addr2(&broadcast_addr);
    *optptr++ = ip4_addr3(&broadcast_addr);
    *optptr++ = ip4_addr4(&broadcast_addr);

    *optptr++ = DHCP_OPTION_INTERFACE_MTU;
    *optptr++ = 2;
    *optptr++ = 0x05;
    *optptr++ = 0xdc;

    *optptr++ = DHCP_OPTION_PERFORM_ROUTER_DISCOVERY;
    *optptr++ = 1;
    *optptr++ = 0x00;

    *optptr++ = 43;
    *optptr++ = 6;

    *optptr++ = 0x01;
    *optptr++ = 4;
    *optptr++ = 0x00;
    *optptr++ = 0x00;
    *optptr++ = 0x00;
    *optptr++ = 0x02;

    return optptr;
}

/******************************************************************************
 * FunctionName : add_end
 * Description  : add end option of DHCP message
 * Parameters   : optptr -- the addr of DHCP message option
 * Returns      : the addr of DHCP message option
*******************************************************************************/
static u8_t *add_end(u8_t *optptr)
{
    *optptr++ = DHCP_OPTION_END;
    return optptr;
}

/******************************************************************************
 * FunctionName : create_msg
 * Description  : create response message
 * Parameters   : m -- DHCP message info
 * Returns      : none
*******************************************************************************/
static void create_msg(struct dhcps_msg *m)
{
    ip4_addr_t client;


    client.addr = *((uint32_t *) &client_address);

    m->op = DHCP_REPLY;
    m->htype = DHCP_HTYPE_ETHERNET;
    m->hlen = 6;
    m->hops = 0;
    m->secs = 0;
    m->flags = htons(BOOTP_BROADCAST);

    memcpy((char *) m->yiaddr, (char *) &client.addr, sizeof(m->yiaddr));

    memset((char *) m->ciaddr, 0, sizeof(m->ciaddr));

    memset((char *) m->siaddr, 0, sizeof(m->siaddr));

    memset((char *) m->giaddr, 0, sizeof(m->giaddr));

    memset((char *) m->sname, 0, sizeof(m->sname));

    memset((char *) m->file, 0, sizeof(m->file));

    memset((char *) m->options, 0, sizeof(m->options));

    u32_t magic_cookie_temp = magic_cookie;

    memcpy((char *) m->options, &magic_cookie_temp, sizeof(magic_cookie_temp));
}

struct pbuf * dhcps_pbuf_alloc(u16_t len);

struct pbuf * dhcps_pbuf_alloc(u16_t len)
{
    u16_t mlen = sizeof(struct dhcps_msg);
    struct pbuf *p;

    if (len > mlen) {
        DHCPS_LOG("dhcps: len=%d mlen=%d", len, mlen);
        mlen = len;
    }
    p = pbuf_alloc(PBUF_TRANSPORT, mlen, PBUF_RAM);
    if (p)
        p->if_idx = RM_WIFI_NETIF_INDEX(WLAN1_IFACE);
    return p;
}

/******************************************************************************
 * FunctionName : send_offer
 * Description  : DHCP message OFFER Response
 * Parameters   : m -- DHCP message info
 * Returns      : none
*******************************************************************************/
static void send_offer(struct dhcps_msg *m, u16_t len)
{
    u8_t *end;
    struct pbuf *p, *q;
    u8_t *data;
    u16_t cnt = 0;
    u16_t i;
#if DHCPS_DEBUG
    err_t SendOffer_err_t;
#endif
    create_msg(m);

    end = add_msg_type(&m->options[4], DHCPOFFER);
    end = add_offer_options(end);
    end = add_end(end);

    p = dhcps_pbuf_alloc(len);

    DHCPS_LOG("udhcp: send_offer>>p->ref = %d\n", p->ref);

    if (p != NULL) {
        DHCPS_LOG("dhcps: send_offer>>pbuf_alloc succeed\n");
        DHCPS_LOG("dhcps: send_offer>>p->tot_len = %d\n", p->tot_len);
        DHCPS_LOG("dhcps: send_offer>>p->len = %d\n", p->len);
        q = p;

        while (q != NULL) {
            data = (u8_t *)q->payload;

            for (i = 0; i < q->len; i++) {
                data[i] = ((u8_t *) m)[cnt++];
            }

            q = q->next;
        }
    } else {

        DHCPS_LOG("dhcps: send_offer>>pbuf_alloc failed\n");
        return;
    }

    ip_addr_t ip_temp = IPADDR4_INIT(0x0);
    ip4_addr_set(ip_2_ip4(&ip_temp), &broadcast_dhcps);
    struct udp_pcb *pcb_dhcps = dhcps_pcb;

#if DHCPS_DEBUG
    SendOffer_err_t = udp_sendto(pcb_dhcps, p, &ip_temp, DHCPS_CLIENT_PORT);
    DHCPS_LOG("\ndhcps: send_offer>>udp_sendto result %x\n", SendOffer_err_t);
#else
    udp_sendto(pcb_dhcps, p, &ip_temp, DHCPS_CLIENT_PORT);
#endif

    if (p->ref != 0) {
        DHCPS_LOG("udhcp: send_offer>>free pbuf\n");
        pbuf_free(p);
    }
}

/******************************************************************************
 * FunctionName : send_nak
 * Description  : DHCP message NACK Response
 * Parameters   : m -- DHCP message info
 * Returns      : none
*******************************************************************************/
static void send_nak(struct dhcps_msg *m, u16_t len)
{
    u8_t *end;
    struct pbuf *p, *q;
    u8_t *data;
    u16_t cnt = 0;
    u16_t i;
#if DHCPS_DEBUG
    err_t SendNak_err_t;
#endif
    create_msg(m);

    end = add_msg_type(&m->options[4], DHCPNAK);
    end = add_end(end);

    p = dhcps_pbuf_alloc(len);

    DHCPS_LOG("udhcp: send_nak>>p->ref = %d\n", p->ref);

    if (p != NULL) {
        DHCPS_LOG("dhcps: send_nak>>pbuf_alloc succeed\n");
        DHCPS_LOG("dhcps: send_nak>>p->tot_len = %d\n", p->tot_len);
        DHCPS_LOG("dhcps: send_nak>>p->len = %d\n", p->len);
        q = p;

        while (q != NULL) {
            data = (u8_t *)q->payload;

            for (i = 0; i < q->len; i++) {
                data[i] = ((u8_t *) m)[cnt++];
            }

            q = q->next;
        }
    } else {
        DHCPS_LOG("dhcps: send_nak>>pbuf_alloc failed\n");
        return;
    }

    ip_addr_t ip_temp = IPADDR4_INIT(0x0);
    ip4_addr_set(ip_2_ip4(&ip_temp), &broadcast_dhcps);
    struct udp_pcb *pcb_dhcps = dhcps_pcb;
#if DHCPS_DEBUG
    SendNak_err_t = udp_sendto(pcb_dhcps, p, &ip_temp, DHCPS_CLIENT_PORT);
    DHCPS_LOG("dhcps: send_nak>>udp_sendto result %x\n", SendNak_err_t);
#else
    udp_sendto(pcb_dhcps, p, &ip_temp, DHCPS_CLIENT_PORT);
#endif

    if (p->ref != 0) {
        DHCPS_LOG("udhcp: send_nak>>free pbuf\n");
        pbuf_free(p);
    }
}

/******************************************************************************
 * FunctionName : send_ack
 * Description  : DHCP message ACK Response
 * Parameters   : m -- DHCP message info
 * Returns      : none
*******************************************************************************/
static void send_ack(struct dhcps_msg *m, u16_t len)
{
    u8_t *end;
    struct pbuf *p, *q;
    u8_t *data;
    u16_t cnt = 0;
    u16_t i;
    err_t SendAck_err_t;
    create_msg(m);

    end = add_msg_type(&m->options[4], DHCPACK);
    end = add_offer_options(end);
    end = add_end(end);

    p = dhcps_pbuf_alloc(len);

    DHCPS_LOG("udhcp: send_ack>>p->ref = %d\n", p->ref);

    if (p != NULL) {
        DHCPS_LOG("dhcps: send_ack>>pbuf_alloc succeed\n");
        DHCPS_LOG("dhcps: send_ack>>p->tot_len = %d\n", p->tot_len);
        DHCPS_LOG("dhcps: send_ack>>p->len = %d\n", p->len);
        q = p;

        while (q != NULL) {
            data = (u8_t *)q->payload;

            for (i = 0; i < q->len; i++) {
                data[i] = ((u8_t *) m)[cnt++];
            }
            q = q->next;
        }
    } else {

        DHCPS_LOG("dhcps: send_ack>>pbuf_alloc failed\n");
        return;
    }

    ip_addr_t ip_temp = IPADDR4_INIT(0x0);
    ip4_addr_set(ip_2_ip4(&ip_temp), &broadcast_dhcps);
    struct udp_pcb *pcb_dhcps = dhcps_pcb;

    SendAck_err_t = udp_sendto(pcb_dhcps, p, &ip_temp, DHCPS_CLIENT_PORT);
#if DHCPS_DEBUG
    DHCPS_LOG("dhcps: send_ack>>udp_sendto result %x\n", SendAck_err_t);
    vTaskDelay(portCONVERT_MS_2_TICKS(100));
#endif

    if (SendAck_err_t == ERR_OK) {
    	DHCPS_LOG("dhcps: send_ack>> ERR_OK\n");
        dhcps_cb(m->yiaddr);
    }

    if (p->ref != 0) {
        DHCPS_LOG("udhcp: send_ack>>free pbuf\n");
        pbuf_free(p);
    }
}

/******************************************************************************
 * FunctionName : dhcps_check_ip_pool
 * Description  : check ip pool is available
 * Parameters   : req_ip - requested ip
 * Returns      : pdTRUE or pdFALSE
*******************************************************************************/
static u8_t check_ip_pool(ip4_addr_t req_ip)
{
	req_ip.addr = lwip_htonl(req_ip.addr);

	if ( (req_ip.addr < lwip_htonl(dhcps_poll.start_ip.addr)) || 
		 (req_ip.addr > lwip_htonl(dhcps_poll.end_ip.addr))   || 
		 (ip4_addr_isany(&client_address))) {
		return pdFALSE;
	} else {
		struct dhcps_pool *pdhcps_pool = NULL;
		req_ip.addr = lwip_htonl(req_ip.addr);
		pdhcps_pool = dhcps_search_ip_pool(req_ip);

		if(pdhcps_pool != NULL) {
			return pdFALSE;
		} else {
			return pdTRUE;
		}
	}
}

#if DHCPS_DEBUG
static char* dhcps_msg_type_2_str(u16_t type)
{
	switch (type) {
		case DHCPDISCOVER:
			return "DISCOVER";
		case DHCPREQUEST:
			return "REQUEST";
		case DHCPDECLINE:
			return "DECLIENT";
		case DHCPRELEASE:
			return "RELEASE";
		default:
			return "CHK_TYPE";
	}
}
#endif // DHCPS_DEBUG

/******************************************************************************
 * FunctionName : parse_options
 * Description  : parse DHCP message options
 * Parameters   : optptr -- DHCP message option info
 *                len -- DHCP message option length
 * Returns      : none
*******************************************************************************/
static u8_t parse_options(u8_t *optptr, s16_t len, struct dhcps_pool *pdhcps_pool)
{
    ip4_addr_t client;
    bool is_dhcp_parse_end = false;
    struct dhcps_state s;

    client.addr = *((uint32_t *) &client_address);

    u8_t *end = optptr + len;
    u16_t type = 0;

    s.state = DHCPS_STATE_IDLE;

    while (optptr < end) {
        DHCPS_LOG("dhcps: (s16_t)*optptr = %d\n", (s16_t)*optptr);

        switch ((s16_t) *optptr) {

            case DHCP_OPTION_MSG_TYPE:	//53
                type = *(optptr + 2);
#if DHCPS_DEBUG
				DHCPS_LOG("dhcps: msg rx type = %s \n", dhcps_msg_type_2_str(type));
#endif // DHCPS_DEBUG
                break;

            case DHCP_OPTION_REQ_IPADDR://50
                DHCPS_LOG("dhcps: offer addr %s\n", ipaddr_ntoa((const ip_addr_t *) &client));
                DHCPS_LOG("dhcps: request addr %s\n", ipaddr_ntoa((const ip_addr_t *) (optptr+2)));

                if (memcmp((char *) &client.addr, (char *) optptr + 2, 4) == 0) {
                    DHCPS_LOG("dhcps: DHCP_OPTION_REQ_IPADDR = 0 ok\n");
                    s.state = DHCPS_STATE_ACK;
                } else {
                    DHCPS_LOG("dhcps: DHCP_OPTION_REQ_IPADDR != 0 err\n");
                    //Check IP pool - Requested IP is available or not.
                    //If requested IP is available, assign the IP. But it's not, send NAK.
                    ip4_addr_t req_ip;
                    char * opt_ip = ipaddr_ntoa((const ip_addr_t *)(optptr+2));
                    ipaddr_aton(opt_ip, (ip_addr_t *)&req_ip);

                    if(check_ip_pool(req_ip) == pdTRUE) {
                    	DHCPS_LOG("dhcps: Request IP is available\n");
						DHCPS_LOG("dhcps: record_cnt=%d, msg_type=%d, pool_addr_alloc=%s \n", 
										ip_lease_tbl_entry_cnt, 
										type,
										ipaddr_ntoa((ip_addr_t *) &pdhcps_pool->ip));

						if (ip_lease_tbl_entry_cnt == 1 && 
							type == DHCPREQUEST 		&&
							pdhcps_pool->ip.addr == dhcps_poll.start_ip.addr) {
							DHCPS_LOG("dhcps: IP lease table empty - 1st time for this STA or dhcps is rebooted ! \n");
							DHCPS_LOG("dhcps: Let STA to restart DHCP process to accept the 1st address (by giving NAK) \n");
							s.state = DHCPS_STATE_NAK;
						} else {
							ip4_addr_copy(pdhcps_pool->ip, req_ip);	//assign requested IP
							s.state = DHCPS_STATE_ACK;	//Requested IP is available.
						}
                    } else {
                    	DHCPS_LOG("dhcps: Request IP is not available\n");
                    	s.state = DHCPS_STATE_NAK;	//Requested IP is already assigned.
                    }
                }

                break;

			case DHCP_OPTION_CLIENT_HOST_NAME : //12
				DHCPS_LOG("dhcps: DHCP_OPTION_CLIENT_HOST_NAME \n");

        		if (pdhcps_pool != NULL) {
					int cp_size;
					memset(pdhcps_pool->client_name, 0, DHCP_CLIENT_HOSTNAME_MAX);

					cp_size = *(optptr + 1);
					for (int i = 0; i < cp_size; i++) {
						pdhcps_pool->client_name[i] = *(optptr + 2 + i);
					}
				}

				break;

            case DHCP_OPTION_END: {
                is_dhcp_parse_end = true;
            }
            break;
        }

        if (is_dhcp_parse_end) {
            break;
        }

        optptr += optptr[1] + 2;
    }

    switch (type) {

        case DHCPDISCOVER://1
            s.state = DHCPS_STATE_OFFER;
            DHCPS_LOG("dhcps: DHCPD_STATE_OFFER\n");
            break;

        case DHCPREQUEST://3
            if (!(s.state == DHCPS_STATE_ACK || s.state == DHCPS_STATE_NAK)) {
                if (renew == true) {
                    s.state = DHCPS_STATE_ACK;
                    DHCPS_LOG("dhcps: DHCPD_STATE_ACK\n");
                } else {
                    s.state = DHCPS_STATE_NAK;
                    DHCPS_LOG("dhcps: DHCPD_STATE_NAK\n");
                }
            }

            break;

        case DHCPDECLINE://4
            s.state = DHCPS_STATE_IDLE;
            DHCPS_LOG("dhcps: DHCPD_STATE_IDLE\n");
            break;

        case DHCPRELEASE://7
            s.state = DHCPS_STATE_RELEASE;
            DHCPS_LOG("dhcps: DHCPD_STATE_IDLE\n");
            break;
    }

    DHCPS_LOG("dhcps: return s.state = %d\n", s.state);
    return (u8_t)s.state;
}

/******************************************************************************
 * FunctionName : parse_msg
 * Description  : parse DHCP message from netif
 * Parameters   : m -- DHCP message info
 *                len -- DHCP message length
 * Returns      : DHCP message type
*******************************************************************************/
static s16_t parse_msg(struct dhcps_msg *m, u16_t len)
{
    u32_t lease_timer = (dhcps_lease_time * DHCPS_LEASE_UNIT)/DHCPS_COARSE_TIMER_SECS;

    if (memcmp((char *)m->options, &magic_cookie, sizeof(magic_cookie)) == 0) {
        DHCPS_LOG("dhcps: len = %d\n", len);
        ip4_addr_t addr_tmp;

        struct dhcps_pool *pdhcps_pool = NULL;
        list_node *pnode = NULL;
        list_node *pback_node = NULL;
        ip4_addr_t first_address;
        bool flag = false;

        first_address.addr = dhcps_poll.start_ip.addr;
        client_address.addr = client_address_plus.addr;
        renew = false;

        if (plist != NULL) {
            for (pback_node = plist; pback_node != NULL; pback_node = pback_node->pnext) {
                pdhcps_pool = pback_node->pnode;

                if (memcmp(pdhcps_pool->mac, m->chaddr, sizeof(pdhcps_pool->mac)) == 0) {
					DHCPS_LOG("dhcps:reconnect from legacy device\n");
					DHCPS_LOG("dhcps:same mac addr found, to reuse existing record ...\n");
                    if (memcmp(&pdhcps_pool->ip.addr, m->ciaddr, sizeof(pdhcps_pool->ip.addr)) == 0) {
                    	DHCPS_LOG("dhcps:reconnect from legacy device: renew\n");
                        renew = true;
                    }

                    client_address.addr = pdhcps_pool->ip.addr;
                    pdhcps_pool->lease_timer = lease_timer;
                    pnode = pback_node;
                    goto POOL_CHECK;
                } else if (pdhcps_pool->ip.addr == client_address_plus.addr) {
                    addr_tmp.addr = htonl(client_address_plus.addr);
                    addr_tmp.addr++;
                    client_address_plus.addr = htonl(addr_tmp.addr);
                    client_address.addr = client_address_plus.addr;
                }

                if (flag == false) { // search the fisrt unused ip
                    if (first_address.addr < pdhcps_pool->ip.addr) {
                        flag = true;
                    } else {
                        addr_tmp.addr = htonl(first_address.addr);
                        addr_tmp.addr++;
                        first_address.addr = htonl(addr_tmp.addr);
                    }
                }
            }
        } else {
			DHCPS_LOG("dhcps: IP lease table empty ... \n");
			ip_lease_tbl_entry_cnt = 0;
            client_address.addr = dhcps_poll.start_ip.addr;
			DHCPS_LOG("dhcps: total_cnt = 0 \n");
        }

        if (client_address_plus.addr > dhcps_poll.end_ip.addr) {
            client_address.addr = first_address.addr;
        }

        if (client_address.addr > dhcps_poll.end_ip.addr) {
            client_address_plus.addr = dhcps_poll.start_ip.addr;
            pdhcps_pool = NULL;
            pnode = NULL;
        } else {
			DHCPS_LOG("dhcps: alloc an IP lease table entry...\n");
            pdhcps_pool = (struct dhcps_pool *)mem_malloc(sizeof(struct dhcps_pool));
            memset(pdhcps_pool , 0x00 , sizeof(struct dhcps_pool));

            pdhcps_pool->ip.addr = client_address.addr;
            memcpy(pdhcps_pool->mac, m->chaddr, sizeof(pdhcps_pool->mac));
            pdhcps_pool->lease_timer = lease_timer;
            pnode = (list_node *)mem_malloc(sizeof(list_node));
            memset(pnode , 0x00 , sizeof(list_node));

            pnode->pnode = pdhcps_pool;
            pnode->pnext = NULL;
            node_insert_to_list(&plist, pnode);

            if (client_address.addr == dhcps_poll.end_ip.addr) {
                client_address_plus.addr = dhcps_poll.start_ip.addr;
            } else {
                addr_tmp.addr = htonl(client_address.addr);
                addr_tmp.addr++;
                client_address_plus.addr = htonl(addr_tmp.addr);
            }
        }

POOL_CHECK:

        if ((client_address.addr > dhcps_poll.end_ip.addr) || (ip4_addr_isany(&client_address))) {
            if (pnode != NULL) {
                node_remove_from_list(&plist, pnode);
                mem_free(pnode);
                pnode = NULL;
            }

            if (pdhcps_pool != NULL) {
            	mem_free(pdhcps_pool);
                pdhcps_pool = NULL;
            }

            return 4;
        }

        s16_t ret = parse_options(&m->options[4], (s16_t)len, pdhcps_pool);

        if (ret == DHCPS_STATE_RELEASE) {
            if (pnode != NULL) {
                node_remove_from_list(&plist, pnode);
                mem_free(pnode);
                pnode = NULL;
            }

            if (pdhcps_pool != NULL) {
            	mem_free(pdhcps_pool);
                pdhcps_pool = NULL;
            }

            memset(&client_address, 0x0, sizeof(client_address));
        }

        DHCPS_LOG("dhcps: xid changed\n");
        DHCPS_LOG("dhcps: client_address.addr = %lx\n", client_address.addr);
        DHCPS_LOG("dhcps: client_address.addr = %s\n", ipaddr_ntoa((ip_addr_t *) &client_address));
		if (pdhcps_pool != NULL) {
        	DHCPS_LOG("dhcps: client_address.name = %s\n", pdhcps_pool->client_name);
		}

        return ret;
    }

    return 0;
}

/******************************************************************************
 * FunctionName : handle_dhcp
 * Description  : If an incoming DHCP message is in response to us, then trigger the state machine
 * Parameters   : arg -- arg user supplied argument (udp_pcb.recv_arg)
 * 				  pcb -- the udp_pcb which received data
 * 			      p -- the packet buffer that was received
 * 				  addr -- the remote IP address from which the packet was received
 * 				  port -- the remote port from which the packet was received
 * Returns      : none
*******************************************************************************/
static void handle_dhcp(void *arg,
                        struct udp_pcb *pcb,
                        struct pbuf *p,
                        const ip_addr_t *addr,
                        u16_t port)
{
	DHCPS_LOG("DHCP Server: Handle DHCP\n");

    struct dhcps_msg *pmsg_dhcps = NULL;
    s16_t tlen, malloc_len;
    u16_t i;
    u16_t dhcps_msg_cnt = 0;
    u8_t *p_dhcps_msg = NULL;
    u8_t *data;

	RA6W1_UNUSED_ARG(arg);
	RA6W1_UNUSED_ARG(pcb);
	RA6W1_UNUSED_ARG(addr);
	RA6W1_UNUSED_ARG(port);

    DHCPS_LOG("\ndhcps: handle_dhcp-> receive a packet\n");

    if (p == NULL) {
        return;
    }

    malloc_len = sizeof(struct dhcps_msg);
    DHCPS_LOG("dhcps: handle_dhcp malloc_len=%d rx_len=%d\n", malloc_len, p->tot_len);

    if (malloc_len < p->tot_len) {
        malloc_len = (s16_t)(p->tot_len);
    }

    pmsg_dhcps = (struct dhcps_msg *)mem_malloc((mem_size_t)malloc_len);
    if (NULL == pmsg_dhcps) {
        pbuf_free(p);
        return;
    }

    memset(pmsg_dhcps , 0x00 , (size_t)malloc_len);
    p_dhcps_msg = (u8_t *)pmsg_dhcps;
    tlen = (s16_t)(p->tot_len);
    data = p->payload;

    DHCPS_LOG("dhcps: handle_dhcp-> p->tot_len = %d\n", tlen);
    DHCPS_LOG("dhcps: handle_dhcp-> p->len = %d\n", p->len);

    for (i = 0; i < p->len; i++) {
        p_dhcps_msg[dhcps_msg_cnt++] = data[i];
    }

    if (p->next != NULL) {
        DHCPS_LOG("dhcps: handle_dhcp-> p->next != NULL\n");
        DHCPS_LOG("dhcps: handle_dhcp-> p->next->tot_len = %d\n", p->next->tot_len);
        DHCPS_LOG("dhcps: handle_dhcp-> p->next->len = %d\n", p->next->len);

        data = p->next->payload;

        for (i = 0; i < p->next->len; i++) {
            p_dhcps_msg[dhcps_msg_cnt++] = data[i];
        }
    }

    DHCPS_LOG("dhcps: handle_dhcp-> parse_msg(p)\n");

    switch (parse_msg(pmsg_dhcps, (u16_t)(tlen - 240))) {
        case DHCPS_STATE_OFFER://1
            DHCPS_LOG("dhcps: handle_dhcp-> DHCPD_STATE_OFFER\n");
            send_offer(pmsg_dhcps, (u16_t)malloc_len);
            break;

        case DHCPS_STATE_ACK://3
            DHCPS_LOG("dhcps: handle_dhcp-> DHCPD_STATE_ACK\n");
            send_ack(pmsg_dhcps, (u16_t)malloc_len);
            break;

        case DHCPS_STATE_NAK://4
            DHCPS_LOG("dhcps: handle_dhcp-> DHCPD_STATE_NAK\n");
            send_nak(pmsg_dhcps, (u16_t)malloc_len);
            break;

        default :
            break;
    }

    DHCPS_LOG("dhcps: handle_dhcp-> pbuf_free(p)\n");
    pbuf_free(p);
    mem_free(pmsg_dhcps);
    pmsg_dhcps = NULL;
}

/******************************************************************************
 * FunctionName : dhcps_poll_set
 * Description  : set ip poll from start to end for station
 * Parameters   : ip -- The current ip addr
 * Returns      : none
*******************************************************************************/
static void dhcps_poll_set(u32_t ip)
{
    u32_t softap_ip = 0, local_ip = 0;
    u32_t start_ip = 0;
    u32_t end_ip = 0;

    if (dhcps_poll.enable == true) {
    	DHCPS_LOG(">> DHCP Poll Enabled\n");
        softap_ip = htonl(ip);
        start_ip = htonl(dhcps_poll.start_ip.addr);
        end_ip = htonl(dhcps_poll.end_ip.addr);

        /*config ip information can't contain local ip*/
        if ((start_ip <= softap_ip) && (softap_ip <= end_ip)) {
            dhcps_poll.enable = false;
        } else {
            /*config ip information must be in the same segment as the local ip*/
            softap_ip >>= 8;

            if (((start_ip >> 8 != softap_ip) || (end_ip >> 8 != softap_ip))
                    || (end_ip - start_ip > DHCPS_MAX_LEASE)) {
                dhcps_poll.enable = false;
            }
        }
    }

    if (dhcps_poll.enable == false) {
    	DHCPS_LOG(">> DHCP Poll Disabled\n");
        local_ip = softap_ip = htonl(ip);
        softap_ip &= 0xFFFFFF00;
        local_ip &= 0xFF;

        if (local_ip >= 0x80) {
            local_ip -= DHCPS_MAX_LEASE;
        } else {
            local_ip ++;
        }

        bzero(&dhcps_poll, sizeof(dhcps_poll));
        dhcps_poll.start_ip.addr = softap_ip | local_ip;
        dhcps_poll.end_ip.addr = softap_ip | (local_ip + DHCPS_MAX_LEASE - 1);
        dhcps_poll.start_ip.addr = htonl(dhcps_poll.start_ip.addr);
        dhcps_poll.end_ip.addr = htonl(dhcps_poll.end_ip.addr);
    }

}


/******************************************************************************
 * FunctionName : dhcps_set_new_lease_cb
 * Description  : set callback for dhcp server when it assign an IP 
 *                to the connected dhcp client
 * Parameters   : cb -- callback for dhcp server
 * Returns      : none
*******************************************************************************/
void dhcps_set_new_lease_cb(dhcps_cb_t cb)
{
    dhcps_cb = cb;
}

/******************************************************************************
 * FunctionName : dhcps_start
 * Description  : start dhcp server function
 * Parameters   : netif -- The current netif addr
 *              : info  -- The current ip info
 * Returns      : none
*******************************************************************************/
void dhcps_start(struct netif *netif, ip4_addr_t ip)
{
	printf("\n>>> DHCP Server Started\n");
    dhcps_netif = netif;

    if (dhcps_netif == NULL)
    {
    	printf(">> DHCP Server netif is null!!\n");
    	return;
    }

    if (dhcps_pcb != NULL) {
        udp_remove(dhcps_pcb);
        dhcps_pcb = NULL;
    }

    dhcps_pcb = udp_new();
    struct udp_pcb *pcb_dhcps = dhcps_pcb;

    if (pcb_dhcps == NULL || ip4_addr_isany_val(ip)) {
        printf("dhcps_start(): could not obtain pcb\n");
        if (pcb_dhcps != NULL) {
            udp_remove(pcb_dhcps);
            dhcps_pcb = NULL;
        }
        return;
    }

    pcb_dhcps->netif_idx = RM_WIFI_NETIF_INDEX(WLAN1_IFACE);

    IP4_ADDR(&broadcast_dhcps, 255, 255, 255, 255);

    server_address.addr = ip.addr;
    dhcps_poll_set(server_address.addr);

    client_address_plus.addr = dhcps_poll.start_ip.addr;

    udp_bind(pcb_dhcps, &netif->ip_addr, DHCPS_SERVER_PORT);

    udp_recv(pcb_dhcps, handle_dhcp, NULL);

    vTaskDelay(portCONVERT_MS_2_TICKS(100));

    DHCPS_LOG("dhcps:dhcps_start->udp_recv function Set a receive callback handle_dhcp for UDP_PCB pcb_dhcps\n");
    DHCPS_LOG(">> DHCP Server Started Complete\n");
}

/******************************************************************************
 * FunctionName : dhcps_stop
 * Description  : stop dhcp server function
 * Parameters   : netif -- The current netif addr
 * Returns      : none
*******************************************************************************/
void dhcps_stop(struct netif *netif)
{
    struct netif *apnetif = netif;

    if (apnetif == NULL) {
    	DHCPS_LOG("dhcps_stop: apnetif == NULL\n");
        return;
    }

    if (dhcps_pcb != NULL) {
        udp_disconnect(dhcps_pcb);
        udp_remove(dhcps_pcb);
        dhcps_pcb = NULL;
    }

    list_node *pnode = NULL;
    list_node *pback_node = NULL;
    pnode = plist;

    while (pnode != NULL) {
        pback_node = pnode;
        pnode = pback_node->pnext;
        node_remove_from_list(&plist, pback_node);
        mem_free(pback_node->pnode);
        pback_node->pnode = NULL;
        mem_free(pback_node);
        pback_node = NULL;
    }
}

/******************************************************************************
 * FunctionName : kill_oldest_dhcps_pool
 * Description  : remove the oldest node from list
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
static void kill_oldest_dhcps_pool(void)
{
    list_node *pre = NULL, *p = NULL;
    list_node *minpre = NULL, *minp = NULL;
    struct dhcps_pool *pdhcps_pool = NULL, *pmin_pool = NULL;
    pre = plist;
    LWIP_ASSERT("Expect the list to have at least 2 nodes", (pre != NULL && pre->pnext != NULL)); // Expect the list to have at least 2 nodes
    p = pre->pnext;
    minpre = pre;
    minp = p;

    while (p != NULL) {
        pdhcps_pool = p->pnode;
        pmin_pool = minp->pnode;

        if (pdhcps_pool->lease_timer < pmin_pool->lease_timer) {
            minp = p;
            minpre = pre;
        }

        pre = p;
        p = p->pnext;
    }

    minpre->pnext = minp->pnext;
    mem_free(minp->pnode);
    minp->pnode = NULL;
    mem_free(minp);
    minp = NULL;
}

/******************************************************************************
 * FunctionName : dhcps_coarse_tmr
 * Description  : the lease time count
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void dhcps_coarse_tmr(void)
{
    u8_t num_dhcps_pool = 0;
    list_node *pback_node = NULL;
    list_node *pnode = NULL;
    struct dhcps_pool *pdhcps_pool = NULL;
    pnode = plist;

    while (pnode != NULL) {
        pdhcps_pool = pnode->pnode;
        pdhcps_pool->lease_timer--;

        if (pdhcps_pool->lease_timer == 0) {
            pback_node = pnode;
            pnode = pback_node->pnext;
            node_remove_from_list(&plist, pback_node);
            mem_free(pback_node->pnode);
            pback_node->pnode = NULL;
            mem_free(pback_node);
            pback_node = NULL;
        } else {
            pnode = pnode->pnext;
            num_dhcps_pool++;
        }
    }

    if (num_dhcps_pool > MAX_STATION_NUM) {
        kill_oldest_dhcps_pool();
    }
}

/******************************************************************************
 * FunctionName : dhcp_search_ip_on_mac
 * Description  : Search ip address based on mac address
 * Parameters   : mac -- The MAC addr
 *				  ip  -- The IP info
 * Returns      : true or false
*******************************************************************************/
bool dhcps_search_ip_on_mac(u8_t *mac, ip4_addr_t *ip)
{
    struct dhcps_pool *pdhcps_pool = NULL;
    list_node *pback_node = NULL;
    bool ret = false;

    for (pback_node = plist; pback_node != NULL; pback_node = pback_node->pnext) {
        pdhcps_pool = pback_node->pnode;

        if (memcmp(pdhcps_pool->mac, mac, sizeof(pdhcps_pool->mac)) == 0) {
            memcpy(&ip->addr, &pdhcps_pool->ip.addr, sizeof(pdhcps_pool->ip.addr));
            ret = true;
            break;
        }
    }

    return ret;
}

/******************************************************************************
 * FunctionName : dhcps_dns_setserver
 * Description  : set DNS server address for dhcpserver
 * Parameters   : dnsserver -- The DNS server address
 * Returns      : none
*******************************************************************************/
void
dhcps_dns_setserver(const ip_addr_t *dnsserver)
{
    if (dnsserver != NULL) {
        dns_server = *(ip_2_ip4(dnsserver));
    } else {
        dns_server = *(ip_2_ip4(IP_ADDR_ANY));
    } 
}

/******************************************************************************
 * FunctionName : dhcps_dns_getserver
 * Description  : get DNS server address for dhcpserver
 * Parameters   : none
 * Returns      : ip4_addr_t
*******************************************************************************/
ip4_addr_t 
dhcps_dns_getserver(void)
{
    return dns_server;
}

static void dhcps_lease_cb(u8_t client_ip[4])
{
	RA6W1_UNUSED_ARG(client_ip);

	DHCPS_LOG("DHCP server assigned IP to a station, IP is: %d.%d.%d.%d\n",
             client_ip[0], client_ip[1], client_ip[2], client_ip[3]);
}

u8_t dhcps_is_running(void);

u8_t dhcps_is_running(void)
{
	if (dhcpsTaskHandle == NULL) {
		return pdFALSE;
	} else {
		return pdTRUE;
	}
}

struct dhcps_pool * dhcps_search_ip_pool(ip4_addr_t ip) {

	struct dhcps_pool *pdhcps_pool;
	list_node * p = NULL;

    if (plist != NULL) {
    	p = plist;

        while (p != NULL) {
            pdhcps_pool = p->pnode;
            p = p->pnext;

            if(ip4_addr_cmp(&ip, &pdhcps_pool->ip)) {
            	DHCPS_LOG("IP:%s Assigned\n", ipaddr_ntoa((ip_addr_t *) &pdhcps_pool->ip));
            	return pdhcps_pool;
            }
        }

    } else {
    	return NULL;
    }

	return NULL;
}

void dhcps_print_lease_pool(u8_t is_print_assigned)
{
    list_node * p = NULL;
    struct dhcps_pool *pdhcps_pool = NULL;
    u32_t count = 0;

    if (dhcps_is_running() == pdFALSE) {
		printf("INFO: DHCP Server is not running.\n");
    	return;
    }

	printf("\n[DHCP Server Lease Table]\n"
		   "[Host]\t\t\t[IP address]%s\t[Lease Expires]\t[ClientName]\n",
		   is_print_assigned ? "\t[Assigned?]" : "");

	print_separate_bar('=', is_print_assigned ? 85 : 70, 1);

	if (is_print_assigned == pdTRUE) {
		ip4_addr_t tempip;
		ip4_addr_t endip;
		ip4_addr_copy(tempip, dhcps_poll.start_ip);
		ip4_addr_copy(endip, dhcps_poll.end_ip);

		tempip.addr = lwip_htonl(tempip.addr);
		endip.addr = lwip_htonl(endip.addr);

		while (tempip.addr <= endip.addr) {
			tempip.addr = lwip_htonl(tempip.addr);
			pdhcps_pool = dhcps_search_ip_pool(tempip);

			if (pdhcps_pool != NULL) {
				printf("%02X:%02X:%02X:%02X:%02X:%02X\t", pdhcps_pool->mac[0],
					pdhcps_pool->mac[1],
					pdhcps_pool->mac[2],
					pdhcps_pool->mac[3],
					pdhcps_pool->mac[4],
					pdhcps_pool->mac[5]);
				printf("%s\t", ipaddr_ntoa((const ip_addr_t *)&pdhcps_pool->ip));
				printf("    %d\t\t", 1);
				printf("    %ld\t", pdhcps_pool->lease_timer);
				printf("%s\n", pdhcps_pool->client_name);

				count++;
			} else {
				printf("%02X:%02X:%02X:%02X:%02X:%02X\t", 0,0,0,0,0,0);
				printf("%s\t", ipaddr_ntoa((const ip_addr_t *)&tempip));
				printf("    %d\t\t", 0);
				printf("    %d\n", 0);
			}

			tempip.addr = lwip_htonl(tempip.addr);
			tempip.addr += 1;
		}
	} else {
		if (plist != NULL) {
			p = plist;

			while (p != NULL) {
				pdhcps_pool = p->pnode;
				printf("%02X:%02X:%02X:%02X:%02X:%02X\t", pdhcps_pool->mac[0],
					pdhcps_pool->mac[1],
					pdhcps_pool->mac[2],
					pdhcps_pool->mac[3],
					pdhcps_pool->mac[4],
					pdhcps_pool->mac[5]);
				printf("%s\t", ipaddr_ntoa((const ip_addr_t *)&pdhcps_pool->ip));
				printf("    %ld\t", pdhcps_pool->lease_timer);
				printf("%s\n", pdhcps_pool->client_name);
				p = p->pnext;

				count++;
			}
		} else {
			DHCPS_LOG("DHCP Server Lease Table is NULL!!\n");
		}
	}

	print_separate_bar('=', is_print_assigned ? 85 : 70, 1);

	printf("Total Lease IP: %ld\n\n", count);
}

void dhcps_set_lease_time(dhcps_time_t ltime)
{
	dhcps_set_option_info(IP_ADDRESS_LEASE_TIME, &ltime, sizeof(dhcps_time_t));
}

void dhcps_set_ip_range(char * sip, char * eip)
{
	dhcps_lease_t dhcps_lease;
	printf("\n>> DHCP IP RANGE: %s ~ %s\n", sip, eip);
	ip4addr_aton(sip, &dhcps_lease.start_ip);
	ip4addr_aton(eip, &dhcps_lease.end_ip);
	dhcps_set_option_info(REQUESTED_IP_ADDRESS, &dhcps_lease, sizeof(dhcps_lease_t));
}

dhcps_lease_t dhcps_get_ip_range(void)
{
	return dhcps_poll;
}

void dhcps_get_info(void)
{
	printf("\nDHCP Server Information:\n");

	struct netif * netif = netif_get_by_index(RM_WIFI_NETIF_INDEX(WLAN1_IFACE));
#ifdef __SUPPORT_MESH__
	ip_addr_t dnsaddr = dhcps_dns_getserver();
#endif	//__SUPPORT_MESH__
#ifdef RM_MAP_PERSISTANT_W
    char * netmask = NULL;
    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, NVR_KEY_NETMASK_1, &netmask);
#else
    char * netmask = DEFAULT_SUBNET_WLAN1;
#endif

	if (netmask == NULL) {
		netmask = DEFAULT_SUBNET_WLAN1;
	}

	if (dhcpsTaskHandle != NULL) {
		printf("\tStatus\t\t\t:\tRunning\n");
		printf("\tDHCP Server IP Address\t:\t%s\n", ipaddr_ntoa(&dhcps_netif->ip_addr));
#ifdef __SUPPORT_MESH__
		printf("\tDNS Server\t\t:\t%s\n", ipaddr_ntoa(&dnsaddr));
#endif // __SUPPORT_MESH__
		printf("\tSubnet mask\t\t:\t%s\n",  ipaddr_ntoa((const ip_addr_t *)&s_dhcps_mask));
#if LWIP_IPV4 && LWIP_IPV6
        if (!ip4_addr_isany_val(dhcps_netif->gw.u_addr.ip4))
#elif LWIP_IPV4
        if (!ip4_addr_isany_val(dhcps_netif->gw))
#endif
		{
			printf("\tDefault Gateway\t\t:\t%s\n",  ipaddr_ntoa(&dhcps_netif->gw));
		}
	} else {
		printf("\tStatus\t\t\t:\tStop\n");
		printf("\tDHCP Server IP Address\t:\t%s\n", ipaddr_ntoa(&netif->ip_addr));
#ifdef __SUPPORT_MESH__
		printf("\tDNS Server\t\t:\t%s\n", ipaddr_ntoa(&dnsaddr));
#endif // __SUPPORT_MESH__
		printf("\tSubnet mask\t\t:\t%s\n",  netmask);
#if LWIP_IPV4 && LWIP_IPV6
        if (!ip4_addr_isany_val(dhcps_netif->gw.u_addr.ip4))
#elif LWIP_IPV4
        if (!ip4_addr_isany_val(dhcps_netif->gw))
#endif
		{
			printf("\tDefault Gateway\t\t:\t%s\n",  ipaddr_ntoa(&netif->gw));
		}
	}

	printf("\tRange of IP Address\t:\t%s ~ ", ipaddr_ntoa((const ip_addr_t *)&dhcps_poll.start_ip));
	printf("%s\n", ipaddr_ntoa((const ip_addr_t *)&dhcps_poll.end_ip));
	printf("\tLease Time\t\t:\t%ld\n",  dhcps_lease_time);
}

static void dhcps_thread(void *arg)
{
    dhcps_time_t lease_time;
    /* Lease IP Range */
    char * startip = NULL;
    char * endip = NULL;

#ifdef __SUPPORT_MESH__
	ip_addr_t dnsaddr;
	dhcps_offer_t offer;
#endif	//__SUPPORT_MESH__
	dhcps_lease_t dhcps_lease;

	dhcps_cmd_param * dhcps_param = (dhcps_cmd_param *)arg;

	if (dhcps_param == NULL) {
		printf("\n>> DHCP Param is Null!!\n");

		if (dhcpsTaskHandle != NULL) {
			dhcpsTaskHandle = NULL;
			vTaskDelete(dhcpsTaskHandle);
		}

		return;
	} else {
		DHCPS_LOG("\n>>> DHCP Server Network Interface:WLAN[%d]\n", dhcps_param->dhcps_interface);
	}

	dhcps_netif = netif_get_by_index(RM_WIFI_NETIF_INDEX(dhcps_param->dhcps_interface));

	if (dhcps_netif == NULL) {
		printf("Network Interface is null\n");

		if (dhcps_param != NULL) {
			vPortFree(dhcps_param);
			dhcps_param = NULL;
		}

		if (dhcpsTaskHandle != NULL) {
			dhcpsTaskHandle = NULL;
			vTaskDelete(dhcpsTaskHandle);
		}

		return;
	}

	dhcps_pcb = NULL;

#ifdef __SUPPORT_MESH__
	if (get_run_mode() == WIFI_DEVICE_MODE_EXT_MESH_POINT
		|| get_run_mode() == WIFI_DEVICE_MODE_EXT_MESH_PORTAL)
	{
		/* DNS */
		if (dhcps_param->dns.addr != 0) {
			ip4_addr_copy(dnsaddr, dhcps_param->dns);
		} else {
			char * dns;
#ifdef __ENABLE_UNUSED__
#if defined ( __SUPPORT_P2P__ )
			if (get_run_mode() == WIFI_DEVICE_MODE_EXT_P2P || get_run_mode() == WIFI_DEVICE_MODE_EXT_P2P_STATION) {
				DHCPS_LOG("\nDHCP Server : P2P mode\n");
				dns = DHCP_SERVER_P2P_DNS_IP;
			}
#endif	// __SUPPORT_P2P__
#endif  //__ENABLE_UNUSED__

#ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, DHCP_SERVER_DNS, &dns);
#endif

			if (dns != NULL) {
				ip4addr_aton(dns, &dnsaddr);
			} else {
				ip4addr_aton(DEFAULT_DNS_WLAN1, &dnsaddr);
			}
		}
	}
#endif // __SUPPORT_MESH__

	/* Lease Time */
	if (dhcps_param->lease_time != 0) {
		lease_time = dhcps_param->lease_time;
	} else {
#if !defined ( __LIGHT_SUPPLICANT__ )
#if defined ( __SUPPORT_P2P__ )
		if (get_run_mode() == WIFI_DEVICE_MODE_EXT_P2P
		|| get_run_mode() == WIFI_DEVICE_MODE_EXT_P2P_GO
		|| get_run_mode() == WIFI_DEVICE_MODE_EXT_P2P_STATION)
		{
			lease_time = DHCP_SERVER_P2P_DEFAULT_LEASE_TIME;
		} else
#endif	// __SUPPORT_P2P__
#endif	// __LIGHT_SUPPLICANT__
#ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG,
                                   DHCP_SERVER_LEASE_TIME, (int *)&lease_time);
#endif
		if ((int)lease_time < 0) {
			lease_time = DEFAULT_DHCP_SERVER_LEASE_TIME;
		}
	}

#if !defined ( __LIGHT_SUPPLICANT__ )
#if defined ( __SUPPORT_P2P__ )
	if (get_run_mode() == WIFI_DEVICE_MODE_EXT_P2P
		|| get_run_mode() == WIFI_DEVICE_MODE_EXT_P2P_GO
		|| get_run_mode() == WIFI_DEVICE_MODE_EXT_P2P_STATION)
	{
		startip = DHCP_SERVER_P2P_START_IP;
		endip = DHCP_SERVER_P2P_END_IP;
	} else
#endif	// __SUPPORT_P2P__
#endif	// __LIGHT_SUPPLICANT__
	{
#ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, DHCP_SERVER_START_IP, &startip);
#endif
#ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, DHCP_SERVER_END_IP, &endip);
#endif
	}

	if (startip != NULL && endip != NULL) {
		ip4addr_aton(startip, &dhcps_lease.start_ip);
		ip4addr_aton(endip, &dhcps_lease.end_ip);   
	} else {
        DHCPS_LOG("\nDHCP Server : set start_ip and end_ip to default (10.0.0.2~10.0.0.11)\n");
   		ip4addr_aton("10.0.0.2", &dhcps_lease.start_ip);
		ip4addr_aton("10.0.0.11", &dhcps_lease.end_ip);        
    }

/*
    // TODO: Always true - need to fix it
    if (startip != DHCP_SERVER_P2P_START_IP) {
*/
//    }
    
/*
    // TODO: Always true - need to fix it
    if (endip != DHCP_SERVER_P2P_END_IP) {
*/
//    }

	dhcps_lease.enable = true;

	vPortFree(dhcps_param);

#ifdef __SUPPORT_MESH__
	if (get_run_mode() == WIFI_DEVICE_MODE_EXT_MESH_POINT
		|| get_run_mode() == WIFI_DEVICE_MODE_EXT_MESH_PORTAL)
	{
		dhcps_dns_setserver(&dnsaddr);
	}
#endif // __SUPPORT_MESH__

	dhcps_set_new_lease_cb(dhcps_lease_cb);
    /* Netmask */
    dhcps_set_option_info(SUBNET_MASK, &dhcps_netif->netmask, sizeof(ip_addr_t));
    dhcps_set_option_info(REQUESTED_IP_ADDRESS, &dhcps_lease, sizeof(dhcps_lease_t));
    dhcps_set_option_info(IP_ADDRESS_LEASE_TIME, &lease_time, sizeof(dhcps_time_t));
#ifdef __SUPPORT_MESH__
	if (get_run_mode() == WIFI_DEVICE_MODE_EXT_MESH_POINT
		|| get_run_mode() == WIFI_DEVICE_MODE_EXT_MESH_PORTAL)
	{
		offer = OFFER_DNS;
	    dhcps_set_option_info(DOMAIN_NAME_SERVER,  &offer, sizeof(dhcps_offer_t));
	}
#endif // __SUPPORT_MESH__
#if LWIP_IPV4 && LWIP_IPV6
    dhcps_start(dhcps_netif, dhcps_netif->ip_addr.u_addr.ip4);
#elif LWIP_IPV4
	dhcps_start(dhcps_netif, dhcps_netif->ip_addr);
#endif

	DHCPS_LOG("\nDHCP Server: %s\n", ipaddr_ntoa((const ip_addr_t *)&dhcps_netif->ip_addr));
#ifdef __SUPPORT_MESH__
	if (get_run_mode() == WIFI_DEVICE_MODE_EXT_MESH_POINT
		|| get_run_mode() == WIFI_DEVICE_MODE_EXT_MESH_PORTAL)
	{
	DHCPS_LOG("  DNS      : %s\n", ipaddr_ntoa((const ip_addr_t *)&dnsaddr));
	}
#endif // __SUPPORT_MESH__

	DHCPS_LOG("  Netmask  : %s\n", ipaddr_ntoa((const ip_addr_t *)&dhcps_netif->netmask));
	DHCPS_LOG("  START IP : %s\n", ipaddr_ntoa((const ip_addr_t *)&dhcps_lease.start_ip));
	DHCPS_LOG("  END IP   : %s\n", ipaddr_ntoa((const ip_addr_t *)&dhcps_lease.end_ip));
	DHCPS_LOG("  Leasetime: %lu sec.\n", lease_time);

	for ( ; ; ) {
		vTaskDelay(portCONVERT_MS_2_TICKS(1000));

        	dhcps_coarse_tmr();
    	}
}

void dhcps_run(void * arg)
{
	dhcps_cmd_param * dhcps_param = (dhcps_cmd_param *)arg;

	if (dhcps_param == NULL) {
		printf("DHCP Server Parameter is NULL\n");
		return;
	}

	if (dhcps_param->cmd == DHCP_SERVER_STATE_START) {
		if (dhcpsTaskHandle == NULL) {
			xTaskCreate(dhcps_thread, "dhcp_server", (DHCPS_THREAD_STACKSIZE),
						(void *)arg, DEFAULT_THREAD_PRIO+4, &dhcpsTaskHandle);
			return;
		} else {
			printf("DHCP Server is already running.\n");
		}

	} else if (dhcps_param->cmd == DHCP_SERVER_STATE_STOP) {
		if (dhcpsTaskHandle != NULL) {
			if (dhcps_netif != NULL) {
				dhcps_stop(dhcps_netif);
				dhcps_netif = NULL;
			}

			vTaskDelete(dhcpsTaskHandle);
			dhcpsTaskHandle = NULL;
			printf("\nDHCP Server stopped.\n");
		} else {
			printf("\nINFO: DHCP Server is not running.\n");
		}
	}

	if (dhcps_param != NULL) {
		vPortFree(dhcps_param);
		dhcps_param = NULL;
	}
}

#endif // LWIP_IPV4
#endif // __SUPPORT_DHCP_SVR__

/*EOF*/

