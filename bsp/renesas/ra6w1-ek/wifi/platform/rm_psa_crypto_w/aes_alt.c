/*
 * Copyright (c) 2001-2019, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR Arm’s non-OSI source license
 */
#include "common.h"

#if defined(MBEDTLS_AES_C) && defined(MBEDTLS_AES_ALT)

 #include "mbedtls/aes.h"
 #include "mbedtls/platform_util.h"
 #include "mbedtls/error.h"
 #include "cc_pal_types.h"
 #include "cc_pal_mem.h"
 #include "cc_pal_abort.h"

/**
 * \brief          Initialize AES context
 *
 *
 * \note           Context block should be pre-allocated by the caller.
 *
 * \param ctx      AES context to be initialized
 */
void mbedtls_aes_init (mbedtls_aes_context * ctx)
{
    AesContext_t * aesCtx = NULL;

    if (NULL == ctx)
    {
        CC_PalAbort("ctx cannot be NULL");
    }

    /* check size of structs match */
    if (sizeof(mbedtls_aes_context) != sizeof(AesContext_t))
    {
        CC_PalAbort("!!!!AES context sizes mismatch!!!\n");
    }

    aesCtx = (AesContext_t *) ctx;

    aesCtx->padType            = CRYPTO_PADDING_NONE;
    aesCtx->dataBlockType      = FIRST_BLOCK;
    aesCtx->inputDataAddrType  = DLLI_ADDR;
    aesCtx->outputDataAddrType = DLLI_ADDR;
}

/**
 * \brief          Clear AES context
 *
 * \param ctx      AES context to be cleared
 */
void mbedtls_aes_free (mbedtls_aes_context * ctx)
{
    if (NULL == ctx)
    {
        CC_PAL_LOG_ERR("ctx cannot be NULL\n");

        return;
    }

    CC_PalMemSet(ctx, 0, sizeof(mbedtls_aes_context));
}

/**
 * @brief Internal function:
 * This function checks the validity of inputs and set the encrypt/decript key & direction
 * called by mbedtls_aes_setkey_* functions.
 *
 * @returns: 0 on success, various error in case of error.
 *
 */
static int aes_setkey (mbedtls_aes_context * ctx, const unsigned char * key, unsigned int keybits,
                       cryptoDirection_t dir)
{
    AesContext_t * aesCtx = NULL;

    /* if the users context ID pointer is NULL return an error */
    if (NULL == ctx)
    {
        CC_PAL_LOG_ERR("ctx cannot be NULL\n");

        return MBEDTLS_ERR_AES_INVALID_INPUT_LENGTH;
    }

    /* check the validity of the key data pointer */
    if (NULL == key)
    {
        CC_PAL_LOG_ERR("key cannot be NULL\n");

        return MBEDTLS_ERR_AES_INVALID_KEY_LENGTH;
    }

    aesCtx            = (AesContext_t *) ctx;
    aesCtx->dir       = dir;
    aesCtx->cryptoKey = USER_KEY;

    switch (keybits)
    {
        case 128:
        {
            aesCtx->keySizeId = KEY_SIZE_128_BIT;
            break;
        }

        case 192:
        {
            aesCtx->keySizeId = KEY_SIZE_192_BIT;
            break;
        }

        case 256:
        {
            aesCtx->keySizeId = KEY_SIZE_256_BIT;
            break;
        }

        default:
            CC_PAL_LOG_ERR("key length (%d) not supported\n", keybits);

            return MBEDTLS_ERR_AES_INVALID_KEY_LENGTH;
    }

    CC_PalMemCopy(aesCtx->keyBuf, key, keybits / 8);

    return 0;                          // no mbedTLS const for OK.
}

/*
 * Copy the key into the context, and set the direction to encryption.
 * A lot of the initialization needed by CC, will be done in the actual crypt function.
 *
 * mbedTLS error codes are much more limited then CC, so have to map a bit.
 *
 *
 */
