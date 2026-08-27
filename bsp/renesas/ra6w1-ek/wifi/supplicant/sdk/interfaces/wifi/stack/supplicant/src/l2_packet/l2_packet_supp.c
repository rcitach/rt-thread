/**
 ****************************************************************************************
 *
 * @file l2_packet_supp.c
 *
 * @brief Layer 2 packet handling
 *
 * Copyright (c) 2023 Renesas Electronics. All rights reserved.
 *
 * This software ("Software") is owned by Renesas Electronics.
 *
 * By using this Software you agree that Renesas Electronics retains all
 * intellectual property and proprietary rights in and to this Software and any
 * use, reproduction, disclosure or distribution of the Software without express
 * written permission or a license agreement from Renesas Electronics is
 * strictly prohibited. This Software is solely for use on or in conjunction
 * with Renesas Electronics products.
 *
 * EXCEPT AS OTHERWISE PROVIDED IN A LICENSE AGREEMENT BETWEEN THE PARTIES, THE
 * SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. EXCEPT AS OTHERWISE
 * PROVIDED IN A LICENSE AGREEMENT BETWEEN THE PARTIES, IN NO EVENT SHALL
 * RENESAS ELECTRONICS BE LIABLE FOR ANY DIRECT, SPECIAL, INDIRECT, INCIDENTAL,
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THE SOFTWARE.
 *
 ****************************************************************************************
 */

#include "FreeRTOS.h"
#include "custom_config_sdk.h"
#include "supp_def.h"
#include "supp_common.h"
#include "wpabuf.h"
#include "supp_debug.h"
#include "supp_eloop.h"
#include "l2_packet.h"
#include "lwip/netif.h"
#include "os.h"
#include "supp_driver.h"
#include "wpa_supplicant_i.h"
#include "driver_fc80211.h"
#include "lwip/pbuf.h"
#include "eapol_common.h"
#include "iface_defs.h"

extern int wpa_supp_ifname_to_ifindex(const char *ifname);
extern struct wpa_supplicant *get_wpa_supplicant(void);
extern int check_net_init(int iface);
extern int i3ed11_l2data_from_sp(int ifindex, unsigned char *data, unsigned int length,
				 bool hasMacHeader, const char *dst);


#ifdef	L2_PACKET_OLD_TYPE
/* L2_PACKET communication by event flags and message queue */
ra6wx_l2_pkt_msg_t l2_pkt_rx_buf;
ra6wx_l2_pkt_msg_t *l2_pkt_rx_buf_p = (ra6wx_l2_pkt_msg_t *)&l2_pkt_rx_buf;

TX_QUEUE ra6wx_l2_pkt_rx_q;
UCHAR	ra6wx_l2_pkt_rx_msg_q_pool[RA6WX_L2_PKT_Q_SIZE];

/* Mutex resources */
TX_MUTEX l2_pkt_mutex;

int l2_pkt_msg_q_init(void)
{
	int	status;
	int	rtn = ERR_OK;

	TX_FUNC_START("            ");

	/* Create Message Queue for FC9000 Driver Event */
	memset(&l2_pkt_rx_buf, 0, sizeof(ra6wx_l2_pkt_msg_t));

	/* UMAC L2_Packet Message Queue */
	status = tx_queue_create(&ra6wx_l2_pkt_rx_q, "l2_pkt_rx_q",
			RA6WX_L2_PKT_Q_SIZE / 4,
			(void *)ra6wx_l2_pkt_rx_msg_q_pool,
			RA6WX_L2_PKT_Q_SIZE);
	if (status != TX_SUCCESS) {
		ra6wx_err_prt("[%s] ra6wx_l2_pkt_rx_q create fail (stauts=%d)\n",
				__func__, status);
		rtn = RA6WX_L2_PKT_RX_Q_CREATE_FAIL;
	}

	/* Create mutex for l2_packet_rx */
	status = tx_mutex_create(&l2_pkt_mutex, "l2_pkt_mutex", TX_NO_INHERIT);
	if (status != ERR_OK) {
		ra6wx_err_prt("[%s] l2_pkt_mutex create fail (status=%d) !!!\n",
				__func__, status);
	}

	TX_FUNC_END("            ");

	return rtn;
}
#endif	/* L2_PACKET_OLD_TYPE */

