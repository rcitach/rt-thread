/**
 * \file pkcs5_alt.c
 *
 * \brief PKCS#5 functions
 *
 * \author Mathias Olsson <mathias@kompetensum.com>
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/*
 * PKCS#5 includes PBKDF2 and more
 *
 * http://tools.ietf.org/html/rfc2898 (Specification)
 * http://tools.ietf.org/html/rfc6070 (Test vectors)
 */

#include "common.h"

#if defined(MBEDTLS_PKCS5_C)

 #include "mbedtls/pkcs5.h"
 #include "mbedtls/error.h"

 #if defined(MBEDTLS_ASN1_PARSE_C)
  #include "mbedtls/asn1.h"
  #include "mbedtls/cipher.h"
  #include "mbedtls/oid.h"
 #endif                                /* MBEDTLS_ASN1_PARSE_C */

 #include <string.h>

 #if defined(MBEDTLS_PLATFORM_C)
  #include "mbedtls/platform.h"
 #else
  #include <stdio.h>
  #define mbedtls_printf    printf
 #endif

 #if defined(MBEDTLS_ASN1_PARSE_C)
static int pkcs5_parse_pbkdf2_params (const mbedtls_asn1_buf * params,
                                      mbedtls_asn1_buf       * salt,
                                      int                    * iterations,
                                      int                    * keylen,
                                      mbedtls_md_type_t      * md_type)
{
    int                   ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_asn1_buf      prf_alg_oid;
    unsigned char       * p   = params->p;
    const unsigned char * end = params->p + params->len;

    if (params->tag != (MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE))
    {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PKCS5_INVALID_FORMAT, MBEDTLS_ERR_ASN1_UNEXPECTED_TAG);
    }

    /*
     *  PBKDF2-params ::= SEQUENCE {
     *    salt              OCTET STRING,
     *    iterationCount    INTEGER,
     *    keyLength         INTEGER OPTIONAL
     *    prf               AlgorithmIdentifier DEFAULT algid-hmacWithSHA1
     *  }
     *
     */
    if ((ret = mbedtls_asn1_get_tag(&p, end, &salt->len, MBEDTLS_ASN1_OCTET_STRING)) != 0)
    {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PKCS5_INVALID_FORMAT, ret);
    }

    salt->p = p;
    p      += salt->len;

    if ((ret = mbedtls_asn1_get_int(&p, end, iterations)) != 0)
    {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PKCS5_INVALID_FORMAT, ret);
    }

    if (p == end)
    {
        return 0;
    }

    if ((ret = mbedtls_asn1_get_int(&p, end, keylen)) != 0)
    {
        if (ret != MBEDTLS_ERR_ASN1_UNEXPECTED_TAG)
        {
            return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PKCS5_INVALID_FORMAT, ret);
        }
    }

    if (p == end)
    {
        return 0;
    }

    if ((ret = mbedtls_asn1_get_alg_null(&p, end, &prf_alg_oid)) != 0)
    {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PKCS5_INVALID_FORMAT, ret);
    }

    if (mbedtls_oid_get_md_hmac(&prf_alg_oid, md_type) != 0)
    {
        return MBEDTLS_ERR_PKCS5_FEATURE_UNAVAILABLE;
    }

    if (p != end)
    {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PKCS5_INVALID_FORMAT, MBEDTLS_ERR_ASN1_LENGTH_MISMATCH);
    }

    return 0;
}

