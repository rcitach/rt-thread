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
#include <strings.h>
#include "common_def.h"
#include "net_common.h"
#include "ota_update.h"
#include "ota_update_common.h"
#include "ota_update_http.h"
#include "util_api.h"
#include "rm_vee_flash_w_rrq_nvram.h"
#if CFG_PMGR
#include "rm_pmgr_w_instance.h"
#endif /* CFG_PMGR */

#if __has_include("rm_awsiot_w_cfg.h")
#include "rm_awsiot_w_cfg.h"
#endif

#if (SUPPORT_FSP_RM_OTA_W == 1)
#include "rm_ota_w.h"
#endif

#ifdef RM_MAP_PERSISTANT_W
#include "rm_map_persistant_w.h"
#endif

#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wsign-conversion"

extern int	interface_select;

static TaskHandle_t ota_proc_xHandle = NULL;
static EventGroupHandle_t ota_event_group;

static ota_update_proc_t _ota_proc = { 0, };
static ota_update_proc_t * ota_proc = &_ota_proc;

static UINT CUSTOM_SFLASH_ADDR = OTA_STOR_USER_START;
static UINT ota_refuse_flag = 0;

#define CRC_PRELOAD             0xFFFF
#define CRC16_CCITT             0x1021

#if (SUPPORT_FSP_RM_OTA_W == 1)
extern ota_instance_t g_ota0;
#endif

