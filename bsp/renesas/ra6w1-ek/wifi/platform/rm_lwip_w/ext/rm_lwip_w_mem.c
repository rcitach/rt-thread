/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/*
 * rm_lwip_w_mem.c
 *
 *  Created on: 11-Sep-2024
 *      Author: Renasas
 */

#include "lwip/opt.h"
#include "lwip/mem.h"
#include "lwip/def.h"
#include "lwip/sys.h"
#include "lwip/stats.h"
#include "lwip/err.h"
#include "rm_lwip_w_mem.h"

#include <string.h>

/*
 * Copied from src/core/mem.c
 * mem_malloc is protected using mutex AND WIP_MEM_ALLOC_PROTECT
 */
#define LWIP_MEM_ALLOC_DECL_PROTECT()
#define LWIP_MEM_ALLOC_PROTECT()
#define LWIP_MEM_ALLOC_UNPROTECT()
#define LWIP_MEM_LFREE_VOLATILE

void lwip_mem_status()
{
	extern void lwip_memp_status();
	extern void print_txl_buffer_status();

#ifndef INTEGRATION_WITH_RTOS_HEAP
	mem_size_t ptr;
	struct mem *mem;
#endif	//INTEGRATION_WITH_RTOS_HEAP
	LWIP_MEM_ALLOC_DECL_PROTECT();
#ifndef INTEGRATION_WITH_RTOS_HEAP
	mem_size_t used_mem = 0;
	mem_size_t free_mem = 0;
	mem_size_t node_count = 0;
#endif	//INTEGRATION_WITH_RTOS_HEAP

	printf("\r\n");
	/* Scan through the heap searching for a free block that is big enough,
	 * beginning with the lowest free block.
	 */
#ifndef INTEGRATION_WITH_RTOS_HEAP
	printf(CYAN_COLOR "\r\n << lwip MEM STATUS >> \r\n" CLEAR_COLOR);

	/* protect the heap from concurrent access */
	sys_mutex_lock(&mem_mutex);
	LWIP_MEM_ALLOC_PROTECT();

	for (ptr = mem_to_ptr(ram); ptr <= MEM_SIZE_ALIGNED; ptr = ptr_to_mem(ptr)->next) {
		mem = ptr_to_mem(ptr);
		printf(" mem: 0x%x used:%d next:0x%x prev:0x%x size:%d \r\n", mem, mem->used, mem->next, mem->prev, mem->next-(ptr+SIZEOF_STRUCT_MEM));

		node_count++;

		if (ptr == MEM_SIZE_ALIGNED) {
			break;
		}

		if (!mem->used)  {
			free_mem += (mem->next - (ptr + SIZEOF_STRUCT_MEM));
		}
		else if(mem->used) {
			used_mem += (mem->next - (ptr + SIZEOF_STRUCT_MEM));
		}
		else {
			printf(RED_COLOR " [%s] Fail check \r\n" CLEAR_COLOR, __func__);
		}
	}

	LWIP_MEM_ALLOC_UNPROTECT();
	sys_mutex_unlock(&mem_mutex);

	printf(CYAN_COLOR " Total mem  : %d \r\n" CLEAR_COLOR, MEM_SIZE);
	printf(CYAN_COLOR " node cnt   : %d \r\n" CLEAR_COLOR, node_count);
	printf(CYAN_COLOR " error_cnt  : %d \r\n" CLEAR_COLOR, mem_alloc_fail_cnt);
	printf(CYAN_COLOR " illegal cnt: %d \r\n" CLEAR_COLOR, mem_alloc_illegal_cnt);
	printf(CYAN_COLOR " Used_mem   : %d \r\n" CLEAR_COLOR, used_mem);
	printf(CYAN_COLOR " Free mem   : %d \r\n" CLEAR_COLOR, free_mem);

	//MEM_STATS_DISPLAY();
#endif	/* INTEGRATION_WITH_RTOS_HEAP */

	lwip_memp_status();
	print_txl_buffer_status();

	return;
}
