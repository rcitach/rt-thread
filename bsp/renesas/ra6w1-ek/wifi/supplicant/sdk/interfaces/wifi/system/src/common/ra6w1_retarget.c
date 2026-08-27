/**
 ****************************************************************************************
 *
 * @file ra6w1_retarget.c
 *
 * @brief Print formatting routines
 *
 * Copyright (C) 2002 Michael Ringgaard. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 ****************************************************************************************
 */

 #include "bsp_api.h"
 
#ifdef RM_STDIO_W
#include "rm_stdio_w_cfg.h" 
#endif
#ifdef CONFIG_RA6W1_PRINTF

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>

#include "osal.h"


/******************************************************************************
 *
 *  ra6w1_vsnprintf
 *
 ******************************************************************************/

#undef  SUPPORT_RA6WX_NOFLOAT
#define SUPPORT_RA6WX_LONGLONG

#ifdef  SUPPORT_RA6WX_LONGLONG
#define ra6wx_num_type   long long
#else   //SUPPORT_RA6WX_LONGLONG
#define ra6wx_num_type   long
#endif  //SUPPORT_RA6WX_LONGLONG

#define SW_UNUSED_ARG(x)     (void)x

#define LARGE   64              // Use 'ABCDEF' instead of 'abcdef'
#define SPECIAL 32              // 0x
#define LEFT    16              // Left justified
#define SPACE   8               // Space if plus
#define PLUS    4               // Show plus
#define SIGN    2               // Unsigned/signed long
#define ZEROPAD 1               // Pad with zero

#define EPSILON (1e-6)

//--------------------------------------------------------------------
//
//--------------------------------------------------------------------

#define is_digit(c) ((c) >= '0' && (c) <= '9')

int ra6w1_vsnprintf(char *buf, size_t n, int linefeed,  const char *fmt, va_list args);
int ra6w1_vasprintf (char **buf, const char *fmt, va_list args);
int ra6w1_asprintf (char **buf, const char *fmt, ...);

static const char g_digits[] =
    "0123456789"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

static int ra6wx_toupper(int c)
{
        #define islower(c)     (c >= (int)'a' && c<= (int)'z')
        return islower(c) ? (c ^ 0x20) : c;
}

static size_t ra6wx_strnlen(const char *s, size_t count)
{
  const char *sc;
  for (sc = s; *sc != '\0' && count--; ++sc)
        ;  // empty loop body
  return (size_t)(sc - s);
}

static int skip_atoi(const char **s)
{
        int i = 0;
        while (is_digit(**s)) {
                i = i*10 + *((*s)++) - '0';
        }
        return i;
}

static char *number(char *str, ra6wx_num_type num, int base, int size, int precision, int type)
{
        int i;
        char sign;
        char *dig = (char *)g_digits;
        char tmp[68];

        if (type & LARGE)  dig = (char *)&(g_digits[36]); //upper_digits
        if (type & LEFT) type &= ~ZEROPAD;
        if ((2 > base) || (36 < base)) return 0;

        sign = 0;
        if (type & SIGN) {
                if (num < 0) {
                        sign = '-';
                        num = -num;
                        size--;
                }
                else if (type & PLUS) {
                        sign = '+';
                        size--;
                }
                else if (type & SPACE) {
                        sign = ' ';
                        size--;
                }
        }

        if (type & SPECIAL) {
                if (base == 16){
                        size -= 2;
                }
                else if (base == 8) {
                        size--;
                }
        }

        i = 0;

        if (num == 0){
                tmp[i++] = '0';
        } else {
                while (num != 0) {
                        tmp[i++] = dig[((unsigned long long) num) % (unsigned) base];
                        num = (ra6wx_num_type)(((unsigned long long) num) / (unsigned) base);
                }
        }

        if (i > precision){
                precision = i;
        }
        size -= precision;
        if (!(type & (ZEROPAD | LEFT))){
                while (size-- > 0) *str++ = ' ';
        }
        if (sign){
                *str++ = sign;
        }

        if (type & SPECIAL){
                if (base == 8){
                        *str++ = '0';
                }
                else if (base == 16) {
                        *str++ = '0';
                        *str++ = g_digits[33];
                }
        }

        if (!(type & LEFT)) {
                if ((type & ZEROPAD) == ZEROPAD ){
                        while (size-- > 0) *str++ = '0';
                }else{
                        while (size-- > 0) *str++ = ' ';
                }
        }
        while (i < precision--) *str++ = '0';
        while (i-- > 0) *str++ = tmp[i];
        while (size-- > 0) *str++ = ' ';

        return str;
}

