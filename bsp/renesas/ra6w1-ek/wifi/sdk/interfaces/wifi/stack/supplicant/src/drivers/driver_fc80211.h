/**
 *****************************************************************************************
 * @file    driver_fc80211.h
 * @brief   Driver interaction with RA6WX fc80211 from wpa_supplicant-2.4
 *****************************************************************************************
 */

/*
 * Driver interaction with Linux nld11/cfgd11 - definitions
 * Copyright (c) 2002-2014, Jouni Malinen <j@w1.fi>
 * Copyright (c) 2003-2004, Instant802 Networks, Inc.
 * Copyright (c) 2005-2006, Devicescape Software, Inc.
 * Copyright (c) 2007, Johannes Berg <johannes@sipsolutions.net>
 * Copyright (c) 2009-2010, Atheros Communications
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 *
 * Copyright (c) 2020-2022 Modified by Renesas Electronics.
 */


#ifndef	__DRIVER_FC80211_H__
#define	__DRIVER_FC80211_H__

#include <stdbool.h>
#include "fc80211_copy.h"

#include "supp_eloop.h"
#include "supp_driver.h"

#ifndef __RS_CFG80211_RDEV_OPS
#include "rwnx_mac_common.h"
#endif //__RS_CFG80211_RDEV_OPS

extern int fc80211_get_interface_softmac_index(ULONG wdev_id, int ifidx);
extern int fc80211_get_interface_mode(ULONG wdev_id, int ifidx);
extern int fc80211_get_interface_channel_width(int ifidx,
			struct wpa_signal_info *sig);
extern UCHAR *fc80211_get_interface_macaddr(ULONG wdev_id, int ifidx);
extern int fc80211_set_interface(ULONG wdev_id, int ifidx,
			enum fc80211_iftype mode);
extern int fc80211_new_interface(ULONG wdev_id, int ifidx, const char *ifname,
			enum fc80211_iftype mode);
extern int fc80211_del_interface(ULONG wdev_id, int ifidx);
extern int fc80211_authenticate(int ifidx,
			struct wpa_driver_auth_params *params,
			enum fc80211_auth_type auth_type);
extern int fc80211_deauthenticate(int ifidx, const UCHAR *bssid,
			USHORT reason_code,
			int local_state_change);
extern int fc80211_associate(int ifidx,
			struct wpa_driver_associate_params *params);
/* P2P GO Inactivity */
extern int fc80211_chk_deauth_send_done(int ifidx);
/* For P2P_PS */
#ifdef  CONFIG_P2P_POWER_SAVE
#ifdef CONFIG_P2P_UNUSED_CMD
extern int fc80211_p2p_go_ps_on_off(int ifindex, int p2p_ps_status);
#endif
#endif /* CONFIG_P2P_POWER_SAVE */
extern int fc80211_connect(int ifidx,
			struct wpa_driver_associate_params *params,
			enum fc80211_auth_type auth_type, int privacy);
extern int fc80211_disconnect(int ifidx, USHORT reason_code);
extern int fc80211_tx_mgmt(ULONG wdev_id, int ifidx,
			unsigned int freq, unsigned int wait,
			const UCHAR *buf, size_t buf_len,
			int offchanok_tx_ok,
			int no_cck, int no_ack, ULONG *cookie);
#if 0	/* by Shingu 20161012 (Not used yet) */
extern int fc80211_set_noa(int ifindex, int count, int duration, int interval,
			   int start);
extern int fc80211_set_opp_ps(int ifindex, int ctwindow);
#endif	/* 0 */
extern int fc80211_enable_rssi_report(int ifindex,
				      int rssi_min_thold, int rssi_max_thold);
extern int fc80211_get_rtctimegap(int ifindex, u64 *rtctimegap);
extern int fc80211_get_empty_rid(void);
extern int fc80211_set_app_keepalivetime(unsigned char tid, unsigned int sec , void (* callback_func)(unsigned int tid));

extern int fc80211_set_cqm_rssi(int ifindex, s32 thresholds, u32 hysteresis);
extern int fc80211_external_auth_status(int ifindex, 
            struct cfg80211_external_auth_params *params);
extern int fc80211_update_ft_ies(int ifindex, u16 mdid, const u8 *ies, size_t ies_len);
extern int fc80211_channel_switch(int ifindex, struct csa_settings *settings);

/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */
/* !!!     NOT DEFINED YET     !!! */
/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */
extern int fc80211_get_reg(ULONG wdev_id, int ifidx,
			int (*handler)(void *), void *arg);
extern int fc80211_start_p2p_dev(ULONG wdev_id, int ifidx);
extern int fc80211_stop_p2p_dev(ULONG wdev_id, int ifidx);
extern int fc80211_set_beacon(int ifidx, struct wpa_driver_ap_params *params);
#ifdef CONFIG_OWE_TRANS
extern int fc80211_set_beacon_dual(int ifidx, struct wpa_driver_ap_params *params);
#endif // CONFIG_OWE_TRANS
extern int fc80211_new_beacon(int ifidx, struct wpa_driver_ap_params *params);
extern int fc80211_del_beacon(int ifidx);
extern int fc80211_join_ibss(int ifidx,
			struct wpa_driver_associate_params *params,
			int privacy);
extern int fc80211_leave_ibss(int ifidx);
extern int fc80211_frame_wait_cancel(int ifindex, u64 cookie);
extern u64 fc80211_remain_on_channel(int ifindex, UINT freq, UINT duration);
extern int fc80211_cancel_remain_on_channel(int ifidx, ULONG cookie);
extern int fc80211_set_tx_bitrate_mask(int ifidx, int disabled);
extern int fc80211_get_scan(ULONG wdev_id, int ifidx, void *arg);
extern int fc80211_get_station(int ifindex, const u8 *sta_mac, int cmd_type, struct station_info *sinfo);
extern int fc80211_req_set_reg(int ifidx, char *alpha2);
extern int fc80211_get_protocol_features(int ifidx, UINT *feat);
extern int fc80211_get_softmac(ULONG wdev_id, int ifidx, void *arg);
extern int fc80211_register_action(ULONG wdev_id, int ifindex, USHORT type,
			    const UCHAR *match, size_t match_len);
