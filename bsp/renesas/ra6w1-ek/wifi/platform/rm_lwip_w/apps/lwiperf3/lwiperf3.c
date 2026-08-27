/**
 * @file
 * lwIP iPerf3 server implementation
 */

/**
 * @ingroup apps
 *
 * This is a simple performance measuring client/server to check your bandwith using
 * iPerf2 on a PC as server/client.
 * It is currently a minimal implementation providing a TCP client/server only.
 *
 * @todo:
 * - implement UDP mode
 * - protect combined sessions handling (via 'related_master_state') against reallocation
 *   (this is a pointer address, currently, so if the same memory is allocated again,
 *    session pairs (tx/rx) can be confused on reallocation)
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
 *  Copyright (c) 2023 Modified by Renesas Electroincs.
 */

#include "FreeRTOS.h"
#include "lwipopts.h"

#if defined ( __SUPPORT_IPERF3__ )

#include "lwip/apps/lwiperf3.h"

#include "lwip/altcp.h"
#include "lwip/priv/altcp_priv.h"
#include "lwip/tcp.h"

#include "lwip/udp.h"
#include "lwip/sockets.h"

#include "lwip/sys.h"

#include "lwip/mem.h"		/* F_F_S */
#include "lwip/ip_addr.h"

#include <string.h>
#include "common_def.h"
#include "supp_def.h"

#include "lwip/priv/tcp_priv.h"

#if defined (__ENABLE_TRANSFER_MNG_TEST_MODE__)
#include "atcmd_transfer_mng.h"
#endif // __ENABLE_TRANSFER_MNG_TEST_MODE__

#include "rm_lwip_w_helper.h"

#define IPERF3_DEBUG 0

#pragma GCC diagnostic ignored "-Wunused-but-set-variable"

#undef	__IPERF3_API_MODE_ALTCP__
#undef	__IPERF3_API_MODE_SOCKET__

#define LWIPERF_FLAGS_ANSWER_TEST	0x80000000
#define LWIPERF_FLAGS_ANSWER_NOW	0x00000001

#define IPERF_TCP_CLIENT_PRIORITY	OS_TASK_PRIORITY_NORMAL+3	//4		//case of 20 has many assertion errors.
#define IPERF3_TCP_SERVER_PRIORITY	OS_TASK_PRIORITY_NORMAL+4	//5
#define IPERF_UDP_CLIENT_PRIORITY	OS_TASK_PRIORITY_NORMAL+5	//6		//20, over 21 --> lmac working unstable
#define IPERF_UDP_SERVER_PRIORITY	OS_TASK_PRIORITY_NORMAL+5	//6


#define IPERF3_MSG printf

#define USE_TCP_SERVER_LISTEN
#define TCP_SERVER_LOOP
int iperf3_server_reverse = 0;

#define IPERF3_WDOG_OFF

#if defined(IPERF3_WDOG_OFF)
int iperf3_wdog_off_cnt = 0;

void lwiperf3_set_watchdog_freeze()
{
    iperf3_wdog_off_cnt++;

    // Stop HW watchdog
    hw_watchdog_freeze();
}

void lwiperf3_set_watchdog_unfreeze()
{
    if (iperf3_wdog_off_cnt == 1) {
        // Start HW Watchdog
        hw_watchdog_unfreeze();
        iperf3_wdog_off_cnt = 0;
    } else {
        iperf3_wdog_off_cnt--;
    }
}
#endif // IPERF3_WDOG_OFF

SemaphoreHandle_t	iperf3_data = NULL;
#ifdef __IPERF_API_MODE_ALTCP__
static void
altcp_iperf_remove_callbacks(struct altcp_pcb *inner_conn)
{

	IPERF_MSG("[%s] called.\n", __func__);

	altcp_arg(inner_conn, NULL);
	altcp_recv(inner_conn, NULL);
	altcp_sent(inner_conn, NULL);
	altcp_err(inner_conn, NULL);
	altcp_poll(inner_conn, NULL, inner_conn->pollinterval);
}

static err_t
altcp_iperf_close(struct altcp_pcb *conn)
{
	struct tcp_pcb *lpcb;

	//IPERF3_MSG("[%s] called\n", __func__);

	lpcb = conn->state;
	if (lpcb) {
		//IPERF3_MSG("[%s] close\n", __func__);
		tcp_close(lpcb);
	}
	altcp_free(conn);

	return ERR_OK;
}

const struct altcp_functions altcp_iperf_functions = {
	altcp_default_set_poll,
	altcp_default_recved,
	altcp_default_bind,
	NULL,						// altcp_default_connect
	NULL,						// altcp_default_listen
	NULL,						// altcp_default_abort
	altcp_iperf_close,			// altcp_default_close
	altcp_default_shutdown,
	altcp_default_write,
	altcp_default_output,
	altcp_default_mss,
	altcp_default_sndbuf,
	altcp_default_sndqueuelen,
	altcp_default_nagle_disable,
	altcp_default_nagle_enable,
	altcp_default_nagle_disabled,
	altcp_default_setprio,
	altcp_default_dealloc,
	altcp_default_get_tcp_addrinfo,
	altcp_default_get_ip,
	altcp_default_get_port
#ifdef LWIP_DEBUG
	, altcp_default_dbg_get_tcp_state
#endif
};
#endif // __IPERF3_API_MODE_ALTCP__

#define IPERF_DATA				"1234567890"
#define IPERF_DATA_LEN			10 //(sizeof(IPERF_DATA)-1)

#define	IPERF_TCP_TX_MIN_SIZE	50

#undef	__DEBUG_IPERF3__			/* for debugging log */
#undef	__DEBUG_BANDWIDTH__
#define PREVIOUS_PACKET_DELAY_COMPENSATION	// for bandwidth

#define CLK2US(clk)		((((long long ) (clk)) * 15625LL) >> 9LL)
#define US2CLK(us)		((((long long ) (us)) << 9LL) / 15625LL)

#define IPERF_TCP_N_UDP					2
#define IPERF_TCP_TX_MAX_PAIR			3
#define IPERF_UDP_TX_MAX_PAIR			3

#define MSG_UDP_RX					"[UDP] Rx Test (Server)"
#define MSG_TCP_RX					"[TCP] Rx Test (Server)"

#define MSG_UDP_TX					"\n\n[UDP] Tx Test (Client) ==> %s%s%s:%d\n"
#define MSG_TCP_TX					"\n\n[TCP] Tx Test (Client) ==> %s%s%s:%d\n"

#define MSG_HEAD_UDP_RX				"UDP_RX:"
#define MSG_HEAD_UDP_TX				"UDP_TX:"
#define MSG_HEAD_TCP_RX				"TCP_RX:"
#define MSG_HEAD_TCP_TX				"TCP_TX:"

#define MSG_UDP_SERVER_STOP_USER	"[iPerf3 UDP Server] Stoped\n"
#define MSG_TCP_SERVER_STOP_USER	"[iPerf3 TCP Server] Stoped\n"
#define MSG_UDP_CLIENT_STOP_USER	"[iPerf3 UDP Client] Stoped\n"
#define MSG_TCP_CLIENT_STOP_USER	"[iPerf3 TCP Client] Stoped\n"

#define MSG_TITLE_INTERVAL			"[ No ]      [Interval]   [Transfer]        [Bandwidth]       %20s"
#define MSG_TITLE_TOTAL				"%s_%cX:[ No ]      [Interval]   [Transfer]        [Bandwidth]\n"

#define KBYTE_INT(x)				(int)(x / 1024ULL)
#define KBYTE_FLOAT(x)				(int)(((float)((float)x / 1024.0) - (float)KBYTE_INT(x))*1000)

#define MBYTE_INT(x)				(int)(x / (1024ULL * 1024ULL))
#define MBYTE_FLOAT(x)				(int)((float)(((float)(x / 1024) / 1024.0) - (float)MBYTE_INT(x))*1000)

#define GBYTE_INT(x)				(int)(x / (1024ULL * 1024ULL * 1024ULL))
#define GBYTE_FLOAT(x)				(int)((float)(((float)(x / (1024ULL * 1024ULL)) / 1024.0) - (float)GBYTE_INT(x))*1000)

#define TBYTE_INT(x)				(int)(x / (1024ULL * 1024ULL * 1024ULL * 1024ULL))
#define TBYTE_FLOAT(x)				(int)((float)(((float)(x / (1024ULL * 1024ULL * 1024ULL)) / 1024.0) - (float)TBYTE_INT(x))*1000)

#define KBPS_INT(x)					(x / 1000)
#define KBPS_FLOAT(x)				(x % 1000)
#define MBPS_INT(x)					(x / (1000 * 1000))
#define MBPS_FLOAT(x)				(x / 1000 % 1000)

#define PACKET_SIZE_MAX				1470
#define DEFAULT_IPV4_PACKET_SIZE	1470

#define IPERF_RX_TIMEOUT_DEFAULT 	300		/* 3 sec */

#define IP_VERSION_V4				0x4
#define IP_VERSION_V6				0x6

/* Currently, only TCP is implemented */
#if LWIP_TCP && LWIP_CALLBACK_API

/** Specify the idle timeout (in seconds) after that the test fails */
#ifndef LWIPERF_TCP_MAX_IDLE_SEC
/* TX Overworking, TCP TX Disconnection is disconnected openly , so recover it 201125 */
#define LWIPERF_TCP_MAX_IDLE_SEC	60U		/* 10->60 20200902 */
#define LWIPERF_TCP_RX_MAX_IDLE_SEC	10		/* polling time 2s, so wait until 10sec, after that disconnection */
#endif
#if LWIPERF_TCP_MAX_IDLE_SEC > 255
#error LWIPERF_TCP_MAX_IDLE_SEC must fit into an u8_t
#endif

/** Change this if you don't want to lwiPerf3 to listen to any IP version */
#ifndef LWIPERF_SERVER_IP_TYPE
#define LWIPERF_SERVER_IP_TYPE		IPADDR_TYPE_ANY
#endif

/* File internal memory allocation (struct lwiperf_*): this defaults to the heap */

/** If this is 1, check that received data has the correct format */
#ifndef LWIPERF_CHECK_RX_DATA
#define LWIPERF_CHECK_RX_DATA		0
#endif

/* defined as local variable for multiple session processing */
//ctrl_info_t	*iperf_ctrlInfo_ptr;

ctrl3_info_t	*iperf3_TCP_TX_ctrl_info[IPERF_TCP_TX_MAX_PAIR] = {NULL,};
ctrl3_info_t	*iperf3_TCP_RX_ctrl_info = NULL;
ctrl3_info_t	*iperf3_UDP_TX_ctrl_info[IPERF_UDP_TX_MAX_PAIR] = {NULL,};
ctrl3_info_t	*iperf3_UDP_RX_ctrl_info = NULL;

UCHAR IP3_UDP_Tx_Finish_Flag = 1;
UCHAR IP3_UDP_Rx_Finish_Flag = 1;
UCHAR IP3_TCP_Rx_Finish_Flag = 1;
UCHAR IP3_TCP_Tx_Finish_Flag = 1;

UCHAR IP3_TCP_Rx_Restart_Flag = 0;

// TX thread's structure to manage state
typedef enum _iperf_tx_thd_state {
	IPERF_TX_THD_STATE_NONE = 0,
	IPERF_TX_THD_STATE_ACTIVE = 1,
	IPERF_TX_THD_STATE_REQ_TERM = 2,
} iperf_tx_thd_state;

typedef struct _iperf_tx_thd {
	TaskHandle_t handler;
	iperf_tx_thd_state state;
} iperf_tx_thd;

/* TCP Task */
iperf_tx_thd	thread_tcp_tx_iperf3[IPERF_TCP_TX_MAX_PAIR];
TaskHandle_t	*thread_tcp_rx_iperf3;

/* UDP Task */
iperf_tx_thd	thread_udp_tx_iperf3[IPERF_UDP_TX_MAX_PAIR];
TaskHandle_t	*thread_udp_rx_iperf3;

TaskHandle_t iperf3_tcp_rx_handler = NULL;
TaskHandle_t iperf3_udp_rx_handler = NULL;
#ifdef MULTI_THREAD_MULTI_SOCKET
#define MAX_TCP_RX_TASK		4
TaskHandle_t iperf_tcp_rx_task_handler[MAX_TCP_RX_TASK];
#endif // MULTI_THREAD_MULTI_SOCKET
//int	tcp_rx_sock;
int	ip3_tcp_tx_sock = -1;
int	ip3_udp_rx_sock = -1;
int	ip3_udp_tx_sock = -1;

/** Basic connection handle */
//struct _lwiperf_state_base;
//typedef struct _lwiperf_state_base lwiperf3_state_base_t;

u8	*ip3_udp_rx_buffer = NULL;

/** List of active iPerf3 sessions */
#ifdef USING_LIST_DELETE
static lwiperf3_state_base_t *lwiperf3_all_connections = NULL;
#endif /*USING_LIST_DELETE*/
/** A const buffer to send from: we want to measure sending, not copying! */
static const u8_t lwiperf_txbuf_const[1600] = {
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
};

#ifdef __IPERF3_API_MODE_ALTCP__
static err_t lwiperf_altcp_poll(void *arg, struct altcp_pcb *tpcb);
static err_t lwiperf_altcp_rx_poll(void *arg, struct altcp_pcb *tpcb);
#endif // __IPERF3_API_MODE_ALTCP__

#if defined (__SUPPORT_EEMBC__)
extern void th_printf(const char *p_fmt, ...);
extern void th_iperf_cb(void);
#endif // __SUPPORT_EEMBC__

static err_t lwiperf_tcp_poll(void *arg, struct tcp_pcb *tpcb);
static err_t lwiperf_tcp_rx_poll(void *arg, struct tcp_pcb *tpcb);
static void lwiperf_tcp_err(void *arg, err_t err);

static err_t iperf3_server_send_data(void *arg, struct tcp_pcb *tpcb, u32_t state);

#ifdef __LIB_IPERF_PRINT_MIB__
extern void cmd_lmac_rx_mib(int argc, char *argv[]);
extern void cmd_lmac_status(int argc, char *argv[]);
extern void lwip_mem_status();
extern void heapInfo(void);

void iperf_print_mib(UINT flag)
{
	char *option[2]={"mib","r"} ;

	if (flag == 1) {
		option[1]="r";
	} else if (flag == 2) {
		option[1]="h";
	} else {
		option[1]="r";
	}

 //   lwip_mem_status();
	heapInfo();
	cmd_lmac_rx_mib(2, option);
	cmd_lmac_status(0, NULL);
	IPERF_MSG("\n\n");
}
#endif /* __LIB_IPERF_PRINT_MIB__ */

extern char *ra6wx_inet_ntoa(struct in_addr address_to_convert);
extern size_t os_strlcpy(char *dest, const char *src, size_t siz);

#if defined (__ENABLE_TRANSFER_MNG_TEST_MODE__)
extern unsigned char g_atcmd_tr_mng_test_flag;
#endif // __ENABLE_TRANSFER_MNG_TEST_MODE__

static unsigned int terminate_iperf_tx_thd(iperf_tx_thd *tx_thd)
{
	const int wait_time = 10;
	const int max_retry_cnt = 100;
	int retry_cnt = 0;

	if (tx_thd->state != IPERF_TX_THD_STATE_NONE) {
		tx_thd->state = IPERF_TX_THD_STATE_REQ_TERM;

		do {
			vTaskDelay(wait_time);
			retry_cnt++;
		} while ((tx_thd->state != IPERF_TX_THD_STATE_NONE) && (retry_cnt < max_retry_cnt));

		if (tx_thd->state != IPERF_TX_THD_STATE_NONE) {
			return pdFALSE;
		}
	}

	return pdTRUE;
}

void print_iperf3(UINT Num,
	UINT64	TransfBytes,
	UINT	StartTime,
	UINT	curTime,
	UINT	intStartTime,
	UINT	ipversion,
	ip_addr_t	*remote_ip,
	u16_t	remote_port,
	UCHAR	bandwidth_format,
	UCHAR	printType)
{
	UINT TotalRunTime  = 0;
	UINT IntvalRunTime = 0;
	UINT ThroughPut    = 0;

#ifdef __DEBUG_IPERF3__
	IPERF3_MSG("[%s] N:%d Bytes:0x%x ST:%d CT:%d IST:%d VER:%d IP:%s CP:%d BF:%c PT:%d\n",
				__func__,
				Num,
				TransfBytes,
				StartTime,
				curTime,
				intStartTime,
				ipversion,
				ipaddr_ntoa(remote_ip),
				remote_port,
				bandwidth_format,
				printType );
#endif // __DEBUG_IPERF3__

	if (StartTime == 0) /* If there is no start time, do not print. */
	{
		return;
	}

	if (TransfBytes < 10)
		return;

	TotalRunTime  = curTime - StartTime;
	IntvalRunTime = curTime - intStartTime;
#if IPERF3_DEBUG
	printf("\ncurTime = %d TotalRunTime : %d, IntvalRunTime:%d TransfBytes:%d configTICK_RATE_HZ: %f\n",curTime, TotalRunTime, IntvalRunTime, TransfBytes, configTICK_RATE_HZ);
#endif
	if (PRINT_TYPE_TCP_TX_TOL > printType) {
		/* Interval */
		/* Bps */
		ThroughPut = (UINT)((unsigned long long)TransfBytes * 8ULL / (unsigned long long)IntvalRunTime * (unsigned long long)configTICK_RATE_HZ);
#if IPERF3_DEBUG
	printf("\nTX_TOL ThroughPut: %d\n", ThroughPut);
#endif
	}
	else {
		/* Total */
		if (Num == 0) {
			IPERF3_MSG(MSG_TITLE_TOTAL,
				(PRINT_TYPE_TCP_RX_TOL == printType || PRINT_TYPE_TCP_TX_TOL == printType) ? "TCP" : "UDP",
				(PRINT_TYPE_TCP_TX_TOL == printType || PRINT_TYPE_UDP_TX_TOL == printType ) ? 'T' : 'R');
		}

		/* Bps */
		ThroughPut = (UINT)((unsigned long long)TransfBytes * 8ULL / (unsigned long long)TotalRunTime * (unsigned long long)configTICK_RATE_HZ);
#if IPERF3_DEBUG
	printf("\nTOTAL ThroughPut: %d\n", ThroughPut);
#endif
	}
	switch(printType)
	{
		case PRINT_TYPE_TCP_TX_TOL:
			IPERF3_MSG(ANSI_BOLD);
			IPERF3_MSG(ANSI_BCOLOR_GREEN ANSI_COLOR_RED MSG_HEAD_TCP_TX);
			break;

		case PRINT_TYPE_TCP_TX_INT:
			IPERF3_MSG(ANSI_BCOLOR_GREEN ANSI_COLOR_RED MSG_HEAD_TCP_TX);
			break;

		case PRINT_TYPE_TCP_RX_TOL:
			IPERF3_MSG(ANSI_BOLD);
			IPERF3_MSG(ANSI_BCOLOR_GREEN ANSI_COLOR_BLUE MSG_HEAD_TCP_RX);
			break;

		case PRINT_TYPE_TCP_RX_INT:
			IPERF3_MSG(ANSI_BCOLOR_GREEN ANSI_COLOR_BLUE MSG_HEAD_TCP_RX);
			break;

		case PRINT_TYPE_UDP_TX_TOL:
			IPERF3_MSG(ANSI_BOLD);
			IPERF3_MSG(ANSI_BCOLOR_YELLOW ANSI_COLOR_RED MSG_HEAD_UDP_TX);
			break;

		case PRINT_TYPE_UDP_TX_INT:
			IPERF3_MSG(ANSI_BCOLOR_YELLOW ANSI_COLOR_RED MSG_HEAD_UDP_TX);
			break;

		case PRINT_TYPE_UDP_RX_TOL:
			IPERF3_MSG(ANSI_BOLD);

			IPERF3_MSG(ANSI_BCOLOR_YELLOW ANSI_COLOR_BLUE MSG_HEAD_UDP_RX);
			break;

		case PRINT_TYPE_UDP_RX_INT:
			IPERF3_MSG(ANSI_BCOLOR_YELLOW ANSI_COLOR_BLUE MSG_HEAD_UDP_RX);
			break;
	}

	/*[0000] [00.00-99.00] */
	if (PRINT_TYPE_TCP_TX_TOL > printType) {
		/* Interval */
		IPERF3_MSG("[%04d]   %4d.%02d-%2d.%02d", Num,									/* Index */
			(Num > 1 ? ((TotalRunTime - IntvalRunTime) / configTICK_RATE_HZ): 0),	/* Interval Start Time */
			(TotalRunTime - IntvalRunTime) % configTICK_RATE_HZ,					/* float */

			TotalRunTime / configTICK_RATE_HZ,										/* Interval End Time */
			TotalRunTime % configTICK_RATE_HZ);										/* float */

	}
	else {
		/* Total */
		IPERF3_MSG(ANSI_BOLD);
		IPERF3_MSG("[Total]     0.00-%2d.%02d", TotalRunTime / configTICK_RATE_HZ, TotalRunTime % configTICK_RATE_HZ);
	}

	/* Transfer (Bytes) */
	if (TransfBytes >= 1024ULL*1024ULL*1024ULL*1024ULL) {
		/* Tbytes */
		IPERF3_MSG("%5d.%03d T",
			TBYTE_INT(TransfBytes),
			TBYTE_FLOAT(TransfBytes));
	} else if (TransfBytes >= 1024ULL*1024ULL*1024ULL) {
		/* Gbytes */
		IPERF3_MSG("%5d.%03d G",
			GBYTE_INT(TransfBytes),
			GBYTE_FLOAT(TransfBytes));
	} else if (TransfBytes >= 1024ULL*1024ULL) {
		/* Mbytes */
		IPERF3_MSG("%5d.%03d M",
			MBYTE_INT(TransfBytes),
			MBYTE_FLOAT(TransfBytes));
	} else if (TransfBytes >= 1024ULL) {
		/* Kbytes */
		IPERF3_MSG("%5d.%03d K",
			KBYTE_INT(TransfBytes),
			KBYTE_FLOAT(TransfBytes));
	} else {
		/* Bytes */
		IPERF3_MSG("%5d.000 ", (unsigned int )TransfBytes);
	}

	IPERF3_MSG("Bytes    %s", TransfBytes < 1024ULL ? " ":"");

	/* Bandwidth (bits) */
	if ((bandwidth_format == 'A' && ThroughPut >= 1000*1000) || bandwidth_format == 'm') {
		/* Mbps */
		IPERF3_MSG("%3d.%03d Mbits/sec", MBPS_INT(ThroughPut), MBPS_FLOAT(ThroughPut));
	} else if ((bandwidth_format == 'A' && ThroughPut >= 1000) || bandwidth_format == 'k') {
		/* Kbps */
		IPERF3_MSG("%3d.%03d Kbits/sec", KBPS_INT(ThroughPut), KBPS_FLOAT(ThroughPut));
	} else if (bandwidth_format == 'A') { /* Bit */
		/* bps */
		IPERF3_MSG("%3d%s Bits/sec%s", ThroughPut, ThroughPut == 0 ? ".000":"", ThroughPut < 1000 ? " ":"");
		//IPERF3_MSG("%3d.000  Bits/sec", ThroughPut);
	} else if (bandwidth_format == 'M') {
		/* MBytes/Sec */
		IPERF3_MSG("%3d.%03d MBytes/sec", MBYTE_INT(ThroughPut), MBYTE_FLOAT(ThroughPut));
	} else if (bandwidth_format == 'K') { /*Kbytes */
		/* KBytes/Sec */
		IPERF3_MSG("%3d.%03d KBytes/sec", KBYTE_INT(ThroughPut), KBYTE_FLOAT(ThroughPut));
	}

	/*
	IPv6 [2605:2700:0:3::4713:93e3]:5001
	IPv4 172.16.3.1:5001
	*/
	IPERF3_MSG(" %s%15s%s:%-5d",
		ipversion==IP_VERSION_V6 ? "[":"",
		ipaddr_ntoa(remote_ip),
		ipversion==IP_VERSION_V6 ? "]":"",
		remote_port);

	IPERF3_MSG(ANSI_BCOLOR_BLACK ANSI_COLOR_WHITE ANSI_NORMAL "\n");
#ifdef __LIB_IPERF_PRINT_MIB__
	if((bandwidth_format == 'A' && (ThroughPut < 1000*1000 && ThroughPut > 0))) {
		iperf_print_mib(1);
	}
#endif // __LIB_IPERF_PRINT_MIB__
	if (printType >= PRINT_TYPE_TCP_TX_TOL) {
		IPERF3_MSG("\n");
	}
}
#ifdef __ENABLE_UNUSED__
static unsigned long sendElapsedTme(unsigned long long startTime, unsigned long long stopTime)
{
	return (unsigned long)(CLK2US(stopTime - startTime));
}
#endif //__ENABLE_UNUSED__

/* return usec */
static unsigned long targetTimePerPacket (unsigned int bandwidth, unsigned int packet_len)
{
	unsigned long UsecPerPacket = 0;
	unsigned long bitPerUsec = 0;

	bitPerUsec = bandwidth/1000/1000;

	UsecPerPacket = (packet_len*8) / bitPerUsec;

#ifdef __DEBUG_BANDWIDTH__
	IPERF3_MSG("bitPerUsec = %u bit\nUsecPerPacket = %u usec\n", bitPerUsec, UsecPerPacket);
#endif /* __DEBUG_BANDWIDTH__ */

	return UsecPerPacket;
}

#ifdef USING_LIST_DELETE
/** Add an iPerf3 session to the 'active' list */
static void
lwiperf_list_add(lwiperf3_state_base_t *item)
{
	if(item->conn_pcb.remote_port == 0) {
		return;
	}

	lwiperf3_state_base_t* item_tmp = (lwiperf3_state_base_t*)pvPortMalloc(sizeof(lwiperf3_state_base_t));

	if(item_tmp == NULL) {
		printf("Memory allocation failed.\n");
		return;
	}
	memcpy(item_tmp, item, sizeof(lwiperf3_state_base_t));
	item_tmp->state = 0;
	memset(item_tmp->data, 0 ,37);
	item_tmp->next = NULL;
#if IPERF3_DEBUG
	printf("\n%s: %d port:%d, state: %d",__func__,__LINE__, item->conn_pcb.remote_port, item->state);
#endif
	if (lwiperf3_all_connections == NULL) {
		lwiperf3_all_connections = item_tmp;
	} else {
		lwiperf3_state_base_t *iter;
		for (iter = lwiperf3_all_connections; iter != NULL; iter = iter->next) {
			if(iter->next == NULL) {
				iter->next = item_tmp;
				break;
			}
		}
	}
}

/** Remove an iperf session from the 'active' list */
static void
lwiperf3_list_remove(struct tcp_pcb *pcb)
{
	lwiperf3_state_base_t *prev = NULL;
	lwiperf3_state_base_t *iter;
	for (iter = lwiperf3_all_connections; iter != NULL; prev = iter, iter = iter->next) {
		if (iter->conn_pcb.remote_port == pcb->remote_port) {
#if IPERF3_DEBUG
		printf("\n found and delete:%d", iter->conn_pcb.remote_port);
#endif
			if (prev == NULL) {
				lwiperf3_all_connections = iter->next;
			} else {
				prev->next = iter->next;
			}
			vPortFree(iter);
			break;
		}
	}
}

/** Remove an iPerf3 session from the 'active' list */
static void
lwiperf_list_remove(lwiperf3_state_base_t *item)
{
	lwiperf3_state_base_t *prev = NULL;
	lwiperf3_state_base_t *iter;
	for (iter = lwiperf3_all_connections; iter != NULL; prev = iter, iter = iter->next) {
		if (iter == item) {
			if (prev == NULL) {
				lwiperf3_all_connections = iter->next;
			} else {
				prev->next = iter->next;
			}
			vPortFree(iter);
			/* @debug: ensure this item is listed only once */
			for (iter = iter->next; iter != NULL; iter = iter->next) {
				LWIP_ASSERT("duplicate entry", iter != item);
			}
			break;
		}
	}
}

static lwiperf3_state_base_t *
lwiperf_list_find(lwiperf3_state_base_t *item)
{
	lwiperf3_state_base_t *iter;
	for (iter = lwiperf3_all_connections; iter != NULL; iter = iter->next) {
		if (iter == item) {
			return item;
		}
	}
	return NULL;
}
#endif // USING_LIST_DELETE

/** Call the report function of an iPerf3 tcp session */
static void
lwip_tcp_conn_report(ctrl3_info_t *ctrlInfo_ptr, enum lwiperf_report_type report_type)
{
	lwiperf3_state_tcp_t *conn;

	conn = &ctrlInfo_ptr->lwiperf_state;

	if ((conn != NULL) && (conn->report_fn != NULL)&&(conn->conn_pcb != NULL)) {
		u32_t now, duration_ms, bandwidth_kbitpsec;
		now = sys_now();
		duration_ms = now - conn->time_started;
		if (duration_ms == 0) {
			bandwidth_kbitpsec = 0;
		}
		else {
			bandwidth_kbitpsec = (conn->bytes_transferred / duration_ms) * 8U;
		}

		if (ctrlInfo_ptr->tcpApiMode == TCP_API_MODE_ALTCP) {
			conn->report_fn(conn->report_arg, report_type,
					&ctrlInfo_ptr->local_ip, ctrlInfo_ptr->local_port,
					&ctrlInfo_ptr->remote_ip, ctrlInfo_ptr->remote_port,
					conn->bytes_transferred, duration_ms, bandwidth_kbitpsec);
		}
		else if (ctrlInfo_ptr->tcpApiMode == TCP_API_MODE_TCP) {
			conn->report_fn(conn->report_arg, report_type,
					&conn->conn_pcb->local_ip, conn->conn_pcb->local_port,
					&conn->conn_pcb->remote_ip, conn->conn_pcb->remote_port,
					conn->bytes_transferred, duration_ms, bandwidth_kbitpsec);
		}
		else {
			IPERF3_MSG(RED_COLOR "[%s:%d] Unknown mode: %d\n" CLEAR_COLOR,
							__func__, __LINE__, ctrlInfo_ptr->tcpApiMode);
		}
	}
}

/** Only iPerf3 tcp result report */
static void lwiperf_tcp_result_report(ctrl3_info_t *ctrlInfo_ptr)
{
	ULONG				current_time;
	lwiperf3_state_tcp_t *conn;

	conn = &ctrlInfo_ptr->lwiperf_state;

#ifdef __DEBUG_IPERF3__
	IPERF3_MSG(CYAN_COLOR "[%s] Closed by abort(ABORTED_REMOTE)\n" CLEAR_COLOR, __func__);
#endif // __DEBUG_IPERF3__
	current_time = xTaskGetTickCount();

	if (!conn->base.server) {
		/* TCP Tx Total */
		print_iperf3(ctrlInfo_ptr->txCount,
				ctrlInfo_ptr->BytesTxed,
				ctrlInfo_ptr->StartTime,
				current_time,
				ctrlInfo_ptr->StartTime,
				ctrlInfo_ptr->version,
				&ctrlInfo_ptr->remote_ip,
				ctrlInfo_ptr->remote_port,
				ctrlInfo_ptr->bandwidth_format,
				PRINT_TYPE_TCP_TX_TOL);

		IP3_TCP_Tx_Finish_Flag = 1;
	}
}

/** Close an iPerf3 tcp session */
static void
lwiperf_tcp_close(ctrl3_info_t *ctrlInfo_ptr, enum lwiperf_report_type report_type)
{
	err_t 				err;
	ULONG				current_time;
	lwiperf3_state_tcp_t	*conn;
#if IPERF3_DEBUG
	printf("\nlwiperf_tcp_close line:%d\n",__LINE__);
#endif
#ifdef __DEBUG_IPERF3__
	IPERF3_MSG(CYAN_COLOR "[%s] >>>>>>\n" CLEAR_COLOR, __func__);
#endif // __DEBUG_IPERF3__

	conn = &ctrlInfo_ptr->lwiperf_state;
	iperf3_server_reverse = 0;

	if (report_type == LWIPERF_TCP_DONE_SERVER) {
		current_time = xTaskGetTickCount();

		/* TCP Rx Total */
		print_iperf3(ctrlInfo_ptr->rxCount,
					ctrlInfo_ptr->BytesRxed,
					ctrlInfo_ptr->StartTime,
					current_time,
					ctrlInfo_ptr->StartTime,
					ctrlInfo_ptr->version,
					&ctrlInfo_ptr->remote_ip,
					ctrlInfo_ptr->remote_port,
					ctrlInfo_ptr->bandwidth_format,
					PRINT_TYPE_TCP_RX_TOL);

#ifdef TCP_SERVER_LOOP
		IP3_TCP_Rx_Restart_Flag = 1;
#else
		IP3_TCP_Rx_Finish_Flag = 1;
#endif
	}
	else if (report_type == LWIPERF_TCP_DONE_CLIENT && ctrlInfo_ptr->StartTime > 0) {
		current_time = xTaskGetTickCount();

#if defined (__SUPPORT_EEMBC__)
		th_printf("m-info-[Sent %llu Bytes]\r\n", ctrlInfo_ptr->BytesTxed);
#endif // __SUPPORT_EEMBC__
		/* TCP Tx Total */
		print_iperf3(ctrlInfo_ptr->txCount,
					ctrlInfo_ptr->BytesTxed,
					ctrlInfo_ptr->StartTime,
					current_time,
					ctrlInfo_ptr->StartTime,
					ctrlInfo_ptr->version,
					&ctrlInfo_ptr->remote_ip,
					ctrlInfo_ptr->remote_port,
					ctrlInfo_ptr->bandwidth_format,
					PRINT_TYPE_TCP_TX_TOL);

		IP3_TCP_Tx_Finish_Flag = 1;
	}
	else /* if ( report_type == LWIPERF_TCP_ABORTED_LOCAL || report_type == LWIPERF_TCP_ABORTED_REMOTE)*/
	{
#ifdef __DEBUG_IPERF3__
		IPERF3_MSG(CYAN_COLOR "[%s] Closed by abort (type:%s)\n" CLEAR_COLOR, __func__, (report_type==LWIPERF_TCP_ABORTED_LOCAL)?"ABORTED_LOCAL":"ABORTED_REMOTE");
#endif // __DEBUG_IPERF3__
		current_time = xTaskGetTickCount();

		if (!conn->base.server) {
			/* TCP Tx Total */
#if defined (__SUPPORT_EEMBC__)
			th_printf("m-info-[Sent %llu Bytes]\r\n", ctrlInfo_ptr->BytesTxed);
#endif // __SUPPORT_EEMBC__
			print_iperf3(ctrlInfo_ptr->txCount,
					ctrlInfo_ptr->BytesTxed,
					ctrlInfo_ptr->StartTime,
					current_time,
					ctrlInfo_ptr->StartTime,
					ctrlInfo_ptr->version,
					&ctrlInfo_ptr->remote_ip,
					ctrlInfo_ptr->remote_port,
					ctrlInfo_ptr->bandwidth_format,
					PRINT_TYPE_TCP_TX_TOL);

			IP3_TCP_Tx_Finish_Flag = 1;
		}
		else {
			/* TCP Rx Total */
			print_iperf3(ctrlInfo_ptr->rxCount,
					ctrlInfo_ptr->BytesRxed,
					ctrlInfo_ptr->StartTime,
					current_time,
					ctrlInfo_ptr->StartTime,
					ctrlInfo_ptr->version,
					&ctrlInfo_ptr->remote_ip,
					ctrlInfo_ptr->remote_port,
					ctrlInfo_ptr->bandwidth_format,
					PRINT_TYPE_TCP_RX_TOL);
#ifdef TCP_SERVER_LOOP
					IP3_TCP_Rx_Restart_Flag = 1;
#else
					IP3_TCP_Rx_Finish_Flag = 1;
#endif
		}
	}

#ifdef USING_LIST_DELETE	/* F_F_S */
	lwiperf_list_remove(&conn->base);
#endif // USING_LIST_DELETE
	lwip_tcp_conn_report(ctrlInfo_ptr, report_type);


#ifdef __IPERF3_API_MODE_ALTCP__
	if (ctrlInfo_ptr->tcpApiMode == TCP_API_MODE_ALTCP) {
		if (conn->conn_alpcb != NULL) {

			altcp_arg(conn->conn_alpcb, NULL);
			altcp_poll(conn->conn_alpcb, NULL, 0);
			altcp_sent(conn->conn_alpcb, NULL);
			altcp_recv(conn->conn_alpcb, NULL);
			altcp_err(conn->conn_alpcb, NULL);
			err = altcp_close(conn->conn_alpcb);

			if (err != ERR_OK) {
				/* don't want to wait for free memory here... */
				tcp_abort(conn->conn_alpcb);
				IPERF3_MSG(RED_COLOR "[%s] conn_alpcb aborted\n" CLEAR_COLOR, __func__);
			}
			else {
#ifdef __DEBUG_IPERF3__
				IPERF3_MSG(CYAN_COLOR "[%s] conn_alpcb closed 0x%x\n" CLEAR_COLOR, __func__, conn->conn_alpcb);
#endif // __DEBUG_IPERF3__
				conn->conn_alpcb = NULL;
			}
		}
		else {
			/* no conn pcb, this is the listener pcb */

			err = altcp_close(conn->server_alpcb);
			LWIP_ASSERT("error", err == ERR_OK);
			if (err != ERR_OK) {
				IPERF3_MSG(RED_COLOR "[%s] server_alpcb aborted\n" CLEAR_COLOR, __func__);
			}
			else {
				conn->server_alpcb = NULL;
#ifdef __DEBUG_IPERF3__
				IPERF3_MSG(CYAN_COLOR "[%s] server_alpcb closed\n" CLEAR_COLOR, __func__);
#endif // __DEBUG_IPERF3__
			}
		}
	}
	else
#endif // __IPERF3_API_MODE_ALTCP__
	if (ctrlInfo_ptr->tcpApiMode == TCP_API_MODE_TCP) {
		if (conn->conn_pcb != NULL) {
			tcp_arg(conn->conn_pcb, NULL);
			tcp_poll(conn->conn_pcb, NULL, 0);
			tcp_sent(conn->conn_pcb, NULL);
			tcp_recv(conn->conn_pcb, NULL);
			tcp_err(conn->conn_pcb, NULL);

			err = tcp_close(conn->conn_pcb);
			if (err != ERR_OK) {
				/* don't want to wait for free memory here... */
				tcp_abort(conn->conn_pcb);
				IPERF3_MSG(RED_COLOR "[%s] conn_pcb aborted\n" CLEAR_COLOR, __func__);
			}
			else {
#ifdef __DEBUG_IPERF3__
				IPERF3_MSG(CYAN_COLOR "[%s] conn_pcb closed 0x%x\n" CLEAR_COLOR, __func__, conn->conn_pcb);
#endif // __DEBUG_IPERF3__

#ifdef USE_TCP_SERVER_LISTEN
				conn->conn_pcb = NULL;//
#endif
			}
		}
#if 0
		else {
			/* no conn pcb, this is the listener pcb */
			err = tcp_close(conn->server_pcb);
			LWIP_ASSERT("error", err == ERR_OK);
			if (err != ERR_OK) {
				IPERF3_MSG(RED_COLOR "[%s] server_pcb aborted\n" CLEAR_COLOR, __func__);
			}
			else {
//				conn->server_pcb = NULL;
#ifdef __DEBUG_IPERF3__
				IPERF3_MSG(CYAN_COLOR "[%s] server_pcb closed\n" CLEAR_COLOR, __func__);
#endif // __DEBUG_IPERF3__
			}
		}
#endif
	}
	else {
		IPERF3_MSG(RED_COLOR "[%s:%d] Unknown mode: %d\n" CLEAR_COLOR,
							__func__, __LINE__, ctrlInfo_ptr->tcpApiMode);
	}
}