static char *eaddr(char *str, unsigned char *addr, int size, int precision, int type)
{
        SW_UNUSED_ARG(precision);

        char tmp[24];
        char *dig = (char *)g_digits;
        int i, len;

        if (type & LARGE){
                dig = (char *)&(g_digits[36]); //upper_digits
        }
        len = 0;
        for (i = 0; i < 6; i++) {
                if (i != 0) {
                        tmp[len++] = ':';
                }
                tmp[len++] = dig[addr[i] >> 4];
                tmp[len++] = dig[addr[i] & 0x0F];
        }

        if (!(type & LEFT)){
                while (len < size--) *str++ = ' ';
        }
        for (i = 0; i < len; ++i) {
                *str++ = tmp[i];
        }
        while (len < size--) *str++ = ' ';

        return str;
}

static char *iaddr(char *str, unsigned char *addr, int size, int precision, int type)
{
        SW_UNUSED_ARG(precision);

        char tmp[24];
        int i, n, len;

        len = 0;
        for (i = 0; i < 4; i++) {
                if (i != 0) {
                        tmp[len++] = '.';
                }
                n = addr[i];

                if (n == 0) {
                        tmp[len++] = g_digits[0];
                } else  {
                        if (n >= 100) {
                                tmp[len++] = g_digits[n / 100];
                                n = n % 100;
                                tmp[len++] = g_digits[n / 10];
                                n = n % 10;
                        }
                        else if (n >= 10) {
                                tmp[len++] = g_digits[n / 10];
                                n = n % 10;
                        }
                        tmp[len++] = g_digits[n];
                }
        }

        if (!(type & LEFT)) {
                while (len < size--) *str++ = ' ';
        }
        for (i = 0; i < len; ++i) {
                *str++ = tmp[i];
        }
        while (len < size--) *str++ = ' ';

        return str;
}

#ifndef SUPPORT_RA6WX_NOFLOAT

static void ra6wx_memcpy(void *dst, void *src, size_t size)
{
        char    *src8, *dst8;

        src8 = (char *)src;
        dst8 = (char *)dst;

        while(size-- > 0 ){
                *dst8++ = *src8++ ;
        }
}

char *fcvtbuf(double arg, int ndigits, int *decpt, int *sign, char *buf);
char *ecvtbuf(double arg, int ndigits, int *decpt, int *sign, char *buf);

