/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "bsp_api.h"
#if defined(__SUPPORT_WIFI_USER_GPIO__)
#include <string.h>
#include <stdlib.h>
#include "common_def.h"
#include "event_groups.h"
#include "custom_config_sdk.h"
#include "r_ext_irq_w.h"
#include "r_gpio_w.h"
#include "rm_wifi.h"
#include "rm_wifi_helper.h"
#include "rm_wifi_user_app_gpio_handle.h"
#if CFG_PMGR
#include "rm_pmgr_w_instance.h"
#endif


/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#define WIFI_APP_GPIO_CHECK_STEP_FACTORY_BTN_MS     (100)          /* 100ms */
#define WIFI_APP_GPIO_CHECK_STEP_WPS_BTN_MS         (100)          /* 100ms */

#define WIFI_APP_GPIO_BTN_WPS_ACTIVE_STATE          (!BTN_WPS_INT_POL)
#define WIFI_APP_GPIO_BTN_FR_ACTIVE_STATE           (!BTN_FR_INT_POL)

#define WIFI_APP_GPIO_EV_TASK_NAME                  ("GPIO_ev")
#define WIFI_APP_GPIO_EV_TASK_STACK_SIZE            ((1024 * 4) / sizeof(unsigned long))

#define WIFI_APP_GPIO_EVENT_BTN_WPS                 (0x01)
#define WIFI_APP_GPIO_EVENT_BTN_FR                  (0x02)

#define WIFI_APP_GPIO_LED_STATE_ON                  (1)
#define WIFI_APP_GPIO_LED_STATE_OFF                 (0)

#define WIFI_APP_GPIO_FACTORY_BUTTON_FALSE          (0)
#define WIFI_APP_GPIO_FACTORY_BUTTON_FACTORY_RESET  (1)
#define WIFI_APP_GPIO_FACTORY_BUTTON_REBOOT         (2)

#if !defined(BTN_FR) || !defined(BTN_WPS)
#error "Please mark EXT_INTR1_PIN, EXT_INTR2_PIN as the Symbolic Name of the chosen pins for BTN_FR, BTN_WPS accordingly"
#endif

/***********************************************************************************************************************
 * Private functions
 **********************************************************************************************************************/
static void srm_wifi_app_gpio_button1_one_touch_cb_fn(void);
static void srm_wifi_app_gpio_set_interrupt(void);
static void srm_wifi_app_gpio_p0_wps_handler(void * param);
void srm_wifi_app_gpio_p0_fr_handler(void * param);
static void srm_wifi_app_gpio_p1_handler(void * param);

#if defined(__SUPPORT_EVK_LED__)
static void srm_wifi_app_gpio_set_led_state(int port, int num, int state);
#endif

static void srm_wifi_app_gpio_event_task(void * param);

static void srm_wifi_app_gpio_factory_reset_default(int reboot_flag);
#if defined(__SUPPORT_EVK_LED__)
static unsigned int srm_wifi_app_gpio_check_factory_button(int btn_gpio_port, int btn_gpio_num,
        int led_gpio_port, int led_gpio_num, int check_time);
#else
static unsigned int srm_wifi_app_gpio_check_factory_button(int btn_gpio_port, int btn_gpio_num, int check_time);
#endif

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/
static EventGroupHandle_t g_evt_grp_gpio = NULL;
static TaskHandle_t       g_gpio_ev_task = NULL;

/* One-touch press operation for Factory-Reset */
static int                (* gp_button1_one_touch_cb)(void) = NULL;

/***********************************************************************************************************************
 * External functions
 **********************************************************************************************************************/

#if defined(__SUPPORT_FACTORY_RST_CONCURR_MODE__)
extern int  factory_reset_btn_onetouch(void);
#endif

/***********************************************************************************************************************
 * External variables
 **********************************************************************************************************************/
extern const ioport_instance_t       g_gpio_w;
extern const wifi_cfg_t g_wifi_cfg;
#if defined(__SUPPORT_FACTORY_RESET_BTN__) || defined(__SUPPORT_WPS_BTN__)
extern       ext_irq_w_instance_ctrl_t   g_external_irq0_ctrl;
extern       ext_irq_w_instance_ctrl_t   g_external_irq1_ctrl;
extern       ext_irq_w_instance_ctrl_t   g_external_irq2_ctrl;
extern const external_irq_instance_t g_external_irq0;
extern const external_irq_instance_t g_external_irq1;
extern const external_irq_instance_t g_external_irq2;
#endif

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * Configure the GPIOs for the buttons.
 **********************************************************************************************************************/
