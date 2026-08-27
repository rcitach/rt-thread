/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include "common.h"

#if defined(MBEDTLS_PSA_CRYPTO_ACCEL_DRV_C)

 #include "aes_vendor.h"

/* Auto-generated values depending on which drivers are registered.
 * ID 0 is reserved for unallocated operations.
 * ID 1 is reserved for the Mbed TLS software driver. */
 #define PSA_CRYPTO_MBED_TLS_DRIVER_ID    (1)

/** Determine standard key size in bits for a vendor type key bit size associated with an elliptic curve.
 *  THis function is invoked during key generation and the user specifies the bits which will be the
 * standard bit size (SIZE_AES_128BIT_KEYLEN_BITS), but the wrapped key has a different size (SIZE_AES_128BIT_KEYLEN_BYTES_WRAPPED)
 * so that is returned.
 * This function is also invoked during key import. Since the wrapped key to be imported has a non-standard size,
 * when this key is imported, the bit length with be non standard. This function will account for that case as well.
 *
 * \param[in] type     Key type
 * \param[in] bits     Vendor key size in bits
 * \param[out] raw     Equivalent standard key size in bits
 */

/*************crypto_accel_driver.h implementations follow***************************/

psa_status_t psa_generate_symmetric_vendor (psa_key_type_t type, size_t bits, uint8_t * output, size_t output_size)
{
    (void) type;
    (void) bits;
    (void) output;
    (void) output_size;

    return PSA_ERROR_NOT_SUPPORTED;
}

void psa_aead_setup_vendor (void * ctx)
{
    (void) ctx;
}

#endif                                 /* MBEDTLS_PSA_CRYPTO_ACCEL_DRV_C */
