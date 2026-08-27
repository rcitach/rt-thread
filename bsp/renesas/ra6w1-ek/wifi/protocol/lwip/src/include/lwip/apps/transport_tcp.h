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
 * Copyright (c) 2021-2022 Modified by Renesas Electronics.
 *
**/

#ifndef _WS_TRANSPORT_TCP_H_
#define _WS_TRANSPORT_TCP_H_

#include "transport.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief      Set TCP keep-alive configuration
 *
 * @param[in]  t               The transport handle
 * @param[in]  keep_alive_cfg  The keep-alive config
 */
void ws_transport_tcp_set_keep_alive(ws_transport_handle_t t, ws_transport_keep_alive_t *keep_alive_cfg);

/**
 * @brief      Create TCP transport, the transport handle must be release ws_transport_destroy callback
 *
 * @return  the allocated ws_transport_handle_t, or NULL if the handle can not be allocated
 */
ws_transport_handle_t ws_transport_tcp_init(void);


#ifdef __cplusplus
}
#endif

#endif /* _WS_TRANSPORT_TCP_H_ */
