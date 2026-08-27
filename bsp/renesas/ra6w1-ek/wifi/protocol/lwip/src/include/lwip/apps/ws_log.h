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
 * Copyright (c) 2023 Modified by Renesas Electronics
 *
**/
#ifndef _WS_LOG_H_
#define _WS_LOG_H_

#include "common_def.h"

#undef WS_MBEDTLS_DEBUG		//mbedTLS debug log

#define WS_DEBUG		0

#if WS_DEBUG
#define WS_LOGD(TAG, ...) printf(CLR_COL __VA_ARGS__)
#define WS_LOGV(TAG, ...) printf(CLR_COL __VA_ARGS__)
#define WS_LOGE(TAG, ...) printf(RED_COL __VA_ARGS__); printf(CLR_COL);
#define WS_LOGI(TAG, ...) printf(CLR_COL __VA_ARGS__)
#define WS_LOGW(TAG, ...) printf(YEL_COL __VA_ARGS__); printf(CLR_COL);
#else
#define WS_LOGD(TAG, ...)
#define WS_LOGV(TAG, ...)
#define WS_LOGE(TAG, ...) printf(RED_COL __VA_ARGS__); printf(CLR_COL);
#define WS_LOGI(TAG, ...) printf(CLR_COL __VA_ARGS__)
#define WS_LOGW(TAG, ...) printf(YEL_COL __VA_ARGS__); printf(CLR_COL);
#endif

#endif	/*_WS_ERR_H_*/
