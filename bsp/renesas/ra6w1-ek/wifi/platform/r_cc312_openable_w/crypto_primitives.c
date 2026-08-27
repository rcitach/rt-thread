/**
 ****************************************************************************************
 *
 * @file crypto_primitives.c
 *
 * @brief Crypto specific
 *
 * Copyright (c) 2023-2024 Renesas Electronics. All rights reserved.
 *
 * This software ("Software") is owned by Renesas Electronics.
 *
 * By using this Software you agree that Renesas Electronics retains all
 * intellectual property and proprietary rights in and to this Software and any
 * use, reproduction, disclosure or distribution of the Software without express
 * written permission or a license agreement from Renesas Electronics is
 * strictly prohibited. This Software is solely for use on or in conjunction
 * with Renesas Electronics products.
 *
 * EXCEPT AS OTHERWISE PROVIDED IN A LICENSE AGREEMENT BETWEEN THE PARTIES, THE
 * SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. EXCEPT AS OTHERWISE
 * PROVIDED IN A LICENSE AGREEMENT BETWEEN THE PARTIES, IN NO EVENT SHALL
 * RENESAS ELECTRONICS BE LIABLE FOR ANY DIRECT, SPECIAL, INDIRECT, INCIDENTAL,
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THE SOFTWARE.
 *
 ****************************************************************************************
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "sdk_defs.h"
#include "hw_sys.h"
#include "r_cc312_common.h"

#if     (dg_configUSE_ROMCRYPTO == 1)
 #include "r_cc312_crypto.h"
 #include "r_cc312_secureboot.h"
 #include "cc_pal_mutex.h"
 #include "romcrypto.h"
#endif                                 // (dg_configUSE_ROMCRYPTO == 1)

// ---------------------------------------------------------
//
// ---------------------------------------------------------

#if     (dg_configUSE_ROMCRYPTO == 1)
static uint32_t embcrypto_otpread_w2w(uint32_t otpaddr);
static uint32_t embcrypto_otpwrite_w2w(uint32_t otpaddr, uint32_t otpData);

#endif

static CRYPTO_PRIMITIVE_TYPE * crypto_primitive;

#if     (dg_configUSE_ROMCRYPTO == 1)
extern CC_PalMutex CCSymCryptoMutex;
extern CC_PalMutex CCAsymCryptoMutex;

static const ROMCRYPTO_PLATFORM_TYPE sym_crypto_platform =
{
    R_CC312_Debug_SecureBoot_Mode,

    embcrypto_remapper,
    embcrypto_otpread_w2w,
    embcrypto_otpwrite_w2w,
    embcrypto_vprint,

    CC_PalAbort,

    CC_PalMemCmpPlat,
    CC_PalMemCopyPlat,
    CC_PalMemMovePlat,
    CC_PalMemSetPlat,
    CC_PalMemSetZeroPlat,
    CC_PalMemMallocPlat,
    CC_PalMemReallocPlat,
    CC_PalMemFreePlat,

    ((CCError_t (*)(void *,                    uint32_t))CC_PalMutexLock),
    ((CCError_t (*)(void *))CC_PalMutexUnlock),

    CC_PalMemMap,
    CC_PalMemUnMap,
    CC_PalWaitInterrupt,
    CC_PalPowerSaveModeSelect,

    &CCSymCryptoMutex,
    &CCAsymCryptoMutex
};
#endif                                 // (dg_configUSE_ROMCRYPTO == 1)

/******************************************************************************
 *  init_crypto_primitives()
 *
 *  Purpose :
 *  Input   :
 *  Output  :
 *  Return  :
 ******************************************************************************/

void init_crypto_primitives (const CRYPTO_PRIMITIVE_TYPE * primitive)
{
#if     (dg_configUSE_ROMCRYPTO == 1)
    init_romcrypto_platform(&sym_crypto_platform);
#endif                                 // (dg_configUSE_ROMCRYPTO == 1)
    crypto_primitive = (CRYPTO_PRIMITIVE_TYPE *) primitive;
}

/******************************************************************************
 *  hal trace
 *
 *  Purpose :
 *  Input   :
 *  Output  :
 *  Return  :
 ******************************************************************************/

void embcrypto_dump (uint16_t tag, void * srcdata, uint16_t len)
{
    if ((crypto_primitive != NULL) && (crypto_primitive->retarget_dump != NULL))
    {
        crypto_primitive->retarget_dump(tag, srcdata, len);
    }
}

void embcrypto_text (uint16_t tag, void * srcdata, uint16_t len)
{
    if ((crypto_primitive != NULL) && (crypto_primitive->retarget_text != NULL))
    {
        crypto_primitive->retarget_text(tag, srcdata, len);
    }
}

void embcrypto_vprint (uint16_t tag, const char * format, va_list arg)
{
    if ((crypto_primitive != NULL) && (crypto_primitive->retarget_vprint != NULL))
    {
        crypto_primitive->retarget_vprint(tag, format, arg);
    }
}

