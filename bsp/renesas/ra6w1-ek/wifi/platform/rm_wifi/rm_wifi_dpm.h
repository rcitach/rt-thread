/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/*******************************************************************************************************************//**
 * @addtogroup WIFI
 * @{
 **********************************************************************************************************************/

#ifndef RM_WIFI_DPM_H
#define RM_WIFI_DPM_H

/**********************************************************************************************************************
 * Includes
 *********************************************************************************************************************/
#include "rm_wifi_dpm_internal.h"


/* Common macro for FSP header files. There is also a corresponding FSP_FOOTER macro at the end of this file. */
FSP_HEADER

#if CFG_PMGR
/**********************************************************************************************************************
 * Macro definitions
 *********************************************************************************************************************/


/***********************************************************************************************************************
 * Typedef definitions
 *********************************************************************************************************************/

/**
 * Set UDP port number to allow to receive UDP packet in DPM sleep.
 * The maximum number of UDP port filter is DPM_MAX_UDP_FILTER (6).
 * @param[in]		d_port			UDP port number.
 */
void RM_WIFI_dpm_udp_port_filter_set(unsigned short d_port);

/**
 * Delete UDP port number. It's set by RM_WIFI_dpm_udp_port_filter_set() function.
 * 
 * @param[in]		d_port			Port number of UDP.
 */
void RM_WIFI_dpm_udp_port_filter_delete(unsigned short d_port);

/**
 * Set UDP hole punching functionality.
 * 
 * @param[in]		period			Period of UDP hole punching functionality.
 * @param[in]		dst_ip			Destination IPv4 address.
 * @param[in]		dst_ip6		Destination IPv6 address.
 * @param[in]		src_port		Source port number.
 * @param[in]		dest_port		Destination port number.

 * @retval     true                 Set UDP hole punching functionality successful
 * @retval     false                Set UDP hole punching functionality not successful
 */
int RM_WIFI_dpm_udp_port_hole_punch_set(int period /* keep period times */,
                                        uint32_t dst_ip,
                                        uint32_t *dst_ip6,
                                        unsigned short src_port,
                                        unsigned short dest_port);

/**
 * Set TCP port number to allow to receive TCP packet in DPM sleep.
 * The maximum number of TCP port filter is DPM_MAX_UDP_FILTER (6).
 * 
 * @param[in]		d_port			TCP port number
 */
void RM_WIFI_dpm_tcp_port_filter_set(unsigned short d_port);

/**
 * Delete TCP port number. It's set by RM_WIFI_dpm_tcp_port_filter_set() function.
 * 
 * @param[in]		d_port			TCP port number.
 */
void RM_WIFI_dpm_tcp_port_delete(unsigned short d_port);

/**
 * Set TIM Wakeup count. Saving TIM Wakeup count to NVRAM is required,
 * TIM Wakeup count will be applied after system reboot.
 * 
 * @param[in]		dtim_period		DTIM period in 100 msec. Default is 10 DTIM (about 1 sec)
 *            						The range is from 1 to 65535.
 * @param[in]		saveflag		Flag to save period of DTIM to NVRAM.
 */
void RM_WIFI_dpm_ptim_wakeup_count_set(int dtim_period , int saveflag);

void RM_WIFI_dpm_ptim_wakeup_count_set_from_nvram(void);

/**
 * Set IP muticast address to allow receving packet in DPM sleep.
 * 
 * @param[in]		mc_addr			IPv4 multicast address.
 */
void RM_WIFI_dpm_ptim_mc_filter_set(ULONG mc_addr);

/**
 * Set IP address condition to enter DPM sleep.
 *
 * DPM system will check IP assignment condition (IPv4 only, IPv6 only, or both IPv4 and IPv6)
 * before DPM_sleep
 *
 * The IP address condition will be applied after system reboot.
 *
 * @param[in]		ip_condition			1 (Check only IPv4 condition)
 *                                          2 (Check only IPv6 condition)
 *                                          3 (Check both IPv4 and IPv6 condition) 
 */
fsp_err_t RM_WIFI_dpm_ip_condition_set(unsigned char ip_condition);

/**
 * Get IP address condition to enter DPM sleep.
 *
 * @param[out]		p_ip_condition			Saved IP address condition.
 */
void RM_WIFI_dpm_ip_condition_get(unsigned char * p_ip_condition);

/**
 * Set IP muticast address to allow receving packet in DPM sleep.
 *
 * @param[in]		mcipv6			IPv6 multicast address.
 */
void RM_WIFI_dpm_ptim_mcv6_filter_set(uint16_t *mcipv6);

#else
/* Dummy functions for rwnx_drv / macsw */
void RM_WIFI_dpm_ptim_wakeup_count_set(int dtim_period, int saveflag);
void RM_WIFI_dpm_ptim_wakeup_count_set_from_nvram(void);
#endif /* CFG_PMGR */

/* Common macro for FSP header files. There is also a corresponding FSP_HEADER macro at the top of this file. */
FSP_FOOTER

#endif /*RM_WIFI_DPM_H*/

/*******************************************************************************************************************//**
 * @} (end addtogroup WIFI)
 **********************************************************************************************************************/
