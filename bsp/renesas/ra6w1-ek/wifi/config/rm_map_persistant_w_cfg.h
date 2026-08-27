/* generated configuration header file - do not edit */
#ifndef RM_MAP_PERSISTANT_W_CFG_H_
#define RM_MAP_PERSISTANT_W_CFG_H_
#ifdef __cplusplus
extern "C" {
#endif
#ifndef RM_MAP_PERSISTANT_W
#define RM_MAP_PERSISTANT_W
#endif
#define dg_configADNVPARAM_PROJ_FILE               "wifi_platform_nvparam.h"

#ifdef dg_configFLASH_ADAPTER
#undef dg_configFLASH_ADAPTER
#endif
#define dg_configFLASH_ADAPTER                     ( 1 )

#define dg_configNVMS_ADAPTER                      ( 1 )
#define dg_configNVMS_VES                          ( 1 )

#ifdef dg_configNVPARAM_ADAPTER
#undef dg_configNVPARAM_ADAPTER
#endif
#define dg_configNVPARAM_ADAPTER                   ( 1 ) // 1: NVRAM, 0: NVRAM Emulator

#if dg_configNVPARAM_ADAPTER
#define dg_configNVPARAM_ADAPTERv2             ( 1 )
#endif /* dg_configNVPARAM_ADAPTER */

#ifdef dg_configNVPARAM_APP_AREA
#undef dg_configNVPARAM_APP_AREA
#endif
#define dg_configNVPARAM_APP_AREA                  ( 2 )

#ifdef __cplusplus
}
#endif
#endif /* RM_MAP_PERSISTANT_W_CFG_H_ */
