/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/
#ifndef MBEDTLS_SHA256_ALT_H
 #define MBEDTLS_SHA256_ALT_H

 #include "common.h"
 #include "sha_alt.h"

 #include <stddef.h>
 #include <stdint.h>

 #ifdef __cplusplus
extern "C" {
 #endif

 #define MBEDTLS_ERR_SHA256_HW_ACCEL_FAILED          -0x0037 /**< SHA-256 hardware accelerator failed */
 #define SIZE_MBEDTLS_SHA256_PROCESS_BUFFER_BYTES    60U

/**
 * \brief          SHA-256 context structure
 */
typedef struct mbedtls_sha256_context
{
    /*! Internal buffer */
    uint32_t buff[SIZE_MBEDTLS_SHA256_PROCESS_BUFFER_BYTES]; /*!< Internal buffer used by SHA256 operation. */
} mbedtls_sha256_context;

 #ifdef __cplusplus
}
 #endif

#endif                                 /* MBEDTLS_SHA256_ALT_H */
