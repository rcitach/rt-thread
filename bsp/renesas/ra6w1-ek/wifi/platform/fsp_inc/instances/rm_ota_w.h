/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef RM_OTA_W_H
#define RM_OTA_W_H

/*******************************************************************************************************************//**
 * @addtogroup RM_OTA_W
 * @{
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include <string.h>
#include "bsp_api.h"
#include "rm_ota_w_api.h"
#include "sdk_defs.h"
#include "FreeRTOS.h"
#include "rm_ota_w_cfg.h"


/* Common macro for FSP header files. There is also a corresponding FSP_FOOTER macro at the end of this file. */
FSP_HEADER

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#ifndef RRQ61X_OSPI_W_ENABLED
#define RRQ61X_OSPI_W_ENABLED
#endif /* RRQ61X_OSPI_W_ENABLED */

#define BOLD_GREEN   "\033[1m\033[32m"
#define PRINTF_RESET "\033[0m"

#if (TEST_OTA_API == 1)
#undef RM_OTA_W_PRINTF_INFO_EN
#undef RM_OTA_W_PRINTF_DBG_EN
#undef RM_OTA_W_PRINTF_ERR_EN
#else
#define RM_OTA_W_PRINTF_INFO_EN
#define RM_OTA_W_PRINTF_DBG_EN
#define RM_OTA_W_PRINTF_ERR_EN
#endif

#if defined (RM_OTA_W_PRINTF_INFO_EN)
#define RM_OTA_W_INFO printf
#else
#define RM_OTA_W_INFO(...) do { } while (0);
#endif /* RM_OTA_W_PRINTF_INFO_EN */

#if defined (RM_OTA_W_PRINTF_DBG_EN)
#define RM_OTA_W_DBG printf
#else
#define RM_OTA_W_DBG(...) do { } while (0);
#endif /* RM_OTA_W_PRINTF_DBG_EN */

#if defined (RM_OTA_W_PRINTF_ERR_EN)
#define RM_OTA_W_ERR printf
#else
#define RM_OTA_W_ERR(...) do { } while (0);
#endif /* RM_OTA_W_PRINTF_ERR_EN */

#define RM_OTA_W_GROUP_EVT_EN          0

/** "OPEN" in ASCII, used to avoid multiple open. */
#define RM_OTA_W_OPEN                  (0x4f505A5AULL)
#define RM_OTA_W_CLOSE                 (0x00000000ULL)

#define RM_OTA_W_CLOSE_EVT             0x01
#define	RM_OTA_W_TASK_NAME             "OTA_update"
#define	RM_OTA_W_DPM_REG_NAME          RM_OTA_W_STOR_USER_START
#define	RM_OTA_W_TASK_STACK_SZ         (1024 * 5) / 4

/* 4KB */
#define RM_OTA_W_SFLASH_BUF_SZ         SF_PARTITION_TBL_SIZE

/* SFLASH address definition */
#define RM_OTA_W_STOR_UNKNOWN_ADDR     0xFFFFFFFF
#define RM_OTA_W_STOR_RTOS_SIZE        SF_RTOS_SIZE
#define RM_OTA_W_STOR_RTOS_0_ADDR      SF_RTOS_0
#define RM_OTA_W_STOR_RTOS_1_ADDR      SF_RTOS_1

#define RM_OTA_W_STOR_USER_SIZE        SF_USER_AREA_SIZE
#define RM_OTA_W_STOR_USER_START       SF_USER_AREA
#define RM_OTA_W_STOR_USER_END         (SF_USER_AREA + SF_USER_AREA_SIZE)

