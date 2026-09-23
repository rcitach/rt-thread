#include "bsp_api.h"
#include "hal_data.h"
#include "r_gpio_w.h"
#include "rm_wifi.h"

gpio_w_instance_ctrl_t g_gpio_w_ctrl;

const ioport_instance_t g_gpio_w =
{
    .p_ctrl = &g_gpio_w_ctrl,
    .p_cfg  = &g_bsp_pin_cfg,
    .p_api  = &g_ioport_on_gpio_w,
};

void Default_WiFi_Handler(void)
{
    BSP_CFG_HANDLE_UNRECOVERABLE_ERROR(0);
    while (1)
    {
        __NOP();
    }
}

#define WIFI_WEAK_HANDLER __attribute__((weak, alias("Default_WiFi_Handler")))
void KDMA_Handler(void) WIFI_WEAK_HANDLER;
void TDES_CBC_Handler(void) WIFI_WEAK_HANDLER;
void PSK_SHA1_Handler(void) WIFI_WEAK_HANDLER;
void CLKCAL_Handler(void) WIFI_WEAK_HANDLER;
void SYSPLL_Lock_Handler(void) WIFI_WEAK_HANDLER;
void RTC_IF_EXTWK_Handler(void) WIFI_WEAK_HANDLER;
void RTC_IF_BLACK_Handler(void) WIFI_WEAK_HANDLER;
void RTC_IF_BROWN_Handler(void) WIFI_WEAK_HANDLER;
void RTC_IF_PCNT_Handler(void) WIFI_WEAK_HANDLER;
void RTC_IF_BCF_MSR_Handler(void) WIFI_WEAK_HANDLER;
void RTC_IF_RTC_ACC_Handler(void) WIFI_WEAK_HANDLER;
void RTC_IF_EXP_Handler(void) WIFI_WEAK_HANDLER;
void RTC_IF_LMR_Handler(void) WIFI_WEAK_HANDLER;
void MRM_Handler(void) WIFI_WEAK_HANDLER;
void DCACHE_MRM_Handler(void) WIFI_WEAK_HANDLER;
void FPLL_Lock_Handler(void) WIFI_WEAK_HANDLER;
void hsu_isr(void) WIFI_WEAK_HANDLER;
void rxl_mpdu_isr(void) WIFI_WEAK_HANDLER;
void txl_transmit_trigger(void) WIFI_WEAK_HANDLER;
void txl_prot_trigger(void) WIFI_WEAK_HANDLER;
void hal_machw_gen_handler(void) WIFI_WEAK_HANDLER;
void hal_machw_bcn_cancellation_handler(void) WIFI_WEAK_HANDLER;
void phy_rc_isr(void) WIFI_WEAK_HANDLER;

const wifi_cfg_t g_wifi_cfg =
{
    .num_uarts           = 0,
    .num_sockets         = wificonfigMAX_SOCKETS,
    .reset_pin           = BSP_IO_PORT_00_PIN_00,
    .p_watchdog_service  = NULL,
    .p_vee_service       = NULL,
    .p_rtc_w_service     = NULL,
    .p_context           = NULL,
    .p_extend            = NULL,
    .coex_enabled        = 0,
    .coex_cfg            = {0},
};