static void cfltcvt(double value, char *buffer, char fmt, int precision)
{
        int decimal_pos, is_negative, exponent_val, idx;
        int magnitude;
        int upper_exp = 0;
        char cvtbuf[80];
        char *digits = NULL;

        if (fmt == 'E' || fmt == 'G') {
                upper_exp = 1;
                fmt += 'a' - 'A';
        }

        if (fmt == 'g') {
                digits = ecvtbuf(value, precision, &decimal_pos, &is_negative, cvtbuf);
                magnitude = decimal_pos - 1;
                if ((magnitude > precision - 1) || (magnitude < -4)) {
                        fmt = 'e';
                       precision = precision - 1;
                } else {
                        fmt = 'f';
                        precision -= decimal_pos;
                }
        }

        if (fmt == 'e') {
                digits = ecvtbuf(value, precision + 1, &decimal_pos, &is_negative, cvtbuf);

                if (is_negative) *buffer++ = '-';
                *buffer++ = *digits;
                if (precision > 0) *buffer++ = '.';

                ra6wx_memcpy(buffer, digits + 1, (size_t)precision);
                buffer += precision;
                *buffer++ = upper_exp ? 'E' : 'e';

                if (decimal_pos == 0) {
                        exponent_val = ((fabs(value - 0.0) < EPSILON)) ? 0 : -1;
                } else {
                        exponent_val = decimal_pos - 1;
                }

                int abs_exp;
                if (0 > exponent_val) {
                        *buffer++ = '-';
                        abs_exp = -exponent_val;
                } else {
                        *buffer++ = '+';
                        abs_exp = exponent_val;
                }

                buffer[2] = (char)((abs_exp % 10) + '0');
                exponent_val = abs_exp / 10;
                buffer[1] = (char)((exponent_val % 10) + '0');
                exponent_val = exponent_val / 10;
                buffer[0] = (char)((exponent_val % 10) + '0');
                buffer += 3;
        }
        else if (fmt == 'f') {
                digits = fcvtbuf(value, precision, &decimal_pos, &is_negative, cvtbuf);
                if (is_negative) { *buffer++ = '-'; }

                if (*digits) {
                        if ( 0 >= decimal_pos )
                        {
                                *buffer++ = '0';
                                *buffer++ = '.';
                                for (idx = 0; idx < -decimal_pos; idx++) {
                                        *buffer++ = '0';
                                }
                                while (*digits) *buffer++ = *digits++;
                        }
                        else
                        {
                                idx = 0;
                                while (*digits) {
                                  if (idx++ == decimal_pos) { *buffer++ = '.'; }
                                  *buffer++ = *digits++;
                                }
                        }
                }
                else
                {
                        *buffer++ = '0';
                        if (0 < precision) {
                                *buffer++ = '.';
                                for (idx = 0; idx < precision; idx++) {
                                        *buffer++ = '0';
                                }
                        }
                }
        }

        *buffer = '\0';  // terminate string
}

static void forcdecpt(char *buffer)
{
        while (*buffer) {
                if (*buffer == '.') {
                        return;
                }
                if (*buffer == 'e' || *buffer == 'E') {
                        break;
                }
                buffer++;
        }

        if (*buffer) {
                int len = (int)strlen(buffer);
                while (len > 0) {
                        buffer[len + 1] = buffer[len];
                        len--;
                }
                *buffer = '.';
        } else {
                *buffer++ = '.';
                *buffer = '\0';
        }
}

static void cropzeros(char *buffer)
{
        char *endPtr;

        // Find the decimal point
        while (*buffer && *buffer != '.') {
                buffer++;
        }

        if (*buffer++) {

                // Look for 'e' or 'E' if present
                while (*buffer && *buffer != 'e' && *buffer != 'E') {
                        buffer++;
                }

                endPtr = buffer - 1;

                // Move backward to remove trailing zeros
                while (*buffer == '0')
                        buffer--;

                // If only a dot remains, remove it as well
                if (*buffer == '.') {
                        buffer--;
                }

                while ((*++buffer = *endPtr++) != 0)
                        ;
        }
}

static char *flt(char *str, double num, int size, int precision, char fmt, int flags)
{
        int i, n;
        char sign, c;
        char tmp[80];

        // Left align means no zero padding
        if (flags & LEFT) flags &= ~ZEROPAD;

        // Determine padding and sign char
        c = (flags & ZEROPAD) ? '0' : ' ';
        sign = 0;
        if (flags & SIGN) {
                if (0.0 > num)  {
                        sign = '-';
                        num = -num;
                        size--;
                }
                else if (flags & PLUS)  {
                        sign = '+';
                        size--;
                }
                else if (flags & SPACE) {
                        sign = ' ';
                        size--;
                }
        }

        // Compute the precision value
        if (0 > precision) {
                precision = 6; // Default precision: 6
        }
        else if (precision == 0 && fmt == 'g') {
                precision = 1; // ANSI specified
        }

        // Convert floating point number to text
        cfltcvt(num, tmp, fmt, precision);

        // '#' and precision == 0 means force a decimal point
        if ((flags & SPECIAL) && precision == 0) {
                forcdecpt(tmp);
        }

        // 'g' format means crop zero unless '#' given
        if (fmt == 'g' && !(flags & SPECIAL)) {
                cropzeros(tmp);
        }

        n = (int)strlen(tmp);

        // Output number with alignment and padding
        size -= n;
        if (!(flags & (ZEROPAD | LEFT))) {
                while (size-- > 0) *str++ = ' ';
        }
        if (sign) {
                *str++ = sign;
        }
        if (!(flags & LEFT)) {
                while (size-- > 0) *str++ = c;
        }
        for (i = 0; i < n; i++) {
                *str++ = tmp[i];
        }
        while (size-- > 0) *str++ = ' ';

        return str;
}