void embcrypto_print (uint16_t tag, const char * fmt, ...)
{
    if ((crypto_primitive != NULL) && (crypto_primitive->retarget_vprint != NULL))
    {
        va_list ap;
        va_start(ap, fmt);

        crypto_primitive->retarget_vprint(tag, fmt, ap);

        va_end(ap);
    }
}

int embcrypto_getchar (uint32_t mode)
{
    if ((crypto_primitive != NULL) && (crypto_primitive->retarget_getchar != NULL))
    {
        return crypto_primitive->retarget_getchar(mode);
    }

    return 0;
}

/******************************************************************************
 *  hal alloc
 *
 *  Purpose :
 *  Input   :
 *  Output  :
 *  Return  :
 ******************************************************************************/

void * embcrypto_malloc (size_t size)
{
    void * allocbuf;
    if ((crypto_primitive == NULL) || (crypto_primitive->raw_malloc == NULL))
    {
        return NULL;
    }

    allocbuf = crypto_primitive->raw_malloc(size);

    // if( allocbuf != NULL ){
    // memset(allocbuf, 0, size);
    // }

    return allocbuf;
}

void * embcrypto_realloc (void * buffer, size_t NewSize)
{
    void * allocbuf;
    if ((crypto_primitive == NULL) || (crypto_primitive->raw_realloc == NULL))
    {
        return NULL;
    }

    allocbuf = crypto_primitive->raw_realloc(buffer, NewSize);

    return allocbuf;
}

void embcrypto_free (void * f)
{
    if ((crypto_primitive == NULL) || (crypto_primitive->raw_free == NULL))
    {
        return;
    }

    crypto_primitive->raw_free(f);
}

void embcrypto_memset (void * dest, int value, size_t num)
{
    // TODO:
    memset(dest, value, num);
}

/******************************************************************************
 *  hal otp
 *
 *  Purpose :
 *  Input   :
 *  Output  :
 *  Return  :
 ******************************************************************************/

uint32_t embcrypto_otpread (uint32_t otpaddr)
{
    uint32_t otpdata;
    if ((crypto_primitive == NULL) || (crypto_primitive->raw_otpread == NULL))
    {
        return (uint32_t) (-1);
    }

    // embcrypto_otpread - byte offset
    // raw_otpread - word offset

    otpdata = crypto_primitive->raw_otpread((otpaddr >> 2));

    return otpdata;
}

uint32_t embcrypto_otpwrite (uint32_t otpaddr, uint32_t otpData)
{
    if ((crypto_primitive == NULL) || (crypto_primitive->raw_otpwrite == NULL))
    {
        return (uint32_t) (-1);
    }

    // embcrypto_otpwrite - byte offset
    // raw_otpwrite - word offset

    return crypto_primitive->raw_otpwrite((otpaddr >> 2), otpData);
}

#if     (dg_configUSE_ROMCRYPTO == 1)
static uint32_t embcrypto_otpread_w2w (uint32_t otpaddr)
{
    uint32_t otpdata;
    if ((crypto_primitive == NULL) || (crypto_primitive->raw_otpread == NULL))
    {
        return (uint32_t) (-1);
    }

    // embcrypto_otpread - byte offset
    // raw_otpread - word offset

    otpdata = crypto_primitive->raw_otpread((otpaddr));

    return otpdata;
}

static uint32_t embcrypto_otpwrite_w2w (uint32_t otpaddr, uint32_t otpData)
{
    if ((crypto_primitive == NULL) || (crypto_primitive->raw_otpwrite == NULL))
    {
        return (uint32_t) (-1);
    }

    // embcrypto_otpwrite - byte offset
    // raw_otpwrite - word offset

    return crypto_primitive->raw_otpwrite((otpaddr), otpData);
}

#endif

/******************************************************************************
 *  flash primitives
 *
 *  Purpose :
 *  Input   :
 *  Output  :
 *  Return  :
 ******************************************************************************/

void * embcrypto_flash_image_open (uint32_t mode, uint32_t imghdr_offset, void * locker)
{
    if ((crypto_primitive == NULL) || (crypto_primitive->flash_image_open == NULL))
    {
        return NULL;
    }

    return crypto_primitive->flash_image_open(mode, imghdr_offset, locker);
}

void embcrypto_flash_image_close (void * handler)
{
    if ((crypto_primitive == NULL) || (crypto_primitive->flash_image_close == NULL))
    {
        return;
    }

    crypto_primitive->flash_image_close(handler);
}

uint32_t embcrypto_flash_image_check (void * handler, uint32_t imgtype, uint32_t imghdr_offset)
{
    if ((crypto_primitive == NULL) || (crypto_primitive->flash_image_check == NULL))
    {
        return 0;
    }

    return crypto_primitive->flash_image_check(handler, imgtype, imghdr_offset);
}

