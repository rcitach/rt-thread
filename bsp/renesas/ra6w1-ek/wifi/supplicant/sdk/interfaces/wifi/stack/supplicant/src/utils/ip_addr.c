/*
 * IP address processing
 * Copyright (c) 2003-2006, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "supp_common.h"
#include "ip_addr.h"
#include "lwip/sockets.h"

//MODIFY_SUPPLICANT_FOR_FREERTOS - start add
#define RA6WX_BSD_URL_BUFSIZE	18
char ra6wx_bsd_url_buffer[RA6WX_BSD_URL_BUFSIZE];

static unsigned int ra6wx_is_space(unsigned char c);
static unsigned int ra6wx_is_lower(unsigned char c);
static unsigned int ra6wx_is_digit(unsigned char c);
static unsigned int ra6wx_is_xdigit(unsigned char c);

static int ra6wx_inet_ntoa_internal(const void *src, char *dst, unsigned long dst_size);
static unsigned int ra6wx_number_convert(unsigned int number,
					char *string,
					unsigned long buffer_len,
					unsigned int base);
//MODIFY_SUPPLICANT_FOR_FREERTOS - end add

bool isIPv4Address(char* str, struct sockaddr_in *valid_ip);
bool isIPv6Address(char* str, struct sockaddr_in6 *valid_ip);

int select_ipaddr_type_from_str(char* str, struct sockaddr_storage *ipaddr);

const char * hostapd_ip_txt(const struct hostapd_ip_addr *addr, char *buf,
			    size_t buflen)
{
	if (buflen == 0 || addr == NULL)
		return NULL;

	if (addr->af == AF_INET) {
		os_strlcpy(buf, inet_ntoa(addr->u.v4), buflen);
	} else {
		buf[0] = '\0';
	}
#ifdef CONFIG_IPV6
	if (addr->af == AF_INET6) {
		if (inet_ntop(AF_INET6, &addr->u.v6, buf, buflen) == NULL)
			buf[0] = '\0';
	}
#endif /* CONFIG_IPV6 */

	return buf;
}


int hostapd_parse_ip_addr(const char *txt, struct hostapd_ip_addr *addr)
{
#ifndef CONFIG_NATIVE_WINDOWS
	if (inet_aton(txt, &addr->u.v4)) {
		addr->af = AF_INET;
		return 0;
	}

#ifdef CONFIG_IPV6
	if (inet_pton(AF_INET6, txt, &addr->u.v6) > 0) {
		addr->af = AF_INET6;
		return 0;
	}
#endif /* CONFIG_IPV6 */
#endif /* CONFIG_NATIVE_WINDOWS */

	return -1;
}

bool isIPv4Address(char* str, struct sockaddr_in *valid_ip) {
#if defined ( __SUPPORT_IPV4__ )
    struct sockaddr_in sa;
    if(inet_pton(AF_INET, str, &(sa.sin_addr))) {
        if (valid_ip) {
            memcpy(valid_ip, &sa, sizeof(struct sockaddr_in));
        }
        return 1; // success
    } else {
        return 0; // fail
    }
#else
    return 0;
#endif // __SUPPORT_IPV4__

}

bool isIPv6Address(char* str, struct sockaddr_in6 *valid_ip) {
#if defined ( __SUPPORT_IPV6__ )
    struct sockaddr_in6 sa;
    if(inet_pton(AF_INET6, str, &(sa.sin6_addr))) {
        if (valid_ip) {
            memcpy(valid_ip, &sa, sizeof(struct sockaddr_in6));
        }
        return 1; // success
    } else {
        return 0; // fail
    }
#else
    return 0;
#endif // __SUPPORT_IPV6__
}

int select_ipaddr_type_from_str(char* str, struct sockaddr_storage *ipaddr) {
    if(isIPv4Address(str, (struct sockaddr_in *)ipaddr)) {
        return IPADDR_TYPE_V4;
    } else if (isIPv6Address(str, (struct sockaddr_in6 *)ipaddr)) {
        return IPADDR_TYPE_V6;
    } else {
        return -1;
    }
}

