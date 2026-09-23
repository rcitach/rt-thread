/**
 ****************************************************************************************
 *
 * @file r_cc312_crypto_secureboot.c
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
#include <stdbool.h>

/* remove !!!!!!!!!!!!! #include "osal.h" */
#include "r_cc312_common.h"

/* cerberus pal */
#include "cc_hal.h"
#include "cc_pal_init.h"
#include "cc_pal_types.h"
#include "cc_pal_types_plat.h"
#include "cc_pal_mem.h"
#include "cc_pal_perf.h"
#include "cc_regs.h"
#include "cc_otp_defs.h"
#include "cc_lib.h"
//REMOVE: #include "cc_rnd.h"
#include "cc_pal_x509_defs.h"

/* mbedtls */
#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#include "mbedtls/platform.h"
#include "mbedtls/memory_buffer_alloc.h"
#include "mbedtls/entropy.h"

#include "r_cc312_crypto.h"
#include "r_cc312_secureboot.h"

/* sbrom */
//#include "cc_sec_defs.h"
#include "secureboot_defs.h"
#include "secureboot_stage_defs.h"
#include "cc_pal_sb_plat.h"
#include "bootimagesverifier_def.h"
#include "bootimagesverifier_api.h"
#include "bsv_api.h"
#include "bsv_otp_api.h"
#include "bsv_error.h"
#include "secdebug_api.h"

#include "cc_cmpu.h"
#include "cc_dmpu.h"

#include "dx_env.h"

#include "mbedtls_cc_util_asset_prov.h"

#define	BSVIT_PRINT_DBG(...)		if(ra6w1_crypto_debug == 1){ CRYPTO_PRINTF( __VA_ARGS__ ); }
#define	BSVIT_PRINT_FDBG(...)		if(ra6w1_crypto_debug == 1){ CRYPTO_PRINTF( __VA_ARGS__ ); }
#define	BSVIT_PRINT_MEASURE(...)	if(ra6w1_crypto_debug == 1){ CRYPTO_PRINTF( __VA_ARGS__ ); }
#define	BSVIT_PRINT_ERROR(...)		if(ra6w1_crypto_debug == 1){ CRYPTO_PRINTF( __VA_ARGS__ ); CRYPTO_DELAY(100000);}
#define	SBROM_DBG_TRIGGER(...)		//if(ra6w1_crypto_debug == 1){ CRYPTO_PRINTF( "SBROM:%08x\n", __VA_ARGS__ ); }
#define	MODE_CRY_STEP(x)	        (0xC0000000|((x<<4)&0x0FFFFFF0))

typedef 	void	*HANDLE;
typedef 	void	(*USR_CALLBACK )(void *);

#define RRQ61X_IMG_BOOT			(0)
#define SUPPORT_BOOT_BRANCH     (0)

#define true    1
#define false   0
#define NULL    ((void *)0)

#define CC_PRINTF(...)              printf(__VA_ARGS__)
/******************************************************************************
 *
 ******************************************************************************/

/**
 * Error Codes
 */
typedef enum BsvItError_t
{
    BSVIT_ERROR__OK = 0,
    BSVIT_ERROR__FAIL = 0x0000FFFF,
}BsvItError_t;

typedef	 struct {
	CCSbCertInfo_t *sbCertInfoCtx;
	uint32_t *pWorkspaceAligned;
	uint32_t  CertHeaderInfoSize;
	uint32_t *pOutCertHeaderInfoAligned;
	uint8_t chainIndex;
	HANDLE  sbootflash;
	CCSbFlashReadFunc flashwrap;
	uint32_t  loadaddr;
	uint32_t  jmpaddr;
	USR_CALLBACK stopproc;
	uint64_t	bcfmeasure;
	uint64_t	bcfmeasureflash;
} RRQ61X_SBOOT_TYPE;

#define RRQ61X_MAX_CHAIN	5

#define SBOOT_BCFM_START(f)		if(ra6w1_crypto_debug == 1){f->bcfmeasure = CRYPTO_MEASURE(0);}
#define SBOOT_BCFM_FINISH(f)	if(ra6w1_crypto_debug == 1){f->bcfmeasure = CRYPTO_MEASURE(0) - (f->bcfmeasure);}
#define SBOOT_BCFM_PRINT(f,str)	if(ra6w1_crypto_debug == 1){BSVIT_PRINT_MEASURE("[BCF] %s - %lld, %lld\n", str, (f->bcfmeasure), (f->bcfmeasureflash) );}

#define SBMEASURE_START()              bcfmeasure = CRYPTO_MEASURE(1)
#define SBMEASURE_FINISH()             {                                                \
                	bcfmeasure = CRYPTO_MEASURE(0) - bcfmeasure;                    \
                	BSVIT_PRINT_MEASURE("[SBMEASURE] - %lld\n", bcfmeasure);        \
                }


/******************************************************************************
 *
 ******************************************************************************/

static BsvItError_t r_cc312_sboot_init(void);
static BsvItError_t r_cc312_sboot_boot(uint32_t faddress, uint32_t *jaddress, USR_CALLBACK stopproc);
static BsvItError_t r_cc312_sdebug_fast(uint32_t lcs, HANDLE sflash, uint32_t faddress);
static BsvItError_t r_cc312_DeviceCompleteDisable(void);
static void r_cc312_debug_set_errorcode(uint32_t ecode);

#ifdef	BUILD_OPT_RRQ61X_FPGA
static BsvItError_t test_prepareOtp(uint32_t mode);
#endif	//BUILD_OPT_RRQ61X_FPGA

static	uint32_t	ra6w1_crypto_debug;
static	uint32_t	ra6w1_crypto_ecode;

extern CCError_t CC_DeviceCompleteDisable(unsigned long   hwBaseAddress);


/******************************************************************************
 *  R_CC312_Debug_SecureBoot( )
 *
 *  Purpose :
 *  Input   :
 *  Output  :
 *  Return  :
 ******************************************************************************/

void R_CC312_Debug_SecureBoot(uint32_t mode)
{
	ra6w1_crypto_debug = mode;
}

uint32_t R_CC312_Debug_SecureBoot_Mode(void)
{
	return ra6w1_crypto_debug;
}

uint32_t R_CC312_Debug_Get_ErrorCode(void)
{
        return ra6w1_crypto_ecode;
}

static void r_cc312_debug_set_errorcode(uint32_t ecode)
{
        ra6w1_crypto_ecode = ecode;
}

/******************************************************************************
 *  R_CC312_SecureBoot( )
 *
 *  Purpose :
 *  Input   :
 *  Output  :
 *  Return  :
 ******************************************************************************/

uint32_t R_CC312_SecureBoot(uint32_t taddress, uint32_t *jaddress, USR_CALLBACK stopproc)
{
	BsvItError_t rc = BSVIT_ERROR__OK;

	CC_PalInit(); // for Mutex
	CC_HalInit();

	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32000));

	rc = r_cc312_sboot_init();
	if( rc != BSVIT_ERROR__OK ) goto end_step;

	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32001));

	rc = r_cc312_sboot_boot(taddress, jaddress, stopproc);
	if( rc != BSVIT_ERROR__OK ) goto end_step;

	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32002));

end_step:
	CC_HalTerminate();
	CC_PalTerminate();

	return (rc == BSVIT_ERROR__OK) ? true : false;
}


uint32_t R_CC312_SecureDebug(HANDLE fhandler, uint32_t faddress)
{
	BsvItError_t rc = BSVIT_ERROR__OK;
	CCError_t ccRc = CC_OK;
	uint32_t lcs;

	CC_PalInit(); // for Mutex
	CC_HalInit();

	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32100));

	rc = r_cc312_sboot_init();
	if( rc != BSVIT_ERROR__OK ) goto end_dbgstep;

	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32101));

	ccRc = CC_BsvLcsGet(RRQ61X_ACRYPT_BASE, &lcs);

	if( ccRc != CC_OK ){ 		
		goto end_dbgstep;
	}

	switch(lcs){
	case CC_BSV_CHIP_MANUFACTURE_LCS:
		SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC321A0));
		BSVIT_PRINT_DBG("SDebug-CM-Skip\n");
		break;

	case CC_BSV_DEVICE_MANUFACTURE_LCS:
	case CC_BSV_SECURE_LCS:
	case CC_BSV_RMA_LCS:
		SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC321B0));
		BSVIT_PRINT_DBG("SDebug-%d\n", lcs);
		rc = r_cc312_sdebug_fast(lcs, fhandler, faddress);
		break;

	default:
		SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC321F0));
		BSVIT_PRINT_DBG("SDebug-???\n");
		//TODO: DeviceCompleteDisable();
		rc = r_cc312_DeviceCompleteDisable();
		break;
	}

	if( rc != BSVIT_ERROR__OK ) goto end_dbgstep;

	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32102));

end_dbgstep:
	CC_HalTerminate();
	CC_PalTerminate();

	return (rc == BSVIT_ERROR__OK) ? true : false;
}


#ifdef	BUILD_OPT_RRQ61X_FPGA
void	R_CC312_TestOtp(uint32_t mode)
{
	CC_PalInit(); // for Mutex
	CC_HalInit();

	test_prepareOtp(mode);

	CC_HalTerminate();
	CC_PalTerminate();
}
#endif	//BUILD_OPT_RRQ61X_FPGA

uint32_t R_CC312_SecureSocID_internal(uint8_t *pSocID)
{
	BsvItError_t rc = BSVIT_ERROR__OK;
	CCError_t ccRc = CC_OK;
	uint32_t lcs;

	CC_PalInit(); // for Mutex
	CC_HalInit();

	rc = r_cc312_sboot_init();
	if( rc != BSVIT_ERROR__OK ) goto sdebug_bail;

	ccRc = CC_BsvLcsGet(RRQ61X_ACRYPT_BASE, &lcs);

	if (ccRc == CC_OK) {
		char *state;
		switch(lcs){
		case CC_BSV_CHIP_MANUFACTURE_LCS:	state = "CM"; break;
		case CC_BSV_DEVICE_MANUFACTURE_LCS:	state = "DM"; break;
		case CC_BSV_SECURE_LCS:			state = "SECURE"; break;
		case CC_BSV_RMA_LCS:			state = "RMA"; break;
		default:				state = "FATAL"; break;
		}
		CRYPTO_PRINTF("LifeCycle: %s\n", state);


        	if( lcs == CC_BSV_SECURE_LCS )
        	{
        		CCHashResult_t *SocID;

        		SocID = (CCHashResult_t *)CRYPTO_MALLOC(sizeof(CCHashResult_t));
        		if( SocID == NULL ){
        			rc = BSVIT_ERROR__FAIL;
        			goto sdebug_bail;
        		}

        		// 1. calculate SOC_ID
        		// export the SOC_ID
        		// this step can also be performed from the runtime software
        		/* Compute the SOC_ID only for secure life cycle
        		 * SOC_ID is a function of HBK and HUK which are fully present only in secure mode. */

        		/* Call API */
        		SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32003));

        		// TODO: CC_BSV_READ_OTP_WORD(RRQ61X_ACRYPT_BASE, (0x10<<2), ccRc);		// OTP enable

        		ccRc = CC_BsvSocIDCompute(RRQ61X_ACRYPT_BASE, (*SocID));
        		SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32004));

        		if (ccRc != CC_OK)
        		{
        			BSVIT_PRINT_ERROR("Failed CC_BsvSocIDCompute - ccRc = 0x%x\n", ccRc);
					r_cc312_debug_set_errorcode((uint32_t)ccRc);

        			rc = BSVIT_ERROR__FAIL;
        		}
        		else
        		{
        		        CRYPTO_PRINTF("CC_BsvSocIDCompute return SocID");

                                if( pSocID != NULL ){
                                        CRYPTO_MEMCPY(pSocID, SocID, sizeof(CCHashResult_t));
                                }
                                        
        			//CRYPTO_DBG_DUMP(0, (*SocID), sizeof(CCHashResult_t));
        			{
        			    size_t i;
        			    uint8_t *SocIDBuff;

        			    SocIDBuff = (uint8_t *)(*SocID);

        			    for( i = 0; i < sizeof(CCHashResult_t); i++ ){
        			    	if( (i % 16) == 0 ){ CRYPTO_PRINTF("\n\t"); }
        			        CRYPTO_PRINTF("%c%c "
        					, "0123456789ABCDEF" [SocIDBuff[i] / 16]
        					, "0123456789ABCDEF" [SocIDBuff[i] % 16] );
        			    }
        			    CRYPTO_PRINTF( "\n" );
        			}
        			CRYPTO_MEMSET((*SocID), 0, sizeof(CCHashResult_t));
        		}

        		CRYPTO_FREE(SocID);
        	}
                
	}

