/*
 * Copyright (c) 2015-2016, Nuvoton Technology Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "common.h"

#if defined(MBEDTLS_DES_C) && defined(MBEDTLS_DES_ALT)
 #include "mbedtls/des.h"
 #include "sdk_defs.h"

 #include "cc_pal_types.h"
 #include "cc_pal_mem.h"
 #include "cc_pal_abort.h"

 #if defined(MBEDTLS_DES_C)

  #include "mbedtls/platform.h"
  #include "mbedtls/threading.h"

  #include <string.h>

  #if defined(MBEDTLS_DES_ALT)

   #define MEM_LONG_READ(addr, data)     *data = *((volatile uint32_t *) (addr))
   #define MEM_LONG_WRITE(addr, data)    *((volatile uint32_t *) (addr)) = data
   #define NOP_EXECUTION()               { \
        __asm volatile ("nop");            \
}
   #if defined(MBEDTLS_THREADING_C)
static mbedtls_threading_mutex_t mutex;
static int init_cnt = 0;
   #endif

void mbedtls_des_init (mbedtls_des_context * ctx)
{
    if (ctx == NULL)
    {
        return;
    }

   #if defined(MBEDTLS_THREADING_C)
    if (init_cnt == 0)
    {
        mbedtls_mutex_init(&mutex);

        // clock on
        CRG_TOP->CLK_AMBA_REG |= CRG_TOP_CLK_AMBA_REG_TDES_ENABLE_Msk;
    }
    init_cnt++;
   #endif
}

void mbedtls_des_free (mbedtls_des_context * ctx)
{
    if (ctx == NULL)
    {
        return;
    }

   #if defined(MBEDTLS_THREADING_C)
    if (init_cnt == 1)
    {
        mbedtls_mutex_free(&mutex);
        CRG_TOP->CLK_AMBA_REG &= ~CRG_TOP_CLK_AMBA_REG_TDES_ENABLE_Msk;
    }
    init_cnt--;
   #endif
}

void mbedtls_des3_init (mbedtls_des3_context * ctx)
{
    if (ctx == NULL)
    {
        return;
    }

   #if defined(MBEDTLS_THREADING_C)
    if (init_cnt == 0)
    {
        mbedtls_mutex_init(&mutex);
        CRG_TOP->CLK_AMBA_REG |= CRG_TOP_CLK_AMBA_REG_TDES_ENABLE_Msk;
    }
    init_cnt++;
   #endif
}

void mbedtls_des3_free (mbedtls_des3_context * ctx)
{
    if (ctx == NULL)
    {
        return;
    }

   #if defined(MBEDTLS_THREADING_C)
    if (init_cnt == 1)
    {
        mbedtls_mutex_free(&mutex);
        CRG_TOP->CLK_AMBA_REG &= ~CRG_TOP_CLK_AMBA_REG_TDES_ENABLE_Msk;
    }
    init_cnt--;
   #endif
}

static const unsigned char odd_parity_table[128] =
{
    1,   2,   4,   7,   8,
    11,  13,  14,  16,  19, 21,  22,  25,   26,  28,   31,   32,  35,  37,  38,  41,  42,  44,
    47,  49,  50,  52,  55, 56,  59,  61,   62,  64,   67,   69,  70,  73,  74,  76,  79,  81,
    82,  84,  87,  88,  91, 93,  94,  97,   98,  100,  103,  104, 107, 109, 110, 112,
    115, 117, 118, 121, 122,124, 127, 128,  131, 133,  134,  137, 138, 140,
    143, 145, 146, 148, 151,152, 155, 157,  158, 161,  162,  164, 167, 168,
    171, 173, 174, 176, 179,181, 182, 185,  186, 188,  191,  193, 194, 196,
    199, 200, 203, 205, 206,208, 211, 213,  214, 217,  218,  220, 223, 224,
    227, 229, 230, 233, 234,236, 239, 241,  242, 244,  247,  248, 251, 253,
    254
};

void mbedtls_des_key_set_parity (unsigned char key[MBEDTLS_DES_KEY_SIZE])
{
    int i;

    for (i = 0; i < MBEDTLS_DES_KEY_SIZE; i++)
    {
        key[i] = odd_parity_table[key[i] / 2];
    }
}

/*
 * Check the given key's parity, returns 1 on failure, 0 on SUCCESS
 */
