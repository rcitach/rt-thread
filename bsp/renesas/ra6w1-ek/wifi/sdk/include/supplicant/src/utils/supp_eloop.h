/**
 *****************************************************************************************
 * @file    supp_eloop.h
 * @brief   Event loop from wpa_supplicant-2.4
 *****************************************************************************************
 */

/*
 * Event loop
 * Copyright (c) 2002-2006, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 *
 * This file defines an event loop interface that supports processing events
 * from registered timeouts (i.e., do something after N seconds), sockets
 * (e.g., a new packet available for reading), and signals. eloop.c is an
 * implementation of this interface using select() and sockets. This is
 * suitable for most UNIX/POSIX systems. When porting to other operating
 * systems, it may be necessary to replace that implementation with OS specific
 * mechanisms.
 *
 * Copyright (c) 2021-2022 Modified by Renesas Electronics.
 */

//MODIFY_SUPPLICANT_FOR_FREERTOS

/**
 *****************************************************************************************
 * @file    supp_eloop.h
 * @brief   Event loop from wpa_supplicant-2.4
 *****************************************************************************************
 */

/*
 * Event loop
 * Copyright (c) 2002-2006, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 *
 * This file defines an event loop interface that supports processing events
 * from registered timeouts (i.e., do something after N seconds), sockets
 * (e.g., a new packet available for reading), and signals. eloop.c is an
 * implementation of this interface using select() and sockets. This is
 * suitable for most UNIX/POSIX systems. When porting to other operating
 * systems, it may be necessary to replace that implementation with OS specific
 * mechanisms.
 *
 * Copyright (C) 2023-2024 Modified by Renesas Electronics
 */


#ifndef SUPP_ELOOP_H
#define SUPP_ELOOP_H


#include "FreeRTOS.h"
#include "timers.h"
#include "supp_common.h" /* For ETH_ALEN */

#define eloop_sock_handler      ra6wx_eloop_sock_handler
#define eloop_signal_handler    ra6wx_eloop_signal_handler

/* Assign 1 flag for fc80211_driver */
#define RA6WX_SP_DRV_FLAG		0x80000000
#define RA6WX_SP_PROBE_FLAG		0x40000000

/* Assign 2 flags for fc9000 supplicant cli */
#define	RA6WX_SP_CLI_TX_FLAG		0x00000001
#define	RA6WX_SP_CLI_RX_FLAG		0x00000002

/* Assign 1 flag for l2_packet_rx */
#define	RA6WX_L2_PKT_RX_EV		0x00000010
#ifdef CONFIG_RECEIVE_DHCP_EVENT
#define	RA6WX_DHCP_EV			0x00000020
#endif /* CONFIG_RECEIVE_DHCP_EVENT */

#define	RA6WX_STOP_EV			0x00000040

#ifdef CONFIG_IMMEDIATE_SCAN
/* Scan result event from supp to cli */
#define RA6W_SCAN_RESULTS_RX_EV		0x00000020
#define RA6W_SCAN_RESULTS_TX_EV		0x00000040
#define RA6WX_SCAN_RESULTS_FAIL_EV      0x00000080
#endif /* CONFIG_IMMEDIATE_SCAN */

/* FC9000 Message Queue total size */
#define	RA6WX_QUEUE_BUF_SIZE	192
#define TO_SUPP_QUEUE_SIZE		128
#define TO_CLI_QUEUE_SIZE		1

#define RA6WX_MAX_CLI_CMD		4
#define	RA6WX_TX_QSIZE			16	// TX_1_ULONG * 4
#define	RA6WX_RX_QSIZE			8	// TX_1_ULONG * 2

#if 1//def	__WPA3_WIFI_CERTFIED__
#define	RA6WX_DRV_EV_DATA_SIZE		2300
#else
#define	RA6WX_DRV_EV_DATA_SIZE		512     //4096 -> 512, 20150729_trinity
#endif	/* __WPA3_WIFI_CERTFIED__ */

#define RA6WX_MAX_DRV_EVENTS		8       //4->8, 20150729_trinity
#define RA6WX_DRV_EV_DATA_BLK_POOL_ERR	1

typedef struct ra6wx_drv_event_attr {
	u16 softmac_idx;
	u16 ifindex;
	u16 wdev_id;
	u16 timed_out;
	int freq;
	int sig_dbm;
	ULONG cookie;
	int ack;
	u8 addr[ETH_ALEN];
	u16 status;
	u16 reason;
	u16 disconnected_by_ap;
	u16 req_ie_len;
	u16 resp_ie_len;
	u16 attr_ie_len;
	u8 n_ssids;
	u8 n_channels;
	u32 flags;
	int duration;
} ra6wx_drv_event_attr_t;

