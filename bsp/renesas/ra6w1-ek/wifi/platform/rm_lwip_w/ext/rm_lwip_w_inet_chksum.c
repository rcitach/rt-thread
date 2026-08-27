/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/*
 * rm_lwip_w_inet_checksum.c
 *
 *  Created on: 14-Oct-2024
 *      Author: renesas
 */

#include "lwip/opt.h"

#include "lwip/inet_chksum.h"
#include "lwip/def.h"

#if (LWIP_CHKSUM_ALGORITHM == 4) /* Alternative version #4 by Dialog */
u16_t
lwip_standard_chksum(const void *dataptr, int len)
{
	u32_t acc;
	u16_t src;
	int 	curlen;
	const u8_t *octetptr , *end_ptr;
	u32_t *long_ptr , *temp_ptr;
	unsigned long src_bin;

    	acc = 0;
	long_ptr = (u32_t *)(const void *)dataptr;

	end_ptr = dataptr + len;
	temp_ptr = long_ptr;
	/* 4 byte alignment checking */
	src_bin = dataptr;
	if (!(src_bin & ~3))
	{
		while(long_ptr < end_ptr)
		{
			temp_ptr = long_ptr;
			acc += (*long_ptr & LOWER_16_MASK);
			acc += (*long_ptr >> SHIFT_BY_16);
			long_ptr++;
		}
		long_ptr = temp_ptr;
	}

	/* after 4 byte checksum , remaining checking */
	octetptr = (char *)long_ptr;
	curlen = end_ptr - octetptr;

	while(curlen > 1)
	{
		src = (*octetptr) << 8;
		octetptr++;

		/* declare second octet as least significant */
		src |= (*octetptr);
		octetptr++;
		acc += src;
		curlen -= 2;
	}

	if (curlen > 0)
	{
  		/* accumulate remaining octet */
		src = (*octetptr) << 8;
		acc += src;
	}

	/* add deferred carry bits */
	acc = (acc >> 16) + (acc & 0x0000ffffUL);
	acc = (acc >> 16) + (acc & 0x0000ffffUL);

	return lwip_htons((u16_t)acc);
}
#endif