#define RM_OTA_W_RTOS_NAME             "RTOS"
#define RM_OTA_W_MCU_FW_NAME           "MCU_FW"
#define RM_OTA_W_BLE_FW_NAME           "BLE_FW"
#define RM_OTA_W_BLE_COMBO_NAME        "BLE_COMBO"
#define RM_OTA_W_CERT_KEY_NAME         "CERT_KEY"
#define RM_OTA_W_START_HDR_CHECK       "200 OK"
#define RM_OTA_W_DOWNLOAD_IDLE         0
#define RM_OTA_W_DOWNLOAD_IN_PROGRESS  1
#define RM_OTA_W_DOWNLOAD_DONE         100
#define RM_OTA_W_CONTENT_LEN_INVALID   0xFFFFFFFF
#define RM_OTA_W_VER_NAME_SIZE         80
/// NVRAM name of Download progress
#define RM_OTA_W_NVRAM_DW_PROGRESS     "OTA_PROG_"
#define RM_OTA_W_MCU_BUF_SIZE	       (2048)

/// Return success
#define RM_OTA_W_SUCCESS               0x00
/// Return failed
#define RM_OTA_W_FAILED                0x01
/// SFLASH address is wrong
#define RM_OTA_W_ERROR_SFLASH_ADDR     0x02
/// FW type is unknown
#define RM_OTA_W_ERROR_TYPE            0x03
/// Server URL is unknown
#define RM_OTA_W_ERROR_URL             0x04
/// FW size is wrong or offset address to be downloaded is wrong
#define RM_OTA_W_ERROR_SIZE            0x05
/// CRC is not correct
#define RM_OTA_W_ERROR_CRC             0x06
/// FW version is unknown
#define RM_OTA_W_VERSION_UNKNOWN       0x07
/// FW version is incompatible
#define RM_OTA_W_VERSION_INCOMPATI     0x08
/// FW not found on the server
#define RM_OTA_W_NOT_FOUND             0x09
/// Failed to connect to server
#define RM_OTA_W_NOT_CONNECTED         0x0a
/// All new FWs have not been downloaded
#define RM_OTA_W_NOT_ALL_DOWNLOAD      0x0b
/// Failed to alloc memory
#define RM_OTA_W_MEM_ALLOC_FAILED      0x0c
#if defined (__IMG_UPDATE_BY_MCU__)
/// Failed to SFLASH write
#define RM_OTA_W_FAILED_WRITE          0x0e
/// Failed to create timer
#define RM_OTA_W_FAILED_TIMER          0x0f
/// Timeout
#define RM_OTA_W_FAILED_TIMEOUT        0x10
/// Not initialized
#define RM_OTA_W_NOT_READY             0x11
#endif
/// BLE FW version is unknown
#define RM_OTA_W_BLE_VERSION_UNKNOWN   0xa1

#define RM_OTA_W_MALLOC  pvPortMalloc
#define RM_OTA_W_FREE    vPortFree

/// Image MAGIC CODE
#define IMAGE_HEADER_MAGIC_CODE        0x36314144
/// Image Name Length
#define IMAGE_HEADER_NAME_LEN          64
#define UNKNOWN_PROGRESS               0
#define RM_OTA_W_BOOT_IDX_0            0U
#define RM_OTA_W_BOOT_IDX_1            1U

/// Process is not ready.
#define RM_OTA_W_STATE_NOT_READY       0
/// Process is ready.
#define RM_OTA_W_STATE_READY           1
/// Process is ongoing.
#define RM_OTA_W_STATE_PROGRESS        2
/// Process completed.
#define RM_OTA_W_STATE_FINISH          3
/// Process stopped.
#define RM_OTA_W_STATE_STOP            4

#define RM_OTA_W_TIMEOUT               15
#define RM_OTA_W_EVT_TIMEOUT           0x00
#define RM_OTA_W_EVT_RECEIVE           0x01
#define RM_OTA_W_EVT_FINISH            0x10

/* 5 secs */
#define RM_OTA_W_WAIT_TIME             50;

#define RM_OTA_W_VER_DELIMITER         "-"
/// address 0x02008 ~ 0x02063
#define RM_OTA_W_VER_START_OFFSET      8
/// IMAGE_HEADER_MAGIC_CODE 0x36314144
#define RM_OTA_W_FW_MAGIC_NUM          {0x44, 0x41, 0x31, 0x36}