extern int fc80211_unexpected_frame(int ifidx);
extern int fc80211_trigger_scan(ULONG wdev_id, int ifidx,
			struct wpa_driver_scan_params *params);
extern int fc80211_abort_scan(ULONG wdev_id, int ifindex);
extern int fc80211_free_umac_scan_results(ULONG wdev_id, int ifindex);
extern int fc80211_sched_scan(int ifidx,
			struct wpa_driver_scan_params *params,
			UINT interval);
extern int fc80211_stop_sched_scan(int ifidx);
extern int fc80211_get_key(int ifindex, u8 *seq,u8 key_idx,const u8 *mac_addr);
extern int fc80211_del_key(int ifidx, const UCHAR *addr,
			const UCHAR *seq, size_t seq_len,
			int key_idx, int set_tx);
extern int fc80211_new_key(int ifidx, const UCHAR *addr,
			const UCHAR *key, size_t key_len,
			const UCHAR *seq, size_t seq_len,
			int key_idx, int set_tx, int alg);
extern int fc80211_set_key(int ifidx, const UCHAR *addr,
			int alg, int key_idx);
extern int fc80211_set_channel(int ifidx,
			struct hostapd_freq_params *freq,
			int set_chan);
extern int fc80211_get_channel(struct wireless_dev *wdev, 
			struct cfg80211_chan_def *chandef);
#ifdef CONFIG_WNM_SLEEP_MODE
extern int fc80211_set_wnm_sleep_mode(int oper, int ifindex, int intval);
#endif /* CONFIG_WNM_SLEEP_MODE */
extern int fc80211_set_bss(int ifindex, int cts, int preamble, int slot,
		    int ht_opmode, int ap_isolate, u8 *rates,int rates_len);


extern int fc80211_set_mac_acl(int ifidx, struct hostapd_acl_params *params);
#if 0
#ifdef CONFIG_MESH
extern int fc80211_set_station(int ifidx, 
			struct hostapd_sta_add_params *param, struct fc80211_sta_flag_update *arg);
#else
extern int fc80211_set_station(int ifidx, const UCHAR *mac_addr, struct fc80211_sta_flag_update *arg);
#endif
#endif
extern int fc80211_new_station(int ifidx,
			struct hostapd_sta_add_params *params, void *arg);
//extern int fc80211_del_station(int ifidx, const UCHAR *addr, int idx);
extern int fc80211_set_softmac(int ifidx, int queue, int aifs, int cwmin,
			int cwmax, int burst_time);
extern int fc80211_set_softmac_rts(int ifidx, UINT rts);
extern int fc80211_get_softmac_rts(int ifidx);
#ifdef __FRAG_ENABLE__
extern int fc80211_set_softmac_frag(int ifidx, UINT frag);
#endif
extern int fc80211_set_softmac_retry(int ifindex, u8 retry, u8 retry_long);
extern int fc80211_get_softmac_retry(int ifindex, u8 retry_long);
extern int fc80211_set_cqm(int ifidx, int threshold, int hysteresis);

#if 0
extern int fc80211_set_rekey_offload(int ifidx,
				const UCHAR *kek, const UCHAR *kck,
				const UCHAR *replay_ctr);
#endif

extern int fc80211_probe_client(int ifidx, const UCHAR *addr);
extern int rs_cfg80211_radar_start(int ifindex, struct hostapd_freq_params *freq);
extern int fc80211_ctrl_bridge(bool bridge_control);
extern int fc80211_set_power_save(int ifidx, int ps_state, int timeout);
extern void fc80211_ctrl_ampdu_rx(int ampdu_rx_control);
extern void fc80211_set_sta_power_save(int ifindex, int pwrsave_mode);
extern void fc80211_set_ampdu_flag(int mode, int val);
extern int fc80211_get_ampdu_flag(int mode);

#ifdef CONFIG_SIMPLE_ROAMING
extern void fc80211_set_roaming_flag(int mode);
#endif /* CONFIG_SIMPLE_ROAMING */

#ifdef	CONFIG_AP
#ifdef CONFIG_AP_NONE_STA
extern int fc80211_none_station(int ifindex);
#endif /* CONFIG_AP_NONE_STA */
extern int fc80211_radar_detect(int ifidx, struct hostapd_freq_params *freq);
#endif	/* CONFIG_AP */

#ifdef	CONFIG_AP_POWER
extern int fc80211_set_ap_power(int ifindex, int type, int power, int *get_power);
#endif /* CONFIG_AP_POWER */

#ifdef	CONFIG_AP /* by Shingu 20161010 (Keep-alive) */
extern int fc80211_sta_null_send(int ifindex, u8 *mac_addr);
#endif	/* CONFIG_AP */

void driver_fc80211_process_global_ev(ra6wx_drv_msg_buf_t *drv_msg_buf);

struct fc80211_global {
	struct	dl_list		interfaces;
	int	if_add_ifindex;
	ULONG	if_add_wdevid;
	int	if_add_wdevid_set;
	int	fc80211_id;
};

struct fc80211_softmac_data {
	struct dl_list	list;
	struct dl_list	bsss;
	struct dl_list	drvs;

	int softmac_idx;
};

void fc80211_global_deinit(void *priv);