#endif

static char *ra6w1_string(char *buf, size_t ssize, char *s, int field_width, int precision, int flags, int linefeed)
{
        int len, i;
        if (s == 0)
        {
                //s = "<NULL>";
                s = "";
        }

        len = (int)ra6wx_strnlen(s, (size_t)precision);
        ssize = ssize - 1;

        if (!(flags & LEFT))
        {
                while ((len < field_width--) && ssize)
                {
                        *buf++ = ' ';
                        ssize--;
                }
        }

        for (i = 0; (i < len) && ssize; ++i)
        {
                if (linefeed == 1 && *s == '\n' && (*(buf-1) != '\r'))
                {
                        *buf++ = '\r';
                        ssize--;
                        *buf++ = *s++;
                        ssize--;
                }
                else
                {
                        if (flags & LARGE)
                        {
                                *buf++ = (char)ra6wx_toupper(*s++);
                        }
                        else
                        {
                                *buf++ = *s++;
                        }
                        ssize--;
                }
        }

        while ((len < field_width--) && ssize)
        {
                *buf++ = ' ';
                ssize--;
        }

        if (ssize == 0 )
        {
                *(buf-1) = '.';
                *(buf-2) = '.';
                *(buf-3) = '.';
        }

        return buf;
}

/******************************************************************************
 *  ra6w1_vsnprintf
 *
 *  Purpose :   vsnprintf
 *  Input   :   n, maximum number of bytes to be used in the buffer
 *              fmt, C string that contains a format string
 *              args, values identifying a variable arguments list initialized
 *                    with va_start.
 *  Output  :   buf, pointer to a buffer where the resulting C string is stored.
 *  Return  :   string length
 ******************************************************************************/

