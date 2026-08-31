#ifndef	__FC80211_COPY_H__
#define	__FC80211_COPY_H__


/*
 * 802.11 netlink interface public header
 *
 * Copyright 2006-2010 Johannes Berg <johannes@sipsolutions.net>
 * Copyright 2008 Michael Wu <flamingice@sourmilk.net>
 * Copyright 2008 Luis Carlos Cobo <luisca@cozybit.com>
 * Copyright 2008 Michael Buesch <m@bues.ch>
 * Copyright 2008, 2009 Luis R. Rodriguez <lrodriguez@atheros.com>
 * Copyright 2008 Jouni Malinen <jouni.malinen@atheros.com>
 * Copyright 2008 Colin McCabe <colin@cozybit.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include "utils/supp_common.h"

/**
 * DOC: Station handling
 *
 * Stations are added per interface, but a special case exists with VLAN
 * interfaces. When a station is bound to an AP interface, it may be moved
 * into a VLAN identified by a VLAN interface index (%FC80211_ATTR_STA_VLAN).
 * The station is still assumed to belong to the AP interface it was added
 * to.
 *
 * Station handling varies per interface type and depending on the driver's
 * capabilities.
 *
 * For drivers supporting TDLS with external setup (SOFTMAC_FLAG_SUPPORTS_TDLS
 * and SOFTMAC_FLAG_TDLS_EXTERNAL_SETUP), the station lifetime is as follows:
 *  - a setup station entry is added, not yet authorized, without any rate
 *    or capability information, this just exists to avoid race conditions
 *  - when the TDLS setup is done, a single FC80211_CMD_SET_STATION is valid
 *    to add rate and capability information to the station and at the same
 *    time mark it authorized.
 *  - %FC80211_TDLS_ENABLE_LINK is then used
 *  - after this, the only valid operation is to remove it by tearing down
 *    the TDLS link (%FC80211_TDLS_DISABLE_LINK)
 *
 * TODO: need more info for other interface types
 */

/**
 * DOC: Frame transmission/registration support
 *
 * Frame transmission and registration support exists to allow userspace
 * management entities such as wpa_supplicant react to management frames
 * that are not being handled by the kernel. This includes, for example,
 * certain classes of action frames that cannot be handled in the kernel
 * for various reasons.
 *
 * Frame registration is done on a per-interface basis and registrations
 * cannot be removed other than by closing the socket. It is possible to
 * specify a registration filter to register, for example, only for a
 * certain type of action frame. In particular with action frames, those
 * that userspace registers for will not be returned as unhandled by the
 * driver, so that the registered application has to take responsibility
 * for doing that.
 *
 * The type of frame that can be registered for is also dependent on the
 * driver and interface type. The frame types are advertised in softmac
 * attributes so applications know what to expect.
 *
 * NOTE: When an interface changes type while registrations are active,
 *       these registrations are ignored until the interface type is
 *       changed again. This means that changing the interface type can
 *       lead to a situation that couldn't otherwise be produced, but
 *       any such registrations will be dormant in the sense that they
 *       will not be serviced, i.e. they will not receive any frames.
 *
 * Frame transmission allows userspace to send for example the required
 * responses to action frames. It is subject to some sanity checking,
 * but many frames can be transmitted. When a frame was transmitted, its
 * status is indicated to the sending socket.
 *
 * For more technical details, see the corresponding command descriptions
 * below.
 */

/**
 * DOC: Virtual interface / concurrency capabilities
 *
 * Some devices are able to operate with virtual MACs, they can have
 * more than one virtual interface. The capability handling for this
 * is a bit complex though, as there may be a number of restrictions
 * on the types of concurrency that are supported.
 *
 * To start with, each device supports the interface types listed in
 * the %FC80211_ATTR_SUPPORTED_IFTYPES attribute, but by listing the
 * types there no concurrency is implied.
 *
 * Once concurrency is desired, more attributes must be observed:
 * To start with, since some interface types are purely managed in
 * software, like the AP-VLAN type in macd11 for example, there's
 * an additional list of these, they can be added at any time and
 * are only restricted by some semantic restrictions (e.g. AP-VLAN
 * cannot be added without a corresponding AP interface). This list
 * is exported in the %FC80211_ATTR_SOFTWARE_IFTYPES attribute.
 *
 * Further, the list of supported combinations is exported. This is
 * in the %FC80211_ATTR_INTERFACE_COMBINATIONS attribute. Basically,
 * it exports a list of "groups", and at any point in time the
 * interfaces that are currently active must fall into any one of
 * the advertised groups. Within each group, there are restrictions
 * on the number of interfaces of different types that are supported
 * and also the number of different channels, along with potentially
 * some other restrictions. See &enum fc80211_if_combination_attrs.
 *
 * All together, these attributes define the concurrency of virtual
 * interfaces that a given device supports.
 */

/**
 * DOC: packet coalesce support
 *
 * In most cases, host that receives IPv4 and IPv6 multicast/broadcast
 * packets does not do anything with these packets. Therefore the
 * reception of these unwanted packets causes unnecessary processing
 * and power consumption.
 *
 * Packet coalesce feature helps to reduce number of received interrupts
 * to host by buffering these packets in firmware/hardware for some
 * predefined time. Received interrupt will be generated when one of the
 * following events occur.
 * a) Expiration of hardware timer whose expiration time is set to maximum
 * coalescing delay of matching coalesce rule.
 * b) Coalescing buffer in hardware reaches it's limit.
 * c) Packet doesn't match any of the configured coalesce rules.
 *
 * User needs to configure following parameters for creating a coalesce
 * rule.
 * a) Maximum coalescing delay
 * b) List of packet patterns which needs to be matched
 * c) Condition for coalescence. pattern 'match' or 'no match'
 * Multiple such rules can be created.
 */

/**
 * enum fc80211_commands - supported fc80211 commands
 *
 * @FC80211_CMD_UNSPEC: unspecified command to catch errors
 *
 * @FC80211_CMD_GET_SOFTMAC: request information about a softmac or dump request
 *	to get a list of all present softmacs.
 * @FC80211_CMD_SET_SOFTMAC: set softmac parameters, needs %FC80211_ATTR_SOFTMAC or
 *	%FC80211_ATTR_IFINDEX; can be used to set %FC80211_ATTR_SOFTMAC_NAME,
 *	%FC80211_ATTR_SOFTMAC_TXQ_PARAMS, %FC80211_ATTR_SOFTMAC_FREQ (and the
 *	attributes determining the channel width; this is used for setting
 *	monitor mode channel),  %FC80211_ATTR_SOFTMAC_RETRY_SHORT,
 *	%FC80211_ATTR_SOFTMAC_RETRY_LONG, %FC80211_ATTR_SOFTMAC_FRAG_THRESHOLD,
 *	and/or %FC80211_ATTR_SOFTMAC_RTS_THRESHOLD.
 *	However, for setting the channel, see %FC80211_CMD_SET_CHANNEL
 *	instead, the support here is for backward compatibility only.
 * @FC80211_CMD_NEW_SOFTMAC: Newly created softmac, response to get request
 *	or rename notification. Has attributes %FC80211_ATTR_SOFTMAC and
 *	%FC80211_ATTR_SOFTMAC_NAME.
 * @FC80211_CMD_DEL_SOFTMAC: Wiphy deleted. Has attributes
 *	%FC80211_ATTR_SOFTMAC and %FC80211_ATTR_SOFTMAC_NAME.
 *
 * @FC80211_CMD_GET_INTERFACE: Request an interface's configuration;
 *	either a dump request on a %FC80211_ATTR_SOFTMAC or a specific get
 *	on an %FC80211_ATTR_IFINDEX is supported.
 * @FC80211_CMD_SET_INTERFACE: Set type of a virtual interface, requires
 *	%FC80211_ATTR_IFINDEX and %FC80211_ATTR_IFTYPE.
 * @FC80211_CMD_NEW_INTERFACE: Newly created virtual interface or response
 *	to %FC80211_CMD_GET_INTERFACE. Has %FC80211_ATTR_IFINDEX,
 *	%FC80211_ATTR_SOFTMAC and %FC80211_ATTR_IFTYPE attributes. Can also
 *	be sent from userspace to request creation of a new virtual interface,
 *	then requires attributes %FC80211_ATTR_SOFTMAC, %FC80211_ATTR_IFTYPE and
 *	%FC80211_ATTR_IFNAME.
 * @FC80211_CMD_DEL_INTERFACE: Virtual interface was deleted, has attributes
 *	%FC80211_ATTR_IFINDEX and %FC80211_ATTR_SOFTMAC. Can also be sent from
 *	userspace to request deletion of a virtual interface, then requires
 *	attribute %FC80211_ATTR_IFINDEX.
 *
 * @FC80211_CMD_GET_KEY: Get sequence counter information for a key specified
 *	by %FC80211_ATTR_KEY_IDX and/or %FC80211_ATTR_MAC.
 * @FC80211_CMD_SET_KEY: Set key attributes %FC80211_ATTR_KEY_DEFAULT,
 *	%FC80211_ATTR_KEY_DEFAULT_MGMT, or %FC80211_ATTR_KEY_THRESHOLD.
 * @FC80211_CMD_NEW_KEY: add a key with given %FC80211_ATTR_KEY_DATA,
 *	%FC80211_ATTR_KEY_IDX, %FC80211_ATTR_MAC, %FC80211_ATTR_KEY_CIPHER,
 *	and %FC80211_ATTR_KEY_SEQ attributes.
 * @FC80211_CMD_DEL_KEY: delete a key identified by %FC80211_ATTR_KEY_IDX
 *	or %FC80211_ATTR_MAC.
 *
 * @FC80211_CMD_GET_BEACON: (not used)
 * @FC80211_CMD_SET_BEACON: change the beacon on an access point interface
 *	using the %FC80211_ATTR_BEACON_HEAD and %FC80211_ATTR_BEACON_TAIL
 *	attributes. For drivers that generate the beacon and probe responses
 *	internally, the following attributes must be provided: %FC80211_ATTR_IE,
 *	%FC80211_ATTR_IE_PROBE_RESP and %FC80211_ATTR_IE_ASSOC_RESP.
 * @FC80211_CMD_START_AP: Start AP operation on an AP interface, parameters
 *	are like for %FC80211_CMD_SET_BEACON, and additionally parameters that
 *	do not change are used, these include %FC80211_ATTR_BEACON_INTERVAL,
 *	%FC80211_ATTR_DTIM_PERIOD, %FC80211_ATTR_SSID,
 *	%FC80211_ATTR_HIDDEN_SSID, %FC80211_ATTR_CIPHERS_PAIRWISE,
 *	%FC80211_ATTR_CIPHER_GROUP, %FC80211_ATTR_WPA_VERSIONS,
 *	%FC80211_ATTR_AKM_SUITES, %FC80211_ATTR_PRIVACY,
 *	%FC80211_ATTR_AUTH_TYPE, %FC80211_ATTR_INACTIVITY_TIMEOUT,
 *	%FC80211_ATTR_ACL_POLICY and %FC80211_ATTR_MAC_ADDRS.
 *	The channel to use can be set on the interface or be given using the
 *	%FC80211_ATTR_SOFTMAC_FREQ and the attributes determining channel width.
 * @FC80211_CMD_NEW_BEACON: old alias for %FC80211_CMD_START_AP
 * @FC80211_CMD_STOP_AP: Stop AP operation on the given interface
 * @FC80211_CMD_DEL_BEACON: old alias for %FC80211_CMD_STOP_AP
 *
 * @FC80211_CMD_GET_STATION: Get station attributes for station identified by
 *	%FC80211_ATTR_MAC on the interface identified by %FC80211_ATTR_IFINDEX.
 * @FC80211_CMD_SET_STATION: Set station attributes for station identified by
 *	%FC80211_ATTR_MAC on the interface identified by %FC80211_ATTR_IFINDEX.
 * @FC80211_CMD_NEW_STATION: Add a station with given attributes to the
 *	the interface identified by %FC80211_ATTR_IFINDEX.
 * @FC80211_CMD_DEL_STATION: Remove a station identified by %FC80211_ATTR_MAC
 *	or, if no MAC address given, all stations, on the interface identified
 *	by %FC80211_ATTR_IFINDEX.
 *
 * @FC80211_CMD_GET_MPATH: Get mesh path attributes for mesh path to
 * 	destination %FC80211_ATTR_MAC on the interface identified by
 * 	%FC80211_ATTR_IFINDEX.
 * @FC80211_CMD_SET_MPATH:  Set mesh path attributes for mesh path to
 * 	destination %FC80211_ATTR_MAC on the interface identified by
 * 	%FC80211_ATTR_IFINDEX.
 * @FC80211_CMD_NEW_MPATH: Create a new mesh path for the destination given by
 *	%FC80211_ATTR_MAC via %FC80211_ATTR_MPATH_NEXT_HOP.
 * @FC80211_CMD_DEL_MPATH: Delete a mesh path to the destination given by
 *	%FC80211_ATTR_MAC.
 * @FC80211_CMD_NEW_PATH: Add a mesh path with given attributes to the
 *	the interface identified by %FC80211_ATTR_IFINDEX.
 * @FC80211_CMD_DEL_PATH: Remove a mesh path identified by %FC80211_ATTR_MAC
 *	or, if no MAC address given, all mesh paths, on the interface identified
 *	by %FC80211_ATTR_IFINDEX.
 * @FC80211_CMD_SET_BSS: Set BSS attributes for BSS identified by
 *	%FC80211_ATTR_IFINDEX.
 *
 * @FC80211_CMD_GET_REG: ask the wireless core to send us its currently set
 * 	regulatory domain.
 * @FC80211_CMD_SET_REG: Set current regulatory domain. CRDA sends this command
 *	after being queried by the kernel. CRDA replies by sending a regulatory
 *	domain structure which consists of %FC80211_ATTR_REG_ALPHA set to our
 *	current alpha2 if it found a match. It also provides
 * 	FC80211_ATTR_REG_RULE_FLAGS, and a set of regulatory rules. Each
 * 	regulatory rule is a nested set of attributes  given by
 * 	%FC80211_ATTR_REG_RULE_FREQ_[START|END] and
 * 	%FC80211_ATTR_FREQ_RANGE_MAX_BW with an attached power rule given by
 * 	%FC80211_ATTR_REG_RULE_POWER_MAX_ANT_GAIN and
 * 	%FC80211_ATTR_REG_RULE_POWER_MAX_EIRP.
 * @FC80211_CMD_REQ_SET_REG: ask the wireless core to set the regulatory domain
 * 	to the specified ISO/IEC 3166-1 alpha2 country code. The core will
 * 	store this as a valid request and then query userspace for it.
 *
 * @FC80211_CMD_GET_MESH_CONFIG: Get mesh networking properties for the
 *	interface identified by %FC80211_ATTR_IFINDEX
 *
 * @FC80211_CMD_SET_MESH_CONFIG: Set mesh networking properties for the
 *      interface identified by %FC80211_ATTR_IFINDEX
 *
 * @FC80211_CMD_SET_MGMT_EXTRA_IE: Set extra IEs for management frames. The
 *	interface is identified with %FC80211_ATTR_IFINDEX and the management
 *	frame subtype with %FC80211_ATTR_MGMT_SUBTYPE. The extra IE data to be
 *	added to the end of the specified management frame is specified with
 *	%FC80211_ATTR_IE. If the command succeeds, the requested data will be
 *	added to all specified management frames generated by
 *	kernel/firmware/driver.
 *	Note: This command has been removed and it is only reserved at this
 *	point to avoid re-using existing command number. The functionality this
 *	command was planned for has been provided with cleaner design with the
 *	option to specify additional IEs in FC80211_CMD_TRIGGER_SCAN,
 *	FC80211_CMD_AUTHENTICATE, FC80211_CMD_ASSOCIATE,
 *	FC80211_CMD_DEAUTHENTICATE, and FC80211_CMD_DISASSOCIATE.
 *
 * @FC80211_CMD_GET_SCAN: get scan results
 * @FC80211_CMD_TRIGGER_SCAN: trigger a new scan with the given parameters
 *	%FC80211_ATTR_TX_NO_CCK_RATE is used to decide whether to send the
 *	probe requests at CCK rate or not.
 * @FC80211_CMD_NEW_SCAN_RESULTS: scan notification (as a reply to
 *	FC80211_CMD_GET_SCAN and on the "scan" multicast group)
 * @FC80211_CMD_SCAN_ABORTED: scan was aborted, for unspecified reasons,
 *	partial scan results may be available
 *
 * @FC80211_CMD_START_SCHED_SCAN: start a scheduled scan at certain
 *	intervals, as specified by %FC80211_ATTR_SCHED_SCAN_INTERVAL.
 *	Like with normal scans, if SSIDs (%FC80211_ATTR_SCAN_SSIDS)
 *	are passed, they are used in the probe requests.  For
 *	broadcast, a broadcast SSID must be passed (ie. an empty
 *	string).  If no SSID is passed, no probe requests are sent and
 *	a passive scan is performed.  %FC80211_ATTR_SCAN_FREQUENCIES,
 *	if passed, define which channels should be scanned; if not
 *	passed, all channels allowed for the current regulatory domain
 *	are used.  Extra IEs can also be passed from the userspace by
 *	using the %FC80211_ATTR_IE attribute.
 * @FC80211_CMD_STOP_SCHED_SCAN: stop a scheduled scan. Returns -ENOENT if
 *	scheduled scan is not running. The caller may assume that as soon
 *	as the call returns, it is safe to start a new scheduled scan again.
 * @FC80211_CMD_SCHED_SCAN_RESULTS: indicates that there are scheduled scan
 *	results available.
 * @FC80211_CMD_SCHED_SCAN_STOPPED: indicates that the scheduled scan has
 *	stopped.  The driver may issue this event at any time during a
 *	scheduled scan.  One reason for stopping the scan is if the hardware
 *	does not support starting an association or a normal scan while running
 *	a scheduled scan.  This event is also sent when the
 *	%FC80211_CMD_STOP_SCHED_SCAN command is received or when the interface
 *	is brought down while a scheduled scan was running.
 *
 * @FC80211_CMD_GET_SURVEY: get survey resuls, e.g. channel occupation
 *      or noise level
 * @FC80211_CMD_NEW_SURVEY_RESULTS: survey data notification (as a reply to
 *	FC80211_CMD_GET_SURVEY and on the "scan" multicast group)
 *
 * @FC80211_CMD_SET_PMKSA: Add a PMKSA cache entry, using %FC80211_ATTR_MAC
 *	(for the BSSID) and %FC80211_ATTR_PMKID.
 * @FC80211_CMD_DEL_PMKSA: Delete a PMKSA cache entry, using %FC80211_ATTR_MAC
 *	(for the BSSID) and %FC80211_ATTR_PMKID.
 * @FC80211_CMD_FLUSH_PMKSA: Flush all PMKSA cache entries.
 *
 * @FC80211_CMD_REG_CHANGE: indicates to userspace the regulatory domain
 * 	has been changed and provides details of the request information
 * 	that caused the change such as who initiated the regulatory request
 * 	(%FC80211_ATTR_REG_INITIATOR), the softmac_idx
 * 	(%FC80211_ATTR_REG_ALPHA2) on which the request was made from if
 * 	the initiator was %FC80211_REGDOM_SET_BY_COUNTRY_IE or
 * 	%FC80211_REGDOM_SET_BY_DRIVER, the type of regulatory domain
 * 	set (%FC80211_ATTR_REG_TYPE), if the type of regulatory domain is
 * 	%FC80211_REG_TYPE_COUNTRY the alpha2 to which we have moved on
 * 	to (%FC80211_ATTR_REG_ALPHA2).
 * @FC80211_CMD_REG_BEACON_HINT: indicates to userspace that an AP beacon
 * 	has been found while world roaming thus enabling active scan or
 * 	any mode of operation that initiates TX (beacons) on a channel
 * 	where we would not have been able to do either before. As an example
 * 	if you are world roaming (regulatory domain set to world or if your
 * 	driver is using a custom world roaming regulatory domain) and while
 * 	doing a passive scan on the 5 GHz band you find an AP there (if not
 * 	on a DFS channel) you will now be able to actively scan for that AP
 * 	or use AP mode on your card on that same channel. Note that this will
 * 	never be used for channels 1-11 on the 2 GHz band as they are always
 * 	enabled world wide. This beacon hint is only sent if your device had
 * 	either disabled active scanning or beaconing on a channel. We send to
 * 	userspace the softmac on which we removed a restriction from
 * 	(%FC80211_ATTR_SOFTMAC) and the channel on which this occurred
 * 	before (%FC80211_ATTR_FREQ_BEFORE) and after (%FC80211_ATTR_FREQ_AFTER)
 * 	the beacon hint was processed.
 *
 * @FC80211_CMD_AUTHENTICATE: authentication request and notification.
 *	This command is used both as a command (request to authenticate) and
 *	as an event on the "mlme" multicast group indicating completion of the
 *	authentication process.
 *	When used as a command, %FC80211_ATTR_IFINDEX is used to identify the
 *	interface. %FC80211_ATTR_MAC is used to specify PeerSTAAddress (and
 *	BSSID in case of station mode). %FC80211_ATTR_SSID is used to specify
 *	the SSID (mainly for association, but is included in authentication
 *	request, too, to help BSS selection. %FC80211_ATTR_SOFTMAC_FREQ is used
 *	to specify the frequence of the channel in MHz. %FC80211_ATTR_AUTH_TYPE
 *	is used to specify the authentication type. %FC80211_ATTR_IE is used to
 *	define IEs (VendorSpecificInfo, but also including RSN IE and FT IEs)
 *	to be added to the frame.
 *	When used as an event, this reports reception of an Authentication
 *	frame in station and IBSS modes when the local MLME processed the
 *	frame, i.e., it was for the local STA and was received in correct
 *	state. This is similar to MLME-AUTHENTICATE.confirm primitive in the
 *	MLME SAP interface (kernel providing MLME, userspace SME). The
 *	included %FC80211_ATTR_FRAME attribute contains the management frame
 *	(including both the header and frame body, but not FCS). This event is
 *	also used to indicate if the authentication attempt timed out. In that
 *	case the %FC80211_ATTR_FRAME attribute is replaced with a
 *	%FC80211_ATTR_TIMED_OUT flag (and %FC80211_ATTR_MAC to indicate which
 *	pending authentication timed out).
 * @FC80211_CMD_ASSOCIATE: association request and notification; like
 *	FC80211_CMD_AUTHENTICATE but for Association and Reassociation
 *	(similar to MLME-ASSOCIATE.request, MLME-REASSOCIATE.request,
 *	MLME-ASSOCIATE.confirm or MLME-REASSOCIATE.confirm primitives).
 * @FC80211_CMD_DEAUTHENTICATE: deauthentication request and notification; like
 *	FC80211_CMD_AUTHENTICATE but for Deauthentication frames (similar to
 *	MLME-DEAUTHENTICATION.request and MLME-DEAUTHENTICATE.indication
 *	primitives).
 * @FC80211_CMD_DISASSOCIATE: disassociation request and notification; like
 *	FC80211_CMD_AUTHENTICATE but for Disassociation frames (similar to
 *	MLME-DISASSOCIATE.request and MLME-DISASSOCIATE.indication primitives).
 *
 * @FC80211_CMD_MICHAEL_MIC_FAILURE: notification of a locally detected Michael
 *	MIC (part of TKIP) failure; sent on the "mlme" multicast group; the
 *	event includes %FC80211_ATTR_MAC to describe the source MAC address of
 *	the frame with invalid MIC, %FC80211_ATTR_KEY_TYPE to show the key
 *	type, %FC80211_ATTR_KEY_IDX to indicate the key identifier, and
 *	%FC80211_ATTR_KEY_SEQ to indicate the TSC value of the frame; this
 *	event matches with MLME-MICHAELMICFAILURE.indication() primitive
 *
 * @FC80211_CMD_JOIN_IBSS: Join a new IBSS -- given at least an SSID and a
 *	FREQ attribute (for the initial frequency if no peer can be found)
 *	and optionally a MAC (as BSSID) and FREQ_FIXED attribute if those
 *	should be fixed rather than automatically determined. Can only be
 *	executed on a network interface that is UP, and fixed BSSID/FREQ
 *	may be rejected. Another optional parameter is the beacon interval,
 *	given in the %FC80211_ATTR_BEACON_INTERVAL attribute, which if not
 *	given defaults to 100 TU (102.4ms).
 * @FC80211_CMD_LEAVE_IBSS: Leave the IBSS -- no special arguments, the IBSS is
 *	determined by the network interface.
 *
 * @FC80211_CMD_TESTMODE: testmode command, takes a softmac (or ifindex) attribute
 *	to identify the device, and the TESTDATA blob attribute to pass through
 *	to the driver.
 *
 * @FC80211_CMD_CONNECT: connection request and notification; this command
 *	requests to connect to a specified network but without separating
 *	auth and assoc steps. For this, you need to specify the SSID in a
 *	%FC80211_ATTR_SSID attribute, and can optionally specify the association
 *	IEs in %FC80211_ATTR_IE, %FC80211_ATTR_AUTH_TYPE, %FC80211_ATTR_USE_MFP,
 *	%FC80211_ATTR_MAC, %FC80211_ATTR_SOFTMAC_FREQ, %FC80211_ATTR_CONTROL_PORT,
 *	%FC80211_ATTR_CONTROL_PORT_ETHERTYPE,
 *	%FC80211_ATTR_CONTROL_PORT_NO_ENCRYPT, %FC80211_ATTR_MAC_HINT, and
 *	%FC80211_ATTR_SOFTMAC_FREQ_HINT.
 *	If included, %FC80211_ATTR_MAC and %FC80211_ATTR_SOFTMAC_FREQ are
 *	restrictions on BSS selection, i.e., they effectively prevent roaming
 *	within the ESS. %FC80211_ATTR_MAC_HINT and %FC80211_ATTR_SOFTMAC_FREQ_HINT
 *	can be included to provide a recommendation of the initial BSS while
 *	allowing the driver to roam to other BSSes within the ESS and also to
 *	ignore this recommendation if the indicated BSS is not ideal. Only one
 *	set of BSSID,frequency parameters is used (i.e., either the enforcing
 *	%FC80211_ATTR_MAC,%FC80211_ATTR_SOFTMAC_FREQ or the less strict
 *	%FC80211_ATTR_MAC_HINT and %FC80211_ATTR_SOFTMAC_FREQ_HINT).
 *	Background scan period can optionally be
 *	specified in %FC80211_ATTR_BG_SCAN_PERIOD,
 *	if not specified default background scan configuration
 *	in driver is used and if period value is 0, bg scan will be disabled.
 *	This attribute is ignored if driver does not support roam scan.
 *	It is also sent as an event, with the BSSID and response IEs when the
 *	connection is established or failed to be established. This can be
 *	determined by the STATUS_CODE attribute.
 * @FC80211_CMD_ROAM: request that the card roam (currently not implemented),
 *	sent as an event when the card/driver roamed by itself.
 * @FC80211_CMD_DISCONNECT: drop a given connection; also used to notify
 *	userspace that a connection was dropped by the AP or due to other
 *	reasons, for this the %FC80211_ATTR_DISCONNECTED_BY_AP and
 *	%FC80211_ATTR_REASON_CODE attributes are used.
 *
 * @FC80211_CMD_SET_SOFTMAC_NETNS: Set a softmac's netns. Note that all devices
 *	associated with this softmac must be down and will follow.
 *
 * @FC80211_CMD_REMAIN_ON_CHANNEL: Request to remain awake on the specified
 *	channel for the specified amount of time. This can be used to do
 *	off-channel operations like transmit a Public Action frame and wait for
 *	a response while being associated to an AP on another channel.
 *	%FC80211_ATTR_IFINDEX is used to specify which interface (and thus
 *	radio) is used. %FC80211_ATTR_SOFTMAC_FREQ is used to specify the
 *	frequency for the operation.
 *	%FC80211_ATTR_DURATION is used to specify the duration in milliseconds
 *	to remain on the channel. This command is also used as an event to
 *	notify when the requested duration starts (it may take a while for the
 *	driver to schedule this time due to other concurrent needs for the
 *	radio).
 *	When called, this operation returns a cookie (%FC80211_ATTR_COOKIE)
 *	that will be included with any events pertaining to this request;
 *	the cookie is also used to cancel the request.
 * @FC80211_CMD_CANCEL_REMAIN_ON_CHANNEL: This command can be used to cancel a
 *	pending remain-on-channel duration if the desired operation has been
 *	completed prior to expiration of the originally requested duration.
 *	%FC80211_ATTR_SOFTMAC or %FC80211_ATTR_IFINDEX is used to specify the
 *	radio. The %FC80211_ATTR_COOKIE attribute must be given as well to
 *	uniquely identify the request.
 *	This command is also used as an event to notify when a requested
 *	remain-on-channel duration has expired.
 *
 * @FC80211_CMD_SET_TX_BITRATE_MASK: Set the mask of rates to be used in TX
 *	rate selection. %FC80211_ATTR_IFINDEX is used to specify the interface
 *	and @FC80211_ATTR_TX_RATES the set of allowed rates.
 *
 * @FC80211_CMD_REGISTER_FRAME: Register for receiving certain mgmt frames
 *	(via @FC80211_CMD_FRAME) for processing in userspace. This command
 *	requires an interface index, a frame type attribute (optional for
 *	backward compatibility reasons, if not given assumes action frames)
 *	and a match attribute containing the first few bytes of the frame
 *	that should match, e.g. a single byte for only a category match or
 *	four bytes for vendor frames including the OUI. The registration
 *	cannot be dropped, but is removed automatically when the netlink
 *	socket is closed. Multiple registrations can be made.
 * @FC80211_CMD_REGISTER_ACTION: Alias for @FC80211_CMD_REGISTER_FRAME for
 *	backward compatibility
 * @FC80211_CMD_FRAME: Management frame TX request and RX notification. This
 *	command is used both as a request to transmit a management frame and
 *	as an event indicating reception of a frame that was not processed in
 *	kernel code, but is for us (i.e., which may need to be processed in a
 *	user space application). %FC80211_ATTR_FRAME is used to specify the
 *	frame contents (including header). %FC80211_ATTR_SOFTMAC_FREQ is used
 *	to indicate on which channel the frame is to be transmitted or was
 *	received. If this channel is not the current channel (remain-on-channel
 *	or the operational channel) the device will switch to the given channel
 *	and transmit the frame, optionally waiting for a response for the time
 *	specified using %FC80211_ATTR_DURATION. When called, this operation
 *	returns a cookie (%FC80211_ATTR_COOKIE) that will be included with the
 *	TX status event pertaining to the TX request.
 *	%FC80211_ATTR_TX_NO_CCK_RATE is used to decide whether to send the
 *	management frames at CCK rate or not in 2GHz band.
 * @FC80211_CMD_FRAME_WAIT_CANCEL: When an off-channel TX was requested, this
 *	command may be used with the corresponding cookie to cancel the wait
 *	time if it is known that it is no longer necessary.
 * @FC80211_CMD_ACTION: Alias for @FC80211_CMD_FRAME for backward compatibility.
 * @FC80211_CMD_FRAME_TX_STATUS: Report TX status of a management frame
 *	transmitted with %FC80211_CMD_FRAME. %FC80211_ATTR_COOKIE identifies
 *	the TX command and %FC80211_ATTR_FRAME includes the contents of the
 *	frame. %FC80211_ATTR_ACK flag is included if the recipient acknowledged
 *	the frame.
 * @FC80211_CMD_ACTION_TX_STATUS: Alias for @FC80211_CMD_FRAME_TX_STATUS for
 *	backward compatibility.
 *
 * @FC80211_CMD_SET_POWER_SAVE: Set powersave, using %FC80211_ATTR_PS_STATE
 * @FC80211_CMD_GET_POWER_SAVE: Get powersave status in %FC80211_ATTR_PS_STATE
 *
 * @FC80211_CMD_SET_CQM: Connection quality monitor configuration. This command
 *	is used to configure connection quality monitoring notification trigger
 *	levels.
 * @FC80211_CMD_NOTIFY_CQM: Connection quality monitor notification. This
 *	command is used as an event to indicate the that a trigger level was
 *	reached.
 * @FC80211_CMD_SET_CHANNEL: Set the channel (using %FC80211_ATTR_SOFTMAC_FREQ
 *	and the attributes determining channel width) the given interface
 *	(identifed by %FC80211_ATTR_IFINDEX) shall operate on.
 *	In case multiple channels are supported by the device, the mechanism
 *	with which it switches channels is implementation-defined.
 *	When a monitor interface is given, it can only switch channel while
 *	no other interfaces are operating to avoid disturbing the operation
 *	of any other interfaces, and other interfaces will again take
 *	precedence when they are used.
 *
 * @FC80211_CMD_SET_WDS_PEER: Set the MAC address of the peer on a WDS interface.
 *
 * @FC80211_CMD_JOIN_MESH: Join a mesh. The mesh ID must be given, and initial
 *	mesh config parameters may be given.
 * @FC80211_CMD_LEAVE_MESH: Leave the mesh network -- no special arguments, the
 *	network is determined by the network interface.
 *
 * @FC80211_CMD_UNPROT_DEAUTHENTICATE: Unprotected deauthentication frame
 *	notification. This event is used to indicate that an unprotected
 *	deauthentication frame was dropped when MFP is in use.
 * @FC80211_CMD_UNPROT_DISASSOCIATE: Unprotected disassociation frame
 *	notification. This event is used to indicate that an unprotected
 *	disassociation frame was dropped when MFP is in use.
 *
 * @FC80211_CMD_NEW_PEER_CANDIDATE: Notification on the reception of a
 *      beacon or probe response from a compatible mesh peer.  This is only
 *      sent while no station information (sta_info) exists for the new peer
 *      candidate and when @FC80211_MESH_SETUP_USERSPACE_AUTH,
 *      @FC80211_MESH_SETUP_USERSPACE_AMPE, or
 *      @FC80211_MESH_SETUP_USERSPACE_MPM is set.  On reception of this
 *      notification, userspace may decide to create a new station
 *      (@FC80211_CMD_NEW_STATION).  To stop this notification from
 *      reoccurring, the userspace authentication daemon may want to create the
 *      new station with the AUTHENTICATED flag unset and maybe change it later
 *      depending on the authentication result.
 *
 * @FC80211_CMD_GET_WOWLAN: get Wake-on-Wireless-LAN (WoWLAN) settings.
 * @FC80211_CMD_SET_WOWLAN: set Wake-on-Wireless-LAN (WoWLAN) settings.
 *	Since wireless is more complex than wired ethernet, it supports
 *	various triggers. These triggers can be configured through this
 *	command with the %FC80211_ATTR_WOWLAN_TRIGGERS attribute. For
 *	more background information, see
 *	http://wireless.kernel.org/en/users/Documentation/WoWLAN.
 *	The @FC80211_CMD_SET_WOWLAN command can also be used as a notification
 *	from the driver reporting the wakeup reason. In this case, the
 *	@FC80211_ATTR_WOWLAN_TRIGGERS attribute will contain the reason
 *	for the wakeup, if it was caused by wireless. If it is not present
 *	in the wakeup notification, the wireless device didn't cause the
 *	wakeup but reports that it was woken up.
 *
 * @FC80211_CMD_SET_REKEY_OFFLOAD: This command is used give the driver
 *	the necessary information for supporting GTK rekey offload. This
 *	feature is typically used during WoWLAN. The configuration data
 *	is contained in %FC80211_ATTR_REKEY_DATA (which is nested and
 *	contains the data in sub-attributes). After rekeying happened,
 *	this command may also be sent by the driver as an MLME event to
 *	inform userspace of the new replay counter.
 *
 * @FC80211_CMD_PMKSA_CANDIDATE: This is used as an event to inform userspace
 *	of PMKSA caching dandidates.
 *
 * @FC80211_CMD_TDLS_OPER: Perform a high-level TDLS command (e.g. link setup).
 *	In addition, this can be used as an event to request userspace to take
 *	actions on TDLS links (set up a new link or tear down an existing one).
 *	In such events, %FC80211_ATTR_TDLS_OPERATION indicates the requested
 *	operation, %FC80211_ATTR_MAC contains the peer MAC address, and
 *	%FC80211_ATTR_REASON_CODE the reason code to be used (only with
 *	%FC80211_TDLS_TEARDOWN).
 * @FC80211_CMD_TDLS_MGMT: Send a TDLS management frame. The
 *	%FC80211_ATTR_TDLS_ACTION attribute determines the type of frame to be
 *	sent. Public Action codes (802.11-2012 8.1.5.1) will be sent as
 *	802.11 management frames, while TDLS action codes (802.11-2012
 *	8.5.13.1) will be encapsulated and sent as data frames. The currently
 *	supported Public Action code is %WLAN_PUB_ACTION_TDLS_DISCOVER_RES
 *	and the currently supported TDLS actions codes are given in
 *	&enum i3ed11_tdls_actioncode.
 *
 * @FC80211_CMD_UNEXPECTED_FRAME: Used by an application controlling an AP
 *	(or GO) interface (i.e. hostapd) to ask for unexpected frames to
 *	implement sending deauth to stations that send unexpected class 3
 *	frames. Also used as the event sent by the kernel when such a frame
 *	is received.
 *	For the event, the %FC80211_ATTR_MAC attribute carries the TA and
 *	other attributes like the interface index are present.
 *	If used as the command it must have an interface index and you can
 *	only unsubscribe from the event by closing the socket. Subscription
 *	is also for %FC80211_CMD_UNEXPECTED_4ADDR_FRAME events.
 *
 * @FC80211_CMD_UNEXPECTED_4ADDR_FRAME: Sent as an event indicating that the
 *	associated station identified by %FC80211_ATTR_MAC sent a 4addr frame
 *	and wasn't already in a 4-addr VLAN. The event will be sent similarly
 *	to the %FC80211_CMD_UNEXPECTED_FRAME event, to the same listener.
 *
 * @FC80211_CMD_PROBE_CLIENT: Probe an associated station on an AP interface
 *	by sending a null data frame to it and reporting when the frame is
 *	acknowleged. This is used to allow timing out inactive clients. Uses
 *	%FC80211_ATTR_IFINDEX and %FC80211_ATTR_MAC. The command returns a
 *	direct reply with an %FC80211_ATTR_COOKIE that is later used to match
 *	up the event with the request. The event includes the same data and
 *	has %FC80211_ATTR_ACK set if the frame was ACKed.
 *
 * @FC80211_CMD_REGISTER_BEACONS: Register this socket to receive beacons from
 *	other BSSes when any interfaces are in AP mode. This helps implement
 *	OLBC handling in hostapd. Beacons are reported in %FC80211_CMD_FRAME
 *	messages. Note that per PHY only one application may register.
 *
 * @FC80211_CMD_SET_NOACK_MAP: sets a bitmap for the individual TIDs whether
 *      No Acknowledgement Policy should be applied.
 *
 * @FC80211_CMD_CH_SWITCH_NOTIFY: An AP or GO may decide to switch channels
 *	independently of the userspace SME, send this event indicating
 *	%FC80211_ATTR_IFINDEX is now on %FC80211_ATTR_SOFTMAC_FREQ and the
 *	attributes determining channel width.
 *
 * @FC80211_CMD_START_P2P_DEVICE: Start the given P2P Device, identified by
 *	its %FC80211_ATTR_WDEV identifier. It must have been created with
 *	%FC80211_CMD_NEW_INTERFACE previously. After it has been started, the
 *	P2P Device can be used for P2P operations, e.g. remain-on-channel and
 *	public action frame TX.
 * @FC80211_CMD_STOP_P2P_DEVICE: Stop the given P2P Device, identified by
 *	its %FC80211_ATTR_WDEV identifier.
 *
 * @FC80211_CMD_CONN_FAILED: connection request to an AP failed; used to
 *	notify userspace that AP has rejected the connection request from a
 *	station, due to particular reason. %FC80211_ATTR_CONN_FAILED_REASON
 *	is used for this.
 *
 * @FC80211_CMD_SET_MCAST_RATE: Change the rate used to send multicast frames
 *	for IBSS or MESH vif.
 *
 * @FC80211_CMD_SET_MAC_ACL: sets ACL for MAC address based access control.
 *	This is to be used with the drivers advertising the support of MAC
 *	address based access control. List of MAC addresses is passed in
 *	%FC80211_ATTR_MAC_ADDRS and ACL policy is passed in
 *	%FC80211_ATTR_ACL_POLICY. Driver will enable ACL with this list, if it
 *	is not already done. The new list will replace any existing list. Driver
 *	will clear its ACL when the list of MAC addresses passed is empty. This
 *	command is used in AP/P2P GO mode. Driver has to make sure to clear its
 *	ACL list during %FC80211_CMD_STOP_AP.
 *
 * @FC80211_CMD_RADAR_DETECT: Start a Channel availability check (CAC). Once
 *	a radar is detected or the channel availability scan (CAC) has finished
 *	or was aborted, or a radar was detected, usermode will be notified with
 *	this event. This command is also used to notify userspace about radars
 *	while operating on this channel.
 *	%FC80211_ATTR_RADAR_EVENT is used to inform about the type of the
 *	event.
 *
 * @FC80211_CMD_GET_PROTOCOL_FEATURES: Get global fc80211 protocol features,
 *	i.e. features for the fc80211 protocol rather than device features.
 *	Returns the features in the %FC80211_ATTR_PROTOCOL_FEATURES bitmap.
 *
 * @FC80211_CMD_UPDATE_FT_IES: Pass down the most up-to-date Fast Transition
 *	Information Element to the WLAN driver
 *
 * @FC80211_CMD_FT_EVENT: Send a Fast transition event from the WLAN driver
 *	to the supplicant. This will carry the target AP's MAC address along
 *	with the relevant Information Elements. This event is used to report
 *	received FT IEs (MDIE, FTIE, RSN IE, TIE, RICIE).
 *
 * @FC80211_CMD_CRIT_PROTOCOL_START: Indicates user-space will start running
 *	a critical protocol that needs more reliability in the connection to
 *	complete.
 *
 * @FC80211_CMD_CRIT_PROTOCOL_STOP: Indicates the connection reliability can
 *	return back to normal.
 *
 * @FC80211_CMD_GET_COALESCE: Get currently supported coalesce rules.
 * @FC80211_CMD_SET_COALESCE: Configure coalesce rules or clear existing rules.
 *
 * @FC80211_CMD_CHANNEL_SWITCH: Perform a channel switch by announcing the
 *	the new channel information (Channel Switch Announcement - CSA)
 *	in the beacon for some time (as defined in the
 *	%FC80211_ATTR_CH_SWITCH_COUNT parameter) and then change to the
 *	new channel. Userspace provides the new channel information (using
 *	%FC80211_ATTR_SOFTMAC_FREQ and the attributes determining channel
 *	width). %FC80211_ATTR_CH_SWITCH_BLOCK_TX may be supplied to inform
 *	other station that transmission must be blocked until the channel
 *	switch is complete.
 *
 * @FC80211_CMD_VENDOR: Vendor-specified command/event. The command is specified
 *	by the %FC80211_ATTR_VENDOR_ID attribute and a sub-command in
 *	%FC80211_ATTR_VENDOR_SUBCMD. Parameter(s) can be transported in
 *	%FC80211_ATTR_VENDOR_DATA.
 *	For feature advertisement, the %FC80211_ATTR_VENDOR_DATA attribute is
 *	used in the softmac data as a nested attribute containing descriptions
 *	(&struct fc80211_vendor_cmd_info) of the supported vendor commands.
 *	This may also be sent as an event with the same attributes.
 *
 * @FC80211_CMD_SET_QOS_MAP: Set Interworking QoS mapping for IP DSCP values.
 *	The QoS mapping information is included in %FC80211_ATTR_QOS_MAP. If
 *	that attribute is not included, QoS mapping is disabled. Since this
 *	QoS mapping is relevant for IP packets, it is only valid during an
 *	association. This is cleared on disassociation and AP restart.
 *
 * @FC80211_CMD_MAX: highest used command number
 * @__FC80211_CMD_AFTER_LAST: internal use
 */