int mbedtls_pkcs5_pbes2 (const mbedtls_asn1_buf * pbe_params,
                         int                      mode,
                         const unsigned char    * pwd,
                         size_t                   pwdlen,
                         const unsigned char    * data,
                         size_t                   datalen,
                         unsigned char          * output)
{
    int                           ret, iterations = 0, keylen = 0;
    unsigned char               * p, * end;
    mbedtls_asn1_buf              kdf_alg_oid, enc_scheme_oid, kdf_alg_params, enc_scheme_params;
    mbedtls_asn1_buf              salt;
    mbedtls_md_type_t             md_type = MBEDTLS_MD_SHA1;
    unsigned char                 key[32], iv[32];
    size_t                        olen = 0;
    const mbedtls_md_info_t     * md_info;
    const mbedtls_cipher_info_t * cipher_info;
    mbedtls_md_context_t          md_ctx;
    mbedtls_cipher_type_t         cipher_alg;
    mbedtls_cipher_context_t      cipher_ctx;

    p   = pbe_params->p;
    end = p + pbe_params->len;

    /*
     *  PBES2-params ::= SEQUENCE {
     *    keyDerivationFunc AlgorithmIdentifier {{PBES2-KDFs}},
     *    encryptionScheme AlgorithmIdentifier {{PBES2-Encs}}
     *  }
     */
    if (pbe_params->tag != (MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE))
    {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PKCS5_INVALID_FORMAT, MBEDTLS_ERR_ASN1_UNEXPECTED_TAG);
    }

    if ((ret = mbedtls_asn1_get_alg(&p, end, &kdf_alg_oid, &kdf_alg_params)) != 0)
    {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PKCS5_INVALID_FORMAT, ret);
    }

    // Only PBKDF2 supported at the moment
    //
    if (MBEDTLS_OID_CMP(MBEDTLS_OID_PKCS5_PBKDF2, &kdf_alg_oid) != 0)
    {
        return MBEDTLS_ERR_PKCS5_FEATURE_UNAVAILABLE;
    }

    if ((ret = pkcs5_parse_pbkdf2_params(&kdf_alg_params, &salt, &iterations, &keylen, &md_type)) != 0)
    {
        return ret;
    }

    md_info = mbedtls_md_info_from_type(md_type);
    if (md_info == NULL)
    {
        return MBEDTLS_ERR_PKCS5_FEATURE_UNAVAILABLE;
    }

    if ((ret = mbedtls_asn1_get_alg(&p, end, &enc_scheme_oid, &enc_scheme_params)) != 0)
    {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PKCS5_INVALID_FORMAT, ret);
    }

    if (mbedtls_oid_get_cipher_alg(&enc_scheme_oid, &cipher_alg) != 0)
    {
        return MBEDTLS_ERR_PKCS5_FEATURE_UNAVAILABLE;
    }

    cipher_info = mbedtls_cipher_info_from_type(cipher_alg);
    if (cipher_info == NULL)
    {
        return MBEDTLS_ERR_PKCS5_FEATURE_UNAVAILABLE;
    }

    /*
     * The value of keylen from pkcs5_parse_pbkdf2_params() is ignored
     * since it is optional and we don't know if it was set or not
     */
    keylen = (int) (cipher_info->key_bitlen / 8);

    if ((enc_scheme_params.tag != MBEDTLS_ASN1_OCTET_STRING) ||
        (enc_scheme_params.len != cipher_info->iv_size))
    {
        return MBEDTLS_ERR_PKCS5_INVALID_FORMAT;
    }

    mbedtls_md_init(&md_ctx);
    mbedtls_cipher_init(&cipher_ctx);

    memcpy(iv, enc_scheme_params.p, enc_scheme_params.len);

    if ((ret = mbedtls_md_setup(&md_ctx, md_info, 1)) != 0)
    {
        goto exit;
    }

    if ((ret =
             mbedtls_pkcs5_pbkdf2_hmac(&md_ctx, pwd, pwdlen, salt.p, salt.len, (uint32_t) iterations, (uint32_t) keylen,
                                       key)) != 0)
    {
        goto exit;
    }

    if ((ret = mbedtls_cipher_setup(&cipher_ctx, cipher_info)) != 0)
    {
        goto exit;
    }

    if ((ret = mbedtls_cipher_setkey(&cipher_ctx, key, 8 * keylen, (mbedtls_operation_t) mode)) != 0)
    {
        goto exit;
    }

    if ((ret = mbedtls_cipher_crypt(&cipher_ctx, iv, enc_scheme_params.len, data, datalen, output, &olen)) != 0)
    {
        ret = MBEDTLS_ERR_PKCS5_PASSWORD_MISMATCH;
    }

