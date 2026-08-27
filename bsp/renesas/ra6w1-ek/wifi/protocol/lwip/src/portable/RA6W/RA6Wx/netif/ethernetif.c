/**
 * @file
 * Ethernet Interface Skeleton
 *
 */

/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 */

/*
 * This file is a skeleton for developing Ethernet network interface
 * drivers for lwIP. Add code to the low_level functions and do a
 * search-and-replace for the word "ethernetif" to replace it with
 * something that better describes your network interface.
 */

#include "lwipopts.h"
#include "lwip/opt.h"

#include "lwip/def.h"
#include "lwip/sys.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/stats.h"
#include "lwip/snmp.h"
#include "lwip/ethip6.h"
#include "lwip/etharp.h"
#include "netif/ppp/pppoe.h"

/* Define those to better describe your network interface. */
#define IFNAME0 'e'
#define IFNAME1 'n'

/* This is temp definition */
#define TASK_LWIP                             20
#define TASK_LWIP_DRV                         17
#define DRIVER_NETDEV_EVENT_rwnx_start_xmit    2

/**
 * Helper struct to hold private data used to operate your ethernet interface.
 * Keeping the ethernet address of the MAC in this struct is not necessary
 * as it is already kept in the struct netif.
 * But this is only an example, anyway...
 */
struct ethernetif {
  struct eth_addr *ethaddr;
  /* Add whatever per-interface state that is needed here. */
};


extern int getMacAddrMswLsw(UINT iface, ULONG *macmsw, ULONG *maclsw);

#if 0    // Unused code on RA6W1 SDK =================================================
/* Forward declarations. */
static void  ethernetif_input(struct netif *netif);
#endif // 0 ============================================================
#define WLAN1_NET_IFACE		2

/**
 * In this function, the hardware should be initialized.
 * Called from ethernetif_init().
 *
 * @param netif the already initialized lwip network interface structure
 *        for this ethernetif
 */
static void
low_level_init(struct netif *netif)
{
  unsigned long macmsw, maclsw;

  /* set MAC hardware address length */
#if LWIP_IPV4
  netif->hwaddr_len = ETHARP_HWADDR_LEN;
#endif // LWIP_IPV6
#if LWIP_IPV6
  netif->hwaddr_len = ETH_HWADDR_LEN;
#endif // LWIP_IPV6
  //printf("%s netif %d\n", __func__ , netif->num);

  /* If Second Network interface is called, mac address last bit is mask */
  if (netif->num == WLAN1_NET_IFACE)
  {
    getMacAddrMswLsw(1, &macmsw,  &maclsw);
  }
  else
  {
	  getMacAddrMswLsw(0, &macmsw,  &maclsw);
  } 

  /* set MAC hardware address */
  netif->hwaddr[0] = (u8_t)(macmsw >> 8);
  netif->hwaddr[1] = macmsw & 0xff;
  netif->hwaddr[2] = (u8_t)(maclsw >> 24);
  netif->hwaddr[3] = maclsw >> 16 & 0xff;
  netif->hwaddr[4] = maclsw >> 8  & 0xff;
  netif->hwaddr[5] = maclsw & 0xff;

  /* maximum transfer unit */
  netif->mtu = 1500;

  /* device capabilities */
  /* don't set NETIF_FLAG_ETHARP if this device is not an ethernet one */
  netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;

#if LWIP_IPV6 && LWIP_IPV6_MLD

  /*
   * For hardware/netifs that implement MAC filtering.
   * All-nodes link-local is handled by default, so we must let the hardware know
   * to allow multicast packets in.
   * Should set mld_mac_filter previously. */
  if (netif->mld_mac_filter != NULL) {
    ip6_addr_t ip6_allnodes_ll;
    ip6_addr_set_allnodes_linklocal(&ip6_allnodes_ll);
    netif->mld_mac_filter(netif, &ip6_allnodes_ll, NETIF_ADD_MAC_FILTER);
  }
#endif /* LWIP_IPV6 && LWIP_IPV6_MLD */

  /* Do whatever else is needed to initialize interface. */
}