/// Firmware version structure.
#define RM_OTA_W_UPDATE_TYPE_MAX       12
#define RM_OTA_W_MODULE_MAX            12
#define RM_OTA_W_SDK_MAX               14
#define RM_OTA_W_CUSTOMER_MAX          20

#define RM_OTA_W_BY_MCU                "tx_size="

/// MCU Firmware version structure.
#define RM_OTA_W_MCU_FW_NAME_LEN       (8)
#define RM_OTA_W_MCU_FW_HEADER_SIZE    sizeof(rm_ota_w_mcu_fw_info_t)

#define RM_OTA_W_CURRENT_ADDR          0
#define RM_OTA_W_NEW_ADDR              1
#define RM_OTA_W_USER_ADDR             2

#define RM_OTA_W_BOOT_IDX_TOGGLE       255
#define RM_OTA_W_SWAP_NOT_FORCED       0
#define RM_OTA_W_SWAP_FORCED           1

#define RM_OTA_W_PROD_HDR_OFST_BOOT_IDX  2
#define RM_OTA_W_PROD_HDR_OFST_UPG_ADD   6
#define RM_OTA_W_PROD_HDR_OFST_CRC       29
#define RM_OTA_W_DL_PROG_ERROR           255

/*************************************************************************************************
 * Type defines for the SPI interface API
 *************************************************************************************************/
typedef enum e_rm_ota_w_access_type
{
    RM_OTA_W_ACCESS_TYPE_READ              = 0,
    RM_OTA_W_ACCESS_TYPE_WRITE             = 1,
    RM_OTA_W_ACCESS_TYPE_COPY              = 2,
    RM_OTA_W_ACCESS_TYPE_ERASE             = 3,
    RM_OTA_W_ACCESS_TYPE_GET_MCU_FW_NAME   = 4,
    RM_OTA_W_ACCESS_TYPE_SET_MCU_FW_NAME   = 5,
    RM_OTA_W_ACCESS_TYPE_GET_MCU_FW_SIZE   = 6,
    RM_OTA_W_ACCESS_TYPE_CHECK_MCU_FW_CRC  = 7,
    RM_OTA_W_ACCESS_TYPE_READ_MCU_FW       = 8,
    RM_OTA_W_ACCESS_TYPE_TX_TO_MCU_FW      = 9,
    RM_OTA_W_ACCESS_TYPE_TX_BY_MCU_FW      = 10,
    RM_OTA_W_ACCESS_TYPE_ERASE_MCU_FW      = 11,
} rm_ota_w_access_type_t;

typedef enum e_rm_ota_w_validate_type
{
    RM_OTA_W_VALIDATE_TYPE_PROD_CRC    = 0,
    RM_OTA_W_VALIDATE_TYPE_IMG_CRC     = 1,
} rm_ota_w_validate_type_t;

/// Status for version check.
typedef enum e_rm_ota_w_version_flags {
    /// Init value.
    RM_OTA_W_HEADER_INIT = 0,
    /// The type of FW to be compared is the same.
    RM_OTA_W_HEADER_SAME_FW_TYPE,
    /// The Module name of FW to be compared is the same.
    RM_OTA_W_HEADER_SAME_MODULE,
    /// The SDK version of FW to be compared is different.
    RM_OTA_W_HEADER_DIFF_SDK,
    /// The customer version of FW to be compared is different.
    RM_OTA_W_HEADER_DIFF_CUST,
    /// The type of FW to be compared is different.
    RM_OTA_W_HEADER_DIFF_FW_TYPE,
    /// The Module of FW to be compared is the different.
    RM_OTA_W_HEADER_DIFF_MODULE,
    /// The magic number of the FW is the same.
    RM_OTA_W_HEADER_SAME_MAGIC,
    /// The magic number of the FW is not compatible.
    RM_OTA_W_HEADER_INCOMPATI_MAGIC,
    /// The FW is not involved in version checking. (user FW or cert)
    RM_OTA_W_HEADER_VER_DONT_CARE,
    /// Error.
    RM_OTA_W_HEADER_ERROR
} rm_ota_w_version_flags_t;

