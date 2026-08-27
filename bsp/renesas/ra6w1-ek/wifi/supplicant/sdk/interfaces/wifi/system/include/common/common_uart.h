/**
 ****************************************************************************************
 *
 * @file common_uart.h
 *
 * @brief Define for UART module
 *
 * Copyright (c) 2016-2022 Renesas Electronics. All rights reserved.
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

#ifndef __COMMON_UART_H__
#define __COMMON_UART_H__

#include <stdarg.h>
#include "FreeRTOS.h"
#include "custom_config_sdk.h"

typedef int (*command_function_t)(int argc, char *argv[]);

typedef struct
{
	/* The command name matched at the command line. */
	char	*name;

	/* Function that runs the command. */
	command_function_t	command;

	/* Minimum number of arguments. */
	int	arg_count;

	/* String describing argument format used
	 * by the generic help generator function. */
	char	*format;

	/* Brief description of the command */
	char	*brief;
} command_t;


/// @brief Baud rate
#define UART_BAUDRATE_921600		921600
#define UART_BAUDRATE_460800		460800
#define UART_BAUDRATE_230400		230400
#define UART_BAUDRATE_115200		115200
#define UART_BAUDRATE_57600			57600
#define UART_BAUDRATE_38400			38400
#define UART_BAUDRATE_28800			28800
#define UART_BAUDRATE_19200			19200
#define UART_BAUDRATE_14400			14400
#define UART_BAUDRATE_9600			9600

/// @brief Data bits
#define UART_DATABITS_5  			5
#define UART_DATABITS_6  			6
#define UART_DATABITS_7  			7
#define UART_DATABITS_8  			8

/// @brief Parity
#define UART_PARITY_NONE 			0
#define UART_PARITY_ODD 			1
#define UART_PARITY_EVEN 			2

/// @brief Stop bits
#define UART_STOPBITS_1 			1

#define UART_FLOWCTL_ON  			1
#define UART_FLOWCTL_OFF 			0

#define UART_UART_2  				2
#define UART_UART_3  				3


#define UART_PRINT_BUF_SIZE			(3072 + 512 )
#define UART_CONF_NAME				"UART_CONFIG"

#define DEFAULT_UART_BAUD			UART_BAUDRATE_115200
#define UART_BITPERCHAR				UART_DATABITS_8
#define UART_PARITY					UART_PARITY_NONE
#define UART_STOPBIT				UART_STOPBITS_1
#define UART_FLOW_CTRL				UART_FLOWCTL_OFF


typedef  enum {
	UART_UNIT_1 = 1,
	UART_UNIT_2 ,
	UART_UNIT_3 ,
	UART_UNIT_MAX
} uart_idx_t;

/// @brief UART configuration
typedef struct _uart_info {
	unsigned int		baud;
	unsigned int		bits;
	unsigned int		parity;
	unsigned int		stopbit;
	unsigned int		flow_ctrl;
} uart_info_t;


// Extern global functions
extern int ra6w1_vsnprintf(char *buf, size_t n, int linefeed, const char *fmt, va_list args);

// Extern global variables
extern uart_info_t g_UART2_config_info;
extern uart_info_t g_UART3_config_info;

extern UCHAR	atcmd_on_uart2_flag;
extern UCHAR	atcmd_on_uart3_flag;


/**
 ****************************************************************************************
 * @brief  Initialize UART device
 * @param[in] uart_idx		Index value of UART interface (UART_UNIT_2, UART_UNIT_3)
 * @return TRUE if succeed to process, others else
 ****************************************************************************************
 */
int 	UART_init(int uart_idx);

/**
 ****************************************************************************************
 * @brief  Deinitialize UART device
 * @param[in] uart_idx		Index value of UART interface (UART_UNIT_2, UART_UNIT_3)
 * @return TRUE if succeed to process, others else
 ****************************************************************************************
 */
int 	UART_deinit(int uart_idx);

