#include "bsp_api.h"
#include "hal_data.h"
#include "r_gpio_w.h"
#include "r_ospi_w.h"
#include "r_rtc_w.h"
#include "r_wdog_w.h"
#include "rm_block_media_spi_w.h"
#include "rm_lwip_w.h"
#include "rm_vee_flash_w.h"
#include "rm_map_persistant_w.h"
#include "rm_watchdog_service_w.h"
#include "rm_wifi.h"

/* Reuse the BSP-generated OSPI control/configuration and expose the instance
 * shape expected by VEE and block-media. */
static const spi_flash_instance_t g_wifi_ospi =
{
    .p_ctrl = &g_ospi_flash_ctrl,
    .p_cfg  = &g_ospi_flash_cfg,
    .p_api  = &g_ospi_w_on_spi_flash,
};

rm_vee_flash_w_instance_ctrl_t g_vee0_ctrl;
rm_block_media_spi_w_instance_ctrl_t g_rm_block_media0_ctrl;

static const rm_block_media_spi_w_extended_cfg_t g_rm_block_media0_extended_cfg =
{
    .p_spi             = &g_wifi_ospi,
    .block_size_bytes  = 4096,
    .block_count_total = 512,
    .base_address      = 0x2A300000,
    .deny_implicit_erase = false,
};

static void wifi_vee_media_callback(rm_block_media_callback_args_t *p_args)
{
    FSP_PARAMETER_NOT_USED(p_args);
}

static const rm_block_media_cfg_t g_rm_block_media0_cfg =
{
    .p_callback = wifi_vee_media_callback,
    .p_context  = &g_vee0_ctrl,
    .p_extend   = &g_rm_block_media0_extended_cfg,
};

const rm_block_media_instance_t g_rm_block_media0 =
{
    .p_ctrl = &g_rm_block_media0_ctrl,
    .p_cfg  = &g_rm_block_media0_cfg,
    .p_api  = &g_rm_block_media_on_spi,
};

static const rm_vee_flash_w_cfg_t g_vee0_cfg_ext =
{
    .p_flash = NULL,
    .p_media = &g_rm_block_media0,
};

static uint16_t g_vee0_record_offset[2049];

const rm_vee_cfg_t g_vee0_cfg =
{
    .start_addr     = 0x2A300000,
    .num_segments   = 2,
    .total_size     = 0x4000,
    .ref_data_size  = RM_VEE_FLASH_W_REF_DATA_SIZE,
    .record_max_id  = 2048,
    .rec_offset     = g_vee0_record_offset,
    .p_callback     = NULL,
    .p_context      = NULL,
    .p_extend       = &g_vee0_cfg_ext,
};

const rm_vee_instance_t g_vee0 =
{
    .p_ctrl = &g_vee0_ctrl,
    .p_cfg  = &g_vee0_cfg,
    .p_api  = &g_rm_vee_on_flash,
};

map_persistant_w_instance_ctrl_t g_map_persistant_w_ctrl;

static const map_persistant_w_cfg_t g_map_persistant_w_cfg =
{
    .p_extend = NULL,
};

const map_persistant_w_instance_t g_map_persistant_w =
{
    .p_ctrl = &g_map_persistant_w_ctrl,
    .p_cfg  = &g_map_persistant_w_cfg,
    .p_api  = &g_map_persistant_w_api,
};

rtc_w_instance_ctrl_t g_wifi_rtc_ctrl;

static const rtc_extended_cfg_t g_wifi_rtc_cfg_extend =
{
    .reserved = 0U,
};

static const rtc_cfg_t g_wifi_rtc_cfg =
{
    .clock_source       = RTC_CLOCK_SOURCE_SUBCLK,
    .freq_compare_value = 0U,
    .p_err_cfg          = NULL,
    .alarm_ipl          = BSP_IRQ_DISABLED,
    .alarm_irq          = FSP_INVALID_VECTOR,
    .periodic_ipl       = BSP_IRQ_DISABLED,
    .periodic_irq       = FSP_INVALID_VECTOR,
    .carry_ipl          = BSP_IRQ_DISABLED,
    .carry_irq          = FSP_INVALID_VECTOR,
    .p_callback         = NULL,
    .p_context          = NULL,
    .p_extend           = &g_wifi_rtc_cfg_extend,
};

static const rtc_instance_t g_wifi_rtc =
{
    .p_ctrl = &g_wifi_rtc_ctrl,
    .p_cfg  = &g_wifi_rtc_cfg,
    .p_api  = &g_rtc_on_rtc_w,
};

wdog_w_instance_ctrl_t g_wifi_wdog_ctrl;

static const wdog_w_extended_cfg_t g_wifi_wdog_cfg_extend =
{
    .wdt_clk_src = WDOG_W_CLK_SRC_RCLP,
};

static const wdt_cfg_t g_wifi_wdog_cfg =
{
    .timeout        = 0x1FFF,
    .clock_division = WDT_CLOCK_DIVISION_1,
    .window_start   = WDT_WINDOW_START_100,
    .window_end     = WDT_WINDOW_END_0,
    .reset_control  = WDT_RESET_CONTROL_RESET,
    .stop_control   = WDT_STOP_CONTROL_DISABLE,
    .p_callback     = NULL,
    .p_context      = NULL,
    .p_extend       = &g_wifi_wdog_cfg_extend,
};

static const wdt_instance_t g_wifi_wdog =
{
    .p_ctrl = &g_wifi_wdog_ctrl,
    .p_cfg  = &g_wifi_wdog_cfg,
    .p_api  = &g_wdt_on_wdog_w,
};

watchdog_service_w_instance_ctrl_t g_watchdog_service0_ctrl;

static const watchdog_service_cfg_t g_watchdog_service0_cfg =
{
    .p_wdt    = &g_wifi_wdog,
    .p_context = NULL,
    .p_extend  = NULL,
};

const watchdog_service_instance_t g_watchdog_service0 =
{
    .p_ctrl = &g_watchdog_service0_ctrl,
    .p_cfg  = &g_watchdog_service0_cfg,
    .p_api  = &g_watchdog_service_on_watchdog_service_w,
};

const lwip_w_cfg_t g_lwip0_cfg =
{
    .p_watchdog_service = &g_watchdog_service0,
    .p_extend           = NULL,
};

lwip_w_cfg_t const *gp_lwip_w_cfg = &g_lwip0_cfg;

/* Keep the Wi-Fi GPIO instance separate from the BSP-generated IOPORT
 * instance.  The prebuilt driver references this exact control symbol. */
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
    .p_watchdog_service  = &g_watchdog_service0,
    .p_vee_service       = &g_vee0,
    .p_rtc_w_service     = &g_wifi_rtc,
    .p_context           = NULL,
    .p_extend            = NULL,
    .coex_enabled        = 0,
    .coex_cfg            = {0},
};
