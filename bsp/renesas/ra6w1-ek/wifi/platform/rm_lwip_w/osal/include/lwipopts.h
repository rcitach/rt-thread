/*
 * Copyright (c) 2001-2003 Swedish Institute of Computer Science.
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
#ifndef LWIP_LWIPOPTS_H
#define LWIP_LWIPOPTS_H

/* For Matter RA6W1 SDK .......... */
#include "FreeRTOS.h"
#if (CFG_WIFI == 1)
#include "custom_config_sdk.h"
#endif
/* ................................. */
#if (CFG_WIFI == 0)
    #ifndef __SUPPORT_IPV4__
        #define __SUPPORT_IPV4__ 1
    #endif

    #ifndef __SUPPORT_IPV6__
        #define __SUPPORT_IPV6__ 1
    #endif
#endif //CFG_WIFI

#define LWIP_IPV4                  1
#define LWIP_IPV6_DHCP6            1
#define LWIP_IPV6                  1


#include "lwipopts_freertos.h"

#define NO_SYS                     0

#if 0  // Defined in lwipopts_freertos.h ..........................
#define LWIP_SOCKET                (NO_SYS==0)
#endif // 0 .......................................................

#define LWIP_NETCONN               (NO_SYS==0)
#define LWIP_NETIF_API             (NO_SYS==0)

#define LWIP_IGMP                  LWIP_IPV4
#define LWIP_ICMP                  LWIP_IPV4

#define LWIP_SNMP                  LWIP_UDP
#define MIB2_STATS                 LWIP_SNMP
#ifdef LWIP_HAVE_MBEDTLS
#define LWIP_SNMP_V3               (LWIP_SNMP)
#endif

#if 0  // Defined in lwipopts_freertos.h ..........................
#define LWIP_DNS                   LWIP_UDP
#endif // 0 .......................................................
#define LWIP_MDNS_RESPONDER        LWIP_UDP

#define LWIP_NUM_NETIF_CLIENT_DATA (LWIP_MDNS_RESPONDER)

#define LWIP_HAVE_LOOPIF           1
#define LWIP_NETIF_LOOPBACK        1
#if 0  // Defined in lwipopts_freertos.h ..........................
#define LWIP_LOOPBACK_MAX_PBUFS    10
#endif // 0 .......................................................

#define TCP_LISTEN_BACKLOG         1

#define LWIP_COMPAT_SOCKETS        1
#define LWIP_SO_SNDTIMEO           1
#define LWIP_SO_RCVTIMEO           1
#define LWIP_SO_RCVBUF             1

#define LWIP_FREERTOS_CHECK_CORE_LOCKING	1
/* Disabled by UMAC team when 19/Nov/2020
 *  When enabled this feature, it is the reason of Exception fault when Rx case.
 */
#if defined(dg_configMATTER_PLATFORM)
#define LWIP_TCPIP_CORE_LOCKING         1
#else
#define LWIP_TCPIP_CORE_LOCKING         0 // 1
#endif

#define LWIP_NETIF_LINK_CALLBACK        1
#define LWIP_NETIF_STATUS_CALLBACK      1
#define LWIP_NETIF_EXT_STATUS_CALLBACK  1

/* ---------- Memory options ---------- */
/* MEM_ALIGNMENT: should be set to the alignment of the CPU for which
   lwIP is compiled. 4 byte alignment -> define MEM_ALIGNMENT to 4, 2
   byte alignment -> define MEM_ALIGNMENT to 2. */
/* MSVC port: intel processors don't need 4-byte alignment,
   but are faster that way! */
#define MEM_ALIGNMENT           4U

#if 0  // Defined in lwipopts_freertos.h ..........................
/* MEM_SIZE: the size of the heap memory. If the application will send
a lot of data that needs to be copied, this should be set high. */
#define MEM_SIZE               10240
#endif // 0 .......................................................

