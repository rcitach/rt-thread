/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "rm_wifi.h"
#include "FreeRTOS.h"
#include "semphr.h"

#include "r_rtc_w_helper.h"
#include "common.h"
#include "mbedtls/platform.h"
#if BSP_FEATURE_CRYPTO_HAS_CC312
#include "r_cc312_common.h"
#endif
#if defined(MBEDTLS_THREADING_C)
#include "mbedtls/threading.h"
#endif
#include "psa/crypto.h"

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

#define RM_WIFI_MBEDTLS_CON_BUF_SIZE    (256)

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global Variables
 **********************************************************************************************************************/

static char g_rm_wifi_mbedtls_con_buf[RM_WIFI_MBEDTLS_CON_BUF_SIZE];

/***********************************************************************************************************************
 * Private Functions
 *********************************************************************************************************************/

////////////////////////////////////////////////////////////////////
// Retargetted IO Functions for Cryptolib
////////////////////////////////////////////////////////////////////

static void rm_wifi_mbedtls_retarget_putstring(uint16_t tag, void * srcdata, uint16_t len)
{
    (void)tag;
    (void)len;

    printf((char *)srcdata);
}

static void rm_wifi_mbedtls_retarget_printf(uint16_t tag, const char * p_format, va_list arg)
{
    static char buf[512];

    (void)tag;

    vsnprintf(buf, 512, p_format, arg);

    printf(buf);
}

static void rm_wifi_mbedtls_retarget_rtosdelay(uint32_t usec)
{
    vTaskDelay(portCONVERT_MS_2_TICKS(usec / 1000));
}

static uint64_t rm_wifi_mbedtls_retarget_tickmeasure(uint32_t flag)
{
    (void)flag;

    return (uint64_t)0uL;
}

static void * rm_wifi_mbedtls_raw_calloc(size_t ssize)
{
    void * p_buf = NULL;

    p_buf = pvPortMalloc(ssize);
    if (p_buf != NULL)
    {
        memset(p_buf, 0x00, ssize);
    }

    return p_buf;
}

static void * rm_wifi_mbedtls_raw_realloc(void * p_buf, size_t size)
{
    typedef struct A_BLOCK_LINK
    {
        struct A_BLOCK_LINK * pxNextFreeBlock; /*<< The next free block in the list. */
        size_t xBlockSize;                     /*<< The size of the free block. */
    } BlockLink_t;

    void * p_newbuf = NULL;
    uint8_t * p_puc = NULL;

    BlockLink_t * p_link = NULL;

#ifdef portBYTE_ALIGNMENT
    static const size_t CCHeapStructSize =
        (sizeof(BlockLink_t) + ((size_t)(portBYTE_ALIGNMENT - 1))) & ~((size_t)portBYTE_ALIGNMENT_MASK);
#else
    #error "rm_wifi_mbedtls_realloc Error : xHeapStructSize"
#endif

    if (p_buf == NULL)
    {
        printf("ASSERT:p_buf is NULL!!\n");
        return NULL;
    }

    p_puc = (uint8_t *)p_buf;
    p_puc -= CCHeapStructSize;
    p_link = (BlockLink_t *)p_puc;

    p_newbuf = pvPortMalloc(size);

    if (p_newbuf != NULL)
    {
        memcpy(p_newbuf, p_buf, p_link->xBlockSize);
    }

    return p_newbuf;
}

static void rm_wifi_mbedtls_raw_free(void * p_ptr)
{
    if (p_ptr)
    {
        vPortFree(p_ptr);
    }

    return;
}

static uint32_t rm_wifi_mbedtls_raw_otp_read(uint32_t otpwoffset)
{
#if (dg_configUSE_HW_OTPC == 1)
    bsp_otp_init();
    return bsp_otp_word_read((uint32_t)(otpwoffset));
#else
    return 0;
#endif
}

