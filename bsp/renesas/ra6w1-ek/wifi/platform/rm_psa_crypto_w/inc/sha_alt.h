/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/
#ifndef MBEDTLS_SHA_ALT_H
 #define MBEDTLS_SHA_ALT_H

 #include "common.h"
 #include "mbedtls/platform.h"
 #include "mbedtls/platform_util.h"
 #include "hash_driver.h"

 #include <stddef.h>
 #include <stdint.h>

 #ifdef __cplusplus
extern "C" {
 #endif

void mbedtls_sha_init_internal(void * ctx);
int  mbedtls_sha_process_internal(void * ctx, const unsigned char * data);
int  mbedtls_sha_finish_internal(void * ctx);
int  mbedtls_sha_update_internal(void * ctx, const unsigned char * input, size_t ilen);
int  mbedtls_sha_starts_internal(void * ctx, hashMode_t mode);

 #ifdef __cplusplus
}
 #endif

#endif                                 /* MBEDTLS_SHA1_ALT_H */