/* MEMP_NUM_PBUF: the number of memp struct pbufs. If the application
   sends a lot of data out of ROM (or other static memory), this
   should be set high. */
#define MEMP_NUM_PBUF           64	/* 16->64 20200724*/
/* MEMP_NUM_RAW_PCB: the number of UDP protocol control blocks. One
   per active RAW "connection". */
#define MEMP_NUM_RAW_PCB        3

#if 0  // Defined in lwipopts_freertos.h ..........................
/* MEMP_NUM_UDP_PCB: the number of UDP protocol control blocks. One
   per active UDP "connection". */
#define MEMP_NUM_UDP_PCB        8
/* MEMP_NUM_TCP_PCB: the number of simulatenously active TCP
   connections. */
#define MEMP_NUM_TCP_PCB        5
/* MEMP_NUM_TCP_PCB_LISTEN: the number of listening TCP
   connections. */
#define MEMP_NUM_TCP_PCB_LISTEN 8
/* MEMP_NUM_TCP_SEG: the number of simultaneously queued TCP
   segments. */
#define MEMP_NUM_TCP_SEG        16
#endif // 0 .......................................................

/* MEMP_NUM_SYS_TIMEOUT: the number of simulateously active
   timeouts. */
#define MEMP_NUM_SYS_TIMEOUT    17

/* The following four are used only with the sequential API and can be
   set to 0 if the application only will use the raw API. */
/* MEMP_NUM_NETBUF: the number of struct netbufs. */
//#define MEMP_NUM_NETBUF         20	/* 20190813 2->20 */
/* MEMP_NUM_NETCONN: the number of struct netconns. */
#if 0  // Defined in lwipopts_freertos.h ..........................
#define MEMP_NUM_NETCONN        12
#endif // 0 .......................................................

/* MEMP_NUM_TCPIP_MSG_*: the number of struct tcpip_msg, which is used
   for sequential API communication and incoming packets. Used in
   src/api/tcpip.c. */
#define MEMP_NUM_TCPIP_MSG_API   16
#define MEMP_NUM_TCPIP_MSG_INPKT 16


/* ---------- Pbuf options ---------- */
/* PBUF_POOL_SIZE: the number of buffers in the pbuf pool. */
#define PBUF_POOL_SIZE          30	/*120->35*/
#define MEMP_NUM_NETBUF         PBUF_POOL_SIZE	/* 20190813 2->20 */

#if 0
/* ----------- UMAC SKB options -------- */
#define UMAC_TX_SKB                    37
#define UMAC_RX_SKB                    PBUF_POOL_SIZE
#endif

#if 0  // Defined in lwipopts_freertos.h ..........................
/* PBUF_POOL_BUFSIZE: the size of each pbuf in the pbuf pool. */
#define PBUF_POOL_BUFSIZE       256
#endif // 0 .......................................................

/* WiFi pkt buffer size , it should be refered socket buffer for MAC Protocol */
/* !!! do not change !!! */
#define MAC_PKT_BUF_SIZE		116

/** SYS_LIGHTWEIGHT_PROT
 * define SYS_LIGHTWEIGHT_PROT in lwipopts.h if you want inter-task protection
 * for certain critical regions during buffer allocation, deallocation and memory
 * allocation and deallocation.
 */
#define SYS_LIGHTWEIGHT_PROT    (NO_SYS==0)

/* ---------- TCP options ---------- */
#define LWIP_TCP                1
#define TCP_TTL                 255

#define LWIP_ALTCP              (LWIP_TCP)
#ifdef LWIP_HAVE_MBEDTLS
#define LWIP_ALTCP_TLS          (LWIP_TCP)
#define LWIP_ALTCP_TLS_MBEDTLS  (LWIP_TCP)
#endif


/* Controls if TCP should queue segments that arrive out of
   order. Define to 0 if your device is low on memory. */
#define TCP_QUEUE_OOSEQ         (LWIP_TCP)

