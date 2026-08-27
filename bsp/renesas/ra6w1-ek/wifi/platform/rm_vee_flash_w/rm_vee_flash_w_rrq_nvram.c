/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include "rm_vee_flash_w_cfg.h"
#ifdef RM_VEE_USE_ENV
 #include <stdarg.h>
 #include <stdio.h>
 #include <string.h>
 #include <stdlib.h>
 #include "bsp_api.h"                  /*FOR OS_FREERTOS*/
 #include "FreeRTOS.h"
 #include "semphr.h"
 #include "task.h"
 #include "rm_vee_flash_w.h"

 #include "rm_vee_api.h"
 #if CFG_PMGR
  #include "rm_pmgr_w_instance.h"
 #endif

 #include "rm_vee_flash_w_rrq_nvram.h"
 #ifdef RM_MAP_PERSISTANT_W
  #include "rm_map_persistant_w.h"
 #endif

 #if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
  #include "r_wdog_w.h"
  #include "rm_wifi.h"
 #endif

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
 #ifndef MIN
  #define MIN(a, b)                      (((a) < (b)) ? (a) : (b))
 #endif

 #define AREA_ID_UNKNOWN    (0xFFFFFFFFU)

 #define ENVRION_PRINT(...)              printf(__VA_ARGS__)
 #define ENVRION_PRINT_LVL1(lvl, ...)    if ((lvl == 1)) {printf(__VA_ARGS__);}
 #define ENVRION_ERROR(...)              printf(__VA_ARGS__)
 #define ENVRION_DEBUG(...)            // printf(__VA_ARGS__)

 #define DELIMETER_MAXLEN    40
 #define DATA_ALIGNMENT(x)               (((x) + 1) & (~0x00001U))
 #define GTAG_ALIGNMENT(x)               (x)

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
typedef enum
{
    BOOTCFG_ID     = 1,                /* 1~ 100 */
    DEVCFG_ID      = 101,              /* 101 ~ 200 */
    WIFICFG_ID     = 201,              /* 201 ~ 1000 */
    SYSCFG_ID      = 1001,             /* 1001 ~ 1300 */
    APPCFG_ID      = 1301,             /* 1301 ~ 1800 */
    BLECFG_ID      = 1801,             /* 1801 ~ 1880 */
    BLESEC_ID      = 1881,             /* 1881 ~ 1900 */
    SECUREASSET_ID = 1901,             /* 1900 ~ 1950 */
    TESTCFG_ID     = 1951,             /* 1951 ~ 1970 */
    WIFIPROFILE_ID = 1971,             /* 1971 ~ 2047 */
    MAX_ID         = 2048,             /* MAX */
} nvram_vee_group_id_t;

enum
{
    FLAG_VARIABLE_LEN = 0x01,          // parameter has variable length
};

typedef struct
{
    const char * name;                 // unique parameter tagname, user has to ensure the unique name
    struct
    {
        uint16_t flags  : 1;           // parameter flags
        uint16_t length : 15;          // parameter max length
    } attr;
} parameter_t;

typedef struct
{
    const char  * name;                // unique area name, user has to ensure the unique name
    parameter_t * parameters;          // list of area parameters
    size_t        num_parameters;      // number of area parameters
} area_t;

typedef struct
{
    area_t * area;
    uint32_t id;
    const rm_vee_instance_t * g_vee;
} rm_vee_group_instance_t;

typedef union
{
    uint8_t  val8;
    uint16_t val16;
    uint32_t val32;
} bin_value_t;

/* Create nvparam configuration from ad_nvparam_defs.h */
 #define IN_AD_NVPARAM_C
 #include "wifi_ad_nvparam_defs.h"

 #ifndef RM_MAP_PERSISTANT_W
void ra6w1_environ_lock(unsigned long flag);

/***********************************************************************************************************************
 * Exported global variables (to be accessed by other files)
 **********************************************************************************************************************/
static bool envron_initialized = false;
  #ifndef OS_BAREMETAL
   #define OS_MUTEX                   SemaphoreHandle_t
   #define OS_MUTEX_CREATE_SUCCESS    1
   #define OS_MUTEX_CREATE_FAILED     0
   #define OS_MUTEX_FOREVER           portMAX_DELAY
   #define OS_ASSERT                  configASSERT
   #define OS_MUTEX_CREATE(mutex)                                         \
    ({                                                                    \
        (mutex) = xSemaphoreCreateRecursiveMutex();                       \
        mutex != NULL ? OS_MUTEX_CREATE_SUCCESS : OS_MUTEX_CREATE_FAILED; \
    })
   #define OS_MUTEX_PUT(mutex)             xSemaphoreGiveRecursive(mutex)
   #define OS_MUTEX_GET(mutex, timeout)    xSemaphoreTakeRecursive((mutex), (timeout))
static OS_MUTEX envron_mutex;
  #endif
 #endif

 #define NVPARAM_AREA_SIZE(NAME)           (sizeof(area_ ## NAME) / sizeof(parameter_t))
 #define NVPARAM_AREA_NAME(NAME)           (area_ ## NAME)

extern const rm_vee_instance_t g_vee0;
const parameter_t * find_parameter_by_name(const area_t * area, char * name, uint16_t * rindex);
uint32_t            find_group_id(const area_t * area);
void                find_group_list(void);

bool                      chk_duplicates_env(void);
void BSP_WEAK_REFERENCE   print_separate_bar(unsigned char text, unsigned char loop_count, unsigned char CR_loop_count);
void                      vee_nvparam_close(rm_vee_group_instance_t * p_vee);
rm_vee_group_instance_t * vee_nvparam_open(const char * area_name);

uint32_t vee_nvparam_read_offset_by_name(rm_vee_group_instance_t * p_vee,
                                         const char              * name,
                                         uint16_t                  offset,
                                         void                   ** datam,
                                         uint8_t                 * flag);
uint16_t vee_nvparam_write_offset_by_name(rm_vee_group_instance_t * p_vee,
                                          const char              * name,
                                          uint16_t                  is_string,
                                          uint16_t                  offset,
                                          const void              * data);
uint16_t vee_nvparam_erase_by_name(rm_vee_group_instance_t * p_vee, const char * name);
void     vee_nvparam_erase_all(rm_vee_group_instance_t * p_vee);
void     vee_nvparam_format(rm_vee_group_instance_t * p_vee);
void     vee_nvparam_refresh(rm_vee_group_instance_t * p_vee);

uint16_t rm_vee_flash_w_multi_use = 0;
static rm_vee_group_instance_t area_group_list[num_areas];
static uint8_t                 rm_vee_flash_w_buffer[RM_VEE_FLASH_W_REF_DATA_SIZE];

bool chk_duplicates_env (void)
{
    bool status = pdFALSE;

    for (int i = 0; i < (int) NVPARAM_AREA_SIZE(bootcfg); ++i)
    {
        for (int j = i + 1; j < (int) NVPARAM_AREA_SIZE(bootcfg); ++j)
        {
            if (strcmp(NVPARAM_AREA_NAME(bootcfg)[i].name, NVPARAM_AREA_NAME(bootcfg)[j].name) == 0)
            {
                printf("NVRAM: Finding duplicates in group %s - %s (gid=%d, gid=%d)\n", "bootcfg",
                       NVPARAM_AREA_NAME(bootcfg)[i].name, i, j);
                status = pdTRUE;
            }
        }
    }

    for (int i = 0; i < (int) NVPARAM_AREA_SIZE(devcfg); ++i)
    {
        for (int j = i + 1; j < (int) NVPARAM_AREA_SIZE(devcfg); ++j)
        {
            if (strcmp(NVPARAM_AREA_NAME(devcfg)[i].name, NVPARAM_AREA_NAME(devcfg)[j].name) == 0)
            {
                printf("NVRAM: Finding duplicates in group %s - %s (gid=%d, gid=%d)\n", "devcfg",
                       NVPARAM_AREA_NAME(devcfg)[i].name, i, j);
                status = pdTRUE;
            }
        }
    }

    for (int i = 0; i < (int) NVPARAM_AREA_SIZE(wificfg); ++i)
    {
        for (int j = i + 1; j < (int) NVPARAM_AREA_SIZE(wificfg); ++j)
        {
            if (strcmp(NVPARAM_AREA_NAME(wificfg)[i].name, NVPARAM_AREA_NAME(wificfg)[j].name) == 0)
            {
                printf("NVRAM: Finding duplicates in group %s - %s (gid=%d, gid=%d)\n", "wificfg",
                       NVPARAM_AREA_NAME(wificfg)[i].name, i, j);
                status = pdTRUE;
            }
        }
    }

    for (int i = 0; i < (int) NVPARAM_AREA_SIZE(syscfg); ++i)
    {
        for (int j = i + 1; j < (int) NVPARAM_AREA_SIZE(syscfg); ++j)
        {
            if (strcmp(NVPARAM_AREA_NAME(syscfg)[i].name, NVPARAM_AREA_NAME(syscfg)[j].name) == 0)
            {
                printf("NVRAM: Finding duplicates in group %s - %s (gid=%d, gid=%d)\n", "syscfg",
                       NVPARAM_AREA_NAME(syscfg)[i].name, i, j);
                status = pdTRUE;
            }
        }
    }

    for (int i = 0; i < (int) NVPARAM_AREA_SIZE(appcfg); ++i)
    {
        for (int j = i + 1; j < (int) NVPARAM_AREA_SIZE(appcfg); ++j)
        {
            if (strcmp(NVPARAM_AREA_NAME(appcfg)[i].name, NVPARAM_AREA_NAME(appcfg)[j].name) == 0)
            {
                printf("NVRAM: Finding duplicates in group %s - %s (gid=%d, gid=%d)\n", "appcfg",
                       NVPARAM_AREA_NAME(appcfg)[i].name, i, j);
                status = pdTRUE;
            }
        }
    }

    for (int i = 0; i < (int) NVPARAM_AREA_SIZE(testcfg); ++i)
    {
        for (int j = i + 1; j < (int) NVPARAM_AREA_SIZE(testcfg); ++j)
        {
            if (strcmp(NVPARAM_AREA_NAME(testcfg)[i].name, NVPARAM_AREA_NAME(appcfg)[j].name) == 0)
            {
                printf("NVRAM: Finding duplicates in group %s - %s (gid=%d, gid=%d)\n", "testcfg",
                       NVPARAM_AREA_NAME(testcfg)[i].name, i, j);
                status = pdTRUE;
            }
        }
    }

    return status;
}

 #ifndef RM_MAP_PERSISTANT_W