//MODIFY_SUPPLICANT_FOR_FREERTOS - start add
char *ra6wx_inet_ntoa(struct in_addr address_to_convert)
{
	char url_buffer[RA6WX_BSD_URL_BUFSIZE] = {0x00,};

	ra6wx_inet_ntoa_internal(&address_to_convert, url_buffer, RA6WX_BSD_URL_BUFSIZE);

	memcpy(ra6wx_bsd_url_buffer, url_buffer, RA6WX_BSD_URL_BUFSIZE);

	return ra6wx_bsd_url_buffer;
}

const char *ra6wx_inet_ntop(int af, const VOID *src, char *dst, unsigned long size)
{
	int shorthand_index;
	int shorthand_len;
	int current_index;
	int current_len;

	int i;
	int index;
	unsigned int rt_size;
	unsigned short *temp;

	if(af == AF_INET) {
		if(ra6wx_inet_ntoa_internal(src, dst, size)) {
			return dst;
		} else {
			return NULL;
		}
	} else if(af == AF_INET6) {
		temp = (unsigned short*)src;

		shorthand_index = -1;
		shorthand_len = 0;
		current_index = -1;
		current_len = 0;

		for(i = 0; i < 8; i++) {
			if(temp[i] == 0) {
				if(current_index == -1) {
					current_index = i;
					current_len = 1;
				} else {
					current_len++;
				}
			} else {
				if(current_index != -1) {
					if(shorthand_index == -1 || current_len > shorthand_len) {
						shorthand_index = current_index;
						shorthand_len = current_len;
					}

					current_index = -1;
				}
			}
		}

		if(current_len > shorthand_len) {
			shorthand_index = current_index;
			shorthand_len = current_len;
		}

		if(shorthand_len < 2) {
			shorthand_index = -1;
		}

		index = 0;

		for(i = 0; i < 8; i++) {
			if((shorthand_index != -1) && 
					(i >= shorthand_index) && 
					(i < (shorthand_index + shorthand_len))) {
				if(i == shorthand_index) {
					if((size - index) < 1) {
						return NULL;
					}

					dst[index++] = ':';
				}
				continue;
			}    

			if(i != 0) {
				if((size - index) < 1) {
					return NULL;
				}

				dst[index++] = ':';
			}

			if(i == 6 && shorthand_index == 0 &&
					(shorthand_len == 6 ||
					 (shorthand_len == 5 && temp[5] == 0xffff))) {
				rt_size = ra6wx_inet_ntoa_internal((unsigned char *)src + 12, &dst[index], size - index);

				if(rt_size) {
					index += rt_size;
				} else {
					return NULL;
				}
			}

			rt_size = ra6wx_number_convert(ntohs(temp[i] & 0xffff), &dst[index], size - index, 16);
			if(!rt_size) {
				return NULL;
			}

			index += rt_size;
		}

		if((shorthand_index != -1) && (shorthand_index + shorthand_len == 8)) {
			if((size - index) < 1) {
				return NULL;
			}

			dst[index++] = ':';
		}

		if((size - index) < 1) {
			return NULL;
		}

		dst[index] = '\0';

		return dst;
	}

	return NULL;
}

