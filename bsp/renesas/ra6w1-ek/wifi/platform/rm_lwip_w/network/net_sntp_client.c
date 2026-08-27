/**
 ****************************************************************************************
 *
 * @file net_sntp_client.c
 *
 * @brief Simple Time Protocol Client module
 *
 * Copyright (c) 2023 Renesas Electronics. All rights reserved.
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
#include "bsp_api.h"
#include "osal.h"

#if CFG_WIFI /* Compile only with WiFi stack */

#include "rm_sntp.h"

#include "net_sntp_client.h"
#include "net_dns_client.h"
#include "rm_vee_flash_w_rrq_nvram.h"

#ifdef RM_MAP_PERSISTANT_W
#include "rm_map_persistant_w.h"
#endif
#if defined ( __SUPPORT_SNTP_CLIENT__ )

#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"


/******************************************************************************
 *
 *  Definitions
 *
 ******************************************************************************/
#undef __DEBUG_SNTP_CLIENT__

/* Configure the SNTP Client to use unicast SNTP. */
#define USE_UNICAST_SNTP

extern unsigned int sntp_sync_flag;

unsigned int stop_sntp(void)
{
	sntp_cmd_param * sntp_param = pvPortMalloc(sizeof(sntp_cmd_param));
	sntp_param->cmd = SNTP_STOP;
	sntp_run(sntp_param);
	return 0;
}

unsigned int start_sntp(void)
{
	sntp_cmd_param * sntp_param = pvPortMalloc(sizeof(sntp_cmd_param));
	sntp_param->cmd = SNTP_START;
	sntp_run(sntp_param);
	return 0;
}


unsigned int get_sntp_use(void)
{
	return sntp_get_use();
}

unsigned int set_sntp_use(int use)
{
	if (use <= 0) {
#ifdef RM_MAP_PERSISTANT_W
        if (RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, NVR_KEY_SNTP_C) != FSP_SUCCESS) {
#endif
            return 1; /* error */
#ifdef RM_MAP_PERSISTANT_W
        }
#endif
		stop_sntp();
	} else {
#ifdef RM_MAP_PERSISTANT_W
        if (RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG,
                                          NVR_KEY_SNTP_C, MODE_ENABLE) != FSP_SUCCESS) {
#endif
            return 1; /* error */
#ifdef RM_MAP_PERSISTANT_W
        }
#endif
		if (use == pdTRUE) {
			start_sntp();
		}
	}
	return 0; /* success */
}

void get_sntp_server(char *svraddr, unsigned int index)
{
    char *addr = sntp_get_server((sntp_svr_t)index);

    if (addr) {
    	strncpy(svraddr, addr, 32);
    	OS_FREE(addr);
	}
}

unsigned char set_sntp_server(unsigned char *svraddr, unsigned int index)
{
	return sntp_set_server((char *)svraddr, index);
}

unsigned char set_sntp_period(int period)
{
	int status;
	status = (int)sntp_set_period(period);
	return status;
}

int get_sntp_period(void)
{
	return sntp_get_period();
}

unsigned int is_sntp_sync(void)
{
    return sntp_sync_flag;
}

#endif  // __SUPPORT_SNTP_CLIENT__
#endif /* CFG_WIFI */
/* EOF */