int mbedtls_aes_setkey_enc (mbedtls_aes_context * ctx, const unsigned char * key, unsigned int keybits)
{
    return aes_setkey(ctx, key, keybits, CRYPTO_DIRECTION_ENCRYPT);
}

/**
 * \brief          AES key schedule (decryption)
 *
 * \param ctx      AES context to be initialized
 * \param key      decryption key
 * \param keybits  must be 128, 192 or 256
 *
 * \return         0 if successful, or MBEDTLS_ERR_AES_INVALID_KEY_LENGTH/
 *                 MBEDTLS_ERR_AES_INVALID_INPUT_LENGTH
 */

int mbedtls_aes_setkey_dec (mbedtls_aes_context * ctx, const unsigned char * key, unsigned int keybits)
{
    return aes_setkey(ctx, key, keybits, CRYPTO_DIRECTION_DECRYPT);
}

/**
 * \brief          AES-ECB block encryption/decryption
 *
 * \param ctx      AES context
 * \param mode     MBEDTLS_AES_ENCRYPT or MBEDTLS_AES_DECRYPT
 * \param input    16-byte input block
 * \param output   16-byte output block
 *
 * \return         0 if successful, MBEDTLS_ERR_AES_INVALID_INPUT_LENGTH otherwise
 */

int mbedtls_aes_crypt_ecb (mbedtls_aes_context * ctx,
                           int                   mode,
                           const unsigned char   input[AES_BLOCK_SIZE],
                           unsigned char         output[AES_BLOCK_SIZE])
{
    AesContext_t * aesCtx = NULL;
    drvError_t     drvRet;
    CCBuffInfo_t   inBuffInfo;
    CCBuffInfo_t   outBuffInfo;

    if ((NULL == ctx) || (NULL == input) || (NULL == output))
    {
        CC_PAL_LOG_ERR("Null pointer exception\n");

        return MBEDTLS_ERR_AES_INVALID_INPUT_LENGTH;
    }

    if ((MBEDTLS_AES_ENCRYPT != mode) && (MBEDTLS_AES_DECRYPT != mode))
    {
        CC_PAL_LOG_ERR("Mode %d is not supported\n", mode);

        return MBEDTLS_ERR_AES_INVALID_INPUT_LENGTH;
    }

    aesCtx = (AesContext_t *) ctx;

    if (((MBEDTLS_AES_ENCRYPT == mode) && (CRYPTO_DIRECTION_ENCRYPT != aesCtx->dir)) ||
        ((MBEDTLS_AES_DECRYPT == mode) && (CRYPTO_DIRECTION_DECRYPT != aesCtx->dir)))
    {
        // someone made a mistake - set key in the wrong direction
        CC_PAL_LOG_ERR("Key & operation mode mismatch: mode = %d. aesCtx->dir = %d\n", mode, aesCtx->dir);

        return MBEDTLS_ERR_AES_INVALID_INPUT_LENGTH;
    }

    aesCtx->mode = CIPHER_ECB;

    drvRet = SetDataBuffersInfo(input, AES_BLOCK_SIZE, &inBuffInfo, output, AES_BLOCK_SIZE, &outBuffInfo);
    if (drvRet != 0)
    {
        CC_PAL_LOG_ERR("illegal data buffers\n");

        return MBEDTLS_ERR_AES_INVALID_INPUT_LENGTH;
    }

    drvRet = ProcessAesDrv(aesCtx, &inBuffInfo, &outBuffInfo, AES_BLOCK_SIZE);

    if (drvRet != AES_DRV_OK)
    {
        CC_PAL_LOG_ERR("ecb crypt failed with error code %d\n", drvRet);

        return MBEDTLS_ERR_AES_INVALID_INPUT_LENGTH;
    }

    return 0;
}

 #if defined(MBEDTLS_CIPHER_MODE_CBC)

/*
 * AES-CBC buffer encryption/decryption
 */