struct wpa_driver_fc80211_data {
	struct fc80211_global *global;
	struct dl_list list;
	struct dl_list softmac_list;
	char	phyname[16];
	void	*ctx;
	int	ifindex;
	int	if_removed;
	int	if_disabled;
	int	ignore_if_down_event;
	struct wpa_driver_capa capa;
	UCHAR *extended_capa, *extended_capa_mask;
	unsigned int extended_capa_len;
	int	has_capability;

	int	operstate;

	int	scan_complete_events;
	enum scan_states {
		NO_SCAN, SCAN_REQUESTED, SCAN_STARTED, SCAN_COMPLETED,
		SCAN_ABORTED
	} scan_state;

	UCHAR	auth_bssid[ETH_ALEN];
	UCHAR	auth_attempt_bssid[ETH_ALEN];
	UCHAR	bssid[ETH_ALEN];
	UCHAR	prev_bssid[ETH_ALEN];
	int	associated;
	UCHAR	ssid[32];
	size_t	ssid_len;
	enum fc80211_iftype	nlmode;
	enum fc80211_iftype	ap_scan_as_station;
	unsigned int	assoc_freq;

	unsigned int disabled_11b_rates:1;
	unsigned int pending_remain_on_chan:1;
	unsigned int in_interface_list:1;
	unsigned int device_ap_sme:1;
	unsigned int poll_command_supported:1;
	unsigned int data_tx_status:1;
	unsigned int scan_for_auth:1;
	unsigned int retry_auth:1;
	unsigned int use_monitor:1;
	unsigned int ignore_next_local_disconnect:1;
	unsigned int ignore_next_local_deauth:1;
	unsigned int allow_p2p_device:1;
	unsigned int hostapd:1;
	unsigned int start_mode_ap:1;
	unsigned int start_iface_up:1;
	unsigned int test_use_roc_tx:1;
	unsigned int ignore_deauth_event:1;
	unsigned int dfs_vendor_cmd_avail:1;

	unsigned int connect_reassoc:1;

	ULONG	remain_on_chan_cookie;
	u64	send_action_cookie;
#define MAX_SEND_FRAME_COOKIES 20
	u64 send_frame_cookies[MAX_SEND_FRAME_COOKIES];
	unsigned int num_send_frame_cookies;
	
	u64 eapol_tx_cookie;

	unsigned int	last_mgmt_freq;

	struct wpa_driver_scan_filter	*filter_ssids;
	size_t num_filter_ssids;

	struct i802_bss *first_bss;

	int	default_if_indices[16];
	int	*if_indices;
	int	num_if_indices;

	/* From failed authentication command */
	int	auth_freq;
	UCHAR	auth_bssid_[ETH_ALEN];
	UCHAR	auth_ssid[32];
	size_t	auth_ssid_len;
	int	auth_alg;
	UCHAR	*auth_ie;
	size_t	auth_ie_len;
	UCHAR	auth_wep_key[4][16];
	size_t	auth_wep_key_len[4];
	int	auth_wep_tx_keyidx;
	int	auth_local_state_change;
	int	auth_p2p;

	int wnm_intval; //CONFIG_WNM_SLEEP_MODE

#ifdef	CONFIG_SUPP27_DRV_80211
	UCHAR	scan_ssid[32];
	size_t	scan_ssid_len;
	UCHAR	scan_bssid[ETH_ALEN];
#endif	/* CONFIG_SUPP27_DRV_80211 */
};

/*
 * This should be replaced with user space header once one is available with C
 * library, etc..
 */

#define IFF_UP		0x1		/* interface is up	*/
#define IFF_RUNNING     0x40            /* driver signals L1 up         */
#define IFF_LOWER_UP	0x10000         /* driver signals L1 up         */
#define IFF_DORMANT	0x20000         /* driver signals dormant       */

#define IFLA_IFNAME	3
#define IFLA_MASTER	10
#define IFLA_WIRELESS	11
#define IFLA_OPERSTATE	16
#define IFLA_LINKMODE	17
#define IF_OPER_DORMANT 5
#define IF_OPER_UP	6

#define NLM_F_REQUEST	1

#define NETLINK_ROUTE	0
#define RTMGRP_LINK	1
#define RTM_BASE	0x10
#define RTM_NEWLINK	(RTM_BASE + 0)
#define RTM_DELLINK	(RTM_BASE + 1)
#define RTM_SETLINK	(RTM_BASE + 3)

#define NLMSG_ALIGNTO	4
#define NLMSG_ALIGN(len) (((len) + NLMSG_ALIGNTO - 1) & ~(NLMSG_ALIGNTO - 1))
#define NLMSG_HDRLEN ((int) NLMSG_ALIGN(sizeof(struct nlmsghdr)))
#define NLMSG_LENGTH(len) ((len) + NLMSG_ALIGN(sizeof(struct nlmsghdr)))
#define NLMSG_SPACE(len) NLMSG_ALIGN(NLMSG_LENGTH(len))
#define NLMSG_DATA(nlh) ((void*) (((char*) nlh) + NLMSG_LENGTH(0)))
#define NLMSG_NEXT(nlh,len) ((len) -= NLMSG_ALIGN((nlh)->nlmsg_len), \
			     (struct nlmsghdr *) \
			     (((char *)(nlh)) + NLMSG_ALIGN((nlh)->nlmsg_len)))
#define NLMSG_OK(nlh,len) ((len) >= (int) sizeof(struct nlmsghdr) && \
			   (nlh)->nlmsg_len >= sizeof(struct nlmsghdr) && \
			   (int) (nlh)->nlmsg_len <= (len))
#define NLMSG_PAYLOAD(nlh,len) ((nlh)->nlmsg_len - NLMSG_SPACE((len)))

#define RTA_ALIGNTO	4
#define RTA_ALIGN(len)	(((len) + RTA_ALIGNTO - 1) & ~(RTA_ALIGNTO - 1))
#define RTA_OK(rta,len)	\
		((len) > 0 && (rta)->rta_len >= sizeof(struct rtattr) && \
		(rta)->rta_len <= (len))