void init_environ (void)
{
    if (envron_initialized == false)
    {
  #ifndef OS_BAREMETAL
        OS_MUTEX_CREATE(envron_mutex);
        OS_ASSERT(envron_mutex);
  #endif
        envron_initialized = true;

        ra6w1_environ_lock(true);

        rm_vee_group_instance_t * rm_vee_param;

        rm_vee_param = vee_nvparam_open(NULL);

        if (rm_vee_param == NULL)
        {
            ENVRION_ERROR("%s: vee_nvparam is null\n", __func__);
            ra6w1_environ_lock(false);

            return;
        }

        ra6w1_environ_lock(false);
    }
}

void ra6w1_environ_lock (unsigned long flag)
{
    // Do Nothing !!!
  #ifndef OS_BAREMETAL
    if (true == flag)
    {
        OS_MUTEX_GET(envron_mutex, OS_MUTEX_FOREVER);
    }
    else
    {
        OS_MUTEX_PUT(envron_mutex);
    }
  #endif
}

char * ra6w1_getenv (const char * groupname, const char * envname)
{
    rm_vee_group_instance_t * rm_vee_param;
    uint16_t  data_length;
    uint8_t * vee_ro_data = NULL;

    if ((groupname == NULL) || (envname == NULL))
    {
        return NULL;
    }

    ra6w1_environ_lock(true);

    rm_vee_param = vee_nvparam_open(groupname);

    if (rm_vee_param == NULL)
    {
        ENVRION_DEBUG("[%s] (%s->%s) vee_nvparam_open is null\n", __func__, groupname, envname);
        ra6w1_environ_lock(false);

        return NULL;
    }

    ENVRION_DEBUG("[%s] (%s->%s)\n", __func__, groupname, envname);
    uint8_t flag = 0;
    data_length = vee_nvparam_read_offset_by_name(rm_vee_param, envname, 0, (void **) (&vee_ro_data), &flag);

    vee_nvparam_close(rm_vee_param);

    if (data_length == 0)
    {
        ra6w1_environ_lock(false);
        ENVRION_DEBUG("[%s] (%s->%s) data_length=0\n", __func__, groupname, envname);

        return NULL;
    }

    ra6w1_environ_lock(false);

    return (char *) vee_ro_data;
}

 #endif

 #ifndef RM_MAP_PERSISTANT_W
char * ra6w1_getenv_auto (const char * groupname, const char * envname, void * value, int8 * data_type)
{
    FSP_PARAMETER_NOT_USED(value);

    rm_vee_group_instance_t * rm_vee_param;
    uint16_t data_length;
    char   * vee_ro_data = NULL;

    if ((groupname == NULL) || (envname == NULL))
    {
        return NULL;
    }

    ra6w1_environ_lock(true);

    rm_vee_param = vee_nvparam_open(groupname);

    if (rm_vee_param == NULL)
    {
        ENVRION_DEBUG("[%s] (%s->%s) vee_nvparam_open is null\n", __func__, groupname, envname);
        ra6w1_environ_lock(false);

        return NULL;
    }

    uint8_t flag = 0;
    data_length = vee_nvparam_read_offset_by_name(rm_vee_param, envname, 0, (void **) (&vee_ro_data), &flag);

    vee_nvparam_close(rm_vee_param);

    if (data_length == 0)
    {
        ENVRION_DEBUG("[%s] (%s->%s) vee_nvparam_open is null\n", __func__, groupname, envname);
        ra6w1_environ_lock(false);

        return NULL;
    }

    switch (flag)
    {
        case FLAG_VARIABLE_LEN:        // string
        {
            *data_type = 0;            // string
            break;
        }

        case 0:                        // int8, int16, int32
        {
            if (data_length == 0)
            {
                ENVRION_ERROR("[%s] nvparam is NOT inited\n", __func__);
            }

            *data_type = (int8) data_length;
            break;
        }

        default:                       // -1 : not found
        {
            *data_type = -1;
            ENVRION_ERROR("[%s] (%s -> %s) not found\n", __func__, groupname, envname);
            break;
        }
    }

    ra6w1_environ_lock(false);

    return vee_ro_data;
}

 #endif

 #ifndef RM_MAP_PERSISTANT_W
int ra6w1_setenv (const char * groupname, const char * envname, char * value)
{
    rm_vee_group_instance_t * rm_vee_param;
    uint16_t data_length;

    if (groupname == NULL)
    {
        ENVRION_ERROR("[%s] (group=NULL->%s)\n", __func__, envname);

        return FALSE;
    }

    if (envname == NULL)
    {
        ENVRION_ERROR("[%s] (%s->envname=NULL)\n", __func__, groupname);

        return FALSE;
    }

    if (value == NULL)
    {
        ENVRION_DEBUG("[%s] (%s->%s) data=null\n", __func__, groupname, envname);

        return TRUE;
    }

    ENVRION_DEBUG("[%s] (%s->%s)\n", __func__, groupname, envname);
    ra6w1_environ_lock(true);

    rm_vee_param = vee_nvparam_open(groupname);

    if (rm_vee_param == NULL)
    {
        ENVRION_ERROR("[%s] (%s->%s) vee_nvparam is null\n", __func__, groupname, envname);
        ra6w1_environ_lock(false);

        return FALSE;
    }

    data_length = vee_nvparam_write_offset_by_name(rm_vee_param, envname, AS_STRING, 0, value);

    vee_nvparam_close(rm_vee_param);

    if (data_length == 0)
    {
        ENVRION_ERROR("[%s] (%s->%s) vee_nvparam error\n", __func__, groupname, envname);
        ra6w1_environ_lock(false);

        return FALSE;
    }

    ra6w1_environ_lock(false);

    return TRUE;
}

int ra6w1_unsetenv (const char * groupname, const char * envname)
{
    rm_vee_group_instance_t * rm_vee_param;
    uint16_t data_length;

    if ((groupname == NULL) || (envname == NULL))
    {
        return pdFALSE;
    }

    ra6w1_environ_lock(true);

    rm_vee_param = vee_nvparam_open(groupname);

    if (rm_vee_param == NULL)
    {
        ENVRION_ERROR("[%s] (%s->%s) vee_nvparam is null\n", __func__, groupname, envname);
        ra6w1_environ_lock(false);

        return pdFALSE;
    }

    ENVRION_DEBUG("[%s] (%s->%s) start vee_nvparam_erase_by_name\n", __func__, groupname, envname);
    data_length = vee_nvparam_erase_by_name(rm_vee_param, envname);

    vee_nvparam_close(rm_vee_param);

    if (data_length == 0)
    {
        ENVRION_DEBUG("[%s] (%s->%s) vee_nvparam Erase Skip\n", __func__, groupname, envname);
    }

    ra6w1_environ_lock(false);

    return pdTRUE;
}

