/**
 * @file
 * lwIP iPerf server implementation
 */

/*
 * Copyright (c) 2014 Simon Goldschmidt
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
 * Author: Simon Goldschmidt
 *
 */
#ifndef LWIP_HDR_APPS_LWIPERF_H
#define LWIP_HDR_APPS_LWIPERF_H

#include "lwip/opt.h"
#include "lwip/ip_addr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LWIPERF_TCP_PORT_DEFAULT  5001
#define LWIPERF_UDP_PORT_DEFAULT  5001
#undef MULTI_THREAD_MULTI_SOCKET

/** lwIPerf test results */
enum lwiperf_report_type
{
  /** The server side test is done */
  LWIPERF_TCP_DONE_SERVER,
  /** The client side test is done */
  LWIPERF_TCP_DONE_CLIENT,
  /** Local error lead to test abort */
  LWIPERF_TCP_ABORTED_LOCAL,
  /** Data check error lead to test abort */
  LWIPERF_TCP_ABORTED_LOCAL_DATAERROR,
  /** Transmit error lead to test abort */
  LWIPERF_TCP_ABORTED_LOCAL_TXERROR,
  /** Remote side aborted the test */
  LWIPERF_TCP_ABORTED_REMOTE
};

/** Control */
enum lwiperf_client_type
{
  /** Unidirectional tx only test */
  LWIPERF_CLIENT,
  /** Do a bidirectional test simultaneously */
  LWIPERF_DUAL,
  /** Do a bidirectional test individually */
  LWIPERF_TRADEOFF
};

/** Prototype of a report function that is called when a session is finished.
    This report function can show the test results.
    @param report_type contains the test result */
typedef void (*lwiperf_report_fn)(void *arg, enum lwiperf_report_type report_type,
  const ip_addr_t* local_addr, u16_t local_port, const ip_addr_t* remote_addr, u16_t remote_port,
  u32_t bytes_transferred, u32_t ms_duration, u32_t bandwidth_kbitpsec);

void* lwiperf_start_tcp_server(const ip_addr_t* local_addr, u16_t local_port,
									lwiperf_report_fn report_fn, void* report_arg);
void* lwiperf_start_tcp_server_default(lwiperf_report_fn report_fn, void* report_arg);
void* lwiperf_start_tcp_client(const ip_addr_t* remote_addr, u16_t remote_port,
									enum lwiperf_client_type type,
									lwiperf_report_fn report_fn, void* report_arg);
void* lwiperf_start_tcp_client_default(const ip_addr_t* remote_addr,
									lwiperf_report_fn report_fn, void* report_arg);

//void  lwiperf_abort(void* lwiperf_session);
#ifndef MULTI_THREAD_MULTI_SOCKET
int api_mode_socket_server(void *pvParameters);
int api_mode_altcp_server(void *pvParameters);

int api_mode_tcp_client(void *pvParameters);
int api_mode_socket_client(void *pvParameters);
#endif
/* start */
#include "FreeRTOS.h"

/**************************************************************************/
/**************************************************************************/
/**                                                                       */
/** NetX Utility                                                          */
/**                                                                       */
/**   NetX/NetX Duo IPerf Test Program                                    */
/**                                                                       */
/**************************************************************************/
/**************************************************************************/

enum print_iperf_type {
	PRINT_TYPE_TCP_TX_INT,
	PRINT_TYPE_TCP_RX_INT,
	PRINT_TYPE_UDP_TX_INT,
	PRINT_TYPE_UDP_RX_INT,

	PRINT_TYPE_TCP_TX_TOL,
	PRINT_TYPE_TCP_RX_TOL,
	PRINT_TYPE_UDP_TX_TOL,
	PRINT_TYPE_UDP_RX_TOL
};


#define IPERF_VERSION				"0.50"

#define IPERF_TCP					0
#define IPERF_UDP					1
#define IPERF_TCP_N_UDP				2

#define IPERF_SERVER				0
#define IPERF_CLIENT				1
#define CLIENT_DEST_PORT			5001

#define IPERF_TCP_TX_MAX_PAIR		3
#define DEFAULT_IPV4_PACKET_SIZE	1470
#define DEFAULT_IPV6_PACKET_SIZE	1448

enum iperf_mode {
	IPERF_CLIENT_TCP,
	IPERF_CLIENT_UDP,
	IPERF_SERVER_TCP,
	IPERF_SERVER_UDP,
};

typedef struct IPERF_CONFIG
{
	UINT	ip_ver;
	ULONG	ipaddr;
	ULONG	ipv6addr[4];
	UINT	TestTime;
	UINT	PacketSize;
	UINT	Interval;
	UCHAR	term;
	UCHAR	WMM_Tos;
	UINT	sendnum;
	UINT	bandwidth;
	UCHAR	bandwidth_format;
	UCHAR	pair;
	UINT	port;
	UINT	network_pool;	/* 0: main pool, 1:iperf_pool */
	UINT	RxTimeOut;
	UINT	window_size;
	UINT	mib_flag;
	UINT	transmit_rate;
	UINT	tcp_api_mode;
} __iperf_config;

UINT  iperf_cli(UCHAR iface, UCHAR iperf_mode, struct IPERF_CONFIG *config);