/**
 * This function should do the actual transmission of the packet. The packet is
 * contained in the pbuf that is passed to the function. This pbuf
 * might be chained.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @param p the MAC packet to send (e.g. IP packet including MAC addresses and type)
 * @return ERR_OK if the packet could be sent
 *         an err_t value if the packet couldn't be sent
 *
 * @note Returning ERR_MEM here if a DMA queue of your MAC is full can lead to
 *       strange results. You might consider waiting for space in the DMA queue
 *       to become available since the stack doesn't retry to send a packet
 *       dropped because of memory failure (except for the TCP timers).
 */
#define ETHER_DIRECT_WIFI_TX	/* definition is also checked in umac_netif.c */
#ifdef ETHER_DIRECT_WIFI_TX
extern u32    rwnx_driver_task_send_event(u32 from, u32 to, u32 event, void * p_parameter);
extern int rwnx_free_txq_count(void);
extern bool rwnx_empty_txq_item(void);
int wifi_fros_netif_tx_cntrl(struct netif *netif, struct pbuf *packet_ptr)
{
    int status;
    LWIP_DEBUGF(NETIF_DEBUG, ("[%s], start\n",__func__));
    if (packet_ptr->if_idx == 0) // if pbuf interface num is wrong, get a netif interface number
    {
        packet_ptr->if_idx = netif_get_index(netif);
        LWIP_DEBUGF(NETIF_DEBUG, ("[%s] index = %d\n",__func__, packet_ptr->if_idx));
    }

    if (!netif_is_up(netif))
    {
        pbuf_free(packet_ptr);
        LWIP_DEBUGF(NETIF_DEBUG, ("%s:: Netif is not up(%d)\n", __func__ , netif->num));
        return ERR_IF;
    }

    /* By Network interface number, get the MAC WiFi interface number */
    /* Second Interface (second netif num + 1)*/
    if (packet_ptr->if_idx == 1 || packet_ptr->if_idx == 3) {
        packet_ptr->if_idx = 1;
    }
    else if( packet_ptr->if_idx == 2) {
        /* First Inerface as default */
        packet_ptr->if_idx = 0;
    } else {
    	LWIP_DEBUGF(NETIF_DEBUG, ("[%s] please TX packet index check(%u)\n", __func__, (u16_t) packet_ptr->if_idx));
    }

    if(!rwnx_free_txq_count()|| rwnx_empty_txq_item())
    {
      return ERR_MEM;
    }

    pbuf_ref(packet_ptr);
    status = rwnx_driver_task_send_event(TASK_LWIP, TASK_LWIP_DRV, DRIVER_NETDEV_EVENT_rwnx_start_xmit, packet_ptr);
    if (status != pdTRUE)
    {
        pbuf_free(packet_ptr);
        return ERR_MEM;
    }

    return ERR_OK;

}

#else
extern void wifi_fros_netif_tx_cntrl(struct pbuf *packet_ptr);
#endif
#if 0
struct pbuf * bridge_first = NULL;
struct pbuf * bridge_last = NULL;

struct netif *rrq6xxx_bridge_push(struct pbuf *p)
{
  struct netif *netif = NULL;
  struct pbuf * last;

  SYS_ARCH_DECL_PROTECT(lev);
  
  netif = netif_get_by_index(p->if_idx);

  SYS_ARCH_PROTECT(lev);
  /* let last point to the last pbuf in chain r */
  for (last = p; last->next != NULL; last = last->next) {
    /* nothing to do here, just get to the last pbuf */
  }

  if (bridge_first != NULL) {
    bridge_last->next = p;
    bridge_last = last;
  } else {
    bridge_first = p;
    bridge_last = last;
  }
  SYS_ARCH_UNPROTECT(lev);

  return netif;
}

