#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "r_cc312_common.h"
#include "cc_pal_types.h"
#include "cc_pal_mem.h"
#include "cc_pal_perf.h"
#include "cc_regs.h"
#include "cc_hal_plat.h"
#include "bsv_api.h"
#include "cryptocell/cc_lib.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "crypto_primitives.h"
#include "r_cc312_crypto.h"

typedef struct
{
    CCRndContext_t *rnd_context;
    CCRndWorkBuff_t *rnd_work_buffer;
} wifi_crypto_driver_t;

static wifi_crypto_driver_t *s_crypto_driver;
static bool s_cc_lib_initialized;

#define WIFI_CRYPTO_ALLOC(pointer, type, count) \
    do \
    { \
        (pointer) = (type *)CRYPTO_MALLOC((count) * sizeof(type)); \
        if ((pointer) == NULL) \
        { \
            goto failed; \
        } \
    } while (0)

static void wifi_crypto_driver_free(void)
{
    if (s_crypto_driver == NULL)
    {
        return;
    }

    if (s_cc_lib_initialized)
    {
        (void)CC_LibFini(s_crypto_driver->rnd_context);
        s_cc_lib_initialized = false;
    }

    if (s_crypto_driver->rnd_context != NULL)
    {
        CRYPTO_FREE(s_crypto_driver->rnd_context->rndState);
        CRYPTO_FREE(s_crypto_driver->rnd_context->entropyCtx);
        CRYPTO_FREE(s_crypto_driver->rnd_context);
    }

    CRYPTO_FREE(s_crypto_driver->rnd_work_buffer);
    CRYPTO_FREE(s_crypto_driver);
    s_crypto_driver = NULL;
}

void R_CC312_Crypto_ClkMgmt(uint32_t mode)
{
    (void)mode;
}

void R_CC312_Crypto_ReInit(void)
{
    if (s_crypto_driver != NULL)
    {
        (void)CC_BsvCoreClkGatingEnable(RRQ61X_ACRYPT_BASE);
    }
}

uint32_t R_CC312_Crypto_Init(uint32_t rflag)
{
    uint32_t lcs;
    CCError_t status;

    if (s_crypto_driver != NULL)
    {
        (void)CC_BsvCoreClkGatingEnable(RRQ61X_ACRYPT_BASE);
        return 2U;
    }

    WIFI_CRYPTO_ALLOC(s_crypto_driver, wifi_crypto_driver_t, 1U);
    WIFI_CRYPTO_ALLOC(s_crypto_driver->rnd_context, CCRndContext_t, 1U);
    WIFI_CRYPTO_ALLOC(s_crypto_driver->rnd_work_buffer, CCRndWorkBuff_t, 1U);

    memset(s_crypto_driver->rnd_context, 0, sizeof(*s_crypto_driver->rnd_context));
    memset(s_crypto_driver->rnd_work_buffer, 0,
           sizeof(*s_crypto_driver->rnd_work_buffer));

    if (sizeof(CCRndState_t) > sizeof(mbedtls_ctr_drbg_context))
    {
        WIFI_CRYPTO_ALLOC(s_crypto_driver->rnd_context->rndState,
                          CCRndState_t, 1U);
    }
    else
    {
        WIFI_CRYPTO_ALLOC(s_crypto_driver->rnd_context->rndState,
                          mbedtls_ctr_drbg_context, 1U);
    }

    WIFI_CRYPTO_ALLOC(s_crypto_driver->rnd_context->entropyCtx,
                      mbedtls_entropy_context, 1U);
    memset(s_crypto_driver->rnd_context->rndState, 0,
           (sizeof(CCRndState_t) > sizeof(mbedtls_ctr_drbg_context)) ?
           sizeof(CCRndState_t) : sizeof(mbedtls_ctr_drbg_context));
    memset(s_crypto_driver->rnd_context->entropyCtx, 0,
           sizeof(mbedtls_entropy_context));

    /* This is the same order used by the original RM_WIFI setup. */
    (void)CC_BsvCoreClkGatingEnable(RRQ61X_ACRYPT_BASE);
    status = CC_BsvLcsGetAndInit(RRQ61X_ACRYPT_BASE, &lcs);
    if (status != CC_OK)
    {
        goto failed;
    }

    s_crypto_driver->rnd_work_buffer->ccRndIntWorkBuff[0] = 0xFC9CC312U;
    s_crypto_driver->rnd_work_buffer->ccRndIntWorkBuff[1] = rflag;

    /* The hardware entropy adapter consumes the hidden rflag set by RM_WIFI. */
    extern void mbedtls_hardware_poll_fci_customize(uint32_t rflag);
    mbedtls_hardware_poll_fci_customize(rflag);

    if (CC_LibInit(s_crypto_driver->rnd_context,
                   s_crypto_driver->rnd_work_buffer) != CC_LIB_RET_OK)
    {
        goto failed;
    }

    s_cc_lib_initialized = true;
    return 1U;

failed:
    wifi_crypto_driver_free();
    return 0U;
}

void R_CC312_Crypto_Finish(void)
{
    wifi_crypto_driver_free();
}

void R_CC312_SetCryptoCoreClkGating(uint32_t mode)
{
    CC_HAL_WRITE_REGISTER(CC_REG_OFFSET(CRY_KERNEL,
                                        HOST_CORE_CLK_GATING_ENABLE),
                          (mode != 0U) ? 1U : 0U);
}