exit:
    mbedtls_md_free(&md_ctx);
    mbedtls_cipher_free(&cipher_ctx);

    return ret;
}

 #endif                                /* MBEDTLS_ASN1_PARSE_C */

 #ifdef MBEDTLS_PKCS5_ALT
  #define PCKS5_RENESAS_HW
 #else
  #undef  PCKS5_RENESAS_HW
 #endif

 #ifdef PCKS5_RENESAS_HW

  #define HW_PBKDF2_CLK       0x400c0000 // amba clock
  #define HW_PBKDF2_START     0x40031000
  #define HW_PBKDF2_IHV1      0x40031004
  #define HW_PBKDF2_IHV2      0x40031018
  #define HW_PBKDF2_DATA      0x4003102c
  #define HW_PBKDF2_DIGEST    0x40031040

  #define MEM_LONG_READ(addr, data)     *data = *((volatile unsigned int *) addr)
  #define MEM_LONG_WRITE(addr, data)    *((volatile unsigned int *) addr) = data

//! Byte swap int
static unsigned int swap_int32 (unsigned int val)
{
    val = ((val << 8) & 0xFF00FF00) | ((val >> 8) & 0xFF00FF);

    return (val << 16) | ((val >> 16) & 0xFFFF);
}

 #endif

int mbedtls_pkcs5_pbkdf2_hmac (mbedtls_md_context_t * ctx,
                               const unsigned char  * password,
                               size_t                 plen,
                               const unsigned char  * salt,
                               size_t                 slen,
                               unsigned int           iteration_count,
                               uint32_t               key_length,
                               unsigned char        * output)
{
    int             ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    int             j;
    unsigned int    i;
    unsigned char   md1[MBEDTLS_MD_MAX_SIZE];
    unsigned char   work[MBEDTLS_MD_MAX_SIZE];
    unsigned char   md_size = mbedtls_md_get_size(ctx->md_info);
    size_t          use_len;
    unsigned char * out_p = output;

    // mbedtls_md_context_t init_ctx;
 #ifndef PCKS5_RENESAS_HW
    unsigned char counter[4];

    memset(counter, 0, 4);
    counter[3] = 1;
 #else
    unsigned int counter32;
    unsigned int swap_counter;
    counter32 = 1;
 #endif

 #if UINT_MAX > 0xFFFFFFFF
    if (iteration_count > 0xFFFFFFFF)
    {
        return MBEDTLS_ERR_PKCS5_BAD_INPUT_DATA;
    }
 #endif

 #ifndef PCKS5_RENESAS_HW
    if ((ret = mbedtls_md_hmac_starts(ctx, password, plen)) != 0)
    {
        return ret;
    }
 #endif
    while (key_length)
    {
 #ifdef PCKS5_RENESAS_HW
        if ((ret = mbedtls_md_hmac_starts(ctx, password, plen)) != 0)
        {
            goto cleanup;
        }
 #endif

        // U1 ends up in work
        //
        if ((ret = mbedtls_md_hmac_update(ctx, salt, slen)) != 0)
        {
            goto cleanup;
        }

 #ifndef PCKS5_RENESAS_HW
        if ((ret = mbedtls_md_hmac_update(ctx, counter, 4)) != 0)
        {
            goto cleanup;
        }

 #else
        swap_counter = swap_int32(counter32);
        if ((ret = mbedtls_md_hmac_update(ctx, (unsigned char *) (&swap_counter), 4)) != 0)
        {
            goto cleanup;
        }
 #endif
        if ((ret = mbedtls_md_hmac_finish(ctx, work)) != 0)
        {
            goto cleanup;
        }

        if ((ret = mbedtls_md_hmac_reset(ctx)) != 0)
        {
            goto cleanup;
        }

        memcpy(md1, work, md_size);
 #ifndef PCKS5_RENESAS_HW
        for (i = 1; i < iteration_count; i++)
        {
            // U2 ends up in md1
            //
            if ((ret = mbedtls_md_hmac_update(ctx, md1, md_size)) != 0)
            {
                goto cleanup;
            }

            if ((ret = mbedtls_md_hmac_finish(ctx, md1)) != 0)
            {
                goto cleanup;
            }

            if ((ret = mbedtls_md_hmac_reset(ctx)) != 0)
            {
                goto cleanup;
            }

            // U1 xor U2
            //
            for (j = 0; j < md_size; j++)
            {
                work[j] ^= md1[j];
            }
        }

 #else

        // digest md1, vector1 ctx.md_ctx, vector2 calculate
        if ((iteration_count > 1) && (mbedtls_md_get_type(ctx->md_info) == MBEDTLS_MD_SHA1))
        {
            unsigned int init_vector1[5], init_vector2[5], data[5], * pwork = (unsigned int *) work;
            char       * cpy_tmp;

            // vector 2 calculate
            // mbedtls_md_clone(ctx->md_ctx, init_ctx.md_ctx);
            mbedtls_md_update(ctx, md1, md_size);

            // ctx->md_info->clone_func( ctx->md_ctx, init_ctx.md_ctx );
            // ctx->md_info->update_func( ctx->md_ctx, md1, md_size );
            cpy_tmp = (char *) (ctx->md_ctx) + 8;
            memcpy(init_vector1, cpy_tmp, 20);

            // memcpy(((void *)HW_PBKDF2_IHV1), cpy_tmp, 20);

            *((volatile char *) (HW_PBKDF2_CLK)) |= CRG_TOP_CLK_AMBA_REG_PSK_CLK_ENABLE_Msk;
            MEM_LONG_WRITE(HW_PBKDF2_START, 0x01);

            MEM_LONG_WRITE(HW_PBKDF2_IHV1, (init_vector1[0]));
            MEM_LONG_WRITE(HW_PBKDF2_IHV1 + 1, (init_vector1[1]));
            MEM_LONG_WRITE(HW_PBKDF2_IHV1 + 2, (init_vector1[2]));
            MEM_LONG_WRITE(HW_PBKDF2_IHV1 + 3, (init_vector1[3]));
            MEM_LONG_WRITE(HW_PBKDF2_IHV1 + 4, (init_vector1[4]));

            {
  #define RENESAS_HW_TEMP_BUF_SIZE    84
                unsigned char * tmp;
                unsigned char opad[RENESAS_HW_TEMP_BUF_SIZE];

                memcpy((void *) opad, (unsigned char *) ctx->hmac_ctx + 64, 64);

                tmp = (unsigned char *) (&opad[64]);
                mbedtls_md_finish(ctx, tmp);
                mbedtls_md_starts(ctx);
                mbedtls_md_update(ctx, opad, 84);

                cpy_tmp = (char *) (ctx->md_ctx) + 8;
                memcpy(init_vector2, cpy_tmp, 20);

                MEM_LONG_WRITE(HW_PBKDF2_IHV2, (init_vector2[0]));
                MEM_LONG_WRITE(HW_PBKDF2_IHV2 + 1, (init_vector2[1]));
                MEM_LONG_WRITE(HW_PBKDF2_IHV2 + 2, (init_vector2[2]));
                MEM_LONG_WRITE(HW_PBKDF2_IHV2 + 3, (init_vector2[3]));
                MEM_LONG_WRITE(HW_PBKDF2_IHV2 + 4, (init_vector2[4]));
            }

            // memcpy((void*)HW_PBKDF2_DATA, md1, 20);
            memcpy(data, md1, 20);
            MEM_LONG_WRITE(HW_PBKDF2_DATA, swap_int32(data[0]));
            MEM_LONG_WRITE(HW_PBKDF2_DATA + 1, swap_int32(data[1]));
            MEM_LONG_WRITE(HW_PBKDF2_DATA + 2, swap_int32(data[2]));
            MEM_LONG_WRITE(HW_PBKDF2_DATA + 3, swap_int32(data[3]));
            MEM_LONG_WRITE(HW_PBKDF2_DATA + 4, swap_int32(data[4]));

            MEM_LONG_WRITE(HW_PBKDF2_START, ((iteration_count - 2) << 16) | 0x03);
            __asm__ volatile (  "nop       \n");
            __asm__ volatile (  "nop       \n");
            __asm__ volatile (  "nop       \n");
            __asm__ volatile (  "nop       \n");
            __asm__ volatile (  "nop       \n");
            while (!(*((volatile unsigned int *) HW_PBKDF2_START) & 0x100))
            {
                ;
            }

            // memcpy(work, (void*)HW_PBKDF2_DIGEST, 20);
            MEM_LONG_READ(HW_PBKDF2_DIGEST, &(data[0]));
            MEM_LONG_READ(HW_PBKDF2_DIGEST + 1, &(data[1]));
            MEM_LONG_READ(HW_PBKDF2_DIGEST + 2, &(data[2]));
            MEM_LONG_READ(HW_PBKDF2_DIGEST + 3, &(data[3]));
            MEM_LONG_READ(HW_PBKDF2_DIGEST + 4, &(data[4]));
            pwork[0] = swap_int32(data[0]);
            pwork[1] = swap_int32(data[1]);
            pwork[2] = swap_int32(data[2]);
            pwork[3] = swap_int32(data[3]);
            pwork[4] = swap_int32(data[4]);
            MEM_LONG_WRITE(HW_PBKDF2_START, 0);
            *((volatile char *) (HW_PBKDF2_CLK)) = *((volatile char *) (HW_PBKDF2_CLK)) &
                                                   (char) ~CRG_TOP_CLK_AMBA_REG_PSK_CLK_ENABLE_Msk;
        }
        else
        {
            for (i = 1; i < iteration_count; i++)
            {
                // U2 ends up in md1
                //
                if ((ret = mbedtls_md_hmac_update(ctx, md1, md_size)) != 0)
                {
                    goto cleanup;
                }

                if ((ret = mbedtls_md_hmac_finish(ctx, md1)) != 0)
                {
                    goto cleanup;
                }

                if ((ret = mbedtls_md_hmac_reset(ctx)) != 0)
                {
                    goto cleanup;
                }

                // U1 xor U2
                //
                for (j = 0; j < md_size; j++)
                {
                    work[j] ^= md1[j];
                }
            }
        }
 #endif

        use_len = (key_length < md_size) ? key_length : md_size;
        memcpy(out_p, work, use_len);

        key_length -= (uint32_t) use_len;
        out_p      += use_len;

 #ifndef PCKS5_RENESAS_HW
        for (i = 4; i > 0; i--)
        {
            if (++counter[i - 1] != 0)
            {
                break;
            }
        }

 #else
        counter32++;
 #endif
    }

cleanup:

    /* Zeroise buffers to clear sensitive data from memory. */
    mbedtls_platform_zeroize(work, MBEDTLS_MD_MAX_SIZE);
    mbedtls_platform_zeroize(md1, MBEDTLS_MD_MAX_SIZE);

    return ret;
}

 #if defined(MBEDTLS_SELF_TEST)

  #if !defined(MBEDTLS_SHA1_C)