int mbedtls_des_key_check_key_parity (const unsigned char key[MBEDTLS_DES_KEY_SIZE])
{
    int i;

    for (i = 0; i < MBEDTLS_DES_KEY_SIZE; i++)
    {
        if (key[i] != odd_parity_table[key[i] / 2])
        {
            return 1;
        }
    }

    return 0;
}

/*
 * Table of weak and semi-weak keys
 *
 * Source: http://en.wikipedia.org/wiki/Weak_key
 *
 * Weak:
 * Alternating ones + zeros (0x0101010101010101)
 * Alternating 'F' + 'E' (0xFEFEFEFEFEFEFEFE)
 * '0xE0E0E0E0F1F1F1F1'
 * '0x1F1F1F1F0E0E0E0E'
 *
 * Semi-weak:
 * 0x011F011F010E010E and 0x1F011F010E010E01
 * 0x01E001E001F101F1 and 0xE001E001F101F101
 * 0x01FE01FE01FE01FE and 0xFE01FE01FE01FE01
 * 0x1FE01FE00EF10EF1 and 0xE01FE01FF10EF10E
 * 0x1FFE1FFE0EFE0EFE and 0xFE1FFE1FFE0EFE0E
 * 0xE0FEE0FEF1FEF1FE and 0xFEE0FEE0FEF1FEF1
 *
 */

   #define WEAK_KEY_COUNT    16

static const unsigned char weak_key_table[WEAK_KEY_COUNT][MBEDTLS_DES_KEY_SIZE] =
{
    {0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01},
    {0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE},
    {0x1F, 0x1F, 0x1F, 0x1F, 0x0E, 0x0E, 0x0E, 0x0E},
    {0xE0, 0xE0, 0xE0, 0xE0, 0xF1, 0xF1, 0xF1, 0xF1},

    {0x01, 0x1F, 0x01, 0x1F, 0x01, 0x0E, 0x01, 0x0E},
    {0x1F, 0x01, 0x1F, 0x01, 0x0E, 0x01, 0x0E, 0x01},
    {0x01, 0xE0, 0x01, 0xE0, 0x01, 0xF1, 0x01, 0xF1},
    {0xE0, 0x01, 0xE0, 0x01, 0xF1, 0x01, 0xF1, 0x01},
    {0x01, 0xFE, 0x01, 0xFE, 0x01, 0xFE, 0x01, 0xFE},
    {0xFE, 0x01, 0xFE, 0x01, 0xFE, 0x01, 0xFE, 0x01},
    {0x1F, 0xE0, 0x1F, 0xE0, 0x0E, 0xF1, 0x0E, 0xF1},
    {0xE0, 0x1F, 0xE0, 0x1F, 0xF1, 0x0E, 0xF1, 0x0E},
    {0x1F, 0xFE, 0x1F, 0xFE, 0x0E, 0xFE, 0x0E, 0xFE},
    {0xFE, 0x1F, 0xFE, 0x1F, 0xFE, 0x0E, 0xFE, 0x0E},
    {0xE0, 0xFE, 0xE0, 0xFE, 0xF1, 0xFE, 0xF1, 0xFE},
    {0xFE, 0xE0, 0xFE, 0xE0, 0xFE, 0xF1, 0xFE, 0xF1}
};

int mbedtls_des_key_check_weak (const unsigned char key[MBEDTLS_DES_KEY_SIZE])
{
    int i;

    for (i = 0; i < WEAK_KEY_COUNT; i++)
    {
        if (memcmp(weak_key_table[i], key, MBEDTLS_DES_KEY_SIZE) == 0)
        {
            return 1;
        }
    }

    return 0;
}

void mbedtls_des_setkey (uint32_t SK[32], const unsigned char key[MBEDTLS_DES_KEY_SIZE])
{
    // not support
}

/*
 * DES key schedule (56-bit, encryption)
 */
