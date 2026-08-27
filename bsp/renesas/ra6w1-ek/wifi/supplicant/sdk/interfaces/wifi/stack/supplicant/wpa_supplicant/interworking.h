/*
 * Interworking (IEEE 802.11u)
 * Copyright (c) 2011-2012, Qualcomm Atheros
 *
 * @file interworking.h
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

#ifndef INTERWORKING_H
#define INTERWORKING_H

enum gas_query_result;

int anqp_send_req(struct wpa_supplicant *wpa_s, const u8 *dst, int freq,
		  u16 info_ids[], size_t num_ids, u32 subtypes,
		  u32 mbo_subtypes);
void anqp_resp_cb(void *ctx, const u8 *dst, u8 dialog_token,
		  enum gas_query_result result,
		  const struct wpabuf *adv_proto,
		  const struct wpabuf *resp, u16 status_code);
int gas_send_request(struct wpa_supplicant *wpa_s, const u8 *dst,
		     const struct wpabuf *adv_proto,
		     const struct wpabuf *query);
int interworking_fetch_anqp(struct wpa_supplicant *wpa_s);
void interworking_stop_fetch_anqp(struct wpa_supplicant *wpa_s);
int interworking_select(struct wpa_supplicant *wpa_s, int auto_select,
			int *freqs);
int interworking_connect(struct wpa_supplicant *wpa_s, struct wpa_bss *bss,
			 int only_add);
void interworking_start_fetch_anqp(struct wpa_supplicant *wpa_s);
int interworking_home_sp_cred(struct wpa_supplicant *wpa_s,
			      struct wpa_cred *cred,
			      struct wpabuf *domain_names);
int domain_name_list_contains(struct wpabuf *domain_names,
			      const char *domain, int exact_match);

#endif /* INTERWORKING_H */