const uint32_t ota_update_crc32_tab[] = {
        0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
        0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
        0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
        0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
        0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
        0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
        0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
        0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
        0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
        0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
        0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
        0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
        0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
        0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
        0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
        0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
        0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
        0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
        0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
        0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
        0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
        0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
        0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
        0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
        0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
        0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
        0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
        0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
        0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
        0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
        0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
        0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
        0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
        0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
        0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
        0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
        0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
        0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
        0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
        0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
        0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
        0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
        0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

uint32_t ota_update_crc32(const void * buf, size_t size)
{
        const uint8_t *p = buf;
        uint32_t crc;

        crc = ~0U;
        while (size--)
        {
            crc = ota_update_crc32_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
        }
        return crc ^ ~0U;
}

void ota_update_set_boot_index(UINT boot_idx)
{
#if (SUPPORT_FSP_RM_OTA_W == 1)
    g_ota0.p_api->bootIdxSet(g_ota0.p_ctrl, (uint8_t) boot_idx);
#endif
    return;
}

UINT ota_update_get_boot_index(void)
{
    uint8_t current_boot_idx = 0;

#if (SUPPORT_FSP_RM_OTA_W == 1)

    g_ota0.p_api->bootIdxGet(g_ota0.p_ctrl, &current_boot_idx);

#endif

    return (UINT) current_boot_idx;
}


UINT ota_update_toggle_boot_index(void)
{
    UINT boot_idx = 0;

    boot_idx = ota_update_get_boot_index();
    ota_update_set_boot_index(!boot_idx);
    ota_refuse_flag = 1;
    OTA_INFO(">>> %s is updated and system reboots. (New boot_idx=%d) <<<\n\n"
             , OTA_RTOS_NAME
             , ota_update_get_boot_index());

    return OTA_SUCCESS;
}


void ota_update_status_atcmd(UINT atcmd_event, UINT status)
{
    RA6W1_UNUSED_ARG(atcmd_event);
    RA6W1_UNUSED_ARG(status);
}

UINT ota_update_check_refuse_flag(void)
{
    return ota_refuse_flag;
}

void ota_update_print_fw_header(image_header_data_t * infoImage)
{
    OTA_INFO("\tVersion-------- %s\n", infoImage->name);
    OTA_INFO("\tData Size------ %ld\n", infoImage->size);
    OTA_INFO("\tIVT------------ 0x%x\n", (unsigned int)(infoImage->ivt_location));
    OTA_INFO("\tHCRC----------- 0x%x\n", (unsigned int)(infoImage->header_crc));
    OTA_INFO("\tDCRC----------- 0x%x\n", (unsigned int)(infoImage->crc));
}

UINT ota_update_get_available_size(ota_update_type update_type)
{
    if (update_type == OTA_TYPE_RTOS)
    {
        return OTA_STOR_RTOS_SIZE;

    }
    else if ((update_type == OTA_TYPE_MCU_FW)
               || (update_type == OTA_TYPE_CERT_KEY))
    {
        if (CUSTOM_SFLASH_ADDR != OTA_STOR_USER_START)
        {
            return OTA_STOR_USER_END - CUSTOM_SFLASH_ADDR;
        }
        else
        {
            return OTA_STOR_USER_SIZE;
        }
#if defined (__SUPPORT_MATTER_IOT__) && defined (__SUPPORT_ATCMD__)
    }
    else if (update_type == OTA_TYPE_MCU_FW_STREAM)
    { //APP_CORE
        return OTA_STOR_RTOS_SIZE;
#endif //(__SUPPORT_MATTER_IOT__) && (__SUPPORT_ATCMD__)
    }
    else
    {
        OTA_ERR("- OTA: Wrong FW type (%d)\n", update_type);
        return 0;
    }

}

UINT ota_update_check_available_size(ota_update_type update_type, UINT size)
{
    UINT alloc_size = 0;

    if (update_type == OTA_TYPE_INIT)
    {
        return OTA_SUCCESS;
    }

    if (update_type >= OTA_TYPE_UNKNOWN)
    {
        OTA_ERR("- OTA: Unknown FW type\n");
        return OTA_ERROR_TYPE;
    }

   alloc_size = ota_update_get_available_size(update_type);

    if (alloc_size <= size)
    {
        OTA_ERR("- OTA: <%s> FW size error. (Allowable size = %d, Receiving size = %d)\n",
                ota_update_type_to_text(update_type), alloc_size, size);
        return OTA_ERROR_SIZE;
    }

    return OTA_SUCCESS;
}

UINT ota_update_get_curr_sflash_addr(ota_update_type update_type)
{
    if (update_type == OTA_TYPE_INIT)
    {
        return OTA_SUCCESS;
    }

    if (update_type >= OTA_TYPE_UNKNOWN)
    {
        OTA_ERR("- OTA: Unknown FW type\n");
        return OTA_ERROR_TYPE;
    }

    if (update_type == OTA_TYPE_RTOS)
    {
        if (ota_update_get_boot_index() == 1)
        {
            return OTA_STOR_RTOS_1_ADDR;
        }
        else
        {
            return OTA_STOR_RTOS_0_ADDR;
        }

    }
    else if ((update_type == OTA_TYPE_MCU_FW)
               || (update_type == OTA_TYPE_CERT_KEY))
    {
        return CUSTOM_SFLASH_ADDR;
    }

    OTA_ERR("- OTA: Wrong FW type (%d)\n", update_type);
    return OTA_STOR_UNKNOWN_ADDR;
}

UINT ota_update_get_new_sflash_addr(ota_update_type update_type)
{
    if (update_type == OTA_TYPE_INIT)
    {
        return OTA_SUCCESS;
    }

    if (update_type >= OTA_TYPE_UNKNOWN)
    {
        OTA_ERR("- OTA: Unknown FW type\n");
        return OTA_ERROR_TYPE;
    }

    if (update_type == OTA_TYPE_RTOS)
    {
        if (ota_update_get_boot_index() == 1)
        {
            return OTA_STOR_RTOS_0_ADDR;
        }
        else
        {
            return OTA_STOR_RTOS_1_ADDR;
        }

#if defined (__SUPPORT_MATTER_IOT__) && defined (__SUPPORT_ATCMD__)
    }
    else if (update_type == OTA_TYPE_MCU_FW_STREAM)
    {//APP_CORE
        return OTA_SUCCESS;
#endif  //(__SUPPORT_MATTER_IOT__) && (__SUPPORT_ATCMD__)
    }
    else if ((update_type == OTA_TYPE_MCU_FW)
               || (update_type == OTA_TYPE_CERT_KEY))
    {
        return CUSTOM_SFLASH_ADDR;
    }

    OTA_ERR("- OTA: Wrong FW type (%d)\n", update_type);
    return OTA_STOR_UNKNOWN_ADDR;
}

UINT ota_update_set_user_sflash_addr(UINT sflash_addr)
{

    if (((sflash_addr >= SF_TLS_CERT_BASE_ADDR) && (sflash_addr < (SF_TLS_CERT_BASE_ADDR + SF_TLS_AREA_SIZE)))
            || ((sflash_addr >= SF_USER_AREA) && (sflash_addr < (SF_USER_AREA + SF_USER_AREA_SIZE))))
    { 
        OTA_INFO("- OTA : download_sflash_addr = 0x%x \n", sflash_addr);
        CUSTOM_SFLASH_ADDR = sflash_addr;

    }
    else
    {
        OTA_ERR("- OTA : sflash address(0x%x) is incorrect \n", sflash_addr);
        return OTA_ERROR_SFLASH_ADDR;
    }

    return OTA_SUCCESS;
}
const char * ota_update_type_to_text(ota_update_type update_type)
{
    if (update_type == OTA_TYPE_RTOS)
    {
        return OTA_RTOS_NAME;
    }
    else if (update_type == OTA_TYPE_MCU_FW)
    {
        return OTA_MCU_FW_NAME;
#if defined (__SUPPORT_MATTER_IOT__) && defined (__SUPPORT_ATCMD__)
    }
    else if (update_type == OTA_TYPE_MCU_FW_STREAM)
    {
        return OTA_MCU_FW_NAME;
#endif //(__SUPPORT_MATTER_IOT__) && (__SUPPROT_ATCMD__)
    }
    else if (update_type == OTA_TYPE_CERT_KEY)
    {
        return OTA_CERT_KEY_NAME;
    }
    return "UNKNOWN";
}

UINT ota_update_parse_version_string(UCHAR * version,
        FW_versionInfo_t * fw_ver)
{
    CHAR * pRev_a = NULL;
    CHAR * pRev_b = NULL;
    UINT str_len = 0;
    UINT total_str_len = 0;
    UINT sum_str_len = 0;
    UINT status = OTA_SUCCESS;

    if (version == NULL || fw_ver == NULL)
    {
        return OTA_FAILED;
    }

    memset(fw_ver, 0x00, sizeof(FW_versionInfo_t));

    total_str_len = strlen((char *) version);

    /* Extract FW_Type */
    pRev_a = (CHAR *) version;
    pRev_b = (CHAR *) strstr((char *) pRev_a, OTA_VER_DELIMITER);
    if (pRev_b == NULL)
    {
        OTA_DBG("  > Delimiter (%s) not found\n", OTA_VER_DELIMITER);
        return OTA_FAILED;
    }

    str_len = pRev_b - pRev_a;
    if (str_len > UPDATE_TYPE_MAX)
    {
        OTA_DBG("  > FW_TYPE is too long (max = %d)\n", UPDATE_TYPE_MAX);
        status = OTA_FAILED;
    }
    memcpy(fw_ver->update_type, pRev_a, str_len > UPDATE_TYPE_MAX ? UPDATE_TYPE_MAX : str_len);
    sum_str_len += str_len;

    /* Extract Module name */
    pRev_b ++;
    sum_str_len ++;
    pRev_a = (CHAR *) strstr((char *) pRev_b, OTA_VER_DELIMITER);
    if (pRev_a == NULL)
    {
        OTA_DBG("  > Delimiter (%s) not found\n", OTA_VER_DELIMITER);
        return OTA_FAILED;
    }

    str_len = pRev_a - pRev_b;
    sum_str_len += str_len;
    if (str_len > MODULE_MAX)
    {
        OTA_DBG("  > Module name is too long (max = %d)\n", MODULE_MAX);
        status = OTA_FAILED;
    }
    memcpy(fw_ver->module, pRev_b, str_len > MODULE_MAX ? MODULE_MAX : str_len);

    /* Extract SDK version */
    pRev_a ++;
    sum_str_len ++;
    pRev_b = (CHAR *) strstr((char *) pRev_a, OTA_VER_DELIMITER);
    if (pRev_b == NULL)
    {
        OTA_DBG("  > Delimiter (%s) not found\n", OTA_VER_DELIMITER);
        return OTA_FAILED;
    }

    str_len = pRev_b - pRev_a;
    sum_str_len += str_len;
    if (str_len > SDK_MAX)
    {
        OTA_DBG("  > SDK version is too long (max = %d)\n", SDK_MAX);
        status = OTA_FAILED;
    }
    memcpy(fw_ver->sdk, pRev_a, str_len > SDK_MAX ? SDK_MAX : str_len);

    /* Extract Customer Version */
    pRev_b ++;
    sum_str_len ++;
    str_len = total_str_len - sum_str_len;
    if (str_len > CUSTOMER_MAX)
    {
        OTA_DBG("  > CUSTOMER is too long (max = %d)\n", CUSTOMER_MAX);
        status = OTA_FAILED;
    }
    memcpy(fw_ver->customer, pRev_b, str_len > CUSTOMER_MAX ? CUSTOMER_MAX : str_len);

    if (!strlen((char *) fw_ver->update_type)
     || !strlen((char *) fw_ver->module)
     || !strlen((char *) fw_ver->sdk)
     || !strlen((char *) fw_ver->customer))
    {
        memset(fw_ver, 0x00, sizeof(FW_versionInfo_t));
        return OTA_FAILED;
    }

    return status;
}

static UINT ota_update_read_new_fw_version(UCHAR * data,
        FW_versionInfo_t * fw_ver)
{
    UCHAR new_ver[IMAGE_HEADER_NAME_LEN] = {0x00, };

    if (data == NULL)
    {
        return OTA_FAILED;
    }

    memset(new_ver, 0x00, IMAGE_HEADER_NAME_LEN);
    memcpy(new_ver, &data[OTA_VER_START_OFFSET], IMAGE_HEADER_NAME_LEN);

    /* parse received version */
    if (ota_update_parse_version_string(new_ver, fw_ver))
    {
        OTA_ERR("   > Failed to parse Server FW version : %s \n", new_ver);
        return OTA_VERSION_UNKNOWN;
    }
    else
    {
        OTA_INFO("   > Server FW version : %s-%s-%s-%s \n",
                 fw_ver->update_type,
                 fw_ver->module,
                 fw_ver->sdk,
                 fw_ver->customer);
    }

    return OTA_SUCCESS;
}

static UINT ota_update_read_current_fw_version(UINT fw_addr, FW_versionInfo_t * fw_ver)
{
    image_header_data_t infoImage	= { 0, };

    if (fw_ver == NULL)
    {
        return OTA_FAILED;
    }

    /* READ SFLASH */
    if (ota_update_get_image_info(fw_addr, &infoImage) == 0)
    {
        return OTA_FAILED;
    }

    /* PARSE VERION */
    if (ota_update_parse_version_string(infoImage.name, fw_ver))
    {
        OTA_ERR("   > Failed to parse Current FW version : %s \n", infoImage.name);
    }

    return OTA_SUCCESS;
}

static UINT ota_update_compare_fw_version(ota_update_type update_type,
        FW_versionInfo_t cur_ver, FW_versionInfo_t new_ver)
{
    UINT ver_check_bit = 0x00;

    OTA_INFO("- OTA Update : <%s> Compare Versions\n", ota_update_type_to_text(update_type));

    /* update_type */
    if (memcmp(new_ver.update_type, cur_ver.update_type, strlen((char *) new_ver.update_type)))
    {
        ver_check_bit = BIT(OTA_HEADER_DIFF_FW_TYPE);
        OTA_INFO("   > Incompatible Image type : %s\n", new_ver.update_type);
        goto chk_finish;
    }
    else
    {
        ver_check_bit |= BIT(OTA_HEADER_SAME_FW_TYPE);
    }

    /* module name */
    if (memcmp(new_ver.module, cur_ver.module, strlen((char *) new_ver.module)))
    {
        ver_check_bit = BIT(OTA_HEADER_DIFF_MODULE);
        OTA_INFO("   > Incompatible Image module : %s\n", new_ver.module);
        goto chk_finish;
    }
    else
    {
        ver_check_bit |= BIT(OTA_HEADER_SAME_MODULE);
    }

    /* SDK version */
    if (memcmp(new_ver.sdk, cur_ver.sdk, strlen((char *) new_ver.sdk)))
    {
        OTA_INFO("   > Different SDK ver : Cur-%s, New-%s \n", cur_ver.sdk, new_ver.sdk);
        ver_check_bit |= BIT(OTA_HEADER_DIFF_SDK);
    }

    /* customer version */
    if (memcmp(new_ver.customer, cur_ver.customer, strlen((char *) new_ver.customer)))
    {
        OTA_INFO("   > Different Customer : Cur-%s, New-%s\n", cur_ver.customer,
                 new_ver.customer);
        ver_check_bit |= BIT(OTA_HEADER_DIFF_CUST);
    }

    if (ver_check_bit & BIT(OTA_HEADER_ERROR))
    {
        OTA_INFO("   > Version comparison failed\n");
        goto chk_finish;
    }

    if (((ver_check_bit & BIT(OTA_HEADER_DIFF_SDK)) == 0)
            && ((ver_check_bit & BIT(OTA_HEADER_DIFF_CUST)) == 0))
    {
        OTA_INFO("   > Same Version : %s-%s-%s-%s \n",
                 new_ver.update_type,
                 new_ver.module,
                 new_ver.sdk,
                 new_ver.customer);
    }

chk_finish:
    return ver_check_bit;
}

UINT ota_update_check_version(ota_update_type update_type,
                              UCHAR * data, UINT data_len)
{
    FW_versionInfo_t curr_ver;
    FW_versionInfo_t rev_ver;
    UINT ret_val = 0;
    CHAR magic[4] = OTA_FW_MAGIC_NUM;
    uint32_t addr = 0x00;

#if defined (DISABLE_OTA_VER_CHK)
    OTA_ERR("- OTA: NO Version check!!\n");
    return OTA_SUCCESS;
#endif // (DISABLE_OTA_VER_CHK)

    if ((data == NULL) || data_len == 0)
    {
        OTA_ERR("- OTA: Unknown version\n");
        return OTA_VERSION_UNKNOWN;
    }

    OTA_DBG("[%s]update_type = %d, data_len = %d, data = %s\n", __func__, update_type, data_len, data);
    if (update_type == OTA_TYPE_INIT)
    {
        return OTA_SUCCESS;

    }
    else if (update_type >= OTA_TYPE_UNKNOWN)
    {
        OTA_ERR("- OTA: Unknown FW type\n");
        return OTA_ERROR_TYPE;

    }
    else if (update_type == OTA_TYPE_RTOS)
    {
        if (data_len < sizeof(FW_versionInfo_t))
        {
            OTA_ERR("- OTA: Data size is too small to check version \n");
            return OTA_VERSION_UNKNOWN;
        }

        /* GET CURRENT VERSION */
#if (SUPPORT_FSP_RM_OTA_W == 1)
        g_ota0.p_api->getAddr(g_ota0.p_ctrl, RM_OTA_W_CURRENT_ADDR, (rm_ota_w_update_type_t) RM_OTA_W_TYPE_RTOS, &addr);
#endif

        if (ota_update_read_current_fw_version(addr, &curr_ver) != OTA_SUCCESS)
        {
            OTA_ERR("- OTA: Failed to read current version \n");
        }

        /* GET RECEIVED VERSION */
        if (ota_update_read_new_fw_version(data, &rev_ver) != OTA_SUCCESS)
        {
            return OTA_VERSION_UNKNOWN;
        }

        /* CHECK MAGIC NUMBER */
        if (memcmp(&data[0], &magic[0], 4) != 0)
        {
            OTA_ERR("- OTA: Wrong magic number (0x%02x 0x%02x 0x%02x 0x%02x)\n",
                    data[0], data[1], data[2], data[3]);

            return OTA_VERSION_UNKNOWN;
        }

        /* COMPARE CURRENT AND RECEIVED */
        ret_val = ota_update_compare_fw_version(update_type, curr_ver, rev_ver);
        if (   (ret_val & BIT(OTA_HEADER_DIFF_FW_TYPE))
                || (ret_val & BIT(OTA_HEADER_DIFF_MODULE))
                || (ret_val & BIT(OTA_HEADER_INCOMPATI_MAGIC))
                || (ret_val & BIT(OTA_HEADER_INIT))
                || (ret_val & BIT(OTA_HEADER_ERROR))) 
        {
            return OTA_VERSION_UNKNOWN;
        }
    }
    return OTA_SUCCESS;
}

UINT ota_update_current_fw_renew(void)
{
    UCHAR *rd_buf = NULL;
    UINT len = 0;
    UINT status = OTA_FAILED;

    OTA_INFO("\n- OTA Update : Renew - Start\n");
    if (ota_update_check_refuse_flag())
    {
        OTA_ERR("- OTA: Try again after reboot\n\n");
        status = OTA_FAILED;
        goto _renew_fail;
    }

    if (ota_update_read_nvram_download_progress(OTA_TYPE_RTOS) == 100)
    {
        /* CRC - RTOS */
        if (ota_update_sflash_rtos_crc(ota_update_get_new_sflash_addr(OTA_TYPE_RTOS)) != OTA_SUCCESS)
        {
            OTA_ERR("- OTA: <%s> CRC Error\n", OTA_RTOS_NAME);
            status = OTA_ERROR_CRC;
            goto _renew_fail;
        }

        len = 80; /* byte */
        rd_buf = OTA_MALLOC(len);
        if (rd_buf != NULL)
        {
            if (ota_update_read_flash(ota_update_get_new_sflash_addr(OTA_TYPE_RTOS), rd_buf, len) != 0)
            {
                if (ota_update_check_version(OTA_TYPE_RTOS,	rd_buf, len) != OTA_SUCCESS)
                {
                    OTA_ERR("- OTA: <%s> Incompatible new version\n", OTA_RTOS_NAME);
                    status = OTA_VERSION_INCOMPATI;
                    OTA_FREE(rd_buf);
                    goto _renew_fail;
                }
            }
            else
            {
                OTA_ERR("- OTA: <%s> Failed to read new version\n", OTA_RTOS_NAME);
                status = OTA_FAILED;
                OTA_FREE(rd_buf);
                goto _renew_fail;
            }
            OTA_FREE(rd_buf);
        }
        else
        {
            OTA_ERR("[%s:%d] Failed to allocate read buffer(%d bytes)\n", __func__, __LINE__, len);
            status = OTA_MEM_ALLOC_FAILED;
            goto _renew_fail;
        }

        /* RENEW - Toggle the boot index */
        if (ota_update_toggle_boot_index() == OTA_SUCCESS)
        {
            return OTA_SUCCESS;
        }

    }
    else
    {
        OTA_INFO("- OTA: Try to download F/W images\n");
        status = OTA_NOT_ALL_DOWNLOAD;
        goto _renew_fail;
    }

_renew_fail:

    /* Store in nvram so that values ??are not lost after reboot. */
    ota_update_write_nvram_download_progress(OTA_TYPE_RTOS, 0);

    /* RENEW FAIL */
    OTA_ERR("\n>>> OTA FW update failed(0x%02x) <<<\n\n", status);

    return status;
}

UINT ota_update_get_image_info(UINT sectorAddr, image_header_data_t * infoImage)
{
    return ota_update_read_flash(sectorAddr, (void *) infoImage, sizeof(image_header_data_t));
}

UINT ota_update_sflash_rtos_crc(UINT sectorAddr)
{
    uint32_t addr_offset = 0;
    uint32_t tot_len = 0;
    uint32_t cal_len = 0;
    uint32_t cal_crc = 0;
    uint32_t crc;
    uint32_t retry_crc = 0;
    size_t size;
    UINT status = OTA_SUCCESS;
    image_header_data_t infoImage;

    unsigned char * buf = NULL;

    buf = OTA_MALLOC(OTA_SFLASH_BUF_SZ);
    if (buf == NULL)
    {
        OTA_ERR("[%s:%d] Failed to allocate buffer(%d bytes)\n", __func__, __LINE__,
                            OTA_SFLASH_BUF_SZ);
        return OTA_MEM_ALLOC_FAILED;
    }

    if (ota_update_get_image_info(sectorAddr, &infoImage))
    {
        ota_update_print_fw_header(&infoImage);
    }
    else
    {
        OTA_ERR("[%s:%d] Failed to get image info\n", __func__, __LINE__);
        status = OTA_FAILED;
        goto finish;
    }

retry:
    addr_offset = sectorAddr + infoImage.ivt_location;
    tot_len = (int) infoImage.size;

    crc = ~0U;
    while (tot_len > 0)
    {
        if (tot_len > OTA_SFLASH_BUF_SZ)
        {
            cal_len = OTA_SFLASH_BUF_SZ;
        }
        else
        {
            cal_len = tot_len;
        }

        memset(buf, 0x00, OTA_SFLASH_BUF_SZ);
        ota_update_read_flash(addr_offset, (uint8_t *) buf, cal_len);

        uint8_t * p = buf;
        size = cal_len;
        while (size--)
        {
            crc = ota_update_crc32_tab[(crc ^ * p++) & 0xFF] ^ (crc >> 8);
        }

        addr_offset += cal_len;
        tot_len -= cal_len; 
    }

    cal_crc = crc ^ ~0U;
    if (infoImage.crc != cal_crc)
    {
        if (retry_crc++ <= 2)
        {
            OTA_INFO("\tRecalculate due to CRC error(%ld)\n", retry_crc);
            goto retry;
        }
        OTA_INFO("  CRC: CRC mismatch!!(0x%lx != 0x%lx)\n", infoImage.crc, cal_crc);
        status = OTA_ERROR_CRC;
    }
    else
    {
        OTA_INFO("\tDCRC(calc)----- 0x%lx\n", cal_crc);
    }

finish:
    if (buf != NULL)
    {
        OTA_FREE(buf);
    }

    return status;
}

#if defined (__SUPPORT_MATTER_IOT__) || defined (__SUPPORT_AWS_IOT_W__) 
UINT ota_update_set_proc_state(UINT state)
#else
static UINT ota_update_set_proc_state(UINT state)
#endif
{
    OTA_DBG("[%s]state = %d\n", __func__, state);
    return ota_proc->update_state = state;
}

UINT ota_update_get_proc_state(void)
{
    return ota_proc->update_state;
}

void ota_update_print_status(ota_update_type update_type, UINT status)
{
    if (status == OTA_SUCCESS)
    {
        if (ota_update_get_proc_state() == OTA_STATE_STOP)
        {
            OTA_INFO("\n- OTA Update : <%s> Download - Stop\n\n", ota_update_type_to_text(update_type));
        }
        else
        {
            OTA_INFO("\n- OTA Update : <%s> Download - Success\n\n", ota_update_type_to_text(update_type));
        }
    }
    else
    {
        OTA_ERR("- OTA Update : <%s> Download - Failed (0x%02x)\n", ota_update_type_to_text(update_type), status);
    }
}

UINT ota_update_get_download_progress(ota_update_type update_type)
{
    vTaskDelay(portCONVERT_MS_2_TICKS(10));

    if (update_type == OTA_TYPE_RTOS)
    {
        return ota_proc->progress_rtos;
    }
    else if (update_type == OTA_TYPE_MCU_FW)
    {
        return ota_proc->progress_mcu_fw;
    }
    else if (update_type == OTA_TYPE_CERT_KEY)
    {
        return ota_proc->progress_cert_key;
    }

    return 0;
}

void ota_update_set_download_progress(ota_update_type update_type, UINT progress)
{
    if (update_type == OTA_TYPE_RTOS)
    {
        ota_proc->progress_rtos = progress;
    }
    else if (update_type == OTA_TYPE_MCU_FW)
    {
        ota_proc->progress_mcu_fw = progress;
    }
    else if (update_type == OTA_TYPE_CERT_KEY)
    {
        ota_proc->progress_cert_key = progress;
    }
}

UINT ota_update_read_nvram_download_progress(ota_update_type update_type)
{
    char nvr_name[32] = {0, };
    int progress = 0;

    sprintf(nvr_name, "%s%s", OTA_NVRAM_DW_PROGRESS, ota_update_type_to_text(update_type));
#ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, nvr_name, &progress);
#else
    read_nvram_appcfg_int(nvr_name, &progress);
#endif
    if (progress <= 0)
    {
        progress = 0;
    }

    return progress;
}

void ota_update_write_nvram_download_progress(ota_update_type update_type, UINT progress)
{
    char nvr_name[32] = {0, };

    sprintf(nvr_name, "%s%s", OTA_NVRAM_DW_PROGRESS, ota_update_type_to_text(update_type));
#ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG,
                                nvr_name, progress);
#else
    write_nvram_appcfg_int(nvr_name, progress);
#endif
}