struct l2_packet_data *l2_packet_init(
	const char *ifname,
	const u8 *own_addr,
	unsigned short protocol,
	void (*rx_callback)(void *ctx, const u8 *src_addr,
					const u8 *buf, size_t len),
	void *rx_callback_ctx,
	int l2_hdr)
{
	struct l2_packet_data *l2;
#ifdef	L2_PACKET_OLD_TYPE
	UINT	status;
#endif	/* L2_PACKET_OLD_TYPE */

	TX_FUNC_START("          ");

	l2 = os_zalloc(sizeof(struct l2_packet_data));
	if (l2 == NULL) {
		ra6wx_eapol_prt("          [%s] l2 memory alloc error\n", __func__);
		return NULL;
	}

	os_strlcpy(l2->ifname, ifname, sizeof(l2->ifname));
	l2->ifindex = wpa_supp_ifname_to_ifindex(ifname);
	l2->rx_callback = rx_callback;
	l2->rx_callback_ctx = rx_callback_ctx;
	l2->l2_hdr = l2_hdr;

	/* by Bright : for FC9000 */
	memcpy(l2->own_addr, own_addr, ETH_ALEN);

#ifdef	L2_PACKET_OLD_TYPE
	/* by Bright :
	 *	Have to get a rx_event and read message queues
	 */
	status = l2_pkt_msg_q_init();
	if (status != ERR_OK) {
		free(l2);

		ra6wx_notice_prt("[%s] L2 MsgQ create error\n", __func__);
		return NULL;
	}
#endif	/* L2_PACKET_OLD_TYPE */

	for(int i=0; i < PREASSIGN_BUF; i++)
		l2->p_buf[i] = pbuf_alloc(PBUF_TRANSPORT, L2_PBUF_SIZE, PBUF_RAM);

	TX_FUNC_END("          ");

	return l2;
}

void l2_packet_deinit(struct l2_packet_data *l2)
{
	TX_FUNC_START("          ");

	if (l2 == NULL) {
		return;
	}

	for(int i=0; i < PREASSIGN_BUF; i++)
		pbuf_free(l2->p_buf[i]);

	os_free(l2);

	TX_FUNC_END("          ");
}