enum fc80211_commands {
/* don't change the order or add anything between, this is ABI! */
	FC80211_CMD_UNSPEC		= 0,

	FC80211_CMD_GET_SOFTMAC,		/* can dump */
	FC80211_CMD_SET_SOFTMAC,
	FC80211_CMD_NEW_SOFTMAC,
	FC80211_CMD_DEL_SOFTMAC,

	FC80211_CMD_GET_INTERFACE,	/* can dump */
	FC80211_CMD_SET_INTERFACE,
	FC80211_CMD_NEW_INTERFACE,
	FC80211_CMD_DEL_INTERFACE,

	FC80211_CMD_GET_KEY,
	FC80211_CMD_SET_KEY,		// 10
	FC80211_CMD_NEW_KEY,
	FC80211_CMD_DEL_KEY,

	FC80211_CMD_GET_BEACON,
	FC80211_CMD_SET_BEACON,
	FC80211_CMD_START_AP, // 15
	FC80211_CMD_NEW_BEACON = FC80211_CMD_START_AP, // 15
	FC80211_CMD_STOP_AP, // 16
	FC80211_CMD_DEL_BEACON = FC80211_CMD_STOP_AP, // 16

	FC80211_CMD_GET_STATION,
	FC80211_CMD_SET_STATION,	//
	FC80211_CMD_NEW_STATION,
	FC80211_CMD_DEL_STATION,	// 20

	FC80211_CMD_GET_MPATH,
	FC80211_CMD_SET_MPATH,
	FC80211_CMD_NEW_MPATH,
	FC80211_CMD_DEL_MPATH,

	FC80211_CMD_SET_BSS, // 25

/*** Unused Commad in FCI ***/
	FC80211_CMD_SET_REG,
	FC80211_CMD_REQ_SET_REG,

	FC80211_CMD_GET_MESH_CONFIG,
	FC80211_CMD_SET_MESH_CONFIG,
/******************************/

	FC80211_CMD_SET_MGMT_EXTRA_IE /* 30 : reserved; not used */,

/*** Unused Commad in FCI ***/
	FC80211_CMD_GET_REG,
/******************************/

	FC80211_CMD_GET_SCAN,
	FC80211_CMD_TRIGGER_SCAN,
	FC80211_CMD_NEW_SCAN_RESULTS,
	FC80211_CMD_SCAN_ABORTED,	// 35

/*** Unused Commad in FCI ***/
	FC80211_CMD_REG_CHANGE,
/******************************/

	FC80211_CMD_AUTHENTICATE,
	FC80211_CMD_ASSOCIATE,
	FC80211_CMD_DEAUTHENTICATE,
	FC80211_CMD_DISASSOCIATE,	// 40

	FC80211_CMD_MICHAEL_MIC_FAILURE,

/*** Unused Commad in FCI ***/
	FC80211_CMD_REG_BEACON_HINT,
/******************************/

	FC80211_CMD_JOIN_IBSS,
	FC80211_CMD_LEAVE_IBSS,

	FC80211_CMD_TESTMODE,		// 45

	FC80211_CMD_CONNECT,
	FC80211_CMD_ROAM,
	FC80211_CMD_DISCONNECT,

	FC80211_CMD_SET_SOFTMAC_NETNS,

	FC80211_CMD_GET_SURVEY,		// 50
	FC80211_CMD_NEW_SURVEY_RESULTS,

/*** Unused Commad in FCI ***/
	FC80211_CMD_SET_PMKSA,
	FC80211_CMD_DEL_PMKSA,
	FC80211_CMD_FLUSH_PMKSA,
/******************************/

	FC80211_CMD_REMAIN_ON_CHANNEL,	// 55
	FC80211_CMD_CANCEL_REMAIN_ON_CHANNEL,

	FC80211_CMD_SET_TX_BITRATE_MASK,

	FC80211_CMD_REGISTER_FRAME, // 58
	FC80211_CMD_REGISTER_ACTION	= FC80211_CMD_REGISTER_FRAME, // 58
	FC80211_CMD_FRAME, // 59
	FC80211_CMD_ACTION		= FC80211_CMD_FRAME, // 59
	FC80211_CMD_FRAME_TX_STATUS, // 60
	FC80211_CMD_ACTION_TX_STATUS	= FC80211_CMD_FRAME_TX_STATUS,	// 60

	FC80211_CMD_SET_POWER_SAVE, // 61
	FC80211_CMD_GET_POWER_SAVE,

	FC80211_CMD_SET_CQM,
	FC80211_CMD_NOTIFY_CQM,

	FC80211_CMD_SET_CHANNEL,	// 65
	FC80211_CMD_SET_WDS_PEER,

	FC80211_CMD_FRAME_WAIT_CANCEL,

	FC80211_CMD_JOIN_MESH,
	FC80211_CMD_LEAVE_MESH,

	FC80211_CMD_UNPROT_DEAUTHENTICATE,	// 70
	FC80211_CMD_UNPROT_DISASSOCIATE,

	FC80211_CMD_NEW_PEER_CANDIDATE,

/*** Unused Commad in FCI ***/
	FC80211_CMD_GET_WOWLAN,
	FC80211_CMD_SET_WOWLAN,

	FC80211_CMD_START_SCHED_SCAN,		// 75
	FC80211_CMD_STOP_SCHED_SCAN,
	FC80211_CMD_SCHED_SCAN_RESULTS,
	FC80211_CMD_SCHED_SCAN_STOPPED,
/******************************/

	FC80211_CMD_SET_REKEY_OFFLOAD,

/*** Unused Commad in FCI ***/
	FC80211_CMD_PMKSA_CANDIDATE,		// 80

	FC80211_CMD_TDLS_OPER,
	FC80211_CMD_TDLS_MGMT,
/******************************/

	FC80211_CMD_UNEXPECTED_FRAME,
	FC80211_CMD_PROBE_CLIENT,
	FC80211_CMD_REGISTER_BEACONS,		// 85
	FC80211_CMD_UNEXPECTED_4ADDR_FRAME,
	FC80211_CMD_SET_NOACK_MAP,
	FC80211_CMD_CH_SWITCH_NOTIFY,
	FC80211_CMD_START_P2P_DEVICE,
	FC80211_CMD_STOP_P2P_DEVICE,		// 90
	FC80211_CMD_CONN_FAILED,
	FC80211_CMD_SET_MCAST_RATE,
	FC80211_CMD_SET_MAC_ACL,
	FC80211_CMD_RADAR_DETECT,
	FC80211_CMD_GET_PROTOCOL_FEATURES,	// 95
	FC80211_CMD_UPDATE_FT_IES,
	FC80211_CMD_FT_EVENT,
	FC80211_CMD_CRIT_PROTOCOL_START,
	FC80211_CMD_CRIT_PROTOCOL_STOP,
	FC80211_CMD_GET_COALESCE,		// 100
	FC80211_CMD_SET_COALESCE,
	FC80211_CMD_CHANNEL_SWITCH,
	FC80211_CMD_VENDOR,
	FC80211_CMD_SET_QOS_MAP,

	/* add new commands above here */
	FC80211_CMD_CH_SWITCH_STARTED_NOTIFY,
	FC80211_CMD_EXTERNAL_AUTH,

	/* used to define FC80211_CMD_MAX below */
	__FC80211_CMD_AFTER_LAST,
	FC80211_CMD_MAX = __FC80211_CMD_AFTER_LAST - 1
};

/*
 * Allow user space programs to use #ifdef on new commands by defining them
 * here
 */
#define FC80211_CMD_SET_BSS		FC80211_CMD_SET_BSS
#define FC80211_CMD_SET_MGMT_EXTRA_IE	FC80211_CMD_SET_MGMT_EXTRA_IE
#define FC80211_CMD_REG_CHANGE		FC80211_CMD_REG_CHANGE
#define FC80211_CMD_AUTHENTICATE	FC80211_CMD_AUTHENTICATE
#define FC80211_CMD_ASSOCIATE		FC80211_CMD_ASSOCIATE
#define FC80211_CMD_DEAUTHENTICATE	FC80211_CMD_DEAUTHENTICATE
#define FC80211_CMD_DISASSOCIATE	FC80211_CMD_DISASSOCIATE
#define FC80211_CMD_REG_BEACON_HINT	FC80211_CMD_REG_BEACON_HINT

#define FC80211_ATTR_FEATURE_FLAGS	FC80211_ATTR_FEATURE_FLAGS

/* source-level API compatibility */
#define FC80211_CMD_GET_MESH_PARAMS	FC80211_CMD_GET_MESH_CONFIG
#define FC80211_CMD_SET_MESH_PARAMS	FC80211_CMD_SET_MESH_CONFIG
#define FC80211_MESH_SETUP_VENDOR_PATH_SEL_IE FC80211_MESH_SETUP_IE

