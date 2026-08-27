/**
 ****************************************************************************************
 *
 * @file net_sntp_client.h
 *
 * @brief Define for Simple Time Protocol Client
 *
 * Copyright (c) 2023-2024 Renesas Electronics. All rights reserved.
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


#ifndef	__NET_SNTP_CLIENT_H__
#define	__NET_SNTP_CLIENT_H__

#include "FreeRTOS.h"
#include "custom_config_sdk.h"

#include "sys_feature.h"
#include "iface_defs.h"
#include "common_def.h"
#include "net_common.h"

#define	SNTP_LOOP_MAX			6
#define JAN_1970				0x83aa7e80		/* 2208988800 1970 - 1900 in seconds */
#define SNTP_CLIENT_STACK_SIZE	1024

/**
 ****************************************************************************************
 * @brief	Get current SNTP running mode
 * @return	1 if sntp client is in "running" state / 0 if sntp client is in "stopped" state
 ****************************************************************************************
 */
unsigned int  get_sntp_use(void);

/**
 ****************************************************************************************
 * @brief		Start / stop sntp client 
 * @param[in]	use start / stop
 * @return		0 on success / 1 on fail
 ****************************************************************************************
 */
unsigned int  set_sntp_use(int use);

/**
 ****************************************************************************************
 * @brief		Get the SNTP server URI (Domain or IP address)
 * @param[out]	svraddr	Buffer pointer to read current SNTP server URI
 * @param[in]	index	Index (0 based) of SNTP server list.
 * @return		None
 ****************************************************************************************
 */
void 		  get_sntp_server(char *svraddr, unsigned int index);

/**
 ****************************************************************************************
 * @brief		Set new SNTP server URI (Domain or IP address)
 * @param[in]	svraddr	Buffer pointer to save new SNTP server URI
 * @param[in]	index	Index (0 based) of SNTP server list to which the server URI is copied
 * @return		0 on success / 1 on fail
 ****************************************************************************************
 */
unsigned char set_sntp_server(unsigned char *svraddr, unsigned int index);

/**
 ****************************************************************************************
 * @brief Get the current SNTP time sync period value
 * @return period value (in sec) on success / Default value on fail
 ****************************************************************************************
 */
int get_sntp_period(void);

/**
 ****************************************************************************************
 * @brief		Set new SNTP time sync period value
 * @param[in]	period	new SNTP time sync value (in sec)
 * @return		0 on success / 1 on fail
 ****************************************************************************************
 */
unsigned char set_sntp_period(int period);

/**
 ****************************************************************************************
 * @brief		query if sntp sync is done
 * @return		1 on sync / 0 on not-synced
 ****************************************************************************************
 */
unsigned int is_sntp_sync(void);

/**
 ****************************************************************************************
 * @brief	Do SNTP Sync now.
 ****************************************************************************************
 */
void sntp_sync_now(void);
unsigned int stop_sntp(void);
unsigned int start_sntp(void);

#endif	/* __NET_SNTP_CLIENT_H__ */

/* EOF */