#define RTA_NEXT(rta,attrlen) \
		((attrlen) -= RTA_ALIGN((rta)->rta_len), \
		(struct rtattr *) (((char *)(rta)) + RTA_ALIGN((rta)->rta_len)))

#define RTA_LENGTH(len) (RTA_ALIGN(sizeof(struct rtattr)) + (len))
#define RTA_DATA(rta) ((void *) (((char *) (rta)) + RTA_LENGTH(0)))
#define RTA_PAYLOAD(rta) ((int) ((rta)->rta_len) - RTA_LENGTH(0))


/* NETLINK_ERRNO_H */

#define NLE_SUCCESS		0
#define NLE_FAILURE		1
#define NLE_INTR		2
#define NLE_BAD_SOCK		3
#define NLE_AGAIN		4
#define NLE_NOMEM		5
#define NLE_EXIST		6
#define NLE_INVAL		7
#define NLE_RANGE		8
#define NLE_MSGSIZE		9
#define NLE_OPNOTSUPP		10
#define NLE_AF_NOSUPPORT	11
#define NLE_OBJ_NOTFOUND	12
#define NLE_NOATTR		13
#define NLE_MISSING_ATTR	14
#define NLE_AF_MISMATCH		15
#define NLE_SEQ_MISMATCH	16
#define NLE_MSG_OVERFLOW	17
#define NLE_MSG_TRUNC		18
#define NLE_NOADDR		19
#define NLE_SRCRT_NOSUPPORT	20
#define NLE_MSG_TOOSHORT	21
#define NLE_MSGTYPE_NOSUPPORT	22
#define NLE_OBJ_MISMATCH	23
#define NLE_NOCACHE		24
#define NLE_BUSY		25
#define NLE_PROTO_MISMATCH	26
#define NLE_NOACCESS		27
#define NLE_PERM		28
#define NLE_PKTLOC_FILE		29
#define NLE_PARSE_ERR		30
#define NLE_NODEV		31
#define NLE_IMMUTABLE		32
#define NLE_DUMP_INTR		33

#define NLE_MAX			NLE_DUMP_INTR

/**
 * @ingroup attr
 * Iterate over a stream of attributes
 * @arg pos     loop counter, set to current attribute
 * @arg head    head of attribute stream
 * @arg len     length of attribute stream
 * @arg rem     initialized to len, holds bytes currently remaining in stream
 */
#define nla_for_each_attr(pos, head, len, rem) \
	for (pos = head, rem = len; \
		nla_ok(pos, rem); \
		pos = nla_next(pos, &(rem)))


/*
 * nla_type (16 bits)
 * +---+---+-------------------------------+
 * | N | O | Attribute Type                |
 * +---+---+-------------------------------+
 * N := Carries nested attributes
 * O := Payload stored in network byte order
 *
 * Note: The N and O flag are mutually exclusive.
 */
#define	NLA_F_NESTED		(1 << 15)
#define	NLA_F_NET_BYTEORDER	(1 << 14)
#define	NLA_TYPE_MASK		~(NLA_F_NESTED | NLA_F_NET_BYTEORDER)

#define	NLA_ALIGNTO		4
#define	NLA_ALIGN(len)		(((len) + NLA_ALIGNTO - 1) & ~(NLA_ALIGNTO - 1))
#define	NLA_HDRLEN		((int) NLA_ALIGN(sizeof(struct nlattr)))

#ifdef CONFIG_ACS

//enum fc80211_dfs_state {
	//supp_FC80211_DFS_USABLE,
	//supp_FC80211_DFS_UNAVAILABLE,
	//supp_FC80211_DFS_AVAILABLE,
//};

enum supp_i3ed11_band
{
	supp_NL80211_BAND_2GHZ,
	supp_NL80211_BAND_5GHZ,
	supp_NL80211_BAND_60GHZ,
	supp_NL80211_BAND_6GHZ,
	supp_NL80211_BAND_S1GHZ,
	supp_NL80211_BAND_LC,

	supp_NUM_NL80211_BANDS,
};

struct supp_ieee80211_channel {
	enum supp_i3ed11_band band;
	u32 center_freq;
	u16 freq_offset;
	u16 hw_value;
	u32 flags;
	int max_antenna_gain;
	int max_power;
	int max_reg_power;
	bool beacon_found;
	u32 orig_flags;
	int orig_mag, orig_mpwr;
	enum fc80211_dfs_state dfs_state;
	unsigned long dfs_state_entered;
	unsigned int dfs_cac_ms;
};

struct supp_survey_info {
	struct supp_ieee80211_channel *channel;
	u64 channel_time;
	u64 channel_time_busy;
	u64 channel_time_ext_busy;
	u64 channel_time_rx;
	u64 channel_time_tx;
#if 1	// add
    u64 time_scan;
    u64 time_bss_rx;
#endif
	u32 filled;
	char noise;
};
#endif /* CONFIG_ACS */
/**
 * @ingroup attr
 * Basic attribute data types
 *
 * See section @core_doc{core_attr_parse,Attribute Parsing} for more details.
 */
enum {
	NLA_UNSPEC,		/**< Unspecified type, binary data chunk */
	NLA_U8,			/**< 8 bit integer */
	NLA_U16,		/**< 16 bit integer */
	NLA_U32,		/**< 32 bit integer */
	NLA_U64,		/**< 64 bit integer */
	NLA_STRING,		/**< NUL terminated character string */
	NLA_FLAG,		/**< Flag */
	NLA_MSECS,		/**< Micro seconds (64bit) */
	NLA_NESTED,		/**< Nested attributes */
	__NLA_TYPE_MAX,
};

