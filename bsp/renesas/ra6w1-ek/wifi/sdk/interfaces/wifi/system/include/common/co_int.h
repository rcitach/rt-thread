/**
 ****************************************************************************************
 *
 * @file co_int.h
 *
 * @brief This file replaces the need to include stdint or stdbool typical headers,
 *        which may not be available in all toolchains, and adds new types
 *
 * Copyright (C) RivieraWaves 2011-2021
 *
 * $Rev: $
 *
 ****************************************************************************************
 */

#ifndef _CO_INT_H_
#define _CO_INT_H_


/**
 ****************************************************************************************
 * @addtogroup CO_INT
 * @ingroup COMMON
 * @brief Common integer standard types (removes use of stdint)
 *
 * @{
 ****************************************************************************************
 */


/*
 * DEFINES
 ****************************************************************************************
 */


//STDINT----------------------------------------------------------------------------------

#if defined(CFG_RWTL) || defined(CFG_RWX1)
/// Exact-width integer types
#ifndef __int8_t_defined
#define __int8_t_defined
typedef signed char int8_t;
typedef short int16_t;
typedef int int32_t;
typedef long long int64_t;
#endif

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

#ifndef __uint32_t_defined
#define __uint32_t_defined
typedef unsigned int uint32_t;
#endif
typedef unsigned long long uint64_t;
#else
#include <stdint.h>

typedef __signed__ char __s8;
typedef unsigned char __u8;

typedef __signed__ short __s16;
typedef unsigned short __u16;

typedef __signed__ int __s32;
typedef unsigned int __u32;

__extension__ typedef __signed__ long long __s64;
__extension__ typedef unsigned long long __u64;

typedef __s8  s8;
typedef __u8  u8;
typedef __s16 s16;
typedef __u16 u16;
typedef __s32 s32;
typedef __u32 u32;
typedef __s64 s64;
typedef __u64 u64;

#ifdef __CHECK_ENDIAN__
#define __bitwise __bitwise__
#else
#define __bitwise
#endif

typedef __u16 __bitwise __le16;
typedef __u16 __bitwise __be16;
typedef __u32 __bitwise __le32;
typedef __u32 __bitwise __be32;
typedef __u64 __bitwise __le64;

#endif


/// @} CO_INT
#endif // _CO_INT_H_
