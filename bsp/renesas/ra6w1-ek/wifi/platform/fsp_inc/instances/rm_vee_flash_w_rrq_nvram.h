/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef RM_VEE_FLASH_W_RRQ_NVRAM_H
#define RM_VEE_FLASH_W_RRQ_NVRAM_H

#include "rm_vee_api.h"

#define ENV_GROUP_ALL         "all"
#define ENV_GROUP_BOOTCFG     "bootcfg"
#define ENV_GROUP_DEVCFG      "devcfg"
#define ENV_GROUP_WIFICFG     "wificfg"
#define ENV_GROUP_SYSCFG      "syscfg"
#define ENV_GROUP_APPCFG      "appcfg"
#define ENV_GROUP_BLECFG      "blecfg"
#define ENV_GROUP_BLESEC      "blesec"
#define ENV_GROUP_TESTCFG     "testcfg"
#define ENV_GROUP_WIFIPROFILE "wifiprofile"

int read_nvram_uint(const char* group, const char *name, int *_val);
int write_nvram_uint(const char* group, const char *name, int val);
int read_nvram_int(const char* group, const char *name, int *_val);
int write_nvram_int(const char* group, const char *name, int val);
char *read_nvram_string(const char* group, const char *name);
int write_nvram_string(const char* group, const char *name, const char *val);
int delete_nvram_env(const char* group, const char *name);
int clear_nvram_envall(const char* group);

#endif
/*******************************************************************************************************************//**
 * @} (end defgroup RM_VEE_FLASH_W_RRQ_NVRAM_H)
 **********************************************************************************************************************/