/* start FOR_UDP */
typedef struct _lwiperf_state_base lwiperf_state_base_t;

struct _lwiperf_state_base {
    /* linked list */
    lwiperf_state_base_t *next;
    /* 1=tcp, 0=udp */
    u8_t tcp;
    /* 1=server, 0=client */
    u8_t server;
    /* master state used to abort sessions (e.g. listener, main client) */
    lwiperf_state_base_t *related_master_state;
};

/** This is the Iperf settings struct sent from the client */
typedef struct _lwiperf_settings {
//#define LWIPERF_FLAGS_ANSWER_TEST	0x80000000
//#define LWIPERF_FLAGS_ANSWER_NOW	0x00000001
	u32_t flags;
	u32_t num_threads; /* unused for now */
	u32_t remote_port;
	u32_t buffer_len; /* unused for now */
	u32_t win_band; /* TCP window / UDP rate: unused for now */
	u32_t amount; /* pos. value: bytes?; neg. values: time (unit is 10ms: 1/100 second) */
} lwiperf_settings_t;

/** Connection handle for a TCP iperf session */
typedef struct _lwiperf_state_tcp {
	lwiperf_state_base_t	base;
	struct altcp_pcb		*server_alpcb;
	struct altcp_pcb		*conn_alpcb;
	struct tcp_pcb			*server_pcb;
	struct tcp_pcb			*conn_pcb;
	u32_t                   time_started;   // msec.
	lwiperf_report_fn		report_fn;
	void					*report_arg;
	u8_t					poll_count;
	u8_t					next_num;
	/* 1 = start server when client is closed */
	u8_t					client_tradeoff_mode;
	u32_t					bytes_transferred;
	lwiperf_settings_t		settings;
	u8_t					have_settings_buf;
	u8_t					specific_remote;
	ip_addr_t				remote_addr;
	ip_addr_t				local_ip;
	u16_t					local_port;
	ip_addr_t				remote_ip;
	u16_t					remote_port;
	unsigned long			expire_time;    // Tick
} lwiperf_state_tcp_t;

/** Connection handle for a UDP iperf session */
typedef struct _lwiperf_state_udp {
	lwiperf_state_base_t	base;
	struct udp_pcb			*server_pcb;
	struct udp_pcb			*conn_pcb;
	u32_t					time_started;
	lwiperf_report_fn		report_fn;
	void					*report_arg;
	u8_t					poll_count;
	u8_t					next_num;
	/* 1 = start server when client is closed */
	u8_t					client_tradeoff_mode;
	u32_t					bytes_transferred;
	lwiperf_settings_t		settings;
	u8_t					have_settings_buf;
	u8_t					specific_remote;
	ip_addr_t				remote_addr;
} lwiperf_state_udp_t;

typedef struct
{
	ULONG	version;
	ULONG	ip;
	ULONG	ipv6[4];
	ULONG	port;
	UCHAR	iface;
	unsigned long long  PacketsTxed;
	unsigned long long  PacketsRxed;
	unsigned long long  BytesTxed;
	unsigned long long  BytesRxed;
	UINT	StartTime;  // Tick count
	UINT	RunTime;    // Tick count (unused)
	UINT	TestTime;   // Tick count (-t #)
	UINT	Interval;   // Tick count (-i #)
	UINT	PacketSize;
	ULONG	wmm_tos;
	UINT	send_num;
	UINT	bandwidth;  // for UDP Tx debug only
	UCHAR	bandwidth_format;
	UINT	RxTimeOut;
	UINT	window_size;
#ifdef __LIB_IPERF_PRINT_MIB__
	UINT	mib_flag;
#endif /* __LIB_IPERF_PRINT_MIB__ */
	UINT	transmit_rate;

	UINT	txCount;    // Interval count for Tx
	UINT	rxCount;    // Interval count for Rx
	UINT	Interval_StartTime; // Tick count
	UINT	Interval_BytesRxed;
	UINT	Interval_BytesTxed;

	ip_addr_t	local_ip;
	u16_t		local_port;
	ip_addr_t	remote_ip;
	u16_t		remote_port;

	lwiperf_report_fn	reportFunc;
	UINT	pair_no;
	lwiperf_state_tcp_t	lwiperf_state;
	int		socket_fd;
	UINT	tcpApiMode;
#ifdef MULTI_THREAD_MULTI_SOCKET
	UINT	taskNo;		/* Only for tcp server */
#endif  
} ctrl_info_t;

typedef struct
{
	int		udp_id;
	ULONG	tv_sec;
	ULONG	tv_usec;
} udp_payload;
/* end FOR_UDP */

/* end */

#ifdef __cplusplus
}
#endif

/* By Throughput, API Mode is set as default, other is option */
#define TCP_API_MODE_TCP		0
#define TCP_API_MODE_ALTCP		1
#define TCP_API_MODE_SOCKET		2	

#define UDP_API_MODE_SOCKET		0
#define UDP_API_MODE_NETCONN		1
#define UDP_API_MODE_RAW		2

#define TCP_API_MODE_MAX		3

#define TCP_API_MODE_DEFAULT	0

#endif /* LWIP_HDR_APPS_LWIPERF_H */
