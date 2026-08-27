/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include "common.h"

#include "vendor.h"
#include "mbedtls/error.h"
#include "mbedtls/psa_util.h"

uint32_t ecp_load_key_size(bool wrapped_mode_ctx, const mbedtls_ecp_group * grp);

psa_status_t vendor_bitlength_to_raw_bitlength (psa_key_type_t type, size_t vendor_bits, size_t * raw_bits)
{
    (void) vendor_bits;
    (void) raw_bits;
    (void) type;

    return PSA_ERROR_NOT_SUPPORTED;
}

/*************crypto_accel_driver.h implementations follow***************************/

/*
 * This function is based off of psa_generate_key_internal() in mbedCrypto.
 */
psa_status_t psa_generate_key_vendor (psa_key_slot_t                        * slot,
                                      size_t                                  bits,
                                      const psa_key_production_parameters_t * params,
                                      size_t                                  params_data_length)
{
    (void) slot;
    (void) bits;
    (void) params;
    (void) params_data_length;

    if ((params == NULL) && (params_data_length != 0))
    {
        return PSA_ERROR_INVALID_ARGUMENT;
    }

    return PSA_ERROR_NOT_SUPPORTED;    // NOLINT(readability-misleading-indentation)
}

/** Import key data into a slot. `slot->attr.type` must have been set
 * previously. This function assumes that the slot does not contain
 * any key material yet. On failure, the slot content is unchanged. */
psa_status_t psa_import_key_into_slot_vendor (const psa_key_attributes_t * attributes,
                                              psa_key_slot_t             * slot,
                                              const uint8_t              * data,
                                              size_t                       data_length,
                                              mbedtls_svc_key_id_t       * key,
                                              bool                         write_to_persistent_memory)
{
    (void) slot;
    (void) data;
    (void) data_length;
    (void) key;
    FSP_PARAMETER_NOT_USED(attributes);

    return PSA_ERROR_NOT_SUPPORTED;
}

/*
 * This function is based off of psa_finish_key_creation() in mbedCrypto.
 */

psa_status_t psa_finish_key_creation_vendor (psa_key_slot_t * slot)
{
    (void) slot;
    psa_status_t status = PSA_SUCCESS;
#if defined(MBEDTLS_PSA_CRYPTO_STORAGE_C)
    size_t buffer_size = 0;
 #if defined(MBEDTLS_AES_C) && ((PSA_CRYPTO_IS_WRAPPED_SUPPORT_REQUIRED(PSA_CRYPTO_CFG_AES_FORMAT)))

    /* Check if the key is of AES type */
    if (PSA_KEY_TYPE_IS_AES(slot->attr.type))
    {
        buffer_size = slot->key.bytes;
    }
    else
 #endif                                // defined(MBEDTLS_AES_C) && ((PSA_CRYPTO_IS_WRAPPED_SUPPORT_REQUIRED(PSA_CRYPTO_CFG_AES_FORMAT)))
    {
        buffer_size = slot->key.bytes;
    }

    if (buffer_size == 0)
    {
        return PSA_ERROR_NOT_SUPPORTED;
    }

    uint8_t * buffer = mbedtls_calloc(1, buffer_size);
    size_t    length = 0;
    if (buffer == NULL)
    {
        return PSA_ERROR_INSUFFICIENT_MEMORY;
    }

    psa_key_type_t type = slot->attr.type;

    if (PSA_KEY_TYPE_IS_UNSTRUCTURED(type) ||
        PSA_KEY_TYPE_IS_RSA(type) ||
        PSA_KEY_TYPE_IS_ECC(type))
    {
        status = psa_export_key_buffer_internal(slot->key.data, slot->key.bytes, buffer, buffer_size, &length);

        if (status == PSA_SUCCESS)
        {
            status = psa_save_persistent_key(&slot->attr, buffer, length);
        }
    }

    mbedtls_platform_zeroize(buffer, buffer_size);
    mbedtls_free(buffer);
#endif                                 // defined (MBEDTLS_PSA_CRYPTO_STORAGE_C)
    return status;
}