/**
 * enum fc80211_attrs - fc80211 netlink attributes
 *
 * @FC80211_ATTR_UNSPEC: unspecified attribute to catch errors
 *
 * @FC80211_ATTR_SOFTMAC: index of softmac to operate on, cf.
 *	/sys/class/ieee80211/<phyname>/index
 * @FC80211_ATTR_SOFTMAC_NAME: softmac name (used for renaming)
 * @FC80211_ATTR_SOFTMAC_TXQ_PARAMS: a nested array of TX queue parameters
 * @FC80211_ATTR_SOFTMAC_FREQ: frequency of the selected channel in MHz,
 *	defines the channel together with the (deprecated)
 *	%FC80211_ATTR_SOFTMAC_CHANNEL_TYPE attribute or the attributes
 *	%FC80211_ATTR_CHANNEL_WIDTH and if needed %FC80211_ATTR_CENTER_FREQ1
 *	and %FC80211_ATTR_CENTER_FREQ2
 * @FC80211_ATTR_CHANNEL_WIDTH: u32 attribute containing one of the values
 *	of &enum fc80211_chan_width, describing the channel width. See the
 *	documentation of the enum for more information.
 * @FC80211_ATTR_CENTER_FREQ1: Center frequency of the first part of the
 *	channel, used for anything but 20 MHz bandwidth
 * @FC80211_ATTR_CENTER_FREQ2: Center frequency of the second part of the
 *	channel, used only for 80+80 MHz bandwidth
 * @FC80211_ATTR_SOFTMAC_CHANNEL_TYPE: included with FC80211_ATTR_SOFTMAC_FREQ
 *	if HT20 or HT40 are to be used (i.e., HT disabled if not included):
 *	FC80211_CHAN_NO_HT = HT not allowed (i.e., same as not including
 *		this attribute)
 *	FC80211_CHAN_HT20 = HT20 only
 *	FC80211_CHAN_HT40MINUS = secondary channel is below the primary channel
 *	FC80211_CHAN_HT40PLUS = secondary channel is above the primary channel
 *	This attribute is now deprecated.
 * @FC80211_ATTR_SOFTMAC_RETRY_SHORT: TX retry limit for frames whose length is
 *	less than or equal to the RTS threshold; allowed range: 1..255;
 *	dot11ShortRetryLimit; u8
 * @FC80211_ATTR_SOFTMAC_RETRY_LONG: TX retry limit for frames whose length is
 *	greater than the RTS threshold; allowed range: 1..255;
 *	dot11ShortLongLimit; u8
 * @FC80211_ATTR_SOFTMAC_FRAG_THRESHOLD: fragmentation threshold, i.e., maximum
 *	length in octets for frames; allowed range: 256..8000, disable
 *	fragmentation with (u32)-1; dot11FragmentationThreshold; u32
 * @FC80211_ATTR_SOFTMAC_RTS_THRESHOLD: RTS threshold (TX frames with length
 *	larger than or equal to this use RTS/CTS handshake); allowed range:
 *	0..65536, disable with (u32)-1; dot11RTSThreshold; u32
 * @FC80211_ATTR_SOFTMAC_COVERAGE_CLASS: Coverage Class as defined by IEEE 802.11
 *	section 7.3.2.9; dot11CoverageClass; u8
 *
 * @FC80211_ATTR_IFINDEX: network interface index of the device to operate on
 * @FC80211_ATTR_IFNAME: network interface name
 * @FC80211_ATTR_IFTYPE: type of virtual interface, see &enum fc80211_iftype
 *
 * @FC80211_ATTR_WDEV: wireless device identifier, used for pseudo-devices
 *	that don't have a netdev (u64)
 *
 * @FC80211_ATTR_MAC: MAC address (various uses)
 *
 * @FC80211_ATTR_KEY_DATA: (temporal) key data; for TKIP this consists of
 *	16 bytes encryption key followed by 8 bytes each for TX and RX MIC
 *	keys
 * @FC80211_ATTR_KEY_IDX: key ID (u8, 0-3)
 * @FC80211_ATTR_KEY_CIPHER: key cipher suite (u32, as defined by IEEE 802.11
 *	section 7.3.2.25.1, e.g. 0x000FAC04)
 * @FC80211_ATTR_KEY_SEQ: transmit key sequence number (IV/PN) for TKIP and
 *	CCMP keys, each six bytes in little endian
 * @FC80211_ATTR_KEY_DEFAULT: Flag attribute indicating the key is default key
 * @FC80211_ATTR_KEY_DEFAULT_MGMT: Flag attribute indicating the key is the
 *	default management key
 * @FC80211_ATTR_CIPHER_SUITES_PAIRWISE: For crypto settings for connect or
 *	other commands, indicates which pairwise cipher suites are used
 * @FC80211_ATTR_CIPHER_SUITE_GROUP: For crypto settings for connect or
 *	other commands, indicates which group cipher suite is used
 *
 * @FC80211_ATTR_BEACON_INTERVAL: beacon interval in TU
 * @FC80211_ATTR_DTIM_PERIOD: DTIM period for beaconing
 * @FC80211_ATTR_BEACON_HEAD: portion of the beacon before the TIM IE
 * @FC80211_ATTR_BEACON_TAIL: portion of the beacon after the TIM IE
 *
 * @FC80211_ATTR_STA_AID: Association ID for the station (u16)
 * @FC80211_ATTR_STA_FLAGS: flags, nested element with NLA_FLAG attributes of
 *	&enum fc80211_sta_flags (deprecated, use %FC80211_ATTR_STA_FLAGS2)
 * @FC80211_ATTR_STA_LISTEN_INTERVAL: listen interval as defined by
 *	IEEE 802.11 7.3.1.6 (u16).
 * @FC80211_ATTR_STA_SUPPORTED_RATES: supported rates, array of supported
 *	rates as defined by IEEE 802.11 7.3.2.2 but without the length
 *	restriction (at most %FC80211_MAX_SUPP_RATES).
 * @FC80211_ATTR_STA_VLAN: interface index of VLAN interface to move station
 *	to, or the AP interface the station was originally added to to.
 * @FC80211_ATTR_STA_INFO: information about a station, part of station info
 *	given for %FC80211_CMD_GET_STATION, nested attribute containing
 *	info as possible, see &enum fc80211_sta_info.
 *
 * @FC80211_ATTR_SOFTMAC_BANDS: Information about an operating bands,
 *	consisting of a nested array.
 *
 * @FC80211_ATTR_MESH_ID: mesh id (1-32 bytes).
 * @FC80211_ATTR_STA_PLINK_ACTION: action to perform on the mesh peer link
 *	(see &enum fc80211_plink_action).
 * @FC80211_ATTR_MPATH_NEXT_HOP: MAC address of the next hop for a mesh path.
 * @FC80211_ATTR_MPATH_INFO: information about a mesh_path, part of mesh path
 * 	info given for %FC80211_CMD_GET_MPATH, nested attribute described at
 *	&enum fc80211_mpath_info.
 *
 * @FC80211_ATTR_MNTR_FLAGS: flags, nested element with NLA_FLAG attributes of
 *      &enum fc80211_mntr_flags.
 *
 * @FC80211_ATTR_REG_ALPHA2: an ISO-3166-alpha2 country code for which the
 * 	current regulatory domain should be set to or is already set to.
 * 	For example, 'CR', for Costa Rica. This attribute is used by the kernel
 * 	to query the CRDA to retrieve one regulatory domain. This attribute can
 * 	also be used by userspace to query the kernel for the currently set
 * 	regulatory domain. We chose an alpha2 as that is also used by the
 * 	IEEE-802.11 country information element to identify a country.
 * 	Users can also simply ask the wireless core to set regulatory domain
 * 	to a specific alpha2.
 * @FC80211_ATTR_REG_RULES: a nested array of regulatory domain regulatory
 *	rules.
 *
 * @FC80211_ATTR_BSS_CTS_PROT: whether CTS protection is enabled (u8, 0 or 1)
 * @FC80211_ATTR_BSS_SHORT_PREAMBLE: whether short preamble is enabled
 *	(u8, 0 or 1)
 * @FC80211_ATTR_BSS_SHORT_SLOT_TIME: whether short slot time enabled
 *	(u8, 0 or 1)
 * @FC80211_ATTR_BSS_BASIC_RATES: basic rates, array of basic
 *	rates in format defined by IEEE 802.11 7.3.2.2 but without the length
 *	restriction (at most %FC80211_MAX_SUPP_RATES).
 *
 * @FC80211_ATTR_HT_CAPABILITY: HT Capability information element (from
 *	association request when used with FC80211_CMD_NEW_STATION)
 *
 * @FC80211_ATTR_SUPPORTED_IFTYPES: nested attribute containing all
 *	supported interface types, each a flag attribute with the number
 *	of the interface mode.
 *
 * @FC80211_ATTR_MGMT_SUBTYPE: Management frame subtype for
 *	%FC80211_CMD_SET_MGMT_EXTRA_IE.
 *
 * @FC80211_ATTR_IE: Information element(s) data (used, e.g., with
 *	%FC80211_CMD_SET_MGMT_EXTRA_IE).
 *
 * @FC80211_ATTR_MAX_NUM_SCAN_SSIDS: number of SSIDs you can scan with
 *	a single scan request, a softmac attribute.
 * @FC80211_ATTR_MAX_NUM_SCHED_SCAN_SSIDS: number of SSIDs you can
 *	scan with a single scheduled scan request, a softmac attribute.
 * @FC80211_ATTR_MAX_SCAN_IE_LEN: maximum length of information elements
 *	that can be added to a scan request
 * @FC80211_ATTR_MAX_SCHED_SCAN_IE_LEN: maximum length of information
 *	elements that can be added to a scheduled scan request
 * @FC80211_ATTR_MAX_MATCH_SETS: maximum number of sets that can be
 *	used with @FC80211_ATTR_SCHED_SCAN_MATCH, a softmac attribute.
 *
 * @FC80211_ATTR_SCAN_FREQUENCIES: nested attribute with frequencies (in MHz)
 * @FC80211_ATTR_SCAN_SSIDS: nested attribute with SSIDs, leave out for passive
 *	scanning and include a zero-length SSID (wildcard) for wildcard scan
 * @FC80211_ATTR_BSS: scan result BSS
 *
 * @FC80211_ATTR_REG_INITIATOR: indicates who requested the regulatory domain
 * 	currently in effect. This could be any of the %FC80211_REGDOM_SET_BY_*
 * @FC80211_ATTR_REG_TYPE: indicates the type of the regulatory domain currently
 * 	set. This can be one of the fc80211_reg_type (%FC80211_REGDOM_TYPE_*)
 *
 * @FC80211_ATTR_SUPPORTED_COMMANDS: softmac attribute that specifies
 *	an array of command numbers (i.e. a mapping index to command number)
 *	that the driver for the given softmac supports.
 *
 * @FC80211_ATTR_FRAME: frame data (binary attribute), including frame header
 *	and body, but not FCS; used, e.g., with FC80211_CMD_AUTHENTICATE and
 *	FC80211_CMD_ASSOCIATE events
 * @FC80211_ATTR_SSID: SSID (binary attribute, 0..32 octets)
 * @FC80211_ATTR_AUTH_TYPE: AuthenticationType, see &enum fc80211_auth_type,
 *	represented as a u32
 * @FC80211_ATTR_REASON_CODE: ReasonCode for %FC80211_CMD_DEAUTHENTICATE and
 *	%FC80211_CMD_DISASSOCIATE, u16
 *
 * @FC80211_ATTR_KEY_TYPE: Key Type, see &enum fc80211_key_type, represented as
 *	a u32
 *
 * @FC80211_ATTR_FREQ_BEFORE: A channel which has suffered a regulatory change
 * 	due to considerations from a beacon hint. This attribute reflects
 * 	the state of the channel _before_ the beacon hint processing. This
 * 	attributes consists of a nested attribute containing
 * 	FC80211_FREQUENCY_ATTR_*
 * @FC80211_ATTR_FREQ_AFTER: A channel which has suffered a regulatory change
 * 	due to considerations from a beacon hint. This attribute reflects
 * 	the state of the channel _after_ the beacon hint processing. This
 * 	attributes consists of a nested attribute containing
 * 	FC80211_FREQUENCY_ATTR_*
 *
 * @FC80211_ATTR_CIPHER_SUITES: a set of u32 values indicating the supported
 *	cipher suites
 *
 * @FC80211_ATTR_FREQ_FIXED: a flag indicating the IBSS should not try to look
 *	for other networks on different channels
 *
 * @FC80211_ATTR_TIMED_OUT: a flag indicating than an operation timed out; this
 *	is used, e.g., with %FC80211_CMD_AUTHENTICATE event
 *
 * @FC80211_ATTR_USE_MFP: Whether management frame protection (IEEE 802.11w) is
 *	used for the association (&enum fc80211_mfp, represented as a u32);
 *	this attribute can be used
 *	with %FC80211_CMD_ASSOCIATE and %FC80211_CMD_CONNECT requests
 *
 * @FC80211_ATTR_STA_FLAGS2: Attribute containing a
 *	&struct fc80211_sta_flag_update.
 *
 * @FC80211_ATTR_CONTROL_PORT: A flag indicating whether user space controls
 *	IEEE 802.1X port, i.e., sets/clears %FC80211_STA_FLAG_AUTHORIZED, in
 *	station mode. If the flag is included in %FC80211_CMD_ASSOCIATE
 *	request, the driver will assume that the port is unauthorized until
 *	authorized by user space. Otherwise, port is marked authorized by
 *	default in station mode.
 * @FC80211_ATTR_CONTROL_PORT_ETHERTYPE: A 16-bit value indicating the
 *	ethertype that will be used for key negotiation. It can be
 *	specified with the associate and connect commands. If it is not
 *	specified, the value defaults to 0x888E (PAE, 802.1X). This
 *	attribute is also used as a flag in the softmac information to
 *	indicate that protocols other than PAE are supported.
 * @FC80211_ATTR_CONTROL_PORT_NO_ENCRYPT: When included along with
 *	%FC80211_ATTR_CONTROL_PORT_ETHERTYPE, indicates that the custom
 *	ethertype frames used for key negotiation must not be encrypted.
 *
 * @FC80211_ATTR_TESTDATA: Testmode data blob, passed through to the driver.
 *	We recommend using nested, driver-specific attributes within this.
 *
 * @FC80211_ATTR_DISCONNECTED_BY_AP: A flag indicating that the DISCONNECT
 *	event was due to the AP disconnecting the station, and not due to
 *	a local disconnect request.
 * @FC80211_ATTR_STATUS_CODE: StatusCode for the %FC80211_CMD_CONNECT
 *	event (u16)
 * @FC80211_ATTR_PRIVACY: Flag attribute, used with connect(), indicating
 *	that protected APs should be used. This is also used with NEW_BEACON to
 *	indicate that the BSS is to use protection.
 *
 * @FC80211_ATTR_CIPHERS_PAIRWISE: Used with CONNECT, ASSOCIATE, and NEW_BEACON
 *	to indicate which unicast key ciphers will be used with the connection
 *	(an array of u32).
 * @FC80211_ATTR_CIPHER_GROUP: Used with CONNECT, ASSOCIATE, and NEW_BEACON to
 *	indicate which group key cipher will be used with the connection (a
 *	u32).
 * @FC80211_ATTR_WPA_VERSIONS: Used with CONNECT, ASSOCIATE, and NEW_BEACON to
 *	indicate which WPA version(s) the AP we want to associate with is using
 *	(a u32 with flags from &enum fc80211_wpa_versions).
 * @FC80211_ATTR_AKM_SUITES: Used with CONNECT, ASSOCIATE, and NEW_BEACON to
 *	indicate which key management algorithm(s) to use (an array of u32).
 *
 * @FC80211_ATTR_REQ_IE: (Re)association request information elements as
 *	sent out by the card, for ROAM and successful CONNECT events.
 * @FC80211_ATTR_RESP_IE: (Re)association response information elements as
 *	sent by peer, for ROAM and successful CONNECT events.
 *
 * @FC80211_ATTR_PREV_BSSID: previous BSSID, to be used by in ASSOCIATE
 *	commands to specify using a reassociate frame
 *
 * @FC80211_ATTR_KEY: key information in a nested attribute with
 *	%FC80211_KEY_* sub-attributes
 * @FC80211_ATTR_KEYS: array of keys for static WEP keys for connect()
 *	and join_ibss(), key information is in a nested attribute each
 *	with %FC80211_KEY_* sub-attributes
 *
 * @FC80211_ATTR_PID: Process ID of a network namespace.
 *
 * @FC80211_ATTR_GENERATION: Used to indicate consistent snapshots for
 *	dumps. This number increases whenever the object list being
 *	dumped changes, and as such userspace can verify that it has
 *	obtained a complete and consistent snapshot by verifying that
 *	all dump messages contain the same generation number. If it
 *	changed then the list changed and the dump should be repeated
 *	completely from scratch.
 *
 * @FC80211_ATTR_4ADDR: Use 4-address frames on a virtual interface
 *
 * @FC80211_ATTR_SURVEY_INFO: survey information about a channel, part of
 *      the survey response for %FC80211_CMD_GET_SURVEY, nested attribute
 *      containing info as possible, see &enum survey_info.
 *
 * @FC80211_ATTR_PMKID: PMK material for PMKSA caching.
 * @FC80211_ATTR_MAX_NUM_PMKIDS: maximum number of PMKIDs a firmware can
 *	cache, a softmac attribute.
 *
 * @FC80211_ATTR_DURATION: Duration of an operation in milliseconds, u32.
 * @FC80211_ATTR_MAX_REMAIN_ON_CHANNEL_DURATION: Device attribute that
 *	specifies the maximum duration that can be requested with the
 *	remain-on-channel operation, in milliseconds, u32.
 *
 * @FC80211_ATTR_COOKIE: Generic 64-bit cookie to identify objects.
 *
 * @FC80211_ATTR_TX_RATES: Nested set of attributes
 *	(enum fc80211_tx_rate_attributes) describing TX rates per band. The
 *	enum fc80211_band value is used as the index (nla_type() of the nested
 *	data. If a band is not included, it will be configured to allow all
 *	rates based on negotiated supported rates information. This attribute
 *	is used with %FC80211_CMD_SET_TX_BITRATE_MASK.
 *
 * @FC80211_ATTR_FRAME_MATCH: A binary attribute which typically must contain
 *	at least one byte, currently used with @FC80211_CMD_REGISTER_FRAME.
 * @FC80211_ATTR_FRAME_TYPE: A u16 indicating the frame type/subtype for the
 *	@FC80211_CMD_REGISTER_FRAME command.
 * @FC80211_ATTR_TX_FRAME_TYPES: softmac capability attribute, which is a
 *	nested attribute of %FC80211_ATTR_FRAME_TYPE attributes, containing
 *	information about which frame types can be transmitted with
 *	%FC80211_CMD_FRAME.
 * @FC80211_ATTR_RX_FRAME_TYPES: softmac capability attribute, which is a
 *	nested attribute of %FC80211_ATTR_FRAME_TYPE attributes, containing
 *	information about which frame types can be registered for RX.
 *
 * @FC80211_ATTR_ACK: Flag attribute indicating that the frame was
 *	acknowledged by the recipient.
 *
 * @FC80211_ATTR_PS_STATE: powersave state, using &enum fc80211_ps_state values.
 *
 * @FC80211_ATTR_CQM: connection quality monitor configuration in a
 *	nested attribute with %FC80211_ATTR_CQM_* sub-attributes.
 *
 * @FC80211_ATTR_LOCAL_STATE_CHANGE: Flag attribute to indicate that a command
 *	is requesting a local authentication/association state change without
 *	invoking actual management frame exchange. This can be used with
 *	FC80211_CMD_AUTHENTICATE, FC80211_CMD_DEAUTHENTICATE,
 *	FC80211_CMD_DISASSOCIATE.
 *
 * @FC80211_ATTR_AP_ISOLATE: (AP mode) Do not forward traffic between stations
 *	connected to this BSS.
 *
 * @FC80211_ATTR_SOFTMAC_TX_POWER_SETTING: Transmit power setting type. See
 *      &enum fc80211_tx_power_setting for possible values.
 * @FC80211_ATTR_SOFTMAC_TX_POWER_LEVEL: Transmit power level in signed mBm units.
 *      This is used in association with @FC80211_ATTR_SOFTMAC_TX_POWER_SETTING
 *      for non-automatic settings.
 *
 * @FC80211_ATTR_SUPPORT_IBSS_RSN: The device supports IBSS RSN, which mostly
 *	means support for per-station GTKs.
 *
 * @FC80211_ATTR_SOFTMAC_ANTENNA_TX: Bitmap of allowed antennas for transmitting.
 *	This can be used to mask out antennas which are not attached or should
 *	not be used for transmitting. If an antenna is not selected in this
 *	bitmap the hardware is not allowed to transmit on this antenna.
 *
 *	Each bit represents one antenna, starting with antenna 1 at the first
 *	bit. Depending on which antennas are selected in the bitmap, 802.11n
 *	drivers can derive which chainmasks to use (if all antennas belonging to
 *	a particular chain are disabled this chain should be disabled) and if
 *	a chain has diversity antennas wether diversity should be used or not.
 *	HT capabilities (STBC, TX Beamforming, Antenna selection) can be
 *	derived from the available chains after applying the antenna mask.
 *	Non-802.11n drivers can derive wether to use diversity or not.
 *	Drivers may reject configurations or RX/TX mask combinations they cannot
 *	support by returning -EINVAL.
 *
 * @FC80211_ATTR_SOFTMAC_ANTENNA_RX: Bitmap of allowed antennas for receiving.
 *	This can be used to mask out antennas which are not attached or should
 *	not be used for receiving. If an antenna is not selected in this bitmap
 *	the hardware should not be configured to receive on this antenna.
 *	For a more detailed description see @FC80211_ATTR_SOFTMAC_ANTENNA_TX.
 *
 * @FC80211_ATTR_SOFTMAC_ANTENNA_AVAIL_TX: Bitmap of antennas which are available
 *	for configuration as TX antennas via the above parameters.
 *
 * @FC80211_ATTR_SOFTMAC_ANTENNA_AVAIL_RX: Bitmap of antennas which are available
 *	for configuration as RX antennas via the above parameters.
 *
 * @FC80211_ATTR_MCAST_RATE: Multicast tx rate (in 100 kbps) for IBSS
 *
 * @FC80211_ATTR_OFFCHANNEL_TX_OK: For management frame TX, the frame may be
 *	transmitted on another channel when the channel given doesn't match
 *	the current channel. If the current channel doesn't match and this
 *	flag isn't set, the frame will be rejected. This is also used as an
 *	fc80211 capability flag.
 *
 * @FC80211_ATTR_BSS_HT_OPMODE: HT operation mode (u16)
 *
 * @FC80211_ATTR_KEY_DEFAULT_TYPES: A nested attribute containing flags
 *	attributes, specifying what a key should be set as default as.
 *	See &enum fc80211_key_default_types.
 *
 * @FC80211_ATTR_MESH_SETUP: Optional mesh setup parameters.  These cannot be
 *	changed once the mesh is active.
 * @FC80211_ATTR_MESH_CONFIG: Mesh configuration parameters, a nested attribute
 *	containing attributes from &enum fc80211_meshconf_params.
 * @FC80211_ATTR_SUPPORT_MESH_AUTH: Currently, this means the underlying driver
 *	allows auth frames in a mesh to be passed to userspace for processing via
 *	the @FC80211_MESH_SETUP_USERSPACE_AUTH flag.
 * @FC80211_ATTR_STA_PLINK_STATE: The state of a mesh peer link as defined in
 *	&enum fc80211_plink_state. Used when userspace is driving the peer link
 *	management state machine.  @FC80211_MESH_SETUP_USERSPACE_AMPE or
 *	@FC80211_MESH_SETUP_USERSPACE_MPM must be enabled.
 *
 * @FC80211_ATTR_WOWLAN_TRIGGERS_SUPPORTED: indicates, as part of the softmac
 *	capabilities, the supported WoWLAN triggers
 * @FC80211_ATTR_WOWLAN_TRIGGERS: used by %FC80211_CMD_SET_WOWLAN to
 *	indicate which WoW triggers should be enabled. This is also
 *	used by %FC80211_CMD_GET_WOWLAN to get the currently enabled WoWLAN
 *	triggers.
 *
 * @FC80211_ATTR_SCHED_SCAN_INTERVAL: Interval between scheduled scan
 *	cycles, in msecs.
 *
 * @FC80211_ATTR_SCHED_SCAN_MATCH: Nested attribute with one or more
 *	sets of attributes to match during scheduled scans.  Only BSSs
 *	that match any of the sets will be reported.  These are
 *	pass-thru filter rules.
 *	For a match to succeed, the BSS must match all attributes of a
 *	set.  Since not every hardware supports matching all types of
 *	attributes, there is no guarantee that the reported BSSs are
 *	fully complying with the match sets and userspace needs to be
 *	able to ignore them by itself.
 *	Thus, the implementation is somewhat hardware-dependent, but
 *	this is only an optimization and the userspace application
 *	needs to handle all the non-filtered results anyway.
 *	If the match attributes don't make sense when combined with
 *	the values passed in @FC80211_ATTR_SCAN_SSIDS (eg. if an SSID
 *	is included in the probe request, but the match attributes
 *	will never let it go through), -EINVAL may be returned.
 *	If ommited, no filtering is done.
 *
 * @FC80211_ATTR_INTERFACE_COMBINATIONS: Nested attribute listing the supported
 *	interface combinations. In each nested item, it contains attributes
 *	defined in &enum fc80211_if_combination_attrs.
 * @FC80211_ATTR_SOFTWARE_IFTYPES: Nested attribute (just like
 *	%FC80211_ATTR_SUPPORTED_IFTYPES) containing the interface types that
 *	are managed in software: interfaces of these types aren't subject to
 *	any restrictions in their number or combinations.
 *
 * @FC80211_ATTR_REKEY_DATA: nested attribute containing the information
 *	necessary for GTK rekeying in the device, see &enum fc80211_rekey_data.
 *
 * @FC80211_ATTR_SCAN_SUPP_RATES: rates per to be advertised as supported in scan,
 *	nested array attribute containing an entry for each band, with the entry
 *	being a list of supported rates as defined by IEEE 802.11 7.3.2.2 but
 *	without the length restriction (at most %FC80211_MAX_SUPP_RATES).
 *
 * @FC80211_ATTR_HIDDEN_SSID: indicates whether SSID is to be hidden from Beacon
 *	and Probe Response (when response to wildcard Probe Request); see
 *	&enum fc80211_hidden_ssid, represented as a u32
 *
 * @FC80211_ATTR_IE_PROBE_RESP: Information element(s) for Probe Response frame.
 *	This is used with %FC80211_CMD_NEW_BEACON and %FC80211_CMD_SET_BEACON to
 *	provide extra IEs (e.g., WPS/P2P IE) into Probe Response frames when the
 *	driver (or firmware) replies to Probe Request frames.
 * @FC80211_ATTR_IE_ASSOC_RESP: Information element(s) for (Re)Association
 *	Response frames. This is used with %FC80211_CMD_NEW_BEACON and
 *	%FC80211_CMD_SET_BEACON to provide extra IEs (e.g., WPS/P2P IE) into
 *	(Re)Association Response frames when the driver (or firmware) replies to
 *	(Re)Association Request frames.
 *
 * @FC80211_ATTR_STA_WME: Nested attribute containing the wme configuration
 *	of the station, see &enum fc80211_sta_wme_attr.
 * @FC80211_ATTR_SUPPORT_AP_UAPSD: the device supports uapsd when working
 *	as AP.
 *
 * @FC80211_ATTR_ROAM_SUPPORT: Indicates whether the firmware is capable of
 *	roaming to another AP in the same ESS if the signal lever is low.
 *
 * @FC80211_ATTR_PMKSA_CANDIDATE: Nested attribute containing the PMKSA caching
 *	candidate information, see &enum fc80211_pmksa_candidate_attr.
 *
 * @FC80211_ATTR_TX_NO_CCK_RATE: Indicates whether to use CCK rate or not
 *	for management frames transmission. In order to avoid p2p probe/action
 *	frames are being transmitted at CCK rate in 2GHz band, the user space
 *	applications use this attribute.
 *	This attribute is used with %FC80211_CMD_TRIGGER_SCAN and
 *	%FC80211_CMD_FRAME commands.
 *
 * @FC80211_ATTR_TDLS_ACTION: Low level TDLS action code (e.g. link setup
 *	request, link setup confirm, link teardown, etc.). Values are
 *	described in the TDLS (802.11z) specification.
 * @FC80211_ATTR_TDLS_DIALOG_TOKEN: Non-zero token for uniquely identifying a
 *	TDLS conversation between two devices.
 * @FC80211_ATTR_TDLS_OPERATION: High level TDLS operation; see
 *	&enum fc80211_tdls_operation, represented as a u8.
 * @FC80211_ATTR_TDLS_SUPPORT: A flag indicating the device can operate
 *	as a TDLS peer sta.
 * @FC80211_ATTR_TDLS_EXTERNAL_SETUP: The TDLS discovery/setup and teardown
 *	procedures should be performed by sending TDLS packets via
 *	%FC80211_CMD_TDLS_MGMT. Otherwise %FC80211_CMD_TDLS_OPER should be
 *	used for asking the driver to perform a TDLS operation.
 *
 * @FC80211_ATTR_DEVICE_AP_SME: This u32 attribute may be listed for devices
 *	that have AP support to indicate that they have the AP SME integrated
 *	with support for the features listed in this attribute, see
 *	&enum fc80211_ap_sme_features.
 *
 * @FC80211_ATTR_DONT_WAIT_FOR_ACK: Used with %FC80211_CMD_FRAME, this tells
 *	the driver to not wait for an acknowledgement. Note that due to this,
 *	it will also not give a status callback nor return a cookie. This is
 *	mostly useful for probe responses to save airtime.
 *
 * @FC80211_ATTR_FEATURE_FLAGS: This u32 attribute contains flags from
 *	&enum fc80211_feature_flags and is advertised in softmac information.
 * @FC80211_ATTR_PROBE_RESP_OFFLOAD: Indicates that the HW responds to probe
 *	requests while operating in AP-mode.
 *	This attribute holds a bitmap of the supported protocols for
 *	offloading (see &enum fc80211_probe_resp_offload_support_attr).
 *
 * @FC80211_ATTR_PROBE_RESP: Probe Response template data. Contains the entire
 *	probe-response frame. The DA field in the 802.11 header is zero-ed out,
 *	to be filled by the FW.
 * @FC80211_ATTR_DISABLE_HT:  Force HT capable interfaces to disable
 *      this feature.  Currently, only supported in macd11 drivers.
 * @FC80211_ATTR_HT_CAPABILITY_MASK: Specify which bits of the
 *      ATTR_HT_CAPABILITY to which attention should be paid.
 *      Currently, only macd11 NICs support this feature.
 *      The values that may be configured are:
 *       MCS rates, MAX-AMSDU, HT-20-40 and HT_CAP_SGI_40
 *       AMPDU density and AMPDU factor.
 *      All values are treated as suggestions and may be ignored
 *      by the driver as required.  The actual values may be seen in
 *      the station debugfs ht_caps file.
 *
 * @FC80211_ATTR_DFS_REGION: region for regulatory rules which this country
 *    abides to when initiating radiation on DFS channels. A country maps
 *    to one DFS region.
 *
 * @FC80211_ATTR_NOACK_MAP: This u16 bitmap contains the No Ack Policy of
 *      up to 16 TIDs.
 *
 * @FC80211_ATTR_INACTIVITY_TIMEOUT: timeout value in seconds, this can be
 *	used by the drivers which has MLME in firmware and does not have support
 *	to report per station tx/rx activity to free up the staion entry from
 *	the list. This needs to be used when the driver advertises the
 *	capability to timeout the stations.
 *
 * @FC80211_ATTR_RX_SIGNAL_DBM: signal strength in dBm (as a 32-bit int);
 *	this attribute is (depending on the driver capabilities) added to
 *	received frames indicated with %FC80211_CMD_FRAME.
 *
 * @FC80211_ATTR_BG_SCAN_PERIOD: Background scan period in seconds
 *      or 0 to disable background scan.
 *
 * @FC80211_ATTR_USER_REG_HINT_TYPE: type of regulatory hint passed from
 *	userspace. If unset it is assumed the hint comes directly from
 *	a user. If set code could specify exactly what type of source
 *	was used to provide the hint. For the different types of
 *	allowed user regulatory hints see fc80211_user_reg_hint_type.
 *
 * @FC80211_ATTR_CONN_FAILED_REASON: The reason for which AP has rejected
 *	the connection request from a station. fc80211_connect_failed_reason
 *	enum has different reasons of connection failure.
 *
 * @FC80211_ATTR_SAE_DATA: SAE elements in Authentication frames. This starts
 *	with the Authentication transaction sequence number field.
 *
 * @FC80211_ATTR_VHT_CAPABILITY: VHT Capability information element (from
 *	association request when used with FC80211_CMD_NEW_STATION)
 *
 * @FC80211_ATTR_SCAN_FLAGS: scan request control flags (u32)
 *
 * @FC80211_ATTR_P2P_CTWINDOW: P2P GO Client Traffic Window (u8), used with
 *	the START_AP and SET_BSS commands
 * @FC80211_ATTR_P2P_OPPPS: P2P GO opportunistic PS (u8), used with the
 *	START_AP and SET_BSS commands. This can have the values 0 or 1;
 *	if not given in START_AP 0 is assumed, if not given in SET_BSS
 *	no change is made.
 *
 * @FC80211_ATTR_LOCAL_MESH_POWER_MODE: local mesh STA link-specific power mode
 *	defined in &enum fc80211_mesh_power_mode.
 *
 * @FC80211_ATTR_ACL_POLICY: ACL policy, see &enum fc80211_acl_policy,
 *	carried in a u32 attribute
 *
 * @FC80211_ATTR_MAC_ADDRS: Array of nested MAC addresses, used for
 *	MAC ACL.
 *
 * @FC80211_ATTR_MAC_ACL_MAX: u32 attribute to advertise the maximum
 *	number of MAC addresses that a device can support for MAC
 *	ACL.
 *
 * @FC80211_ATTR_RADAR_EVENT: Type of radar event for notification to userspace,
 *	contains a value of enum fc80211_radar_event (u32).
 *
 * @FC80211_ATTR_EXT_CAPA: 802.11 extended capabilities that the kernel driver
 *	has and handles. The format is the same as the IE contents. See
 *	802.11-2012 8.4.2.29 for more information.
 * @FC80211_ATTR_EXT_CAPA_MASK: Extended capabilities that the kernel driver
 *	has set in the %FC80211_ATTR_EXT_CAPA value, for multibit fields.
 *
 * @FC80211_ATTR_STA_CAPABILITY: Station capabilities (u16) are advertised to
 *	the driver, e.g., to enable TDLS power save (PU-APSD).
 *
 * @FC80211_ATTR_STA_EXT_CAPABILITY: Station extended capabilities are
 *	advertised to the driver, e.g., to enable TDLS off channel operations
 *	and PU-APSD.
 *
 * @FC80211_ATTR_PROTOCOL_FEATURES: global fc80211 feature flags, see
 *	&enum fc80211_protocol_features, the attribute is a u32.
 *
 * @FC80211_ATTR_SPLIT_SOFTMAC_DUMP: flag attribute, userspace supports
 *	receiving the data for a single softmac split across multiple
 *	messages, given with softmac dump message
 *
 * @FC80211_ATTR_MDID: Mobility Domain Identifier
 *
 * @FC80211_ATTR_IE_RIC: Resource Information Container Information
 *	Element
 *
 * @FC80211_ATTR_CRIT_PROT_ID: critical protocol identifier requiring increased
 *	reliability, see &enum fc80211_crit_proto_id (u16).
 * @FC80211_ATTR_MAX_CRIT_PROT_DURATION: duration in milliseconds in which
 *      the connection should have increased reliability (u16).
 *
 * @FC80211_ATTR_PEER_AID: Association ID for the peer TDLS station (u16).
 *	This is similar to @FC80211_ATTR_STA_AID but with a difference of being
 *	allowed to be used with the first @FC80211_CMD_SET_STATION command to
 *	update a TDLS peer STA entry.
 *
 * @FC80211_ATTR_COALESCE_RULE: Coalesce rule information.
 *
 * @FC80211_ATTR_CH_SWITCH_COUNT: u32 attribute specifying the number of TBTT's
 *	until the channel switch event.
 * @FC80211_ATTR_CH_SWITCH_BLOCK_TX: flag attribute specifying that transmission
 *	must be blocked on the current channel (before the channel switch
 *	operation).
 * @FC80211_ATTR_CSA_IES: Nested set of attributes containing the IE information
 *	for the time while performing a channel switch.
 * @FC80211_ATTR_CSA_C_OFF_BEACON: Offset of the channel switch counter
 *	field in the beacons tail (%FC80211_ATTR_BEACON_TAIL).
 * @FC80211_ATTR_CSA_C_OFF_PRESP: Offset of the channel switch counter
 *	field in the probe response (%FC80211_ATTR_PROBE_RESP).
 *
 * @FC80211_ATTR_RXMGMT_FLAGS: flags for fc80211_send_mgmt(), u32.
 *	As specified in the &enum fc80211_rxmgmt_flags.
 *
 * @FC80211_ATTR_STA_SUPPORTED_CHANNELS: array of supported channels.
 *
 * @FC80211_ATTR_STA_SUPPORTED_OPER_CLASSES: array of supported
 *      supported operating classes.
 *
 * @FC80211_ATTR_HANDLE_DFS: A flag indicating whether user space
 *	controls DFS operation in IBSS mode. If the flag is included in
 *	%FC80211_CMD_JOIN_IBSS request, the driver will allow use of DFS
 *	channels and reports radar events to userspace. Userspace is required
 *	to react to radar events, e.g. initiate a channel switch or leave the
 *	IBSS network.
 *
 * @FC80211_ATTR_SUPPORT_5_MHZ: A flag indicating that the device supports
 *	5 MHz channel bandwidth.
 * @FC80211_ATTR_SUPPORT_10_MHZ: A flag indicating that the device supports
 *	10 MHz channel bandwidth.
 *
 * @FC80211_ATTR_OPMODE_NOTIF: Operating mode field from Operating Mode
 *	Notification Element based on association request when used with
 *	%FC80211_CMD_NEW_STATION; u8 attribute.
 *
 * @FC80211_ATTR_VENDOR_ID: The vendor ID, either a 24-bit OUI or, if
 *	%FC80211_VENDOR_ID_IS_LINUX is set, a special Linux ID (not used yet)
 * @FC80211_ATTR_VENDOR_SUBCMD: vendor sub-command
 * @FC80211_ATTR_VENDOR_DATA: data for the vendor command, if any; this
 *	attribute is also used for vendor command feature advertisement
 * @FC80211_ATTR_VENDOR_EVENTS: used for event list advertising in the softmac
 *	info, containing a nested array of possible events
 *
 * @FC80211_ATTR_QOS_MAP: IP DSCP mapping for Interworking QoS mapping. This
 *	data is in the format defined for the payload of the QoS Map Set element
 *	in IEEE Std 802.11-2012, 8.4.2.97.
 *
 * @FC80211_ATTR_MAC_HINT: MAC address recommendation as initial BSS
 * @FC80211_ATTR_SOFTMAC_FREQ_HINT: frequency of the recommended initial BSS
 *
 * @FC80211_ATTR_MAX_AP_ASSOC_STA: Device attribute that indicates how many
 *	associated stations are supported in AP mode (including P2P GO); u32.
 *	Since drivers may not have a fixed limit on the maximum number (e.g.,
 *	other concurrent operations may affect this), drivers are allowed to
 *	advertise values that cannot always be met. In such cases, an attempt
 *	to add a new station entry with @FC80211_CMD_NEW_STATION may fail.
 *
 * @FC80211_ATTR_TDLS_PEER_CAPABILITY: flags for TDLS peer capabilities, u32.
 *	As specified in the &enum fc80211_tdls_peer_capability.
 *
 * @FC80211_ATTR_IFACE_SOCKET_OWNER: flag attribute, if set during interface
 *	creation then the new interface will be owned by the netlink socket
 *	that created it and will be destroyed when the socket is closed
 *
 * @FC80211_ATTR_MAX: highest attribute number currently defined
 * @__FC80211_ATTR_AFTER_LAST: internal use
 */
enum fc80211_attrs {
/* don't change the order or add anything between, this is ABI! */
	FC80211_ATTR_UNSPEC,

	FC80211_ATTR_SOFTMAC,
	FC80211_ATTR_SOFTMAC_NAME,

	FC80211_ATTR_IFINDEX,
	FC80211_ATTR_IFNAME,
	FC80211_ATTR_IFTYPE,

	FC80211_ATTR_MAC,

	FC80211_ATTR_KEY_DATA,
	FC80211_ATTR_KEY_IDX,
	FC80211_ATTR_KEY_CIPHER,
	FC80211_ATTR_KEY_SEQ,
	FC80211_ATTR_KEY_DEFAULT,

	FC80211_ATTR_BEACON_INTERVAL,
	FC80211_ATTR_DTIM_PERIOD,
	FC80211_ATTR_BEACON_HEAD,
	FC80211_ATTR_BEACON_TAIL,

	FC80211_ATTR_STA_AID,
	FC80211_ATTR_STA_FLAGS,
	FC80211_ATTR_STA_LISTEN_INTERVAL,
	FC80211_ATTR_STA_SUPPORTED_RATES,
	FC80211_ATTR_STA_VLAN,
	FC80211_ATTR_STA_INFO,

	FC80211_ATTR_SOFTMAC_BANDS,

	FC80211_ATTR_MNTR_FLAGS,

	FC80211_ATTR_MESH_ID,
	FC80211_ATTR_STA_PLINK_ACTION,
	FC80211_ATTR_MPATH_NEXT_HOP,
	FC80211_ATTR_MPATH_INFO,

	FC80211_ATTR_BSS_CTS_PROT,
	FC80211_ATTR_BSS_SHORT_PREAMBLE,
	FC80211_ATTR_BSS_SHORT_SLOT_TIME,

	FC80211_ATTR_HT_CAPABILITY,

	FC80211_ATTR_SUPPORTED_IFTYPES,

	FC80211_ATTR_REG_ALPHA2,
	FC80211_ATTR_REG_RULES,

	FC80211_ATTR_MESH_CONFIG,

	FC80211_ATTR_BSS_BASIC_RATES,

	FC80211_ATTR_SOFTMAC_TXQ_PARAMS,
	FC80211_ATTR_SOFTMAC_FREQ,
	FC80211_ATTR_SOFTMAC_CHANNEL_TYPE,

	FC80211_ATTR_KEY_DEFAULT_MGMT,

	FC80211_ATTR_MGMT_SUBTYPE,
	FC80211_ATTR_IE,

	FC80211_ATTR_MAX_NUM_SCAN_SSIDS,

	FC80211_ATTR_SCAN_FREQUENCIES,
	FC80211_ATTR_SCAN_SSIDS,
	FC80211_ATTR_GENERATION, /* replaces old SCAN_GENERATION */
	FC80211_ATTR_BSS,

	FC80211_ATTR_REG_INITIATOR,
	FC80211_ATTR_REG_TYPE,

	FC80211_ATTR_SUPPORTED_COMMANDS,

	FC80211_ATTR_FRAME,
	FC80211_ATTR_SSID,
	FC80211_ATTR_AUTH_TYPE,
	FC80211_ATTR_REASON_CODE,

	FC80211_ATTR_KEY_TYPE,

	FC80211_ATTR_MAX_SCAN_IE_LEN,
	FC80211_ATTR_CIPHER_SUITES,

	FC80211_ATTR_FREQ_BEFORE,
	FC80211_ATTR_FREQ_AFTER,

