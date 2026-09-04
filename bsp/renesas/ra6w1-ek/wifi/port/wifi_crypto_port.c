#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <rtthread.h>

#include "FreeRTOS.h"
#include "semphr.h"

#include "bsp_otp.h"
#include "crypto_primitives.h"
#include "mbedtls/platform.h"
#include "mbedtls/threading.h"
#include "r_cc312_crypto.h"

#include "wifi_crypto_port.h"

static int s_crypto_platform_ready;
static int s_crypto_engine_ready;

static void wifi_crypto_retarget_putstring(uint16_t tag, void *srcdata,
                                           uint16_t len)
{
    uint16_t index;
    const char *text = (const char *)srcdata;

    (void)tag;

    if (text == NULL)
    {
        return;
    }

    for (index = 0U; index < len; index++)
    {
        rt_kprintf("%c", text[index]);
    }
}

static void wifi_crypto_retarget_printf(uint16_t tag, const char *format,
                                        va_list args)
{
    char buffer[256];

    (void)tag;

    if (format == NULL)
    {
        return;
    }

    (void)vsnprintf(buffer, sizeof(buffer), format, args);
    rt_kprintf("%s", buffer);
}

static void *wifi_crypto_raw_malloc(size_t size)
{
    void *buffer;

    if (size == 0U)
    {
        return NULL;
    }

    buffer = rt_malloc(size);
    if (buffer != NULL)
    {
        memset(buffer, 0, size);
    }

    return buffer;
}

static void *wifi_crypto_raw_realloc(void *buffer, size_t size)
{
    if (buffer == NULL)
    {
        return NULL;
    }

    return rt_realloc(buffer, size);
}

static void wifi_crypto_raw_free(void *buffer)
{
    if (buffer != NULL)
    {
        rt_free(buffer);
    }
}

static uint32_t wifi_crypto_raw_otp_read(uint32_t otp_word_offset)
{
    bsp_otp_init();
    return bsp_otp_word_read(otp_word_offset);
}

static uint32_t wifi_crypto_raw_otp_write(uint32_t otp_word_offset,
                                          uint32_t otp_data)
{
    bsp_otp_init();
    bsp_otp_prog(&otp_data, otp_word_offset, 1U);
    return 0U;
}

static void wifi_crypto_retarget_delay(uint32_t usec)
{
    uint32_t delay_ms;

    if (usec == 0U)
    {
        return;
    }

    delay_ms = (usec + 999U) / 1000U;
    rt_thread_mdelay((rt_int32_t)delay_ms);
}

static uint64_t wifi_crypto_retarget_tickmeasure(uint32_t flag)
{
    (void)flag;
    return 0U;
}

static void *wifi_crypto_mbedtls_calloc(size_t count, size_t size)
{
    size_t total;
    void *buffer;

    if ((size == 0U) || (count > ((size_t)-1) / size))
    {
        return NULL;
    }

    total = count * size;
    buffer = rt_malloc(total);
    if (buffer != NULL)
    {
        memset(buffer, 0, total);
    }

    return buffer;
}

static void wifi_crypto_mbedtls_free(void *buffer)
{
    if (buffer != NULL)
    {
        rt_free(buffer);
    }
}

static int wifi_crypto_mbedtls_snprintf(char *buffer, size_t size,
                                        const char *format, ...)
{
    int result;
    va_list args;

    va_start(args, format);
    result = vsnprintf(buffer, size, format, args);
    va_end(args);

    return result;
}

static int wifi_crypto_mbedtls_printf(const char *format, ...)
{
    int result;
    char buffer[256];
    va_list args;

    va_start(args, format);
    result = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    rt_kprintf("%s", buffer);
    return result;
}

static void wifi_crypto_mbedtls_exit(int status)
{
    rt_kprintf("[wifi-demo] mbedTLS exit: %d\n", status);
}

