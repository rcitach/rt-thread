/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef RM_MAP_PERSISTANT_W_API_H
#define RM_MAP_PERSISTANT_W_API_H

/******************************************************************************************************************//**
 * @ingroup RENESAS_STORAGE_INTERFACES
 * @defgroup RM_MAP_PERSISTANT_W_API RM_MAP_PERSISTANT_W Interface
 * @brief Interface for accessing MAP_PERSISTANT_W Storage.
 *
 * @section RM_MAP_PERSISTANT_W_API_SUMMARY Summary
 * This section defines the API for the MAP_PERSISTANT_W (Storage) Module.
 * The MAP_PERSISTANT_W Module provides interface to control, access, write to persistant storage .
 *
 *
 *
 * @{
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Includes
 *********************************************************************************************************************/

/* Register definitions, common services and error codes. */
#include "bsp_api.h"

/* Common macro for FSP header files. There is also a corresponding FSP_FOOTER macro at the end of this file. */
FSP_HEADER

/**********************************************************************************************************************
 * Macro definitions
 *********************************************************************************************************************/
#define ENV_GROUP_BOOTCFG       "bootcfg"
#define ENV_GROUP_DEVCFG        "devcfg"
#define ENV_GROUP_WIFICFG       "wificfg"
#define ENV_GROUP_SYSCFG        "syscfg"
#define ENV_GROUP_APPCFG        "appcfg"
#define ENV_GROUP_TESTCFG       "testcfg"
#define ENV_GROUP_WIFIPROFILE   "wifiprofile"
/**********************************************************************************************************************
 * Typedef definitions
 *********************************************************************************************************************/

/** MAP control block.  Allocate an instance specific control block to pass into the MAP API calls.
 */
typedef void map_persistant_w_ctrl_t;

typedef struct map_persistant_w_cfg
{
    /** Placeholder for extension. */
    void const * p_extend;
} map_persistant_w_cfg_t;

/** MAP_PERSISTANT_W driver structure. General MAP_PERSISTANT_W functions implemented at the HAL layer will follow this API. */
typedef struct map_persistant_w_api
{
    /** Open the MAP module.
     * Add constraint for RAM
     *
     * @param[in] p_ctrl               Pointer to MAP handle.
     **/
    fsp_err_t (* open)(map_persistant_w_ctrl_t * const p_ctrl);

    /** Close the MAP module.
     *
     * @param[in] p_ctrl               Pointer to MAP device handle
     * @retval FSP_SUCCESS             Configuration was successful.
     **/
    fsp_err_t (* close)(map_persistant_w_ctrl_t * const p_ctrl);

    /** Close the MAP module.
     *
     * @param[in] p_ctrl               Pointer to MAP device handle
     * @param[in] group                Group of the variable 
     * @param[in] name                 Name of the variable
     * @param[in,out] val              Updates the return value 
     * @retval FSP_SUCCESS             Configuration was successful.
     **/

    fsp_err_t (* read_uint)(map_persistant_w_ctrl_t * const p_ctrl,
                            const char* group, const char *name, void *val,
                            uint16_t  *data_length);

    /** Close the MAP module.
     *
     * @param[in] p_ctrl               Pointer to MAP device handle
     * @param[in] group                Group of the variable
     * @param[in] name                 Name of the variable
     * @param[in] val                  Value to be updated
     * @retval FSP_SUCCESS             Configuration was successful.
     **/
    fsp_err_t (* write_uint)(map_persistant_w_ctrl_t * const p_ctrl, 
                             const char* group, const char *name, void *val,
			     uint32_t len);

    /** Close the MAP module.
     *
     * @param[in] p_ctrl               Pointer to MAP device handle
     * @param[in] group                Group of the variable
     * @param[in] name                 Name of the variable
     * @param[in,out] val              Updates the return value
     * @retval FSP_SUCCESS             Configuration was successful.
     **/
    fsp_err_t (* read_int)(map_persistant_w_ctrl_t * const p_ctrl,
                           const char* group, const char *name, int *val);

    /** Close the MAP module.
     *
     * @param[in] p_ctrl               Pointer to MAP device handle
     * @param[in] group                Group of the variable
     * @param[in] name                 Name of the variable
     * @param[in] val                  Value to be updated
     * @retval FSP_SUCCESS             Configuration was successful.
     **/
    fsp_err_t (* write_int)(map_persistant_w_ctrl_t * const p_ctrl,
                            const char* group, const char *name, int val);

    /** Close the MAP module.
     *
     * @param[in] p_ctrl               Pointer to MAP device handle
     * @param[in] group                Group of the variable
     * @param[in] name                 Name of the variable
     * @retval FSP_SUCCESS             Configuration was successful.
     **/
    fsp_err_t (* read_string)(map_persistant_w_ctrl_t * const p_ctrl,
                              const char* group, const char *name,
			      char **val);

    /** Close the MAP module.
     *
     * @param[in] p_ctrl               Pointer to MAP device handle
     * @param[in] group                Group of the variable
     * @param[in] name                 Name of the variable
     * @param[in] val                  Value to be updated
     * @retval FSP_SUCCESS             Configuration was successful.
     **/
    fsp_err_t (* write_string)(map_persistant_w_ctrl_t * const p_ctrl,
                               const char* group, const char *name, const char *val);

    /** Close the MAP module.
     *
     * @param[in] p_ctrl               Pointer to MAP device handle
     * @param[in] group                Group of the variable
     * @param[in] name                 Name of the variable
     * @retval FSP_SUCCESS             Configuration was successful.
     **/
    fsp_err_t (* erase)(map_persistant_w_ctrl_t * const p_ctrl,
                        const char* group, const char *name);

    /** Close the MAP module.
     *
     * @param[in] p_ctrl               Pointer to MAP device handle
     * @param[in] group                Group of the variable
     * @retval FSP_SUCCESS             Configuration was successful.
     **/
    fsp_err_t (* erase_group)(map_persistant_w_ctrl_t * const p_ctrl,
                              const char* group);

} map_persistant_w_api_t;

/** This structure encompasses everything that is needed to use an instance of this interface. */
typedef struct st_map_persistant_w_instance
{
    map_persistant_w_ctrl_t            * p_ctrl;    ///< Pointer to the control structure for this instance
    map_persistant_w_cfg_t const * const p_cfg;     ///< Pointer to the configuration structure for this instance
    map_persistant_w_api_t const * const p_api;     ///< Pointer to the API structure for this instance
} map_persistant_w_instance_t;

/** Common macro for FSP header files. There is also a corresponding FSP_HEADER macro at the top of this file. */
FSP_FOOTER

#endif

/******************************************************************************************************************//**
 * @} (end defgroup RM_MAP_PERSISTANT_W_API)
 *********************************************************************************************************************/