static UINT ota_update_evt_wait(int timeout)
{
    ULONG events;
    int start_flag = 0;

    OTA_DBG("[%s]Event wait timeout = %d secs\n", __func__, timeout);
    while (1)
    {
        events = xEventGroupWaitBits(ota_event_group,
                                     OTA_EVT_RECEIVE | OTA_EVT_FINISH,
                                     pdTRUE,
                                     pdFALSE,
                                     portCONVERT_MS_2_TICKS(timeout * 1000));

        if (events == OTA_EVT_RECEIVE)
        {
            continue;
        }
        else if (events & OTA_EVT_FINISH)
        {
            break;
        }
        else if ((events == OTA_EVT_TIMEOUT) && (start_flag == 0))
        {
            start_flag = 1;
            continue;
        }
        else
        {
            OTA_DBG("[%s] Error event = 0x%lx\n", __func__, event);
            if (ota_event_group)
            {
                vEventGroupDelete(ota_event_group);
                ota_event_group = NULL;
            }
            if (events == OTA_EVT_TIMEOUT) {
                return OTA_FAILED_TIMEOUT;
            }
            return OTA_FAILED;
        }
        break;
    }

    if (ota_event_group)
    {
        vEventGroupDelete(ota_event_group);
        ota_event_group = NULL;
    }

    return ota_http_client_get_downlaod_status();
}

