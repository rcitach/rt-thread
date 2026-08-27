/**
 ****************************************************************************************
 *
 * @file nvedit.h
 *
 * @brief NVRAM Driver
 *
 * Copyright (c) 2023 Renesas Electronics. All rights reserved.
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


#ifndef __nvedit_h__
#define __nvedit_h__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#ifdef	BUILD_OPT_VSIM
#define	SUPPORT_NVEDIT      /* PoSim support */
#else	//BUILD_OPT_VSIM
#define	SUPPORT_NVEDIT
#endif	//BUILD_OPT_VSIM

#ifdef	SUPPORT_NVEDIT

#define	SUPPORT_NVEDIT_FULL

#ifdef	BUILD_OPT_RA6W1_FPGA
#undef	SUPPORT_NVRAM_NOR
#define	SUPPORT_NVRAM_SFLASH
#else	//BUILD_OPT_RA6W1_FPGA
#undef	SUPPORT_NVRAM_NOR
#define	SUPPORT_NVRAM_SFLASH
#endif	//BUILD_OPT_RA6W1_FPGA

#undef	SUPPORT_NVRAM_RAM
#define	SUPPORT_NVRAM_HASH
#define	SUPPORT_NVRAM_SECU
#define SUPPORT_NVRAM_FOOTPRINT

#else	//SUPPORT_NVEDIT
#undef	SUPPORT_NVEDIT_FULL
#undef 	SUPPORT_NVRAM_NOR
#undef	SUPPORT_NVRAM_SFLASH
#undef	SUPPORT_NVRAM_RAM
#undef	SUPPORT_NVRAM_HASH
#undef	SUPPORT_NVRAM_SECU

#endif	//SUPPORT_NVEDIT
//---------------------------------------------------------
// Global Definition
//---------------------------------------------------------

typedef 	struct __nvedit_handler__	NVEDIT_HANDLER_TYPE;
typedef		struct	__nvedit_tree_item__	NVEDIT_TREE_ITEM;
typedef		struct	__nvedit_tree_tab__	NVEDIT_TABLE_TYPE;

//---------------------------------------------------------
//	DRIVER :: Commands & Parameters
//---------------------------------------------------------
//
//	Equipment Type
//

typedef enum	{
	NVEDIT_0	= 0,	/* Common */
	NVEDIT_1	,	/* Application Only */
	NVEDIT_MAX
} NVEDIT_LIST;

//
//	IOCTL Commands
//

typedef enum	{
	NVEDIT_SET_DEBUG = 0,

	NVEDIT_CMD_INIT,
	NVEDIT_CMD_SAVE,
	NVEDIT_CMD_LOAD,
	NVEDIT_CMD_CLEAR,
	NVEDIT_CMD_ERASE,
	NVEDIT_CMD_ROOT,
	NVEDIT_CMD_ADD,
	NVEDIT_CMD_DEL,
	NVEDIT_CMD_FIND,
	NVEDIT_CMD_LITEFIND,
	NVEDIT_CMD_LITEUPDATE,

	NVEDIT_CMD_PRINT,
	NVEDIT_CMD_CHANGE_BASE,
	NVEDIT_CMD_GETCOUNT,
	NVEDIT_CMD_RESETCOUNT,
	NVEDIT_CMD_SETSIZE,
	NVEDIT_CMD_SECURE,

	NVEDIT_SET_MAX
} NVEDIT_IOCTL_LIST;

typedef		enum	{
	NVEDIT_NOR = 0,
	NVEDIT_SFLASH,
	NVEDIT_RAM,
	NVEDIT_STORAGE_MAX,
} NVEDIT_STORAGE_LIST;

//---------------------------------------------------------
//	DRIVER :: Bit Field Definition
//---------------------------------------------------------

#define	NVEDIT_WORBUF_DEFAULT	0x1000			// 4 KB

#define	NVITEM_MAGIC_CODE	0xFC1F9000

#define	NVITEM_LEN_MASK		0x00FFFFFF
#define	NVITEM_TYPE_MASK	0x7F000000		// Do not modify !!
#define	NVITEM_VAR_MARK		0x20000000
#define	NVITEM_NOD_MARK		0x40000000
#define	NVITEM_HASH_MARK	0x80000000		// Do not modify !!
#define	NVITEM_SET_TYPE(x)	(((x)<<24)&NVITEM_TYPE_MASK)
#define	NVITEM_GET_TYPE(x)	(((x)&NVITEM_TYPE_MASK)>>24)

#define	NVITEM_NAME_MAX		63
#define	NVEDIT_MAX_DEPTH	6

#if	(NVITEM_NAME_MAX+1) > ((NVEDIT_MAX_DEPTH+1)*4)
#define	NVEDIT_TOKENBUF_SIZ	(NVITEM_NAME_MAX+1)
#else	//(NVITEM_NAME_MAX+1) > ((NVEDIT_MAX_DEPTH+1)*4)
#define	NVEDIT_TOKENBUF_SIZ	((NVEDIT_MAX_DEPTH+1)*4)
#endif	//(NVITEM_NAME_MAX+1) > ((NVEDIT_MAX_DEPTH+1)*4)

//---------------------------------------------------------
//	DRIVER :: Structure
//---------------------------------------------------------

struct	__nvedit_tree_tab__
{
	UINT32	magic;
	UINT32	total;
	UINT32  reserved;
	UINT32	chksum;
};

struct	__nvedit_tree_item__
{
	char	*name;
#ifdef	SUPPORT_NVRAM_HASH
	UINT32	hash;
#endif	//SUPPORT_NVRAM_HASH
	UINT32	length;
	VOID	*value;

	NVEDIT_TREE_ITEM	*parent;
	NVEDIT_TREE_ITEM	*child;
	NVEDIT_TREE_ITEM	*next;
};

struct	__nvedit_handler__
{
	UINT32			dev_addr;  // Unique Address
	// Device-dependant
	UINT32			dev_unit;
	UINT32			instance;
	UINT32			update;

	UINT32			nor_base;
	UINT32			sflash_base;
	UINT32			sflash_foot_base;
	UINT32			ram_base;

	UINT32			debug;

	HANDLE			nor_device;
	HANDLE			sflash_device;

	NVEDIT_TABLE_TYPE	table;
	UINT8			*workbuf;
	NVEDIT_TREE_ITEM	*root;
	VOID			*mempool;
	UINT32			poolsiz;
	NVEDIT_TREE_ITEM	*find;
	UINT32			secure;
	char			tokenbuffer[NVEDIT_TOKENBUF_SIZ];
};

//---------------------------------------------------------
//	DRIVER :: APP-Interface
//---------------------------------------------------------

extern HANDLE	NVEDIT_CREATE( UINT32 dev_id );
extern int	NVEDIT_INIT (HANDLE handler);
extern int	NVEDIT_IOCTL(HANDLE handler, UINT32 cmd , VOID *data );
extern int	NVEDIT_READ(HANDLE handler, UINT32 addr, VOID *p_data, UINT32 p_dlen);
extern int	NVEDIT_WRITE(HANDLE handler, UINT32 addr, VOID *p_data, UINT32 p_dlen);
extern int	NVEDIT_CLOSE(HANDLE handler);

//---------------------------------------------------------
//	DRIVER :: DEVICE-Interface
//---------------------------------------------------------


//---------------------------------------------------------
//	DRIVER :: ISR
//---------------------------------------------------------



//---------------------------------------------------------
//	DRIVER :: Utility
//---------------------------------------------------------



#endif /* __nvedit_h__ */

/* EOF */