/** Try to send more data on an iPerf3 tcp session */
static err_t
lwiperf_tcp_client_send_more(ctrl3_info_t *ctrlInfo_ptr)
{
	int send_more;
	err_t err = 0;
	u16_t txlen;
	u16_t txlen_max;
	void *txptr;
	u8_t apiflags;

	ULONG		current_time = 0;
	ULONG		packet_size = IPERF_TCP_TX_MIN_SIZE;
#ifdef __ENABLE_UNUSED__
	int			total_cpy_len;
	int			data_cpy_ptr = 0;
	unsigned long long startTime = 0, stopTime = 0;
	long		remain_time = 0;		/* us */
	unsigned long	Tx_ElapsedTme;
#ifdef PREVIOUS_PACKET_DELAY_COMPENSATION
	int			WaitOver = 0;
#endif // PREVIOUS_PACKET_DELAY_COMPENSATION
#endif	//__ENABLE_UNUSED__
	long		bandwidth_time = 0;		/* bandwidth target time usec */
	int			gap = 0;

#ifdef __DEBUG_BANDWIDTH__
	int			tx_overtime = 0;
	int			tx_undertime = 0;
	int			error_cnt = 0;
	int			waitOver_cnt = 0;
#endif /* __DEBUG_BANDWIDTH__ */
	lwiperf3_state_tcp_t *conn = &ctrlInfo_ptr->lwiperf_state;

#ifdef __DEBUG_IPERF3__
	IPERF3_MSG(CYAN_COLOR "[%s] >>>>>>\n" CLEAR_COLOR, __func__);
#endif // __DEBUG_IPERF3__

	LWIP_ASSERT("conn invalid", (conn != NULL) && conn->base.tcp && (conn->base.server == 0));

	if (ctrlInfo_ptr->PacketSize > 0) {
		if (ctrlInfo_ptr->PacketSize < IPERF_TCP_TX_MIN_SIZE) {
			packet_size = IPERF_TCP_TX_MIN_SIZE;
		}
		else if (ctrlInfo_ptr->PacketSize > TCP_MSS) {
			packet_size = TCP_MSS;
		}
		else {
			packet_size = ctrlInfo_ptr->PacketSize;
		}
	}
	else {
		packet_size = TCP_MSS;
	}

#ifdef __ENABLE_UNUSED__
	total_cpy_len = packet_size;
	while (total_cpy_len) {
		memcpy(send_data + data_cpy_ptr,
				IPERF_DATA,
				(total_cpy_len > IPERF_DATA_LEN ? IPERF_DATA_LEN : total_cpy_len));

		data_cpy_ptr = data_cpy_ptr + IPERF_DATA_LEN;
		total_cpy_len = total_cpy_len - (total_cpy_len > IPERF_DATA_LEN ? IPERF_DATA_LEN : total_cpy_len);
	}
#endif  //__ENABLE_UNUSED__

	bandwidth_time = (long)targetTimePerPacket(ctrlInfo_ptr->bandwidth, packet_size);

	current_time = xTaskGetTickCount();

	do {
		send_more = 0;
#ifdef __ENABLE_UNUSED__
		if (conn->settings.amount & PP_HTONL(0x80000000)) {
			/* this session is time-limited */
			u32_t now = sys_now();
			u32_t diff_ms = now - conn->time_started;
			u32_t time = (u32_t) - (s32_t)lwip_htonl(conn->settings.amount);
			u32_t time_ms = time * 10;
			if (diff_ms >= time_ms) {
				/* time specified by the client is over -> close the connection */
#if 1//def __DEBUG_IPERF3__
				IPERF3_MSG(CYAN_COLOR "[%s:%d] Error (diff_ms:0x%x, time_ms:0x%x)\n" CLEAR_COLOR,
										__func__, __LINE__, diff_ms, time_ms);
#endif // __DEBUG_IPERF3__
				lwiperf_tcp_close(ctrlInfo_ptr, LWIPERF_TCP_DONE_CLIENT);
				return ERR_OK;
			}
		}
		else {
			/* this session is byte-limited */
			u32_t amount_bytes = lwip_htonl(conn->settings.amount);
			/* @todo: this can send up to 1*MSS more than requested... */
			if (amount_bytes >= conn->bytes_transferred) {
				/* all requested bytes transferred -> close the connection */
#ifdef __DEBUG_IPERF3__
				IPERF3_MSG(CYAN_COLOR "[%s:%d] Error (amount_bytes:%d, conn->bytes_transferred:%d)\n" CLEAR_COLOR,
										__func__, __LINE__, amount_bytes, conn->bytes_transferred);
#endif // __DEBUG_IPERF3__
				lwiperf_tcp_close(ctrlInfo_ptr, LWIPERF_TCP_DONE_CLIENT);
				return ERR_OK;
			}
		}
#else
		if (current_time >= conn->expire_time ||  IP3_TCP_Tx_Finish_Flag) {
			lwiperf_tcp_err(ctrlInfo_ptr, ERR_TIMEOUT);
			return ERR_OK;
		}

#endif	//__ENABLE_UNUSED__

#ifdef __DEBUG_IPERF3__
		IPERF3_MSG(CYAN_COLOR "[%s] bytes_transferred:%d\n" CLEAR_COLOR, __func__, conn->bytes_transferred);
#endif // __DEBUG_IPERF3__
		if (conn->bytes_transferred < 24) {
			/* transmit the settings a first time */
			txptr = &((u8_t *)&conn->settings)[conn->bytes_transferred];
			txlen_max = (u16_t)(24 - conn->bytes_transferred);
			apiflags = TCP_WRITE_FLAG_COPY;
		}
		else if (conn->bytes_transferred < 48) {
			/* transmit the settings a second time */
			txptr = &((u8_t *)&conn->settings)[conn->bytes_transferred - 24];
			txlen_max = (u16_t)(48 - conn->bytes_transferred);
			apiflags = TCP_WRITE_FLAG_COPY | TCP_WRITE_FLAG_MORE;
			send_more = 1;
		}
		else {
			/* transmit data */
			/* @todo: every x bytes, transmit the settings again */
			txptr = LWIP_CONST_CAST(void *, &lwiperf_txbuf_const[conn->bytes_transferred % 10]);
			//txlen_max = TCP_MSS;
			txlen_max = (u16_t)packet_size;
			if (conn->bytes_transferred == 48) { /* @todo: fix this for intermediate settings, too */
				//txlen_max = TCP_MSS - 24;
				txlen_max = (u16_t)(packet_size - 24);
			}
			apiflags = 0;	/* no copying needed */
			send_more = 1;
		}

		txlen = txlen_max;

		do {
#ifdef __DEBUG_IPERF3__
			IPERF3_MSG(CYAN_COLOR "[%s] tcp_write len: %d apiflags:0x%x\n" CLEAR_COLOR, __func__, txlen, apiflags);

#endif // __DEBUG_IPERF3__
			apiflags = TCP_WRITE_FLAG_COPY | TCP_WRITE_FLAG_MORE;	/* F_F_S */

#ifdef __IPERF3_API_MODE_ALTCP__
			if (ctrlInfo_ptr->tcpApiMode == TCP_API_MODE_ALTCP) {
				err = altcp_write(conn->conn_alpcb, txptr, txlen, apiflags);
			}
			else
#endif // __IPERF3_API_MODE_ALTCP__
			if (ctrlInfo_ptr->tcpApiMode == TCP_API_MODE_TCP) {
				err = tcp_write(conn->conn_pcb, txptr, txlen, apiflags);
			}
			else {
				IPERF3_MSG(RED_COLOR "[%s:%d] Unknown mode: %d\n" CLEAR_COLOR,
							__func__, __LINE__, ctrlInfo_ptr->tcpApiMode);
			}

			if (err == ERR_MEM) {
#ifdef __DEBUG_IPERF3__
				IPERF3_MSG(RED_COLOR "[%s] tcp_write err(ERR_MEM)\n" CLEAR_COLOR, __func__);
#endif // __DEBUG_IPERF3__
				txlen /= 2;
			}
			else if (err == ERR_OK) {
				/* Update the counter. */
				ctrlInfo_ptr->PacketsTxed++;
				if (packet_size == txlen)
				{
					ctrlInfo_ptr->BytesTxed += packet_size;
				}
				else
				{
					ctrlInfo_ptr->BytesTxed += txlen;
				}
			}
			else {
				IPERF3_MSG(RED_COLOR "[%s] tcp_write err\n" CLEAR_COLOR, __func__);
			}

			current_time = xTaskGetTickCount();
#ifdef __ENABLE_UNUSED__  // not support
			if (ctrlInfo_ptr->bandwidth > 0) {

				stopTime = R_BSP_SystemRtcCountGet();

				Tx_ElapsedTme = sendElapsedTme(startTime, stopTime);

#ifdef PREVIOUS_PACKET_DELAY_COMPENSATION
				if (remain_time < 0) { /* Calibrate the previous packet transmission time. */
					remain_time = (bandwidth_time + remain_time) - (long)Tx_ElapsedTme;
				}
				else
#endif /* PREVIOUS_PACKET_DELAY_COMPENSATION */
				{
					remain_time = bandwidth_time - (long)Tx_ElapsedTme;
				}

				if (remain_time > 0) {
#ifdef PREVIOUS_PACKET_DELAY_COMPENSATION
					WaitOver = WaitT(remain_time);
#endif // PREVIOUS_PACKET_DELAY_COMPENSATION
					remain_time = 0;

#ifdef PREVIOUS_PACKET_DELAY_COMPENSATION
					if (WaitOver > 0) { /* Correct the wait time. */
						//IPERF3_MSG("WO = %d ", WaitOver);
						remain_time -= WaitOver;
					}
#endif /* PREVIOUS_PACKET_DELAY_COMPENSATION */
#ifdef __DEBUG_BANDWIDTH__
					tx_undertime++;
				}
				else {
					tx_overtime++;
#endif /* __DEBUG_BANDWIDTH__ */
				}
			}
#endif	//__ENABLE_UNUSED__
			if (ctrlInfo_ptr->Interval > 0) {
				/* Time Calculation: Next On-Time Second */
				gap = (int)(ctrlInfo_ptr->Interval - ((ctrlInfo_ptr->Interval_StartTime - ctrlInfo_ptr->StartTime) % configTICK_RATE_HZ));

				if (err == ERR_OK) {
					if (packet_size == txlen)
					{
						ctrlInfo_ptr->Interval_BytesTxed += packet_size;
					}
					else
					{
						ctrlInfo_ptr->Interval_BytesTxed += txlen;
					}
				}

#ifdef __DEBUG_IPERF3__
				IPERF3_MSG("[%s] Interval:%d StartTime:%d Interval_StartTime:%d current_time:%d gap:%d\n",
										__func__,
										ctrlInfo_ptr->Interval,
										ctrlInfo_ptr->StartTime,
										ctrlInfo_ptr->Interval_StartTime,
										current_time,
										gap);
#endif // __DEBUG_IPERF3__

				if ((ctrlInfo_ptr->Interval_StartTime < current_time) && (ctrlInfo_ptr->Interval_StartTime + (UINT)gap <= current_time)) {
					//IPERF3_MSG("gap=%03d ", gap);
#ifdef __DEBUG_BANDWIDTH__
					IPERF3_MSG("O:%04d U:%04d/T:%04d E%02d wo:%04d",
							tx_overtime,
							tx_undertime,
							tx_undertime+tx_overtime+error_cnt,
							error_cnt
							, waitOver_cnt
					);

					tx_overtime = 0;
					tx_undertime = 0;
					error_cnt = 0;
					waitOver_cnt = 0;
					//remain_time = 0;
#endif /* __DEBUG_BANDWIDTH__ */

					ctrlInfo_ptr->txCount++;

					/* TCP Tx print Interval */
					print_iperf3(ctrlInfo_ptr->txCount,
							(ULONG)ctrlInfo_ptr->Interval_BytesTxed,
							ctrlInfo_ptr->StartTime,
							current_time,
							ctrlInfo_ptr->Interval_StartTime,
							ctrlInfo_ptr->version,
							&ctrlInfo_ptr->remote_ip,
							ctrlInfo_ptr->remote_port,
							ctrlInfo_ptr->bandwidth_format,
							PRINT_TYPE_TCP_TX_INT);

					ctrlInfo_ptr->Interval_BytesTxed = 0;
					ctrlInfo_ptr->Interval_StartTime = current_time;	/* Initialization: interval start time */
#ifdef __LIB_IPERF_PRINT_MIB__
					iperf_print_mib(ctrlInfo_ptr->mib_flag);
#endif /* __LIB_IPERF_PRINT_MIB__ */
				}
			}

		} while ((err == ERR_MEM) && (txlen >= (packet_size / 2)));

		if (err == ERR_OK) {
			conn->bytes_transferred += txlen;
		}
		else {
			send_more = 0;
		}
	} while (ctrlInfo_ptr->transmit_rate ? 0 : send_more);


#ifdef __IPERF3_API_MODE_ALTCP__
	if (ctrlInfo_ptr->tcpApiMode == TCP_API_MODE_ALTCP) {
		altcp_output(conn->conn_alpcb);
	}
	else
#endif // __IPERF3_API_MODE_ALTCP__
	if (ctrlInfo_ptr->tcpApiMode == TCP_API_MODE_TCP) {
		tcp_output(conn->conn_pcb);
	}
	else {
		IPERF3_MSG(RED_COLOR "[%s:%d] Unknown mode: %d\n" CLEAR_COLOR,
							__func__, __LINE__, ctrlInfo_ptr->tcpApiMode);
	}

#ifdef __DEBUG_IPERF3__
	IPERF3_MSG(CYAN_COLOR "[%s] End\n" CLEAR_COLOR, __func__);
#endif // __DEBUG_IPERF3__
	return ERR_OK;
}

#ifdef __IPERF3_API_MODE_ALTCP__
/** TCP sent callback, try to send more data */
static err_t
lwiperf_altcp_client_sent(void *arg, struct altcp_pcb *tpcb, u16_t len)
{
#ifdef __DEBUG_IPERF3__
	IPERF3_MSG(CYAN_COLOR "[%s] >>>>>> len: %d\n" CLEAR_COLOR, __func__, len);
#endif // __DEBUG_IPERF3__

	ctrl3_info_t	*ctrlInfo_ptr = (ctrl3_info_t *)arg;
	lwiperf3_state_tcp_t *conn = &ctrlInfo_ptr->lwiperf_state;

	/* @todo: check 'len' (e.g. to time ACK of all data)? for now, we just send more... */
	LWIP_ASSERT("invalid conn", conn->conn_alpcb == tpcb);
	LWIP_UNUSED_ARG(tpcb);
	LWIP_UNUSED_ARG(len);

	conn->poll_count = 0;

	return lwiperf_tcp_client_send_more(ctrlInfo_ptr);
}
#endif // __IPERF3_API_MODE_ALTCP__
#if 0
static err_t
lwiperf_tcp_client_sent(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
#ifdef __DEBUG_IPERF3__
	IPERF3_MSG(CYAN_COLOR "[%s] >>>>>> len: %d\n" CLEAR_COLOR, __func__, len);
#endif // __DEBUG_IPERF3__

	ctrl3_info_t	*ctrlInfo_ptr = (ctrl3_info_t *)arg;
	lwiperf3_state_tcp_t *conn = &ctrlInfo_ptr->lwiperf_state;

	/* @todo: check 'len' (e.g. to time ACK of all data)? for now, we just send more... */
	LWIP_ASSERT("invalid conn", conn->conn_pcb == tpcb);
	LWIP_UNUSED_ARG(tpcb);
	LWIP_UNUSED_ARG(len);

	conn->poll_count = 0;

	return lwiperf_tcp_client_send_more(ctrlInfo_ptr);
}
#endif

#ifdef FOR_TEST_DONT_NEED
#ifdef __IPERF3_API_MODE_ALTCP__
static err_t
lwiperf_altcp_client_recv(void *arg, struct altcp_pcb *tpcb, struct pbuf *p, err_t err)
{
	err_t ret_err = ERR_OK;
	unsigned char *buf = NULL;
	size_t buflen = 0;

#ifdef __DEBUG_IPERF3__
	IPERF3_MSG(CYAN_COLOR "[%s] >>>>>> err: %d\n" CLEAR_COLOR, __func__, err);
#endif // __DEBUG_IPERF3__
	if (p == NULL) {
		ret_err = ERR_OK;
	}
	else if (err != ERR_OK) {
		altcp_recved(tpcb, p->tot_len);	// advertise window size
		pbuf_free(p);
		ret_err = err;
	}
	else {
		altcp_recved(tpcb, p->tot_len);	// advertise window size

		buflen = p->len;

		buf = pvPortMalloc(buflen + 1);
		if (buf == NULL) {
			IPERF3_MSG(RED_COLOR "[%s:%d] Failed to allocate memory to send msg(%ld)\n" CLEAR_COLOR,
																			__func__, __LINE__, buflen);
		}
		else {
			memcpy(buf, p->payload, p->len);
			buf[buflen] = '\0';

			tcp_write(tpcb, buf, buflen, 1);
		}

		pbuf_free(p);
	}

	return ERR_OK;
}
#endif // __IPERF3_API_MODE_ALTCP__

lwiperf_tcp_client_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
	err_t ret_err = ERR_OK;
	unsigned char *buf = NULL;
	size_t buflen = 0;

#ifdef __DEBUG_IPERF3__
	IPERF3_MSG(CYAN_COLOR "[%s] >>>>>> err: %d\n" CLEAR_COLOR, __func__, err);
#endif
	if (p == NULL) {
		ret_err = ERR_OK;
	}
	else if (err != ERR_OK) {
		tcp_recved(tpcb, p->tot_len);	// advertise window size

		pbuf_free(p);
		ret_err = err;
	}
	else {
		tcp_recved(tpcb, p->tot_len);	// advertise window size

		buflen = p->len;

		buf = pvPortMalloc(buflen + 1);
		if (buf == NULL) {
			IPERF3_MSG(RED_COLOR "[%s:%d] Failed to allocate memory to send msg(%ld)\n" CLEAR_COLOR,
																			__func__, __LINE__, buflen);
		}
		else {
			memcpy(buf, p->payload, p->len);
			buf[buflen] = '\0';

			tcp_write(tpcb, buf, buflen, 1);
		}

		pbuf_free(p);
	}

	return ERR_OK;
}
#endif

#ifdef __IPERF3_API_MODE_ALTCP__
/** TCP connected callback (active connection), send data now */
static err_t
lwiperf_altcp_client_connected(void *arg, struct altcp_pcb *tpcb, err_t err)
{
	ctrl3_info_t				*ctrlInfo_ptr;
	lwiperf3_state_tcp_t		*conn;
	ip_addr_t				*local_ip;
	u16_t					local_port;
	ip_addr_t				*remote_ip;
	u16_t					remote_port;

#ifdef __DEBUG_IPERF3__
	IPERF3_MSG(CYAN_COLOR "[%s] >>>>>> connected. err: %d\n" CLEAR_COLOR, __func__, err);
#endif // __DEBUG_IPERF3__

	ctrlInfo_ptr = (ctrl3_info_t *)arg;
	conn = &ctrlInfo_ptr->lwiperf_state;

	LWIP_ASSERT("invalid conn", conn->conn_alpcb == tpcb);
	LWIP_UNUSED_ARG(tpcb);

	/* Tx Start MSG */
	IPERF3_MSG(MSG_TCP_TX,
		ctrlInfo_ptr->version==IP_VERSION_V6 ? "[":"",
		ipaddr_ntoa(&ctrlInfo_ptr->remote_ip),
		ctrlInfo_ptr->version==IP_VERSION_V6 ? "]":"",
		ctrlInfo_ptr->port);

	if (err != ERR_OK) {
#ifdef __DEBUG_IPERF3__
		IPERF3_MSG(RED_COLOR "[%s:%d] tcp close by abort_remote \n" CLEAR_COLOR, __func__, __LINE__);
#endif // __DEBUG_IPERF3__
		lwiperf_tcp_close(ctrlInfo_ptr, LWIPERF_TCP_ABORTED_REMOTE);
		return ERR_OK;
	}
	conn->poll_count = 0;
	conn->time_started = sys_now();

	ctrlInfo_ptr->StartTime = xTaskGetTickCount();
	conn->expire_time = ctrlInfo_ptr->StartTime + (ctrlInfo_ptr->TestTime);

	local_ip = altcp_get_ip(tpcb, 1);
	local_port = altcp_get_port(tpcb, 1);

#ifdef __DEBUG_IPERF3__
	IPERF3_MSG(CYAN_COLOR"[%s] Local IP: %s:%d", __func__,
									ipaddr_ntoa(local_ip),
									local_port);
#endif // __DEBUG_IPERF3__

	remote_ip = altcp_get_ip(tpcb, 0);
	remote_port = altcp_get_port(tpcb, 0);

#ifdef __DEBUG_IPERF3__
	IPERF3_MSG(" connected with Remote IP: %s:%d\n" CLEAR_COLOR,
									ipaddr_ntoa(remote_ip),
									remote_port);
#endif // __DEBUG_IPERF3__

    if (ctrlInfo_ptr->version == IP_VERSION_V4) {
    	ip_addr_copy_from_ip4(ctrlInfo_ptr->local_ip, *local_ip);
    	ctrlInfo_ptr->local_port = local_port;
    	ip_addr_copy_from_ip4(ctrlInfo_ptr->remote_ip, *remote_ip);
    	ctrlInfo_ptr->remote_port = remote_port;
    } else if (ctrlInfo_ptr->version == IP_VERSION_V6) {
    	ip_addr_copy_from_ip6(ctrlInfo_ptr->local_ip, *local_ip);
    	ctrlInfo_ptr->local_port = local_port;
    	ip_addr_copy_from_ip6(ctrlInfo_ptr->remote_ip, *remote_ip);
    	ctrlInfo_ptr->remote_port = remote_port;
    }

	/* TCP Tx Interval Title */
	if (ctrlInfo_ptr->Interval > 0) {
		IPERF3_MSG("\n"
			ANSI_BCOLOR_GREEN ANSI_COLOR_RED
			MSG_HEAD_TCP_TX MSG_TITLE_INTERVAL
			ANSI_BCOLOR_BLACK ANSI_BCOLOR_WHITE	ANSI_NORMAL "\n",
			"[Dst IP:Port]");
	}

	ctrlInfo_ptr->StartTime = ctrlInfo_ptr->Interval_StartTime = xTaskGetTickCount();

	return lwiperf_tcp_client_send_more(ctrlInfo_ptr);
}
#endif // __IPERF3_API_MODE_ALTCP__
static err_t
lwiperf_tcp_client_connected(void *arg, struct tcp_pcb *tpcb, err_t err)
{
	ctrl3_info_t				*ctrlInfo_ptr;
	lwiperf3_state_tcp_t		*conn;

#ifdef __DEBUG_IPERF3__
	IPERF3_MSG(CYAN_COLOR "[%s] >>>>>> connected. err: %d\n" CLEAR_COLOR, __func__, err);
#endif // __DEBUG_IPERF3__

	ctrlInfo_ptr = (ctrl3_info_t *)arg;
	conn = &ctrlInfo_ptr->lwiperf_state;

	LWIP_ASSERT("invalid conn", conn->conn_pcb == tpcb);
	LWIP_UNUSED_ARG(tpcb);

	if (err != ERR_OK) {
#ifdef __DEBUG_IPERF3__
		IPERF3_MSG(RED_COLOR "[%s:%d] tcp close by abort_remote \n" CLEAR_COLOR, __func__, __LINE__);
#endif // __DEBUG_IPERF3__
		lwiperf_tcp_err(ctrlInfo_ptr, err);
		//lwiperf_tcp_close(ctrlInfo_ptr, LWIPERF_TCP_ABORTED_REMOTE);
		return ERR_OK;
	}
	conn->poll_count = 0;

#ifdef __DEBUG_IPERF3__
	IPERF3_MSG(CYAN_COLOR"[%s] Local IP: %s:%d ", __func__,
									ipaddr_ntoa(&conn->conn_pcb->local_ip),
									conn->conn_pcb->local_port);
	IPERF3_MSG(" connected with Remote IP: %s:%d\n" CLEAR_COLOR,
									ipaddr_ntoa(&conn->conn_pcb->remote_ip),
									conn->conn_pcb->remote_port);
#endif // __DEBUG_IPERF3__
#if LWIP_IPV4 && LWIP_IPV6
    if (ctrlInfo_ptr->version == IP_VERSION_V4) {
        ip_addr_copy_from_ip4(ctrlInfo_ptr->local_ip, conn->conn_pcb->local_ip.u_addr.ip4);
        ctrlInfo_ptr->local_port = conn->conn_pcb->local_port;

        ip_addr_copy_from_ip4(ctrlInfo_ptr->remote_ip, conn->conn_pcb->remote_ip.u_addr.ip4);
        ctrlInfo_ptr->remote_port = conn->conn_pcb->remote_port;
    }
    else if (ctrlInfo_ptr->version == IP_VERSION_V6) {
        ip_addr_copy_from_ip6(ctrlInfo_ptr->local_ip, conn->conn_pcb->local_ip.u_addr.ip6);
        ctrlInfo_ptr->local_port = conn->conn_pcb->local_port;

        ip_addr_copy_from_ip6(ctrlInfo_ptr->remote_ip, conn->conn_pcb->remote_ip.u_addr.ip6);
        ctrlInfo_ptr->remote_port = conn->conn_pcb->remote_port;
    }
#elif LWIP_IPV4
    ip_addr_copy_from_ip4(ctrlInfo_ptr->local_ip, conn->conn_pcb->local_ip);
    ctrlInfo_ptr->local_port = conn->conn_pcb->local_port;

    ip_addr_copy_from_ip4(ctrlInfo_ptr->remote_ip, conn->conn_pcb->remote_ip);
    ctrlInfo_ptr->remote_port = conn->conn_pcb->remote_port;
#elif LWIP_IPV6
    ip_addr_copy_from_ip6(ctrlInfo_ptr->local_ip, conn->conn_pcb->local_ip);
    ctrlInfo_ptr->local_port = conn->conn_pcb->local_port;

    ip_addr_copy_from_ip6(ctrlInfo_ptr->remote_ip, conn->conn_pcb->remote_ip);
    ctrlInfo_ptr->remote_port = conn->conn_pcb->remote_port;
#endif

	/* Tx Start MSG */
	IPERF3_MSG(MSG_TCP_TX,
		ctrlInfo_ptr->version==IP_VERSION_V6 ? "[":"",
		ipaddr_ntoa(&ctrlInfo_ptr->remote_ip),
		ctrlInfo_ptr->version==IP_VERSION_V6 ? "]":"",
		ctrlInfo_ptr->port);

	/* TCP Tx Interval Title */
	if (ctrlInfo_ptr->Interval > 0) {
		IPERF3_MSG(ANSI_BCOLOR_GREEN ANSI_COLOR_RED
			MSG_HEAD_TCP_TX MSG_TITLE_INTERVAL
			ANSI_BCOLOR_BLACK ANSI_BCOLOR_WHITE	ANSI_NORMAL "\n",
			"[Dst IP:Port]");
	}

	conn->time_started = sys_now();
	ctrlInfo_ptr->StartTime = ctrlInfo_ptr->Interval_StartTime = xTaskGetTickCount();
	conn->expire_time = ctrlInfo_ptr->StartTime + (ctrlInfo_ptr->TestTime);

	ctrlInfo_ptr->RunTime = 0;
	ctrlInfo_ptr->txCount = 0;
	ctrlInfo_ptr->PacketsTxed = 0;
	ctrlInfo_ptr->PacketsRxed = 0;
	ctrlInfo_ptr->BytesTxed = 0;
	ctrlInfo_ptr->BytesRxed = 0;

	return lwiperf_tcp_client_send_more(ctrlInfo_ptr);
}

/** Start TCP connection back to the client (either parallel or after the
 * receive test has finished.
 */
#define MAX_TCP_TX_TOUT_SEC	2
#define MAX_TCP_RX_TOUT_SEC	3	/* If there are many session , have to extend rx timeout in socket mode */
#define MAX_TCP_TX_ERR_CNT	60

