/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "bsp_api.h"
#include "rm_map_persistant_w.h"
#include "rm_vee_flash_w_rrq_nvram.h"
#include "rm_vee_flash_w.h"

#include "rm_vee_api.h"
#if CFG_PMGR
#include "rm_pmgr_w_instance.h"
#endif

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#ifndef MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))
#endif

#define AREA_ID_UNKNOWN     (0xFFFFFFFFU)

#define ENVRION_PRINT(...)              printf(__VA_ARGS__)
#define ENVRION_PRINT_LVL1(lvl, ...)    if((lvl == 1)){ printf(__VA_ARGS__); }
#define ENVRION_ERROR(...)              printf(__VA_ARGS__)
#define ENVRION_DEBUG(...)              //printf(__VA_ARGS__)

#define DELIMETER_MAXLEN        40
#define DATA_ALIGNMENT(x)       (((x) + 1) & (~0x00001U))
#define GTAG_ALIGNMENT(x)       (x)

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
static bool rm_map_persistant_w = false;

typedef enum {
    BOOTCFG_ID      = 1,     /* 1~ 100 */
    DEVCFG_ID       = 101,   /* 101 ~ 200 */
    WIFICFG_ID      = 201,   /* 201 ~ 1000 */
    SYSCFG_ID       = 1001,  /* 1001 ~ 1300 */
    APPCFG_ID       = 1301,  /* 1301 ~ 1800 */
    BLECFG_ID       = 1801,  /* 1801 ~ 1880 */
    BLESEC_ID       = 1881,  /* 1881 ~ 1900 */
    SECUREASSET_ID  = 1901,  /* 1900 ~ 1950 */
    TESTCFG_ID      = 1951,  /* 1951 ~ 2000 */
    WIFIPROFILE_ID  = 2001,  /* 2001 ~ 2047 */
    MAX_ID          = 2048,  /* MAX */
} nvram_vee_group_id_t;


enum {
    FLAG_VARIABLE_LEN = 0x01,       // parameter has variable length
};

typedef struct {
    const char *name;               // unique parameter tagname, user has to ensure the unique name
    struct {
      uint16_t  flags : 1;          // parameter flags
      uint16_t  length: 15;         // parameter max length
    } attr;
} parameter_t;


typedef struct {
    const char   *name;             // unique area name, user has to ensure the unique name
    parameter_t  *parameters;       // list of area parameters
    size_t       num_parameters;    // number of area parameters
} area_t;

typedef struct {
    area_t *area;
    uint32_t id;
    const rm_vee_instance_t *g_vee;
}   rm_vee_group_instance_t ;

typedef union {
    uint8_t  val8;
    uint16_t val16;
    uint32_t val32;
} bin_value_t;
/* Create nvparam configuration from ad_nvparam_defs.h */
#define IN_AD_NVPARAM_C
#include "wifi_ad_nvparam_defs.h"

extern const parameter_t *find_parameter_by_name(const area_t *area, char *name, uint16_t *rindex);
extern uint32_t find_group_id(const area_t *area);
extern void find_group_list(void);

extern rm_vee_group_instance_t *vee_nvparam_open(const char *area_name);
extern void vee_nvparam_close(rm_vee_group_instance_t *p_vee);

extern uint32_t vee_nvparam_read_offset_by_name(rm_vee_group_instance_t *p_vee,
                            const char *name, uint16_t offset, void **datam, uint8_t *flag);
extern uint16_t vee_nvparam_write_offset_by_name(rm_vee_group_instance_t *p_vee,
                            const char *name, uint16_t is_string,
                            uint16_t offset, const void *data);
extern uint16_t vee_nvparam_erase_by_name(rm_vee_group_instance_t *p_vee, const char *name);
extern void vee_nvparam_erase_all(rm_vee_group_instance_t *p_vee);
extern void vee_nvparam_format(rm_vee_group_instance_t *p_vee);
extern void vee_nvparam_refresh(rm_vee_group_instance_t *p_vee);
//
// DPM for RRQ61X
//
extern int RM_PMGR_W_dpm_sleep_is_started(void);

extern uint16_t    rm_vee_flash_w_multi_use;
#define NVPARAM_AREA_SIZE(NAME) (sizeof(area_ ## NAME) / sizeof(parameter_t))
#define NVPARAM_AREA_NAME(NAME) (area_ ## NAME)

/**
 * ex)
 * print_separate_bar("=", 10, 2);
 *
 * "==========\n\n"
 *
 */
static void print_separate_bar(unsigned char text, unsigned char loop_count, unsigned char CR_loop_count)
{
    unsigned char prt_str[260];

    memset(prt_str, 0, 256);

    if ((loop_count + CR_loop_count) + 1 > 260) {
        loop_count = (unsigned char)(260 - (CR_loop_count - 1));
    }

    memset(prt_str, text, loop_count);

    if (CR_loop_count > 0) {
        memset(prt_str + loop_count, '\n', CR_loop_count);
    }
    printf("%s", prt_str);
}

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/
map_persistant_w_instance_ctrl_t * gp_map_persistant_w_ctrl = NULL;

/***********************************************************************************************************************
 * Global Variables
 **********************************************************************************************************************/
const map_persistant_w_api_t g_map_persistant_w_api =
{
    .open          = RM_MAP_PERSISTANT_W_Open,
    .close         = RM_MAP_PERSISTANT_W_Close,
    .read_uint     = RM_MAP_PERSISTANT_W_Read_UINT,
    .write_uint    = RM_MAP_PERSISTANT_W_Write_UINT,
    .read_int      = RM_MAP_PERSISTANT_W_Read_INT,
    .write_int     = RM_MAP_PERSISTANT_W_Write_INT,
    .read_string   = RM_MAP_PERSISTANT_W_Read_STRING,
    .write_string  = RM_MAP_PERSISTANT_W_Write_STRING,
    .erase         = RM_MAP_PERSISTANT_W_Erase,
    .erase_group   = RM_MAP_PERSISTANT_W_Erase_GROUP
};
/*******************************************************************************************************************//**
 * @addtogroup RM_MAP_PERSISTANT_W
 * @{
 **********************************************************************************************************************/

map_persistant_w_instance_ctrl_t * RM_MAP_PERSISTANT_W_get_ctrl(void)
{
	return gp_map_persistant_w_ctrl;
}
/*******************************************************************************************************************//**
 * Perform any necessary initialization for RM_MAP_PERSISTANT_W
 *
 * @retval     FSP_SUCCESS                   RM_MAP_PERSISTANT_W instance opened
 * @retval     FSP_ERR_ALREADY_OPEN          RM_MAP_PERSISTANT_W instance is already open
 **********************************************************************************************************************/