int ra6w1_clearenv (const char * groupname)
{
    rm_vee_group_instance_t * rm_vee_param;

    if (groupname != NULL)
    {
        ra6w1_environ_lock(true);
        rm_vee_param = vee_nvparam_open(groupname);

        if (rm_vee_param == NULL)
        {
            ENVRION_ERROR("[%s] vee_nvparam is null\n", __func__);
            ra6w1_environ_lock(false);

            return FALSE;
        }

        ENVRION_DEBUG("[%s] (%s) %d start vee_nvparam_erase_all\n", __func__, groupname, __LINE__);
        vee_nvparam_erase_all(rm_vee_param);
        vee_nvparam_close(rm_vee_param);
        ra6w1_environ_lock(false);
    }
    else
    {
        ra6w1_environ_lock(true);

        rm_vee_param = vee_nvparam_open(NULL);

        if (rm_vee_param == NULL)
        {
            ENVRION_ERROR("[%s] vee_nvparam(format) is null\n", __func__);
            ra6w1_environ_lock(false);

            return FALSE;
        }

        vee_nvparam_format(rm_vee_param);
        vee_nvparam_close(rm_vee_param);

        ra6w1_environ_lock(false);
    }

    return TRUE;
}

 #endif

/**
 * ex)
 * print_separate_bar("=", 10, 2);
 *
 * "==========\n\n"
 *
 */
void BSP_WEAK_REFERENCE print_separate_bar (unsigned char text, unsigned char loop_count, unsigned char CR_loop_count)
{
    unsigned char prt_str[260];

    memset(prt_str, 0, 256);

    if ((loop_count + CR_loop_count) + 1 > 260)
    {
        loop_count = (unsigned char) (260 - (CR_loop_count - 1));
    }

    memset(prt_str, text, loop_count);

    if (CR_loop_count > 0)
    {
        memset(prt_str + loop_count, '\n', CR_loop_count);
    }

    printf("%s", prt_str);
}

 #ifndef RM_MAP_PERSISTANT_W
int ra6w1_printenv (const char * groupname, int printall)
{
    rm_vee_group_instance_t * p_vee;
    int  used_recs  = 0;
    int  total_recs = 0;
    int  status     = TRUE;
    char delimeterspace[DELIMETER_MAXLEN + 1];

    if (groupname == NULL)
    {
        return FALSE;
    }

    ra6w1_environ_lock(true);

    p_vee = vee_nvparam_open(groupname);

    if (p_vee == NULL)
    {
        ENVRION_ERROR("vee_nvparam is null(%s)\n", (groupname));
        ra6w1_environ_lock(false);

        return FALSE;
    }

    uint32_t gid = find_group_id(p_vee->area);

    ENVRION_PRINT("%s(gid:%03lu):\n\n", groupname, gid);

    total_recs = p_vee->area->num_parameters;

    if (printall)
    {
        ENVRION_PRINT("\tTag_Name(ID)                                 [Used/Max/WriteLen] [Data]\n");
        ENVRION_PRINT("\t");
        print_separate_bar('-', 80, 1);
    }
    else
    {
        ENVRION_PRINT("\tTag_Name(ID)                                 [Used/MaxLen] [Data]\n");
        ENVRION_PRINT("\t");
        print_separate_bar('-', 70, 1);
    }

    for (size_t i = 0; i < p_vee->area->num_parameters; i++)
    {
        parameter_t * param  = &(p_vee->area->parameters[i]);
        uint32_t      rec_id = gid + i;
        uint8_t     * stored_data;
        uint32_t      stored_len;

        fsp_err_t err = p_vee->g_vee->p_api->recordPtrGet(p_vee->g_vee->p_ctrl, rec_id, &stored_data, &stored_len);

        uint32_t name_length = strlen(param->name);
        uint32_t idx         = 0;

        while (idx < (DELIMETER_MAXLEN - name_length))
        {
            delimeterspace[idx] = '.';
            idx++;
        }

        if (rec_id > 1000)
        {
            idx--;
        }

        delimeterspace[idx] = '\0';

        if (FSP_ERR_NOT_FOUND == err)
        {
            if (param->attr.flags == FLAG_VARIABLE_LEN)
            {
                if (printall)
                {
                    ENVRION_PRINT("\t%s(%03lu) %s [00/%02u/%02u] <Empty, No Rec>\n",
                                  param->name,
                                  rec_id,
                                  delimeterspace,
                                  (param->attr.length - PARAM_STR_EXTRA),
                                       /* flash write len */
                                  DATA_ALIGNMENT(param->attr.length));
                }
            }
            else
            {
                if (printall)
                {
                    char data_type[4]; // u32
                    if (param->attr.length == 1)
                    {
                        strcpy(data_type, " u8");
                    }
                    else if (param->attr.length == 2)
                    {
                        strcpy(data_type, "u16");
                    }
                    else
                    {
                        strcpy(data_type, "u32");
                    }

                    ENVRION_PRINT("\t%s(%03lu) %s [ %s /%02u] <Empty, No Rec>\n",
                                  param->name,
                                  rec_id,
                                  delimeterspace,
                                  data_type,
                                       /* flash write len */
                                  DATA_ALIGNMENT(param->attr.length));
                }
            }

            continue;
        }
        else if (FSP_SUCCESS != err)
        {
            status = FALSE;
            break;
        }

        uint16_t lenfield = *(uint16_t *) (&(stored_data[stored_len - sizeof(uint16_t)]));

        if (FLAG_VARIABLE_LEN == (param->attr.flags)) // String Type
        {
            if (lenfield == (0xFFFFU))
            {
                if (printall)
                {
                    ENVRION_PRINT("\t%s(%03lu) %s [%02u/%02u/%02u] <Empty, Erased>\n",
                                  param->name,
                                  rec_id,
                                  delimeterspace,
                                  0,
                                  (param->attr.length - PARAM_STR_EXTRA),
                                  DATA_ALIGNMENT(param->attr.length));
                }
            }
            else
            {
                lenfield -= STR_END;
                used_recs++;
                if (printall)
                {
                    ENVRION_PRINT("\t%s(%03lu) %s [%02u/%02u/%02u]", param->name, rec_id, delimeterspace, lenfield,
                                  (param->attr.length - PARAM_STR_EXTRA), DATA_ALIGNMENT(param->attr.length));
                }
                else
                {
                    ENVRION_PRINT("\t%s(%03lu) %s [%02u/%02u]", param->name, rec_id, delimeterspace, lenfield,
                                  (param->attr.length - PARAM_STR_EXTRA));
                }

                ENVRION_PRINT(" %s\n", (char *) (&(stored_data[0])));
            }
        }
        else                           // bin type (uint8, untt16, uint32)
        {
            bin_value_t * bin_value = (bin_value_t *) stored_data;

            switch (param->attr.length)
            {
                case sizeof(uint8_t):
                {
                    if (lenfield == (0xFFFFU))
                    {
                        ENVRION_PRINT_LVL1(printall,
                                           "\t%s(%03lu) %s [u8/%lu]  <Empty, Erased>\n",
                                           param->name,
                                           rec_id,
                                           delimeterspace,
                                           stored_len);
                    }
                    else
                    {
                        used_recs++;
                        if (printall)
                        {
                            ENVRION_PRINT("\t%s(%03lu) %s [u8/%lu]  %u(0x%x)\n",
                                          param->name,
                                          rec_id,
                                          delimeterspace,
                                          stored_len,
                                          bin_value->val8,
                                          bin_value->val8);
                        }
                        else
                        {
                            ENVRION_PRINT("\t%s(%03lu) %s [u8/%u] %u\n",
                                          param->name,
                                          rec_id,
                                          delimeterspace,
                                          param->attr.length,
                                          bin_value->val8);
                        }
                    }

                    break;
                }

                case sizeof(uint16_t):
                {
                    if (lenfield == (0xFFFFU))
                    {
                        if (printall)
                        {
                            ENVRION_PRINT("\t%s(%03lu) %s [u16/%lu]  <Empty, Erased>\n",
                                          param->name,
                                          rec_id,
                                          delimeterspace,
                                          stored_len);
                        }
                    }
                    else
                    {
                        used_recs++;
                        if (printall)
                        {
                            ENVRION_PRINT("\t%s(%lu) %s [u16/%lu]  %u(0x%x)\n",
                                          param->name,
                                          rec_id,
                                          delimeterspace,
                                          stored_len,
                                          bin_value->val16,
                                          bin_value->val16);
                        }
                        else
                        {
                            ENVRION_PRINT("\t%s(%lu) %s [u16/%u] %u\n",
                                          param->name,
                                          rec_id,
                                          delimeterspace,
                                          param->attr.length,
                                          bin_value->val16);
                        }
                    }

                    break;
                }

                default:
                {
                    if (lenfield == (0xFFFFU))
                    {
                        if (printall)
                        {
                            ENVRION_PRINT("\t%s(%03lu) %s [u32/%lu]  <Empty, Erased>\n",
                                          param->name,
                                          rec_id,
                                          delimeterspace,
                                          stored_len);
                        }
                    }
                    else
                    {
                        used_recs++;
                        if (!printall)
                        {
                            ENVRION_PRINT("\t%s(%03lu) %s [u32/%u] %lu\n",
                                          param->name,
                                          rec_id,
                                          delimeterspace,
                                          param->attr.length,
                                          bin_value->val32);
                        }
                        else
                        {
                            ENVRION_PRINT("\t%s(%03lu) %s [u32/%lu]  %lu(0x%lx)\n",
                                          param->name,
                                          rec_id,
                                          delimeterspace,
                                          stored_len,
                                          bin_value->val32,
                                          bin_value->val32);
                        }
                    }

                    break;
                }
            }
        }
    }

    if ((used_recs == 0) && !printall)
    {
        ENVRION_PRINT("\t<Empty>\n");
    }

    // Used Records / Total Records
    ENVRION_PRINT("Total %d/%d\n\n\n", used_recs, total_recs);

    if ((strcmp(ENV_GROUP_APPCFG, groupname) == 0) && printall)
    {
        rm_vee_status_t vee_status;
        p_vee->g_vee->p_api->statusGet(p_vee->g_vee->p_ctrl, &vee_status);

        if (printall)
        {
            ENVRION_PRINT("VEE: last_id - %lu\n", vee_status.last_id);
            ENVRION_PRINT("VEE: segment_erase_count - %lu\n", vee_status.segment_erase_count);
            ENVRION_PRINT("VEE: space_available - %lu\n", vee_status.space_available);
        }
    }

    vee_nvparam_close(p_vee);

    ra6w1_environ_lock(false);

    return status;
}

 #endif

 #ifndef RM_MAP_PERSISTANT_W
