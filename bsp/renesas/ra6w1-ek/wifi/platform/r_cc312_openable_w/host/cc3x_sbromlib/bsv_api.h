/*
 * Copyright (c) 2017-2020 ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _BSV_API_H
#define _BSV_API_H

#ifdef __cplusplus
extern "C"
{
#endif

/*!
@addtogroup bsv_api
@{
*/

/*! @file
@brief This file contains the Boot Services APIs and definitions.
*/

#include "cc_pal_types.h"
#include "cc_sec_defs.h"

/* Life cycle state definitions. */
#if !defined(CC_BSV_CHIP_MANUFACTURE_LCS)
/*! Defines the CM life-cycle state value. */
#define CC_BSV_CHIP_MANUFACTURE_LCS     0x0
#endif
#if !defined(CC_BSV_DEVICE_MANUFACTURE_LCS)
/*! Defines the DM life-cycle state value. */
#define CC_BSV_DEVICE_MANUFACTURE_LCS       0x1
#endif
#if !defined(CC_BSV_SECURE_LCS)
/*! Defines the Secure life-cycle state value. */
#define CC_BSV_SECURE_LCS           0x5
#endif
#if !defined(CC_BSV_RMA_LCS)
/*! Defines the RMA life-cycle state value. */
#define CC_BSV_RMA_LCS              0x7
#endif

/*----------------------------
      PUBLIC FUNCTIONS
-----------------------------------*/

/*!
@brief This function must be the first Arm CryptoCell 3xx SBROM library API
called.

It verifies the HW product and version numbers, and initializes the HW.

@return CC_OK on success.
@return A non-zero value from bsv_error.h on failure.
*/
CCError_t CC_BsvInit(
    /*! [in] The base address for CryptoCell HW registers. */
    unsigned long hwBaseAddress
    );

/*!
@brief This function retrieves the security life-cycle state from the NVM
manager.

@return CC_OK on success.
@return A non-zero value from bsv_error.h on failure.
*/
CCError_t CC_BsvLcsGet(
    /*! [in] The base address for CryptoCell HW registers. */
    unsigned long hwBaseAddress,
    /*! [out] The value of the current security life-cycle state. */
	uint32_t *pLcs
    );

/*!
@brief This function retrieves the HW security life-cycle state and performs
validity checks.

If LCS is RMA, the function performs additional initializations (sets the OTP
secret keys to a fixed value).
\note   If the LCS is invalid, the function returns an error, upon which your
code must completely disable the device.

@return CC_OK on success.
@return A non-zero value from bsv_error.h on failure.
*/
CCError_t CC_BsvLcsGetAndInit(
    /*! [in] The base address for CryptoCell HW registers. */
    unsigned long hwBaseAddress,
    /*! [out] The returned life-cycle state. */
	uint32_t *pLcs
    );

/*!
@brief This function is called in RMA LCS to erase one or more of the private
keys.

@return CC_OK on success.
@return A non-zero value from bsv_error.h on failure.
*/
CCError_t CC_BsvOTPPrivateKeysErase(
    /*! [in] The base address for CryptoCell HW registers. */
	unsigned long hwBaseAddress,
	/*! [in] The HUK secret key: CC_TRUE: HUK is erased. CC_FALSE: HUK
	remains unchanged. */
    CCBool_t isHukErase,
    /*! [in] Kpicv secret key: CC_TRUE: Kpicv is erased. CC_FALSE: Kpicv
	remains unchanged. */
	CCBool_t isKpicvErase,
    /*! [in] Kceicv secret key: CC_TRUE: Kceicv is erased. CC_FALSE:
	Kceicv remains unchanged. */
	CCBool_t isKceicvErase,
    /*! [in] Kcp secret key: CC_TRUE: Kcp is erased. CC_FALSE: Kcp
	remains unchanged.  */
	CCBool_t isKcpErase,
    /*! [in] Kce secret key: CC_TRUE: Kce is erased. CC_FALSE: Kce
	remains unchanged. */
	CCBool_t isKceErase,
    /*! [out] Returned status word. */
	uint32_t *pStatus
    );

/*!
@brief This function sets the "fatal error" flag in the NVM manager, to
disable the use of any HW Keys or security services.

@return CC_OK on success.
@return A non-zero value from bsv_error.h on failure.
*/
CCError_t CC_BsvFatalErrorSet(
    /*! [in] The base address for CryptoCell HW registers. */
	unsigned long hwBaseAddress
    );

/*!
@brief This function permanently sets the RMA life-cycle state per OEM or ICV.

@return CC_OK on success.
@return A non-zero value from bsv_error.h on failure.
*/
CCError_t CC_BsvRMAModeEnable(
    /*! [in] The base address for CryptoCell HW registers. */
	unsigned long hwBaseAddress
    );

/*!
@brief This function derives the unique <i>SoC_ID</i> of the device as hashed
(Hbk || AES_CMAC (HUK)).

\note   <i>SoC_ID</i> is required for the creation of debug certificates.
        Therefore, the OEM or ICV must provide a method for a developer
		to discover the <i>SoC_ID</i> of a target device without having to first
		enable debugging. One suggested implementation is to have the ROM code
		of the device compute the <i>SoC_ID</i> and place it in a specific
		location in the flash memory, where it can be accessed by the developer.

@return CC_OK on success.
@return A non-zero value from bsv_error.h on failure.
*/
CCError_t CC_BsvSocIDCompute(
                 /*! [in] The base address for CryptoCell HW registers. */
				 unsigned long hwBaseAddress,
                 /*! [out] The derived SOC ID. */
				 CCHashResult_t hashResult
    );


