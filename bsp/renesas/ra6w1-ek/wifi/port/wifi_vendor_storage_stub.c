#include <stddef.h>
#include <stdint.h>

#include "bsp_api.h"

/*
 * The prebuilt Wi-Fi archives reference the vendor persistence ABI even when
 * the standalone driver does not use persistent configuration.  Keep only the
 * required symbols until an RT-Thread FAL-backed implementation is added.
 */
static uint32_t g_wifi_storage_stub_ctrl;

void * RM_MAP_PERSISTANT_W_get_ctrl(void)
{
    return &g_wifi_storage_stub_ctrl;
}

fsp_err_t RM_MAP_PERSISTANT_W_Read_INT(void * const p_ctrl,
                                      const char * group,
                                      const char * name,
                                      int * value)
{
    FSP_PARAMETER_NOT_USED(p_ctrl);
    FSP_PARAMETER_NOT_USED(group);
    FSP_PARAMETER_NOT_USED(name);

    if (value)
    {
        *value = 0;
    }

    return FSP_ERR_NOT_FOUND;
}

fsp_err_t RM_MAP_PERSISTANT_W_Read_STRING(void * const p_ctrl,
                                         const char * group,
                                         const char * name,
                                         char ** value)
{
    FSP_PARAMETER_NOT_USED(p_ctrl);
    FSP_PARAMETER_NOT_USED(group);
    FSP_PARAMETER_NOT_USED(name);

    if (value)
    {
        *value = NULL;
    }

    return FSP_ERR_NOT_FOUND;
}

fsp_err_t RM_MAP_PERSISTANT_W_Write_INT(void * const p_ctrl,
                                       const char * group,
                                       const char * name,
                                       int value)
{
    FSP_PARAMETER_NOT_USED(p_ctrl);
    FSP_PARAMETER_NOT_USED(group);
    FSP_PARAMETER_NOT_USED(name);
    FSP_PARAMETER_NOT_USED(value);

    return FSP_ERR_UNSUPPORTED;
}

/* RM_CERT_ERR_EMPTY_CERTIFICATE from the removed vendor rm_cert interface. */
int RM_CERT_Read(int module, int type, int * format, uint8_t * output, size_t * output_length)
{
    FSP_PARAMETER_NOT_USED(module);
    FSP_PARAMETER_NOT_USED(type);
    FSP_PARAMETER_NOT_USED(output);

    if (format)
    {
        *format = -1;
    }
    if (output_length)
    {
        *output_length = 0;
    }

    return 10;
}

void wifi_conn_fail_noti_to_atcmd_host(void)
{
}

int is_in_softap_acs_mode(void)
{
    return 0;
}