int mbedtls_pkcs5_self_test (int verbose)
{
    if (verbose != 0)
    {
        mbedtls_printf("  PBKDF2 (SHA1): skipped\n\n");
    }

    return 0;
}

  #else

   #define MAX_TESTS    6              // 5

static const size_t plen_test_data[MAX_TESTS] =
{8, 8, 8, 24, 9, 9};

static const unsigned char password_test_data[MAX_TESTS][32] =
{
    "password",
    "password",
    "password",
    "passwordPASSWORDpassword",
    "pass\0word",
    "N12345678",
};

static const size_t slen_test_data[MAX_TESTS] =
{4, 4, 4, 36, 5, 9};

static const unsigned char salt_test_data[MAX_TESTS][40] =
{
    "salt",
    "salt",
    "salt",
    "saltSALTsaltSALTsaltSALTsaltSALTsalt",
    "sa\0lt",
    "wyyang_ap"
};

static const uint32_t it_cnt_test_data[MAX_TESTS] =
{1, 2, 4096, 4096, 4096, 4096};

static const uint32_t key_len_test_data[MAX_TESTS] =
{20, 20, 20, 25, 16, 32};

static const unsigned char result_key_test_data[MAX_TESTS][32] =
{
    {0x0c, 0x60, 0xc8, 0x0f, 0x96, 0x1f, 0x0e, 0x71,
     0xf3, 0xa9, 0xb5, 0x24, 0xaf, 0x60, 0x12, 0x06,
     0x2f, 0xe0, 0x37, 0xa6},
    {0xea, 0x6c, 0x01, 0x4d, 0xc7, 0x2d, 0x6f, 0x8c,
     0xcd, 0x1e, 0xd9, 0x2a, 0xce, 0x1d, 0x41, 0xf0,
     0xd8, 0xde, 0x89, 0x57},
    {0x4b, 0x00, 0x79, 0x01, 0xb7, 0x65, 0x48, 0x9a,
     0xbe, 0xad, 0x49, 0xd9, 0x26, 0xf7, 0x21, 0xd0,
     0x65, 0xa4, 0x29, 0xc1},
    {0x3d, 0x2e, 0xec, 0x4f, 0xe4, 0x1c, 0x84, 0x9b,
     0x80, 0xc8, 0xd8, 0x36, 0x62, 0xc0, 0xe4, 0x4a,
     0x8b, 0x29, 0x1a, 0x96, 0x4c, 0xf2, 0xf0, 0x70,
     0x38},
    {0x56, 0xfa, 0x6a, 0xa7, 0x55, 0x48, 0x09, 0x9d,
     0xcc, 0x37, 0xd7, 0xf0, 0x34, 0x25, 0xe0, 0xc3},

    {0x19, 0x85, 0x3d, 0x63, 0x9a, 0x2c, 0x15, 0x81,
     0xf6, 0xd1, 0xdf, 0xc4, 0x9c, 0x9c, 0x8e, 0x96,
     0xe3, 0x7c, 0x01, 0xad, 0xe1, 0x4b, 0x8b, 0xc3,
     0xe4, 0x18, 0x0d, 0x0b, 0x20, 0x40, 0xe4, 0x99},
};