void * embcrypto_flash_image_certificate (void     * handler,
                                          uint32_t   imghdr_offset,
                                          uint32_t   certindex,
                                          uint32_t * certsize)
{
    if ((crypto_primitive == NULL) || (crypto_primitive->flash_image_certificate == NULL))
    {
        return NULL;
    }

    return crypto_primitive->flash_image_certificate(handler, imghdr_offset, certindex, certsize);
}

uint32_t embcrypto_flash_image_extract (void     * handler,
                                        uint32_t   imghdr_offset,
                                        uint32_t * load_addr,
                                        uint32_t * jmp_addr)
{
    if ((crypto_primitive == NULL) || (crypto_primitive->flash_image_extract == NULL))
    {
        return 0;
    }

    return crypto_primitive->flash_image_extract(handler, imghdr_offset, load_addr, jmp_addr);
}

uint32_t embcrypto_flash_image_load (void * handler, uint32_t imghdr_offset, uint32_t * load_addr, uint32_t * jmp_addr)
{
    if ((crypto_primitive == NULL) || (crypto_primitive->flash_image_load == NULL))
    {
        return 0;
    }

    return crypto_primitive->flash_image_load(handler, imghdr_offset, load_addr, jmp_addr);
}

uint32_t embcrypto_flash_image_read (void * handler, uint32_t img_offset, void * load_addr, uint32_t img_secsize)
{
    if ((crypto_primitive == NULL) || (crypto_primitive->flash_image_read == NULL))
    {
        return 0;
    }

    return crypto_primitive->flash_image_read(handler, img_offset, load_addr, img_secsize);
}

uint32_t embcrypto_random (void)
{

    // TODO:
    return 0;
}

void embcrypto_rtosdelay (uint32_t usec)
{
    if ((crypto_primitive == NULL) || (crypto_primitive->rtosdelay == NULL))
    {
        return;
    }

    crypto_primitive->rtosdelay(usec);
}

uint64_t embcrypto_tickmeasure (uint32_t flag)
{
    if ((crypto_primitive == NULL) || (crypto_primitive->tickmeasure == NULL))
    {
        return 0;
    }

    return crypto_primitive->tickmeasure(flag);
}

uint32_t embcrypto_remapper (uint32_t vaddr)
{
    uint32_t               phy_addr;
    uint32_t               flash_region_base_offset;
    uint32_t               flash_region_size;
    HW_SYS_REMAP_ADDRESS_0 remap_addr0;
    static const uint32    remap[] =
    {
        MEMORY_ROM_BASE,
        MEMORY_OTP_BASE,
        MEMORY_OQSPIC_S_BASE,          // MEMORY_OQSPIC_BASE,
        MEMORY_SYSRAM_BASE,
        MEMORY_OQSPIC_S_BASE,
        MEMORY_OTP_BASE,
        MEMORY_CACHERAM_BASE,
        0
    };

    static const uint32 flash_region_sizes[] =
    {
        32 * 1024 * 1024,
        16 * 1024 * 1024,
        8 * 1024 * 1024,
        4 * 1024 * 1024,
        2 * 1024 * 1024,
        1 * 1024 * 1024,
        512 * 1024,
        256 * 1024,
    };

    remap_addr0 = hw_sys_get_memory_remapping();

    if (remap_addr0 != HW_SYS_REMAP_ADDRESS_0_TO_QSPI_FLASH)
    {
        if (vaddr >= MEMORY_REMAPPED_END)
        {
            phy_addr = vaddr;
        }
        else
        {
            phy_addr = vaddr + remap[remap_addr0];
        }
    }
    else
    {
        /* Take into account flash region base, offset and size */
        /* Wrong Address: flash_region_base_offset = REG_GETF(CACHE, CACHE_FLASH_REG, FLASH_REGION_BASE) << CACHE_CACHE_FLASH_REG_FLASH_REGION_BASE_Pos; */
        flash_region_base_offset = MEMORY_OQSPIC_S_BASE;

        flash_region_base_offset += REG_GETF(CACHE, CACHE_FLASH_REG, FLASH_REGION_OFFSET) << 2;
        flash_region_size         = flash_region_sizes[REG_GETF(CACHE, CACHE_FLASH_REG, FLASH_REGION_SIZE)];

        if (vaddr < MEMORY_REMAPPED_END)
        {
            /*
             * In the remapped region, accesses are only allowed when
             * 0 <= addr < flash_region_size.
             */
            ASSERT_ERROR(vaddr < flash_region_size);

            phy_addr = flash_region_base_offset + vaddr;
        }
        else if (IS_OQSPIC_ADDRESS(vaddr))
        {
            /*
             * In QSPI AHB-C bus, accesses are only allowed when
             * flash_region_base_offset <= addr
             *   AND
             * addr < flash_region_base_offset + flash_region_base_offset
             */
            ASSERT_ERROR(vaddr >= flash_region_base_offset);
            ASSERT_ERROR(vaddr < flash_region_base_offset + flash_region_size);
            phy_addr = vaddr;
        }
        else
        {
            phy_addr = vaddr;
        }
    }

    return phy_addr;
}
