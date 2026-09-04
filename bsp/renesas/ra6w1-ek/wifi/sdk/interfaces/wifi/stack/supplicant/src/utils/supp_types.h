/*
 * wpa_supplicant/hostapd / common helper functions, etc.
 * Copyright (c) 2002-2007, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 *
 * Copyright (c) 2023 Modified by Renesas Electronics.
 */

#ifndef SUPP_TYPES_H
#define SUPP_TYPES_H

#include "sdk_defs.h"

static inline int is_zero_ether_addr(const u8 *a)
{
	return !(a[0] | a[1] | a[2] | a[3] | a[4] | a[5]);
}

static inline int wpa_supp_is_broadcast_ether_addr(const u8 *a)
{
	return (a[0] & a[1] & a[2] & a[3] & a[4] & a[5]) == 0xff;
}

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))


#endif /* SUPP_TYPES_H */

/* EOF */