void l2_packet_receive(ULONG data)
{
    extern void wpa_supplicant_rx_eapol(void *ctx, const u8 *src_addr,
                    const u8 *buf, size_t len);

    struct wpa_supplicant *wpa_s;
    struct ieee8023_hdr *hdr;
    struct pbuf	*pbuf_p;
    u8     dst[ETH_ALEN];

    TX_FUNC_START("          ");

    wpa_s = get_wpa_supplicant();

    if (!wpa_s) {
        pbuf_free((struct pbuf *)data);
        printf(RED_COLOR " <%s> wpa_s error \n" CLEAR_COLOR, __func__);
        return;
    }

    pbuf_p = (struct pbuf *)data;
    hdr = (struct ieee8023_hdr *) pbuf_p->payload;

    memcpy(dst, hdr->dest, ETH_ALEN);

    ra6wx_eapol_prt("<%s> dst = " MACSTR "\n", __func__, MAC2STR(hdr->dest));
#if 0
	ra6wx_eapol_prt("<%s> src = " MACSTR "\n", __func__, MAC2STR(hdr->src));
	ra6wx_eapol_prt("<%s> ethertype = 0x%x \n", __func__, hdr->ethertype);
	ra6wx_eapol_prt("<%s> pbuf_p = 0x%x \n", __func__, pbuf_p);
	ra6wx_eapol_prt("<%s> payload = 0x%x \n", __func__, pbuf_p->payload);
	ra6wx_eapol_prt("<%s> tot_len = %d \n", __func__, pbuf_p->tot_len);
#endif
    if (pbuf_p->if_idx == RM_WIFI_NETIF_INDEX(WLAN0_IFACE)) {
        ra6wx_eapol_prt("<%s> wpa_s->own_addr = " MACSTR "\n", __func__, MAC2STR(wpa_s->parent->own_addr));
        
        if (os_memcmp(dst, wpa_s->parent->own_addr, ETH_ALEN) == 0) {
            wpa_supplicant_rx_eapol(wpa_s->parent,
                                    hdr->src,
                                    (u8 *) pbuf_p->payload+6+6+2,
                                    pbuf_p->tot_len-6-6-2);
        }
        else {
            printf(RED_COLOR " [%s] diff dest addr dst = " MACSTR " wpa_s->own_addr = " MACSTR "\n" CLEAR_COLOR,
                                __func__, MAC2STR(hdr->dest), MAC2STR(wpa_s->parent->own_addr));
        }

    } else if (pbuf_p->if_idx == RM_WIFI_NETIF_INDEX(WLAN1_IFACE)) {
        ra6wx_eapol_prt("<%s> wpa_s->own_addr = " MACSTR "\n", __func__, MAC2STR(wpa_s->own_addr));

        if (os_memcmp(dst, wpa_s->own_addr, ETH_ALEN) == 0) {
            wpa_supplicant_rx_eapol(wpa_s,
                                    hdr->src,
                                    (u8 *) pbuf_p->payload+6+6+2,
                                    pbuf_p->tot_len-6-6-2);
        }
        else {
            printf(RED_COLOR " [%s] diff dest addr dst = " MACSTR " wpa_s->own_addr = " MACSTR "\n" CLEAR_COLOR,
                                __func__, MAC2STR(hdr->dest), MAC2STR(wpa_s->own_addr));
        }

    }

    pbuf_free(pbuf_p);

    TX_FUNC_END("          ");
}

int l2_packet_send(struct l2_packet_data *l2, const u8 *dst_addr, u16 proto,
		   const u8 *buf, size_t len)
{
	TX_FUNC_START("");
	int ret = 0;

	if (l2 == NULL)
		return -1;

	if (l2->l2_hdr) {
		ra6wx_eapol_prt("[%s] What do I do for l2_hdr exist case ???\n",
						__func__);
	}

	/* direct function call to UMAC */
	ret = i3ed11_l2data_from_sp(l2->ifindex, (unsigned char *)buf, (unsigned int)len, FALSE,
				 (const char *)dst_addr);

	TX_FUNC_END("");

	return ret;
}

int l2_packet_get_own_addr(struct l2_packet_data *l2, u8 *addr)
{
	memcpy(addr, l2->own_addr, ETH_ALEN);

	return 0;
}

int l2_packet_get_ip_addr(struct l2_packet_data *l2, char *buf, size_t len)
{
	struct netif *netif = netif_get_by_index(RM_WIFI_NETIF_INDEX(l2->ifindex));

	/* Check the network initialization */
	if (check_net_init(l2->ifindex) != pdPASS) {
		return -1;
	}

	if (netif == NULL) {
		return -1;
	}
#if defined (CONFIG_IPV4) && defined (CONFIG_IPV6)
	strcpy(buf, ipaddr_ntoa(&netif->ip_addr));
#elif defined (CONFIG_IPV4)
	strcpy(buf, ipaddr_ntoa(&netif->ip_addr));
#elif defined (CONFIG_IPV6)
	strcpy(buf, ipaddr_ntoa(&netif->ip6_addr[0]));
#endif
	return 0;
}

struct l2_packet_data *get_l2_packet_data(void)
{
	struct wpa_supplicant *wpa_s = NULL;

	wpa_s = get_wpa_supplicant();

	if((!wpa_s)||(!wpa_s->l2))
		return NULL;
	
	return wpa_s->l2;
}

void l2_packet_notify_auth_start(struct l2_packet_data *l2)
{
}

/* EOF */