int ra6wx_inet_aton(const char *address_buffer_ptr, struct in_addr *addr)
{
	unsigned long value;
	int base, ip_address_index;
	unsigned char tempchar;
	const unsigned char *buffer_ptr;
	unsigned int ip_address_number[4];
	unsigned int *ip_number_ptr;
	unsigned int dot_flag;   

	buffer_ptr = (const unsigned char *) address_buffer_ptr;
	ip_number_ptr = ip_address_number;

	tempchar = *buffer_ptr;

	if (ra6wx_is_digit(tempchar)== FALSE) {
		return (0);
	}

	dot_flag = 1;

	do {
		value = 0; 

		if(dot_flag== 1) {
			base = 10;

			if (*buffer_ptr == '0') 
			{
				buffer_ptr++;

				if ((*buffer_ptr== 'x') || (*buffer_ptr == 'X')) {
					base = 16;

					buffer_ptr++;
				} else {
					base = 8;
					buffer_ptr--;
				}
			}
		}

		tempchar = *buffer_ptr;

		while (*buffer_ptr != '\0')  {
			if (ra6wx_is_digit(tempchar))  {
				dot_flag = 0;
				value = (value * base) + (tempchar - '0');
				buffer_ptr++;
				tempchar = *buffer_ptr;
			} else if (ra6wx_is_xdigit(tempchar)) {
				if(base == 16) {
					char c = (char)(ra6wx_is_lower(tempchar) ? 'a' : 'A');
					dot_flag = 0;

					value = (value << 4) + (tempchar + 10 - c);

					buffer_ptr++;

					tempchar = *buffer_ptr;
				} else {
					return (0);
				}                    
			} else {
				break;
			}
		}

		if (*buffer_ptr == '.')  {
			dot_flag = 1;

			if (value > 0xff) {
				return (0);
			}

			if (ip_number_ptr >= (ip_address_number + 3)) {
				return (0);
			}

			*ip_number_ptr = value;

			ip_number_ptr++;

			buffer_ptr++;
		} else {
			break;
		}

	} while (1);

	if (*buffer_ptr) {
		if((*buffer_ptr != '\0') && (!ra6wx_is_space(*buffer_ptr))) {
			return (0);
		}
	}

	ip_address_index = ip_number_ptr - ip_address_number + 1;

	if ((ip_address_index == 0) || (ip_address_index > 4)) {
		return (0);
	}

	if (ip_address_index == 4) {
		int i;

		if (value > 0xff) {
			return (0);
		}

		for (i = 0; i<=2; i++) {
			value |= ip_address_number[i] << (24 - (i*8));
		}
	} else if (ip_address_index == 1) {
		/* We are done, this address contained one 32 bit word (no separators).  */
	} else if (ip_address_index ==  2) {
		if (value > 0xffffff) {
			return (0);
		}

		value |= (ip_address_number[0] << 24);
	} else if (ip_address_index ==  3) {
		int i;

		if (value > 0xffff) {
			return (0);
		}

		for (i = 0; i<=1; i++) {
			value |= ip_address_number[i] << (24 - (i*8));
		}
	}

	if (addr) {
		addr->s_addr = htonl(value);
	}

	return (1);
}

int  ra6wx_inet_pton(int af, const char *src, void *dst)
{
	char ch;
	unsigned short value; 
	unsigned int minuend;    
	unsigned int digit_counter;

	unsigned char   *colon_location;
	unsigned char   *dst_cur_ptr;
	unsigned char   *dst_end_ptr;

	const char *ipv4_addr_start = NULL;
	unsigned int n, i; 

	struct  in_addr ipv4_addr;

	if(af == AF_INET) {
		if(ra6wx_inet_aton(src, &ipv4_addr)) {
			*((unsigned long *)dst) = ipv4_addr.s_addr;
			return 1;
		}
		return 0;
	} else if(af == AF_INET6) {
		if(*src == ':') {
			if(*src++ != ':') {
				return 0;
			}
		}

		value          = 0;
		digit_counter  = 0;
		colon_location = NULL;
		dst_cur_ptr    = dst;
		dst_end_ptr    = (unsigned char*)dst + 15;
		memset(dst_cur_ptr, 0, 16);

		while(*src != 0) {
			ch = *src++;

			minuend = 0;
			if(ch >= '0' && ch <= '9') {
				minuend = 48;   
			} else if(ch >= 'a' && ch <= 'f') {
				minuend = 87;       
			} else if(ch >= 'A' && ch <= 'F') {
				minuend = 55;
			}

			if(minuend) {
				value <<= 4;
				value |= (ch - minuend);

				if(++digit_counter > 4) {
					return 0;
				}

				continue;
			}

			if(ch == ':') {
				ipv4_addr_start = src;

				if(!digit_counter) {
					if(colon_location) {
						return 0;
					}

					colon_location = dst_cur_ptr;
					continue;
				} else if(*src == '\0') {
					return 0;
				}

				if(dst_cur_ptr + 2 > dst_end_ptr) {
					return 0;
				}

				*(dst_cur_ptr++) = (unsigned char) (value >> 8) & 0xff;
				*(dst_cur_ptr++) = (unsigned char) (value) & 0xff;

				digit_counter = 0;
				value = 0;

				continue;
			} else if(ch == '.') {
				if(dst_cur_ptr + 4 > dst_end_ptr)
				{
					return 0;
				}

				if(ra6wx_inet_aton(ipv4_addr_start, &ipv4_addr)) {

					*(dst_cur_ptr++) = (unsigned char) (ipv4_addr.s_addr)       & 0xff;
					*(dst_cur_ptr++) = (unsigned char) (ipv4_addr.s_addr >> 8 ) & 0xff;
					*(dst_cur_ptr++) = (unsigned char) (ipv4_addr.s_addr >> 16) & 0xff;
					*(dst_cur_ptr++) = (unsigned char) (ipv4_addr.s_addr >> 24) & 0xff;

					digit_counter = 0;

					break;
				} else {
					return 0;
				}
			}

			return 0;
		}

		if(digit_counter) {
			if(dst_cur_ptr + 2 > dst_end_ptr) {
				return 0;
			}

			*(dst_cur_ptr++) = (unsigned char) (value >> 8) & 0xff;
			*(dst_cur_ptr++) = (unsigned char) (value)      & 0xff;
		}

		if(colon_location) {
			n = dst_cur_ptr  - colon_location;

			if(dst_cur_ptr == dst_end_ptr) {
				return 0;
			}

			for(i = 1; i <=n; i++) {
				*(dst_end_ptr--) = colon_location[n - i];
				colon_location[n-i]  = 0;
			}

			dst_cur_ptr = dst_end_ptr;
		}

		if(dst_cur_ptr != dst_end_ptr) {
			return 0;
		}

		return 1;
	} else {
		return -1;
	}
}