void rm_wifi_app_gpio_config_button (void)
{
    /* Register call-back function for factory-reset button short-press. */
    srm_wifi_app_gpio_button1_one_touch_cb_fn();

    /* Set GPIO interrupt for WPS and factory-reset button. */
    srm_wifi_app_gpio_set_interrupt();

#if defined(__TRIGGER_DPM_MCU_WAKEUP__)
    /* Retain GPIO for DPM sleep. */
#if CFG_PMGR
    if ((pdTRUE == RM_PMGR_W_dpm_is_enabled()) && (pdFALSE == RM_PMGR_W_dpm_is_wakeup()))
    {
        //Requires board-specific settings
    }
#endif /* CFG_PMGR */
#endif
}

/*******************************************************************************************************************//**
 * Check the state of WPS button.
 **********************************************************************************************************************/
#if defined(__SUPPORT_EVK_LED__)
unsigned int rm_wifi_app_gpio_check_wps_button (int btn_gpio_port, int btn_gpio_num,
        int led_gpio_port, int led_gpio_num, int check_time)
#else
unsigned int rm_wifi_app_gpio_check_wps_button (int btn_gpio_port, int btn_gpio_num, int check_time)
#endif
{
    unsigned int result         = pdFALSE;
    int          pin_status;
    int          check_time_cnt = 0;
    int          first_loop     = 0;

    /* STEP #1 : Check button pushed. */
    do
    {
        g_gpio_w.p_api->pinRead(g_gpio_w.p_ctrl, ((btn_gpio_port << BSP_IO_PORT_OFFSET) | btn_gpio_num),
                                (bsp_io_level_t *) &pin_status);

        if (WIFI_APP_GPIO_BTN_WPS_ACTIVE_STATE == pin_status)
        {
            /* Button is in 'pressed' state. */
#if defined(__SUPPORT_EVK_LED__)
            if (0 == check_time_cnt)
            {
                srm_wifi_app_gpio_set_led_state(led_gpio_port, led_gpio_num, WIFI_APP_GPIO_LED_STATE_ON);
            }
#endif

            check_time_cnt++;
        }
        else
        {
            /* Button is in 'released' state. */
#if defined(__SUPPORT_EVK_LED__)
            if (0 != check_time_cnt)
            {
                srm_wifi_app_gpio_set_led_state(led_gpio_port, led_gpio_num, WIFI_APP_GPIO_LED_STATE_OFF);
            }
#endif

            return pdFALSE;
        }

        vTaskDelay(portCONVERT_MS_2_TICKS(WIFI_APP_GPIO_CHECK_STEP_WPS_BTN_MS));

        if (0 == first_loop)
        {
            printf("\33[2K" "WPS after %d seconds\n",
                check_time - (check_time_cnt / (WIFI_APP_GPIO_CHECK_STEP_WPS_BTN_MS/10)));
        }
        else
        {
            if (0 == (check_time_cnt % 10))
            {
                printf("%d", check_time - (check_time_cnt / (WIFI_APP_GPIO_CHECK_STEP_WPS_BTN_MS/10)));
            }
            else
            {
                printf(".");
            }
        }

        first_loop++;
    } while (check_time_cnt < ((WIFI_APP_GPIO_CHECK_STEP_WPS_BTN_MS / 10) * check_time));

    printf("\33[2K" "\nReady to use WPS.\n");

    check_time_cnt = 0;

    /* STEP #2 : Check button released. */
    do
    {
        g_gpio_w.p_api->pinRead(g_gpio_w.p_ctrl, ((btn_gpio_port << BSP_IO_PORT_OFFSET) | btn_gpio_num),
                                (bsp_io_level_t *) &pin_status);

        if (WIFI_APP_GPIO_BTN_WPS_ACTIVE_STATE != pin_status)
        {
#if defined(__SUPPORT_EVK_LED__)
            srm_wifi_app_gpio_set_led_state(led_gpio_port, led_gpio_num, WIFI_APP_GPIO_LED_STATE_OFF);
#endif
            printf("\33[2K"  "Start WPS.\n");
            result = pdTRUE;
            break;
        }
        else
        {
            /* Wait until button is released. */
#if defined(__SUPPORT_EVK_LED__)
            /* WPS Status LED Blink. */
            if (0 == (check_time_cnt % 2))
            {
                srm_wifi_app_gpio_set_led_state(led_gpio_port, led_gpio_num, WIFI_APP_GPIO_LED_STATE_OFF);
            }
            else
            {
                srm_wifi_app_gpio_set_led_state(led_gpio_port, led_gpio_num, WIFI_APP_GPIO_LED_STATE_ON);
            }
#endif

            vTaskDelay(portCONVERT_MS_2_TICKS(10));
            check_time_cnt++;
        }
    } while (1);

    return result;
}