int ra6w1_nvram_refresh (void)
{
    rm_vee_group_instance_t * rm_vee_param;

    if (rm_vee_flash_w_multi_use == 0)
    {
        return FALSE;
    }

    ra6w1_environ_lock(true);

    rm_vee_param = vee_nvparam_open(NULL);

    if (rm_vee_param == NULL)
    {
        ENVRION_ERROR("[%s] vee_nvparam is null\n", __func__);
        ra6w1_environ_lock(false);

        return FALSE;
    }

    ENVRION_DEBUG("[%s]\n", __func__);
    vee_nvparam_refresh(rm_vee_param);
    vee_nvparam_close(rm_vee_param);
    ra6w1_environ_lock(false);

    return TRUE;
}

 #endif
 #ifndef RM_MAP_PERSISTANT_W
int ra6w1_setenv_bin (const char * groupname, const char * envname, void * value)
{
    rm_vee_group_instance_t * rm_vee_param;
    uint16_t data_length;

    if (groupname == NULL)
    {
        ENVRION_ERROR("[%s] (group=NULL->%s)\n", __func__, envname);

        return FALSE;
    }

    if (envname == NULL)
    {
        ENVRION_ERROR("[%s] (%s->envname=NULL)\n", __func__, groupname);

        return FALSE;
    }

    if (value == NULL)
    {
        ENVRION_DEBUG("[%s] (%s->%s) data=null\n", __func__, groupname, envname);

        return TRUE;
    }

    ra6w1_environ_lock(true);

    rm_vee_param = vee_nvparam_open(groupname);

    if (rm_vee_param == NULL)
    {
        ENVRION_ERROR("[%s] (%s->%s) vee_nvparam is null\n", __func__, groupname, envname);
        ra6w1_environ_lock(false);

        return FALSE;
    }

    data_length = vee_nvparam_write_offset_by_name(rm_vee_param, envname, AS_INT, 0, value);

    vee_nvparam_close(rm_vee_param);

    if (data_length == 0)
    {
        ENVRION_ERROR("[%s] (%s->%s) vee_nvparam error\n", __func__, groupname, envname);
        ra6w1_environ_lock(false);

        return FALSE;
    }

    ra6w1_environ_lock(false);

    return TRUE;
}

int ra6w1_getenv_bin (const char * groupname, const char * envname, void * value, unsigned long len)
{
    FSP_PARAMETER_NOT_USED(len);

    rm_vee_group_instance_t * rm_vee_param;
    uint16_t      data_length;
    uint8_t     * vee_ro_data = NULL;
    uint8_t       flag        = 0;
    bin_value_t * bin_value;

    if ((groupname == NULL) || (envname == NULL) || (value == NULL))
    {
        return 0;
    }

    ra6w1_environ_lock(true);

    rm_vee_param = vee_nvparam_open(groupname);

    if (rm_vee_param == NULL)
    {
        ENVRION_DEBUG("[%s] (%s->%s) vee_nvparam_open is null\n", __func__, groupname, envname);
        ra6w1_environ_lock(false);

        return 0;
    }

    data_length = vee_nvparam_read_offset_by_name(rm_vee_param, envname, 0, (void **) (&vee_ro_data), &flag);

    vee_nvparam_close(rm_vee_param);

    if (data_length == 0)
    {
        ENVRION_DEBUG("[%s] (%s->%s) vee_nvparam : Empty(data_length=0)\n", __func__, groupname, envname);
        ra6w1_environ_lock(false);

        return 0;
    }

    if (0 != (flag & FLAG_VARIABLE_LEN)) /* 0 != (0&1) */
    {
        ENVRION_DEBUG("[%s] (%s->%s) nvparam does not match(string type).\n", __func__, groupname, envname);
        ra6w1_environ_lock(false);

        return 0;
    }

    bin_value = (bin_value_t *) vee_ro_data;

    switch (data_length)
    {
        case sizeof(uint8_t):
        {
            *((uint8_t *) value) = bin_value->val8;
            ENVRION_DEBUG("[%s] (%s->%s)  u8 %d\n", __func__, groupname, envname, *(uint8_t *) value);
            break;
        }

        case sizeof(uint16_t):
        {
            *((uint16_t *) value) = bin_value->val16;
            ENVRION_DEBUG("[%s] (%s->%s)  u16 %d\n", __func__, groupname, envname, *(uint16_t *) value);
            break;
        }

        default:
        {
            *((uint32_t *) value) = bin_value->val32;
            ENVRION_DEBUG("[%s] (%s->%s)  u32 %ld\n", __func__, groupname, envname, *(uint32_t *) value);
            break;
        }
    }

    ra6w1_environ_lock(false);

    return data_length;
}

 #endif

/***********************************************************************************************************************
 * Private global variables and functions
 **********************************************************************************************************************/
const parameter_t * find_parameter_by_name (const area_t * area, char * name, uint16_t * rindex)
{
    int i;

    for (i = 0; i < (int) area->num_parameters; i++)
    {
        if (strcmp(area->parameters[i].name, name) == 0)
        {
            *rindex = (uint16_t) i;

            return &area->parameters[i];
        }
    }

    return NULL;
}

uint32_t find_group_id (const area_t * area)
{
    int i;

    for (i = 0; i < (int) num_areas; i++)
    {
        if (area_group_list[i].area == area)
        {
            return area_group_list[i].id;
        }
    }

    return AREA_ID_UNKNOWN;
}

void find_group_list (void)
{
    uint16 total = 0;

    ENVRION_PRINT("group list:\n");

    for (int i = 0; i < (int) num_areas; i++)
    {
        ENVRION_PRINT("\t%7s (ID: %4lu ~ %4lu), %3u tags\n",
                      area_group_list[i].area->name,
                      area_group_list[i].id,
                      (uint32) (area_group_list[i].id + area_group_list[i].area->num_parameters - 1),
                      area_group_list[i].area->num_parameters);
        total = (uint16) (total + area_group_list[i].area->num_parameters);
    }

    ENVRION_PRINT("\n\tTotal: %u tags\n", total);
}