	FC80211_ATTR_FREQ_FIXED,


	FC80211_ATTR_SOFTMAC_RETRY_SHORT,
	FC80211_ATTR_SOFTMAC_RETRY_LONG,
	FC80211_ATTR_SOFTMAC_FRAG_THRESHOLD,
	FC80211_ATTR_SOFTMAC_RTS_THRESHOLD,

	FC80211_ATTR_TIMED_OUT,

	FC80211_ATTR_USE_MFP,

	FC80211_ATTR_STA_FLAGS2,

	FC80211_ATTR_CONTROL_PORT,

	FC80211_ATTR_TESTDATA,

	FC80211_ATTR_PRIVACY,

	FC80211_ATTR_DISCONNECTED_BY_AP,
	FC80211_ATTR_STATUS_CODE,

	FC80211_ATTR_CIPHER_SUITES_PAIRWISE,
	FC80211_ATTR_CIPHER_SUITE_GROUP,
	FC80211_ATTR_WPA_VERSIONS,
	FC80211_ATTR_AKM_SUITES,

	FC80211_ATTR_REQ_IE,
	FC80211_ATTR_RESP_IE,

	FC80211_ATTR_PREV_BSSID,

	FC80211_ATTR_KEY,
	FC80211_ATTR_KEYS,

	FC80211_ATTR_PID,

	FC80211_ATTR_4ADDR,

	FC80211_ATTR_SURVEY_INFO,

	FC80211_ATTR_PMKID,
	FC80211_ATTR_MAX_NUM_PMKIDS,

	FC80211_ATTR_DURATION,

	FC80211_ATTR_COOKIE,

	FC80211_ATTR_SOFTMAC_COVERAGE_CLASS,

	FC80211_ATTR_TX_RATES,

	FC80211_ATTR_FRAME_MATCH,

	FC80211_ATTR_ACK,

	FC80211_ATTR_PS_STATE,

	FC80211_ATTR_CQM,

	FC80211_ATTR_LOCAL_STATE_CHANGE,

	FC80211_ATTR_AP_ISOLATE,

	FC80211_ATTR_SOFTMAC_TX_POWER_SETTING,
	FC80211_ATTR_SOFTMAC_TX_POWER_LEVEL,

	FC80211_ATTR_TX_FRAME_TYPES,
	FC80211_ATTR_RX_FRAME_TYPES,
	FC80211_ATTR_FRAME_TYPE,

	FC80211_ATTR_CONTROL_PORT_ETHERTYPE,
	FC80211_ATTR_CONTROL_PORT_NO_ENCRYPT,

	FC80211_ATTR_SUPPORT_IBSS_RSN,

	FC80211_ATTR_SOFTMAC_ANTENNA_TX,
	FC80211_ATTR_SOFTMAC_ANTENNA_RX,

	FC80211_ATTR_MCAST_RATE,

	FC80211_ATTR_OFFCHANNEL_TX_OK,

	FC80211_ATTR_BSS_HT_OPMODE,

	FC80211_ATTR_KEY_DEFAULT_TYPES,

	FC80211_ATTR_MAX_REMAIN_ON_CHANNEL_DURATION,

	FC80211_ATTR_MESH_SETUP,

	FC80211_ATTR_SOFTMAC_ANTENNA_AVAIL_TX,
	FC80211_ATTR_SOFTMAC_ANTENNA_AVAIL_RX,

	FC80211_ATTR_SUPPORT_MESH_AUTH,
	FC80211_ATTR_STA_PLINK_STATE,

	FC80211_ATTR_WOWLAN_TRIGGERS,
	FC80211_ATTR_WOWLAN_TRIGGERS_SUPPORTED,

	FC80211_ATTR_SCHED_SCAN_INTERVAL,

	FC80211_ATTR_INTERFACE_COMBINATIONS,
	FC80211_ATTR_SOFTWARE_IFTYPES,

	FC80211_ATTR_REKEY_DATA,

	FC80211_ATTR_MAX_NUM_SCHED_SCAN_SSIDS,
	FC80211_ATTR_MAX_SCHED_SCAN_IE_LEN,

	FC80211_ATTR_SCAN_SUPP_RATES,

	FC80211_ATTR_HIDDEN_SSID,

	FC80211_ATTR_IE_PROBE_RESP,
	FC80211_ATTR_IE_ASSOC_RESP,

	FC80211_ATTR_STA_WME,
	FC80211_ATTR_SUPPORT_AP_UAPSD,

	FC80211_ATTR_ROAM_SUPPORT,

	FC80211_ATTR_SCHED_SCAN_MATCH,
	FC80211_ATTR_MAX_MATCH_SETS,

	FC80211_ATTR_PMKSA_CANDIDATE,

	FC80211_ATTR_TX_NO_CCK_RATE,

	FC80211_ATTR_TDLS_ACTION,
	FC80211_ATTR_TDLS_DIALOG_TOKEN,
	FC80211_ATTR_TDLS_OPERATION,
	FC80211_ATTR_TDLS_SUPPORT,
	FC80211_ATTR_TDLS_EXTERNAL_SETUP,

	FC80211_ATTR_DEVICE_AP_SME,

	FC80211_ATTR_DONT_WAIT_FOR_ACK,

	FC80211_ATTR_FEATURE_FLAGS,

	FC80211_ATTR_PROBE_RESP_OFFLOAD,

	FC80211_ATTR_PROBE_RESP,

	FC80211_ATTR_DFS_REGION,

	FC80211_ATTR_DISABLE_HT,
	FC80211_ATTR_HT_CAPABILITY_MASK,

	FC80211_ATTR_NOACK_MAP,

	FC80211_ATTR_INACTIVITY_TIMEOUT,

	FC80211_ATTR_RX_SIGNAL_DBM,

	FC80211_ATTR_BG_SCAN_PERIOD,

	FC80211_ATTR_WDEV,

	FC80211_ATTR_USER_REG_HINT_TYPE,

	FC80211_ATTR_CONN_FAILED_REASON,

	FC80211_ATTR_SAE_DATA,

	FC80211_ATTR_VHT_CAPABILITY,

	FC80211_ATTR_SCAN_FLAGS,

	FC80211_ATTR_CHANNEL_WIDTH,
	FC80211_ATTR_CENTER_FREQ1,
	FC80211_ATTR_CENTER_FREQ2,

	FC80211_ATTR_P2P_CTWINDOW,
	FC80211_ATTR_P2P_OPPPS,

	FC80211_ATTR_LOCAL_MESH_POWER_MODE,

	FC80211_ATTR_ACL_POLICY,

	FC80211_ATTR_MAC_ADDRS,

	FC80211_ATTR_MAC_ACL_MAX,

	FC80211_ATTR_RADAR_EVENT,

	FC80211_ATTR_EXT_CAPA,
	FC80211_ATTR_EXT_CAPA_MASK,

	FC80211_ATTR_STA_CAPABILITY,
	FC80211_ATTR_STA_EXT_CAPABILITY,

	FC80211_ATTR_PROTOCOL_FEATURES,
	FC80211_ATTR_SPLIT_SOFTMAC_DUMP,

	FC80211_ATTR_DISABLE_VHT,
	FC80211_ATTR_VHT_CAPABILITY_MASK,

	FC80211_ATTR_MDID,
	FC80211_ATTR_IE_RIC,

	FC80211_ATTR_CRIT_PROT_ID,
	FC80211_ATTR_MAX_CRIT_PROT_DURATION,

	FC80211_ATTR_PEER_AID,

	FC80211_ATTR_COALESCE_RULE,

	FC80211_ATTR_CH_SWITCH_COUNT,
	FC80211_ATTR_CH_SWITCH_BLOCK_TX,
	FC80211_ATTR_CSA_IES,
	FC80211_ATTR_CSA_C_OFF_BEACON,
	FC80211_ATTR_CSA_C_OFF_PRESP,

	FC80211_ATTR_RXMGMT_FLAGS,

	FC80211_ATTR_STA_SUPPORTED_CHANNELS,

	FC80211_ATTR_STA_SUPPORTED_OPER_CLASSES,

	FC80211_ATTR_HANDLE_DFS,

	FC80211_ATTR_SUPPORT_5_MHZ,
	FC80211_ATTR_SUPPORT_10_MHZ,

	FC80211_ATTR_OPMODE_NOTIF,

	FC80211_ATTR_VENDOR_ID,
	FC80211_ATTR_VENDOR_SUBCMD,
	FC80211_ATTR_VENDOR_DATA,
	FC80211_ATTR_VENDOR_EVENTS,

	FC80211_ATTR_QOS_MAP,

	FC80211_ATTR_MAC_HINT,
	FC80211_ATTR_SOFTMAC_FREQ_HINT,

	FC80211_ATTR_MAX_AP_ASSOC_STA,

	FC80211_ATTR_TDLS_PEER_CAPABILITY,

	FC80211_ATTR_IFACE_SOCKET_OWNER,

	/* add attributes here, update the policy in fc80211.c */

	__FC80211_ATTR_AFTER_LAST,
	FC80211_ATTR_MAX = __FC80211_ATTR_AFTER_LAST - 1
};

/* source-level API compatibility */
#define FC80211_ATTR_SCAN_GENERATION	FC80211_ATTR_GENERATION
#define	FC80211_ATTR_MESH_PARAMS	FC80211_ATTR_MESH_CONFIG

/*
 * Allow user space programs to use #ifdef on new attributes by defining them
 * here
 */
#define FC80211_CMD_CONNECT		FC80211_CMD_CONNECT
#define FC80211_ATTR_HT_CAPABILITY	FC80211_ATTR_HT_CAPABILITY
#define FC80211_ATTR_BSS_BASIC_RATES	FC80211_ATTR_BSS_BASIC_RATES
#define FC80211_ATTR_SOFTMAC_TXQ_PARAMS	FC80211_ATTR_SOFTMAC_TXQ_PARAMS
#define FC80211_ATTR_SOFTMAC_FREQ		FC80211_ATTR_SOFTMAC_FREQ
#define FC80211_ATTR_SOFTMAC_CHANNEL_TYPE FC80211_ATTR_SOFTMAC_CHANNEL_TYPE
#define FC80211_ATTR_MGMT_SUBTYPE	FC80211_ATTR_MGMT_SUBTYPE
#define FC80211_ATTR_IE			FC80211_ATTR_IE
#define FC80211_ATTR_REG_INITIATOR	FC80211_ATTR_REG_INITIATOR
#define FC80211_ATTR_REG_TYPE		FC80211_ATTR_REG_TYPE
#define FC80211_ATTR_FRAME		FC80211_ATTR_FRAME
#define FC80211_ATTR_SSID		FC80211_ATTR_SSID
#define FC80211_ATTR_AUTH_TYPE		FC80211_ATTR_AUTH_TYPE
#define FC80211_ATTR_REASON_CODE	FC80211_ATTR_REASON_CODE
#define FC80211_ATTR_CIPHER_SUITES_PAIRWISE FC80211_ATTR_CIPHER_SUITES_PAIRWISE
#define FC80211_ATTR_CIPHER_SUITE_GROUP	FC80211_ATTR_CIPHER_SUITE_GROUP
#define FC80211_ATTR_WPA_VERSIONS	FC80211_ATTR_WPA_VERSIONS
#define FC80211_ATTR_AKM_SUITES		FC80211_ATTR_AKM_SUITES
#define FC80211_ATTR_KEY		FC80211_ATTR_KEY
#define FC80211_ATTR_KEYS		FC80211_ATTR_KEYS
#define FC80211_ATTR_FEATURE_FLAGS	FC80211_ATTR_FEATURE_FLAGS

#define FC80211_MAX_SUPP_RATES			32
#define FC80211_MAX_SUPP_HT_RATES		77
#define FC80211_MAX_SUPP_REG_RULES		32
#define FC80211_TKIP_DATA_OFFSET_ENCR_KEY	0
#define FC80211_TKIP_DATA_OFFSET_TX_MIC_KEY	16
#define FC80211_TKIP_DATA_OFFSET_RX_MIC_KEY	24
#define FC80211_HT_CAPABILITY_LEN		26
#define FC80211_VHT_CAPABILITY_LEN		12

#define FC80211_MAX_NR_CIPHER_SUITES		5
#define FC80211_MAX_NR_AKM_SUITES		2

#define FC80211_MIN_REMAIN_ON_CHANNEL_TIME	10

/* default RSSI threshold for scan results if none specified. */
#define FC80211_SCAN_RSSI_THOLD_OFF		-300

#define FC80211_CQM_TXE_MAX_INTVL		1800

/**
 * enum fc80211_iftype - (virtual) interface types
 *
 * @FC80211_IFTYPE_UNSPECIFIED: unspecified type, driver decides
 * @FC80211_IFTYPE_ADHOC: independent BSS member
 * @FC80211_IFTYPE_STATION: managed BSS member
 * @FC80211_IFTYPE_AP: access point
 * @FC80211_IFTYPE_AP_VLAN: VLAN interface for access points; VLAN interfaces
 *	are a bit special in that they must always be tied to a pre-existing
 *	AP type interface.
 * @FC80211_IFTYPE_WDS: wireless distribution interface
 * @FC80211_IFTYPE_MONITOR: monitor interface receiving all frames
 * @FC80211_IFTYPE_MESH_POINT: mesh point
 * @FC80211_IFTYPE_P2P_CLIENT: P2P client
 * @FC80211_IFTYPE_P2P_GO: P2P group owner
 * @FC80211_IFTYPE_P2P_DEVICE: P2P device interface type, this is not a netdev
 *	and therefore can't be created in the normal ways, use the
 *	%FC80211_CMD_START_P2P_DEVICE and %FC80211_CMD_STOP_P2P_DEVICE
 *	commands to create and destroy one
 * @FC80211_IFTYPE_MAX: highest interface type number currently defined
 * @NUM_FC80211_IFTYPES: number of defined interface types
 *
 * These values are used with the %FC80211_ATTR_IFTYPE
 * to set the type of an interface.
 *
 */
enum fc80211_iftype {
	FC80211_IFTYPE_UNSPECIFIED,
	FC80211_IFTYPE_ADHOC,
	FC80211_IFTYPE_STATION,
	FC80211_IFTYPE_AP,
	FC80211_IFTYPE_AP_VLAN,
	FC80211_IFTYPE_WDS,
	FC80211_IFTYPE_MONITOR,
	FC80211_IFTYPE_MESH_POINT,
	FC80211_IFTYPE_P2P_CLIENT,
	FC80211_IFTYPE_P2P_GO,
	FC80211_IFTYPE_P2P_DEVICE,
	FC80211_IFTYPE_OCB,
	FC80211_IFTYPE_NAN,

	/* keep last */
	NUM_FC80211_IFTYPES,
	FC80211_IFTYPE_MAX = NUM_FC80211_IFTYPES - 1
};

/**
 * enum fc80211_sta_flags - station flags
 *
 * Station flags. When a station is added to an AP interface, it is
 * assumed to be already associated (and hence authenticated.)
 *
 * @__FC80211_STA_FLAG_INVALID: attribute number 0 is reserved
 * @FC80211_STA_FLAG_AUTHORIZED: station is authorized (802.1X)
 * @FC80211_STA_FLAG_SHORT_PREAMBLE: station is capable of receiving frames
 *	with short barker preamble
 * @FC80211_STA_FLAG_WME: station is WME/QoS capable
 * @FC80211_STA_FLAG_MFP: station uses management frame protection
 * @FC80211_STA_FLAG_AUTHENTICATED: station is authenticated
 * @FC80211_STA_FLAG_TDLS_PEER: station is a TDLS peer -- this flag should
 *	only be used in managed mode (even in the flags mask). Note that the
 *	flag can't be changed, it is only valid while adding a station, and
 *	attempts to change it will silently be ignored (rather than rejected
 *	as errors.)
 * @FC80211_STA_FLAG_ASSOCIATED: station is associated; used with drivers
 *	that support %FC80211_FEATURE_FULL_AP_CLIENT_STATE to transition a
 *	previously added station into associated state
 * @FC80211_STA_FLAG_MAX: highest station flag number currently defined
 * @__FC80211_STA_FLAG_AFTER_LAST: internal use
 */
enum fc80211_sta_flags {
	__FC80211_STA_FLAG_INVALID,
	FC80211_STA_FLAG_AUTHORIZED,
	FC80211_STA_FLAG_SHORT_PREAMBLE,
	FC80211_STA_FLAG_WME,
	FC80211_STA_FLAG_MFP,
	FC80211_STA_FLAG_AUTHENTICATED,
	FC80211_STA_FLAG_TDLS_PEER,
	FC80211_STA_FLAG_ASSOCIATED,

	/* keep last */
	__FC80211_STA_FLAG_AFTER_LAST,
	FC80211_STA_FLAG_MAX = __FC80211_STA_FLAG_AFTER_LAST - 1
};

/**
 * enum fc80211_sta_p2p_ps_status - station support of P2P PS
 *
 * @FC80211_P2P_PS_UNSUPPORTED: station doesn't support P2P PS mechanism
 * @@FC80211_P2P_PS_SUPPORTED: station supports P2P PS mechanism
 * @NUM_FC80211_P2P_PS_STATUS: number of values
 */
enum fc80211_sta_p2p_ps_status {
	FC80211_P2P_PS_UNSUPPORTED = 0,
	FC80211_P2P_PS_SUPPORTED,

	NUM_FC80211_P2P_PS_STATUS,
};

#define FC80211_STA_FLAG_MAX_OLD_API	FC80211_STA_FLAG_TDLS_PEER

/**
 * struct fc80211_sta_flag_update - station flags mask/set
 * @mask: mask of station flags to set
 * @set: which values to set them to
 *
 * Both mask and set contain bits as per &enum fc80211_sta_flags.
 */
struct fc80211_sta_flag_update {
	u32	mask;
	u32	set;
};

/**
 * enum fc80211_he_gi - HE guard interval
 * @FC80211_RATE_INFO_HE_GI_0_8: 0.8 usec
 * @FC80211_RATE_INFO_HE_GI_1_6: 1.6 usec
 * @FC80211_RATE_INFO_HE_GI_3_2: 3.2 usec
 */
enum fc80211_he_gi {
	FC80211_RATE_INFO_HE_GI_0_8,
	FC80211_RATE_INFO_HE_GI_1_6,
	FC80211_RATE_INFO_HE_GI_3_2,
};

/**
 * enum fc80211_he_ltf - HE long training field
 * @FC80211_RATE_INFO_HE_1xLTF: 3.2 usec
 * @FC80211_RATE_INFO_HE_2xLTF: 6.4 usec
 * @FC80211_RATE_INFO_HE_4xLTF: 12.8 usec
 */
enum fc80211_he_ltf {
	FC80211_RATE_INFO_HE_1XLTF,
	FC80211_RATE_INFO_HE_2XLTF,
	FC80211_RATE_INFO_HE_4XLTF,
};

/**
 * enum fc80211_rate_info - bitrate information
 *
 * These attribute types are used with %FC80211_STA_INFO_TXRATE
 * when getting information about the bitrate of a station.
 * There are 2 attributes for bitrate, a legacy one that represents
 * a 16-bit value, and new one that represents a 32-bit value.
 * If the rate value fits into 16 bit, both attributes are reported
 * with the same value. If the rate is too high to fit into 16 bits
 * (>6.5535Gbps) only 32-bit attribute is included.
 * User space tools encouraged to use the 32-bit attribute and fall
 * back to the 16-bit one for compatibility with older kernels.
 *
 * @__FC80211_RATE_INFO_INVALID: attribute number 0 is reserved
 * @FC80211_RATE_INFO_BITRATE: total bitrate (u16, 100kbit/s)
 * @FC80211_RATE_INFO_MCS: mcs index for 802.11n (u8)
 * @FC80211_RATE_INFO_40_MHZ_WIDTH: 40 MHz dualchannel bitrate
 * @FC80211_RATE_INFO_SHORT_GI: 400ns guard interval
 * @FC80211_RATE_INFO_BITRATE32: total bitrate (u32, 100kbit/s)
 * @FC80211_RATE_INFO_MAX: highest rate_info number currently defined
 * @FC80211_RATE_INFO_VHT_MCS: MCS index for VHT (u8)
 * @FC80211_RATE_INFO_VHT_NSS: number of streams in VHT (u8)
 * @FC80211_RATE_INFO_80_MHZ_WIDTH: 80 MHz VHT rate
 * @FC80211_RATE_INFO_80P80_MHZ_WIDTH: 80+80 MHz VHT rate
 * @FC80211_RATE_INFO_160_MHZ_WIDTH: 160 MHz VHT rate
 * @__FC80211_RATE_INFO_AFTER_LAST: internal use
 */
enum fc80211_rate_info {
	__FC80211_RATE_INFO_INVALID,
	FC80211_RATE_INFO_BITRATE,
	FC80211_RATE_INFO_MCS,
	FC80211_RATE_INFO_40_MHZ_WIDTH,
	FC80211_RATE_INFO_SHORT_GI,
	FC80211_RATE_INFO_BITRATE32,
	FC80211_RATE_INFO_VHT_MCS,
	FC80211_RATE_INFO_VHT_NSS,
	FC80211_RATE_INFO_80_MHZ_WIDTH,
	FC80211_RATE_INFO_80P80_MHZ_WIDTH,
	FC80211_RATE_INFO_160_MHZ_WIDTH,

	/* keep last */
	__FC80211_RATE_INFO_AFTER_LAST,
	FC80211_RATE_INFO_MAX = __FC80211_RATE_INFO_AFTER_LAST - 1
};

/**
 * enum fc80211_sta_bss_param - BSS information collected by STA
 *
 * These attribute types are used with %FC80211_STA_INFO_BSS_PARAM
 * when getting information about the bitrate of a station.
 *
 * @__FC80211_STA_BSS_PARAM_INVALID: attribute number 0 is reserved
 * @FC80211_STA_BSS_PARAM_CTS_PROT: whether CTS protection is enabled (flag)
 * @FC80211_STA_BSS_PARAM_SHORT_PREAMBLE:  whether short preamble is enabled
 *	(flag)
 * @FC80211_STA_BSS_PARAM_SHORT_SLOT_TIME:  whether short slot time is enabled
 *	(flag)
 * @FC80211_STA_BSS_PARAM_DTIM_PERIOD: DTIM period for beaconing (u8)
 * @FC80211_STA_BSS_PARAM_BEACON_INTERVAL: Beacon interval (u16)
 * @FC80211_STA_BSS_PARAM_MAX: highest sta_bss_param number currently defined
 * @__FC80211_STA_BSS_PARAM_AFTER_LAST: internal use
 */
enum fc80211_sta_bss_param {
	__FC80211_STA_BSS_PARAM_INVALID,
	FC80211_STA_BSS_PARAM_CTS_PROT,
	FC80211_STA_BSS_PARAM_SHORT_PREAMBLE,
	FC80211_STA_BSS_PARAM_SHORT_SLOT_TIME,
	FC80211_STA_BSS_PARAM_DTIM_PERIOD,
	FC80211_STA_BSS_PARAM_BEACON_INTERVAL,

	/* keep last */
	__FC80211_STA_BSS_PARAM_AFTER_LAST,
	FC80211_STA_BSS_PARAM_MAX = __FC80211_STA_BSS_PARAM_AFTER_LAST - 1
};

/**
 * enum fc80211_sta_info - station information
 *
 * These attribute types are used with %FC80211_ATTR_STA_INFO
 * when getting information about a station.
 *
 * @__FC80211_STA_INFO_INVALID: attribute number 0 is reserved
 * @FC80211_STA_INFO_INACTIVE_TIME: time since last activity (u32, msecs)
 * @FC80211_STA_INFO_RX_BYTES: total received bytes (u32, from this station)
 * @FC80211_STA_INFO_TX_BYTES: total transmitted bytes (u32, to this station)
 * @FC80211_STA_INFO_RX_BYTES64: total received bytes (u64, from this station)
 * @FC80211_STA_INFO_TX_BYTES64: total transmitted bytes (u64, to this station)
 * @FC80211_STA_INFO_SIGNAL: signal strength of last received PPDU (u8, dBm)
 * @FC80211_STA_INFO_TX_BITRATE: current unicast tx rate, nested attribute
 * 	containing info as possible, see &enum fc80211_rate_info
 * @FC80211_STA_INFO_RX_PACKETS: total received packet (u32, from this station)
 * @FC80211_STA_INFO_TX_PACKETS: total transmitted packets (u32, to this
 *	station)
 * @FC80211_STA_INFO_TX_RETRIES: total retries (u32, to this station)
 * @FC80211_STA_INFO_TX_FAILED: total failed packets (u32, to this station)
 * @FC80211_STA_INFO_SIGNAL_AVG: signal strength average (u8, dBm)
 * @FC80211_STA_INFO_LLID: the station's mesh LLID
 * @FC80211_STA_INFO_PLID: the station's mesh PLID
 * @FC80211_STA_INFO_PLINK_STATE: peer link state for the station
 *	(see %enum fc80211_plink_state)
 * @FC80211_STA_INFO_RX_BITRATE: last unicast data frame rx rate, nested
 *	attribute, like FC80211_STA_INFO_TX_BITRATE.
 * @FC80211_STA_INFO_BSS_PARAM: current station's view of BSS, nested attribute
 *     containing info as possible, see &enum fc80211_sta_bss_param
 * @FC80211_STA_INFO_CONNECTED_TIME: time since the station is last connected
 * @FC80211_STA_INFO_STA_FLAGS: Contains a struct fc80211_sta_flag_update.
 * @FC80211_STA_INFO_BEACON_LOSS: count of times beacon loss was detected (u32)
 * @FC80211_STA_INFO_T_OFFSET: timing offset with respect to this STA (s64)
 * @FC80211_STA_INFO_LOCAL_PM: local mesh STA link-specific power mode
 * @FC80211_STA_INFO_PEER_PM: peer mesh STA link-specific power mode
 * @FC80211_STA_INFO_NONPEER_PM: neighbor mesh STA power save mode towards
 *	non-peer STA
 * @FC80211_STA_INFO_CHAIN_SIGNAL: per-chain signal strength of last PPDU
 *	Contains a nested array of signal strength attributes (u8, dBm)
 * @FC80211_STA_INFO_CHAIN_SIGNAL_AVG: per-chain signal strength average
 *	Same format as FC80211_STA_INFO_CHAIN_SIGNAL.
 * @__FC80211_STA_INFO_AFTER_LAST: internal
 * @FC80211_STA_INFO_MAX: highest possible station info attribute
 */
enum fc80211_sta_info {
	__FC80211_STA_INFO_INVALID,
	FC80211_STA_INFO_INACTIVE_TIME,
	FC80211_STA_INFO_RX_BYTES,
	FC80211_STA_INFO_TX_BYTES,
	FC80211_STA_INFO_LLID,
	FC80211_STA_INFO_PLID,
	FC80211_STA_INFO_PLINK_STATE,
	FC80211_STA_INFO_SIGNAL,
	FC80211_STA_INFO_TX_BITRATE,
	FC80211_STA_INFO_RX_PACKETS,
	FC80211_STA_INFO_TX_PACKETS,
	FC80211_STA_INFO_TX_RETRIES,
	FC80211_STA_INFO_TX_FAILED,
	FC80211_STA_INFO_SIGNAL_AVG,
	FC80211_STA_INFO_RX_BITRATE,
	FC80211_STA_INFO_BSS_PARAM,
	FC80211_STA_INFO_CONNECTED_TIME,
	FC80211_STA_INFO_STA_FLAGS,
	FC80211_STA_INFO_BEACON_LOSS,
	FC80211_STA_INFO_T_OFFSET,
	FC80211_STA_INFO_LOCAL_PM,
	FC80211_STA_INFO_PEER_PM,
	FC80211_STA_INFO_NONPEER_PM,
	FC80211_STA_INFO_RX_BYTES64,
	FC80211_STA_INFO_TX_BYTES64,
	FC80211_STA_INFO_CHAIN_SIGNAL,
	FC80211_STA_INFO_CHAIN_SIGNAL_AVG,
	FC80211_STA_INFO_EXPECTED_THROUGHPUT,
	FC80211_STA_INFO_RX_DROP_MISC,
	FC80211_STA_INFO_BEACON_RX,
	FC80211_STA_INFO_BEACON_SIGNAL_AVG,
	FC80211_STA_INFO_TID_STATS,
	FC80211_STA_INFO_RX_DURATION,
	FC80211_STA_INFO_PAD,
	FC80211_STA_INFO_ACK_SIGNAL,
	FC80211_STA_INFO_ACK_SIGNAL_AVG,
	FC80211_STA_INFO_RX_MPDUS,
	FC80211_STA_INFO_FCS_ERROR_COUNT,
	FC80211_STA_INFO_CONNECTED_TO_GATE,
	FC80211_STA_INFO_TX_DURATION,
	FC80211_STA_INFO_AIRTIME_WEIGHT,
	FC80211_STA_INFO_AIRTIME_LINK_METRIC,
	FC80211_STA_INFO_ASSOC_AT_BOOTTIME,
	FC80211_STA_INFO_CONNECTED_TO_AS,

	/* keep last */
	__FC80211_STA_INFO_AFTER_LAST,
	FC80211_STA_INFO_MAX = __FC80211_STA_INFO_AFTER_LAST - 1
};

/**
 * enum fc80211_mpath_flags - fc80211 mesh path flags
 *
 * @FC80211_MPATH_FLAG_ACTIVE: the mesh path is active
 * @FC80211_MPATH_FLAG_RESOLVING: the mesh path discovery process is running
 * @FC80211_MPATH_FLAG_SN_VALID: the mesh path contains a valid SN
 * @FC80211_MPATH_FLAG_FIXED: the mesh path has been manually set
 * @FC80211_MPATH_FLAG_RESOLVED: the mesh path discovery process succeeded
 */
enum fc80211_mpath_flags {
	FC80211_MPATH_FLAG_ACTIVE =	1<<0,
	FC80211_MPATH_FLAG_RESOLVING =	1<<1,
	FC80211_MPATH_FLAG_SN_VALID =	1<<2,
	FC80211_MPATH_FLAG_FIXED =	1<<3,
	FC80211_MPATH_FLAG_RESOLVED =	1<<4,
};

/**
 * enum fc80211_mpath_info - mesh path information
 *
 * These attribute types are used with %FC80211_ATTR_MPATH_INFO when getting
 * information about a mesh path.
 *
 * @__FC80211_MPATH_INFO_INVALID: attribute number 0 is reserved
 * @FC80211_MPATH_INFO_FRAME_QLEN: number of queued frames for this destination
 * @FC80211_MPATH_INFO_SN: destination sequence number
 * @FC80211_MPATH_INFO_METRIC: metric (cost) of this mesh path
 * @FC80211_MPATH_INFO_EXPTIME: expiration time for the path, in msec from now
 * @FC80211_MPATH_INFO_FLAGS: mesh path flags, enumerated in
 * 	&enum fc80211_mpath_flags;
 * @FC80211_MPATH_INFO_DISCOVERY_TIMEOUT: total path discovery timeout, in msec
 * @FC80211_MPATH_INFO_DISCOVERY_RETRIES: mesh path discovery retries
 * @FC80211_MPATH_INFO_MAX: highest mesh path information attribute number
 *	currently defind
 * @__FC80211_MPATH_INFO_AFTER_LAST: internal use
 */
enum fc80211_mpath_info {
	__FC80211_MPATH_INFO_INVALID,
	FC80211_MPATH_INFO_FRAME_QLEN,
	FC80211_MPATH_INFO_SN,
	FC80211_MPATH_INFO_METRIC,
	FC80211_MPATH_INFO_EXPTIME,
	FC80211_MPATH_INFO_FLAGS,
	FC80211_MPATH_INFO_DISCOVERY_TIMEOUT,
	FC80211_MPATH_INFO_DISCOVERY_RETRIES,

	/* keep last */
	__FC80211_MPATH_INFO_AFTER_LAST,
	FC80211_MPATH_INFO_MAX = __FC80211_MPATH_INFO_AFTER_LAST - 1
};