/*******************************************************************************************************************//**
 * Create GPIO handler event.
 **********************************************************************************************************************/
int rm_wifi_app_gpio_handle_create_event (void)
{
    if (!g_evt_grp_gpio)
    {
        /* Create sync-up event. */
        g_evt_grp_gpio = xEventGroupCreate();
        if (NULL == g_evt_grp_gpio)
        {
            printf("\n\n>>> Failed to create GPIO event group !!!\n\n");
            return pdFALSE;
        }
    }

    return pdTRUE;
}

/*******************************************************************************************************************//**
 * Start GPIO event task.
 **********************************************************************************************************************/
void rm_wifi_app_gpio_handle_task_start (void)
{
    BaseType_t status;

    /* Create GPIO event handling task. */
    status = xTaskCreate(srm_wifi_app_gpio_event_task,
                         WIFI_APP_GPIO_EV_TASK_NAME,
                         WIFI_APP_GPIO_EV_TASK_STACK_SIZE,
                         (void *) NULL,
                         tskIDLE_PRIORITY + 2,    /* Don't assign as 0 priority */
                         &g_gpio_ev_task);

    if (pdPASS != status)
    {
        printf(RED_COLOR " [%s] creating srm_wifi_app_gpio_event_task fail (status:%ld) \n" CLEAR_COLOR, __func__,
               status);
    }
}

/***********************************************************************************************************************
 * Private Functions
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * Register call-back function for factory-reset button.
 **********************************************************************************************************************/
static void srm_wifi_app_gpio_button1_one_touch_cb_fn (void)
{
#if defined(__SUPPORT_WIFI_CONCURRENT__) && defined(__SUPPORT_FACTORY_RST_CONCURR_MODE__)
    gp_button1_one_touch_cb = factory_reset_btn_onetouch;
#else
    gp_button1_one_touch_cb = NULL;
#endif
}

/*******************************************************************************************************************//**
 * Set GPIO interrupt for WPS and factory-reset button.
 **********************************************************************************************************************/
static void srm_wifi_app_gpio_set_interrupt (void)
{
#if defined(__SUPPORT_FACTORY_RESET_BTN__)
    ext_irq_w_extended_cfg_t * p_irq1_ext_cfg = (ext_irq_w_extended_cfg_t *) g_external_irq1.p_cfg->p_extend;
    bsp_io_port_pin_t irq1_pin = p_irq1_ext_cfg->irq_pin;
    bsp_io_port_t irq1_port = irq1_pin >> BSP_IO_PORT_OFFSET;

    g_gpio_w.p_api->pinCfg(g_gpio_w.p_ctrl, ((irq1_port << BSP_IO_PORT_OFFSET) | (irq1_pin & BSP_IO_PIN_BITS)),
                            (uint32_t) (BTN_FR_MODE | GPIO_W_CFG_IRQ_ENABLE));
#endif

#if defined(__SUPPORT_WPS_BTN__)
    g_gpio_w.p_api->pinCfg(g_gpio_w.p_ctrl, ((BTN_WPS_PORT << BSP_IO_PORT_OFFSET) | BTN_WPS_PIN),
                            (uint32_t) (BTN_WPS_MODE | GPIO_W_CFG_IRQ_ENABLE));
#endif

#if defined(__SUPPORT_FACTORY_RESET_BTN__)
    g_external_irq1.p_api->open(&g_external_irq1_ctrl, g_external_irq1.p_cfg);

    if (0U == BTN_FR_PORT)
    {
        g_external_irq1.p_api->callbackSet(&g_external_irq1_ctrl, (void *) g_external_irq1.p_cfg->p_callback, NULL, NULL);
    }
    else
    {
        g_external_irq1.p_api->callbackSet(&g_external_irq1_ctrl, (void *) srm_wifi_app_gpio_p1_handler, NULL, NULL);
    }
    g_external_irq1.p_api->enable(&g_external_irq1_ctrl);
#endif

#if defined(__SUPPORT_WPS_BTN__)
    g_external_irq2.p_api->open(&g_external_irq2_ctrl, g_external_irq2.p_cfg);

    if (0U == BTN_WPS_PORT)
    {
        g_external_irq2.p_api->callbackSet(&g_external_irq2_ctrl, (void *) srm_wifi_app_gpio_p0_wps_handler, NULL,
                                           NULL);
    }
    else
    {
        g_external_irq2.p_api->callbackSet(&g_external_irq2_ctrl, (void *) srm_wifi_app_gpio_p1_handler, NULL, NULL);
    }
    g_external_irq2.p_api->enable(&g_external_irq2_ctrl);
#endif

#if defined(__SUPPORT_EVK_LED__)
    g_gpio_w.p_api->pinCfg(g_gpio_w.p_ctrl, ((FR_WPS_LED_PORT << BSP_IO_PORT_OFFSET) | FR_WPS_LED_PIN),
                           (uint32_t) (FR_WPS_LED_MODE | FR_WPS_LED_FUNC));
#endif
}