int mbedtls_des_setkey_enc (mbedtls_des_context * ctx, const unsigned char key[MBEDTLS_DES_KEY_SIZE])
{
    uint32_t * ukey = (uint32_t *) key;
    if (ctx == NULL)
    {
        return 0;
    }

   #if 0
    #if defined(MBEDTLS_THREADING_C)
    if (mbedtls_mutex_lock(&mutex) != 0)
    {
        return MBEDTLS_ERR_THREADING_MUTEX_ERROR;
    }
    #endif

    MEM_LONG_WRITE(DES_KEY1, *ukey);
    MEM_LONG_WRITE(DES_KEY1 + 4, *(ukey + 1));
    MEM_LONG_WRITE(DES_KEY2, *ukey);
    MEM_LONG_WRITE(DES_KEY2 + 4, *(ukey + 1));
    MEM_LONG_WRITE(DES_KEY3, *ukey);
    MEM_LONG_WRITE(DES_KEY3 + 4, *(ukey + 1));
    #if defined(MBEDTLS_THREADING_C)
    if (mbedtls_mutex_unlock(&mutex) != 0)
    {
        return MBEDTLS_ERR_THREADING_MUTEX_ERROR;
    }
    #endif
   #else
    ctx->sk[0] = *ukey;
    ctx->sk[1] = *(ukey + 1);
    ctx->sk[2] = *ukey;
    ctx->sk[3] = *(ukey + 1);
    ctx->sk[4] = *ukey;
    ctx->sk[5] = *(ukey + 1);
   #endif

    return 0;
}

/*
 * DES key schedule (56-bit, decryption)
 */
int mbedtls_des_setkey_dec (mbedtls_des_context * ctx, const unsigned char key[MBEDTLS_DES_KEY_SIZE])
{
    uint32_t * ukey = (uint32_t *) key;
    if (ctx == NULL)
    {
        return 0;
    }

   #if 0
    #if defined(MBEDTLS_THREADING_C)
    if (mbedtls_mutex_lock(&mutex) != 0)
    {
        return MBEDTLS_ERR_THREADING_MUTEX_ERROR;
    }
    #endif

    MEM_LONG_WRITE(DES_KEY1, *ukey);
    MEM_LONG_WRITE(DES_KEY1 + 4, *(ukey + 1));
    MEM_LONG_WRITE(DES_KEY2, *ukey);
    MEM_LONG_WRITE(DES_KEY2 + 4, *(ukey + 1));
    MEM_LONG_WRITE(DES_KEY3, *ukey);
    MEM_LONG_WRITE(DES_KEY3 + 4, *(ukey + 1));
    #if defined(MBEDTLS_THREADING_C)
    if (mbedtls_mutex_unlock(&mutex) != 0)
    {
        return MBEDTLS_ERR_THREADING_MUTEX_ERROR;
    }
    #endif
   #else
    ctx->sk[0] = *ukey;
    ctx->sk[1] = *(ukey + 1);
    ctx->sk[2] = *ukey;
    ctx->sk[3] = *(ukey + 1);
    ctx->sk[4] = *ukey;
    ctx->sk[5] = *(ukey + 1);
   #endif

    return 0;
}

/*
 * Triple-DES key schedule (112-bit, encryption)
 */
int mbedtls_des3_set2key_enc (mbedtls_des3_context * ctx, const unsigned char key[MBEDTLS_DES_KEY_SIZE * 2])
{
    uint32_t * ukey = (uint32_t *) key;
    if (ctx == NULL)
    {
        return 0;
    }

   #if 0
    #if defined(MBEDTLS_THREADING_C)
    if (mbedtls_mutex_lock(&mutex) != 0)
    {
        return MBEDTLS_ERR_THREADING_MUTEX_ERROR;
    }
    #endif
    MEM_LONG_WRITE(DES_KEY1, *ukey);
    MEM_LONG_WRITE(DES_KEY1 + 4, *(ukey + 1));
    MEM_LONG_WRITE(DES_KEY2, *(ukey + 2));
    MEM_LONG_WRITE(DES_KEY2 + 4, *(ukey + 3));
    MEM_LONG_WRITE(DES_KEY3, *ukey);
    MEM_LONG_WRITE(DES_KEY3 + 4, *(ukey + 1));
    #if defined(MBEDTLS_THREADING_C)
    if (mbedtls_mutex_unlock(&mutex) != 0)
    {
        return MBEDTLS_ERR_THREADING_MUTEX_ERROR;
    }
    #endif
   #else
    ctx->sk[0] = *ukey;
    ctx->sk[1] = *(ukey + 1);
    ctx->sk[2] = *(ukey + 2);
    ctx->sk[3] = *(ukey + 3);
    ctx->sk[4] = *ukey;
    ctx->sk[5] = *(ukey + 1);
   #endif

    return 0;
}