int mbedtls_pkcs5_self_test (int verbose)
{
    mbedtls_md_context_t sha1_ctx;
    const mbedtls_md_info_t * info_sha1;
    int ret, i;
    unsigned char key[64];

    mbedtls_md_init(&sha1_ctx);

    info_sha1 = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
    if (info_sha1 == NULL)
    {
        ret = 1;
        goto exit;
    }

    if ((ret = mbedtls_md_setup(&sha1_ctx, info_sha1, 1)) != 0)
    {
        ret = 1;
        goto exit;
    }

    for (i = 0; i < MAX_TESTS; i++)
    {
        if (verbose != 0)
        {
            mbedtls_printf("  PBKDF2 (SHA1) #%d: ", i);
        }

        ret = mbedtls_pkcs5_pbkdf2_hmac(&sha1_ctx,
                                        password_test_data[i],
                                        plen_test_data[i],
                                        salt_test_data[i],
                                        slen_test_data[i],
                                        it_cnt_test_data[i],
                                        key_len_test_data[i],
                                        key);
        if ((ret != 0) ||
            (memcmp(result_key_test_data[i], key, key_len_test_data[i]) != 0))
        {
            if (verbose != 0)
            {
                mbedtls_printf("more than 20 length of key are failed now Need to Debug \n");
            }

            ret = 1;
            goto exit;
        }

        if (verbose != 0)
        {
            mbedtls_printf("passed\n");
        }
    }

    if (verbose != 0)
    {
        mbedtls_printf("\n");
    }

exit:
    mbedtls_md_free(&sha1_ctx);

    return ret;
}