#if 0  // Defined in lwipopts_freertos.h ..........................
/* TCP Maximum segment size. */
#define TCP_MSS                 1024

/* TCP sender buffer space (bytes). */
#define TCP_SND_BUF             2048
#endif // 0 .......................................................

/* TCP sender buffer space (pbufs). This must be at least = 2 *
   TCP_SND_BUF/TCP_MSS for things to work. */
#define TCP_SND_QUEUELEN       (4 * TCP_SND_BUF/TCP_MSS)

/* TCP writable space (bytes). This must be less than or equal
   to TCP_SND_BUF. It is the amount of space which must be
   available in the tcp snd_buf for select to return writable */
#define TCP_SNDLOWAT           (TCP_SND_BUF/2)

#if 0  // Defined in lwipopts_freertos.h ..........................
/* TCP receive window. */
#define TCP_WND                 (20 * 1024)
#endif // 0 .......................................................

/* Maximum number of retransmissions of data segments. */
#define TCP_MAXRTX              12

/* Maximum number of retransmissions of SYN segments. */
#define TCP_SYNMAXRTX           4

/* When TCP is finished, the registered callback function is called. */
#define TCP_FINISHED_CALLBACK   1

/* TCP Keepalive support */
#define LWIP_TCP_KEEPALIVE      1

/* ---------- ARP options ---------- */
#define LWIP_ARP                LWIP_IPV4
#define ARP_TABLE_SIZE          10
#define ARP_QUEUEING            1


/* ---------- IP options ---------- */
/* Define IP_FORWARD to 1 if you wish to have the ability to forward
   IP packets across network interfaces. If you are going to run lwIP
   on a device with only one network interface, define this to 0. */
#define IP_FORWARD              0	// 1



/* IP reassembly and segmentation.These are orthogonal even
 * if they both deal with IP fragments */
#define IP_REASSEMBLY           1
#define IP_REASS_MAX_PBUFS      (10 * ((1500 + PBUF_POOL_BUFSIZE - 1) / PBUF_POOL_BUFSIZE))
#define MEMP_NUM_REASSDATA      IP_REASS_MAX_PBUFS
#define IP_FRAG                 1
#define IPV6_FRAG_COPYHEADER    1

/* ---------- ICMP options ---------- */
#define ICMP_TTL                255

#if 0  // Defined in lwipopts_freertos.h ..........................
/* ---------- DHCP options ---------- */
/* Define LWIP_DHCP to 1 if you want DHCP configuration of
   interfaces. */
#define LWIP_DHCP               LWIP_UDP

/* 1 if you want to do an ARP check on the offered address
   (recommended). */
#define DHCP_DOES_ARP_CHECK    (LWIP_DHCP)
#endif // 0 .......................................................


/* ---------- AUTOIP options ------- */
#define LWIP_AUTOIP            (LWIP_DHCP)
#define LWIP_DHCP_AUTOIP_COOP  (LWIP_DHCP && LWIP_AUTOIP)


/* ---------- UDP options ---------- */
#define LWIP_UDP                1
#define LWIP_UDPLITE            LWIP_UDP
#define UDP_TTL                 255


/* ---------- RAW options ---------- */
#define LWIP_RAW                1


/* ---------- Statistics options ---------- */

#define LWIP_STATS              1
#define LWIP_STATS_DISPLAY      1

#if LWIP_STATS
#define LINK_STATS              1
#define IP_STATS                1
#define ICMP_STATS              1
#define IGMP_STATS              1
#define IPFRAG_STATS            1
#define UDP_STATS               1
#define TCP_STATS               1
#define MEM_STATS               1
#define MEMP_STATS              1
#define PBUF_STATS              1
#define SYS_STATS               1
#endif /* LWIP_STATS */

/* ---------- NETBIOS options ---------- */
#define LWIP_NETBIOS_RESPOND_NAME_QUERY 1

/* ---------- PPP options ---------- */
// PPP is not enabled. 201015. Chang.
#define PPP_SUPPORT             0      /* Set > 0 for PPP */

