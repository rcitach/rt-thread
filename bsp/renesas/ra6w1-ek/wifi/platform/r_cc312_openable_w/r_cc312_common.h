/* ${REA_DISCLAIMER_PLACEHOLDER} */

/*******************************************************************************************************************//**
 * @addtogroup R_CC312_OPENABLE_W
 * @{
 **********************************************************************************************************************/

#ifndef __cal_h__
#define __cal_h__

//--------------------------------------------------------------------
//	Dependency
//--------------------------------------------------------------------

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sdk_defs.h>

#include "crypto_primitives.h"

//--------------------------------------------------------------------
// Peripheral options
//--------------------------------------------------------------------

//--------------------------------------------------------------------
// Peripheral Headers
//--------------------------------------------------------------------

#include "r_cc312_crypto.h"
#include "r_cc312_secureboot.h"

/******************************************************************************
 *
 *  crypto Macro
 *
 ******************************************************************************/

#define RRQ61X_ACRYPT_BASE       (CC312_BASE)

#ifndef rrq61func_ntohl
#define	rrq61func_ntohl(x)      ((((x) & (uint32_t)0x000000ffUL) << 24) | \
                                 (((x) & (uint32_t)0x0000ff00UL) <<  8) | \
                                 (((x) & (uint32_t)0x00ff0000UL) >>  8) | \
                                 (((x) & (uint32_t)0xff000000UL) >> 24))
#endif

/******************************************************************************
 *
 *  Retargeted primitives
 *
 ******************************************************************************/

#define	CRYPTO_DUMP(...)		embcrypto_dump(0, __VA_ARGS__ )
#define	CRYPTO_PRINTF(...)		embcrypto_print(0, __VA_ARGS__ )
#define	CRYPTO_VPRINTF(...)		embcrypto_vprint(0, __VA_ARGS__ )
#define CRYPTO_SPRINTF(...)		sprintf( __VA_ARGS__ )
#define	CRYPTO_GETC()			embcrypto_getchar(OAL_SUSPEND)
#define	CRYPTO_GETC_NOWAIT()		embcrypto_getchar(OAL_NO_SUSPEND)
#define CRYPTO_PUTC(ch)			embcrypto_print(0, "%c", ch )
#define CRYPTO_PUTS(s)			embcrypto_print(0, s )

#define CRYPTO_STRLEN(...)		strlen( __VA_ARGS__ )
#define	CRYPTO_STRCPY(...)		strcpy( __VA_ARGS__ )
#define CRYPTO_STRCMP(...)		strcmp( __VA_ARGS__ )
#define CRYPTO_STRCAT(...)		strcat( __VA_ARGS__ )
#define CRYPTO_STRNCMP(...)		strncmp( __VA_ARGS__ )
#define CRYPTO_STRNCPY(...)		strncpy( __VA_ARGS__ )
#define CRYPTO_STRCHR(...)		strchr( __VA_ARGS__ )

#define CRYPTO_MEMCPY(...)		memcpy( __VA_ARGS__ )
#define CRYPTO_MEMCMP(...)		memcmp( __VA_ARGS__ )
#define CRYPTO_MEMSET(...)		embcrypto_memset( __VA_ARGS__ )
#define CRYPTO_DELAY(...)		embcrypto_rtosdelay( __VA_ARGS__ )
#define CRYPTO_MEASURE(...)		embcrypto_tickmeasure( __VA_ARGS__ )

#define CRYPTO_DBG_NONE(...)		// CodeOpt.:: embcrypto_print(5, __VA_ARGS__ )
#define CRYPTO_DBG_BASE(...)		// CodeOpt.:: embcrypto_print(4, __VA_ARGS__ )
#define CRYPTO_DBG_INFO(...)		// CodeOpt.:: embcrypto_print(3, __VA_ARGS__ )
#define CRYPTO_DBG_WARN(...)		embcrypto_print(2, __VA_ARGS__ )
#define CRYPTO_DBG_ERROR(...)		embcrypto_print(1, __VA_ARGS__ )
#define CRYPTO_DBG_DUMP(tag, ...)	// CodeOpt.:: embcrypto_dump(tag, __VA_ARGS__ )
#define CRYPTO_DBG_TEXT(tag, ...)	// CodeOpt.:: embcrypto_text(tag, __VA_ARGS__ )

#define CRYPTO_RRQ61X_REMAPPER(...)    embcrypto_remapper( __VA_ARGS__ )

#endif /* __cal_h__ */
/*******************************************************************************************************************//**
 * @} (end addtogroup R_CC312_OPENABLE_W)
 **********************************************************************************************************************/