int ra6w1_vsnprintf(char *buf, size_t n, int linefeed,  const char *fmt, va_list args)
{
        ra6wx_num_type num;
        char *str;
        char *endstr;   /* end of string */
        char *s;        /* temporary */
        int num_base;
        int flags;      // Flags to number()
        int field_width;// Width of output field
        int precision;  // Min. # of digits for integers;
                        // max number of chars for from string
        int qualifier;  // 'h', 'l', or 'L' for integer fields
        int truncate;   /* temporary */
        int tmp_len;        /* temporary */

        if (n < 1 ){
                *buf = '\0';
                return 0;
        }

        endstr = (char *)((uint32_t)buf + n - 1);

        for (str = buf; *fmt; fmt++)
        {
                if (str == endstr )
                {
                        break;
                }

                if (*fmt != '%')
                {
                        if (linefeed == 0 ){
                                do {
                                        if (str == endstr ){  break;  }
                                        if (*fmt == '\0' ){ break; }
                                        *str++ = *fmt++;
                                } while(*fmt != '%');
                        }
                        else
                        {
                                do {
                                        if (str == endstr)
                                        {
                                                break;
                                        }

                                        if (*fmt == '\0')
                                        {
                                                break;
                                        }

                                        if (*fmt == '\n' && (str == buf || *(str-1) != '\r'))
                                        {
                                                *str++ = '\r';

                                                if (str == endstr)
                                                {
                                                        break;
                                                }
                                        }
                                        *str++ = *fmt++;

                                } while(*fmt != '%');
                        }

                        if (*fmt != '%' ){
                                fmt--;
                                continue;
                        }
                }

                // Process flags
                flags = 0;
                truncate = 1; /* temporary flag */

                do{
                        fmt++; // This also skips first '%'
                        switch (*fmt)
                        {
                        case '-': flags |= LEFT; break;
                        case '+': flags |= PLUS; break;
                        case ' ': flags |= SPACE; break;
                        case '#': flags |= SPECIAL; break;
                        case '0': flags |= ZEROPAD; break;
                        default : truncate = 0; break;
                        }
                }while(truncate==1);

                // Get field width
                field_width = -1;
                if (is_digit(*fmt)) {
                        field_width = skip_atoi(&fmt);
                }
                else if (*fmt == '*') {
                        fmt++;
                        field_width = va_arg(args, int);
                        if (field_width < 0) {
                                field_width = -field_width;
                                flags |= LEFT;
                        }
                }

                // Get the precision
                precision = -1;
                if (*fmt == '.')
                {
                        ++fmt;
                        if (is_digit(*fmt)) {
                                precision = skip_atoi(&fmt);
                        }
                        else if (*fmt == '*') {
                                ++fmt;
                                precision = va_arg(args, int);
                        }
                        if (precision < 0) {
                                precision = 0;
                        }
                }

                // Get the conversion qualifier
                qualifier = -1;
                if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L') {
                        qualifier = *fmt;
                        fmt++;
#ifdef  SUPPORT_RA6WX_LONGLONG
                        if (*fmt == 'l' ){
                                qualifier = 'L';
                                fmt++;
                        }
#endif  //SUPPORT_RA6WX_LONGLONG
                }

                // Default base
                num_base = 10;

                switch (*fmt)
                {
                        case 'c':
                        if (!(flags & LEFT)) {
                                while (--field_width > 0) *str++ = ' ';
                        }
                        *str++ = (char) va_arg(args, int);
                        while (--field_width > 0) *str++ = ' ';
                        continue;

                        case 's': /* case sensitive */
                        s = va_arg(args, char *);

                        tmp_len = ((int)(endstr - str) - field_width);
                        if (tmp_len < 0 ) tmp_len = 0;

                        str = ra6w1_string(str, (size_t)tmp_len, s, field_width, precision, flags, linefeed);
                        continue;

                        case 'S': /* upper, case insensitive */
                        flags |= LARGE;
                        s = va_arg(args, char *);

                        tmp_len = ((int)(endstr - str) - field_width);
                        if (tmp_len < 0 ) tmp_len = 0;

                        str = ra6w1_string(str, (size_t)tmp_len, s, field_width, precision, flags, linefeed);
                        continue;

                        case 'p':
                        if (field_width == -1)  {
                                flags |= ZEROPAD;
                                field_width = 2 * sizeof(void *);
                        }
                        str = number(str, (unsigned long) va_arg(args, void *), 16, field_width, precision, flags);
                        continue;

                        case 'n':
                        if (qualifier == 'l') {
                                long *ip = va_arg(args, long *);
                                *ip = (str - buf);
                        } else  {
                                int *ip = va_arg(args, int *);
                                *ip = (str - buf);
                        }
                        continue;

                        case 'a':
                        if (qualifier == 'l') 
                        {
                                str = eaddr(str, va_arg(args, unsigned char *), field_width, precision, flags);
                        } 
                        else 
                        {
                                str = iaddr(str, va_arg(args, unsigned char *), field_width, precision, flags);
                        }
                        continue;

                        case 'A':
                        flags |= LARGE;
                        if (qualifier == 'l') 
                        {
                                str = eaddr(str, va_arg(args, unsigned char *), field_width, precision, flags);
                        } 
                        else 
                        {
                                str = iaddr(str, va_arg(args, unsigned char *), field_width, precision, flags);
                        }
                        continue;

                        case 'x': num_base = 16; break;

                        case 'X':
                        flags |= LARGE;
                        num_base = 16;
                        break;

                        case 'o': num_base = 8; break;

                        case 'd':
                        case 'i':
                        flags |= SIGN;
                        break;

                        case 'u':
                        break;

#ifndef SUPPORT_RA6WX_NOFLOAT
                        case 'e':
                        case 'E':
                        case 'f':
                        case 'g':
                        case 'G':
                        str = flt(str, va_arg(args, double)
                                , field_width, precision, *fmt, flags | SIGN);
                        continue;

#endif
                        default:
                        if (*fmt != '%') {
                                *str++ = '%';
                        }
                        if (*fmt) {
                                *str++ = *fmt;
                        } else {
                                --fmt;
                        }
                        continue;
                }

#ifdef  SUPPORT_RA6WX_LONGLONG
                if (qualifier == 'L') {
                        num = (ra6wx_num_type)va_arg(args, unsigned long long);
                }else
#endif  //SUPPORT_RA6WX_LONGLONG
                if (qualifier == 'l') {
                        num = va_arg(args, unsigned long);
                }
                else if (qualifier == 'h') {
                        if (flags & SIGN) {
                                num = (ra6wx_num_type)va_arg(args, int);
                        } else {
                                num = (ra6wx_num_type)va_arg(args, unsigned int);
                        }
                }
                else if (flags & SIGN) {
                        num = (ra6wx_num_type)va_arg(args, int);
                } else {
                        num = (ra6wx_num_type)va_arg(args, unsigned int);
                }

                str = number(str, (ra6wx_num_type)num, num_base, field_width, precision, flags);

        }

        *str = '\0';
        return ( str - buf );
}

