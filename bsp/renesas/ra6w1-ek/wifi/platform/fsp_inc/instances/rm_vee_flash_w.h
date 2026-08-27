/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef RM_VEE_FLASH_W_H
#define RM_VEE_FLASH_W_H

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "rm_vee_api.h"
#include "r_flash_api.h"
#include "rm_block_media_api.h"
#include "rm_vee_flash_w_cfg.h"

/* Common macro for FSP header files. There is also a corresponding FSP_FOOTER macro at the end of this file. */
FSP_HEADER

/*******************************************************************************************************************//**
 * @addtogroup RM_VEE_FLASH_W
 * @{
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

/* On MCUs with single byte data flash writes this can be lowered to 2. However, data
 *  returned via RM_VEE_FLASH_W_RecordPtrGet will not be guaranteed to be 32 bit aligned. */
#ifndef RM_VEE_FLASH_W_DF_WRITE_SIZE
 #define RM_VEE_FLASH_W_DF_WRITE_SIZE    (4)
#endif
/* If you are not using the block media as rm_vee_flash_w_cfg_t, 
   set RM_VEE_FLASH_W_DF_MEDIA_SIZE to 0 to reduce a memory usage.  
   If flash sector size is not 4096 bytes, 
   set RM_VEE_FLASH_W_DF_MEDIA_SIZE to the desired size. */
#ifndef RM_VEE_FLASH_W_DF_MEDIA_SIZE
 #define RM_VEE_FLASH_W_DF_MEDIA_SIZE    (4096)
#endif

/* Number of pending events list for managing events propagated from the SW callback in rm_block_media_api. 
 * The VEE callback has too much nested call depth. So, in the case of SW callback, the stack usage was quite high.
 * Therefore, this approach of using the pending events may help keep the stack usage lower.
 */
#ifndef RM_VEE_PEND_EVENT_LIST_SIZE
 #define RM_VEE_PEND_EVENT_LIST_SIZE    (4)
#endif

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/** User configuration structure, used in open function */
typedef struct st_rm_vee_flash_w_cfg
{
    flash_instance_t const * p_flash;  ///< Pointer to a flash instance
    rm_block_media_instance_t const * p_media;  ///< Pointer to a block media instance
} rm_vee_flash_w_cfg_t;

/* Segment Header */
typedef struct
{
    uint32_t refresh_cnt;
    uint16_t pad;
    uint16_t valid_code;
} rm_vee_seg_hdr_t;

/* Record Header (only written to flash when variable length records are configured) */
typedef struct
{
    uint16_t length;                   // length of data portion of this record
    uint16_t offset;                   // temp value "passed to"/used at interrupt level
} rm_vee_rec_hdr_t;

/* Record Trailer */
typedef struct
{
    uint16_t id;
    uint16_t valid_code;
} rm_vee_rec_end_t;

/* Reference Data Update Area Header */
typedef struct
{
    uint16_t pad;
    uint16_t valid_code;               // alternate area contains updated refdata
} rm_vee_ref_hdr_t;

/* Reference Data for delegating tasks */
typedef struct
{
    uint32_t length_update;             // for RM_VEE_FLASH_W_PRV_STATES_WRITE_REC_REFRESH 
                                        //     and RM_VEE_FLASH_W_PRV_STATES_WRITE_REFDATA
    bool     wr_in_progress;
    bool     wr_in_continue;
    bool     refseg_in_progress;
    bool     media_error;
    struct {
        bool    pended;
        uint8_t wrptr;
        uint8_t rdptr;
        flash_callback_args_t event[RM_VEE_PEND_EVENT_LIST_SIZE];
    } pendevent;
} rm_vee_delegate_hdr_t;


