/**
 ****************************************************************************************
 *
 * @file sys_sort.h
 *
 * @brief Sort Algorithms
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


#ifndef	__sys_sort_h__
#define	__sys_sort_h__

/******************************************************************************
 *
 *  Definitions
 *
 ******************************************************************************/

#undef	SORT_FULL_LIB
#define	SORT_TEMPSIZE		256

typedef  int	(sort_comp_func)(const void *, const void *);

/******************************************************************************
 *
 *  Core Functions
 *
 ******************************************************************************/

#ifdef	SORT_FULL_LIB
extern void ra6wx_shellsortInner(void *a, size_t num, size_t size, void *temp, sort_comp_func *comp);

extern void ra6wx_heapsortInner(void *a, int num, size_t size, void *temp, sort_comp_func *comp);
#endif	//SORT_FULL_LIB

extern void ra6wx_quicksortInner( void *a, int l, int r, size_t size, void *temp, sort_comp_func *comp);

extern void ra6wx_mergesortInner(void *a, int high, size_t size, UINT32 *tmp, sort_comp_func *comp);
/*
	Wrapper Example

int ra6wx_mergesort(void *a, unsigned int num, size_t size, sort_comp_func *comp)
{
	if (num > 1) {
		void *tmp = (void *)APP_MALLOC(num*size);

		ra6wx_mergesortInner(a, num, size, tmp, comp);

		APP_FREE(tmp);
		return 1;
	}
	return -1;
}

*/

extern void ra6wx_insertsortInner(void *a, int num, size_t size, UINT32 *tmp, sort_comp_func *comp);

/******************************************************************************
 *
 *  Wrapper Functions
 *
 ******************************************************************************/

#ifdef	SORT_FULL_LIB

//
// Shell	: parameter, size must be less than "SORT_TEMPSIZE". (temp)
//
extern int ra6wx_shellsort(void *item, size_t num, size_t size, sort_comp_func *comp);

//
// Heap		: parameter, size must be less than "SORT_TEMPSIZE". (temp)
//
extern int ra6wx_heapsort(void *item, int num, size_t size, sort_comp_func *comp);

#endif	//SORT_FULL_LIB

//
// Quick	: parameter, size must be less than "SORT_TEMPSIZE>>1". (pivot & temp)
//
extern int ra6wx_quicksort(void *item, size_t num, size_t size, sort_comp_func *comp);

//
// ModiQuick	: parameter, size must be less than "SORT_TEMPSIZE>>1". (pivot & temp)
//
extern int ra6wx_mquicksort(void *item, size_t num, size_t size, sort_comp_func *comp);

//
// Insert	: parameter, size must be less than "SORT_TEMPSIZE". (temp)
//
extern int ra6wx_insertsort(void *a, int num, size_t size, sort_comp_func *comp);

/******************************************************************************
 *
 *  CRC Functions
 *
 ******************************************************************************/

extern UINT8 da16x_crc8ccitt(void *data, size_t siz, UINT8 seed);

#endif	/*__sys_sort_h__*/
