/**
 * Copyright 2015-2018 Espressif Systems (Shanghai) PTE LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Copyright (c) 2021-2022 Modified by Renesas Electronics
 *
**/
#include "FreeRTOS.h"
#include "lwipopts.h"
	
#if defined ( __SUPPORT_WEBSOCKET_CLIENT__ )

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "transport_utils.h"

struct timeval* ws_transport_utils_ms_to_timeval(int timeout_ms, struct timeval *tv)
{
    if (timeout_ms == -1) {
        return NULL;
    }
    tv->tv_sec = timeout_ms / 1000;
    tv->tv_usec = (suseconds_t)((timeout_ms - (tv->tv_sec * 1000)) * 1000);
    return tv;
}

#endif // __SUPPORT_WEBSOCKET_CLIENT__

/* EOF */