sdebug_bail:
	CC_HalTerminate();
	CC_PalTerminate();

	return (rc == BSVIT_ERROR__OK) ? true : false;
}


uint32_t R_CC312_SecureSocID(void)
{
        return R_CC312_SecureSocID_internal(NULL);
}

/******************************************************************************
 *  test_prepareOtp( )
 *
 *  Purpose :
 *  Input   :
 *  Output  :
 *  Return  :
 ******************************************************************************/
#ifdef	BUILD_OPT_RRQ61X_FPGA

extern BsvItError_t bsvIt_burnOtp(unsigned int *otpBuf, unsigned int nextLcs);

static BsvItError_t test_prepareOtp(uint32_t mode)
{
#define TEST_OTP_SIZE_IN_WORDS 		0x2C

    BsvItError_t rc = BSVIT_ERROR__OK;
    const uint32_t OTP_SIZE = TEST_OTP_SIZE_IN_WORDS * sizeof(uint32_t);

    uint32_t otpLcs;

    uint32_t * pOtpValuesAligned = NULL;

    BSVIT_PRINT_DBG("prepare Otp\n");

    if( mode != CC_BSV_CHIP_MANUFACTURE_LCS ){
    	return BSVIT_ERROR__FAIL;
    }

    otpLcs = CC_BSV_CHIP_MANUFACTURE_LCS;

    /* Allocate dma-able region */
    pOtpValuesAligned   = (uint32_t *)CRYPTO_MALLOC(OTP_SIZE);

    /* Copy OTP image to dma-able memory space */
    memset(pOtpValuesAligned, 0x0, OTP_SIZE);

    {
    	uint32_t i;
		uint32_t OtpWord;
		for( i = 0; i < 0x2C; i++ ){
			CC_BsvOTPWordRead(RRQ61X_ACRYPT_BASE, i, &OtpWord );
			CRYPTO_PRINTF("OTP[%02x] := %08x\n", i, OtpWord );
		}
    }

    /*  Burn the OTP for secure LCS and with the Kpicv, kceicv */
    if (bsvIt_burnOtp(pOtpValuesAligned, otpLcs) != 0)
    {
        BSVIT_PRINT_ERROR("Failed to bsvIt_burnOtp\n");
        rc = BSVIT_ERROR__FAIL;
        goto bail;
    }

bail:
    CRYPTO_FREE(pOtpValuesAligned);

    return rc;
}

#endif	//BUILD_OPT_RRQ61X_FPGA
/******************************************************************************
 *  r_cc312_sboot_init( )
 ******************************************************************************/

static BsvItError_t r_cc312_sboot_init(void)
{
	BsvItError_t rc = BSVIT_ERROR__OK;
	CCError_t ccRc = CC_OK;
	uint32_t lcs;
	uint32_t rcRmaFlag;
	uint64_t bcfmeasure;

	SBMEASURE_START();

	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32008));

	ccRc = CC_BsvInit(RRQ61X_ACRYPT_BASE);

	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32009));

	if(ccRc != CC_OK){
		r_cc312_debug_set_errorcode((uint32_t)ccRc);
		rc = r_cc312_DeviceCompleteDisable();

		goto init_bail;
	}

	/* Init library */
	ccRc = CC_BsvLcsGetAndInit(RRQ61X_ACRYPT_BASE, &lcs);

	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC3200A));

	if (ccRc != CC_OK)
	{
		uint32_t* pWorkspaceAligned = NULL;

		pWorkspaceAligned = (uint32_t *)CRYPTO_MALLOC(CC_SB_MIN_WORKSPACE_SIZE_IN_BYTES);

		BSVIT_PRINT_ERROR("Failed CC_BsvLcsGetAndInit - ccRc = 0x%x\n", ccRc);
		r_cc312_debug_set_errorcode((uint32_t)ccRc);
		rc = BSVIT_ERROR__FAIL;

		CC_BsvFatalErrorSet(RRQ61X_ACRYPT_BASE);

		SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC3200E));

		/* this is done in order to lock the DCU and not enable debug */
		ccRc = CC_BsvSecureDebugSet(RRQ61X_ACRYPT_BASE,
		                NULL,
		                0,
		                &rcRmaFlag,
		                pWorkspaceAligned,
		                CC_SB_MIN_WORKSPACE_SIZE_IN_BYTES);

		SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC3200F));

		CRYPTO_FREE(pWorkspaceAligned);
                if( ccRc != CC_OK ){
					r_cc312_debug_set_errorcode((uint32_t)ccRc);
                }
		//TODO: Abort_bootCode(); /* doesn't return */
		goto init_bail;
	}

	/* 1. Enable core clock gating */
	ccRc = CC_BsvCoreClkGatingEnable(RRQ61X_ACRYPT_BASE);

	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC3200D));

	if (ccRc != CC_OK)
	{
		BSVIT_PRINT_ERROR("Failed CC_BsvCoreClkGatingEnable - ccRc = 0x%x\n", ccRc);
		r_cc312_debug_set_errorcode((uint32_t)ccRc);
		rc = BSVIT_ERROR__FAIL;

		goto init_bail;
	}

	/* 2. Set secure mode to unsecured */
	ccRc = CC_BsvSecModeSet(RRQ61X_ACRYPT_BASE, CC_FALSE, CC_FALSE);

	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC3200B));

	if ((ccRc != CC_OK) && (ccRc != CC_BSV_APB_SECURE_IS_LOCKED_ERR))
	{
		BSVIT_PRINT_ERROR("Failed CC_BsvSecModeSet - ccRc = 0x%x\n", ccRc);
		r_cc312_debug_set_errorcode((uint32_t)ccRc);
		rc = BSVIT_ERROR__FAIL;

		goto init_bail;
	}

	/* 3. Set privileged mode to unprivileged */
	ccRc = CC_BsvPrivModeSet(RRQ61X_ACRYPT_BASE, CC_FALSE, CC_FALSE);

	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC3200C));

	if ((ccRc != CC_OK) && (ccRc != CC_BSV_APB_PRIVILEG_IS_LOCKED_ERR))
	{
		BSVIT_PRINT_ERROR("Failed CC_BsvPrivModeSet - ccRc = 0x%x\n", ccRc);
		r_cc312_debug_set_errorcode((uint32_t)ccRc);
		rc = BSVIT_ERROR__FAIL;

		goto init_bail;
	}

init_bail:
	SBMEASURE_FINISH();

	return rc;
}

/******************************************************************************
 *  r_cc312_sboot_boot( )
 ******************************************************************************/

static BsvItError_t r_cc312_sboot_CM(uint32_t lcs, uint32_t faddress, uint32_t *jaddress, USR_CALLBACK stopproc);
static BsvItError_t r_cc312_sboot_DM(uint32_t lcs, uint32_t faddress, uint32_t *jaddress, USR_CALLBACK stopproc);
static BsvItError_t r_cc312_sboot_Secure(uint32_t lcs, uint32_t faddress, uint32_t *jaddress, USR_CALLBACK stopproc);
static BsvItError_t r_cc312_sboot_RMA(uint32_t lcs, uint32_t faddress, uint32_t *jaddress, USR_CALLBACK stopproc);

static BsvItError_t r_cc312_sboot_boot(uint32_t faddress , uint32_t *jaddress, USR_CALLBACK stopproc)
{
	BsvItError_t rc = BSVIT_ERROR__OK;
	CCError_t ccRc = CC_OK;
	uint32_t lcs;

	ccRc = CC_BsvLcsGet(RRQ61X_ACRYPT_BASE, &lcs);

	switch(lcs){
	case CC_BSV_CHIP_MANUFACTURE_LCS:
		SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC320C0));
		BSVIT_PRINT_DBG("SBoot-CM: %08x\n", faddress);
		rc = r_cc312_sboot_CM(lcs, faddress, jaddress, stopproc);
		break;

	case CC_BSV_DEVICE_MANUFACTURE_LCS:
		SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC320D0));
		BSVIT_PRINT_DBG("SBoot-DM: %08x\n", faddress);
		rc = r_cc312_sboot_DM(lcs, faddress, jaddress, stopproc);
		break;

	case CC_BSV_SECURE_LCS:
		SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC320A0));
		BSVIT_PRINT_DBG("SBoot-SECURE: %08x\n", faddress);
		rc = r_cc312_sboot_Secure(lcs, faddress, jaddress, stopproc);
		break;

	case CC_BSV_RMA_LCS:
		SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC320E0));
		BSVIT_PRINT_DBG("SBoot-RMA: %08x\n", faddress);
		rc = r_cc312_sboot_RMA(lcs, faddress, jaddress, stopproc);
		break;

	default:
		SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC320F0));

		BSVIT_PRINT_DBG("SBoot-???\n");
		//TODO: DeviceCompleteDisable();
		rc = r_cc312_DeviceCompleteDisable();
		r_cc312_debug_set_errorcode((uint32_t)ccRc);

		break;
	}

	return rc;
}

/******************************************************************************
 *  FLASH Wrapper
 ******************************************************************************/

static	uint32_t cc312_sboot_flashRead(CCAddr_t flashAddress, uint8_t *pMemDst, uint32_t sizeToRead, void* context)
{
	RRQ61X_SBOOT_TYPE *sboot;

	sboot = (RRQ61X_SBOOT_TYPE *)context;

	BSVIT_PRINT_FDBG( "FLASH(%p): %08x, %p, siz %d\n", context, flashAddress, pMemDst, sizeToRead);
	embcrypto_flash_image_read( (HANDLE)(sboot->sbootflash), flashAddress, pMemDst, sizeToRead);
	//CRYPTO_DBG_DUMP(0, pMemDst, sizeToRead);

	return BSVIT_ERROR__OK;
}

