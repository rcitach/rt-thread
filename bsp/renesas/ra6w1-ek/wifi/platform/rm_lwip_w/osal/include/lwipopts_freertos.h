/*
 * Copyright (c) 2001, 2002 Swedish Institute of Computer Science.
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

#ifndef __LWIPOPTS_FREERTOS_H__
#define __LWIPOPTS_FREERTOS_H__

#include "osal.h"
#include "FreeRTOS.h"

/* ***************TCP Settings***************/
/* tcpip_thread should run on HIGH priority */
#define TCPIP_THREAD_NAME           "lwIP"
#define TCPIP_THREAD_STACKSIZE      ( 128 * 8 ) /* 1024 * sizeof(int) */
#define TCPIP_THREAD_PRIO           ( 21 )      /* Magic ? */

// #define FIXED_RETRANSMIT_TIMEOUT	/* Custom feature - Disabled by default */

#ifdef FIXED_RETRANSMIT_TIMEOUT
#define TCP_TMR_INTERVAL            100         /* The TCP timer interval in milliseconds */
#define TCP_RETRANSMISSION_TIMEOUT  1000
#define TCP_MSL                     6000UL
#endif
/*******************************************/

#define TCPIP_MBOX_SIZE              128 // 16
#define DEFAULT_RAW_RECVMBOX_SIZE    128 // 16
#define DEFAULT_UDP_RECVMBOX_SIZE    128 // 16
#define DEFAULT_TCP_RECVMBOX_SIZE    128 // 16
#define DEFAULT_ACCEPTMBOX_SIZE      128 // 16

#define DEFAULT_THREAD_STACKSIZE     1024
#define DEFAULT_THREAD_PRIO			 ( OS_TASK_PRIORITY_NORMAL + 2 )

/*fix http IOT issue */
#define LWIP_WND_SCALE                1
#define TCP_RCV_SCALE                 1
#define MEMP_NUM_NETDB                4

/*
 *-----------------------------
 * Socket options
 * ----------------------------
 */
#define LWIP_NETBUF_RECVINFO          1
#define LWIP_SOCKET                   1

/*fix reuse address issue */
#define SO_REUSE                      1
//#define LWIP_SO_SNDTIMEO              1
#define LWIP_SO_RCVTIMEO              1
/*for ip display */
#define LWIP_NETIF_STATUS_CALLBACK    1

#define ETH_PAD_SIZE                  0


/**
 * LWIP_EVENT_API and LWIP_CALLBACK_API: Only one of these should be set to 1.
 *     LWIP_EVENT_API==1: The user defines lwip_tcp_event() to receive all
 *         events (accept, sent, etc) that happen in the system.
 *     LWIP_CALLBACK_API==1: The PCB callback function is called directly
 *         for the event. This is the default.
 */
#define LWIP_EVENT_API       0
#define LWIP_CALLBACK_API    1

/*
 * ------------------------------------
 * ---------- Memory options ----------
 * ------------------------------------
 */

/**
 * MEM_SIZE: the size of the heap memory. If the application will send
 * a lot of data that needs to be copied, this should be set high.
 */
#ifdef INTEGRATION_WITH_RTOS_HEAP
#define MEM_SIZE	(20)				/* 50 KBytes -> 20 bytes INTEGRATION_WITH_RTOS_HEAP */
#else
#define MEM_SIZE	(50 * 1024)
#endif

/*
 * ------------------------------------------------
 * ---------- Internal Memory Pool Sizes ----------
 * ------------------------------------------------
 */

/* MEMP_NUM_UDP_PCB: the number of UDP protocol control blocks. One
 * per active UDP "connection". */
#define MEMP_NUM_UDP_PCB           16

/* MEMP_NUM_TCP_PCB: the number of simulatenously active TCP
 * connections. */
#define MEMP_NUM_TCP_PCB           32

/* MEMP_NUM_TCP_PCB_LISTEN: the number of listening TCP
 * connections. */
#define MEMP_NUM_TCP_PCB_LISTEN    32 /*16 original */

/* MEMP_NUM_TCP_SEG: the number of simultaneously queued TCP
 * segments. */
#define MEMP_NUM_TCP_SEG           255

/* MEMP_NUM_ARP_QUEUE: the number of simulateously queued outgoing
 * packets (pbufs) that are waiting for an ARP request (to resolve
 * their destination address) to finish. (requires the ARP_QUEUEING option) */
#define MEMP_NUM_ARP_QUEUE         8

/**
 * MEMP_NUM_NETCONN: the number of struct netconns.
 * (only needed if you use the sequential API, like api_lib.c)
 */
