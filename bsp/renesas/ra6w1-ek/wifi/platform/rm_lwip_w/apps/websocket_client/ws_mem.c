/**
 *****************************************************************************************
 * @file    ws_mem.c
 * @brief    Memory-releated APIs (malloc, calloc, realloc, free)
  *****************************************************************************************
 */

/*
Copyright (c) 2014 Roger Light <roger@atchoo.org>

All rights reserved. This program and the accompanying materials
are made available under the terms of the Eclipse Public License v1.0
and Eclipse Distribution License v1.0 which accompany this distribution.

The Eclipse Public License is available at
   http://www.eclipse.org/legal/epl-v10.html
and the Eclipse Distribution License is available at
  http://www.eclipse.org/org/documents/edl-v10.php.

Contributors:
   Roger Light - initial implementation and documentation.


Copyright (c) 2023 Modified by Renesas Electronics

*/

#include "FreeRTOS.h"
#include "lwipopts.h"

#if defined ( __SUPPORT_WEBSOCKET_CLIENT__ )

#include "ws_mem.h"
#include "ws_log.h"
#include "os.h"
#include "string.h"


static inline void * ws_realloc(void *ptr, size_t size)
{
	void *result;

	if (ptr == NULL) {
		return os_zalloc(size);
	} else {
		result = os_zalloc(size);
		if (result == NULL)
			return NULL;
	}

	memcpy(result, ptr, size);
	
	if(ptr != NULL)
	{
		vPortFree(ptr);
	}

	
	return result;
}

static inline char * ws_strdup(const char *str)
{
	size_t len;
	char *result;

	// if str is null, allocate 1 byte and initialize with '\0'
	if (str == NULL)
	{
		len = 0;		
	}
	else
	{
		len = strlen(str);
	}

	result = pvPortMalloc(len + 1);

	if(result) {
		if (len == 0)
		{
			result[0] = '\0';
		}
		else
		{
			memcpy(result, str, len + 1);
		}
	}

	return result;
}

#if (_POSIX_C_SOURCE < 200809L)
static inline size_t ws_strnlen(const char *s, size_t maxlen)
{
        size_t len;

        for (len = 0; len < maxlen; len++, s++) {
                if (!*s)
                break;
        }
        return len;
}
#endif


static inline char * ws_strndup(const char *str, size_t n)
{
    #if (_POSIX_C_SOURCE >= 200809L)
        size_t len = strnlen(str, n);
    #else
        size_t len = ws_strnlen(str, n);
    #endif

        char *new = (char *) pvPortMalloc(len + 1);

        if (new == NULL)
        {
                return NULL;
        }

        new[len] = '\0';

        return (char *) memcpy (new, str, len);
}

void *_ws_calloc(size_t nmemb, size_t size)
{
	void *mem = os_calloc(nmemb, size);
	return mem;
}

void *_ws_malloc(size_t size)
{
	void* mem = pvPortMalloc(size);
	return mem;
}

void *_ws_realloc(void *ptr, size_t size)
{
	void *mem;
	
	mem = ws_realloc(ptr, size);
	return mem;
}

char *_ws_strdup(const char *s)
{
	char* str = ws_strdup(s);

	return str;
}

char *_ws_strndup(const char *s, size_t n)
{
	char* str = ws_strndup(s, n);

	return str;
}

void _ws_free(void *mem)
{
	if (mem==NULL)
	{
		return;
	}
	
	vPortFree(mem);

	mem=NULL;
}

#endif // __SUPPORT_WEBSOCKET_CLIENT__

/* EOF */