/******************************************************************************
 *  Utils for SBROM Setup
 ******************************************************************************/

static BsvItError_t CC312_SBOOT_OPEN(RRQ61X_SBOOT_TYPE *cc321sboot, uint32_t faddress, USR_CALLBACK stopproc)
{
	BsvItError_t rc = BSVIT_ERROR__OK;

	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32010));

	if( cc321sboot == NULL ){
		SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32011));
		return BSVIT_ERROR__FAIL;
	}

	CRYPTO_MEMSET(cc321sboot, 0x00, sizeof(RRQ61X_SBOOT_TYPE));

	SBOOT_BCFM_START(cc321sboot);

	cc321sboot->sbCertInfoCtx = (CCSbCertInfo_t *)CRYPTO_MALLOC(sizeof(CCSbCertInfo_t));
	cc321sboot->pOutCertHeaderInfoAligned = (uint32_t *)CRYPTO_MALLOC(sizeof(CCX509CertHeaderInfo_t));
	cc321sboot->CertHeaderInfoSize = sizeof(CCX509CertHeaderInfo_t);
	cc321sboot->pWorkspaceAligned = (uint32_t *)CRYPTO_MALLOC(CC_SB_MIN_WORKSPACE_SIZE_IN_BYTES);

	if( (cc321sboot->sbCertInfoCtx == NULL)
	    || (cc321sboot->pOutCertHeaderInfoAligned == NULL)
	    || (cc321sboot->pWorkspaceAligned == NULL) ){
		rc = BSVIT_ERROR__FAIL;
		SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32012));
		r_cc312_debug_set_errorcode((uint32_t)MODE_CRY_STEP(0xC32012));

		goto setup_fin;
	}

	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32013));

	cc321sboot->sbootflash = embcrypto_flash_image_open( (RRQ61X_IMG_BOOT|(sizeof(uint32_t)*8)) , faddress, NULL);
	if( cc321sboot->sbootflash == NULL ){
		rc = BSVIT_ERROR__FAIL;
		SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32014));
		r_cc312_debug_set_errorcode((uint32_t)MODE_CRY_STEP(0xC32014));

		goto setup_fin;
	}

	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32015));

	embcrypto_flash_image_check(cc321sboot->sbootflash, 0, faddress);

	cc321sboot->flashwrap = (CCSbFlashReadFunc) cc312_sboot_flashRead;
	cc321sboot->stopproc = (USR_CALLBACK) stopproc;

setup_fin:
	SBOOT_BCFM_FINISH(cc321sboot);
	SBOOT_BCFM_PRINT(cc321sboot, __func__ );

	return rc;
}

static void CC312_SBOOT_CLOSE(RRQ61X_SBOOT_TYPE *cc321sboot, uint32_t *jmpaddr)
{
	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32020));

	if( cc321sboot == NULL ){
		if( jmpaddr != NULL ){
			*jmpaddr = 0;
		}
		return ;
	}

	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32021));

	SBOOT_BCFM_START(cc321sboot);

	if( cc321sboot->sbCertInfoCtx != NULL ) 	{
		CRYPTO_FREE(cc321sboot->sbCertInfoCtx);
	}
	if( cc321sboot->pOutCertHeaderInfoAligned != NULL ) {
		CRYPTO_FREE(cc321sboot->pOutCertHeaderInfoAligned);
	}
	if( cc321sboot->pWorkspaceAligned != NULL ) {
		CRYPTO_FREE(cc321sboot->pWorkspaceAligned);
	}
	if( cc321sboot->sbootflash != NULL ){
		SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32022));
		embcrypto_flash_image_close(cc321sboot->sbootflash);
	}

	SBOOT_BCFM_FINISH(cc321sboot);
	SBOOT_BCFM_PRINT(cc321sboot, __func__ );

	if( jmpaddr != NULL ){
		SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32023));

		*jmpaddr = cc321sboot->jmpaddr;

		if(  (*jmpaddr != 0) && (cc321sboot->stopproc != NULL) ){
			SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32024));
			cc321sboot->stopproc( (void *)(*jmpaddr) );
		}
	}

	CRYPTO_MEMSET(cc321sboot, 0x00, sizeof(RRQ61X_SBOOT_TYPE));

	return ;
}

static BsvItError_t CC312_SBOOT_VERIFICATION(RRQ61X_SBOOT_TYPE *cc321sboot, uint32_t faddress)
{
	uint32_t certsize;
	BsvItError_t rc = BSVIT_ERROR__OK;
	CCError_t ccRc = CC_OK;

	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32030));

	if( cc321sboot == NULL ){
		return BSVIT_ERROR__FAIL;
	}

	SBOOT_BCFM_START(cc321sboot);

	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32031));

	ccRc = CC_SbCertChainVerificationInit(cc321sboot->sbCertInfoCtx);
	if (ccRc != CC_OK)
	{
		BSVIT_PRINT_ERROR("CC_SbCertChainVerificationInit failed with - 0x%X\n", ccRc);
		r_cc312_debug_set_errorcode((uint32_t)ccRc);
		rc = BSVIT_ERROR__FAIL;
		SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32032));

		goto verify_fin;
	}
	else
	{
		BSVIT_PRINT_DBG("CC_SbCertChainVerificationInit succeeded!!\n");
	}

	/* Verify chain certificates */
	for (cc321sboot->chainIndex = 0; cc321sboot->chainIndex < RRQ61X_MAX_CHAIN ; ++cc321sboot->chainIndex)
	{
		CCAddr_t storeFlashAddress;

		SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32033));
		certsize = 0;
		storeFlashAddress = (CCAddr_t)embcrypto_flash_image_certificate(cc321sboot->sbootflash, faddress, (uint32_t)(cc321sboot->chainIndex), &certsize);

		if( storeFlashAddress == 0 ){
			SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32034));
			if( cc321sboot->chainIndex == 0){
				r_cc312_debug_set_errorcode((uint32_t)MODE_CRY_STEP(0xC32034));
				rc = BSVIT_ERROR__OK; // for RMA

				goto verify_fin;
			}
			break;
		}

		BSVIT_PRINT_DBG("CC_SbCertVerifySingle %d = %08x, %d\n", cc321sboot->chainIndex, storeFlashAddress, certsize);

		/* verify key certificate */
		SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32035));

		ccRc = CC_SbCertVerifySingle(cc321sboot->flashwrap,
					cc321sboot,
					(unsigned long)RRQ61X_ACRYPT_BASE,
					storeFlashAddress,
					cc321sboot->sbCertInfoCtx,
					//pOutCertHeaderInfoAligned, CertHeaderInfoSize,
					NULL, 0,
					cc321sboot->pWorkspaceAligned,
					CC_SB_MIN_WORKSPACE_SIZE_IN_BYTES
				);

		if (ccRc != CC_OK)
		{
			BSVIT_PRINT_ERROR("CC_SbCertVerifySingle for cert[%d] failed with - 0x%X\n", cc321sboot->chainIndex, ccRc);
			r_cc312_debug_set_errorcode((uint32_t)ccRc);
			rc = BSVIT_ERROR__FAIL;
			SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32036));

			goto verify_fin;
		}
		else
		{
			SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32037));
			BSVIT_PRINT_DBG("CC_SbCertVerifySingle for cert[%d] succeeded!!\n", cc321sboot->chainIndex);
		}
	}

	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32038));
	embcrypto_flash_image_extract(cc321sboot->sbootflash, faddress, &(cc321sboot->loadaddr), &(cc321sboot->jmpaddr));

verify_fin:
	SBOOT_BCFM_FINISH(cc321sboot);
	SBOOT_BCFM_PRINT(cc321sboot, __func__ );

	return rc;
}

static BsvItError_t CC312_SBOOT_DEBUG(uint32_t lcs, RRQ61X_SBOOT_TYPE *cc321sboot, uint32_t faddress)
{
	BsvItError_t rc = BSVIT_ERROR__OK;
	CCError_t ccRc = CC_OK;
	uint32_t *pWorkspaceAligned = NULL;
	uint32_t *pCertPkgPtr = NULL;
	uint32_t pkgSize = 0;
	uint32_t rcRmaFlag = 0;
	uint32_t rmaEntrySignalsSet = 0;
	uint32_t	storeFlashAddress;

	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32040));

	if( cc321sboot == NULL ){
		r_cc312_debug_set_errorcode((uint32_t)0);

		return BSVIT_ERROR__FAIL;
	}

	pWorkspaceAligned = (uint32_t *)CRYPTO_MALLOC(CC_SB_MIN_WORKSPACE_SIZE_IN_BYTES);
	if( pWorkspaceAligned == NULL ){
		r_cc312_debug_set_errorcode((uint32_t)0);

		return BSVIT_ERROR__FAIL;
	}

	SBOOT_BCFM_START(cc321sboot);

	CRYPTO_MEMSET(pWorkspaceAligned, 0, CC_SB_MIN_WORKSPACE_SIZE_IN_BYTES);

	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32041));

	storeFlashAddress = (uint32_t)embcrypto_flash_image_certificate(cc321sboot->sbootflash, faddress, 0xFFFFFFFF, (uint32_t *)&pkgSize);

	if( storeFlashAddress == 0 ){
		rc = BSVIT_ERROR__OK; // Skip
		r_cc312_debug_set_errorcode((uint32_t)MODE_CRY_STEP(0xC32042));
		SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32042));
		pCertPkgPtr = NULL; 			// DCU Test

		goto /*dbg_fin*/ dbg_nul_cert;		// DCU Test
	}

	pCertPkgPtr = (uint32_t *)CRYPTO_MALLOC(pkgSize);
	if( pCertPkgPtr == NULL ){
		rc = BSVIT_ERROR__FAIL;
		r_cc312_debug_set_errorcode((uint32_t)MODE_CRY_STEP(0xC32043));
		SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32043));

		goto /*dbg_fin*/ dbg_nul_cert;		// DCU Test
	}

	BSVIT_PRINT_DBG("DbgCert: %08x, %d\n", storeFlashAddress, pkgSize);

	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32044));

	embcrypto_flash_image_read( cc321sboot->sbootflash, storeFlashAddress, pCertPkgPtr, pkgSize);

	//////////////////////////////////////////////////////
