/*
 * Compatibility header for the RA6W1 Wi-Fi SDK.
 *
 * The distributed rwnx_cfg.h includes rs_util.h, while this SDK drop keeps
 * the utility declarations in the prebuilt rwnx driver archive. The public
 * cfg80211 types used by the low-level test do not require any declaration
 * from that header.
 */

#ifndef RS_UTIL_H
#define RS_UTIL_H

#endif /* RS_UTIL_H */
