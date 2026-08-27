/*
 * Copyright (c) 2001-2019, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR Arm’s non-OSI source license
 */

#ifndef MBEDTLS_GCM_ALT_H
 #define MBEDTLS_GCM_ALT_H

 #include "aesgcm_driver.h"

 #define MBEDTLS_GCM_CONTEXT_SIZE_IN_WORDS    40

 #ifdef __cplusplus
extern "C" {
 #endif

/**
 * \brief          GCM context structure
 */
typedef struct
{
    uint32_t buf[MBEDTLS_GCM_CONTEXT_SIZE_IN_WORDS];
} mbedtls_gcm_context;

 #ifdef __cplusplus
}
 #endif

#endif                                 /* MBEDTLS_GCM_ALT_H */