dbg_nul_cert:						// DCU Test
	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32045));

	ccRc = CC_BsvSecureDebugSet(RRQ61X_ACRYPT_BASE,
	                pCertPkgPtr,
	                pkgSize,
	                &rcRmaFlag,
	                pWorkspaceAligned,
	                CC_SB_MIN_WORKSPACE_SIZE_IN_BYTES);

	if (ccRc != CC_OK)
	{
		if( ccRc == CC_BSV_AO_WRITE_FAILED_ERR ){
			BSVIT_PRINT_ERROR("Already locked in CC_BsvSecureDebugSet, ccRc 0x%x\n", ccRc);
			r_cc312_debug_set_errorcode((uint32_t)ccRc);
			rc = BSVIT_ERROR__OK; // Reboot or DPM Boot case
			SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC3204A));
		}else{
			BSVIT_PRINT_ERROR("Failed to CC_BsvSecureDebugSet, ccRc 0x%x\n", ccRc);
			r_cc312_debug_set_errorcode((uint32_t)ccRc);
			rc = BSVIT_ERROR__FAIL;
			SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32046));

			goto dbg_fin;
		}
	}else{
		BSVIT_PRINT_DBG("CC_BsvSecureDebugSet for DbgCert succeeded!!\n");
	}

	/* is the provided certificate RMA certificate */
	if( (lcs != CC_BSV_RMA_LCS) && (rcRmaFlag == true) )
	{
		BSVIT_PRINT_DBG("RMA mode enable - %d\n", lcs);
		SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32047));

		ccRc = CC_BsvRMAModeEnable(RRQ61X_ACRYPT_BASE);
		if (ccRc != CC_OK)
		{
			r_cc312_debug_set_errorcode((uint32_t)ccRc);
			rc = BSVIT_ERROR__FAIL;

			goto dbg_fin;
		}
	}

	//TODO: rmaEntrySignalsSet = ???;

	if( (lcs != CC_BSV_RMA_LCS) && (rcRmaFlag || rmaEntrySignalsSet ) )
	{
		//TODO: BootROM_RmaModeEntry();
		SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32048));
	}

	rmaEntrySignalsSet = R_CC312_SecureBoot_GetLock2(); // ICV-Lock Test

	if( (lcs == CC_BSV_SECURE_LCS) && (rcRmaFlag != true) && (rmaEntrySignalsSet != 0) ){
		// CC_BsvICVKeyLock, 4-75
		// lock the ICV Keys to prevent the OEM.
		ccRc = CC_BsvICVKeyLock(RRQ61X_ACRYPT_BASE, true, true);
		if (ccRc != CC_OK)
		{
			BSVIT_PRINT_ERROR("Failed CC_BsvICVKeyLock - rc = 0x%x\n", ccRc);
			r_cc312_debug_set_errorcode((uint32_t)ccRc);
			rc = BSVIT_ERROR__FAIL;

			goto dbg_fin;
		}

		// CC_BsvICVRMAFlagBitLock
		// prevent the OEM from a one-sided transition to RMA.
		ccRc = CC_BsvICVRMAFlagBitLock(RRQ61X_ACRYPT_BASE);
		if (ccRc != CC_OK)
		{
			BSVIT_PRINT_ERROR("Failed CC_BsvICVRMAFlagBitLock - rc = 0x%x\n", ccRc);
			r_cc312_debug_set_errorcode((uint32_t)ccRc);
			rc = BSVIT_ERROR__FAIL;

			goto dbg_fin;
		}
	}

	BSVIT_PRINT_DBG("CC312_SBOOT_DEBUG:OK\n");

	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32049));

dbg_fin:
	if(pWorkspaceAligned != NULL) CRYPTO_FREE(pWorkspaceAligned);
	if(pCertPkgPtr != NULL) CRYPTO_FREE(pCertPkgPtr);

	SBOOT_BCFM_FINISH(cc321sboot);
	SBOOT_BCFM_PRINT(cc321sboot, __func__ );

	return rc;
}


static BsvItError_t r_cc312_sdebug_fast(uint32_t lcs, HANDLE sflash, uint32_t faddress)
{
	BsvItError_t rc = BSVIT_ERROR__OK;
	CCError_t ccRc = CC_OK;
	uint32_t *pWorkspaceAligned = NULL;
	uint32_t *pCertPkgPtr = NULL;
	uint32_t pkgSize = 0;
	uint32_t rcRmaFlag = 0;
	uint32_t rmaEntrySignalsSet = 0;
	uint32_t  storeFlashAddress;

	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32140));

	pWorkspaceAligned = (uint32_t *)CRYPTO_MALLOC(CC_SB_MIN_WORKSPACE_SIZE_IN_BYTES);
	if( pWorkspaceAligned == NULL ){
		r_cc312_debug_set_errorcode((uint32_t)0);

		return BSVIT_ERROR__FAIL;
	}

	CRYPTO_MEMSET(pWorkspaceAligned, 0, CC_SB_MIN_WORKSPACE_SIZE_IN_BYTES);

	if( sflash != NULL ){
		SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32141));
		storeFlashAddress = (uint32_t)embcrypto_flash_image_certificate(sflash, faddress, 0xFFFFFFFF, (uint32_t *)&pkgSize);
	}else{
		SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC3214B));
		storeFlashAddress = 0;
	}

	if( storeFlashAddress == 0 ){
		rc = BSVIT_ERROR__OK; // Skip
		r_cc312_debug_set_errorcode((uint32_t)MODE_CRY_STEP(0xC32142));
		SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32142));
		pCertPkgPtr = NULL; 			// DCU Test

		goto /*fstdbg_fin*/ fstdbg_nul_cert;	// DCU Test
	}

	pCertPkgPtr = (uint32_t *)CRYPTO_MALLOC(pkgSize);
	if( pCertPkgPtr == NULL ){
		rc = BSVIT_ERROR__FAIL;
		r_cc312_debug_set_errorcode((uint32_t)MODE_CRY_STEP(0xC32143));
		SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32143));

		goto /*fstdbg_fin*/ fstdbg_nul_cert;	// DCU Test
	}

	BSVIT_PRINT_DBG("FstDbgCert: %08x, %d\n", storeFlashAddress, pkgSize);

	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32144));

	embcrypto_flash_image_read( sflash, storeFlashAddress, pCertPkgPtr, pkgSize);

	//////////////////////////////////////////////////////
fstdbg_nul_cert:						// DCU Test
	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32145));

	ccRc = CC_BsvSecureDebugSet(RRQ61X_ACRYPT_BASE,
	                pCertPkgPtr,
	                pkgSize,
	                &rcRmaFlag,
	                pWorkspaceAligned,
	                CC_SB_MIN_WORKSPACE_SIZE_IN_BYTES);

	if (ccRc != CC_OK)
	{
		if( ccRc == CC_BSV_AO_WRITE_FAILED_ERR ){
			BSVIT_PRINT_ERROR("FstDebug: Already locked in CC_BsvSecureDebugSet, ccRc 0x%x\n", ccRc);
			r_cc312_debug_set_errorcode((uint32_t)ccRc);
			rc = BSVIT_ERROR__OK; // Reboot or DPM Boot case
			SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC3214A));
		}else{
			BSVIT_PRINT_ERROR("FstDebug: Failed to CC_BsvSecureDebugSet, ccRc 0x%x\n", ccRc);
			r_cc312_debug_set_errorcode((uint32_t)ccRc);
			rc = BSVIT_ERROR__FAIL;
			SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32146));
		}
		goto fstdbg_fin;
	}else{
		BSVIT_PRINT_DBG("FstDebug: CC_BsvSecureDebugSet succeeded!!\n");
	}

	/* is the provided certificate RMA certificate */
	if( (lcs != CC_BSV_RMA_LCS) && (rcRmaFlag == true) )
	{
		BSVIT_PRINT_DBG("FstDebug: RMA mode enable - %d\n", lcs);
		SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32147));

		ccRc = CC_BsvRMAModeEnable(RRQ61X_ACRYPT_BASE);
		if (ccRc != CC_OK)
		{
			r_cc312_debug_set_errorcode((uint32_t)ccRc);
			rc = BSVIT_ERROR__FAIL;

			goto fstdbg_fin;
		}
	}

	//TODO: rmaEntrySignalsSet = ???;

	if( (lcs != CC_BSV_RMA_LCS) && (rcRmaFlag || rmaEntrySignalsSet ) )
	{
		//TODO: BootROM_RmaModeEntry();
		SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32148));
	}

	rmaEntrySignalsSet = R_CC312_SecureBoot_GetLock2(); // ICV-Lock Test

	if( (lcs == CC_BSV_SECURE_LCS) && (rcRmaFlag != true) && (rmaEntrySignalsSet != 0) ){
		// CC_BsvICVKeyLock, 4-75
		// lock the ICV Keys to prevent the OEM.
		ccRc = CC_BsvICVKeyLock(RRQ61X_ACRYPT_BASE, true, true);
		if (ccRc != CC_OK)
		{
			BSVIT_PRINT_ERROR("FstDebug: Failed CC_BsvICVKeyLock - rc = 0x%x\n", ccRc);
			r_cc312_debug_set_errorcode((uint32_t)ccRc);
			rc = BSVIT_ERROR__FAIL;

			goto fstdbg_fin;
		}

		// CC_BsvICVRMAFlagBitLock
		// prevent the OEM from a one-sided transition to RMA.
		ccRc = CC_BsvICVRMAFlagBitLock(RRQ61X_ACRYPT_BASE);
		if (ccRc != CC_OK)
		{
			BSVIT_PRINT_ERROR("FstDebug: Failed CC_BsvICVRMAFlagBitLock - rc = 0x%x\n", ccRc);
			r_cc312_debug_set_errorcode((uint32_t)ccRc);
			rc = BSVIT_ERROR__FAIL;

			goto fstdbg_fin;
		}
	}

	BSVIT_PRINT_DBG("FstDebug:OK\n");

	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32149));

fstdbg_fin:
	if(pWorkspaceAligned != NULL) CRYPTO_FREE(pWorkspaceAligned);
	if(pCertPkgPtr != NULL) CRYPTO_FREE(pCertPkgPtr);

	return rc;
}


/******************************************************************************
 *  r_cc312_sboot_CM( )
 ******************************************************************************/

static BsvItError_t r_cc312_sboot_CM(uint32_t lcs, uint32_t faddress, uint32_t *jaddress, USR_CALLBACK stopproc)
{
	BsvItError_t rc = BSVIT_ERROR__OK;
	RRQ61X_SBOOT_TYPE *cc321sboot, tcc321sboot;
	uint32_t	jmpaddr;

	cc321sboot = (RRQ61X_SBOOT_TYPE *)&tcc321sboot;

	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC320C1));

	rc = CC312_SBOOT_OPEN(cc321sboot, faddress, stopproc);
	if( rc != BSVIT_ERROR__OK ){
		goto cm_bail;
	}

	// 1. test secure boot and secure debug process
	// DCU Test : Secure Boot after Secure Debug
	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC320C3));

	rc = CC312_SBOOT_DEBUG(lcs, cc321sboot, faddress);
	if( rc != BSVIT_ERROR__OK ){
		goto cm_bail;
	}

	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC320C2));

	rc = CC312_SBOOT_VERIFICATION(cc321sboot, faddress);
	if( rc != BSVIT_ERROR__OK ){
		goto cm_bail;
	}

	// 2. check for the existence of an ICV signature
	//	run ICV tool

	// 3. Load 2nd stage boot loader and continue with cold boot sequece runtime software

cm_bail:
	if( rc == BSVIT_ERROR__OK )
	{
		SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC320C4));

		CC312_SBOOT_CLOSE(cc321sboot, &jmpaddr );

#if     (SUPPORT_BOOT_BRANCH == 1)
		if( (jmpaddr != 0) && (jaddress == NULL) )
		{ /* NOTICE !! do not modify this clause !! */
			volatile uint32_t  dst_addr;
			/* get Reset handler address     */
			dst_addr = *((uint32_t *)((jmpaddr)|0x04));
			/* Jump into App                 */
			dst_addr = (dst_addr|0x01);
			ASM_BRANCH(dst_addr);	/* Jump into App */
		}
