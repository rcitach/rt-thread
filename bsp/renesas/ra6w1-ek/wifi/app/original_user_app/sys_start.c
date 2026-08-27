/**
 ****************************************************************************************
 *
 * @file sys_start.c
 *
 * @brief System entry point for RRQ61000 IoT service
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

#include "FreeRTOS.h"
#include "custom_config_sdk.h"
#include "app_start.h"

static void _system_start(int net_chk_flag)
{
    /* Start system applications for RA6W1 */
    start_sys_apps(net_chk_flag);

    /*
     * Entry point of user's applications
     *     : defined in user_apps_table.c
     */
    /* Start user applications for RA6W1 */
    start_user_apps(net_chk_flag);

    return;
}

/**
 ******************************************************************************
 * brief      System entry point
 * input[in]  init_state
 * return     None
 ******************************************************************************
 */
int start_system(char init_state, int net_chk_flag)
{
    int	status = 0;

    /* Entry point for customer main */
	if (init_state == pdTRUE) {
		_system_start(net_chk_flag);
	} else {
		printf("\nFailed to initialize the RamLib or pTIM !!!\n");
	}

    return status;
}

/* EOF */