/*******************************************************************************************************************//**
 * Callback for GPIO interrupt for WPS button.
 **********************************************************************************************************************/
static void srm_wifi_app_gpio_p0_wps_handler (void * param)
{
    FSP_PARAMETER_NOT_USED(param);

    /* This callback is in interrupt. */
    BaseType_t xResult;

    if (g_evt_grp_gpio)
    {
        /* Set event to GPIO_event task. : callback function for GPIO interrupt */
        xResult = xEventGroupSetBitsFromISR(g_evt_grp_gpio,
                                            WIFI_APP_GPIO_EVENT_BTN_WPS,
                                            pdFALSE);    /* xHigherPriorityTaskWoken */

        if (pdFAIL != xResult)
        {
            /* If xHigherPriorityTaskWoken is now set to pdTRUE then a context
             * switch should be requested. The macro used is port specific and will
             * be either portYIELD_FROM_ISR() or portEND_SWITCHING_ISR() - refer to
             * the documentation page for the port being used. */
            portYIELD_FROM_ISR(pdFALSE);
        }
    }

    /*
     *       OS_TASK_NOTIFY_FROM_ISR(...);
    */
}

/*******************************************************************************************************************//**
 * Callback for GPIO interrupt for factory-reset button.
 **********************************************************************************************************************/
void srm_wifi_app_gpio_p0_fr_handler (void * param)
{
    FSP_PARAMETER_NOT_USED(param);

    /* This callback is in interrupt. */
    BaseType_t xResult;

    if (g_evt_grp_gpio)
    {
        /* Set event to GPIO_event task. : callback function for GPIO interrupt */
        xResult = xEventGroupSetBitsFromISR(g_evt_grp_gpio,
                                            WIFI_APP_GPIO_EVENT_BTN_FR,
                                            pdFALSE);    /* xHigherPriorityTaskWoken */

        if (pdFAIL != xResult)
        {
            /* If xHigherPriorityTaskWoken is now set to pdTRUE then a context
             * switch should be requested. The macro used is port specific and will
             * be either portYIELD_FROM_ISR() or portEND_SWITCHING_ISR() - refer to
             * the documentation page for the port being used. */
            portYIELD_FROM_ISR(pdFALSE);
        }
    }

    /*
     *       OS_TASK_NOTIFY_FROM_ISR(...);
    */
}

/*******************************************************************************************************************//**
 * Callback for GPIO interrupt for others.
 **********************************************************************************************************************/
static void srm_wifi_app_gpio_p1_handler (void * param)
{
    FSP_PARAMETER_NOT_USED(param);

    /* This callback is in interrupt. */
    printf("int GPIO0 %08lx\r\n", GPIO->GPIO_INT_STS_P1_REG);
}

/*******************************************************************************************************************//**
 * Set LED On/Off state.
 **********************************************************************************************************************/
#if defined(__SUPPORT_EVK_LED__)
static void srm_wifi_app_gpio_set_led_state (int port, int num, int state)
{
    if (WIFI_APP_GPIO_LED_STATE_OFF == state)
    {
        g_gpio_w.p_api->pinWrite(g_gpio_w.p_ctrl, ((port << BSP_IO_PORT_OFFSET) | num), BSP_IO_LEVEL_LOW);
    }
    else
    {
        g_gpio_w.p_api->pinWrite(g_gpio_w.p_ctrl, ((port << BSP_IO_PORT_OFFSET) | num), BSP_IO_LEVEL_HIGH);
    }
}
#endif