#define MEMP_NUM_NETCONN           32


/*
 * ----------------------------------
 * ---------- Pbuf options ----------
 * ----------------------------------
 */


/* PBUF_POOL_BUFSIZE: the size of each pbuf in the pbuf pool.
 * MT7687 packets have extra TXD header and packet offset. */
#define PBUF_POOL_BUFSIZE    ( 1536 + 128 ) // (2048  + 128)
/*
 * ---------------------------------
 * ---------- TCP options ----------
 * ---------------------------------
 */


/* TCP Maximum segment size. */
/* 1472 is possible, But RA6W1 is used as 1460 */
#define TCP_MSS        1460	//1476

/* TCP sender buffer space (bytes). */
//#define TCP_SND_BUF    ( 24 * 1024 )        /*(12 * 1024) */
//#define TCP_WND        ( 24 * 1024 )

/**  TCP sender buffer.
 * To achieve good performance, this should be at least 2 * TCP_MSS
 * For maximum throughput, set this to the same value as TCP_WND
 */
#define TCP_SND_BUF    (16 * TCP_MSS )        /*(12 * 1024) */

/** The size of a TCP window , 
 * If TCP_RCV_SCALE is defined, it's TCP Max window size in the TCP header.
 */
#define TCP_WND        (16 * TCP_MSS)	//( 24 * 1024 )

#if LWIP_IPV4    ///////////////////////////////////////

/*
 * ---------------------------------
 * ---------- ARP options ----------
 * ---------------------------------
 */


/**
 * ARP_QUEUEING==1: Multiple outgoing packets are queued during hardware address
 * resolution. By default, only the most recent packet is queued per IP address.
 * This is sufficient for most protocols and mainly reduces TCP connection
 * startup time. Set this to 1 if you know your application sends more than one
 * packet in a row to an IP address that is not in the ARP cache.
 */
#define ARP_QUEUEING    1


/*
 * ---------------------------------
 * ---------- DHCP options ---------
 * ---------------------------------
 */

#ifndef RM_LWIP_W
#if (CFG_WIFI == 1)
/* Define LWIP_DHCP to 1 if you want DHCP configuration of
 * interfaces. */
#define LWIP_DHCP    1

/* 1 if you want to do an ARP check on the offered address
 * (recommended). */

/**
 * DHCP_DOES_ARP_CHECK==1: Do an ARP check on the offered address.
 */
#define DHCP_DOES_ARP_CHECK    1


/*
 * ---------------------------------
 * ---------- DHCP Server options ---------
 * ---------------------------------
 */


/* Define LWIP_DHCP Server to 1 if you want DHCP Server configuration of
 * interfaces. */
#define LWIP_DHCPS    1

#endif /* CFG_WIFI */
#endif /* RM_LWIP_W */

/*
 * ----------------------------------
 * ---------- DNS options -----------
 * ----------------------------------
 */


#ifndef LWIP_DNS
 #define LWIP_DNS    1
#endif



/*
 * ------------------------------------------------
 * ---------- Network Interfaces options ----------
 * ------------------------------------------------
 */

/**
 * LWIP_NETIF_LOOPBACK==1: Support sending packets with a destination IP
 * address equal to the netif IP address, looping them back up the stack.
 */
#define LWIP_NETIF_LOOPBACK    1


/**
 * LWIP_LOOPBACK_MAX_PBUFS: Maximum number of pbufs on queue for loopback
 * sending for each netif (0 = disabled)
 */
#define LWIP_LOOPBACK_MAX_PBUFS    12

#endif // LWIP_IPV4        //////////////////////////


#if LWIP_IPV6    ///////////////////////////////////////

/*
 * ---------------------------------
 * ---------- DHCP options ---------
 * ---------------------------------
 */


/*
 * ---------------------------------
 * ---------- DHCP Server options ---------
 * ---------------------------------
 */


/*
 * ----------------------------------
 * ---------- DNS options -----------
 * ----------------------------------
 */

#ifndef LWIP_DNS
#define LWIP_DNS    1
#endif


/*
 * ------------------------------------------------
 * ---------- Network Interfaces options ----------
 * ------------------------------------------------
 */


/**
 * LWIP_NETIF_LOOPBACK==1: Support sending packets with a destination IP
 * address equal to the netif IP address, looping them back up the stack.
 */
//efine LWIP_NETIF_LOOPBACK    1


/**
 * LWIP_LOOPBACK_MAX_PBUFS: Maximum number of pbufs on queue for loopback
 * sending for each netif (0 = disabled)
 */