#ifdef MULTI_THREAD_MULTI_SOCKET
#if 0
void *lwiperf_tcp_client(void *pvParameters)	/* TASK */
{
	int				clntSock;
	struct sockaddr_in	servAddr;
	int				retryCount = 0;
	int				status;
	char			*send_data = NULL;
	int				data_cpy_ptr = 0;
	int				total_cpy_len = 0;
	ULONG			current_time;
	int				gap = 0;
	ULONG			packet_size = IPERF_TCP_TX_MIN_SIZE;
	long			remain_time = 0;		/* us */
	unsigned long long startTime, stopTime;
	unsigned long	Tx_ElapsedTme;
	ULONG			expire_time;
	long			bandwidth_time = 0; /*bandwidth target time usec */
#ifdef PREVIOUS_PACKET_DELAY_COMPENSATION
	int				WaitOver = 0;
#endif // PREVIOUS_PACKET_DELAY_COMPENSATION
#ifdef __DEBUG_IPERF3__
	UCHAR			destip[40];
#endif // __DEBUG_IPERF3__
	int				tx_error_cnt = 0;

	err_t				err;
	lwiperf3_state_tcp_t *client_conn;
	struct altcp_pcb 	*newalpcb;
	struct tcp_pcb 		*newpcb;
	ip_addr_t 			remote_addr;
	ip_addr_t 			*remote_ip;
	u16_t 				remote_port;
	lwiperf3_state_tcp_t	*state_tcp;
	lwiperf_report_fn	report_fn;
	lwiperf_settings_t	Settings;
	lwiperf_settings_t	*settings_ptr;
	ctrl3_info_t			*ctrlInfo_ptr;
	UINT				tcp_apiMode;
	struct timeval		tv;

	/* Set the pointer. */
	ctrlInfo_ptr = (ctrl3_info_t *)pvParameters;

	tcp_apiMode = ctrlInfo_ptr->tcpApiMode;

	if (tcp_apiMode == TCP_API_MODE_ALTCP) {
		IPERF3_MSG(CYAN_COLOR "[%s] ALTCP mode(%d)\n" CLEAR_COLOR, __func__, tcp_apiMode);
		goto API_MODE_TCP;
	}
	else if (tcp_apiMode == TCP_API_MODE_TCP) {
		IPERF3_MSG(CYAN_COLOR "[%s] TCP mode(%d)\n" CLEAR_COLOR, __func__, tcp_apiMode);
		goto API_MODE_TCP;
	}
	else if (tcp_apiMode == TCP_API_MODE_SOCKET) {
		IPERF3_MSG(CYAN_COLOR "[%s] SOCKET mode(%d)\n" CLEAR_COLOR, __func__, tcp_apiMode);
		goto API_MODE_SOCKET;
	}
	else {
		IPERF3_MSG(RED_COLOR "[%s:%d] Unknown mode: %d\n" CLEAR_COLOR,
							__func__, __LINE__, tcp_apiMode);
	}

API_MODE_SOCKET:

	ctrlInfo_ptr->PacketsTxed = 0;
	ctrlInfo_ptr->BytesTxed = 0;
	ctrlInfo_ptr->StartTime = 0;
	ctrlInfo_ptr->RunTime = 0;

	IP3_TCP_Tx_Finish_Flag = 0;

	IPERF3_MSG(" Server addr: %s:%d\n",
									ipaddr_ntoa(&ctrlInfo_ptr->remote_ip),
									ctrlInfo_ptr->port);

#ifdef __DEBUG_IPERF3__
	longtoip(ctrlInfo_ptr->ip, (char*)destip);
	IPERF3_MSG("[%s]\n IP:%s\n port:%d\n Packet size:%d\n TestTime:%d sec.\n SendNum:%d\n Interval:%d sec.\n",
									__func__,
									destip,
									ctrlInfo_ptr->port,
									ctrlInfo_ptr->PacketSize,
									ctrlInfo_ptr->TestTime / configTICK_RATE_HZ,
									ctrlInfo_ptr->send_num,
									ctrlInfo_ptr->Interval / configTICK_RATE_HZ);

#endif // __DEBUG_IPERF3__

	clntSock = socket(AF_INET, SOCK_STREAM, 0);
	if(clntSock == -1) {
		IPERF3_MSG("Socket Error");
	}

	ip3_tcp_tx_sock = clntSock;

	/* LWIP TCP Socket processing/Assertion is dependant on TCP TX Timeout */
	tv.tv_sec = MAX_TCP_TX_TOUT_SEC;
	tv.tv_usec = 0;
	status = setsockopt(clntSock, SOL_SOCKET, SO_SNDTIMEO, (const void *)&tv, sizeof(struct timeval));
#ifdef __DEBUG_IPERF3__
	IPERF3_MSG("[%s] setsockopt SO_RCVTIMEO result:%d\n", __func__, status);
#endif // __DEBUG_IPERF3__

	memset(&servAddr, 0, sizeof(servAddr));

	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = htonl(ctrlInfo_ptr->ip);
	servAddr.sin_port = htons(LWIPERF3_TCP_PORT_DEFAULT);

try_connect:

	if(connect(clntSock, (void *)&servAddr, sizeof(servAddr)) < 0) {
		IPERF3_MSG(RED_COLOR "[%s] tcp socket connect fail...\n" CLEAR_COLOR, __func__);
		if (++retryCount > 5) {
			retryCount = 0;
			goto last;
		}
		else {
			vTaskDelay(portCONVERT_MS_2_TICKS(1000));
			goto try_connect;
		}
	}
	else {
#ifdef __DEBUG_IPERF3__
		IPERF3_MSG("Connect OK! Start...\n");
#endif // __DEBUG_IPERF3__
	}

	/* Set the test start time. */
	ctrlInfo_ptr->StartTime = ctrlInfo_ptr->Interval_StartTime = xTaskGetTickCount();

	expire_time = ctrlInfo_ptr->StartTime + (ctrlInfo_ptr->TestTime);

	/* Set the packet size. */
	if (ctrlInfo_ptr->PacketSize > 0) {
		if (ctrlInfo_ptr->PacketSize < IPERF_TCP_TX_MIN_SIZE) {
			packet_size = IPERF_TCP_TX_MIN_SIZE;
		}
		else if (ctrlInfo_ptr->PacketSize > TCP_MSS) {
			packet_size = TCP_MSS;
		}
		else {
			packet_size = ctrlInfo_ptr->PacketSize;
		}
	}
	else {
		packet_size = TCP_MSS;
	}

	send_data = pvPortMalloc(packet_size);
	if (send_data == NULL) {
		IPERF3_MSG("[%s] send_data alloc fail ...\n", __func__);
		goto last;
	}

	memset(send_data, 0, packet_size);

	total_cpy_len = packet_size;

	/*** make data ***/
	while (total_cpy_len) {
		memcpy(send_data + data_cpy_ptr,
			IPERF_DATA,
			(total_cpy_len > IPERF_DATA_LEN ? IPERF_DATA_LEN : total_cpy_len));

		data_cpy_ptr = data_cpy_ptr + IPERF_DATA_LEN;
		total_cpy_len = total_cpy_len - (total_cpy_len > IPERF_DATA_LEN ? IPERF_DATA_LEN : total_cpy_len);
	}

	/* Tx Start MSG */
	IPERF3_MSG(MSG_TCP_TX,
		ctrlInfo_ptr->version==IP_VERSION_V6 ? "[":"",
		destip,
		ctrlInfo_ptr->version==IP_VERSION_V6 ? "]":"",
		ctrlInfo_ptr->port);

	/* Interval Title */
	if (ctrlInfo_ptr->Interval > 0) {
		IPERF3_MSG(ANSI_BCOLOR_GREEN ANSI_COLOR_RED
			MSG_HEAD_TCP_TX MSG_TITLE_INTERVAL
			ANSI_BCOLOR_BLACK ANSI_BCOLOR_WHITE ANSI_NORMAL "\n",
			"[Dst IP:Port]");
	}

	tx_error_cnt = 0;
	/* Loop to transmit the packet. */
	while (xTaskGetTickCount() <= expire_time && !IP3_TCP_Tx_Finish_Flag) {

		if (ctrlInfo_ptr->bandwidth > 0) {
			startTime = xTaskGetTickCount();
		}

		status = send(clntSock, send_data, packet_size, 0);
		if (status <= 0) {
#if 1
#ifdef __DEBUG_IPERF3__
			IPERF3_MSG(RED_COLOR " ERR iPerf3 TCP_Send (cnt:%d status:%d)\n" CLEAR_COLOR, tx_error_cnt, status);
#endif // __DEBUG_IPERF3__

			if ( ++tx_error_cnt < MAX_TCP_TX_ERR_CNT) {
				vTaskDelay(1);
			}
			else {
				IPERF3_MSG(RED_COLOR " ERR iPerf3 TCP_Send (cnt:%d status:%d)\n" CLEAR_COLOR, tx_error_cnt, status);
				goto IPERF_TCP_TX_END;
			}
#else
			if( ++tx_error_cnt < MAX_TCP_TX_ERR_CNT){
				vTaskDelay(1);

				IPERF3_MSG(RED_COLOR " ERR iPerf3 TCP_Send (cnt:%d status:%d)\n" CLEAR_COLOR, tx_error_cnt, status);
			}else {
				IPERF3_MSG(RED_COLOR " ERR iPerf3 TCP_Send (cnt:%d status:%d)\n" CLEAR_COLOR, tx_error_cnt, status);
				goto IPERF_TCP_TX_END;
			}
#endif
		}
		else {
			/* Update the counter. */
			ctrlInfo_ptr->PacketsTxed++;
			ctrlInfo_ptr->BytesTxed += packet_size;
			tx_error_cnt = 0;
		}

		current_time = xTaskGetTickCount();

		if (ctrlInfo_ptr->bandwidth > 0) {

			stopTime = xTaskGetTickCount();

			Tx_ElapsedTme = sendElapsedTme(startTime, stopTime);

#ifdef PREVIOUS_PACKET_DELAY_COMPENSATION
			if (remain_time < 0) { // ?ÃŒ?Ã¼ Ã†Ã¤Ã…Â¶ ?Ã¼Â¼Ã› Â½ÃƒÂ°Â£ ÂºÂ¸Ã�Â¤
				remain_time = (bandwidth_time + remain_time) - (long)Tx_ElapsedTme;
			} else
#endif /* PREVIOUS_PACKET_DELAY_COMPENSATION */
			{
				remain_time = bandwidth_time - (long)Tx_ElapsedTme;
			}

			if (remain_time > 0) {
#ifdef PREVIOUS_PACKET_DELAY_COMPENSATION
				WaitOver = WaitT(remain_time);
#endif // PREVIOUS_PACKET_DELAY_COMPENSATION
				remain_time = 0;

#ifdef PREVIOUS_PACKET_DELAY_COMPENSATION
				if (WaitOver > 0) { /* wait ÃƒÃŠÂ°Ãº Â½ÃƒÂ°Â£ ÂºÂ¸Ã�Â¤ */
					//IPERF3_MSG("WO = %d ", WaitOver);
					remain_time -= WaitOver;
				}
#endif /* PREVIOUS_PACKET_DELAY_COMPENSATION */
#ifdef __DEBUG_BANDWIDTH__
				tx_undertime++;
			}
			else {
				tx_overtime++;
#endif /* __DEBUG_BANDWIDTH__ */
			}
		}

		if (ctrlInfo_ptr->Interval > 0) {
			/* Â´Ã™?Â½ Ã�Â¤Â°Â¢ÃƒÃŠ Â±Ã®Ã�Ã¶ Â½ÃƒÂ°Â£ Â°Ã¨Â»Ãª */
			gap = ctrlInfo_ptr->Interval - ((ctrlInfo_ptr->Interval_StartTime - ctrlInfo_ptr->StartTime) % configTICK_RATE_HZ);

			if (status > 0) {
				ctrlInfo_ptr->Interval_BytesTxed += packet_size;
			}

			if ((ctrlInfo_ptr->Interval_StartTime < current_time) && (ctrlInfo_ptr->Interval_StartTime + gap <= current_time)) {
				//IPERF3_MSG("gap=%03d ", gap);
#ifdef __DEBUG_BANDWIDTH__
				IPERF3_MSG("O:%04d U:%04d/T:%04d E%02d wo:%04d",
					tx_overtime,
					tx_undertime,
					tx_undertime+tx_overtime+error_cnt,
					error_cnt,
					waitOver_cnt
				);

				tx_overtime = 0;
				tx_undertime = 0;
				error_cnt = 0;
				waitOver_cnt = 0;
				//remain_time = 0;
#endif /* __DEBUG_BANDWIDTH__ */

				ctrlInfo_ptr->txCount++;

				/* TCP Tx print Interval */
				print_iperf3(ctrlInfo_ptr->txCount,
							(ULONG)ctrlInfo_ptr->Interval_BytesTxed,
							ctrlInfo_ptr->StartTime,
							current_time,
							ctrlInfo_ptr->Interval_StartTime,
							ctrlInfo_ptr->version,
							&ctrlInfo_ptr->remote_ip,
							ctrlInfo_ptr->remote_port,
							ctrlInfo_ptr->bandwidth_format,
							PRINT_TYPE_TCP_TX_INT);

				ctrlInfo_ptr->Interval_BytesTxed = 0;
				ctrlInfo_ptr->Interval_StartTime = current_time; /* interval start time ÃƒÃŠÂ±Ã¢ÃˆÂ­ */
#ifdef __LIB_IPERF_PRINT_MIB__
				iperf_print_mib(ctrlInfo_ptr->mib_flag);
#endif /* __LIB_IPERF_PRINT_MIB__ */
			}
		}

		if (ctrlInfo_ptr->transmit_rate) {		/* slow send */
			vTaskDelay(ctrlInfo_ptr->transmit_rate);
		}
	}

IPERF_TCP_TX_END:

	/* TCP Tx Total */
	print_iperf3(ctrlInfo_ptr->txCount,
					ctrlInfo_ptr->BytesTxed,
					ctrlInfo_ptr->StartTime,
					current_time,
					ctrlInfo_ptr->StartTime,
					ctrlInfo_ptr->version,
					&ctrlInfo_ptr->remote_ip,
					ctrlInfo_ptr->remote_port,
					ctrlInfo_ptr->bandwidth_format,
					PRINT_TYPE_TCP_TX_TOL);

	vPortFree(send_data);
last:
#if 0
	close(clntSock);
#else
	closesocket(clntSock);
#endif
	IP3_TCP_Tx_Finish_Flag = 1;

#ifdef __DEBUG_BANDWIDTH__
	IPERF3_MSG("Last Tx_ElapsedTme=%u\n", Tx_ElapsedTme);
	IPERF3_MSG("Last remain_time=%d\n", remain_time);
#endif /* __DEBUG_BANDWIDTH__ */


	IPERF3_MSG(GREEN_COLOR "End of Testing\n" CLEAR_COLOR);

	thread_tcp_tx_iperf3[ctrlInfo_ptr->pair_no] = NULL;
	vTaskDelete(iperf_tcp_tx_handler);

	return NULL;

API_MODE_TCP:

	IPERF3_MSG(CYAN_COLOR "\n TCP Client start\n" CLEAR_COLOR);
//	IP3_TCP_Tx_Finish_Flag = 0;

	Settings.flags = 0;
	Settings.num_threads = htonl(1);
	Settings.remote_port = htonl(LWIPERF3_TCP_PORT_DEFAULT);
	/* TODO: implement passing duration/amount of bytes to transfer */
	Settings.amount = htonl((u32_t)-1000);

	settings_ptr = &Settings;

	//LWIP_ASSERT("remote_ip != NULL", remote_ip != NULL);
	//LWIP_ASSERT("remote_ip != NULL", settings_ptr != NULL);
	//LWIP_ASSERT("new_conn != NULL", new_conn != NULL);
	//*new_conn = NULL;

	/* Set the pointer. */
	//ctrlInfo_ptr = (ctrl3_info_t *)pvParameters;

	IPERF3_MSG(" Server addr: %s:%d\n",
									ipaddr_ntoa(&ctrlInfo_ptr->remote_ip),
									ctrlInfo_ptr->port);

#ifdef __DEBUG_IPERF3__
	longtoip(ctrlInfo_ptr->ip, (char*)destip);
	IPERF3_MSG("[%s]\n IP:%s\n port:%d\n Packet size:%d\n TestTime:%d sec.\n SendNum:%d\n Interval:%d sec.\n",
																__func__,
																destip,
																ctrlInfo_ptr->port,
																ctrlInfo_ptr->PacketSize,
																ctrlInfo_ptr->TestTime / configTICK_RATE_HZ,
																ctrlInfo_ptr->send_num,
																ctrlInfo_ptr->Interval / configTICK_RATE_HZ);

#endif // __DEBUG_IPERF3__

	client_conn = &ctrlInfo_ptr->lwiperf_state;
	memset(client_conn, 0, sizeof(lwiperf3_state_tcp_t));

	report_fn = ctrlInfo_ptr->reportFunc;

	IP3_TCP_Tx_Finish_Flag = 0;

	remote_ip = &ctrlInfo_ptr->remote_ip;
	remote_port = ctrlInfo_ptr->remote_port;
	IPERF3_MSG(" Remote ip: %s:%d\n", ipaddr_ntoa(remote_ip), remote_port);

	if (tcp_apiMode == TCP_API_MODE_ALTCP) {
		newalpcb = altcp_new_ip_type(NULL, IP_GET_TYPE(remote_ip));
		if (newalpcb == NULL) {
			return ERR_MEM;
		}
		client_conn->conn_alpcb = newalpcb;
	}
	else if (tcp_apiMode == TCP_API_MODE_TCP) {
		newpcb = tcp_new_ip_type(IP_GET_TYPE(remote_ip));
		if (newpcb == NULL) {
			return ERR_MEM;
		}
		client_conn->conn_pcb = newpcb;
	}
	else {
		IPERF3_MSG(RED_COLOR "[%s:%d] Unknown mode: %d\n" CLEAR_COLOR,
							__func__, __LINE__, tcp_apiMode);
	}

	client_conn->base.tcp = 1;
	client_conn->base.related_master_state = NULL;	/* F_F_S */
	client_conn->time_started = sys_now();	/* @todo: set this again on 'connected' */
	client_conn->report_fn = report_fn;
	client_conn->report_arg = NULL;			/* F_F_S */
	client_conn->next_num = 4;				/* initial nr is '4' since the header has 24 byte */
	client_conn->bytes_transferred = 0;
	memcpy(&client_conn->settings, settings_ptr, sizeof(lwiperf_settings_t));
	client_conn->have_settings_buf = 1;

	if (tcp_apiMode == TCP_API_MODE_ALTCP) {
		ip_addr_copy(client_conn->remote_ip, *remote_ip);
		client_conn->remote_port = remote_port;
	}

	ctrlInfo_ptr->PacketsTxed = 0;
	ctrlInfo_ptr->BytesTxed = 0;
	ctrlInfo_ptr->StartTime = 0;
	ctrlInfo_ptr->RunTime = 0;

	if (tcp_apiMode == TCP_API_MODE_ALTCP) {
		altcp_arg(newalpcb, ctrlInfo_ptr);
		altcp_sent(newalpcb, lwiperf_altcp_client_sent);
#ifdef FOR_TEST_DONT_NEED
		altcp_recv(newalpcb, lwiperf_altcp_client_recv);
#endif
		altcp_poll(newalpcb, lwiperf_altcp_poll, ctrlInfo_ptr->transmit_rate);
		altcp_err(newalpcb, lwiperf_tcp_err);
	}
	else if (tcp_apiMode == TCP_API_MODE_TCP) {
		tcp_arg(newpcb, ctrlInfo_ptr);
		tcp_sent(newpcb, lwiperf_tcp_client_sent);
#ifdef FOR_TEST_DONT_NEED
		tcp_recv(newpcb, lwiperf_tcp_client_recv);
#endif
		tcp_poll(newpcb, lwiperf_tcp_poll, ctrlInfo_ptr->transmit_rate);
		tcp_err(newpcb, lwiperf_tcp_err);
	}
	else {
		IPERF3_MSG(RED_COLOR "[%s:%d] Unknown mode: %d\n" CLEAR_COLOR,
							__func__, __LINE__, tcp_apiMode);
	}

	ip_addr_copy(remote_addr, *remote_ip);

	client_conn->expire_time = xTaskGetTickCount() + (ctrlInfo_ptr->TestTime);

	if (tcp_apiMode == TCP_API_MODE_ALTCP) {
		err = altcp_connect(newalpcb, remote_ip, remote_port, lwiperf_altcp_client_connected);
	}
	else if (tcp_apiMode == TCP_API_MODE_TCP) {
		err = tcp_connect(newpcb, remote_ip, remote_port, lwiperf_tcp_client_connected);
	}
	else {
		IPERF3_MSG(RED_COLOR "[%s:%d] Unknown mode: %d\n" CLEAR_COLOR,
							__func__, __LINE__, tcp_apiMode);
	}

	if (err != ERR_OK) {
#ifdef __DEBUG_IPERF3__
		IPERF3_MSG(RED_COLOR "[%s:%d] tcp close by abort_local \n" CLEAR_COLOR, __func__, __LINE__);
#endif // __DEBUG_IPERF3__
		lwiperf_tcp_close(ctrlInfo_ptr, LWIPERF_TCP_ABORTED_LOCAL);
		return err;
	}
#ifdef USING_LIST_DELETE	/* F_F_S */
	lwiperf_list_add(&client_conn->base);
#endif // USING_LIST_DELETE

	do {
		vTaskDelay(1000 / portTICK_RATE_MS);
	} while (!IP3_TCP_Tx_Finish_Flag);

	thread_tcp_tx_iperf3[ctrlInfo_ptr->pair_no] = NULL;
	vTaskDelete(iperf_tcp_tx_handler);

	return NULL;
}
#endif
#else // undef MULTI_THREAD_MULTI_SOCKET
#if 0
void lwiperf_tcp_client(void *pvParameters)	/* TASK */
{

	ctrl3_info_t			*ctrlInfo_ptr;
	UINT				tcp_apiMode;
 	int ret;

	/* Set the pointer. */
	ctrlInfo_ptr = (ctrl3_info_t *)pvParameters;
	tcp_apiMode = ctrlInfo_ptr->tcpApiMode;
	IP3_TCP_Tx_Finish_Flag = 0;

	const unsigned int pair_no = ctrlInfo_ptr->pair_no;
	thread_tcp_tx_iperf3[pair_no].state = IPERF_TX_THD_STATE_ACTIVE;

	if (tcp_apiMode == TCP_API_MODE_TCP ) {
#ifdef __DEBUG_IPERF3__
		IPERF3_MSG(CYAN_COLOR "[%s] API mode(%d)\n" CLEAR_COLOR, __func__, tcp_apiMode);
#endif // __DEBUG_IPERF3__
		ret = api_mode_tcp_client(ctrlInfo_ptr);
	}
#ifdef __IPERF3_API_MODE_ALTCP__
	else if (tcp_apiMode == TCP_API_MODE_ALTCP ) {
#ifdef __DEBUG_IPERF3__
		IPERF3_MSG(CYAN_COLOR "[%s] ALTCP mode(%d)\n" CLEAR_COLOR, __func__, tcp_apiMode);
#endif // __DEBUG_IPERF3__
		ret = api_mode_tcp_client(ctrlInfo_ptr);
	}
#endif // __IPERF3_API_MODE_ALTCP__
#ifdef __IPERF3_API_MODE_SOCKET__
	else if (tcp_apiMode == TCP_API_MODE_SOCKET) {
#ifdef __DEBUG_IPERF3__
		IPERF3_MSG(CYAN_COLOR "[%s] SOCKET mode(%d)\n" CLEAR_COLOR, __func__, tcp_apiMode);
#endif // __DEBUG_IPERF3__
		ret = api_mode_socket_client(ctrlInfo_ptr);
	}
#endif // __IPERF3_API_MODE_SOCKET__

	thread_tcp_tx_iperf3[pair_no].state = IPERF_TX_THD_STATE_NONE;
	thread_tcp_tx_iperf3[pair_no].handler = NULL;
	vTaskDelete(NULL);

 	return;
}
#endif
#ifdef __IPERF3_API_MODE_SOCKET__
int api_mode_socket_client(void *pvParameters)
{
	int             clntSock;
	struct sockaddr_in  servAddr;
	int             retryCount = 0;
	int             status;
	char            *send_data = NULL;
	int             data_cpy_ptr = 0;
	int             total_cpy_len = 0;
	ULONG           current_time;
	int             gap = 0;
	ULONG           packet_size = IPERF_TCP_TX_MIN_SIZE;
	long            remain_time = 0;        /* us */
	unsigned long long startTime, stopTime;
	unsigned long   Tx_ElapsedTme;
	ULONG           expire_time;
	long            bandwidth_time = 0; /*bandwidth target time usec */
#ifdef PREVIOUS_PACKET_DELAY_COMPENSATION
	int             WaitOver = 0;
#endif // PREVIOUS_PACKET_DELAY_COMPENSATION
	int             tx_error_cnt = 0;

	ctrl3_info_t         *ctrlInfo_ptr;
	struct timeval      tv;

	ctrlInfo_ptr = (ctrl3_info_t *)pvParameters;
	ctrlInfo_ptr->PacketsTxed = 0;
	ctrlInfo_ptr->BytesTxed = 0;
	ctrlInfo_ptr->StartTime = 0;
	ctrlInfo_ptr->RunTime = 0;

	IPERF3_MSG(" Server addr: %s:%d\n",
									ipaddr_ntoa(&ctrlInfo_ptr->remote_ip),
									ctrlInfo_ptr->port);

#ifdef __DEBUG_IPERF3__
	IPERF3_MSG("[%s]\n IP:%s\n port:%d\n Packet size:%d\n TestTime:%d sec.\n SendNum:%d\n Interval:%d sec.\n",
									__func__,
									ipaddr_ntoa(&ctrlInfo_ptr->remote_ip),
									ctrlInfo_ptr->port,
									ctrlInfo_ptr->PacketSize,
									ctrlInfo_ptr->TestTime / configTICK_RATE_HZ,
									ctrlInfo_ptr->send_num,
									ctrlInfo_ptr->Interval / configTICK_RATE_HZ);

#endif // __DEBUG_IPERF3__

	clntSock = socket(AF_INET, SOCK_STREAM, 0);
	if(clntSock == -1) {
		IPERF3_MSG("Socket Error");
		return -1;
	}

	ip3_tcp_tx_sock = clntSock;

	/* LWIP TCP Socket processing/Assertion is dependant on TCP TX Timeout */
	tv.tv_sec = MAX_TCP_TX_TOUT_SEC;
	tv.tv_usec = 0;
	status = setsockopt(clntSock, SOL_SOCKET, SO_SNDTIMEO, (const void *)&tv, sizeof(struct timeval));
#ifdef __DEBUG_IPERF3__
	IPERF3_MSG("[%s] setsockopt SO_RCVTIMEO result:%d\n", __func__, status);
#endif // __DEBUG_IPERF3__

	memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = htonl(ctrlInfo_ptr->ip);
	servAddr.sin_port = htons(LWIPERF3_TCP_PORT_DEFAULT);

try_connect:
	if(connect(clntSock, (void *)&servAddr, sizeof(servAddr)) < 0) {
		IPERF3_MSG(RED_COLOR "[%s] tcp socket connect fail...\n" CLEAR_COLOR, __func__);
		if (++retryCount > 2) {
			retryCount = 0;
			goto last;
		}
		else {
			vTaskDelay(1000 / portTICK_RATE_MS);
			goto try_connect;
		}
	}

	/* Set the test start time. */
	ctrlInfo_ptr->StartTime = ctrlInfo_ptr->Interval_StartTime = xTaskGetTickCount();
	expire_time = ctrlInfo_ptr->StartTime + (ctrlInfo_ptr->TestTime);

	/* Set the packet size. */
	if (ctrlInfo_ptr->PacketSize > 0) {
		if (ctrlInfo_ptr->PacketSize < IPERF_TCP_TX_MIN_SIZE) {
			packet_size = IPERF_TCP_TX_MIN_SIZE;
		}
		else if (ctrlInfo_ptr->PacketSize > TCP_MSS) {
			packet_size = TCP_MSS;
		}
		else {
			packet_size = ctrlInfo_ptr->PacketSize;
		}
	}
	else {
		packet_size = TCP_MSS;
	}

	send_data = pvPortMalloc(packet_size);
	if (send_data == NULL) {
		IPERF3_MSG("[%s] send_data alloc fail ...\n", __func__);
		goto last;
	}

	memset(send_data, 0, packet_size);
	total_cpy_len = packet_size;

	/*** make data ***/
	while (total_cpy_len) {
		memcpy(send_data + data_cpy_ptr,
			IPERF_DATA,
			(total_cpy_len > IPERF_DATA_LEN ? IPERF_DATA_LEN : total_cpy_len));

		data_cpy_ptr = data_cpy_ptr + IPERF_DATA_LEN;
		total_cpy_len = total_cpy_len - (total_cpy_len > IPERF_DATA_LEN ? IPERF_DATA_LEN : total_cpy_len);
	}

	/* Tx Start MSG */
	IPERF3_MSG(MSG_TCP_TX,
		ctrlInfo_ptr->version==IP_VERSION_V6 ? "[":"", ipaddr_ntoa(&ctrlInfo_ptr->remote_ip),
		ctrlInfo_ptr->version==IP_VERSION_V6 ? "]":"", ctrlInfo_ptr->port);

	/* Interval Title */
	if (ctrlInfo_ptr->Interval > 0) {
		IPERF3_MSG(ANSI_BCOLOR_GREEN ANSI_COLOR_RED
			MSG_HEAD_TCP_TX MSG_TITLE_INTERVAL
			ANSI_BCOLOR_BLACK ANSI_BCOLOR_WHITE ANSI_NORMAL "\n",
			"[Dst IP:Port]");
	}

	tx_error_cnt = 0;
	/* Loop to transmit the packet. */
	while (xTaskGetTickCount() <= expire_time && !IP3_TCP_Tx_Finish_Flag) {

		if (ctrlInfo_ptr->bandwidth > 0) {
			startTime = xTaskGetTickCount();
		}

		status = send(clntSock, send_data, packet_size, 0);
		if (status <= 0) {
#ifdef __DEBUG_IPERF3__
			IPERF3_MSG(RED_COLOR " ERR iPerf3 TCP_Send (cnt:%d status:%d)\n" CLEAR_COLOR, tx_error_cnt, status);
#endif // __DEBUG_IPERF3__
#if 0
			if ( ++tx_error_cnt < MAX_TCP_TX_ERR_CNT) {
				vTaskDelay(1);
			}else
#endif
			{
				IPERF3_MSG(RED_COLOR " ERR iPerf3 TCP_Send (cnt:%d status:%d)\n" CLEAR_COLOR, tx_error_cnt, status);
				goto IPERF_TCP_TX_END;
			}
 		}
		else {
			/* Update the counter. */
			ctrlInfo_ptr->PacketsTxed++;
			ctrlInfo_ptr->BytesTxed += packet_size;
			tx_error_cnt = 0;
		}

		current_time = xTaskGetTickCount();

		if (ctrlInfo_ptr->bandwidth > 0) {

			stopTime = xTaskGetTickCount();

			Tx_ElapsedTme = sendElapsedTme(startTime, stopTime);

#ifdef PREVIOUS_PACKET_DELAY_COMPENSATION
			if (remain_time < 0) {
				remain_time = (bandwidth_time + remain_time) - (long)Tx_ElapsedTme;
			} else
#endif /* PREVIOUS_PACKET_DELAY_COMPENSATION */
			{
				remain_time = bandwidth_time - (long)Tx_ElapsedTme;
			}

			if (remain_time > 0) {
#ifdef PREVIOUS_PACKET_DELAY_COMPENSATION
				WaitOver = WaitT(remain_time);
#endif // PREVIOUS_PACKET_DELAY_COMPENSATION
				remain_time = 0;

#ifdef PREVIOUS_PACKET_DELAY_COMPENSATION
				if (WaitOver > 0) { /* wait ÃƒÃŠÂ°Ãº Â½ÃƒÂ°Â£ ÂºÂ¸Ã�Â¤ */
					//IPERF3_MSG("WO = %d ", WaitOver);
					remain_time -= WaitOver;
				}
#endif /* PREVIOUS_PACKET_DELAY_COMPENSATION */
#ifdef __DEBUG_BANDWIDTH__
				tx_undertime++;
			}
			else {
				tx_overtime++;
#endif /* __DEBUG_BANDWIDTH__ */
			}
		}

		if (ctrlInfo_ptr->Interval > 0) {
			/* Â´Ã™?Â½ Ã�Â¤Â°Â¢ÃƒÃŠ Â±Ã®Ã�Ã¶ Â½ÃƒÂ°Â£ Â°Ã¨Â»Ãª */
			gap = ctrlInfo_ptr->Interval - ((ctrlInfo_ptr->Interval_StartTime - ctrlInfo_ptr->StartTime) % configTICK_RATE_HZ);

			if (status > 0) {
				ctrlInfo_ptr->Interval_BytesTxed += packet_size;
			}

			if ((ctrlInfo_ptr->Interval_StartTime < current_time) && (ctrlInfo_ptr->Interval_StartTime + gap <= current_time)) {
				//IPERF3_MSG("gap=%03d ", gap);
#ifdef __DEBUG_BANDWIDTH__
				IPERF3_MSG("O:%04d U:%04d/T:%04d E%02d wo:%04d",
					tx_overtime,
					tx_undertime,
					tx_undertime+tx_overtime+error_cnt,
					error_cnt,
					waitOver_cnt
				);

				tx_overtime = 0;
				tx_undertime = 0;
				error_cnt = 0;
				waitOver_cnt = 0;
				//remain_time = 0;
#endif /* __DEBUG_BANDWIDTH__ */

				ctrlInfo_ptr->txCount++;

				/* TCP Tx print Interval */
				print_iperf3(ctrlInfo_ptr->txCount,
							(ULONG)ctrlInfo_ptr->Interval_BytesTxed,
							ctrlInfo_ptr->StartTime,
							current_time,
							ctrlInfo_ptr->Interval_StartTime,
							ctrlInfo_ptr->version,
							&ctrlInfo_ptr->remote_ip,
							ctrlInfo_ptr->remote_port,
							ctrlInfo_ptr->bandwidth_format,
							PRINT_TYPE_TCP_TX_INT);

				ctrlInfo_ptr->Interval_BytesTxed = 0;
				ctrlInfo_ptr->Interval_StartTime = current_time; /* interval start time ÃƒÃŠÂ±Ã¢ÃˆÂ­ */
#ifdef __LIB_IPERF_PRINT_MIB__
				iperf_print_mib(ctrlInfo_ptr->mib_flag);
#endif /* __LIB_IPERF_PRINT_MIB__ */
			}
		}

		if (ctrlInfo_ptr->transmit_rate) {		/* slow send */
			vTaskDelay(ctrlInfo_ptr->transmit_rate);
		}
	}

IPERF_TCP_TX_END:

	/* TCP Tx Total */
	print_iperf3(ctrlInfo_ptr->txCount,
					ctrlInfo_ptr->BytesTxed,
					ctrlInfo_ptr->StartTime,
					current_time,
					ctrlInfo_ptr->StartTime,
					ctrlInfo_ptr->version,
					&ctrlInfo_ptr->remote_ip,
					ctrlInfo_ptr->remote_port,
					ctrlInfo_ptr->bandwidth_format,
					PRINT_TYPE_TCP_TX_TOL);

	vPortFree(send_data);
last:
	closesocket(clntSock);

	IP3_TCP_Tx_Finish_Flag = 1;

#ifdef __DEBUG_BANDWIDTH__
	IPERF3_MSG("Last Tx_ElapsedTme=%u\n", Tx_ElapsedTme);
	IPERF3_MSG("Last remain_time=%d\n", remain_time);
#endif /* __DEBUG_BANDWIDTH__ */

	IPERF3_MSG(GREEN_COLOR "End of Testing\n" CLEAR_COLOR);
	return NULL;

}
#endif // __IPERF3_API_MODE_SOCKET__
#if 0
int api_mode_tcp_client(void *pvParameters)
{
#ifdef __DEBUG_IPERF3__
	UCHAR           ipstr[16];
#endif // __DEBUG_IPERF3__

	err_t               err = 0;
	lwiperf3_state_tcp_t *client_conn;
#ifdef __IPERF3_API_MODE_ALTCP__
	struct altcp_pcb    *newalpcb;
#endif // __IPERF3_API_MODE_ALTCP__
	struct tcp_pcb      *newpcb;
	ip_addr_t           remote_addr;
	ip_addr_t           *remote_ip;
	u16_t               remote_port;
	lwiperf_report_fn   report_fn;
	lwiperf_settings_t  Settings;
	lwiperf_settings_t  *settings_ptr;
	ctrl3_info_t         *ctrlInfo_ptr;
	UINT                tcp_apiMode;

	/* Set the pointer. */
	ctrlInfo_ptr = (ctrl3_info_t *)pvParameters;
	tcp_apiMode = ctrlInfo_ptr->tcpApiMode;

#ifdef __DEBUG_IPERF3__
	if(ctrlInfo_ptr->tcpApiMode == TCP_API_MODE_TCP)
		IPERF3_MSG(CYAN_COLOR "\n API TCP Client start\n" CLEAR_COLOR );
	else if(ctrlInfo_ptr->tcpApiMode == TCP_API_MODE_ALTCP)
		IPERF3_MSG(CYAN_COLOR "\n ALTCP Client start\n" CLEAR_COLOR);
#endif // __DEBUG_IPERF3__

	Settings.flags = 0;
	Settings.num_threads = htonl(1);
	Settings.remote_port = htonl(LWIPERF3_TCP_PORT_DEFAULT);
	/* TODO: implement passing duration/amount of bytes to transfer */
	Settings.amount = htonl((u32_t)-1000);
	settings_ptr = &Settings;

	IP3_TCP_Tx_Finish_Flag = 0;

#ifdef __DEBUG_IPERF3__
	IPERF3_MSG(" Server addr: %s:%d\n",
									ipaddr_ntoa(&ctrlInfo_ptr->remote_ip),
									ctrlInfo_ptr->port);

	if (ctrlInfo_ptr->version == IP_VERSION_V4) {
    	longtoip(ctrlInfo_ptr->ip, (char*)ipstr);
    } else if (ctrlInfo_ptr->version == IP_VERSION_V6) {
        ipv6long2str(ctrlInfo_ptr->ipv6, (char*)ipstr);
    }

	IPERF3_MSG("[%s]\n IP:%s\n port:%d\n Packet size:%d\n TestTime:%d sec.\n SendNum:%d\n Interval:%d sec.\n",
				__func__,
				ipstr,
				ctrlInfo_ptr->port,
				ctrlInfo_ptr->PacketSize,
				ctrlInfo_ptr->TestTime / configTICK_RATE_HZ,
				ctrlInfo_ptr->send_num,
				ctrlInfo_ptr->Interval / configTICK_RATE_HZ);
#endif // __DEBUG_IPERF3__

	client_conn = &ctrlInfo_ptr->lwiperf_state;
	memset(client_conn, 0, sizeof(lwiperf3_state_tcp_t));

	report_fn = ctrlInfo_ptr->reportFunc;
	remote_ip = &ctrlInfo_ptr->remote_ip;
	remote_port = ctrlInfo_ptr->remote_port;

#ifdef __DEBUG_IPERF3__
	IPERF3_MSG(" Remote ip: %s:%d\n", ipaddr_ntoa(remote_ip), remote_port);
#endif // __DEBUG_IPERF3__

#ifdef __IPERF3_API_MODE_ALTCP__
	if (tcp_apiMode == TCP_API_MODE_ALTCP) {
		newalpcb = altcp_new_ip_type(NULL, IP_GET_TYPE(remote_ip));
		if (newalpcb == NULL) {
			return ERR_MEM;
		}
		client_conn->conn_alpcb = newalpcb;
	}
	else
#endif // __IPERF3_API_MODE_ALTCP__
	if (tcp_apiMode == TCP_API_MODE_TCP) {
		newpcb = tcp_new_ip_type(IP_GET_TYPE(remote_ip));
		if (newpcb == NULL) {
			return ERR_MEM;
		}
		client_conn->conn_pcb = newpcb;
	}
	else {
		IPERF3_MSG(RED_COLOR "[%s:%d] Unknown mode: %d\n" CLEAR_COLOR,
							__func__, __LINE__, tcp_apiMode);
	}

	client_conn->base.tcp = 1;
	client_conn->base.related_master_state = NULL;	/* F_F_S */
	client_conn->time_started = sys_now();	/* @todo: set this again on 'connected' */
	client_conn->report_fn = report_fn;
	client_conn->report_arg = NULL;			/* F_F_S */
	client_conn->next_num = 4;				/* initial nr is '4' since the header has 24 byte */
	client_conn->bytes_transferred = 0;
	memcpy(&client_conn->settings, settings_ptr, sizeof(lwiperf_settings_t));
	client_conn->have_settings_buf = 1;

	if (tcp_apiMode == TCP_API_MODE_ALTCP || tcp_apiMode == TCP_API_MODE_TCP) {
		ip_addr_copy(client_conn->remote_ip, *remote_ip);
		client_conn->remote_port = remote_port;
	}

	ctrlInfo_ptr->PacketsTxed = 0;
	ctrlInfo_ptr->BytesTxed = 0;
	ctrlInfo_ptr->StartTime = 0;
	ctrlInfo_ptr->RunTime = 0;

#ifdef __IPERF3_API_MODE_ALTCP__
	if (tcp_apiMode == TCP_API_MODE_ALTCP) {
		altcp_arg(newalpcb, ctrlInfo_ptr);
		altcp_sent(newalpcb, lwiperf_altcp_client_sent);
#ifdef FOR_TEST_DONT_NEED
		altcp_recv(newalpcb, lwiperf_altcp_client_recv);
#endif
		altcp_poll(newalpcb, lwiperf_altcp_poll, ctrlInfo_ptr->transmit_rate);
		altcp_err(newalpcb, lwiperf_tcp_err);
	}
	else
#endif // __IPERF3_API_MODE_ALTCP__
	if (tcp_apiMode == TCP_API_MODE_TCP) {
		tcp_arg(newpcb, ctrlInfo_ptr);
		tcp_sent(newpcb, lwiperf_tcp_client_sent);
#ifdef FOR_TEST_DONT_NEED
		tcp_recv(newpcb, lwiperf_tcp_client_recv);
#endif
		tcp_poll(newpcb, lwiperf_tcp_poll, (u8_t)(ctrlInfo_ptr->transmit_rate));
		tcp_err(newpcb, lwiperf_tcp_err);
	}
	else {
		IPERF3_MSG(RED_COLOR "[%s:%d] Unknown mode: %d\n" CLEAR_COLOR,
							__func__, __LINE__, tcp_apiMode);
	}

	ip_addr_copy(remote_addr, *remote_ip);

	client_conn->expire_time = xTaskGetTickCount() + 200; // connection timeout(2Sec)

#ifdef __IPERF3_API_MODE_ALTCP__
	if (tcp_apiMode == TCP_API_MODE_ALTCP) {
		err = altcp_connect(newalpcb, &remote_addr, remote_port, lwiperf_altcp_client_connected);
	}
	else
#endif // __IPERF3_API_MODE_ALTCP__
	if (tcp_apiMode == TCP_API_MODE_TCP) {
		err = tcp_connect(newpcb, &remote_addr, remote_port, lwiperf_tcp_client_connected);
	}
	else {
		IPERF3_MSG(RED_COLOR "[%s:%d] Unknown mode: %d\n" CLEAR_COLOR,
							__func__, __LINE__, tcp_apiMode);
	}

	if (err != ERR_OK) {
#ifdef __DEBUG_IPERF3__
		IPERF3_MSG(RED_COLOR "[%s:%d] tcp close by abort_local \n" CLEAR_COLOR, __func__, __LINE__);
#endif // __DEBUG_IPERF3__
		lwiperf_tcp_close(ctrlInfo_ptr, LWIPERF_TCP_ABORTED_LOCAL);
	}
#ifdef USING_LIST_DELETE	/* F_F_S */
	lwiperf_list_add(&client_conn->base);
#endif // USING_LIST_DELETE

	do {
		vTaskDelay(portCONVERT_MS_2_TICKS(1000));
	} while (!IP3_TCP_Tx_Finish_Flag);

#if defined (__SUPPORT_EEMBC__)
	th_iperf_cb();
#endif // __SUPPORT_EEMBC__

	return 0;

}
#endif
#endif // MULTI_THREAD_MULTI_SOCKET