/* Drv event data from UMAC */
typedef struct ra6wx_drv_event_data {
	ra6wx_drv_event_attr_t	attr;
	int	data_len;
	char	data[];
} ra6wx_drv_ev_data_t;

/* FC80211 Link Layer -> Supplicant RX Buffer */
typedef struct ra6wx_drv_msg_buffer {
	ULONG   temp[2];
	UINT	cmd;
	struct ra6wx_drv_event_data *data;
} ra6wx_drv_msg_buf_t;

/****************************************************************/

#define	CLI_SCAN_RSP_BUF_SIZE		6144

#define RA6WX_MAX_CLI_RX_EVENTS			1
#define RA6WX_CLI_RX_EV_DATA_BLK_POOL_ERR	1
#define RA6WX_CLI_CMD_TX_BLK_POOL_ERR		2

typedef struct ra6wx_cli_cmd_buffer {
	char	cmd_str[RA6WX_QUEUE_BUF_SIZE];
} ra6wx_cli_cmd_buf_t;

typedef struct ra6wx_cli_tx_msg {
	ra6wx_cli_cmd_buf_t *cli_tx_buf;
} ra6wx_cli_tx_msg_t;

/* CLI event data from SP */
typedef struct ra6wx_cli_rx_event_data {
	/* ra6wx_cli_event_attr_t attr; */
	char	data[CLI_SCAN_RSP_BUF_SIZE];
	int	data_len;
} ra6wx_cli_rx_ev_data_t;

/* Supplicant -> CLI RSP Buffer */
typedef struct ra6w1_cli_rsp_buffer {
	struct ra6wx_cli_rx_event_data *event_data;
} ra6w1_cli_rsp_buf_t;


/////////////////////////////////////////////////////////////////////////////

/* !!! NOTICE !!!
 * These values are common code between supplicant and command_net.c
 */

/*
 * #ifdef CONFIG_WIFI_MONITOR || #ifdef	__SUPPORT_WIFI__
 */

#define WIFI_MONITOR_EV_DATA_SIZE	52 //64->52, 20170223_trinity

/* Supplicant -> App Event Buffer */

typedef struct wifi_monitor_msg_buffer {
    UINT    cmd;
    UINT    data_len;
    ULONG  value;  //__SUPPORT_ASD__, 20170223_trinity
    CHAR    data[WIFI_MONITOR_EV_DATA_SIZE];
} wifi_monitor_msg_buf_t;

#define WIFI_EVENT_SEND_MSG_TO_APP	0x01
#define WIFI_EVENT_SEND_MSG_TO_UART	0x02

#define WIFI_CMD_P2P_READ_AP_STR	0
#define WIFI_CMD_P2P_READ_PIN		1
#define WIFI_CMD_P2P_READ_MAIN_STR	2
#define WIFI_CMD_P2P_READ_GID_STR	3
#define WIFI_CMD_SEND_DHCP_RECONFIG	4
#define WIFI_CMD_ASD_BCN_NOTI		5		/* Notification */

#if 0
/* __SUPPORT_VIRTUAL_ONELINK__ */
#define WIFI_CMD_FACTORY_PROMISC	6		/* Promiscuous mode */
struct smart_config_send_event{
  int freq;
  int size;
  u8 data[3];
};
typedef enum promisc_cmd {
	PROMISC_CMD_IDLE = 0,
	PROMISC_CMD_ENABLE = 1,
	PROMISC_CMD_DISABLE = 2,
	PROMISC_CMD_FREQ,
	PROMISC_CMD_SSID,
	PROMISC_CMD_PWD,
	PROMISC_CMD_PAIRWISE,
	PROMISC_CMD_APP_ADDRESS,
	PROMISC_CMD_VOL_DONE,
#if 1 /* FEATURE_VOL_ONE_EVENT */
	PROMISC_CMD_ALL_DATA,
#endif
#if 1//def VOL_EVENT_THREAD
	PROMISC_SCONFIG_COMPLETE,
	PROMISC_CHANNEL_STARTING = 11,
	PROMISC_CHANNEL_EVENT = 12,
	PROMISC_CHANNEL_EVENT_START = 13,
	PROMISC_CONNNECTION_DATA_EVENT = 14,
	PROMISC_CONNNECTION_DATA_EVENT_S = 15,
	PROMISC_CMD_CANCEL = 16,
#endif
	PROMISC_CMD_DONE,
}PROMISC_CMD;