/**
 * enum fc80211_band_attr - band attributes
 * @__FC80211_BAND_ATTR_INVALID: attribute number 0 is reserved
 * @FC80211_BAND_ATTR_FREQS: supported frequencies in this band,
 *	an array of nested frequency attributes
 * @FC80211_BAND_ATTR_RATES: supported bitrates in this band,
 *	an array of nested bitrate attributes
 * @FC80211_BAND_ATTR_HT_MCS_SET: 16-byte attribute containing the MCS set as
 *	defined in 802.11n
 * @FC80211_BAND_ATTR_HT_CAPA: HT capabilities, as in the HT information IE
 * @FC80211_BAND_ATTR_HT_AMPDU_FACTOR: A-MPDU factor, as in 11n
 * @FC80211_BAND_ATTR_HT_AMPDU_DENSITY: A-MPDU density, as in 11n
 * @FC80211_BAND_ATTR_VHT_MCS_SET: 32-byte attribute containing the MCS set as
 *	defined in 802.11ac
 * @FC80211_BAND_ATTR_VHT_CAPA: VHT capabilities, as in the HT information IE
 * @FC80211_BAND_ATTR_MAX: highest band attribute currently defined
 * @__FC80211_BAND_ATTR_AFTER_LAST: internal use
 */
enum fc80211_band_attr {
	__FC80211_BAND_ATTR_INVALID,
	FC80211_BAND_ATTR_FREQS,
	FC80211_BAND_ATTR_RATES,

	FC80211_BAND_ATTR_HT_MCS_SET,
	FC80211_BAND_ATTR_HT_CAPA,
	FC80211_BAND_ATTR_HT_AMPDU_FACTOR,
	FC80211_BAND_ATTR_HT_AMPDU_DENSITY,

	FC80211_BAND_ATTR_VHT_MCS_SET,
	FC80211_BAND_ATTR_VHT_CAPA,

	/* keep last */
	__FC80211_BAND_ATTR_AFTER_LAST,
	FC80211_BAND_ATTR_MAX = __FC80211_BAND_ATTR_AFTER_LAST - 1
};

#define FC80211_BAND_ATTR_HT_CAPA FC80211_BAND_ATTR_HT_CAPA

/**
 * enum fc80211_frequency_attr - frequency attributes
 * @__FC80211_FREQUENCY_ATTR_INVALID: attribute number 0 is reserved
 * @FC80211_FREQUENCY_ATTR_FREQ: Frequency in MHz
 * @FC80211_FREQUENCY_ATTR_DISABLED: Channel is disabled in current
 *	regulatory domain.
 * @FC80211_FREQUENCY_ATTR_NO_IR: no mechanisms that initiate radiation
 * 	are permitted on this channel, this includes sending probe
 * 	requests, or modes of operation that require beaconing.
 * @FC80211_FREQUENCY_ATTR_RADAR: Radar detection is mandatory
 *	on this channel in current regulatory domain.
 * @FC80211_FREQUENCY_ATTR_MAX_TX_POWER: Maximum transmission power in mBm
 *	(100 * dBm).
 * @FC80211_FREQUENCY_ATTR_DFS_STATE: current state for DFS
 *	(enum fc80211_dfs_state)
 * @FC80211_FREQUENCY_ATTR_DFS_TIME: time in miliseconds for how long
 *	this channel is in this DFS state.
 * @FC80211_FREQUENCY_ATTR_NO_HT40_MINUS: HT40- isn't possible with this
 *	channel as the control channel
 * @FC80211_FREQUENCY_ATTR_NO_HT40_PLUS: HT40+ isn't possible with this
 *	channel as the control channel
 * @FC80211_FREQUENCY_ATTR_NO_80MHZ: any 80 MHz channel using this channel
 *	as the primary or any of the secondary channels isn't possible,
 *	this includes 80+80 channels
 * @FC80211_FREQUENCY_ATTR_NO_160MHZ: any 160 MHz (but not 80+80) channel
 *	using this channel as the primary or any of the secondary channels
 *	isn't possible
 * @FC80211_FREQUENCY_ATTR_DFS_CAC_TIME: DFS CAC time in milliseconds.
 * @FC80211_FREQUENCY_ATTR_INDOOR_ONLY: Only indoor use is permitted on this
 *	channel. A channel that has the INDOOR_ONLY attribute can only be
 *	used when there is a clear assessment that the device is operating in
 *	an indoor surroundings, i.e., it is connected to AC power (and not
 *	through portable DC inverters) or is under the control of a master
 *	that is acting as an AP and is connected to AC power.
 * @FC80211_FREQUENCY_ATTR_GO_CONCURRENT: GO operation is allowed on this
 *	channel if it's connected concurrently to a BSS on the same channel on
 *	the 2 GHz band or to a channel in the same UNII band (on the 5 GHz
 *	band), and IEEE80211_CHAN_RADAR is not set. Instantiating a GO on a
 *	channel that has the GO_CONCURRENT attribute set can be done when there
 *	is a clear assessment that the device is operating under the guidance of
 *	an authorized master, i.e., setting up a GO while the device is also
 *	connected to an AP with DFS and radar detection on the UNII band (it is
 *	up to user-space, i.e., wpa_supplicant to perform the required
 *	verifications)
 * @FC80211_FREQUENCY_ATTR_NO_20MHZ: 20 MHz operation is not allowed
 *	on this channel in current regulatory domain.
 * @FC80211_FREQUENCY_ATTR_NO_10MHZ: 10 MHz operation is not allowed
 *	on this channel in current regulatory domain.
 * @FC80211_FREQUENCY_ATTR_MAX: highest frequency attribute number
 *	currently defined
 * @__FC80211_FREQUENCY_ATTR_AFTER_LAST: internal use
 *
 * See https://apps.fcc.gov/eas/comments/GetPublishedDocument.html?id=327&tn=528122
 * for more information on the FCC description of the relaxations allowed
 * by FC80211_FREQUENCY_ATTR_INDOOR_ONLY and
 * FC80211_FREQUENCY_ATTR_GO_CONCURRENT.
 */
enum fc80211_frequency_attr {
	__FC80211_FREQUENCY_ATTR_INVALID,
	FC80211_FREQUENCY_ATTR_FREQ,
	FC80211_FREQUENCY_ATTR_DISABLED,
	FC80211_FREQUENCY_ATTR_NO_IR,
	__FC80211_FREQUENCY_ATTR_NO_IBSS,
	FC80211_FREQUENCY_ATTR_RADAR,
	FC80211_FREQUENCY_ATTR_MAX_TX_POWER,
	FC80211_FREQUENCY_ATTR_DFS_STATE,
	FC80211_FREQUENCY_ATTR_DFS_TIME,
	FC80211_FREQUENCY_ATTR_NO_HT40_MINUS,
	FC80211_FREQUENCY_ATTR_NO_HT40_PLUS,
	FC80211_FREQUENCY_ATTR_NO_80MHZ,
	FC80211_FREQUENCY_ATTR_NO_160MHZ,
	FC80211_FREQUENCY_ATTR_DFS_CAC_TIME,
	FC80211_FREQUENCY_ATTR_INDOOR_ONLY,
	FC80211_FREQUENCY_ATTR_GO_CONCURRENT,
	FC80211_FREQUENCY_ATTR_NO_20MHZ,
	FC80211_FREQUENCY_ATTR_NO_10MHZ,

	/* keep last */
	__FC80211_FREQUENCY_ATTR_AFTER_LAST,
	FC80211_FREQUENCY_ATTR_MAX = __FC80211_FREQUENCY_ATTR_AFTER_LAST - 1
};

#define FC80211_FREQUENCY_ATTR_MAX_TX_POWER FC80211_FREQUENCY_ATTR_MAX_TX_POWER
#define FC80211_FREQUENCY_ATTR_PASSIVE_SCAN	FC80211_FREQUENCY_ATTR_NO_IR
#define FC80211_FREQUENCY_ATTR_NO_IBSS		FC80211_FREQUENCY_ATTR_NO_IR
#define FC80211_FREQUENCY_ATTR_NO_IR		FC80211_FREQUENCY_ATTR_NO_IR

/**
 * enum fc80211_bitrate_attr - bitrate attributes
 * @__FC80211_BITRATE_ATTR_INVALID: attribute number 0 is reserved
 * @FC80211_BITRATE_ATTR_RATE: Bitrate in units of 100 kbps
 * @FC80211_BITRATE_ATTR_2GHZ_SHORTPREAMBLE: Short preamble supported
 *	in 2.4 GHz band.
 * @FC80211_BITRATE_ATTR_MAX: highest bitrate attribute number
 *	currently defined
 * @__FC80211_BITRATE_ATTR_AFTER_LAST: internal use
 */
enum fc80211_bitrate_attr {
	__FC80211_BITRATE_ATTR_INVALID,
	FC80211_BITRATE_ATTR_RATE,
	FC80211_BITRATE_ATTR_2GHZ_SHORTPREAMBLE,

	/* keep last */
	__FC80211_BITRATE_ATTR_AFTER_LAST,
	FC80211_BITRATE_ATTR_MAX = __FC80211_BITRATE_ATTR_AFTER_LAST - 1
};

/**
 * enum fc80211_initiator - Indicates the initiator of a reg domain request
 * @FC80211_REGDOM_SET_BY_CORE: Core queried CRDA for a dynamic world
 * 	regulatory domain.
 * @FC80211_REGDOM_SET_BY_USER: User asked the wireless core to set the
 * 	regulatory domain.
 * @FC80211_REGDOM_SET_BY_DRIVER: a wireless drivers has hinted to the
 * 	wireless core it thinks its knows the regulatory domain we should be in.
 * @FC80211_REGDOM_SET_BY_COUNTRY_IE: the wireless core has received an
 * 	802.11 country information element with regulatory information it
 * 	thinks we should consider. cfgd11 only processes the country
 *	code from the IE, and relies on the regulatory domain information
 *	structure passed by userspace (CRDA) from our wireless-regdb.
 *	If a channel is enabled but the country code indicates it should
 *	be disabled we disable the channel and re-enable it upon disassociation.
 */
enum fc80211_reg_initiator {
	FC80211_REGDOM_SET_BY_CORE,
	FC80211_REGDOM_SET_BY_USER,
	FC80211_REGDOM_SET_BY_DRIVER,
	FC80211_REGDOM_SET_BY_COUNTRY_IE,
};

/**
 * enum fc80211_reg_type - specifies the type of regulatory domain
 * @FC80211_REGDOM_TYPE_COUNTRY: the regulatory domain set is one that pertains
 *	to a specific country. When this is set you can count on the
 *	ISO / IEC 3166 alpha2 country code being valid.
 * @FC80211_REGDOM_TYPE_WORLD: the regulatory set domain is the world regulatory
 * 	domain.
 * @FC80211_REGDOM_TYPE_CUSTOM_WORLD: the regulatory domain set is a custom
 * 	driver specific world regulatory domain. These do not apply system-wide
 * 	and are only applicable to the individual devices which have requested
 * 	them to be applied.
 * @FC80211_REGDOM_TYPE_INTERSECTION: the regulatory domain set is the product
 *	of an intersection between two regulatory domains -- the previously
 *	set regulatory domain on the system and the last accepted regulatory
 *	domain request to be processed.
 */
enum fc80211_reg_type {
	FC80211_REGDOM_TYPE_COUNTRY,
	FC80211_REGDOM_TYPE_WORLD,
	FC80211_REGDOM_TYPE_CUSTOM_WORLD,
	FC80211_REGDOM_TYPE_INTERSECTION,
};

/**
 * enum fc80211_reg_rule_attr - regulatory rule attributes
 * @__FC80211_REG_RULE_ATTR_INVALID: attribute number 0 is reserved
 * @FC80211_ATTR_REG_RULE_FLAGS: a set of flags which specify additional
 * 	considerations for a given frequency range. These are the
 * 	&enum fc80211_reg_rule_flags.
 * @FC80211_ATTR_FREQ_RANGE_START: starting frequencry for the regulatory
 * 	rule in KHz. This is not a center of frequency but an actual regulatory
 * 	band edge.
 * @FC80211_ATTR_FREQ_RANGE_END: ending frequency for the regulatory rule
 * 	in KHz. This is not a center a frequency but an actual regulatory
 * 	band edge.
 * @FC80211_ATTR_FREQ_RANGE_MAX_BW: maximum allowed bandwidth for this
 *	frequency range, in KHz.
 * @FC80211_ATTR_POWER_RULE_MAX_ANT_GAIN: the maximum allowed antenna gain
 * 	for a given frequency range. The value is in mBi (100 * dBi).
 * 	If you don't have one then don't send this.
 * @FC80211_ATTR_POWER_RULE_MAX_EIRP: the maximum allowed EIRP for
 * 	a given frequency range. The value is in mBm (100 * dBm).
 * @FC80211_ATTR_DFS_CAC_TIME: DFS CAC time in milliseconds.
 *	If not present or 0 default CAC time will be used.
 * @FC80211_REG_RULE_ATTR_MAX: highest regulatory rule attribute number
 *	currently defined
 * @__FC80211_REG_RULE_ATTR_AFTER_LAST: internal use
 */
enum fc80211_reg_rule_attr {
	__FC80211_REG_RULE_ATTR_INVALID,
	FC80211_ATTR_REG_RULE_FLAGS,

	FC80211_ATTR_FREQ_RANGE_START,
	FC80211_ATTR_FREQ_RANGE_END,
	FC80211_ATTR_FREQ_RANGE_MAX_BW,

	FC80211_ATTR_POWER_RULE_MAX_ANT_GAIN,
	FC80211_ATTR_POWER_RULE_MAX_EIRP,

	FC80211_ATTR_DFS_CAC_TIME,

	/* keep last */
	__FC80211_REG_RULE_ATTR_AFTER_LAST,
	FC80211_REG_RULE_ATTR_MAX = __FC80211_REG_RULE_ATTR_AFTER_LAST - 1
};

/**
 * enum fc80211_sched_scan_match_attr - scheduled scan match attributes
 * @__FC80211_SCHED_SCAN_MATCH_ATTR_INVALID: attribute number 0 is reserved
 * @FC80211_SCHED_SCAN_MATCH_ATTR_SSID: SSID to be used for matching,
 *	only report BSS with matching SSID.
 * @FC80211_SCHED_SCAN_MATCH_ATTR_RSSI: RSSI threshold (in dBm) for reporting a
 *	BSS in scan results. Filtering is turned off if not specified. Note that
 *	if this attribute is in a match set of its own, then it is treated as
 *	the default value for all matchsets with an SSID, rather than being a
 *	matchset of its own without an RSSI filter. This is due to problems with
 *	how this API was implemented in the past. Also, due to the same problem,
 *	the only way to create a matchset with only an RSSI filter (with this
 *	attribute) is if there's only a single matchset with the RSSI attribute.
 * @FC80211_SCHED_SCAN_MATCH_ATTR_MAX: highest scheduled scan filter
 *	attribute number currently defined
 * @__FC80211_SCHED_SCAN_MATCH_ATTR_AFTER_LAST: internal use
 */
enum fc80211_sched_scan_match_attr {
	__FC80211_SCHED_SCAN_MATCH_ATTR_INVALID,

	FC80211_SCHED_SCAN_MATCH_ATTR_SSID,
	FC80211_SCHED_SCAN_MATCH_ATTR_RSSI,

	/* keep last */
	__FC80211_SCHED_SCAN_MATCH_ATTR_AFTER_LAST,
	FC80211_SCHED_SCAN_MATCH_ATTR_MAX =
		__FC80211_SCHED_SCAN_MATCH_ATTR_AFTER_LAST - 1
};

/* only for backward compatibility */
#define FC80211_ATTR_SCHED_SCAN_MATCH_SSID FC80211_SCHED_SCAN_MATCH_ATTR_SSID

/**
 * enum fc80211_reg_rule_flags - regulatory rule flags
 *
 * @FC80211_RRF_NO_OFDM: OFDM modulation not allowed
 * @FC80211_RRF_NO_CCK: CCK modulation not allowed
 * @FC80211_RRF_NO_INDOOR: indoor operation not allowed
 * @FC80211_RRF_NO_OUTDOOR: outdoor operation not allowed
 * @FC80211_RRF_DFS: DFS support is required to be used
 * @FC80211_RRF_PTP_ONLY: this is only for Point To Point links
 * @FC80211_RRF_PTMP_ONLY: this is only for Point To Multi Point links
 * @FC80211_RRF_NO_IR: no mechanisms that initiate radiation are allowed,
 * 	this includes probe requests or modes of operation that require
 * 	beaconing.
 * @FC80211_RRF_AUTO_BW: maximum available bandwidth should be calculated
 *	base on contiguous rules and wider channels will be allowed to cross
 *	multiple contiguous/overlapping frequency ranges.
 */
enum fc80211_reg_rule_flags {
	FC80211_RRF_NO_OFDM		= 1<<0,
	FC80211_RRF_NO_CCK		= 1<<1,
	FC80211_RRF_NO_INDOOR		= 1<<2,
	FC80211_RRF_NO_OUTDOOR		= 1<<3,
	FC80211_RRF_DFS			= 1<<4,
	FC80211_RRF_PTP_ONLY		= 1<<5,
	FC80211_RRF_PTMP_ONLY		= 1<<6,
	FC80211_RRF_NO_IR		= 1<<7,
	__FC80211_RRF_NO_IBSS		= 1<<8,
	FC80211_RRF_AUTO_BW		= 1<<11,
};

#define FC80211_RRF_PASSIVE_SCAN	FC80211_RRF_NO_IR
#define FC80211_RRF_NO_IBSS		FC80211_RRF_NO_IR
#define FC80211_RRF_NO_IR		FC80211_RRF_NO_IR

/* For backport compatibility with older userspace */
#define FC80211_RRF_NO_IR_ALL		(FC80211_RRF_NO_IR | __FC80211_RRF_NO_IBSS)

/**
 * enum fc80211_dfs_regions - regulatory DFS regions
 *
 * @FC80211_DFS_UNSET: Country has no DFS master region specified
 * @FC80211_DFS_FCC: Country follows DFS master rules from FCC
 * @FC80211_DFS_ETSI: Country follows DFS master rules from ETSI
 * @FC80211_DFS_JP: Country follows DFS master rules from JP/MKK/Telec
 */
enum fc80211_dfs_regions {
	FC80211_DFS_UNSET	= 0,
	FC80211_DFS_FCC		= 1,
	FC80211_DFS_ETSI	= 2,
	FC80211_DFS_JP		= 3,
	FC80211_DFS_KR      = 4,
    FC80211_DFS_CH      = 5,
};

/**
 * enum fc80211_user_reg_hint_type - type of user regulatory hint
 *
 * @FC80211_USER_REG_HINT_USER: a user sent the hint. This is always
 *	assumed if the attribute is not set.
 * @FC80211_USER_REG_HINT_CELL_BASE: the hint comes from a cellular
 *	base station. Device drivers that have been tested to work
 *	properly to support this type of hint can enable these hints
 *	by setting the FC80211_FEATURE_CELL_BASE_REG_HINTS feature
 *	capability on the struct softmac. The wireless core will
 *	ignore all cell base station hints until at least one device
 *	present has been registered with the wireless core that
 *	has listed FC80211_FEATURE_CELL_BASE_REG_HINTS as a
 *	supported feature.
 * @FC80211_USER_REG_HINT_INDOOR: a user sent an hint indicating that the
 *	platform is operating in an indoor environment.
 */
enum fc80211_user_reg_hint_type {
	FC80211_USER_REG_HINT_USER	= 0,
	FC80211_USER_REG_HINT_CELL_BASE = 1,
	FC80211_USER_REG_HINT_INDOOR    = 2,
};

/**
 * enum fc80211_survey_info - survey information
 *
 * These attribute types are used with %FC80211_ATTR_SURVEY_INFO
 * when getting information about a survey.
 *
 * @__FC80211_SURVEY_INFO_INVALID: attribute number 0 is reserved
 * @FC80211_SURVEY_INFO_FREQUENCY: center frequency of channel
 * @FC80211_SURVEY_INFO_NOISE: noise level of channel (u8, dBm)
 * @FC80211_SURVEY_INFO_IN_USE: channel is currently being used
 * @FC80211_SURVEY_INFO_CHANNEL_TIME: amount of time (in ms) that the radio
 *	spent on this channel
 * @FC80211_SURVEY_INFO_CHANNEL_TIME_BUSY: amount of the time the primary
 *	channel was sensed busy (either due to activity or energy detect)
 * @FC80211_SURVEY_INFO_CHANNEL_TIME_EXT_BUSY: amount of time the extension
 *	channel was sensed busy
 * @FC80211_SURVEY_INFO_CHANNEL_TIME_RX: amount of time the radio spent
 *	receiving data
 * @FC80211_SURVEY_INFO_CHANNEL_TIME_TX: amount of time the radio spent
 *	transmitting data
 * @FC80211_SURVEY_INFO_MAX: highest survey info attribute number
 *	currently defined
 * @__FC80211_SURVEY_INFO_AFTER_LAST: internal use
 */
enum fc80211_survey_info {
	__FC80211_SURVEY_INFO_INVALID,
	FC80211_SURVEY_INFO_FREQUENCY,
	FC80211_SURVEY_INFO_NOISE,
	FC80211_SURVEY_INFO_IN_USE,
	FC80211_SURVEY_INFO_CHANNEL_TIME,
	FC80211_SURVEY_INFO_CHANNEL_TIME_BUSY,
	FC80211_SURVEY_INFO_CHANNEL_TIME_EXT_BUSY,
	FC80211_SURVEY_INFO_CHANNEL_TIME_RX,
	FC80211_SURVEY_INFO_CHANNEL_TIME_TX,

	/* keep last */
	__FC80211_SURVEY_INFO_AFTER_LAST,
	FC80211_SURVEY_INFO_MAX = __FC80211_SURVEY_INFO_AFTER_LAST - 1
};

/**
 * enum fc80211_mntr_flags - monitor configuration flags
 *
 * Monitor configuration flags.
 *
 * @__FC80211_MNTR_FLAG_INVALID: reserved
 *
 * @FC80211_MNTR_FLAG_FCSFAIL: pass frames with bad FCS
 * @FC80211_MNTR_FLAG_PLCPFAIL: pass frames with bad PLCP
 * @FC80211_MNTR_FLAG_CONTROL: pass control frames
 * @FC80211_MNTR_FLAG_OTHER_BSS: disable BSSID filtering
 * @FC80211_MNTR_FLAG_COOK_FRAMES: report frames after processing.
 *	overrides all other flags.
 * @FC80211_MNTR_FLAG_ACTIVE: use the configured MAC address
 *	and ACK incoming unicast packets.
 *
 * @__FC80211_MNTR_FLAG_AFTER_LAST: internal use
 * @FC80211_MNTR_FLAG_MAX: highest possible monitor flag
 */
enum fc80211_mntr_flags {
	__FC80211_MNTR_FLAG_INVALID,
	FC80211_MNTR_FLAG_FCSFAIL,
	FC80211_MNTR_FLAG_PLCPFAIL,
	FC80211_MNTR_FLAG_CONTROL,
	FC80211_MNTR_FLAG_OTHER_BSS,
	FC80211_MNTR_FLAG_COOK_FRAMES,
	FC80211_MNTR_FLAG_ACTIVE,

	/* keep last */
	__FC80211_MNTR_FLAG_AFTER_LAST,
	FC80211_MNTR_FLAG_MAX = __FC80211_MNTR_FLAG_AFTER_LAST - 1
};

/**
 * enum fc80211_mesh_power_mode - mesh power save modes
 *
 * @FC80211_MESH_POWER_UNKNOWN: The mesh power mode of the mesh STA is
 *	not known or has not been set yet.
 * @FC80211_MESH_POWER_ACTIVE: Active mesh power mode. The mesh STA is
 *	in Awake state all the time.
 * @FC80211_MESH_POWER_LIGHT_SLEEP: Light sleep mode. The mesh STA will
 *	alternate between Active and Doze states, but will wake up for
 *	neighbor's beacons.
 * @FC80211_MESH_POWER_DEEP_SLEEP: Deep sleep mode. The mesh STA will
 *	alternate between Active and Doze states, but may not wake up
 *	for neighbor's beacons.
 *
 * @__FC80211_MESH_POWER_AFTER_LAST - internal use
 * @FC80211_MESH_POWER_MAX - highest possible power save level
 */

enum fc80211_mesh_power_mode {
	FC80211_MESH_POWER_UNKNOWN,
	FC80211_MESH_POWER_ACTIVE,
	FC80211_MESH_POWER_LIGHT_SLEEP,
	FC80211_MESH_POWER_DEEP_SLEEP,

	__FC80211_MESH_POWER_AFTER_LAST,
	FC80211_MESH_POWER_MAX = __FC80211_MESH_POWER_AFTER_LAST - 1
};

/**
 * enum fc80211_meshconf_params - mesh configuration parameters
 *
 * Mesh configuration parameters. These can be changed while the mesh is
 * active.
 *
 * @__FC80211_MESHCONF_INVALID: internal use
 *
 * @FC80211_MESHCONF_RETRY_TIMEOUT: specifies the initial retry timeout in
 *	millisecond units, used by the Peer Link Open message
 *
 * @FC80211_MESHCONF_CONFIRM_TIMEOUT: specifies the initial confirm timeout, in
 *	millisecond units, used by the peer link management to close a peer link
 *
 * @FC80211_MESHCONF_HOLDING_TIMEOUT: specifies the holding timeout, in
 *	millisecond units
 *
 * @FC80211_MESHCONF_MAX_PEER_LINKS: maximum number of peer links allowed
 *	on this mesh interface
 *
 * @FC80211_MESHCONF_MAX_RETRIES: specifies the maximum number of peer link
 *	open retries that can be sent to establish a new peer link instance in a
 *	mesh
 *
 * @FC80211_MESHCONF_TTL: specifies the value of TTL field set at a source mesh
 *	point.
 *
 * @FC80211_MESHCONF_AUTO_OPEN_PLINKS: whether we should automatically open
 *	peer links when we detect compatible mesh peers. Disabled if
 *	@FC80211_MESH_SETUP_USERSPACE_MPM or @FC80211_MESH_SETUP_USERSPACE_AMPE are
 *	set.
 *
 * @FC80211_MESHCONF_HWMP_MAX_PREQ_RETRIES: the number of action frames
 *	containing a PREQ that an MP can send to a particular destination (path
 *	target)
 *
 * @FC80211_MESHCONF_PATH_REFRESH_TIME: how frequently to refresh mesh paths
 *	(in milliseconds)
 *
 * @FC80211_MESHCONF_MIN_DISCOVERY_TIMEOUT: minimum length of time to wait
 *	until giving up on a path discovery (in milliseconds)
 *
 * @FC80211_MESHCONF_HWMP_ACTIVE_PATH_TIMEOUT: The time (in TUs) for which mesh
 *	points receiving a PREQ shall consider the forwarding information from
 *	the root to be valid. (TU = time unit)
 *
 * @FC80211_MESHCONF_HWMP_PREQ_MIN_INTERVAL: The minimum interval of time (in
 *	TUs) during which an MP can send only one action frame containing a PREQ
 *	reference element
 *
 * @FC80211_MESHCONF_HWMP_NET_DIAM_TRVS_TIME: The interval of time (in TUs)
 *	that it takes for an HWMP information element to propagate across the
 *	mesh
 *
 * @FC80211_MESHCONF_HWMP_ROOTMODE: whether root mode is enabled or not
 *
 * @FC80211_MESHCONF_ELEMENT_TTL: specifies the value of TTL field set at a
 *	source mesh point for path selection elements.
 *
 * @FC80211_MESHCONF_HWMP_RANN_INTERVAL:  The interval of time (in TUs) between
 *	root announcements are transmitted.
 *
 * @FC80211_MESHCONF_GATE_ANNOUNCEMENTS: Advertise that this mesh station has
 *	access to a broader network beyond the MBSS.  This is done via Root
 *	Announcement frames.
 *
 * @FC80211_MESHCONF_HWMP_PERR_MIN_INTERVAL: The minimum interval of time (in
 *	TUs) during which a mesh STA can send only one Action frame containing a
 *	PERR element.
 *
 * @FC80211_MESHCONF_FORWARDING: set Mesh STA as forwarding or non-forwarding
 *	or forwarding entity (default is TRUE - forwarding entity)
 *
 * @FC80211_MESHCONF_RSSI_THRESHOLD: RSSI threshold in dBm. This specifies the
 *	threshold for average signal strength of candidate station to establish
 *	a peer link.
 *
 * @FC80211_MESHCONF_SYNC_OFFSET_MAX_NEIGHBOR: maximum number of neighbors
 *	to synchronize to for 11s default synchronization method
 *	(see 11C.12.2.2)
 *
 * @FC80211_MESHCONF_HT_OPMODE: set mesh HT protection mode.
 *
 * @FC80211_MESHCONF_ATTR_MAX: highest possible mesh configuration attribute
 *
 * @FC80211_MESHCONF_HWMP_PATH_TO_ROOT_TIMEOUT: The time (in TUs) for
 *	which mesh STAs receiving a proactive PREQ shall consider the forwarding
 *	information to the root mesh STA to be valid.
 *
 * @FC80211_MESHCONF_HWMP_ROOT_INTERVAL: The interval of time (in TUs) between
 *	proactive PREQs are transmitted.
 *
 * @FC80211_MESHCONF_HWMP_CONFIRMATION_INTERVAL: The minimum interval of time
 *	(in TUs) during which a mesh STA can send only one Action frame
 *	containing a PREQ element for root path confirmation.
 *
 * @FC80211_MESHCONF_POWER_MODE: Default mesh power mode for new peer links.
 *	type &enum fc80211_mesh_power_mode (u32)
 *
 * @FC80211_MESHCONF_AWAKE_WINDOW: awake window duration (in TUs)
 *
 * @FC80211_MESHCONF_PLINK_TIMEOUT: If no tx activity is seen from a STA we've
 *	established peering with for longer than this time (in seconds), then
 *	remove it from the STA's list of peers.  Default is 30 minutes.
 *
 * @__FC80211_MESHCONF_ATTR_AFTER_LAST: internal use
 */
enum fc80211_meshconf_params {
	__FC80211_MESHCONF_INVALID,
	FC80211_MESHCONF_RETRY_TIMEOUT,
	FC80211_MESHCONF_CONFIRM_TIMEOUT,
	FC80211_MESHCONF_HOLDING_TIMEOUT,
	FC80211_MESHCONF_MAX_PEER_LINKS,
	FC80211_MESHCONF_MAX_RETRIES,
	FC80211_MESHCONF_TTL,
	FC80211_MESHCONF_AUTO_OPEN_PLINKS,
	FC80211_MESHCONF_HWMP_MAX_PREQ_RETRIES,
	FC80211_MESHCONF_PATH_REFRESH_TIME,
	FC80211_MESHCONF_MIN_DISCOVERY_TIMEOUT,
	FC80211_MESHCONF_HWMP_ACTIVE_PATH_TIMEOUT,
	FC80211_MESHCONF_HWMP_PREQ_MIN_INTERVAL,
	FC80211_MESHCONF_HWMP_NET_DIAM_TRVS_TIME,
	FC80211_MESHCONF_HWMP_ROOTMODE,
	FC80211_MESHCONF_ELEMENT_TTL,
	FC80211_MESHCONF_HWMP_RANN_INTERVAL,
	FC80211_MESHCONF_GATE_ANNOUNCEMENTS,
	FC80211_MESHCONF_HWMP_PERR_MIN_INTERVAL,
	FC80211_MESHCONF_FORWARDING,
	FC80211_MESHCONF_RSSI_THRESHOLD,
	FC80211_MESHCONF_SYNC_OFFSET_MAX_NEIGHBOR,
	FC80211_MESHCONF_HT_OPMODE,
	FC80211_MESHCONF_HWMP_PATH_TO_ROOT_TIMEOUT,
	FC80211_MESHCONF_HWMP_ROOT_INTERVAL,
	FC80211_MESHCONF_HWMP_CONFIRMATION_INTERVAL,
	FC80211_MESHCONF_POWER_MODE,
	FC80211_MESHCONF_AWAKE_WINDOW,
	FC80211_MESHCONF_PLINK_TIMEOUT,

	/* keep last */
	__FC80211_MESHCONF_ATTR_AFTER_LAST,
	FC80211_MESHCONF_ATTR_MAX = __FC80211_MESHCONF_ATTR_AFTER_LAST - 1
};

