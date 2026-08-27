/*
 * Copyright (c) 2001-2019, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __DX_REG_BASE_HOST_H__
#define __DX_REG_BASE_HOST_H__

#include <stdbool.h>
#include <stdint.h>
#include <sdk_defs.h>

#include "r_cc312_common.h"

/* Identify platform: ARM MPS2 PLUS */
#define DX_PLAT_MPS2_PLUS 1

#define DX_BASE_CC 		(RRQ61X_ACRYPT_BASE|0x00000) // Org. 0x50010000
#define DX_BASE_CODE 		(RRQ61X_ACRYPT_BASE|0x04000) // Org. 0x50030000

#undef DX_BASE_ENV_REGS 	//(RRQ61X_ACRYPT_BASE|0x08000) // Org. 0x50028000
#undef DX_BASE_ENV_NVM_LOW 	//(RRQ61X_ACRYPT_BASE|0x0A000) // Org. 0x5002A000
#undef DX_BASE_ENV_NVM_HI  	//(RRQ61X_ACRYPT_BASE|0x0B000) // Org. 0x5002B000
#undef DX_BASE_ENV_PERF_RAM 	//(RRQ61X_ACRYPT_BASE|0x02000) // Org. 0x40009000

#define DX_BASE_HOST_RGF 	0x0UL
#define DX_BASE_CRY_KERNEL     	0x0UL

#define DX_BASE_RNG 		0x0000UL
#endif /*__DX_REG_BASE_HOST_H__*/