int mbedtls_pkcs5_pbes2_ext (const mbedtls_asn1_buf * pbe_params,
                             int                      mode,
                             const unsigned char    * pwd,
                             size_t                   pwdlen,
                             const unsigned char    * data,
                             size_t                   datalen,
                             unsigned char          * output,
                             size_t                   output_size,
                             size_t                 * output_len)
{
    int ret, iterations = 0, keylen = 0;
    unsigned char * p, * end;
    mbedtls_asn1_buf kdf_alg_oid, enc_scheme_oid, kdf_alg_params, enc_scheme_params;
    mbedtls_asn1_buf salt;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA1;
    unsigned char key[32], iv[32];
    const mbedtls_cipher_info_t * cipher_info;
    mbedtls_cipher_type_t cipher_alg;
    mbedtls_cipher_context_t cipher_ctx;
    unsigned int padlen = 0;

    p   = pbe_params->p;
    end = p + pbe_params->len;

    /*
     *  PBES2-params ::= SEQUENCE {
     *    keyDerivationFunc AlgorithmIdentifier {{PBES2-KDFs}},
     *    encryptionScheme AlgorithmIdentifier {{PBES2-Encs}}
     *  }
     */
    if (pbe_params->tag != (MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE))
    {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PKCS5_INVALID_FORMAT, MBEDTLS_ERR_ASN1_UNEXPECTED_TAG);
    }

    if ((ret = mbedtls_asn1_get_alg(&p, end, &kdf_alg_oid, &kdf_alg_params)) != 0)
    {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PKCS5_INVALID_FORMAT, ret);
    }

    // Only PBKDF2 supported at the moment
    //
    if (MBEDTLS_OID_CMP(MBEDTLS_OID_PKCS5_PBKDF2, &kdf_alg_oid) != 0)
    {
        return MBEDTLS_ERR_PKCS5_FEATURE_UNAVAILABLE;
    }

    if ((ret = pkcs5_parse_pbkdf2_params(&kdf_alg_params, &salt, &iterations, &keylen, &md_type)) != 0)
    {
        return ret;
    }

    if ((ret = mbedtls_asn1_get_alg(&p, end, &enc_scheme_oid, &enc_scheme_params)) != 0)
    {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PKCS5_INVALID_FORMAT, ret);
    }

    if (mbedtls_oid_get_cipher_alg(&enc_scheme_oid, &cipher_alg) != 0)
    {
        return MBEDTLS_ERR_PKCS5_FEATURE_UNAVAILABLE;
    }

    cipher_info = mbedtls_cipher_info_from_type(cipher_alg);
    if (cipher_info == NULL)
    {
        return MBEDTLS_ERR_PKCS5_FEATURE_UNAVAILABLE;
    }

    /*
     * The value of keylen from pkcs5_parse_pbkdf2_params() is ignored
     * since it is optional and we don't know if it was set or not
     */
    keylen = (int) mbedtls_cipher_info_get_key_bitlen(cipher_info) / 8;

    if ((enc_scheme_params.tag != MBEDTLS_ASN1_OCTET_STRING) ||
        (enc_scheme_params.len != mbedtls_cipher_info_get_iv_size(cipher_info)))
    {
        return MBEDTLS_ERR_PKCS5_INVALID_FORMAT;
    }

    if (mode == MBEDTLS_PKCS5_DECRYPT)
    {
        if (output_size < datalen)
        {
            return MBEDTLS_ERR_ASN1_BUF_TOO_SMALL;
        }
    }

    if (mode == MBEDTLS_PKCS5_ENCRYPT)
    {
        padlen = cipher_info->block_size - (datalen % cipher_info->block_size);
        if (output_size < (datalen + padlen))
        {
            return MBEDTLS_ERR_ASN1_BUF_TOO_SMALL;
        }
    }

    mbedtls_cipher_init(&cipher_ctx);

    memcpy(iv, enc_scheme_params.p, enc_scheme_params.len);

    if ((ret = mbedtls_pkcs5_pbkdf2_hmac_ext(md_type, pwd, pwdlen, salt.p, salt.len, iterations, keylen, key)) != 0)
    {
        goto exit;
    }

    if ((ret = mbedtls_cipher_setup(&cipher_ctx, cipher_info)) != 0)
    {
        goto exit;
    }

    if ((ret = mbedtls_cipher_setkey(&cipher_ctx, key, 8 * keylen, (mbedtls_operation_t) mode)) != 0)
    {
        goto exit;
    }

   #if defined(MBEDTLS_CIPHER_MODE_WITH_PADDING)
    {
        /* PKCS5 uses CBC with PKCS7 padding (which is the same as
         * "PKCS5 padding" except that it's typically only called PKCS5
         * with 64-bit-block ciphers).
         */
        mbedtls_cipher_padding_t padding = MBEDTLS_PADDING_PKCS7;
    #if !defined(MBEDTLS_CIPHER_PADDING_PKCS7)

        /* For historical reasons, when decrypting, this function works when
         * decrypting even when support for PKCS7 padding is disabled. In this
         * case, it ignores the padding, and so will never report a
         * password mismatch.
         */
        if (mode == MBEDTLS_DECRYPT)
        {
            padding = MBEDTLS_PADDING_NONE;
        }
    #endif
        if ((ret = mbedtls_cipher_set_padding_mode(&cipher_ctx, padding)) != 0)
        {
            goto exit;
        }
    }
   #endif                              /* MBEDTLS_CIPHER_MODE_WITH_PADDING */
    if ((ret = mbedtls_cipher_crypt(&cipher_ctx, iv, enc_scheme_params.len, data, datalen, output, output_len)) != 0)
    {
        ret = MBEDTLS_ERR_PKCS5_PASSWORD_MISMATCH;
    }