int mbedtls_aes_crypt_cbc (mbedtls_aes_context * ctx,
                           int                   mode,
                           size_t                length,
                           unsigned char         iv[AES_IV_SIZE],
                           const unsigned char * input,
                           unsigned char       * output)
{
    AesContext_t * aesCtx = NULL;
    drvError_t     drvRet;
    CCBuffInfo_t   inBuffInfo;
    CCBuffInfo_t   outBuffInfo;

    if (0 == length)                   /* In case input size is 0 - do nothing and return with success*/
    {
        return 0;
    }

    if ((NULL == ctx) || (NULL == input) || (NULL == output) || (NULL == iv))
    {
        CC_PAL_LOG_ERR("Null pointer exception\n");

        return MBEDTLS_ERR_AES_INVALID_INPUT_LENGTH;
    }

    if ((MBEDTLS_AES_ENCRYPT != mode) && (MBEDTLS_AES_DECRYPT != mode))
    {
        CC_PAL_LOG_ERR("Mode %d is not supported\n", mode);

        return MBEDTLS_ERR_AES_INVALID_INPUT_LENGTH;
    }

    if (length % AES_BLOCK_SIZE)
    {
        CC_PAL_LOG_ERR("Length should be a multiple of the block size (16 bytes)\n");

        return MBEDTLS_ERR_AES_INVALID_INPUT_LENGTH;
    }

    aesCtx = (AesContext_t *) ctx;

    if (((MBEDTLS_AES_ENCRYPT == mode) && (CRYPTO_DIRECTION_ENCRYPT != aesCtx->dir)) ||
        ((MBEDTLS_AES_DECRYPT == mode) && (CRYPTO_DIRECTION_DECRYPT != aesCtx->dir)))
    {
        // someone made a mistake - set key in the wrong direction
        CC_PAL_LOG_ERR("Key & operation mode mismatch: operation %d key %d\n", mode, aesCtx->dir);

        return MBEDTLS_ERR_AES_INVALID_INPUT_LENGTH;
    }

    CC_PalMemCopy(aesCtx->ivBuf, iv, AES_IV_SIZE);
    aesCtx->mode = CIPHER_CBC;

    drvRet = SetDataBuffersInfo(input, length, &inBuffInfo, output, length, &outBuffInfo);
    if (drvRet != 0)
    {
        CC_PAL_LOG_ERR("illegal data buffers\n");

        return MBEDTLS_ERR_AES_INVALID_INPUT_LENGTH;
    }

    drvRet = ProcessAesDrv(aesCtx, &inBuffInfo, &outBuffInfo, length);

    if (drvRet != AES_DRV_OK)
    {
        CC_PAL_LOG_ERR("cbc crypt failed with error code %d\n", drvRet);

        return MBEDTLS_ERR_AES_INVALID_INPUT_LENGTH;
    }

    CC_PalMemCopy(iv,
                  aesCtx->ivBuf,
                  AES_IV_SIZE);

    return 0;
}

 #endif                                /* MBEDTLS_CIPHER_MODE_CBC */

 #if defined(MBEDTLS_CIPHER_MODE_CFB)

/*
 * AES-CFB128 buffer encryption/decryption
 */