void rrq6xxx_bridge_mbox(struct netif *netif)
{
    int status = ERR_BUF;

    SYS_ARCH_DECL_PROTECT(lev);

    if(netif)
    {
        if(!(netif->flags & NETIF_FLAG_UP))
            netif->flags |= NETIF_FLAG_UP;

      SYS_ARCH_PROTECT(lev);
      while (bridge_first != NULL) {
        struct pbuf *in, *in_end;
        in = in_end = bridge_first;
        while (in_end->len != in_end->tot_len) {
          in_end = in_end->next;
        }

        if (in_end == bridge_last) {
          bridge_first = bridge_last = NULL;
        } else {
          bridge_first = in_end->next;
        }

        in_end->next = NULL;
        SYS_ARCH_UNPROTECT(lev);

        if (in->if_idx == 1 || in->if_idx == 3) {
            in->if_idx = 1;
        }
        else if( in->if_idx == 2) {
            in->if_idx = 0;
        } else {
          LWIP_DEBUGF(NETIF_DEBUG, "[%s] please TX packet index check(%d) \n",in->if_idx);
        }

        status = rwnx_driver_task_send_event(TASK_LWIP, TASK_LWIP_DRV, DRIVER_NETDEV_EVENT_rwnx_start_xmit, in);
        if (status != pdTRUE)
        {
            //printf("[%s](%d)\n",__func__, __LINE__);
            pbuf_free(in);
        } 
        SYS_ARCH_PROTECT(lev);
      }
      SYS_ARCH_UNPROTECT(lev);
  }
}
#endif

void rrq16x_lwip_mbox_rx(struct pbuf *p)
{
    struct netif *netif = NULL;

    netif = netif_get_by_index(p->if_idx);

    /* Send data to ip stack */
    if (netif) {
        if (!(netif->flags & NETIF_FLAG_UP)) {
            pbuf_free(p);
            return;
        }

        if (netif->input(p, netif) != ERR_OK) {
            pbuf_free(p);
        }
    }
    else
    {
        pbuf_free(p);
    }
}

static err_t
low_level_output(struct netif *netif, struct pbuf *p)
{

#ifdef ETHER_DIRECT_WIFI_TX 

	return (err_t)wifi_fros_netif_tx_cntrl(netif, p);

#else
	struct ethernetif *ethernetif = netif->state;
  	struct pbuf *q;
  //initiate transfer();

#if ETH_PAD_SIZE
  pbuf_remove_header(p, ETH_PAD_SIZE); /* drop the padding word */
#endif

  for (q = p; q != NULL; q = q->next) {
  	char *pPacket;
    /* Send the data from the pbuf to the interface, one pbuf at a
       time. The size of the data in each pbuf is kept in the ->len
       variable. */
    //send data from(q->payload, q->len);
    pPacket = (char *)q->payload;
    //printf("%x len->%d %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x \n", q, q->len , pPacket[0], pPacket[1], pPacket[2], pPacket[3], pPacket[4], pPacket[5], pPacket[6], pPacket[7], 
    //	pPacket[8], pPacket[9], pPacket[10], pPacket[11], pPacket[12], pPacket[13], pPacket[14] );

	wifi_fros_netif_tx_cntrl(q);
  }

#if ETH_PAD_SIZE
  pbuf_add_header(p, ETH_PAD_SIZE); /* reclaim the padding word */
#endif

  //LINK_STATS_INC(link.xmit);
  return ERR_OK;

#endif
}

#if 0    // Unused code on RA6W1 SDK =================================================
/**
 * Should allocate a pbuf and transfer the bytes of the incoming
 * packet from the interface into the pbuf.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return a pbuf filled with the received packet (including MAC header)
 *         NULL on memory error
 */