#endif  //(SUPPORT_BOOT_BRANCH == 1)
		if( jaddress != NULL ){
			*jaddress = jmpaddr;
		}
	}else{
		SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC320CE));

		CC312_SBOOT_CLOSE(cc321sboot, NULL );
	}

	return rc;
}

/******************************************************************************
 *  r_cc312_sboot_DM( )
 ******************************************************************************/

static BsvItError_t r_cc312_sboot_DM(uint32_t lcs, uint32_t faddress, uint32_t *jaddress, USR_CALLBACK stopproc)
{
	BsvItError_t rc = BSVIT_ERROR__OK;
	RRQ61X_SBOOT_TYPE *cc321sboot, tcc321sboot;
	uint32_t	jmpaddr;

	cc321sboot = (RRQ61X_SBOOT_TYPE *)&tcc321sboot;

	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC320D1));

	// DCU Test : Secure Boot after Secure Debug

	rc = CC312_SBOOT_OPEN(cc321sboot, faddress, stopproc);
	if( rc != BSVIT_ERROR__OK ){
		// DCU Test
		uint32_t	rcRmaFlag = 0;
		R_CC312_SecureBoot_Fatal(&rcRmaFlag);
		goto dm_bail;
	}

	// 1. check secure debug certificate

	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC320D3));

	rc = CC312_SBOOT_DEBUG(lcs, cc321sboot, faddress);
	if( rc != BSVIT_ERROR__OK ){
		goto dm_bail;
	}

	// 2. check secure boot certificate

	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC320D2));

	rc = CC312_SBOOT_VERIFICATION(cc321sboot, faddress);
	if( rc != BSVIT_ERROR__OK ){
		goto dm_bail;
	}

	// 3. check for the existence of an OEM signature
	//	run OEM tool

	// 4. load 2nd stage boot loader and continue with cold boot sequence runtime software

dm_bail:
	if( rc == BSVIT_ERROR__OK )
	{
		SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC320D4));

		CC312_SBOOT_CLOSE(cc321sboot, &jmpaddr );

#if     (SUPPORT_BOOT_BRANCH == 1)
		if( (jmpaddr != 0) && (jaddress == NULL) )
		{ /* NOTICE !! do not modify this clause !! */
			volatile uint32_t  dst_addr;
			/* get Reset handler address     */
			dst_addr = *((uint32_t *)((jmpaddr)|0x04));
			/* Jump into App                 */
			dst_addr = (dst_addr|0x01);
			ASM_BRANCH(dst_addr);	/* Jump into App */
		}
#endif  //(SUPPORT_BOOT_BRANCH == 1)
		if( jaddress != NULL ){
			*jaddress = jmpaddr;
		}
	}else{
		SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC320DE));

		CC312_SBOOT_CLOSE(cc321sboot, NULL );
	}


	return rc;
}

/******************************************************************************
 *  r_cc312_sboot_Secure( )
 ******************************************************************************/

static BsvItError_t r_cc312_sboot_Secure(uint32_t lcs, uint32_t faddress, uint32_t *jaddress, USR_CALLBACK stopproc)
{
	BsvItError_t rc = BSVIT_ERROR__OK;

	RRQ61X_SBOOT_TYPE *cc321sboot, tcc321sboot;
	uint32_t	jmpaddr;

	cc321sboot = (RRQ61X_SBOOT_TYPE *)&tcc321sboot;

	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC320A1));

	// DCU Test : Secure Boot after Secure Debug

	rc = CC312_SBOOT_OPEN(cc321sboot, faddress, stopproc);
	if( rc != BSVIT_ERROR__OK ){
		// DCU Test
		uint32_t	rcRmaFlag = 0;
		R_CC312_SecureBoot_Fatal(&rcRmaFlag);
		goto secure_bail;
	}

	// 1. calculate SOC_ID
	// export the SOC_ID

	// 2. check secure debug certificate
	// RMA may enter here

	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC320A3));

	rc = CC312_SBOOT_DEBUG(lcs, cc321sboot, faddress);
	if( rc != BSVIT_ERROR__OK ){
		goto secure_bail;
	}

	// 3. check secure boot certificate
	//	verify secure boot cert., load trusted code, 2nd stage boot loader or Secure OS, using Secure boot Sequence
	//	Two entities handling (ICV & OEM)

	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC320A2));

	rc = CC312_SBOOT_VERIFICATION(cc321sboot, faddress);
	if( rc != BSVIT_ERROR__OK ){
		goto secure_bail;
	}

secure_bail:
	if( rc == BSVIT_ERROR__OK )
	{
		SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC320A4));

		CC312_SBOOT_CLOSE(cc321sboot, &jmpaddr );

#if     (SUPPORT_BOOT_BRANCH == 1)
		if( (jmpaddr != 0) && (jaddress == NULL) )
		{ /* NOTICE !! do not modify this clause !! */
			volatile uint32_t  dst_addr;
			/* get Reset handler address     */
			dst_addr = *((uint32_t *)((jmpaddr)|0x04));
			/* Jump into App                 */
			dst_addr = (dst_addr|0x01);
			ASM_BRANCH(dst_addr);	/* Jump into App */
		}
#endif  //(SUPPORT_BOOT_BRANCH == 1)
		if( jaddress != NULL ){
			*jaddress = jmpaddr;
		}
	}else{
		SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC320AE));

		CC312_SBOOT_CLOSE(cc321sboot, NULL );
	}


	return rc;
}

/******************************************************************************
 *  r_cc312_sboot_RMA( )
 ******************************************************************************/

static BsvItError_t r_cc312_sboot_RMA(uint32_t lcs, uint32_t faddress, uint32_t *jaddress, USR_CALLBACK stopproc)
{
	BsvItError_t rc = BSVIT_ERROR__OK;

	RRQ61X_SBOOT_TYPE *cc321sboot, tcc321sboot;
	uint32_t	jmpaddr;

	cc321sboot = (RRQ61X_SBOOT_TYPE *)&tcc321sboot;

	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC320E1));

	// 1. OEM secure debug cerificate issue RMA
	// 3. ICV secure debug cerificate issue RMA

	rc = CC312_SBOOT_OPEN(cc321sboot, faddress, stopproc);
	if( rc != BSVIT_ERROR__OK ){
		goto rma_bail;
	}

	// DCU Test : Secure Boot after Secure Debug

	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC320E3));

	rc = CC312_SBOOT_DEBUG(lcs, cc321sboot, faddress);
	if( rc != BSVIT_ERROR__OK ){
		goto rma_bail;
	}

	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC320E2));

	rc = CC312_SBOOT_VERIFICATION(cc321sboot, faddress);
	if( rc != BSVIT_ERROR__OK ){
		goto rma_bail;
	}

rma_bail:
	if( rc == BSVIT_ERROR__OK )
	{
		SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC320E4));

		CC312_SBOOT_CLOSE(cc321sboot, &jmpaddr );

		// 2. call CC_BsvRMAModeEnable() and POR
		// 4. call CC_BsvRMAModeEnable() and POR

		CC_BsvRMAModeEnable(RRQ61X_ACRYPT_BASE);

		// TODO: Unsecure ??

#if     (SUPPORT_BOOT_BRANCH == 1)
		if( (jmpaddr != 0) && (jaddress == NULL) )
		{ /* NOTICE !! do not modify this clause !! */
			volatile uint32_t  dst_addr;
			/* get Reset handler address     */
			dst_addr = *((uint32_t *)((jmpaddr)|0x04));
			/* Jump into App                 */
			dst_addr = (dst_addr|0x01);
			ASM_BRANCH(dst_addr);	/* Jump into App */
		}else
#endif  //(SUPPORT_BOOT_BRANCH == 1)
		if( (jmpaddr != 0) && jaddress != NULL ){
			*jaddress = jmpaddr;
		}

	}else{
		SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC320EE));

		CC312_SBOOT_CLOSE(cc321sboot, NULL );
	}

	return rc;
}

/******************************************************************************
 *  R_CC312_SecureBoot_Fatal( ) : Test only
 ******************************************************************************/

uint32_t	R_CC312_SecureBoot_Fatal(uint32_t *rcRmaFlag)
{
	uint32_t rc;
	uint32_t *pWorkspaceAligned;

	pWorkspaceAligned = (uint32_t *)CRYPTO_MALLOC(CC_SB_MIN_WORKSPACE_SIZE_IN_BYTES);

	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC320F1));

	rc = CC_BsvFatalErrorSet(RRQ61X_ACRYPT_BASE);
	if( rc != 0 ){
		SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC320F2));
		goto fatal_bail;
	}

	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC320F3));

	rc = CC_BsvSecureDebugSet(RRQ61X_ACRYPT_BASE,
	                NULL, 0, (uint32_t *)rcRmaFlag
	                , pWorkspaceAligned, CC_SB_MIN_WORKSPACE_SIZE_IN_BYTES);
	if( rc != 0 ){
		SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC320F4));
		goto fatal_bail;
	}

fatal_bail:
	CRYPTO_FREE(pWorkspaceAligned);

	return (uint32_t)rc;
}

/******************************************************************************
 *  R_CC312_SecureBoot_Fatal( ) : Test only
 ******************************************************************************/

static BsvItError_t r_cc312_DeviceCompleteDisable(void)
{
	BsvItError_t rc = BSVIT_ERROR__OK;
	CCError_t ccRc;

	ccRc = CC_DeviceCompleteDisable(RRQ61X_ACRYPT_BASE);

	if( ccRc == CC_BSV_AO_WRITE_FAILED_ERR ){
		rc = BSVIT_ERROR__OK; // Reboot or DPM Boot case
		SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC321FA));
	}else{
		rc = BSVIT_ERROR__FAIL;
		SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC321F6));
	}

	return rc;
}

/******************************************************************************
 *  R_CC312_SecureBoot_CMPU( )
 ******************************************************************************/

#define HUK_RA6W1_HIDDEN

uint32_t	R_CC312_SecureBoot_CMPU(uint8_t *pCmpuData, uint32_t rflag)
{
	uint32_t *workspace;
	CCError_t status;

	workspace = (uint32_t *)CRYPTO_MALLOC(CMPU_WORKSPACE_MINIMUM_SIZE);

	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32090));