/*******************************************************************************************************************//**
 * GPIO event task.
 **********************************************************************************************************************/
static void srm_wifi_app_gpio_event_task (void * param)
{
    FSP_PARAMETER_NOT_USED(param);

    EventBits_t gpio_ev_bits   = 0;
    EventBits_t target_ev_bits = 0;

#if defined(__SUPPORT_FACTORY_RESET_BTN__) || defined(__SUPPORT_WPS_BTN__)
    unsigned int status = 0;
#endif

#if defined(__SUPPORT_WPS_BTN__)
    target_ev_bits = target_ev_bits | WIFI_APP_GPIO_EVENT_BTN_WPS;
#endif

#if defined(__SUPPORT_FACTORY_RESET_BTN__)
    target_ev_bits = target_ev_bits | WIFI_APP_GPIO_EVENT_BTN_FR;
#endif

#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
    uint8_t sys_wdog_id = WATCHDOG_SERVICE_W_NOT_REGISTERED_ID;
    g_wifi_cfg.p_watchdog_service->p_api->registerTask(g_wifi_cfg.p_watchdog_service->p_ctrl,
                                                           g_wifi_cfg.p_watchdog_service->p_cfg, &sys_wdog_id);
#endif

    while (pdTRUE)
    {
#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
        g_wifi_cfg.p_watchdog_service->p_api->notify(g_wifi_cfg.p_watchdog_service->p_ctrl,
                                                         g_wifi_cfg.p_watchdog_service->p_cfg, sys_wdog_id);
        g_wifi_cfg.p_watchdog_service->p_api->suspend(g_wifi_cfg.p_watchdog_service->p_ctrl, sys_wdog_id);
#endif

        gpio_ev_bits = xEventGroupWaitBits(g_evt_grp_gpio,
                                           target_ev_bits,
                                           pdTRUE,
                                           pdFALSE,
                                           portMAX_DELAY);

#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
        g_wifi_cfg.p_watchdog_service->p_api->resumeAndNotify(g_wifi_cfg.p_watchdog_service->p_ctrl,
                                                                  g_wifi_cfg.p_watchdog_service->p_cfg,
                                                                  sys_wdog_id);
#endif

        /* Event occurred. */
        if (gpio_ev_bits & WIFI_APP_GPIO_EVENT_BTN_WPS)
        {
            xEventGroupClearBits(g_evt_grp_gpio, WIFI_APP_GPIO_EVENT_BTN_WPS);
        }
        else if (gpio_ev_bits & WIFI_APP_GPIO_EVENT_BTN_FR)
        {
            xEventGroupClearBits(g_evt_grp_gpio, WIFI_APP_GPIO_EVENT_BTN_FR);
        }
        else
        {
            continue;
        }

#if defined(__SUPPORT_FACTORY_RESET_BTN__)
        if (gpio_ev_bits & WIFI_APP_GPIO_EVENT_BTN_FR) /* Factory Reset BTN */
        {
            ext_irq_w_extended_cfg_t * p_irq1_ext_cfg = (ext_irq_w_extended_cfg_t *) g_external_irq1.p_cfg->p_extend;
            bsp_io_port_pin_t irq1_pin = p_irq1_ext_cfg->irq_pin;
            bsp_io_port_t irq1_port = irq1_pin >> BSP_IO_PORT_OFFSET;

 #if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
            g_wifi_cfg.p_watchdog_service->p_api->notify(g_wifi_cfg.p_watchdog_service->p_ctrl,
                                                             g_wifi_cfg.p_watchdog_service->p_cfg, sys_wdog_id);
            g_wifi_cfg.p_watchdog_service->p_api->suspend(g_wifi_cfg.p_watchdog_service->p_ctrl, sys_wdog_id);
 #endif

 #if defined(__SUPPORT_EVK_LED__)
            status = srm_wifi_app_gpio_check_factory_button(irq1_port, (irq1_pin & BSP_IO_PIN_BITS),
                                                             FR_WPS_LED_PORT, FR_WPS_LED_PIN, BTN_FR_CHK_TIME);
 #else
            status = srm_wifi_app_gpio_check_factory_button(irq1_port, (irq1_pin & BSP_IO_PIN_BITS), BTN_FR_CHK_TIME);
 #endif

            if (WIFI_APP_GPIO_FACTORY_BUTTON_FACTORY_RESET == status)
            {
                srm_wifi_app_gpio_factory_reset_default(pdTRUE);
            }
            else if (WIFI_APP_GPIO_FACTORY_BUTTON_REBOOT == status) /* Reboot */
            {
                reset();
            }

 #if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
            g_wifi_cfg.p_watchdog_service->p_api->resumeAndNotify(g_wifi_cfg.p_watchdog_service->p_ctrl,
                                                                      g_wifi_cfg.p_watchdog_service->p_cfg,
                                                                      sys_wdog_id);
 #endif
        }
#endif

#if defined(__SUPPORT_WPS_BTN__) && defined(__SUPPORT_FACTORY_RESET_BTN__)
        else
        {
#endif

#if defined(__SUPPORT_WPS_BTN__)
            if (gpio_ev_bits & WIFI_APP_GPIO_EVENT_BTN_WPS) /* WPS BTN */
            {
 #if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
                g_wifi_cfg.p_watchdog_service->p_api->notify(g_wifi_cfg.p_watchdog_service->p_ctrl,
                                                                 g_wifi_cfg.p_watchdog_service->p_cfg, sys_wdog_id);
                g_wifi_cfg.p_watchdog_service->p_api->suspend(g_wifi_cfg.p_watchdog_service->p_ctrl,
                                                                  sys_wdog_id);
 #endif

 #if defined(__SUPPORT_EVK_LED__)
                status = rm_wifi_app_gpio_check_wps_button(BTN_WPS_PORT, BTN_WPS_PIN,
                                                            FR_WPS_LED_PORT, FR_WPS_LED_PIN, BTN_WPS_CHK_TIME);
 #else
                status = rm_wifi_app_gpio_check_wps_button(BTN_WPS_PORT, BTN_WPS_PIN, BTN_WPS_CHK_TIME);
 #endif

                if (pdTRUE == status)
                {
                    char reply[10];
                    memset(reply, 0, 10);
                    ra6w1_cli_reply("wps_pbc any", NULL, reply);
                }

 #if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
                g_wifi_cfg.p_watchdog_service->p_api->resumeAndNotify(g_wifi_cfg.p_watchdog_service->p_ctrl,
                                                                          g_wifi_cfg.p_watchdog_service->p_cfg,
                                                                          sys_wdog_id);
 #endif
            }
 #if defined(__SUPPORT_WPS_BTN__) && defined(__SUPPORT_FACTORY_RESET_BTN__)
        }
 #endif
#endif
    }

#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
    g_wifi_cfg.p_watchdog_service->p_api->unregisterTask(g_wifi_cfg.p_watchdog_service->p_ctrl, sys_wdog_id);
#endif

    vTaskDelete(NULL);
}