/******************************************************************************
 *  ra6w1_vasprintf
 *
 *  Purpose :   vasprintf
 *  Input   :   fmt, C string that contains a format string
 *              ..., additional arguments.
 *  Output  :   buf, pointer to a buffer where the resulting C string is stored.
 *  Return  :   string length
 ******************************************************************************/

int ra6w1_vasprintf (char **buf, const char *fmt, va_list args) {
	int len = 0;
	int tmp_buf_size=256;
	char tmp_buf[tmp_buf_size];


	len = ra6w1_vsnprintf(tmp_buf, (size_t) tmp_buf_size, 0, fmt, args);

	if (len < 0) {
		return -1;
	}

	*buf = pvPortMalloc((size_t) (len + 1));

	if (NULL==*buf) {
		return -1;
	}

	len = ra6w1_vsnprintf(*buf, 1024*4, 0, fmt, args);

	return len;
}

/******************************************************************************
 *  ra6w1_asprintf
 *
 *  Purpose :  asprintf
 *  Input   :   fmt, C string that contains a format string
 *              ..., additional arguments.
 *  Output  :   buf, pointer to a buffer where the resulting C string is stored.
 *  Return  :   string length
 ******************************************************************************/

int ra6w1_asprintf (char **buf, const char *fmt, ...) {
  va_list args;
  int len = 0;

  va_start(args, fmt);
  len = ra6w1_vasprintf(buf, fmt, args);
  va_end(args);

  return len;
}

#if dg_configUSE_CONSOLE == 0
/******************************************************************************
 *
 *  printf
 *
 ******************************************************************************/

__attribute((externally_visible))
int printf(const char *__restrict format, ...)
{
        extern int _write(int file, char *ptr, int len);
#ifdef OS_BAREMETAL
        #define PBUF_SIZE       512
        /*static*/ char print_buffer[PBUF_SIZE];
#else 
        #define PBUF_SIZE       1024
        char *print_buffer;
#endif //OS_BAREMETAL	
        int ret;
        va_list param_list;

#ifndef OS_BAREMETAL
	print_buffer = (char *)OS_MALLOC(PBUF_SIZE);
#endif

        va_start(param_list, format);
        
        ret = ra6w1_vsnprintf(print_buffer, (PBUF_SIZE-1), 0, format, param_list);

        _write(0, print_buffer, ret);

        va_end(param_list);

#ifndef OS_BAREMETAL
	OS_FREE(print_buffer);
#endif
        return ret;
}

__attribute((externally_visible))
int vprintf(const char *format, va_list ap)
{
        extern int _write(int file, char *ptr, int len);
#ifdef OS_BAREMETAL
        #define PBUF_SIZE       512
        /*static*/ char print_buffer[PBUF_SIZE];
#else 
        #define PBUF_SIZE       1024
        char *print_buffer;
#endif //OS_BAREMETAL	
        int ret;

#ifndef OS_BAREMETAL
	print_buffer = (char *)OS_MALLOC(PBUF_SIZE);
#endif

        ret = ra6w1_vsnprintf(print_buffer, (PBUF_SIZE-1), 0, format, ap);

        _write(0, print_buffer, ret);

#ifndef OS_BAREMETAL
	OS_FREE(print_buffer);
#endif
        return ret;
        
}
#endif

#endif /* CONFIG_RA6W1_PRINTF */

/* EOF */
