/**
 ****************************************************************************************
 *
 * @file r_cc312_crypto.c
 *
 * @brief CC312 Integration
 *
 * Copyright (c) 2023-2024 Renesas Electronics. All rights reserved.
 *
 * This software ("Software") is owned by Renesas Electronics.
 *
 * By using this Software you agree that Renesas Electronics retains all
 * intellectual property and proprietary rights in and to this Software and any
 * use, reproduction, disclosure or distribution of the Software without express
 * written permission or a license agreement from Renesas Electronics is
 * strictly prohibited. This Software is solely for use on or in conjunction
 * with Renesas Electronics products.
 *
 * EXCEPT AS OTHERWISE PROVIDED IN A LICENSE AGREEMENT BETWEEN THE PARTIES, THE
 * SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. EXCEPT AS OTHERWISE
 * PROVIDED IN A LICENSE AGREEMENT BETWEEN THE PARTIES, IN NO EVENT SHALL
 * RENESAS ELECTRONICS BE LIABLE FOR ANY DIRECT, SPECIAL, INDIRECT, INCIDENTAL,
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THE SOFTWARE.
 *
 ****************************************************************************************
 */



#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

/* remove !!!!!!!!!!!!! #include "osal.h" */
#include "r_cc312_common.h"

/* cerberus pal */
#include "cc_pal_types.h"
#include "cc_pal_types_plat.h"
#include "cc_pal_mem.h"
#include "cc_pal_perf.h"
#include "cc_regs.h"
#include "cc_otp_defs.h"
#include "cryptocell/cc_lib.h"
//REMOVE: #include "cc_rnd.h"
#include "cc_hal_plat.h"
#include "bsv_api.h"

#include "dx_nvm.h"
#include "dx_rng.h"
#include "dx_id_registers.h"

/* mbedtls */
#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#include "mbedtls/platform.h"
#include "mbedtls/memory_buffer_alloc.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/md.h"

#include "r_cc312_crypto.h"

/******************************************************************************
 *
 ******************************************************************************/


typedef		struct {
	CCRndContext_t *rndContext;
	CCRndWorkBuff_t *rndWorkBuff;
} RRQ61X_CRYPTO_TYPE;

static RRQ61X_CRYPTO_TYPE *r_cc312_crypto_driver;


#define RRQ61X_Crypto_ALLOC(_ptr, _type, _size) 				\
        { 								\
		_ptr = ( _type *)CRYPTO_MALLOC( _size * sizeof(_type));	\
		if( _ptr == NULL ) goto failed;				\
        }

#define RRQ61X_Crypto_FREE(_ptr) 						\
        if( _ptr != NULL ){ 						\
		CRYPTO_FREE( _ptr );					\
		_ptr = NULL;						\
        }

#define ASIC_DBG_TRIGGER(...)

/******************************************************************************
 *  R_CC312_Crypto_ClkMgmt( )
 *
 *  Purpose :
 *  Input   :
 *  Output  :
 *  Return  :
 ******************************************************************************/

void	R_CC312_Crypto_ClkMgmt(uint32_t mode)
{
	// TODO: how to ?
}

/******************************************************************************
 *  R_CC312_Crypto_Init( )
 *
 *  Purpose :
 *  Input   :
 *  Output  :
 *  Return  : 0 - error, 1 - init, 2 - reinit
 ******************************************************************************/

extern void	mbedtls_hardware_poll_fci_customize(uint32_t rflag);
extern CCError_t CC_BsvCoreClkGatingEnable(unsigned long hwBaseAddress);

void    R_CC312_Crypto_ReInit(void)
{
        if( r_cc312_crypto_driver != NULL ){
                CC_BsvCoreClkGatingEnable(RRQ61X_ACRYPT_BASE);
        }
}

