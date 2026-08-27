/**
 ****************************************************************************************
 *
 * @file rm_wifi_reg_pwr_db.h
 *
 * @brief Regulatory Power data base API
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


#ifndef _RM_WIFI_REG_PWR_DB_H__
#define _RM_WIFI_REG_PWR_DB_H__

#define MAX_CH_NUM_2G_L    11
#define MAX_CH_NUM_2G_H    3
#define MAX_CH_NUM_2G      (MAX_CH_NUM_2G_L + MAX_CH_NUM_2G_H)

#define MAX_CH_NUM_5G_S    2
#define MAX_CH_NUM_5G_M    4
#define MAX_CH_NUM_5G_L    12
#define MAX_CH_NUM_5G      28

enum ra6w1_band_group_2g {
    RRQ61X_BND_GRP_2G_OFDM, 	// 2.4G OFDM	 : (14) 1 ~14
    RRQ61X_BND_GRP_2G_DSSS, 	// 2.4G	DSSS	 : (14) 1 ~14

    RRQ61X_BND_GRP_2G_NUM
};

enum ra6w1_band_group_5g {
    /* UNII-I : (04) 36, 40, 44, 48 (5180 ~ 5240) */
    RRQ61X_BND_GRP_5G_I,
    /* UNII-II, DFS : (04) 52, 56, 60, 64 (5260 ~ 5320) */
    RRQ61X_BND_GRP_5G_II,
    /* UNII-IIe, DFS : (12) 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144 (5500 ~ 5720) */
    RRQ61X_BND_GRP_5G_II_E,
    /* UNII-III : (04) 149, 153, 157, 161 (5745 ~ 5805) */
    RRQ61X_BND_GRP_5G_III,
    /* ISM : (01) 165 (5825) */
    RRQ61X_BND_GRP_5G_ISM,
    /* UNII-IV : (01) 169 (5845) */
    RRQ61X_BND_GRP_5G_IV_1,
    /* UNII-IV : (02) 173, 177 (5865 ~ 5885) */
    RRQ61X_BND_GRP_5G_IV_2,

    RRQ61X_BND_GRP_5G_NUM
};

enum ra6w1_band_group_all {
    RRQ61X_BND_GRP_ALL_2G_OFDM,
    RRQ61X_BND_GRP_ALL_2G_DSSS,
    RRQ61X_BND_GRP_ALL_5G_OFDM,
    RRQ61X_BND_GRP_ALL_6G,

    RRQ61X_BND_GROUP_NUM
};

enum ra6w1_reg_flag {
    /* value sync with enum ieee80211_channel_flags in rwnx driver */
    RRQ61X_REG_FLAG_NO_IR  = 1<<1,
    RRQ61X_REG_FLAG_DFS	   = 1<<3,

    RRQ61X_REG_FLAG_NUM
};

enum ra6w1_dfs_domain {
    /* value sync with enum fc80211_dfs_regions in rwnx driver */
    RRQ61X_DFS_UNSET       = 0,
    RRQ61X_DFS_FCC         = 1,
    RRQ61X_DFS_ETSI        = 2,
    RRQ61X_DFS_JP          = 3,
    RRQ61X_DFS_KR          = 4,
    RRQ61X_DFS_CH          = 5,
};

struct pwr_lvl_2g_l {
    enum ra6w1_band_group_2g band_grp;
    uint8_t num_channel;
    uint8_t ch_pwr[MAX_CH_NUM_2G_L];
};

struct pwr_lvl_2g_h {
    enum ra6w1_band_group_2g band_grp;
    uint8_t num_channel;
    uint8_t ch_pwr[MAX_CH_NUM_2G_H];
};

struct pwr_lvl_5g_s {
    enum ra6w1_band_group_5g band_grp;
    uint8_t num_channel;
    uint8_t flags;
    uint8_t ch_pwr[MAX_CH_NUM_5G_S];
};

struct pwr_lvl_5g_m {
    enum ra6w1_band_group_5g band_grp;
    uint8_t num_channel;
    uint8_t flags;
    uint8_t ch_pwr[MAX_CH_NUM_5G_M];
};

struct pwr_lvl_5g_l {
    enum ra6w1_band_group_5g band_grp;
    uint8_t num_channel;
    uint8_t flags;
    uint8_t ch_pwr[MAX_CH_NUM_5G_L];
};

