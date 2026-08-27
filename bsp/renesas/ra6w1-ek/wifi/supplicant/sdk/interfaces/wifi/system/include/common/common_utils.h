/**
 ****************************************************************************************
 *
 * @file common_util.h
 *
 * @brief Define for System common
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


#ifndef	__COMMON_UTILS_H__
#define	__COMMON_UTILS_H__


/* ANSI CODE */
#ifndef ESCCODE
  #define ESCCODE 						"\33"
#endif

/* Set  Attributes */
#ifndef ANSI_BOLD
  #define ANSI_BOLD 					"\33[1m"
#endif
#ifndef ANSI_UNDERLINE
  #define ANSI_UNDERLINE 				"\33[4m"
#endif
#ifndef ANSI_BLINK
  #define ANSI_BLINK 					"\33[5m"
#endif
#ifndef ANSI_REVERSE
  #define ANSI_REVERSE 				"\33[7m"
#endif

#ifndef ANSI_R_BOLD
  #define ANSI_R_BOLD 					"\33[21m"
#endif
#ifndef ANSI_R_UNDERLINE
  #define ANSI_R_UNDERLINE 			"\33[24m"
#endif
#ifndef ANSI_R_BLINK
  #define ANSI_R_BLINK 				"\33[25m"
#endif
#ifndef ANSI_R_REVERSE
  #define ANSI_R_REVERSE 				"\33[27m"
#endif

/* Rset  Attributes */
#ifndef ANSI_NORMAL
  #define ANSI_NORMAL 					"\33[0m"
#endif
#ifndef ANSI_RESET_BOLD
  #define ANSI_RESET_BOLD 				"\33[21m"
#endif
#ifndef ANSI_RESET_UNDERLINE
  #define ANSI_RESET_UNDERLINE 		"\33[24m"
#endif
#ifndef ANSI_RESET_BLINK
  #define ANSI_RESET_BLINK 			"\33[25m"
#endif
#ifndef ANSI_RESET_REVERSE
  #define ANSI_RESET_REVERSE 			"\33[27m"
#endif

/* Control */
#ifndef ANSI_CLEAR
  #define ANSI_CLEAR 					"\33[2J"
#endif
#ifndef ANSI_CURON
  #define ANSI_CURON 					"\33[?25h"
#endif
#ifndef ANSI_CUROFF
  #define ANSI_CUROFF 					"\33[?25l"
#endif
#ifndef ANSI_BELL
  #define ANSI_BELL 					"\7"
#endif
#ifndef ANSI_ERASE
  #define ANSI_ERASE 					"\33[0J"
#endif
#ifndef ANSI_LEFT
  #define ANSI_LEFT           		  	"\33[1D"
#endif
#ifndef ANSI_RIGHT
  #define ANSI_RIGHT          		  	"\33[1C"
#endif

/* Foreground Color (text) */
#ifndef ANSI_COLOR_BLACK
  #define ANSI_COLOR_BLACK 			"\33[30m"
#endif
#ifndef ANSI_COLOR_RED
  #define ANSI_COLOR_RED 				"\33[31m"
#endif
#ifndef ANSI_COLOR_GREEN
  #define ANSI_COLOR_GREEN 			"\33[32m"
#endif
#ifndef ANSI_COLOR_YELLOW
  #define ANSI_COLOR_YELLOW 			"\33[33m"
#endif
#ifndef ANSI_COLOR_BLUE
  #define ANSI_COLOR_BLUE 				"\33[34m"
#endif
#ifndef ANSI_COLOR_MAGENTA
  #define ANSI_COLOR_MAGENTA 			"\33[35m"
#endif
#ifndef ANSI_COLOR_CYAN
  #define ANSI_COLOR_CYAN 				"\33[36m"
#endif
#ifndef ANSI_COLOR_WHITE
  #define ANSI_COLOR_WHITE 			"\33[37m"
#endif
#ifndef ANSI_COLOR_LIGHT_RED
  #define ANSI_COLOR_LIGHT_RED 		"\33[1;31m"
#endif
#ifndef ANSI_COLOR_LIGHT_GREEN
  #define ANSI_COLOR_LIGHT_GREEN 		"\33[1;32m"