/*!
@brief This function must be called when the user needs to lock one of the ICV
keys from further usage.

@return CC_OK on success.
@return A non-zero value from bsv_error.h on failure.
*/
CCError_t CC_BsvICVKeyLock(
    /*! [in] The base address for CryptoCell HW registers. */
    unsigned long hwBaseAddress,
    /*! [in] The ICV provisioning key mode: CC_TRUE: Kpicv is locked for
	further usage. CC_FALSE: Kpicv is not locked.  */
	CCBool_t isICVProvisioningKeyLock,
    /*! [in] The ICV code encryption key mode: CC_TRUE: Kceicv is locked
	for further usage. CC_FALSE: Kceicv is not locked. */
	CCBool_t isICVCodeEncKeyLock
    );

/*!
@brief This function is called by the ICV code to disable the OEM code
from changing the ICV RMA bit flag.

@return CC_OK on success.
@return A non-zero value from bsv_error.h on failure.
*/
CCError_t CC_BsvICVRMAFlagBitLock(
    /*! [in] The base address for CryptoCell HW registers. */
	unsigned long hwBaseAddress
    );

/*!
@brief This API enables the core_clk gating mechanism, which is disabled
during power-up.

@return CC_OK on success.
@return A non-zero value from bsv_error.h on failure.
*/
CCError_t CC_BsvCoreClkGatingEnable(
    /*! [in] The base address for CryptoCell HW registers. */
	unsigned long hwBaseAddress
    );

/*!
@brief This function controls the APB secure filter, allowing only secure
transactions to access CryptoCell-312 registers.

@return CC_OK on success.
@return A non-zero value from bsv_error.h on failure.
*/
CCError_t CC_BsvSecModeSet(
    /*! [in] The base address for CryptoCell HW registers. */
	unsigned long hwBaseAddress,
    /*! [in] The APB secure filter mode: CC_TRUE: only secure accesses are
	served. CC_FALSE: both secure and non-secure accesses are served. */
	CCBool_t isSecAccessMode,
    /*! [in] The APB security lock mode: CC_TRUE: secure filter mode is locked
	for further changes. CC_FALSE: secure filter mode is not locked. */
	CCBool_t isSecModeLock
    );

/*!
@brief This function activates the APB privilege filter, allowing only secure
transactions to access CryptoCell-312 registers.

@return CC_OK on success.
@return A non-zero value from bsv_error.h on failure.
*/
CCError_t CC_BsvPrivModeSet(
    /*! [in] The base address for CryptoCell HW registers. */
	unsigned long hwBaseAddress,
    /*! [in] The APB privileged mode: CC_TRUE: only privileged accesses are
	served. CC_FALSE: both privileged and non-privileged accesses are served. */
	CCBool_t isPrivAccessMode,
    /*! [in] The privileged lock mode: CC_TRUE: privileged mode is locked for
	further changes. CC_FALSE: privileged mode is not locked. */
	CCBool_t isPrivModeLock
    );

/*!
@brief This function unpacks the ICV asset packet and returns the asset data.

@return CC_OK on success.
@return A non-zero value from bsv_error.h on failure.
 */
CCError_t CC_BsvIcvAssetProvisioningOpen(
    /*! [in] The base address for CryptoCell HW registers. */
	unsigned long    hwBaseAddress,
    /*! [in] The asset identifier. */
	uint32_t         assetId,
    /*! [in] The asset package word-array formatted to unpack. */
	uint32_t         *pAssetPkgBuff,
    /*! [in] The exact length of the asset package in bytes. Must be multiple
	of 16 bytes. */
	size_t           assetPackageLen,
    /*! [out] The decrypted contents of the asset data. */
	uint8_t          *pOutAssetData,
    /*! [in/out] As input: the size of the allocated asset data buffer.
	The maximal size is 512 bytes. As output: the actual size of the decrypted
	asset data buffer. The maximal size is 512 bytes. */
	size_t           *pAssetDataLen
    );


/*!
@brief This function unpacks the OEM asset packet and returns the asset data.

@return CC_OK on success.
@return A non-zero value from bsv_error.h on failure.
 */
CCError_t CC_BsvOemAssetProvisioningOpen(
    /*! [in] The base address for CryptoCell HW registers. */
	unsigned long    hwBaseAddress,
    /*! [in] The asset identifier. */
	uint32_t         assetId,
    /*! [in] The asset package word-array formatted to unpack. */
	uint32_t         *pAssetPkgBuff,
    /*! [in] The exact length of the asset package in bytes. Must be multiple
	of 16 bytes. */
	size_t           assetPackageLen,
    /*! [out] The decrypted contents of the asset data. */
	uint8_t          *pOutAssetData,
    /*! [in/out] As input: the size of the allocated asset data buffer.
	The maximal size is 512 bytes. As output: the actual size of the decrypted
	asset data buffer. The maximal size is 512 bytes. */
	size_t           *pAssetDataLen
    );


#ifdef __cplusplus
}
#endif

/**
@}
 */

#endif