/*******************************************************************************************************************//**
 * Default factory reset.
 **********************************************************************************************************************/
static void srm_wifi_app_gpio_factory_reset_default (int reboot_flag)
{
    printf(ANSI_COLOR_LIGHT_RED "\nFactory Reseting...\n" ANSI_COLOR_DEFULT );

#if defined(__SUPPORT_FACTORY_RST_APMODE__)
    factory_reset_ap_mode();
#elif defined(__SUPPORT_FACTORY_RST_STAMODE__)
    factory_reset_sta_mode();
#elif defined( __SUPPORT_FACTORY_RST_CONCURR_MODE__ )
    factory_reset_concurrent_mode();
#else
    factory_reset_user_define();
#endif

    vTaskDelay(portCONVERT_MS_2_TICKS(100));

    if (pdTRUE == reboot_flag)
    {
        reset();

        /* Wait for system-reboot. */
        while (1)
        {
            vTaskDelay(portCONVERT_MS_2_TICKS(100));
        }
    }
}

/*******************************************************************************************************************//**
 * Features of the factory reset button.
 *  1. Press for 5 seconds or more, release: factory reset
 *  2. Press for 1 second, release: sys_mode switch (for concurrent mode)
 *  3. Press for 1 to 4 seconds, release: reboot
 **********************************************************************************************************************/
#if defined(__SUPPORT_EVK_LED__)
static unsigned int srm_wifi_app_gpio_check_factory_button (int btn_gpio_port, int btn_gpio_num,
        int led_gpio_port, int led_gpio_num, int check_time)
