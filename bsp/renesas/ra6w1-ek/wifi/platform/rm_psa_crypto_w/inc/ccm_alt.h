/*
 * Copyright (c) 2001-2019, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR Arm’s non-OSI source license
 */

#ifndef MBEDTLS_CCM_ALT_H
 #define MBEDTLS_CCM_ALT_H

 #include "mbedtls/ccm.h"
 #include "mbedtls/cipher.h"
 #include "aesccm_driver.h"

 #define MBEDTLS_AESCCM_STAR_NONCE_SIZE_BYTES             13 /*! The size of the AES CCM star nonce in bytes. */
 #define MBEDTLS_AESCCM_STAR_SOURCE_ADDRESS_SIZE_BYTES    8  /*! The size of source address of the AES CCM star in bytes. */
 #define MBEDTLS_AESCCM_MODE_CCM                          0  /*! AES CCM mode: CCM. */
 #define MBEDTLS_AESCCM_MODE_STAR                         1  /*! AES CCM mode: CCM star. */
 #define MBEDTLS_CCM_CONTEXT_SIZE_IN_WORDS                264

 #ifdef __cplusplus
extern "C" {
 #endif

/**
 * \brief          The CCM context-type definition. The CCM context is passed
 *                 to the APIs called.
 */
typedef struct mbedtls_ccm_context
{
    uint32_t buf[MBEDTLS_CCM_CONTEXT_SIZE_IN_WORDS];
} mbedtls_ccm_context;

int  mbedtls_ccm_get_security_level(uint8_t sizeOfT, uint8_t * pSecurityLevel);
void mbedtls_ccm_init_int(mbedtls_ccm_context * ctx);
int  mbedtls_ccm_setkey_int(mbedtls_ccm_context * ctx,
                            mbedtls_cipher_id_t   cipher,
                            const unsigned char * key,
                            unsigned int          keybits);
void mbedtls_ccm_free_int(mbedtls_ccm_context * ctx);
int  mbedtls_ccm_encrypt_and_tag_int(mbedtls_ccm_context * ctx,
                                     size_t                length,
                                     const unsigned char * iv,
                                     size_t                iv_len,
                                     const unsigned char * add,
                                     size_t                add_len,
                                     const unsigned char * input,
                                     unsigned char       * output,
                                     unsigned char       * tag,
                                     size_t                tag_len,
                                     uint32_t              ccmMode);
int mbedtls_ccm_auth_decrypt_int(mbedtls_ccm_context * ctx,
                                 size_t                length,
                                 const unsigned char * iv,
                                 size_t                iv_len,
                                 const unsigned char * add,
                                 size_t                add_len,
                                 const unsigned char * input,
                                 unsigned char       * output,
                                 const unsigned char * tag,
                                 size_t                tag_len,
                                 uint32_t              ccmMode);

 #ifdef __cplusplus
}
 #endif

#endif                                 /* MBEDTLS_CCM_ALT_H */