exit:
    mbedtls_cipher_free(&cipher_ctx);

    return ret;
}

static int pkcs5_pbkdf2_hmac (mbedtls_md_context_t * ctx,
                              const unsigned char  * password,
                              size_t                 plen,
                              const unsigned char  * salt,
                              size_t                 slen,
                              unsigned int           iteration_count,
                              uint32_t               key_length,
                              unsigned char        * output)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned int i;
    unsigned char md1[MBEDTLS_MD_MAX_SIZE];
    unsigned char work[MBEDTLS_MD_MAX_SIZE];
    unsigned char md_size = mbedtls_md_get_size(ctx->md_info);
    size_t use_len;
    unsigned char * out_p = output;
    unsigned char counter[4];

    memset(counter, 0, 4);
    counter[3] = 1;

   #if UINT_MAX > 0xFFFFFFFF
    if (iteration_count > 0xFFFFFFFF)
    {
        return MBEDTLS_ERR_PKCS5_BAD_INPUT_DATA;
    }
   #endif

    if ((ret = mbedtls_md_hmac_starts(ctx, password, plen)) != 0)
    {
        return ret;
    }

    while (key_length)
    {
        // U1 ends up in work
        //
        if ((ret = mbedtls_md_hmac_update(ctx, salt, slen)) != 0)
        {
            goto cleanup;
        }

        if ((ret = mbedtls_md_hmac_update(ctx, counter, 4)) != 0)
        {
            goto cleanup;
        }

        if ((ret = mbedtls_md_hmac_finish(ctx, work)) != 0)
        {
            goto cleanup;
        }

        if ((ret = mbedtls_md_hmac_reset(ctx)) != 0)
        {
            goto cleanup;
        }

        memcpy(md1, work, md_size);

        for (i = 1; i < iteration_count; i++)
        {
            // U2 ends up in md1
            //
            if ((ret = mbedtls_md_hmac_update(ctx, md1, md_size)) != 0)
            {
                goto cleanup;
            }

            if ((ret = mbedtls_md_hmac_finish(ctx, md1)) != 0)
            {
                goto cleanup;
            }

            if ((ret = mbedtls_md_hmac_reset(ctx)) != 0)
            {
                goto cleanup;
            }

            // U1 xor U2
            //
            mbedtls_xor(work, work, md1, md_size);
        }

        use_len = (key_length < md_size) ? key_length : md_size;
        memcpy(out_p, work, use_len);

        key_length -= (uint32_t) use_len;
        out_p      += use_len;

        for (i = 4; i > 0; i--)
        {
            if (++counter[i - 1] != 0)
            {
                break;
            }
        }
    }