#else
static unsigned int srm_wifi_app_gpio_check_factory_button (int btn_gpio_port, int btn_gpio_num, int check_time)
#endif
{
    unsigned int result         = WIFI_APP_GPIO_FACTORY_BUTTON_FALSE;
    int          pin_status     = 0;
    int          check_time_cnt = 0;
    int          first_loop     = 0;

    /* STEP #1 : Check button pushed. */
    do
    {
        g_gpio_w.p_api->pinRead(g_gpio_w.p_ctrl, ((btn_gpio_port << BSP_IO_PORT_OFFSET) | btn_gpio_num),
                                (bsp_io_level_t *) &pin_status);

        if (WIFI_APP_GPIO_BTN_FR_ACTIVE_STATE == pin_status)
        {
            /* Button is in 'pressed' state. */
#if defined(__SUPPORT_EVK_LED__)
            if (2 == check_time_cnt)
            {
                srm_wifi_app_gpio_set_led_state(led_gpio_port, led_gpio_num, WIFI_APP_GPIO_LED_STATE_ON);
            }
#endif

            check_time_cnt++;
        }
        else
        {
            /* Button is in 'released' state. */
            if ((check_time_cnt > 10) && (check_time_cnt < (10 * check_time)))
            {
                /* 1 ~ reset_time Sec. */
#if defined(__SUPPORT_EVK_LED__)
                srm_wifi_app_gpio_set_led_state(led_gpio_port, led_gpio_num, WIFI_APP_GPIO_LED_STATE_OFF);
#endif

                result = WIFI_APP_GPIO_FACTORY_BUTTON_REBOOT; /* Reboot */
                goto end;
            }
            else if ((check_time_cnt > 0) && (check_time_cnt < 10))
            {
                /* In case of one-touch button push ... */

                /* Registered in system_start.c by application operation. */
                if (NULL != gp_button1_one_touch_cb)
                {
                    if (gp_button1_one_touch_cb() == pdTRUE)
                    {
                        /* Reboot to change runinng mode. */
                        reset();
                    }
                }
            }

#if defined(__SUPPORT_EVK_LED__)
            srm_wifi_app_gpio_set_led_state(led_gpio_port, led_gpio_num, WIFI_APP_GPIO_LED_STATE_OFF);
#endif

            result = WIFI_APP_GPIO_FACTORY_BUTTON_FALSE; /* false */
            goto end;
        }

        vTaskDelay(portCONVERT_MS_2_TICKS(WIFI_APP_GPIO_CHECK_STEP_FACTORY_BTN_MS));

        if (0 == first_loop)
        {
            printf("\33[2K" "Factory reset after %d seconds\n",
                   check_time - (check_time_cnt / (WIFI_APP_GPIO_CHECK_STEP_FACTORY_BTN_MS/10)));
        }
        else
        {
            if ((check_time_cnt % 10) == 0)
            {
                printf("%d", check_time - (check_time_cnt / (WIFI_APP_GPIO_CHECK_STEP_FACTORY_BTN_MS/10)));
            }
            else
            {
                printf(".");
            }
        }

        first_loop++;
    } while (check_time_cnt < ((WIFI_APP_GPIO_CHECK_STEP_FACTORY_BTN_MS/10) * check_time));

    printf("\33[2K" "\nReady to Factory Reset.\n");

    check_time_cnt = 0;

    /* STEP #2 : Check button released. */
    do
    {
        g_gpio_w.p_api->pinRead(g_gpio_w.p_ctrl, ((btn_gpio_port << BSP_IO_PORT_OFFSET) | btn_gpio_num),
                                (bsp_io_level_t *) &pin_status);

        if (pin_status != WIFI_APP_GPIO_BTN_WPS_ACTIVE_STATE)
        {
            printf("\33[2K"  "Start Factory Reset.\n");
            result = WIFI_APP_GPIO_FACTORY_BUTTON_FACTORY_RESET;
            break;
        }
        else
        {
            /* Wait until button is released. */
#if defined(__SUPPORT_EVK_LED__)
            /* Factory Status LED Blink */
            srm_wifi_app_gpio_set_led_state(led_gpio_port, led_gpio_num, WIFI_APP_GPIO_LED_STATE_ON);
            vTaskDelay(portCONVERT_MS_2_TICKS(WIFI_APP_GPIO_CHECK_STEP_FACTORY_BTN_MS)); /* 100ms */
            srm_wifi_app_gpio_set_led_state(led_gpio_port, led_gpio_num, WIFI_APP_GPIO_LED_STATE_OFF);
            vTaskDelay(portCONVERT_MS_2_TICKS(WIFI_APP_GPIO_CHECK_STEP_FACTORY_BTN_MS)); /* 100ms */
#endif
        }
    } while (1);

end:
    return result;
}