#ifdef HUK_RA6W1_HIDDEN
	if( rflag != 0  && rflag == 0xDA3CC312){
                // for VSIM
                int i;
                const uint8_t rngpattern[] = {
  0x12, 0xC3, 0x9C, 0xFC, 0x46, 0x64, 0x64, 0x64, 0xAD, 0x2D, 0x49, 0x59, 0x92, 0xD4, 0x5A, 0xDA, 0xB2, 0x35, 0x2D, 0xD3, 0x96, 0xB5, 0xAC, 0x65, 0x45, 0x4A, 0x49, 0x59, 0x5A, 0x4A, 0x2B, 0x69
, 0xD2, 0x9E, 0xB4, 0xB6, 0xD6, 0x96, 0x96, 0x95, 0x2D, 0x6D, 0x25, 0x69, 0x5B, 0xB6, 0x94, 0x56, 0xD2, 0x94, 0xA5, 0xA5, 0xB4, 0xA5, 0x24, 0x49, 0xA5, 0x25, 0x29, 0x29, 0x49, 0x4A, 0x53, 0xB6
, 0x48, 0x92, 0x92, 0xB6, 0xB4, 0x25, 0x25, 0x6D, 0x29, 0xA9, 0x25, 0x6D, 0xAD, 0x94, 0x4A, 0xD2, 0xDA, 0xD2, 0x8A, 0x52, 0xDB, 0x34, 0x2D, 0x6B, 0x5B, 0x2B, 0xA9, 0xAD, 0xB4, 0x35, 0x29, 0x25
, 0x6D, 0x52, 0x5A, 0x6A, 0x4B, 0x52, 0x5A, 0x13, 0x4A, 0xD3, 0x96, 0x96, 0xD4, 0x5A, 0x4B, 0x5B, 0xAD, 0x24, 0x6D, 0x29, 0x49, 0x4B, 0xD2, 0x92, 0x52, 0x4A, 0x4A, 0x49, 0x6B, 0x5B, 0x2B, 0x25
, 0xAC, 0xA5, 0x6C, 0x69, 0x4B, 0x4B, 0x9B, 0x52, 0x52, 0x92, 0x92, 0x69, 0xE9, 0xA5, 0x6D, 0x4B, 0x4B, 0x4B, 0x53, 0x5B, 0x4B, 0x2D, 0xCD, 0xDA, 0x49, 0x49, 0x4A, 0x5B, 0x9B, 0xDA, 0xB6, 0x25
, 0xAC, 0xB5, 0x6D, 0x2D, 0x29, 0x69, 0x2D, 0x25, 0x6B, 0x6B, 0xDA, 0xD4, 0x90, 0xD6, 0xB5, 0x96, 0xAD, 0x6D, 0x25, 0xAD, 0x5B, 0x29, 0x69, 0x49, 0x2D, 0xAD, 0xAD, 0x6D, 0x69, 0x2B, 0x49, 0x9B
, 0xB4, 0xB4, 0x52, 0xD2, 0xD2, 0xDA, 0x92, 0x92, 0x49, 0x5B, 0x2D, 0x6D, 0x4D, 0x2D, 0x2D, 0x53, 0xB2, 0x9A, 0xB4, 0xA5, 0x64, 0x65, 0xAD, 0xAD, 0x2D, 0x2D, 0x25, 0x6D, 0x49, 0x49, 0x6A, 0x4B
, 0x29, 0x49, 0x6A, 0x2D, 0x2B, 0x69, 0x69, 0x7B, 0x53, 0xB2, 0xB5, 0x95, 0xB4, 0x24, 0x2D, 0xDB, 0x5A, 0xDA, 0xD2, 0xDA, 0xDA, 0xAC, 0xB6, 0x92, 0x4B, 0x5A, 0x92, 0xB6, 0xB5, 0xAC, 0xC9, 0x52
, 0x4A, 0xDA, 0xB6, 0x95, 0xB2, 0xB6, 0x35, 0x6D, 0x49, 0x4A, 0x4B, 0x49, 0x65, 0xCB, 0xD2, 0x5A, 0xB6, 0xA4, 0xD2, 0x66, 0xDB, 0xDE, 0x56, 0x52, 0x2A, 0x2B, 0x65, 0x4D, 0x52, 0xDB, 0x8A, 0x2C
, 0xA5, 0x92, 0xD4, 0xB4, 0xDA, 0x36, 0xA9, 0x69, 0x66, 0x4B, 0x63, 0x69, 0x4D, 0x2A, 0x6B, 0x59, 0x4B, 0x5A, 0x52, 0xDA, 0x96, 0xB4, 0xD6, 0xD2, 0xA6, 0xB6, 0xB6, 0xAC, 0xB4, 0x92, 0xA4, 0x24
, 0x52, 0x92, 0x94, 0xD4, 0xB6, 0xB4, 0x6D, 0x49, 0xCB, 0x96, 0xA4, 0x94, 0x96, 0xA5, 0xB6, 0x64, 0x6B, 0x29, 0x6D, 0x6D, 0xD9, 0xB4, 0x94, 0xA6, 0xB5, 0x2C, 0x25, 0x6D, 0x5B, 0x6D, 0xDA, 0x4A
, 0x4A, 0x5A, 0x46, 0x49, 0x4D, 0x5A, 0x53, 0x92, 0x52, 0xB6, 0x65, 0xA5, 0xAD, 0xB5, 0xA4, 0xAC, 0x4B, 0xDA, 0x92, 0xB6, 0xB4, 0x52, 0xD6, 0x94, 0xB6, 0x4C, 0x5B, 0x56, 0x93, 0xB4, 0xD5, 0xD4
, 0xD2, 0xD6, 0x9A, 0xD6, 0x5A, 0x4B, 0xDB, 0xD4, 0x49, 0x2B, 0x2B, 0x6D, 0x4A, 0x49, 0xC9, 0xDA, 0x96, 0x96, 0x92, 0xB4, 0x6D, 0xAD, 0xA5, 0x24, 0xA5, 0xB5, 0x94, 0x25, 0xA5, 0xA5, 0x94, 0x34
, 0x6D, 0x49, 0x4B, 0x49, 0x49, 0xD6, 0xD3, 0xDA, 0x32, 0x59, 0x4A, 0x4B, 0xDB, 0xD6, 0x72, 0xDA, 0x96, 0xA6, 0xA5, 0xA5, 0xAC, 0x95, 0xD4, 0xDA, 0x25, 0x2D, 0x6D, 0xD9, 0x4A, 0x5D, 0x92, 0xB6
, 0xA4, 0xD4, 0xB6, 0x92, 0xD4, 0xA6, 0x6D, 0xA5, 0xA4, 0x4C, 0x5A, 0x52, 0x9B, 0xB2, 0xD6, 0xB6, 0x6D, 0xCB, 0xDA, 0xA6, 0xA5, 0x2C, 0xB1, 0x25, 0x25, 0x25, 0x69, 0x6B, 0x4A, 0xD6, 0xB6, 0x2D
, 0xA9, 0xA5, 0xA4, 0x95, 0xA4, 0xB4, 0xB5, 0x5A, 0x49, 0x49, 0x69, 0x4B, 0xD2, 0xDA, 0x4A, 0x52, 0xDA, 0x92, 0xD6, 0x66, 0x4D, 0xAB, 0x29, 0x65, 0x6B, 0x29, 0x69, 0x2D, 0xA5, 0xAD, 0x25, 0x93
, 0x2D, 0x69, 0x49, 0x25, 0x6B, 0x5B, 0x4B, 0x6B, 0xD9, 0xB2, 0x92, 0x96, 0xB2, 0xD6, 0xB4, 0x6D, 0x2D, 0x6B, 0x49, 0x6D, 0x9E, 0xA6, 0xB5, 0xA4, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x39, 0x3E, 0x35
, 0x2E, 0x2E, 0x2E, 0x2E, 0x51, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA8, 0x27, 0x02, 0x00, 0xC8, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

		workspace[0] = (uint32_t)0xFC9CC312; // hidden tag
		workspace[1] = (uint32_t)0xDA3CC312; // 2nd hidden tag

                for( i = 0; i < (304/4); i++ ){
                        workspace[2+i] = ((uint32_t *)rngpattern)[i];
                }
	}else
#endif //HUK_RA6W1_HIDDEN	
	if( rflag != 0 ){
		workspace[0] = (uint32_t)0xFC9CC312; // hidden tag
		workspace[1] = (uint32_t)rflag;
	}

	status = CCProd_Cmpu(RRQ61X_ACRYPT_BASE
		, (CCCmpuData_t *)pCmpuData
		, (unsigned long)workspace
		, CMPU_WORKSPACE_MINIMUM_SIZE);

	CRYPTO_FREE(workspace);

	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32091));

	//TEST : Test_HalPerformPowerOnReset();
	r_cc312_debug_set_errorcode((uint32_t)status);

	return ((status == CC_OK)? true:false);
}

/******************************************************************************
 *  R_CC312_SecureBoot_DMPU( )
 ******************************************************************************/

uint32_t	R_CC312_SecureBoot_DMPU(uint8_t *pDmpuData)
{
	uint32_t *workspace;
	CCError_t status;

	workspace = (uint32_t *)CRYPTO_MALLOC(DMPU_WORKSPACE_MINIMUM_SIZE);

	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32092));

	status = CCProd_Dmpu(RRQ61X_ACRYPT_BASE
		, (CCDmpuData_t *)pDmpuData
		, (unsigned long)workspace
		, DMPU_WORKSPACE_MINIMUM_SIZE);

	CRYPTO_FREE(workspace);

	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32093));

	//TEST : Test_HalPerformPowerOnReset();
	r_cc312_debug_set_errorcode((uint32_t)status);

	return ((status == CC_OK)? true:false);
}

#ifdef	BUILD_OPT_RRQ61X_FPGA
void	R_CC312_SecureBoot_ColdReset(void)
{
	SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32C10)|0x0D);
	//200918: Test_HalPerformPowerOnReset(); // FPGA Test
}
#endif //BUILD_OPT_RRQ61X_FPGA

/******************************************************************************
 *  R_CC312_SecureBoot_GetLock( )
 ******************************************************************************/

#define WRITE_ENV(offset, val) { \
	volatile uint32_t ii1; \
        (*(volatile uint32_t *)(DX_BASE_ENV_REGS + (offset))) = (uint32_t)(val); \
        for(ii1=0; ii1<500; ii1++);\
}

#define READ_ENV(offset) \
	*(volatile uint32_t *)(DX_BASE_ENV_REGS + (offset))


uint32_t R_CC312_SecureBoot_GetLock(void)
{
	uint32_t OtpWord;

	CC_BsvOTPWordRead(RRQ61X_ACRYPT_BASE, CC_OTP_SB_LOADER_CODE_OFFSET, &OtpWord );

	return (((OtpWord & 0x01) == 0x01) ? true : false );
}

uint32_t R_CC312_SecureBoot_SetLock(void)
{
	uint32_t error = 0;

	uint32_t OtpWord /*, envflag*/;

	OtpWord = 0x01;

	//envflag = READ_ENV(DX_ENV_OTP_FILTER_OFF_REG_OFFSET);
	//WRITE_ENV(DX_ENV_OTP_FILTER_OFF_REG_OFFSET , 0x1UL);

	error = CC_BsvOTPWordWrite(RRQ61X_ACRYPT_BASE, CC_OTP_SB_LOADER_CODE_OFFSET, OtpWord);

	//WRITE_ENV(DX_ENV_OTP_FILTER_OFF_REG_OFFSET , envflag);
	r_cc312_debug_set_errorcode((uint32_t)error);

	return ((error == CC_OK) ? true : false );
}

void R_CC312_SecureBoot_OTPLock(uint32_t mode)
{

	//WRITE_ENV(DX_ENV_OTP_FILTER_OFF_REG_OFFSET , mode);

}

uint32_t R_CC312_SecureBoot_SecureLCS(void)
{
	uint32_t lcs;

	CC_BsvLcsGet(RRQ61X_ACRYPT_BASE, &lcs);

	return ((lcs == CC_BSV_SECURE_LCS) ? true : false);
}