cleanup:

    /* Zeroise buffers to clear sensitive data from memory. */
    mbedtls_platform_zeroize(work, MBEDTLS_MD_MAX_SIZE);
    mbedtls_platform_zeroize(md1, MBEDTLS_MD_MAX_SIZE);

    return ret;
}

int mbedtls_pkcs5_pbkdf2_hmac_ext (mbedtls_md_type_t     md_alg,
                                   const unsigned char * password,
                                   size_t                plen,
                                   const unsigned char * salt,
                                   size_t                slen,
                                   unsigned int          iteration_count,
                                   uint32_t              key_length,
                                   unsigned char       * output)
{
    mbedtls_md_context_t md_ctx;
    const mbedtls_md_info_t * md_info = NULL;
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    md_info = mbedtls_md_info_from_type(md_alg);
    if (md_info == NULL)
    {
        return MBEDTLS_ERR_PKCS5_FEATURE_UNAVAILABLE;
    }

    mbedtls_md_init(&md_ctx);

    if ((ret = mbedtls_md_setup(&md_ctx, md_info, 1)) != 0)
    {
        goto exit;
    }

    ret = pkcs5_pbkdf2_hmac(&md_ctx, password, plen, salt, slen, iteration_count, key_length, output);
exit:
    mbedtls_md_free(&md_ctx);

    return ret;
}

  #endif                               /* MBEDTLS_SHA1_C */

 #endif                                /* MBEDTLS_SELF_TEST */

#endif                                 /* MBEDTLS_PKCS5_C */