#define NLA_TYPE_MAX (__NLA_TYPE_MAX - 1)


/**
 * @ingroup msg
 * @defgroup attr Attributes
 * Netlink Attributes Construction/Parsing Interface
 *
 * Related sections in the development guide:
 * - @core_doc{core_attr,Netlink Attributes}
 *
 * @{
 *
 * Header
 * ------
 * ~~~~{.c}
 * #include <netlink/attr.h>
 * ~~~~
 */

/**
 * @name Attribute Size Calculation
 * @{
 */

struct nlmsghdr
{
	u32 nlmsg_len;
	u16 nlmsg_type;
	u16 nlmsg_flags;
	u32 nlmsg_seq;
	u32 nlmsg_pid;
};

struct ifinfomsg
{
	unsigned char ifi_family;
	unsigned char __ifi_pad;
	unsigned short ifi_type;
	int ifi_index;
	unsigned ifi_flags;
	unsigned ifi_change;
};

struct rtattr
{
	unsigned short rta_len;
	unsigned short rta_type;
};

/*
 *  <------- NLA_HDRLEN ------> <-- NLA_ALIGN(payload)-->
 * +---------------------+- - -+- - - - - - - - - -+- - -+
 * |        Header       | Pad |     Payload       | Pad |
 * |   (struct nlattr)   | ing |                   | ing |
 * +---------------------+- - -+- - - - - - - - - -+- - -+
 *  <-------------- nlattr->nla_len -------------->
 */

struct nlattr {
	u16	nla_len;
	u16	nla_type;
};

/**
 * @ingroup attr
 * Attribute validation policy.
 *
 * See section @core_doc{core_attr_parse,Attribute Parsing} for more details.
 */
struct nla_policy {
	/** Type of attribute or NLA_UNSPEC */
	u16	type;

	/** Minimal length of payload required */
	u16	minlen;

	/** Maximal length of payload allowed */
	u16	maxlen;
};

struct nlmsgerr {
	int	error;
	struct nlmsghdr msg;
};

enum {
	NETLINK_UNCONNECTED = 0,
	NETLINK_CONNECTED,
};

/**
 * @ingroup attr
 * Iterate over a stream of attributes
 * @arg pos     loop counter, set to current attribute
 * @arg head    head of attribute stream
 * @arg len     length of attribute stream
 * @arg rem     initialized to len, holds bytes currently remaining in stream
 */
#define nla_for_each_attr(pos, head, len, rem) \
	for (pos = head, rem = len; \
		nla_ok(pos, rem); \
		pos = nla_next(pos, &(rem)))

/**
 * @ingroup attr
 * Iterate over a stream of nested attributes
 * @arg pos     loop counter, set to current attribute
 * @arg nla     attribute containing the nested attributes
 * @arg rem     initialized to len, holds bytes currently remaining in stream
 */
#define nla_for_each_nested(pos, nla, rem) \
	for (pos = nLa_data(nla), rem = nla_len(nla); \
		nla_ok(pos, rem); \
		pos = nla_next(pos, &(rem)))

extern u8	nla_get_u8(struct nlattr *nla);
extern u16	nla_get_u16(struct nlattr *nla);
extern u32	nla_get_u32(struct nlattr *nla);
extern u64	nla_get_u64(struct nlattr *nla);
extern int	nla_len(const struct nlattr *nla);
extern void	*nla_data(const struct nlattr *nla);
extern int	nla_len(const struct nlattr *nla);
extern int	nla_ok(const struct nlattr *nla, int remaining);
extern void	*nla_data(const struct nlattr *nla);
extern struct	nlattr *nla_next(const struct nlattr *nla, int *remaining);

#ifdef CONFIG_ACS
extern int fc80211_dump_survey(int ifindex, int idx, void *get_survey);
#endif /* CONFIG_ACS */

u32 wpa_alg_to_cipher_suite(enum wpa_alg alg, size_t key_len);
int is_ap_interface(enum fc80211_iftype nlmode);


//
// External global functions
//
extern int get_run_mode(void);
#if CFG_PMGR
extern int RM_PMGR_W_dpm_is_enabled(void);
extern int RM_PMGR_W_dpm_is_wakeup(void);
#endif /* CFG_PMGR */

extern void ra6w1_regdb_data_init(char* country);
extern int ra6w1_regdb_get_ch_range_by_country_n_band(char* country, int band, int* min_ch, int* max_ch, unsigned int* ch_bitmap_5g, unsigned int exclude_flags);
extern int ra6w1_regdb_get_init_country(char* country);


//
// External global function for RW driver rs_cfg80211
//

extern void __cfg80211_disconnected(int ifindex, u16 reason, const u8 *ie, unsigned long ie_len, bool locally_generated);
extern int rsdev_cfg80211_associate(struct wpa_driver_fc80211_data *drv, struct wpa_driver_associate_params *param);
extern int rsdev_cfg80211_del_interface(unsigned long wdev_id, int ifindex);
extern int rsdev_cfg80211_del_station(struct wpa_driver_fc80211_data *drv, u8 *mac_addr, int deauth, u16 reason_code);
extern int rsdev_cfg80211_new_interface(unsigned long wdev_id, int ifindex, char *ifname, enum fc80211_iftype type);
extern int rsdev_cfg80211_new_station(struct wpa_driver_fc80211_data *drv, struct hostapd_sta_add_params *arg, struct fc80211_sta_flag_update *upd);
extern int rsdev_cfg80211_scan(unsigned long wdev_id, struct wpa_driver_fc80211_data *drv, struct wpa_driver_scan_params *params);
extern int rsdev_cfg80211_set_default_mgmt_key(int ifindex, struct key_parse *key_p);
extern int rsdev_cfg80211_set_default_beacon_key(int ifindex, struct key_parse *key_p);
extern int rsdev_cfg80211_add_key(int ifindex, struct key_parse *key_p, const u8 *addr);
extern int rsdev_cfg80211_set_key(int ifindex, struct key_parse *key_p, u8 *mac_addr);
extern int rsdev_cfg80211_del_key(int ifindex, struct key_parse *key_p, const u8 *addr);
extern int rsdev_cfg80211_set_pmk(struct cfg80211_pmk_conf *pmk_conf);
extern int rsdev_cfg80211_set_station(struct wpa_driver_fc80211_data *drv, struct hostapd_sta_add_params *arg, struct fc80211_sta_flag_update *upd);
extern int rsdev_cfg80211_tx_mgmt(struct wpa_driver_fc80211_data *drv, unsigned int freq, unsigned int wait, const UCHAR *buf, size_t buf_len, int offchanok, int no_cck, int no_ack, u64 *cookie, const u16 *csa_offs, size_t csa_offs_len);
extern int rsdev_tx_control_port(const u8 *dest, u16 proto, const u8 *buf, size_t len, int no_encrypt, u64 *cookie);

