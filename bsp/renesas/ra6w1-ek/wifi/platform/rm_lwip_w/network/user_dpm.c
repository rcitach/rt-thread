/**
 ****************************************************************************************
 *
 * @file user_dpm.c
 *
 * @brief User APIs for DPM operation
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


#include "FreeRTOS.h"
#include "custom_config_sdk.h"

#if CFG_PMGR

#include "user_dpm.h"
#include "rm_pmgr_w_instance.h"

//////////////////////////////////////////////////////////////////////////////
/// DPM related functions for USER ///////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

void show_rtm_for_app(void)
{
    user_dpm_supp_net_info_t    *user_dpm_supp_net_info;
    user_dpm_supp_ip_info_t     *user_dpm_supp_ip_info;
    user_dpm_supp_conn_info_t   *user_dpm_supp_conn_info;
    user_dpm_supp_key_info_t    *user_dpm_supp_key_info;

    RM_PMGR_W_rtm_static_get(RTM_STATIC_KEY_SUPPL_NET_INFO_PTR, NULL, NULL, (void**)(&user_dpm_supp_net_info));
    RM_PMGR_W_rtm_static_get(RTM_STATIC_KEY_SUPPL_IP_INFO_PTR, NULL, NULL, (void**)(&user_dpm_supp_ip_info));
    RM_PMGR_W_rtm_static_get(RTM_STATIC_KEY_SUPPL_CONN_INFO_PTR, NULL, NULL, (void**)(&user_dpm_supp_conn_info));
    RM_PMGR_W_rtm_static_get(RTM_STATIC_KEY_SUPPL_KEY_INFO_PTR, NULL, NULL, (void**)(&user_dpm_supp_key_info));

    if (   user_dpm_supp_net_info == NULL
        || user_dpm_supp_ip_info == NULL
        || user_dpm_supp_conn_info == NULL
        || user_dpm_supp_key_info == NULL)
    {
        printf("[DPM] Retention memory address doesn't assigned it.\n");
        return;
    }

    printf("\n=== Retention Memory informat for DPM ====\n");

    printf(" --- Connected AP Informat ---------------\n");
    printf("  ssid               = %s \n", user_dpm_supp_conn_info->ssid);
    printf("  bssid              = %02x:%02x:%02x:%02x:%02x:%02x \n",
            user_dpm_supp_conn_info->bssid[0],
            user_dpm_supp_conn_info->bssid[1],
            user_dpm_supp_conn_info->bssid[2],
            user_dpm_supp_conn_info->bssid[3],
            user_dpm_supp_conn_info->bssid[4],
            user_dpm_supp_conn_info->bssid[5]);

    printf("\n --- Config SSID Key informat -------------\n");
    printf("  wep_tx_keyidx      = %d\n", user_dpm_supp_key_info->wep_tx_keyidx);
    printf("  wep_key_len        = %d\n", user_dpm_supp_key_info->wep_key_len);
    printf("  ptk.wpa_alg        = %d\n", user_dpm_supp_key_info->ptk.wpa_alg);
    printf("  gtk.wpa_alg        = %d\n", user_dpm_supp_key_info->gtk.wpa_alg);
    printf("  proto              = %d\n", user_dpm_supp_key_info->proto);
    printf("  key_mgmt           = %d\n", user_dpm_supp_key_info->key_mgmt);
    printf("  pairwise_cipher    = %d\n", user_dpm_supp_key_info->pairwise_cipher);
    printf("  group_cipher       = %d\n", user_dpm_supp_key_info->group_cipher);

    printf("\n --- DHCP Network Informat ---------------\n");
    printf("  wifi_mode          = %d\n", user_dpm_supp_net_info->wifi_mode);
    printf("  country_code       = %s\n", user_dpm_supp_net_info->country);
    printf("  net_mode           = %s\n", user_dpm_supp_net_info->net_mode == 1 ? "DHCPC" : "STATIC");


    printf("\n --- IP Informat --------------------------\n");
    printf("  ip_addr            = %d.%d.%d.%d\n",
            (int)((user_dpm_supp_ip_info->dpm_ip_addr      ) & 0xff),
			(int)((user_dpm_supp_ip_info->dpm_ip_addr >>  8) & 0xff),
			(int)((user_dpm_supp_ip_info->dpm_ip_addr >> 16) & 0xff),
			(int)((user_dpm_supp_ip_info->dpm_ip_addr >> 24) & 0xff));

    printf("  netmask            = %d.%d.%d.%d\n",
    		(int)((user_dpm_supp_ip_info->dpm_netmask      ) & 0xff),
			(int)((user_dpm_supp_ip_info->dpm_netmask >>  8) & 0xff),
			(int)((user_dpm_supp_ip_info->dpm_netmask >> 16) & 0xff),
			(int)((user_dpm_supp_ip_info->dpm_netmask >> 24) & 0xff));

    printf("  gateway            = %d.%d.%d.%d\n",
    		(int)((user_dpm_supp_ip_info->dpm_gateway      ) & 0xff),
			(int)((user_dpm_supp_ip_info->dpm_gateway >>  8) & 0xff),
			(int)((user_dpm_supp_ip_info->dpm_gateway >> 16) & 0xff),
			(int)((user_dpm_supp_ip_info->dpm_gateway >> 24) & 0xff));

    printf("  dns_addr #1        = %d.%d.%d.%d\n",
    		(int)((user_dpm_supp_ip_info->dpm_dns_addr[0]      ) & 0xff),
			(int)((user_dpm_supp_ip_info->dpm_dns_addr[0] >>  8) & 0xff),
			(int)((user_dpm_supp_ip_info->dpm_dns_addr[0] >> 16) & 0xff),
			(int)((user_dpm_supp_ip_info->dpm_dns_addr[0] >> 24) & 0xff));

    if (user_dpm_supp_ip_info->dpm_dns_addr[1] > 0) {
        printf("  dns_addr #2        = %d.%d.%d.%d\n",
        		(int)((user_dpm_supp_ip_info->dpm_dns_addr[1]      ) & 0xff),
				(int)((user_dpm_supp_ip_info->dpm_dns_addr[1] >>  8) & 0xff),
				(int)((user_dpm_supp_ip_info->dpm_dns_addr[1] >> 16) & 0xff),
				(int)((user_dpm_supp_ip_info->dpm_dns_addr[1] >> 24) & 0xff));
    }

    printf("  dhcp_xid           = %x\n",(unsigned int)(user_dpm_supp_ip_info->dpm_dhcp_xid));
    printf("  lease              = %d secs\n",(int)(user_dpm_supp_ip_info->dpm_lease / 100));
    printf("  renewal            = %d secs\n",(int)(user_dpm_supp_ip_info->dpm_renewal / 100));
    printf("  dpm_rebind         = %d secs\n",(int)(user_dpm_supp_ip_info->dpm_timeout / 100));
}

#if !defined ( __DISABLE_DPM_ABNORM__ )
int chk_abnormal_wakeup(void)
{
    extern UCHAR get_last_abnormal_act(void);

    if (RM_PMGR_W_dpm_is_wakeup() && get_last_abnormal_act() > 0) {
        return pdTRUE;
    }

    return pdFALSE;
}
#endif // !__DISABLE_DPM_ABNORM__

#endif /* CFG_PMGR */
/* EOF */
