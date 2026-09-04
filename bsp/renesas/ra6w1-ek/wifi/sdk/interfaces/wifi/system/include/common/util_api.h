/**
 ****************************************************************************************
 *
 * @file util_api.h
 *
 * @brief Utility APIs for user function
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

#ifndef __UTIL_API_H__
#define __UTIL_API_H__

#include "FreeRTOS.h"
#include "custom_config_sdk.h"

#define SUPPORT_WLAN1_LOCAL_MACADDRESS 0


/* External global functions */
extern int get_run_mode(void);
extern void PRINTF_ATCMD(const char *fmt,...);
extern UINT is_supplicant_done(void);

#if defined ( __RUNTIME_CALCULATION__ )
extern unsigned long long get_fci_dpm_curtime(void);
#endif    // __RUNTIME_CALCULATION__

//// For get SCAN result API ////////////////////////////////////

#define    AP_OPEN_MODE        0
#define    AP_SECURE_MODE        1

//////////////////////////////////////////////////////////////////

typedef enum {
    MAC_SPOOFING,
    NVRAM_MAC,
    OTP_MAC
} MACADDR_TYPE;

typedef enum {
    E_WRITE_OK,
    E_WRITE_ERROR,
    E_ERASE_OK,
    E_ERASE_ERROR,
    E_DIGIT_ERROR,
    E_MCAST_ERROR,
    E_LOCAL_ERROR,
    E_SAME_ERROR,
    E_CANCELED,
    E_INVALID_ERROR,
    E_UNKNOW
} ERROR_WRITE;


/* Don't need description */
int is_in_softap_acs_mode(void);


//////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
//
//// For SFLASH device APIs ///////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

bool util_sflash_read(int sflash_addr, void *rd_buf, int len);
bool util_sflash_write(int sflash_addr, char *wr_buf, int len);
bool util_sflash_erase(int sflash_addr, int len);
bool util_sflash_copy(int dest_addr, int src_addr, int len);


///////////////////////////////////////////////////////////////////////////////
//
//// For Additional global APIS ///////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

/**
 ****************************************************************************************
 * @brief      Run wpa cli command operation
 * @param[in]  cmdline   wpa-cli command string
 * @param[in]  delimit   Delimiter character for each parameter
 * @param[in]  cli_reply Reply buffer pointer
 * @return     TRUE on success, FALSE on fail
 ****************************************************************************************
 */
int ra6w1_cli_reply(char *cmdline, char *delimit, char *cli_reply);

#endif // __UTIL_API_H__

/* EOF */