static unsigned int ra6wx_number_convert(unsigned int number,
				char *string,
				unsigned long buffer_len,
				unsigned int base)
{
	unsigned int j = 0;
	unsigned int digit = 0;
	unsigned int size = 0;

	while (size < buffer_len) {
		for (j = size; j != 0; j--) {
			string[j] =  string[j-1];
		}

		digit =  number % base;
		number =  number / base;

		if(digit < 10) {
			string[0] = (char) (digit + 0x30);
		} else {
			string[0] = (char) (digit + 0x57);
		}

		size++;

		if (number == 0) {
			break;
		}
	}

	string[size] = 0;

	if (number) {
		size =  0;
		string[0] = '0';
	}

	return(size);
}


static unsigned int ra6wx_is_space(unsigned char c)
{
	if ((c >= 0x09) && (c <= 0x0D)) {
		return TRUE;
	} else if (c == 20) {
		return TRUE;
	} else {
		return FALSE;
	}
}

static unsigned int ra6wx_is_lower(unsigned char c)
{
	if ((c >= 0x61) && (c <= 0x7A)) {
		return TRUE;
	}
	else {
		return FALSE;
	}

}

static unsigned int ra6wx_is_digit(unsigned char c)
{
	if ((c >= 0x30) && (c <= 0x39)) {
		return TRUE;
	} else {
		return FALSE;
	}
}

static unsigned int ra6wx_is_xdigit(unsigned char c)
{
	if ((c >= 0x30) && (c <= 0x39)) {
		return TRUE;
	}

	if ((c >= 0x41) && (c <= 0x46)) {
		return TRUE;
	}

	if ((c >= 0x61) && (c <= 0x66)) {
		return TRUE;
	}

	return FALSE;
}



static int ra6wx_inet_ntoa_internal(const void *src, char *dst, unsigned long dst_size)
{
	char *temp;
	unsigned int size;
	unsigned int index = 0;


	temp = (char *)src;

	memset(dst, 0, dst_size);

	size = ra6wx_number_convert(*temp & 0xff, &dst[0], dst_size - index, 10);

	if(!size) {
		return 0;
	}

	temp++;
	index += size;

	if((dst_size - index) < 1) {
		return 0; 
	}

	dst[index++] = '.';

	/* And repeat three more times...*/
	size = ra6wx_number_convert(*temp & 0xff, &dst[index], dst_size - index, 10);

	if(!size)
		return 0;

	temp++;
	index += size;

	if((dst_size - index) < 1)
		return 0; 

	dst[index++] = '.';

	size = ra6wx_number_convert(*temp & 0xff, &dst[index], dst_size - index, 10);

	if(!size)
		return 0;

	temp++;
	index += size;

	if((dst_size - index) < 1)
		return 0; 

	dst[index++] = '.';

	size = ra6wx_number_convert(*temp & 0xff, &dst[index], dst_size - index, 10);

	if(!size)
		return 0;

	index += size;

	if((dst_size - index) < 1)
		return 0; 

	dst[index++] = '\0';

	/* Return the size of the dst string. */
	return index;
}
//MODIFY_SUPPLICANT_FOR_FREERTOS - end add
