/**
 *****************************************************************************************
 * @file	wpa_ctrl.h
 * @brief	wpa_supplicant/hostapd control interface library from wpa_supplicant-2.4
 *****************************************************************************************
 */

/*
 * wpa_supplicant/hostapd control interface library
 * Copyright (c) 2004-2007, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 *
 * Copyright (C) 2023-2024 Modified by Renesas Electronics
 */

#include "includes.h"
#include "wpa_ctrl.h"
#include "supp_eloop.h"
#include "osal.h"

/* For Event Group */
extern EventGroupHandle_t ra6w1_sp_event_group;

/* For Message queue */
extern QueueHandle_t TO_SUPP_QUEUE;
extern QueueHandle_t ra6wx_cli_msg_tx_q;
extern QueueHandle_t ra6wx_cli_msg_rx_q;

#ifdef CONFIG_SCAN_REPLY_OPTIMIZE
extern UINT ra6w1_cli_eloop_run(char **reply);
#else
extern UINT ra6w1_cli_eloop_run(char *reply);
#endif /* CONFIG_SCAN_REPLY_OPTIMIZE */


#ifdef CONFIG_SCAN_REPLY_OPTIMIZE
int wpa_ctrl_request(const char *cmd, size_t cmd_len,
		     char  **reply, size_t *reply_len,
		     void (*msg_cb)(char *msg))
#else
int wpa_ctrl_request(const char *cmd, size_t cmd_len,
		     char *reply, size_t *reply_len,
		     void (*msg_cb)(char *msg))
#endif /* CONFIG_SCAN_REPLY_OPTIMIZE */
{
    (void) msg_cb;
	BaseType_t	status;
	UINT	rsp_status;
	ULONG	ra6w1_events = 0;
#ifdef CONFIG_SUPP_EVENT_DEPRECATED
	ULONG	temp_pack[2];
#endif
	ra6wx_cli_tx_msg_t	cli_cmd_tx_buf;	/* CLI -> Supplicant */

	ra6wx_cli_tx_msg_t *cli_cmd_tx_msg_p = (ra6wx_cli_tx_msg_t *)&cli_cmd_tx_buf;
	void *cli_tx_cmd_buf;

	if (cmd_len > RA6WX_QUEUE_BUF_SIZE) {
		ra6w1_cli_prt("[%s] cmd_len=%d\n", __func__, cmd_len);
		return -1;
	}

	ra6w1_cli_prt("<%s> START : cmd=[%s]\n", __func__, cmd);

	cli_tx_cmd_buf = pvPortMalloc(sizeof(ra6wx_cli_cmd_buf_t));

	if (cli_tx_cmd_buf == NULL) {
		return -1;
	}

	memset((void *)cli_tx_cmd_buf, 0, sizeof(ra6wx_cli_cmd_buf_t));

	cli_cmd_tx_msg_p->cli_tx_buf = (ra6wx_cli_cmd_buf_t *)cli_tx_cmd_buf;
	strcpy(cli_cmd_tx_msg_p->cli_tx_buf->cmd_str, cmd);

	if (strncmp(cmd, "SCAN_RESULTS", 12) == 0) {
		ra6w1_events = xEventGroupWaitBits(ra6w1_sp_event_group,
										RA6W_SCAN_RESULTS_RX_EV | RA6WX_SCAN_RESULTS_FAIL_EV,
										pdFALSE,
										pdFALSE,
										portCONVERT_MS_2_TICKS(5000)); // 5 Sec..

		if (ra6w1_events & RA6W_SCAN_RESULTS_RX_EV) {
			ra6w1_cli_prt("Scan succeeded. \n");
		} else if (ra6w1_events & RA6WX_SCAN_RESULTS_FAIL_EV) {
			ra6w1_cli_prt("Scan Failed. \n");

			ra6w1_events = xEventGroupWaitBits(ra6w1_sp_event_group,
											RA6WX_SCAN_RESULTS_FAIL_EV,
											pdTRUE,
											pdFALSE,
											OS_EVENT_NO_WAIT);

			ra6w1_events = xEventGroupWaitBits(ra6w1_sp_event_group,
											RA6W_SCAN_RESULTS_TX_EV,
											pdTRUE,
											pdFALSE,
											OS_EVENT_NO_WAIT);

			vPortFree(cli_tx_cmd_buf);

			return -1;
		} else {
			ra6w1_cli_prt("<%s> No scan result received (ra6w1_events=%lu)\r\n", __func__, ra6w1_events);

			ra6w1_events = xEventGroupWaitBits(ra6w1_sp_event_group,
											RA6W_SCAN_RESULTS_TX_EV,
											pdTRUE,
											pdFALSE,
											OS_EVENT_NO_WAIT);

			vPortFree(cli_tx_cmd_buf);

			return -1;
		}
	}

	ra6w1_events = xEventGroupWaitBits(ra6w1_sp_event_group,
									RA6W_SCAN_RESULTS_RX_EV,
									pdTRUE,
									pdFALSE,
									OS_EVENT_NO_WAIT);

#ifdef CONFIG_SUPP_EVENT_DEPRECATED
	temp_pack[0] = RA6WX_SP_CLI_TX_FLAG;
	temp_pack[1] = (ULONG)cli_cmd_tx_msg_p;

	status = xQueueSend(TO_SUPP_QUEUE, &temp_pack, portMAX_DELAY/*OS_QUEUE_NO_WAIT*/);
#else
	ULONG *temp_pack;

	temp_pack = (void *) pvPortMalloc(sizeof(ULONG)*2);

	if (temp_pack == NULL) {
			return -1;
	}

	temp_pack[0] = RA6WX_SP_CLI_TX_FLAG;
	temp_pack[1] = (ULONG)cli_cmd_tx_msg_p;

	status = xQueueSend(TO_SUPP_QUEUE, &temp_pack, portMAX_DELAY/*OS_QUEUE_NO_WAIT*/);
#endif // CONFIG_SUPP_EVENT_DEPRECATED

	if (status != pdTRUE) {
		ra6wx_err_prt("[%s] msg send error !!! (%d)\r\n", __func__, status);

#ifndef CONFIG_SUPP_EVENT_DEPRECATED
		if (temp_pack) {
			vPortFree(temp_pack);
		}
#endif // CONFIG_SUPP_EVENT_DEPRECATED
		if (cli_tx_cmd_buf) {
			vPortFree(cli_tx_cmd_buf);
		}
		return -1;
	}

	/* Receive Message */
	rsp_status = ra6w1_cli_eloop_run(reply);

	if (*reply) {
#ifdef CONFIG_SCAN_REPLY_OPTIMIZE
	*reply_len = strlen(*reply); 
#else
	*reply_len = strlen(reply); 
#endif /* CONFIG_SCAN_REPLY_OPTIMIZE */
	} else {
		*reply_len = 0;
	}
 
	ra6w1_cli_prt("<%s> FINISH\n", __func__);

	if (rsp_status == pdTRUE) {
		rsp_status = 0; // Success
	} else {
		rsp_status = 1; // Fail
	}
	return (int) rsp_status;
}

/* EOF */
