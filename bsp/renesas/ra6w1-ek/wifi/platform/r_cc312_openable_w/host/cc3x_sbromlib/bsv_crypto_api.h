/*
 * Copyright (c) 2001-2019, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


#ifndef _BSV_CRYPTO_API_H
#define _BSV_CRYPTO_API_H

#ifdef __cplusplus
extern "C"
{
#endif

/*! @file
@brief This file contains cryptographic ROM APIs : SHA256, CMAC KDF, and CCM.
*/

#include "cc_pal_types.h"
#include "bsv_crypto_defs.h"
#include "cc_sec_defs.h"

/*----------------------------
      PUBLIC FUNCTIONS
-----------------------------------*/

/*!
@brief This function calculates SHA256 digest over contiguous memory in an integrated operation.

@return CC_OK on success.
@return A non-zero value from bsv_error.h on failure.
*/
CCError_t CC_BsvSHA256(
    /*! [in] The base address for CryptoCell HW registers. */
    unsigned long   hwBaseAddress,
    /*! [in] A pointer to the input data to be hashed. The buffer must be
    contiguous. */
    uint8_t     *pDataIn,
    /*! [in] The size of the data to be hashed in bytes. Limited to 64KB. */
    size_t      dataSize,
    /*! [out] A pointer to a word-aligned 32-byte buffer. */
    CCHashResult_t  hashBuff
    );

/*!
@brief  The key derivation function is as specified in the "KDF in Counter Mode" section of
    NIST Special Publication 800-108: Recommendation for Key Derivation Using Pseudorandom Functions.
    Key derivation is based on length l, label L, context C and derivation key Ki.
        AES-CMAC is used as the pseudorandom function (PRF).
\note   When using this API the label and context for each use-case must be well defined.
\note   We recommend to derive only 256-bit keys from HUK or 256-bit user keys.

@return CC_OK on success.
@return A non-zero value from bsv_error.h on failure.
*/

/*  A key derivation functions can iterates n times until l bits of keying material are generated.
        For each of the iteration of the PRF, i=1 to n, do:
        result(0) = 0;
        K(i) = PRF (Ki, [i] || Label || 0x00 || Context || length);
        results(i) = result(i-1) || K(i);

        concisely, result(i) = K(i) || k(i-1) || .... || k(0)*/
CCError_t CC_BsvKeyDerivation(
    /*! [in] The base address for CryptoCell HW registers. */
    unsigned long       hwBaseAddress,
    /*! [in] Defines the type of the key provided in \p *pUserKey: HUK, Krtl,
    KCP, KPICV, 128-bit User key, and 256-bit User Key. */
    CCBsvKeyType_t      keyType,
    /*! [in] A pointer to the buffer holding the user key. */
    uint32_t            *pUserKey,
    /*! [in] The size of the user key in bytes (limited to 16 bytes or
    32 bytes). */
    size_t              userKeySize,
    /*! [in] A string that identifies the purpose for the derived keying
    material.*/
    const uint8_t       *pLabel,
    /*! [in] The label size. Must be between 1 to 8 bytes in length. */
    size_t              labelSize,
    /*! [in] A binary string containing the information related to the derived
    keying material. */
    const uint8_t       *pContextData,
    /*! [in] The context size should be between 1 to 32 bytes in length. */
    size_t              contextSize,
    /*! [out] The keying material output. Must be at least the size defined
    in \p derivedKeySize. */
    uint8_t         *pDerivedKey,
    /*! [in] The size of the derived keying material in bytes. Limited to
    128 bits or 256 bits. */
    size_t          derivedKeySize
    );


/*!
@brief This API allows a limited AES-CCM decrypt and verify operation, needed for AES-CCM verification during boot.
AES-CCM combines counter mode encryption with CBC-MAC authentication.
Input to CCM includes the following elements:
<ul><li> Payload - text data that is both decrypted and verified.</li>
<li> Associated data (Adata) - data that is authenticated but not encrypted, e.g., a header.</li>
<li> Nonce - A unique value that is assigned to the payload and the associated data.</li></ul>

@return CC_OK on success.
@return A non-zero value on failure as defined bsv_error.h.
*/
CCError_t CC_BsvAesCcm(
    /*! [in] The base address for CryptoCell HW registers. */
    unsigned long       hwBaseAddress,
    /*! [in] A pointer to the 128-bit AES-CCM key. */
    CCBsvCcmKey_t       keyBuf,
    /*! [in] Pointer to the 12-byte Nonce. */
    CCBsvCcmNonce_t     nonceBuf,
    /*! [in] A pointer to the associated data. The buffer must be contiguous. */
    uint8_t                 *pAssocData,
    /*! [in] The byte size of the associated data. Limited to (2^16-2^8) bytes. */
    size_t                  assocDataSize,
    /*! [in] A pointer to the cipher-text data for decryption. The buffer must be contiguous. */
    uint8_t                 *pTextDataIn,
    /*! [in] The byte size of the full text data. Limited to 64KB. */
    size_t                  textDataSize,
    /*! [out] A pointer to the output (plain text data). The buffer must be contiguous. */
    uint8_t                 *pTextDataOut,
    /*! [in] A pointer to the MAC result buffer. */
    CCBsvCcmMacRes_t        macBuf
);


#ifdef __cplusplus
}
#endif

#endif



