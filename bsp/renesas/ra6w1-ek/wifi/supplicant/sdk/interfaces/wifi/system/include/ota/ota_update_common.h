/**
 ****************************************************************************************
 *
 * @file ota_update_common.h
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


#if !defined (__OTA_UPDATE_COMMON_H__)
#define	__OTA_UPDATE_COMMON_H__

#if defined (__SUPPORT_OTA__)
#include <stdio.h>
#include "sys_app_defs.h"

#include "ra6w1_image.h"
#include "ota_update.h"

#if __has_include("rm_awsiot_w_cfg.h")
#include "rm_awsiot_w_cfg.h"
#endif

#if !defined (BIT)
#define BIT(x) (1 << (x))
#endif // (BIT)

// For dynamic memory allocation ...
#define USE_HEAP_MEM
#if defined (USE_HEAP_MEM)
#define OTA_MALLOC 	pvPortMalloc
#define OTA_FREE		vPortFree
#else
#define OTA_MALLOC 	APP_MALLOC
#define OTA_FREE		APP_FREE
#endif // (USE_HEAP_MEM)
/// Disable version checking for debugging
#undef DISABLE_OTA_VER_CHK

/// OTA update thread name
#define	OTA_TASK_NAME				"OTA_update"
#define	OTA_DPM_REG_NAME			OTA_TASK_NAME

#define	OTA_TASK_STACK_SZ			(1024 * 5) / 4 //WORD
#define OTA_SFLASH_BUF_SZ			SF_PARTITION_TBL_SIZE // 4KB

#define OTA_TIMEOUT 				15   //sec
#define OTA_EVT_TIMEOUT				0x00
#define OTA_EVT_RECEIVE				0x01
#define OTA_EVT_FINISH				0x10

/// NVRAM name of Download progress
#define OTA_NVRAM_DW_PROGRESS        "OTA_PROG_"

/// Process is not ready.
#define OTA_STATE_NOT_READY    		0
/// Process is ready.
#define OTA_STATE_READY       		1
/// Process is ongoing.
#define OTA_STATE_PROGRESS			2
/// Process completed.
#define OTA_STATE_FINISH			3
/// Process stopped.
#define OTA_STATE_STOP				4

#define OTA_VER_DELIMITER			"-"
#define OTA_VER_START_OFFSET		8   //address 0x02008 ~ 0x02063
#define OTA_FW_MAGIC_NUM			{0x44, 0x41, 0x31, 0x36} //IMAGE_HEADER_MAGIC_CODE 0x36314144

/// Firmware version structure max length in bytes.
#define UPDATE_TYPE_MAX             12
#define MODULE_MAX                  12
#define SDK_MAX                     14
#define CUSTOMER_MAX                20

typedef struct {
    // Update type(soc)
    CHAR update_type[UPDATE_TYPE_MAX + 1];
    // Module name
    CHAR module[MODULE_MAX + 1];
    // SDK version
    CHAR sdk[SDK_MAX + 1];
    // Customer version
    CHAR customer[CUSTOMER_MAX + 1];
} FW_versionInfo_t;

// Status for version check.
enum ota_version_flags {
    // Init value.
    OTA_HEADER_INIT                 = 0,
    // The type of FW to be compared is the same.
    OTA_HEADER_SAME_FW_TYPE,
    // The Module name of FW to be compared is the same.
    OTA_HEADER_SAME_MODULE,
    // The SDK version of FW to be compared is the different.
    OTA_HEADER_DIFF_SDK,
    // The customer version of FW to be compared is the different.
    OTA_HEADER_DIFF_CUST,
    // The type of FW to be compared is the different.
    OTA_HEADER_DIFF_FW_TYPE,
    // The Module of FW to be compared is the different.
    OTA_HEADER_DIFF_MODULE,
    // The magic number of the FW is the same.
    OTA_HEADER_SAME_MAGIC,
    // The magic number of the FW is not compatible.
    OTA_HEADER_INCOMPATI_MAGIC,
    // The FW is not involved in version checking. (user FW or cert)
    OTA_HEADER_VER_DONT_CARE,
    // Error.
    OTA_HEADER_ERROR
};

/// Struct for flash writing
typedef struct {
    UINT	sflash_addr;
    UINT	total_length;
    UINT	length;
    UINT	offset;
    UCHAR	* buffer;
} ota_update_sflash_t;

/// Structure to download firmware
typedef struct {
    ota_update_type	update_type;
    ota_update_sflash_t write;
    UINT    download_status;
    UINT    version_check;
    UINT    content_length;
    UINT    received_length;
    UINT    httpc_result;
} ota_update_download_t;

/// Settings structure used for ota update requests.
typedef struct {
    /// FW type being downloaded
    ota_update_type	update_type;
    /// OTA Server address where RTOS firmware exists.
    char	url[OTA_HTTP_URL_LEN];
    /// If the value is true, if the new firmware download is successful, it will reboot with the new firmware. Only for RTOS
    UINT	auto_renew;
    /// Flash address where the downloaded FW is written.
    UINT 	download_sflash_addr;
    /// Callback function pointer to check the download status.
    void 	(* download_notify) (ota_update_type update_type, UINT ret_status, UINT progress);
    /// Callback function pointer to check the renew state. Only for RTOS.
    void 	(* renew_notify) (UINT ret_status);
    /// Status of the download process (not_ready, ready, progress, finish, stop).
    UINT	update_state;
    /// Downlaod status. (success or failed)
    UINT	status;
    /// RTOS download progress.
    UINT	progress_rtos;
    /// MCU FW download progress.
    UINT 	progress_mcu_fw;
    /// CERT_KEY download progress.
    UINT 	progress_cert_key;
} ota_update_proc_t;


UINT ota_update_check_version(ota_update_type update_type, UCHAR * data, UINT data_len);
UINT ota_update_parse_version_string(UCHAR * version, FW_versionInfo_t * fw_ver);
const char * ota_update_type_to_text(ota_update_type update_type);
UINT32 ota_update_sflash_rtos_crc(UINT sectorAddr);
UINT ota_update_get_available_size(ota_update_type update_type);
UINT ota_update_check_available_size(ota_update_type update_type, UINT size);
UINT ota_update_get_curr_sflash_addr(ota_update_type update_type);
UINT ota_update_set_user_sflash_addr(UINT sflash_addr);
UINT ota_update_check_all_download(void);
UINT ota_update_current_fw_renew(void);
void ota_update_status_atcmd(UINT atcmd_event, UINT status);


UINT ota_update_check_refuse_flag(void);
UINT ota_update_process_create(OTA_UPDATE_CONFIG * update_conf);
UINT ota_update_check_state(void);
UINT ota_update_process_stop(void);
void ota_update_set_download_progress(ota_update_type update_type, UINT progress);
UINT ota_update_get_download_progress(ota_update_type update_type);
void ota_update_write_nvram_download_progress(ota_update_type update_type, UINT progress);
UINT ota_update_read_nvram_download_progress(ota_update_type update_type);
void ota_update_evt_send(UINT event);
#if defined (__SUPPORT_MATTER_IOT__) || defined (__SUPPORT_AWS_IOT_W__) 
UINT ota_update_set_proc_state(UINT state);
#endif
UINT ota_update_get_proc_state(void);
void ota_update_print_status(ota_update_type update_type, UINT status);
UINT ota_update_buffer_write_flash(ota_update_sflash_t * sflash_ctx, UCHAR * data, UINT length);
UINT ota_update_get_image_info(UINT sectorAddr, image_header_data_t * infoImage);
uint32_t ota_update_crc32(const void * buf, size_t size);
void ota_update_set_boot_index(UINT boot_idx);
UINT ota_update_get_boot_index(void);
UINT ota_update_toggle_boot_index(void);

UINT ota_update_sflash_product_header_crc(void);
bool cmd_ota_update(int argc, char * argv[]);
void ota_update_print_fw_header(image_header_data_t * infoImage);

#endif	/* (__SUPPORT_OTA__) */
#endif	/* (__OTA_UPDATE_COMMON_H__) */

/* EOF */
