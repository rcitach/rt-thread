/**
 ****************************************************************************************
 *
 * @file lmac_struct.h
 *
 * @brief
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

#define BASE_ADDR 0x10B00000
typedef unsigned int u32;
typedef struct MAC_REGISTER{
    volatile u32 *addr;
    u32 reset_value;
    u32 write_mask;
}ST_MAC_REGISTER;

#define MAX_REGISTER    343    /*number of mac register*/
ST_MAC_REGISTER fc9000_mac[/*MAX_REGISTER*/] = {
    { (volatile u32*)(BASE_ADDR + 0x00000000), 0x00000000, 0x00000000}, /*signature*/
    { (volatile u32*)(BASE_ADDR + 0x00000004), 0x00000000, 0x00000000}, /*version*/
    { (volatile u32*)(BASE_ADDR + 0x00000008), 0x00000000, 0x00000000},
    { (volatile u32*)(BASE_ADDR + 0x0000000c), 0x00000000, 0x00000000},
    { (volatile u32*)(BASE_ADDR + 0x00000010), 0x00000000, 0xffffffff},
    { (volatile u32*)(BASE_ADDR + 0x00000014), 0x00000000, 0x0000ffff},
    { (volatile u32*)(BASE_ADDR + 0x00000018), 0x00000000, 0xffffffff},
    { (volatile u32*)(BASE_ADDR + 0x0000001c), 0x00000000, 0x0000ffff},
    { (volatile u32*)(BASE_ADDR + 0x00000020), 0x00000000, 0xffffffff},
    { (volatile u32*)(BASE_ADDR + 0x00000024), 0x00000000, 0x0000ffff},
    { (volatile u32*)(BASE_ADDR + 0x00000028), 0x00000000, 0xffffffff},
    { (volatile u32*)(BASE_ADDR + 0x0000002c), 0x00000000, 0x0000ffff},
    { (volatile u32*)(BASE_ADDR + 0x00000038), 0x00000000, 0x000000f0},
    { (volatile u32*)(BASE_ADDR + 0x0000003c), 0x00000000, 0x0000ffff},
    { (volatile u32*)(BASE_ADDR + 0x00000044), 0x00000000, 0xffffffff},
    { (volatile u32*)(BASE_ADDR + 0x00000048), 0x00000001, 0x00000001},
    { (volatile u32*)(BASE_ADDR + 0x0000004c), 0x0000c881, 0x0401cf8f},
    { (volatile u32*)(BASE_ADDR + 0x00000054), 0x00000000, 0x00000002},
    { (volatile u32*)(BASE_ADDR + 0x00000058), 0x00000000, 0x00000000},
    { (volatile u32*)(BASE_ADDR + 0x0000005c), 0x00000000, 0x0000000f},
    { (volatile u32*)(BASE_ADDR + 0x00000060), 0x15078788, 0x7fffbfff},
    { (volatile u32*)(BASE_ADDR + 0x00000064), 0x00000000, 0xffffffff}, /*R_value user defined*/
    { (volatile u32*)(BASE_ADDR + 0x00000068), 0x00000000, 0x0fffffff},
    { (volatile u32*)(BASE_ADDR + 0x00000090), 0x00000000, 0x0000ffff},
    { (volatile u32*)(BASE_ADDR + 0x00000094), 0x00000000, 0x0000ffff},
    { (volatile u32*)(BASE_ADDR + 0x00000098), 0x00000407, 0x0000ffff},
    { (volatile u32*)(BASE_ADDR + 0x0000009c), 0x08000000, 0x1cffffff},
    { (volatile u32*)(BASE_ADDR + 0x000000a0), 0x00000404, 0x0000ffff},
    { (volatile u32*)(BASE_ADDR + 0x000000ac), 0x00000000, 0xffffffff},
    { (volatile u32*)(BASE_ADDR + 0x000000b0), 0x00000000, 0xffffffff},
    { (volatile u32*)(BASE_ADDR + 0x000000b4), 0x00000000, 0xffffffff},
    { (volatile u32*)(BASE_ADDR + 0x000000b8), 0x00000000, 0xffffffff},
    { (volatile u32*)(BASE_ADDR + 0x000000bc), 0xffffffff, 0xffffffff},
    { (volatile u32*)(BASE_ADDR + 0x000000c0), 0x0000ffff, 0x0000ffff},
    { (volatile u32*)(BASE_ADDR + 0x000000c4), 0x00000000, 0xc3ff03ff},      
    { (volatile u32*)(BASE_ADDR + 0x000000dc), 0x00000150, 0x00000fff},
    { (volatile u32*)(BASE_ADDR + 0x000000e0), 0x00000000, 0xffffffff},
    { (volatile u32*)(BASE_ADDR + 0x000000e4), 0x00000000, 0x0fffffff},
    { (volatile u32*)(BASE_ADDR + 0x000000e8), 0x00000000, 0x00ffffff},
    { (volatile u32*)(BASE_ADDR + 0x000000ec), 0x00000000, 0x3fffffff},
    { (volatile u32*)(BASE_ADDR + 0x000000f0), 0x00000000, 0xfffff000},
    { (volatile u32*)(BASE_ADDR + 0x000000f4), 0x00000000, 0x00ffffff},
    { (volatile u32*)(BASE_ADDR + 0x000000f8), 0x00000000, 0x00ffffff},
    { (volatile u32*)(BASE_ADDR + 0x000000fc), 0x00000000, 0x00ffffff},
    { (volatile u32*)(BASE_ADDR + 0x00000100), 0x2160c019, 0xffffffff},
    { (volatile u32*)(BASE_ADDR + 0x00000104), 0x00000000, 0x3fffffff},
    { (volatile u32*)(BASE_ADDR + 0x0000010c), 0x00000009, 0x000000ff},
    { (volatile u32*)(BASE_ADDR + 0x00000110), 0x00000f9c, 0x0000ffff},
    { (volatile u32*)(BASE_ADDR + 0x00000114), 0x000f0f9c, 0x00ffffff},
    { (volatile u32*)(BASE_ADDR + 0x00000118), 0x00004000, 0xffffc3ff},
    //{ (volatile u32*)(BASE_ADDR + 0x0000011c), 0x00000000, 0xffffffff},  /*Timer*/
    //{ (volatile u32*)(BASE_ADDR + 0x00000120), 0x00000000, 0xffffffff},  /*Timer*/
    { (volatile u32*)(BASE_ADDR + 0x00000124), 0x00000000, 0x00000000},
    { (volatile u32*)(BASE_ADDR + 0x00000128), 0x00000000, 0xffffffff},
    { (volatile u32*)(BASE_ADDR + 0x0000012c), 0x00000000, 0x00000000},
    { (volatile u32*)(BASE_ADDR + 0x00000150), 0x0000ffff, 0x0000ffff},
    { (volatile u32*)(BASE_ADDR + 0x00000200), 0x00000a47, 0x0fffffff},
    { (volatile u32*)(BASE_ADDR + 0x00000204), 0x00000a43, 0x0fffffff},
    { (volatile u32*)(BASE_ADDR + 0x00000208), 0x0005e432, 0x0fffffff},
    { (volatile u32*)(BASE_ADDR + 0x0000020c), 0x0002f322, 0x0fffffff},
    { (volatile u32*)(BASE_ADDR + 0x00000220), 0x00000000, 0xffffffff},
    { (volatile u32*)(BASE_ADDR + 0x00000224), 0x00000000, 0x00000033},
    { (volatile u32*)(BASE_ADDR + 0x00000280), 0x00000000, 0xffffffff},
    { (volatile u32*)(BASE_ADDR + 0x00000284), 0x00000000, 0x0000ffff},
    { (volatile u32*)(BASE_ADDR + 0x00000288), 0x00000000, 0xffffffff},
    { (volatile u32*)(BASE_ADDR + 0x0000028c), 0x00000000, 0x0000ffff},
    { (volatile u32*)(BASE_ADDR + 0x00000300), 0x00000000, 0xffffffff},
    { (volatile u32*)(BASE_ADDR + 0x00000304), 0x00000000, 0x3fffffdf},
    { (volatile u32*)(BASE_ADDR + 0x00000308), 0x00000000, 0x0000ffff},
    { (volatile u32*)(BASE_ADDR + 0x0000030c), 0x00000000, 0xffff31ff},
    { (volatile u32*)(BASE_ADDR + 0x00000310), 0x00008548, 0x0000ffff},
    { (volatile u32*)(BASE_ADDR + 0x00000314), 0x0000ffff, 0x003fffff},
    { (volatile u32*)(BASE_ADDR + 0x0000031c), 0x0000ffff, 0x0000ffff},
    { (volatile u32*)(BASE_ADDR + 0x00000320), 0x00000000, 0x00000001},
    { (volatile u32*)(BASE_ADDR + 0x00000500), 0x00000000, 0x00000000},
    { (volatile u32*)(BASE_ADDR + 0x00000504), 0x00000000, 0x00000000},
    { (volatile u32*)(BASE_ADDR + 0x0000050c), 0x00000000, 0x00000000},
    { (volatile u32*)(BASE_ADDR + 0x00000510), 0x00000000, 0x0000ffff},
    { (volatile u32*)(BASE_ADDR + 0x00000514), 0x00000000, 0x03ffffff},
    { (volatile u32*)(BASE_ADDR + 0x00000518), 0x00002344, 0x00000000},
    { (volatile u32*)(BASE_ADDR + 0x0000051c), 0x00000000, 0x00000000},
    { (volatile u32*)(BASE_ADDR + 0x00000520), 0x00000000, 0x00000000},
    { (volatile u32*)(BASE_ADDR + 0x0000055c), 0x00000000, 0x00000001},
};