typedef enum e_rm_ota_w_refuse_state
{
    /// INIT value
    RM_OTA_W_REFUSE_CLR,
    /// RTOS FW update
    RM_OTA_W_REFUSE_SET,
} rm_ota_w_refuse_type_t;

typedef enum e_rm_ota_w_state
{
    RM_OTA_W_STATUS_IDLE,                 ///< The flash is idle.
    RM_OTA_W_STATUS_BUSY                  ///< The flash is currently processing a command.
} rm_ota_w_state_t;

typedef struct st_rm_ota_w_mcu_fw_info
{
    char name[RM_OTA_W_MCU_FW_NAME_LEN];
    UINT size;
    UINT crc;
} rm_ota_w_mcu_fw_info_t;

typedef struct st_rm_ota_w_fw_version_info
{
    /// Update type
    CHAR update_type[RM_OTA_W_UPDATE_TYPE_MAX + 1];
    /// Module name
    CHAR module[RM_OTA_W_MODULE_MAX + 1];
    /// SDK version
    CHAR sdk[RM_OTA_W_SDK_MAX + 1];
    /// Customer version
    CHAR customer[RM_OTA_W_CUSTOMER_MAX + 1];
} rm_ota_w_fw_version_info_t;

typedef struct st_rm_ota_w_status
{
    uint32_t      error_code;
    uint32_t      download_porgress;
    rm_ota_w_state_t state;
} rm_ota_w_status_t;

typedef struct st_rm_ota_w_info
{
    uint8_t     * version;
    uint32_t      data_size;
    uint32_t      ivt;
    uint32_t      hcrc;
    uint32_t      dcrc;
} rm_ota_w_info_t;

/** Possible Flash operation states */
typedef enum e_rm_ota_w_bgo_operation
{
    RM_OTA_W_OPERATION_NON_BGO,
    RM_OTA_W_OPERATION_DF_BGO_WRITE,
    RM_OTA_W_OPERATION_DF_BGO_ERASE,
    RM_OTA_W_OPERATION_DF_BGO_BLANKCHECK,
} rm_ota_w_bgo_operation_t;

/// Struct for flash writing
typedef struct st_rm_ota_w_update_sflas
{
    UINT sflash_addr;                       ///< Starting address for the image
    UINT total_length;                      ///< Overall length of the block
    UINT length;                            ///< Buffer length
    UINT offset;                            ///< Update offset from starting address
    UCHAR * buffer;                         ///< Pointer to the data buffer
} rm_ota_w_update_sflash_t;

/// Structure to download firmware
typedef struct st_rm_ota_w_update_download
{
    rm_ota_w_update_type_t update_type;     ///< Update type
    rm_ota_w_update_sflash_t write;         ///< Write struct field
    UINT download_status;                   ///< Status of the downloading operation
    UINT version_check;                     ///< Version check information
    UINT content_length;                    ///< The size of the download image in bytes
    UINT received_length;                   ///< Updated data which was received in the current process
} rm_ota_w_update_download_t;

