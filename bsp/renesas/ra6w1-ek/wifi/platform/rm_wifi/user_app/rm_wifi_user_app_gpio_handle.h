/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef RM_WIFI_USER_APP_GPIO_HANDLE_H
#define RM_WIFI_USER_APP_GPIO_HANDLE_H
 #if defined(__SUPPORT_WIFI_USER_GPIO__)
typedef struct st_external_irq_instance external_irq_instance_t;

/***********************************************************************************************************************
 * Function Prototypes
 **********************************************************************************************************************/
void rm_wifi_app_gpio_config_button(void);
#if defined(__SUPPORT_EVK_LED__)
unsigned int rm_wifi_app_gpio_check_wps_button(int btn_gpio_port, int btn_gpio_num,
        int led_gpio_port, int led_gpio_num, int check_time);
#else
unsigned int rm_wifi_app_gpio_check_wps_button(int btn_gpio_port, int btn_gpio_num, int check_time);
#endif
int rm_wifi_app_gpio_handle_create_event(void);
void rm_wifi_app_gpio_handle_task_start(void);
int rm_wifi_app_gpio_wakeup_set(bsp_io_port_pin_t port_pin, bsp_io_wakeup_edge_t edge,
                                const external_irq_instance_t *p_ext_irq_wakeup);

 #endif
#endif