void ota_update_evt_send(UINT event)
{
    if (ota_event_group != NULL)
    {
        xEventGroupSetBits(ota_event_group, event);
    }
}

UINT ota_update_process_stop(void)
{
    UINT status = OTA_SUCCESS;
    UINT curr_state;
    UINT wait_cnt = 0;

    curr_state = ota_update_get_proc_state();
    if (curr_state == OTA_STATE_PROGRESS)
    {
        ota_update_evt_send(OTA_EVT_FINISH);
        ota_update_set_proc_state(OTA_STATE_STOP);
        while (ota_http_client_get_result() != HTTPC_RESULT_LOCAL_ABORT)
        {
            vTaskDelay(portCONVERT_MS_2_TICKS(100));
            wait_cnt++;

            if (wait_cnt == 100)
            {
                OTA_DBG("[%s] Wait to stop ...(%d)\n", __func__, wait_cnt / 10);
                OTA_ERR("- OTA : Failed to stop previous execution\n");
                wait_cnt = 0;
                return OTA_FAILED;
            }
            else if ((wait_cnt % 100) == 0)
            {
                OTA_DBG("[%s] Wait to stop ...(%d)\n", __func__, wait_cnt / 10);
            }
        }
        ota_http_client_set_downlaod_status(OTA_SUCCESS);
        return OTA_SUCCESS;

    }
    else if ((curr_state == OTA_STATE_FINISH) || (curr_state == OTA_STATE_STOP))
    {
        OTA_ERR("- OTA : In progressing... Please wait.\n");
        status = OTA_FAILED;
    }
    else
    {
        OTA_DBG("- OTA : No operation\n");
        status = OTA_FAILED;
    }

    ota_http_client_set_downlaod_status(status);
    ota_update_evt_send(OTA_EVT_FINISH);

    return status;
}