/**
 ****************************************************************************************
 * @brief  Get handler of UART device
 * @param[in] uart_idx		Index value of UART interface (UART_UNIT_2, UART_UNIT_3)
 * @return Handler of UART
 ****************************************************************************************
 */
HANDLE 	get_UART_handler(int uart_idx);


/**
 ****************************************************************************************
 * @brief  Set UART configuration
 * @param[in] uart_idx			Index value of UART interface (UART_UNIT_2, UART_UNIT_3)
 * @param[in] *uart_conf_info	Buffer pointer of configuration data
 * @param[in] atcmd_flag		AT-CMD flag
 * @return TX_SUCCESS if succeed to process, others else
 ****************************************************************************************
 */
int 	set_user_UART_conf(uart_idx_t uart_idx, uart_info_t *uart_conf_info, char atcmd_flag);

/**
 ****************************************************************************************
 * @brief  Check RX buffer
 * @param[in] uart_idx	Index value of UART interface (UART_UNIT_2, UART_UNIT_3)
 * @return				TRUE if empty, others FALSE
 ****************************************************************************************
 */
int 	is_UART_rx_empty(uart_idx_t uart_idx);

/**
 ****************************************************************************************
 * @brief  Get character from UART device
 * @param[in] uart_idx	Index value of UART interface (UART_UNIT_2, UART_UNIT_3)
 * @param[in] mode		Read waiting mode
 * @return				Read_character
 ****************************************************************************************
 */
int  	getchar_UART(uart_idx_t uart_idx, unsigned long mode);

/**
 ****************************************************************************************
 * @brief  Put character string to UART device
 * @param[in] uart_idx	Index value of UART interface (UART_UNIT_2, UART_UNIT_3)
 * @param[in] *data		Text string to write
 * @param[in] len		Write length
 * @return				None
 ****************************************************************************************
 */
void	puts_UART(uart_idx_t uart_idx, char *data, int data_len);

/**
 ****************************************************************************************
 * @brief  Print-out character string to UART device
 * @param[in] uart_idx	Index value of UART interface (UART_UNIT_2, UART_UNIT_3)
 * @param[in] *fmt		Print-out string format
 * @return				None
 ****************************************************************************************
 */
void	PRINTF_UART(uart_idx_t uart_idx, const char *fmt,...);

/////////////////////////////////////////////////////////////////////////////////////////

/**
 ****************************************************************************************
 * @brief  Get the current uart config info
 * @param[in] uart_idx	Index value of UART interface (UART_UNIT_2, UART_UNIT_3)
 * @return uart config info				
 ****************************************************************************************
 */
uart_info_t* uart_config_get(uart_idx_t uart_idx);


/**
 ****************************************************************************************
 * @brief  Load (from NVRAM or Retention - preconfigured) and Get the uart info
 * @param[in] uart_idx	Index value of UART interface (UART_UNIT_2, UART_UNIT_3)
 * @return uart config info, this is used to set driver, see uart_drv_config_set()
 ****************************************************************************************
 */
uart_info_t* uart_config_load(uart_idx_t uart_idx);

/**
 ****************************************************************************************
 * @brief  Set uart driver with user configuration
 * @param[in] uart_idx	Index value of UART interface (UART_UNIT_2, UART_UNIT_3)
 * @param[in] uart_conf_info uart info user configured
 * @return pdTRUE for Success, pdFALSE for failure
 ****************************************************************************************
 */
int          uart_drv_config_set(uart_idx_t uart_idx, uart_info_t* uart_conf_info);

/**
 ****************************************************************************************
 * @brief  Open (initialize) uart with uart config currently configured.
 * @param[in] uart_idx	Index value of UART interface (UART_UNIT_2, UART_UNIT_3)
 * @return pdTRUE for Success, pdFALSE for failure.
 ****************************************************************************************
 */
int          uart_open(uart_idx_t uart_idx);


#endif	// !__COMMON_UART_H__

/* EOF */
