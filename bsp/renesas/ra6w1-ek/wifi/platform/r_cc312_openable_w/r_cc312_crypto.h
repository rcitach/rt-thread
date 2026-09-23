/* ${REA_DISCLAIMER_PLACEHOLDER} */

/*******************************************************************************************************************//**
 * @addtogroup R_CC312_OPENABLE_W
 * @{
 **********************************************************************************************************************/

#ifndef __r_cc312_crypto_h__
#define __r_cc312_crypto_h__

//--------------------------------------------------------------------
//	Target System
//--------------------------------------------------------------------

#include "r_cc312_common.h"

/*******************************************************************************************************************//**
 * @brief  Initialize Crypto Clock.
 *
 * @param[in]       mode			pll callbak mode \n
 *                                  TRUE is support the pll callback of Crypto Engine.
 *  
 ***********************************************************************************************************************/
extern void	R_CC312_Crypto_ClkMgmt(uint32_t mode);

/*******************************************************************************************************************//**
 * @brief  Initialize Crypto Engine.
 *
 * @param[in]       rflag		parameter for TRNG initialization 
 *  
 * @retval true		    function succeeded
 * @retval false 		function failed 
 ***********************************************************************************************************************/
extern uint32_t	R_CC312_Crypto_Init(uint32_t rflag);

/*******************************************************************************************************************//**
 * @brief  Re-initialize Crypto Engine after wakeup.
 *
 ***********************************************************************************************************************/
extern void	R_CC312_Crypto_ReInit(void);

/*******************************************************************************************************************//**
 * @brief  Deinitialize Crypto Engine.
 *
 ***********************************************************************************************************************/
extern void	R_CC312_Crypto_Finish(void);


/*******************************************************************************************************************//**
 * @brief  Enable the clock of HW Crypto.
 *
 * @param[in]       mode			if true, then the clock is enabled, othersize it is diabled.
 *  
 ***********************************************************************************************************************/
extern void	R_CC312_SetCryptoCoreClkGating(uint32_t mode);


/*******************************************************************************************************************//**
 * @brief Reads requested length of random data from the TRNG. Generate `buffSize` of random bytes
 * and store them in `pRndWorkBuff` buffer.
 *
 * @param[in]       sampleCount     set sampling ratio (rng_clocks) between consecutive bits.
 * @param[in]       buffSize		the length of random bytes.
 * @param[inout]    pRndWorkBuff	empty output buffer for storing the random bytes.
 *
 * @retval Zero 		function succeeded 
 * @retval Non-Zero		function failed
 *
 **********************************************************************************************************************/
extern int  R_CC312_Crypto_TRNG(uint32_t sampleCount, uint32_t buffSize, uint8_t *pRndWorkBuff);

#endif /* __r_cc312_crypto_h__ */
/*******************************************************************************************************************//**
 * @} (end addtogroup R_CC312_OPENABLE_W)
 **********************************************************************************************************************/