/******************************************************************************
 * New Secure Model : ICV-Lock Test
 ******************************************************************************/

uint32_t R_CC312_SecureBoot_GetLock2(void)
{
	uint32_t OtpWord;

	CC_BsvOTPWordRead(RRQ61X_ACRYPT_BASE, CC_OTP_SB_LOADER_CODE_OFFSET, &OtpWord );

	return (((OtpWord & 0x03) == 0x03) ? true : false );
}

uint32_t R_CC312_SecureBoot_SetLock2(void)
{
	uint32_t error = 0;

	uint32_t OtpWord /*, envflag*/;

	OtpWord = 0x02;

	//envflag = READ_ENV(DX_ENV_OTP_FILTER_OFF_REG_OFFSET);
	//WRITE_ENV(DX_ENV_OTP_FILTER_OFF_REG_OFFSET , 0x1UL);

	error = CC_BsvOTPWordWrite(RRQ61X_ACRYPT_BASE, CC_OTP_SB_LOADER_CODE_OFFSET, OtpWord);

	//WRITE_ENV(DX_ENV_OTP_FILTER_OFF_REG_OFFSET , envflag);
	r_cc312_debug_set_errorcode((uint32_t)error);

	return ((error == CC_OK) ? true : false );
}

/******************************************************************************
 *  R_CC312_Secure_Asset( )
 ******************************************************************************/

uint32_t R_CC312_Secure_Asset(uint32_t Owner, uint32_t AssetID
		, uint32_t *InAssetData, uint32_t AssetSize
		, uint32_t *OutAssetData, uint32_t *OutAssetSize)
{
	CCError_t status;

	status = mbedtls_util_asset_pkg_unpack(
			(CCAssetProvKeyType_t)Owner,
			(uint32_t)AssetID,
			(uint32_t *)InAssetData,
			(size_t)AssetSize,
			(uint32_t *)OutAssetData,
			(size_t *)OutAssetSize );

	if (status != CC_OK){
		r_cc312_debug_set_errorcode((uint32_t)status);

		return 0; // zero size
	}

	return (*OutAssetSize);
}

/******************************************************************************
 *  R_CC312_Secure_Asset_RuntimePack( )
 ******************************************************************************/
#include "cc_hal_plat.h"
#include "cc_regs.h"
#include "cc_pal_mem.h"
#include "cc_pal_mutex.h"
#include "cc_pal_abort.h"
#include "cc_util_int_defs.h"
#include "mbedtls_cc_util_defs.h"
#include "cc_util_error.h"
#include "cc_aes_defs.h"
#include "mbedtls/ccm.h" 
#include "aes_driver.h"
#include "driver_defs.h"
#include "cc_util_cmac.h"
#include "mbedtls_cc_util_asset_prov.h"
#include "cc_util_asset_prov_int.h"
#include "prod_util.h"
#include "cmpu_llf_rnd.h"
#include "prod_crypto_driver.h"
#include "cmpu_derivation.h"

typedef struct {
        uint32_t  token;
        uint32_t  version;
        uint32_t  assetSize;
        uint32_t  reserved[CC_ASSET_PROV_RESERVED_WORD_SIZE];
        uint8_t   nonce[CC_ASSET_PROV_NONCE_SIZE];
        uint8_t   enctag[CC_ASSET_PROV_TAG_SIZE];
} CCRunAssetProvPkg_t;

int32_t R_CC312_Secure_Asset_RuntimePack(AssetKeyType_t KeyType, uint32_t noncetype
		, AssetUserKeyData_t *KeyData, uint32_t AssetID, char *title
		, uint8_t *InAssetData, uint32_t AssetSize, uint8_t *OutAssetPkgData)
{
	uint32_t  rc = CC_OK;
	uint32_t  i;
	CCUtilAesCmacResult_t         keyProv = { 0 };
	uint8_t     dataIn[CC_UTIL_MAX_KDF_SIZE_IN_BYTES] = { 0 };
	uint32_t    dataInSize = CC_UTIL_MAX_KDF_SIZE_IN_BYTES;
	uint8_t     provLabel = 'P';
	CCRunAssetProvPkg_t * pAssetPackage = NULL;
	mbedtls_ccm_context   ccmCtx;
	uint8_t	            * pencRunAsset;

	/* Validate Inputs */
	if ((InAssetData == NULL) ||
		(OutAssetPkgData == NULL) ||
		/*(AssetSize > CC_ASSET_PROV_MAX_ASSET_SIZE) ||*/
		(AssetSize == 0) ||
		((AssetSize % CC_ASSET_PROV_BLOCK_SIZE) != 0) ||
		(((UtilKeyType_t) KeyType) > UTIL_KCEICV_KEY) )
	{
		CC_PAL_LOG_ERR("Invalid params");
		return (int32_t) CC_UTIL_ILLEGAL_PARAMS_ERROR;

	}

	rc = CCProd_Init();
	if (rc != CC_OK) {
			CC_PAL_LOG_ERR("Failed to CCProd_Init 0x%x\n", rc);
			goto cc312_sa_pack_end_raw;
	}

	pAssetPackage = (CCRunAssetProvPkg_t *) OutAssetPkgData;
	pencRunAsset  = (uint8_t *) (OutAssetPkgData+ sizeof(CCRunAssetProvPkg_t));

	/* fill asset size, must be multiply of 16 bytes */
	pAssetPackage->assetSize = AssetSize;

	/* fill package token and version */
    pAssetPackage->token   = CC_RUNASSET_PROV_TOKEN;
    pAssetPackage->version = CC_RUNASSET_PROV_VERSION;

	if( title != NULL ){
		CRYPTO_MEMCPY(pAssetPackage->reserved, title
			, (CC_ASSET_PROV_RESERVED_WORD_SIZE*sizeof(uint32_t)) );
	}

	/* Generate dataIn buffer for CMAC: iteration || 'P' || 0x00 || asset Id || 0x80
		since deruved key is 128 bits we have only 1 iteration */
	rc = UtilCmacBuildDataForDerivation(&provLabel,sizeof(provLabel),
	                              (uint8_t *) &AssetID, sizeof(AssetID),
	                             dataIn, (size_t *) &dataInSize,
	                             (size_t)CC_UTIL_AES_CMAC_RESULT_SIZE_IN_BYTES);
	if (rc != 0) {
		CC_PAL_LOG_ERR("Failed UtilCmacBuildDataForDerivation 0x%x", rc);
		CRYPTO_MEMSET(pAssetPackage, 0, sizeof(CCRunAssetProvPkg_t));
		goto cc312_sa_pack_end_raw;
	}
	dataIn[0] = 1;  // only 1 iteration
	rc = UtilCmacDeriveKey(((UtilKeyType_t) KeyType),
				((CCAesUserKeyData_t *) KeyData),
				dataIn, dataInSize,
				keyProv);
	if (rc != 0) {
		CC_PAL_LOG_ERR("Failed UtilCmacDeriveKey 0x%x", rc);
		CRYPTO_MEMSET(pAssetPackage, 0, sizeof(CCRunAssetProvPkg_t));
		goto cc312_sa_pack_end_raw;
	}

	/* Decrypt and authenticate the BLOB */
	mbedtls_ccm_init(&ccmCtx);

	rc = mbedtls_ccm_setkey(&ccmCtx, MBEDTLS_CIPHER_ID_AES, keyProv, CC_UTIL_AES_CMAC_RESULT_SIZE_IN_BYTES * CC_BITS_IN_BYTE);
	if (rc != 0) {
		CC_PAL_LOG_ERR("Failed to mbedtls_ccm_setkey 0x%x\n", rc);
		CRYPTO_MEMSET(pAssetPackage, 0, sizeof(CCRunAssetProvPkg_t));
		goto cc312_sa_pack_end;
	}

	if( noncetype == 0xFFFFFFFF ){
		for(i = 0; i < CC_ASSET_PROV_NONCE_SIZE; i++ ){
			pAssetPackage->nonce[i] = (uint8_t) embcrypto_random();
		}
	}else{
		uint8_t   *pKey;
		uint8_t   *pIv;
		uint32_t *pEntrSrc;
		uint32_t  sourceSize;
		uint32_t *pRndWorkBuff;

		pRndWorkBuff = (uint32_t *) CRYPTO_MALLOC(CMPU_WORKSPACE_MINIMUM_SIZE);
		pKey = (uint8_t *) CRYPTO_MALLOC(CC_PROD_AES_Key256Bits_SIZE_IN_BYTES);
		pIv  = (uint8_t *) CRYPTO_MALLOC(CC_PROD_AES_IV_COUNTER_SIZE_IN_BYTES);

		if( (pRndWorkBuff == NULL) || (pKey == NULL) || (pIv == NULL) ){
			if( pRndWorkBuff != NULL ){
				CRYPTO_FREE(pRndWorkBuff);
			}
			if( pKey != NULL ){
				CRYPTO_FREE(pKey);
			}
			if( pIv != NULL ){
				CRYPTO_FREE(pIv);
			}
			rc = CC_UTIL_FATAL_ERROR;
			goto cc312_sa_pack_end;
		}

		pRndWorkBuff[0] = (uint32_t)0xFC9CC312; // hidden tag
		pRndWorkBuff[1] = (uint32_t)noncetype;

	        rc = CC_PROD_LLF_RND_GetTrngSource((uint32_t **) &pEntrSrc, &sourceSize, pRndWorkBuff);
	        if (rc != CC_OK) {
	                CC_PAL_LOG_ERR("failed CC_PROD_LLF_RND_GetTrngSource, error is 0x%X\n", rc);
	                goto cc312_sa_pack_end_free;
	        }
	        rc = CC_PROD_Derivation_Instantiate(pEntrSrc,
					sourceSize,
					pKey,
					pIv);
	        if (rc != CC_OK) {
	                CC_PAL_LOG_ERR("failed to CC_PROD_Derivation_Instantiate, error 0x%x\n", rc);
	                goto cc312_sa_pack_end_free;
	        }
	        rc = CC_PROD_Derivation_Generate(pKey,
					pIv,
					(uint32_t *) (pRndWorkBuff),
					(CC_ASSET_PROV_NONCE_SIZE*2));
	        if (rc != CC_OK) {
	                CC_PAL_LOG_ERR("failed to CC_PROD_Derivation_Generate, error 0x%x\n", rc);
	                goto cc312_sa_pack_end_free;
	        }

	        rc = CC_PROD_LLF_RND_VerifyGeneration((uint8_t *) (pRndWorkBuff));
	        if (rc != CC_OK) {
	                CC_PAL_LOG_ERR("failed to CC_PROD_LLF_RND_VerifyGeneration, error 0x%x\n", rc);
	                goto cc312_sa_pack_end_free;
	        }

		CRYPTO_MEMCPY((pAssetPackage->nonce), pRndWorkBuff, CC_ASSET_PROV_NONCE_SIZE);

cc312_sa_pack_end_free:
		CRYPTO_FREE(pRndWorkBuff);
		CRYPTO_FREE(pKey);
		CRYPTO_FREE(pIv);

		if( rc != CC_OK){
			goto cc312_sa_pack_end;
		}
	}

	rc = mbedtls_ccm_encrypt_and_tag(&ccmCtx, pAssetPackage->assetSize,
			pAssetPackage->nonce, CC_ASSET_PROV_NONCE_SIZE,
			(uint8_t *) pAssetPackage, CC_ASSET_PROV_ADATA_SIZE,
			(uint8_t *) InAssetData, pencRunAsset,
			pAssetPackage->enctag, CC_ASSET_PROV_TAG_SIZE);


	if (rc != 0) {
		CC_PAL_LOG_ERR("Failed to mbedtls_ccm_auth_decrypt 0x%x\n", rc);
		CRYPTO_MEMSET(pAssetPackage, 0, sizeof(CCRunAssetProvPkg_t));
		goto cc312_sa_pack_end;
	}

	rc = (sizeof(CCRunAssetProvPkg_t) + pAssetPackage->assetSize);

cc312_sa_pack_end:
	mbedtls_ccm_free( &ccmCtx );
cc312_sa_pack_end_raw:
	CCPROD_Fini();

	// Set output data
	return (int32_t)rc;
}