int is_sta_interface(enum fc80211_iftype nlmode);
#ifdef	CONFIG_P2P
int is_p2p_net_interface(enum fc80211_iftype nlmode);
#endif /* CONFIG_P2P */
void fc80211_mark_disconnected(struct wpa_driver_fc80211_data *drv);
int fc80211_get_softmac_index(struct i802_bss *bss);
enum fc80211_iftype fc80211_get_ifmode(struct i802_bss *bss);
int fc80211_get_macaddr(struct i802_bss *bss);
struct fc80211_softmac_data *fc80211_get_softmac_data_ap(struct i802_bss *bss);
void fc80211_put_softmac_data_ap(struct i802_bss *bss);
int wpa_driver_fc80211_get_bssid(void *priv, UCHAR *bssid);
int wpa_driver_fc80211_get_ssid(void *priv, UCHAR *ssid);
void mlme_event_connect(struct wpa_driver_fc80211_data *drv, struct i802_bss *bss, ra6wx_drv_msg_buf_t *drv_msg_buf);
void mlme_event_auth(struct wpa_driver_fc80211_data *drv,
			    const UCHAR *frame, size_t len);
unsigned int fc80211_get_assoc_freq(struct wpa_driver_fc80211_data *drv);
void mlme_event_assoc(struct wpa_driver_fc80211_data *drv,
			    const UCHAR *frame, size_t len);
void mlme_timeout_event(struct wpa_driver_fc80211_data *drv,
				ra6wx_drv_msg_buf_t *drv_msg_buf);
void mlme_event_mgmt(struct i802_bss *bss,
			    ra6wx_drv_msg_buf_t *drv_msg_buf,
			    const UCHAR *frame, size_t len);
void mlme_event_mgmt_tx_status(struct wpa_driver_fc80211_data *drv,
					ra6wx_drv_msg_buf_t *drv_msg_buf,
					const UCHAR *frame,size_t len);
void mlme_event_deauth_disassoc(struct wpa_driver_fc80211_data *drv,
				       enum wpa_event_type type,
				       const UCHAR *frame, size_t len);
UINT get_fc80211_protocol_features(struct wpa_driver_fc80211_data *drv);
int wpa_driver_fc80211_capa(struct wpa_driver_fc80211_data *drv);
void * wpa_driver_fc80211_drv_init(void *ctx, const char *ifname,
					  void *global_priv, int hostapd,
					  const UCHAR *set_addr);
void * wpa_driver_fc80211_init(void *ctx, const char *ifname,
				      void *global_priv);
int fc80211_register_action_frame(struct i802_bss *bss,
					 const UCHAR *match, size_t match_len);
int fc80211_mgmt_subscribe_non_ap(struct i802_bss *bss);
int fc80211_register_spurious_class3(struct i802_bss *bss);
int fc80211_mgmt_subscribe_ap(struct i802_bss *bss);
int fc80211_mgmt_subscribe_ap_dev_sme(struct i802_bss *bss);
void fc80211_mgmt_unsubscribe(struct i802_bss *bss, const char *reason);
#ifdef	CONFIG_P2P
void fc80211_del_p2pdev(struct i802_bss *bss);
int fc80211_set_p2pdev(struct i802_bss *bss, int start);
#endif	/* CONFIG_P2P */
int wpa_driver_fc80211_del_beacon(struct wpa_driver_fc80211_data *drv);
int wpa_driver_fc80211_scan(struct i802_bss *bss,
				   struct wpa_driver_scan_params *params);
struct wpa_scan_results *
fc80211_get_scan_results(struct wpa_driver_fc80211_data *drv);
struct wpa_scan_results *
wpa_driver_fc80211_get_scan_results(void *priv);
int wpa_driver_fc80211_set_country(void *priv, const char *alpha2);
int wpa_driver_fc80211_get_country(void *priv, char *alpha2);
int wpa_driver_fc80211_free_umac_scan_results(void *priv);
void fc80211_dump_scan(struct wpa_driver_fc80211_data *drv);
int wpa_driver_fc80211_set_key(struct i802_bss *bss,
				      struct wpa_driver_set_key_params *params);
int fc80211_set_conn_keys(struct wpa_driver_associate_params *params,
				int *privacy);
int wpa_driver_fc80211_disconnect(struct wpa_driver_fc80211_data *drv,
					 int reason_code);
int wpa_driver_fc80211_deauthenticate(struct i802_bss *bss,
					     const UCHAR *addr, int reason_code);
void fc80211_copy_auth_params(struct wpa_driver_fc80211_data *drv,
				     struct wpa_driver_auth_params *params);