rm_vee_group_instance_t * vee_nvparam_open (const char * area_name)
{
    int            i;
    const area_t * area = NULL;
    const char   * p_areaname;

    if (0 == rm_vee_flash_w_multi_use)
    {
        // 1st group
        area_group_list[0].area  = (area_t *) (&areas[0]); // bootcfg
        area_group_list[0].id    = BOOTCFG_ID;
        area_group_list[0].g_vee = NULL;

        // 2nd group ~
        for (i = 1; i < (int) num_areas; i++)
        {
            area_group_list[i].area = (area_t *) (&areas[i]);

            switch (i)
            {
                case 1:                // devcfg
                {
                    area_group_list[i].id = GTAG_ALIGNMENT(DEVCFG_ID);
                    break;
                }

                case 2:                // wificfg
                {
                    area_group_list[i].id = GTAG_ALIGNMENT(WIFICFG_ID);
                    break;
                }

                case 3:                // syscfg
                {
                    area_group_list[i].id = GTAG_ALIGNMENT(SYSCFG_ID);
                    break;
                }

                case 4:                // appcfg
                {
                    area_group_list[i].id = GTAG_ALIGNMENT(APPCFG_ID);
                    break;
                }

                case 5:                // blecfg
                {
                    area_group_list[i].id = GTAG_ALIGNMENT(BLECFG_ID);
                    break;
                }

                case 6:                // blesec
                {
                    area_group_list[i].id = GTAG_ALIGNMENT(BLESEC_ID);
                    break;
                }

                case 7:                // testcfg
                {
                    area_group_list[i].id = GTAG_ALIGNMENT(TESTCFG_ID);
                    break;
                }

                case 8:                // wifiprofilecfg
                {
                    area_group_list[i].id = GTAG_ALIGNMENT(WIFIPROFILE_ID);
                    break;
                }
            }

            area_group_list[i].g_vee = NULL;
        }
    }

    if (NULL == area_name)
    {
        p_areaname = areas[0].name;
    }
    else
    {
        p_areaname = area_name;
    }

    for (i = 0; i < (int) num_areas; i++)
    {
        if (!strcmp(areas[i].name, p_areaname))
        {
            area = &areas[i];
            break;
        }
    }

    if (!area)
    {
        return NULL;
    }

    if (0 == rm_vee_flash_w_multi_use)
    {
        fsp_err_t err;
        err = g_vee0.p_api->open(g_vee0.p_ctrl, g_vee0.p_cfg);
        FSP_ERROR_RETURN(FSP_SUCCESS == err, NULL);

        area_group_list[i].g_vee = &g_vee0;
        rm_vee_flash_w_multi_use++;
    }
    else
    {
        area_group_list[i].g_vee = &g_vee0;
        rm_vee_flash_w_multi_use++;
    }

    return &(area_group_list[i]);
}

void vee_nvparam_close (rm_vee_group_instance_t * p_vee)
{
    if (NULL != p_vee->g_vee)
    {
        rm_vee_flash_w_multi_use--;
        if (0 == rm_vee_flash_w_multi_use)
        {
            g_vee0.p_api->close(g_vee0.p_ctrl);
        }

        p_vee->g_vee = NULL;
    }
}

uint32_t vee_nvparam_read_offset_by_name (rm_vee_group_instance_t * p_vee,
                                          const char              * name,
                                          uint16_t                  offset,
                                          void                   ** data,
                                          uint8_t                 * flag)
{
    uint16_t  recidx = 0;
    uint32_t  gid;
    uint32_t  rec_id;
    uint8_t * stored_data;
    uint32_t  stored_len;
    fsp_err_t err;
    uint16_t  lenfield;

    if ((NULL == p_vee) || (NULL == p_vee->g_vee) || (NULL == name) || (NULL == data))
    {
        return 0;
    }

    *data = NULL;

    parameter_t * param = (parameter_t *) find_parameter_by_name(p_vee->area, (char *) name, &recidx);
    if (NULL == param)
    {
        return 0;
    }

    gid    = find_group_id(p_vee->area);
    rec_id = gid + recidx;

    err = p_vee->g_vee->p_api->recordPtrGet(p_vee->g_vee->p_ctrl, rec_id, &stored_data, &stored_len);
    if (FSP_SUCCESS != err)
    {
        ENVRION_DEBUG(ANSI_COLOR_LIGHT_RED "\t[%s] '%s' read err=%d\n" ANSI_COLOR_DEFULT, __func__, name, err);

        return 0;
    }

    lenfield = *(uint16_t *) (&(stored_data[stored_len - sizeof(uint16_t)]));

    if (lenfield == (0xFFFFU))
    {
        ENVRION_DEBUG("\t[%s] '%s' record empty\n", __func__, name);

        return 0;
    }

    if (offset != 0)
    {
        if (offset >= lenfield)
        {
            ENVRION_DEBUG("\t[%s] '%s' data overflow (off:%u, len:%u))\n", __func__, name, offset, lenfield);

            return 0;
        }

        lenfield -= offset;
    }

    *data = &(stored_data[offset]);
    *flag = param->attr.flags;

    return lenfield;
}

