/*
 * Copyright (c) 2015-2016, Nuvoton Technology Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
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
 */

#ifndef MBEDTLS_DES_ALT_H
 #define MBEDTLS_DES_ALT_H

 #include "common.h"
 #include "bsp_api.h"

/* TIN-TODO: These macros are used for direct register accesses. Maybe rework is needed? */
 #define DES_CONTROL    0x40030000
 #define CBC_IV         0x40030004
 #define DES_KEY1       0x4003000C
 #define DES_KEY2       0x40030014
 #define DES_KEY3       0x4003001C
 #define DES_INPUT      0x40030024
 #define DES_OUTPUT     0x4003002C

 #ifdef __cplusplus
extern "C" {
 #endif

/**
 * \brief          DES context structure
 */
typedef struct mbedtls_des_context
{
    uint32_t sk[6];
} mbedtls_des_context;

/**
 * \brief          Triple-DES context structure
 */
typedef struct mbedtls_des3_context
{
    uint32_t sk[6];
} mbedtls_des3_context;

 #ifdef __cplusplus
}
 #endif

#endif                                 /* des_alt.h */
