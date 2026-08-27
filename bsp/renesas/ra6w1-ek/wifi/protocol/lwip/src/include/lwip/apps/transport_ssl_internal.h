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

#ifndef _WS_TRANSPORT_INTERNAL_H_
#define _WS_TRANSPORT_INTERNAL_H_

/**
 * @brief      Sets error to common transport handle
 *
 *             Note: This function copies the supplied error handle object to tcp_transport's internal
 *             error handle object
 *
 * @param[in]  A transport handle
 *
 */
void ws_transport_set_errors(ws_transport_handle_t t, const ws_tls_error_handle_t error_handle);


#endif /* _WS_TRANSPORT_INTERNAL_H_ */