/**
 * enum fc80211_mesh_setup_params - mesh setup parameters
 *
 * Mesh setup parameters.  These are used to start/join a mesh and cannot be
 * changed while the mesh is active.
 *
 * @__FC80211_MESH_SETUP_INVALID: Internal use
 *
 * @FC80211_MESH_SETUP_ENABLE_VENDOR_PATH_SEL: Enable this option to use a
 *	vendor specific path selection algorithm or disable it to use the
 *	default HWMP.
 *
 * @FC80211_MESH_SETUP_ENABLE_VENDOR_METRIC: Enable this option to use a
 *	vendor specific path metric or disable it to use the default Airtime
 *	metric.
 *
 * @FC80211_MESH_SETUP_IE: Information elements for this mesh, for instance, a
 *	robust security network ie, or a vendor specific information element
 *	that vendors will use to identify the path selection methods and
 *	metrics in use.
 *
 * @FC80211_MESH_SETUP_USERSPACE_AUTH: Enable this option if an authentication
 *	daemon will be authenticating mesh candidates.
 *
 * @FC80211_MESH_SETUP_USERSPACE_AMPE: Enable this option if an authentication
 *	daemon will be securing peer link frames.  AMPE is a secured version of
 *	Mesh Peering Management (MPM) and is implemented with the assistance of
 *	a userspace daemon.  When this flag is set, the kernel will send peer
 *	management frames to a userspace daemon that will implement AMPE
 *	functionality (security capabilities selection, key confirmation, and
 *	key management).  When the flag is unset (default), the kernel can
 *	autonomously complete (unsecured) mesh peering without the need of a
 *	userspace daemon.
 *
 * @FC80211_MESH_SETUP_ENABLE_VENDOR_SYNC: Enable this option to use a
 *	vendor specific synchronization method or disable it to use the default
 *	neighbor offset synchronization
 *
 * @FC80211_MESH_SETUP_USERSPACE_MPM: Enable this option if userspace will
 *	implement an MPM which handles peer allocation and state.
 *
 * @FC80211_MESH_SETUP_AUTH_PROTOCOL: Inform the kernel of the authentication
 *	method (u8, as defined in IEEE 8.4.2.100.6, e.g. 0x1 for SAE).
 *	Default is no authentication method required.
 *
 * @FC80211_MESH_SETUP_ATTR_MAX: highest possible mesh setup attribute number
 *
 * @__FC80211_MESH_SETUP_ATTR_AFTER_LAST: Internal use
 */
enum fc80211_mesh_setup_params {
	__FC80211_MESH_SETUP_INVALID,
	FC80211_MESH_SETUP_ENABLE_VENDOR_PATH_SEL,
	FC80211_MESH_SETUP_ENABLE_VENDOR_METRIC,
	FC80211_MESH_SETUP_IE,
	FC80211_MESH_SETUP_USERSPACE_AUTH,
	FC80211_MESH_SETUP_USERSPACE_AMPE,
	FC80211_MESH_SETUP_ENABLE_VENDOR_SYNC,
	FC80211_MESH_SETUP_USERSPACE_MPM,
	FC80211_MESH_SETUP_AUTH_PROTOCOL,

	/* keep last */
	__FC80211_MESH_SETUP_ATTR_AFTER_LAST,
	FC80211_MESH_SETUP_ATTR_MAX = __FC80211_MESH_SETUP_ATTR_AFTER_LAST - 1
};

/**
 * enum fc80211_txq_attr - TX queue parameter attributes
 * @__FC80211_TXQ_ATTR_INVALID: Attribute number 0 is reserved
 * @FC80211_TXQ_ATTR_AC: AC identifier (FC80211_AC_*)
 * @FC80211_TXQ_ATTR_TXOP: Maximum burst time in units of 32 usecs, 0 meaning
 *	disabled
 * @FC80211_TXQ_ATTR_CWMIN: Minimum contention window [a value of the form
 *	2^n-1 in the range 1..32767]
 * @FC80211_TXQ_ATTR_CWMAX: Maximum contention window [a value of the form
 *	2^n-1 in the range 1..32767]
 * @FC80211_TXQ_ATTR_AIFS: Arbitration interframe space [0..255]
 * @__FC80211_TXQ_ATTR_AFTER_LAST: Internal
 * @FC80211_TXQ_ATTR_MAX: Maximum TXQ attribute number
 */
enum fc80211_txq_attr {
	__FC80211_TXQ_ATTR_INVALID,
	FC80211_TXQ_ATTR_AC,
	FC80211_TXQ_ATTR_TXOP,
	FC80211_TXQ_ATTR_CWMIN,
	FC80211_TXQ_ATTR_CWMAX,
	FC80211_TXQ_ATTR_AIFS,

	/* keep last */
	__FC80211_TXQ_ATTR_AFTER_LAST,
	FC80211_TXQ_ATTR_MAX = __FC80211_TXQ_ATTR_AFTER_LAST - 1
};

enum fc80211_ac {
	FC80211_AC_VO,
	FC80211_AC_VI,
	FC80211_AC_BE,
	FC80211_AC_BK,
	FC80211_NUM_ACS
};

/* backward compat */
#define FC80211_TXQ_ATTR_QUEUE	FC80211_TXQ_ATTR_AC
#define FC80211_TXQ_Q_VO	FC80211_AC_VO
#define FC80211_TXQ_Q_VI	FC80211_AC_VI
#define FC80211_TXQ_Q_BE	FC80211_AC_BE
#define FC80211_TXQ_Q_BK	FC80211_AC_BK

/**
 * enum fc80211_channel_type - channel type
 * @FC80211_CHAN_NO_HT: 20 MHz, non-HT channel
 * @FC80211_CHAN_HT20: 20 MHz HT channel
 * @FC80211_CHAN_HT40MINUS: HT40 channel, secondary channel
 *	below the control channel
 * @FC80211_CHAN_HT40PLUS: HT40 channel, secondary channel
 *	above the control channel
 */
enum fc80211_channel_type {
	FC80211_CHAN_NO_HT,
	FC80211_CHAN_HT20,
	FC80211_CHAN_HT40MINUS,
	FC80211_CHAN_HT40PLUS
};

/**
 * enum fc80211_key_mode - Key mode
 *
 * @FC80211_KEY_RX_TX: (Default)
 *	Key can be used for Rx and Tx immediately
 *
 * The following modes can only be selected for unicast keys and when the
 * driver supports @FC80211_EXT_FEATURE_EXT_KEY_ID:
 *
 * @FC80211_KEY_NO_TX: Only allowed in combination with @FC80211_CMD_NEW_KEY:
 *	Unicast key can only be used for Rx, Tx not allowed, yet
 * @FC80211_KEY_SET_TX: Only allowed in combination with @FC80211_CMD_SET_KEY:
 *	The unicast key identified by idx and mac is cleared for Tx and becomes
 *	the preferred Tx key for the station.
 */
enum fc80211_key_mode {
	FC80211_KEY_RX_TX,
	FC80211_KEY_NO_TX,
	FC80211_KEY_SET_TX
};

/**
 * enum fc80211_chan_width - channel width definitions
 *
 * These values are used with the %FC80211_ATTR_CHANNEL_WIDTH
 * attribute.
 *
 * @FC80211_CHAN_WIDTH_20_NOHT: 20 MHz, non-HT channel
 * @FC80211_CHAN_WIDTH_20: 20 MHz HT channel
 * @FC80211_CHAN_WIDTH_40: 40 MHz channel, the %FC80211_ATTR_CENTER_FREQ1
 *	attribute must be provided as well
 * @FC80211_CHAN_WIDTH_80: 80 MHz channel, the %FC80211_ATTR_CENTER_FREQ1
 *	attribute must be provided as well
 * @FC80211_CHAN_WIDTH_80P80: 80+80 MHz channel, the %FC80211_ATTR_CENTER_FREQ1
 *	and %FC80211_ATTR_CENTER_FREQ2 attributes must be provided as well
 * @FC80211_CHAN_WIDTH_160: 160 MHz channel, the %FC80211_ATTR_CENTER_FREQ1
 *	attribute must be provided as well
 * @FC80211_CHAN_WIDTH_5: 5 MHz OFDM channel
 * @FC80211_CHAN_WIDTH_10: 10 MHz OFDM channel
 */
enum fc80211_chan_width {
	FC80211_CHAN_WIDTH_20_NOHT,
	FC80211_CHAN_WIDTH_20,
	FC80211_CHAN_WIDTH_40,
	FC80211_CHAN_WIDTH_80,
	FC80211_CHAN_WIDTH_80P80,
	FC80211_CHAN_WIDTH_160,
	FC80211_CHAN_WIDTH_5,
	FC80211_CHAN_WIDTH_10,
	FC80211_CHAN_WIDTH_1,
	FC80211_CHAN_WIDTH_2,
	FC80211_CHAN_WIDTH_4,
	FC80211_CHAN_WIDTH_8,
	FC80211_CHAN_WIDTH_16,
};

/**
 * enum fc80211_bss_scan_width - control channel width for a BSS
 *
 * These values are used with the %FC80211_BSS_CHAN_WIDTH attribute.
 *
 * @FC80211_BSS_CHAN_WIDTH_20: control channel is 20 MHz wide or compatible
 * @FC80211_BSS_CHAN_WIDTH_10: control channel is 10 MHz wide
 * @FC80211_BSS_CHAN_WIDTH_5: control channel is 5 MHz wide
 */
enum fc80211_bss_scan_width {
	FC80211_BSS_CHAN_WIDTH_20,
	FC80211_BSS_CHAN_WIDTH_10,
	FC80211_BSS_CHAN_WIDTH_5,
};

/**
 * enum fc80211_bss - netlink attributes for a BSS
 *
 * @__FC80211_BSS_INVALID: invalid
 * @FC80211_BSS_BSSID: BSSID of the BSS (6 octets)
 * @FC80211_BSS_FREQUENCY: frequency in MHz (u32)
 * @FC80211_BSS_TSF: TSF of the received probe response/beacon (u64)
 * @FC80211_BSS_BEACON_INTERVAL: beacon interval of the (I)BSS (u16)
 * @FC80211_BSS_CAPABILITY: capability field (CPU order, u16)
 * @FC80211_BSS_INFORMATION_ELEMENTS: binary attribute containing the
 *	raw information elements from the probe response/beacon (bin);
 *	if the %FC80211_BSS_BEACON_IES attribute is present, the IEs here are
 *	from a Probe Response frame; otherwise they are from a Beacon frame.
 *	However, if the driver does not indicate the source of the IEs, these
 *	IEs may be from either frame subtype.
 * @FC80211_BSS_SIGNAL_MBM: signal strength of probe response/beacon
 *	in mBm (100 * dBm) (s32)
 * @FC80211_BSS_SIGNAL_UNSPEC: signal strength of the probe response/beacon
 *	in unspecified units, scaled to 0..100 (u8)
 * @FC80211_BSS_STATUS: status, if this BSS is "used"
 * @FC80211_BSS_SEEN_MS_AGO: age of this BSS entry in ms
 * @FC80211_BSS_BEACON_IES: binary attribute containing the raw information
 *	elements from a Beacon frame (bin); not present if no Beacon frame has
 *	yet been received
 * @FC80211_BSS_CHAN_WIDTH: channel width of the control channel
 *	(u32, enum fc80211_bss_scan_width)
 * @__FC80211_BSS_AFTER_LAST: internal
 * @FC80211_BSS_MAX: highest BSS attribute
 */
enum fc80211_bss {
	__FC80211_BSS_INVALID,
	FC80211_BSS_BSSID,
	FC80211_BSS_FREQUENCY,
	FC80211_BSS_TSF,
	FC80211_BSS_BEACON_INTERVAL,
	FC80211_BSS_CAPABILITY,
	FC80211_BSS_INFORMATION_ELEMENTS,
	FC80211_BSS_SIGNAL_MBM,
	FC80211_BSS_SIGNAL_UNSPEC,
	FC80211_BSS_STATUS,
	FC80211_BSS_SEEN_MS_AGO,
	FC80211_BSS_BEACON_IES,
	FC80211_BSS_CHAN_WIDTH,

	/* keep last */
	__FC80211_BSS_AFTER_LAST,
	FC80211_BSS_MAX = __FC80211_BSS_AFTER_LAST - 1
};

/**
 * enum fc80211_bss_status - BSS "status"
 * @FC80211_BSS_STATUS_AUTHENTICATED: Authenticated with this BSS.
 *	Note that this is no longer used since cfg80211 no longer
 *	keeps track of whether or not authentication was done with
 *	a given BSS.
 * @FC80211_BSS_STATUS_ASSOCIATED: Associated with this BSS.
 * @FC80211_BSS_STATUS_IBSS_JOINED: Joined to this IBSS.
 *
 * The BSS status is a BSS attribute in scan dumps, which
 * indicates the status the interface has wrt. this BSS.
 */
enum fc80211_bss_status {
	/// Nothing.
	FCD11_BSS_STATUS_NOTHING,
	/// Authenticated with this BSS.
	FCD11_BSS_STATUS_AUTHENTICATED,
	/// Associated with this BSS.
	FCD11_BSS_STATUS_ASSOCIATED,
	/// Joined to this IBSS.
	FCD11_BSS_STATUS_IBSS_JOINED,

};

/**
 * enum fc80211_auth_type - AuthenticationType
 *
 * @FC80211_AUTHTYPE_OPEN_SYSTEM: Open System authentication
 * @FC80211_AUTHTYPE_SHARED_KEY: Shared Key authentication (WEP only)
 * @FC80211_AUTHTYPE_FT: Fast BSS Transition (IEEE 802.11r)
 * @FC80211_AUTHTYPE_NETWORK_EAP: Network EAP (some Cisco APs and mainly LEAP)
 * @FC80211_AUTHTYPE_SAE: Simultaneous authentication of equals
 * @__FC80211_AUTHTYPE_NUM: internal
 * @FC80211_AUTHTYPE_MAX: maximum valid auth algorithm
 * @FC80211_AUTHTYPE_AUTOMATIC: determine automatically (if necessary by
 *	trying multiple times); this is invalid in netlink -- leave out
 *	the attribute for this on CONNECT commands.
 */
enum fc80211_auth_type {
	FC80211_AUTHTYPE_OPEN_SYSTEM,
	FC80211_AUTHTYPE_SHARED_KEY,
	FC80211_AUTHTYPE_FT,
	FC80211_AUTHTYPE_NETWORK_EAP,
	FC80211_AUTHTYPE_SAE,
	FC80211_AUTHTYPE_FILS_SK,
	FC80211_AUTHTYPE_FILS_SK_PFS,
	FC80211_AUTHTYPE_FILS_PK,

	/* keep last */
	__FC80211_AUTHTYPE_NUM,
	FC80211_AUTHTYPE_MAX = __FC80211_AUTHTYPE_NUM - 1,
	FC80211_AUTHTYPE_AUTOMATIC
};

/**
 * enum fc80211_key_type - Key Type
 * @FC80211_KEYTYPE_GROUP: Group (broadcast/multicast) key
 * @FC80211_KEYTYPE_PAIRWISE: Pairwise (unicast/individual) key
 * @FC80211_KEYTYPE_PEERKEY: PeerKey (DLS)
 * @NUM_FC80211_KEYTYPES: number of defined key types
 */
enum fc80211_key_type {
	FC80211_KEYTYPE_GROUP,
	FC80211_KEYTYPE_PAIRWISE,
	FC80211_KEYTYPE_PEERKEY,

	NUM_FC80211_KEYTYPES
};

/**
 * enum fc80211_mfp - Management frame protection state
 * @FC80211_MFP_NO: Management frame protection not used
 * @FC80211_MFP_REQUIRED: Management frame protection required
 */
enum fc80211_mfp {
	FC80211_MFP_NO,
	FC80211_MFP_REQUIRED,
	FC80211_MFP_OPTIONAL,
};

enum fc80211_wpa_versions {
	FC80211_WPA_VERSION_1 = 1 << 0,
	FC80211_WPA_VERSION_2 = 1 << 1,
};

/**
 * enum fc80211_key_default_types - key default types
 * @__FC80211_KEY_DEFAULT_TYPE_INVALID: invalid
 * @FC80211_KEY_DEFAULT_TYPE_UNICAST: key should be used as default
 *	unicast key
 * @FC80211_KEY_DEFAULT_TYPE_MULTICAST: key should be used as default
 *	multicast key
 * @NUM_FC80211_KEY_DEFAULT_TYPES: number of default types
 */
enum fc80211_key_default_types {
	__FC80211_KEY_DEFAULT_TYPE_INVALID,
	FC80211_KEY_DEFAULT_TYPE_UNICAST,
	FC80211_KEY_DEFAULT_TYPE_MULTICAST,

	NUM_FC80211_KEY_DEFAULT_TYPES
};

/**
 * enum fc80211_key_attributes - key attributes
 * @__FC80211_KEY_INVALID: invalid
 * @FC80211_KEY_DATA: (temporal) key data; for TKIP this consists of
 *	16 bytes encryption key followed by 8 bytes each for TX and RX MIC
 *	keys
 * @FC80211_KEY_IDX: key ID (u8, 0-3)
 * @FC80211_KEY_CIPHER: key cipher suite (u32, as defined by IEEE 802.11
 *	section 7.3.2.25.1, e.g. 0x000FAC04)
 * @FC80211_KEY_SEQ: transmit key sequence number (IV/PN) for TKIP and
 *	CCMP keys, each six bytes in little endian
 * @FC80211_KEY_DEFAULT: flag indicating default key
 * @FC80211_KEY_DEFAULT_MGMT: flag indicating default management key
 * @FC80211_KEY_TYPE: the key type from enum fc80211_key_type, if not
 *	specified the default depends on whether a MAC address was
 *	given with the command using the key or not (u32)
 * @FC80211_KEY_DEFAULT_TYPES: A nested attribute containing flags
 *	attributes, specifying what a key should be set as default as.
 *	See &enum fc80211_key_default_types.
 * @__FC80211_KEY_AFTER_LAST: internal
 * @FC80211_KEY_MAX: highest key attribute
 */
enum fc80211_key_attributes {
	__FC80211_KEY_INVALID,
	FC80211_KEY_DATA,
	FC80211_KEY_IDX,
	FC80211_KEY_CIPHER,
	FC80211_KEY_SEQ,
	FC80211_KEY_DEFAULT,
	FC80211_KEY_DEFAULT_MGMT,
	FC80211_KEY_TYPE,
	FC80211_KEY_DEFAULT_TYPES,

	/* keep last */
	__FC80211_KEY_AFTER_LAST,
	FC80211_KEY_MAX = __FC80211_KEY_AFTER_LAST - 1
};

/**
 * enum fc80211_tx_rate_attributes - TX rate set attributes
 * @__FC80211_TXRATE_INVALID: invalid
 * @FC80211_TXRATE_LEGACY: Legacy (non-MCS) rates allowed for TX rate selection
 *	in an array of rates as defined in IEEE 802.11 7.3.2.2 (u8 values with
 *	1 = 500 kbps) but without the IE length restriction (at most
 *	%FC80211_MAX_SUPP_RATES in a single array).
 * @FC80211_TXRATE_HT: HT (MCS) rates allowed for TX rate selection
 *	in an array of MCS numbers.
 * @FC80211_TXRATE_VHT: VHT rates allowed for TX rate selection,
 *	see &struct fc80211_txrate_vht
 * @FC80211_TXRATE_GI: configure GI, see &enum fc80211_txrate_gi
 * @__FC80211_TXRATE_AFTER_LAST: internal
 * @FC80211_TXRATE_MAX: highest TX rate attribute
 */
enum fc80211_tx_rate_attributes {
	__FC80211_TXRATE_INVALID,
	FC80211_TXRATE_LEGACY,
	FC80211_TXRATE_HT,
	FC80211_TXRATE_VHT,
	FC80211_TXRATE_GI,

	/* keep last */
	__FC80211_TXRATE_AFTER_LAST,
	FC80211_TXRATE_MAX = __FC80211_TXRATE_AFTER_LAST - 1
};

#define FC80211_TXRATE_MCS FC80211_TXRATE_HT
#define FC80211_VHT_NSS_MAX		8

/**
 * struct fc80211_txrate_vht - VHT MCS/NSS txrate bitmap
 * @mcs: MCS bitmap table for each NSS (array index 0 for 1 stream, etc.)
 */
struct fc80211_txrate_vht {
	u16 mcs[FC80211_VHT_NSS_MAX];
};

#define FC80211_HE_NSS_MAX		8

enum fc80211_txrate_gi {
	FC80211_TXRATE_DEFAULT_GI,
	FC80211_TXRATE_FORCE_SGI,
	FC80211_TXRATE_FORCE_LGI,
};

/**
 * enum fc80211_band - Frequency band
 * @FC80211_BAND_2GHZ: 2.4 GHz ISM band
 * @FC80211_BAND_5GHZ: around 5 GHz band (4.9 - 5.7 GHz)
 * @FC80211_BAND_60GHZ: around 60 GHz band (58.32 - 64.80 GHz)
 * @FC80211_BAND_6GHZ: around 6 GHz band (5.9 - 7.2 GHz)
 * @NUM_FC80211_BANDS: number of bands, avoid using this in userspace
 */
enum fc80211_band {
	FC80211_BAND_2GHZ,
	FC80211_BAND_5GHZ,
	FC80211_BAND_60GHZ,
	FC80211_BAND_6GHZ,

	NUM_FC80211_BANDS
};

/**
 * enum fc80211_ps_state - powersave state
 * @FC80211_PS_DISABLED: powersave is disabled
 * @FC80211_PS_ENABLED: powersave is enabled
 */
enum fc80211_ps_state {
	FC80211_PS_DISABLED,
	FC80211_PS_ENABLED,
};

/**
 * enum fc80211_attr_cqm - connection quality monitor attributes
 * @__FC80211_ATTR_CQM_INVALID: invalid
 * @FC80211_ATTR_CQM_RSSI_THOLD: RSSI threshold in dBm. This value specifies
 *	the threshold for the RSSI level at which an event will be sent. Zero
 *	to disable.
 * @FC80211_ATTR_CQM_RSSI_HYST: RSSI hysteresis in dBm. This value specifies
 *	the minimum amount the RSSI level must change after an event before a
 *	new event may be issued (to reduce effects of RSSI oscillation).
 * @FC80211_ATTR_CQM_RSSI_THRESHOLD_EVENT: RSSI threshold event
 * @FC80211_ATTR_CQM_PKT_LOSS_EVENT: a u32 value indicating that this many
 *	consecutive packets were not acknowledged by the peer
 * @FC80211_ATTR_CQM_TXE_RATE: TX error rate in %. Minimum % of TX failures
 *	during the given %FC80211_ATTR_CQM_TXE_INTVL before an
 *	%FC80211_CMD_NOTIFY_CQM with reported %FC80211_ATTR_CQM_TXE_RATE and
 *	%FC80211_ATTR_CQM_TXE_PKTS is generated.
 * @FC80211_ATTR_CQM_TXE_PKTS: number of attempted packets in a given
 *	%FC80211_ATTR_CQM_TXE_INTVL before %FC80211_ATTR_CQM_TXE_RATE is
 *	checked.
 * @FC80211_ATTR_CQM_TXE_INTVL: interval in seconds. Specifies the periodic
 *	interval in which %FC80211_ATTR_CQM_TXE_PKTS and
* @FC80211_ATTR_CQM_BEACON_LOSS_EVENT: flag attribute that's set in a beacon
 *	loss event
 *	%FC80211_ATTR_CQM_TXE_RATE must be satisfied before generating an
 *	%FC80211_CMD_NOTIFY_CQM. Set to 0 to turn off TX error reporting.
 * @__FC80211_ATTR_CQM_AFTER_LAST: internal
 * @FC80211_ATTR_CQM_MAX: highest key attribute
 */
enum fc80211_attr_cqm {
	__FC80211_ATTR_CQM_INVALID,
	FC80211_ATTR_CQM_RSSI_THOLD,
	FC80211_ATTR_CQM_RSSI_HYST,
	FC80211_ATTR_CQM_RSSI_THRESHOLD_EVENT,
	FC80211_ATTR_CQM_PKT_LOSS_EVENT,
	FC80211_ATTR_CQM_TXE_RATE,
	FC80211_ATTR_CQM_TXE_PKTS,
	FC80211_ATTR_CQM_TXE_INTVL,
	FC80211_ATTR_CQM_BEACON_LOSS_EVENT,

	/* keep last */
	__FC80211_ATTR_CQM_AFTER_LAST,
	FC80211_ATTR_CQM_MAX = __FC80211_ATTR_CQM_AFTER_LAST - 1
};

/**
 * enum fc80211_cqm_rssi_threshold_event - RSSI threshold event
 * @FC80211_CQM_RSSI_THRESHOLD_EVENT_LOW: The RSSI level is lower than the
 *      configured threshold
 * @FC80211_CQM_RSSI_THRESHOLD_EVENT_HIGH: The RSSI is higher than the
 *      configured threshold
 * @FC80211_CQM_RSSI_BEACON_LOSS_EVENT: The device experienced beacon loss.
 *	(Note that deauth/disassoc will still follow if the AP is not
 *	available. This event might get used as roaming event, etc.)
 */
enum fc80211_cqm_rssi_threshold_event {
	FC80211_CQM_RSSI_THRESHOLD_EVENT_LOW,
	FC80211_CQM_RSSI_THRESHOLD_EVENT_HIGH,
	FC80211_CQM_RSSI_BEACON_LOSS_EVENT,
};


/**
 * enum fc80211_tx_power_setting - TX power adjustment
 * @FC80211_TX_POWER_AUTOMATIC: automatically determine transmit power
 * @FC80211_TX_POWER_LIMITED: limit TX power by the mBm parameter
 * @FC80211_TX_POWER_FIXED: fix TX power to the mBm parameter
 */
enum fc80211_tx_power_setting {
	FC80211_TX_POWER_AUTOMATIC,
	FC80211_TX_POWER_LIMITED,
	FC80211_TX_POWER_FIXED,
	FC80211_TX_POWER_GET = 9
};

/**
 * enum fc80211_tid_config - TID config state
 * @FC80211_TID_CONFIG_ENABLE: Enable config for the TID
 * @FC80211_TID_CONFIG_DISABLE: Disable config for the TID
 */
enum fc80211_tid_config {
	FC80211_TID_CONFIG_ENABLE,
	FC80211_TID_CONFIG_DISABLE,
};

/* enum fc80211_tx_rate_setting - TX rate configuration type
 * @FC80211_TX_RATE_AUTOMATIC: automatically determine TX rate
 * @FC80211_TX_RATE_LIMITED: limit the TX rate by the TX rate parameter
 * @FC80211_TX_RATE_FIXED: fix TX rate to the TX rate parameter
 */
enum fc80211_tx_rate_setting {
	FC80211_TX_RATE_AUTOMATIC,
	FC80211_TX_RATE_LIMITED,
	FC80211_TX_RATE_FIXED,
};

/**
 * enum fc80211_packet_pattern_attr - packet pattern attribute
 * @__FC80211_PKTPAT_INVALID: invalid number for nested attribute
 * @FC80211_PKTPAT_PATTERN: the pattern, values where the mask has
 *	a zero bit are ignored
 * @FC80211_PKTPAT_MASK: pattern mask, must be long enough to have
 *	a bit for each byte in the pattern. The lowest-order bit corresponds
 *	to the first byte of the pattern, but the bytes of the pattern are
 *	in a little-endian-like format, i.e. the 9th byte of the pattern
 *	corresponds to the lowest-order bit in the second byte of the mask.
 *	For example: The match 00:xx:00:00:xx:00:00:00:00:xx:xx:xx (where
 *	xx indicates "don't care") would be represented by a pattern of
 *	twelve zero bytes, and a mask of "0xed,0x01".
 *	Note that the pattern matching is done as though frames were not
 *	802.11 frames but 802.3 frames, i.e. the frame is fully unpacked
 *	first (including SNAP header unpacking) and then matched.
 * @FC80211_PKTPAT_OFFSET: packet offset, pattern is matched after
 *	these fixed number of bytes of received packet
 * @NUM_FC80211_PKTPAT: number of attributes
 * @MAX_FC80211_PKTPAT: max attribute number
 */
enum fc80211_packet_pattern_attr {
	__FC80211_PKTPAT_INVALID,
	FC80211_PKTPAT_MASK,
	FC80211_PKTPAT_PATTERN,
	FC80211_PKTPAT_OFFSET,

	NUM_FC80211_PKTPAT,
	MAX_FC80211_PKTPAT = NUM_FC80211_PKTPAT - 1,
};

/**
 * struct fc80211_pattern_support - packet pattern support information
 * @max_patterns: maximum number of patterns supported
 * @min_pattern_len: minimum length of each pattern
 * @max_pattern_len: maximum length of each pattern
 * @max_pkt_offset: maximum Rx packet offset
 *
 * This struct is carried in %FC80211_WOWLAN_TRIG_PKT_PATTERN when
 * that is part of %FC80211_ATTR_WOWLAN_TRIGGERS_SUPPORTED or in
 * %FC80211_ATTR_COALESCE_RULE_PKT_PATTERN when that is part of
 * %FC80211_ATTR_COALESCE_RULE in the capability information given
 * by the kernel to userspace.
 */
struct fc80211_pattern_support {
	u32 max_patterns;
	u32 min_pattern_len;
	u32 max_pattern_len;
	u32 max_pkt_offset;
};

/* only for backward compatibility */
#define __FC80211_WOWLAN_PKTPAT_INVALID __FC80211_PKTPAT_INVALID
#define FC80211_WOWLAN_PKTPAT_MASK FC80211_PKTPAT_MASK
#define FC80211_WOWLAN_PKTPAT_PATTERN FC80211_PKTPAT_PATTERN
#define FC80211_WOWLAN_PKTPAT_OFFSET FC80211_PKTPAT_OFFSET
#define NUM_FC80211_WOWLAN_PKTPAT NUM_FC80211_PKTPAT
#define MAX_FC80211_WOWLAN_PKTPAT MAX_FC80211_PKTPAT
#define fc80211_wowlan_pattern_support fc80211_pattern_support

