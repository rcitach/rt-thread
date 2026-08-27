/*
 * Copyright (c) 2001-2019, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR Arm’s non-OSI source license
 */

/*!
@file
@brief This file contains definitions that are used for the Boot Services HAL layer.
*/
#ifndef _SECURE_BOOT_STAGE_DEFS_H
#ifndef _CC_HAL_SB_PLAT_H
#define _CC_HAL_SB_PLAT_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "r_cc312_common.h"

#define SB_HAL_READ_REGISTER(addr,val) 	\
		((val) = (*((volatile uint32_t*)(RRQ61X_ACRYPT_BASE | (addr)))))

#define SB_HAL_WRITE_REGISTER(addr,val)	\
		((*((volatile uint32_t*)(RRQ61X_ACRYPT_BASE | (addr)))) = (unsigned long)(val))

#ifdef __cplusplus
}
#endif

#endif
#endif	/*_SECURE_BOOT_STAGE_DEFS_H*/
