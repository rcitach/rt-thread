/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef RM_WIFI_DPM_INTERNAL_H
#define RM_WIFI_DPM_INTERNAL_H

#include "bsp_api.h"

#if CFG_PMGR
/**********************************************************************************************************************
 * Includes
 *********************************************************************************************************************/
#include "sleep_mgmt_regs.h"
#include "romac4rtos.h"

#include "FreeRTOS.h"
#include "custom_config_sdk.h"

#include <stdarg.h>

#include "defs.h"
#include "lwip/err.h"

#include "rm_pmgr_w_instance.h"

#include "ra6w1_dpm_system.h"
#include "common_def.h"
#include "rm_vee_flash_w_rrq_nvram.h"

/**********************************************************************************************************************
 * Macro definitions
 *********************************************************************************************************************/

#define DPM_SOCK_MAX_UDP (8)

/***********************************************************************************************************************
 * Typedef definitions
 *********************************************************************************************************************/

void RM_WIFI_dpm_arp_filter_set(unsigned long accept_addr, unsigned long subnet_addr);

void RM_WIFI_dpm_ptim_event_set(int event);

unsigned int RM_WIFI_dpm_ptim_event_get(void);

uint32_t RM_WIFI_dpm_ptim_event_get_pre(void);

unsigned int RM_WIFI_dpm_get_cur_bcn_count();

int	RM_WIFI_dpm_supp_state_get(void);

int	RM_WIFI_dpm_supp_is_connected(void);

void RM_WIFI_dpm_supp_conn_state_set(int state);

void RM_WIFI_dpm_conn_info_clear(void);

void RM_WIFI_dpm_ptim_auto_arp_enable(int mode);
#else

/* Dummy functions for rwnx_drv / macsw */
unsigned int RM_WIFI_dpm_ptim_event_get(void);

#endif /* CFG_PMGR */

#endif/*RM_WIFI_DPM_INTERNAL_H*/