/******************************************************************************
 *  R_CC312_Secure_Asset_RuntimeUnpack( )
 ******************************************************************************/

int32_t R_CC312_Secure_Asset_RuntimeUnpack(AssetKeyType_t KeyType
        , AssetUserKeyData_t * KeyData, uint32_t AssetID
        , uint8_t * InAssetPkgData, uint32_t AssetPkgSize, uint8_t * OutAssetData)
{
    CCUtilAesCmacResult_t   keyProv       = {0};
    CCRunAssetProvPkg_t   * pAssetPackage = NULL;
    mbedtls_ccm_context     ccmCtx;
    uint32_t                rc                                    = CC_OK;
    uint32_t                dataInSize                            = CC_UTIL_MAX_KDF_SIZE_IN_BYTES;
    uint8_t                 dataIn[CC_UTIL_MAX_KDF_SIZE_IN_BYTES] = {0};
    uint8_t                 provLabel                             = 'P';
    uint8_t               * pencRunAsset;
    bool                    sbstyle = 0;

    /* Validate Inputs */
    if ((InAssetPkgData == NULL) ||
        (OutAssetData == NULL) ||
        (((UtilKeyType_t)KeyType) > UTIL_KCEICV_KEY) )
    {
        CC_PAL_LOG_ERR("Invalid params");
        return (int32_t)CC_UTIL_ILLEGAL_PARAMS_ERROR;
    }

    rc = CCProd_Init();
    if (rc != CC_OK) {
            CC_PAL_LOG_ERR("Failed to CCProd_Init 0x%x\n", rc);
            return (int32_t)rc;
    }

    pAssetPackage = (CCRunAssetProvPkg_t *) InAssetPkgData;
    pencRunAsset  = (uint8_t *) (InAssetPkgData + sizeof(CCRunAssetProvPkg_t));

    /* Validate asset size, must be multiply of 16 bytes */
    if ((pAssetPackage->assetSize == 0) ||
        (pAssetPackage->assetSize % CC_ASSET_PROV_BLOCK_SIZE)) {
        CC_PAL_LOG_ERR("Invalid asset size 0x%x", pAssetPackage->assetSize);
        return (int32_t)CC_UTIL_ILLEGAL_PARAMS_ERROR;
    }

    /* Verify package token and version */
    if ((pAssetPackage->token != CC_RUNASSET_PROV_TOKEN) ||
        (pAssetPackage->version != CC_RUNASSET_PROV_VERSION)) {
        CC_PRINTF("sbstyle =1\n");
        sbstyle = 1;
    }

    /* Generate dataIn buffer for CMAC: iteration || 'P' || 0x00 || asset Id || 0x80
        since deruved key is 128 bits we have only 1 iteration */
    rc = UtilCmacBuildDataForDerivation(&provLabel, sizeof(provLabel),
                                        (uint8_t *) &AssetID, sizeof(AssetID),
                                        dataIn, (size_t *) &dataInSize,
                                        (size_t) CC_UTIL_AES_CMAC_RESULT_SIZE_IN_BYTES);
    if (rc != 0) {
        CC_PAL_LOG_ERR("Failed UtilCmacBuildDataForDerivation 0x%x", rc);
        return rc;
    }

    dataIn[0] = 1;  // only 1 iteration
    rc = UtilCmacDeriveKey(((UtilKeyType_t) KeyType),
                           ((CCAesUserKeyData_t *) KeyData),
                           dataIn, dataInSize,
                           keyProv);
    if (rc != 0) {
        CC_PAL_LOG_ERR("Failed UtilCmacDeriveKey 0x%x", rc);
        return rc;
    }

    CC_PRINTF("keyProv: ");
    for (int i = 0; i < CC_UTIL_AES_CMAC_RESULT_SIZE_IN_BYTES; i++)
    {
        CC_PRINTF("%02X", keyProv[i]);
    }
    CC_PRINTF("\n");

    /* Decrypt and authenticate the BLOB */
    mbedtls_ccm_init(&ccmCtx);

    rc = mbedtls_ccm_setkey(&ccmCtx,
                            MBEDTLS_CIPHER_ID_AES, keyProv, CC_UTIL_AES_CMAC_RESULT_SIZE_IN_BYTES * CC_BITS_IN_BYTE);
    if (rc != 0) {
        mbedtls_ccm_free(&ccmCtx);
        CC_PAL_LOG_ERR("Failed to mbedtls_ccm_setkey 0x%x\n", rc);
        return rc;
    }

    if (sbstyle == 0)
    {
        rc = mbedtls_ccm_auth_decrypt(&ccmCtx, pAssetPackage->assetSize,
                    pAssetPackage->nonce, CC_ASSET_PROV_NONCE_SIZE,
                    (uint8_t *) pAssetPackage, CC_ASSET_PROV_ADATA_SIZE,
                    pencRunAsset, OutAssetData,
                    pAssetPackage->enctag, CC_ASSET_PROV_TAG_SIZE);
    }else{
        if (0 == strncmp((char *) pAssetPackage->reserved, "RunPack", sizeof("RunPack")-1))
        {
            rc = mbedtls_ccm_auth_decrypt(&ccmCtx,
                                            pAssetPackage->assetSize,
                                            pAssetPackage->nonce,
                                            CC_ASSET_PROV_NONCE_SIZE,
                                            (uint8_t *) pAssetPackage,
                                            CC_ASSET_PROV_ADATA_SIZE,
                                            pencRunAsset,
                                            OutAssetData,
                                            pAssetPackage->enctag,
                                            CC_ASSET_PROV_TAG_SIZE);
        }
        else
        {
            rc = mbedtls_ccm_auth_decrypt(&ccmCtx,
                                            pAssetPackage->assetSize,
                                            pAssetPackage->nonce,
                                            CC_ASSET_PROV_NONCE_SIZE,
                                            (uint8_t *) pAssetPackage,
                                            CC_ASSET_PROV_ADATA_SIZE,
                                            pAssetPackage->enctag,
                                            OutAssetData,
                                            pAssetPackage->enctag + pAssetPackage->assetSize,
                                            CC_ASSET_PROV_TAG_SIZE);
        }
    }

    if (rc != 0) {
        mbedtls_ccm_free(&ccmCtx);
        CC_PAL_LOG_ERR("Failed to mbedtls_ccm_auth_decrypt 0x%x\n", rc);
        return rc;
    }

    mbedtls_ccm_free(&ccmCtx);

    /* Set output data */
    return (pAssetPackage->assetSize);
}

/******************************************************************************
 *  R_CC312_CalcHuk_Test( )
 ******************************************************************************/

uint32_t R_CC312_CalcHuk_Test(uint8_t *HUKBuffer, uint32_t *HUKsize)
{
    uint32_t   error = 0;
    uint32_t   zeroCount = 0;
    uint8_t    pKey[CC_PROD_AES_Key256Bits_SIZE_IN_BYTES] = { 0 };
    uint8_t    pIv[CC_PROD_AES_IV_COUNTER_SIZE_IN_BYTES] = { 0 };
    uint32_t * pEntrSrc;
    uint32_t   sourceSize;
    uint32_t * pRndWorkBuff;
    uint32_t * pBuffForOtp;

    *HUKsize = 0;
    pBuffForOtp = (uint32_t *) HUKBuffer;
    /*Call CC_PROD_LLF_RND_GetTrngSource to get entropy bits and to check entropy size*/
    pRndWorkBuff = (uint32_t *) CRYPTO_MALLOC(CMPU_WORKSPACE_MINIMUM_SIZE);

    CCProd_Init();

    error = CC_PROD_LLF_RND_GetTrngSource((uint32_t **) &pEntrSrc, &sourceSize, pRndWorkBuff);
    if (error != CC_OK) {
            CRYPTO_PRINTF("failed CC_PROD_LLF_RND_GetTrngSource, error is 0x%X\n", error);
            goto r_cc312_calchuk_test_done;
    }

    error = CC_PROD_Derivation_Instantiate(pEntrSrc,
                                           sourceSize,
                                           pKey,
                                           pIv);
    if (error != CC_OK) {
            CRYPTO_PRINTF("failed to CC_PROD_Derivation_Instantiate, error 0x%x\n", error);
            goto r_cc312_calchuk_test_done;
    }

    error = CC_PROD_Derivation_Generate(pKey,
                                        pIv,
                                        pBuffForOtp,
                                        CC_OTP_HUK_SIZE_IN_WORDS * sizeof(uint32_t));
    if (error != CC_OK) {
            CRYPTO_PRINTF("failed to CC_PROD_Derivation_Generate, error 0x%x\n", error);
            goto r_cc312_calchuk_test_done;
    }

    error = CC_PROD_LLF_RND_VerifyGeneration((uint8_t *) pBuffForOtp);
    if (error != CC_OK) {
            CRYPTO_PRINTF("failed to CC_PROD_LLF_RND_VerifyGeneration, error 0x%x\n", error);
            goto r_cc312_calchuk_test_done;
    }

    /*Count number of zero bits in HUK OTP fileds*/
    error  = CC_PROD_GetZeroCount(pBuffForOtp, CC_OTP_HUK_SIZE_IN_WORDS, &zeroCount);
    if (error != CC_OK) {
            CRYPTO_PRINTF("Invalid Huk zero count\n");
            goto r_cc312_calchuk_test_done;
    }

    *HUKsize = (CC_OTP_HUK_SIZE_IN_WORDS * sizeof(uint32_t));

    error = CC_OK;

r_cc312_calchuk_test_done:

    CCPROD_Fini();

    CRYPTO_FREE(pRndWorkBuff);

    return error;
}


uint32_t R_CC312_SecureBoot_RMA(void)
{
    BsvItError_t rc = BSVIT_ERROR__OK;

    CC_PalInit(); // for Mutex
    CC_HalInit();

    SBROM_DBG_TRIGGER(MODE_CRY_STEP(0xC32000));

    rc = r_cc312_sboot_init();
    if( rc != BSVIT_ERROR__OK ) goto end_step;

end_step:
    CC_HalTerminate();
    CC_PalTerminate();

    return (rc == BSVIT_ERROR__OK) ? true : false;
}