#endif
#ifndef ANSI_COLOR_LIGHT_YELLOW
  #define ANSI_COLOR_LIGHT_YELLOW 		"\33[1;33m"
#endif
#ifndef ANSI_COLOR_LIGHT_BLUE
  #define ANSI_COLOR_LIGHT_BLUE 		"\33[1;34m"
#endif
#ifndef ANSI_COLOR_LIGHT_MAGENTA
  #define ANSI_COLOR_LIGHT_MAGENTA 	"\33[1;35m"
#endif
#ifndef ANSI_COLOR_LIGHT_CYAN
  #define ANSI_COLOR_LIGHT_CYAN 		"\33[1;36m"
#endif
#ifndef ANSI_COLOR_LIGHT_WHITE
  #define ANSI_COLOR_LIGHT_WHITE 		"\33[1;37m"
#endif
#ifndef ANSI_COLOR_DEFULT
  #define ANSI_COLOR_DEFULT 			"\33[0;39m"
#endif

/* Background Color */
#ifndef ANSI_BCOLOR_BLACK
  #define ANSI_BCOLOR_BLACK 			"\33[40m"
#endif
#ifndef ANSI_BCOLOR_RED
  #define ANSI_BCOLOR_RED 				"\33[41m"
#endif
#ifndef ANSI_BCOLOR_GREEN
  #define ANSI_BCOLOR_GREEN 			"\33[42m"
#endif
#ifndef ANSI_BCOLOR_YELLOW
  #define ANSI_BCOLOR_YELLOW 			"\33[43m"
#endif
#ifndef ANSI_BCOLOR_BLUE
  #define ANSI_BCOLOR_BLUE 			"\33[44m"
#endif
#ifndef ANSI_BCOLOR_MAGENTA
  #define ANSI_BCOLOR_MAGENTA 			"\33[45m"
#endif
#ifndef ANSI_BCOLOR_CYAN
  #define ANSI_BCOLOR_CYAN 			"\33[46m"
#endif
#ifndef ANSI_BCOLOR_WHITE
  #define ANSI_BCOLOR_WHITE 			"\33[47m"
#endif
#ifndef ANSI_BCOLOR_LIGHT_RED
  #define ANSI_BCOLOR_LIGHT_RED 		"\33[1;41m"
#endif
#ifndef ANSI_BCOLOR_LIGHT_GREEN
  #define ANSI_BCOLOR_LIGHT_GREEN 		"\33[1;42m"
#endif
#ifndef ANSI_BCOLOR_LIGHT_YELLOW
  #define ANSI_BCOLOR_LIGHT_YELLOW 	"\33[1;43m"
#endif
#ifndef ANSI_BCOLOR_LIGHT_BLUE
  #define ANSI_BCOLOR_LIGHT_BLUE 		"\33[1;44m"
#endif
#ifndef ANSI_BCOLOR_LIGHT_MAGENTA
  #define ANSI_BCOLOR_LIGHT_MAGENTA 	"\33[1;45m"
#endif
#ifndef ANSI_BCOLOR_LIGHT_CYAN
  #define ANSI_BCOLOR_LIGHT_CYAN 		"\33[1;46m"
#endif
#ifndef ANSI_BCOLOR_LIGHT_WHITE
  #define ANSI_BCOLOR_LIGHT_WHITE 		"\33[1:47m"
#endif

#ifndef ANSI_BCOLOR_DEFULT
  #define ANSI_BCOLOR_DEFULT 			"\33[0;49m"
#endif



#ifndef VT_CLEAR
  #define VT_CLEAR 					printf("\33[2J")
#endif
#ifndef VT_CURPOS
  #define VT_CURPOS(X,Y) 				printf("\33[%d;%dH", Y, X)
#endif
#ifndef VT_NORMAL
  #define VT_NORMAL 					printf("\33[0m")
#endif
#ifndef VT_BOLD
  #define VT_BOLD 						printf("\33[1m")
#endif
#ifndef VT_BLINK
  #define VT_BLINK 					printf("\33[5m")