UINT ota_update_check_state(void)
{
    UINT curr_state;

    curr_state = ota_update_get_proc_state();
    if (curr_state == OTA_STATE_PROGRESS)
    {
        OTA_ERR("- OTA : %s is downloading... Try again after finishing.\n",
                ota_update_type_to_text(ota_proc->update_type));
        return OTA_FAILED;
    }
    else if ((curr_state == OTA_STATE_FINISH) || (curr_state == OTA_STATE_STOP))
    {
        OTA_ERR("- OTA : In progressing(%d)... Please wait.\n", curr_state);
        return OTA_FAILED;
    }

    return OTA_SUCCESS;
}

static void ota_update_process(void * arg)
{
    RA6W1_UNUSED_ARG(arg);
    const int evt_timeout = OTA_TIMEOUT;
    uint32_t progress;

#if CFG_PMGR
    /* DPM mode */
    RM_PMGR_W_dpm_job_name_set(OTA_DPM_REG_NAME, 0);
#endif /* CFG_PMGR */

    ota_event_group = xEventGroupCreate();
    if (ota_event_group == NULL)
    {
        OTA_ERR("- OTA : Failed to create event_flags\n");
        ota_proc->status = OTA_FAILED;
        goto download_finish;
    }

    OTA_DBG("[%s] update_type = %s\n", __func__, ota_update_type_to_text(ota_proc->update_type));
    ota_update_set_proc_state(OTA_STATE_PROGRESS);
    ota_update_set_download_progress(ota_proc->update_type, 0);

    /*******************************/
    /* HTTP Client REQUEST */
    /*******************************/
    ota_proc->status = ota_update_http_client_request(ota_proc);
    if (ota_proc->status != OTA_SUCCESS)
    {
        goto download_finish;
    }

    /* Wait Event */
    OTA_DBG("[%s] Wait for an event... \n", __func__);
    ota_proc->status = ota_update_evt_wait(evt_timeout);

download_finish:
    ota_update_status_atcmd(6, ota_proc->status);
    /* Print status */
    ota_update_print_status(ota_proc->update_type, ota_proc->status);
    progress = ota_update_get_download_progress(ota_proc->update_type);

    if (ota_proc->download_notify != NULL)
    {
        ota_proc->download_notify(ota_proc->update_type,
                                  ota_proc->status,
                                  progress);
    }

    ota_update_set_proc_state(OTA_STATE_READY);
    OTA_DBG("[%s] RENEW available(status = 0x%02x, auto_renew = %d, proc_state = %d)\n", __func__,
            ota_proc->status,
            ota_proc->auto_renew,
            ota_update_get_proc_state());

    if ((ota_proc->status == OTA_SUCCESS) && (ota_update_get_proc_state() != OTA_STATE_STOP))
    {
        if (ota_proc->auto_renew > 0)
        {
#if (SUPPORT_FSP_RM_OTA_W == 1)
            g_ota0.p_api->swap(g_ota0.p_ctrl);
#endif
        }
        else
        {
            if (progress == 100)
            {
                ota_update_write_nvram_download_progress(ota_proc->update_type, 
                                                         ota_update_get_download_progress(ota_proc->update_type));
            }
        }
    }

#if CFG_PMGR
    /* DPM mode */
    RM_PMGR_W_dpm_job_name_clear(OTA_DPM_REG_NAME);
#endif /* CFG_PMGR */

    while (1)
    {
        vTaskDelay(portCONVERT_MS_2_TICKS(100));
    }

    return;
}