/*
 * Triple-DES key schedule (112-bit, decryption)
 */
int mbedtls_des3_set2key_dec (mbedtls_des3_context * ctx, const unsigned char key[MBEDTLS_DES_KEY_SIZE * 2])
{
    uint32_t * ukey = (uint32_t *) key;
    if (ctx == NULL)
    {
        return 0;
    }

   #if 0
    #if defined(MBEDTLS_THREADING_C)
    if (mbedtls_mutex_lock(&mutex) != 0)
    {
        return MBEDTLS_ERR_THREADING_MUTEX_ERROR;
    }
    #endif

    MEM_LONG_WRITE(DES_KEY1, *ukey);
    MEM_LONG_WRITE(DES_KEY1 + 4, *(ukey + 1));
    MEM_LONG_WRITE(DES_KEY2, *(ukey + 2));
    MEM_LONG_WRITE(DES_KEY2 + 4, *(ukey + 3));
    MEM_LONG_WRITE(DES_KEY3, *ukey);
    MEM_LONG_WRITE(DES_KEY3 + 4, *(ukey + 1));
    #if defined(MBEDTLS_THREADING_C)
    if (mbedtls_mutex_unlock(&mutex) != 0)
    {
        return MBEDTLS_ERR_THREADING_MUTEX_ERROR;
    }
    #endif
   #else
    ctx->sk[0] = *ukey;
    ctx->sk[1] = *(ukey + 1);
    ctx->sk[2] = *(ukey + 2);
    ctx->sk[3] = *(ukey + 3);
    ctx->sk[4] = *ukey;
    ctx->sk[5] = *(ukey + 1);
   #endif

    return 0;
}

/*
 * Triple-DES key schedule (168-bit, encryption)
 */
int mbedtls_des3_set3key_enc (mbedtls_des3_context * ctx, const unsigned char key[MBEDTLS_DES_KEY_SIZE * 3])
{
    uint32_t * ukey = (uint32_t *) key;
    if (ctx == NULL)
    {
        return 0;
    }

   #if 0
    #if defined(MBEDTLS_THREADING_C)
    if (mbedtls_mutex_lock(&mutex) != 0)
    {
        return MBEDTLS_ERR_THREADING_MUTEX_ERROR;
    }
    #endif

    MEM_LONG_WRITE(DES_KEY1, *ukey);
    MEM_LONG_WRITE(DES_KEY1 + 4, *(ukey + 1));
    MEM_LONG_WRITE(DES_KEY2, *(ukey + 2));
    MEM_LONG_WRITE(DES_KEY2 + 4, *(ukey + 3));
    MEM_LONG_WRITE(DES_KEY3, *(ukey + 4));
    MEM_LONG_WRITE(DES_KEY3 + 4, *(ukey + 5));
    #if defined(MBEDTLS_THREADING_C)
    if (mbedtls_mutex_unlock(&mutex) != 0)
    {
        return MBEDTLS_ERR_THREADING_MUTEX_ERROR;
    }
    #endif
   #else
    ctx->sk[0] = *ukey;
    ctx->sk[1] = *(ukey + 1);
    ctx->sk[2] = *(ukey + 2);
    ctx->sk[3] = *(ukey + 3);
    ctx->sk[4] = *(ukey + 4);
    ctx->sk[5] = *(ukey + 5);
   #endif

    return 0;
}

/*
 * Triple-DES key schedule (168-bit, decryption)
 */
