/*
 * WPA Supplicant - Basic mesh peer management
 * Copyright (c) 2013-2014, cozybit, Inc.  All rights reserved.
 *
 * @file mesh_mpm.h
 *
 * @brief
 *
 * Copyright (c) 2016-2022 Renesas Electronics. All rights reserved.
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

#ifndef MESH_MPM_H
#define MESH_MPM_H

/* notify MPM of new mesh peer to be inserted in MPM and driver */
void wpa_mesh_new_mesh_peer(struct wpa_supplicant *wpa_s, const u8 *addr,
			    struct ieee802_11_elems *elems);
void mesh_mpm_deinit(struct wpa_supplicant *wpa_s, struct hostapd_iface *ifmsh);
void mesh_mpm_auth_peer(struct wpa_supplicant *wpa_s, const u8 *addr);
void mesh_mpm_free_sta(struct hostapd_data *hapd, struct sta_info *sta);
void wpa_mesh_set_plink_state(struct wpa_supplicant *wpa_s,
			      struct sta_info *sta,
			      enum mesh_plink_state state);
int mesh_mpm_close_peer(struct wpa_supplicant *wpa_s, const u8 *addr);
int mesh_mpm_connect_peer(struct wpa_supplicant *wpa_s, const u8 *addr,
			  int duration);

#ifdef CONFIG_MESH

void mesh_mpm_action_rx(struct wpa_supplicant *wpa_s,
			const struct ieee80211_mgmt *mgmt, size_t len);
void mesh_mpm_mgmt_rx(struct wpa_supplicant *wpa_s, struct rx_mgmt *rx_mgmt);

#else /* CONFIG_MESH */

static inline void mesh_mpm_action_rx(struct wpa_supplicant *wpa_s,
				      const struct ieee80211_mgmt *mgmt,
				      size_t len)
{
}

static inline void mesh_mpm_mgmt_rx(struct wpa_supplicant *wpa_s,
				    struct rx_mgmt *rx_mgmt)
{
}

#endif /* CONFIG_MESH */

#endif /* MESH_MPM_H */
