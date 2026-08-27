/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/**
 *  aes_alt.h
 *
 *  This file contains AES definitions and functions for the alternate implementation.
 *
 */

#ifndef AES_VENDOR_H
 #define AES_VENDOR_H

 #include "common.h"

 #include <stddef.h>
 #include <stdint.h>

 #include "vendor.h"
 #include "mbedtls/aes.h"

 #ifdef __cplusplus
extern "C"
{
 #endif

 #define PSA_AES_BITS_VENDOR_RAW(bit_length)    (0) // Wrapped keys are unsupported

 #ifdef __cplusplus
}
 #endif

#endif                                 /* AES_VENDOR_H */