static struct pbuf *
low_level_input(struct netif *netif)
{
  struct ethernetif *ethernetif = netif->state;
  struct pbuf *p, *q;
  u16_t len;

  printf("%s +++++\r\n", __func__);
#if 0
  /* Obtain the size of the packet and put it into the "len"
     variable. */
  len = ;

#if ETH_PAD_SIZE
  len += ETH_PAD_SIZE; /* allow room for Ethernet padding */
#endif

  /* We allocate a pbuf chain of pbufs from the pool. */
  p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);

  if (p != NULL) {

#if ETH_PAD_SIZE
    pbuf_remove_header(p, ETH_PAD_SIZE); /* drop the padding word */
#endif

    /* We iterate over the pbuf chain until we have read the entire
     * packet into the pbuf. */
    for (q = p; q != NULL; q = q->next) {
      /* Read enough bytes to fill this pbuf in the chain. The
       * available data in the pbuf is given by the q->len
       * variable.
       * This does not necessarily have to be a memcpy, you can also preallocate
       * pbufs for a DMA-enabled MAC and after receiving truncate it to the
       * actually received size. In this case, ensure the tot_len member of the
       * pbuf is the sum of the chained pbuf len members.
       */
      read data into(q->payload, q->len);
    }
    acknowledge that packet has been read();

    MIB2_STATS_NETIF_ADD(netif, ifinoctets, p->tot_len);
    if (((u8_t *)p->payload)[0] & 1) {
      /* broadcast or multicast packet*/
      MIB2_STATS_NETIF_INC(netif, ifinnucastpkts);
    } else {
      /* unicast packet*/
      MIB2_STATS_NETIF_INC(netif, ifinucastpkts);
    }
#if ETH_PAD_SIZE
    pbuf_add_header(p, ETH_PAD_SIZE); /* reclaim the padding word */
#endif

    LINK_STATS_INC(link.recv);
  } else {
    drop packet();
    LINK_STATS_INC(link.memerr);
    LINK_STATS_INC(link.drop);
    MIB2_STATS_NETIF_INC(netif, ifindiscards);
  }
#endif
  return p;
}

/**
 * This function should be called when a packet is ready to be read
 * from the interface. It uses the function low_level_input() that
 * should handle the actual reception of bytes from the network
 * interface. Then the type of the received packet is determined and
 * the appropriate input function is called.
 *
 * @param netif the lwip network interface structure for this ethernetif
 */
static void
ethernetif_input(struct netif *netif)
{
  struct ethernetif *ethernetif;
  struct eth_hdr *ethhdr;
  struct pbuf *p;

  ethernetif = netif->state;

  printf("%s +++++\r\n", __func__);
  /* move received packet into a new pbuf */
  p = low_level_input(netif);
  /* if no packet could be read, silently ignore this */
  if (p != NULL) {
    /* pass all packets to ethernet_input, which decides what packets it supports */
    if (netif->input(p, netif) != ERR_OK) {
      LWIP_DEBUGF(NETIF_DEBUG, "ethernetif_input: IP input error\n");
      pbuf_free(p);
      p = NULL;
    }
  }
}
#endif // 0 ============================================================

/**
 * Should be called at the beginning of the program to set up the
 * network interface. It calls the function low_level_init() to do the
 * actual setup of the hardware.
 *
 * This function should be passed as a parameter to netif_add().
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return ERR_OK if the loopif is initialized
 *         ERR_MEM if private data couldn't be allocated
 *         any other err_t on error
 */
err_t
ethernetif_init(struct netif *netif)
{
  struct ethernetif *ethernetif;

  LWIP_ASSERT("netif != NULL", (netif != NULL));

  ethernetif = mem_malloc(sizeof(struct ethernetif));
  if (ethernetif == NULL) {
    LWIP_DEBUGF(NETIF_DEBUG, ("ethernetif_init: out of memory\n"));
    return ERR_MEM;
  }

#if LWIP_NETIF_HOSTNAME
  /* Initialize interface hostname */
  netif->hostname = "lwip";
#endif /* LWIP_NETIF_HOSTNAME */

  /*
   * Initialize the snmp variables and counters inside the struct netif.
   * The last argument should be replaced with your link speed, in units
   * of bits per second.
   */
  //MIB2_INIT_NETIF(netif, snmp_ifType_ethernet_csmacd, LINK_SPEED_OF_YOUR_NETIF_IN_BPS);

  netif->state = ethernetif;
  netif->name[0] = IFNAME0;
  netif->name[1] = IFNAME1;
  /* We directly use etharp_output() here to save a function call.
   * You can instead declare your own function an call etharp_output()
   * from it if you have to do some checks before sending (e.g. if link
   * is available...) */
#if LWIP_IPV4
  netif->output = etharp_output;
#endif
#if LWIP_IPV6
  netif->output_ip6 = ethip6_output;
#endif /* LWIP_IPV6 */
  netif->linkoutput = low_level_output;

  ethernetif->ethaddr = (struct eth_addr *) & (netif->hwaddr[0]);

  /* initialize the hardware */
  low_level_init(netif);

  return ERR_OK;
}