/**
 * enum fc80211_wowlan_triggers - WoWLAN trigger definitions
 * @__FC80211_WOWLAN_TRIG_INVALID: invalid number for nested attributes
 * @FC80211_WOWLAN_TRIG_ANY: wake up on any activity, do not really put
 *	the chip into a special state -- works best with chips that have
 *	support for low-power operation already (flag)
 * @FC80211_WOWLAN_TRIG_DISCONNECT: wake up on disconnect, the way disconnect
 *	is detected is implementation-specific (flag)
 * @FC80211_WOWLAN_TRIG_MAGIC_PKT: wake up on magic packet (6x 0xff, followed
 *	by 16 repetitions of MAC addr, anywhere in payload) (flag)
 * @FC80211_WOWLAN_TRIG_PKT_PATTERN: wake up on the specified packet patterns
 *	which are passed in an array of nested attributes, each nested attribute
 *	defining a with attributes from &struct fc80211_wowlan_trig_pkt_pattern.
 *	Each pattern defines a wakeup packet. Packet offset is associated with
 *	each pattern which is used while matching the pattern. The matching is
 *	done on the MSDU, i.e. as though the packet was an 802.3 packet, so the
 *	pattern matching is done after the packet is converted to the MSDU.
 *
 *	In %FC80211_ATTR_WOWLAN_TRIGGERS_SUPPORTED, it is a binary attribute
 *	carrying a &struct fc80211_pattern_support.
 *
 *	When reporting wakeup. it is a u32 attribute containing the 0-based
 *	index of the pattern that caused the wakeup, in the patterns passed
 *	to the kernel when configuring.
 * @FC80211_WOWLAN_TRIG_GTK_REKEY_SUPPORTED: Not a real trigger, and cannot be
 *	used when setting, used only to indicate that GTK rekeying is supported
 *	by the device (flag)
 * @FC80211_WOWLAN_TRIG_GTK_REKEY_FAILURE: wake up on GTK rekey failure (if
 *	done by the device) (flag)
 * @FC80211_WOWLAN_TRIG_EAP_IDENT_REQUEST: wake up on EAP Identity Request
 *	packet (flag)
 * @FC80211_WOWLAN_TRIG_4WAY_HANDSHAKE: wake up on 4-way handshake (flag)
 * @FC80211_WOWLAN_TRIG_RFKILL_RELEASE: wake up when rfkill is released
 *	(on devices that have rfkill in the device) (flag)
 * @FC80211_WOWLAN_TRIG_WAKEUP_PKT_80211: For wakeup reporting only, contains
 *	the 802.11 packet that caused the wakeup, e.g. a deauth frame. The frame
 *	may be truncated, the @FC80211_WOWLAN_TRIG_WAKEUP_PKT_80211_LEN
 *	attribute contains the original length.
 * @FC80211_WOWLAN_TRIG_WAKEUP_PKT_80211_LEN: Original length of the 802.11
 *	packet, may be bigger than the @FC80211_WOWLAN_TRIG_WAKEUP_PKT_80211
 *	attribute if the packet was truncated somewhere.
 * @FC80211_WOWLAN_TRIG_WAKEUP_PKT_8023: For wakeup reporting only, contains the
 *	802.11 packet that caused the wakeup, e.g. a magic packet. The frame may
 *	be truncated, the @FC80211_WOWLAN_TRIG_WAKEUP_PKT_8023_LEN attribute
 *	contains the original length.
 * @FC80211_WOWLAN_TRIG_WAKEUP_PKT_8023_LEN: Original length of the 802.3
 *	packet, may be bigger than the @FC80211_WOWLAN_TRIG_WAKEUP_PKT_8023
 *	attribute if the packet was truncated somewhere.
 * @FC80211_WOWLAN_TRIG_TCP_CONNECTION: TCP connection wake, see DOC section
 *	"TCP connection wakeup" for more details. This is a nested attribute
 *	containing the exact information for establishing and keeping alive
 *	the TCP connection.
 * @FC80211_WOWLAN_TRIG_TCP_WAKEUP_MATCH: For wakeup reporting only, the
 *	wakeup packet was received on the TCP connection
 * @FC80211_WOWLAN_TRIG_WAKEUP_TCP_CONNLOST: For wakeup reporting only, the
 *	TCP connection was lost or failed to be established
 * @FC80211_WOWLAN_TRIG_WAKEUP_TCP_NOMORETOKENS: For wakeup reporting only,
 *	the TCP connection ran out of tokens to use for data to send to the
 *	service
 * @NUM_FC80211_WOWLAN_TRIG: number of wake on wireless triggers
 * @MAX_FC80211_WOWLAN_TRIG: highest wowlan trigger attribute number
 *
 * These nested attributes are used to configure the wakeup triggers and
 * to report the wakeup reason(s).
 */
enum fc80211_wowlan_triggers {
	__FC80211_WOWLAN_TRIG_INVALID,
	FC80211_WOWLAN_TRIG_ANY,
	FC80211_WOWLAN_TRIG_DISCONNECT,
	FC80211_WOWLAN_TRIG_MAGIC_PKT,
	FC80211_WOWLAN_TRIG_PKT_PATTERN,
	FC80211_WOWLAN_TRIG_GTK_REKEY_SUPPORTED,
	FC80211_WOWLAN_TRIG_GTK_REKEY_FAILURE,
	FC80211_WOWLAN_TRIG_EAP_IDENT_REQUEST,
	FC80211_WOWLAN_TRIG_4WAY_HANDSHAKE,
	FC80211_WOWLAN_TRIG_RFKILL_RELEASE,
	FC80211_WOWLAN_TRIG_WAKEUP_PKT_80211,
	FC80211_WOWLAN_TRIG_WAKEUP_PKT_80211_LEN,
	FC80211_WOWLAN_TRIG_WAKEUP_PKT_8023,
	FC80211_WOWLAN_TRIG_WAKEUP_PKT_8023_LEN,
	FC80211_WOWLAN_TRIG_TCP_CONNECTION,
	FC80211_WOWLAN_TRIG_WAKEUP_TCP_MATCH,
	FC80211_WOWLAN_TRIG_WAKEUP_TCP_CONNLOST,
	FC80211_WOWLAN_TRIG_WAKEUP_TCP_NOMORETOKENS,

	/* keep last */
	NUM_FC80211_WOWLAN_TRIG,
	MAX_FC80211_WOWLAN_TRIG = NUM_FC80211_WOWLAN_TRIG - 1
};

/**
 * DOC: TCP connection wakeup
 *
 * Some devices can establish a TCP connection in order to be woken up by a
 * packet coming in from outside their network segment, or behind NAT. If
 * configured, the device will establish a TCP connection to the given
 * service, and periodically send data to that service. The first data
 * packet is usually transmitted after SYN/ACK, also ACKing the SYN/ACK.
 * The data packets can optionally include a (little endian) sequence
 * number (in the TCP payload!) that is generated by the device, and, also
 * optionally, a token from a list of tokens. This serves as a keep-alive
 * with the service, and for NATed connections, etc.
 *
 * During this keep-alive period, the server doesn't send any data to the
 * client. When receiving data, it is compared against the wakeup pattern
 * (and mask) and if it matches, the host is woken up. Similarly, if the
 * connection breaks or cannot be established to start with, the host is
 * also woken up.
 *
 * Developer's note: ARP offload is required for this, otherwise TCP
 * response packets might not go through correctly.
 */

/**
 * struct fc80211_wowlan_tcp_data_seq - WoWLAN TCP data sequence
 * @start: starting value
 * @offset: offset of sequence number in packet
 * @len: length of the sequence value to write, 1 through 4
 *
 * Note: don't confuse with the TCP sequence number(s), this is for the
 * keepalive packet payload. The actual value is written into the packet
 * in little endian.
 */
struct fc80211_wowlan_tcp_data_seq {
	u32 start, offset, len;
};

/**
 * struct fc80211_wowlan_tcp_data_token - WoWLAN TCP data token config
 * @offset: offset of token in packet
 * @len: length of each token
 * @token_stream: stream of data to be used for the tokens, the length must
 *	be a multiple of @len for this to make sense
 */
struct fc80211_wowlan_tcp_data_token {
	u32 offset, len;
	u8 token_stream[];
};

/**
 * struct fc80211_wowlan_tcp_data_token_feature - data token features
 * @min_len: minimum token length
 * @max_len: maximum token length
 * @bufsize: total available token buffer size (max size of @token_stream)
 */
struct fc80211_wowlan_tcp_data_token_feature {
	u32 min_len, max_len, bufsize;
};

/**
 * enum fc80211_wowlan_tcp_attrs - WoWLAN TCP connection parameters
 * @__FC80211_WOWLAN_TCP_INVALID: invalid number for nested attributes
 * @FC80211_WOWLAN_TCP_SRC_IPV4: source IPv4 address (in network byte order)
 * @FC80211_WOWLAN_TCP_DST_IPV4: destination IPv4 address
 *	(in network byte order)
 * @FC80211_WOWLAN_TCP_DST_MAC: destination MAC address, this is given because
 *	route lookup when configured might be invalid by the time we suspend,
 *	and doing a route lookup when suspending is no longer possible as it
 *	might require ARP querying.
 * @FC80211_WOWLAN_TCP_SRC_PORT: source port (u16); optional, if not given a
 *	socket and port will be allocated
 * @FC80211_WOWLAN_TCP_DST_PORT: destination port (u16)
 * @FC80211_WOWLAN_TCP_DATA_PAYLOAD: data packet payload, at least one byte.
 *	For feature advertising, a u32 attribute holding the maximum length
 *	of the data payload.
 * @FC80211_WOWLAN_TCP_DATA_PAYLOAD_SEQ: data packet sequence configuration
 *	(if desired), a &struct fc80211_wowlan_tcp_data_seq. For feature
 *	advertising it is just a flag
 * @FC80211_WOWLAN_TCP_DATA_PAYLOAD_TOKEN: data packet token configuration,
 *	see &struct fc80211_wowlan_tcp_data_token and for advertising see
 *	&struct fc80211_wowlan_tcp_data_token_feature.
 * @FC80211_WOWLAN_TCP_DATA_INTERVAL: data interval in seconds, maximum
 *	interval in feature advertising (u32)
 * @FC80211_WOWLAN_TCP_WAKE_PAYLOAD: wake packet payload, for advertising a
 *	u32 attribute holding the maximum length
 * @FC80211_WOWLAN_TCP_WAKE_MASK: Wake packet payload mask, not used for
 *	feature advertising. The mask works like @FC80211_PKTPAT_MASK
 *	but on the TCP payload only.
 * @NUM_FC80211_WOWLAN_TCP: number of TCP attributes
 * @MAX_FC80211_WOWLAN_TCP: highest attribute number
 */
enum fc80211_wowlan_tcp_attrs {
	__FC80211_WOWLAN_TCP_INVALID,
	FC80211_WOWLAN_TCP_SRC_IPV4,
	FC80211_WOWLAN_TCP_DST_IPV4,
	FC80211_WOWLAN_TCP_DST_MAC,
	FC80211_WOWLAN_TCP_SRC_PORT,
	FC80211_WOWLAN_TCP_DST_PORT,
	FC80211_WOWLAN_TCP_DATA_PAYLOAD,
	FC80211_WOWLAN_TCP_DATA_PAYLOAD_SEQ,
	FC80211_WOWLAN_TCP_DATA_PAYLOAD_TOKEN,
	FC80211_WOWLAN_TCP_DATA_INTERVAL,
	FC80211_WOWLAN_TCP_WAKE_PAYLOAD,
	FC80211_WOWLAN_TCP_WAKE_MASK,

	/* keep last */
	NUM_FC80211_WOWLAN_TCP,
	MAX_FC80211_WOWLAN_TCP = NUM_FC80211_WOWLAN_TCP - 1
};

/**
 * struct fc80211_coalesce_rule_support - coalesce rule support information
 * @max_rules: maximum number of rules supported
 * @pat: packet pattern support information
 * @max_delay: maximum supported coalescing delay in msecs
 *
 * This struct is carried in %FC80211_ATTR_COALESCE_RULE in the
 * capability information given by the kernel to userspace.
 */
struct fc80211_coalesce_rule_support {
	u32 max_rules;
	struct fc80211_pattern_support pat;
	u32 max_delay;
};

/**
 * enum fc80211_attr_coalesce_rule - coalesce rule attribute
 * @__FC80211_COALESCE_RULE_INVALID: invalid number for nested attribute
 * @FC80211_ATTR_COALESCE_RULE_DELAY: delay in msecs used for packet coalescing
 * @FC80211_ATTR_COALESCE_RULE_CONDITION: condition for packet coalescence,
 *	see &enum fc80211_coalesce_condition.
 * @FC80211_ATTR_COALESCE_RULE_PKT_PATTERN: packet offset, pattern is matched
 *	after these fixed number of bytes of received packet
 * @NUM_FC80211_ATTR_COALESCE_RULE: number of attributes
 * @FC80211_ATTR_COALESCE_RULE_MAX: max attribute number
 */
enum fc80211_attr_coalesce_rule {
	__FC80211_COALESCE_RULE_INVALID,
	FC80211_ATTR_COALESCE_RULE_DELAY,
	FC80211_ATTR_COALESCE_RULE_CONDITION,
	FC80211_ATTR_COALESCE_RULE_PKT_PATTERN,

	/* keep last */
	NUM_FC80211_ATTR_COALESCE_RULE,
	FC80211_ATTR_COALESCE_RULE_MAX = NUM_FC80211_ATTR_COALESCE_RULE - 1
};

/**
 * enum fc80211_coalesce_condition - coalesce rule conditions
 * @FC80211_COALESCE_CONDITION_MATCH: coalaesce Rx packets when patterns
 *	in a rule are matched.
 * @FC80211_COALESCE_CONDITION_NO_MATCH: coalesce Rx packets when patterns
 *	in a rule are not matched.
 */
enum fc80211_coalesce_condition {
	FC80211_COALESCE_CONDITION_MATCH,
	FC80211_COALESCE_CONDITION_NO_MATCH
};

/**
 * enum fc80211_iface_limit_attrs - limit attributes
 * @FC80211_IFACE_LIMIT_UNSPEC: (reserved)
 * @FC80211_IFACE_LIMIT_MAX: maximum number of interfaces that
 *	can be chosen from this set of interface types (u32)
 * @FC80211_IFACE_LIMIT_TYPES: nested attribute containing a
 *	flag attribute for each interface type in this set
 * @NUM_FC80211_IFACE_LIMIT: number of attributes
 * @MAX_FC80211_IFACE_LIMIT: highest attribute number
 */
enum fc80211_iface_limit_attrs {
	FC80211_IFACE_LIMIT_UNSPEC,
	FC80211_IFACE_LIMIT_MAX,
	FC80211_IFACE_LIMIT_TYPES,

	/* keep last */
	NUM_FC80211_IFACE_LIMIT,
	MAX_FC80211_IFACE_LIMIT = NUM_FC80211_IFACE_LIMIT - 1
};

/**
 * enum fc80211_if_combination_attrs -- interface combination attributes
 *
 * @FC80211_IFACE_COMB_UNSPEC: (reserved)
 * @FC80211_IFACE_COMB_LIMITS: Nested attributes containing the limits
 *	for given interface types, see &enum fc80211_iface_limit_attrs.
 * @FC80211_IFACE_COMB_MAXNUM: u32 attribute giving the total number of
 *	interfaces that can be created in this group. This number doesn't
 *	apply to interfaces purely managed in software, which are listed
 *	in a separate attribute %FC80211_ATTR_INTERFACES_SOFTWARE.
 * @FC80211_IFACE_COMB_STA_AP_BI_MATCH: flag attribute specifying that
 *	beacon intervals within this group must be all the same even for
 *	infrastructure and AP/GO combinations, i.e. the GO(s) must adopt
 *	the infrastructure network's beacon interval.
 * @FC80211_IFACE_COMB_NUM_CHANNELS: u32 attribute specifying how many
 *	different channels may be used within this group.
 * @FC80211_IFACE_COMB_RADAR_DETECT_WIDTHS: u32 attribute containing the bitmap
 *	of supported channel widths for radar detection.
 * @NUM_FC80211_IFACE_COMB: number of attributes
 * @MAX_FC80211_IFACE_COMB: highest attribute number
 *
 * Examples:
 *	limits = [ #{STA} <= 1, #{AP} <= 1 ], matching BI, channels = 1, max = 2
 *	=> allows an AP and a STA that must match BIs
 *
 *	numbers = [ #{AP, P2P-GO} <= 8 ], channels = 1, max = 8
 *	=> allows 8 of AP/GO
 *
 *	numbers = [ #{STA} <= 2 ], channels = 2, max = 2
 *	=> allows two STAs on different channels
 *
 *	numbers = [ #{STA} <= 1, #{P2P-client,P2P-GO} <= 3 ], max = 4
 *	=> allows a STA plus three P2P interfaces
 *
 * The list of these four possiblities could completely be contained
 * within the %FC80211_ATTR_INTERFACE_COMBINATIONS attribute to indicate
 * that any of these groups must match.
 *
 * "Combinations" of just a single interface will not be listed here,
 * a single interface of any valid interface type is assumed to always
 * be possible by itself. This means that implicitly, for each valid
 * interface type, the following group always exists:
 *	numbers = [ #{<type>} <= 1 ], channels = 1, max = 1
 */
enum fc80211_if_combination_attrs {
	FC80211_IFACE_COMB_UNSPEC,
	FC80211_IFACE_COMB_LIMITS,
	FC80211_IFACE_COMB_MAXNUM,
	FC80211_IFACE_COMB_STA_AP_BI_MATCH,
	FC80211_IFACE_COMB_NUM_CHANNELS,
	FC80211_IFACE_COMB_RADAR_DETECT_WIDTHS,

	/* keep last */
	NUM_FC80211_IFACE_COMB,
	MAX_FC80211_IFACE_COMB = NUM_FC80211_IFACE_COMB - 1
};


/**
 * enum fc80211_plink_state - state of a mesh peer link finite state machine
 *
 * @FC80211_PLINK_LISTEN: initial state, considered the implicit
 *	state of non existant mesh peer links
 * @FC80211_PLINK_OPN_SNT: mesh plink open frame has been sent to
 *	this mesh peer
 * @FC80211_PLINK_OPN_RCVD: mesh plink open frame has been received
 *	from this mesh peer
 * @FC80211_PLINK_CNF_RCVD: mesh plink confirm frame has been
 *	received from this mesh peer
 * @FC80211_PLINK_ESTAB: mesh peer link is established
 * @FC80211_PLINK_HOLDING: mesh peer link is being closed or cancelled
 * @FC80211_PLINK_BLOCKED: all frames transmitted from this mesh
 *	plink are discarded
 * @NUM_FC80211_PLINK_STATES: number of peer link states
 * @MAX_FC80211_PLINK_STATES: highest numerical value of plink states
 */
enum fc80211_plink_state {
	FC80211_PLINK_LISTEN,
	FC80211_PLINK_OPN_SNT,
	FC80211_PLINK_OPN_RCVD,
	FC80211_PLINK_CNF_RCVD,
	FC80211_PLINK_ESTAB,
	FC80211_PLINK_HOLDING,
	FC80211_PLINK_BLOCKED,

	/* keep last */
	NUM_FC80211_PLINK_STATES,
	MAX_FC80211_PLINK_STATES = NUM_FC80211_PLINK_STATES - 1
};

/**
 * enum fc80211_plink_action - actions to perform in mesh peers
 *
 * @FC80211_PLINK_ACTION_NO_ACTION: perform no action
 * @FC80211_PLINK_ACTION_OPEN: start mesh peer link establishment
 * @FC80211_PLINK_ACTION_BLOCK: block traffic from this mesh peer
 * @NUM_FC80211_PLINK_ACTIONS: number of possible actions
 */
enum fc80211_plink_actions {
	FC80211_PLINK_ACTION_NO_ACTION,
	FC80211_PLINK_ACTION_OPEN,
	FC80211_PLINK_ACTION_BLOCK,

	NUM_FC80211_PLINK_ACTIONS,
};


#define FC80211_KCK_LEN			16
#define FC80211_KEK_LEN			16
#define FC80211_REPLAY_CTR_LEN		8

/**
 * enum fc80211_rekey_data - attributes for GTK rekey offload
 * @__FC80211_REKEY_DATA_INVALID: invalid number for nested attributes
 * @FC80211_REKEY_DATA_KEK: key encryption key (binary)
 * @FC80211_REKEY_DATA_KCK: key confirmation key (binary)
 * @FC80211_REKEY_DATA_REPLAY_CTR: replay counter (binary)
 * @NUM_FC80211_REKEY_DATA: number of rekey attributes (internal)
 * @MAX_FC80211_REKEY_DATA: highest rekey attribute (internal)
 */
enum fc80211_rekey_data {
	__FC80211_REKEY_DATA_INVALID,
	FC80211_REKEY_DATA_KEK,
	FC80211_REKEY_DATA_KCK,
	FC80211_REKEY_DATA_REPLAY_CTR,

	/* keep last */
	NUM_FC80211_REKEY_DATA,
	MAX_FC80211_REKEY_DATA = NUM_FC80211_REKEY_DATA - 1
};

/**
 * enum fc80211_hidden_ssid - values for %FC80211_ATTR_HIDDEN_SSID
 * @FC80211_HIDDEN_SSID_NOT_IN_USE: do not hide SSID (i.e., broadcast it in
 *	Beacon frames)
 * @FC80211_HIDDEN_SSID_ZERO_LEN: hide SSID by using zero-length SSID element
 *	in Beacon frames
 * @FC80211_HIDDEN_SSID_ZERO_CONTENTS: hide SSID by using correct length of SSID
 *	element in Beacon frames but zero out each byte in the SSID
 */
enum fc80211_hidden_ssid {
	FC80211_HIDDEN_SSID_NOT_IN_USE,
	FC80211_HIDDEN_SSID_ZERO_LEN,
	FC80211_HIDDEN_SSID_ZERO_CONTENTS
};

/**
 * enum fc80211_sta_wme_attr - station WME attributes
 * @__FC80211_STA_WME_INVALID: invalid number for nested attribute
 * @FC80211_STA_WME_UAPSD_QUEUES: bitmap of uapsd queues. the format
 *	is the same as the AC bitmap in the QoS info field.
 * @FC80211_STA_WME_MAX_SP: max service period. the format is the same
 *	as the MAX_SP field in the QoS info field (but already shifted down).
 * @__FC80211_STA_WME_AFTER_LAST: internal
 * @FC80211_STA_WME_MAX: highest station WME attribute
 */
enum fc80211_sta_wme_attr {
	__FC80211_STA_WME_INVALID,
	FC80211_STA_WME_UAPSD_QUEUES,
	FC80211_STA_WME_MAX_SP,

	/* keep last */
	__FC80211_STA_WME_AFTER_LAST,
	FC80211_STA_WME_MAX = __FC80211_STA_WME_AFTER_LAST - 1
};

/**
 * enum fc80211_pmksa_candidate_attr - attributes for PMKSA caching candidates
 * @__FC80211_PMKSA_CANDIDATE_INVALID: invalid number for nested attributes
 * @FC80211_PMKSA_CANDIDATE_INDEX: candidate index (u32; the smaller, the higher
 *	priority)
 * @FC80211_PMKSA_CANDIDATE_BSSID: candidate BSSID (6 octets)
 * @FC80211_PMKSA_CANDIDATE_PREAUTH: RSN pre-authentication supported (flag)
 * @NUM_FC80211_PMKSA_CANDIDATE: number of PMKSA caching candidate attributes
 *	(internal)
 * @MAX_FC80211_PMKSA_CANDIDATE: highest PMKSA caching candidate attribute
 *	(internal)
 */
enum fc80211_pmksa_candidate_attr {
	__FC80211_PMKSA_CANDIDATE_INVALID,
	FC80211_PMKSA_CANDIDATE_INDEX,
	FC80211_PMKSA_CANDIDATE_BSSID,
	FC80211_PMKSA_CANDIDATE_PREAUTH,

	/* keep last */
	NUM_FC80211_PMKSA_CANDIDATE,
	MAX_FC80211_PMKSA_CANDIDATE = NUM_FC80211_PMKSA_CANDIDATE - 1
};

/**
 * enum fc80211_tdls_operation - values for %FC80211_ATTR_TDLS_OPERATION
 * @FC80211_TDLS_DISCOVERY_REQ: Send a TDLS discovery request
 * @FC80211_TDLS_SETUP: Setup TDLS link
 * @FC80211_TDLS_TEARDOWN: Teardown a TDLS link which is already established
 * @FC80211_TDLS_ENABLE_LINK: Enable TDLS link
 * @FC80211_TDLS_DISABLE_LINK: Disable TDLS link
 */
enum fc80211_tdls_operation {
	FC80211_TDLS_DISCOVERY_REQ,
	FC80211_TDLS_SETUP,
	FC80211_TDLS_TEARDOWN,
	FC80211_TDLS_ENABLE_LINK,
	FC80211_TDLS_DISABLE_LINK,
};

/*
 * enum fc80211_ap_sme_features - device-integrated AP features
 * Reserved for future use, no bits are defined in
 * FC80211_ATTR_DEVICE_AP_SME yet.
enum fc80211_ap_sme_features {
};
 */

/**
 * enum fc80211_feature_flags - device/driver features
 * @FC80211_FEATURE_SK_TX_STATUS: This driver supports reflecting back
 *	TX status to the socket error queue when requested with the
 *	socket option.
 * @FC80211_FEATURE_HT_IBSS: This driver supports IBSS with HT datarates.
 * @FC80211_FEATURE_INACTIVITY_TIMER: This driver takes care of freeing up
 *	the connected inactive stations in AP mode.
 * @FC80211_FEATURE_CELL_BASE_REG_HINTS: This driver has been tested
 *	to work properly to suppport receiving regulatory hints from
 *	cellular base stations.
 * @FC80211_FEATURE_SAE: This driver supports simultaneous authentication of
 *	equals (SAE) with user space SME (FC80211_CMD_AUTHENTICATE) in station
 *	mode
 * @FC80211_FEATURE_LOW_PRIORITY_SCAN: This driver supports low priority scan
 * @FC80211_FEATURE_SCAN_FLUSH: Scan flush is supported
 * @FC80211_FEATURE_AP_SCAN: Support scanning using an AP vif
 * @FC80211_FEATURE_VIF_TXPOWER: The driver supports per-vif TX power setting
 * @FC80211_FEATURE_NEED_OBSS_SCAN: The driver expects userspace to perform
 *	OBSS scans and generate 20/40 BSS coex reports. This flag is used only
 *	for drivers implementing the CONNECT API, for AUTH/ASSOC it is implied.
 * @FC80211_FEATURE_P2P_GO_CTWIN: P2P GO implementation supports CT Window
 *	setting
 * @FC80211_FEATURE_P2P_GO_OPPPS: P2P GO implementation supports opportunistic
 *	powersave
 * @FC80211_FEATURE_FULL_AP_CLIENT_STATE: The driver supports full state
 *	transitions for AP clients. Without this flag (and if the driver
 *	doesn't have the AP SME in the device) the driver supports adding
 *	stations only when they're associated and adds them in associated
 *	state (to later be transitioned into authorized), with this flag
 *	they should be added before even sending the authentication reply
 *	and then transitioned into authenticated, associated and authorized
 *	states using station flags.
 *	Note that even for drivers that support this, the default is to add
 *	stations in authenticated/associated state, so to add unauthenticated
 *	stations the authenticated/associated bits have to be set in the mask.
 * @FC80211_FEATURE_ADVERTISE_CHAN_LIMITS: cfgd11 advertises channel limits
 *	(HT40, VHT 80/160 MHz) if this flag is set
 * @FC80211_FEATURE_USERSPACE_MPM: This driver supports a userspace Mesh
 *	Peering Management entity which may be implemented by registering for
 *	beacons or FC80211_CMD_NEW_PEER_CANDIDATE events. The mesh beacon is
 *	still generated by the driver.
 * @FC80211_FEATURE_ACTIVE_MONITOR: This driver supports an active monitor
 *	interface. An active monitor interface behaves like a normal monitor
 *	interface, but gets added to the driver. It ensures that incoming
 *	unicast packets directed at the configured interface address get ACKed.
 * @FC80211_FEATURE_AP_MODE_CHAN_WIDTH_CHANGE: This driver supports dynamic
 *	channel bandwidth change (e.g., HT 20 <-> 40 MHz channel) during the
 *	lifetime of a BSS.
 */
enum fc80211_feature_flags {
	FC80211_FEATURE_SK_TX_STATUS			= 1 << 0,
	FC80211_FEATURE_HT_IBSS				= 1 << 1,
	FC80211_FEATURE_INACTIVITY_TIMER		= 1 << 2,
	FC80211_FEATURE_CELL_BASE_REG_HINTS		= 1 << 3,
	/* bit 4 is reserved - don't use */
	FC80211_FEATURE_SAE				= 1 << 5,
	FC80211_FEATURE_LOW_PRIORITY_SCAN		= 1 << 6,
	FC80211_FEATURE_SCAN_FLUSH			= 1 << 7,
	FC80211_FEATURE_AP_SCAN				= 1 << 8,
	FC80211_FEATURE_VIF_TXPOWER			= 1 << 9,
	FC80211_FEATURE_NEED_OBSS_SCAN			= 1 << 10,
	FC80211_FEATURE_P2P_GO_CTWIN			= 1 << 11,
	FC80211_FEATURE_P2P_GO_OPPPS			= 1 << 12,
	/* bit 13 is reserved */
	FC80211_FEATURE_ADVERTISE_CHAN_LIMITS		= 1 << 14,
	FC80211_FEATURE_FULL_AP_CLIENT_STATE		= 1 << 15,
	FC80211_FEATURE_USERSPACE_MPM			= 1 << 16,
	FC80211_FEATURE_ACTIVE_MONITOR			= 1 << 17,
	FC80211_FEATURE_AP_MODE_CHAN_WIDTH_CHANGE	= 1 << 18,
	FC80211_FEATURE_TDLS_CHANNEL_SWITCH		= 1 << 28,
};

