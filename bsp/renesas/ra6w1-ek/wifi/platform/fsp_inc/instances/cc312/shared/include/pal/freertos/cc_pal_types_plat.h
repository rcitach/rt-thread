/*
 * Copyright (c) 2001-2019, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR Arm?�s non-OSI source license
 */

#ifndef _CC_PAL_TYPES_PLAT_H
#define _CC_PAL_TYPES_PLAT_H

/*! @file
@brief This file contains basic platform-dependent type definitions.
*/
/* Host specific types for standard (ISO-C99) compilant platforms */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "rm_psa_crypto_w_hwcfg.h"

typedef uintptr_t	    CCVirtAddr_t;
typedef uint32_t            CCBool_t;
typedef uint32_t            CCStatus;

#define CCError_t           CCStatus
#define CC_INFINITE         0xFFFFFFFF

#define CEXPORT_C
#define CIMPORT_C

#endif /*_CC_PAL_TYPES_PLAT_H*/