static uint32_t rm_wifi_mbedtls_raw_otp_write(uint32_t otpwoffset, uint32_t otpData)
{
#if (dg_configUSE_HW_OTPC == 1)
    uint32_t TmpOtp = otpData;
    bsp_otp_init();
    bsp_otp_prog(&TmpOtp, (uint32_t)(otpwoffset), 1);
#endif
    return 0;
}

////////////////////////////////////////////////////////////////////
// Retargetted IO Functions for mbedTLS
////////////////////////////////////////////////////////////////////

static int rm_wifi_mbedtls_crypto_snprintf(char * p_buf, size_t n, const char * p_fmt, ...)
{
    int ret = 0;
    va_list args;

    va_start(args, p_fmt);
    ret = vsnprintf(p_buf, n, p_fmt, args);
    va_end(args);

    return ret;
}

static int rm_wifi_mbedtls_crypto_printf(const char * p_fmt, ...)
{
    size_t n;
    va_list ap;

    va_start(ap, p_fmt);

    n = RM_WIFI_MBEDTLS_CON_BUF_SIZE;
    n = (size_t) vsnprintf(g_rm_wifi_mbedtls_con_buf, n, p_fmt, ap);
    printf(g_rm_wifi_mbedtls_con_buf);

    va_end(ap);

    return (int)true;
}

static void * rm_wifi_mbedtls_crypto_calloc(size_t n, size_t size)
{
    void * p_buf = NULL;
    size_t allocsiz = n * size;

    if (allocsiz == 0)
    {
        printf("ASSERT:size-is-zero!!\n");
        return NULL;
    }

    p_buf = pvPortMalloc(allocsiz);
    if (p_buf != NULL)
    {
        memset(p_buf, 0x00, allocsiz);
    }

    return p_buf;
}

static void rm_wifi_mbedtls_crypto_free(void * p_ptr)
{
    if (p_ptr)
    {
        vPortFree(p_ptr);
    }

    return;
}

static void rm_wifi_mbedtls_crypto_exit(int status)
{
    printf("<Crypto.exit %d>\n", status);
}

static mbedtls_time_t rm_wifi_mbedtls_crypto_time(mbedtls_time_t * p_timer)
{
    mbedtls_time_t t;
    ra6w1_time64((__time64_t *)p_timer, (__time64_t *)&t);
    return (t);
}

#if defined(MBEDTLS_PLATFORM_NV_SEED_ALT)
static int rm_wifi_mbedtls_crypto_nv_seed_read(unsigned char * p_buf, size_t buf_len)
{
    int i;
    int alen;

    alen = buf_len & (~0x03);

    for (i = 0 ; i < alen ; i += sizeof(uint32_t))
    {
        *((uint32_t *) & (p_buf[i])) = SysTick->VAL;
    }

    return buf_len;
}

static int rm_wifi_mbedtls_crypto_nv_seed_write(unsigned char * p_buf, size_t buf_len)
{
    unsigned int i;
    uint32_t seed;

    for (i = 0 ; i < buf_len ; i += sizeof(uint32_t))
    {
        seed = *((uint32_t *)&(p_buf[i]));
        seed = seed ^ SysTick->VAL;
        srand(seed);
    }

    return buf_len;
}
#endif  //defined(MBEDTLS_PLATFORM_NV_SEED_ALT)

#if defined(MBEDTLS_THREADING_C) && defined(MBEDTLS_THREADING_ALT)
static void rm_wifi_mbedtls_crypto_mutex_init(mbedtls_threading_mutex_t * p_mutex)
{
    if (p_mutex == NULL)
    {
        printf("%s:null\n", __func__);
        return;
    }

    p_mutex->mutex = xSemaphoreCreateMutex();
}

static void rm_wifi_mbedtls_crypto_mutex_free(mbedtls_threading_mutex_t * p_mutex)
{
    if (p_mutex == NULL)
    {
        printf("%s:null\n", __func__);
        return;
    }

    if (p_mutex->mutex)
    {
        vSemaphoreDelete(p_mutex->mutex);
    }

    p_mutex->mutex = NULL;
}