/** Instance control block.  This is private to the FSP and should not be used or modified by the application. */
typedef struct st_rm_vee_flash_w_instance_ctrl
{
    uint32_t                 open;
    volatile uint32_t        mode;
    volatile uint32_t        state;
    uint32_t                 active_seg_addr;
    uint32_t                 next_write_addr;
    uint32_t                 ref_hdr_addr;
    rm_vee_cfg_t const     * p_cfg;
    bool                     new_refdata_valid; // update area written successfully
    bool                     factory_refdata;   // update area written successfully
    volatile bool            irq_flag;
    uint8_t                  xfer_buf[RM_VEE_FLASH_W_REFRESH_BUFFER_SIZE];
    rm_vee_seg_hdr_t         seg_hdr;
    rm_vee_rec_hdr_t         rec_hdr;
    uint8_t const          * p_rec_data;
    rm_vee_rec_end_t         rec_end;
    rm_vee_ref_hdr_t         ref_hdr;
    volatile flash_event_t   flash_event;
    flash_event_t            flash_err_event;          // error event from Flash driver
    uint32_t                 last_id;                  // ID of last record successfully written
    uint32_t                 refresh_type;
    uint32_t                 refresh_src_seg_addr;
    uint32_t                 refresh_src_rec_end_addr; // addr of first byte after last record
    uint32_t                 refresh_src_refdata_addr;
    uint32_t                 refresh_start_rec_id;     // ID of first record copied during Refresh
    uint32_t                 refresh_cur_rec_id;       // g_rec_offset[] index used during Refresh
    uint32_t                 refresh_dst_rec_end_addr; // addr of first byte after last record
    uint32_t                 refresh_xfer_src_addr;
    uint32_t                 refresh_xfer_bytes_left;
    flash_instance_t const * p_flash;
    rm_block_media_instance_t const * p_media;
    uint32_t                 segment_size;
    uint8_t                  data_buffer[RM_VEE_FLASH_W_DF_WRITE_SIZE];
    uint8_t                  media_buffer[RM_VEE_FLASH_W_DF_MEDIA_SIZE];
    rm_vee_delegate_hdr_t    delegation;

    void (* p_callback)(rm_vee_callback_args_t *); // Pointer to callback
    rm_vee_callback_args_t * p_callback_memory;    // Pointer to optional callback argument memory
    void * p_context;            // Pointer to context to be passed into callback function
} rm_vee_flash_w_instance_ctrl_t;

enum {
    AS_INT,       // Int written as int
    AS_STRING,    // Int or string written as string
};

/**********************************************************************************************************************
 * Exported global variables
 **********************************************************************************************************************/

/** @cond INC_HEADER_DEFS_SEC */
/** Filled in Interface API structure for this Instance. */
extern const rm_vee_api_t g_rm_vee_on_flash;

/** @endcond */

/***********************************************************************************************************************
 * Public APIs
 **********************************************************************************************************************/
fsp_err_t RM_VEE_FLASH_W_Open(rm_vee_ctrl_t * const p_api_ctrl, rm_vee_cfg_t const * const p_cfg);
fsp_err_t RM_VEE_FLASH_W_RecordWrite(rm_vee_ctrl_t * const p_api_ctrl,
                                   uint32_t const        rec_id,
                                   uint8_t const * const p_rec_data,
                                   uint32_t const        num_bytes);
fsp_err_t RM_VEE_FLASH_W_RecordPtrGet(rm_vee_ctrl_t * const p_api_ctrl,
                                    uint32_t const        rec_id,
                                    uint8_t ** const      pp_rec_data,
                                    uint32_t * const      p_num_bytes);
fsp_err_t RM_VEE_FLASH_W_RefDataWrite(rm_vee_ctrl_t * const p_api_ctrl, uint8_t const * const p_ref_data);
fsp_err_t RM_VEE_FLASH_W_RefDataPtrGet(rm_vee_ctrl_t * const p_api_ctrl, uint8_t ** const pp_ref_data);
fsp_err_t RM_VEE_FLASH_W_StatusGet(rm_vee_ctrl_t * const p_api_ctrl, rm_vee_status_t * const p_status);
fsp_err_t RM_VEE_FLASH_W_Refresh(rm_vee_ctrl_t * const p_api_ctrl);
fsp_err_t RM_VEE_FLASH_W_Format(rm_vee_ctrl_t * const p_api_ctrl, uint8_t const * const p_ref_data);
fsp_err_t RM_VEE_FLASH_W_CallbackSet(rm_vee_ctrl_t * const          p_api_ctrl,
                                   void (                       * p_callback)(rm_vee_callback_args_t *),
                                   void * const             p_context,
                                   rm_vee_callback_args_t * const p_callback_memory);
fsp_err_t RM_VEE_FLASH_W_Close(rm_vee_ctrl_t * const p_api_ctrl);

/* Common macro for FSP header files. There is also a corresponding FSP_HEADER macro at the top of this file. */
FSP_FOOTER

#endif                                 // RM_VEE_FLASH_W_H

/*******************************************************************************************************************//**
 * @} (end defgroup RM_VEE_FLASH_W)
 **********************************************************************************************************************/
