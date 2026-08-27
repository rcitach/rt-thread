/**
 *****************************************************************************************
 * @file	memory_mosq.h
 * @brief	Header file - Memory-releated APIs
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

Copyright (c) 2021-2022 Modified by Renesas Electronics.
*/

#ifndef __WS_MEM_H__
#define __WS_MEM_H__

/**
 ****************************************************************************************
 * @brief calloc() for websocket_client module
 ****************************************************************************************
 */
void *_ws_calloc(size_t nmemb, size_t size);

/**
 ****************************************************************************************
 * @brief free() for websocket_client module
 ****************************************************************************************
 */
void _ws_free(void *mem);

/**
 ****************************************************************************************
 * @brief malloc() for websocket_client module
 ****************************************************************************************
 */
void *_ws_malloc(size_t size);

/**
 ****************************************************************************************
 * @brief realloc() for websocket_client module
 ****************************************************************************************
 */
void *_ws_realloc(void *ptr, size_t size);

/**
 ****************************************************************************************
 * @brief strdup() for websocket_client module
 ****************************************************************************************
 */
char *_ws_strdup(const char *s);

/**
 ****************************************************************************************
 * @brief strndup() for websocket_client module
 ****************************************************************************************
 */
char *_ws_strndup(const char *s, size_t n);

#endif	/* __WS_MEM_H__ */