uint16_t vee_nvparam_write_offset_by_name (rm_vee_group_instance_t * p_vee,
                                           const char              * name,
                                           uint16_t                  is_string,
                                           uint16_t                  offset,
                                           const void              * data)
{
    uint16_t        recidx = 0;
    parameter_t   * param;
    uint32_t        gid;
    uint32_t        rec_id;
    uint8_t       * stored_data;
    uint32_t        stored_len;
    uint16_t        length;
    uint16_t        maxlength;
    uint16_t        lenfield = 0;
    fsp_err_t       err;
    uint16_t      * tmprecdata;
    rm_vee_status_t vee_status;
    int             retry_cnt = 0;
    int32_t         int_data;

    if ((NULL == p_vee) || (NULL == p_vee->g_vee))
    {
        ENVRION_ERROR(ANSI_COLOR_LIGHT_RED "\t[%s] p_vee or g_vee NULL, name=%s\n" ANSI_COLOR_DEFULT, __func__, name);

        return 0;
    }
    else if (NULL == name)
    {
        ENVRION_ERROR(ANSI_COLOR_LIGHT_RED "\t[%s] name=NULL, data=%s\n" ANSI_COLOR_DEFULT,
                      __func__,
                      (const char *) data);

        return 0;
    }
    else if (NULL == data)
    {
        ENVRION_ERROR(ANSI_COLOR_LIGHT_RED "\t[%s] data=NULL, name=%s\n" ANSI_COLOR_DEFULT, __func__, name);

        return 0;
    }

    param = (parameter_t *) find_parameter_by_name(p_vee->area, (char *) name, &recidx);
    if (NULL == param)
    {
        ENVRION_ERROR("\t[%s] '%s' not found\n", __func__, name);

        return 0;
    }

    gid       = find_group_id(p_vee->area);
    rec_id    = gid + recidx;
    maxlength = (uint16_t) DATA_ALIGNMENT(param->attr.length);

    if (FLAG_VARIABLE_LEN != (param->attr.flags)) // bin type
    {
        maxlength += sizeof(uint16_t);
        length     = param->attr.length;
        if (is_string == AS_STRING)               // int written as string
        {
            int_data = atoi((char *) data);
            data     = &int_data;
        }
    }
    else                               // String Type
    {
        length = strlen((char *) data) + 1;
        if (DATA_ALIGNMENT(length + sizeof(uint16_t)) > maxlength)
        {
            ENVRION_ERROR("\t[%s] length mismatch error (len:%u, max:%u)\n", __func__,
                          DATA_ALIGNMENT(length + sizeof(uint16_t)), maxlength);

            return 0;
        }

        /* It is not compatible with the ad_nvpram,
         * but it will make you free from the length limitation of ad_nvparam.*/
        maxlength += sizeof(uint16_t);
    }

    if (maxlength < (offset + length + sizeof(uint16_t)))
    {
        ENVRION_ERROR("\t[%s] '%s' length over\n", __func__, param->name);

        return 0;
    }

    err = p_vee->g_vee->p_api->recordPtrGet(p_vee->g_vee->p_ctrl, rec_id, &stored_data, &stored_len);

    if (FSP_SUCCESS == err)
    {
        lenfield = *(uint16_t *) (&(stored_data[stored_len - sizeof(uint16_t)]));

        if (lenfield == (0xFFFFU))
        {
            ENVRION_DEBUG("\t[%s] '%s' record empty\n", __func__, name);
        }
        else if (lenfield > maxlength)
        {
            ENVRION_ERROR("\t[%s] Read '%s' length error=%d (lenfield:%u, max:%u)\n",
                          __func__,
                          param->name,
                          err,
                          lenfield,
                          maxlength);

            return 0;
        }
    }
    else if (FSP_ERR_NOT_FOUND != err)
    {
        ENVRION_ERROR("\t[%s] Read '%s' vee_nvparam error (err=%d)\n", __func__, param->name, err);

        return 0;
    }
    else
    {
        ENVRION_DEBUG("\t[%s] Read '%s' error=%d length=%d (lenfield:%u,max:%u)\n",
                      __func__,
                      param->name,
                      err,
                      length,
                      lenfield,
                      maxlength);
    }

    // Check whether the value is the same as the original data.
    {
 #if 0                                 // Debug
        if (lenfield == 0x0)
        {
            ENVRION_DEBUG("[%s] %s current Empty(No rec)\n", __func__, name);
        }
        else if (lenfield == 0xFFFFU)
        {
            ENVRION_DEBUG("[%s] %s current Empty(Rec)\n", __func__, name);
        }
 #endif                                /* 0 */

        if ((lenfield == length) && (memcmp(stored_data, data, length) == 0))
        {
            ENVRION_DEBUG(ANSI_COLOR_LIGHT_GREEN "\t[%s] Write Skip: Same as original data. %s=" ANSI_COLOR_DEFULT,
                          __func__,
                          name);

            if (param->attr.flags != FLAG_VARIABLE_LEN)
            {
 #if 0                                 // Debug
                bin_value_t * bin_value = (bin_value_t *) data;

                switch (param->attr.length)
                {
                    case sizeof(uint8_t):
                    {
                        ENVRION_DEBUG("u8 %d", bin_value->val8);
                        break;
                    }

                    case sizeof(uint16_t):
                    {
                        ENVRION_DEBUG("u16 %d", bin_value->val16);
                        break;
                    }

                    default:
                    {
                        ENVRION_DEBUG("u32 %ld", bin_value->val32);
                        break;
                    }
                }
 #endif                                /* 0 */
            }
            else
            {
                ENVRION_DEBUG("|%s|", (const char *) data);
            }

            ENVRION_DEBUG("\n" ANSI_COLOR_DEFULT);

            return length;
        }
    }

    stored_len = maxlength;

    memset(&(rm_vee_flash_w_buffer[0]), 0xFF, maxlength);
    memcpy(&(rm_vee_flash_w_buffer[offset]), data, length);

    tmprecdata  = (uint16_t *) (&(rm_vee_flash_w_buffer[stored_len - sizeof(uint16_t)]));
    *tmprecdata = length;

    ENVRION_DEBUG(ANSI_COLOR_LIGHT_CYAN "\t[%s] Real Write %s rid=%lu data[%lu] := %u " ANSI_COLOR_DEFULT,
                  __func__,
                  param->name,
                  rec_id,
                  (stored_len - sizeof(uint16_t)),
                  *tmprecdata);

    if (param->attr.flags != FLAG_VARIABLE_LEN)
    {
 #if 0                                 // Debug
        bin_value_t * bin_value = (bin_value_t *) data;

        switch (param->attr.length)
        {
            case sizeof(uint8_t):
            {
                ENVRION_DEBUG(" u8 %d", bin_value->val8);
                break;
            }

            case sizeof(uint16_t):
            {
                ENVRION_DEBUG("u16 %d", bin_value->val16);
                break;
            }

            default:
            {
                ENVRION_DEBUG("u32 %ld", bin_value->val32);
                break;
            }
        }
 #endif                                /* 0 */
    }
    else
    {
        ENVRION_DEBUG("|%s|", (char *) rm_vee_flash_w_buffer);
    }

    ENVRION_DEBUG("\n" ANSI_COLOR_DEFULT);

    // Writing progress.
    retry_cnt = 0;
    do
    {
 #if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
        R_WDOG_W_Freeze(g_wifi_cfg.p_watchdog_service->p_cfg->p_wdt->p_ctrl, true);
 #endif
        err = p_vee->g_vee->p_api->recordWrite(p_vee->g_vee->p_ctrl, rec_id, rm_vee_flash_w_buffer, stored_len);
 #if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
        R_WDOG_W_Freeze(g_wifi_cfg.p_watchdog_service->p_cfg->p_wdt->p_ctrl, false);
 #endif
        if ((FSP_ERR_IN_USE == err) || (FSP_ERR_TIMEOUT == err))
        {
            // Failed to start writing.(Wait and retry)
            vTaskDelay(portCONVERT_MS_2_TICKS(100));
            retry_cnt++;
            if (retry_cnt >= 50)       // about 5sec.
            {
                ENVRION_ERROR(ANSI_COLOR_LIGHT_RED "\t[%s] %s Write start err=%u\n" ANSI_COLOR_DEFULT,
                              __func__,
                              name,
                              err);

                return 0;
            }

            ENVRION_DEBUG(ANSI_COLOR_LIGHT_YELLOW "\t[%s] %s Retry(%d) Write err=%u\n" ANSI_COLOR_DEFULT,
                          __func__,
                          name,
                          retry_cnt,
                          err);
        }
        else if (FSP_SUCCESS != err)
        {
            // Write failed.
            ENVRION_ERROR(ANSI_COLOR_LIGHT_RED "\t[%s] %s Write Failed err=%u\n" ANSI_COLOR_DEFULT, __func__, name,
                          err);

            return 0;
        }
    } while (FSP_SUCCESS != err);

    err = p_vee->g_vee->p_api->statusGet(p_vee->g_vee->p_ctrl, &vee_status);

    // Check writing progress.
    retry_cnt = 0;
    do
    {
        err = p_vee->g_vee->p_api->statusGet(p_vee->g_vee->p_ctrl, &vee_status);
        if ((vee_status.state == RM_VEE_STATE_BUSY) || (vee_status.state == RM_VEE_STATE_REFRESH))
        {
            ENVRION_DEBUG(ANSI_COLOR_LIGHT_YELLOW "\t[%s] Wait for Writing %s state=%u\n" ANSI_COLOR_DEFULT,
                          __func__,
                          name,
                          vee_status.state);
            vTaskDelay(portCONVERT_MS_2_TICKS(100));
            retry_cnt++;
            if (retry_cnt >= 50)       // about 5sec.
            {
                ENVRION_ERROR(ANSI_COLOR_LIGHT_YELLOW "\t[%s] %s Write progress state=%u cnt=%d\n" ANSI_COLOR_DEFULT,
                              __func__,
                              name,
                              vee_status.state,
                              retry_cnt);

                return 0;
            }
        }
    } while (vee_status.state != RM_VEE_STATE_READY &&
             vee_status.state != RM_VEE_STATE_OVERFLOW &&
             vee_status.state != RM_VEE_STATE_HARDWARE_FAIL);

    stored_len = 0;
    if (vee_status.state == RM_VEE_STATE_READY)
    {
        stored_len = length;
    }
    else
    {
        ENVRION_ERROR(ANSI_COLOR_LIGHT_RED "\t[%s] %s state=%u stored_len=%ld length=%d\n" ANSI_COLOR_DEFULT,
                      __func__,
                      name,
                      vee_status.state,
                      stored_len,
                      length);
    }

    return (uint16_t) stored_len;
}

