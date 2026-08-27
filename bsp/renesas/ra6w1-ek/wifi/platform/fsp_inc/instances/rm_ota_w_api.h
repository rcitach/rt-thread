/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/*******************************************************************************************************************//**
 * @ingroup RENESAS_SYSTEM_INTERFACES
 * @defgroup OTA_API OTA Interface
 * @brief Interface for the OTA Functionality.
 *
 * @section OTA_API_SUMMARY Summary
 *
 * The OTA interface provides the ability to read, write, erase, and blank check the FW versions and data regions.
 *
 *
 * @{
 **********************************************************************************************************************/

#ifndef RM_OTA_W_API_H
#define RM_OTA_W_API_H

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

/* Register definitions, common services and error codes. */
#include "bsp_api.h"

/* Common macro for FSP header files. There is also a corresponding FSP_FOOTER macro at the end of this file. */
FSP_HEADER

/**********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#define RM_OTA_W_CACHE_LEN         (256)
#define RM_OTA_W_IMAGE_NAME_LEN    (64)

/**********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/** Update type for certain operations */
typedef enum e_rm_ota_w_update_type
{
    /// INIT value
    RM_OTA_W_TYPE_INIT,
    /// RTOS FW update
    RM_OTA_W_TYPE_RTOS,
    /// BLE FW update
    RM_OTA_W_TYPE_BLE_FW,
    /// RTOS and BLE FW update
    RM_OTA_W_TYPE_BLE_COMBO,
    /// MCU FW update
    RM_OTA_W_TYPE_MCU_FW,
    /// Certification key update
    RM_OTA_W_TYPE_CERT_KEY,
#ifdef __SUPPORT_MATTER_IOT__
    /// MCU FW update by stream
    RM_OTA_W_TYPE_MCU_FW_STREAM,
#endif
    RM_OTA_W_TYPE_UNKNOWN,
} rm_ota_w_update_type_t;

/** Event types returned by the ISR callback when used in Data OTA BGO mode */
typedef enum e_ota_event
{
    RM_OTA_W_DOWNLOAD_RESULT_OK            = 0,    ///< File successfully received
    RM_OTA_W_DOWNLOAD_RESULT_ERR_UNKNOWN   = 1,    ///< Unknown error
    RM_OTA_W_DOWNLOAD_RESULT_ERR_CONNECT   = 2,    ///< Connection to server failed
    RM_OTA_W_DOWNLOAD_RESULT_ERR_HOSTNAME  = 3,    ///< Failed to resolve server hostname
    RM_OTA_W_DOWNLOAD_RESULT_ERR_CLOSED    = 4,    ///< Connection unexpectedly closed by remote server
    RM_OTA_W_DOWNLOAD_RESULT_ERR_TIMEOUT   = 5,    ///< Connection timed out (server didn't respond in time)
    RM_OTA_W_DOWNLOAD_RESULT_ERR_SVR_RESP  = 6,    ///< Server responded with an error code
    RM_OTA_W_DOWNLOAD_RESULT_ERR_MEM       = 7,    ///< Local memory error
    RM_OTA_W_DOWNLOAD_RESULT_LOCAL_ABORT   = 8,    ///< Local abort
    RM_OTA_W_DOWNLOAD_RESULT_ERR_CONTENT_LEN = 9   ///< Content length mismatch
} ota_event_t;

/** Settings structure used for ota update requests */
typedef struct st_rm_ota_w_update_proc
{
    rm_ota_w_update_type_t	update_type;            ///< FW type being downloaded
    char update_cache[RM_OTA_W_CACHE_LEN];         ///< Cache data from last update.
    uint32_t auto_swap;                         ///< If the value is true, if the new firmware download is successful, it will reboot with the new firmware. Only for RTOS
    uint32_t download_sflash_addr;              ///< Flash address where the downloaded FW is written.
    void (* download_notify)(rm_ota_w_update_type_t update_type, uint32_t ret_status, uint32_t progress);   ///< Callback function pointer to check the download status.
    void (* swap_notify)(uint32_t ret_status);  ///< Callback function pointer to check the swap state. Only for RTOS.
    uint32_t update_state;                      ///< Status of the download process (not_ready, ready, progress, finish, stop).
    uint32_t status;                            ///< Download status. (success or failed)
    uint32_t progress_rtos;                     ///< RTOS download progress.
    uint32_t progress_mcu_fw;                   ///< MCU FW download progress.
    uint32_t progress_cert_key;                 ///< CERT_KEY download progress.
} rm_ota_w_update_proc_t;

/** Image header data for updates */
typedef struct st_rm_ota_w_image_header_data
{
    uint32_t magic_code;
    uint32_t version;
    uint8_t name[RM_OTA_W_IMAGE_NAME_LEN];
    uint32_t ivt_location;
    uint32_t size;
    uint32_t crc;
    uint32_t secure_boot_option;
    uint32_t secure_cert_size;
    uint32_t secure_cert_crc;
    uint32_t header_crc;
} rm_ota_w_image_header_data_t;

typedef struct st_ota_block_info
{
    uint32_t block_section_st_addr;             ///< Starting address for this block section (blocks of this size)
    uint32_t block_section_end_addr;            ///< Ending address for this block section (blocks of this size)
    uint32_t block_size;                        ///< OTA erase block size
    uint32_t block_size_write;                  ///< OTA write block size
} ota_block_info_t;

/** OTA block details */
typedef struct st_ota_regions
{
    uint32_t num_regions;                       ///< Length of block info array
    ota_block_info_t const * p_block_array;     ///< Block info array base address
} ota_regions_t;

