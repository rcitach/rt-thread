/**
 ****************************************************************************************
 *
 * @file sys_image.h
 *
 * @brief Image management
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


#ifndef	__sys_image_h__
#define	__sys_image_h__

/******************************************************************************
 *
 *  Definitions
 *
 ******************************************************************************/

/*
 * Image Types
 */
#define IMAGE_HEADER_TYPE_MULTI		4			/* Image for multi-images */


#define	NOR_BOOT_RUN	1
#define	NOR_BOOT_CHECK	2
#define	NOR_BOOT_HEADER	4

/*
 * OTP Tag Types
 */

typedef	struct	{
	UINT8	tag   :1;
	UINT8	xtal  :3;
	UINT8	sflash:1;
	UINT8	dio   :1;
	UINT8	otp   :1;
	UINT8	mark[3];
}	OTP_TAG_TYPE;

/******************************************************************************
 *
 *
 *
 ******************************************************************************/

extern UINT32 nor_image_load(UINT32 mode, UINT32 imghdr_offset, UINT32 *load_addr, UINT32 *jmp_addr);

/******************************************************************************
 *
 *
 *
 ******************************************************************************/

extern void		ra6wx_sflash_reset_parameters(void);
extern UINT32	ra6wx_sflash_get_parasize(void);
extern UINT32	ra6wx_sflash_get_maxspeed(void);
extern void		ra6wx_sflash_set_maxspeed(UINT32 spiclock);
extern UINT32	ra6wx_sflash_get_bussel(void);
extern void		ra6wx_sflash_set_bussel(UINT32 bussel);
extern UINT32	ra6wx_sflash_setup_parameter(UINT32 *parameters);
extern void		ra6wx_sflash_set_image(UINT32 ipoly, UINT32 ioffset, UINT32 isize);
extern void		ra6wx_sflash_set_image(UINT32 ipoly, UINT32 ioffset, UINT32 isize);

extern UINT32	otp_image_check(UINT32 img_offset);
extern UINT32	otp_image_store(UINT32 img_offset, UINT32 otp_offset, UINT32 img_length);
extern UINT32	otp_image_load(UINT32 otp_offset, UINT32 *loadaddr, UINT32 *jmpaddr);

/******************************************************************************
 *
 *
 *
 ******************************************************************************/

#endif	/*__sys_image_h__*/

/* EOF */
