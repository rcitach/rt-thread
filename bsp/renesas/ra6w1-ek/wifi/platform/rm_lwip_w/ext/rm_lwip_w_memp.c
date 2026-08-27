/*
 * rm_lwip_w_memp.c
 *
 *  Created on: 11-Sep-2024
 *      Author: Renasas
 */

#include "lwip/opt.h"

#include "lwip/memp.h"
#include "lwip/sys.h"
#include "lwip/stats.h"
#include "rm_lwip_w_memp.h"

extern const struct memp_desc *const memp_pools[MEMP_MAX];

static void
print_memp_status(const struct memp_desc *desc)
{
    printf(CYAN_COLOR 	" %16s" CLEAR_COLOR
						" (size:%4d,"
#if !MEMP_MEM_MALLOC
						"  num:%3d)"
#else
						"  avail:%3d)"
#endif
						YELLOW_COLOR
						"  used:%2d"
						"  max:%2d"
						CLEAR_COLOR
						"  err:%2d"
						"  illegal:%2d"
#if !MEMP_MEM_MALLOC
						"  region:0x%hhn~0x%hhn"
#endif
						"\r\n",
						desc->desc,
						desc->size,
#if !MEMP_MEM_MALLOC
						desc->num,
#else
						desc->stats->avail,
#endif
						desc->stats->used,
						desc->stats->max,
						desc->stats->err,
						desc->stats->illegal
#if !MEMP_MEM_MALLOC
						,
						desc->base,
						desc->base + ((size_t)desc->num * (MEMP_SIZE + desc->size
#if MEMP_OVERFLOW_CHECK
						+ MEM_SANITY_REGION_AFTER_ALIGNED
#endif
						))
#endif

						);

}

void lwip_memp_status()
{
	int i;

    printf(CYAN_COLOR "\r\n << MEMP STATUS : total %d >> \r\n" CLEAR_COLOR, MEMP_MAX);
	for(i=0; i<MEMP_MAX; i++) {
    	printf(" %2d ", i+1);
		print_memp_status(memp_pools[i]);

		//MEMP_STATS_DISPLAY(i);
	}
	return;
}