fsp_err_t RM_MAP_PERSISTANT_W_Open (map_persistant_w_ctrl_t * const p_api_ctrl)
{

    if (rm_map_persistant_w == false) { 	

        map_persistant_w_instance_ctrl_t * p_ctrl = (map_persistant_w_instance_ctrl_t *) p_api_ctrl;
        fsp_err_t err;   

        FSP_ASSERT(p_ctrl != NULL);
        FSP_ERROR_RETURN(MAP_PERSISTANT_W_OPEN != p_ctrl->map_persistant_w_open, FSP_ERR_ALREADY_OPEN);

        memset(p_ctrl, 0U, sizeof(map_persistant_w_instance_ctrl_t));

        /* Create mutexes */
        p_ctrl->map_persistant_w_mutex = xSemaphoreCreateRecursiveMutex();
        FSP_ERROR_RETURN(NULL != p_ctrl->map_persistant_w_mutex, FSP_ERR_INVALID_STATE);

        xSemaphoreTakeRecursive(p_ctrl->map_persistant_w_mutex, portMAX_DELAY);

        rm_vee_group_instance_t *rm_vee_param;

        rm_vee_param = vee_nvparam_open(NULL);

        if (rm_vee_param == NULL) {
                ENVRION_ERROR("%s: vee_nvparam is null\n", __func__);
                xSemaphoreGiveRecursive(p_ctrl->map_persistant_w_mutex);
                return FSP_ERR_NOT_INITIALIZED;
        }
        xSemaphoreGiveRecursive(p_ctrl->map_persistant_w_mutex);

	rm_map_persistant_w = true;
        p_ctrl->map_persistant_w_open = MAP_PERSISTANT_W_OPEN;
        gp_map_persistant_w_ctrl = p_api_ctrl;

        if (!RM_MAP_PERSISTANT_W_get_ctrl())
        {
            err = FSP_ERR_ASSERTION;
            return err;
        }
    }

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Close the MAP_PERSISTANT_W Instance
 *
 * @retval     FSP_SUCCESS        MAP_PERSISTANT_W driver closed
 * @retval     FSP_ERR_ASSERTION  Null Pointer
 * @retval     FSP_ERR_NOT_OPEN   MAP_PERSISTANT_W instance is not open yet
 **********************************************************************************************************************/
fsp_err_t RM_MAP_PERSISTANT_W_Close (map_persistant_w_ctrl_t * const p_api_ctrl)
{
    map_persistant_w_instance_ctrl_t * p_ctrl = (map_persistant_w_instance_ctrl_t *) p_api_ctrl;

    FSP_ASSERT(p_ctrl != NULL);
    FSP_ERROR_RETURN(MAP_PERSISTANT_W_OPEN == p_ctrl->map_persistant_w_open, FSP_ERR_NOT_OPEN);

    if(p_ctrl->map_persistant_w_mutex)
    {
        vSemaphoreDelete(p_ctrl->map_persistant_w_mutex);
        p_ctrl->map_persistant_w_mutex = NULL;
    }

    p_ctrl->map_persistant_w_open = 0;
    p_ctrl = NULL;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Read unsigned int from the MAP PERSISTANT_W storage
 *
 * @retval     FSP_SUCCESS        MAP_PERSISTANT_W driver closed
 * @retval     FSP_ERR_ASSERTION  Null Pointer
 * @retval     FSP_ERR_NOT_OPEN   MAP_PERSISTANT_W instance is not open yet
 **********************************************************************************************************************/
fsp_err_t RM_MAP_PERSISTANT_W_Read_UINT(map_persistant_w_ctrl_t * const p_api_ctrl, 
                                      const char* group, const char *name, void *value,
				      uint16_t  *data_length)
{
    map_persistant_w_instance_ctrl_t * p_ctrl = (map_persistant_w_instance_ctrl_t *) p_api_ctrl;

    FSP_ASSERT(p_ctrl != NULL);
    FSP_ERROR_RETURN(MAP_PERSISTANT_W_OPEN == p_ctrl->map_persistant_w_open, FSP_ERR_NOT_OPEN);

    rm_vee_group_instance_t *rm_vee_param;
    uint8_t *vee_ro_data = NULL;
    uint8_t flag = 0;
    bin_value_t *bin_value;

    if (group == NULL || name == NULL || value == NULL) {
        data_length = 0;
        return FSP_ERR_INVALID_ARGUMENT;
    }

    xSemaphoreTakeRecursive(p_ctrl->map_persistant_w_mutex, portMAX_DELAY);

    rm_vee_param  =  vee_nvparam_open(group);

    if ( rm_vee_param == NULL ) {
        ENVRION_DEBUG("[%s] (%s->%s) vee_nvparam_open is null\n", __func__, group, name);
        xSemaphoreGiveRecursive(p_ctrl->map_persistant_w_mutex);
        data_length = 0;
	return FSP_ERR_NOT_FOUND;
    }

    *data_length = vee_nvparam_read_offset_by_name(rm_vee_param, name, 0, (void **) (&vee_ro_data), &flag);

    vee_nvparam_close(rm_vee_param);

    if (*data_length == 0) {
        ENVRION_DEBUG("[%s] (%s->%s) vee_nvparam : Empty(data_length=0)\n", __func__, group, name);
        xSemaphoreGiveRecursive(p_ctrl->map_persistant_w_mutex);
        data_length = 0;
	return FSP_ERR_BUFFER_EMPTY;
    }

    if (0 != (flag & FLAG_VARIABLE_LEN)) {  /* 0 != (0&1) */
        ENVRION_DEBUG("[%s] (%s->%s) nvparam does not match(string type).\n", __func__, group, name);
        xSemaphoreGiveRecursive(p_ctrl->map_persistant_w_mutex);
        data_length = 0;
	return FSP_ERR_NOT_FOUND;
    }

    bin_value = (bin_value_t *)vee_ro_data;

    switch (*data_length) {
        case sizeof(uint8_t):
        {
                *((uint8_t *)value) = bin_value->val8;
                ENVRION_DEBUG("[%s] (%s->%s)  u8 %d\n", __func__, group, name, *(uint8_t *) value);
                break;
        }

        case sizeof(uint16_t):
        {
                *((uint16_t *)value) = bin_value->val16;
                ENVRION_DEBUG("[%s] (%s->%s)  u16 %d\n", __func__, group, name, *(uint16_t *) value);
                break;
        }

        default:
        {
                *((uint32_t *)value) = bin_value->val32;
                ENVRION_DEBUG("[%s] (%s->%s)  u32 %ld\n", __func__, group, name, *(uint32_t *) value);
                break;
        }
    }

    xSemaphoreGiveRecursive(p_ctrl->map_persistant_w_mutex);

    return FSP_SUCCESS;

}

/*******************************************************************************************************************//**
 * Write unsigned int to MAP PERSISTANT_W Storage
 *
 * @retval     FSP_SUCCESS        MAP_PERSISTANT_W driver closed
 * @retval     FSP_ERR_ASSERTION  Null Pointer
 * @retval     FSP_ERR_NOT_OPEN   MAP_PERSISTANT_W instance is not open yet
 **********************************************************************************************************************/
fsp_err_t RM_MAP_PERSISTANT_W_Write_UINT(map_persistant_w_ctrl_t * const p_api_ctrl,
                                       const char* group, const char *name, void *value,
				       uint32_t len)

{
    map_persistant_w_instance_ctrl_t * p_ctrl = (map_persistant_w_instance_ctrl_t *) p_api_ctrl;

    RA6W1_UNUSED_ARG(len);

    FSP_ASSERT(p_ctrl != NULL);
    FSP_ERROR_RETURN(MAP_PERSISTANT_W_OPEN == p_ctrl->map_persistant_w_open, FSP_ERR_NOT_OPEN);

#if CFG_PMGR
    /* At this case,,, dpm_sleep operation was started already */
    if (RM_PMGR_W_dpm_sleep_is_started()) {
        printf("[%s] Already DPM Sleep started !!!\n", __func__);
        return FSP_ERR_WRITE_FAILED;
    }
#endif /* CFG_PMGR */

    rm_vee_group_instance_t *rm_vee_param;
    uint16_t  data_length;

    if (group == NULL) {
        ENVRION_ERROR("[%s] (group=NULL->%s)\n", __func__, name);
        return FSP_ERR_INVALID_ARGUMENT;
    }

    if (name == NULL) {
        ENVRION_ERROR("[%s] (%s->name=NULL)\n", __func__, group);
        return FSP_ERR_INVALID_ARGUMENT;
    }

    if (value == NULL) {
        ENVRION_DEBUG("[%s] (%s->%s) data=null\n", __func__, group, name);
        return FSP_ERR_INVALID_ARGUMENT;
    }

    xSemaphoreTakeRecursive(p_ctrl->map_persistant_w_mutex, portMAX_DELAY);

    rm_vee_param = vee_nvparam_open(group);

    if (rm_vee_param == NULL) {
        ENVRION_ERROR("[%s] (%s->%s) vee_nvparam is null\n", __func__, group, name);
        xSemaphoreGiveRecursive(p_ctrl->map_persistant_w_mutex);
        return FSP_ERR_NOT_FOUND;
    }

    data_length = vee_nvparam_write_offset_by_name(rm_vee_param, name, AS_INT, 0, value);

    vee_nvparam_close(rm_vee_param);

    if (data_length == 0) {
        ENVRION_ERROR("[%s] (%s->%s) vee_nvparam error\n", __func__, group, name);
        xSemaphoreGiveRecursive(p_ctrl->map_persistant_w_mutex);
        return FSP_ERR_WRITE_FAILED;
    }

    xSemaphoreGiveRecursive(p_ctrl->map_persistant_w_mutex);

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Read the Integer from the MAP PERSISTANT_W Storage
 *
 * @retval     FSP_SUCCESS        MAP_PERSISTANT_W driver closed
 * @retval     FSP_ERR_ASSERTION  Null Pointer
 * @retval     FSP_ERR_NOT_OPEN   MAP_PERSISTANT_W instance is not open yet
 **********************************************************************************************************************/
fsp_err_t RM_MAP_PERSISTANT_W_Read_INT(map_persistant_w_ctrl_t * const p_api_ctrl,
                                     const char* group, const char *name, int *val)
{
    map_persistant_w_instance_ctrl_t * p_ctrl = (map_persistant_w_instance_ctrl_t *) p_api_ctrl;

    FSP_ASSERT(p_ctrl != NULL);
    FSP_ERROR_RETURN(MAP_PERSISTANT_W_OPEN == p_ctrl->map_persistant_w_open, FSP_ERR_NOT_OPEN);

    rm_vee_group_instance_t *rm_vee_param;
    uint16_t  data_length;
    int * vee_ro_data = NULL;

    if (group == NULL || name == NULL) {
        *val = -1;
        return FSP_ERR_INVALID_ARGUMENT;
    }

    xSemaphoreTakeRecursive(p_ctrl->map_persistant_w_mutex, portMAX_DELAY);

    rm_vee_param  =  vee_nvparam_open(group);

    if (rm_vee_param == NULL) {
        ENVRION_DEBUG("[%s] (%s->%s) vee_nvparam_open is null\n", __func__, group, name);
        xSemaphoreGiveRecursive(p_ctrl->map_persistant_w_mutex);
	vee_ro_data = NULL;
	*val = -1;
        return FSP_ERR_NOT_FOUND;
    }

    uint8_t flag = 0;
    data_length = vee_nvparam_read_offset_by_name(rm_vee_param, name, 0, (void **) (&vee_ro_data), &flag);

    vee_nvparam_close(rm_vee_param);

    if (data_length == 0) {
        xSemaphoreGiveRecursive(p_ctrl->map_persistant_w_mutex);
        ENVRION_DEBUG("[%s] (%s->%s) data_length=0\n", __func__, group, name);
	vee_ro_data = NULL;
	*val = -1;
        return FSP_ERR_NOT_FOUND;
    }

    xSemaphoreGiveRecursive(p_ctrl->map_persistant_w_mutex);
    *val = (int) *vee_ro_data;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Write Integer to the MAP PERSISTANT_W Storage
 *
 * @retval     FSP_SUCCESS        MAP_PERSISTANT_W driver closed
 * @retval     FSP_ERR_ASSERTION  Null Pointer
 * @retval     FSP_ERR_NOT_OPEN   MAP_PERSISTANT_W instance is not open yet
 **********************************************************************************************************************/
fsp_err_t RM_MAP_PERSISTANT_W_Write_INT(map_persistant_w_ctrl_t * const p_api_ctrl, 
		                      const char* group, const char *name, int value)
{

    map_persistant_w_instance_ctrl_t * p_ctrl = (map_persistant_w_instance_ctrl_t *) p_api_ctrl;

    FSP_ASSERT(p_ctrl != NULL);
    FSP_ERROR_RETURN(MAP_PERSISTANT_W_OPEN == p_ctrl->map_persistant_w_open, FSP_ERR_NOT_OPEN);

    int ret = 0;
    int curr_val;
    rm_vee_group_instance_t *rm_vee_param;
    uint16_t  data_length;


#if CFG_PMGR
    /* At this case,,, dpm_sleep operation was started already */
    if (RM_PMGR_W_dpm_sleep_is_started()) {
        printf("[%s] Already DPM Sleep started !!!\n", __func__);
        return FSP_ERR_WRITE_FAILED;
    }
#endif

    ret = RM_MAP_PERSISTANT_W_Read_INT(p_ctrl, group, name, &curr_val);

    if (ret == 0 && curr_val == value)
        goto end;

    if (group == NULL) {
        ENVRION_ERROR("[%s] (group=NULL->%s)\n", __func__, name);
        return FSP_ERR_INVALID_ARGUMENT;
    }

    if (name == NULL) {
        ENVRION_ERROR("[%s] (%s->name=NULL)\n", __func__, group);
        return FSP_ERR_INVALID_ARGUMENT;
    }

    xSemaphoreTakeRecursive(p_ctrl->map_persistant_w_mutex, portMAX_DELAY);

    rm_vee_param = vee_nvparam_open(group);

    if (rm_vee_param == NULL) {
        ENVRION_ERROR("[%s] (%s->%s) vee_nvparam is null\n", __func__, group, name);
        xSemaphoreGiveRecursive(p_ctrl->map_persistant_w_mutex);
        return FSP_ERR_NOT_FOUND;
    }

    data_length = vee_nvparam_write_offset_by_name(rm_vee_param, name, AS_INT, 0, &value);

    vee_nvparam_close(rm_vee_param);

    if (data_length == 0) {
        ENVRION_ERROR("[%s] (%s->%s) vee_nvparam error\n", __func__, group, name);
        xSemaphoreGiveRecursive(p_ctrl->map_persistant_w_mutex);
        return FSP_ERR_WRITE_FAILED;
    }

    xSemaphoreGiveRecursive(p_ctrl->map_persistant_w_mutex);

end:
    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Read the string from  the MAP PERSISTANT_W Storage
 *
 * @retval     FSP_SUCCESS        MAP_PERSISTANT_W driver closed
 * @retval     FSP_ERR_ASSERTION  Null Pointer
 * @retval     FSP_ERR_NOT_OPEN   MAP_PERSISTANT_W instance is not open yet
 **********************************************************************************************************************/
fsp_err_t RM_MAP_PERSISTANT_W_Read_STRING(map_persistant_w_ctrl_t * const p_api_ctrl,
                                        const char* group, const char *name, char **val)
{
    map_persistant_w_instance_ctrl_t * p_ctrl = (map_persistant_w_instance_ctrl_t *) p_api_ctrl;

    FSP_ASSERT(p_ctrl != NULL);
    FSP_ERROR_RETURN(MAP_PERSISTANT_W_OPEN == p_ctrl->map_persistant_w_open, FSP_ERR_NOT_OPEN);

    rm_vee_group_instance_t *rm_vee_param;
    uint16_t  data_length;
    uint8_t *vee_ro_data = NULL;

    if (group == NULL || name == NULL) {
        *val = NULL;
        return FSP_ERR_INVALID_ARGUMENT;
    }

    xSemaphoreTakeRecursive(p_ctrl->map_persistant_w_mutex, portMAX_DELAY);

    rm_vee_param  =  vee_nvparam_open(group);

    if (rm_vee_param == NULL) {
        ENVRION_DEBUG("[%s] (%s->%s) vee_nvparam_open is null\n", __func__, group, name);
        xSemaphoreGiveRecursive(p_ctrl->map_persistant_w_mutex);
        *val = NULL;
        return FSP_ERR_NOT_FOUND;
    }

    uint8_t flag = 0;
    data_length = vee_nvparam_read_offset_by_name(rm_vee_param, name, 0, (void **) (&vee_ro_data), &flag);
    ENVRION_DEBUG("[%s] %d (%s->%s)\n", __func__, __LINE__, group, name);
    vee_nvparam_close(rm_vee_param);

    if (data_length == 0) {
        xSemaphoreGiveRecursive(p_ctrl->map_persistant_w_mutex);
        ENVRION_DEBUG("[%s] (%s->%s) data_length=0\n", __func__, group, name);
        *val = NULL;
        return FSP_ERR_NOT_FOUND;
    }

    xSemaphoreGiveRecursive(p_ctrl->map_persistant_w_mutex);
    *val =  (char *) vee_ro_data;

     ENVRION_DEBUG("[%s] %d (%s->%s) val=%s\n", __func__, __LINE__, group, name, *val);
    return FSP_SUCCESS;

}

/*******************************************************************************************************************//**
 * Write String to the MAP PERSISTANT_W Storage
 *
 * @retval     FSP_SUCCESS        MAP_PERSISTANT_W driver closed
 * @retval     FSP_ERR_ASSERTION  Null Pointer
 * @retval     FSP_ERR_NOT_OPEN   MAP_PERSISTANT_W instance is not open yet
 **********************************************************************************************************************/
fsp_err_t RM_MAP_PERSISTANT_W_Write_STRING(map_persistant_w_ctrl_t * const p_api_ctrl,
                                         const char* group, const char *name, const char *value)
{

    map_persistant_w_instance_ctrl_t * p_ctrl = (map_persistant_w_instance_ctrl_t *) p_api_ctrl;

    FSP_ASSERT(p_ctrl != NULL);
    FSP_ERROR_RETURN(MAP_PERSISTANT_W_OPEN == p_ctrl->map_persistant_w_open, FSP_ERR_NOT_OPEN);

#if CFG_PMGR
    /* At this case,,, dpm_sleep operation was started already */
    if (RM_PMGR_W_dpm_sleep_is_started()) {
        printf("[%s] Already DPM Sleep started !!!\n", __func__);
        return FSP_ERR_WRITE_FAILED;
    }
#endif /* CFG_PMGR */

    rm_vee_group_instance_t *rm_vee_param;
    uint16_t  data_length;

    if (group == NULL) {
        ENVRION_ERROR("[%s] (group=NULL->%s)\n", __func__, name);
        return FSP_ERR_INVALID_ARGUMENT;
    }

    if (name == NULL) {
        ENVRION_ERROR("[%s] (%s->name=NULL)\n", __func__, group);
        return FSP_ERR_INVALID_ARGUMENT;
    }


    if (value == NULL) {
        ENVRION_DEBUG("[%s] (%s->%s) data=null\n", __func__, group, name);
        return FSP_ERR_INVALID_ARGUMENT;
    }

    xSemaphoreTakeRecursive(p_ctrl->map_persistant_w_mutex, portMAX_DELAY);

    rm_vee_param = vee_nvparam_open(group);

    if (rm_vee_param == NULL) {
        ENVRION_ERROR("[%s] (%s->%s) vee_nvparam is null\n", __func__, group, name);
        xSemaphoreGiveRecursive(p_ctrl->map_persistant_w_mutex);
        return FSP_ERR_NOT_FOUND;
    }

    data_length = vee_nvparam_write_offset_by_name(rm_vee_param, name, AS_STRING, 0, value);

    vee_nvparam_close(rm_vee_param);

    if (data_length == 0) {
        ENVRION_ERROR("[%s] (%s->%s) vee_nvparam error\n", __func__, group, name);
        xSemaphoreGiveRecursive(p_ctrl->map_persistant_w_mutex);
        return FSP_ERR_WRITE_FAILED;
    }

    xSemaphoreGiveRecursive(p_ctrl->map_persistant_w_mutex);
    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Write String or int to the MAP PERSISTANT_W Storage
 *
 * @retval     FSP_SUCCESS        MAP_PERSISTANT_W driver closed
 * @retval     FSP_ERR_ASSERTION  Null Pointer
 * @retval     FSP_ERR_NOT_OPEN   MAP_PERSISTANT_W instance is not open yet
 **********************************************************************************************************************/
fsp_err_t RM_MAP_PERSISTANT_W_Write_Auto(map_persistant_w_ctrl_t * const p_api_ctrl,
                                         const char * group, const char * name, const char * value)
{

    map_persistant_w_instance_ctrl_t * p_ctrl = (map_persistant_w_instance_ctrl_t *) p_api_ctrl;

    FSP_ASSERT(p_ctrl != NULL);
    FSP_ERROR_RETURN(MAP_PERSISTANT_W_OPEN == p_ctrl->map_persistant_w_open, FSP_ERR_NOT_OPEN);

#if CFG_PMGR
    /* At this case,,, dpm_sleep operation was started already */
    if (RM_PMGR_W_dpm_sleep_is_started()) {
        printf("[%s] Already DPM Sleep started !!!\n", __func__);
        return FSP_ERR_WRITE_FAILED;
    }
#endif /* CFG_PMGR */

    rm_vee_group_instance_t *rm_vee_param;
    uint16_t  data_length;

    if (group == NULL) {
        ENVRION_ERROR("[%s] (group=NULL->%s)\n", __func__, name);
        return FSP_ERR_INVALID_ARGUMENT;
    }

    if (name == NULL) {
        ENVRION_ERROR("[%s] (%s->name=NULL)\n", __func__, group);
        return FSP_ERR_INVALID_ARGUMENT;
    }


    if (value == NULL) {
        ENVRION_DEBUG("[%s] (%s->%s) data=null\n", __func__, group, name);
        return FSP_ERR_INVALID_ARGUMENT;
    }

    xSemaphoreTakeRecursive(p_ctrl->map_persistant_w_mutex, portMAX_DELAY);

    rm_vee_param = vee_nvparam_open(group);

    if (rm_vee_param == NULL) {
        ENVRION_ERROR("[%s] (%s->%s) vee_nvparam is null\n", __func__, group, name);
        xSemaphoreGiveRecursive(p_ctrl->map_persistant_w_mutex);
        return FSP_ERR_NOT_FOUND;
    }

    data_length = vee_nvparam_write_offset_by_name(rm_vee_param, name, AS_STRING, 0, value);

    vee_nvparam_close(rm_vee_param);

    if (data_length == 0) {
        ENVRION_ERROR("[%s] (%s->%s) vee_nvparam error\n", __func__, group, name);
        xSemaphoreGiveRecursive(p_ctrl->map_persistant_w_mutex);
        return FSP_ERR_WRITE_FAILED;
    }

    xSemaphoreGiveRecursive(p_ctrl->map_persistant_w_mutex);
    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Erase the entry from MAP PERSISTANT_W Storage
 *
 * @retval     FSP_SUCCESS        MAP_PERSISTANT_W driver closed
 * @retval     FSP_ERR_ASSERTION  Null Pointer
 * @retval     FSP_ERR_NOT_OPEN   MAP_PERSISTANT_W instance is not open yet
 **********************************************************************************************************************/
fsp_err_t RM_MAP_PERSISTANT_W_Erase(map_persistant_w_ctrl_t * const p_api_ctrl,
                                  const char* group, const char *name)
{
    map_persistant_w_instance_ctrl_t * p_ctrl = (map_persistant_w_instance_ctrl_t *) p_api_ctrl;

    FSP_ASSERT(p_ctrl != NULL);
    FSP_ERROR_RETURN(MAP_PERSISTANT_W_OPEN == p_ctrl->map_persistant_w_open, FSP_ERR_NOT_OPEN);

    rm_vee_group_instance_t *rm_vee_param;
    uint16_t  data_length;
#if CFG_PMGR
    /* At this case,,, dpm_sleep operation was started already */
    if (RM_PMGR_W_dpm_sleep_is_started()) {
        printf("[%s] Already DPM Sleep started !!!\n", __func__);
        return FSP_ERR_ERASE_FAILED;
    }
#endif /* CFG_PMGR */

    if (group == NULL || name == NULL) {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    xSemaphoreTakeRecursive(p_ctrl->map_persistant_w_mutex, portMAX_DELAY);

    rm_vee_param  =  vee_nvparam_open(group);

    if (rm_vee_param == NULL) {
        ENVRION_ERROR("[%s] (%s->%s) vee_nvparam is null\n", __func__, group, name);
        xSemaphoreGiveRecursive(p_ctrl->map_persistant_w_mutex);
        return FSP_ERR_NOT_FOUND;
    }

    data_length = vee_nvparam_erase_by_name(rm_vee_param, name);

    vee_nvparam_close(rm_vee_param);

    if (data_length == 0) {
        ENVRION_DEBUG("[%s] (%s->%s) vee_nvparam Erase Skip\n", __func__, group, name);
        xSemaphoreGiveRecursive(p_ctrl->map_persistant_w_mutex);
        return FSP_SUCCESS;
    }

    xSemaphoreGiveRecursive(p_ctrl->map_persistant_w_mutex);
    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Erase particular group entries from the MAP PERSISTANT_W Storage
 *
 * @retval     FSP_SUCCESS        MAP_PERSISTANT_W driver closed
 * @retval     FSP_ERR_ASSERTION  Null Pointer
 * @retval     FSP_ERR_NOT_OPEN   MAP_PERSISTANT_W instance is not open yet
 **********************************************************************************************************************/
fsp_err_t RM_MAP_PERSISTANT_W_Erase_GROUP(map_persistant_w_ctrl_t * const p_api_ctrl,
                                        const char* group)
{
    map_persistant_w_instance_ctrl_t * p_ctrl = (map_persistant_w_instance_ctrl_t *) p_api_ctrl;

    FSP_ASSERT(p_ctrl != NULL);
    FSP_ERROR_RETURN(MAP_PERSISTANT_W_OPEN == p_ctrl->map_persistant_w_open, FSP_ERR_NOT_OPEN);

#if CFG_PMGR
    /* At this case,,, dpm_sleep operation was started already */
    if (RM_PMGR_W_dpm_sleep_is_started()) {
        printf("[%s] Already DPM Sleep started !!!\n", __func__);
        return FSP_ERR_NOT_ERASED;
    }
#endif /* CFG_PMGR */

    rm_vee_group_instance_t *rm_vee_param;

    if (group != NULL) {

        xSemaphoreTakeRecursive(p_ctrl->map_persistant_w_mutex, portMAX_DELAY);
        rm_vee_param = vee_nvparam_open(group);

        if (rm_vee_param == NULL) {
            ENVRION_ERROR("[%s] vee_nvparam is null\n", __func__);
            xSemaphoreGiveRecursive(p_ctrl->map_persistant_w_mutex);
            return FSP_ERR_NOT_FOUND;
        }

        vee_nvparam_erase_all(rm_vee_param);

        vee_nvparam_close(rm_vee_param);

        xSemaphoreGiveRecursive(p_ctrl->map_persistant_w_mutex);
    } else {
        xSemaphoreTakeRecursive(p_ctrl->map_persistant_w_mutex, portMAX_DELAY);

        rm_vee_param = vee_nvparam_open(NULL);

        if (rm_vee_param == NULL) {
            ENVRION_ERROR("[%s] vee_nvparam(format) is null\n", __func__);
            xSemaphoreGiveRecursive(p_ctrl->map_persistant_w_mutex);
            return FSP_ERR_NOT_FOUND;
        }

        vee_nvparam_format(rm_vee_param);
        vee_nvparam_close(rm_vee_param);

        xSemaphoreGiveRecursive(p_ctrl->map_persistant_w_mutex);
    }
    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Reflash the MAP PERSISTANT_W Storage
 *
 * @retval     FSP_SUCCESS        MAP_PERSISTANT_W driver closed
 * @retval     FSP_ERR_ASSERTION  Null Pointer
 * @retval     FSP_ERR_NOT_OPEN   MAP_PERSISTANT_W instance is not open yet
 **********************************************************************************************************************/
fsp_err_t RM_MAP_PERSISTANT_W_reflash(map_persistant_w_ctrl_t * const p_api_ctrl)
{
    map_persistant_w_instance_ctrl_t * p_ctrl = (map_persistant_w_instance_ctrl_t *) p_api_ctrl;

    FSP_ASSERT(p_ctrl != NULL);
    FSP_ERROR_RETURN(MAP_PERSISTANT_W_OPEN == p_ctrl->map_persistant_w_open, FSP_ERR_NOT_OPEN);

    rm_vee_group_instance_t *rm_vee_param;

    if (rm_vee_flash_w_multi_use == 0) {
        return FSP_ERR_ASSERTION;
    }

    xSemaphoreTakeRecursive(p_ctrl->map_persistant_w_mutex, portMAX_DELAY);

    rm_vee_param = vee_nvparam_open(NULL);

    if (rm_vee_param == NULL) {
        ENVRION_ERROR("[%s] vee_nvparam is null\n", __func__);
        xSemaphoreGiveRecursive(p_ctrl->map_persistant_w_mutex);
        return FSP_ERR_NOT_FOUND;
    }

    vee_nvparam_refresh(rm_vee_param);

    vee_nvparam_close(rm_vee_param);

    xSemaphoreGiveRecursive(p_ctrl->map_persistant_w_mutex);
    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Display the MAP PERSISTANT_W Storage
 *
 * @retval     FSP_SUCCESS        MAP_PERSISTANT_W driver closed
 * @retval     FSP_ERR_ASSERTION  Null Pointer
 * @retval     FSP_ERR_NOT_OPEN   MAP_PERSISTANT_W instance is not open yet
 **********************************************************************************************************************/
fsp_err_t RM_MAP_PERSISTANT_W_Display(map_persistant_w_ctrl_t * const p_api_ctrl,
                                    const char *groupname, int printall)
{
    map_persistant_w_instance_ctrl_t * p_ctrl = (map_persistant_w_instance_ctrl_t *) p_api_ctrl;

    FSP_ASSERT(p_ctrl != NULL);
    FSP_ERROR_RETURN(MAP_PERSISTANT_W_OPEN == p_ctrl->map_persistant_w_open, FSP_ERR_NOT_OPEN);

    rm_vee_group_instance_t *p_vee;

    int         used_recs = 0;
    int         total_recs = 0;
    int         status = FSP_SUCCESS;
    char        delimeterspace[DELIMETER_MAXLEN+1];

    if (groupname == NULL) {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    xSemaphoreTakeRecursive(p_ctrl->map_persistant_w_mutex, portMAX_DELAY);

    p_vee  =  vee_nvparam_open(groupname);

    if (p_vee == NULL) {
        ENVRION_ERROR("vee_nvparam is null(%s)\n", groupname);
        xSemaphoreGiveRecursive(p_ctrl->map_persistant_w_mutex);
        return FSP_ERR_NOT_FOUND;
    }

    uint32_t gid = find_group_id(p_vee->area);

    ENVRION_PRINT("%s(gid:%03lu):\n\n", groupname, gid);

    total_recs = p_vee->area->num_parameters;

    if (printall) {
        ENVRION_PRINT("\tTag_Name(ID)                                 [Used/Max/WriteLen] [Data]\n");
        ENVRION_PRINT("\t");
        print_separate_bar('-', 80, 1);
    } else {
        ENVRION_PRINT("\tTag_Name(ID)                                 [Used/MaxLen] [Data]\n");
        ENVRION_PRINT("\t");
        print_separate_bar('-', 70, 1);
    }

    for (size_t i = 0; i < p_vee->area->num_parameters; i++) {
        parameter_t *param = &(p_vee->area->parameters[i]);
        uint32_t rec_id = gid + i;
        uint8_t  *stored_data;
        uint32_t  stored_len;

        fsp_err_t err = p_vee->g_vee->p_api->recordPtrGet(p_vee->g_vee->p_ctrl, rec_id, &stored_data, &stored_len);

        uint32_t name_length = strlen(param->name);
        uint32_t idx = 0;

        while (idx < (DELIMETER_MAXLEN-name_length)) {
            delimeterspace[idx] = '.';
            idx++;
        }

        if (rec_id > 1000) {
            idx--;
        }

        delimeterspace[idx] = '\0';

        if (FSP_ERR_NOT_FOUND == err) {
            if (param->attr.flags == FLAG_VARIABLE_LEN) {
                if (printall) {
                    ENVRION_PRINT("\t%s(%03lu) %s [00/%02u/%02u] <Empty, No Rec>\n",
                        param->name, rec_id, delimeterspace,
                        (param->attr.length - PARAM_STR_EXTRA),
                        /* flash write len */
                        DATA_ALIGNMENT(param->attr.length));
                }
            } else {
                if (printall) {
                    char data_type[4]; // u32
                    if (param->attr.length == 1) {
                        strcpy(data_type, " u8");
                    } else if (param->attr.length == 2) {
                        strcpy(data_type, "u16");
                    } else {
                        strcpy(data_type, "u32");
                    }

                    ENVRION_PRINT("\t%s(%03lu) %s [ %s /%02u] <Empty, No Rec>\n",
                        param->name, rec_id, delimeterspace, data_type,
                        /* flash write len */
                        DATA_ALIGNMENT(param->attr.length));
                }
            }
            continue;
        } else if(FSP_SUCCESS != err) {
            status = -1;
            break;
        }

        uint16_t lenfield  = *(uint16_t *)(&(stored_data[stored_len - sizeof(uint16_t)]));

        if (FLAG_VARIABLE_LEN == (param->attr.flags)) { // String Type
            if (lenfield == (0xFFFFU)) {
                if (printall) {
                    ENVRION_PRINT("\t%s(%03lu) %s [%02u/%02u/%02u] <Empty, Erased>\n",
                        param->name, rec_id, delimeterspace, 0,
                        (param->attr.length - PARAM_STR_EXTRA),
                        DATA_ALIGNMENT(param->attr.length));
                }
            } else {
                lenfield -= STR_END;
                used_recs++;
                if (printall) {
                    ENVRION_PRINT("\t%s(%03lu) %s [%02u/%02u/%02u]",
                        param->name, rec_id, delimeterspace,  lenfield,
                        (param->attr.length - PARAM_STR_EXTRA),
                        DATA_ALIGNMENT(param->attr.length));
                } else {
                    ENVRION_PRINT("\t%s(%03lu) %s [%02u/%02u]",
                        param->name, rec_id, delimeterspace,  lenfield,
                        (param->attr.length - PARAM_STR_EXTRA));
                }
                ENVRION_PRINT(" %s\n", (char *) (&(stored_data[0])) );
            }
        } else { // bin type (uint8, untt16, uint32)
            bin_value_t *bin_value = (bin_value_t *)stored_data;
            switch (param->attr.length) {
                case sizeof(uint8_t):
                {
                    if (lenfield == (0xFFFFU)) {
                        ENVRION_PRINT_LVL1(printall,
                            "\t%s(%03lu) %s [u8/%lu]  <Empty, Erased>\n",
                            param->name, rec_id, delimeterspace,
                            stored_len);
                    } else {
                        used_recs++;
                        if (printall) {
                            ENVRION_PRINT("\t%s(%03lu) %s [u8/%lu]  %u(0x%x)\n",
                                param->name, rec_id, delimeterspace,
                                stored_len, bin_value->val8, bin_value->val8);
                        } else {
                            ENVRION_PRINT("\t%s(%03lu) %s [u8/%u] %u\n",
                                param->name, rec_id, delimeterspace,
                                param->attr.length, bin_value->val8);
                        }
                    }
                    break;
                }

                case sizeof(uint16_t):
                {
                    if (lenfield == (0xFFFFU)) {
                        if (printall) {
                            ENVRION_PRINT("\t%s(%03lu) %s [u16/%lu]  <Empty, Erased>\n",
                                param->name, rec_id, delimeterspace, stored_len);
                        }
                    } else {
                        used_recs++;
                        if (printall) {
                            ENVRION_PRINT("\t%s(%lu) %s [u16/%lu]  %u(0x%x)\n",
                                param->name, rec_id, delimeterspace,
                                stored_len, bin_value->val16, bin_value->val16);
                        } else {
                            ENVRION_PRINT("\t%s(%lu) %s [u16/%u] %u\n",
                                param->name, rec_id, delimeterspace,
                                param->attr.length, bin_value->val16);
                        }
                    }
                    break;
                }

                default:
                {
                    if (lenfield == (0xFFFFU)) {
                        if (printall) {
                            ENVRION_PRINT("\t%s(%03lu) %s [u32/%lu]  <Empty, Erased>\n",
                                param->name, rec_id, delimeterspace, stored_len);
                        }
                    } else {
                        used_recs++;
                        if (!printall) {
                            ENVRION_PRINT("\t%s(%03lu) %s [u32/%u] %lu\n",
                                param->name, rec_id, delimeterspace,
                                param->attr.length, bin_value->val32);
                        } else {
                            ENVRION_PRINT("\t%s(%03lu) %s [u32/%lu]  %lu(0x%lx)\n",
                                param->name, rec_id, delimeterspace, stored_len,
                                bin_value->val32, bin_value->val32);
                        }
                    }
                    break;
                }
            }
        }
    }

    if (used_recs == 0 && !printall) {
        ENVRION_PRINT("\t<Empty>\n");
    }

    // Used Records / Total Records
    ENVRION_PRINT("Total %d/%d\n\n\n", used_recs, total_recs);

    if (strcmp(ENV_GROUP_APPCFG, groupname) == 0 && printall) {
        rm_vee_status_t vee_status;
        p_vee->g_vee->p_api->statusGet(p_vee->g_vee->p_ctrl, &vee_status);

        if (printall) {
            ENVRION_PRINT("VEE: last_id - %lu\n", vee_status.last_id);
            ENVRION_PRINT("VEE: segment_erase_count - %lu\n", vee_status.segment_erase_count);
            ENVRION_PRINT("VEE: space_available - %lu\n", vee_status.space_available);
        }
    }

    vee_nvparam_close(p_vee);

    xSemaphoreGiveRecursive(p_ctrl->map_persistant_w_mutex);
    return status;
}

/*******************************************************************************************************************//**
 * Auto the MAP_PERSISTANT_W Instance
 *
 * @retval     FSP_SUCCESS        MAP_PERSISTANT_W driver closed
 * @retval     FSP_ERR_ASSERTION  Null Pointer
 * @retval     FSP_ERR_NOT_OPEN   MAP_PERSISTANT_W instance is not open yet
 **********************************************************************************************************************/
fsp_err_t RM_MAP_PERSISTANT_W_Read_Auto(map_persistant_w_ctrl_t * const p_api_ctrl,
                                 const char * groupname,
                                 const char * envname,
                                 int8_t * data_length, uint8_t ** data_ptr)
{
    map_persistant_w_instance_ctrl_t * p_ctrl = (map_persistant_w_instance_ctrl_t *) p_api_ctrl;

    FSP_ASSERT(p_ctrl != NULL);
    FSP_ERROR_RETURN(MAP_PERSISTANT_W_OPEN == p_ctrl->map_persistant_w_open, FSP_ERR_NOT_OPEN);

    rm_vee_group_instance_t * rm_vee_param;
    uint8_t * vee_ro_data = NULL;

    if (groupname == NULL || envname == NULL) {
        *data_ptr = NULL;
        return FSP_ERR_INVALID_ARGUMENT;
    }

    xSemaphoreTakeRecursive(p_ctrl->map_persistant_w_mutex, portMAX_DELAY);

    rm_vee_param  =  vee_nvparam_open(groupname);

    if ( rm_vee_param == NULL ) {
        ENVRION_DEBUG("[%s] (%s->%s) vee_nvparam_open is null\n", __func__, groupname, envname);
        xSemaphoreGiveRecursive(p_ctrl->map_persistant_w_mutex);
	*data_ptr = NULL;
        return FSP_ERR_NOT_FOUND;
    }

    uint8_t flag = 0;
    *data_length = vee_nvparam_read_offset_by_name(rm_vee_param, envname, 0, (void **) (&vee_ro_data), &flag);

    vee_nvparam_close(rm_vee_param);

    if (*data_length == 0) {
        ENVRION_DEBUG("[%s] (%s->%s) vee_nvparam_open is null\n", __func__, groupname, envname);
	xSemaphoreGiveRecursive(p_ctrl->map_persistant_w_mutex);
        *data_ptr = NULL;
        return FSP_ERR_ASSERTION;
    }
    switch (flag) {
        case FLAG_VARIABLE_LEN: // string
        {
            *data_length = 0; // string
            break;
        }

        case 0: // int8, int16, int32
        {
            if (*data_length == 0) {
                ENVRION_ERROR("[%s] nvparam is NOT inited\n", __func__);
            }
            break;
        }

        default: // -1 : not found
        {
            *data_length = -1;
            ENVRION_ERROR("[%s] (%s -> %s) not found\n", __func__, groupname, envname);
            break;
        }
    }

    xSemaphoreGiveRecursive(p_ctrl->map_persistant_w_mutex);
    *data_ptr = vee_ro_data;

    return FSP_SUCCESS;
}


/*******************************************************************************************************************//**
 * Write BINARY with index offset to the MAP PERSISTANT_W Storage
 *
 * @retval     FSP_SUCCESS        MAP_PERSISTANT_W driver closed
 * @retval     FSP_ERR_ASSERTION  Null Pointer
 * @retval     FSP_ERR_NOT_OPEN   MAP_PERSISTANT_W instance is not open yet
 **********************************************************************************************************************/
fsp_err_t RM_MAP_PERSISTANT_W_Write_BIN(map_persistant_w_ctrl_t * const p_api_ctrl,
                                         	   const char* group, uint32_t id_offset, uint8_t * value)
{

    map_persistant_w_instance_ctrl_t * p_ctrl = (map_persistant_w_instance_ctrl_t *) p_api_ctrl;

    FSP_ASSERT(p_ctrl != NULL);
    FSP_ERROR_RETURN(MAP_PERSISTANT_W_OPEN == p_ctrl->map_persistant_w_open, FSP_ERR_NOT_OPEN);

#if CFG_PMGR
    /* At this case,,, dpm_sleep operation was started already */
    if (RM_PMGR_W_dpm_sleep_is_started()) {
        printf("[%s] Already DPM Sleep started !!!\n", __func__);
        return FSP_ERR_WRITE_FAILED;
    }
#endif /* CFG_PMGR */

    rm_vee_group_instance_t *rm_vee_param;
    fsp_err_t err ;
    uint32_t base_indx;

    if (group == NULL) {
        ENVRION_DEBUG("[%s] (group=NULL)\n", __func__);
        return FSP_ERR_INVALID_ARGUMENT;
    }

    if (value == NULL) {
        ENVRION_DEBUG("[%s] (%s) data=null\n", __func__, group);
        return FSP_ERR_INVALID_ARGUMENT;
    }

    xSemaphoreTakeRecursive(p_ctrl->map_persistant_w_mutex, portMAX_DELAY);
    rm_vee_param = vee_nvparam_open(group);

    if (rm_vee_param == NULL) {
		ENVRION_DEBUG("[%s] (%s) vee_nvparam is null\n", __func__, group);
		xSemaphoreGiveRecursive(p_ctrl->map_persistant_w_mutex);
		return FSP_ERR_NOT_FOUND;
    }

    base_indx =  find_group_id(rm_vee_param->area);
    const parameter_t *param = &rm_vee_param->area->parameters[id_offset];

    err =  rm_vee_param->g_vee->p_api->recordWrite(rm_vee_param->g_vee->p_ctrl, base_indx + id_offset, value, param->attr.length);

    vee_nvparam_close(rm_vee_param);

    xSemaphoreGiveRecursive(p_ctrl->map_persistant_w_mutex);
    return err;
}

 
/*******************************************************************************************************************//**
 * Read BINARY with index offset to the MAP PERSISTANT_W Storage
 *
 * @retval     FSP_SUCCESS        MAP_PERSISTANT_W driver closed
 * @retval     FSP_ERR_ASSERTION  Null Pointer
 * @retval     FSP_ERR_NOT_OPEN   MAP_PERSISTANT_W instance is not open yet
 **********************************************************************************************************************/ 
fsp_err_t RM_MAP_PERSISTANT_W_Read_BIN(map_persistant_w_ctrl_t * const p_api_ctrl,
                                         	 const char* group, uint32_t id_offset, uint8_t ** value)
{

	volatile map_persistant_w_instance_ctrl_t * p_ctrl = (map_persistant_w_instance_ctrl_t *) p_api_ctrl;
    FSP_ASSERT(p_ctrl != NULL);
    FSP_ERROR_RETURN(MAP_PERSISTANT_W_OPEN == p_ctrl->map_persistant_w_open, FSP_ERR_NOT_OPEN);

    uint8_t *vee_ro_data = NULL;
    uint32_t num_of_bytes = 0;
    *value = NULL;
	fsp_err_t err ;

    static rm_vee_group_instance_t *rm_vee_param;
    uint32_t base_indx;

    if (group == NULL) {
        ENVRION_DEBUG("[%s] (group=NULL)\n", __func__);
        return FSP_ERR_INVALID_ARGUMENT;
    }

    xSemaphoreTakeRecursive(p_ctrl->map_persistant_w_mutex, portMAX_DELAY);
    rm_vee_param = vee_nvparam_open(group);

    if (rm_vee_param == NULL) {
        ENVRION_DEBUG("[%s] (%s) vee_nvparam is null\n", __func__, group);
        xSemaphoreGiveRecursive(p_ctrl->map_persistant_w_mutex);
        return FSP_ERR_NOT_FOUND;
    }

    base_indx =  find_group_id(rm_vee_param->area);

    err = rm_vee_param->g_vee->p_api->recordPtrGet(rm_vee_param->g_vee->p_ctrl, base_indx + id_offset,  (uint8_t **) &vee_ro_data, &num_of_bytes);
    vee_nvparam_close(rm_vee_param);

    if (FSP_SUCCESS != err) {
    	*value = NULL;
		ENVRION_DEBUG("\33[1;31m" "\t[%s] '%s' read err=%d\n" "\33[0;39m", __func__, group, err);
		xSemaphoreGiveRecursive(p_ctrl->map_persistant_w_mutex);
		return FSP_ERR_NOT_FOUND;
     }

    *value = vee_ro_data;

    xSemaphoreGiveRecursive(p_ctrl->map_persistant_w_mutex);
    return FSP_SUCCESS;
}
/*******************************************************************************************************************//**
 * @} (end addtogroup RM_MAP_PERSISTANT_W)
 **********************************************************************************************************************/