//efine LWIP_LOOPBACK_MAX_PBUFS    12

#endif // LWIP_IPV6        //////////////////////////

/** LWIP_NETCONN_SEM_PER_THREAD==1: Use one (thread-local) semaphore per
 * thread calling socket/netconn functions instead of allocating one
 * semaphore per netconn (and per select etc.)
 */
#define LWIP_NETCONN_SEM_PER_THREAD    1

/** LWIP_NETCONN_FULLDUPLEX==1: Enable code that allows reading from one thread,
 * writing from a 2nd thread and closing from a 3rd thread at the same time.
 * ATTENTION: Some requirements:
 * - LWIP_NETCONN_SEM_PER_THREAD==1 is required to use one socket/netconn from
 *   multiple threads at once
 * - sys_mbox_free() has to unblock receive tasks waiting on recvmbox/acceptmbox
 *   and prevent a task pending on this during/after deletion
 */
#define LWIP_NETCONN_FULLDUPLEX        1

#if CFG_MBEDTLS
#define LWIP_HAVE_MBEDTLS
#endif /* CFG_MBEDTLS */

#define LWIP_CHECKSUM_ON_COPY           1

/*
 * ------------------------------------
 * ----------- Debug options ----------
 * ------------------------------------
 */

#define LWIP_DBG_MIN_LEVEL		0

#define PPP_DEBUG			LWIP_DBG_OFF
#define MEM_DEBUG			LWIP_DBG_OFF
#define MEMP_DEBUG			LWIP_DBG_OFF
#define PBUF_DEBUG			LWIP_DBG_OFF
#define API_LIB_DEBUG		LWIP_DBG_OFF
#define API_MSG_DEBUG		LWIP_DBG_OFF
#define TCPIP_DEBUG			LWIP_DBG_OFF
#define NETIF_DEBUG			LWIP_DBG_OFF
#define SOCKETS_DEBUG		LWIP_DBG_OFF
#define DNS_DEBUG			LWIP_DBG_OFF
#define AUTOIP_DEBUG		LWIP_DBG_OFF
#define DHCP_DEBUG			LWIP_DBG_OFF
#define DHCPS_DEBUG			LWIP_DBG_OFF
#define IP_DEBUG			LWIP_DBG_OFF
#define IP_REASS_DEBUG		LWIP_DBG_OFF
#define ICMP_DEBUG			LWIP_DBG_OFF
#define IGMP_DEBUG			LWIP_DBG_OFF
#define UDP_DEBUG			LWIP_DBG_OFF
#define TCP_DEBUG			LWIP_DBG_OFF
#define TCP_INPUT_DEBUG		LWIP_DBG_OFF
#define TCP_OUTPUT_DEBUG	LWIP_DBG_OFF
#define TCP_RTO_DEBUG		LWIP_DBG_OFF
#define TCP_CWND_DEBUG		LWIP_DBG_OFF
#define TCP_WND_DEBUG		LWIP_DBG_OFF
#define TCP_FR_DEBUG		LWIP_DBG_OFF
#define TCP_QLEN_DEBUG		LWIP_DBG_OFF
#define TCP_RST_DEBUG		LWIP_DBG_OFF
#define HTTPC_DEBUG         LWIP_DBG_OFF
#define HTTPD_DEBUG         LWIP_DBG_OFF
#define ETHARP_DEBUG		LWIP_DBG_OFF
#define SNTP_DEBUG          LWIP_DBG_OFF
#define PING_DEBUG			LWIP_DBG_OFF

#define LWIP_DBG_TYPES_ON              (LWIP_DBG_ON | LWIP_DBG_TRACE | LWIP_DBG_STATE | LWIP_DBG_FRESH | LWIP_DBG_HALT)

#ifdef LWIP_TESTS_DEBUG
    #define LWIP_DEBUG_PRINTF printf
#else
    #define LWIP_DEBUG_PRINTF(...) (void)0
#endif

#ifdef HTTPS_TESTS_DEBUG
    #define HTTPS_DEBUG_PRINTF printf
#else
    #define HTTPS_DEBUG_PRINTF(...) (void)0
#endif