uint32_t R_CC312_Crypto_Init(uint32_t rflag)
{
	uint32_t lcs;
	CCError_t ccRc;

	if( r_cc312_crypto_driver != NULL ){
                CC_BsvCoreClkGatingEnable(RRQ61X_ACRYPT_BASE);
		return (uint32_t)2; // re-init
	}else{

		/* initilaise driver */
		RRQ61X_Crypto_ALLOC( r_cc312_crypto_driver, RRQ61X_CRYPTO_TYPE, 1);

		/* use global ptr to be able to free them on exit */
		RRQ61X_Crypto_ALLOC( r_cc312_crypto_driver->rndContext, CCRndContext_t, 1);
		RRQ61X_Crypto_ALLOC( r_cc312_crypto_driver->rndWorkBuff, CCRndWorkBuff_t, 1);

		/* init Rnd context's inner member */
		if( sizeof(CCRndState_t) > sizeof(mbedtls_ctr_drbg_context) ){
			RRQ61X_Crypto_ALLOC( r_cc312_crypto_driver->rndContext->rndState, CCRndState_t, 1);
		}else{
			RRQ61X_Crypto_ALLOC( r_cc312_crypto_driver->rndContext->rndState, mbedtls_ctr_drbg_context, 1);
		}
		RRQ61X_Crypto_ALLOC( r_cc312_crypto_driver->rndContext->entropyCtx, mbedtls_entropy_context, 1);
	}

	// Unsecure Mode ???
	ASIC_DBG_TRIGGER(MODE_CRY_STEP(0xC321C1));
	CC_BsvCoreClkGatingEnable(RRQ61X_ACRYPT_BASE);

	ccRc = CC_BsvLcsGetAndInit(RRQ61X_ACRYPT_BASE, &lcs);

	if (ccRc != CC_OK)
	{
		ASIC_DBG_TRIGGER(MODE_CRY_STEP(0xC320FF));
		goto failed;
	}

	// RRQ61x hidden parameter
	r_cc312_crypto_driver->rndWorkBuff->ccRndIntWorkBuff[0] = (uint32_t)0xFC9CC312; // hidden tag
	r_cc312_crypto_driver->rndWorkBuff->ccRndIntWorkBuff[1] = (uint32_t)rflag;
	mbedtls_hardware_poll_fci_customize((uint32_t)rflag); // hidden tag

	/* initialise CC library */
	if(CC_LibInit(r_cc312_crypto_driver->rndContext
			, r_cc312_crypto_driver->rndWorkBuff) == CC_LIB_RET_OK)
	{
		return (uint32_t)1;
	}

failed:
	R_CC312_Crypto_Finish();

	return (uint32_t)0;

}

/******************************************************************************
 *  R_CC312_Crypto_Finish( )
 *
 *  Purpose :
 *  Input   :
 *  Output  :
 *  Return  :
 ******************************************************************************/

void R_CC312_Crypto_Finish(void)
{
	if( r_cc312_crypto_driver == NULL ){
		return;
	}

	CC_LibFini(r_cc312_crypto_driver->rndContext);

	RRQ61X_Crypto_FREE(r_cc312_crypto_driver->rndContext->rndState);
	RRQ61X_Crypto_FREE(r_cc312_crypto_driver->rndContext->entropyCtx);
	RRQ61X_Crypto_FREE(r_cc312_crypto_driver->rndContext);
	RRQ61X_Crypto_FREE(r_cc312_crypto_driver->rndWorkBuff);

	RRQ61X_Crypto_FREE(r_cc312_crypto_driver);

	r_cc312_crypto_driver = NULL;
}


/******************************************************************************
 *  R_CC312_SetCryptoCoreClkGating( )
 *
 *  Purpose :
 *  Input   :
 *  Output  :
 *  Return  :
 ******************************************************************************/

void	R_CC312_SetCryptoCoreClkGating(uint32_t mode)
{
	if( mode == (uint32_t)true ){
		CC_HAL_WRITE_REGISTER(CC_REG_OFFSET(CRY_KERNEL, HOST_CORE_CLK_GATING_ENABLE), 0x01);
	}else{
		CC_HAL_WRITE_REGISTER(CC_REG_OFFSET(CRY_KERNEL, HOST_CORE_CLK_GATING_ENABLE), 0x00);
	}
}


/******************************************************************************
 *  R_CC312_Crypto_TRNG( )
 *
 *  Purpose :
 *  Input   :
 *  Output  :
 *  Return  :
 ******************************************************************************/
/* TRNG mode definition */
#define TRNG_MODE_FAST                         0
#define TRNG_MODE_FE                           1
#define TRNG_MODE_80090B                       2