#ifdef __IPERF3_API_MODE_ALTCP__
/** Receive data on an iPerf3 tcp session */
static err_t
lwiperf_altcp_recv(void *arg, struct altcp_pcb *tpcb, struct pbuf *pbuf_ptr, err_t err)
{
#ifdef WATCH_DOG_ENABLE
//	extern void watch_dog_kicking(UINT rescale_time);
#endif	/* WATCH_DOG_ENABLE */

	u8_t tmp;
	u16_t tot_len;
	u32_t packet_idx;
	struct pbuf *q;
	lwiperf3_state_tcp_t *conn;
	ULONG		current_time = 0;
	UINT		status;
	ctrl3_info_t *ctrlInfo_ptr;
	int		gap = 0;

	ctrlInfo_ptr = (lwiperf3_state_tcp_t *)arg;
	conn = &ctrlInfo_ptr->lwiperf_state;

#ifdef __DEBUG_IPERF3__
	IPERF3_MSG(CYAN_COLOR "[%s] >>>>>>\n" CLEAR_COLOR, __func__);
#endif // __DEBUG_IPERF3__

#ifdef WATCH_DOG_ENABLE
//	watch_dog_kicking(5);	/* 5 SEC */
#endif	/* WATCH_DOG_ENABLE */

	LWIP_ASSERT("pcb mismatch", conn->conn_alpcb == tpcb);
	LWIP_UNUSED_ARG(tpcb);

	if (err != ERR_OK) {
#ifdef __DEBUG_IPERF3__
		IPERF3_MSG(RED_COLOR "[%s:%d] tcp close by abort_remote \n" CLEAR_COLOR, __func__, __LINE__);
#endif // __DEBUG_IPERF3__
		lwiperf_tcp_close(ctrlInfo_ptr, LWIPERF_TCP_ABORTED_REMOTE);
		return ERR_OK;
	}
	if (pbuf_ptr == NULL) {
		//IPERF3_MSG(RED_COLOR "[%s:%d] ERROR???\n" CLEAR_COLOR, __func__, __LINE__);
		lwiperf_tcp_close(ctrlInfo_ptr, LWIPERF_TCP_DONE_SERVER);
		return ERR_OK;
	}
	tot_len = pbuf_ptr->tot_len;

#ifdef __DEBUG_IPERF3__
	IPERF3_MSG(CYAN_COLOR "[%s] >>>>>> tot_len: %d\n" CLEAR_COLOR, __func__, tot_len);
#endif // __DEBUG_IPERF3__

	conn->poll_count = 0;

	if ((!conn->have_settings_buf) || ((conn->bytes_transferred - 24) % (1024 * 128) == 0)) {
		/* wait for 24-byte header */
		if (pbuf_ptr->tot_len < sizeof(lwiperf_settings_t)) {
#ifdef __DEBUG_IPERF3__
			IPERF3_MSG(RED_COLOR "[%s:%d] tcp close by abort_local \n" CLEAR_COLOR, __func__, __LINE__);
#endif // __DEBUG_IPERF3__
			lwiperf_tcp_close(ctrlInfo_ptr, LWIPERF_TCP_ABORTED_LOCAL_DATAERROR);
			pbuf_free(pbuf_ptr);
			return ERR_OK;
		}
		if (!conn->have_settings_buf) {
			if (pbuf_copy_partial(pbuf_ptr, &conn->settings, sizeof(lwiperf_settings_t), 0) != sizeof(lwiperf_settings_t)) {
#ifdef __DEBUG_IPERF3__
				IPERF3_MSG(RED_COLOR "[%s:%d] tcp close by abort_local \n" CLEAR_COLOR, __func__, __LINE__);
#endif // __DEBUG_IPERF3__
				lwiperf_tcp_close(ctrlInfo_ptr, LWIPERF_TCP_ABORTED_LOCAL);
				pbuf_free(pbuf_ptr);
				return ERR_OK;
			}
			conn->have_settings_buf = 1;
			if (conn->settings.flags & PP_HTONL(LWIPERF_FLAGS_ANSWER_TEST)) {
				IPERF3_MSG(RED_COLOR "[%s:%d] ERROR???\n" CLEAR_COLOR, __func__, __LINE__);
			}
		} else {
			if (conn->settings.flags & PP_HTONL(LWIPERF_FLAGS_ANSWER_TEST)) {
				if (pbuf_memcmp(pbuf_ptr, 0, &conn->settings, sizeof(lwiperf_settings_t)) != 0) {
#ifdef __DEBUG_IPERF3__
					IPERF3_MSG(RED_COLOR "[%s:%d] tcp close by abort_local \n" CLEAR_COLOR, __func__, __LINE__);
#endif // __DEBUG_IPERF3__
					lwiperf_tcp_close(ctrlInfo_ptr, LWIPERF_TCP_ABORTED_LOCAL_DATAERROR);
					pbuf_free(pbuf_ptr);
					return ERR_OK;
				}
			}
		}
		conn->bytes_transferred += sizeof(lwiperf_settings_t);
		if (conn->bytes_transferred <= 24) {
			conn->time_started = sys_now();
			altcp_recved(tpcb, pbuf_ptr->tot_len);
			pbuf_free(pbuf_ptr);
			return ERR_OK;
		}
		conn->next_num = 4;		/* 24 bytes received... */
		tmp = pbuf_remove_header(pbuf_ptr, 24);
		LWIP_ASSERT("pbuf_remove_header failed", tmp == 0);
		LWIP_UNUSED_ARG(tmp);	/* for LWIP_NOASSERT */
	}

	packet_idx = 0;
	for (q = pbuf_ptr; q != NULL; q = q->next) {
#if LWIPERF_CHECK_RX_DATA
		const u8_t *payload = (const u8_t *)q->payload;
		u16_t i;
		for (i = 0; i < q->len; i++) {
			u8_t val = payload[i];
			u8_t num = val - '0';
			if (num == conn->next_num) {
				conn->next_num++;
				if (conn->next_num == 10) {
					conn->next_num = 0;
				}
			} else {
#ifdef __DEBUG_IPERF3__
				IPERF3_MSG(RED_COLOR "[%s:%d] tcp close by abort_local \n" CLEAR_COLOR, __func__, __LINE__);
#endif // __DEBUG_IPERF3__
				lwiperf_tcp_close(ctrlInfo_ptr, LWIPERF_TCP_ABORTED_LOCAL_DATAERROR);
				pbuf_free(pbuf_ptr);
				return ERR_OK;
			}
		}
#endif
		packet_idx += q->len;
	}
	LWIP_ASSERT("count mismatch", packet_idx == pbuf_ptr->tot_len);
	conn->bytes_transferred += packet_idx;
#ifdef __DEBUG_IPERF3__
	//IPERF3_MSG(CYAN_COLOR "[%s] >>>>>> conn->bytes_transferred : %d\n" CLEAR_COLOR, __func__, conn->bytes_transferred);
#endif // __DEBUG_IPERF3__

	/* Update the counter. */
	ctrlInfo_ptr->PacketsRxed++;
	ctrlInfo_ptr->BytesRxed += packet_idx;

	current_time = xTaskGetTickCount();

	if (ctrlInfo_ptr->Interval > 0) {
		/* Â´Ã™?Â½ Ã�Â¤Â°Â¢ÃƒÃŠ Â±Ã®Ã�Ã¶ Â½ÃƒÂ°Â£ Â°Ã¨Â»Ãª */
		gap = ctrlInfo_ptr->Interval - ((ctrlInfo_ptr->Interval_StartTime - ctrlInfo_ptr->StartTime) % configTICK_RATE_HZ);

		ctrlInfo_ptr->Interval_BytesRxed += packet_idx;;

		if ((ctrlInfo_ptr->Interval_StartTime < current_time) && (ctrlInfo_ptr->Interval_StartTime + gap <= current_time)) {
			//IPERF3_MSG("gap=%03d ", gap);

			ctrlInfo_ptr->rxCount++;

			/* print TCP Rx Interval */
			print_iperf3(ctrlInfo_ptr->rxCount,
				ctrlInfo_ptr->Interval_BytesRxed,
				ctrlInfo_ptr->StartTime,
				current_time,
				ctrlInfo_ptr->Interval_StartTime,
				ctrlInfo_ptr->version,
				&ctrlInfo_ptr->remote_ip,
				ctrlInfo_ptr->remote_port,
				ctrlInfo_ptr->bandwidth_format,
				PRINT_TYPE_TCP_RX_INT);

			ctrlInfo_ptr->Interval_BytesRxed = 0;
			ctrlInfo_ptr->Interval_StartTime = current_time;	/* interval start time ÃƒÃŠÂ±Ã¢ÃˆÂ­ */
#ifdef __LIB_IPERF_PRINT_MIB__
			iperf_print_mib(ctrlInfo_ptr->mib_flag);
#endif /* __LIB_IPERF_PRINT_MIB__ */
		}
	}

	altcp_recved(tpcb, tot_len);
	pbuf_free(pbuf_ptr);
	return ERR_OK;
}
#endif // __IPERF3_API_MODE_ALTCP__

#if 1 /*New chages*/
static void
iperf_server_update_control(u32_t r_port, u32_t bReverse, u32_t ulAmount, u32_t xRemainingTime) {
	  lwiperf3_state_base_t *iter;

	  if (xSemaphoreTake(iperf3_data, portNO_DELAY) == pdTRUE) {
	      for (iter = lwiperf3_all_connections; iter != NULL; iter = iter->next) {
	        if (r_port == iter->conn_pcb.remote_port) {
#if IPERF3_DEBUG
	  printf("\nfun:%s port:%d , bReverse: %d xRemainingTime: %d\n",
			  __func__, iter->conn_pcb.remote_port, bReverse, xRemainingTime);
#endif
	        	iter->bReverse = bReverse;
	        	iter->ulAmount = ulAmount;
	        	iter->xRemainingTime = xRemainingTime;
	        }
	      }
	  	  xSemaphoreGive(iperf3_data);
	  }
}

static void
lwiperf_list_get_client(u32_t r_port, u32_t status, char *data, u32_t bytes) {
  lwiperf3_state_base_t *iter;
  u32_t result = 0;
  if (xSemaphoreTake(iperf3_data, portNO_DELAY) == pdTRUE) {
      for (iter = lwiperf3_all_connections; iter != NULL; iter = iter->next) {
        if (iter->data != NULL) {
          result = memcmp(iter->data, data, TCP_SOCKET_IDENTIFIER_LEN);
          if ((result == 0) && (r_port != iter->conn_pcb.remote_port)) {
            if ((status == tcp_Two_PacketState) &&
            		((iter->state == DATA_TRANSFER_STATE) || (iter->state == DATA_SENDING_STATE))) {
              iter->state = DATA_RESULT_STATE;
              break;
            } else if (status == tcp_One_PacketCountCopy) {
              iter->bytes = bytes;
              break;
            } else if (status == tcp_four_PacketCountGet) {
              iter->state = DATA_SENDING_INIT;
#if IPERF3_DEBUG
        printf("\n DATA_SENDING_INIT !!!\n");
#endif
              break;
            }
          }
        }
      }
    xSemaphoreGive(iperf3_data);
  }
}

static u32_t
lwiperf_list_find_client(u32_t port, const char *pay, u16_t len)
{
  lwiperf3_state_base_t *iter;
  u32_t result = 0, Control = 0, Reverse = 0, Amount = 0, Time = 0;

  if (xSemaphoreTake(iperf3_data, portNO_DELAY) == pdTRUE) {
	  for (iter = lwiperf3_all_connections; iter != NULL; iter = iter->next) {
	    if ((iter->data != NULL) && (pay != NULL)) {
	      result = memcmp(iter->data, pay, len);
	      if ( (result == 0) && (port != iter->conn_pcb.remote_port)) {
	#if IPERF3_DEBUG
	        printf("\nDATA STREAM RECEIVED\n");
	#endif
	        iter->bIsControl = 1;
	        Control = 1;
	        Reverse = iter->bReverse;
	        Amount = iter->ulAmount;
	        Time = iter->xRemainingTime;
	        iter->state = 3;/*Test start*/
	        break;
	      }
	    }
	  }
	  for (iter = lwiperf3_all_connections; iter != NULL; iter = iter->next) {
	    if (port == iter->conn_pcb.remote_port) {
	      if ((pay != NULL) && (len == TCP_SOCKET_IDENTIFIER_LEN)) {
	        memset(iter->data, 0, TCP_SOCKET_IDENTIFIER_LEN);
	        memcpy(iter->data, pay, len);
	#if IPERF3_DEBUG
	        printf("\nDATA COPIED\n");
	#endif
	        if (Control == 1) {
	          iter->state = DATA_TRANSFER_STATE;
	          iter->bReverse = Reverse;
	          iter->bIsControl = 0;
	          iter->ulAmount = Amount;
	          iter->xRemainingTime = Time;
	#if IPERF3_DEBUG
	          printf("\nDATA COPIED state DATA_TRANSFER_STATE");
	          printf("\nReverse = %d, Amount = %d, Time = %d\n", Reverse, Amount, Time);
	#endif
	          result = iter->state;
	        } else {
	          result = 0;
	        }
	        break;
	      } else if((iter->state == DATA_RESULT_STATE) 
	               || (iter->state == DATA_TRANSFER_STATE) 
	               || (iter->state == DATA_SENDING_STATE)
				   || (iter->state == DATA_SENDING_INIT)) {
	      	 result = iter->state;
	      	 break;
	      }
	    }
	  }
	  xSemaphoreGive(iperf3_data);
  }
  return result;
}

static u32_t
lwiperf_list_update_client(u32_t r_port, u32_t status, u32_t bytes)
{
  lwiperf3_state_base_t *iter;
  u32_t result = 0;
  u32_t update = 0;
  char data[TCP_SOCKET_IDENTIFIER_LEN];

  if (xSemaphoreTake(iperf3_data, portNO_DELAY) == pdTRUE) {
	  for (iter = lwiperf3_all_connections; iter != NULL; iter = iter->next) {
	    if ((iter->data != NULL) && (r_port == iter->conn_pcb.remote_port)) {
	        memset(data, 0, TCP_SOCKET_IDENTIFIER_LEN);
	        memcpy(data, iter->data, TCP_SOCKET_IDENTIFIER_LEN);
	      if ((iter->state != DATA_TRANSFER_STATE) && (status == tcp_Two_PacketState)) {
	    	  update = 1;
	#if IPERF3_DEBUG
	        printf("\nstatus tcp_Two_PacketState\n");
	#endif
	        break;
	      } else if ((iter->state == DATA_SENDING_INIT) && (status == tcp_five_PacketCountGet)) {
	    	  iter->state = DATA_SENDING_STATE;
	    	  result = iter->xRemainingTime;
#if IPERF3_DEBUG
        printf("\nstatus tcp_five_PacketCountGet, time: %d\n", iter->xRemainingTime);
#endif
	      	  break;
	      } else if ((iter->state == DATA_TRANSFER_STATE) && (status == tcp_One_PacketCountCopy)) {
	    	  update = 1;
	        break;
	      } else if (status == tcp_Three_PacketCountGet) {
	#if IPERF3_DEBUG
	        printf("\nout bytes = %d\n", iter->bytes);
	#endif
	        result = iter->bReverse;
	        break;
	      } else if ((iter->state == 100) && (status == tcp_four_PacketCountGet)) {
	    	  update = 1;
	        break;
	      }
	    }
	  }
  	  xSemaphoreGive(iperf3_data);
  }
  if (update) {
	  lwiperf_list_get_client(r_port, status, data, bytes);
  }
  return result;
}

static void
iperf_server_update_status(u32_t r_port, u32_t status) {
	  lwiperf3_state_base_t *iter;
	  u32_t is_reverse = 0;
#if IPERF3_DEBUG
	  printf("\nfun:%s, status: %d\n",__func__, status);
#endif
	  if (xSemaphoreTake(iperf3_data, portNO_DELAY) == pdTRUE) {
	      for (iter = lwiperf3_all_connections; iter != NULL; iter = iter->next) {
	        if (r_port == iter->conn_pcb.remote_port) {
	        	if ((status == 100) && (iter->bReverse)) {
	        		is_reverse = 1;
	        	}
		        iter->state = status;
	        	break;
	        }
	      }
	  	  xSemaphoreGive(iperf3_data);
	  }
	  if (is_reverse == 1) {
#if IPERF3_DEBUG
	  printf("\nfun:%s, status: %d, port:%d \n",__func__, status, r_port);
#endif
		  lwiperf_list_update_client(r_port, tcp_four_PacketCountGet, 0);
	  }
}

static err_t iperf3_tcp_server_data(struct tcp_pcb *tpcb, u32_t port, const char *pay, u16_t len, u32_t bytes) {

  u32_t conn_state = 0;
  int test_end = 4;
  u32_t ret = 1;

  conn_state = lwiperf_list_find_client(port, pay, len);

  if ((len == 1) && (pay != NULL)) {
      ret = memcmp(pay, &test_end, len);
      if (ret == 0) {
#if IPERF3_DEBUG
        printf("\n Test end received!!! \n");
#endif
        lwiperf_list_update_client(tpcb->remote_port, tcp_Two_PacketState, bytes);
        iperf_server_update_status(tpcb->remote_port, 5);
        return ERR_OK;
      }
  } else if(((conn_state == DATA_RESULT_STATE) || (conn_state == DATA_TRANSFER_STATE)) && (pay != NULL)) {
    lwiperf_list_update_client(port, tcp_One_PacketCountCopy, bytes);
    return ERR_OK;
  } else if((conn_state == DATA_RESULT_STATE) && (pay == NULL)) {
    lwiperf_list_update_client(port, tcp_One_PacketCountCopy, bytes);
#if IPERF3_DEBUG
    printf("\nConnection status: %d remote port:%d,bytes: %"U32_F" len: %d \n", conn_state, port, bytes, len);
#endif
    return ERR_OK;
  }
  return ERR_MEM;
}

static u32_t
get_iperf_server_status(u32_t r_port) {
	  lwiperf3_state_base_t *iter;
	  u32_t result = 200;
	  if (xSemaphoreTake(iperf3_data, portNO_DELAY) == pdTRUE) {
	      for (iter = lwiperf3_all_connections; iter != NULL; iter = iter->next) {
	        if (r_port == iter->conn_pcb.remote_port) {
	        	result = iter->state;
	        	break;
	        }
	      }
	  	  xSemaphoreGive(iperf3_data);
	  }
	  return result;
}
#if 0
static err_t
lwiperf_tcp_server_send_more(lwiperf3_state_tcp_t *conn, struct tcp_pcb *tpcb)
{
  err_t err;
  u32_t txlen;
  void *txptr = NULL;
  u8_t apiflags;

  if (conn->bytes_transferred < conn->base.xRemainingTime) {

      /* transmit data */
      /* @todo: every x bytes, transmit the settings again */
      txptr = LWIP_CONST_CAST(void *, &lwiperf_txbuf_const);
      txlen = TCP_MSS - 12;
      apiflags = TCP_WRITE_FLAG_COPY; /* no copying needed */

    err = tcp_write(tpcb, txptr, txlen, apiflags);
    if (err == ERR_OK) {
      conn->bytes_transferred += txlen;
    }
  }
  return ERR_OK;
}
#endif

static err_t iperf3_server_send_data(void *arg, struct tcp_pcb *tpcb, u32_t state) {

  ctrl3_info_t *ctrlInfo_ptr = (ctrl3_info_t *)arg;
  lwiperf3_state_tcp_t *conn;
  lwiperf_settings_t  *settings_ptr;
  lwiperf_settings_t  Settings;
  u32_t TestTime = 0;

    conn = &ctrlInfo_ptr->lwiperf_state;
	conn->time_started = sys_now();
    if (state == DATA_SENDING_INIT ) {
    	TestTime = lwiperf_list_update_client(tpcb->remote_port, tcp_five_PacketCountGet, 0);
    	if (TestTime == 0)
    		TestTime = 1000;
		ctrlInfo_ptr->TestTime = TestTime;
		ctrlInfo_ptr->StartTime = ctrlInfo_ptr->Interval_StartTime = xTaskGetTickCount();
		conn->expire_time = ctrlInfo_ptr->StartTime + (ctrlInfo_ptr->TestTime);
#if IPERF3_DEBUG
		printf("\n TestTime:%d, StartTime:%d, expire_time:%d\n", ctrlInfo_ptr->TestTime, ctrlInfo_ptr->StartTime, conn->expire_time);
#endif
		ctrlInfo_ptr->RunTime = 0;
		ctrlInfo_ptr->txCount = 0;
		ctrlInfo_ptr->PacketsTxed = 0;
		ctrlInfo_ptr->PacketsRxed = 0;
		ctrlInfo_ptr->BytesTxed = 0;
		ctrlInfo_ptr->BytesRxed = 0;
		Settings.flags = 0;
		Settings.num_threads = htonl(1);
		Settings.remote_port = htonl(tpcb->remote_port);
		/* TODO: implement passing duration/amount of bytes to transfer */
		Settings.amount = htonl((u32_t)-1000);
		settings_ptr = &Settings;
		IP3_TCP_Tx_Finish_Flag = 0;

		memcpy(&conn->settings, settings_ptr, sizeof(lwiperf_settings_t));
#if IPERF3_DEBUG
		printf("\n reverse start : %d\n", tcp_five_PacketCountGet);
#endif
    }
#if IPERF3_DEBUG
    printf("\n Data sending Timeout!!! bytes_transferred:%d, xRemainingTime:%d \n",
    		conn->bytes_transferred, ctrlInfo_ptr->TestTime);
#endif
	return lwiperf_tcp_client_send_more(ctrlInfo_ptr);
}

static err_t iperf3_server_control(void *arg, struct tcp_pcb *tpcb, struct pbuf *p)
{
  lwiperf3_state_tcp_t *conn;
  ctrl3_info_t *ctrlInfo_ptr;
  u16_t txlen = 0;
  void *txptr;
  u8_t apiflags;
  u16_t packet_len=0;
  u16_t flag = 0;
  err_t err = ERR_OK;
  u32_t conn_state = 0;
  const u8_t *pay = NULL;
  u32_t ulLength = 0;
  char test_result[200];
  unsigned long long bytes = 0;
  u32_t ret = 1;
  ULONG	current_time = 0;

  u32_t state = get_iperf_server_status(tpcb->remote_port);
  if(state == 200) {
	  return ERR_MEM;
  }

  ctrlInfo_ptr = (ctrl3_info_t *)arg;

  if (p != NULL) {
    pay = (const u8_t *)p->payload;
    packet_len = p->len;
  }
  if ((state == DATA_SENDING_INIT) || (state == DATA_SENDING_STATE)) {
#if IPERF3_DEBUG
    printf("\n Data sending state!!! \n");
#endif
    return iperf3_server_send_data(arg, tpcb, state);
    /*TCP data transfer start*/
  } else if (state == DATA_RESULT_STATE) {
#if IPERF3_DEBUG
    printf("\n DATA_RESULT_STATE !!! \n");
#endif
    tcp_send_fin(tpcb);
    iperf3_server_reverse = 0;
    lwiperf3_list_remove(tpcb);
  }
  
  ret = iperf3_tcp_server_data(tpcb, tpcb->remote_port, (const char*)pay, packet_len, ctrlInfo_ptr->BytesRxed);
  if (ERR_OK == ret)
    return ERR_MEM;

#if IPERF3_DEBUG
  printf("\nline:%d, status:%d, port :%d\n", __LINE__, state, tpcb->remote_port);
#endif

  apiflags = TCP_WRITE_FLAG_COPY;

  if (state == 100) {
	  return ERR_OK;
  }
  if (state == 9) {
	  current_time = xTaskGetTickCount();
	  print_iperf3(ctrlInfo_ptr->rxCount,
			       ctrlInfo_ptr->BytesTxed,
				   ctrlInfo_ptr->StartTime,
				   current_time,
				   ctrlInfo_ptr->StartTime,
				   ctrlInfo_ptr->version,
				   &ctrlInfo_ptr->remote_ip,
				   ctrlInfo_ptr->remote_port,
				   ctrlInfo_ptr->bandwidth_format,
				   PRINT_TYPE_TCP_TX_TOL);
	  iperf_server_update_status(tpcb->remote_port, 10);
	  lwiperf3_list_remove(tpcb);
	  //IP3_TCP_Rx_Restart_Flag = 1;
	  //IP3_TCP_Rx_Finish_Flag = 0;
	  iperf3_server_reverse = 0;
  }
  switch (state) {
    case eTCP_8_WaitDisplay:
#if IPERF3_DEBUG
      printf("\n eTCP_7_WaitTransfer \n");
#endif
      flag = 14;
      txlen =1;
      iperf_server_update_status(tpcb->remote_port, 9);
      break;
    case eTCP_6_WaitDone:
    uint32_t len;
    uint32_t rev;
#if IPERF3_DEBUG
      printf("\n eTCP_6_WaitTransfer \n");
#endif
      bytes = lwiperf_list_update_client(tpcb->remote_port, tcp_Three_PacketCountGet, 0);
      tcp_send_empty_ack(tpcb);
      if (bytes)
          bytes = ctrlInfo_ptr->BytesTxed;
      else
          bytes = ctrlInfo_ptr->BytesRxed;
      len = sprintf( test_result, "{""\"cpu_util_total\":0,""\"cpu_util_user\":0,""\"cpu_util_system\":0,"
                     "\"sender_has_retransmits\":-1,""\"streams\":[""{""\"id\":1,""\"bytes\":%d,""\"retransmits\":-1,"
                     "\"jitter\":0,""\"errors\":0,""\"packets\":0""}""]""}", bytes);
      memset(test_result, 0, 200);
      rev = ((len >> 24) & 0xFF) |
            ((len >> 8)  & 0xFF00) |
            ((len << 8)  & 0xFF0000) |
            ((len << 24) & 0xFF000000);
      *((uint32_t *) test_result) = rev;
      txlen = 4;
      ulLength  = txlen;
#if IPERF3_DEBUG
      printf("\nLength = %d , size:%d val :%2x:%2x:%2x:%2x\n", len, txlen,
              test_result[0],test_result[1],test_result[2],test_result[3]);
#endif
      iperf_server_update_status(tpcb->remote_port, 7);
      break;
    case eTCP_7_WaitTransfer:
      bytes = lwiperf_list_update_client(tpcb->remote_port, tcp_Three_PacketCountGet, 0);
      tcp_send_empty_ack(tpcb);
#if IPERF3_DEBUG
      printf("\n eTCP_7_WaitTransfer \n");
#endif
      if (bytes)
    	  bytes = ctrlInfo_ptr->BytesTxed;
      else
    	  bytes = ctrlInfo_ptr->BytesRxed;
      ulLength = sprintf( test_result, "{""\"cpu_util_total\":0,""\"cpu_util_user\":0,""\"cpu_util_system\":0,"
				"\"sender_has_retransmits\":-1,""\"streams\":[""{""\"id\":1,""\"bytes\":%d,""\"retransmits\":-1,"
				"\"jitter\":0,""\"errors\":0,""\"packets\":0""}""]""}", bytes);
#if IPERF3_DEBUG
      printf("\nTest Result: %s\n", test_result);
#endif
      txlen = ulLength;
      iperf_server_update_status(tpcb->remote_port, 8);
      break;
    case eTCP_5_WaitHeader2:
#if IPERF3_DEBUG
      printf("\n eTCP_5_WaitHeader2 \n");
#endif
      flag = 13;
      txlen =1;
      iperf_server_update_status(tpcb->remote_port, 6);
      break;
    case eTCP_4_WaitCount2:
      flag = 2;
      txlen =1;
#if IPERF3_DEBUG
      printf("\n eTCP_4_WaitCount2 \n");
#endif
      iperf_server_update_status(tpcb->remote_port, 100);
      break;
    case eTCP_3_WaitOneTwo:
      flag = 1;
      txlen =1;
#if IPERF3_DEBUG
      printf("\n eTCP_3_WaitOneTwo \n");
#endif
      iperf_server_update_status(tpcb->remote_port, 4);
      break;
    case eTCP_0_WaitName:
      flag = 9;
      txlen =1;
#if IPERF3_DEBUG
      printf("\n eTCP_0_WaitName \n");
#endif
      iperf_server_update_status(tpcb->remote_port, 1);
      break;
    case eTCP_1_WaitCount:
      tcp_send_empty_ack(tpcb);
#if IPERF3_DEBUG
      printf("\n eTCP_1_WaitCount \n");
#endif
      iperf_server_update_status(tpcb->remote_port, 2);
      break;
    case eTCP_2_WaitHeader:
      if (packet_len > 0) {
        char *pcPtr;
        strncpy(test_result, (const char*)pay, packet_len);
        test_result[packet_len] = '\0';
#if IPERF3_DEBUG
        printf( "Control string: %s\n", test_result);
#endif
        conn->base.bIsControl = 1;
        for( pcPtr = test_result; *pcPtr; pcPtr++ ) {
          if( strncmp( pcPtr, "\"reverse\"", 9 ) == 0 ) {
             if (pcPtr[ 10 ] == 't') {
#if IPERF3_DEBUG
               printf("\nControl found:\n");
#endif
               iperf3_server_reverse = 1;
               conn->base.bReverse = 1;
             }
          } else if( strncmp( pcPtr, "\"num\"", 5 ) == 0 ) {
            uint32_t ulAmount;
            if( sscanf( pcPtr + 6, "%u", &( ulAmount ) ) > 0 ) {
               conn->base.ulAmount = ulAmount;
#if IPERF3_DEBUG
                printf("\nControl ulAmount:%d\n", ulAmount);
#endif
            }
          } else if( strncmp( pcPtr, "\"time\"", 6 ) == 0 ) {
            uint32_t ulSeconds;
            if( sscanf( pcPtr + 7, "%u", &( ulSeconds ) ) > 0 ) {
              conn->base.xRemainingTime = ulSeconds * 1000ul;
#if IPERF3_DEBUG
              printf("\nControl xRemainingTime:%d\n", conn->base.xRemainingTime);
#endif
            }
          }
        }
      }
      iperf_server_update_control(tpcb->remote_port, conn->base.bReverse,
    		  conn->base.ulAmount, conn->base.xRemainingTime);
      flag = 10;
      txlen =1;
#if IPERF3_DEBUG
      printf("\n eTCP_2_WaitHeader \n");
#endif
      break;
     default:
    return ERR_MEM;
  }

  if (ulLength > 0) {
    txptr = test_result;
  } else {
    txptr = &flag;
  }
  if (txlen == 0)
    return ERR_OK;

  err = tcp_write(tpcb, txptr, txlen, apiflags);
  if (err ==  ERR_MEM) {
    printf("\ntcp_write ERROR\n");
  }
  return err;
}

