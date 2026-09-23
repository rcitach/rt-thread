/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/
#ifndef MBEDTLS_SHA1_ALT_H
 #define MBEDTLS_SHA1_ALT_H

 #include "common.h"
 #include "sha_alt.h"

 #include <stddef.h>
 #include <stdint.h>

 #ifdef __cplusplus
extern "C" {
 #endif

 #define MBEDTLS_ERR_SHA1_HW_ACCEL_FAILED          -0x0035 /**< SHA-1 hardware accelerator failed */
 #define SIZE_MBEDTLS_SHA1_PROCESS_BUFFER_BYTES    60U

/**
 * \brief          SHA-1 context structure
 */
typedef struct mbedtls_sha1_context
{
    uint32_t buff[SIZE_MBEDTLS_SHA1_PROCESS_BUFFER_BYTES]; /*!< Internal buffer used by SHA1 operation. */
} mbedtls_sha1_context;

 #ifdef __cplusplus
}
 #endif

#endif                                 /* MBEDTLS_SHA1_ALT_H */