/* error code definition */
#define CC_TRNG_INVALID_PARAM_TRNG_MODE        (-1)
#define CC_TRNG_INVALID_PARAM_ROSC_LEN         (-2)
#define CC_TRNG_INVALID_PARAM_SAMPLE_CNT       (-3)
#define CC_TRNG_INVALID_PARAM_BUF_SIZE         (-4)
#define CC_TRNG_INVALID_PARAM_NULL_PTR         (-5)
#define CC_TRNG_SAMPLE_LOST                    0x1

#define HW_RNG_ISR_REG_EHR_VALID            0x1
#define HW_RNG_ISR_REG_AUTOCORR_ERR         0x2

#define HW_TRNG_VALID_REG_EHR_NOT_READY     0x0

#define HW_TRNG_DEBUG_CONTROL_REG_VNC_BYPASS         0x2
#define HW_TRNG_DEBUG_CONTROL_REG_CRNGT_BYPASS       0x4
#define HW_TRNG_DEBUG_CONTROL_REG_AUTOCORR_BYPASS    0x8
#define HW_TRNG_DEBUG_CONTROL_REG_FAST      (HW_TRNG_DEBUG_CONTROL_REG_VNC_BYPASS | \
                                            HW_TRNG_DEBUG_CONTROL_REG_CRNGT_BYPASS | \
                                            HW_TRNG_DEBUG_CONTROL_REG_AUTOCORR_BYPASS)
#define HW_TRNG_DEBUG_CONTROL_REG_FE        0x0
#define HW_TRNG_DEBUG_CONTROL_REG_80090B    (HW_TRNG_DEBUG_CONTROL_REG_VNC_BYPASS | \
                                            HW_TRNG_DEBUG_CONTROL_REG_AUTOCORR_BYPASS)

#define OUTPUT_FORMAT_HEADER_LENGTH_IN_WORDS   4
#define OUTPUT_FORMAT_FOOTER_LENGTH_IN_WORDS   3
#define OUTPUT_FORMAT_OVERHEAD_LENGTH_IN_WORDS (OUTPUT_FORMAT_HEADER_LENGTH_IN_WORDS + \
                                                OUTPUT_FORMAT_FOOTER_LENGTH_IN_WORDS)
#define OUTPUT_FORMAT_HEADER_SIG_VAL           0xAABBCCDD
#define OUTPUT_FORMAT_FOOTER_SIG_VAL           0xDDCCBBAA
#define OUTPUT_FORMAT_TRNG_MODE_SHIFT          24
#define OUTPUT_FORMAT_ROSC_LEN_SHIFT           30
#define OUTPUT_FORMAT_HEADER_LENGTH_IN_BYTES   (4*sizeof(uint32_t))
#define OUTPUT_FORMAT_FOOTER_LENGTH_IN_BYTES   (3*sizeof(uint32_t))

/* other size and length definition */
#define EHR_SIZE_IN_WORDS                      6
#define EHR_SIZE_IN_BYTES                      (6*sizeof(uint32_t))
#define MAX_BUFFER_LENGTH                      (1<<24)
#define MIN_BUFFER_LENGTH                      ((OUTPUT_FORMAT_OVERHEAD_LENGTH_IN_WORDS + \
                                               EHR_SIZE_IN_WORDS) * sizeof(uint32_t))

#define TRNG_BUFFER_SIZE_IN_WORDS              6