uint16_t vee_nvparam_erase_by_name (rm_vee_group_instance_t * p_vee, const char * name)
{
    uint16_t      recidx;
    char        * wildcard_flg      = NULL;
    int           prefix_search_len = 0;
    int           i                 = 0;
    parameter_t * param             = NULL;

    uint32_t  gid = 0;
    uint32_t  rec_id;
    uint8_t * stored_data;
    uint32_t  stored_len = 0;

    // uint32_t  return_len = 0;
    rm_vee_status_t vee_status;
    size_t          size;
    fsp_err_t       err;

    if ((NULL == p_vee) || (NULL == p_vee->g_vee) || (NULL == name))
    {
        return pdFAIL;
    }

    wildcard_flg = strstr(name, "*");

    if (wildcard_flg)
    {
        prefix_search_len = (wildcard_flg - name);
        if (prefix_search_len == 0)
        {
            return pdFAIL;
        }
    }

    do
    {
        if (wildcard_flg)
        {
            if (strncmp(p_vee->area->parameters[i].name, name, (size_t) prefix_search_len) == 0)
            {
                param = &p_vee->area->parameters[i];
                if (NULL == param)
                {
                    ENVRION_DEBUG("\t[%s] name=%s rid=%lu <No Rec>\n", __func__, p_vee->area->parameters[i].name,
                                  (gid + i));
                    i++;
                    continue;
                }

                recidx = (uint16_t) i;
            }
            else
            {
                i++;
                continue;
            }
        }
        else
        {
            param = (parameter_t *) find_parameter_by_name(p_vee->area, (char *) name, &recidx);
            if (NULL == param)
            {
                ENVRION_DEBUG("\t[%s] name=%s rid=%u <No Rec>\n", __func__, name, recidx);

                return pdPASS;
            }
        }

        gid    = find_group_id(p_vee->area);
        rec_id = gid + recidx;

        err = p_vee->g_vee->p_api->recordPtrGet(p_vee->g_vee->p_ctrl, rec_id, &stored_data, &stored_len);

        if (FSP_SUCCESS != err)
        {
            if (FSP_ERR_NOT_FOUND == err)
            {
                ENVRION_DEBUG("\t[%s] Erase Skip: %s(%s) Already Empty(No rec) err=%u\n",
                              __func__,
                              name,
                              p_vee->area->parameters[wildcard_flg ? i : recidx].name,
                              err);
            }
            else
            {
                ENVRION_ERROR(ANSI_COLOR_LIGHT_RED "\t[%s] Erase Error: %s(%s) err=%u\n" ANSI_COLOR_DEFULT,
                              __func__,
                              name,
                              p_vee->area->parameters[wildcard_flg ? i : recidx].name,
                              err);
            }

            if (wildcard_flg)
            {
                i++;
                continue;
            }
            else
            {
                return pdFAIL;
            }
        }

 #if 1                                 // Check Empty
        uint16_t lenfield;
        lenfield = *(uint16_t *) (&(stored_data[stored_len - sizeof(uint16_t)]));

        if ((lenfield == (0xFFFFU)) || (lenfield == (0x0)))
        {
            if (lenfield == (0x0))
            {
                ENVRION_DEBUG("\t[%s] Erase Skip: %s(%s) Already Empty(No rec)\n",
                              __func__,
                              name,
                              p_vee->area->parameters[i].name);
            }
            else if (lenfield == (0xFFFFU))
            {
                ENVRION_DEBUG("\t[%s] Erase Skip: %s(%s) Already Empty(Rec)\n", __func__, name,
                              p_vee->area->parameters[i].name);
            }
            else
            {
                ENVRION_DEBUG("\t[%s] Erase %s(%s)\n", __func__, name, p_vee->area->parameters[i].name);
            }

            if (wildcard_flg)
            {
                i++;
                continue;
            }
            else
            {
                return pdPASS;
            }
        }
 #endif                                /* 1 */
        size = DATA_ALIGNMENT(param->attr.length + sizeof(uint16_t));
        ENVRION_DEBUG(ANSI_COLOR_LIGHT_CYAN "\t[%s] NVRAM Erase %s - size=%d\n" ANSI_COLOR_DEFULT,
                      __func__,
                      param->name,
                      size);
        memset(rm_vee_flash_w_buffer, 0xFF, size);
        ENVRION_DEBUG("\t[%s] %s(%s) rid=%lu <Empty Rec>\n", __func__, name, p_vee->area->parameters[i].name, rec_id);
 #if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
        R_WDOG_W_Freeze(g_wifi_cfg.p_watchdog_service->p_cfg->p_wdt->p_ctrl, true);
 #endif
        err = p_vee->g_vee->p_api->recordWrite(p_vee->g_vee->p_ctrl, rec_id, rm_vee_flash_w_buffer, size);
 #if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
        R_WDOG_W_Freeze(g_wifi_cfg.p_watchdog_service->p_cfg->p_wdt->p_ctrl, false);
 #endif
        ENVRION_DEBUG("\t[%s] Erase %s(%s) err=%u\n", __func__, name, p_vee->area->parameters[i].name, err);
        err = p_vee->g_vee->p_api->statusGet(p_vee->g_vee->p_ctrl, &vee_status);
        do
        {
            err = p_vee->g_vee->p_api->statusGet(p_vee->g_vee->p_ctrl, &vee_status);
            if ((vee_status.state == RM_VEE_STATE_BUSY) || (vee_status.state == RM_VEE_STATE_REFRESH))
            {
                ENVRION_DEBUG(ANSI_COLOR_LIGHT_YELLOW "\t[%s] Wait for Erasing %s state=%u\n" ANSI_COLOR_DEFULT,
                              __func__,
                              p_vee->area->parameters[i].name,
                              vee_status.state);
                vTaskDelay(portCONVERT_MS_2_TICKS(100));
            }
        } while (vee_status.state != RM_VEE_STATE_READY &&
                 vee_status.state != RM_VEE_STATE_OVERFLOW &&
                 vee_status.state != RM_VEE_STATE_HARDWARE_FAIL);

        if (vee_status.state != RM_VEE_STATE_READY)
        {
            ENVRION_ERROR(ANSI_COLOR_LIGHT_RED "\t[%s] Erase Error %s state=%u\n" ANSI_COLOR_DEFULT,
                          __func__,
                          p_vee->area->parameters[i].name,
                          vee_status.state);
        }

        i++;
    } while (wildcard_flg && (i < (int) p_vee->area->num_parameters));

    return pdPASS;
}

void vee_nvparam_erase_all (rm_vee_group_instance_t * p_vee)
{
    fsp_err_t       err;
    uint32_t        rec_id = 0;
    size_t          size   = 0;
    uint8_t       * stored_data;
    uint32_t        stored_len = 0;
    rm_vee_status_t vee_status;

    if ((NULL == p_vee) || (NULL == p_vee->g_vee))
    {
        return;
    }

    memset(rm_vee_flash_w_buffer, 0xFF, RM_VEE_FLASH_W_REF_DATA_SIZE);

    for (size_t i = 0; i < p_vee->area->num_parameters; i++)
    {
        uint32_t gid = find_group_id(p_vee->area);

        if (gid == AREA_ID_UNKNOWN)
        {
            break;
        }

        rec_id = gid + i;
        size   = DATA_ALIGNMENT(p_vee->area->parameters[i].attr.length + sizeof(uint16_t));

        ENVRION_DEBUG("[%s] gid=%lu rid=%lu - '%s'\n", __func__, gid, rec_id, p_vee->area->parameters[i].name);

 #if 1                                 // Only recorded data is erase.
        err = p_vee->g_vee->p_api->recordPtrGet(p_vee->g_vee->p_ctrl, rec_id, &stored_data, &stored_len);
        if (FSP_SUCCESS != err)
        {
            if (err == FSP_ERR_NOT_FOUND)
            {
                ENVRION_DEBUG("[%s] Erase Skip: %s(rid=%lu) Already Empty(No Rec) err=%u Not Found\n",
                              __func__,
                              p_vee->area->parameters[i].name,
                              rec_id,
                              err);
            }
            else
            {
                ENVRION_ERROR("\t[%s] Erase Error: %s(rid=%lu) err=%u\n",
                              __func__,
                              p_vee->area->parameters[i].name,
                              rec_id,
                              err);
            }

            continue;
        }

        // Check Empty
        uint16_t lenfield;
        lenfield = *(uint16_t *) (&(stored_data[stored_len - sizeof(uint16_t)]));

        if ((lenfield == (0xFFFFU)) || (lenfield == (0x0)))
        {
            if (lenfield == (0x0))
            {
                ENVRION_DEBUG("\t[%s] Erase Skip: %s(rid=%lu) Already Empty(No rec)\n",
                              __func__,
                              p_vee->area->parameters[i].name,
                              rec_id);
            }
            else if (lenfield == (0xFFFFU))
            {
                ENVRION_DEBUG("\t[%s] Erase Skip: %s(rid=%lu) Already Empty(Rec)\n",
                              __func__,
                              p_vee->area->parameters[i].name,
                              rec_id);
            }

            continue;
        }
 #endif                                /* 1 */

        ENVRION_DEBUG(ANSI_COLOR_LIGHT_CYAN "\t[%s] Real Erase '%s' rid=%lu\n" ANSI_COLOR_DEFULT,
                      __func__,
                      p_vee->area->parameters[i].name,
                      rec_id);
 #if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
        R_WDOG_W_Freeze(g_wifi_cfg.p_watchdog_service->p_cfg->p_wdt->p_ctrl, true);
 #endif
        err = p_vee->g_vee->p_api->recordWrite(p_vee->g_vee->p_ctrl, rec_id, rm_vee_flash_w_buffer, size);
 #if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
        R_WDOG_W_Freeze(g_wifi_cfg.p_watchdog_service->p_cfg->p_wdt->p_ctrl, false);
 #endif
        if (FSP_SUCCESS != err)
        {
            return;
        }

        err = p_vee->g_vee->p_api->statusGet(p_vee->g_vee->p_ctrl, &vee_status);
        do
        {
            err = p_vee->g_vee->p_api->statusGet(p_vee->g_vee->p_ctrl, &vee_status);
            if ((vee_status.state == RM_VEE_STATE_BUSY) || (vee_status.state == RM_VEE_STATE_REFRESH))
            {
                ENVRION_DEBUG(ANSI_COLOR_LIGHT_YELLOW "\t[%s] Wait for Erasing %s state=%u\n" ANSI_COLOR_DEFULT,
                              __func__,
                              p_vee->area->parameters[i].name,
                              vee_status.state);
                vTaskDelay(portCONVERT_MS_2_TICKS(100));
            }
        } while (vee_status.state != RM_VEE_STATE_READY &&
                 vee_status.state != RM_VEE_STATE_OVERFLOW &&
                 vee_status.state != RM_VEE_STATE_HARDWARE_FAIL);

        if (vee_status.state != RM_VEE_STATE_READY)
        {
            ENVRION_ERROR(ANSI_COLOR_LIGHT_RED "\t[%s] Erase Error %s state=%u\n" ANSI_COLOR_DEFULT,
                          __func__,
                          p_vee->area->parameters[i].name,
                          vee_status.state);
        }
    }
}

