/**
 ****************************************************************************************
 *
 * @file crypto_primitives.h
 *
 * @brief RA6W1/RA6W2 crypto
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


#ifndef __crypto_primitives_h__
#define __crypto_primitives_h__

#include "r_cc312_common.h"
#include <stdarg.h>

/*******************************************************************************************************************//**
 * @addtogroup R_CC312_OPENABLE_W
 * @{
 **********************************************************************************************************************/

/******************************************************************************
 *
 *  Retargeted functions
 *
 ******************************************************************************/

/**
 * \brief Structure for Retargeted symbols in ROMCrypto
 *
 */
typedef		struct	{
	void	(*retarget_dump)(uint16_t tag, void *srcdata, uint16_t len);            /*!< \brief callback of hexadump function */
	void	(*retarget_text)(uint16_t tag, void *srcdata, uint16_t len);            /*!< \brief callback of puts function */
	void	(*retarget_vprint)(uint16_t tag, const char *format, va_list arg);      /*!< \brief callback of vprintf function */
	char	(*retarget_getchar)(uint32_t mode );                                    /*!< \brief callback of getchar function */

	void 	*(*raw_malloc)(size_t size);                                    /*!< \brief callback of malloc function */
	void 	*(*raw_realloc)(void* Buffer, size_t  NewSize);                 /*!< \brief callback of realloc function */
	void 	(*raw_free)(void *f);                                           /*!< \brief callback of free function */
        
        uint32_t (*raw_otpread)(uint32_t otpwoffset);                           /*!< \brief callback of OTP read Wrapper */
        uint32_t (*raw_otpwrite)(uint32_t otpwoffset, uint32_t otpData);        /*!< \brief callback of OTP write Wrapper */

        void	*(*flash_image_open)(uint32_t mode, uint32_t imghdr_offset, void *locker);              /*!< \brief callback for flash access */
        void	(*flash_image_close)(void *handler);                                                    /*!< \brief callback for flash access */
        uint32_t (*flash_image_check)(void *handler, uint32_t imgtype, uint32_t imghdr_offset);         /*!< \brief callback for flash access */
        void	*(*flash_image_certificate)(void *handler, uint32_t imghdr_offset, uint32_t certindex, uint32_t *certsize);     /*!< \brief callback for flash access */
        uint32_t (*flash_image_extract)(void *handler, uint32_t imghdr_offset, uint32_t *load_addr, uint32_t *jmp_addr);        /*!< \brief callback for flash access */
        uint32_t (*flash_image_load)(void *handler, uint32_t imghdr_offset, uint32_t *load_addr, uint32_t *jmp_addr);           /*!< \brief callback for flash access */
        uint32_t (*flash_image_read)(void *handler, uint32_t img_offset, void *load_addr, uint32_t img_secsize);                /*!< \brief callback for flash access */

        void    (*rtosdelay)(uint32_t usec);            /*!< \brief callback of OS delay function */
        uint64_t (*tickmeasure)(uint32_t flag);         /*!< \brief callback for time measurement */
} CRYPTO_PRIMITIVE_TYPE;

/*******************************************************************************************************************//**
 * @brief Initiaize ROMCrypto Library
 *
 * @param[in]   primitive               retargeted symbols used in RomCrypto
 *  
 ***********************************************************************************************************************/
extern void	init_crypto_primitives(const CRYPTO_PRIMITIVE_TYPE *primitive);

/*******************************************************************************************************************//**
 * @} (end addtogroup R_CC312_OPENABLE_W)
 **********************************************************************************************************************/

//--------------------------------------------------------------------
//	Memory
//--------------------------------------------------------------------

extern void *embcrypto_malloc(size_t size);
extern void *embcrypto_realloc(void *buffer, size_t NewSize);
extern void embcrypto_free(void *f);

#define	CRYPTO_MALLOC(...)		embcrypto_malloc( __VA_ARGS__ )
#define	CRYPTO_REALLOC(...)		embcrypto_realloc( __VA_ARGS__ )
#define	CRYPTO_FREE(...)		embcrypto_free( __VA_ARGS__ )

extern uint32_t embcrypto_otpread(uint32_t otpaddr);
extern uint32_t embcrypto_otpwrite(uint32_t otpaddr, uint32_t otpData);

#define	CRYPTO_OTP_READ(...)		embcrypto_otpread( __VA_ARGS__ )
#define	CRYPTO_OTP_WRITE(...)	        embcrypto_otpwrite( __VA_ARGS__ )

//--------------------------------------------------------------------
//	Console
//--------------------------------------------------------------------

extern void	embcrypto_dump(uint16_t tag, void *srcdata, uint16_t len);
extern void	embcrypto_text(uint16_t tag, void *srcdata, uint16_t len);
extern void	embcrypto_vprint(uint16_t tag, const char *format, va_list arg);
extern void	embcrypto_print(uint16_t tag, const char *fmt,...);
extern int	embcrypto_getchar(uint32_t mode );
extern void 	embcrypto_memset(void * dest, int value, size_t num);

extern void	*embcrypto_flash_image_open(uint32_t mode, uint32_t imghdr_offset, void *locker);
extern void	embcrypto_flash_image_close(void *handler);
extern uint32_t	embcrypto_flash_image_check(void *handler, uint32_t imgtype, uint32_t imghdr_offset);
extern void	*embcrypto_flash_image_certificate(void *handler, uint32_t imghdr_offset, uint32_t certindex, uint32_t *certsize);
extern uint32_t	embcrypto_flash_image_extract(void *handler, uint32_t imghdr_offset, uint32_t *load_addr, uint32_t *jmp_addr);
extern uint32_t	embcrypto_flash_image_load(void *handler, uint32_t imghdr_offset, uint32_t *load_addr, uint32_t *jmp_addr);
extern uint32_t	embcrypto_flash_image_read(void *handler, uint32_t img_offset, void *load_addr, uint32_t img_secsize);

extern uint32_t	embcrypto_random(void);
extern void     embcrypto_rtosdelay(uint32_t usec);
extern uint64_t embcrypto_tickmeasure(uint32_t flag);

extern uint32_t embcrypto_remapper(uint32_t vaddr);

#endif /* __crypto_primitives_h__ */