int R_CC312_Crypto_TRNG(uint32_t sampleCount, uint32_t buffSize, uint8_t *pRndWorkBuff)
{
        /* LOCAL DEFINATIONS AND INITIALIZATIONS*/
        /*return value*/
        uint32_t Error = 0;

        /* loop variable */
        uint32_t i = 0;
        uint32_t j = 0;

        /* the number of full blocks needed */
        uint32_t NumOfBlocks = 0;

        /* hardware parameters */
        uint32_t EhrSizeInWords = EHR_SIZE_IN_WORDS;
        uint32_t tmpSampleCnt = 0;
        uint32_t dataArray[TRNG_BUFFER_SIZE_IN_WORDS] = {0};
        uint32_t *dataBuff_ptr = dataArray;

        uint32_t pRndWorkBuffidx = 0;

        uint32_t TRNGMode = TRNG_MODE_80090B;
        uint32_t roscLength = 3; //TRNG_ROSC_MAX_LENGTH

        /* ............... validate inputs .................................... */

        if (sampleCount < 1)
        {
                sampleCount = 1;
        }

        if ((buffSize < MIN_BUFFER_LENGTH) || (buffSize>= MAX_BUFFER_LENGTH))
        {
                return CC_TRNG_INVALID_PARAM_BUF_SIZE;
        }

        if (NULL == pRndWorkBuff)
        {
                return CC_TRNG_INVALID_PARAM_NULL_PTR;
        }

        /* ........... initializing the hardware .............................. */
        /* -------------------------------------------------------------------- */
        CC_HAL_WRITE_REGISTER(CC_REG_OFFSET(RNG, RNG_CLK_ENABLE), 1);

        /* reset the RNG block */
        CC_HAL_WRITE_REGISTER(CC_REG_OFFSET(RNG, RNG_SW_RESET), 0x1);

        /* enable RNG clock and set sample counter value until it is set correctly*/
        do
        {
                /* enable the HW RND clock   */
                CC_HAL_WRITE_REGISTER(CC_REG_OFFSET(RNG, RNG_CLK_ENABLE), 1);

                /* set sampling ratio (rng_clocks) between consecutive bits */
                CC_HAL_WRITE_REGISTER(CC_REG_OFFSET(RNG, SAMPLE_CNT1), sampleCount);

                /* read the sampling ratio  */
                tmpSampleCnt = CC_HAL_READ_REGISTER(CC_REG_OFFSET(RNG, SAMPLE_CNT1));        

        }while (tmpSampleCnt != sampleCount); /* wait until the sample counter is set correctly*/

        /* set RNG rosc Length */
        CC_HAL_WRITE_REGISTER(CC_REG_OFFSET(RNG, TRNG_CONFIG), roscLength);

        /* configure TRNG debug control register based on different mode. */
        if (TRNGMode == TRNG_MODE_FAST)
        {
                /* fast TRNG: bypass VNC, CRNGT and auto correlate, activate none. */
                CC_HAL_WRITE_REGISTER(CC_REG_OFFSET(RNG, TRNG_DEBUG_CONTROL), HW_TRNG_DEBUG_CONTROL_REG_FAST);
        }
        else if (TRNGMode == TRNG_MODE_FE)
        {
                /* FE TRNG: bypass none, activate all */
                CC_HAL_WRITE_REGISTER(CC_REG_OFFSET(RNG, TRNG_DEBUG_CONTROL), HW_TRNG_DEBUG_CONTROL_REG_FE);
        }
        else if (TRNGMode == TRNG_MODE_80090B)
        {
                /* 800-90B TRNG: bypass VNC and auto correlate, activate CRNGT */
                CC_HAL_WRITE_REGISTER(CC_REG_OFFSET(RNG, TRNG_DEBUG_CONTROL), HW_TRNG_DEBUG_CONTROL_REG_80090B);
        }

        /* enable the RND source */
        CC_HAL_WRITE_REGISTER(CC_REG_OFFSET(RNG, RND_SOURCE_ENABLE), 1);

        /* ........... executing the RND operation ............................ */
        /* -------------------------------------------------------------------- */
        /* write header into buffer */
        *(dataBuff_ptr++) = OUTPUT_FORMAT_HEADER_SIG_VAL;
        *(dataBuff_ptr++) = (TRNGMode << OUTPUT_FORMAT_ROSC_LEN_SHIFT) |
                        (roscLength << OUTPUT_FORMAT_TRNG_MODE_SHIFT) |
                        buffSize;
        *(dataBuff_ptr++) = sampleCount;
        *(dataBuff_ptr++) = OUTPUT_FORMAT_HEADER_SIG_VAL;

        memcpy(&(pRndWorkBuff[pRndWorkBuffidx]), dataArray, OUTPUT_FORMAT_HEADER_LENGTH_IN_BYTES);
        pRndWorkBuffidx += OUTPUT_FORMAT_HEADER_LENGTH_IN_BYTES;
        
        dataBuff_ptr = dataArray;

        /* calculate the number of full blocks needed */
        NumOfBlocks = (buffSize/sizeof(uint32_t) - OUTPUT_FORMAT_OVERHEAD_LENGTH_IN_WORDS) / EhrSizeInWords;

        /* fill the Output buffer with up to full blocks */
        /* BEGIN TIMING: start time measurement at this point */
        for (i = 0; i < NumOfBlocks; i++)
        {
                uint32_t valid_at_start, valid;
                /*
                * TRNG data collecting must be continuous.
                * verification methodology:
                * 1) initial state detection is vaild_at_start == HW_TRNG_VALID_REG_EHR_NOT_READY
                * 2) proceed only after HW_RNG_ISR_REG_EHR_VALID bit changed from 0 to 1
                *    (that means EHR data is ready for reading)
                */
                valid_at_start = CC_HAL_READ_REGISTER(CC_REG_OFFSET(RNG, TRNG_VALID));
                valid = valid_at_start;
                /*
                 * wait for EHR valid. ISR_REG indicates whether EHR valid or any error detected.
                 * bit[0]-EHR_VALID, bit[1]-AUTOCORR_ERR, bit[2]-CRNGT_ERR, bit[3]-VN_ERR
                 */
                while ((valid & (HW_RNG_ISR_REG_EHR_VALID|HW_RNG_ISR_REG_AUTOCORR_ERR)) == 0x0)
                {
                        valid = CC_HAL_READ_REGISTER(CC_REG_OFFSET(RNG, RNG_ISR));
                }
                
                /*
                * after EHR data is ready, first check any error detected
                * different bit in variable Error indicates different error
                * Error bit[0] samples were lost during collection.
                * Error bit[1] autocorrelation error. corresponding to ISR_REG[1].
                *              this error will cause TRNG cease to function until next reset
                * Error bit[2] CRNGT error. corresponding to ISR_REG[2].
                *              This error occurs when 2 consecutive blocks of 16 collected bits are equal.
                * Error bit[3] Von Neumann error. corresponding to ISR_REG[3].
                *              this error occurs if 32 consecutive collected bits are identical.
                */
                /* if it is not the first iteration, valid_at_start must be not ready. otherwise it means samples were lost */
                if ((valid_at_start != HW_TRNG_VALID_REG_EHR_NOT_READY) && (i != 0))
                {
                        Error = CC_TRNG_SAMPLE_LOST;
                }

                /*pass bit[31:1] to Error. mask out bit[0] which is used for CC_TRNG_SAMPLE_LOST*/
                if ((valid & ~CC_TRNG_SAMPLE_LOST) != 0)
                {
                Error |= (valid & ~CC_TRNG_SAMPLE_LOST);
                }

                if (Error & HW_RNG_ISR_REG_AUTOCORR_ERR)
                {
                break; /* autocorrelation error is irrecoverable */
                }

                /* clean up interrupt status */
                CC_HAL_WRITE_REGISTER(CC_REG_OFFSET(RNG, RNG_ICR), ~0UL);

                /*
                * load the current random data from EHR registers to the output buffer.
                * NOTE: TRNG hardware will auto start to re-fill bits to EHR_DATA_REG0-5
                *       after the host read out all 6 registers
                */
                for (j=0; j<EhrSizeInWords; j++)
                {
                        *(dataBuff_ptr++) = CC_HAL_READ_REGISTER(DX_EHR_DATA_0_REG_OFFSET + (i * sizeof(uint32_t)));
                }

                memcpy(&(pRndWorkBuff[pRndWorkBuffidx]), dataArray, EHR_SIZE_IN_BYTES);
                pRndWorkBuffidx += EHR_SIZE_IN_BYTES;
                
                dataBuff_ptr = dataArray;
        }
        /* END TIMING: end time measurement at this point */

        /* write footer into buffer */
        *(dataBuff_ptr++) = OUTPUT_FORMAT_FOOTER_SIG_VAL;
        /* only record sample lost error in output buffer */
        *(dataBuff_ptr++) = Error & CC_TRNG_SAMPLE_LOST;
        *(dataBuff_ptr++) = OUTPUT_FORMAT_FOOTER_SIG_VAL;

        memcpy(&(pRndWorkBuff[pRndWorkBuffidx]), dataArray, OUTPUT_FORMAT_FOOTER_LENGTH_IN_BYTES);
        pRndWorkBuffidx += OUTPUT_FORMAT_FOOTER_LENGTH_IN_BYTES;


        /* disable the RND source */
        CC_HAL_WRITE_REGISTER(CC_REG_OFFSET(RNG, RND_SOURCE_ENABLE), 0UL);

        return Error;
}