void vee_nvparam_format (rm_vee_group_instance_t * p_vee)
{
    fsp_err_t       err;
    rm_vee_status_t vee_status;

    if ((NULL == p_vee) || (NULL == p_vee->g_vee))
    {
        return;
    }

 #if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
    R_WDOG_W_Freeze(g_wifi_cfg.p_watchdog_service->p_cfg->p_wdt->p_ctrl, true);
 #endif
    err = p_vee->g_vee->p_api->format(p_vee->g_vee->p_ctrl, NULL);
 #if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
    R_WDOG_W_Freeze(g_wifi_cfg.p_watchdog_service->p_cfg->p_wdt->p_ctrl, false);
 #endif
    if (FSP_SUCCESS != err)
    {
        return;
    }

    err = p_vee->g_vee->p_api->statusGet(p_vee->g_vee->p_ctrl, &vee_status);
    do
    {
        err = p_vee->g_vee->p_api->statusGet(p_vee->g_vee->p_ctrl, &vee_status);
        ENVRION_DEBUG(ANSI_COLOR_LIGHT_YELLOW "\t[%s] state=%u\n" ANSI_COLOR_DEFULT, __func__, vee_status.state);
        vTaskDelay(portCONVERT_MS_2_TICKS(100));
    } while (vee_status.state != RM_VEE_STATE_READY &&
             vee_status.state != RM_VEE_STATE_OVERFLOW &&
             vee_status.state != RM_VEE_STATE_HARDWARE_FAIL);

    if (vee_status.state != RM_VEE_STATE_READY)
    {
        ENVRION_ERROR(ANSI_COLOR_LIGHT_RED "\t[%s] Format Error state=%u\n" ANSI_COLOR_DEFULT,
                      __func__,
                      vee_status.state);
    }
}

void vee_nvparam_refresh (rm_vee_group_instance_t * p_vee)
{
    fsp_err_t       err;
    rm_vee_status_t vee_status;

    if ((NULL == p_vee) || (NULL == p_vee->g_vee))
    {
        return;
    }

    ENVRION_DEBUG(ANSI_COLOR_LIGHT_BLUE "\t[%s] Start refresh\n" ANSI_COLOR_DEFULT, __func__);
 #if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
    R_WDOG_W_Freeze(g_wifi_cfg.p_watchdog_service->p_cfg->p_wdt->p_ctrl, true);
 #endif
    err = p_vee->g_vee->p_api->refresh(p_vee->g_vee->p_ctrl);
 #if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
    R_WDOG_W_Freeze(g_wifi_cfg.p_watchdog_service->p_cfg->p_wdt->p_ctrl, false);
 #endif
    ENVRION_DEBUG(ANSI_COLOR_LIGHT_BLUE "\t[%s] End refresh\n" ANSI_COLOR_DEFULT, __func__);
    if (FSP_SUCCESS != err)
    {
        ENVRION_ERROR(ANSI_COLOR_LIGHT_RED "\t[%s] err=%u\n" ANSI_COLOR_DEFULT, __func__, err);

        return;
    }

    err = p_vee->g_vee->p_api->statusGet(p_vee->g_vee->p_ctrl, &vee_status);
    do
    {
        err = p_vee->g_vee->p_api->statusGet(p_vee->g_vee->p_ctrl, &vee_status);
        if (FSP_SUCCESS != err)
        {
            ENVRION_ERROR(ANSI_COLOR_LIGHT_RED "\t[%s] statusGet err=%u\n" ANSI_COLOR_DEFULT, __func__, err);
            continue;
        }

        ENVRION_DEBUG(ANSI_COLOR_LIGHT_YELLOW "\t[%s] state=%u\n" ANSI_COLOR_DEFULT, __func__, vee_status.state);
        vTaskDelay(portCONVERT_MS_2_TICKS(100));
    } while (vee_status.state != RM_VEE_STATE_READY &&
             vee_status.state != RM_VEE_STATE_OVERFLOW &&
             vee_status.state != RM_VEE_STATE_HARDWARE_FAIL);

    ENVRION_DEBUG("\t[%s] state=%u\n", __func__, vee_status.state);
}

//////////////////////////////////////////////////////////////////////////////////////////
// RRQ NVRAM API
//////////////////////////////////////////////////////////////////////////////////////////
 #ifndef RM_MAP_PERSISTANT_W
int read_nvram_uint (const char * group, const char * name, int * _val)
{
    ra6w1_getenv_bin(group, (char *) name, _val, 4);

    return 0;
}

int write_nvram_uint (const char * group, const char * name, int val)
{
  #if CFG_PMGR

    /* At this case,,, dpm_sleep operation was started already */
    if (RM_PMGR_W_dpm_sleep_is_started())
    {
        printf("[%s] Already DPM Sleep started !!!\n", __func__);

        return -1;
    }
  #endif                               /* CFG_PMGR */

    if (ra6w1_setenv_bin(group, (char *) name, &val) == 0)
    {
        printf("[%s] NVRAM Write: Failed(%s=%d)\n", __func__, name, val);

        return -2;
    }

    return 0;
}

int read_nvram_int (const char * group, const char * name, int * _val)
{
    int * value;

    value = (int *) ra6w1_getenv(group, (char *) name);

    if (value == NULL)
    {
        // printf("[%s] '%s' not found.\n", __func__, name);
        *_val = -1;

        return -1;
    }

    *_val = *value;

    return 0;
}

int write_nvram_int (const char * group, const char * name, int val)
{
    int    ret = 0;
    int  * value_env;
    char * valstr_env = NULL;

  #if CFG_PMGR

    /* At this case,,, dpm_sleep operation was started already */
    if (RM_PMGR_W_dpm_sleep_is_started())
    {
        printf("[%s] Already DPM Sleep started !!!\n", __func__);

        return -1;
    }
  #endif                               /* CFG_PMGR */

    value_env = (int *) ra6w1_getenv(group, (char *) name);
    if ((value_env != NULL) && (*value_env == val))
    {
        goto end;
    }

    if (ra6w1_setenv_bin(group, (char *) name, &val) == 0)
    {
        printf("[%s] NVRAM Write: Failed to set %s=%d\n", __func__, name, val);
        ret = -1;
    }

end:

    return ret;
}

char * read_nvram_string (const char * group, const char * name)
{
    return ra6w1_getenv(group, (char *) name);
}

int write_nvram_string (const char * group, const char * name, const char * val)
{
  #if CFG_PMGR

    /* At this case,,, dpm_sleep operation was started already */
    if (RM_PMGR_W_dpm_sleep_is_started())
    {
        printf("[%s] Already DPM Sleep started !!!\n", __func__);

        return -1;
    }
  #endif                               /* CFG_PMGR */

    if (ra6w1_setenv(group, (char *) name, (char *) val) == 0)
    {

        // printf("[%s] NVRAM Write: Failed(%s=%s)\n", __func__, name, val);
        return -2;
    }

    return 0;
}

int delete_nvram_env (const char * group, const char * name)
{
  #if CFG_PMGR

    /* At this case,,, dpm_sleep operation was started already */
    if (RM_PMGR_W_dpm_sleep_is_started())
    {
        printf("[%s] Already DPM Sleep started !!!\n", __func__);

        return -1;
    }
  #endif                               /* CFG_PMGR */

    return ra6w1_unsetenv(group, (char *) name);
}

int clear_nvram_envall (const char * group)
{
  #if CFG_PMGR

    /* At this case,,, dpm_sleep operation was started already */
    if (RM_PMGR_W_dpm_sleep_is_started())
    {
        printf("[%s] Already DPM Sleep started !!!\n", __func__);

        return -1;
    }
  #endif                               /* CFG_PMGR */
    return ra6w1_clearenv(group);
}

 #endif

#endif
