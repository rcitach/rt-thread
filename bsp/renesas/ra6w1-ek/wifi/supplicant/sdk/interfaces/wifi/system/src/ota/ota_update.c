/**
 ****************************************************************************************
 *
 * @file ota_common.c
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
#include "FreeRTOS.h"
#include "custom_config_sdk.h"

#if defined (__SUPPORT_OTA__)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common_def.h"
#include "ota_update.h"
#include "ota_update_common.h"
#include "ota_update_http.h"
#include "util_api.h"
#include "rm_vee_flash_w_rrq_nvram.h"

#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wsign-conversion"

extern bool reset(void);

size_t ota_update_read_flash(UINT addr, VOID *buf, UINT len)
{
    util_sflash_read(addr, buf, len);
    return len;
}

size_t ota_update_erase_flash(UINT addr, UINT len)
{
    if (util_sflash_erase(addr, len) != pdTRUE) {
        /* Error */
        return 0; 
    }
    return len;
}

size_t ota_update_write_flash(UINT addr, VOID *buf, UINT len)
{
    if (util_sflash_write(addr, buf, len) != pdTRUE) {
        /* Error */
        return 0; 
    }
    return len;
}

size_t ota_update_copy_flash(UINT dest_addr, UINT src_addr, UINT len)
{
    if (util_sflash_copy(dest_addr, src_addr, len) != pdTRUE) {
        /* Error */
        return 0; 
    }
    return len;
}

UINT ota_update_start_download(OTA_UPDATE_CONFIG *ota_update_conf)
{
    UINT status = OTA_SUCCESS;

    status = ota_update_process_create(ota_update_conf);

    return status;
}

UINT ota_update_stop_download(void)
{
    UINT status = OTA_SUCCESS;

    status = ota_update_process_stop();

    return status;
}

UINT ota_update_start_renew(OTA_UPDATE_CONFIG *ota_update_conf)
{
    UINT status = OTA_SUCCESS;
    UINT reboot_wait_cnt = 50; /* 5 secs */

    status = ota_update_check_state();
    if (status == OTA_SUCCESS) {
        status = ota_update_current_fw_renew();
    }

    ota_update_status_atcmd(7, status);

    if ((ota_update_conf != NULL) && (ota_update_conf->renew_notify != NULL)) {
        ota_update_conf->renew_notify(status);
    }

    if (status == OTA_SUCCESS) {
        while (reboot_wait_cnt-- > 0) {
            vTaskDelay(portCONVERT_MS_2_TICKS(100));

            if ((reboot_wait_cnt % 10) == 0) {
                printf("\r- OTA: Reboot after %d secs ...", reboot_wait_cnt / 10);
            }
        }
        printf("\n");
		
		reset();
    }

    return status;
}

UINT ota_update_get_progress(ota_update_type update_type)
{
    return ota_update_get_download_progress(update_type);
}

UINT ota_update_set_tls_auth_mode(int tls_auth_mode)
{
    return ota_http_client_set_tls_auth_mode(tls_auth_mode);
}
UINT ota_update_set_tls_version(int tls_ver)
{
    return ota_http_client_set_tls_version(tls_ver);
}
UINT ota_update_set_sni(char *sni)
{
    return ota_http_client_set_sni(sni);
}
UINT ota_update_set_alpn(char *alpn0, char *alpn1, char *alpn2)
{
    return ota_http_client_set_alpn(alpn0, alpn1, alpn2);
}

UINT ota_update_get_tls_auth_mode(void)
{
    return ota_http_client_get_tls_auth_mode();
}
UINT ota_update_get_tls_version(void)
{
    return ota_http_client_get_tls_version();
}
size_t ota_update_get_sni(char *sni)
{
    return ota_http_client_get_sni(sni);
}
size_t ota_update_get_alpn0(char *alpn0)
{
    return ota_http_client_get_alpn0(alpn0);
}
size_t ota_update_get_alpn1(char *alpn1)
{
    return ota_http_client_get_alpn1(alpn1);
}
size_t ota_update_get_alpn2(char *alpn2)
{
    return ota_http_client_get_alpn2(alpn2);
}
void ota_update_del_all_alpn(void)
{
    return ota_http_client_del_all_alpn();
}
#endif	// ( __SUPPORT_OTA__)

/* EOF */
