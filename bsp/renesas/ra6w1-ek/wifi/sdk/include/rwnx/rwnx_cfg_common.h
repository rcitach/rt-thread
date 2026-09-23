/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef _RWNX_CFG_COMMON_H
#define _RWNX_CFG_COMMON_H

struct cfg80211_crypto_settings {
    u32 wpa_versions;

    bool rwnx_crypto_debug_en;
    u8  rwnx_crypto_debug_level;

    bool control_port;
    __be16 control_port_ethertype;
    bool control_port_no_encrypt;

    u32 cipher_group;
    int n_ciphers_pairwise;
    u32 ciphers_pairwise[FC80211_MAX_NR_CIPHER_SUITES];
    int n_akm_suites;
    u32 akm_suites[FC80211_MAX_NR_AKM_SUITES];

    const u8 *psk;
    const u8 *sae_pwd;
    u8 sae_pwd_len;
    enum fc80211_sae_pwe_mechanism sae_pwe;
};

#endif