#endif
#ifndef VT_REVERSE
  #define VT_REVERSE 					printf("\33[7m")
#endif
#ifndef VT_CURON
  #define VT_CURON 					printf("\33[?25h")
#endif
#ifndef VT_CUROFF
  #define VT_CUROFF 					printf("\33[?25l")
#endif
#ifndef VT_BELL
  #define VT_BELL 						printf("\007")
#endif
#ifndef VT_ERASE
  #define VT_ERASE 					printf("\33[0J")
#endif
#ifndef VT_LEFT
  #define VT_LEFT 						printf("\33[1D")
#endif
#ifndef VT_RIGHT
  #define VT_RIGHT 					printf("\33[1C")
#endif
#ifndef VT_LINECLEAR
  #define VT_LINECLEAR(X) 				VT_CURPOS(1, X); printf("\33[2K")
#endif
#ifndef VT_COLORBLACK
  #define VT_COLORBLACK 				printf("\33[30m")
#endif
#ifndef VT_COLORRED
  #define VT_COLORRED 					printf("\33[31m")
#endif
#ifndef VT_COLORGREEN
  #define VT_COLORGREEN 				printf("\33[32m")
#endif
#ifndef VT_COLORYELLOW
  #define VT_COLORYELLOW 				printf("\33[33m")
#endif
#ifndef VT_COLORBLUE
  #define VT_COLORBLUE 				printf("\33[34m")
#endif
#ifndef VT_COLORMAGENTA
  #define VT_COLORMAGENTA 				printf("\33[35m")
#endif
#ifndef VT_COLORCYAN
  #define VT_COLORCYAN 				printf("\33[36m")
#endif
#ifndef VT_COLORWHITE
  #define VT_COLORWHITE 				printf("\33[37m")
#endif
#ifndef VT_COLORDEFULT
  #define VT_COLORDEFULT 				printf("\33[39m")
#endif
#ifndef VT_BCOLORBLACK
  #define VT_BCOLORBLACK 				printf("\33[40m")
#endif
#ifndef VT_BCOLORRED
  #define VT_BCOLORRED 				printf("\33[41m")
#endif
#ifndef VT_BCOLORGREEN
  #define VT_BCOLORGREEN 				printf("\33[42m")
#endif
#ifndef VT_BCOLORYELLOW
  #define VT_BCOLORYELLOW 				printf("\33[43m")
#endif
#ifndef VT_BCOLORBLUE
  #define VT_BCOLORBLUE 				printf("\33[44m")
#endif
#ifndef VT_BCOLORMAGENTA
  #define VT_BCOLORMAGENTA 			printf("\33[45m")
#endif
#ifndef VT_BCOLORCYAN
  #define VT_BCOLORCYAN 				printf("\33[46m")
#endif
#ifndef VT_BCOLORWHITE
  #define VT_BCOLORWHITE 				printf("\33[47m")
#endif
#ifndef VT_BDEFULT
  #define VT_BDEFULT 					printf("\33[49m")
#endif
#ifndef VT_COLOROFF
  #define VT_COLOROFF 					VT_BOLD;VT_BCOLORBLACK;VT_COLORWHITE
#endif


//ISDIGIT onderzoekt of een karakter numeric is
#ifndef ISDIGIT
  #define ISDIGIT(c)(  (c < '0' || c > '9') ? 0 : 1)
#endif

void pntdumpbin ( unsigned char* pbyBin, int nLen, int simple);

/**
 ****************************************************************************************
 * @brief      wpa_cli for RRQ61000 wpa_supplicant
 * @param[in]  cmdline    Input wpa_cli command string.
 * @param[in]  delimit    Command string delimiter
 * @param[in]  cli_reply  Result reply string buffer
 * @return     0 on success, others on fail
 ****************************************************************************************
 */
int  ra6w1_cli_reply(char *cmdline, char *delimit, char *cli_reply);

int  factory_reset(int reboot_flag);
long subnetRangeLastIP(long ip, long subnet);
UINT writeDataToFlash(UINT *destFlashAddr, UINT *srcMemAddr, UINT size);
#endif	/* __COMMON_UTILS_H__ */

/* EOF */