static mbedtls_time_t wifi_crypto_mbedtls_time(mbedtls_time_t *timer)
{
    mbedtls_time_t now;

    now = (mbedtls_time_t)(rt_tick_get() / RT_TICK_PER_SECOND);
    if (timer != NULL)
    {
        *timer = now;
    }

    return now;
}

static void wifi_crypto_mutex_init(mbedtls_threading_mutex_t *mutex)
{
    if (mutex != NULL)
    {
        mutex->mutex = xSemaphoreCreateMutex();
    }
}

static void wifi_crypto_mutex_free(mbedtls_threading_mutex_t *mutex)
{
    if ((mutex != NULL) && (mutex->mutex != NULL))
    {
        vSemaphoreDelete((SemaphoreHandle_t)mutex->mutex);
        mutex->mutex = NULL;
    }
}

static int wifi_crypto_mutex_lock(mbedtls_threading_mutex_t *mutex)
{
    if ((mutex == NULL) || (mutex->mutex == NULL))
    {
        return MBEDTLS_ERR_THREADING_BAD_INPUT_DATA;
    }

    if (xSemaphoreTake((SemaphoreHandle_t)mutex->mutex, portMAX_DELAY) != pdTRUE)
    {
        return MBEDTLS_ERR_THREADING_MUTEX_ERROR;
    }

    return 0;
}

static int wifi_crypto_mutex_unlock(mbedtls_threading_mutex_t *mutex)
{
    if ((mutex == NULL) || (mutex->mutex == NULL))
    {
        return MBEDTLS_ERR_THREADING_BAD_INPUT_DATA;
    }

    if (xSemaphoreGive((SemaphoreHandle_t)mutex->mutex) != pdTRUE)
    {
        return MBEDTLS_ERR_THREADING_MUTEX_ERROR;
    }

    return 0;
}

static const CRYPTO_PRIMITIVE_TYPE s_crypto_primitive =
{
    NULL,
    &wifi_crypto_retarget_putstring,
    &wifi_crypto_retarget_printf,
    NULL,
    &wifi_crypto_raw_malloc,
    &wifi_crypto_raw_realloc,
    &wifi_crypto_raw_free,
    &wifi_crypto_raw_otp_read,
    &wifi_crypto_raw_otp_write,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    &wifi_crypto_retarget_delay,
    &wifi_crypto_retarget_tickmeasure
};

static void wifi_demo_crypto_platform_init(void)
{
    if (s_crypto_platform_ready)
    {
        return;
    }

    /* crypto_primitives stores this table pointer, so it must be static. */
    init_crypto_primitives(&s_crypto_primitive);

    (void)mbedtls_platform_set_calloc_free(&wifi_crypto_mbedtls_calloc,
                                           &wifi_crypto_mbedtls_free);
    (void)mbedtls_platform_set_snprintf(&wifi_crypto_mbedtls_snprintf);
    (void)mbedtls_platform_set_printf(&wifi_crypto_mbedtls_printf);
    (void)mbedtls_platform_set_exit(&wifi_crypto_mbedtls_exit);
    (void)mbedtls_platform_set_time(&wifi_crypto_mbedtls_time);

#if defined(MBEDTLS_THREADING_C) && defined(MBEDTLS_THREADING_ALT)
    mbedtls_threading_set_alt(&wifi_crypto_mutex_init,
                              &wifi_crypto_mutex_free,
                              &wifi_crypto_mutex_lock,
                              &wifi_crypto_mutex_unlock);
#endif

    s_crypto_platform_ready = 1;
}

int wifi_demo_crypto_init(void)
{
    uint32_t status;

    if (s_crypto_engine_ready)
    {
        return 2;
    }

    wifi_demo_crypto_platform_init();

    /* The vendor RM_WIFI path uses this exact flag for the CC312 TRNG setup. */
    status = R_CC312_Crypto_Init(0xffffffffU);
    if ((status == 1U) || (status == 2U))
    {
        s_crypto_engine_ready = 1;
        return (int)status;
    }

    rt_kprintf("[wifi-demo] R_CC312_Crypto_Init failed: %lu\n",
               (unsigned long)status);
    return 0;
}
