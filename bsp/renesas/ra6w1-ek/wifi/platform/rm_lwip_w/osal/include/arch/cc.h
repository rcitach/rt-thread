/*
 * Copyright (c) 2001-2003 Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 */
#ifndef __ARCH_CC_H__
#define __ARCH_CC_H__

/* Include some files for defining library routines */
#include <stdio.h>		/* printf, fflush, FILE */
#include <string.h>
#include <stdlib.h> /* abort */
/*#include <errno.h> */
#if ( !defined( __CC_ARM ) ) && ( !defined( __ICCARM__ ) ) && ( !defined( __ARMCC_VERSION ) )
	#include <sys/time.h>
#endif

#include "FreeRTOS.h"
#include "common_def.h"

#ifndef BYTE_ORDER
	#define BYTE_ORDER	LITTLE_ENDIAN
#endif

#define LWIP_DEBUG_USE_PRINTF

#define LWIP_PLATFORM_BYTESWAP	0

/** @todo fix some warnings: don't use #pragma if compiling with cygwin gcc */
/*#ifndef __GNUC__ */
#if ( !defined( __ICCARM__ ) ) && ( !defined( __GNUC__ ) ) && ( !defined( __CC_ARM ) )
	#include <limits.h>
	#pragma warning (disable: 4244) /* disable conversion warning (implicit integer promotion!) */
	#pragma warning (disable: 4127) /* conditional expression is constant */
	#pragma warning (disable: 4996) /* 'strncpy' was declared deprecated */
	#pragma warning (disable: 4103) /* structure packing changed by including file */
#endif

/* Alternate error codes */
#define LWIP_ERRNO_INCLUDE "arch/errno.h"

#if !defined( LWIP_PROVIDE_ERRNO ) && !defined( LWIP_ERRNO_INCLUDE ) && !defined( LWIP_ERRNO_STDINCLUDE )
	#define LWIP_PROVIDE_ERRNO
#endif

/* Define generic types used in lwIP */
#define LWIP_NO_STDINT_H	1
typedef unsigned char	u8_t;
typedef signed char		s8_t;
typedef unsigned short	u16_t;
typedef signed short	s16_t;
typedef unsigned long	u32_t;
typedef signed long		s32_t;

typedef size_t			mem_ptr_t;
typedef u32_t			sys_prot_t;

/* Define (sn)printf formatters for these lwIP types */
#define X8_F		"02x"
#define U16_F		"hu"
#define S16_F		"hd"
#define X16_F		"hx"
#define U32_F		"lu"
#define S32_F		"ld"
#define X32_F		"lx"
#define SZT_F		"u"

//#define	rand() xTaskGetTickCount()		/* F_F_S (for further study) */

/* Compiler hints for packing structures */
#if defined( __ICCARM__ )
	#define PACK_STRUCT_BEGIN	__packed
	#define PACK_STRUCT_STRUCT	__packed
#else
	#define PACK_STRUCT_STRUCT	__attribute__( ( packed ) )
#endif

#ifdef LWIP_DEBUG_USE_PRINTF
/* Plaform specific diagnostic output */
	#define LWIP_PLATFORM_DIAG(x)		do { printf x; } while( 0 )
#else
	#define LWIP_PLATFORM_DIAG(x)		do { configPRINTF x; } while( 0 )
#endif

#define Printf(...) do { LWIP_PLATFORM_DIAG((__VA_ARGS__)); } while(0)

#define LWIP_PLATFORM_ASSERT( x )																\
	do { configPRINTF( RED_COLOR "Assertion \"%s\" failed at line %d in %s\n" CLEAR_COLOR,	\
								x, __LINE__, __func__ ); fflush( NULL ); } while( 0 )

#define LWIP_ERROR( message, expression, handler )												\
	do {																						\
		if( !( expression ) ) {																	\
		        configPRINTF( RED_COLOR "Assertion \"%s\" failed at line %d in %s\n" CLEAR_COLOR,	\
								message, __LINE__, __func__); handler;							\
		}																						\
	} while( 0 )

/* C runtime functions redefined */
/*#define snprintf _snprintf //2015-07-22 Cheng Liu @132663 */

u32_t dns_lookup_external_hosts_file( const char * name );

#ifndef LWIP_RAND
unsigned int rm_lwip_w_os_get_random();
#define LWIP_RAND()    ((u32_t) rm_lwip_w_os_get_random())
#endif

#if !(dg_configCODE_LOCATION == NON_VOLATILE_IS_FLASH)
#define LWIP_DECLARE_MEMORY_ALIGNED(variable_name, size) u8_t variable_name[LWIP_MEM_ALIGN_BUFFER(1)]
#endif

/* Custom error codes */
#define ER_SUCCESS						0x00
#define ER_NO_PACKET                    0x01
#define ER_DELETED                      0x02
#define ER_WAIT_ERROR                   0x03
#define ER_SIZE_ERROR                   0x04
#define ER_DELETE_ERROR                 0x05
#define ER_INVALID_PACKET               0x06
#define ER_NOT_ENABLED                  0x07
#define ER_ALREADY_ENABLED              0x08
#define ER_NO_MORE_ENTRIES              0x09
#define ER_IP_ADDRESS_ERROR             0x0A
#define ER_ALREADY_BOUND                0x0B
#define ER_NOT_CREATED                  0x0C
#define ER_DUPLICATE_LISTEN             0x0D
#define ER_IN_PROGRESS                  0x0E
#define ER_NOT_CONNECTED                0x0F
#define ER_NOT_SUCCESSFUL               0x11
#define ER_CONNECTION_PENDING           0x12
#define ER_NOT_IMPLEMENTED              0x13
#define ER_NOT_SUPPORTED                0x14
#define ER_INVALID_PARAMETERS           0x15
#define ER_NOT_FOUND                    0x16
#define ER_DUPLICATED_ENTRY             0x17
#define ER_PARAMETER_ERROR              0x18
#define ER_NO_MEMORY                    0x19
#define ER_NO_EVENTS                    0x1A
#define ER_QUEUE_ERROR                  0x1B
#define ER_QUEUE_EMPTY                  0x1C
#define ER_DISCONNECTED                 0x1D
#define ER_INIT_SECURE                  0x1E
#define ER_SSL                          0x1F
#define ER_BIND                         0x20
#define ER_SOCKET                       0x21
#define ER_LISTEN                       0x22
#define ER_MUTEX_CREATE                 0x23
#define ER_ARGUMENT                     0x24

/* Extension to lwip err_enum_t */
#define ERR_UNKNOWN   -17 /* Unknown    */
#define ERR_NOT_FOUND -18 /* Not 200 OK */

/* For ignoring the compile option "-Wconversion" */
#include "common_compile_opt.h"

#if CFG_PMGR
#define IP_PCB_MAX_NAME (20)
#ifdef __SUPPORT_DPM_TCP_KEEPALIVE__
void lwip_send_tcp_keepalive(void *m);
#endif /* __SUPPORT_DPM_TCP_KEEPALIVE__ */
#define LWIP_SOCK_MAX_NAME 20
struct lwip_sock_name {
  char name[LWIP_SOCK_MAX_NAME];
};
#endif /* CFG_PMGR */

bool rm_lwip_w_setp_tcp_server_action(void *arg);
void rm_lwip_w_tcpip_init_action(void);

#define RM_LWIP_W_TCPIP_THREAD_PRIO (21)

void ra6w1_mem_init(void);
char* nd6_get_default_gateway(void);

#endif /* __ARCH_CC_H__ */