int mbedtls_aes_crypt_cfb128 (mbedtls_aes_context * ctx, int mode, size_t length, size_t * iv_off,
                              unsigned char iv[AES_IV_SIZE], const unsigned char * input, unsigned char * output)
{
    int    c;
    int    ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t n;

    if ((NULL == ctx) || (NULL == iv_off) || (NULL == iv) || (NULL == input) || (NULL == output))
    {
        return MBEDTLS_ERR_AES_BAD_INPUT_DATA;
    }

    if ((MBEDTLS_AES_ENCRYPT != mode) && (MBEDTLS_AES_DECRYPT != mode))
    {
        return MBEDTLS_ERR_AES_BAD_INPUT_DATA;
    }

    n = *iv_off;

    if (n > 15)
    {
        return MBEDTLS_ERR_AES_BAD_INPUT_DATA;
    }

    if (mode == MBEDTLS_AES_DECRYPT)
    {
        while (length--)
        {
            if (n == 0)
            {
                ret = mbedtls_aes_crypt_ecb(ctx, MBEDTLS_AES_ENCRYPT, iv, iv);
                if (ret != 0)
                {
                    goto exit;
                }
            }

            c         = *input++;
            *output++ = (unsigned char) (c ^ iv[n]);
            iv[n]     = (unsigned char) c;

            n = (n + 1) & 0x0F;
        }
    }
    else
    {
        while (length--)
        {
            if (n == 0)
            {
                ret = mbedtls_aes_crypt_ecb(ctx, MBEDTLS_AES_ENCRYPT, iv, iv);
                if (ret != 0)
                {
                    goto exit;
                }
            }

            iv[n] = *output++ = (unsigned char) (iv[n] ^ *input++);

            n = (n + 1) & 0x0F;
        }
    }

    *iv_off = n;
    ret     = 0;

exit:

    return ret;
}

/*
 * AES-CFB8 buffer encryption/decryption
 */
int mbedtls_aes_crypt_cfb8 (mbedtls_aes_context * ctx,
                            int                   mode,
                            size_t                length,
                            unsigned char         iv[AES_IV_SIZE],
                            const unsigned char * input,
                            unsigned char       * output)
{
    int           ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char c;
    unsigned char ov[17];

    if ((NULL == ctx) || (NULL == iv) || (NULL == input) || (NULL == output))
    {
        return MBEDTLS_ERR_AES_BAD_INPUT_DATA;
    }

    if ((MBEDTLS_AES_ENCRYPT != mode) && (MBEDTLS_AES_DECRYPT != mode))
    {
        return MBEDTLS_ERR_AES_BAD_INPUT_DATA;
    }

    while (length--)
    {
        memcpy(ov, iv, 16);
        ret = mbedtls_aes_crypt_ecb(ctx, MBEDTLS_AES_ENCRYPT, iv, iv);
        if (ret != 0)
        {
            goto exit;
        }

        if (mode == MBEDTLS_AES_DECRYPT)
        {
            ov[16] = *input;
        }

        c = *output++ = (unsigned char) (iv[0] ^ *input++);

        if (mode == MBEDTLS_AES_ENCRYPT)
        {
            ov[16] = c;
        }

        memcpy(iv, ov + 1, 16);
    }

    ret = 0;

exit:

    return ret;
}

 #endif                                /*MBEDTLS_CIPHER_MODE_CFB */
 #if defined(MBEDTLS_CIPHER_MODE_CTR) || defined(MBEDTLS_CIPHER_MODE_OFB)

/*
 * AES-CTR buffer encryption/decryption
 */
static int aes_crypt_ctr_ofb (mbedtls_aes_context * ctx,
                              aesMode_t             mode,
                              size_t                length,
                              size_t              * iv_off,
                              unsigned char         iv[AES_BLOCK_SIZE],
                              const unsigned char * input,
                              unsigned char       * output)
{
    AesContext_t * aesCtx = NULL;
    drvError_t     drvRet;
    CCBuffInfo_t   inBuffInfo;
    CCBuffInfo_t   outBuffInfo;

    if (0 == length)                   /* In case input size is 0 - do nothing and return with success*/
    {
        return 0;
    }

    if ((NULL == ctx) || (NULL == iv) || (NULL == input) || (NULL == output))
    {
        CC_PAL_LOG_ERR("Null pointer exception\n");

        return MBEDTLS_ERR_AES_INVALID_INPUT_LENGTH;
    }

    if ((iv_off != NULL) && (*iv_off != 0))
    {
        CC_PAL_LOG_ERR("offset other then 0 is not supported\n");

        return MBEDTLS_ERR_AES_INVALID_INPUT_LENGTH;
    }

    aesCtx = (AesContext_t *) ctx;

    aesCtx->mode = mode;
    CC_PalMemCopy(aesCtx->ivBuf, iv, AES_BLOCK_SIZE);

    drvRet = SetDataBuffersInfo(input, length, &inBuffInfo, output, length, &outBuffInfo);
    if (drvRet != 0)
    {
        CC_PAL_LOG_ERR("illegal data buffers\n");

        return MBEDTLS_ERR_AES_INVALID_INPUT_LENGTH;
    }

    drvRet = ProcessAesDrv(aesCtx, &inBuffInfo, &outBuffInfo, length);
    if (drvRet != AES_DRV_OK)
    {
        CC_PAL_LOG_ERR("ctr/ofb crypt failed with error code %d\n", drvRet);

        return MBEDTLS_ERR_AES_INVALID_INPUT_LENGTH;
    }

    CC_PalMemCopy(iv,
                  aesCtx->ivBuf,
                  AES_BLOCK_SIZE);

    return 0;
}

 #endif                                /* MBEDTLS_CIPHER_MODE_CTR || MBEDTLS_CIPHER_MODE_OFB*/

 #if defined(MBEDTLS_CIPHER_MODE_CTR)

