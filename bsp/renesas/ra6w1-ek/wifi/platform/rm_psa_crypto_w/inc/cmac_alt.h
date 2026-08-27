/*
 * Copyright (c) 2001-2019, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR Arm’s non-OSI source license
 */

#ifndef MBEDTLS_CMAC_ALT_H
 #define MBEDTLS_CMAC_ALT_H

 #include "mbedtls/cipher.h"
 #include "aes_driver.h"

 #ifdef __cplusplus
extern "C" {
 #endif

 #define MBEDTLS_CMAC_CONTEXT_SIZE_IN_WORDS    33
 #define PSA_CMAC_BITS_VENDOR_RAW(bit_length)    (0) // Wrapped keys are unsupported

/**
 * \brief          CMAC cipher context structure
 */
typedef struct mbedtls_cmac_context_t
{
    /*! Internal buffer */
    uint32_t buf[MBEDTLS_CMAC_CONTEXT_SIZE_IN_WORDS];
} mbedtls_cmac_context_t;

 #ifdef __cplusplus
}
 #endif

#endif                                 /* MBEDTLS_CMAC_ALT_H */
