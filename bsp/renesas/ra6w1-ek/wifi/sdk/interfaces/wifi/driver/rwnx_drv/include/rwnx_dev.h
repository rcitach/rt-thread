/**********************************************************************************************************
* Copyright (c) 2020 - 2026, Renesas Electronics Corporation and/or its affiliates
*
*
* By installing, copying, downloading, accessing, or otherwise using this software
* or any part thereof and the related documentation from Renesas Electronics Corporation
* and/or its affiliates ("Renesas"), You, either individually  or on behalf of an entity
* employing or engaging You, agree to be bound by this Software License Agreement.
* If you do not agree or no longer agree, you are not permitted to use this software or
* related documentation.
*
* Redistribution and use in source and binary forms, with or without modification,
* are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice, this
*    list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form, except as embedded into a Renesas
*    integrated circuit in a product or a software update for
*    such product, must reproduce the above copyright notice, this list of
*    conditions and the following disclaimer in the documentation and/or other
*    materials provided with the distribution.
*
* 3. Neither the name of Renesas nor the names of its
*    contributors may be used to endorse or promote products derived from this
*    software without specific prior written permission.
*
* 4. This software, with or without modification, must only be used with a
*    Renesas integrated circuit, or other such integrated circuit permitted by Renesas in writing.
*
* 5. Any software provided in binary form under this license must not be reverse
*    engineered, decompiled, modified and/or disassembled.
*
* THIS SOFTWARE IS PROVIDED BY RENESAS "AS IS" AND ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL RENESAS OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
* GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
* OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**********************************************************************************************************/

#ifndef _RWNX_DEV_H
#define _RWNX_DEV_H

#include <limits.h>
#include "rwnx_le_util.h"
#include "rwnx_ether.h"

#define MAX_ADDR_LEN 32
typedef unsigned long net_stats_t;

struct rwnx_dev_iface_stats {
	net_stats_t	rx_packets;
	net_stats_t	tx_packets;
	net_stats_t	rx_bytes;
	net_stats_t	tx_bytes;
	net_stats_t	rx_errors;
	net_stats_t	tx_errors;
	net_stats_t	rx_dropped;
	net_stats_t	tx_dropped;
	net_stats_t	multicast;
	net_stats_t	collisions;
	net_stats_t	rx_length_errors;
	net_stats_t	rx_over_errors;
	net_stats_t	rx_crc_errors;
	net_stats_t	rx_frame_errors;
	net_stats_t	rx_fifo_errors;
	net_stats_t	rx_missed_errors;
	net_stats_t	tx_aborted_errors;
	net_stats_t	tx_carrier_errors;
	net_stats_t	tx_fifo_errors;
	net_stats_t	tx_heartbeat_errors;
	net_stats_t	tx_window_errors;
	net_stats_t	rx_compressed;
	net_stats_t	tx_compressed;
};

struct rwnx_dev_iface {
	char name[32];
	int  ifType;
	int  ifindex;
	unsigned char perm_addr[MAX_ADDR_LEN];

	enum {
		RTNL_LINK_INITIALIZED,
		RTNL_LINK_INITIALIZING,
	} rtnl_link_state;
};

#endif	/* _RWNX_DEV_H */