/*
 * AES-CTR buffer encryption/decryption
 */
int mbedtls_aes_crypt_ctr (mbedtls_aes_context * ctx,
                           size_t                length,
                           size_t              * nc_off,
                           unsigned char         nonce_counter[AES_BLOCK_SIZE],
                           unsigned char         stream_block[AES_BLOCK_SIZE],
                           const unsigned char * input,
                           unsigned char       * output)
{
    CC_UNUSED_PARAM(stream_block);

    return aes_crypt_ctr_ofb(ctx, CIPHER_CTR, length, nc_off, nonce_counter, input, output);
}

 #endif                                /* MBEDTLS_CIPHER_MODE_CTR */

 #if defined(MBEDTLS_CIPHER_MODE_OFB)

/*
 * AES-CTR buffer encryption/decryption
 */
int mbedtls_aes_crypt_ofb (mbedtls_aes_context * ctx,
                           size_t                length,
                           size_t              * iv_off,
                           unsigned char         iv[AES_BLOCK_SIZE],
                           const unsigned char * input,
                           unsigned char       * output)
{
    return aes_crypt_ctr_ofb(ctx, CIPHER_OFB, length, iv_off, iv, input, output);
}

 #endif                                /* MBEDTLS_CIPHER_MODE_OFB */

/*
 * AES-ECB block encryption
 */
int mbedtls_internal_aes_encrypt (mbedtls_aes_context * ctx, const unsigned char input[16], unsigned char output[16])
{
    return mbedtls_aes_crypt_ecb(ctx, MBEDTLS_AES_ENCRYPT, input, output);
}

/**
 * \brief           Internal AES block encryption function
 *                  (Only exposed to allow overriding it,
 *                  see MBEDTLS_AES_ENCRYPT_ALT)
 *
 * \param ctx       AES context
 * \param input     Plaintext block
 * \param output    Output (ciphertext) block
 */
void mbedtls_aes_encrypt (mbedtls_aes_context * ctx, const unsigned char input[16], unsigned char output[16])
{
    mbedtls_internal_aes_encrypt(ctx, input, output);
}

/*
 * AES-ECB block decryption
 */
int mbedtls_internal_aes_decrypt (mbedtls_aes_context * ctx, const unsigned char input[16], unsigned char output[16])
{
    return mbedtls_aes_crypt_ecb(ctx, MBEDTLS_AES_DECRYPT, input, output);
}

void mbedtls_aes_decrypt (mbedtls_aes_context * ctx, const unsigned char input[16], unsigned char output[16])
{
    mbedtls_internal_aes_decrypt(ctx, input, output);
}

 #if defined(MBEDTLS_CIPHER_MODE_XTS)

void mbedtls_aes_xts_init (mbedtls_aes_xts_context * ctx)
{
    if (NULL == ctx)
    {
        return;
    }

    mbedtls_aes_init(&ctx->crypt);
    mbedtls_aes_init(&ctx->tweak);
}

