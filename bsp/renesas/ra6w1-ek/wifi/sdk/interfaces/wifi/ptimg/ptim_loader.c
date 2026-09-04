/**
 ****************************************************************************************
 *
 * @file sdk_ptim_loader.c
 *
 * @brief Helper source to load PTIM image
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

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "bsp_pd.h"
#include "ptim_loader.h"
#include "bsp_api.h"
#include "bsp_definitions.h"
#include "bsp_sflash_map_ra6w1.h"
#include "bsp_defaults.h"

#define BOOTER_TIM_ADDR  (0x28600004)
#define BOOTER_FAST_BOOT (0x2860001C)
#define FAST_BOOT        (0x74736166)


#define MEM_LRD(addr, data) *data = *((volatile uint32_t *)(addr))
#define MEM_LWR(addr, data) *((volatile uint32_t *)(addr)) = data


typedef struct {
    /// image mode for booter
    uint32_t mode;
    /// image version
    uint32_t ver;
    /// image size
    uint32_t sz;
    /// crc for image
    uint32_t crc;
    /// ptim image. the retention offset for ptim image is always multiples of 512.
    uint32_t img[0];
} timg_info_hdr;


extern uint8_t ptimg[];

uint32_t ptim_version_get(void)
{
    timg_info_hdr *ptimg_hdr = (timg_info_hdr *)ptimg;

    return ptimg_hdr->ver;
}

void load_ptimg(uint32_t ptimg_rtm_addr)
{
    timg_info_hdr *ptimg_hdr = (timg_info_hdr *) ptimg;

    // ready power-down
    // hw_rtc_ready_power_down(1);

    #if 0
    printf("PTIM Image Header\n\r");
    printf("\tmode %08x\n\r", ptimg_hdr->mode);
    printf("\tver  %08x\n\r", ptimg_hdr->ver);
    printf("\tsize %d\n\r", ptimg_hdr->sz);
    printf("\tcrc  %08x\n\r", ptimg_hdr->crc);
    printf("PTIM IMG Address: %08x\n\r", ptimg_rtm_addr);
    #endif

    assert(ptimg_hdr->sz < dg_configPTIMG_SIZE);

    memcpy((void*) ptimg_rtm_addr, ptimg, ptimg_hdr->sz + PTIMG_HEADER_SZ);

    MEM_LWR(BOOTER_TIM_ADDR, ptimg_rtm_addr);
    MEM_LWR(BOOTER_FAST_BOOT, FAST_BOOT);

    R_BSP_RetainedMemFlagSet();

    // hw_rtc_into_power_down(0, 40);
}