struct cntry_reg_group_2g {
    const struct pwr_lvl_2g_l *pwr_lvl_2g_low[RRQ61X_BND_GRP_2G_NUM];
    const struct pwr_lvl_2g_h *pwr_lvl_2g_high[RRQ61X_BND_GRP_2G_NUM];
};

struct cntry_reg_group_5g {
    const struct pwr_lvl_5g_m *pwr_lvl_5g_i;
    const struct pwr_lvl_5g_m *pwr_lvl_5g_ii;
    const struct pwr_lvl_5g_l *pwr_lvl_5g_iie;
    const struct pwr_lvl_5g_m *pwr_lvl_5g_iii;
    const struct pwr_lvl_5g_s *pwr_lvl_5g_ism;
    const struct pwr_lvl_5g_s *pwr_lvl_5g_iv_1;
    const struct pwr_lvl_5g_s *pwr_lvl_5g_iv_2;
};

enum reg_group_2g_set {
    FCC_2G_GRP_SET_1,
    FCC_2G_GRP_SET_2,
    FCC_2G_GRP_SET_3,
    ETSI_2G_GRP_SET_1,
    ETSI_2G_GRP_SET_3,
    JP_2G_GRP_SET_1,
    JP_2G_GRP_SET_4,
    OTH_2G_GRP_SET_1,
    WR_2G_GRP_SET_1,
    OTH_2G_GRP_SET_4,
    OTH_2G_GRP_SET_5,

    REG_2G_GRP_SET_TOTAL_NUM
};

enum reg_group_5g_set {
    FCC_5G_GRP_SET_1,
    FCC_5G_GRP_SET_2,
    FCC_5G_GRP_SET_3,
    FCC_5G_GRP_SET_4,
    FCC_5G_GRP_SET_5,
    FCC_5G_GRP_SET_6,
    FCC_5G_GRP_SET_7,
    FCC_5G_GRP_SET_8,
    FCC_5G_GRP_SET_9,
    FCC_5G_GRP_SET_10,

    ETSI_5G_GRP_SET_1,
    ETSI_5G_GRP_SET_2,
    ETSI_5G_GRP_SET_3,
    ETSI_5G_GRP_SET_4,
    ETSI_5G_GRP_SET_5,
    ETSI_5G_GRP_SET_6,

    JP_5G_GRP_SET_1,
    JP_5G_GRP_SET_2,
    JP_5G_GRP_SET_3,
    JP_5G_GRP_SET_4,
    JP_5G_GRP_SET_5,
    JP_5G_GRP_SET_6,
    JP_5G_GRP_SET_7,
    JP_5G_GRP_SET_8,
    JP_5G_GRP_SET_9,

    OTH_5G_GRP_SET_1,
    OTH_5G_GRP_SET_2,
    OTH_5G_GRP_SET_3,

    OTH_5G_GRP_SET_4,
    OTH_5G_GRP_SET_5,

    REG_5G_GRP_SET_TOTAL_NUM
};