UINT ota_update_process_create(OTA_UPDATE_CONFIG * update_conf)
{
    UINT status = OTA_SUCCESS;
    UINT wait_cnt = 0;
    BaseType_t	xReturned;
    TaskHandle_t task_handle;

    RA6W1_UNUSED_ARG(status);

    if (ota_update_get_proc_state() == OTA_STATE_PROGRESS) 
    {
        if (ota_update_process_stop() != OTA_SUCCESS)
        {
            return OTA_FAILED;
        }
        vTaskDelay(portCONVERT_MS_2_TICKS(100));
    }

    OTA_INFO("\n- OTA Update : <%s> Download - Start\n", ota_update_type_to_text(update_conf->update_type));

    wait_cnt = 0;
    while (chk_network_ready((UCHAR) interface_select) != pdTRUE)
    {
        vTaskDelay(portCONVERT_MS_2_TICKS(100));
        wait_cnt++;

        if (wait_cnt == 100)
        {
            OTA_DBG("[%s] Wait to initialize WLAN ...(%d)\n", __func__, wait_cnt / 10);
            OTA_ERR("- OTA : No network connection\n");
            wait_cnt = 0;
            return OTA_FAILED;

        }
        else if ((wait_cnt % 100) == 0)
        {
            OTA_DBG("[%s] Wait to initialize WLAN ...(%d)\n", __func__, wait_cnt / 10);
        }
    }

    if ((update_conf->update_type == OTA_TYPE_RTOS)
        || (update_conf->update_type == OTA_TYPE_BLE_COMBO))
    {
        ota_proc->progress_rtos = 0;
    }

    if (update_conf->update_type == OTA_TYPE_MCU_FW)
    {
        ota_proc->progress_mcu_fw = 0;
    }

    if (update_conf->update_type == OTA_TYPE_CERT_KEY)
    {
        ota_proc->progress_cert_key = 0;
    }

    ota_proc->update_type = update_conf->update_type;
    memcpy(ota_proc->url, update_conf->url, OTA_HTTP_URL_LEN);
    ota_proc->auto_renew = update_conf->auto_renew;
    ota_proc->download_sflash_addr = update_conf->download_sflash_addr;
    ota_proc->download_notify = update_conf->download_notify;
    ota_proc->renew_notify = update_conf->renew_notify;

    if (ota_proc_xHandle)
    {
        task_handle = ota_proc_xHandle;
        ota_proc_xHandle = NULL;
        vTaskDelete(task_handle);
    }

    xReturned = xTaskCreate(ota_update_process,
                            OTA_TASK_NAME,
                            OTA_TASK_STACK_SZ,
                            NULL,
                            OS_TASK_PRIORITY_USER,
                            &ota_proc_xHandle);

    if (xReturned != pdPASS)
    {
        OTA_ERR(RED_COLOR " [%s] Failed task create %s \r\n" CLEAR_COLOR, __func__, OTA_TASK_NAME);
        return OTA_FAILED;
    }

    return OTA_SUCCESS;
}