static err_t
lwiperf_tcp_client_sent(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
	ctrl3_info_t	*ctrlInfo_ptr = (ctrl3_info_t *)arg;
	lwiperf3_state_tcp_t *conn = &ctrlInfo_ptr->lwiperf_state;
#if IPERF3_DEBUG
	IPERF3_MSG(CYAN_COLOR "[%s] >>>>>> len: %d port:%d\n" CLEAR_COLOR, __func__, len, tpcb->remote_port);
#endif // __DEBUG_IPERF3__
	u32_t state = get_iperf_server_status(conn->conn_pcb->remote_port);

	if ((state == DATA_SENDING_INIT) || (state == DATA_SENDING_STATE)) {
#if IPERF3_DEBUG
	    printf("\n Data sending state init!!! port:%d\n", conn->conn_pcb->remote_port);
#endif
	    tpcb = conn->conn_pcb;
#if IPERF3_DEBUG
	IPERF3_MSG(CYAN_COLOR "[%s] >>>>>> iperf3_server port:%d\n" CLEAR_COLOR, __func__, tpcb->remote_port);
#endif // __DEBUG_IPERF3__
	    return iperf3_server_send_data(arg, tpcb, state);
	    /*TCP data transfer start*/
	} else {
		return ERR_OK;
	}
}
#endif /*New chages*/
/* iPerf3 TCP Server */
static err_t
lwiperf_tcp_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *pbuf_ptr, err_t err)
{
#ifdef WATCH_DOG_ENABLE
//	extern void watch_dog_kicking(UINT rescale_time);
#endif	/* WATCH_DOG_ENABLE */
	u8_t tmp;
	u16_t tot_len;
	u32_t packet_idx;
	struct pbuf *q;
	lwiperf3_state_tcp_t *conn;
	ULONG		current_time = 0;
	ctrl3_info_t *ctrlInfo_ptr;
	int		gap = 0;

	ctrlInfo_ptr = (ctrl3_info_t *)arg;
	conn = &ctrlInfo_ptr->lwiperf_state;

#if 0 //def __DEBUG_IPERF3__
	IPERF3_MSG(CYAN_COLOR "[%s] >>>>>>\n" CLEAR_COLOR, __func__);
#endif // __DEBUG_IPERF3__

#ifdef WATCH_DOG_ENABLE
//	watch_dog_kicking(5);	/* 5 SEC */
#endif	/* WATCH_DOG_ENABLE */

	LWIP_ASSERT("pcb mismatch", conn->conn_pcb == tpcb);
	LWIP_UNUSED_ARG(tpcb);

	if (err != ERR_OK) {
#ifdef __DEBUG_IPERF3__
		IPERF3_MSG(RED_COLOR "[%s:%d] tcp close by abort_remote \n" CLEAR_COLOR, __func__, __LINE__);
#endif // __DEBUG_IPERF3__
		lwiperf_tcp_close(ctrlInfo_ptr, LWIPERF_TCP_ABORTED_REMOTE);
		return ERR_OK;
	}
	if (pbuf_ptr == NULL) {
		//IPERF3_MSG(RED_COLOR "[%s:%d] ERROR???\n" CLEAR_COLOR, __func__, __LINE__);
		lwiperf_tcp_close(ctrlInfo_ptr, LWIPERF_TCP_DONE_SERVER);

#if defined (__ENABLE_TRANSFER_MNG_TEST_MODE__)
		g_atcmd_tr_mng_test_flag = pdTRUE;
#endif // __ENABLE_TRANSFER_MNG_TEST_MODE__

		return ERR_OK;
	}
	tot_len = pbuf_ptr->tot_len;
#if defined (__ENABLE_TRANSFER_MNG_TEST_MODE__)
	PUTS_ATCMD(pbuf_ptr->payload, pbuf_ptr->len);
#endif // __ENABLE_TRANSFER_MNG_TEST_MODE__

#if 0 //def __DEBUG_IPERF3__
	IPERF3_MSG(CYAN_COLOR "[%s] >>>>>> tot_len: %d\n" CLEAR_COLOR, __func__, tot_len);
#endif // __DEBUG_IPERF3__

	conn->poll_count = 0;

	if (iperf3_server_control(arg, tpcb, pbuf_ptr) == ERR_OK) {
		pbuf_free(pbuf_ptr);
		return ERR_OK;
	}
	if ((!conn->have_settings_buf) || ((conn->bytes_transferred - 24) % (1024 * 128) == 0)) {
		/* wait for 24-byte header */
		if (pbuf_ptr->tot_len < sizeof(lwiperf_settings_t)) {
#ifdef __DEBUG_IPERF3__
			IPERF3_MSG(RED_COLOR "[%s:%d] tcp close by abort_local \n" CLEAR_COLOR, __func__, __LINE__);
#endif // __DEBUG_IPERF3__
			lwiperf_tcp_close(ctrlInfo_ptr, LWIPERF_TCP_ABORTED_LOCAL_DATAERROR);
			pbuf_free(pbuf_ptr);
			return ERR_OK;
		}

		if (!conn->have_settings_buf) {
			if (pbuf_copy_partial(pbuf_ptr, &conn->settings, sizeof(lwiperf_settings_t), 0) != sizeof(lwiperf_settings_t)) {
#ifdef __DEBUG_IPERF3__
				IPERF3_MSG(RED_COLOR "[%s:%d] tcp close by abort_local \n" CLEAR_COLOR, __func__, __LINE__);
#endif // __DEBUG_IPERF3__
				lwiperf_tcp_close(ctrlInfo_ptr, LWIPERF_TCP_ABORTED_LOCAL);
				pbuf_free(pbuf_ptr);
				return ERR_OK;
			}
			conn->have_settings_buf = 1;
			if (conn->settings.flags & PP_HTONL(LWIPERF_FLAGS_ANSWER_TEST)) {
				IPERF3_MSG(RED_COLOR "[%s:%d] ERROR???\n" CLEAR_COLOR, __func__, __LINE__);
			}
		} else {
			if (conn->settings.flags & PP_HTONL(LWIPERF_FLAGS_ANSWER_TEST)) {
				if (pbuf_memcmp(pbuf_ptr, 0, &conn->settings, sizeof(lwiperf_settings_t)) != 0) {
#ifdef __DEBUG_IPERF3__
					IPERF3_MSG(RED_COLOR "[%s:%d] tcp close by abort_local \n" CLEAR_COLOR, __func__, __LINE__);
#endif // __DEBUG_IPERF3__
					lwiperf_tcp_close(ctrlInfo_ptr, LWIPERF_TCP_ABORTED_LOCAL_DATAERROR);
					pbuf_free(pbuf_ptr);
					return ERR_OK;
				}
			}
		}

		conn->bytes_transferred += sizeof(lwiperf_settings_t);

		if (conn->bytes_transferred <= 24) {
			/* IPERF3 not waiting
			conn->time_started = sys_now();
			tcp_recved(tpcb, pbuf_ptr->tot_len);
			*/
			pbuf_free(pbuf_ptr);
			return ERR_OK;
		}

		conn->next_num = 4;		/* 24 bytes received... */
		tmp = pbuf_remove_header(pbuf_ptr, 24);
		LWIP_ASSERT("pbuf_remove_header failed", tmp == 0);
		LWIP_UNUSED_ARG(tmp);	/* for LWIP_NOASSERT */
	}

	packet_idx = 0;
	for (q = pbuf_ptr; q != NULL; q = q->next) {
#if LWIPERF_CHECK_RX_DATA
		const u8_t *payload = (const u8_t *)q->payload;
		u16_t i;
		for (i = 0; i < q->len; i++) {
			u8_t val = payload[i];
			u8_t num = val - '0';
			if (num == conn->next_num) {
				conn->next_num++;
				if (conn->next_num == 10) {
					conn->next_num = 0;
				}
			} else {
#ifdef __DEBUG_IPERF3__
				IPERF3_MSG(RED_COLOR "[%s:%d] tcp close by abort_local \n" CLEAR_COLOR, __func__, __LINE__);
#endif // __DEBUG_IPERF3__
				lwiperf_tcp_close(ctrlInfo_ptr, LWIPERF_TCP_ABORTED_LOCAL_DATAERROR);
				pbuf_free(pbuf_ptr);
				return ERR_OK;
			}
		}
#endif
		packet_idx += q->len;
	}
	LWIP_ASSERT("count mismatch", packet_idx == pbuf_ptr->tot_len);
	conn->bytes_transferred += packet_idx;

	tcp_recved(tpcb, tot_len);
	pbuf_free(pbuf_ptr);

	/* Update the counter. */
	ctrlInfo_ptr->PacketsRxed++;
	ctrlInfo_ptr->BytesRxed += packet_idx;

	current_time = xTaskGetTickCount();

	if (ctrlInfo_ptr->Interval > 0) {
		/* Â´Ã™?Â½ Ã�Â¤Â°Â¢ÃƒÃŠ Â±Ã®Ã�Ã¶ Â½ÃƒÂ°Â£ Â°Ã¨Â»Ãª */
		gap = (int)(ctrlInfo_ptr->Interval - ((ctrlInfo_ptr->Interval_StartTime - ctrlInfo_ptr->StartTime) % configTICK_RATE_HZ));

		ctrlInfo_ptr->Interval_BytesRxed += packet_idx;;

		if ((ctrlInfo_ptr->Interval_StartTime < current_time) && (ctrlInfo_ptr->Interval_StartTime + (UINT)gap <= current_time)) {
			//IPERF3_MSG("gap=%03d ", gap);

			ctrlInfo_ptr->rxCount++;

			/* print TCP Rx Interval */
			print_iperf3(ctrlInfo_ptr->rxCount,
				ctrlInfo_ptr->Interval_BytesRxed,
				ctrlInfo_ptr->StartTime,
				current_time,
				ctrlInfo_ptr->Interval_StartTime,
				ctrlInfo_ptr->version,
				&ctrlInfo_ptr->remote_ip,
				ctrlInfo_ptr->remote_port,
				ctrlInfo_ptr->bandwidth_format,
				PRINT_TYPE_TCP_RX_INT);

			ctrlInfo_ptr->Interval_BytesRxed = 0;
			ctrlInfo_ptr->Interval_StartTime = current_time;	/* interval start time ÃƒÃŠÂ±Ã¢ÃˆÂ­ */
#ifdef __LIB_IPERF_PRINT_MIB__
			iperf_print_mib(ctrlInfo_ptr->mib_flag);
#endif /* __LIB_IPERF_PRINT_MIB__ */
		}
	}

	return ERR_OK;
}

/** Error callback, iPerf3 tcp session aborted */
static void
lwiperf_tcp_err(void *arg, err_t err)
{
#ifdef __DEBUG_IPERF3__
	IPERF3_MSG(RED_COLOR "[%s] >>>>>> err=%d\n" CLEAR_COLOR, __func__, err);
#endif // __DEBUG_IPERF3__
	ctrl3_info_t *ctrlInfo_ptr = (ctrl3_info_t *)arg;

	LWIP_UNUSED_ARG(err);

	if (ctrlInfo_ptr->StartTime == 0)
	{
		IPERF3_MSG("\n iPerf3 connection failed.(%s, err=%d)\n", ipaddr_ntoa(&ctrlInfo_ptr->remote_ip), err);
	}

	if (err == ERR_ABRT)
		return;

	/** In TCP Client, if Remote is unopened and halt, In next TCP TX Test hang is happened.
	 * In TCP ERR_RST Case , only report TCP Test result, TCP close is executed by LWIP Stack
	 */
	if (err == ERR_RST) {
		lwiperf_tcp_result_report(ctrlInfo_ptr);
		return;
	}

#ifdef __DEBUG_IPERF3__
	IPERF3_MSG(RED_COLOR "[%s:%d] tcp close by abort_remote \n" CLEAR_COLOR, __func__, __LINE__);
#endif // __DEBUG_IPERF3__
	lwiperf_tcp_close(ctrlInfo_ptr, LWIPERF_TCP_ABORTED_REMOTE);
}

#ifdef __IPERF3_API_MODE_ALTCP__
/** TCP poll callback, try to send more data */
static err_t
lwiperf_altcp_poll(void *arg, struct altcp_pcb *tpcb)
{
	ctrl3_info_t *ctrlInfo_ptr = (ctrl3_info_t *)arg;
	lwiperf3_state_tcp_t *conn = &ctrlInfo_ptr->lwiperf_state;

#ifdef __DEBUG_IPERF3__
	IPERF3_MSG(CYAN_COLOR "[%s] >>>>>>\n" CLEAR_COLOR, __func__);
#endif // __DEBUG_IPERF3__

	LWIP_ASSERT("pcb mismatch", conn->conn_alpcb == tpcb);
	LWIP_UNUSED_ARG(tpcb);
	if (++conn->poll_count >= LWIPERF_TCP_MAX_IDLE_SEC) {
#ifdef __DEBUG_IPERF3__
		IPERF3_MSG(RED_COLOR "[%s:%d] tcp close by abort_local \n" CLEAR_COLOR, __func__, __LINE__);
#endif
		lwiperf_tcp_close(ctrlInfo_ptr, LWIPERF_TCP_ABORTED_LOCAL);
		return ERR_OK; /* lwiperf_tcp_close frees conn */
	}

	if (!conn->base.server) {
		lwiperf_tcp_client_send_more(ctrlInfo_ptr);
	}

	return ERR_OK;
}

static err_t
lwiperf_altcp_rx_poll(void *arg, struct altcp_pcb *tpcb)
{
	ctrl3_info_t *ctrlInfo_ptr = (ctrl3_info_t *)arg;
	lwiperf3_state_tcp_t *conn = &ctrlInfo_ptr->lwiperf_state;

#ifdef __DEBUG_IPERF3__
	IPERF3_MSG(CYAN_COLOR "[%s] >>>>>>\n" CLEAR_COLOR, __func__);
#endif // __DEBUG_IPERF3__

	LWIP_ASSERT("pcb mismatch", conn->conn_alpcb == tpcb);
	LWIP_UNUSED_ARG(tpcb);
	if (++conn->poll_count >= LWIPERF_TCP_RX_MAX_IDLE_SEC) {
#ifdef __DEBUG_IPERF3__
		IPERF3_MSG(RED_COLOR "[%s:%d] tcp close by abort_local \n" CLEAR_COLOR, __func__, __LINE__);
#endif // __DEBUG_IPERF3__
		lwiperf_tcp_close(ctrlInfo_ptr, LWIPERF_TCP_ABORTED_LOCAL);
		return ERR_OK; /* lwiperf_tcp_close frees conn */
	}
#if 0
	if (!conn->base.server) {
		lwiperf_tcp_client_send_more(ctrlInfo_ptr);
	}
#endif
	return ERR_OK;
}
#endif // __IPERF3_API_MODE_ALTCP__

static err_t lwiperf_tcp_poll(void *arg, struct tcp_pcb *tpcb)
{
	ctrl3_info_t *ctrlInfo_ptr = (ctrl3_info_t *)arg;
	lwiperf3_state_tcp_t *conn = &ctrlInfo_ptr->lwiperf_state;

#ifdef __DEBUG_IPERF3__
	IPERF3_MSG(CYAN_COLOR "[%s] >>>>>>\n" CLEAR_COLOR, __func__);
#endif // __DEBUG_IPERF3__

	LWIP_ASSERT("pcb mismatch", conn->conn_pcb == tpcb);
	LWIP_UNUSED_ARG(tpcb);
	iperf3_server_control(arg, tpcb, NULL);
	if (++conn->poll_count >= LWIPERF_TCP_MAX_IDLE_SEC) {
#ifdef __DEBUG_IPERF3__
		IPERF3_MSG(RED_COLOR "[%s:%d] tcp close by abort_local \n" CLEAR_COLOR, __func__, __LINE__);
#endif // __DEBUG_IPERF3__
		//lwiperf_tcp_close(ctrlInfo_ptr, LWIPERF_TCP_ABORTED_LOCAL);
		return ERR_OK; /* lwiperf_tcp_close frees conn */
	}

	if (thread_tcp_tx_iperf3[ctrlInfo_ptr->pair_no].state == IPERF_TX_THD_STATE_REQ_TERM) {
#ifdef __DEBUG_IPERF3__
		IPERF3_MSG("[%s]Get request to terminate thread_tcp_tx_iperf3(%d)\n",
				  __func__, ctrlInfo_ptr->pair_no);
#endif // __DEBUG_IPERF3__
		lwiperf_tcp_close(ctrlInfo_ptr, LWIPERF_TCP_ABORTED_LOCAL);
		return ERR_OK; /* lwiperf_tcp_close frees conn */
	}

	if (!conn->base.server) {
		lwiperf_tcp_client_send_more(ctrlInfo_ptr);
	}

	return ERR_OK;
}

err_t lwiperf_tcp_rx_poll(void *arg, struct tcp_pcb *tpcb)
{
	ctrl3_info_t *ctrlInfo_ptr = (ctrl3_info_t *)arg;
	lwiperf3_state_tcp_t *conn = &ctrlInfo_ptr->lwiperf_state;

#ifdef __DEBUG_IPERF3__
	IPERF3_MSG(CYAN_COLOR "[%s] >>>>>> port :%d\n" CLEAR_COLOR, __func__, tpcb->remote_port);
#endif // __DEBUG_IPERF3__

	iperf3_server_control(arg, tpcb, NULL);

	LWIP_ASSERT("pcb mismatch", conn->conn_pcb == tpcb);
	LWIP_UNUSED_ARG(tpcb);
	if (++conn->poll_count >= LWIPERF_TCP_RX_MAX_IDLE_SEC) {
#ifdef __DEBUG_IPERF3__
		IPERF3_MSG(RED_COLOR "[%s:%d] tcp close by abort_local \n" CLEAR_COLOR, __func__, __LINE__);
#endif // __DEBUG_IPERF3__
		//lwiperf_tcp_close(ctrlInfo_ptr, LWIPERF_TCP_ABORTED_LOCAL);
#ifdef USING_LIST_DELETE	/* F_F_S */
		//lwiperf3_list_remove(tpcb);
#endif // USING_LIST_DELETE
		return ERR_OK; /* lwiperf_tcp_close frees conn */
	}
#if 0
	if (!conn->base.server) { // TCP Client
		lwiperf_tcp_client_send_more(ctrlInfo_ptr);
	}
#endif
	return ERR_OK;
}

#ifdef __IPERF3_API_MODE_ALTCP__
/** This is called when a new client connects for an iPerf3 tcp session */
static err_t
lwiperf_altcp_accept(void *arg, struct altcp_pcb *newpcb, err_t err)
{
	lwiperf3_state_tcp_t *state_tcp, *conn;
	ctrl3_info_t			*ctrlInfo_ptr;
	ip_addr_t			*local_ip;
	u16_t				local_port;
	ip_addr_t			*remote_ip;
	u16_t				remote_port;

	ctrlInfo_ptr = (ctrl3_info_t *)arg;
	conn = &ctrlInfo_ptr->lwiperf_state;
	state_tcp = &ctrlInfo_ptr->lwiperf_state;

#ifdef __DEBUG_IPERF3__
	IPERF3_MSG(CYAN_COLOR "[%s] >>>>>>\n" CLEAR_COLOR, __func__);
#endif // __DEBUG_IPERF3__

	if ((err != ERR_OK) || (newpcb == NULL) || (arg == NULL)) {
#ifdef __DEBUG_IPERF3__
		IPERF3_MSG(RED_COLOR "[%s] >>>>>> err:%d\n" CLEAR_COLOR, __func__, err);
#endif // __DEBUG_IPERF3__
		return ERR_VAL;
	}

	//state_tcp = (lwiperf3_state_tcp_t *)arg;
	LWIP_ASSERT("invalid session", state_tcp->base.server);
	LWIP_ASSERT("invalid listen pcb", state_tcp->server_alpcb != NULL);
#ifndef USE_TCP_SERVER_LISTEN
	LWIP_ASSERT("invalid conn pcb", state_tcp->conn_alpcb == NULL);
#else
	if(state_tcp->conn_alpcb != NULL)
	{
		tcp_abort(state_tcp->conn_alpcb);
		state_tcp->conn_alpcb = NULL;
	}
#endif

	conn->base.tcp = 1;
	conn->base.server = 1;
	conn->base.related_master_state = &state_tcp->base;
	conn->conn_alpcb = newpcb;
	conn->time_started = sys_now();
	conn->report_fn = state_tcp->report_fn;
	conn->report_arg = state_tcp->report_arg;

	ctrlInfo_ptr->PacketsRxed = 0;
	ctrlInfo_ptr->BytesRxed = 0;
	ctrlInfo_ptr->StartTime = 0;
	ctrlInfo_ptr->RunTime = 0;

	/* setup the tcp rx connection */
	altcp_arg(newpcb, ctrlInfo_ptr);
	altcp_recv(newpcb, lwiperf_altcp_recv);
	altcp_poll(newpcb, lwiperf_altcp_rx_poll, 2U);
	altcp_err(conn->conn_alpcb, lwiperf_tcp_err);

	ctrlInfo_ptr->StartTime = ctrlInfo_ptr->Interval_StartTime = xTaskGetTickCount();

	IPERF3_MSG("\n\n[TCP] Receive Test (Server)\n");

	local_ip = altcp_get_ip(newpcb, 1);
	local_port = altcp_get_port(newpcb, 1);

	IPERF3_MSG(CYAN_COLOR" Local IP: %s:%d\n" CLEAR_COLOR, ipaddr_ntoa(local_ip), local_port);

	remote_ip = altcp_get_ip(newpcb, 0);
	remote_port = altcp_get_port(newpcb, 0);

	IPERF3_MSG(CYAN_COLOR" Remote IP: %s:%d\n" CLEAR_COLOR, ipaddr_ntoa(remote_ip), remote_port);

	ip_addr_copy_from_ip4(ctrlInfo_ptr->local_ip, *local_ip.u_addr.ip4);
	ctrlInfo_ptr->local_port = local_port;
	ip_addr_copy_from_ip4(ctrlInfo_ptr->remote_ip, *remote_ip.u_addr.ip4);
	ctrlInfo_ptr->remote_port = remote_port;

	if (ctrlInfo_ptr->Interval > 0) {
		IPERF3_MSG(ANSI_BCOLOR_GREEN ANSI_COLOR_BLUE
			MSG_HEAD_TCP_RX MSG_TITLE_INTERVAL
			ANSI_BCOLOR_BLACK ANSI_COLOR_WHITE ANSI_NORMAL "\n",
			"[Src IP:Port]");
	}

#ifdef USING_LIST_DELETE	/* F_F_S */
	if (state_tcp->specific_remote) {
		/* this listener belongs to a client, so make the client the master of the newly created connection */
		conn->base.related_master_state = state_tcp->base.related_master_state;
		/* if dual mode or (tradeoff mode AND client is done): close the listener */
		if (!state_tcp->client_tradeoff_mode || !lwiperf_list_find(state_tcp->base.related_master_state)) {
			/* prevent report when closing: this is expected */
			state_tcp->report_fn = NULL;
#ifdef __DEBUG_IPERF3__
			IPERF3_MSG(RED_COLOR "[%s:%d] tcp close by abort_local \n" CLEAR_COLOR, __func__, __LINE__);
#endif // __DEBUG_IPERF3__
			lwiperf_tcp_close(ctrlInfo_ptr, LWIPERF_TCP_ABORTED_LOCAL);
		}
	}

	lwiperf_list_add(&conn->base);
#endif // USING_LIST_DELETE
	return ERR_OK;
}
#endif // __IPERF3_API_MODE_ALTCP__

/** This is called when a new client connects for an iPerf3 tcp session */
/* iPerf3 TCP Server */
static err_t
lwiperf3_tcp_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
	lwiperf3_state_tcp_t *state_tcp, *conn;
	ctrl3_info_t		*ctrlInfo_ptr;

	ctrlInfo_ptr = (ctrl3_info_t *)arg;
	conn = &ctrlInfo_ptr->lwiperf_state;
	state_tcp = &ctrlInfo_ptr->lwiperf_state;

#ifdef __DEBUG_IPERF3__
	IPERF3_MSG(CYAN_COLOR "[%s] >>>>>>\n" CLEAR_COLOR, __func__);
#endif // __DEBUG_IPERF3__

	if ((err != ERR_OK) || (newpcb == NULL) || (arg == NULL)) {
#ifdef __DEBUG_IPERF3__
		IPERF3_MSG(RED_COLOR "[%s] >>>>>> err:%d\n" CLEAR_COLOR, __func__, err);
#endif // __DEBUG_IPERF3__
		return ERR_VAL;
	}

	LWIP_ASSERT("invalid session", state_tcp->base.server);
	LWIP_ASSERT("invalid listen pcb", state_tcp->server_pcb != NULL);
#if 0
#ifndef USE_TCP_SERVER_LISTEN	
	LWIP_ASSERT("invalid conn pcb", state_tcp->conn_pcb == NULL);
#else

	if (state_tcp->conn_pcb != NULL)
	{
		return ERR_VAL;
	}
#endif // USE_TCP_SERVER_LISTEN
#endif
	if (state_tcp->specific_remote) {
		LWIP_ASSERT("state_tcp->base.related_master_state != NULL", state_tcp->base.related_master_state != NULL);
		if (!ip_addr_cmp(&newpcb->remote_ip, &state_tcp->remote_addr)) {
			/* this listener belongs to a client session, and this is not the correct remote */
			return ERR_VAL;
		}
	}
	else {
#ifndef USE_TCP_SERVER_LISTEN
		LWIP_ASSERT("state_tcp->base.related_master_state == NULL", state_tcp->base.related_master_state == NULL);
#endif
	}

	conn->base.tcp = 1;
	conn->base.server = 1;
	conn->base.state = 0;
	memcpy(&conn->base.conn_pcb, newpcb, sizeof(struct tcp_pcb));
	memset(state_tcp->base.data, 0, 37);
	conn->base.related_master_state = &state_tcp->base;
	conn->conn_pcb = newpcb;
	conn->time_started = sys_now();
	conn->report_fn = state_tcp->report_fn;
	conn->report_arg = state_tcp->report_arg;

	ctrlInfo_ptr->rxCount = 0;
	ctrlInfo_ptr->PacketsRxed = 0;
	ctrlInfo_ptr->BytesRxed = 0;
	ctrlInfo_ptr->StartTime = 0;
	ctrlInfo_ptr->RunTime = 0;

	/* setup the tcp rx connection */
	tcp_arg(newpcb, ctrlInfo_ptr);
	tcp_recv(newpcb, lwiperf_tcp_recv);
	if(iperf3_server_reverse == 0) {
		tcp_poll(newpcb, lwiperf_tcp_rx_poll, 4U);
	} else {
#if IPERF3_DEBUG
	printf("\nlwiperf3_tcp_accept line:%d\n",__LINE__);
#endif
		tcp_poll(newpcb, lwiperf_tcp_rx_poll, 1U);
		tcp_sent(newpcb, lwiperf_tcp_client_sent);
	}
	tcp_err(conn->conn_pcb, lwiperf_tcp_err);

#if IPERF3_DEBUG
	printf("\nlwiperf3_tcp_accept line:%d\n",__LINE__);
#endif
#ifdef __DEBUG_IPERF3__
	IPERF3_MSG("\n[%s] Accept info\n", __func__);
	IPERF3_MSG(" Local IP: %s:%d\n", ipaddr_ntoa(&conn->conn_pcb->local_ip), conn->conn_pcb->local_port);
	IPERF3_MSG(" Remote IP: %s:%d\n", ipaddr_ntoa(&conn->conn_pcb->remote_ip), conn->conn_pcb->remote_port);
#endif // __DEBUG_IPERF3__
#if LWIP_IPV4 && LWIP_IPV6
    if (ctrlInfo_ptr->version == IP_VERSION_V4) {
        ip_addr_copy_from_ip4(ctrlInfo_ptr->local_ip, conn->conn_pcb->local_ip.u_addr.ip4);
        ctrlInfo_ptr->local_port = conn->conn_pcb->local_port;
        ip_addr_copy_from_ip4(ctrlInfo_ptr->remote_ip, conn->conn_pcb->remote_ip.u_addr.ip4);
        ctrlInfo_ptr->remote_port = conn->conn_pcb->remote_port;
    } else if (ctrlInfo_ptr->version == IP_VERSION_V6) {
        ip_addr_copy_from_ip6(ctrlInfo_ptr->local_ip, conn->conn_pcb->local_ip.u_addr.ip6);
        ctrlInfo_ptr->local_port = conn->conn_pcb->local_port;
        ip_addr_copy_from_ip6(ctrlInfo_ptr->remote_ip, conn->conn_pcb->remote_ip.u_addr.ip6);
        ctrlInfo_ptr->remote_port = conn->conn_pcb->remote_port;
    }
#elif LWIP_IPV4
    ip_addr_copy_from_ip4(ctrlInfo_ptr->local_ip, conn->conn_pcb->local_ip);
    ctrlInfo_ptr->local_port = conn->conn_pcb->local_port;
    ip_addr_copy_from_ip4(ctrlInfo_ptr->remote_ip, conn->conn_pcb->remote_ip);
    ctrlInfo_ptr->remote_port = conn->conn_pcb->remote_port;
#elif LWIP_IPV6
    ip_addr_copy_from_ip6(ctrlInfo_ptr->local_ip, conn->conn_pcb->local_ip);
    ctrlInfo_ptr->local_port = conn->conn_pcb->local_port;
    ip_addr_copy_from_ip6(ctrlInfo_ptr->remote_ip, conn->conn_pcb->remote_ip);
    ctrlInfo_ptr->remote_port = conn->conn_pcb->remote_port;
#endif
#ifdef USING_LIST_DELETE	/* F_F_S */
	if (state_tcp->specific_remote) {
		/* this listener belongs to a client, so make the client the master of the newly created connection */
		conn->base.related_master_state = state_tcp->base.related_master_state;
		/* if dual mode or (tradeoff mode AND client is done): close the listener */
		if (!state_tcp->client_tradeoff_mode || !lwiperf_list_find(state_tcp->base.related_master_state)) {
			/* prevent report when closing: this is expected */
			state_tcp->report_fn = NULL;
#ifdef __DEBUG_IPERF3__
			IPERF3_MSG(RED_COLOR "[%s:%d] tcp close by abort_local \n" CLEAR_COLOR, __func__, __LINE__);
#endif // __DEBUG_IPERF3__
			lwiperf_tcp_close(ctrlInfo_ptr, LWIPERF_TCP_ABORTED_LOCAL);
		}
	}

	lwiperf_list_add(&conn->base);
#endif // USING_LIST_DELETE

	IPERF3_MSG("\n\n[TCP] Test (Server)\n");

	if (ctrlInfo_ptr->Interval > 0) {
		IPERF3_MSG(ANSI_BCOLOR_GREEN ANSI_COLOR_BLUE
			MSG_HEAD_TCP_RX MSG_TITLE_INTERVAL
			ANSI_BCOLOR_BLACK ANSI_COLOR_WHITE ANSI_NORMAL "\n",
			"[Src IP:Port]");
	}

	ctrlInfo_ptr->StartTime = ctrlInfo_ptr->Interval_StartTime = xTaskGetTickCount();

	return ERR_OK;
}

#define	RX_BUF_SIZE	1500
char Ip3rcvData[RX_BUF_SIZE];
#if 1
void *tcpRxTaskIp3(void *pvParameters)	/* TASK */
{
	int				peer_sock;
	int				rx_bytes;
	char			*rx_buf;
	ctrl3_info_t		*ctrlInfo_ptr;
	ULONG			current_time = 0;
	int				gap = 0;

	struct timeval	tv;
	int				res;
#ifdef MULTI_THREAD_MULTI_SOCKET
	UINT			taskNo;
	TaskHandle_t	taskHandler;
#endif
	/* Set the pointer. */
	ctrlInfo_ptr = (ctrl3_info_t *)pvParameters;
#ifdef MULTI_THREAD_MULTI_SOCKET
	//IPERF3_MSG("\n [TCP] Start receive Task (taskNo:%d)\n", ctrlInfo_ptr->taskNo);

	taskNo = ctrlInfo_ptr->taskNo;
	taskHandler = iperf_tcp_rx_task_handler[taskNo];
#endif
	peer_sock = ctrlInfo_ptr->socket_fd;

	rx_buf = &Ip3rcvData[0];
	memset((void *)rx_buf, 0, RX_BUF_SIZE);

	printf("\nFun: %s, line:%d\n",__func__,__LINE__);


	tv.tv_sec = MAX_TCP_RX_TOUT_SEC;
	tv.tv_usec = 0;
	res = setsockopt(peer_sock, SOL_SOCKET, SO_RCVTIMEO, (const void *)&tv, sizeof(struct timeval));

loop:
	rx_bytes = read(peer_sock, rx_buf, RX_BUF_SIZE);
	if (rx_bytes == -1) {	/* timeout */
		IPERF3_MSG(" rx_bytes : %d\n", rx_bytes);
		if (IP3_TCP_Rx_Finish_Flag) {
			goto end;
		}
	}
	else if (rx_bytes > 0) {

		ctrlInfo_ptr->PacketsRxed++;
		ctrlInfo_ptr->BytesRxed += (unsigned long long)rx_bytes;

		current_time = xTaskGetTickCount();

		if (ctrlInfo_ptr->Interval > 0) {
			gap = (int)(ctrlInfo_ptr->Interval - ((ctrlInfo_ptr->Interval_StartTime - ctrlInfo_ptr->StartTime) % configTICK_RATE_HZ));

			ctrlInfo_ptr->Interval_BytesRxed += (UINT)rx_bytes;;

			if ((ctrlInfo_ptr->Interval_StartTime < current_time) && (ctrlInfo_ptr->Interval_StartTime + (UINT)gap <= current_time)) {
				//IPERF3_MSG("gap=%03d ", gap);
				ctrlInfo_ptr->rxCount++;

				/* print TCP Rx Interval */
				print_iperf3(ctrlInfo_ptr->rxCount,
					ctrlInfo_ptr->Interval_BytesRxed,
					ctrlInfo_ptr->StartTime,
					current_time,
					ctrlInfo_ptr->Interval_StartTime,
					ctrlInfo_ptr->version,
					&ctrlInfo_ptr->remote_ip,
					ctrlInfo_ptr->remote_port,
					ctrlInfo_ptr->bandwidth_format,
					PRINT_TYPE_TCP_RX_INT);

				ctrlInfo_ptr->Interval_BytesRxed = 0;
				ctrlInfo_ptr->Interval_StartTime = current_time;	/* interval start time ÃƒÃŠÂ±Ã¢ÃˆÂ­ */
#ifdef __LIB_IPERF_PRINT_MIB__
				iperf_print_mib(ctrlInfo_ptr->mib_flag);
#endif /* __LIB_IPERF_PRINT_MIB__ */
			}
		}
		if (IP3_TCP_Rx_Finish_Flag) {
			goto end;
		}
		goto loop;
	}
#if 0
	else {
		IPERF3_MSG(" rx_bytes : %d\n", rx_bytes);
		if (IP3_TCP_Rx_Finish_Flag) {
			goto end;
		}
	}
#endif
end:
	/* TCP Rx Total */
	print_iperf3(ctrlInfo_ptr->rxCount,
				ctrlInfo_ptr->BytesRxed,
				ctrlInfo_ptr->StartTime,
				current_time,
				ctrlInfo_ptr->StartTime,
				ctrlInfo_ptr->version,
				&ctrlInfo_ptr->remote_ip,
				ctrlInfo_ptr->remote_port,
				ctrlInfo_ptr->bandwidth_format,
				PRINT_TYPE_TCP_RX_TOL);
	IPERF3_MSG("\n");

	close(peer_sock);
	vTaskDelay(portCONVERT_MS_2_TICKS(100));
#ifdef MULTI_THREAD_MULTI_SOCKET
	iperf_tcp_rx_task_handler[taskNo] = NULL;
	vTaskDelete(taskHandler);
#endif
	return NULL;
}