static int rm_wifi_mbedtls_crypto_mutex_lock(mbedtls_threading_mutex_t * p_mutex)
{
    if (p_mutex == NULL)
    {
        printf("%s:null\n", __func__);
        return MBEDTLS_ERR_THREADING_BAD_INPUT_DATA;
    }

    if (xSemaphoreTake(p_mutex->mutex, portMAX_DELAY) != pdTRUE)
    {
        return MBEDTLS_ERR_THREADING_MUTEX_ERROR;
    }

    return 0;
}

static int rm_wifi_mbedtls_crypto_mutex_unlock(mbedtls_threading_mutex_t * p_mutex)
{
    if (p_mutex == NULL)
    {
        printf("%s:null\n", __func__);
        return MBEDTLS_ERR_THREADING_BAD_INPUT_DATA;
    }

    if (xSemaphoreGive(p_mutex->mutex) != pdTRUE)
    {
        return MBEDTLS_ERR_THREADING_MUTEX_ERROR;
    }

    return 0;
}
#endif  //defined(MBEDTLS_THREADING_C) && defined(MBEDTLS_THREADING_ALT)

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/

void RM_WIFI_mbedtls_setup_psa_crypto(void)
{
#if (dg_configCODE_LOCATION == NON_VOLATILE_IS_FLASH)
    static const CRYPTO_PRIMITIVE_TYPE crypto_primitive =
    {
        NULL,
        &rm_wifi_mbedtls_retarget_putstring,
        &rm_wifi_mbedtls_retarget_printf,
        NULL,
        &rm_wifi_mbedtls_raw_calloc,
        &rm_wifi_mbedtls_raw_realloc,
        &rm_wifi_mbedtls_raw_free,
        &rm_wifi_mbedtls_raw_otp_read,
        &rm_wifi_mbedtls_raw_otp_write,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        &rm_wifi_mbedtls_retarget_rtosdelay,
        &rm_wifi_mbedtls_retarget_tickmeasure
    };

    init_crypto_primitives(&crypto_primitive);

    mbedtls_platform_set_calloc_free(&rm_wifi_mbedtls_crypto_calloc, &rm_wifi_mbedtls_crypto_free);
    mbedtls_platform_set_snprintf(&rm_wifi_mbedtls_crypto_snprintf);
    mbedtls_platform_set_printf(&rm_wifi_mbedtls_crypto_printf);

    mbedtls_platform_set_exit(&rm_wifi_mbedtls_crypto_exit);
    mbedtls_platform_set_time(&rm_wifi_mbedtls_crypto_time);
#if defined(MBEDTLS_PLATFORM_NV_SEED_ALT)
    mbedtls_platform_set_nv_seed(&rm_wifi_mbedtls_crypto_nv_seed_read,
                                 &rm_wifi_mbedtls_crypto_nv_seed_write);
#endif //defined(MBEDTLS_PLATFORM_NV_SEED_ALT)
#if defined(MBEDTLS_THREADING_C) && defined(MBEDTLS_THREADING_ALT)
    mbedtls_threading_set_alt(&rm_wifi_mbedtls_crypto_mutex_init, &rm_wifi_mbedtls_crypto_mutex_free,
                              &rm_wifi_mbedtls_crypto_mutex_lock, &rm_wifi_mbedtls_crypto_mutex_unlock);
#endif  //defined(MBEDTLS_THREADING_C) && defined(MBEDTLS_THREADING_ALT)

#if BSP_FEATURE_CRYPTO_HAS_CC312
    R_CC312_Crypto_Init(0xffffffff);
#endif

#if defined(MBEDTLS_USE_PSA_CRYPTO) || defined(MBEDTLS_SSL_PROTO_TLS1_3)
    psa_crypto_init();
#endif  /* MBEDTLS_USE_PSA_CRYPTO || MBEDTLS_SSL_PROTO_TLS1_3 */
#endif

    return ;
}