void mbedtls_aes_xts_free (mbedtls_aes_xts_context * ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    mbedtls_aes_free(&ctx->crypt);
    mbedtls_aes_free(&ctx->tweak);
}

static int mbedtls_aes_xts_decode_keys (const unsigned char  * key,
                                        unsigned int           keybits,
                                        const unsigned char ** key1,
                                        unsigned int         * key1bits,
                                        const unsigned char ** key2,
                                        unsigned int         * key2bits)
{
    const unsigned int half_keybits  = keybits / 2;
    const unsigned int half_keybytes = half_keybits / 8;

    switch (keybits)
    {
        case 256:
        {}
        break;

        case 512:
        {}
        break;

        default:

            return MBEDTLS_ERR_AES_INVALID_KEY_LENGTH;
    }

    *key1bits = half_keybits;
    *key2bits = half_keybits;
    *key1     = &key[0];
    *key2     = &key[half_keybytes];

    return 0;
}

int mbedtls_aes_xts_setkey_enc (mbedtls_aes_xts_context * ctx, const unsigned char * key, unsigned int keybits)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    const unsigned char * key1, * key2;
    unsigned int          key1bits, key2bits;

    if ((NULL == ctx) || (NULL == key))
    {
        return MBEDTLS_ERR_AES_BAD_INPUT_DATA;
    }

    ret = mbedtls_aes_xts_decode_keys(key, keybits, &key1, &key1bits, &key2, &key2bits);
    if (ret != 0)
    {
        return ret;
    }

    /* Set the tweak key. Always set tweak key for the encryption mode. */
    ret = mbedtls_aes_setkey_enc(&ctx->tweak, key2, key2bits);
    if (ret != 0)
    {
        return ret;
    }

    /* Set crypt key for encryption. */
    return mbedtls_aes_setkey_enc(&ctx->crypt, key1, key1bits);
}

int mbedtls_aes_xts_setkey_dec (mbedtls_aes_xts_context * ctx, const unsigned char * key, unsigned int keybits)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    const unsigned char * key1, * key2;
    unsigned int          key1bits, key2bits;

    if ((NULL == ctx) || (NULL == key))
    {
        return MBEDTLS_ERR_AES_BAD_INPUT_DATA;
    }

    ret = mbedtls_aes_xts_decode_keys(key, keybits, &key1, &key1bits, &key2, &key2bits);
    if (ret != 0)
    {
        return ret;
    }

    /* Set the tweak key. Always set tweak key for encryption. */
    ret = mbedtls_aes_setkey_enc(&ctx->tweak, key2, key2bits);
    if (ret != 0)
    {
        return ret;
    }

    /* Set crypt key for decryption. */
    return mbedtls_aes_setkey_dec(&ctx->crypt, key1, key1bits);
}

typedef unsigned char mbedtls_be128[16];

/*
 * GF(2^128) multiplication function
 *
 * This function multiplies a field element by x in the polynomial field
 * representation. It uses 64-bit word operations to gain speed but compensates
 * for machine endianness and hence works correctly on both big and little
 * endian machines.
 */
static void mbedtls_gf128mul_x_ble (unsigned char r[16], const unsigned char x[16])
{
    uint64_t a, b, ra, rb;

    a = MBEDTLS_GET_UINT64_LE(x, 0);
    b = MBEDTLS_GET_UINT64_LE(x, 8);

    ra = (a << 1) ^ 0x0087 >> (8 - ((b >> 63) << 3));
    rb = (a >> 63) | (b << 1);

    MBEDTLS_PUT_UINT64_LE(ra, r, 0);
    MBEDTLS_PUT_UINT64_LE(rb,
                          r,
                          8);
}

/*
 * AES-XTS buffer encryption/decryption
 */

int mbedtls_aes_crypt_xts (mbedtls_aes_xts_context * ctx,
                           int                       mode,
                           size_t                    length,
                           const unsigned char       data_unit[AES_BLOCK_SIZE],
                           const unsigned char     * input,
                           unsigned char           * output)