/** Ota b instance control block. DO NOT INITIALIZE. */
typedef struct st_ota_instance_ctrl
{
    uint32_t                opened;                     ///< To check whether api has been opened or not.
    ota_cfg_t const         * p_cfg;
    uint32_t                timeout_erase_cf_small_block;
    uint32_t                timeout_write_cf;
    uint32_t                timeout_write_df;
    uint32_t                timeout_dbfull;
    uint32_t                timeout_blank_check;
    uint32_t                timeout_write_config;
    uint32_t                timeout_erase_cf_large_block;
    uint32_t                timeout_erase_df_block;
    uint32_t                source_start_offset;        ///< Offset for continous download update
    uint32_t                dest_end_address;
    uint32_t                operations_remaining;
    rm_ota_w_bgo_operation_t current_operation;          ///< Operation in progress, for example, FLASH_OPERATION_CF_ERASE
    rm_ota_w_update_type_t  ota_update_type;            ///< Update type
    uint8_t                 update_cache[RM_OTA_W_CACHE_LEN];  ///< Server address where firmware exists.
    void (* download_notify)(rm_ota_w_update_type_t update_type, uint32_t ret_status, uint32_t progress); ///< Callback function pointer to check the download status.
    void (* swap_notify)(uint32_t ret_status);          ///< Callback function pointer to check the swap state. Only for RTOS.
    uint8_t                 ota_auto_swap;              ///< If true, reboot after FW download process is successfully (Only for RTOS)
    uint32_t                download_sflash_addr;       ///< Address of SFLASH 2nd FW is stored. (Only for MCU_FW and CERT_KEY)
    uint8_t                 update_state;               ///< Download status. (success or failed)
    uint8_t                 status;                     ///< Status of the download process (not_ready, ready, progress, finish, stop).
    uint8_t                 progress_rtos;              ///< RTOS download progress.
    uint8_t                 progress_mcu_fw;            ///< MCU FW download progress.
    uint8_t                 progress_cert_key;          ///< CERT_KEY download progress.
    uint8_t	                progress_ble;               ///< BLE download progress.
    rm_ota_w_update_download_t download_info;
    uint8_t                 ota_refuse_flag;
    uint32_t                ota_custom_address;         ///< Set the address for custom data for fw_mcu and cert keys
    char                    mcu_fw_name[RM_OTA_W_MCU_FW_NAME_LEN]; ///< Set MCU FW name
    void (* p_callback)(ota_callback_args_t *);         ///< Pointer to callback
    ota_callback_args_t * p_callback_memory;            ///< Pointer to optional callback argument memory
    void const            * p_context;                  ///< Pointer to context to be passed into callback function
} rm_ota_w_instance_ctrl_t;

/**********************************************************************************************************************
 * Exported global variables
 **********************************************************************************************************************/

/** @cond INC_HEADER_DEFS_SEC */
/** Filled in Interface API structure for this Instance. */
extern const ota_api_t g_ota_on_ota_w;
/** @endcond */

/***********************************************************************************************************************
 * Public APIs
 **********************************************************************************************************************/
fsp_err_t RM_OTA_W_Open(ota_ctrl_t * const p_api_ctrl, ota_cfg_t const * const p_cfg);
fsp_err_t RM_OTA_W_Close(ota_ctrl_t * const p_api_ctrl);
fsp_err_t RM_OTA_W_Swap(ota_ctrl_t * const p_api_ctrl);
fsp_err_t RM_OTA_W_GetImageInfo(ota_ctrl_t * const p_api_ctrl,
                               rm_ota_w_update_type_t update_type,
                               uint32_t sector_addr,
                               rm_ota_w_image_header_data_t * info_image);
fsp_err_t RM_OTA_W_BootIdxSet(ota_ctrl_t * const p_api_ctrl, uint8_t boot_idx);
fsp_err_t RM_OTA_W_BootIdxGet(ota_ctrl_t * const p_api_ctrl, uint8_t * boot_idx);
fsp_err_t RM_OTA_W_GetAddr(ota_ctrl_t * const p_api_ctrl, uint8_t state, rm_ota_w_update_type_t update_type, uint32_t * addr);
fsp_err_t RM_OTA_W_SetAddr(ota_ctrl_t * const p_api_ctrl, uint8_t update_type, uint32_t addr);
fsp_err_t RM_OTA_W_Cert(ota_ctrl_t * const p_api_ctrl, uint8_t cert_type, uint32_t sector_addr);

/*******************************************************************************************************************//**
 * @} (end ingroup RM_OTA_W)
 **********************************************************************************************************************/

/** Common macro for FSP header files. There is also a corresponding FSP_HEADER macro at the top of this file. */
FSP_FOOTER

#endif
