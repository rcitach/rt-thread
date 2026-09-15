/*
 * Copyright (c) 2001-2019, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR Arm’s non-OSI source license
 */

#ifndef MBEDTLS_POLY1305_ALT_H
 #define MBEDTLS_POLY1305_ALT_H
 #include "poly.h"
 #include "chacha_driver.h"

 #define MBEDTLS_ERR_POLY1305_FEATURE_UNAVAILABLE    -0x0057
 #define MBEDTLS_ERR_POLY1305_HW_ACCEL_FAILED        -0x0059

 #ifdef __cplusplus
extern "C"
{
 #endif

/************************ defines  ****************************/
 #define MBEDTLS_POLY_KEY_SIZE_WORDS    8  /*! The size of the POLY key in words. */
 #define MBEDTLS_POLY_KEY_SIZE_BYTES    32 /*! The size of the POLY key in bytes. */
 #define MBEDTLS_POLY_MAC_SIZE_WORDS    4  /*! The size of the POLY MAC in words. */
 #define MBEDTLS_POLY_MAC_SIZE_BYTES    16 /*! The size of the POLY MAC in bytes. */

/************************ Typedefs  ****************************/
/*! The definition of the ChaCha-MAC buffer. */
typedef uint32_t mbedtls_poly_mac[MBEDTLS_POLY_MAC_SIZE_WORDS];

/*! The definition of the ChaCha-key buffer. */
typedef uint32_t mbedtls_poly_key[MBEDTLS_POLY_KEY_SIZE_WORDS];

typedef struct mbedtls_poly1305_context
{
    uint32_t r[4];                     /** The value for 'r' (low 128 bits of the key). */
    uint32_t s[4];                     /** The value for 's' (high 128 bits of the key). */
    uint32_t acc[5];                   /** The accumulator number. */
    uint8_t  queue[16];                /** The current partial block of data. */
    size_t   queue_len;                /** The number of bytes stored in 'queue'. */
} mbedtls_poly1305_context;

 #ifdef __cplusplus
}
 #endif

#endif                                 /* MBEDTLS_POLY1305_ALT_H */