/** Information about the ota blocks */
typedef struct st_ota_info
{
    ota_regions_t code_ota;                     ///< Information about the code ota regions
    ota_regions_t data_ota;                     ///< Information about the code ota regions
} ota_info_t;

/** OTA control block. Allocate an instance specific control block to pass into the OTA API calls.
 */
typedef void ota_ctrl_t;

/** Callback function parameter data */
typedef struct st_ota_user_cb_data
{
    ota_event_t event;                          ///< Event can be used to identify what caused the callback (ota ready or error).
    void const  * p_context;                    ///< Placeholder for user data.  Set in @ref ota_api_t::open function in::ota_cfg_t.
} ota_callback_args_t;

/** OTA Configuration */
typedef struct st_ota_cfg
{
    bool data_ota_bgo;                          ///< True if BGO (Background Operation) is enabled for Data OTA.

    /* Configuration for OTA Event processing */
    void (* p_callback)(ota_callback_args_t * p_args); ///< Callback provided when a OTA interrupt ISR occurs.

    /* Pointer to OTA peripheral specific configuration */
    void const * p_extend;                      ///< OTA hardware dependent configuration
    void const * p_context;                     ///< Placeholder for user data.  Passed to user callback in @ref ota_callback_args_t.
    uint8_t      ipl;                           ///< OTA ready interrupt priority
    IRQn_Type    irq;                           ///< OTA ready interrupt number
    uint8_t      err_ipl;                       ///< OTA error interrupt priority
    IRQn_Type    err_irq;                       ///< OTA error interrupt number
} ota_cfg_t;

/** Shared Interface definition for OTA */
typedef struct st_ota_api
{
    /** Open OTA device.
     *
     * @param[in]     p_ctrl            Pointer to OTA device control. Must be declared by user. Value set here.
     * @param[in]     ota_cfg_t         Pointer to OTA configuration structure. All elements of this structure
     *                                  must be set by the user.
     */
    fsp_err_t (* open)(ota_ctrl_t * const p_ctrl, ota_cfg_t const * const p_cfg);

    /** Swap is the process for swapping programs after 2nd download program is valid.
     *  When the program data is successfully applied, the new program data will be executed after reboot.
     *  The swap process will trigger the reset if version check is pass.
     *
     * @param[in]     p_ctrl            Pointer to OTA device control.
     */
    fsp_err_t (* swap)(ota_ctrl_t * const p_ctrl);

    /** Get program image in a specific location
     *
     * @param[in]     p_ctrl            Pointer to OTA device control.
     * @param[in]     update_type       Specifies the update type id for info request.
     * @param[in]     sector_addr       Sector address for version header info.
     * @param[out]    info_image        Parsed data of the image information.
     */
    fsp_err_t (* getImageInfo)(ota_ctrl_t * const p_ctrl,
                               rm_ota_w_update_type_t update_type,
                               uint32_t sector_addr,
                               rm_ota_w_image_header_data_t * info_image);

    /** Set the image index.
     *
     * @param[in]     p_ctrl            Pointer to OTA device control.
     * @param[in]     boot_idx          Boot index to be set.
     */
    fsp_err_t (* bootIdxSet)(ota_ctrl_t * const p_ctrl, uint8_t boot_idx);

    /** Get Current image index which is set
     *
     * @param[in]     p_ctrl            Pointer to OTA device control.
     * @param[in]     boot_idx          Boot index to get.
     */
    fsp_err_t (* bootIdxGet)(ota_ctrl_t * const p_ctrl, uint8_t * boot_idx);

    /** Load key data value from device
     *
     * @param[in]     p_ctrl            Pointer to OTA device control.
     * @param[in]     state             Specifies which program data, current or other.
     * @param[in]     update_type       Specifies the update type id for info request.
     * @param[out]    addr              Pointer for the version address
     */
    fsp_err_t (* getAddr)(ota_ctrl_t * const p_ctrl, uint8_t state, rm_ota_w_update_type_t update_type, uint32_t * addr);

    /** Set Address value for user device
     *
     * @param[in]     p_ctrl            Pointer to OTA device control.
     * @param[in]     update_type       Specifies which program data, current or other.
     * @param[in]     addr              New address value
     */
    fsp_err_t (* setAddr)(ota_ctrl_t * const p_ctrl, uint8_t key_type, uint32_t key_val);


    /** Data validation of image data
     *
     * @param[in]     p_ctrl            Pointer to OTA device control.
     * @param[in]     cert_type         Type of the validation data.
     * @param[in]     sector_addr       Address for sector data.
     */
    fsp_err_t (* cert)(ota_ctrl_t * const p_ctrl, uint8_t cert_type, uint32_t sector_addr);

    /** Close FLASH device.
     *
     * @param[in]     p_ctrl            Pointer to FLASH device control.
     */
    fsp_err_t (* close)(ota_ctrl_t * const p_ctrl);
} ota_api_t;

/** This structure encompasses everything that is needed to use an instance of this interface. */
typedef struct st_ota_instance
{
    ota_ctrl_t      * p_ctrl;        ///< Pointer to the control structure for this instance
    ota_cfg_t const * p_cfg;         ///< Pointer to the configuration structure for this instance
    ota_api_t const * p_api;         ///< Pointer to the API structure for this instance
} ota_instance_t;

/******************************************************************************************************************//**
 * @} (end defgroup OTA_API)
 *********************************************************************************************************************/

/* Common macro for FSP header files. There is also a corresponding FSP_HEADER macro at the top of this file. */
FSP_FOOTER

#endif
