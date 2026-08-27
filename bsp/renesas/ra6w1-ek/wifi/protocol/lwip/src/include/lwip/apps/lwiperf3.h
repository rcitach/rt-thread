#ifndef LWIP_HDR_APPS_LWIPERF3_H
#define LWIP_HDR_APPS_LWIPERF3_H


#include "lwip/opt.h"
#include "lwip/ip_addr.h"
#include "lwip/tcp.h"
#include "lwip/apps/lwiperf.h"

#ifdef __cplusplus
extern "C" {
#endif

#define USING_LIST_DELETE 1
#define LWIPERF3_TCP_PORT_DEFAULT  5201

#define DATA_TRANSFER_STATE   20
#define DATA_RESULT_STATE     21
#define DATA_SENDING_INIT     22
#define DATA_SENDING_STATE    23
#define DATA_SENDING_END      24
#define TCP_SOCKET_IDENTIFIER_LEN 37
typedef enum
{
  eTCP_0_WaitName,	/* Expect a text like "osboxes.1460335312.612572.527642c36f" */
  eTCP_1_WaitCount,
  eTCP_2_WaitHeader,
  eTCP_3_WaitOneTwo,
  eTCP_4_WaitCount2,
  eTCP_5_WaitHeader2,
  eTCP_6_WaitDone,
  eTCP_7_WaitTransfer,
  eTCP_8_WaitDisplay
} eTCP_Server_Status_t;

typedef enum {
  tcp_One_PacketCountCopy = 1,
  tcp_Two_PacketState = 2,
  tcp_Three_PacketCountGet = 3,
  tcp_four_PacketCountGet = 4,
  tcp_five_PacketCountGet = 5
} tcp_socket_state;

UINT  iperf3_cli(UCHAR iface, UCHAR iperf_mode, struct IPERF_CONFIG *config);
#ifndef MULTI_THREAD_MULTI_SOCKET
int api_mode_socket_server3(void *pvParameters);
#endif
/* start FOR_UDP */
typedef struct _lwiperf3_state_base lwiperf3_state_base_t;

struct _lwiperf3_state_base {
  /* linked list */
  lwiperf3_state_base_t *next;
  /* 1=tcp, 0=udp */
  u8_t tcp;
  /* 1=server, 0=client */
  u8_t server;
  struct tcp_pcb conn_pcb;
  u8_t state;
  char data[TCP_SOCKET_IDENTIFIER_LEN];
  u32_t bytes;
  u32_t bIsControl;
  u32_t bReverse;
  u32_t ulAmount;
  u32_t xRemainingTime;
  /* master state used to abort sessions (e.g. listener, main client) */
  lwiperf3_state_base_t *related_master_state;
};

/** Connection handle for a TCP iperf session */
typedef struct _lwiperf3_state_tcp {
	lwiperf3_state_base_t	base;
	struct altcp_pcb		*server_alpcb;
	struct altcp_pcb		*conn_alpcb;
	struct tcp_pcb			*server_pcb;
	struct tcp_pcb			*conn_pcb;
	u32_t time_started;
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
	unsigned long			expire_time;
} lwiperf3_state_tcp_t;

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
	UINT	StartTime;
	UINT	RunTime;
	UINT	TestTime;
	UINT	Interval;
	UINT	PacketSize;
	ULONG	wmm_tos;
	UINT	send_num;
	UINT	bandwidth;
	UCHAR	bandwidth_format;
	UINT	RxTimeOut;
	UINT	window_size;
#ifdef __LIB_IPERF_PRINT_MIB__
	UINT	mib_flag;
#endif /* __LIB_IPERF_PRINT_MIB__ */
	UINT	transmit_rate;

	UINT	txCount;
	UINT	rxCount;
	UINT	Interval_StartTime;
	UINT	Interval_BytesRxed;
	UINT	Interval_BytesTxed;

	ip_addr_t	local_ip;
	u16_t		local_port;
	ip_addr_t	remote_ip;
	u16_t		remote_port;

	lwiperf_report_fn	reportFunc;
	UINT	pair_no;
	lwiperf3_state_tcp_t	lwiperf_state;
	int		socket_fd;
	UINT	tcpApiMode;
#ifdef MULTI_THREAD_MULTI_SOCKET
	UINT	taskNo;		/* Only for tcp server */
#endif
} ctrl3_info_t;

#endif /* LWIP_HDR_APPS_LWIPERF_H */
