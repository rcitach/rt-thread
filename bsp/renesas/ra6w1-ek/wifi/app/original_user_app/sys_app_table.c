/**
 ****************************************************************************************
 *
 * @file sys_app_table.c
 *
 * @brief System application
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

#include "sys_app_defs.h"
#include "common_def.h"
#include "lwip/dhcp.h"
#include "rm_wifi.h"
#if defined (__SUPPORT_MQTT__)
#include "mqtt_client.h"
#endif
#if CFG_PMGR
#include "rm_pmgr_w_instance.h"
#endif /* CFG_PMGR */

#if (TCP_CLIENT_APP_START == 1)
#include "tcp_client.h"
#endif

#if defined ( __SUPPORT_DHCPC_IP_TO_STATIC_IP__ )
extern int ra6w1_get_temp_staticip_mode(int iface_flag);
#endif	//( __SUPPORT_DHCPC_IP_TO_STATIC_IP__ )

extern int  interface_select;
extern unsigned char fast_connection_sleep_flag;


#define   FAST_CON_ARP_CHECK_MAX_CNT    5    // During N * 200ms = 1s checking
#undef    DEBUG_PRINT_CONNECT_TIME

/******************************************************************************
 * RA6Wx system applications
 *****************************************************************************/

const app_task_info_t sys_apps_table[] = {
  /* name, entry_func, stack_size, priority, timeslice, net_chk_flag, dpm_flag, port_no, run_sys_mode */

  /****** For fucntion features ***************************************/
#if defined ( __SUPPORT_MQTT__ )
  { APP_MQTT_SUB,   mqtt_auto_start,                1024, (OS_TASK_PRIORITY_USER + 6), TRUE,  TRUE,  UNDEF_PORT,      RUN_STA_MODE },
#endif    // __SUPPORT_MQTT__

#if defined ( __HTTP_SVR_AUTO_START__ )
  { APP_HTTP_SVR,   auto_run_http_svr,               512, (OS_TASK_PRIORITY_USER + 6), TRUE,  FALSE, UNDEF_PORT,      RUN_ALL_MODE  },
#endif // __HTTP_SVR_AUTO_START__

#if (TCP_CLIENT_APP_START == 1)
  { JOB_ID_TCPC_CONN, tcpc_task_starter, TCPC_TASK_SIZE,  (OS_TASK_PRIORITY_USER + 6), TRUE, TRUE, 0, RUN_STA_MODE },
#endif

  /******* End of List *****************************************************/
  { NULL,    NULL,    0, 0, FALSE, FALSE, UNDEF_PORT,    0    }

};


/* EOF */