int mbedtls_des3_set3key_dec (mbedtls_des3_context * ctx, const unsigned char key[MBEDTLS_DES_KEY_SIZE * 3])
{
    uint32_t * ukey = (uint32_t *) key;
    if (ctx == NULL)
    {
        return 0;
    }

   #if 0
    #if defined(MBEDTLS_THREADING_C)
    if (mbedtls_mutex_lock(&mutex) != 0)
    {
        return MBEDTLS_ERR_THREADING_MUTEX_ERROR;
    }
    #endif

    MEM_LONG_WRITE(DES_KEY1, *ukey);
    MEM_LONG_WRITE(DES_KEY1 + 4, *(ukey + 1));
    MEM_LONG_WRITE(DES_KEY2, *(ukey + 2));
    MEM_LONG_WRITE(DES_KEY2 + 4, *(ukey + 3));
    MEM_LONG_WRITE(DES_KEY3, *(ukey + 4));
    MEM_LONG_WRITE(DES_KEY3 + 4, *(ukey + 5));
    #if defined(MBEDTLS_THREADING_C)
    if (mbedtls_mutex_unlock(&mutex) != 0)
    {
        return MBEDTLS_ERR_THREADING_MUTEX_ERROR;
    }
    #endif
   #else
    ctx->sk[0] = *ukey;
    ctx->sk[1] = *(ukey + 1);
    ctx->sk[2] = *(ukey + 2);
    ctx->sk[3] = *(ukey + 3);
    ctx->sk[4] = *(ukey + 4);
    ctx->sk[5] = *(ukey + 5);
   #endif

    return 0;
}

/*
 * DES-CBC buffer encryption/decryption
 */

static void alt_wait (void)
{
    uint32_t iter;
    for (iter = 0; iter < 38; iter++)
    {
        NOP_EXECUTION();
        NOP_EXECUTION();
        NOP_EXECUTION();
    }
}

static int alt_des3_crypt_cbc (mbedtls_des3_context * ctx,
                               int                    mode,
                               size_t                 length,
                               unsigned char          iv[8],
                               const unsigned char  * input,
                               unsigned char        * output)
{
    uint32_t * uiv     = (uint32_t *) iv;
    uint32_t * uinput  = (uint32_t *) input;
    uint32_t * uoutput = (uint32_t *) output;

    unsigned int i;

    if (length % 8)
    {
        return -1;
    }

    if (ctx == NULL)
    {
        return 0;
    }

    MEM_LONG_WRITE(CBC_IV, *uiv);
    MEM_LONG_WRITE((CBC_IV + 4), *(uiv + 1));

    if (mode == MBEDTLS_DES_ENCRYPT)
    {
        // first data write
        MEM_LONG_WRITE(DES_INPUT, *uinput);
        MEM_LONG_WRITE((DES_INPUT + 4), *(++uinput));

        // ed_sel en
        // start
        MEM_LONG_WRITE(DES_CONTROL, 0x00010101);
        NOP_EXECUTION();
        NOP_EXECUTION();
        NOP_EXECUTION();
        NOP_EXECUTION();

        // wait
        alt_wait();
        if (((*((volatile unsigned int *) (DES_CONTROL))) & 0x01000000) == 0)
        {
            goto alt_des_exit;
        }

        // get 64bit data
        MEM_LONG_READ(DES_OUTPUT, (uoutput++));
        MEM_LONG_READ(DES_OUTPUT + 4, (uoutput++));
        for (i = 8; i < length; i += 8)
        {
            // data write
            MEM_LONG_WRITE(DES_INPUT, *(++uinput));
            MEM_LONG_WRITE((DES_INPUT + 4), *(++uinput));

            // start
            MEM_LONG_WRITE(DES_CONTROL, 0x00000101);
            NOP_EXECUTION();
            NOP_EXECUTION();
            NOP_EXECUTION();
            NOP_EXECUTION();

            // wait
            alt_wait();
            if (((*((volatile unsigned int *) (DES_CONTROL))) & 0x01000000) == 0)
            {
                goto alt_des_exit;
            }

            // get
            MEM_LONG_READ(DES_OUTPUT, (uoutput++));
            MEM_LONG_READ(DES_OUTPUT + 4, (uoutput++));
        }

        // vector update
        memcpy(iv, (uoutput - 2), 8);
    }
    else
    {
        // first data write
        MEM_LONG_WRITE(DES_INPUT, *uinput);
        MEM_LONG_WRITE((DES_INPUT + 4), *(++uinput));

        // ed_sel en
        // start
        MEM_LONG_WRITE(DES_CONTROL, 0x00010100);
        NOP_EXECUTION();
        NOP_EXECUTION();
        NOP_EXECUTION();
        NOP_EXECUTION();

        // wait
        alt_wait();
        if (((*((volatile unsigned int *) (DES_CONTROL))) & 0x01000000) == 0)
        {
            goto alt_des_exit;
        }

        // vector update
        memcpy(iv, (uinput - 1), 8);

        // get 64bit data
        MEM_LONG_READ(DES_OUTPUT, (uoutput++));
        MEM_LONG_READ(DES_OUTPUT + 4, (uoutput++));
        for (i = 8; i < length; i += 8)
        {
            // data write
            MEM_LONG_WRITE(DES_INPUT, *(++uinput));
            MEM_LONG_WRITE((DES_INPUT + 4), *(++uinput));

            // start
            MEM_LONG_WRITE(DES_CONTROL, 0x00000100);
            NOP_EXECUTION();
            NOP_EXECUTION();
            NOP_EXECUTION();
            NOP_EXECUTION();

            // wait
            alt_wait();
            if (((*((volatile unsigned int *) (DES_CONTROL))) & 0x01000000) == 0)
            {
                goto alt_des_exit;
            }

            // vector update
            memcpy(iv, (uinput - 1), 8);

            // get
            MEM_LONG_READ(DES_OUTPUT, (uoutput++));
            MEM_LONG_READ(DES_OUTPUT + 4, (uoutput++));
        }

        // vector update
        // memcpy(iv, (uinput - 1), 8);
    }

alt_des_exit:

    return 0;
}