#define WAKEUP_HOLD_TIMER_NAME "WAKEUP_HOLD"
#define WAKEUP_HOLD_TIMEOUT_TICKS pdMS_TO_TICKS(5000)
static TimerHandle_t wakeup_hold_timer = NULL;
static bool wakeup_hold = false;

static void wakeup_hold_timer_cb(TimerHandle_t xTimer)
{
    FSP_PARAMETER_NOT_USED(xTimer);

    if (wakeup_hold)
    {
#if CFG_PMGR
        RM_PMGR_W_remove_sleep_constraint(RM_PMGR_W_get_ctrl(), PMGR_CONSTRAINT_SLEEP_PROHIBITED);
#endif
        wakeup_hold = false;
    }
}

static void wakeup_isr(external_irq_callback_args_t *args)
{
    FSP_PARAMETER_NOT_USED(args);
    BaseType_t xHigherPriorityTaskWoken = false;
    BaseType_t xResult;

    if (!wakeup_hold_timer)
    {
        return;
    }

    if (wakeup_hold)
    {
        xResult = xTimerChangePeriodFromISR(wakeup_hold_timer,
                                            WAKEUP_HOLD_TIMEOUT_TICKS,
                                            &xHigherPriorityTaskWoken);
    }
    else
    {
#if CFG_PMGR
        RM_PMGR_W_add_sleep_constraint(RM_PMGR_W_get_ctrl(), PMGR_CONSTRAINT_SLEEP_PROHIBITED);
#endif
        wakeup_hold = true;
        xResult = xTimerStartFromISR(wakeup_hold_timer,
                                    &xHigherPriorityTaskWoken);
    }

    if (xResult == pdPASS)
    {
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

static void wakeup_ext_irq_set(const external_irq_instance_t *p_ext_irq_wakeup)
{
    wakeup_hold_timer = xTimerCreate(WAKEUP_HOLD_TIMER_NAME,
                                     WAKEUP_HOLD_TIMEOUT_TICKS,
                                     pdFALSE,
                                     NULL,
                                     wakeup_hold_timer_cb);
    if (!wakeup_hold_timer)
    {
        return;
    }

    p_ext_irq_wakeup->p_api->open(p_ext_irq_wakeup->p_ctrl, p_ext_irq_wakeup->p_cfg);
    p_ext_irq_wakeup->p_api->callbackSet(p_ext_irq_wakeup->p_ctrl, wakeup_isr, NULL, NULL);
    p_ext_irq_wakeup->p_api->enable(p_ext_irq_wakeup->p_ctrl);
}

int rm_wifi_app_gpio_wakeup_set(bsp_io_port_pin_t port_pin, bsp_io_wakeup_edge_t edge,
                                const external_irq_instance_t *p_ext_irq_wakeup)
{
    bsp_io_wakeup_pin_t wakeup_pin = bsp_prv_port_pin_to_wakeup_gpio(port_pin);

    if (wakeup_pin == 0)
    {
        printf("Error: GPIO_P%d_%d does not support wakeup\n",
               (port_pin >> BSP_IO_PORT_OFFSET) & BSP_IO_PORT_BITS,
               (port_pin & BSP_IO_PIN_BITS));
        return -1;
    }

    /* Set GPIO as wakeup source */
    R_BSP_WakeupSourcePinSetRetained(wakeup_pin, edge);

    /* Configure wakeup ISR */
    if (p_ext_irq_wakeup)
    {
        bsp_io_port_pin_t irq_pin = ((ext_irq_w_extended_cfg_t *)p_ext_irq_wakeup->p_cfg->p_extend)->irq_pin;
        external_irq_trigger_t irq_trigger = p_ext_irq_wakeup->p_cfg->trigger;

        if ((irq_pin == port_pin) && ((bsp_io_wakeup_edge_t)irq_trigger == edge))
        {
            wakeup_ext_irq_set(p_ext_irq_wakeup);
        }
    }

    return 0;
}
#endif
