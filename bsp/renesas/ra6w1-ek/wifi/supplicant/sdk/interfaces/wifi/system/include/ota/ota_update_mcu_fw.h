/**
 ****************************************************************************************
 *
 * @file ota_update_mcu_fw.h
 *
 * @brief Over the air firmware update module
 *
 * Copyright (c) 2016-2022 Renesas Electronics. All rights reserved.
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


#if !defined (__OTA_UPDATE_MCU_FW_H__)
#define	__OTA_UPDATE_MCU_FW_H__

#include "FreeRTOS.h"
#include "custom_config_sdk.h"
#include <stdio.h>
#include "sys_app_defs.h"

#if defined (__OTA_UPDATE_MCU_FW__)
#define OTA_BY_MCU			        "tx_size="

/// MCU Firmware version structure.
#define OTA_MCU_FW_NAME_LEN 		(8)
typedef struct {
    char 	name[OTA_MCU_FW_NAME_LEN];
    UINT 	size;
    UINT 	crc;
}	ota_mcu_fw_info_t;
#define OTA_MCU_FW_HEADER_SIZE 		sizeof(ota_mcu_fw_info_t)

UINT ota_update_atcmd_parser(char *in_buf);
UINT ota_update_by_mcu_init(UINT fw_type, UINT len);
UINT ota_update_by_mcu_get_total_len(void);
UINT ota_update_by_mcu_download(UCHAR *rev_data, UINT rev_data_len);
UINT ota_update_gen_mcu_header(UINT size);
void ota_update_atcmd_set_tx_size(UINT tx_size);
void ota_update_iface_printf(const char *fmt, ...);
void ota_update_iface_puts(char *data, int data_len);

#endif	/* (__OTA_UPDATE_MCU_FW__) */
#endif	/* (__OTA_UPDATE_MCU_FW_H__) */

/* EOF */