const char * wpa_auth_type_txt(enum fc80211_auth_type type);
int wpa_driver_fc80211_auth(struct i802_bss *bss, struct wpa_driver_auth_params *params);
struct hostapd_hw_modes *
wpa_driver_fc80211_postprocess_modes(struct hostapd_hw_modes *modes,
				     USHORT *num_modes);
struct hostapd_hw_modes *
wpa_driver_fc80211_get_hw_feature_data(void *priv, USHORT *num_modes, USHORT *flags);
int wpa_driver_fc80211_send_frame(struct i802_bss *bss,
					 const void *data, size_t len,
					 int encrypt, int noack,
					 unsigned int freq, int no_cck,
					 int offchanok, unsigned int wait_time,
					 const u16 *csa_offs, size_t csa_offs_len);
int wpa_driver_fc80211_send_mlme( struct i802_bss *bss, const u8 *data,
						size_t data_len, int noack,
						unsigned int freq, int no_cck,
						int offchanok,
						unsigned int wait_time,
						const u16 *csa_offs,
						size_t csa_offs_len, int no_encrypt);
int fc80211_bss_set(struct i802_bss *bss, int cts, int preamble,
			   int slot, int ht_opmode, int ap_isolate,
			   int *basic_rates);
int wpa_driver_fc80211_set_acl(void *priv,
				      struct hostapd_acl_params *params);
#ifdef CONFIG_AP_ISOLATION
int wpa_driver_fc80211_set_ap_isolate(void *priv,
			      int isolate);
#endif /* CONFIG_AP_ISOLATION */
int wpa_driver_fc80211_set_ap(void *priv,
			      struct wpa_driver_ap_params *params);
UINT sta_flags_fc80211(int flags);
int wpa_driver_fc80211_sta_add(void *priv,
				      struct hostapd_sta_add_params *params);
int wpa_driver_fc80211_sta_remove(struct i802_bss *bss, const UCHAR *addr, int deauth, u16 reason_code);
void fc80211_remove_iface(struct wpa_driver_fc80211_data *drv,
				 int ifidx);
int fc80211_create_iface_once(struct wpa_driver_fc80211_data *drv,
				     const char *ifname,
				     enum fc80211_iftype iftype,
				     const UCHAR *addr, int wds,
				     int (*handler)(void *),
				     void *arg);
int fc80211_create_iface(struct wpa_driver_fc80211_data *drv,
				const char *ifname,
				enum fc80211_iftype iftype,
				const UCHAR *addr,
				int wds,
				int (*handler)(void *),
				void *arg,
				int use_existing);
#ifdef	CONFIG_AP
int fc80211_setup_ap(struct i802_bss *bss);
void fc80211_teardown_ap(struct i802_bss *bss);
#endif	/* CONFIG_AP */
int fc80211_tx_control_port(void *priv, const u8 *dest,
				   u16 proto, const u8 *buf, size_t len,
				   int no_encrypt);
#ifdef	IEEE8021X_EAPOL
int fc80211_send_eapol_data(struct i802_bss *bss,
				   const UCHAR *addr, const UCHAR *data,
				   size_t data_len);
int wpa_driver_fc80211_hapd_send_eapol(
	void *priv, const UCHAR *addr, const UCHAR *data,
	size_t data_len, int encrypt, const UCHAR *own_addr, UINT flags);
#endif	/* IEEE8021X_EAPOL */
int wpa_driver_fc80211_sta_set_flags(void *priv, const UCHAR *addr,
					    unsigned int total_flags,
					    unsigned int flags_or, unsigned int flags_and);
#ifdef	CONFIG_AP
int wpa_driver_fc80211_ap(struct wpa_driver_fc80211_data *drv,
				 struct wpa_driver_associate_params *params);
int fc80211_ap_pwrsave(void *priv, int ps_state, int timeout);
int fc80211_addba_reject(void *priv, u8 addba_reject);
#endif	/* CONFIG_AP */
int wpa_driver_fc80211_try_connect(
	struct wpa_driver_fc80211_data *drv,
	struct wpa_driver_associate_params *params);
int wpa_driver_fc80211_connect(
	struct wpa_driver_fc80211_data *drv,
	struct wpa_driver_associate_params *params);
int wpa_driver_fc80211_associate(
	void *priv, struct wpa_driver_associate_params *params);
int fc80211_set_mode(struct i802_bss *bss,
			    int ifindex, enum fc80211_iftype mode);
int wpa_driver_fc80211_get_capa(void *priv, struct wpa_driver_capa *capa);
int wpa_driver_fc80211_set_supp_port(void *priv, int authorized);
int i802_set_freq(void *priv, struct hostapd_freq_params *freq);
int i802_get_seqnum(const char *iface, void *priv, const UCHAR *addr,
			   int idx, UCHAR *seq);
int i802_set_rts(void *priv, int rts);
int i802_get_rts(void *priv);
int i802_set_retry(void *priv, u8 retry, u8 retry_long);
int i802_get_retry(void *priv, u8 retry_long);
int i802_flush(void *priv);
#ifdef CONFIG_AP_POWER
int i802_set_ap_power(struct i802_bss *bss, int type, int power, int *get_power);
#endif /* CONFIG_AP_POWER */
int i802_read_sta_data(struct i802_bss *bss,
		       struct hostap_sta_driver_data *data, const UCHAR *addr);
int i802_set_tx_queue_params(void *priv, int queue, int aifs,
				    int cwmin, int cwmax, int burst_time);
int i802_sta_clear_stats(void *priv, const UCHAR *addr);
int i802_sta_deauth(void *priv, const UCHAR *own_addr, const UCHAR *addr,
		    u16 reason);
int i802_sta_disassoc(void *priv, const UCHAR *own_addr, const UCHAR *addr,
		      u16  reason);