int acceptClientIp3(int sock, struct sockaddr_in *client, int size, int timeout)
{
	int				iResult;
	struct timeval	tv;
	fd_set 			set;
	int				client_addr_sz = size;
	int				peer_sock;

	FD_ZERO(&set);
	FD_SET((unsigned int)sock, &set);

	tv.tv_sec = (long)timeout;
	tv.tv_usec = 0;

	iResult = select(sock+1, &set, (fd_set *)NULL, (fd_set *)NULL, &tv);
	if(iResult > 0) {
		peer_sock = accept(sock, (struct sockaddr *)client, (socklen_t *)&client_addr_sz);
		return peer_sock;
	}
	else {
		return -1;
	}
}
#endif
#ifdef MULTI_THREAD_MULTI_SOCKET
void *lwiperf_tcp_server(void *pvParameters)	/* TASK */
{
	int 					sock;
	int 					peer_sock;
	int 					client_addr_sz;
	struct sockaddr_in		server;
	struct sockaddr_in		client;
	struct netif			*netif;
	int						rx_bytes;
	int						rcv_buf;
	char					clientIP[20];
	BaseType_t				xReturned;

	err_t					err;
	struct altcp_pcb		*alpcb;
	struct tcp_pcb			*pcb;
	ctrl3_info_t				*ctrlInfo_ptr;
	ip_addr_t				*local_addr;
	u16_t					local_port;
	lwiperf_report_fn		report_fn;
	void					*report_arg = NULL;
	lwiperf3_state_base_t	*related_master_state = NULL;
#ifdef TCP_SERVER_LOOP
	int						reInit = 0;
#endif
	UINT					tcp_apiMode;
	UINT					taskNo = 0;
	char					tcpRxTaskName[20];
	ip_addr_t               tmp_addr;

	/* Set the pointer. */
	ctrlInfo_ptr = (ctrl3_info_t *)pvParameters;

	tcp_apiMode = ctrlInfo_ptr->tcpApiMode;

	if (tcp_apiMode == TCP_API_MODE_ALTCP) {
		//IPERF3_MSG(CYAN_COLOR "[%s] ALTCP mode: %d\n" CLEAR_COLOR, __func__, tcp_apiMode);
		goto API_MODE_TCP;
	}
	else if (tcp_apiMode == TCP_API_MODE_TCP) {
		//IPERF3_MSG(CYAN_COLOR "[%s] TCP mode: %d\n" CLEAR_COLOR, __func__, tcp_apiMode);
		goto API_MODE_TCP;
	}
	else if (tcp_apiMode == TCP_API_MODE_SOCKET) {
		//IPERF3_MSG(CYAN_COLOR "[%s] SOCKET mode: %d\n" CLEAR_COLOR, __func__, tcp_apiMode);
		goto API_MODE_SOCKET;
	}
	else {
		IPERF3_MSG(RED_COLOR "[%s:%d] Unknown mode: %d\n" CLEAR_COLOR,
							__func__, __LINE__, tcp_apiMode);
	}

	for (int i=0; i<MAX_TCP_RX_TASK; i++) {
		iperf_tcp_rx_task_handler[i] = NULL;
	}

API_MODE_SOCKET:

	IPERF3_MSG(CYAN_COLOR "\n TCP Server start\n" CLEAR_COLOR);

	IP3_TCP_Rx_Finish_Flag = 0;

	/* Set the pointer. */
	//ctrlInfo_ptr = (ctrl3_info_t *)pvParameters;

	// Create socket
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1) {
		IPERF3_MSG("Could not create socket\n");
		return 1;
	}
#ifdef __DEBUG_IPERF3__
	IPERF3_MSG(">>> Socket created OK ...\n");
#endif // __DEBUG_IPERF3__
//	tcp_rx_sock = sock;

	// Prepare the sockaddr_in structure
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(LWIPERF3_TCP_PORT_DEFAULT);

	// Bind
	if (bind(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
		//print the error message
		close (sock);
		IPERF3_MSG("bind failed. Error\n");
		return 1;
	}
#ifdef __DEBUG_IPERF3__
	IPERF3_MSG(">>> Socket bind done\n");
#endif // __DEBUG_IPERF3__

	// Window size set
	rcv_buf = TCP_WND;	//do as default(8*MSS), 4096 is small receive window size
	setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *)&rcv_buf, sizeof(rcv_buf));

	// Listen
	if (listen(sock, 5) < 0) {
		IPERF3_MSG("Can't listening connect.\n");
		exit(0);
	}

	// Accept and incoming connection
#ifdef __DEBUG_IPERF3__
	IPERF3_MSG(">>> Waiting for incoming connections\n");
#endif // __DEBUG_IPERF3__

	memset(&ctrlInfo_ptr->lwiperf_state, 0, sizeof(lwiperf3_state_tcp_t));
	ctrlInfo_ptr->lwiperf_state.base.tcp = 1;
	ctrlInfo_ptr->lwiperf_state.base.server = 1;
	ctrlInfo_ptr->lwiperf_state.base.related_master_state = related_master_state;
	ctrlInfo_ptr->lwiperf_state.report_fn = report_fn;
	ctrlInfo_ptr->lwiperf_state.report_arg = report_arg;

	client_addr_sz = sizeof(struct sockaddr_in);

	while(1) {
		while(1) {
			peer_sock = acceptClientIp3(sock, &client, sizeof(struct sockaddr_in), 1);
			if (peer_sock >= 0) {
				break;
			}
			else {
				if (IP3_TCP_Rx_Finish_Flag) {
					goto task_end_s;
				}
			}
		}

		ctrlInfo_ptr->PacketsRxed = 0;
		ctrlInfo_ptr->BytesRxed = 0;
		ctrlInfo_ptr->StartTime = 0;
		ctrlInfo_ptr->RunTime = 0;

		ctrlInfo_ptr->StartTime = ctrlInfo_ptr->Interval_StartTime = xTaskGetTickCount();

		inet_ntop(AF_INET, &client.sin_addr.s_addr, clientIP, sizeof(clientIP));

		netif = netif_get_by_index(2);	/* WLAN0:2, WLAN1:3 */
		ip_addr_copy_from_ip4(ctrlInfo_ptr->local_ip, netif->ip_addr.u_addr.ip4);
		ctrlInfo_ptr->local_port = LWIPERF3_TCP_PORT_DEFAULT;

		ipaddr_aton(clientIP, &tmp_addr);
		ip4_addr_set_u32(&ctrlInfo_ptr->remote_ip.u_addr.ip4, lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&tmp_addr))));
		ctrlInfo_ptr->remote_port = lwip_htons(client.sin_port);

		IPERF3_MSG(CYAN_COLOR" Local IP: %s:%d ",
							ipaddr_ntoa(&ctrlInfo_ptr->local_ip),
							ctrlInfo_ptr->local_port);

		IPERF3_MSG(" connected with Remote IP: %s:%d\n" CLEAR_COLOR,
									ipaddr_ntoa(&ctrlInfo_ptr->remote_ip),
									ctrlInfo_ptr->remote_port);

		if (ctrlInfo_ptr->Interval > 0) {
			IPERF3_MSG("\n"
			ANSI_BCOLOR_GREEN ANSI_COLOR_BLUE
			MSG_HEAD_TCP_RX MSG_TITLE_INTERVAL
			ANSI_BCOLOR_BLACK ANSI_COLOR_WHITE ANSI_NORMAL "\n",
			"[Src IP:Port]");
		}

		ctrlInfo_ptr->socket_fd = peer_sock;

		for(taskNo = 0; taskNo < MAX_TCP_RX_TASK; taskNo++) {
			if (iperf_tcp_rx_task_handler[taskNo] == NULL) {
				break;
			}
		}

		ctrlInfo_ptr->taskNo = taskNo;

		memset(tcpRxTaskName, 0, 20);
		sprintf(tcpRxTaskName, "i_tcp_rx%d", ctrlInfo_ptr->taskNo);

		xReturned = xTaskCreate(tcpRxTaskIp3,
							tcpRxTaskName,
							1024,
							(void *)ctrlInfo_ptr,
							IPERF_TCP_SERVER_PRIORITY,
							&iperf_tcp_rx_task_handler[ctrlInfo_ptr->taskNo]);


		/* Wait ... One is emptied and goes into the accept state. */
		vTaskDelay(50 / portTICK_RATE_MS);
		{
			int i;
			//IPERF3_MSG("[%s] Check all full MAX_TCP_RX_TASK(%d, %d)\n", __func__,  MAX_TCP_RX_TASK, taskNo);
wait_loop:
			for(i = 0; i < MAX_TCP_RX_TASK; i++) {
				if (iperf_tcp_rx_task_handler[i] == NULL) {
					//IPERF3_MSG("[%s] One(%d) is emptied and goes into the accept state.\n", __func__, i);
					break;
				}
				vTaskDelay(500 / portTICK_RATE_MS);
			}
			if (i == MAX_TCP_RX_TASK) {
				goto wait_loop;
			}
			else {
				//IPERF3_MSG("[%s] Goes into the accept state.(taskNo:%d)\n", __func__, i);
			}

			continue;
		}
	}

task_end_s:
	close (sock);

	IPERF3_MSG(MSG_TCP_SERVER_STOP_USER);

	thread_tcp_rx_iperf3 = NULL;
	iperf3_TCP_RX_ctrl_info = NULL;

	vTaskDelete(iperf3_tcp_rx_handler);

	return NULL;

API_MODE_TCP:

	IPERF3_MSG(CYAN_COLOR "\n TCP Server start\n" CLEAR_COLOR);

	LWIP_ASSERT_CORE_LOCKED();

#if defined(TCP_SERVER_LOOP)
TCP_RX_LOOP:
	if (reInit) {
		IPERF3_MSG(CYAN_COLOR "\n TCP Server restart\n" CLEAR_COLOR);
	}

	IP3_TCP_Rx_Restart_Flag = 0;
#endif

	IP3_TCP_Rx_Finish_Flag = 0;

	/* Set the pointer. */
	//ctrlInfo_ptr = (ctrl3_info_t *)pvParameters;

	local_addr = IP_ADDR_ANY;
	local_port = LWIPERF3_TCP_PORT_DEFAULT;
	report_fn  = ctrlInfo_ptr->reportFunc;

	memset(&ctrlInfo_ptr->lwiperf_state, 0, sizeof(lwiperf3_state_tcp_t));
	ctrlInfo_ptr->lwiperf_state.base.tcp = 1;
	ctrlInfo_ptr->lwiperf_state.base.server = 1;
	ctrlInfo_ptr->lwiperf_state.base.related_master_state = related_master_state;
	ctrlInfo_ptr->lwiperf_state.report_fn = report_fn;
	ctrlInfo_ptr->lwiperf_state.report_arg = report_arg;

	if (tcp_apiMode == TCP_API_MODE_ALTCP) {
		alpcb = altcp_new_ip_type(NULL, LWIPERF_SERVER_IP_TYPE);
		if (alpcb == NULL) {
			IPERF3_MSG(RED_COLOR "[%s:%d] alpcb err\n" CLEAR_COLOR, __func__, __LINE__);
			goto task_end;
		}
		err = altcp_bind(alpcb, local_addr, local_port);
		if (err != ERR_OK) {
			IPERF3_MSG(RED_COLOR "[%s:%d] bind err\n" CLEAR_COLOR, __func__, __LINE__);
			goto task_end;
		}
		ctrlInfo_ptr->lwiperf_state.server_alpcb = altcp_listen_with_backlog(alpcb, 1);
		if (ctrlInfo_ptr->lwiperf_state.server_alpcb == NULL) {
			if (alpcb != NULL) {
				altcp_close(alpcb);
			}
			IPERF3_MSG(RED_COLOR "[%s:%d] listen err\n" CLEAR_COLOR, __func__, __LINE__);
			goto task_end;
		}
		alpcb = NULL;
		altcp_arg(ctrlInfo_ptr->lwiperf_state.server_alpcb, ctrlInfo_ptr);
		altcp_accept(ctrlInfo_ptr->lwiperf_state.server_alpcb, lwiperf_altcp_accept);
		ctrlInfo_ptr->lwiperf_state.server_alpcb->fns = &altcp_iperf_functions;
	}
	else if (tcp_apiMode == TCP_API_MODE_TCP) {
		pcb = tcp_new_ip_type(LWIPERF_SERVER_IP_TYPE);
		if (pcb == NULL) {
			IPERF3_MSG(RED_COLOR "[%s:%d] pcb err\n" CLEAR_COLOR, __func__, __LINE__);
			goto task_end;
		}
		err = tcp_bind(pcb, local_addr, local_port);
		if (err != ERR_OK) {
			IPERF3_MSG(RED_COLOR "[%s:%d] bind err\n" CLEAR_COLOR, __func__, __LINE__);
			goto task_end;
		}
		ctrlInfo_ptr->lwiperf_state.server_pcb = tcp_listen_with_backlog(pcb, 1);
		if (ctrlInfo_ptr->lwiperf_state.server_pcb == NULL) {
			if (pcb != NULL) {
				tcp_close(pcb);
			}
			IPERF3_MSG(RED_COLOR "[%s:%d] listen err\n" CLEAR_COLOR, __func__, __LINE__);
			goto task_end;
		}
		pcb = NULL;
		tcp_arg(ctrlInfo_ptr->lwiperf_state.server_pcb, ctrlInfo_ptr);
		tcp_accept(ctrlInfo_ptr->lwiperf_state.server_pcb, lwiperf3_tcp_accept);
	}
	else {
		IPERF3_MSG(RED_COLOR "[%s:%d] Unknown mode: %d\n" CLEAR_COLOR,
							__func__, __LINE__, tcp_apiMode);
	}

#ifdef USING_LIST_DELETE	/* F_F_S */
	lwiperf_list_add(&ctrlInfo_ptr->lwiperf_state.base);
#endif // USING_LIST_DELETE

	while(1) {
#ifdef TCP_SERVER_LOOP
		if (IP3_TCP_Rx_Finish_Flag || IP3_TCP_Rx_Restart_Flag)
#else
		if (IP3_TCP_Rx_Finish_Flag)
#endif
		{
			if (tcp_apiMode == TCP_API_MODE_ALTCP) {
				if (ctrlInfo_ptr->lwiperf_state.server_alpcb != NULL) {
					err = altcp_close(ctrlInfo_ptr->lwiperf_state.server_alpcb);
					if (err != ERR_OK) {
						IPERF3_MSG(RED_COLOR "[%s] server_alpcb tcp_close error\n" CLEAR_COLOR, __func__);
					}
					else {
#ifdef __DEBUG_IPERF3__
						IPERF3_MSG(CYAN_COLOR "[%s] server_alpcb closed 0x%x\n" CLEAR_COLOR,
											__func__, ctrlInfo_ptr->lwiperf_state.server_alpcb);
#endif // __DEBUG_IPERF3__
					}

				}
				if (ctrlInfo_ptr->lwiperf_state.conn_alpcb != NULL) {
#ifdef TCP_SERVER_LOOP
					if (IP3_TCP_Rx_Finish_Flag) {
						lwiperf_tcp_close(ctrlInfo_ptr, LWIPERF_TCP_DONE_SERVER);
						IP3_TCP_Rx_Restart_Flag = 0;
					}
#endif
				}
			}
			else if (tcp_apiMode == TCP_API_MODE_TCP) {
				if (ctrlInfo_ptr->lwiperf_state.server_pcb != NULL) {

					err = tcp_close(ctrlInfo_ptr->lwiperf_state.server_pcb);

					if (err != ERR_OK) {
						IPERF3_MSG(RED_COLOR "[%s] server_pcb tcp_close error\n" CLEAR_COLOR, __func__);
					}
					else {
#ifdef __DEBUG_IPERF3__
						IPERF3_MSG(CYAN_COLOR "[%s] server_pcb closed 0x%x\n" CLEAR_COLOR,
											__func__, ctrlInfo_ptr->lwiperf_state.server_pcb);
#endif // __DEBUG_IPERF3__
					}
				}

				if (ctrlInfo_ptr->lwiperf_state.conn_pcb != NULL) {
#ifdef TCP_SERVER_LOOP
					if (IP3_TCP_Rx_Finish_Flag) {
						lwiperf_tcp_close(ctrlInfo_ptr, LWIPERF_TCP_DONE_SERVER);
						IP3_TCP_Rx_Restart_Flag = 0;
					}
#endif
				}
			}
			else {
				IPERF3_MSG(RED_COLOR "[%s:%d] Unknown mode: %d\n" CLEAR_COLOR,
							__func__, __LINE__, tcp_apiMode);
			}

#ifdef TCP_SERVER_LOOP
			if (IP3_TCP_Rx_Restart_Flag) {
				vTaskDelay(100 / portTICK_RATE_MS);
#ifdef __DEBUG_IPERF3__
				IPERF3_MSG("[%s] Finish->Reinit\n", __func__);
#endif // __DEBUG_IPERF3__
				IP3_TCP_Rx_Finish_Flag = 0;
				reInit = 1;
				goto TCP_RX_LOOP;
			}
#endif
			vPortFree(ctrlInfo_ptr);

			break;
		}

		vTaskDelay(500 / portTICK_RATE_MS);
	}

task_end:
	IPERF3_MSG(MSG_TCP_SERVER_STOP_USER);

	thread_tcp_rx_iperf3 = NULL;
	iperf3_TCP_RX_ctrl_info = NULL;

	vTaskDelete(iperf3_tcp_rx_handler);

	return NULL;
}
#else
void lwiperf3_tcp_server(void *pvParameters)	/* TASK */
{
	UINT					tcp_apiMode;
	ctrl3_info_t				*ctrlInfo_ptr;
	int ret;

#if defined(IPERF3_WDOG_OFF)
    lwiperf3_set_watchdog_freeze();
#endif // IPERF3_WDOG_OFF
	/* Set the pointer. */
	ctrlInfo_ptr = (ctrl3_info_t *)pvParameters;
	tcp_apiMode = ctrlInfo_ptr->tcpApiMode;
	IP3_TCP_Rx_Finish_Flag = 0;

	if (tcp_apiMode == TCP_API_MODE_ALTCP || tcp_apiMode == TCP_API_MODE_TCP) {
		ret = api_mode_altcp_server3((ctrl3_info_t *)pvParameters);
		if( ret < 0)
			IPERF3_MSG("[%s] abnormal case ret(%d)", __func__,ret);
	}
	else
#ifdef __IPERF3_API_MODE_SOCKET__
	if (tcp_apiMode == TCP_API_MODE_SOCKET) {
		ret = api_mode_socket_server((ctrl3_info_t *)pvParameters);
		if( ret < 0)
			IPERF3_MSG("[%s] abnormal case ret(%d)", __func__,ret);
	}
	else
#endif // __IPERF3_API_MODE_SOCKET__
	{
		IPERF3_MSG(RED_COLOR "[%s:%d] Unknown mode: %d\n" CLEAR_COLOR,
							__func__, __LINE__, tcp_apiMode);
	}

	IPERF3_MSG(MSG_TCP_SERVER_STOP_USER);

	/* Fix me lator */
	/* By iPerf3 Deletion cmd, fault is happened , so comment */
	vPortFree(ctrlInfo_ptr);

	thread_tcp_rx_iperf3 = NULL;
	iperf3_TCP_RX_ctrl_info = NULL;

#if defined(IPERF3_WDOG_OFF)
    lwiperf3_set_watchdog_unfreeze();
#endif // IPERF3_WDOG_OFF
	vTaskDelete(iperf3_tcp_rx_handler);

	return;
}

#ifdef __IPERF3_API_MODE_SOCKET__
int api_mode_socket_server(void *pvParameters)
{
	int                     sock;
	int                     peer_sock;
	struct sockaddr_in      server;
	struct sockaddr_in      client;
	struct netif            *netif;
	int                     rcv_buf;
	char                    clientIP[40];
	ctrl3_info_t             *ctrlInfo_ptr;
	lwiperf_report_fn       report_fn;
	void                    *report_arg = NULL;
	lwiperf3_state_base_t    *related_master_state = NULL;
	ip_addr_t tmp_addr;

	IPERF3_MSG(CYAN_COLOR "\n Socket Mode TCP Server start\n" CLEAR_COLOR);

	/* Set the pointer. */
	ctrlInfo_ptr = (ctrl3_info_t *)pvParameters;
#if 0
{
	int ret = 0;
	int listen_sock = -1;
	int client_sock = -1;

	struct sockaddr_in server_addr;

	struct sockaddr_in client_addr;
	int client_addrlen = sizeof(struct sockaddr_in);

	int len = 0;
	char data_buffer[2048] = {0x00,};

	memset(&server_addr, 0x00, sizeof(struct sockaddr_in));
	memset(&client_addr, 0x00, sizeof(struct sockaddr_in));

	IPERF3_MSG("[%s] Start of TCP Server sample\n", __func__);

	listen_sock = socket(PF_INET, SOCK_STREAM, 0);
	if (listen_sock < 0) {
		IPERF3_MSG("[%s] Failed to create listen socket\n", __func__);
		goto end_of_task;
	}
	// Window size set
	rcv_buf = TCP_WND;	//do as default(8*MSS), 4096 is small receive window size
	setsockopt(listen_sock, SOL_SOCKET, SO_RCVBUF, (char *)&rcv_buf, sizeof(rcv_buf));

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(LWIPERF3_TCP_PORT_DEFAULT);

	ret = bind(listen_sock, (struct sockaddr *)&server_addr,
			   sizeof(struct sockaddr_in));
	if (ret == -1) {
		IPERF3_MSG("[%s] Failed to bind socket\n", __func__);
		goto end_of_task;
	}

	ret = listen(listen_sock, 1);
	if (ret != 0) {
		IPERF3_MSG("[%s] Failed to listen socket of tcp server(%d)\n",
			   __func__, ret);
		goto end_of_task;
	}

	while (1) {
		client_sock = -1;
		memset(&client_addr, 0x00, sizeof(struct sockaddr_in));
		client_addrlen = sizeof(struct sockaddr_in);

		client_sock = accept(listen_sock, (struct sockaddr *)&client_addr,
							 (socklen_t *)&client_addrlen);
		if (client_sock < 0) {
			continue;
		}
#if 0
		IPERF3_MSG("Connected client(%d.%d.%d.%d:%d)\n",
			   (ntohl(client_addr.sin_addr.s_addr) >> 24) & 0xFF,
			   (ntohl(client_addr.sin_addr.s_addr) >> 16) & 0xFF,
			   (ntohl(client_addr.sin_addr.s_addr) >>  8) & 0xFF,
			   (ntohl(client_addr.sin_addr.s_addr)      ) & 0xFF,
			   (ntohs(client_addr.sin_port)));
#endif
		memset(data_buffer, 0x00, sizeof(data_buffer));

		while (1) {

	//		IPERF3_MSG("< Read from client: ");

			len = recv(client_sock, data_buffer, sizeof(data_buffer), 0);
			if (len <= 0) {
				IPERF3_MSG("[%s] Failed to receive data(%d)\n", __func__, len);
				break;
			}

//			data_buffer[len] = '\0';

//			IPERF3_MSG("%d bytes read\n", len);

//			tcp_server_sample_hex_dump("Received data", data_buffer, len);

//			IPERF3_MSG("> Write to client: ");

//			len = send(client_sock, data_buffer, len, 0);
//			if (len <= 0) {
//				IPERF3_MSG("[%s] Failed to send data\n", __func__);
//				break;
//			}

//			IPERF3_MSG("%d bytes written\n", len);

	//		tcp_server_sample_hex_dump("Sent data", data_buffer, len);
		}

		IPERF3_MSG("%d bytes written\n", len);
		if (IP3_TCP_Rx_Finish_Flag) {
			close(client_sock);

	    	IPERF3_MSG("Disconnected client\n");
		}

	}

end_of_task:

	IPERF3_MSG("[%s] End of TCP Server sample\n", __func__);

	close(listen_sock);
	close(client_sock);

	vTaskDelete(NULL);

	return ;
}

#else
	// Create socket
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1) {
		IPERF3_MSG("Could not create socket\n");
		return -1;
	}
//	tcp_rx_sock = sock;

	// Prepare the sockaddr_in structure
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(LWIPERF3_TCP_PORT_DEFAULT);

	// Bind
	if (bind(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
		//print the error message
		close (sock);
		IPERF3_MSG("bind failed. Error\n");
		return -1;
	}

	// Window size set
	rcv_buf = TCP_WND;	//do as default(8*MSS), 4096 is small receive window size
	setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *)&rcv_buf, sizeof(rcv_buf));

	// Listen
	if (listen(sock, 5) < 0) {
		IPERF3_MSG("Can't listening connect.\n");
		exit(0);
	}

	// Accept and incoming connection

	memset(&ctrlInfo_ptr->lwiperf_state, 0, sizeof(lwiperf3_state_tcp_t));
	ctrlInfo_ptr->lwiperf_state.base.tcp = 1;
	ctrlInfo_ptr->lwiperf_state.base.server = 1;
	ctrlInfo_ptr->lwiperf_state.base.related_master_state = related_master_state;
	ctrlInfo_ptr->lwiperf_state.report_fn = report_fn;
	ctrlInfo_ptr->lwiperf_state.report_arg = report_arg;

	while(1) {
		while(1) {
			peer_sock = acceptClientIp3(sock, &client, sizeof(struct sockaddr_in), 1);
			if (peer_sock >= 0) {
				break;
			}
			else {
				if (IP3_TCP_Rx_Finish_Flag) {
					goto task_end_s;
				}
			}
		}

		ctrlInfo_ptr->PacketsRxed = 0;
		ctrlInfo_ptr->BytesRxed = 0;
		ctrlInfo_ptr->StartTime = 0;
		ctrlInfo_ptr->RunTime = 0;
		ctrlInfo_ptr->rxCount = 0;

		ctrlInfo_ptr->StartTime = ctrlInfo_ptr->Interval_StartTime = xTaskGetTickCount();

		inet_ntop(AF_INET, &client.sin_addr.s_addr, clientIP, sizeof(clientIP));

		netif = netif_get_by_index(2);	/* WLAN0:2, WLAN1:3 */
		ip_addr_copy_from_ip4(ctrlInfo_ptr->local_ip, netif->ip_addr.u_addr.ip4);
		ctrlInfo_ptr->local_port = LWIPERF3_TCP_PORT_DEFAULT;

		ipaddr_aton(clientIP, &tmp_addr);
		ip4_addr_set_u32(&ctrlInfo_ptr->remote_ip.u_addr.ip4, lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&tmp_addr))));
		ctrlInfo_ptr->remote_port = lwip_htons(client.sin_port);

		IPERF3_MSG(CYAN_COLOR "Local IP: %s:%d ",
									ipaddr_ntoa(&ctrlInfo_ptr->local_ip),
									ctrlInfo_ptr->local_port);

		IPERF3_MSG("connected with Remote IP: %s:%d\n" CLEAR_COLOR,
									ipaddr_ntoa(&ctrlInfo_ptr->remote_ip),
									ctrlInfo_ptr->remote_port);

		if (ctrlInfo_ptr->Interval > 0) {
			IPERF3_MSG(ANSI_BCOLOR_GREEN ANSI_COLOR_BLUE
						MSG_HEAD_TCP_RX MSG_TITLE_INTERVAL
						ANSI_BCOLOR_BLACK ANSI_COLOR_WHITE ANSI_NORMAL "\n",
						"[Src IP:Port]");
		}

		ctrlInfo_ptr->socket_fd = peer_sock;
		tcpRxTaskIp3(ctrlInfo_ptr);
		vTaskDelay(1);
	}
#endif
task_end_s:
	close (sock);
	return 0;
}
#endif // __IPERF3_API_MODE_SOCKET__
#if 1
int api_mode_altcp_server3(void *pvParameters)
{
	err_t                   err;
#ifdef __IPERF3_API_MODE_ALTCP__
	struct altcp_pcb        *alpcb;
#endif // __IPERF3_API_MODE_ALTCP__
	struct tcp_pcb          *pcb;
	ctrl3_info_t             *ctrlInfo_ptr;
	const ip_addr_t               *local_addr;
	u16_t                   local_port;
	lwiperf_report_fn       report_fn;
	void                    *report_arg = NULL;
	lwiperf3_state_base_t    *related_master_state = NULL;

#if defined(TCP_SERVER_LOOP) && !defined(USE_TCP_SERVER_LISTEN)
	int                     reInit = 0;
#endif
	UINT					tcp_apiMode;

	ctrlInfo_ptr = (ctrl3_info_t *)pvParameters;

#ifdef __DEBUG_IPERF3__
	if (ctrlInfo_ptr->tcpApiMode == TCP_API_MODE_TCP)
		IPERF3_MSG(CYAN_COLOR "\nAPI Mode TCP Server start\n" CLEAR_COLOR);
	else if (ctrlInfo_ptr->tcpApiMode == TCP_API_MODE_ALTCP)
		IPERF3_MSG(CYAN_COLOR "\n ALTCP Server start\n" CLEAR_COLOR);
#endif // __DEBUG_IPERF3__

//	LWIP_ASSERT_CORE_LOCKED();

#if defined(TCP_SERVER_LOOP) && !defined(USE_TCP_SERVER_LISTEN)
TCP_RX_LOOP:
	if (reInit) {
		IPERF3_MSG(CYAN_COLOR "\nTCP Server restart\n" CLEAR_COLOR);
	}

	IP3_TCP_Rx_Restart_Flag = 0;
#endif


	/* Set the pointer. */
	tcp_apiMode = ctrlInfo_ptr->tcpApiMode;
#if LWIP_IPV4 && LWIP_IPV6
    if (ctrlInfo_ptr->version == IP_VERSION_V6) {
        local_addr = IP6_ADDR_ANY;
    } else if (ctrlInfo_ptr->version == IP_VERSION_V4) {
        local_addr = IP4_ADDR_ANY;
    }
#elif LWIP_IPV4
    local_addr = IP4_ADDR_ANY;
#elif LWIP_IPV6
    local_addr = IP6_ADDR_ANY;
#endif

	local_port = ctrlInfo_ptr->port;
	report_fn  = ctrlInfo_ptr->reportFunc;

	memset(&ctrlInfo_ptr->lwiperf_state, 0, sizeof(lwiperf3_state_tcp_t));
	ctrlInfo_ptr->lwiperf_state.base.tcp = 1;
	ctrlInfo_ptr->lwiperf_state.base.server = 1;
	ctrlInfo_ptr->lwiperf_state.base.related_master_state = related_master_state;
	ctrlInfo_ptr->lwiperf_state.report_fn = report_fn;
	ctrlInfo_ptr->lwiperf_state.report_arg = report_arg;

#ifdef __IPERF3_API_MODE_ALTCP__
	if (tcp_apiMode == TCP_API_MODE_ALTCP) {
		alpcb = altcp_new_ip_type(NULL, LWIPERF_SERVER_IP_TYPE);
		if (alpcb == NULL) {
			IPERF3_MSG(RED_COLOR "[%s:%d] alpcb err\n" CLEAR_COLOR, __func__, __LINE__);
			goto task_end;
		}
		err = altcp_bind(alpcb, local_addr, local_port);
		if (err != ERR_OK) {
			if (alpcb != NULL) {
				altcp_close(alpcb);
			}
			IPERF3_MSG(RED_COLOR "[%s:%d] bind err\n" CLEAR_COLOR, __func__, __LINE__);
			goto task_end;
		}
		ctrlInfo_ptr->lwiperf_state.server_alpcb = altcp_listen_with_backlog(alpcb, 1);
		if (ctrlInfo_ptr->lwiperf_state.server_alpcb == NULL) {
			if (alpcb != NULL) {
				altcp_close(alpcb);
			}
			IPERF3_MSG(RED_COLOR "[%s:%d] listen err\n" CLEAR_COLOR, __func__, __LINE__);
			goto task_end;
		}
		alpcb = NULL;
		altcp_arg(ctrlInfo_ptr->lwiperf_state.server_alpcb, ctrlInfo_ptr);
		altcp_accept(ctrlInfo_ptr->lwiperf_state.server_alpcb, lwiperf_altcp_accept);
		ctrlInfo_ptr->lwiperf_state.server_alpcb->fns = &altcp_iperf_functions;
	}
	else
#endif // __IPERF3_API_MODE_ALTCP__
	if (tcp_apiMode == TCP_API_MODE_TCP) {
		pcb = tcp_new_ip_type(LWIPERF_SERVER_IP_TYPE);
		if (pcb == NULL) {
			IPERF3_MSG(RED_COLOR "[%s:%d] pcb err\n" CLEAR_COLOR, __func__, __LINE__);
			goto task_end;
		}

		err = tcp_bind(pcb, local_addr, local_port);
		if (err != ERR_OK) {
			if (pcb != NULL) {
				tcp_close(pcb);
			}
			IPERF3_MSG(RED_COLOR "[%s:%d] bind err\n" CLEAR_COLOR, __func__, __LINE__);
			goto task_end;
		}

		ctrlInfo_ptr->lwiperf_state.server_pcb = tcp_listen_with_backlog(pcb, 1);
		if (ctrlInfo_ptr->lwiperf_state.server_pcb == NULL) {
			if (pcb != NULL) {
				tcp_close(pcb);
			}
			IPERF3_MSG(RED_COLOR "[%s:%d] listen err\n" CLEAR_COLOR, __func__, __LINE__);
			goto task_end;
		}
		pcb = NULL;
		tcp_arg(ctrlInfo_ptr->lwiperf_state.server_pcb, ctrlInfo_ptr);
		tcp_accept(ctrlInfo_ptr->lwiperf_state.server_pcb, lwiperf3_tcp_accept);
	}
	else {
		IPERF3_MSG(RED_COLOR "[%s:%d] Unknown mode: %d\n" CLEAR_COLOR,
							__func__, __LINE__, tcp_apiMode);
	}

#ifdef USING_LIST_DELETE	/* F_F_S */
	lwiperf_list_add(&ctrlInfo_ptr->lwiperf_state.base);
#endif // USING_LIST_DELETE

	IPERF3_MSG("\niPerf3 Server(TCP): Ready\n");

	while (1) {
#if defined(TCP_SERVER_LOOP) && !defined(USE_TCP_SERVER_LISTEN)
		if (IP3_TCP_Rx_Finish_Flag || IP3_TCP_Rx_Restart_Flag)
#else
		if (IP3_TCP_Rx_Finish_Flag)
#endif
		{
#ifdef __IPERF3_API_MODE_ALTCP__
			if (tcp_apiMode == TCP_API_MODE_ALTCP) {
				if (ctrlInfo_ptr->lwiperf_state.server_alpcb != NULL) {
					err = altcp_close(ctrlInfo_ptr->lwiperf_state.server_alpcb);
					if (err != ERR_OK) {
						IPERF3_MSG(RED_COLOR "[%s] server_alpcb tcp_close error\n" CLEAR_COLOR, __func__);
					}
					else {
#ifdef __DEBUG_IPERF3__
						IPERF3_MSG(CYAN_COLOR "[%s] server_alpcb closed 0x%x\n" CLEAR_COLOR,
											__func__, ctrlInfo_ptr->lwiperf_state.server_alpcb);
#endif // __DEBUG_IPERF3__
					}

				}
				if (ctrlInfo_ptr->lwiperf_state.conn_alpcb != NULL) {
#ifdef TCP_SERVER_LOOP
					if (IP3_TCP_Rx_Finish_Flag) {
						lwiperf_tcp_close(ctrlInfo_ptr, LWIPERF_TCP_DONE_SERVER);
						IP3_TCP_Rx_Restart_Flag = 0;
					}
#endif
				}
			}
			else
#endif // __IPERF3_API_MODE_ALTCP__
			if (tcp_apiMode == TCP_API_MODE_TCP) {
				if (ctrlInfo_ptr->lwiperf_state.server_pcb != NULL) {

					err = tcp_close(ctrlInfo_ptr->lwiperf_state.server_pcb);

					if (err != ERR_OK) {
						IPERF3_MSG(RED_COLOR "[%s] server_pcb tcp_close error\n" CLEAR_COLOR, __func__);
					}
					else {
#ifdef __DEBUG_IPERF3__
						IPERF3_MSG(CYAN_COLOR "[%s] server_pcb closed 0x%x\n" CLEAR_COLOR,
											__func__, ctrlInfo_ptr->lwiperf_state.server_pcb);
#endif // __DEBUG_IPERF3__
					}
				}

				if (ctrlInfo_ptr->lwiperf_state.conn_pcb != NULL) {
#ifdef TCP_SERVER_LOOP
					if (IP3_TCP_Rx_Finish_Flag) {
						lwiperf_tcp_close(ctrlInfo_ptr, LWIPERF_TCP_DONE_SERVER);
						IP3_TCP_Rx_Restart_Flag = 0;
					}
#endif
				}
			}
			else {
				IPERF3_MSG(RED_COLOR "[%s:%d] Unknown mode: %d\n" CLEAR_COLOR,
							__func__, __LINE__, tcp_apiMode);
			}

#if defined(TCP_SERVER_LOOP) && !defined(USE_TCP_SERVER_LISTEN)
			if (IP3_TCP_Rx_Restart_Flag) {
				vTaskDelay(portCONVERT_MS_2_TICKS(100));
#ifdef __DEBUG_IPERF3__
				IPERF3_MSG("[%s] Finish->Reinit\n", __func__);
#endif // __DEBUG_IPERF3__
				IP3_TCP_Rx_Finish_Flag = 0;
				reInit = 1;
				goto TCP_RX_LOOP;
			}
#endif
			break;
		}
		vTaskDelay(100 / portTICK_RATE_MS);
	}

task_end:
	return 0;
}
#endif
#endif // MULTI_THREAD_MULTI_SOCKET