{
    int           ret      = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t        blocks   = length / 16;
    size_t        leftover = length % 16;
    unsigned char tweak[16];
    unsigned char prev_tweak[16];
    unsigned char tmp[16];

    if ((NULL == ctx) || (NULL == data_unit) || (NULL == input) || (NULL == output))
    {
        return MBEDTLS_ERR_AES_BAD_INPUT_DATA;
    }

    if ((MBEDTLS_AES_ENCRYPT != mode) && (MBEDTLS_AES_DECRYPT != mode))
    {
        return MBEDTLS_ERR_AES_BAD_INPUT_DATA;
    }

    /* Data units must be at least 16 bytes long. */
    if (length < 16)
    {
        return MBEDTLS_ERR_AES_INVALID_INPUT_LENGTH;
    }

    /* NIST SP 800-38E disallows data units larger than 2**20 blocks. */
    if (length > (1 << 20) * 16)
    {
        return MBEDTLS_ERR_AES_INVALID_INPUT_LENGTH;
    }

    /* Compute the tweak. */
    ret = mbedtls_aes_crypt_ecb(&ctx->tweak, MBEDTLS_AES_ENCRYPT, data_unit, tweak);
    if (ret != 0)
    {
        return ret;
    }

    while (blocks--)
    {
        size_t i;

        if (leftover && (mode == MBEDTLS_AES_DECRYPT) && (blocks == 0))
        {
            /* We are on the last block in a decrypt operation that has
             * leftover bytes, so we need to use the next tweak for this block,
             * and this tweak for the lefover bytes. Save the current tweak for
             * the leftovers and then update the current tweak for use on this,
             * the last full block. */
            memcpy(prev_tweak, tweak, sizeof(tweak));
            mbedtls_gf128mul_x_ble(tweak, tweak);
        }

        for (i = 0; i < 16; i++)
        {
            tmp[i] = input[i] ^ tweak[i];
        }

        ret = mbedtls_aes_crypt_ecb(&ctx->crypt, mode, tmp, tmp);
        if (ret != 0)
        {
            return ret;
        }

        for (i = 0; i < 16; i++)
        {
            output[i] = tmp[i] ^ tweak[i];
        }

        /* Update the tweak for the next block. */
        mbedtls_gf128mul_x_ble(tweak, tweak);

        output += 16;
        input  += 16;
    }

    if (leftover)
    {
        /* If we are on the leftover bytes in a decrypt operation, we need to
         * use the previous tweak for these bytes (as saved in prev_tweak). */
        unsigned char * t = mode == MBEDTLS_AES_DECRYPT ? prev_tweak : tweak;

        /* We are now on the final part of the data unit, which doesn't divide
         * evenly by 16. It's time for ciphertext stealing. */
        size_t          i;
        unsigned char * prev_output = output - 16;

        /* Copy ciphertext bytes from the previous block to our output for each
         * byte of ciphertext we won't steal. At the same time, copy the
         * remainder of the input for this final round (since the loop bounds
         * are the same). */
        for (i = 0; i < leftover; i++)
        {
            output[i] = prev_output[i];
            tmp[i]    = input[i] ^ t[i];
        }

        /* Copy ciphertext bytes from the previous block for input in this
         * round. */
        for ( ; i < 16; i++)
        {
            tmp[i] = prev_output[i] ^ t[i];
        }

        ret = mbedtls_aes_crypt_ecb(&ctx->crypt, mode, tmp, tmp);
        if (ret != 0)
        {
            return ret;
        }

        /* Write the result back to the previous block, overriding the previous
         * output we copied. */
        for (i = 0; i < 16; i++)
        {
            prev_output[i] = tmp[i] ^ t[i];
        }
    }

    return 0;
}

 #endif                                // defined(MBEDTLS_CIPHER_MODE_XTS)

#endif                                 // defined(MBEDTLS_AES_C) && defined (MBEDTLS_AES_ALT)
