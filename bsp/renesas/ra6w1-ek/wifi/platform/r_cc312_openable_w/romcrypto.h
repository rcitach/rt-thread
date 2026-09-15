/* ${REA_DISCLAIMER_PLACEHOLDER} */

/*******************************************************************************************************************//**
 * @addtogroup R_CC312_OPENABLE_W
 * @{
 **********************************************************************************************************************/

#ifndef __ROMCRYPTO_RA6W1_H__
#define __ROMCRYPTO_RA6W1_H__

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

#include "cc_pal_types.h"
#include "cc_pal_mutex.h"
#include "cc_pal_mem.h"
#include "cc_pal_memmap.h"
#include "cc_pal_perf.h"

typedef         struct  {
        uint32_t (*Debug_SecureBoot_Mode)(void);

        uint32_t (*embcrypto_remapper)(uint32_t vaddr);
        uint32_t (*embcrypto_otpread)(uint32_t otpwoffset);
        uint32_t (*embcrypto_otpwrite)(uint32_t otpwoffset, uint32_t otpData);
        void    (*embcrypto_vprint)(uint16_t tag, const char *format, va_list arg);

        void    (*CC_PalAbort)(const char *exp);

        int32_t (*CC_PalMemCmpPlat)(const void* aTarget, const void* aSource, size_t aSize);
        void* (*CC_PalMemCopyPlat)(void* aDestination, const void* aSource, size_t aSize);
        void  (*CC_PalMemMovePlat)(void* aDestination, const void* aSource, size_t aSize);
        void  (*CC_PalMemSetPlat)(void* aTarget, uint8_t aChar, size_t aSize);
        void  (*CC_PalMemSetZeroPlat)(void* aTarget, size_t aSize);
        void* (*CC_PalMemMallocPlat)(size_t  aSize);
        void* (*CC_PalMemReallocPlat)(void* aBuffer, size_t  aNewSize);
        void  (*CC_PalMemFreePlat)(void* aBuffer);

        CCError_t (*CC_PalMutexLock)(void *pMutexId, uint32_t timeOut);
        CCError_t (*CC_PalMutexUnlock)(void *pMutexId);

        uint32_t (*CC_PalMemMap)(CCDmaAddr_t physicalAddress, uint32_t mapSize, uint32_t **ppVirtBuffAddr);
        uint32_t (*CC_PalMemUnMap)(uint32_t *pVirtBuffAddr, uint32_t mapSize);

        CCError_t (*CC_PalWaitInterrupt)( uint32_t data);

        CCError_t (*CC_PalPowerSaveModeSelect)(CCBool isPowerSaveMode);

        void *CCSymCryptoMutex;
        void *CCAsymCryptoMutex;
} ROMCRYPTO_PLATFORM_TYPE;

extern const char *get_romcrypto_info(void);
extern const char *get_romcrypto_date(void);
extern const char *get_romcrypto_time(void);

extern void init_romcrypto_platform(const ROMCRYPTO_PLATFORM_TYPE *primitive);

extern uint32_t R_CC312_Debug_SecureBoot_Mode(void);

extern uint32_t embcrypto_remapper(uint32_t vaddr);
extern uint32_t embcrypto_otpread(uint32_t otpwoffset);
extern uint32_t embcrypto_otpwrite(uint32_t otpwoffset, uint32_t otpData);

extern void    embcrypto_vprint(uint16_t tag, const char *format, va_list arg);
extern void    embcrypto_print(uint16_t tag, const char *fmt,...);

extern void    CC_PalAbort(const char *exp);

extern int32_t CC_PalMemCmpPlat(const void* aTarget, const void* aSource, size_t aSize);
extern void*  CC_PalMemCopyPlat(void* aDestination, const void* aSource, size_t aSize);
extern void   CC_PalMemMovePlat(void* aDestination, const void* aSource, size_t aSize);
extern void   CC_PalMemSetPlat(void* aTarget, uint8_t aChar, size_t aSize);
extern void   CC_PalMemSetZeroPlat(void* aTarget, size_t aSize);
extern void*  CC_PalMemMallocPlat(size_t  aSize);
extern void*  CC_PalMemReallocPlat(void* aBuffer, size_t  aNewSize);
extern void   CC_PalMemFreePlat(void* aBuffer);

extern CCError_t CC_PalMutexLock(CC_PalMutex *pMutexId, uint32_t timeOut);
extern CCError_t CC_PalMutexUnlock(CC_PalMutex *pMutexId);

extern uint32_t CC_PalMemMap(CCDmaAddr_t physicalAddress, uint32_t mapSize, uint32_t **ppVirtBuffAddr);
extern uint32_t CC_PalMemUnMap(uint32_t *pVirtBuffAddr, uint32_t mapSize);

extern CCError_t CC_PalWaitInterrupt( uint32_t data);

extern CCError_t CC_PalPowerSaveModeSelect(CCBool isPowerSaveMode);

extern CC_PalMutex *getCCSymCryptoMutex(void);
extern CC_PalMutex *getCCAsymCryptoMutex(void);

//--------------------------------------------------------------------
// Retargeted primitives
//--------------------------------------------------------------------

#endif /* __ROMCRYPTO_RA6W1_H__ */
/*******************************************************************************************************************//**
 * @} (end addtogroup R_CC312_OPENABLE_W)
 **********************************************************************************************************************/