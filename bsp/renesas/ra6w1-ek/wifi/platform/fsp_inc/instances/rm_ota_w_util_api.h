/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef RM_OTA_W_UTIL_API_H
#define RM_OTA_W_UTIL_API_H

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "bsp_api.h"

/* Common macro for FSP header files. There is also a corresponding FSP_FOOTER macro at the end of this file. */
FSP_HEADER

void rm_ota_w_util_api_sflash_open(void);
void rm_ota_w_util_api_sflash_close(void);
bool rm_ota_w_util_api_sflash_read(uint32_t sflash_addr, void *rd_buf, uint32_t len);
bool rm_ota_w_util_api_sflash_write(uint32_t sflash_addr, char *wr_buf, uint32_t len);
bool rm_ota_w_util_api_sflash_erase(uint32_t sflash_addr, uint32_t len);
bool rm_ota_w_util_api_sflash_copy(uint32_t dest_addr, uint32_t src_addr, uint32_t len);

/** Common macro for FSP header files. There is also a corresponding FSP_HEADER macro at the top of this file. */
FSP_FOOTER

#endif // RM_OTA_W_UTIL_API_H
