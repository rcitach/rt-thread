/*
 * hostapd / P2P integration
 * Copyright (c) 2009-2010, Atheros Communications
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/supp_common.h"
#include "common/ieee802_11_defs.h"
#include "p2p/supp_p2p.h"
#include "hostapd.h"
#include "ap_config.h"
#include "ap_drv_ops.h"
#include "sta_info.h"
#include "p2p_hostapd.h"


#ifdef CONFIG_P2P

#ifdef  CONFIG_P2P_POWER_SAVE
struct notice_of_absence {
	u8 index;
	u8 cw_opp;
	u8 count;
	int duration;
	int interval;
	int start_time;
} __packed;
#endif  /* CONFIG_P2P_POWER_SAVE */

int hostapd_p2p_get_mib_sta(struct hostapd_data *hapd, struct sta_info *sta,
			    char *buf, size_t buflen)
{
	if (sta->p2p_ie == NULL)
		return 0;

	return p2p_ie_text(sta->p2p_ie, buf, buf + buflen);
}

#ifdef	CONFIG_P2P_POWER_SAVE
int hostapd_p2p_set_opp_ps(struct hostapd_data *hapd, int ctwindow)	//MODIFY_SUPPLICANT_FOR_FREERTOS-add
{
	struct notice_of_absence noa_ie;

	ra6wx_p2p_prt("[%s] P2P: Set Opportunistic Power Save parameters: "
				"ctwindow=%d\n", __func__, ctwindow);

	os_memset(&noa_ie, 0, sizeof(noa_ie));

	noa_ie.cw_opp = ctwindow;
	noa_ie.cw_opp += 1 << 7;

	p2p_group_notif_noa(hapd->p2p_group, (const u8 *)&noa_ie,
					sizeof(noa_ie));

	return 0;
}


#if 1	//MODIFY_SUPPLICANT_FOR_FREERTOS
int hostapd_p2p_set_noa(struct hostapd_data *hapd, u8 count, int start,
			int duration)
{
	wpa_printf(MSG_DEBUG, "P2P: Set NoA parameters: count=%u start=%d "
		   "duration=%d", count, start, duration);

	if (count == 0) {
		hapd->noa_enabled = 0;
		hapd->noa_start = 0;
		hapd->noa_duration = 0;
	}

	if (count != 255) {
		wpa_printf(MSG_DEBUG, "P2P: Non-periodic NoA - set "
			   "NoA parameters");
		return hostapd_driver_set_noa(hapd, count, start, duration);
	}

	hapd->noa_enabled = 1;
	hapd->noa_start = start;
	hapd->noa_duration = duration;

	if (hapd->num_sta_no_p2p == 0) {
		wpa_printf(MSG_DEBUG, "P2P: No legacy STAs connected - update "
			   "periodic NoA parameters");
		return hostapd_driver_set_noa(hapd, count, start, duration);
	}

	wpa_printf(MSG_DEBUG, "P2P: Legacy STA(s) connected - do not enable "
		   "periodic NoA");
	return 0;
}
#else
int hostapd_p2p_set_noa(struct hostapd_data *hapd, u8 count, int duration,
            int interval, int start)
{
	struct notice_of_absence noa_ie;

	ra6wx_p2p_prt("[%s] P2P: Set NoA parameters: count=%u duration=%d "
				"interval=%d start_time=%d\n",
				__func__, count, duration, interval, start);

	os_memset(&noa_ie, 0, sizeof(noa_ie));

	noa_ie.count = count;
	noa_ie.duration = duration;
	noa_ie.interval = interval;
	noa_ie.start_time = start;

	p2p_group_notif_noa(hapd->p2p_group, (const u8 *)&noa_ie, sizeof(noa_ie));

	return 0;
}
#endif

#if 1	/* by Shingu 20170524 (P2P_PS) */	//MODIFY_SUPPLICANT_FOR_FREERTOS
#ifdef  CONFIG_P2P_UNUSED_CMD
int hostapd_p2p_default_noa(struct hostapd_data *hapd, int status)
{
#if 0	/* by Shingu 20170601 (P2P_PS by LMAC) */
	if (status) {
		struct notice_of_absence noa_ie;

		ra6wx_p2p_prt("[%s] P2P: Set NoA parameters: count=255 "
			"duration=51200 interval=102400 start_time=1\n",
			__func__);

		os_memset(&noa_ie, 0, sizeof(noa_ie));

		noa_ie.count = 255;
		noa_ie.duration = 51200;
		noa_ie.interval = 102400;
		noa_ie.start_time = 1;

		p2p_group_notif_noa(hapd->p2p_group, (const u8 *)&noa_ie, sizeof(noa_ie));
	} else {
		p2p_group_notif_noa(hapd->p2p_group, NULL, 0);
	}
#endif  /* 0 */

	if (status == 1 || status == 0)
		hostapd_drv_p2p_ps_from_sp(hapd, status);
	else
		return -1;

	return 0;
}
#endif
#endif  /* 1 */
#endif  //CONFIG_P2P_POWER_SAVE

void hostapd_p2p_non_p2p_sta_connected(struct hostapd_data *hapd)
{
	wpa_printf(MSG_DEBUG, "P2P: First non-P2P device connected");

	if (hapd->noa_enabled) {
		wpa_printf(MSG_DEBUG, "P2P: Disable periodic NoA");
		hostapd_driver_set_noa(hapd, 0, 0, 0);
	}
}


void hostapd_p2p_non_p2p_sta_disconnected(struct hostapd_data *hapd)
{
	wpa_printf(MSG_DEBUG, "P2P: Last non-P2P device disconnected");

	if (hapd->noa_enabled) {
		wpa_printf(MSG_DEBUG, "P2P: Enable periodic NoA");
		hostapd_driver_set_noa(hapd, 255, hapd->noa_start,
				       hapd->noa_duration);
	}
}

#endif /* CONFIG_P2P */


#ifdef CONFIG_P2P_MANAGER
u8 * hostapd_eid_p2p_manage(struct hostapd_data *hapd, u8 *eid)
{
	u8 bitmap;
	*eid++ = WLAN_EID_VENDOR_SPECIFIC;
	*eid++ = 4 + 3 + 1;
	WPA_PUT_BE32(eid, P2P_IE_VENDOR_TYPE);
	eid += 4;

	*eid++ = P2P_ATTR_MANAGEABILITY;
	WPA_PUT_LE16(eid, 1);
	eid += 2;
	bitmap = P2P_MAN_DEVICE_MANAGEMENT;
	if (hapd->conf->p2p & P2P_ALLOW_CROSS_CONNECTION)
		bitmap |= P2P_MAN_CROSS_CONNECTION_PERMITTED;
	bitmap |= P2P_MAN_COEXISTENCE_OPTIONAL;
	*eid++ = bitmap;

	return eid;
}
#endif /* CONFIG_P2P_MANAGER */
