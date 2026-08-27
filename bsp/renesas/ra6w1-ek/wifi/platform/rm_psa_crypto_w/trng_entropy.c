/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include "common.h"

#include "mbedtls/error.h"
#include "mbedtls/entropy.h"
#include "entropy_poll.h"

#include "mbedtls/platform.h"

#if defined(MBEDTLS_ENTROPY_HARDWARE_ALT)
 #include "llf_rnd_trng.h"
 #include "cc_rng_plat.h"
 #include "cc_rnd_common.h"
 #include "cc_pal_log.h"
 #include "cc_pal_mem.h"

static uint32_t mbedtls_hardware_poll_fci_rflag;

static void mbedtls_hardware_poll_cleanup (CCRndWorkBuff_t * p_rndWordBuff)
{
    mbedtls_platform_zeroize(p_rndWordBuff, sizeof(CCRndWorkBuff_t));
    mbedtls_free(p_rndWordBuff);
}

void mbedtls_hardware_poll_fci_customize (uint32_t rflag)
{
    mbedtls_hardware_poll_fci_rflag = rflag;
}

/*******************************************************************************************************************//**
 * Reads a random value from the TRNG.
 **********************************************************************************************************************/
int mbedtls_hardware_poll (void * data, unsigned char * output, size_t len, size_t * olen)
{
    CCRndWorkBuff_t * rndWorkBuff_ptr;
    CCRndState_t      rndState;
    CCRndParams_t     trngParams;
    int               ret;
    uint32_t        * entrSource_ptr;
    uint32_t          hiddenscaler = mbedtls_hardware_poll_fci_rflag;

    CC_UNUSED_PARAM(data);

    if (NULL == output)
    {
        CC_PAL_LOG_ERR("output cannot be NULL\n");

        return -1;
    }

    if (NULL == olen)
    {
        CC_PAL_LOG_ERR("olen cannot be NULL\n");

        return -1;
    }

    if (0 == len)
    {
        CC_PAL_LOG_ERR("len cannot be zero\n");

        return -1;
    }

    rndWorkBuff_ptr = (CCRndWorkBuff_t *) mbedtls_calloc(1, sizeof(CCRndWorkBuff_t));
    if (NULL == rndWorkBuff_ptr)
    {
        CC_PAL_LOG_ERR("Error: cannot allocate memory for rndWorkbuff\n");

        return -1;
    }

    CC_PalMemSetZero(&rndState, sizeof(CCRndState_t));
    CC_PalMemSetZero(&trngParams, sizeof(CCRndParams_t));

    ret = RNG_PLAT_SetUserRngParameters(&trngParams);
    if (ret != 0)
    {
        CC_PAL_LOG_ERR("Error: RNG_PLAT_SetUserRngParameters() failed.\n");
        mbedtls_hardware_poll_cleanup(rndWorkBuff_ptr);

        return -1;
    }

    if (((hiddenscaler >> 24) & 0x0ff) != 0)
    {
        trngParams.userParams.SubSamplingRatio1 = trngParams.userParams.SubSamplingRatio1 /
                                                  ((hiddenscaler >> 24) & 0x0ff);
    }

    if (((hiddenscaler >> 16) & 0x0ff) != 0)
    {
        trngParams.userParams.SubSamplingRatio2 = trngParams.userParams.SubSamplingRatio2 /
                                                  ((hiddenscaler >> 16) & 0x0ff);
    }

    if (((hiddenscaler >> 8) & 0x0ff) != 0)
    {
        trngParams.userParams.SubSamplingRatio3 = trngParams.userParams.SubSamplingRatio3 /
                                                  ((hiddenscaler >> 8) & 0x0ff);
    }

    if (((hiddenscaler >> 0) & 0x0ff) != 0)
    {
        trngParams.userParams.SubSamplingRatio4 = trngParams.userParams.SubSamplingRatio4 /
                                                  ((hiddenscaler >> 0) & 0x0ff);
    }

    ret = LLF_RND_GetTrngSource(&rndState,                    /*in/out*/
                                &trngParams,                  /*in/out*/
                                0,                            /*in  -  isContinued - false*/
                                (uint32_t *) &len,            /*in/out*/
                                &entrSource_ptr,              /*out*/
                                (uint32_t *) olen,            /*out*/
                                (uint32_t *) rndWorkBuff_ptr, /*in*/
                                0 /*in - isFipsSupport false*/);
    if (ret != 0)
    {
        CC_PAL_LOG_ERR("Error: LLF_RND_GetTrngSource() failed.\n");
        mbedtls_hardware_poll_cleanup(rndWorkBuff_ptr);

        return -1;
    }

    if (*olen <= len)
    {
        CC_PalMemCopy(output, entrSource_ptr + CC_RND_TRNG_SRC_INNER_OFFSET_WORDS, *olen);
    }
    else
    {
        *olen = len;
        CC_PalMemCopy(output, entrSource_ptr + CC_RND_TRNG_SRC_INNER_OFFSET_WORDS, *olen);
    }

    mbedtls_hardware_poll_cleanup(rndWorkBuff_ptr);

    return 0;
}

#endif                                 /* MBEDTLS_ENTROPY_HARDWARE_ALT */