typedef enum vol_pairwise_set {
	VOL_PAIRWISE_OPEN = 0,
	VOL_PAIRWISE_WEP40 = 1,
	VOL_PAIRWISE_TKIP = 2,
	VOL_PAIRWISE_CCMP = 4,
	VOL_PAIRWISE_WEP104=5,
	VOL_PAIRWISE_UNKNOWN,
	VOL_PAIRWISE_END,
}VOL_PAIRWISE_SET;

#if 1//def VOL_EVENT_THREAD	
struct vol_event_send{
	int event_type;
	int channel;
	u8 ssid[32];
	int ssid_len;
	u8 passwd[128];
	int passwd_len;
	int pairwise;
	u8 address[4];
};

#define VOL_EVENT_Q_COUNT 5
extern OAL_QUEUE	vol_queue;
extern ULONG		vol_queuebuf[VOL_EVENT_Q_COUNT];
#endif
#endif
/////////////////////////////////////////////////////////////////////////////

/****************************************************************/

/**
 * eloop_event_type - eloop socket event type for eloop_register_sock()
 * @EVENT_TYPE_READ: Socket has data available for reading
 * @EVENT_TYPE_WRITE: Socket has room for new data to be written
 * @EVENT_TYPE_EXCEPTION: An exception has been reported
 */
typedef enum {
	EVENT_TYPE_READ = 0,
	EVENT_TYPE_WRITE,
	EVENT_TYPE_EXCEPTION
} eloop_event_type;

/**
 * eloop_sock_handler - eloop socket event callback type
 * @sock: File descriptor number for the socket
 * @eloop_ctx: Registered callback context data (eloop_data)
 * @sock_ctx: Registered callback context data (user_data)
 */
typedef void (*eloop_sock_handler)(int sock, void *eloop_ctx, void *sock_ctx);

/**
 * eloop_event_handler - eloop generic event callback type
 * @eloop_ctx: Registered callback context data (eloop_data)
 * @user_ctx: Registered callback context data (user_data)
 */
//typedef void (*eloop_event_handler)(void *eloop_ctx, void *user_ctx);
typedef void (*ra6wx_eloop_event_handler)(void *eloop_ctx, void *user_ctx);

/**
 * eloop_timeout_handler - eloop timeout event callback type
 * @eloop_ctx: Registered callback context data (eloop_data)
 * @user_ctx: Registered callback context data (user_data)
 */
typedef void (*eloop_timeout_handler)(void *eloop_data, void *user_ctx);

/**
 * eloop_signal_handler - eloop signal event callback type
 * @sig: Signal number
 * @signal_ctx: Registered callback context data (user_data from
 * eloop_register_signal(), eloop_register_signal_terminate(), or
 * eloop_register_signal_reconfig() call)
 */
typedef void (*eloop_signal_handler)(int sig, void *signal_ctx);

int ra6wx_eloop_init(void);

#define ELOOP_ALL_CTX (void *) -1

int eloop_terminated(void);

int eloop_register_timeout(unsigned int secs,
					unsigned int usecs,
			   		eloop_timeout_handler handler,
			   		void *eloop_data,
					void *user_data);

int eloop_cancel_timeout(eloop_timeout_handler handler,
			 		void *ra6wx_eloop_data,
					void *user_data);

int eloop_cancel_timeout_one(eloop_timeout_handler handler,
			     void *eloop_data, void *user_data,
			     struct os_reltime *remaining);

int eloop_is_timeout_registered(eloop_timeout_handler handler,
					void *ra6wx_eloop_data,
					void *user_data);

int eloop_deplete_timeout(unsigned int req_secs,
					unsigned int req_usecs,
			  		eloop_timeout_handler handler,
					void *ra6wx_eloop_data,
			  		void *user_data);

struct wpa_global;
struct wpa_supplicant;

void ra6wx_eloop_run(struct wpa_global *global, struct wpa_supplicant *wpa_s);

int eloop_sock_requeue(void);

int eloop_register_read_sock(int sock, eloop_sock_handler handler,
			     void *eloop_data, void *user_data);

void eloop_unregister_read_sock(int sock);

int eloop_replenish_timeout(unsigned int req_secs, unsigned int req_usecs,
			    eloop_timeout_handler handler, void *eloop_data,
			    void *user_data);

void eloop_terminate(void);

void eloop_destroy(void);

void eloop_wait_for_read_sock(int sock);

void *init_supp_mempool(void *pointer);

int request_stop_supplicant(void);

int request_start_supplicant(void);
#endif /* SUPP_ELOOP_H */