int mbedtls_des_crypt_cbc (mbedtls_des_context * ctx,
                           int                   mode,
                           size_t                length,
                           unsigned char         iv[8],
                           const unsigned char * input,
                           unsigned char       * output)
{
    int ret;
   #if defined(MBEDTLS_THREADING_C)
    if (mbedtls_mutex_lock(&mutex) != 0)
    {
        return MBEDTLS_ERR_THREADING_MUTEX_ERROR;
    }
   #endif
    MEM_LONG_WRITE(DES_KEY1, ctx->sk[0]);
    MEM_LONG_WRITE(DES_KEY1 + 4, ctx->sk[1]);
    MEM_LONG_WRITE(DES_KEY2, ctx->sk[2]);
    MEM_LONG_WRITE(DES_KEY2 + 4, ctx->sk[3]);
    MEM_LONG_WRITE(DES_KEY3, ctx->sk[4]);
    MEM_LONG_WRITE(DES_KEY3 + 4, ctx->sk[5]);
    ret = alt_des3_crypt_cbc((mbedtls_des3_context *) ctx, mode, length, iv, input, output);
   #if defined(MBEDTLS_THREADING_C)
    if (mbedtls_mutex_unlock(&mutex) != 0)
    {
        return MBEDTLS_ERR_THREADING_MUTEX_ERROR;
    }
   #endif

    return ret;
}

/*
 * 3DES-CBC buffer encryption/decryption
 */
int mbedtls_des3_crypt_cbc (mbedtls_des3_context * ctx,
                            int                    mode,
                            size_t                 length,
                            unsigned char          iv[8],
                            const unsigned char  * input,
                            unsigned char        * output)
{
    int ret;
   #if defined(MBEDTLS_THREADING_C)
    if (mbedtls_mutex_lock(&mutex) != 0)
    {
        return MBEDTLS_ERR_THREADING_MUTEX_ERROR;
    }
   #endif
    MEM_LONG_WRITE(DES_KEY1, ctx->sk[0]);
    MEM_LONG_WRITE(DES_KEY1 + 4, ctx->sk[1]);
    MEM_LONG_WRITE(DES_KEY2, ctx->sk[2]);
    MEM_LONG_WRITE(DES_KEY2 + 4, ctx->sk[3]);
    MEM_LONG_WRITE(DES_KEY3, ctx->sk[4]);
    MEM_LONG_WRITE(DES_KEY3 + 4, ctx->sk[5]);
    ret = alt_des3_crypt_cbc(ctx, mode, length, iv, input, output);
   #if defined(MBEDTLS_THREADING_C)
    if (mbedtls_mutex_unlock(&mutex) != 0)
    {
        return MBEDTLS_ERR_THREADING_MUTEX_ERROR;
    }
   #endif

    return ret;
}

int mbedtls_des_crypt_ecb (mbedtls_des_context * ctx, const unsigned char input[8], unsigned char output[8])
{

    // not support;
    return 0;
}

int mbedtls_des3_crypt_ecb (mbedtls_des3_context * ctx, const unsigned char input[8], unsigned char output[8])
{

    // not support;
    return 0;
}

  #endif                               /* MBEDTLS_DES_ALT */
 #endif                                /* MBEDTLS_DES_C */
#endif