/* Write the downloaded data directly to sflash. */
UINT ota_update_buffer_write_flash(ota_update_sflash_t * sflash_ctx,
                             UCHAR *data, UINT length)
{
    UINT status = OTA_SUCCESS;
    UINT buff_offset = 0;
    UINT copyToBufLen = 0;
    UINT writeToFlashLen = 0;
    UINT input_len = 0;
    UCHAR *input_data = NULL;

    if (sflash_ctx == NULL)
    {
        OTA_ERR("[%s:%d] sflash_ctx error\n", __func__, __LINE__);
        status = OTA_FAILED;
        goto finish;
    }

    if (length == 0)
    {
        OTA_ERR("[%s:%d] Data length error\n", __func__, __LINE__);
        status = OTA_FAILED;
        goto finish;
    }

    if (sflash_ctx->buffer == NULL)
    {
        sflash_ctx->buffer = (UCHAR *) OTA_MALLOC(OTA_SFLASH_BUF_SZ);
        if (sflash_ctx->buffer == NULL)
        {
            OTA_ERR("[%s:%d] Failed to allocate receive buffer(%d bytes)\n", __func__, __LINE__,
                    OTA_SFLASH_BUF_SZ);
            status = OTA_MEM_ALLOC_FAILED;
            goto finish;
        }
        memset(sflash_ctx->buffer, 0x00, OTA_SFLASH_BUF_SZ);
    }

    input_data = data;
    input_len = length;

    sflash_ctx->length += input_len;

    if (sflash_ctx->offset > 0 )
    {
        writeToFlashLen = sflash_ctx->offset;
    }

    writeToFlashLen += input_len;

    while (writeToFlashLen > 0)
    {
        copyToBufLen = OTA_SFLASH_BUF_SZ - sflash_ctx->offset;

        if (copyToBufLen > (input_len - buff_offset))
        {
            copyToBufLen = (input_len - buff_offset);
        }

        if (copyToBufLen > 0)
        {
            memcpy(&sflash_ctx->buffer[sflash_ctx->offset],
                   (input_data + buff_offset),
                   copyToBufLen);
            buff_offset += copyToBufLen;
            sflash_ctx->offset += copyToBufLen;
        }

        if ((sflash_ctx->offset == OTA_SFLASH_BUF_SZ)
                || (sflash_ctx->offset == sflash_ctx->total_length)
                || ((sflash_ctx->length == sflash_ctx->total_length) && (sflash_ctx->offset > 0)))
        {
            /* SFLASH write */
            if (ota_update_write_flash(sflash_ctx->sflash_addr,
                                 &sflash_ctx->buffer[0],
                                 sflash_ctx->offset) != sflash_ctx->offset) {
                OTA_ERR("[%s:%d] Flash write failed(addr=0x%x, size=%d)\n", __func__, __LINE__,
                    sflash_ctx->sflash_addr,
                    sflash_ctx->offset);
                status = OTA_FAILED;
                goto finish;
            }

            sflash_ctx->sflash_addr += sflash_ctx->offset;

            memset(sflash_ctx->buffer, 0x00, OTA_SFLASH_BUF_SZ);
            sflash_ctx->offset = 0;

            if (writeToFlashLen >= OTA_SFLASH_BUF_SZ)
            {
                writeToFlashLen = writeToFlashLen - OTA_SFLASH_BUF_SZ;
            }
        }
        else
        {
            break;
        }
    }

finish:

    if ((status != OTA_SUCCESS)
            || (sflash_ctx->length == sflash_ctx->total_length))
    {
        sflash_ctx->sflash_addr = 0x00;
        sflash_ctx->total_length = 0;
        sflash_ctx->length = 0;
        sflash_ctx->offset = 0;

        if (sflash_ctx->buffer != NULL)
        {
            OTA_FREE(sflash_ctx->buffer);
            sflash_ctx->buffer = NULL;
        }
    }

    return status;
}

#endif	// (__SUPPORT_OTA__)

/* EOF */