/* idx into cc_regdb_data[] */
enum country_code {
    CNTRY_AD,  /* Andorra */
    CNTRY_AE,  /* UAE */
    CNTRY_AF,  /* Afghanistan */
    CNTRY_AI,  /* Anguilla */
    CNTRY_AL,  /* Albania */
    CNTRY_AM,  /* Armenia */
    CNTRY_AN,  /* Netherlands Antilles */
    CNTRY_AR,  /* Argentina */
    CNTRY_AS,  /* Samoa */
    CNTRY_AT,  /* Austria */
    CNTRY_AU,  /* Australia */
    CNTRY_AW,  /* Aruba */
    CNTRY_AZ,  /* Azerbaijan */
    CNTRY_BA,  /* Bosnia */
    CNTRY_BB,  /* Barbados */
    CNTRY_BD,  /* Bangladesh */
    CNTRY_BE,  /* Belgium */
    CNTRY_BF,  /* Burkina Faso */
    CNTRY_BG,  /* Bulgaria */
    CNTRY_BH,  /* Bahrain */
    CNTRY_BL,  /* Barthelemy */
    CNTRY_BM,  /* Bermuda */
    CNTRY_BN,  /* Brunei */
    CNTRY_BO,  /* Bolivia */
    CNTRY_BR,  /* Brazil */
    CNTRY_BS,  /* Bahamas */
    CNTRY_BT,  /* Bhutan*/
    CNTRY_BY,  /* Belarus */
    CNTRY_BZ,  /* Belize */
    CNTRY_CA,  /* Canada*/
    CNTRY_CF,  /* Central Africa : FCC 1~ 13 */
    CNTRY_CH,  /* Switzerland */
    CNTRY_CI,  /* Cote d'Ivoire */
    CNTRY_CL,  /* Chile */
    CNTRY_CN,  /* China */
    CNTRY_CO,  /* Colombia*/
    CNTRY_CR,  /* Costa Rica */
    CNTRY_CU,  /* Cuba */
    CNTRY_CX,  /* Christmas Island */
    CNTRY_CY,  /* Cyprus*/
    CNTRY_CZ,  /* Czech */
    CNTRY_DE,  /* Germany */
    CNTRY_DK,  /* Denmark */
    CNTRY_DM,  /* Dominica*/
    CNTRY_DO,  /* Dominican Rep */
    CNTRY_DZ,  /* Algeria*/
    CNTRY_EC,  /* Ecuador */
    CNTRY_EE,  /* Estonia*/
    CNTRY_EG,  /* Egypt*/
    CNTRY_ES,  /* Spain */
    CNTRY_ET,  /* Ethiopia */
    CNTRY_EU,  /* Europe */
    CNTRY_FI,  /* Finland */
    CNTRY_FM,  /* Micronesia */
    CNTRY_FR,  /* France*/
    CNTRY_GA,  /* Gabon */
    CNTRY_GB,  /* United Kingdom */
    CNTRY_GD,  /* Grenada */
    CNTRY_GE,  /* Georgia */
    CNTRY_GF,  /* French Guiana */
    CNTRY_GH,  /* Ghana */
    CNTRY_GL,  /* Greenland */
    CNTRY_GP,  /* Guadeloupe */
    CNTRY_GR,  /* Greece*/
    CNTRY_GT,  /* Guatemala */
    CNTRY_GU,  /* Guam*/
    CNTRY_GY,  /* Guyana */
    CNTRY_HK,  /* Hong Kong */
    CNTRY_HN,  /* Honduras*/
    CNTRY_HR,  /* Croatia */
    CNTRY_HT,  /* Haiti */
    CNTRY_HU,  /* Hungary */
    CNTRY_ID,  /* Indonesia*/
    CNTRY_IE,  /* Ireland */
    CNTRY_IL,  /* Israel*/
    CNTRY_IN,  /* India */
    CNTRY_IR,  /* Iran */
    CNTRY_IS,  /* Iceland */
    CNTRY_IT,  /* Italy */
    CNTRY_JM,  /* Jamaica */
    CNTRY_JO,  /* Jordan */
    CNTRY_JP,  /* Japan */
    CNTRY_KE,  /* Kenya */
    CNTRY_KH,  /* Cambodia*/
    CNTRY_KN,  /* St.Kitts and Nevis */
    CNTRY_KP,  /* N.Korea */
    CNTRY_KR,  /* S.Korea */
    CNTRY_KW,  /* Kuwait*/
    CNTRY_KY,  /* Cayman Islands */
    CNTRY_KZ,  /* Kazakhstan */
    CNTRY_LB,  /* Lebanon */
    CNTRY_LC,  /* Saint Lucia */
    CNTRY_LI,  /* Liechtenstein */
    CNTRY_LK,  /* Sri Lanka */
    CNTRY_LS,  /* Lesotho */
    CNTRY_LT,  /* Lithuania */
    CNTRY_LU,  /* Luxembourg */
    CNTRY_LV,  /* Latvia*/
    CNTRY_MA,  /* Morocco */
    CNTRY_MC,  /* Monaco*/
    CNTRY_MD,  /* Moldova */
    CNTRY_ME,  /* Montenegro */
    CNTRY_MF,  /* Saint Martin */
    CNTRY_MH,  /* Marshall Islands */
    CNTRY_MK,  /* Macedonia */
    CNTRY_MN,  /* Mongolia*/
    CNTRY_MO,  /* Macao */
    CNTRY_MP,  /* Northen Mariana Islands */
    CNTRY_MQ,  /* Martinique */
    CNTRY_MR,  /* Mauritania */
    CNTRY_MT,  /* Malta */
    CNTRY_MU,  /* Mauritius */
    CNTRY_MV,  /* Maldives*/
    CNTRY_MW,  /* Malawi*/
    CNTRY_MX,  /* Mexico */
    CNTRY_MY,  /* Malaysia*/
    CNTRY_NG,  /* Nigeria*/
    CNTRY_NI,  /* Nicaragua*/
    CNTRY_NL,  /* Netherlands */
    CNTRY_NO,  /* Norway*/
    CNTRY_NP,  /* Nepal */
    CNTRY_NZ,  /* New Zealand*/
    CNTRY_OM,  /* Oman */
    CNTRY_PA,  /* Panama*/
    CNTRY_PE,  /* Peru */
    CNTRY_PF,  /* Polynesia */
    CNTRY_PG,  /* Papua New Guinea*/
    CNTRY_PH,  /* Philippines*/
    CNTRY_PK,  /* Pakistan */
    CNTRY_PL,  /* Poland*/
    CNTRY_PM,  /* St. Pierre and Miquelon */
    CNTRY_PR,  /* Puerto Rico */
    CNTRY_PT,  /* Portugal*/
    CNTRY_PW,  /* Palau */
    CNTRY_PY,  /* Paraguay*/
    CNTRY_QA,  /* Qatar */
    CNTRY_RE,  /* Reunion */
    CNTRY_RO,  /* Romania */
    CNTRY_RS,  /* Serbia*/
    CNTRY_RU,  /* Russia*/
    CNTRY_RW,  /* Rwanda */
    CNTRY_SA,  /* Saudi */
    CNTRY_SE,  /* Sweden*/
    CNTRY_SG,  /* Singapore */
    CNTRY_SI,  /* Slovenia*/
    CNTRY_SK,  /* Slovakia*/
    CNTRY_SN,  /* Senegal */
    CNTRY_SR,  /* Suriname*/
    CNTRY_SV,  /* El Salvador*/
    CNTRY_SY,  /* Syria */
    CNTRY_TC,  /* Turks Caicos */
    CNTRY_TD,  /* Chad */
    CNTRY_TG,  /* Togo */
    CNTRY_TH,  /* Thailand*/
    CNTRY_TN,  /* Tunisia */
    CNTRY_TR,  /* Turkey*/
    CNTRY_TT,  /* Trinidad and Tobago*/
    CNTRY_TW,  /* Taiwan */
    CNTRY_TZ,  /* Tanzania*/
    CNTRY_UA,  /* Ukraine */
    CNTRY_UG,  /* Uganda */
    CNTRY_UK,  /* United Kingdom */
    CNTRY_US,  /* USA */
    CNTRY_UY,  /* Uruguay */
    CNTRY_UZ,  /* Uzbekistan */
    CNTRY_VA,  /* Vatican City */
    CNTRY_VC,  /* St. Vincent and Grenadines*/
    CNTRY_VE,  /* Venezuela */
    CNTRY_VI,  /* Virgin Islands, US */
    CNTRY_VN,  /* Viet Nam*/
    CNTRY_VU,  /* Vanuatu */
    CNTRY_WF,  /* Wallis and Futuna Islands */
    CNTRY_WS,  /* Samoa */
    CNTRY_YE,  /* Yemen */
    CNTRY_YT,  /* Mayotte */
    CNTRY_ZA,  /* S.Africa*/
    CNTRY_ZW,  /* Zimbabwe*/

    CNTRY_ZZ,  /* for debug */

    CC_NUM
};

struct cntry_reg_group_table {
    enum reg_group_2g_set pl_2g_set;
    enum reg_group_5g_set pl_5g_set;
};

extern const struct cntry_reg_group_2g cc_regdb_2g_data_grp[REG_2G_GRP_SET_TOTAL_NUM];
extern const struct cntry_reg_group_5g cc_regdb_5g_data_grp[REG_5G_GRP_SET_TOTAL_NUM];
extern const struct cntry_reg_group_table cc_regdb_data[CC_NUM];

#endif /* _RM_WIFI_REG_PWR_DB_H__ */