#if PPP_SUPPORT

#define NUM_PPP                 1      /* Max PPP sessions. */


/* Select modules to enable.  Ideally these would be set in the makefile but
 * we're limited by the command line length so you need to modify the settings
 * in this file.
 */
#define PPPOE_SUPPORT           1
#define PPPOS_SUPPORT           1

#define PAP_SUPPORT             1      /* Set > 0 for PAP. */
#define CHAP_SUPPORT            1      /* Set > 0 for CHAP. */
#define MSCHAP_SUPPORT          0      /* Set > 0 for MSCHAP */
#define CBCP_SUPPORT            0      /* Set > 0 for CBCP (NOT FUNCTIONAL!) */
#define CCP_SUPPORT             0      /* Set > 0 for CCP */
#define VJ_SUPPORT              1      /* Set > 0 for VJ header compression. */
#define MD5_SUPPORT             1      /* Set > 0 for MD5 (see also CHAP) */

#endif /* PPP_SUPPORT */

/* The following defines must be done even in OPTTEST mode: */

#if !defined(NO_SYS) || !NO_SYS /* default is 0 */
void sys_check_core_locking(void);
#define LWIP_ASSERT_CORE_LOCKED()  sys_check_core_locking()
#endif

/*for dhcp server*/
#define CONFIG_LWIP_DHCPS_MAX_STATION_NUM	10
#define CONFIG_LWIP_DHCPS_LEASE_UNIT	1

/* In TCP Multi session PCB ADD/REM Func, 
 * ActivePCB lock/unlock , by Chang
 */
#undef TCP_MULTI_SESS_PCB_MUTEX

/* Memory config */
#define INTEGRATION_WITH_RTOS_HEAP

#ifdef INTEGRATION_WITH_RTOS_HEAP
#define MEM_LIBC_MALLOC 1
#define mem_clib_free   sys_mem_free
#define mem_clib_malloc sys_mem_malloc
#define mem_clib_calloc sys_mem_calloc
#endif /* INTEGRATION_WITH_RTOS_HEAP */

#ifndef LWIP_HOOK_FILENAME
#define LWIP_HOOK_FILENAME "rm_lwip_w_hooks.h"
#endif

#define LWIP_TIMEVAL_PRIVATE 0

size_t strnlen(const char *s, size_t maxlen);

#if CFG_PMGR && defined(__DISABLE_DPM_MOD_IN_SDK__)
//#define CFG_PMGR = 0
#endif /* CFG_PMGR AND __DISABLE_DPM_MOD_IN_SDK__ */

/* from inet_checksum.c */
#define LWIP_CHKSUM_ALGORITHM 2

#ifndef LWIP_TCPIP_CORE_LOCKING
#define LWIP_TCPIP_CORE_LOCKING     0
#endif /* LWIP_TCPIP_CORE_LOCKING */
#define LWIP_ARP                    LWIP_IPV4
#if LWIP_IPV4
#define LWIP_ETHERNET               LWIP_ARP
#endif /* LWIP_IPV4 */
#if LWIP_IPV6
#undef  LWIP_ETHERNET
#define LWIP_ETHERNET               1
#define ETH_HWADDR_LEN              6
#endif /* LWIP_IPV6 */
#define LWIP_DHCP_AUTOIP_COOP_TRIES 11
#define DNS_MAX_RETRIES             3
#define SLIPIF_THREAD_PRIO          OS_TASK_PRIORITY_NORMAL
#define LWIP_IPV6_NUM_ADDRESSES     2
#define LWIP_ND6_RDNSS_MAX_DNS_SERVERS  1

#if !defined(LWIP_DEBUG) && defined(LWIP_DEBUG_PRINTS)
#define LWIP_DEBUG
#endif /* LWIP_DEBUG_PRINTS */

#endif /* LWIP_LWIPOPTS_H */