int i802_chk_deauth_send_done(void *priv);
#ifdef  CONFIG_AP /* by Shingu 20161010 (Keep-alive) */
int i802_chk_keep_alive(void *priv, const UCHAR *addr);
#endif  /* CONFIG_AP */
void dump_ifidx(struct wpa_driver_fc80211_data *drv);
int i802_check_bridge(struct wpa_driver_fc80211_data *drv,
			     struct i802_bss *bss,
			     const char *brname, const char *ifname);
void *i802_init(struct hostapd_data *hapd,
		       struct wpa_init_params *params);
void i802_deinit(void *priv);
enum fc80211_iftype wpa_driver_fc80211_if_type(
	enum wpa_driver_if_type type);
#ifdef CONFIG_P2P
int fc80211_addr_in_use(struct fc80211_global *global, const UCHAR *addr);
int fc80211_p2p_interface_addr(struct wpa_driver_fc80211_data *drv,
				      UCHAR *new_addr);
#endif /* CONFIG_P2P */
int fc80211_wdev_handler(void *arg);
int wpa_driver_fc80211_if_add(void *priv, enum wpa_driver_if_type type,
				     const char *ifname, const UCHAR *addr,
				     void *bss_ctx, void **drv_priv,
				     char *force_ifname, UCHAR *if_addr,
				     const char *bridge, int use_existing);
int wpa_driver_fc80211_send_action(struct i802_bss *bss,
					  unsigned int freq,
					  unsigned int wait_time,
					  const UCHAR *dst, const UCHAR *src,
					  const UCHAR *bssid,
					  const UCHAR *data, size_t data_len,
					  int no_cck);
void wpa_driver_fc80211_send_action_cancel_wait(void *priv);
int wpa_driver_fc80211_remain_on_channel(void *priv, unsigned int freq,
						unsigned int duration);
int wpa_driver_fc80211_cancel_remain_on_channel(void *priv);
int wpa_driver_fc80211_deinit_ap(void *priv);
#ifdef	CONFIG_P2P
int wpa_driver_fc80211_deinit_p2p_cli(void *priv);
#endif	/* CONFIG_P2P */
void wpa_driver_fc80211_resume(void *priv);
int fc80211_get_link_signal(struct wpa_driver_fc80211_data *drv,
			    struct wpa_signal_info *sig);
int fc80211_get_link_noise(struct wpa_driver_fc80211_data *drv,
			   struct wpa_signal_info *sig_change);
int fc80211_signal_monitor(void *priv, int threshold, int hysteresis);
int wpa_driver_fc80211_shared_freq(void *priv);
int fc80211_send_frame(void *priv, const UCHAR *data, size_t data_len,
			      int encrypt);
int fc80211_set_param(void *priv, const char *param);
void * fc80211_global_init(void);
const char * fc80211_get_radio_name(void *priv);
int driver_fc80211_set_key(void *priv, struct wpa_driver_set_key_params *params);
int driver_fc80211_scan2(void *priv, struct wpa_driver_scan_params *params);
int driver_fc80211_deauthenticate(void *priv, const UCHAR *addr, u16 reason);
int driver_fc80211_authenticate(void *priv,
				       struct wpa_driver_auth_params *params);
void driver_fc80211_deinit(void *priv);
int driver_fc80211_if_remove(void *priv, enum wpa_driver_if_type type,
				    const char *ifname);
int driver_fc80211_send_mlme(void *priv, const u8 *data,
						size_t data_len, int noack,
						unsigned int freq,
						const u16 *csa_offs, size_t csa_offs_len,
						int no_encrypt, unsigned int wait);
int driver_fc80211_sta_remove(void *priv, const UCHAR *addr);
#ifdef CONFIG_STA_POWER_SAVE
int fc80211_sta_pwrsave(void *priv, int pwrsave_mode);
#endif /* CONFIG_STA_POWER_SAVE */
#ifdef CONFIG_AP_NONE_STA
int driver_fc80211_ap_none_station(void *priv);
#endif /* CONFIG_AP_NONE_STA */
#ifdef CONFIG_AP_POWER
int driver_fc80211_set_ap_power(void *priv, int type, int power, int *get_power);
#endif /* CONFIG_AP_POWER */
int driver_fc80211_read_sta_data(void *priv,
					struct hostap_sta_driver_data *data,
					const UCHAR *addr);
int driver_fc80211_send_action(void *priv, unsigned int freq,
				      unsigned int wait_time,
				      const UCHAR *dst, const UCHAR *src,
				      const UCHAR *bssid,
				      const UCHAR *data, size_t data_len,
				      int no_cck);
int driver_fc80211_probe_req_report(void *priv, int report);
const UCHAR *wpa_driver_fc80211_get_macaddr(void *priv);
int fc80211_get_dump_survey(void *priv, unsigned int channel);
void send_scan_event(struct wpa_driver_fc80211_data *drv,
						int aborted, ra6wx_drv_msg_buf_t *drv_msg_buf);
void mlme_event(struct wpa_driver_fc80211_data *drv,
					struct i802_bss *bss, ra6wx_drv_msg_buf_t *drv_msg_buf);
void mlme_event_disconnect(struct wpa_driver_fc80211_data *drv, struct i802_bss *bss,
					ra6wx_drv_msg_buf_t *drv_msg_buf);
int driver_fc80211_get_rtctimegap(void *priv, u64 *rtctimegap);
int driver_fc80211_send_external_auth_status(void *priv,
					     struct external_auth *params);
void fc80211_set_ampdu_mode(int mode, int val);
int fc80211_get_ampdu_mode(int mode);
const char * fc80211_cmd_to_string(enum fc80211_commands cmd);
void process_drv_event(struct i802_bss *bss, ra6wx_drv_msg_buf_t *drv_msg_buf);



#endif	/* __DRIVER_FC80211_H__ */

/* EOF */
