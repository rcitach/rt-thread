/*
 * Copyright (c) 2001-2019, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR Arm’s non-OSI source license
 */

#ifndef MBEDTLS_AES_ALT_H
 #define MBEDTLS_AES_ALT_H

 #include "common.h"
 #include "aes_driver.h"

 #include <stddef.h>
 #include <stdint.h>

 #ifdef __cplusplus
extern "C" {
 #endif

 #define MBEDTLS_AES_CONTEXT_SIZE_IN_WORDS    24

/**
 * \brief          AES context structure
 *
 * \note           Max len of key - 256.
 */
typedef struct mbedtls_aes_context
{
    uint32_t buf[MBEDTLS_AES_CONTEXT_SIZE_IN_WORDS];
} mbedtls_aes_context;

 #if defined(MBEDTLS_CIPHER_MODE_XTS)

/**
 * \brief The AES XTS context-type definition.
 */
typedef struct mbedtls_aes_xts_context
{
    mbedtls_aes_context crypt;         /*!< The AES context to use for AES block encryption or decryption. */
    mbedtls_aes_context tweak;         /*!< The AES context used for tweak computation. */
} mbedtls_aes_xts_context;
 #endif /* MBEDTLS_CIPHER_MODE_XTS */

 #ifdef __cplusplus
}
 #endif

#endif                                 /* aes_alt.h */
