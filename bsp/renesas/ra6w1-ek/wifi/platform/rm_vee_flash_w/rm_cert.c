/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/
#include "rm_vee_flash_w_cfg.h"
#ifdef RM_VEE_USE_CERT

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
 #include "rm_cert.h"
 #include "custom_config_sdk.h"        /* For __SUPPORT_ATCMD_TLS__ */
 #include "r_ospi_w_cfg.h"             /* For RRQ61X_OSPI_W_ENABLED */

 #if defined(__SUPPORT_ATCMD_TLS__)
  #include "rm_atcmd_w_core_socket_cert_mng.h"
 #endif
 #ifndef RRQ61X_OSPI_W_ENABLED
  #include "ad_flash.h"
 #else                                 /* RRQ61X_OSPI_W_ENABLED */
  #include "util_api.h"
 #endif /* RRQ61X_OSPI_W_ENABLED */
 #include "r_cc312_secureboot.h"

extern uint32_t CC_BsvLcsGet(unsigned long hwBaseAddress, uint32_t * pLcs);

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

 #undef  RM_CERT_ENABLE_DBG_INFO
 #undef  RM_CERT_ENABLE_DBG_ERR

 #define RM_CERT_PRT    printf

 #if defined(RM_CERT_ENABLE_DBG_INFO)
  #define RM_CERT_INFO(fmt, ...) \
    RM_CERT_PRT("[%s:%d]" fmt, __func__, __LINE__, ## __VA_ARGS__)
 #else
  #define RM_CERT_INFO(...)    do {} while (0)
 #endif                                /* (RM_CERT_ENABLE_DBG_INFO) */

 #if defined(RM_CERT_ENABLE_DBG_ERR)
  #define RM_CERT_ERR(fmt, ...) \
    RM_CERT_PRT("[%s:%d]" fmt, __func__, __LINE__, ## __VA_ARGS__)
 #else
  #define RM_CERT_ERR(...)    do {} while (0)
 #endif                                /* (RM_CERT_ENABLE_DBG_ERR) */

 #define RM_CERT_DEVICE_MANUFACTURE_LCS    (0x1u)

/***********************************************************************************************************************
 * External functions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private global variables and functions
 **********************************************************************************************************************/

static void *rm_cert_internal_calloc(size_t n, size_t size);
static void rm_cert_internal_free(void *f);
static int rm_cert_is_valid_module(rm_cert_module_t module);
static int rm_cert_is_valid_type(rm_cert_module_t module, rm_cert_type_t type);
static int rm_cert_is_valid_format(rm_cert_format_t format);
static int rm_cert_is_valid_length(size_t len);
static uint32_t rm_cert_get_flash_address(rm_cert_module_t module, rm_cert_type_t type);
static rm_cert_err_t rm_cert_write_by_addr(uint32_t addr, rm_cert_format_t format, uint8_t *in, size_t inlen);
static rm_cert_err_t rm_cert_read_by_addr(uint32_t addr, rm_cert_format_t *format, uint8_t *out, size_t *outlen);
static rm_cert_err_t rm_cert_delete_by_addr(uint32_t addr);

/* Only test key instead of KCP */
static const uint8_t test_key_cert[] =
{
    0xd5, 0xe9, 0xda, 0x41, 0xa6, 0x5b, 0x7f, 0xd2, 0xe5, 0xad, 0xf4, 0xb8, 0xf8, 0x43, 0x25, 0x3f
};
static AssetUserKeyData_t userKeyData_cert =
{
    .pKey    = (uint8_t *) test_key_cert,
    .keySize = 16,
};

static void * rm_cert_internal_calloc (size_t n, size_t size)
{
    void * buf    = NULL;
    size_t buflen = (n * size);

    buf = pvPortMalloc(buflen);
    if (buf)
    {
        memset(buf, 0x00, buflen);
    }

    return buf;
}

static void rm_cert_internal_free (void * f)
{
    if (f == NULL)
    {
        return;
    }

    vPortFree(f);
    f = NULL;
}

void * (* rm_cert_calloc)(size_t n, size_t size) = rm_cert_internal_calloc;
void   (* rm_cert_free)(void * ptr) = rm_cert_internal_free;

static int rm_cert_is_valid_module (rm_cert_module_t module)
{
    RM_CERT_INFO("Module:%d\n", module);

    if ((module == RM_CERT_MODULE_MQTT           && CERT_MQTTS_CLI_USED == 1)  ||
        (module == RM_CERT_MODULE_HTTPS_CLIENT   && CERT_HTTPS_CLI_USED == 1)  ||
        (module == RM_CERT_MODULE_WPA_ENTERPRISE && CERT_WPA_ENT_USED   == 1)  ||
        (module == RM_CERT_MODULE_OTA            && CERT_OTA_USED       == 1)  ||
        (module == RM_CERT_MODULE_HTTPS_SERVER   && CERT_HTTPS_SVR_USED == 1)  ||
        (module == RM_CERT_MODULE_ATCMD          && CERT_ATCMD_USED     == 1)  ||
        (module == RM_CERT_MODULE_AWS            && CERT_AWS_USED       == 1)  ||
        (module == RM_CERT_MODULE_MATTER         && CERT_MATTER_USED    == 1)  ||
        (module == RM_CERT_MODULE_MISC1          && CERT_MISC1_USED     == 1)  ||
        (module == RM_CERT_MODULE_MISC2          && CERT_MISC2_USED     == 1)  ||
        (module == RM_CERT_MODULE_MISC3          && CERT_MISC3_USED     == 1)  ||
        (module == RM_CERT_MODULE_MISC4          && CERT_MISC4_USED     == 1)  ||
        (module == RM_CERT_MODULE_MISC5          && CERT_MISC5_USED     == 1)  ||
        (module == RM_CERT_MODULE_MISC6          && CERT_MISC6_USED     == 1)  ||
        (module == RM_CERT_MODULE_MISC7          && CERT_MISC7_USED     == 1)  ||
        (module == RM_CERT_MODULE_MISC8          && CERT_MISC8_USED     == 1))
    {
        return pdTRUE;
    }
    else
    {
        return pdFALSE;
    }
}

static int rm_cert_is_valid_type(rm_cert_module_t module, rm_cert_type_t type)
{
    RM_CERT_INFO("Type:%d\n", type);

    if (module == RM_CERT_MODULE_AWS)
    {
        switch (type) {
        case RM_CERT_TYPE_CA_CERT:
        case RM_CERT_TYPE_INITIAL_CERT:
        case RM_CERT_TYPE_INITIAL_PRIV_KEY:
        case RM_CERT_TYPE_UNIQUE_CERT:
        case RM_CERT_TYPE_UNIQUE_PRIV_KEY:
            return pdTRUE;
        default:
            return pdFALSE;
        }
    } else if (module == RM_CERT_MODULE_MATTER)
    {
        switch (type) {
        case RM_CERT_TYPE_CD:
        case RM_CERT_TYPE_DAC_CERT:
        case RM_CERT_TYPE_PAI_CERT:
        case RM_CERT_TYPE_DAC_PRIV_KEY:
        case RM_CERT_TYPE_DAC_PUB_KEY:
            return pdTRUE;
        default:
            return pdFALSE;
        }
    } else if ((module >= RM_CERT_MODULE_MISC1) && (module <= RM_CERT_MODULE_MISC8))
    {
        switch (type) {
        case RM_CERT_TYPE_CA_CERT:
        case RM_CERT_TYPE_CERT:
        case RM_CERT_TYPE_PRIVATE_KEY:
        case RM_CERT_TYPE_DH_PARAMS:
        case RM_CERT_TYPE_EXCHANGE:
            return pdTRUE;
        default:
            return pdFALSE;
        }
    } else
    {
        switch (type) {
        case RM_CERT_TYPE_CA_CERT:
        case RM_CERT_TYPE_CERT:
        case RM_CERT_TYPE_PRIVATE_KEY:
        case RM_CERT_TYPE_DH_PARAMS:
            return pdTRUE;
        default:
            return pdFALSE;
        }
    }
}


static int rm_cert_is_valid_format(rm_cert_format_t format)
{
    RM_CERT_INFO("Format:%d\n", format);

    switch (format)
    {
        case RM_CERT_FORMAT_DER:
        case RM_CERT_FORMAT_PEM:
        {
            return pdTRUE;
        }

        default:

            return pdFALSE;
    }
}

static int rm_cert_is_valid_length (size_t len)
{
    RM_CERT_INFO("Length:%d\n", len);

    if (len > RM_CERT_MAX_LENGTH)
    {
        return pdFALSE;
    }

    return pdTRUE;
}

static uint32_t rm_cert_get_flash_address (rm_cert_module_t module, rm_cert_type_t type)
{
    RM_CERT_INFO("Module:%d, type:%d\n", module, type);

    if (module == RM_CERT_MODULE_MQTT)
    {
        if (type == RM_CERT_TYPE_CA_CERT)
        {
            return SF_TLS_CERT_MQTT_CLI_CA_ADDR;
        }
        else if (type == RM_CERT_TYPE_CERT)
        {
            return SF_TLS_CERT_MQTT_CLI_CERTIFICATE_ADDR;
        }
        else if (type == RM_CERT_TYPE_PRIVATE_KEY)
        {
            return SF_TLS_CERT_MQTT_CLI_PRIVATE_KEY_ADDR;
        }
        else if (type == RM_CERT_TYPE_DH_PARAMS)
        {
            return SF_TLS_CERT_MQTT_CLI_DH_PARAMETER_ADDR;
        }
    }
    else if (module == RM_CERT_MODULE_HTTPS_CLIENT)
    {
        if (type == RM_CERT_TYPE_CA_CERT)
        {
            return SF_TLS_CERT_HTTPS_CLI_CA_ADDR;
        }
        else if (type == RM_CERT_TYPE_CERT)
        {
            return SF_TLS_CERT_HTTPS_CLI_CERTIFICATE_ADDR;
        }
        else if (type == RM_CERT_TYPE_PRIVATE_KEY)
        {
            return SF_TLS_CERT_HTTPS_CLI_PRIVATE_KEY_ADDR;
        }
        else if (type == RM_CERT_TYPE_DH_PARAMS)
        {
            return SF_TLS_CERT_HTTPS_CLI_DH_PARAMETER_ADDR;
        }
    }
    else if (module == RM_CERT_MODULE_WPA_ENTERPRISE)
    {
        if (type == RM_CERT_TYPE_CA_CERT)
        {
            return SF_TLS_CERT_WPA_ENT_CA_ADDR;
        }
        else if (type == RM_CERT_TYPE_CERT)
        {
            return SF_TLS_CERT_WPA_ENT_CERTIFICATE_ADDR;
        }
        else if (type == RM_CERT_TYPE_PRIVATE_KEY)
        {
            return SF_TLS_CERT_WPA_ENT_PRIVATE_KEY_ADDR;
        }
        else if (type == RM_CERT_TYPE_DH_PARAMS)
        {
            return SF_TLS_CERT_WPA_ENT_DH_PARAMETER_ADDR;
        }
    }
    else if (module == RM_CERT_MODULE_OTA)
    {
        if (type == RM_CERT_TYPE_CA_CERT)
        {
            return SF_TLS_CERT_OTA_CA_ADDR;
        }
        else if (type == RM_CERT_TYPE_CERT)
        {
            return SF_TLS_CERT_OTA_CERTIFICATE_ADDR;
        }
        else if (type == RM_CERT_TYPE_PRIVATE_KEY)
        {
            return SF_TLS_CERT_OTA_PRIVATE_KEY_ADDR;
        }
        else if (type == RM_CERT_TYPE_DH_PARAMS)
        {
            return SF_TLS_CERT_OTA_DH_PARAMETER_ADDR;
        }
    }
    else if (module == RM_CERT_MODULE_HTTPS_SERVER)
    {
        if (type == RM_CERT_TYPE_CA_CERT)
        {
            return SF_TLS_CERT_HTTPS_SVR_CA_ADDR;
        }
        else if (type == RM_CERT_TYPE_CERT)
        {
            return SF_TLS_CERT_HTTPS_SVR_CERTIFICATE_ADDR;
        }
        else if (type == RM_CERT_TYPE_PRIVATE_KEY)
        {
            return SF_TLS_CERT_HTTPS_SVR_PRIVATE_KEY_ADDR;
        }
        else if (type == RM_CERT_TYPE_DH_PARAMS)
        {
            return SF_TLS_CERT_HTTPS_SVR_DH_PARAMETER_ADDR;
        }
    } else if (module == RM_CERT_MODULE_ATCMD) {
        if (type == RM_CERT_TYPE_CA_CERT) {
            return SFLASH_ATCMD_TLS_CERT_01;
        } else if (type == RM_CERT_TYPE_CERT) {
            return SFLASH_ATCMD_TLS_CERT_02;
        } else if (type == RM_CERT_TYPE_PRIVATE_KEY) {
            return SFLASH_ATCMD_TLS_CERT_03;
        } else if (type == RM_CERT_TYPE_DH_PARAMS) {
            return SFLASH_ATCMD_TLS_CERT_04;
        }
    }
    else if (module == RM_CERT_MODULE_AWS) {
        if (type == RM_CERT_TYPE_CA_CERT) {
            return SF_TLS_CERT_AWS_CA_ADDR;
        } else if (type == RM_CERT_TYPE_INITIAL_CERT) {
            return SF_TLS_CERT_AWS_INITIAL_CERT_ADDR;
        } else if (type == RM_CERT_TYPE_INITIAL_PRIV_KEY) {
            return SF_TLS_CERT_AWS_INITIAL_PRIV_KEY_ADDR;
        }else if (type == RM_CERT_TYPE_UNIQUE_CERT) {
            return SF_TLS_CERT_AWS_UNIQUE_CERT_ADDR;
        }else if (type == RM_CERT_TYPE_UNIQUE_PRIV_KEY) {
            return SF_TLS_CERT_AWS_UNIQUE_PRIV_KEY_ADDR;
        }
    }
    else if (module == RM_CERT_MODULE_MATTER) {
        if (type == RM_CERT_TYPE_CD) {
            return SF_MATTER_CERT_CD_ADDR;
        } else if (type == RM_CERT_TYPE_DAC_CERT) {
            return SF_MATTER_CERT_DAC_CERTIFICATE_ADDR;
        } else if (type == RM_CERT_TYPE_PAI_CERT) {
            return SF_MATTER_CERT_PAI_CERTIFICATE_ADDR;
        } else if (type == RM_CERT_TYPE_DAC_PRIV_KEY) {
            return SF_MATTER_CERT_DAC_PRIVATE_KEY_ADDR;
        } else if (type == RM_CERT_TYPE_DAC_PUB_KEY) {
            return SF_MATTER_CERT_DAC_PUBLIC_KEY_ADDR;
        }
    }
    else if (module == RM_CERT_MODULE_MISC1) {
        if (type == RM_CERT_TYPE_CA_CERT) {
            return SF_TLS_CERT_MISC1_CA_ADDR;
        } else if (type == RM_CERT_TYPE_CA_CERT) {
            return SF_TLS_CERT_MISC1_CERTIFICATE_ADDR;
        } else if (type == RM_CERT_TYPE_PRIVATE_KEY) {
            return SF_TLS_CERT_MISC1_PRIVATE_KEY_ADDR;
        } else if (type == RM_CERT_TYPE_DH_PARAMS) {
            return SF_TLS_CERT_MISC1_DH_PARAMETER_ADDR;
        } else if (type == RM_CERT_TYPE_EXCHANGE) {
            return SF_TLS_CERT_MISC1_EXCHANGE_ADDR;
        }
    }
    else if (module == RM_CERT_MODULE_MISC2) {
        if (type == RM_CERT_TYPE_CA_CERT) {
            return SF_TLS_CERT_MISC2_CA_ADDR;
        } else if (type == RM_CERT_TYPE_CA_CERT) {
            return SF_TLS_CERT_MISC2_CERTIFICATE_ADDR;
        } else if (type == RM_CERT_TYPE_PRIVATE_KEY) {
            return SF_TLS_CERT_MISC2_PRIVATE_KEY_ADDR;
        } else if (type == RM_CERT_TYPE_DH_PARAMS) {
            return SF_TLS_CERT_MISC2_DH_PARAMETER_ADDR;
        } else if (type == RM_CERT_TYPE_EXCHANGE) {
            return SF_TLS_CERT_MISC2_EXCHANGE_ADDR;
        }
    }
    else if (module == RM_CERT_MODULE_MISC3) {
        if (type == RM_CERT_TYPE_CA_CERT) {
            return SF_TLS_CERT_MISC3_CA_ADDR;
        } else if (type == RM_CERT_TYPE_CA_CERT) {
            return SF_TLS_CERT_MISC3_CERTIFICATE_ADDR;
        } else if (type == RM_CERT_TYPE_PRIVATE_KEY) {
            return SF_TLS_CERT_MISC3_PRIVATE_KEY_ADDR;
        } else if (type == RM_CERT_TYPE_DH_PARAMS) {
            return SF_TLS_CERT_MISC3_DH_PARAMETER_ADDR;
        } else if (type == RM_CERT_TYPE_EXCHANGE) {
            return SF_TLS_CERT_MISC3_EXCHANGE_ADDR;
        }
    }
    else if (module == RM_CERT_MODULE_MISC4) {
        if (type == RM_CERT_TYPE_CA_CERT) {
            return SF_TLS_CERT_MISC4_CA_ADDR;
        } else if (type == RM_CERT_TYPE_CA_CERT) {
            return SF_TLS_CERT_MISC4_CERTIFICATE_ADDR;
        } else if (type == RM_CERT_TYPE_PRIVATE_KEY) {
            return SF_TLS_CERT_MISC4_PRIVATE_KEY_ADDR;
        } else if (type == RM_CERT_TYPE_DH_PARAMS) {
            return SF_TLS_CERT_MISC4_DH_PARAMETER_ADDR;
        } else if (type == RM_CERT_TYPE_EXCHANGE) {
            return SF_TLS_CERT_MISC4_EXCHANGE_ADDR;
        }
    }
    else if (module == RM_CERT_MODULE_MISC5) {
        if (type == RM_CERT_TYPE_CA_CERT) {
            return SF_TLS_CERT_MISC5_CA_ADDR;
        } else if (type == RM_CERT_TYPE_CA_CERT) {
            return SF_TLS_CERT_MISC5_CERTIFICATE_ADDR;
        } else if (type == RM_CERT_TYPE_PRIVATE_KEY) {
            return SF_TLS_CERT_MISC5_PRIVATE_KEY_ADDR;
        } else if (type == RM_CERT_TYPE_DH_PARAMS) {
            return SF_TLS_CERT_MISC5_DH_PARAMETER_ADDR;
        } else if (type == RM_CERT_TYPE_EXCHANGE) {
            return SF_TLS_CERT_MISC5_EXCHANGE_ADDR;
        }
    }
    else if (module == RM_CERT_MODULE_MISC6) {
        if (type == RM_CERT_TYPE_CA_CERT) {
            return SF_TLS_CERT_MISC6_CA_ADDR;
        } else if (type == RM_CERT_TYPE_CA_CERT) {
            return SF_TLS_CERT_MISC6_CERTIFICATE_ADDR;
        } else if (type == RM_CERT_TYPE_PRIVATE_KEY) {
            return SF_TLS_CERT_MISC6_PRIVATE_KEY_ADDR;
        } else if (type == RM_CERT_TYPE_DH_PARAMS) {
            return SF_TLS_CERT_MISC6_DH_PARAMETER_ADDR;
        } else if (type == RM_CERT_TYPE_EXCHANGE) {
            return SF_TLS_CERT_MISC6_EXCHANGE_ADDR;
        }
    }
    else if (module == RM_CERT_MODULE_MISC7) {
        if (type == RM_CERT_TYPE_CA_CERT) {
            return SF_TLS_CERT_MISC7_CA_ADDR;
        } else if (type == RM_CERT_TYPE_CA_CERT) {
            return SF_TLS_CERT_MISC7_CERTIFICATE_ADDR;
        } else if (type == RM_CERT_TYPE_PRIVATE_KEY) {
            return SF_TLS_CERT_MISC7_PRIVATE_KEY_ADDR;
        } else if (type == RM_CERT_TYPE_DH_PARAMS) {
            return SF_TLS_CERT_MISC7_DH_PARAMETER_ADDR;
        } else if (type == RM_CERT_TYPE_EXCHANGE) {
            return SF_TLS_CERT_MISC7_EXCHANGE_ADDR;
        }
    }
    else if (module == RM_CERT_MODULE_MISC8) {
        if (type == RM_CERT_TYPE_CA_CERT) {
            return SF_TLS_CERT_MISC8_CA_ADDR;
        } else if (type == RM_CERT_TYPE_CA_CERT) {
            return SF_TLS_CERT_MISC8_CERTIFICATE_ADDR;
        } else if (type == RM_CERT_TYPE_PRIVATE_KEY) {
            return SF_TLS_CERT_MISC8_PRIVATE_KEY_ADDR;
        } else if (type == RM_CERT_TYPE_DH_PARAMS) {
            return SF_TLS_CERT_MISC8_DH_PARAMETER_ADDR;
        } else if (type == RM_CERT_TYPE_EXCHANGE) {
            return SF_TLS_CERT_MISC8_EXCHANGE_ADDR;
        }
    }

    return 0;
}

rm_cert_module_t RM_CERT_GetModule (uint32_t flash_addr)
{
    RM_CERT_INFO("Addr:0x%x\n", (unsigned int) flash_addr);

    if (CERT_MQTTS_CLI_USED == 1)
    {
        switch (flash_addr)
        {
            case SF_TLS_CERT_MQTT_CLI_CA_ADDR:
            case SF_TLS_CERT_MQTT_CLI_CERTIFICATE_ADDR:
            case SF_TLS_CERT_MQTT_CLI_PRIVATE_KEY_ADDR:
            case SF_TLS_CERT_MQTT_CLI_DH_PARAMETER_ADDR:
            {
                return RM_CERT_MODULE_MQTT;
            }

            default:
            {
                break;
            }
        }
    }

    if (CERT_HTTPS_CLI_USED == 1)
    {
        switch (flash_addr)
        {
            case SF_TLS_CERT_HTTPS_CLI_CA_ADDR:
            case SF_TLS_CERT_HTTPS_CLI_CERTIFICATE_ADDR:
            case SF_TLS_CERT_HTTPS_CLI_PRIVATE_KEY_ADDR:
            case SF_TLS_CERT_HTTPS_CLI_DH_PARAMETER_ADDR:
            {
                return RM_CERT_MODULE_HTTPS_CLIENT;
            }

            default:
            {
                break;
            }
        }
    }

    if (CERT_WPA_ENT_USED == 1)
    {
        switch (flash_addr)
        {
            case SF_TLS_CERT_WPA_ENT_CA_ADDR:
            case SF_TLS_CERT_WPA_ENT_CERTIFICATE_ADDR:
            case SF_TLS_CERT_WPA_ENT_PRIVATE_KEY_ADDR:
            case SF_TLS_CERT_WPA_ENT_DH_PARAMETER_ADDR:
            {
                return RM_CERT_MODULE_WPA_ENTERPRISE;
            }

            default:
            {
                break;
            }
        }
    }

    if (CERT_OTA_USED == 1)
    {
        switch (flash_addr)
        {
            case SF_TLS_CERT_OTA_CA_ADDR:
            case SF_TLS_CERT_OTA_CERTIFICATE_ADDR:
            case SF_TLS_CERT_OTA_PRIVATE_KEY_ADDR:
            case SF_TLS_CERT_OTA_DH_PARAMETER_ADDR:
            {
                return RM_CERT_MODULE_OTA;
            }

            default:
            {
                break;
            }
        }
    }
    if (CERT_HTTPS_SVR_USED == 1) {
        switch (flash_addr) {
        case SF_TLS_CERT_HTTPS_SVR_CA_ADDR:
        case SF_TLS_CERT_HTTPS_SVR_CERTIFICATE_ADDR:
        case SF_TLS_CERT_HTTPS_SVR_PRIVATE_KEY_ADDR:
        case SF_TLS_CERT_HTTPS_SVR_DH_PARAMETER_ADDR:
            return RM_CERT_MODULE_HTTPS_SERVER;
        default:
            break;
        }
    }
    if (CERT_ATCMD_USED == 1) {
        switch (flash_addr) {
        case SFLASH_ATCMD_TLS_CERT_01:
        case SFLASH_ATCMD_TLS_CERT_02:
        case SFLASH_ATCMD_TLS_CERT_03:
        case SFLASH_ATCMD_TLS_CERT_04:
        case SFLASH_ATCMD_TLS_CERT_05:
        case SFLASH_ATCMD_TLS_CERT_06:
        case SFLASH_ATCMD_TLS_CERT_07:
        case SFLASH_ATCMD_TLS_CERT_08:
        case SFLASH_ATCMD_TLS_CERT_09:
        case SFLASH_ATCMD_TLS_CERT_10:
        case SFLASH_ATCMD_TLS_CERT_11:
        case SFLASH_ATCMD_TLS_CERT_12:
        case SFLASH_ATCMD_TLS_CERT_13:
        case SFLASH_ATCMD_TLS_CERT_14:
        case SFLASH_ATCMD_TLS_CERT_15:
        case SFLASH_ATCMD_TLS_CERT_16:
            return RM_CERT_MODULE_ATCMD;
        default:
            break;
        }
    }
    if (CERT_AWS_USED == 1) {
        switch (flash_addr) {
        case SF_TLS_CERT_AWS_CA_ADDR:
        case SF_TLS_CERT_AWS_INITIAL_CERT_ADDR:
        case SF_TLS_CERT_AWS_INITIAL_PRIV_KEY_ADDR:
        case SF_TLS_CERT_AWS_UNIQUE_CERT_ADDR:
        case SF_TLS_CERT_AWS_UNIQUE_PRIV_KEY_ADDR:
            return RM_CERT_MODULE_AWS;
        default:
            break;
        }
    }
    if (CERT_MATTER_USED == 1) {
        switch (flash_addr) {
        case SF_MATTER_CERT_CD_ADDR:
        case SF_MATTER_CERT_DAC_CERTIFICATE_ADDR:
        case SF_MATTER_CERT_PAI_CERTIFICATE_ADDR:
        case SF_MATTER_CERT_DAC_PRIVATE_KEY_ADDR:
        case SF_MATTER_CERT_DAC_PUBLIC_KEY_ADDR:
            return RM_CERT_MODULE_MATTER;
        default:
            break;
        }
    }
    if (CERT_MISC1_USED == 1) {
        switch (flash_addr) {
        case SF_TLS_CERT_MISC1_CA_ADDR:
        case SF_TLS_CERT_MISC1_CERTIFICATE_ADDR:
        case SF_TLS_CERT_MISC1_PRIVATE_KEY_ADDR:
        case SF_TLS_CERT_MISC1_DH_PARAMETER_ADDR:
        case SF_TLS_CERT_MISC1_EXCHANGE_ADDR:
            return RM_CERT_MODULE_MISC1;
        default:
            break;
        }
    }
    if (CERT_MISC2_USED == 1) {
        switch (flash_addr) {
        case SF_TLS_CERT_MISC2_CA_ADDR:
        case SF_TLS_CERT_MISC2_CERTIFICATE_ADDR:
        case SF_TLS_CERT_MISC2_PRIVATE_KEY_ADDR:
        case SF_TLS_CERT_MISC2_DH_PARAMETER_ADDR:
        case SF_TLS_CERT_MISC2_EXCHANGE_ADDR:
            return RM_CERT_MODULE_MISC2;
        default:
            break;
        }
    }
    if (CERT_MISC3_USED == 1) {
        switch (flash_addr) {
        case SF_TLS_CERT_MISC3_CA_ADDR:
        case SF_TLS_CERT_MISC3_CERTIFICATE_ADDR:
        case SF_TLS_CERT_MISC3_PRIVATE_KEY_ADDR:
        case SF_TLS_CERT_MISC3_DH_PARAMETER_ADDR:
        case SF_TLS_CERT_MISC3_EXCHANGE_ADDR:
            return RM_CERT_MODULE_MISC3;
        default:
            break;
        }
    }
    if (CERT_MISC4_USED == 1) {
        switch (flash_addr) {
        case SF_TLS_CERT_MISC4_CA_ADDR:
        case SF_TLS_CERT_MISC4_CERTIFICATE_ADDR:
        case SF_TLS_CERT_MISC4_PRIVATE_KEY_ADDR:
        case SF_TLS_CERT_MISC4_DH_PARAMETER_ADDR:
        case SF_TLS_CERT_MISC4_EXCHANGE_ADDR:
            return RM_CERT_MODULE_MISC4;
        default:
            break;
        }
    }
    if (CERT_MISC5_USED == 1) {
        switch (flash_addr) {
        case SF_TLS_CERT_MISC5_CA_ADDR:
        case SF_TLS_CERT_MISC5_CERTIFICATE_ADDR:
        case SF_TLS_CERT_MISC5_PRIVATE_KEY_ADDR:
        case SF_TLS_CERT_MISC5_DH_PARAMETER_ADDR:
        case SF_TLS_CERT_MISC5_EXCHANGE_ADDR:
            return RM_CERT_MODULE_MISC5;
        default:
            break;
        }
    }
    if (CERT_MISC6_USED == 1) {
        switch (flash_addr) {
        case SF_TLS_CERT_MISC6_CA_ADDR:
        case SF_TLS_CERT_MISC6_CERTIFICATE_ADDR:
        case SF_TLS_CERT_MISC6_PRIVATE_KEY_ADDR:
        case SF_TLS_CERT_MISC6_DH_PARAMETER_ADDR:
        case SF_TLS_CERT_MISC6_EXCHANGE_ADDR:
            return RM_CERT_MODULE_MISC6;
        default:
            break;
        }
    }
    if (CERT_MISC7_USED == 1) {
        switch (flash_addr) {
        case SF_TLS_CERT_MISC7_CA_ADDR:
        case SF_TLS_CERT_MISC7_CERTIFICATE_ADDR:
        case SF_TLS_CERT_MISC7_PRIVATE_KEY_ADDR:
        case SF_TLS_CERT_MISC7_DH_PARAMETER_ADDR:
        case SF_TLS_CERT_MISC7_EXCHANGE_ADDR:
            return RM_CERT_MODULE_MISC7;
        default:
            break;
        }
    }
    if (CERT_MISC8_USED == 1) {
        switch (flash_addr) {
        case SF_TLS_CERT_MISC8_CA_ADDR:
        case SF_TLS_CERT_MISC8_CERTIFICATE_ADDR:
        case SF_TLS_CERT_MISC8_PRIVATE_KEY_ADDR:
        case SF_TLS_CERT_MISC8_DH_PARAMETER_ADDR:
        case SF_TLS_CERT_MISC8_EXCHANGE_ADDR:
            return RM_CERT_MODULE_MISC8;
        default:
            break;
        }
    }
    return RM_CERT_MODULE_NONE;
}

rm_cert_type_t RM_CERT_GetType (uint32_t flash_addr)
{
    RM_CERT_INFO("Addr:0x%x\n", (unsigned int) flash_addr);

    if (CERT_MQTTS_CLI_USED == 1)
    {
        switch (flash_addr)
        {
            case SF_TLS_CERT_MQTT_CLI_CA_ADDR:
            {
                return RM_CERT_TYPE_CA_CERT;
            }

            case SF_TLS_CERT_MQTT_CLI_CERTIFICATE_ADDR:
            {
                return RM_CERT_TYPE_CERT;
            }

            case SF_TLS_CERT_MQTT_CLI_PRIVATE_KEY_ADDR:
            {
                return RM_CERT_TYPE_PRIVATE_KEY;
            }

            case SF_TLS_CERT_MQTT_CLI_DH_PARAMETER_ADDR:

                return RM_CERT_TYPE_DH_PARAMS;
        }
    }

    if (CERT_HTTPS_CLI_USED == 1)
    {
        switch (flash_addr)
        {
            case SF_TLS_CERT_HTTPS_CLI_CA_ADDR:
            {
                return RM_CERT_TYPE_CA_CERT;
            }

            case SF_TLS_CERT_HTTPS_CLI_CERTIFICATE_ADDR:
            {
                return RM_CERT_TYPE_CERT;
            }

            case SF_TLS_CERT_HTTPS_CLI_PRIVATE_KEY_ADDR:
            {
                return RM_CERT_TYPE_PRIVATE_KEY;
            }

            case SF_TLS_CERT_HTTPS_CLI_DH_PARAMETER_ADDR:

                return RM_CERT_TYPE_DH_PARAMS;
        }
    }

    if (CERT_WPA_ENT_USED == 1)
    {
        switch (flash_addr)
        {
            case SF_TLS_CERT_WPA_ENT_CA_ADDR:
            {
                return RM_CERT_TYPE_CA_CERT;
            }

            case SF_TLS_CERT_WPA_ENT_CERTIFICATE_ADDR:
            {
                return RM_CERT_TYPE_CERT;
            }

            case SF_TLS_CERT_WPA_ENT_PRIVATE_KEY_ADDR:
            {
                return RM_CERT_TYPE_PRIVATE_KEY;
            }

            case SF_TLS_CERT_WPA_ENT_DH_PARAMETER_ADDR:

                return RM_CERT_TYPE_DH_PARAMS;
        }
    }

    if (CERT_OTA_USED == 1)
    {
        switch (flash_addr)
        {
            case SF_TLS_CERT_OTA_CA_ADDR:
            {
                return RM_CERT_TYPE_CA_CERT;
            }

            case SF_TLS_CERT_OTA_CERTIFICATE_ADDR:
            {
                return RM_CERT_TYPE_CERT;
            }

            case SF_TLS_CERT_OTA_PRIVATE_KEY_ADDR:
            {
                return RM_CERT_TYPE_PRIVATE_KEY;
            }

            case SF_TLS_CERT_OTA_DH_PARAMETER_ADDR:

                return RM_CERT_TYPE_DH_PARAMS;
        }
    }

    if (CERT_HTTPS_SVR_USED == 1)
    {
        switch (flash_addr)
        {
            case SF_TLS_CERT_HTTPS_SVR_CA_ADDR:
            {
                return RM_CERT_TYPE_CA_CERT;
            }

            case SF_TLS_CERT_HTTPS_SVR_CERTIFICATE_ADDR:
            {
                return RM_CERT_TYPE_CERT;
            }

            case SF_TLS_CERT_HTTPS_SVR_PRIVATE_KEY_ADDR:
            {
                return RM_CERT_TYPE_PRIVATE_KEY;
            }

            case SF_TLS_CERT_HTTPS_SVR_DH_PARAMETER_ADDR:

                return RM_CERT_TYPE_DH_PARAMS;
        }
    }
    if (CERT_ATCMD_USED == 1) {
        switch (flash_addr) {
        case SFLASH_ATCMD_TLS_CERT_01:
        case SFLASH_ATCMD_TLS_CERT_05:
        case SFLASH_ATCMD_TLS_CERT_09:
        case SFLASH_ATCMD_TLS_CERT_13:
            return RM_CERT_TYPE_CA_CERT;
        case SFLASH_ATCMD_TLS_CERT_02:
        case SFLASH_ATCMD_TLS_CERT_06:
        case SFLASH_ATCMD_TLS_CERT_10:
        case SFLASH_ATCMD_TLS_CERT_14:
            return RM_CERT_TYPE_CERT;
        case SFLASH_ATCMD_TLS_CERT_03:
        case SFLASH_ATCMD_TLS_CERT_07:
        case SFLASH_ATCMD_TLS_CERT_11:
        case SFLASH_ATCMD_TLS_CERT_15:
            return RM_CERT_TYPE_PRIVATE_KEY;
        case SFLASH_ATCMD_TLS_CERT_04:
        case SFLASH_ATCMD_TLS_CERT_08:
        case SFLASH_ATCMD_TLS_CERT_12:
        case SFLASH_ATCMD_TLS_CERT_16:
            return RM_CERT_TYPE_DH_PARAMS;
        }
    }
    if (CERT_AWS_USED == 1) {
        switch (flash_addr) {
        case SF_TLS_CERT_AWS_CA_ADDR:
            return RM_CERT_TYPE_CA_CERT;
        case SF_TLS_CERT_AWS_INITIAL_CERT_ADDR:
            return RM_CERT_TYPE_INITIAL_CERT;
        case SF_TLS_CERT_AWS_INITIAL_PRIV_KEY_ADDR:
            return RM_CERT_TYPE_INITIAL_PRIV_KEY;
        case SF_TLS_CERT_AWS_UNIQUE_CERT_ADDR:
            return RM_CERT_TYPE_UNIQUE_CERT;
        case SF_TLS_CERT_AWS_UNIQUE_PRIV_KEY_ADDR:
            return RM_CERT_TYPE_UNIQUE_PRIV_KEY;
        }
    }
    if (CERT_MATTER_USED == 1) {
        switch (flash_addr) {
        case SF_MATTER_CERT_CD_ADDR:
            return RM_CERT_TYPE_CD;
        case SF_MATTER_CERT_DAC_CERTIFICATE_ADDR:
            return RM_CERT_TYPE_DAC_CERT;
        case SF_MATTER_CERT_PAI_CERTIFICATE_ADDR:
            return RM_CERT_TYPE_PAI_CERT;
        case SF_MATTER_CERT_DAC_PRIVATE_KEY_ADDR:
            return RM_CERT_TYPE_DAC_PRIV_KEY;
        case SF_MATTER_CERT_DAC_PUBLIC_KEY_ADDR:
            return RM_CERT_TYPE_DAC_PUB_KEY;
        }
    }
    if (CERT_MISC1_USED == 1) {
        switch (flash_addr) {
        case SF_TLS_CERT_MISC1_CA_ADDR:
            return RM_CERT_TYPE_CA_CERT;
        case SF_TLS_CERT_MISC1_CERTIFICATE_ADDR:
            return RM_CERT_TYPE_CERT;
        case SF_TLS_CERT_MISC1_PRIVATE_KEY_ADDR:
            return RM_CERT_TYPE_PRIVATE_KEY;
        case SF_TLS_CERT_MISC1_DH_PARAMETER_ADDR:
            return RM_CERT_TYPE_DH_PARAMS;
        case SF_TLS_CERT_MISC1_EXCHANGE_ADDR:
            return RM_CERT_TYPE_EXCHANGE;
        }
    }
    if (CERT_MISC2_USED == 1) {
        switch (flash_addr) {
        case SF_TLS_CERT_MISC2_CA_ADDR:
            return RM_CERT_TYPE_CA_CERT;
        case SF_TLS_CERT_MISC2_CERTIFICATE_ADDR:
            return RM_CERT_TYPE_CERT;
        case SF_TLS_CERT_MISC2_PRIVATE_KEY_ADDR:
            return RM_CERT_TYPE_PRIVATE_KEY;
        case SF_TLS_CERT_MISC2_DH_PARAMETER_ADDR:
            return RM_CERT_TYPE_DH_PARAMS;
        case SF_TLS_CERT_MISC2_EXCHANGE_ADDR:
            return RM_CERT_TYPE_EXCHANGE;
        }
    }
    if (CERT_MISC3_USED == 1) {
        switch (flash_addr) {
        case SF_TLS_CERT_MISC3_CA_ADDR:
            return RM_CERT_TYPE_CA_CERT;
        case SF_TLS_CERT_MISC3_CERTIFICATE_ADDR:
            return RM_CERT_TYPE_CERT;
        case SF_TLS_CERT_MISC3_PRIVATE_KEY_ADDR:
            return RM_CERT_TYPE_PRIVATE_KEY;
        case SF_TLS_CERT_MISC3_DH_PARAMETER_ADDR:
            return RM_CERT_TYPE_DH_PARAMS;
        case SF_TLS_CERT_MISC3_EXCHANGE_ADDR:
            return RM_CERT_TYPE_EXCHANGE;
        }
    }
    if (CERT_MISC4_USED == 1) {
        switch (flash_addr) {
        case SF_TLS_CERT_MISC4_CA_ADDR:
            return RM_CERT_TYPE_CA_CERT;
        case SF_TLS_CERT_MISC4_CERTIFICATE_ADDR:
            return RM_CERT_TYPE_CERT;
        case SF_TLS_CERT_MISC4_PRIVATE_KEY_ADDR:
            return RM_CERT_TYPE_PRIVATE_KEY;
        case SF_TLS_CERT_MISC4_DH_PARAMETER_ADDR:
            return RM_CERT_TYPE_DH_PARAMS;
        case SF_TLS_CERT_MISC4_EXCHANGE_ADDR:
            return RM_CERT_TYPE_EXCHANGE;
        }
    }
    if (CERT_MISC5_USED == 1) {
        switch (flash_addr) {
        case SF_TLS_CERT_MISC5_CA_ADDR:
            return RM_CERT_TYPE_CA_CERT;
        case SF_TLS_CERT_MISC5_CERTIFICATE_ADDR:
            return RM_CERT_TYPE_CERT;
        case SF_TLS_CERT_MISC5_PRIVATE_KEY_ADDR:
            return RM_CERT_TYPE_PRIVATE_KEY;
        case SF_TLS_CERT_MISC5_DH_PARAMETER_ADDR:
            return RM_CERT_TYPE_DH_PARAMS;
        case SF_TLS_CERT_MISC5_EXCHANGE_ADDR:
            return RM_CERT_TYPE_EXCHANGE;
        }
    }
    if (CERT_MISC6_USED == 1) {
        switch (flash_addr) {
        case SF_TLS_CERT_MISC6_CA_ADDR:
            return RM_CERT_TYPE_CA_CERT;
        case SF_TLS_CERT_MISC6_CERTIFICATE_ADDR:
            return RM_CERT_TYPE_CERT;
        case SF_TLS_CERT_MISC6_PRIVATE_KEY_ADDR:
            return RM_CERT_TYPE_PRIVATE_KEY;
        case SF_TLS_CERT_MISC6_DH_PARAMETER_ADDR:
            return RM_CERT_TYPE_DH_PARAMS;
        case SF_TLS_CERT_MISC6_EXCHANGE_ADDR:
            return RM_CERT_TYPE_EXCHANGE;
        }
    }
    if (CERT_MISC7_USED == 1) {
        switch (flash_addr) {
        case SF_TLS_CERT_MISC7_CA_ADDR:
            return RM_CERT_TYPE_CA_CERT;
        case SF_TLS_CERT_MISC7_CERTIFICATE_ADDR:
            return RM_CERT_TYPE_CERT;
        case SF_TLS_CERT_MISC7_PRIVATE_KEY_ADDR:
            return RM_CERT_TYPE_PRIVATE_KEY;
        case SF_TLS_CERT_MISC7_DH_PARAMETER_ADDR:
            return RM_CERT_TYPE_DH_PARAMS;
        case SF_TLS_CERT_MISC7_EXCHANGE_ADDR:
            return RM_CERT_TYPE_EXCHANGE;
        }
    }
    if (CERT_MISC8_USED == 1) {
        switch (flash_addr) {
        case SF_TLS_CERT_MISC8_CA_ADDR:
            return RM_CERT_TYPE_CA_CERT;
        case SF_TLS_CERT_MISC8_CERTIFICATE_ADDR:
            return RM_CERT_TYPE_CERT;
        case SF_TLS_CERT_MISC8_PRIVATE_KEY_ADDR:
            return RM_CERT_TYPE_PRIVATE_KEY;
        case SF_TLS_CERT_MISC8_DH_PARAMETER_ADDR:
            return RM_CERT_TYPE_DH_PARAMS;
        case SF_TLS_CERT_MISC8_EXCHANGE_ADDR:
            return RM_CERT_TYPE_EXCHANGE;
        }
    }
    return RM_CERT_TYPE_NONE;
}

int RM_CERT_IsPemFormat (const char * buf)
{
    /* RA6W1 only checks ----BEGIN string.
     * -----BEGIN CERTIFICATE-----
     * -----BEGIN RSA PRIVATE KEY-----
     * -----BEGIN EC PRIVATE KEY-----
     * -----BEGIN PRIVATE KEY-----
     * -----BEGIN ENCRYPTED PRIVATE KEY-----
     */
    const char * pem_prefix     = "-----BEGIN";
    const size_t pem_prefix_len = strlen(pem_prefix);

    if ((strlen(buf) > pem_prefix_len) &&
        strstr(buf, pem_prefix))
    {
        return pdTRUE;
    }

    return pdFALSE;
}

int RM_CERT_IsDerFormat(const char *buf)
{
    /* RA6W1 only checks buffer contains DER encoded ASN.1 data
     * The buffer starts with an ASN.1 SEQUENCE tag (0x30)
     * The following byte contains a valid DER length encoding
     */
    if (strlen(buf) < 2)
    {
        return pdFALSE;
    }

    /* DER objects typically start with SEQUENCE (0x30) */
    if (buf[0] != 0x30)
    {
        return pdFALSE;
    }

    /* Validate DER length encoding */
    if ((unsigned char) buf[1] < 0x80)/* short form */
    {
        return pdTRUE;
    }
    else if ((unsigned char) buf[1] >= 0x81 && (unsigned char) buf[1] <= 0x84)/* long form */
    {
        return pdTRUE;
    }

    return pdFALSE;
}

static int cert_del_all = 0;
int RM_CERT_DeleteAll (void)
{
    int err = 0, i;
    int any_deleted = 0;

    uint32_t flash_addr[] = {
        SF_TLS_CERT_MQTT_CLI_CA_ADDR,               /* #0 */
        CERT_MQTTS_CLI_USED,
        SF_TLS_CERT_MQTT_CLI_CERTIFICATE_ADDR,
        CERT_MQTTS_CLI_USED,
        SF_TLS_CERT_MQTT_CLI_PRIVATE_KEY_ADDR,
        CERT_MQTTS_CLI_USED,
        SF_TLS_CERT_MQTT_CLI_DH_PARAMETER_ADDR,
        CERT_MQTTS_CLI_USED,
        
        SF_TLS_CERT_HTTPS_CLI_CA_ADDR,              /* #1 */
        CERT_HTTPS_CLI_USED,
        SF_TLS_CERT_HTTPS_CLI_CERTIFICATE_ADDR,
        CERT_HTTPS_CLI_USED,
        SF_TLS_CERT_HTTPS_CLI_PRIVATE_KEY_ADDR,
        CERT_HTTPS_CLI_USED,
        SF_TLS_CERT_HTTPS_CLI_DH_PARAMETER_ADDR,
        CERT_HTTPS_CLI_USED,
        
        SF_TLS_CERT_WPA_ENT_CA_ADDR,                /* #2 */
        CERT_WPA_ENT_USED,
        SF_TLS_CERT_WPA_ENT_CERTIFICATE_ADDR,
        CERT_WPA_ENT_USED,
        SF_TLS_CERT_WPA_ENT_PRIVATE_KEY_ADDR,
        CERT_WPA_ENT_USED,
        SF_TLS_CERT_WPA_ENT_DH_PARAMETER_ADDR,
        CERT_WPA_ENT_USED,
        
        SF_TLS_CERT_OTA_CA_ADDR,                    /* #3 */
        CERT_OTA_USED,
        SF_TLS_CERT_OTA_CERTIFICATE_ADDR,
        CERT_OTA_USED,
        SF_TLS_CERT_OTA_PRIVATE_KEY_ADDR,
        CERT_OTA_USED,
        SF_TLS_CERT_OTA_DH_PARAMETER_ADDR,
        CERT_OTA_USED,
        
        SF_TLS_CERT_HTTPS_SVR_CA_ADDR,              /* #4 */
        CERT_HTTPS_SVR_USED,
        SF_TLS_CERT_HTTPS_SVR_CERTIFICATE_ADDR,
        CERT_HTTPS_SVR_USED,
        SF_TLS_CERT_HTTPS_SVR_PRIVATE_KEY_ADDR,
        CERT_HTTPS_SVR_USED,
        SF_TLS_CERT_HTTPS_SVR_DH_PARAMETER_ADDR,
        CERT_HTTPS_SVR_USED,

        SF_TLS_CERT_AWS_CA_ADDR,                    /* #6 */
        CERT_AWS_USED,
        SF_TLS_CERT_AWS_UNIQUE_CERT_ADDR,
        CERT_AWS_USED,
        SF_TLS_CERT_AWS_UNIQUE_PRIV_KEY_ADDR,
        CERT_AWS_USED,

        SF_MATTER_CERT_CD_ADDR,                     /* #7 */
        CERT_MATTER_USED,
        SF_MATTER_CERT_DAC_CERTIFICATE_ADDR,
        CERT_MATTER_USED,
        SF_MATTER_CERT_PAI_CERTIFICATE_ADDR,
        CERT_MATTER_USED,
        SF_MATTER_CERT_DAC_PRIVATE_KEY_ADDR,
        CERT_MATTER_USED,
        SF_MATTER_CERT_DAC_PUBLIC_KEY_ADDR,
        CERT_MATTER_USED,

        SF_TLS_CERT_MISC1_CA_ADDR,                   /* #8 */
        CERT_MISC1_USED,
        SF_TLS_CERT_MISC1_CERTIFICATE_ADDR,
        CERT_MISC1_USED,
        SF_TLS_CERT_MISC1_PRIVATE_KEY_ADDR,
        CERT_MISC1_USED,
        SF_TLS_CERT_MISC1_DH_PARAMETER_ADDR,
        CERT_MISC1_USED,
        SF_TLS_CERT_MISC1_EXCHANGE_ADDR,
        CERT_MISC1_USED,

        SF_TLS_CERT_MISC2_CA_ADDR,                   /* #9 */
        CERT_MISC2_USED,
        SF_TLS_CERT_MISC2_CERTIFICATE_ADDR,
        CERT_MISC2_USED,
        SF_TLS_CERT_MISC2_PRIVATE_KEY_ADDR,
        CERT_MISC2_USED,
        SF_TLS_CERT_MISC2_DH_PARAMETER_ADDR,
        CERT_MISC2_USED,
        SF_TLS_CERT_MISC2_EXCHANGE_ADDR,
        CERT_MISC2_USED,

        SF_TLS_CERT_MISC3_CA_ADDR,                   /* #10 */
        CERT_MISC3_USED,
        SF_TLS_CERT_MISC3_CERTIFICATE_ADDR,
        CERT_MISC3_USED,
        SF_TLS_CERT_MISC3_PRIVATE_KEY_ADDR,
        CERT_MISC3_USED,
        SF_TLS_CERT_MISC3_DH_PARAMETER_ADDR,
        CERT_MISC3_USED,
        SF_TLS_CERT_MISC3_EXCHANGE_ADDR,
        CERT_MISC3_USED,

        SF_TLS_CERT_MISC4_CA_ADDR,                   /* #11 */
        CERT_MISC4_USED,
        SF_TLS_CERT_MISC4_CERTIFICATE_ADDR,
        CERT_MISC4_USED,
        SF_TLS_CERT_MISC4_PRIVATE_KEY_ADDR,
        CERT_MISC4_USED,
        SF_TLS_CERT_MISC4_DH_PARAMETER_ADDR,
        CERT_MISC4_USED,
        SF_TLS_CERT_MISC4_EXCHANGE_ADDR,
        CERT_MISC4_USED,

        SF_TLS_CERT_MISC5_CA_ADDR,                   /* #12 */
        CERT_MISC5_USED,
        SF_TLS_CERT_MISC5_CERTIFICATE_ADDR,
        CERT_MISC5_USED,
        SF_TLS_CERT_MISC5_PRIVATE_KEY_ADDR,
        CERT_MISC5_USED,
        SF_TLS_CERT_MISC5_DH_PARAMETER_ADDR,
        CERT_MISC5_USED,
        SF_TLS_CERT_MISC5_EXCHANGE_ADDR,
        CERT_MISC5_USED,

        SF_TLS_CERT_MISC6_CA_ADDR,                   /* #13 */
        CERT_MISC6_USED,
        SF_TLS_CERT_MISC6_CERTIFICATE_ADDR,
        CERT_MISC6_USED,
        SF_TLS_CERT_MISC6_PRIVATE_KEY_ADDR,
        CERT_MISC6_USED,
        SF_TLS_CERT_MISC6_DH_PARAMETER_ADDR,
        CERT_MISC6_USED,
        SF_TLS_CERT_MISC6_EXCHANGE_ADDR,
        CERT_MISC6_USED,

        SF_TLS_CERT_MISC7_CA_ADDR,                   /* #14 */
        CERT_MISC7_USED,
        SF_TLS_CERT_MISC7_CERTIFICATE_ADDR,
        CERT_MISC7_USED,
        SF_TLS_CERT_MISC7_PRIVATE_KEY_ADDR,
        CERT_MISC7_USED,
        SF_TLS_CERT_MISC7_DH_PARAMETER_ADDR,
        CERT_MISC7_USED,
        SF_TLS_CERT_MISC7_EXCHANGE_ADDR,
        CERT_MISC7_USED,

        SF_TLS_CERT_MISC8_CA_ADDR,                   /* #15 */
        CERT_MISC8_USED,
        SF_TLS_CERT_MISC8_CERTIFICATE_ADDR,
        CERT_MISC8_USED,
        SF_TLS_CERT_MISC8_PRIVATE_KEY_ADDR,
        CERT_MISC8_USED,
        SF_TLS_CERT_MISC8_DH_PARAMETER_ADDR,
        CERT_MISC8_USED,
        SF_TLS_CERT_MISC8_EXCHANGE_ADDR,
        CERT_MISC8_USED
    };

    for (i = 0; i < (int) (sizeof(flash_addr) / sizeof(flash_addr[0])); i = i + 2)
    {
        rm_cert_module_t module = RM_CERT_GetModule(flash_addr[i]);
        rm_cert_type_t   type   = RM_CERT_GetType(flash_addr[i]);

        if ((flash_addr[i + 1] == 1) && RM_CERT_IsExistCert(module, type))
        {
            cert_del_all = 1;
            err         += RM_CERT_Delete(module, type);
            cert_del_all = 0;
            printf(".");
            vTaskDelay(portCONVERT_MS_2_TICKS(200));
            any_deleted++;
        }
    }

    if (any_deleted)
    {
        printf("\n");
    }

 #if defined(__SUPPORT_ATCMD_TLS__)
    for (i = 0; i < ATCMD_CM_MAX_CERT_NUM; i++)
    {
        err += atcmd_cm_clear_cert_by_idx(i);
    }
 #endif                                /* __SUPPORT_ATCMD_TLS__ */
    return err;
}

rm_cert_err_t RM_CERT_Write (rm_cert_module_t module,
                             rm_cert_type_t   type,
                             rm_cert_format_t format,
                             uint8_t        * in,
                             size_t           inlen)
{
    rm_cert_err_t  err        = RM_CERT_ERR_OK;
    uint32_t       flash_addr = 0x00;
    config_enc_t * cfg_enc    = NULL;
    size_t         write_len;

    RM_CERT_INFO("module(%d), type(%d), format(%d), inlen(%d)\n", module, type, format, inlen);

    /* Check module */
    if (!rm_cert_is_valid_module(module))
    {
        RM_CERT_ERR("Invaild module(%d)\n", module);

        return RM_CERT_ERR_INVALID_MODULE;
    }

    /* Check type */
    if (!rm_cert_is_valid_type(module, type)) {
        RM_CERT_ERR("Invaild type(%d)\n", type);

        return RM_CERT_ERR_INVALID_TYPE;
    }

    /* Get flash memory address */
    flash_addr = rm_cert_get_flash_address(module, type);

    if (!flash_addr)
    {
        RM_CERT_ERR("Failed to get flash memory(%d,%d)\n", module, type);

        return RM_CERT_ERR_INVALID_FLASH_ADDR;
    }

    if ((flash_addr == SF_TLS_CERT_AWS_INITIAL_CERT_ADDR) ||
        (flash_addr == SF_TLS_CERT_AWS_INITIAL_PRIV_KEY_ADDR))
    {
        if (RM_CERT_IsExistCert(module, type))
        {
            RM_CERT_ERR("Already present. Write protected\n");
            return RM_CERT_ERR_NOK;
        }
    }

    if ((inlen + PACK_OVERHEAD) > CONFIG_MAX_PKG_LEN) {
        RM_CERT_ERR("RM_CERT_Write length is too big - 0x%x\n", inlen);

        return RM_CERT_ERR_INVALID_LENGTH;
    }

    cfg_enc = rm_cert_calloc(sizeof(config_enc_t), sizeof(uint8_t));

    if (cfg_enc == NULL)
    {
        return RM_CERT_ERR_MEM_FAILED;
    }

    err = RM_CERT_secure_asset_encrypt(ASSET_ID_CERT, in, (uint32_t) inlen, cfg_enc);

    if (err != RM_CERT_ERR_OK)
    {
        RM_CERT_ERR("secure_asset_encrypt - failed\n");
        goto end;
    }

    write_len = sizeof(cfg_enc->info) + (size_t) cfg_enc->info.encrypted_len;
    err       = rm_cert_write_by_addr(flash_addr, format, (uint8_t *) cfg_enc, write_len);

end:
    rm_cert_free(cfg_enc);
    cfg_enc = NULL;

    return err;
}

static rm_cert_err_t rm_cert_write_by_addr (uint32_t addr, rm_cert_format_t format, uint8_t * in, size_t inlen)
{
    rm_cert_err_t err         = RM_CERT_ERR_OK;
    rm_cert_t   * rm_cert_ptr = NULL;
    uint8_t     * buf         = NULL;

 #ifndef RRQ61X_OSPI_W_ENABLED
    int    offset = 0;
    size_t w;
 #endif                                //! RRQ61X_OSPI_W_ENABLED

    RM_CERT_INFO("flash memory address(0x%x)\n", (unsigned int) addr);

    /* Check format */
    if (!rm_cert_is_valid_format(format))
    {
        RM_CERT_ERR("Invaild format(%d)\n", format);

        return RM_CERT_ERR_INVALID_FORMAT;
    }

    /* Check length */
    if (!rm_cert_is_valid_length(inlen))
    {
        RM_CERT_ERR("Invaild length(%zu)\n", inlen);

        return RM_CERT_ERR_INVALID_LENGTH;
    }

    /* TODO: is required to check vaildation of certificate using parsing? */
    /* Check cert */
    if (!in)
    {
        RM_CERT_ERR("Invalid certificate\n");

        return RM_CERT_ERR_INVALID_PARAMS;
    }

    buf = rm_cert_calloc(FLASH_WRITE_LENGTH, sizeof(uint8_t));
    if (!buf)
    {
        RM_CERT_ERR("Failed to allocate memory(%zu)\n", FLASH_WRITE_LENGTH);

        return RM_CERT_ERR_MEM_FAILED;
    }

    /* Construct cert data */
    rm_cert_ptr                = (rm_cert_t *) buf;
    rm_cert_ptr->info.format   = (unsigned int) format;
    rm_cert_ptr->info.cert_len = inlen;
    memcpy(rm_cert_ptr->cert, in, inlen);

    if ((format == RM_CERT_FORMAT_PEM) && (strlen((const char *) in) == inlen))
    {
        rm_cert_ptr->info.cert_len += 1;
    }

    rm_cert_ptr = NULL;

 #ifdef RRQ61X_OSPI_W_ENABLED
    if (util_sflash_write((int) addr, (char *) buf, FLASH_WRITE_LENGTH) == pdTRUE)
    {
        err = RM_CERT_ERR_OK;
    }
    else
    {
        err = RM_CERT_ERR_NOK;
    }

 #else                                 ///////////////////////////////////////////////////////////////
    ad_flash_init();

    offset = ad_flash_update_possible(addr, (uint8_t *) buf, FLASH_WRITE_LENGTH);
    if (offset == FLASH_WRITE_LENGTH)
    {
        /* same content existing, no need to write to flash */
        err = RM_CERT_ERR_OK;
    }
    else
    {
        if (offset >= 0)
        {
            /* no need to erase flash */
        }
        else
        {
            /* offset < 0, erasing flash is needed */
            ad_flash_erase_region(addr, FLASH_WRITE_LENGTH);
            offset = 0;
        }

        w = offset + ad_flash_write(addr + offset, buf + offset, FLASH_WRITE_LENGTH - offset);

        /* check that the intended content-size is actually written in flash */
        if (w == FLASH_WRITE_LENGTH)
        {
            err = RM_CERT_ERR_OK;
        }
        else
        {
            err = RM_CERT_ERR_NOK;
        }
    }
 #endif                                /* RRQ61X_OSPI_W_ENABLED */

    /* Release cert data */
    if (buf)
    {
        rm_cert_free(buf);
        buf = NULL;
    }

    return err;
}

rm_cert_err_t RM_CERT_Read (rm_cert_module_t   module,
                            rm_cert_type_t     type,
                            rm_cert_format_t * format,
                            uint8_t          * out,
                            size_t           * outlen)
{
    uint32_t flash_addr = 0x00;

    RM_CERT_INFO("module(%d), type(%d), outlen(%d)\n", module, type, *outlen);

    if (format)
    {
        *format = RM_CERT_FORMAT_NONE;
    }

    /* Check module */
    if (!rm_cert_is_valid_module(module))
    {
        RM_CERT_ERR("Invaild module(%d)\n", module);

        return RM_CERT_ERR_INVALID_MODULE;
    }

    /* Check type */
    if (!rm_cert_is_valid_type(module, type)) {
        RM_CERT_ERR("Invaild type(%d)\n", type);

        return RM_CERT_ERR_INVALID_TYPE;
    }

    /* Get flash memory address */
    flash_addr = rm_cert_get_flash_address(module, type);
    if (!flash_addr)
    {
        RM_CERT_ERR("Failed to get flash memory(%d,%d)\n", module, type);

        return RM_CERT_ERR_INVALID_FLASH_ADDR;
    }

    return rm_cert_read_by_addr(flash_addr, format, out, outlen);
}

static rm_cert_err_t rm_cert_read_by_addr (uint32_t addr, rm_cert_format_t * format, uint8_t * out, size_t * outlen)
{
    rm_cert_err_t     err         = RM_CERT_ERR_OK;
    rm_cert_t       * rm_cert_ptr = NULL;
    uint8_t         * buf         = NULL;
    char            * cert_ptr    = NULL;
    size_t            cert_len    = 0;
    config_enc_info_t info;
    uint8_t         * asset_buf      = NULL;
    uint32_t          asset_size_u32 = 0U;
    uint8_t         * pkg_buf        = NULL;
    bool              is_enc         = pdFALSE;
    size_t            buflen         = FLASH_WRITE_LENGTH;

#if defined (__SUPPORT_ATCMD_TLS__)
    atcmd_cm_cert_t * atcmd_rm_cert_ptr = NULL;
    char            * at_cert           = NULL;
    unsigned char     at_type = 0;
    unsigned char     at_format = 0;
    size_t            at_cert_len = 0;
#endif /* __SUPPORT_ATCMD_TLS__ */

    RM_CERT_INFO("flash memory address(0x%x)\n", (unsigned int) addr);

    if (format)
    {
        *format = RM_CERT_FORMAT_NONE;
    }

    if (outlen == NULL)
    {
        return RM_CERT_ERR_NOK;
    }

    buf = rm_cert_calloc(buflen, sizeof(uint8_t)); /* need to modify! */
    if (!buf)
    {
        RM_CERT_ERR("Failed to allocate memory(%zu)\n", buflen);

        return RM_CERT_ERR_MEM_FAILED;
    }

    /* Read certificate */
 #ifdef RRQ61X_OSPI_W_ENABLED
    util_sflash_read((int) addr, (uint8_t *) buf, FLASH_WRITE_LENGTH);
 #else
    ad_flash_init();

    ad_flash_read(addr, (uint8_t *) buf, FLASH_WRITE_LENGTH);
 #endif                                /* RRQ61X_OSPI_W_ENABLED */

 #if defined (__SUPPORT_ATCMD_TLS__)
    if ((addr >= SFLASH_ATCMD_TLS_CERT_01) && (addr <= SFLASH_ATCMD_TLS_CERT_16))
    {
        atcmd_rm_cert_ptr = (atcmd_cm_cert_t *) buf;
        if (atcmd_rm_cert_ptr->info.cert_len == -1)
        {
            atcmd_rm_cert_ptr->info.cert_len = 0;
        }

        if (atcmd_rm_cert_ptr->info.cert_len > 0)
        {
            at_cert = rm_cert_calloc(atcmd_rm_cert_ptr->info.cert_len, sizeof(uint8_t));
            if (!at_cert)
            {
                RM_CERT_ERR("Failed to allocate memory(%zu)\n", ATCMD_CM_MAX_CERT_BODY);
                rm_cert_free(buf);
                return RM_CERT_ERR_MEM_FAILED;
            }
            memcpy(at_cert, atcmd_rm_cert_ptr->cert, atcmd_rm_cert_ptr->info.cert_len);
            at_type = atcmd_rm_cert_ptr->info.type;
            at_format = atcmd_rm_cert_ptr->info.format;
            at_cert_len = atcmd_rm_cert_ptr->info.cert_len;
        }
    }
#endif /* __SUPPORT_ATCMD_TLS__ */

    rm_cert_ptr = (rm_cert_t *) buf;

#if defined (__SUPPORT_ATCMD_TLS__)
    if (at_cert_len > 0)
    {
        memcpy(rm_cert_ptr->cert, at_cert, at_cert_len);
        memset(rm_cert_ptr->cert + at_cert_len, 0, RM_CERT_MAX_LENGTH - at_cert_len);
        rm_cert_ptr->info.type = at_type;
        rm_cert_ptr->info.format = at_format;
        rm_cert_ptr->info.cert_len = at_cert_len;
        rm_cert_free(at_cert);
    }
#endif /* __SUPPORT_ATCMD_TLS__ */

    /* Auto-detect encrypted format by header magic/asset_id.
     * If there isn't even enough data for the encrypted headers, treat as plain.
     */
    if (buflen < (sizeof(rm_cert_info_t) + sizeof(config_enc_info_t)))
    {
        goto plain;
    }

    memcpy(&info, rm_cert_ptr->cert, sizeof(info));

    is_enc = (info.magic == CONFIG_MAGIC) && (info.asset_id == ASSET_ID_CERT);

    if (is_enc)
    {
        if ((info.encrypted_len > CONFIG_MAX_PKG_LEN) ||
            (buflen < (sizeof(rm_cert_info_t) + sizeof(config_enc_info_t) + (size_t) info.encrypted_len)))
        {
            rm_cert_free(buf);

            return RM_CERT_ERR_NOK;
        }

        asset_buf = rm_cert_calloc(CONFIG_MAX_PKG_LEN, sizeof(uint8_t));
        if (asset_buf == NULL)
        {
            rm_cert_free(buf);

            return RM_CERT_ERR_MEM_FAILED;
        }

        pkg_buf = rm_cert_ptr->cert + sizeof(config_enc_info_t);
        err     = RM_CERT_secure_asset_decrypt(info.asset_id, pkg_buf, info.encrypted_len, asset_buf, &asset_size_u32);
        if (err != RM_CERT_ERR_OK)
        {
            RM_CERT_ERR("secure_asset_decrypt - failed\n");
            rm_cert_free(buf);
            rm_cert_free(asset_buf);

            return err;
        }

        if ((size_t) asset_size_u32 > CONFIG_MAX_PKG_LEN)
        {
            rm_cert_free(buf);
            rm_cert_free(asset_buf);

            return RM_CERT_ERR_NOK;
        }

        memcpy(rm_cert_ptr->cert, asset_buf, (size_t) asset_size_u32);
        rm_cert_ptr->info.cert_len = (unsigned int) asset_size_u32;
        rm_cert_free(asset_buf);
    }

plain:

    if (buf[0] == 0xFF)
    {
        /* No certificate */
        RM_CERT_INFO("Empty certificate\n");
        err = RM_CERT_ERR_EMPTY_CERTIFICATE;
    }
    else if (RM_CERT_IsPemFormat((const char *) rm_cert_ptr->cert))
    {
        if (out && *outlen < rm_cert_ptr->info.cert_len)
        {
            RM_CERT_ERR("Not enough space(%zu:%zu)\n", *outlen, rm_cert_ptr->info.cert_len);
            err = RM_CERT_ERR_INVALID_LENGTH;
        }

        cert_ptr = (char *) rm_cert_ptr->cert;
        cert_len = rm_cert_ptr->info.cert_len;

        if ((err == RM_CERT_ERR_OK) && format)
        {
            *format = RM_CERT_FORMAT_PEM;
        }
    }
    else
    {
        if (out && *outlen < rm_cert_ptr->info.cert_len)
        {
            RM_CERT_ERR("Not enough space(%zu:%u)\n", *outlen, rm_cert_ptr->info.cert_len);
            err = RM_CERT_ERR_INVALID_LENGTH;
        }

        cert_ptr = (char *) rm_cert_ptr->cert;
        cert_len = rm_cert_ptr->info.cert_len;

        if ((err == RM_CERT_ERR_OK) && format)
        {
            *format = (int) rm_cert_ptr->info.format;
        }
    }

    if ((err == RM_CERT_ERR_OK) && cert_ptr && cert_len)
    {
        if (out)
        {
            memcpy(out, cert_ptr, cert_len);
        }
        *outlen = cert_len;
    }

    if (err)
    {
 #if !defined(RM_CERT_ENABLE_DBG_INFO)
        if (err != RM_CERT_ERR_EMPTY_CERTIFICATE)
 #endif                                /* ! RM_CERT_ENABLE_DBG_INFO */
        {
            RM_CERT_ERR("Failed to get certificate"
                        "(addr:0x%x, outlen:%zu, err:%d)\n",
                        (unsigned int) addr,
                        *outlen,
                        err);
        }

        goto end;
    }

end:

    cert_ptr    = NULL;
    cert_len    = 0;
    rm_cert_ptr = NULL;

    if (buf)
    {
        rm_cert_free(buf);
        buf = NULL;
    }

    return err;
}

rm_cert_err_t RM_CERT_Delete (rm_cert_module_t module, rm_cert_type_t type)
{
    uint32_t flash_addr = 0x00;

    RM_CERT_INFO("module(%d), type(%d)\n", module, type);

    /* Check module */
    if (!rm_cert_is_valid_module(module))
    {
        RM_CERT_ERR("Invaild module(%d)\n", module);

        return RM_CERT_ERR_INVALID_MODULE;
    }

    /* Check type */
    if (!rm_cert_is_valid_type(module, type)) {
        RM_CERT_ERR("Invaild type(%d)\n", type);

        return RM_CERT_ERR_INVALID_TYPE;
    }

    /* Get flash memory address */
    flash_addr = rm_cert_get_flash_address(module, type);
    if (!flash_addr)
    {
        RM_CERT_ERR("Failed to get flash memory(%d,%d)\n", module, type);

        return RM_CERT_ERR_INVALID_FLASH_ADDR;
    }

    if ((flash_addr == SF_TLS_CERT_AWS_INITIAL_CERT_ADDR) ||
        (flash_addr == SF_TLS_CERT_AWS_INITIAL_PRIV_KEY_ADDR))
    {
        if (RM_CERT_IsExistCert(module, type))
        {
            RM_CERT_ERR("Cannot be deleted\n");
            return RM_CERT_ERR_NOK;
        }
    }

    return rm_cert_delete_by_addr(flash_addr);
}

static rm_cert_err_t rm_cert_delete_by_addr (uint32_t addr)
{
    rm_cert_err_t err = RM_CERT_ERR_OK;
    RM_CERT_INFO("flash memory address(0x%x)\n", (unsigned int) addr);

 #ifdef RRQ61X_OSPI_W_ENABLED
    err = util_sflash_erase((int) addr, FLASH_WRITE_LENGTH);
 #else
    err = ad_flash_erase_region(addr, FLASH_WRITE_LENGTH);
 #endif                                /* RRQ61X_OSPI_W_ENABLED */

    if (err != true)
    {
        RM_CERT_ERR("Failed to erase data(0x%x)\n", (unsigned int) addr);
        err = RM_CERT_ERR_NOK;
    }
    else
    {
        err = RM_CERT_ERR_OK;
    }

    return err;
}

int RM_CERT_IsExistCert (rm_cert_module_t module, rm_cert_type_t type)
{
    int err = RM_CERT_ERR_OK;
    size_t len = RM_CERT_MAX_LENGTH;

    RM_CERT_INFO("module(%d), type(%d)\n", module, type);

    err = RM_CERT_Read(module, type, NULL, NULL, &len);
    if (err == RM_CERT_ERR_OK) {
        return pdTRUE;
    }

    return pdFALSE;
}

rm_cert_err_t RM_CERT_secure_asset_encrypt (uint32_t       asset_id,
                                            uint8_t      * asset_buf,
                                            uint32_t       asset_size,
                                            config_enc_t * cfg_enc)
{
    rm_cert_err_t err = RM_CERT_ERR_OK;
    uint32_t      lcs;
    uint32_t      rc = CC_BsvLcsGet(RRQ61X_ACRYPT_BASE, &lcs);
    int32_t       pkg_size_sign;
    uint32_t      aligned_size;
    uint8_t     * local_buf = NULL;

    if ((cfg_enc == NULL) || (asset_buf == NULL) || (asset_size == 0U))
    {
        return RM_CERT_ERR_INVALID_PARAMS;
    }

    aligned_size = (((asset_size + 15U) >> 4) << 4);

    if (aligned_size > (CONFIG_MAX_PKG_LEN - PACK_OVERHEAD))
    {
        RM_CERT_ERR("RM_CERT_secure_asset length is too big - (0x%lx > 0x%x)\n", aligned_size,
                    (CONFIG_MAX_PKG_LEN - PACK_OVERHEAD));

        return RM_CERT_ERR_INVALID_LENGTH;
    }

    cfg_enc->info.magic    = CONFIG_MAGIC;
    cfg_enc->info.asset_id = asset_id;

    local_buf = rm_cert_calloc(aligned_size, sizeof(uint8_t));

    if (local_buf == NULL)
    {
        return RM_CERT_ERR_MEM_FAILED;
    }

    /* ALWAYS pad into RAM (asset_buf might be in flash/rodata) */
    memcpy(local_buf, asset_buf, asset_size);

    if ((rc == 0U) && (lcs >= (uint32_t) RM_CERT_DEVICE_MANUFACTURE_LCS))
    {
        pkg_size_sign = R_CC312_Secure_Asset_RuntimePack(ASSET_KCP_KEY,
                                                         0,
                                                         NULL,
                                                         asset_id,
                                                         "RunPack",
                                                         local_buf,
                                                         aligned_size,
                                                         cfg_enc->encrypted_data);
    }
    else
    {
        pkg_size_sign = R_CC312_Secure_Asset_RuntimePack(ASSET_USER_KEY,
                                                         0,
                                                         &userKeyData_cert,
                                                         asset_id,
                                                         "RunPack",
                                                         local_buf,
                                                         aligned_size,
                                                         cfg_enc->encrypted_data);
    }

    if ((pkg_size_sign > 0) && ((uint32_t) pkg_size_sign <= CONFIG_MAX_PKG_LEN))
    {
        cfg_enc->info.encrypted_len = (uint32_t) pkg_size_sign;
    }
    else
    {
        err = RM_CERT_ERR_NOK;
    }

    rm_cert_free(local_buf);

    return err;
}

rm_cert_err_t RM_CERT_secure_asset_decrypt (uint32_t   asset_id,
                                            uint8_t  * pkg_buf,
                                            uint32_t   pkg_size,
                                            uint8_t  * asset_buf,
                                            uint32_t * asset_size)
{
    rm_cert_err_t err = RM_CERT_ERR_OK;
    uint32_t      lcs;
    uint32_t      rc = CC_BsvLcsGet(RRQ61X_ACRYPT_BASE, &lcs);
    int32_t       asset_size_sign;

    if ((pkg_buf == NULL) || (pkg_size == 0) ||
        (asset_buf == NULL) || (asset_size == NULL))
    {
        return RM_CERT_ERR_INVALID_PARAMS;
    }

    if (pkg_size > CONFIG_MAX_PKG_LEN)
    {
        return RM_CERT_ERR_INVALID_PARAMS;
    }

    if ((rc == 0U) && (lcs >= (uint32_t) RM_CERT_DEVICE_MANUFACTURE_LCS))
    {
        asset_size_sign =
            R_CC312_Secure_Asset_RuntimeUnpack(ASSET_KCP_KEY, NULL, asset_id, pkg_buf, pkg_size, asset_buf);
    }
    else
    {
        asset_size_sign = R_CC312_Secure_Asset_RuntimeUnpack(ASSET_USER_KEY,
                                                             &userKeyData_cert,
                                                             asset_id,
                                                             pkg_buf,
                                                             pkg_size,
                                                             asset_buf);
    }

    if ((asset_size_sign > 0) && ((uint32_t) asset_size_sign < CONFIG_MAX_PKG_LEN))
    {
        *asset_size = (uint32_t) asset_size_sign;
    }
    else
    {
        err = RM_CERT_ERR_NOK;
    }

    return err;
}

#endif

/* EOF */
