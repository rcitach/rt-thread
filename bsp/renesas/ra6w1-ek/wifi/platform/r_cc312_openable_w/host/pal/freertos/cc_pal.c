/*
 * Copyright (c) 2001-2019, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR Arm’s non-OSI source license
 */

#include "rm_psa_crypto_w_hwcfg.h"

#ifdef CC_IOT

/************* Include Files ****************/
#include "cc_pal_init.h"
#include "cc_pal_dma_plat.h"
#include "cc_pal_log.h"
#include "dx_reg_base_host.h"
#include "cc_pal_mutex.h"
#include "cc_pal_mem.h"
#include "cc_pal_abort.h"
#include "cc_pal_pm.h"
#include "cc_pal_interrupt_ctrl_plat.h"

extern CC_PalMutex CCSymCryptoMutex;
extern CC_PalMutex CCAsymCryptoMutex;
extern CC_PalMutex CCRndCryptoMutex;
extern CC_PalMutex *pCCRndCryptoMutex;
extern CC_PalMutex CCApbFilteringRegMutex;

#define PAL_WORKSPACE_MEM_BASE_ADDR	    0
#define PAL_WORKSPACE_MEM_SIZE		    0


static int pal_init_counter = 0;

static int CC_Pal_Instance_Check(void)
{
       return  pal_init_counter;
}

static void CC_Pal_Instance_Incr(void)
{
       pal_init_counter++;
}

static void CC_Pal_Instance_Decr(void)
{
       pal_init_counter--;
}

/**
 * @brief   PAL layer entry point.
 *          The function initializes customer platform sub components,
 *           such as memory mapping used later by CRYS to get physical contiguous memory.
 *
 *
 * @return Returns a non-zero value in case of failure
 */
int CC_PalInit(void)
{

    CCError_t rc = CC_FAIL;

    if(CC_Pal_Instance_Check() != 0 ){
        // Re-Init
        CC_Pal_Instance_Incr();
        return CC_SUCCESS;
    }

    CC_Pal_Instance_Incr();

    CC_PalLogInit();

    /* Currently in FreeRtos palDma is not needed - and therefore is implemented as empty. */
    rc = CC_PalDmaInit(PAL_WORKSPACE_MEM_SIZE, PAL_WORKSPACE_MEM_BASE_ADDR);
    if (rc != CC_SUCCESS)
    {
        //return rc;
        goto pal_init_exit;
    }

    /* Initialize power management module */
    CC_PalPowerSaveModeInit();
    
    /* Initialize mutex that protects shared memory and crypto access */
    rc = CC_PalMutexCreate(&CCSymCryptoMutex);
    if (rc != CC_SUCCESS)
    {
        CC_PalAbort("Fail to create SYM mutex\n");
        goto pal_init_exit;
    }
    /* Initialize mutex that protects shared memory and crypto access */
    rc = CC_PalMutexCreate(&CCAsymCryptoMutex);
    if (rc != CC_SUCCESS)
    {
        CC_PalAbort("Fail to create ASYM mutex\n");
        goto pal_init_exit;
    }
    /* Initialize mutex that protects shared memory and crypto access */
    rc = CC_PalMutexCreate(&CCRndCryptoMutex);
    if (rc != CC_SUCCESS) {
        CC_PalAbort("Fail to create RND mutex\n");
        goto pal_init_exit;
    }
    pCCRndCryptoMutex = &CCRndCryptoMutex;
    
    /* Initialize mutex that protects APBC access */
    rc = CC_PalMutexCreate(&CCApbFilteringRegMutex);
    if (rc != CC_SUCCESS) {
        CC_PalAbort("Fail to create APBC mutex\n");
        goto pal_init_exit;
    }

    rc = CC_PalInitIrq();

    if (rc != CC_SUCCESS) {
        //CC_PalAbort("Fail to call CC_PalInitIrq\n");
        goto pal_init_exit;
    }

pal_init_exit:

    if( rc != CC_SUCCESS){
        CC_PalTerminate();
    }
        
    return rc;
}


/**
 * @brief   PAL layer entry point.
 *          The function initializes customer platform sub components,
 *           such as memory mapping used later by CRYS to get physical contiguous memory.
 *
 *
 * @return None
 */
void CC_PalTerminate(void)
{
    CCError_t err = CC_FAIL;

    if(CC_Pal_Instance_Check() == 0 ){
        // CC312 is not initilized.
        return;
    }    

    CC_Pal_Instance_Decr();

    if(CC_Pal_Instance_Check() != 0 ){
        // CC312 is used by others.
        return;
    }    

    CC_PalDmaTerminate();
    CC_PalFinishIrq();

    if(CCSymCryptoMutex != NULL ){
        err = CC_PalMutexDestroy(&CCSymCryptoMutex);
        if (err != CC_SUCCESS)
            {
                CC_PAL_LOG_DEBUG("failed to destroy mutex CCSymCryptoMutex\n");
            }
        CC_PalMemSetZero(&CCSymCryptoMutex, sizeof(CC_PalMutex));
    }else{
        CC_PAL_LOG_DEBUG("already to destroy mutex CCSymCryptoMutex\n");
    }
    
    
    if(CCAsymCryptoMutex != NULL ){
        err = CC_PalMutexDestroy(&CCAsymCryptoMutex);
        if (err != CC_SUCCESS)
            {
                CC_PAL_LOG_DEBUG("failed to destroy mutex CCAsymCryptoMutex\n");
            }
        CC_PalMemSetZero(&CCAsymCryptoMutex, sizeof(CC_PalMutex));
    }else{
        CC_PAL_LOG_DEBUG("already to destroy mutex CCAsymCryptoMutex\n");
    }
    
    
    if(CCRndCryptoMutex != NULL ){
        err = CC_PalMutexDestroy(&CCRndCryptoMutex);
        if (err != CC_SUCCESS)
            {
                CC_PAL_LOG_DEBUG("failed to destroy mutex CCRndCryptoMutex\n");
            }
        CC_PalMemSetZero(&CCRndCryptoMutex, sizeof(CC_PalMutex));
    }else{
        CC_PAL_LOG_DEBUG("already to destroy mutex CCRndCryptoMutex\n");
    }
    
    
    if(CCApbFilteringRegMutex != NULL ){
        err = CC_PalMutexDestroy(&CCApbFilteringRegMutex);
        if (err != 0){
                CC_PAL_LOG_DEBUG("failed to destroy mutex CCApbFilteringRegMutex\n");
        }
        CC_PalMemSetZero(&CCApbFilteringRegMutex, sizeof(CC_PalMutex));
    }else{
        CC_PAL_LOG_DEBUG("already to destroy mutex CCApbFilteringRegMutex\n");
    }

}

#endif

