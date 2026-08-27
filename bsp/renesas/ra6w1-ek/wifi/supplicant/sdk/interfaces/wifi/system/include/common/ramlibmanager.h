/**
 ****************************************************************************************
 *
 * @file ramlibmanager.h
 *
 * @brief RAM-Library Manager
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

#ifndef __ramlibmanager_h__
#define __ramlibmanager_h__

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/******************************************************************************
 *  RamLIB Manager
 ******************************************************************************/

extern int ramlib_manager_check(void);
extern int ramlib_manager_init(UINT32 skip, UINT32 loadaddr);
extern void ramlib_manager_bind(UINT32);
extern char *ramlib_get_logo(void);
extern UINT8 ramlib_get_info(void);
extern void ramlib_clear(void);

/******************************************************************************
 *  PTIM Mananger
 ******************************************************************************/

extern int ptim_manager_check(UINT32 storeaddr, UINT32 force);
extern int ptim_manager_load(UINT32 loadaddr, UINT32 storeaddr );
extern void ptim_clear(UINT32 storeaddr);


#endif /* __ramlibmanager_h__ */

/* EOF */