#ifdef LWIP_TESTCASE
 #if LWIP_IPV4 && LWIP_IPV6
  #define PING_RESULT(ping_ok)    do {                                                 \
        static char ip_str[64] = {0};                                                  \
        LWIP_DEBUG_PRINTF("Ping response details:\n");                                 \
        if (IP_IS_V4_VAL(*addr)) {                                                     \
            struct ip_hdr * iphdr = (struct ip_hdr *) ((char *) p->payload - 20);      \
            ip4_addr_t      src_ip4;                                                   \
            ip4_addr_copy(src_ip4, iphdr->src);                                        \
            strncpy(ip_str, ip4addr_ntoa(&src_ip4), sizeof(ip_str) - 1);               \
        }                                                                              \
        else if (IP_IS_V6_VAL(*addr)) {                                                \
            struct ip6_hdr * ip6hdr = (struct ip6_hdr *) ((char *) (p)->payload - 40); \
            ip6_addr_t       src_ip6;                                                  \
            ip6_addr_copy_to_packed(src_ip6, ip6hdr->src);                             \
            ip6addr_ntoa_r(&src_ip6, ip_str, sizeof(ip_str));                          \
        }                                                                              \
        LWIP_DEBUG_PRINTF("64 bytes from %s: icmp_seq=%d type=%d time=%lu ms\n",       \
               ip_str,                                                                 \
               ntohs(iecho->seqno),                                                    \
               iecho->type,                                                            \
               response_time);                                                         \
        if (ntohs(iecho->seqno) > 5)                                                   \
            sys_untimeout(ping_timeout, NULL);                                         \
} while(0)
 #elif LWIP_IPV4
  #define PING_RESULT(ping_ok)    do {                                                 \
        static char ip_str[64] = {0};                                                  \
        LWIP_DEBUG_PRINTF("Ping response details:\n");                                 \
        struct ip_hdr * iphdr = (struct ip_hdr *) ((char *) p->payload - 20);          \
        ip4_addr_t      src_ip4;                                                       \
        ip4_addr_copy(src_ip4, iphdr->src);                                            \
        strncpy(ip_str, ip4addr_ntoa(&src_ip4), sizeof(ip_str) - 1);                   \
        LWIP_DEBUG_PRINTF("64 bytes from %s: icmp_seq=%d type=%d time=%lu ms\n",       \
               ip_str,                                                                 \
               ntohs(iecho->seqno),                                                    \
               iecho->type,                                                            \
               response_time);                                                         \
        if (ntohs(iecho->seqno) > 5)                                                   \
            sys_untimeout(ping_timeout, pcb);                                          \
} while (0)
 #elif LWIP_IPV6
  #define PING_RESULT(ping_ok)    do {                                                 \
        static char ip_str[64] = {0};                                                  \
        LWIP_DEBUG_PRINTF("Ping response details:\n");                                 \
        struct ip6_hdr * ip6hdr = (struct ip6_hdr *) ((char *) (p)->payload - 40);     \
        ip6_addr_t       src_ip6;                                                      \
        ip6_addr_copy_to_packed(src_ip6, ip6hdr->src);                                 \
        ip6addr_ntoa_r(&src_ip6, ip_str, sizeof(ip_str));                              \
        LWIP_DEBUG_PRINTF("64 bytes from %s: icmp_seq=%d type=%d time=%lu ms\n",       \
               ip_str,                                                                 \
               ntohs(iecho->seqno),                                                    \
               iecho->type,                                                            \
               response_time);                                                         \
        if (ntohs(iecho->seqno) > 5)                                                   \
            sys_untimeout(ping_timeout, NULL);                                         \
} while (0)
 #endif
#endif

/*
 * Use the errno.h mechanism from FreeRTOS.
 */
#if ( configUSE_POSIX_ERRNO == 1 )

/**
 * @brief System error variable, errno.
 */
    extern int FreeRTOS_errno;
    #undef errno
    #define errno    FreeRTOS_errno
#endif

/*
 * STACK_TO_ZEROCOPY : Do not copy pbuf on the MAC 802.11 stack from TCP/IP stack
 */
#define STACK_TO_ZEROCOPY

#ifdef STACK_TO_ZEROCOPY
#define LWIP_NETIF_TX_SINGLE_PBUF       1
#define TXDESC_IN_PBUF
#if defined(TXDESC_IN_PBUF)
#define PBUF_LINK_ENCAPSULATION_HLEN	(104 + 220 + 68 + 64)	// include TX_DESC & TXL_BUFFER & tx_hw_desc
#else
#define PBUF_LINK_ENCAPSULATION_HLEN	172 // 32
#endif
#define PBUF_PAYLOAD_MARGIN_LEN			36
#else
#define PBUF_PAYLOAD_MARGIN_LEN			0
#endif

#define	MIN_RETAINED_HEAP_MEMORY	(30 * KBYTE)

#endif /* __LWIPOPTS_FREERTOS_H__ */