/**
 * enum fc80211_ext_feature_index - bit index of extended features.
 * @FC80211_EXT_FEATURE_VHT_IBSS: This driver supports IBSS with VHT datarates.
 * @FC80211_EXT_FEATURE_RRM: This driver supports RRM. When featured, user can
 *	request to use RRM (see %FC80211_ATTR_USE_RRM) with
 *	%FC80211_CMD_ASSOCIATE and %FC80211_CMD_CONNECT requests, which will set
 *	the ASSOC_REQ_USE_RRM flag in the association request even if
 *	FC80211_FEATURE_QUIET is not advertized.
 * @FC80211_EXT_FEATURE_MU_MIMO_AIR_SNIFFER: This device supports MU-MIMO air
 *	sniffer which means that it can be configured to hear packets from
 *	certain groups which can be configured by the
 *	%FC80211_ATTR_MU_MIMO_GROUP_DATA attribute,
 *	or can be configured to follow a station by configuring the
 *	%FC80211_ATTR_MU_MIMO_FOLLOW_MAC_ADDR attribute.
 * @FC80211_EXT_FEATURE_SCAN_START_TIME: This driver includes the actual
 *	time the scan started in scan results event. The time is the TSF of
 *	the BSS that the interface that requested the scan is connected to
 *	(if available).
 * @FC80211_EXT_FEATURE_BSS_PARENT_TSF: Per BSS, this driver reports the
 *	time the last beacon/probe was received. The time is the TSF of the
 *	BSS that the interface that requested the scan is connected to
 *	(if available).
 * @FC80211_EXT_FEATURE_SET_SCAN_DWELL: This driver supports configuration of
 *	channel dwell time.
 * @FC80211_EXT_FEATURE_BEACON_RATE_LEGACY: Driver supports beacon rate
 *	configuration (AP/mesh), supporting a legacy (non HT/VHT) rate.
 * @FC80211_EXT_FEATURE_BEACON_RATE_HT: Driver supports beacon rate
 *	configuration (AP/mesh) with HT rates.
 * @FC80211_EXT_FEATURE_BEACON_RATE_VHT: Driver supports beacon rate
 *	configuration (AP/mesh) with VHT rates.
 * @FC80211_EXT_FEATURE_FILS_STA: This driver supports Fast Initial Link Setup
 *	with user space SME (FC80211_CMD_AUTHENTICATE) in station mode.
 * @FC80211_EXT_FEATURE_MGMT_TX_RANDOM_TA: This driver supports randomized TA
 *	in @FC80211_CMD_FRAME while not associated.
 * @FC80211_EXT_FEATURE_MGMT_TX_RANDOM_TA_CONNECTED: This driver supports
 *	randomized TA in @FC80211_CMD_FRAME while associated.
 * @FC80211_EXT_FEATURE_SCHED_SCAN_RELATIVE_RSSI: The driver supports sched_scan
 *	for reporting BSSs with better RSSI than the current connected BSS
 *	(%FC80211_ATTR_SCHED_SCAN_RELATIVE_RSSI).
 * @FC80211_EXT_FEATURE_CQM_RSSI_LIST: With this driver the
 *	%FC80211_ATTR_CQM_RSSI_THOLD attribute accepts a list of zero or more
 *	RSSI threshold values to monitor rather than exactly one threshold.
 * @FC80211_EXT_FEATURE_FILS_SK_OFFLOAD: Driver SME supports FILS shared key
 *	authentication with %FC80211_CMD_CONNECT.
 * @FC80211_EXT_FEATURE_4WAY_HANDSHAKE_STA_PSK: Device wants to do 4-way
 *	handshake with PSK in station mode (PSK is passed as part of the connect
 *	and associate commands), doing it in the host might not be supported.
 * @FC80211_EXT_FEATURE_4WAY_HANDSHAKE_STA_1X: Device wants to do doing 4-way
 *	handshake with 802.1X in station mode (will pass EAP frames to the host
 *	and accept the set_pmk/del_pmk commands), doing it in the host might not
 *	be supported.
 * @FC80211_EXT_FEATURE_FILS_MAX_CHANNEL_TIME: Driver is capable of overriding
 *	the max channel attribute in the FILS request params IE with the
 *	actual dwell time.
 * @FC80211_EXT_FEATURE_ACCEPT_BCAST_PROBE_RESP: Driver accepts broadcast probe
 *	response
 * @FC80211_EXT_FEATURE_OCE_PROBE_REQ_HIGH_TX_RATE: Driver supports sending
 *	the first probe request in each channel at rate of at least 5.5Mbps.
 * @FC80211_EXT_FEATURE_OCE_PROBE_REQ_DEFERRAL_SUPPRESSION: Driver supports
 *	probe request tx deferral and suppression
 * @FC80211_EXT_FEATURE_MFP_OPTIONAL: Driver supports the %FC80211_MFP_OPTIONAL
 *	value in %FC80211_ATTR_USE_MFP.
 * @FC80211_EXT_FEATURE_LOW_SPAN_SCAN: Driver supports low span scan.
 * @FC80211_EXT_FEATURE_LOW_POWER_SCAN: Driver supports low power scan.
 * @FC80211_EXT_FEATURE_HIGH_ACCURACY_SCAN: Driver supports high accuracy scan.
 * @FC80211_EXT_FEATURE_DFS_OFFLOAD: HW/driver will offload DFS actions.
 *	Device or driver will do all DFS-related actions by itself,
 *	informing user-space about CAC progress, radar detection event,
 *	channel change triggered by radar detection event.
 *	No need to start CAC from user-space, no need to react to
 *	"radar detected" event.
 * @FC80211_EXT_FEATURE_CONTROL_PORT_OVER_FC80211: Driver supports sending and
 *	receiving control port frames over nl80211 instead of the netdevice.
 * @FC80211_EXT_FEATURE_ACK_SIGNAL_SUPPORT: This driver/device supports
 *	(average) ACK signal strength reporting.
 * @FC80211_EXT_FEATURE_TXQS: Driver supports FQ-CoDel-enabled intermediate
 *      TXQs.
 * @FC80211_EXT_FEATURE_SCAN_RANDOM_SN: Driver/device supports randomizing the
 *	SN in probe request frames if requested by %FC80211_SCAN_FLAG_RANDOM_SN.
 * @FC80211_EXT_FEATURE_SCAN_MIN_PREQ_CONTENT: Driver/device can omit all data
 *	except for supported rates from the probe request content if requested
 *	by the %FC80211_SCAN_FLAG_MIN_PREQ_CONTENT flag.
 * @FC80211_EXT_FEATURE_ENABLE_FTM_RESPONDER: Driver supports enabling fine
 *	timing measurement responder role.
 *
 * @FC80211_EXT_FEATURE_CAN_REPLACE_PTK0: Driver/device confirm that they are
 *      able to rekey an in-use key correctly. Userspace must not rekey PTK keys
 *      if this flag is not set. Ignoring this can leak clear text packets and/or
 *      freeze the connection.
 * @FC80211_EXT_FEATURE_EXT_KEY_ID: Driver supports "Extended Key ID for
 *      Individually Addressed Frames" from IEEE802.11-2016.
 *
 * @FC80211_EXT_FEATURE_AIRTIME_FAIRNESS: Driver supports getting airtime
 *	fairness for transmitted packets and has enabled airtime fairness
 *	scheduling.
 *
 * @FC80211_EXT_FEATURE_AP_PMKSA_CACHING: Driver/device supports PMKSA caching
 *	(set/del PMKSA operations) in AP mode.
 *
 * @FC80211_EXT_FEATURE_SCHED_SCAN_BAND_SPECIFIC_RSSI_THOLD: Driver supports
 *	filtering of sched scan results using band specific RSSI thresholds.
 *
 * @FC80211_EXT_FEATURE_STA_TX_PWR: This driver supports controlling tx power
 *	to a station.
 *
 * @FC80211_EXT_FEATURE_SAE_OFFLOAD: Device wants to do SAE authentication in
 *	station mode (SAE password is passed as part of the connect command).
 *
 * @FC80211_EXT_FEATURE_VLAN_OFFLOAD: The driver supports a single netdev
 *	with VLAN tagged frames and separate VLAN-specific netdevs added using
 *	vconfig similarly to the Ethernet case.
 *
 * @FC80211_EXT_FEATURE_AQL: The driver supports the Airtime Queue Limit (AQL)
 *	feature, which prevents bufferbloat by using the expected transmission
 *	time to limit the amount of data buffered in the hardware.
 *
 * @FC80211_EXT_FEATURE_BEACON_PROTECTION: The driver supports Beacon protection
 *	and can receive key configuration for BIGTK using key indexes 6 and 7.
 * @FC80211_EXT_FEATURE_BEACON_PROTECTION_CLIENT: The driver supports Beacon
 *	protection as a client only and cannot transmit protected beacons.
 *
 * @FC80211_EXT_FEATURE_CONTROL_PORT_NO_PREAUTH: The driver can disable the
 *	forwarding of preauth frames over the control port. They are then
 *	handled as ordinary data frames.
 *
 * @FC80211_EXT_FEATURE_PROTECTED_TWT: Driver supports protected TWT frames
 *
 * @FC80211_EXT_FEATURE_DEL_IBSS_STA: The driver supports removing stations
 *      in IBSS mode, essentially by dropping their state.
 *
 * @FC80211_EXT_FEATURE_MULTICAST_REGISTRATIONS: management frame registrations
 *	are possible for multicast frames and those will be reported properly.
 *
 * @FC80211_EXT_FEATURE_SCAN_FREQ_KHZ: This driver supports receiving and
 *	reporting scan request with %FC80211_ATTR_SCAN_FREQ_KHZ. In order to
 *	report %FC80211_ATTR_SCAN_FREQ_KHZ, %FC80211_SCAN_FLAG_FREQ_KHZ must be
 *	included in the scan request.
 *
 * @FC80211_EXT_FEATURE_CONTROL_PORT_OVER_FC80211_TX_STATUS: The driver
 *	can report tx status for control port over nl80211 tx operations.
 *
 * @FC80211_EXT_FEATURE_OPERATING_CHANNEL_VALIDATION: Driver supports Operating
 *	Channel Validation (OCV) when using driver's SME for RSNA handshakes.
 *
 * @FC80211_EXT_FEATURE_4WAY_HANDSHAKE_AP_PSK: Device wants to do 4-way
 *	handshake with PSK in AP mode (PSK is passed as part of the start AP
 *	command).
 *
 * @FC80211_EXT_FEATURE_SAE_OFFLOAD_AP: Device wants to do SAE authentication
 *	in AP mode (SAE password is passed as part of the start AP command).
 *
 * @FC80211_EXT_FEATURE_FILS_DISCOVERY: Driver/device supports FILS discovery
 *	frames transmission
 *
 * @FC80211_EXT_FEATURE_UNSOL_BCAST_PROBE_RESP: Driver/device supports
 *	unsolicited broadcast probe response transmission
 *
 * @FC80211_EXT_FEATURE_BEACON_RATE_HE: Driver supports beacon rate
 *	configuration (AP/mesh) with HE rates.
 *
 * @FC80211_EXT_FEATURE_SECURE_LTF: Device supports secure LTF measurement
 *      exchange protocol.
 *
 * @FC80211_EXT_FEATURE_SECURE_RTT: Device supports secure RTT measurement
 *      exchange protocol.
 *
 * @FC80211_EXT_FEATURE_PROT_RANGE_NEGO_AND_MEASURE: Device supports management
 *      frame protection for all management frames exchanged during the
 *      negotiation and range measurement procedure.
 *
 * @FC80211_EXT_FEATURE_BSS_COLOR: The driver supports BSS color collision
 *	detection and change announcemnts.
 *
 * @FC80211_EXT_FEATURE_FILS_CRYPTO_OFFLOAD: Driver running in AP mode supports
 *	FILS encryption and decryption for (Re)Association Request and Response
 *	frames. Userspace has to share FILS AAD details to the driver by using
 *	@FC80211_CMD_SET_FILS_AAD.
 *
 * @NUM_FC80211_EXT_FEATURES: number of extended features.
 * @MAX_FC80211_EXT_FEATURES: highest extended feature index.
 */
enum fc80211_ext_feature_index {
	FC80211_EXT_FEATURE_VHT_IBSS,
	FC80211_EXT_FEATURE_RRM,
	FC80211_EXT_FEATURE_MU_MIMO_AIR_SNIFFER,
	FC80211_EXT_FEATURE_SCAN_START_TIME,
	FC80211_EXT_FEATURE_BSS_PARENT_TSF,
	FC80211_EXT_FEATURE_SET_SCAN_DWELL,
	FC80211_EXT_FEATURE_BEACON_RATE_LEGACY,
	FC80211_EXT_FEATURE_BEACON_RATE_HT,
	FC80211_EXT_FEATURE_BEACON_RATE_VHT,
	FC80211_EXT_FEATURE_FILS_STA,
	FC80211_EXT_FEATURE_MGMT_TX_RANDOM_TA,
	FC80211_EXT_FEATURE_MGMT_TX_RANDOM_TA_CONNECTED,
	FC80211_EXT_FEATURE_SCHED_SCAN_RELATIVE_RSSI,
	FC80211_EXT_FEATURE_CQM_RSSI_LIST,
	FC80211_EXT_FEATURE_FILS_SK_OFFLOAD,
	FC80211_EXT_FEATURE_4WAY_HANDSHAKE_STA_PSK,
	FC80211_EXT_FEATURE_4WAY_HANDSHAKE_STA_1X,
	FC80211_EXT_FEATURE_FILS_MAX_CHANNEL_TIME,
	FC80211_EXT_FEATURE_ACCEPT_BCAST_PROBE_RESP,
	FC80211_EXT_FEATURE_OCE_PROBE_REQ_HIGH_TX_RATE,
	FC80211_EXT_FEATURE_OCE_PROBE_REQ_DEFERRAL_SUPPRESSION,
	FC80211_EXT_FEATURE_MFP_OPTIONAL,
	FC80211_EXT_FEATURE_LOW_SPAN_SCAN,
	FC80211_EXT_FEATURE_LOW_POWER_SCAN,
	FC80211_EXT_FEATURE_HIGH_ACCURACY_SCAN,
	FC80211_EXT_FEATURE_DFS_OFFLOAD,
	FC80211_EXT_FEATURE_CONTROL_PORT_OVER_FC80211,
	FC80211_EXT_FEATURE_ACK_SIGNAL_SUPPORT,
	/* we renamed this - stay compatible */
	FC80211_EXT_FEATURE_DATA_ACK_SIGNAL_SUPPORT = FC80211_EXT_FEATURE_ACK_SIGNAL_SUPPORT,
	FC80211_EXT_FEATURE_TXQS,
	FC80211_EXT_FEATURE_SCAN_RANDOM_SN,
	FC80211_EXT_FEATURE_SCAN_MIN_PREQ_CONTENT,
	FC80211_EXT_FEATURE_CAN_REPLACE_PTK0,
	FC80211_EXT_FEATURE_ENABLE_FTM_RESPONDER,
	FC80211_EXT_FEATURE_AIRTIME_FAIRNESS,
	FC80211_EXT_FEATURE_AP_PMKSA_CACHING,
	FC80211_EXT_FEATURE_SCHED_SCAN_BAND_SPECIFIC_RSSI_THOLD,
	FC80211_EXT_FEATURE_EXT_KEY_ID,
	FC80211_EXT_FEATURE_STA_TX_PWR,
	FC80211_EXT_FEATURE_SAE_OFFLOAD,
	FC80211_EXT_FEATURE_VLAN_OFFLOAD,
	FC80211_EXT_FEATURE_AQL,
	FC80211_EXT_FEATURE_BEACON_PROTECTION,
	FC80211_EXT_FEATURE_CONTROL_PORT_NO_PREAUTH,
	FC80211_EXT_FEATURE_PROTECTED_TWT,
	FC80211_EXT_FEATURE_DEL_IBSS_STA,
	FC80211_EXT_FEATURE_MULTICAST_REGISTRATIONS,
	FC80211_EXT_FEATURE_BEACON_PROTECTION_CLIENT,
	FC80211_EXT_FEATURE_SCAN_FREQ_KHZ,
	FC80211_EXT_FEATURE_CONTROL_PORT_OVER_FC80211_TX_STATUS,
	FC80211_EXT_FEATURE_OPERATING_CHANNEL_VALIDATION,
	FC80211_EXT_FEATURE_4WAY_HANDSHAKE_AP_PSK,
	FC80211_EXT_FEATURE_SAE_OFFLOAD_AP,
	FC80211_EXT_FEATURE_FILS_DISCOVERY,
	FC80211_EXT_FEATURE_UNSOL_BCAST_PROBE_RESP,
	FC80211_EXT_FEATURE_BEACON_RATE_HE,
	FC80211_EXT_FEATURE_SECURE_LTF,
	FC80211_EXT_FEATURE_SECURE_RTT,
	FC80211_EXT_FEATURE_PROT_RANGE_NEGO_AND_MEASURE,
	FC80211_EXT_FEATURE_BSS_COLOR,
	FC80211_EXT_FEATURE_FILS_CRYPTO_OFFLOAD,

	/* add new features before the definition below */
	NUM_FC80211_EXT_FEATURES,
	MAX_FC80211_EXT_FEATURES = NUM_FC80211_EXT_FEATURES - 1
};

/**
 * enum fc80211_probe_resp_offload_support_attr - optional supported
 *	protocols for probe-response offloading by the driver/FW.
 *	To be used with the %FC80211_ATTR_PROBE_RESP_OFFLOAD attribute.
 *	Each enum value represents a bit in the bitmap of supported
 *	protocols. Typically a subset of probe-requests belonging to a
 *	supported protocol will be excluded from offload and uploaded
 *	to the host.
 *
 * @FC80211_PROBE_RESP_OFFLOAD_SUPPORT_WPS: Support for WPS ver. 1
 * @FC80211_PROBE_RESP_OFFLOAD_SUPPORT_WPS2: Support for WPS ver. 2
 * @FC80211_PROBE_RESP_OFFLOAD_SUPPORT_P2P: Support for P2P
 * @FC80211_PROBE_RESP_OFFLOAD_SUPPORT_80211U: Support for 802.11u
 */
enum fc80211_probe_resp_offload_support_attr {
	FC80211_PROBE_RESP_OFFLOAD_SUPPORT_WPS =	1<<0,
	FC80211_PROBE_RESP_OFFLOAD_SUPPORT_WPS2 =	1<<1,
	FC80211_PROBE_RESP_OFFLOAD_SUPPORT_P2P =	1<<2,
	FC80211_PROBE_RESP_OFFLOAD_SUPPORT_80211U =	1<<3,
};

/**
 * enum fc80211_connect_failed_reason - connection request failed reasons
 * @FC80211_CONN_FAIL_MAX_CLIENTS: Maximum number of clients that can be
 *	handled by the AP is reached.
 * @FC80211_CONN_FAIL_BLOCKED_CLIENT: Connection request is rejected due to ACL.
 */
enum fc80211_connect_failed_reason {
	FC80211_CONN_FAIL_MAX_CLIENTS,
	FC80211_CONN_FAIL_BLOCKED_CLIENT,
};

/**
 * enum fc80211_timeout_reason - timeout reasons
 *
 * @FC80211_TIMEOUT_UNSPECIFIED: Timeout reason unspecified.
 * @FC80211_TIMEOUT_SCAN: Scan (AP discovery) timed out.
 * @FC80211_TIMEOUT_AUTH: Authentication timed out.
 * @FC80211_TIMEOUT_ASSOC: Association timed out.
 */
enum fc80211_timeout_reason {
	FC80211_TIMEOUT_UNSPECIFIED,
	FC80211_TIMEOUT_SCAN,
	FC80211_TIMEOUT_AUTH,
	FC80211_TIMEOUT_ASSOC,
};

/**
 * enum fc80211_scan_flags -  scan request control flags
 *
 * Scan request control flags are used to control the handling
 * of FC80211_CMD_TRIGGER_SCAN and FC80211_CMD_START_SCHED_SCAN
 * requests.
 *
 * @FC80211_SCAN_FLAG_LOW_PRIORITY: scan request has low priority
 * @FC80211_SCAN_FLAG_FLUSH: flush cache before scanning
 * @FC80211_SCAN_FLAG_AP: force a scan even if the interface is configured
 *	as AP and the beaconing has already been configured. This attribute is
 *	dangerous because will destroy stations performance as a lot of frames
 *	will be lost while scanning off-channel, therefore it must be used only
 *	when really needed
 */
enum fc80211_scan_flags {
	FC80211_SCAN_FLAG_LOW_PRIORITY			= 1<<0,
	FC80211_SCAN_FLAG_FLUSH				= 1<<1,
	FC80211_SCAN_FLAG_AP				= 1<<2,
	FC80211_SCAN_FLAG_RANDOM_ADDR				= 1<<3,
	FC80211_SCAN_FLAG_FILS_MAX_CHANNEL_TIME			= 1<<4,
	FC80211_SCAN_FLAG_ACCEPT_BCAST_PROBE_RESP		= 1<<5,
	FC80211_SCAN_FLAG_OCE_PROBE_REQ_HIGH_TX_RATE		= 1<<6,
	FC80211_SCAN_FLAG_OCE_PROBE_REQ_DEFERRAL_SUPPRESSION	= 1<<7,
};

/**
 * enum fc80211_acl_policy - access control policy
 *
 * Access control policy is applied on a MAC list set by
 * %FC80211_CMD_START_AP and %FC80211_CMD_SET_MAC_ACL, to
 * be used with %FC80211_ATTR_ACL_POLICY.
 *
 * @FC80211_ACL_POLICY_ACCEPT_UNLESS_LISTED: Deny stations which are
 *	listed in ACL, i.e. allow all the stations which are not listed
 *	in ACL to authenticate.
 * @FC80211_ACL_POLICY_DENY_UNLESS_LISTED: Allow the stations which are listed
 *	in ACL, i.e. deny all the stations which are not listed in ACL.
 */
enum fc80211_acl_policy {
	FC80211_ACL_POLICY_ACCEPT_UNLESS_LISTED,
	FC80211_ACL_POLICY_DENY_UNLESS_LISTED,
};

/**
 * enum fc80211_smps_mode - SMPS mode
 *
 * Requested SMPS mode (for AP mode)
 *
 * @FC80211_SMPS_OFF: SMPS off (use all antennas).
 * @FC80211_SMPS_STATIC: static SMPS (use a single antenna)
 * @FC80211_SMPS_DYNAMIC: dynamic smps (start with a single antenna and
 *	turn on other antennas after CTS/RTS).
 */
enum fc80211_smps_mode {
	FC80211_SMPS_OFF,
	FC80211_SMPS_STATIC,
	FC80211_SMPS_DYNAMIC,

	__FC80211_SMPS_AFTER_LAST,
	FC80211_SMPS_MAX = __FC80211_SMPS_AFTER_LAST - 1
};

/**
 * enum fc80211_radar_event - type of radar event for DFS operation
 *
 * Type of event to be used with FC80211_ATTR_RADAR_EVENT to inform userspace
 * about detected radars or success of the channel available check (CAC)
 *
 * @FC80211_RADAR_DETECTED: A radar pattern has been detected. The channel is
 *	now unusable.
 * @FC80211_RADAR_CAC_FINISHED: Channel Availability Check has been finished,
 *	the channel is now available.
 * @FC80211_RADAR_CAC_ABORTED: Channel Availability Check has been aborted, no
 *	change to the channel status.
 * @FC80211_RADAR_NOP_FINISHED: The Non-Occupancy Period for this channel is
 *	over, channel becomes usable.
 * @FC80211_RADAR_PRE_CAC_EXPIRED: Channel Availability Check done on this
 *	non-operating channel is expired and no longer valid. New CAC must
 *	be done on this channel before starting the operation. This is not
 *	applicable for ETSI dfs domain where pre-CAC is valid for ever.
 * @FC80211_RADAR_CAC_STARTED: Channel Availability Check has been started,
 *	should be generated by HW if FC80211_EXT_FEATURE_DFS_OFFLOAD is enabled.
 */
enum fc80211_radar_event {
	FC80211_RADAR_DETECTED,
	FC80211_RADAR_CAC_FINISHED,
	FC80211_RADAR_CAC_ABORTED,
	FC80211_RADAR_NOP_FINISHED,
	FC80211_RADAR_PRE_CAC_EXPIRED,
	FC80211_RADAR_CAC_STARTED,
};

/**
 * enum fc80211_dfs_state - DFS states for channels
 *
 * Channel states used by the DFS code.
 *
 * @FC80211_DFS_USABLE: The channel can be used, but channel availability
 *	check (CAC) must be performed before using it for AP or IBSS.
 * @FC80211_DFS_UNAVAILABLE: A radar has been detected on this channel, it
 *	is therefore marked as not available.
 * @FC80211_DFS_AVAILABLE: The channel has been CAC checked and is available.
 */
enum fc80211_dfs_state {
	FC80211_DFS_USABLE,
	FC80211_DFS_UNAVAILABLE,
	FC80211_DFS_AVAILABLE,
};

/**
 * enum enum fc80211_protocol_features - fc80211 protocol features
 * @FC80211_PROTOCOL_FEATURE_SPLIT_SOFTMAC_DUMP: fc80211 supports splitting
 *	softmac dumps (if requested by the application with the attribute
 *	%FC80211_ATTR_SPLIT_SOFTMAC_DUMP. Also supported is filtering the
 *	softmac dump by %FC80211_ATTR_SOFTMAC, %FC80211_ATTR_IFINDEX or
 *	%FC80211_ATTR_WDEV.
 */
enum fc80211_protocol_features {
	FC80211_PROTOCOL_FEATURE_SPLIT_SOFTMAC_DUMP =	1 << 0,
};

/**
 * enum fc80211_crit_proto_id - fc80211 critical protocol identifiers
 *
 * @FC80211_CRIT_PROTO_UNSPEC: protocol unspecified.
 * @FC80211_CRIT_PROTO_DHCP: BOOTP or DHCPv6 protocol.
 * @FC80211_CRIT_PROTO_EAPOL: EAPOL protocol.
 * @FC80211_CRIT_PROTO_APIPA: APIPA protocol.
 * @NUM_FC80211_CRIT_PROTO: must be kept last.
 */
enum fc80211_crit_proto_id {
	FC80211_CRIT_PROTO_UNSPEC,
	FC80211_CRIT_PROTO_DHCP,
	FC80211_CRIT_PROTO_EAPOL,
	FC80211_CRIT_PROTO_APIPA,
	/* add other protocols before this one */
	NUM_FC80211_CRIT_PROTO
};

/* maximum duration for critical protocol measures */
#define FC80211_CRIT_PROTO_MAX_DURATION		5000 /* msec */

/**
 * enum fc80211_rxmgmt_flags - flags for received management frame.
 *
 * Used by cfgd11_rx_mgmt()
 *
 * @FC80211_RXMGMT_FLAG_ANSWERED: frame was answered by device/driver.
 */
enum fc80211_rxmgmt_flags {
	FC80211_RXMGMT_FLAG_ANSWERED = 1 << 0,
};

/*
 * If this flag is unset, the lower 24 bits are an OUI, if set
 * a Linux fc80211 vendor ID is used (no such IDs are allocated
 * yet, so that's not valid so far)
 */
#define FC80211_VENDOR_ID_IS_LINUX	0x80000000

/**
 * struct fc80211_vendor_cmd_info - vendor command data
 * @vendor_id: If the %FC80211_VENDOR_ID_IS_LINUX flag is clear, then the
 *	value is a 24-bit OUI; if it is set then a separately allocated ID
 *	may be used, but no such IDs are allocated yet. New IDs should be
 *	added to this file when needed.
 * @subcmd: sub-command ID for the command
 */
struct fc80211_vendor_cmd_info {
	u32 vendor_id;
	u32 subcmd;
};

/**
 * enum fc80211_bss_select_attr - attributes for bss selection.
 *
 * @__FC80211_BSS_SELECT_ATTR_INVALID: reserved.
 * @FC80211_BSS_SELECT_ATTR_RSSI: Flag indicating only RSSI-based BSS selection
 *	is requested.
 * @FC80211_BSS_SELECT_ATTR_BAND_PREF: attribute indicating BSS
 *	selection should be done such that the specified band is preferred.
 *	When there are multiple BSS-es in the preferred band, the driver
 *	shall use RSSI-based BSS selection as a second step. The value of
 *	this attribute is according to &enum fc80211_band (u32).
 * @FC80211_BSS_SELECT_ATTR_RSSI_ADJUST: When present the RSSI level for
 *	BSS-es in the specified band is to be adjusted before doing
 *	RSSI-based BSS selection. The attribute value is a packed structure
 *	value as specified by &struct FC80211_BSS_select_rssi_adjust.
 * @FC80211_BSS_SELECT_ATTR_MAX: highest bss select attribute number.
 * @__FC80211_BSS_SELECT_ATTR_AFTER_LAST: internal use.
 *
 * One and only one of these attributes are found within %FC80211_ATTR_BSS_SELECT
 * for %FC80211_CMD_CONNECT. It specifies the required BSS selection behaviour
 * which the driver shall use.
 */
enum fc80211_bss_select_attr {
	__FC80211_BSS_SELECT_ATTR_INVALID,
	FC80211_BSS_SELECT_ATTR_RSSI,
	FC80211_BSS_SELECT_ATTR_BAND_PREF,
	FC80211_BSS_SELECT_ATTR_RSSI_ADJUST,

	/* keep last */
	__FC80211_BSS_SELECT_ATTR_AFTER_LAST,
	FC80211_BSS_SELECT_ATTR_MAX = __FC80211_BSS_SELECT_ATTR_AFTER_LAST - 1
};

/**
 * enum fc80211_nan_function_type - NAN function type
 *
 * Defines the function type of a NAN function
 *
 * @FC80211_NAN_FUNC_PUBLISH: function is publish
 * @FC80211_NAN_FUNC_SUBSCRIBE: function is subscribe
 * @FC80211_NAN_FUNC_FOLLOW_UP: function is follow-up
 */
enum fc80211_nan_function_type {
	FC80211_NAN_FUNC_PUBLISH,
	FC80211_NAN_FUNC_SUBSCRIBE,
	FC80211_NAN_FUNC_FOLLOW_UP,

	/* keep last */
	__FC80211_NAN_FUNC_TYPE_AFTER_LAST,
	FC80211_NAN_FUNC_MAX_TYPE = __FC80211_NAN_FUNC_TYPE_AFTER_LAST - 1,
};

#define FC80211_NAN_FUNC_SERVICE_ID_LEN 6

/**
 * fc80211_external_auth_action - Action to perform with external
 *     authentication request. Used by FC80211_ATTR_EXTERNAL_AUTH_ACTION.
 * @FC80211_EXTERNAL_AUTH_START: Start the authentication.
 * @FC80211_EXTERNAL_AUTH_ABORT: Abort the ongoing authentication.
 */
enum fc80211_external_auth_action {
	FC80211_EXTERNAL_AUTH_START,
	FC80211_EXTERNAL_AUTH_ABORT,
};

/**
 * enum fc80211_preamble - frame preamble types
 * @FC80211_PREAMBLE_LEGACY: legacy (HR/DSSS, OFDM, ERP PHY) preamble
 * @FC80211_PREAMBLE_HT: HT preamble
 * @FC80211_PREAMBLE_VHT: VHT preamble
 * @FC80211_PREAMBLE_DMG: DMG preamble
 * @FC80211_PREAMBLE_HE: HE preamble
 */
enum fc80211_preamble {
	FC80211_PREAMBLE_LEGACY,
	FC80211_PREAMBLE_HT,
	FC80211_PREAMBLE_VHT,
	FC80211_PREAMBLE_DMG,
	FC80211_PREAMBLE_HE,
};

/**
 * enum fc80211_peer_measurement_type - peer measurement types
 * @FC80211_PMSR_TYPE_INVALID: invalid/unused, needed as we use
 *	these numbers also for attributes
 *
 * @FC80211_PMSR_TYPE_FTM: flight time measurement
 *
 * @NUM_FC80211_PMSR_TYPES: internal
 * @FC80211_PMSR_TYPE_MAX: highest type number
 */
enum fc80211_peer_measurement_type {
	FC80211_PMSR_TYPE_INVALID,

	FC80211_PMSR_TYPE_FTM,

	NUM_FC80211_PMSR_TYPES,
	FC80211_PMSR_TYPE_MAX = NUM_FC80211_PMSR_TYPES - 1
};

/**
 * enum fc80211_peer_measurement_status - peer measurement status
 * @FC80211_PMSR_STATUS_SUCCESS: measurement completed successfully
 * @FC80211_PMSR_STATUS_REFUSED: measurement was locally refused
 * @FC80211_PMSR_STATUS_TIMEOUT: measurement timed out
 * @FC80211_PMSR_STATUS_FAILURE: measurement failed, a type-dependent
 *	reason may be available in the response data
 */
enum fc80211_peer_measurement_status {
	FC80211_PMSR_STATUS_SUCCESS,
	FC80211_PMSR_STATUS_REFUSED,
	FC80211_PMSR_STATUS_TIMEOUT,
	FC80211_PMSR_STATUS_FAILURE,
};

/**
 * enum fc80211_peer_measurement_ftm_failure_reasons - FTM failure reasons
 * @FC80211_PMSR_FTM_FAILURE_UNSPECIFIED: unspecified failure, not used
 * @FC80211_PMSR_FTM_FAILURE_NO_RESPONSE: no response from the FTM responder
 * @FC80211_PMSR_FTM_FAILURE_REJECTED: FTM responder rejected measurement
 * @FC80211_PMSR_FTM_FAILURE_WRONG_CHANNEL: we already know the peer is
 *	on a different channel, so can't measure (if we didn't know, we'd
 *	try and get no response)
 * @FC80211_PMSR_FTM_FAILURE_PEER_NOT_CAPABLE: peer can't actually do FTM
 * @FC80211_PMSR_FTM_FAILURE_INVALID_TIMESTAMP: invalid T1/T4 timestamps
 *	received
 * @FC80211_PMSR_FTM_FAILURE_PEER_BUSY: peer reports busy, you may retry
 *	later (see %FC80211_PMSR_FTM_RESP_ATTR_BUSY_RETRY_TIME)
 * @FC80211_PMSR_FTM_FAILURE_BAD_CHANGED_PARAMS: parameters were changed
 *	by the peer and are no longer supported
 */
enum fc80211_peer_measurement_ftm_failure_reasons {
	FC80211_PMSR_FTM_FAILURE_UNSPECIFIED,
	FC80211_PMSR_FTM_FAILURE_NO_RESPONSE,
	FC80211_PMSR_FTM_FAILURE_REJECTED,
	FC80211_PMSR_FTM_FAILURE_WRONG_CHANNEL,
	FC80211_PMSR_FTM_FAILURE_PEER_NOT_CAPABLE,
	FC80211_PMSR_FTM_FAILURE_INVALID_TIMESTAMP,
	FC80211_PMSR_FTM_FAILURE_PEER_BUSY,
	FC80211_PMSR_FTM_FAILURE_BAD_CHANGED_PARAMS,
};

/**
 * enum fc80211_sae_pwe_mechanism - The mechanism(s) allowed for SAE PWE
 *	derivation. Applicable only when WPA3-Personal SAE authentication is
 *	used.
 *
 * @FC80211_SAE_PWE_UNSPECIFIED: not specified, used internally to indicate that
 *	attribute is not present from userspace.
 * @FC80211_SAE_PWE_HUNT_AND_PECK: hunting-and-pecking loop only
 * @FC80211_SAE_PWE_HASH_TO_ELEMENT: hash-to-element only
 * @FC80211_SAE_PWE_BOTH: both hunting-and-pecking loop and hash-to-element
 *	can be used.
 */
enum fc80211_sae_pwe_mechanism {
	FC80211_SAE_PWE_UNSPECIFIED,
	FC80211_SAE_PWE_HUNT_AND_PECK,
	FC80211_SAE_PWE_HASH_TO_ELEMENT,
	FC80211_SAE_PWE_BOTH,
};

/**
 * enum fc80211_sar_type - type of SAR specs
 *
 * @FC80211_SAR_TYPE_POWER: power limitation specified in 0.25dBm unit
 *
 */
enum fc80211_sar_type {
	FC80211_SAR_TYPE_POWER,

	/* add new type here */

	/* Keep last */
	NUM_FC80211_SAR_TYPE,
};

#endif	/* __FC80211_COPY_H__ */

/* EOF */