#if 0    // UNUSED_CODE in RRQ61x
/**
 * @ingroup iperf
 * Abort an iPerf3 session (handle returned by lwiperf_start_tcp_server*())
 */
void
lwiperf_abort(void *lwiperf_session)
{
	lwiperf3_state_base_t *i, *dealloc, *last = NULL;

	LWIP_ASSERT_CORE_LOCKED();

	for (i = lwiperf3_all_connections; i != NULL; ) {
		if ((i == lwiperf_session) || (i->related_master_state == lwiperf_session)) {
			dealloc = i;
			i = i->next;
			if (last != NULL) {
				last->next = i;
			}
			vPortFree(dealloc);	/* @todo: type? */
		}
		else {
			last = i;
			i = i->next;
		}
	}
}
#endif
#if 1
void lwiperf3_report_func(void *arg,
	enum lwiperf_report_type report_type,
	const ip_addr_t* local_addr,
	u16_t local_port,
	const ip_addr_t* remote_addr,
	u16_t remote_port,
	u32_t bytes_transferred,
	u32_t ms_duration,
	u32_t bandwidth_kbitpsec)
{
#if 0    // UNUSED_CODE in RRQ61x
	char		t_ip[16];
	u32_t		addr;

	IPERF3_MSG(" <<< Report >>>\n");
	IPERF3_MSG("  report_type: %d\n", report_type);

	addr = ip4_addr_get_u32(local_addr);
	longtoip(lwip_htonl(addr), t_ip);
	IPERF3_MSG("  local_addr: %s\n", t_ip);

	IPERF3_MSG("  local_port: %d\n", local_port);

	addr = ip4_addr_get_u32(remote_addr);
	longtoip(lwip_htonl(addr), t_ip);
	IPERF3_MSG("  remote_addr: %s\n", t_ip);

	IPERF3_MSG("  remote_port: %d\n", remote_port);
	IPERF3_MSG("  bytes_transferred: %d\n", bytes_transferred);
	IPERF3_MSG("  ms_duration: %d\n", ms_duration);
	IPERF3_MSG("  bandwidth_kbitpsec: %d\n", bandwidth_kbitpsec);
	IPERF3_MSG(" \n");
#else
	RA6W1_UNUSED_ARG(arg);
	RA6W1_UNUSED_ARG(report_type);
	RA6W1_UNUSED_ARG(local_addr);
	RA6W1_UNUSED_ARG(local_port);
	RA6W1_UNUSED_ARG(remote_addr);
	RA6W1_UNUSED_ARG(remote_port);
	RA6W1_UNUSED_ARG(bytes_transferred);
	RA6W1_UNUSED_ARG(ms_duration);
	RA6W1_UNUSED_ARG(bandwidth_kbitpsec);
#endif

	return ;
}
#endif
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UDP Server -- START ////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if 0
#undef	__DEBUG_IPERF3__TEMP__
#define MAX_UDP_RX_TOUT_SEC    3
void lwiperf_udp_server(void *pvParameters)	/* TASK */
{
	ctrl3_info_t	*ctrlInfo_ptr;
	int			packetID = 0;
	UINT		sender_port = 0;

	UINT		i = 0;
	ULONG		current_time = 0;
	unsigned int rx_error_cnt = 0;
	int			gap = 0;
	union {
		struct sockaddr_storage ss;
#ifdef CONFIG_IPV4
		struct sockaddr_in sin;
#endif /* CONFIG_IPV4 */
#ifdef CONFIG_IPV6
		struct sockaddr_in6 sin6;
#endif /* CONFIG_IPV6 */
	} from;
	socklen_t	fromlen;
	char		abuf[50];
	int			len, res;
#ifdef CONFIG_IPV4
	struct sockaddr_in local_addr;
#endif // CONFIG_IPV4
#ifdef CONFIG_IPV6
    struct sockaddr_in6 local_ip6addr;
#endif // CONFIG_IPV6
	int 		optval = 1;
	int			sockfd;
	struct timeval	tv;
#if LWIP_IPV4
	ip_addr_t tmp_addr;
#endif // LWIP_IPV4

#ifdef __DEBUG_IPERF3__
	IPERF3_MSG(CYAN_COLOR "\n UDP Server start\n" CLEAR_COLOR);
#endif // __DEBUG_IPERF3__

	/* Set the pointer. */
	ctrlInfo_ptr = (ctrl3_info_t *)pvParameters;

	if (ctrlInfo_ptr->port == 0) {
		ctrlInfo_ptr->port = LWIPERF_UDP_PORT_DEFAULT;
	}

	if (ctrlInfo_ptr->RxTimeOut == 0) {
		ctrlInfo_ptr->RxTimeOut = IPERF_RX_TIMEOUT_DEFAULT;
	}

    if (ctrlInfo_ptr->version == IP_VERSION_V6) {
        sockfd = socket(AF_INET6, SOCK_DGRAM, 0);
    } else if (ctrlInfo_ptr->version == IP_VERSION_V4) {
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    }

	if (sockfd == -1) {
		IPERF3_MSG("[%s:%d]Error - socket()\n", __func__, __LINE__);
		goto end_of_task;
	}
	ip3_udp_rx_sock = sockfd;

	res = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));
#ifdef __DEBUG_IPERF3__
	IPERF3_MSG("[%s] setsockopt SO_REUSEADDR result:%d\n", __func__, res);
#endif // __DEBUG_IPERF3__

#if 0
	tv.tv_sec = 1; 
    tv.tv_usec = 0;
    res = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const void *)&tv, sizeof(struct timeval));
#ifdef __DEBUG_IPERF3__
	IPERF3_MSG("[%s] setsockopt SO_RCVTIMEO result:%d\n", __func__, res);
#endif // __DEBUG_IPERF3__
#endif

#if LWIP_IPV4 && LWIP_IPV6
    if (ctrlInfo_ptr->version == IP_VERSION_V6) {
        memset(&local_ip6addr, 0x00, sizeof(local_ip6addr));
        local_ip6addr.sin6_family = AF_INET6;
        local_ip6addr.sin6_addr = in6addr_any;
        local_ip6addr.sin6_port = htons((u16_t)(ctrlInfo_ptr->port)/*LWIPERF_UDP_PORT_DEFAULT */);
        if (bind(sockfd, (struct sockaddr *)&local_ip6addr, sizeof(struct sockaddr_in6))) {
            IPERF3_MSG("[%s:%d] Error - bind(%s)\n", __func__, __LINE__, strerror(errno));
            goto End_of_socket;
        }
    } else if (ctrlInfo_ptr->version == IP_VERSION_V4) {
        memset(&local_addr, 0x00, sizeof(local_addr));
        local_addr.sin_family = AF_INET;
        local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        local_addr.sin_port = htons((u16_t)(ctrlInfo_ptr->port)/*LWIPERF_UDP_PORT_DEFAULT */);
        if (bind(sockfd, (struct sockaddr *)&local_addr, sizeof(struct sockaddr_in))) {
            IPERF3_MSG("[%s:%d] Error - bind(%s)\n", __func__, __LINE__, strerror(errno));
            goto End_of_socket;
        }
    }
#elif LWIP_IPV4
    memset(&local_addr, 0x00, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    local_addr.sin_port = htons((u16_t)(ctrlInfo_ptr->port)/*LWIPERF_UDP_PORT_DEFAULT */);
    if (bind(sockfd, (struct sockaddr *)&local_addr, sizeof(struct sockaddr_in))) {
        IPERF3_MSG("[%s:%d] Error - bind(%s)\n", __func__, __LINE__, strerror(errno));
        goto End_of_socket;
    }
#elif LWIP_IPV6
    memset(&local_ip6addr, 0x00, sizeof(local_ip6addr));
    local_ip6addr.sin6_family = AF_INET6;
    local_ip6addr.sin6_addr = in6addr_any;
    local_ip6addr.sin6_port = htons((u16_t)(ctrlInfo_ptr->port)/*LWIPERF_UDP_PORT_DEFAULT */);
    if (bind(sockfd, (struct sockaddr *)&local_ip6addr, sizeof(struct sockaddr_in6))) {
        IPERF3_MSG("[%s:%d] Error - bind(%s)\n", __func__, __LINE__, strerror(errno));
        goto End_of_socket;
    }
#endif

	ip3_udp_rx_buffer = pvPortMalloc(DEFAULT_IPV4_PACKET_SIZE);
	if (ip3_udp_rx_buffer == NULL) {
		IPERF3_MSG(RED_COLOR "[%s] Error mem alloc\n" CLEAR_COLOR, __func__);
		goto End_of_socket;
	}
#ifdef __DEBUG_IPERF3__ /* munchang.jung_20150312 */
		IPERF3_MSG("[%s]\n"
				"port=%d\n"\
				"TestTime=%d sec.\n"\
				"SendNum=%d\n"
				"Interval=%d sec.\n"\
				,__func__,
				ctrlInfo_ptr->port,
				ctrlInfo_ptr->TestTime / 100,
				ctrlInfo_ptr->send_num,
				ctrlInfo_ptr->Interval / 100);
#endif // __DEBUG_IPERF3__

	IPERF3_MSG("\niPerf3 Server(UDP): Ready\n");

IPERF_UDP_RX_LOOP:

	/* Update the test result. */
	ctrlInfo_ptr->rxCount = 0;
	ctrlInfo_ptr->PacketsRxed = 0;
	ctrlInfo_ptr->BytesRxed = 0;
	ctrlInfo_ptr->StartTime = 0;
	ctrlInfo_ptr->RunTime = 0;
	i = 0;
	current_time = 0;
	ctrlInfo_ptr->Interval_StartTime = 0;
	ctrlInfo_ptr->Interval_BytesRxed = 0;
	rx_error_cnt = 0;

	IP3_UDP_Rx_Finish_Flag = 0;

	/* Receive a UDP packet. */
	do {
		fromlen = sizeof(from);
		len = recvfrom(sockfd,
					ip3_udp_rx_buffer,
					DEFAULT_IPV4_PACKET_SIZE,
					0,
					(struct sockaddr *)&from.ss,
					(socklen_t *)&fromlen);
		if (!len) {
			IPERF3_MSG(RED_COLOR "[%s] Error recvfrom: %s\n" CLEAR_COLOR, __func__, strerror(errno));
			break;
		}

		if (IP3_UDP_Rx_Finish_Flag) {
			goto RX_END;
		}
	} while (len < 0);
#if LWIP_IPV4 && LWIP_IPV6
    if (ctrlInfo_ptr->version == IP_VERSION_V4) {
        os_strlcpy(abuf, ra6wx_inet_ntoa(from.sin.sin_addr), sizeof(abuf));
        sender_port = ntohs(from.sin.sin_port);
    } else if (ctrlInfo_ptr->version == IP_VERSION_V6) {
        inet_ntop(AF_INET6, &(from.sin6.sin6_addr), abuf, sizeof(abuf));
        sender_port = ntohs(from.sin6.sin6_port);
    }
#elif LWIP_IPV4
    os_strlcpy(abuf, ra6wx_inet_ntoa(from.sin.sin_addr), sizeof(abuf));
    sender_port = ntohs(from.sin.sin_port);
#elif LWIP_IPV6
    inet_ntop(AF_INET6, &(from.sin6.sin6_addr), abuf, sizeof(abuf));
    sender_port = ntohs(from.sin6.sin6_port);
#endif

#ifdef __DEBUG_IPERF3__
	IPERF3_MSG(" Received %d bytes from %s:%d\n", len, abuf, sender_port);
#endif // __DEBUG_IPERF3__
	ctrlInfo_ptr->remote_port = (u16_t)sender_port;
#if LWIP_IPV4 && LWIP_IPV6
    if (ctrlInfo_ptr->version == IP_VERSION_V4) {
        ipaddr_aton(abuf, &tmp_addr);
        ctrlInfo_ptr->ip = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&tmp_addr)));
        memset(&tmp_addr, 0x00, sizeof(ip_addr_t));
        ip4_addr_set_u32(&ctrlInfo_ptr->remote_ip.u_addr.ip4, lwip_htonl(ctrlInfo_ptr->ip));
    } else if (ctrlInfo_ptr->version == IP_VERSION_V6) {
        ctrlInfo_ptr->remote_ip.type = IPADDR_TYPE_V6;
        ctrlInfo_ptr->remote_ip.u_addr.ip6.addr[0] = from.sin6.sin6_addr.un.u32_addr[0];
        ctrlInfo_ptr->remote_ip.u_addr.ip6.addr[1] = from.sin6.sin6_addr.un.u32_addr[1];
        ctrlInfo_ptr->remote_ip.u_addr.ip6.addr[2] = from.sin6.sin6_addr.un.u32_addr[2];
        ctrlInfo_ptr->remote_ip.u_addr.ip6.addr[3] = from.sin6.sin6_addr.un.u32_addr[3];
    }
#elif LWIP_IPV4
    ipaddr_aton(abuf, &tmp_addr);
    ctrlInfo_ptr->ip = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&tmp_addr)));
    ip4_addr_set_u32(&ctrlInfo_ptr->remote_ip, lwip_htonl(ctrlInfo_ptr->ip));
#elif LWIP_IPV6
    ctrlInfo_ptr->remote_ip.addr[0] = from.sin6.sin6_addr.un.u32_addr[0];
    ctrlInfo_ptr->remote_ip.addr[1] = from.sin6.sin6_addr.un.u32_addr[1];
    ctrlInfo_ptr->remote_ip.addr[2] = from.sin6.sin6_addr.un.u32_addr[2];
    ctrlInfo_ptr->remote_ip.addr[3] = from.sin6.sin6_addr.un.u32_addr[3];
#endif
	/* Detect the end of the test signal. */
	packetID = *(int*)(ip3_udp_rx_buffer);
	packetID = (int)lwip_htonl((u32_t)packetID);

	/* Check the packet ID. */
	if (packetID < 0 && (unsigned int)packetID != 0xEFEFEFEF) { /* IPv6 Â½Ãƒ?Ã› packetID 0xEFEFEFEF ?Ã“ */

#ifdef __DEBUG_IPERF3__
		IPERF3_MSG(" [ERR] Rx Failed !!! id:%d\n", packetID);
#endif // __DEBUG_IPERF3__

		/* Send the UDP packet. */
		res = sendto(sockfd, ip3_udp_rx_buffer, (size_t)len, 0, (struct sockaddr *)&from.ss, fromlen);

		goto IPERF_UDP_RX_LOOP;
	}

	/* Set the test start time. */
	ctrlInfo_ptr->StartTime = ctrlInfo_ptr->Interval_StartTime = xTaskGetTickCount();

	IPERF3_MSG("\n\n" MSG_UDP_RX "\n");

	if (ctrlInfo_ptr->Interval > 0) {
		IPERF3_MSG(ANSI_BCOLOR_YELLOW ANSI_COLOR_BLUE
			MSG_HEAD_UDP_RX MSG_TITLE_INTERVAL
			ANSI_BCOLOR_BLACK ANSI_COLOR_WHITE ANSI_NORMAL "\n",
			"[Src IP:Port]");
	}

	tv.tv_sec = 1;
	tv.tv_usec = 0;
	res = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const void *)&tv, sizeof(struct timeval));
#ifdef __DEBUG_IPERF3__
	IPERF3_MSG(CYAN_COLOR "[%s] Change recv timeout 1S -> 200ms result:%d\n" CLEAR_COLOR, __func__, res);
#endif // __DEBUG_IPERF3__

	fromlen = sizeof(from);

	/* Loop to transmit the packet. */
	while (!IP3_UDP_Rx_Finish_Flag) {
		/* Receive a UDP packet. */

		/* If communication is disconnected during UDP RX test, it will be exited. */
		/* Wait for 1 second, if there is no data during reception, use the gap time to sync the interval output every second. */
		len = recvfrom(sockfd,
					ip3_udp_rx_buffer,
					DEFAULT_IPV4_PACKET_SIZE,
					0,
					(struct sockaddr *)&from.ss,
					(socklen_t *)&fromlen);

#ifdef __DEBUG_IPERF3__TEMP__
		packetID = *(int*)(ip3_udp_rx_buffer);
		packetID = lwip_htonl(packetID);
		IPERF3_MSG(" ===> len:%d id:%d\n" CLEAR_COLOR, len, packetID);
#endif

		/* Check status. */
		if (len < 0) {
#ifdef __DEBUG_IPERF3__
			IPERF3_MSG(RED_COLOR "[%s] Receive Error(len:%d, cnt:%d)\n" CLEAR_COLOR, __func__, len, rx_error_cnt);
#endif // __DEBUG_IPERF3__
			if (rx_error_cnt < (ctrlInfo_ptr->RxTimeOut / configTICK_RATE_HZ)) {
				rx_error_cnt++;
			}
			else {
#ifdef __DEBUG_IPERF3__TEMP__
				IPERF3_MSG(RED_COLOR "[%s] Receive Error(len:%d, cnt:%d, timeout:%d)\n" CLEAR_COLOR, __func__, len, rx_error_cnt, (ctrlInfo_ptr->RxTimeOut / configTICK_RATE_HZ));
#endif
				break;
			}
		}
		else {
			rx_error_cnt = 0;
			/* Detect the end of the test signal. */
			packetID = *(int*)(ip3_udp_rx_buffer);
			packetID = (int)lwip_htonl((u32_t)packetID);

			/* Check the packet ID. */
			if (packetID < 0) {
				int send_cnt = 0;
#ifdef __DEBUG_IPERF3__
				IPERF3_MSG(CYAN_COLOR " [MSG] Detect the end of the test signal\n" CLEAR_COLOR);
#endif // __DEBUG_IPERF3__

				do {
					/* Send the UDP packet. */
					res = sendto(sockfd,
								ip3_udp_rx_buffer,
								(size_t)len,
								0,
								(struct sockaddr *)&from.ss,
								fromlen);

					send_cnt++;
					vTaskDelay(1);
#ifdef __DEBUG_IPERF3__
					IPERF3_MSG(CYAN_COLOR " [MSG] Send end of the test signal (res=%d send_cnt=%d)\n" CLEAR_COLOR,
																__func__, res, send_cnt);
#endif // __DEBUG_IPERF3__

				} while (res != len && send_cnt < 10);

				break;
			}
		}

		if (len > 0) {
			/* Update the counter.	*/
			ctrlInfo_ptr->PacketsRxed++;
			ctrlInfo_ptr->BytesRxed += (unsigned long long)len;
		}

		current_time = xTaskGetTickCount();
		if (ctrlInfo_ptr->Interval > 0) {
			/* Â´Ã™?Â½ Ã�Â¤Â°Â¢ÃƒÃŠ Â±Ã®Ã�Ã¶ Â½ÃƒÂ°Â£ Â°Ã¨Â»Ãª */
			gap = (int)(ctrlInfo_ptr->Interval - ((ctrlInfo_ptr->Interval_StartTime - ctrlInfo_ptr->StartTime) % configTICK_RATE_HZ));

			if (len > 0) {
				ctrlInfo_ptr->Interval_BytesRxed += (UINT)len;
			}

			if ((ctrlInfo_ptr->Interval_StartTime < current_time) && (ctrlInfo_ptr->Interval_StartTime + (UINT)gap <= current_time)) {
				//IPERF3_MSG("gap=%03d ", gap);
				ctrlInfo_ptr->rxCount++;

				/* print UDP Rx Interval */
				print_iperf3(ctrlInfo_ptr->rxCount,
					ctrlInfo_ptr->Interval_BytesRxed,
					ctrlInfo_ptr->StartTime,
					current_time,
					ctrlInfo_ptr->Interval_StartTime,
					ctrlInfo_ptr->version,
					&ctrlInfo_ptr->remote_ip,
					ctrlInfo_ptr->remote_port,
					ctrlInfo_ptr->bandwidth_format,
					PRINT_TYPE_UDP_RX_INT);

				ctrlInfo_ptr->Interval_BytesRxed = 0;
				ctrlInfo_ptr->Interval_StartTime = current_time; /* interval start time ÃƒÃŠÂ±Ã¢ÃˆÂ­ */
#ifdef __LIB_IPERF_PRINT_MIB__
				iperf_print_mib(ctrlInfo_ptr->mib_flag);
#endif /* __LIB_IPERF_PRINT_MIB__ */
			}
		}
	}

	/* print UDP Rx Total */
	print_iperf3(ctrlInfo_ptr->rxCount,
		ctrlInfo_ptr->BytesRxed,
		ctrlInfo_ptr->StartTime,
		current_time,
		ctrlInfo_ptr->StartTime,
		ctrlInfo_ptr->version,
		&ctrlInfo_ptr->remote_ip,
		ctrlInfo_ptr->remote_port,
		ctrlInfo_ptr->bandwidth_format,
		PRINT_TYPE_UDP_RX_TOL);

	if (!UDP_Rx_Finish_Flag) {
		vTaskDelay(1000 / portTICK_RATE_MS);
		goto IPERF_UDP_RX_LOOP;
	}

RX_END:
	vPortFree(ctrlInfo_ptr);
End_of_socket:
	closesocket(sockfd);
end_of_task:
	vPortFree(ip3_udp_rx_buffer);
	IPERF3_MSG(MSG_UDP_SERVER_STOP_USER);

	thread_udp_rx_iperf3 = NULL;
	vTaskDelete(iperf3_udp_rx_handler);

	return;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UDP Server -- END  /////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UDP Client -- START ////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void lwiperf_udp_client(void *pvParameters)	/* TASK */
{
	UINT		res;
	ULONG		expire_time;
	ctrl3_info_t	*ctrlInfo_ptr;
	long		udp_id = 0;
	TickType_t sendtime = 0;

	ULONG		current_time;
	UINT		txcount = 0;
	int			data_cpy_ptr = 0;
	int			total_cpy_len = 0;
    udp_payload *iperf_udp_head_ptr;
	char		*send_data = NULL;
	int			gap = 0;

	long 		bandwidth_time = 0;		/* bandwidth target time usec */
#ifdef __ENABLE_UNUSED__
	unsigned long long startTime = 0, stopTime = 0;
	long		remain_time = 0;		/* us */
	unsigned long	Tx_ElapsedTme;
#ifdef PREVIOUS_PACKET_DELAY_COMPENSATION
	int			WaitOver = 0;
#endif // PREVIOUS_PACKET_DELAY_COMPENSATION
#endif	//__ENABLE_UNUSED__

#ifdef __DEBUG_BANDWIDTH__
	int			tx_overtime = 0;
	int			tx_undertime = 0;
	int			error_cnt = 0;
	int			waitOver_cnt = 0;
#endif /* __DEBUG_BANDWIDTH__ */
#if LWIP_IPV4
	struct	sockaddr_in peer_addr;
	struct	sockaddr_in local_addr;
#endif // LWIP_IPV4
#if LWIP_IPV6
    struct  sockaddr_in6 peer_ip6addr;
    struct  sockaddr_in6 local_ip6addr;
#endif // LWIP_IPV6
	int		sockfd;
	int		ret;
	int 	optval = 1;
#ifdef __ENABLE_UNUSED__
	udp_payload		*payload_ptr;
#endif	//__ENABLE_UNUSED__
	struct timeval	tv;

#ifdef __DEBUG_IPERF3__
	UCHAR           ipstr[16];

	IPERF3_MSG(CYAN_COLOR "\n UDP Client start\n" CLEAR_COLOR);
#endif // __DEBUG_IPERF3__

	/* Initialize the value. */
	udp_id = 0;
	ctrlInfo_ptr = (ctrl3_info_t *)pvParameters;

	const unsigned int pair_no = ctrlInfo_ptr->pair_no;
	thread_udp_tx_iperf3[pair_no].state = IPERF_TX_THD_STATE_ACTIVE;

	if (ctrlInfo_ptr->port == 0) {
		ctrlInfo_ptr->port = LWIPERF3_TCP_PORT_DEFAULT;
	}

	ctrlInfo_ptr->Interval_StartTime = 0;
	ctrlInfo_ptr->PacketsTxed = 0;
	ctrlInfo_ptr->BytesTxed = 0;
	ctrlInfo_ptr->StartTime = 0;
	ctrlInfo_ptr->RunTime = 0;
	ctrlInfo_ptr->txCount = 0;
	IP3_UDP_Tx_Finish_Flag = 0;

#ifdef __DEBUG_IPERF3__
    if (ctrlInfo_ptr->version == IP_VERSION_V4) {
    	longtoip(ctrlInfo_ptr->ip, (char*)ipstr);
    } else if (ctrlInfo_ptr->version == IP_VERSION_V6) {
        ipv6long2str(ctrlInfo_ptr->ipv6, (char*)ipstr);
    }

	IPERF3_MSG("[%s]\n IP:%s\n port:%d\n Packet size:%d\n TestTime:%d sec.\n SendNum:%d\n Interval:%d sec.\n",
		__func__,
		ipstr,
		ctrlInfo_ptr->port,
		ctrlInfo_ptr->PacketSize,
		ctrlInfo_ptr->TestTime / configTICK_RATE_HZ,
		ctrlInfo_ptr->send_num,
		ctrlInfo_ptr->Interval / configTICK_RATE_HZ);

#endif // __DEBUG_IPERF3__

	/* Set the packet size. */
	if (ctrlInfo_ptr->PacketSize == 0) {
		ctrlInfo_ptr->PacketSize = DEFAULT_IPV4_PACKET_SIZE;
	}

	send_data = pvPortMalloc(ctrlInfo_ptr->PacketSize);

	if (send_data == NULL) {
		IPERF3_MSG("pvPortMalloc Fail(UDP:send_data=%d)\n", ctrlInfo_ptr->PacketSize);
		goto end_of_task;
	}

#ifdef __DEBUG_IPERF3__
	IPERF3_MSG("pvPortMalloc (UDP:send_data=%d)\n", ctrlInfo_ptr->PacketSize);
#endif // __DEBUG_IPERF3__

	memset(send_data, 0, ctrlInfo_ptr->PacketSize);

	total_cpy_len = (int)(ctrlInfo_ptr->PacketSize - sizeof(udp_payload));
	data_cpy_ptr = sizeof(udp_payload);

	while (total_cpy_len) {
		memcpy(send_data + data_cpy_ptr,
			IPERF_DATA,
			(size_t)(total_cpy_len > IPERF_DATA_LEN ? IPERF_DATA_LEN : total_cpy_len));

		data_cpy_ptr = data_cpy_ptr + IPERF_DATA_LEN;
		total_cpy_len = total_cpy_len - (total_cpy_len > IPERF_DATA_LEN ? IPERF_DATA_LEN : total_cpy_len);
	}

#if !defined (__SUPPORT_EEMBC__)
	/* UDP Transmit Test Starts in 2 seconds. */
	vTaskDelay(2000 / portTICK_RATE_MS);
#endif // !defined (__SUPPORT_EEMBC__)

#ifdef __ENABLE_UNUSED__
{
	struct udp_pcb *pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
	struct pbuf *psend;
	u16_t tmp_cnt;
	ip_addr_t *server_addr = &ctrlInfo_ptr->remote_ip;

	if (pcb == NULL) {
		return ERR_MEM;
	}

	memset(&local_addr, 0x00, sizeof(local_addr));
	local_addr.sin_family = AF_INET;
	local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	local_addr.sin_port = 0;
	/* Write ABCs into the packet payload! */
	peer_addr.sin_family = AF_INET;
	peer_addr.sin_addr.s_addr = htonl(ctrlInfo_ptr->ip);
	peer_addr.sin_port = htons(ctrlInfo_ptr->port);


	/* set up local and remote port for the pcb -> listen on all interfaces on all src/dest IPs */
	udp_bind(pcb, IP4_ADDR_ANY, 0);
	udp_connect(pcb, IP4_ADDR_ANY, peer_addr.sin_port);

	while(1)
	{
		psend= pbuf_alloc(PBUF_TRANSPORT,ctrlInfo_ptr->PacketSize, PBUF_RAM);
		if (psend==NULL){
			IPERF3_MSG("[%s], udp test pubf alloc fail = %d\n",__func__, tmp_cnt);
			//goto Yang_test;
			vTaskDelay(1);
			continue;
		}

		iperf_udp_head_ptr = (udp_payload *)send_data;
		iperf_udp_head_ptr->udp_id  = htonl(udp_id);
		iperf_udp_head_ptr->tv_sec  = htonl(xTaskGetTickCount() / configTICK_RATE_HZ);
		iperf_udp_head_ptr->tv_usec = htonl(xTaskGetTickCount() / configTICK_RATE_HZ * 1000000);

		memcpy(psend->payload, send_data, ctrlInfo_ptr->PacketSize);

		res = udp_sendto(pcb, psend, server_addr, 5001);
		if (res != ERR_OK)
		{
			IPERF3_MSG("udp sento fail res = %d\n",res);
			pbuf_free(psend);
			goto Yang_test;;
		}

		if(++tmp_cnt > 20000)
			break;
		/* Performance checking */
		pbuf_free(psend);

	}
	IPERF3_MSG(" udp tx test done\n");
	goto Yang_test;
}
#endif	//__ENABLE_UNUSED__

	IPERF3_MSG(MSG_UDP_TX,
		ctrlInfo_ptr->version==IP_VERSION_V6 ? "[":"",
		ipaddr_ntoa(&ctrlInfo_ptr->remote_ip),
		ctrlInfo_ptr->version==IP_VERSION_V6 ? "]":"",
		ctrlInfo_ptr->port);

    if (ctrlInfo_ptr->version==IP_VERSION_V6) {
        sockfd = socket(AF_INET6, SOCK_DGRAM, 0);
    } else if (ctrlInfo_ptr->version==IP_VERSION_V4) {
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    }

	if (sockfd == -1) {
		IPERF3_MSG("[%s:%d] Error - socket()\n", __func__, __LINE__);
		goto end_of_task;
	}
	ip3_udp_tx_sock = sockfd;

	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));
#if LWIP_IPV4 && LWIP_IPV6
    if (ctrlInfo_ptr->version == IP_VERSION_V6) {
        memset(&local_ip6addr, 0x00, sizeof(local_ip6addr));
        local_ip6addr.sin6_family = AF_INET6;
        local_ip6addr.sin6_addr = in6addr_any;
        local_ip6addr.sin6_port = 0;
        if (bind(sockfd, (struct sockaddr *)&local_ip6addr, sizeof(struct sockaddr_in6))) {
            IPERF3_MSG("[%s:%d] Error - bind(%s)\n", __func__, __LINE__, strerror(errno));
            goto End_of_socket;
        }
    } else if (ctrlInfo_ptr->version == IP_VERSION_V4) {
        memset(&local_addr, 0x00, sizeof(local_addr));
        local_addr.sin_family = AF_INET;
        local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        local_addr.sin_port = 0;
        if (bind(sockfd, (struct sockaddr *)&local_addr, sizeof(struct sockaddr_in))) {
            IPERF3_MSG("[%s:%d] Error - bind(%s)\n", __func__, __LINE__, strerror(errno));
            goto End_of_socket;
        }
    }
#elif LWIP_IPV4
    memset(&local_addr, 0x00, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    local_addr.sin_port = 0;
    if (bind(sockfd, (struct sockaddr *)&local_addr, sizeof(struct sockaddr_in))) {
        IPERF3_MSG("[%s:%d] Error - bind(%s)\n", __func__, __LINE__, strerror(errno));
        goto End_of_socket;
    }
#elif LWIP_IPV6
    memset(&local_ip6addr, 0x00, sizeof(local_ip6addr));
    local_ip6addr.sin6_family = AF_INET6;
    local_ip6addr.sin6_addr = in6addr_any;
    local_ip6addr.sin6_port = 0;
    if (bind(sockfd, (struct sockaddr *)&local_ip6addr, sizeof(struct sockaddr_in6))) {
        IPERF3_MSG("[%s:%d] Error - bind(%s)\n", __func__, __LINE__, strerror(errno));
        goto End_of_socket;
    }
#endif

	/* UDP Tx Interval Title */
	if (ctrlInfo_ptr->Interval > 0) {
		IPERF3_MSG(ANSI_BCOLOR_YELLOW ANSI_COLOR_RED
			MSG_HEAD_UDP_TX MSG_TITLE_INTERVAL
			ANSI_BCOLOR_BLACK ANSI_COLOR_WHITE ANSI_NORMAL "\n",
			"[Dst IP:Port]");
	}

	if (ctrlInfo_ptr->send_num > 0) {
		expire_time = 0XFFFFFFFF;
	}

	/* Set the test start time. */
	ctrlInfo_ptr->StartTime = ctrlInfo_ptr->Interval_StartTime = xTaskGetTickCount();
	expire_time = ctrlInfo_ptr->StartTime + (ctrlInfo_ptr->TestTime);

	bandwidth_time = (long)targetTimePerPacket(ctrlInfo_ptr->bandwidth, ctrlInfo_ptr->PacketSize);
#if LWIP_IPV4 && LWIP_IPV6
    if (ctrlInfo_ptr->version == IP_VERSION_V6) {
        /* Write ABCs into the packet payload! */
        peer_ip6addr.sin6_family = AF_INET6;
        peer_ip6addr.sin6_addr.un.u32_addr[0] = htonl(ctrlInfo_ptr->ipv6[0]);
        peer_ip6addr.sin6_addr.un.u32_addr[1] = htonl(ctrlInfo_ptr->ipv6[1]);
        peer_ip6addr.sin6_addr.un.u32_addr[2] = htonl(ctrlInfo_ptr->ipv6[2]);
        peer_ip6addr.sin6_addr.un.u32_addr[3] = htonl(ctrlInfo_ptr->ipv6[3]);
        peer_ip6addr.sin6_port = htons((u16_t)(ctrlInfo_ptr->port));
    }
    else if (ctrlInfo_ptr->version == IP_VERSION_V4) {
        /* Write ABCs into the packet payload! */
        peer_addr.sin_family = AF_INET;
        peer_addr.sin_addr.s_addr = htonl(ctrlInfo_ptr->ip);
        peer_addr.sin_port = htons((u16_t)(ctrlInfo_ptr->port));
    }
#elif LWIP_IPV4
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_addr.s_addr = htonl(ctrlInfo_ptr->ip);
    peer_addr.sin_port = htons((u16_t)(ctrlInfo_ptr->port));
#elif LWIP_IPV6
    peer_ip6addr.sin6_family = AF_INET6;
    peer_ip6addr.sin6_addr.un.u32_addr[0] = htonl(ctrlInfo_ptr->ipv6[0]);
    peer_ip6addr.sin6_addr.un.u32_addr[1] = htonl(ctrlInfo_ptr->ipv6[1]);
    peer_ip6addr.sin6_addr.un.u32_addr[2] = htonl(ctrlInfo_ptr->ipv6[2]);
    peer_ip6addr.sin6_addr.un.u32_addr[3] = htonl(ctrlInfo_ptr->ipv6[3]);
    peer_ip6addr.sin6_port = htons((u16_t)(ctrlInfo_ptr->port));
#endif
	do {
		if (thread_udp_tx_iperf3[pair_no].state != IPERF_TX_THD_STATE_ACTIVE) {
			goto End_of_socket;
		}

#ifdef __ENABLE_UNUSED__ // not support
		if (ctrlInfo_ptr->bandwidth > 0) {
			startTime = R_BSP_SystemRtcCountGet();
		}
#endif	//__ENABLE_UNUSED__

		iperf_udp_head_ptr = (udp_payload *)send_data;
		iperf_udp_head_ptr->udp_id  = htonl(udp_id);
		sendtime = xTaskGetTickCount();
		iperf_udp_head_ptr->tv_sec  = htonl(sendtime / configTICK_RATE_HZ);
		iperf_udp_head_ptr->tv_usec = htonl((sendtime % configTICK_RATE_HZ) * (1000000 / configTICK_RATE_HZ));

#if LWIP_IPV4 && LWIP_IPV6
        if (ctrlInfo_ptr->version == IP_VERSION_V6) {
            res = (UINT)sendto(sockfd, send_data, ctrlInfo_ptr->PacketSize, 0,
                               (struct sockaddr *)&peer_ip6addr, sizeof(peer_ip6addr));
        } else if (ctrlInfo_ptr->version == IP_VERSION_V4) {
            res = (UINT)sendto(sockfd, send_data, ctrlInfo_ptr->PacketSize, 0,
                                (struct sockaddr *)&peer_addr, sizeof(peer_addr));
        }
#elif LWIP_IPV4
        res = (UINT)sendto(sockfd, send_data, ctrlInfo_ptr->PacketSize, 0,
                           (struct sockaddr *)&peer_addr, sizeof(peer_addr));
#elif LWIP_IPV6
        res = (UINT)sendto(sockfd, send_data, ctrlInfo_ptr->PacketSize, 0,
                           (struct sockaddr *)&peer_ip6addr, sizeof(peer_ip6addr));
#endif

#ifdef __DEBUG_IPERF3__
		IPERF3_MSG(CYAN_COLOR "[%s] sockfd:%d size:%d res:%d\n" CLEAR_COLOR,
									__func__, sockfd, ctrlInfo_ptr->PacketSize, res);
#endif

		if (res == ctrlInfo_ptr->PacketSize) {
#ifdef __ENABLE_UNUSED__  // not support
			if (ctrlInfo_ptr->bandwidth > 0) {

				stopTime = R_BSP_SystemRtcCountGet();

				Tx_ElapsedTme = sendElapsedTme(startTime, stopTime);

#ifdef PREVIOUS_PACKET_DELAY_COMPENSATION
				if (remain_time < 0) { // ?ÃŒ?Ã¼ Ã†Ã¤Ã…Â¶ ?Ã¼Â¼Ã› Â½ÃƒÂ°Â£ ÂºÂ¸Ã�Â¤
					remain_time = (bandwidth_time + remain_time) - (long)Tx_ElapsedTme;
				} else
#endif /* PREVIOUS_PACKET_DELAY_COMPENSATION */
				{
					remain_time = bandwidth_time - (long)Tx_ElapsedTme;
				}

				if (remain_time > 0) {
					//IPERF3_MSG(ANSI_COLOR_GREEN "r=%d - %5u = %5d usec\n" ANSI_COLOR_DEFULT, bandwidth_time, Tx_ElapsedTme, remain_time);
#ifdef PREVIOUS_PACKET_DELAY_COMPENSATION
					WaitOver = WaitT(remain_time);
#endif	// PREVIOUS_PACKET_DELAY_COMPENSATION
					remain_time = 0;

#ifdef PREVIOUS_PACKET_DELAY_COMPENSATION
					if (WaitOver) { /* wait ÃƒÃŠÂ°Ãº Â½ÃƒÂ°Â£ ÂºÂ¸Ã�Â¤ */
						//IPERF3_MSG("WO = %d ", WaitOver);
						//remain_time -= WaitOver;
						WaitOver = 0;
#ifdef __DEBUG_BANDWIDTH__
						waitOver_cnt++;
#endif /* __DEBUG_BANDWIDTH__ */
					}
#endif /* PREVIOUS_PACKET_DELAY_COMPENSATION */
#ifdef __DEBUG_BANDWIDTH__
					tx_undertime++;
				}
				else {
					IPERF3_MSG(ANSI_COLOR_RED "r= %5d - %5u = %5d usec\n" ANSI_COLOR_DEFULT, bandwidth_time, Tx_ElapsedTme, remain_time);
					tx_overtime++;
#endif /* __DEBUG_BANDWIDTH__ */
				}
				remain_time = 0;	// debug
			}
#endif	//__ENABLE_UNUSED__
			txcount++;
			/* Update the ID. */
			udp_id = (udp_id + 1) & 0x7FFFFFFF;

			/* Update the counter. */
			ctrlInfo_ptr->PacketsTxed++;
			ctrlInfo_ptr->BytesTxed += ctrlInfo_ptr->PacketSize;

			// If bandwidth is not controlled, sleep 10ms after sending 1000 packets.
			if (ctrlInfo_ptr->bandwidth == 0) {
				if (ctrlInfo_ptr->PacketsTxed % 1000 == 0) {
#ifdef __DEBUG_IPERF3__
					IPERF3_MSG("-s-");
#endif // __DEBUG_IPERF3__
					vTaskDelay(1);
				}
			}
		}
		else {
#ifdef __DEBUG_BANDWIDTH__
			error_cnt++;
#endif /* __DEBUG_BANDWIDTH__ */
			vTaskDelay(1); // When an error occurs, sleep for 10ms.
		//    taskYIELD();
		}

		current_time = xTaskGetTickCount();

		if (ctrlInfo_ptr->Interval > 0) {

			/* Â´Ã™?Â½ Ã�Â¤Â°Â¢ÃƒÃŠ Â±Ã®Ã�Ã¶ Â½ÃƒÂ°Â£ Â°Ã¨Â»Ãª */
			gap = (int)(ctrlInfo_ptr->Interval - ((ctrlInfo_ptr->Interval_StartTime - ctrlInfo_ptr->StartTime) % configTICK_RATE_HZ));

			if (res == ctrlInfo_ptr->PacketSize) {
				ctrlInfo_ptr->Interval_BytesTxed += ctrlInfo_ptr->PacketSize;
			}

			if ((ctrlInfo_ptr->Interval_StartTime < current_time) && (ctrlInfo_ptr->Interval_StartTime + (UINT)gap <= current_time)) {
				//IPERF3_MSG("gap=%03d ", gap);
#ifdef __DEBUG_BANDWIDTH__
				IPERF3_MSG("O:%04d U:%04d/T:%04d E%02d wo:%04d",
						tx_overtime,
						tx_undertime,
						tx_undertime+tx_overtime+error_cnt,
						error_cnt,
						waitOver_cnt);

				tx_overtime = 0;
				tx_undertime = 0;
				error_cnt = 0;
				waitOver_cnt = 0;
				//remain_time = 0;
#endif /* __DEBUG_BANDWIDTH__ */

				ctrlInfo_ptr->txCount++;

#if 0
				IPERF3_MSG("[%s] N:%d Bytes:0x%x ST:%d CT:%d IST:%d VER:%d IP:%s CP:%d BF:%c PT:%d\n",
					__func__,
					ctrlInfo_ptr->txCount,
					ctrlInfo_ptr->Interval_BytesTxed,
					ctrlInfo_ptr->StartTime,
					current_time,
					ctrlInfo_ptr->Interval_StartTime,
					ctrlInfo_ptr->version,
					ipaddr_ntoa(&ctrlInfo_ptr->remote_ip),
					ctrlInfo_ptr->port,
					ctrlInfo_ptr->bandwidth_format,
					PRINT_TYPE_UDP_TX_INT);
#endif
				/* print UDP Tx Interval */
				print_iperf3(ctrlInfo_ptr->txCount,
					(ULONG)ctrlInfo_ptr->Interval_BytesTxed,
					ctrlInfo_ptr->StartTime,
					current_time,
					ctrlInfo_ptr->Interval_StartTime,
					ctrlInfo_ptr->version,
					&ctrlInfo_ptr->remote_ip,
					(u16_t)ctrlInfo_ptr->port,
					ctrlInfo_ptr->bandwidth_format,
					PRINT_TYPE_UDP_TX_INT);

				ctrlInfo_ptr->Interval_BytesTxed = 0;
				ctrlInfo_ptr->Interval_StartTime = current_time; /* interval start time ÃƒÃŠÂ±Ã¢ÃˆÂ­ */
#ifdef __LIB_IPERF_PRINT_MIB__
				iperf_print_mib(ctrlInfo_ptr->mib_flag);
#endif /* __LIB_IPERF_PRINT_MIB__ */
			}
		}

		if ((ctrlInfo_ptr->send_num > 0) && (txcount == ctrlInfo_ptr->send_num)) {
			expire_time = 0;
		}

		if (ctrlInfo_ptr->transmit_rate) {		/* slow send */
			vTaskDelay(ctrlInfo_ptr->transmit_rate);
		}

	} while(current_time <= expire_time && !IP3_UDP_Tx_Finish_Flag);

#if defined (__SUPPORT_EEMBC__)
	th_printf("m-info-[Sent %llu datagrams (%llu Bytes)]\r\n",
			ctrlInfo_ptr->PacketsTxed, ctrlInfo_ptr->BytesTxed);
#endif // __SUPPORT_EEMBC__

	/* print UDP Tx Total */
	print_iperf3(ctrlInfo_ptr->txCount,
				(ULONG)ctrlInfo_ptr->BytesTxed,
				ctrlInfo_ptr->StartTime,
				current_time,
				ctrlInfo_ptr->StartTime,
				ctrlInfo_ptr->version,
				&ctrlInfo_ptr->remote_ip,
				ctrlInfo_ptr->remote_port,
				ctrlInfo_ptr->bandwidth_format,
				PRINT_TYPE_UDP_TX_TOL);

#ifdef __DEBUG_IPERF3__
	IPERF3_MSG("Sent %llu datagrams\n\n", ctrlInfo_ptr->PacketsTxed);
#endif

#ifdef __DEBUG_BANDWIDTH__
	IPERF3_MSG("Last Tx_ElapsedTme=%u\n", Tx_ElapsedTme);
	IPERF3_MSG("Last remain_time=%d\n", remain_time);
#endif /* __DEBUG_BANDWIDTH__ */

	if (ctrlInfo_ptr->send_num == 0) {
		udp_id++;

		for(int i = 0; i < 10; i++) {
			int len = 0;
			char buf[100];
			int addr_len;

			/* Send the end of the test signal*/
            iperf_udp_head_ptr = (udp_payload *)send_data;
            iperf_udp_head_ptr->udp_id  = htonl(0 - udp_id);
            sendtime = xTaskGetTickCount();
            iperf_udp_head_ptr->tv_sec  = htonl(sendtime / configTICK_RATE_HZ);
            iperf_udp_head_ptr->tv_usec = htonl((sendtime % configTICK_RATE_HZ) * (1000000 / configTICK_RATE_HZ));

#if LWIP_IPV4 && LWIP_IPV6
            if (ctrlInfo_ptr->version == IP_VERSION_V6) {
                sendto(sockfd, send_data, ctrlInfo_ptr->PacketSize, 0,
                    (struct sockaddr *)&peer_ip6addr, sizeof(peer_ip6addr));
            } else if (ctrlInfo_ptr->version == IP_VERSION_V4) {
                sendto(sockfd, send_data, ctrlInfo_ptr->PacketSize, 0,
                    (struct sockaddr *)&peer_addr, sizeof(peer_addr));
            }
#elif LWIP_IPV4
            sendto(sockfd, send_data, ctrlInfo_ptr->PacketSize, 0,
                (struct sockaddr *)&peer_addr, sizeof(peer_addr));
#elif LWIP_IPV6
            sendto(sockfd, send_data, ctrlInfo_ptr->PacketSize, 0,
                (struct sockaddr *)&peer_ip6addr, sizeof(peer_ip6addr));
#endif
			tv.tv_sec = 0;
			tv.tv_usec = 100000;
			res = (UINT)setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const void *)&tv, sizeof(struct timeval));

#if LWIP_IPV4 && LWIP_IPV6
            if (ctrlInfo_ptr->version == IP_VERSION_V6) {
                len = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *)&peer_ip6addr, (socklen_t *)&addr_len);
            } else if (ctrlInfo_ptr->version == IP_VERSION_V4) {
                len = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *)&peer_addr, (socklen_t *)&addr_len);
            }
#elif LWIP_IPV4
            len = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *)&peer_addr, (socklen_t *)&addr_len);
#elif LWIP_IPV6
            len = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *)&peer_ip6addr, (socklen_t *)&addr_len);
#endif
			if (len > 0) {
				break;
			}

		}
	}

End_of_socket:
	closesocket(sockfd);

end_of_task:
	if (ctrlInfo_ptr) {
		vPortFree(ctrlInfo_ptr);
	}

	if (send_data) {
		vPortFree(send_data);
	}

	if (IP3_UDP_Tx_Finish_Flag) {
		IPERF3_MSG(MSG_UDP_CLIENT_STOP_USER);
	}

	IP3_UDP_Tx_Finish_Flag = 1;

#if defined (__SUPPORT_EEMBC__)
	th_iperf_cb();
#endif // __SUPPORT_EEMBC__

	thread_udp_tx_iperf3[pair_no].handler = NULL;
	thread_udp_tx_iperf3[pair_no].state = IPERF_TX_THD_STATE_NONE;

	vTaskDelete(NULL);

	return;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UDP Client -- END //////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
#if defined ( __SUPPORT_WIFI_CONN_CB__ )

#ifdef __DA16400_PORT__
extern	void wifi_conn_notify_cb_regist(void (*user_cb)(void));
#endif	// __DA16400_PORT__
extern	void wifi_conn_fail_notify_cb_regist(void (*user_cb)(short reason_code));
extern	void wifi_disconn_notify_cb_regist(void (*user_cb)(short reason_code));

extern int get_run_mode(void);
SemaphoreHandle_t	iperf3_wifi_conn_notify_mutex = NULL;
/* Station mode */
unsigned char	iperf3_wifi_conn_flag			= FALSE;
unsigned char	iperf3_wifi_disconn_flag			= FALSE;
short			iperf3_wifi_disconn_reason		= 0;
/* AP mode */
unsigned char	iperf3_ap_wifi_conn_flag			= FALSE;
unsigned char	iperf3_ap_wifi_disconn_flag		= FALSE;
short			iperf3_ap_wifi_disconn_reason	= 0;

static void iperf_wifi_conn_cb(void)
{
	IPERF3_MSG(" [%s] Called. \n", __func__);

	/* Wait until 3 seconds to get mutex */
	if (xSemaphoreTake(iperf3_wifi_conn_notify_mutex, 300) != pdTRUE) {
		IPERF3_MSG(RED_COLOR " Failed to get iperf3_wifi_conn_notify_mutex during 3 seconds !!!\n" CLEAR_COLOR);
		return;
	}

	if (get_run_mode() == WIFI_DEVICE_MODE_EXT_AP) {
		iperf3_ap_wifi_conn_flag = TRUE;
	}
	else {
		iperf3_wifi_conn_flag = TRUE;
	}

	xSemaphoreGive(iperf3_wifi_conn_notify_mutex);
}

static void iperf_wifi_disconn_cb(short reason_code)
{
	int	i;

	IPERF3_MSG(" [%s] Called(%d) \n", __func__, reason_code);

	/* Wait until 3 seconds to get mutex */
	if (xSemaphoreTake(iperf3_wifi_conn_notify_mutex, 300) != pdTRUE) {
		IPERF3_MSG(RED_COLOR " Failed to get iperf3_wifi_conn_notify_mutex during 3 seconds !!!\n" CLEAR_COLOR);
		return;
	}

	if (get_run_mode() == WIFI_DEVICE_MODE_EXT_AP) {
		iperf3_ap_wifi_disconn_flag = TRUE;
		iperf3_ap_wifi_disconn_reason = reason_code;
	}
	else {
		iperf3_wifi_disconn_flag	= TRUE;
		iperf3_wifi_disconn_reason = reason_code;
	}

	IP3_TCP_Rx_Finish_Flag = 1;
	IP3_UDP_Rx_Finish_Flag = 1;

	for (i = 0; i < IPERF_TCP_TX_MAX_PAIR; i++) {
		if (thread_tcp_tx_iperf3[i].state != IPERF_TX_THD_STATE_NONE) {
			if (terminate_iperf_tx_thd(&(thread_tcp_tx_iperf3[i]))) {
				IPERF3_MSG(" iPerf3 TCP Client Pair %d is deleted\n", i);
			} else {
				IPERF3_MSG(" Failed to delete iPerf3 TCP Client Pair %d\n", i);
				break;
			}
		}
	}

	for (i = 0; i < IPERF_UDP_TX_MAX_PAIR; i++) {
		if (thread_udp_tx_iperf3[i].state != IPERF_TX_THD_STATE_NONE) {
			if (terminate_iperf_tx_thd(&(thread_udp_tx_iperf3[i]))) {
				IPERF3_MSG(" iPerf3 UDP Client Pair %d is deleted\n", i);
			} else {
				IPERF3_MSG(" Failed to delete iPerf3 UDP Client Pair %d\n", i);
				break;
			}
		}
	}

	xSemaphoreGive(iperf3_wifi_conn_notify_mutex);
}
#endif	// defined ( __SUPPORT_WIFI_CONN_CB__ )


ip_addr_t	targetIpIperf3;
extern unsigned char ipv6_support_flag;
UINT	iperf3_cli(UCHAR iface, UCHAR iperf_mode, struct IPERF_CONFIG *config)
{
	extern unsigned int ra6w1_network_main_check_network_ready(unsigned char iface);

	BaseType_t	xReturned;

	ctrl3_info_t 	*iperf_ctrlInfo_ptr;

	if (config->term == pdTRUE) {
		switch (iperf_mode) {
			case IPERF_SERVER_TCP:
				IP3_TCP_Rx_Finish_Flag = 1;
				break;
#if 0
			case IPERF_SERVER_UDP:
				IP3_UDP_Rx_Finish_Flag = 1;
				break;

			case IPERF_CLIENT_TCP:

				if (terminate_iperf_tx_thd(&(thread_tcp_tx_iperf3[config->pair]))) {
					IPERF3_MSG(" iPerf3 TCP Client Pair %d is deleted\n", config->pair);
				} else {
					IPERF3_MSG(" Failed to delete iPerf3 TCP Client Pair %d\n", config->pair);
				}

				break;

			case IPERF_CLIENT_UDP:

				if (terminate_iperf_tx_thd(&(thread_udp_tx_iperf3[config->pair]))) {
					IPERF3_MSG(" iPerf3 UDP Client Pair %d is deleted\n", config->pair);
				} else {
					IPERF3_MSG(" Failed to delete iPerf3 UDP Client Pair %d\n", config->pair);
				}

				break;
#endif
		}
		return pdTRUE;
	}

	if (ra6w1_network_main_check_network_ready(iface) == pdFALSE) {
		IPERF3_MSG("Network Not Ready!!!\n");
		return pdFALSE;
	}

	struct netif *netif = netif_get_by_index(iface + 2);
#if LWIP_IPV4 && LWIP_IPV6
    if (lwip_htonl(config->ipaddr) == ip4_addr_get_u32(&netif->ip_addr.u_addr.ip4)) {
        IPERF3_MSG("iPerf3 Same IPaddress!!!\n");
        return pdFALSE;
    }
#elif LWIP_IPV4
    if (lwip_htonl(config->ipaddr) == ip4_addr_get_u32(&netif->ip_addr)) {
        IPERF3_MSG("iPerf3 Same IPaddress!!!\n");
        return pdFALSE;
    }
#endif

	iperf_ctrlInfo_ptr = NULL;
	iperf_ctrlInfo_ptr = (ctrl3_info_t *)pvPortMalloc(sizeof(ctrl3_info_t));

	if (iperf_ctrlInfo_ptr == NULL) {
		IPERF3_MSG(RED_COLOR "[%s] pvPortMalloc error\n" CLEAR_COLOR, __func__);
		return 1;
	}

	memset((ctrl3_info_t*)iperf_ctrlInfo_ptr, 0, sizeof(ctrl3_info_t));

	iperf_ctrlInfo_ptr->iface = iface;	/* not used */

	iperf_ctrlInfo_ptr->transmit_rate = config->transmit_rate;

#if LWIP_IPV4 && LWIP_IPV6
	if (config->ip_ver == IP_VERSION_V6) {	/* IPv6 */
		iperf_ctrlInfo_ptr->version = IP_VERSION_V6;
		iperf_ctrlInfo_ptr->remote_ip.type = IPADDR_TYPE_V6;
		memcpy(iperf_ctrlInfo_ptr->ipv6, config->ipv6addr, sizeof(ULONG)*4);
	} else {	/* IPv4 */
		iperf_ctrlInfo_ptr->version = IP_VERSION_V4;
		iperf_ctrlInfo_ptr->remote_ip.type = IPADDR_TYPE_V4;
		iperf_ctrlInfo_ptr->ip = config->ipaddr;
	}
#elif LWIP_IPV4
	iperf_ctrlInfo_ptr->version = IP_VERSION_V4;
	iperf_ctrlInfo_ptr->ip = config->ipaddr;
#elif LWIP_IPV6
	iperf_ctrlInfo_ptr->version = IP_VERSION_V6;
	memcpy(iperf_ctrlInfo_ptr->ipv6, config->ipv6addr, sizeof(ULONG)*4);
#endif

	iperf_ctrlInfo_ptr->TestTime = config->TestTime;

	iperf_ctrlInfo_ptr->bandwidth_format = config->bandwidth_format;

	if (config->RxTimeOut == 0) {
		iperf_ctrlInfo_ptr->RxTimeOut = IPERF_RX_TIMEOUT_DEFAULT;
	}
	else {
		iperf_ctrlInfo_ptr->RxTimeOut = config->RxTimeOut;
	}

	/* TCP Window size */
	if (config->window_size == 0) {
		if (iperf_mode == IPERF_SERVER_TCP) {
#if 0 //RA6W1 New SSK and UMAC TWIG
			iperf_ctrlInfo_ptr->window_size = 32*1024;
#else
			iperf_ctrlInfo_ptr->window_size = 30*1024;
#endif
		}
		else {
			iperf_ctrlInfo_ptr->window_size = 65535;
		}
	}
	else {
		iperf_ctrlInfo_ptr->window_size = (config->window_size * 1024);
	}

	iperf_ctrlInfo_ptr->PacketSize = config->PacketSize;

	iperf_ctrlInfo_ptr->send_num = config->sendnum;

	if (config->bandwidth > 100) {
		config->bandwidth = 0;
	}
	else {
		/* Mbps to bps*/
		iperf_ctrlInfo_ptr->bandwidth = config->bandwidth * 1000000;
	}

#ifdef __LIB_IPERF_PRINT_MIB__
	iperf_ctrlInfo_ptr->mib_flag = config->mib_flag;
#endif /* __LIB_IPERF_PRINT_MIB__ */

	if (config->port >= 5001 && config->port < 32768) {
		iperf_ctrlInfo_ptr->port = config->port;
	}
	else {
		iperf_ctrlInfo_ptr->port = LWIPERF3_TCP_PORT_DEFAULT;
	}

	if (config->Interval > 10) {
		iperf_ctrlInfo_ptr->Interval = config->Interval;
	}

	if (config->tcp_api_mode < 3) {
		iperf_ctrlInfo_ptr->tcpApiMode = config->tcp_api_mode;
//		IPERF3_MSG(CYAN_COLOR "[%s] iPerf3 TCP API mode: %d\n" CLEAR_COLOR,
//								__func__, iperf_ctrlInfo_ptr->tcpApiMode);
	}

	iperf3_data = xSemaphoreCreateMutex();
#if defined ( __SUPPORT_WIFI_CONN_CB__ )
	iperf3_wifi_conn_notify_mutex = xSemaphoreCreateMutex();

	if (iperf3_wifi_conn_notify_mutex == NULL) {
		IPERF3_MSG(RED_COLOR " >>> Failed to create Wi-Fi connection notify call-back mutex !!!\n" CLEAR_COLOR);
		return pdTRUE;
	}


	/* Wi-Fi connection call-back */
	wifi_conn_notify_cb_regist(iperf_wifi_conn_cb);

	/* Wi-Fi disconnection call-back */
	wifi_disconn_notify_cb_regist(iperf_wifi_disconn_cb);
#endif	// defined ( __SUPPORT_WIFI_CONN_CB__ )

	switch (iperf_mode) {

		case IPERF_SERVER_TCP:
			if (thread_tcp_rx_iperf3 != NULL) {
				IPERF3_MSG("iPerf3 TCP Server is already exist.\n");
				vPortFree(iperf_ctrlInfo_ptr);
				return pdTRUE;
			}

			iperf_ctrlInfo_ptr->reportFunc = lwiperf3_report_func;

			iperf3_TCP_RX_ctrl_info = iperf_ctrlInfo_ptr;
			xReturned = xTaskCreate(lwiperf3_tcp_server,
							"iPerf3_TCP_Rx",
							1024,
							(void *)iperf_ctrlInfo_ptr,
							//tskIDLE_PRIORITY+1,
							IPERF3_TCP_SERVER_PRIORITY,
							&iperf3_tcp_rx_handler);
			if (xReturned != pdPASS) {
				IPERF3_MSG(RED_COLOR "[%s] Failed task create %s\n" CLEAR_COLOR, __func__, "iperf_udp_rx");
				thread_tcp_rx_iperf3 = NULL;
			}
			else {
				thread_tcp_rx_iperf3 = &iperf3_tcp_rx_handler;
			}

			break;
#if 0
		case IPERF_CLIENT_TCP:

			if (thread_tcp_tx_iperf3[config->pair].state != IPERF_TX_THD_STATE_NONE) {
				if (terminate_iperf_tx_thd(&(thread_tcp_tx_iperf3[config->pair]))) {
					IPERF3_MSG(" iPerf3 TCP Client Pair %d is deleted\n", config->pair);
				} else {
					IPERF3_MSG(" Failed to delete iPerf3 TCP Client Pair %d\n", config->pair);
					vPortFree(iperf_ctrlInfo_ptr);
					break;
				}
			}

			iperf_ctrlInfo_ptr->reportFunc = lwiperf3_report_func;
			iperf_ctrlInfo_ptr->pair_no = config->pair;

			memset(&iperf_ctrlInfo_ptr->remote_ip, 0, sizeof(ip_addr_t));

#if LWIP_IPV4 && LWIP_IPV6
            if (iperf_ctrlInfo_ptr->version == IP_VERSION_V6) {
                iperf_ctrlInfo_ptr->remote_ip.type = IPADDR_TYPE_V6;
                iperf_ctrlInfo_ptr->remote_ip.u_addr.ip6.addr[0] = lwip_htonl(iperf_ctrlInfo_ptr->ipv6[0]);
                iperf_ctrlInfo_ptr->remote_ip.u_addr.ip6.addr[1] = lwip_htonl(iperf_ctrlInfo_ptr->ipv6[1]);
                iperf_ctrlInfo_ptr->remote_ip.u_addr.ip6.addr[2] = lwip_htonl(iperf_ctrlInfo_ptr->ipv6[2]);
                iperf_ctrlInfo_ptr->remote_ip.u_addr.ip6.addr[3] = lwip_htonl(iperf_ctrlInfo_ptr->ipv6[3]);
            } else if (iperf_ctrlInfo_ptr->version == IP_VERSION_V4) {
                ip4_addr_set_u32(&iperf_ctrlInfo_ptr->remote_ip.u_addr.ip4, lwip_htonl(iperf_ctrlInfo_ptr->ip));
            }
#elif LWIP_IPV4
            ip4_addr_set_u32(&iperf_ctrlInfo_ptr->remote_ip, lwip_htonl(iperf_ctrlInfo_ptr->ip));
#elif LWIP_IPV6
            iperf_ctrlInfo_ptr->remote_ip.addr[0] = lwip_htonl(iperf_ctrlInfo_ptr->ipv6[0]);
            iperf_ctrlInfo_ptr->remote_ip.addr[1] = lwip_htonl(iperf_ctrlInfo_ptr->ipv6[1]);
            iperf_ctrlInfo_ptr->remote_ip.addr[2] = lwip_htonl(iperf_ctrlInfo_ptr->ipv6[2]);
            iperf_ctrlInfo_ptr->remote_ip.addr[3] = lwip_htonl(iperf_ctrlInfo_ptr->ipv6[3]);
#endif

			if (config->port >= 5001 && config->port < 32768) {
				iperf_ctrlInfo_ptr->remote_port = (u16_t)config->port;
			} else {
				iperf_ctrlInfo_ptr->remote_port = LWIPERF3_TCP_PORT_DEFAULT;
			}

			iperf3_TCP_TX_ctrl_info[config->pair] = iperf_ctrlInfo_ptr;
			xReturned = xTaskCreate(lwiperf_tcp_client,
							"iPerf_TCP_Tx",
							//1024,
							2048,
							(void *)iperf_ctrlInfo_ptr,
							//tskIDLE_PRIORITY+1,
							IPERF_TCP_CLIENT_PRIORITY,
							&(thread_tcp_tx_iperf3[config->pair].handler));
			if (xReturned != pdPASS) {
				IPERF3_MSG(RED_COLOR "[%s] Failed task create %s\n" CLEAR_COLOR, __func__, "iperf_tcp_tx");
			}

			break;

		case IPERF_SERVER_UDP:

			if (thread_udp_rx_iperf3 != NULL) {
				IPERF3_MSG("iPerf3 UDP Server is already exist.\n");
				vPortFree(iperf_ctrlInfo_ptr);
				return pdTRUE;
			}

			iperf3_UDP_RX_ctrl_info = iperf_ctrlInfo_ptr;
			xReturned = xTaskCreate(lwiperf_udp_server,
							"iPerf_UDP_Rx",
							1024,
							(void *)iperf_ctrlInfo_ptr,
							//tskIDLE_PRIORITY+1,
							IPERF_UDP_SERVER_PRIORITY,
							&iperf3_udp_rx_handler);
			if (xReturned != pdPASS) {
				IPERF3_MSG(RED_COLOR "[%s] Failed task create %s\n" CLEAR_COLOR, __func__, "iperf_udp_rx");
				thread_udp_rx_iperf3 = NULL;
			}
			else {
				thread_udp_rx_iperf3 = &iperf3_udp_rx_handler;
			}

			break;

		case IPERF_CLIENT_UDP:

			if (thread_udp_tx_iperf3[config->pair].state != IPERF_TX_THD_STATE_NONE) {
				if (terminate_iperf_tx_thd(&(thread_udp_tx_iperf3[config->pair]))) {
					IPERF3_MSG(" iPerf3 UDP Client Pair %d is deleted\n", config->pair);
				} else {
					IPERF3_MSG(" Failed to delete iPerf3 UDP Client Pair %d\n", config->pair);
					vPortFree(iperf_ctrlInfo_ptr);
					break;
				}
			}

			memset(&iperf_ctrlInfo_ptr->remote_ip, 0, sizeof(ip_addr_t));
#if LWIP_IPV4 && LWIP_IPV6
            if (iperf_ctrlInfo_ptr->version == IP_VERSION_V6) {
                iperf_ctrlInfo_ptr->remote_ip.type = IPADDR_TYPE_V6;
                iperf_ctrlInfo_ptr->remote_ip.u_addr.ip6.addr[0] = lwip_htonl(iperf_ctrlInfo_ptr->ipv6[0]);
                iperf_ctrlInfo_ptr->remote_ip.u_addr.ip6.addr[1] = lwip_htonl(iperf_ctrlInfo_ptr->ipv6[1]);
                iperf_ctrlInfo_ptr->remote_ip.u_addr.ip6.addr[2] = lwip_htonl(iperf_ctrlInfo_ptr->ipv6[2]);
                iperf_ctrlInfo_ptr->remote_ip.u_addr.ip6.addr[3] = lwip_htonl(iperf_ctrlInfo_ptr->ipv6[3]);
            } else if (iperf_ctrlInfo_ptr->version == IP_VERSION_V4) {
                ip4_addr_set_u32(&iperf_ctrlInfo_ptr->remote_ip.u_addr.ip4, lwip_htonl(iperf_ctrlInfo_ptr->ip));
            }
#elif LWIP_IPV4
            ip4_addr_set_u32(&iperf_ctrlInfo_ptr->remote_ip, lwip_htonl(iperf_ctrlInfo_ptr->ip));
#elif LWIP_IPV6
            iperf_ctrlInfo_ptr->remote_ip.addr[0] = lwip_htonl(iperf_ctrlInfo_ptr->ipv6[0]);
            iperf_ctrlInfo_ptr->remote_ip.addr[1] = lwip_htonl(iperf_ctrlInfo_ptr->ipv6[1]);
            iperf_ctrlInfo_ptr->remote_ip.addr[2] = lwip_htonl(iperf_ctrlInfo_ptr->ipv6[2]);
            iperf_ctrlInfo_ptr->remote_ip.addr[3] = lwip_htonl(iperf_ctrlInfo_ptr->ipv6[3]);
#endif

			iperf_ctrlInfo_ptr->port = LWIPERF3_TCP_PORT_DEFAULT;

#ifdef __DEBUG_IPERF3__
			IPERF3_MSG(" Server addr: %s:%d\n",
													ipaddr_ntoa(&iperf_ctrlInfo_ptr->remote_ip),
													iperf_ctrlInfo_ptr->port);
#endif // __DEBUG_IPERF3__

			iperf3_UDP_TX_ctrl_info[config->pair] = iperf_ctrlInfo_ptr;
			xReturned = xTaskCreate(lwiperf_udp_client,
							"iPerf_UDP_Tx",
							//1024,
							3096,
							(void *)iperf_ctrlInfo_ptr,
							//tskIDLE_PRIORITY+1,
							IPERF_UDP_CLIENT_PRIORITY,
							&(thread_udp_tx_iperf3[config->pair].handler));
			if (xReturned != pdPASS) {
				IPERF3_MSG(RED_COLOR "[%s] Failed task create %s\n" CLEAR_COLOR, __func__, "iperf_udp_tx");
			}

			break;
#endif
		default:
			IPERF3_MSG(RED_COLOR " Unknown mode???\n" CLEAR_COLOR);
			break;
	}
	return pdTRUE;
}

#endif /* LWIP_TCP && LWIP_CALLBACK_API */

#endif // __SUPPORT_IPERF__

/* EOF */
