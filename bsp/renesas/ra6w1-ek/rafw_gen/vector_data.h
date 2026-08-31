/* generated vector header file - do not edit */
#ifndef VECTOR_DATA_H
#define VECTOR_DATA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <rtconfig.h>
#include "bsp_api.h"

void uart_w_isr(void);
void r_ext_irq_w_isr(void);

/* Wi-Fi middleware interrupt handlers.  These entries are part of the
 * RA6W1 Wi-Fi hardware contract and are not provided by RT-Thread. */
void KDMA_Handler(void);
void TDES_CBC_Handler(void);
void PSK_SHA1_Handler(void);
void SYSPLL_Lock_Handler(void);
void RTC_IF_EXTWK_Handler(void);
void RTC_IF_BLACK_Handler(void);
void RTC_IF_BROWN_Handler(void);
void RTC_IF_PCNT_Handler(void);
void RTC_IF_BCF_MSR_Handler(void);
void RTC_IF_RTC_ACC_Handler(void);
void RTC_IF_EXP_Handler(void);
void RTC_IF_LMR_Handler(void);
void MRM_Handler(void);
void DCACHE_MRM_Handler(void);
void CC312_Handler(void);
void FPLL_Lock_Handler(void);
void hsu_isr(void);
void rxl_mpdu_isr(void);
void txl_transmit_trigger(void);
void txl_prot_trigger(void);
void hal_machw_gen_handler(void);
void hal_machw_bcn_cancellation_handler(void);
void phy_rc_isr(void);

#define VECTOR_NUMBER_WIFI_KDMA_IRQ       ((IRQn_Type) KDMA_IRQn)
#define WIFI_KDMA_IRQ_IRQn                ((IRQn_Type) KDMA_IRQn)
#define VECTOR_NUMBER_WIFI_TDES_CBC_IRQ  ((IRQn_Type) TDES_CBC_IRQn)
#define WIFI_TDES_CBC_IRQ_IRQn            ((IRQn_Type) TDES_CBC_IRQn)
#define VECTOR_NUMBER_WIFI_PSK_SHA1_IRQ   ((IRQn_Type) PSK_SHA1_IRQn)
#define WIFI_PSK_SHA1_IRQ_IRQn            ((IRQn_Type) PSK_SHA1_IRQn)
#define VECTOR_NUMBER_WIFI_SYSPLL_LOCK_IRQ ((IRQn_Type) SYSPLL_LOCK_IRQn)
#define WIFI_SYSPLL_LOCK_IRQ_IRQn          ((IRQn_Type) SYSPLL_LOCK_IRQn)
#define VECTOR_NUMBER_WIFI_RTCW_EXTWK_IRQ ((IRQn_Type) RTC_IF_EXTWK_IRQn)
#define WIFI_RTCW_EXTWK_IRQ_IRQn          ((IRQn_Type) RTC_IF_EXTWK_IRQn)
#define VECTOR_NUMBER_WIFI_RTCW_BLACK_IRQ ((IRQn_Type) RTC_IF_BLACK_IRQn)
#define WIFI_RTCW_BLACK_IRQ_IRQn          ((IRQn_Type) RTC_IF_BLACK_IRQn)
#define VECTOR_NUMBER_WIFI_RTCW_BROWN_IRQ ((IRQn_Type) RTC_IF_BROWN_IRQn)
#define WIFI_RTCW_BROWN_IRQ_IRQn          ((IRQn_Type) RTC_IF_BROWN_IRQn)
#define VECTOR_NUMBER_WIFI_RTCW_PCNT_IRQ  ((IRQn_Type) RTC_IF_PCNT_IRQn)
#define WIFI_RTCW_PCNT_IRQ_IRQn           ((IRQn_Type) RTC_IF_PCNT_IRQn)
#define VECTOR_NUMBER_WIFI_RTCW_BCF_MSR_IRQ ((IRQn_Type) RTC_IF_BCF_MSR_IRQn)
#define WIFI_RTCW_BCF_MSR_IRQ_IRQn        ((IRQn_Type) RTC_IF_BCF_MSR_IRQn)
#define VECTOR_NUMBER_WIFI_RTCW_RTC_ACC_IRQ ((IRQn_Type) RTC_IF_RTC_ACC_IRQn)
#define WIFI_RTCW_RTC_ACC_IRQ_IRQn        ((IRQn_Type) RTC_IF_RTC_ACC_IRQn)
#define VECTOR_NUMBER_WIFI_RTCW_EXP_IRQ   ((IRQn_Type) RTC_IF_EXP_IRQn)
#define WIFI_RTCW_EXP_IRQ_IRQn            ((IRQn_Type) RTC_IF_EXP_IRQn)
#define VECTOR_NUMBER_WIFI_RTCW_LMR_IRQ   ((IRQn_Type) RTC_IF_LMR_IRQn)
#define WIFI_RTCW_LMR_IRQ_IRQn            ((IRQn_Type) RTC_IF_LMR_IRQn)
#define VECTOR_NUMBER_WIFI_MRM_IRQ        ((IRQn_Type) MRM_IRQn)
#define WIFI_MRM_IRQ_IRQn                 ((IRQn_Type) MRM_IRQn)
#define VECTOR_NUMBER_WIFI_DCACHE_MRM_IRQ ((IRQn_Type) DCACHE_MRM_IRQn)
#define WIFI_DCACHE_MRM_IRQ_IRQn          ((IRQn_Type) DCACHE_MRM_IRQn)
#define VECTOR_NUMBER_CC_IRQ              ((IRQn_Type) CC312_IRQn)
#define CC_IRQ_IRQn                       ((IRQn_Type) CC312_IRQn)
#define VECTOR_NUMBER_WIFI_FPLL_LOCK_IRQ ((IRQn_Type) FPLL_LOCK_IRQn)
#define WIFI_FPLL_LOCK_IRQ_IRQn          ((IRQn_Type) FPLL_LOCK_IRQn)
#define VECTOR_NUMBER_WIFI_HSU_IRQ       ((IRQn_Type) WIFI_HSU_IRQn)
#define WIFI_HSU_IRQ_IRQn                ((IRQn_Type) WIFI_HSU_IRQn)
#define VECTOR_NUMBER_WIFI_MACTIMER_IRQ  ((IRQn_Type) WIFI_MACTIMER_IRQn)
#define WIFI_MACTIMER_IRQ_IRQn           ((IRQn_Type) WIFI_MACTIMER_IRQn)
#define VECTOR_NUMBER_WIFI_MACRX_IRQ     ((IRQn_Type) WIFI_MACRX_IRQn)
#define WIFI_MACRX_IRQ_IRQn              ((IRQn_Type) WIFI_MACRX_IRQn)
#define VECTOR_NUMBER_WIFI_MACTX_IRQ     ((IRQn_Type) WIFI_MACTX_IRQn)
#define WIFI_MACTX_IRQ_IRQn              ((IRQn_Type) WIFI_MACTX_IRQn)
#define VECTOR_NUMBER_WIFI_MACPROT_IRQ   ((IRQn_Type) WIFI_MACPROT_IRQn)
#define WIFI_MACPROT_IRQ_IRQn            ((IRQn_Type) WIFI_MACPROT_IRQn)
#define VECTOR_NUMBER_WIFI_MACINTGEN_IRQ ((IRQn_Type) WIFI_MACINTGEN_IRQn)
#define WIFI_MACINTGEN_IRQ_IRQn          ((IRQn_Type) WIFI_MACINTGEN_IRQn)
#define VECTOR_NUMBER_WIFI_MACBCN_IRQ    ((IRQn_Type) WIFI_MACBCN_IRQn)
#define WIFI_MACBCN_IRQ_IRQn             ((IRQn_Type) WIFI_MACBCN_IRQn)
#define VECTOR_NUMBER_WIFI_RC_IRQ        ((IRQn_Type) WIFI_RC_IRQn)
#define WIFI_RC_IRQ_IRQn                 ((IRQn_Type) WIFI_RC_IRQn)

#define VECTOR_NUMBER_UARTW0_IRQ ((IRQn_Type) UART_IRQn) /* UARTW IRQ (Generic interrupt) */
#define UARTW0_IRQ_IRQn          ((IRQn_Type) UART_IRQn) /* UARTW IRQ (Generic interrupt) */

#ifdef BSP_USING_UART1
#define VECTOR_NUMBER_UARTW1_IRQ ((IRQn_Type) UART2_IRQn) /* UARTW2 IRQ (Generic interrupt) */
#define UARTW1_IRQ_IRQn          ((IRQn_Type) UART2_IRQn) /* UARTW2 IRQ (Generic interrupt) */
#endif

#ifdef BSP_USING_UART2
#define VECTOR_NUMBER_UARTW2_IRQ ((IRQn_Type) UART3_IRQn) /* UARTW3 IRQ (Generic interrupt) */
#define UARTW2_IRQ_IRQn          ((IRQn_Type) UART3_IRQn) /* UARTW3 IRQ (Generic interrupt) */
#endif

#define VECTOR_NUMBER_EXTIRQW_P0_IRQ ((IRQn_Type) GPIO_P0_IRQn) /* EXTIRQW P0 IRQ */
#define EXTIRQW_P0_IRQ_IRQn          ((IRQn_Type) GPIO_P0_IRQn) /* EXTIRQW P0 IRQ */
#define VECTOR_NUMBER_EXTIRQW_P1_IRQ ((IRQn_Type) GPIO_P1_IRQn) /* EXTIRQW P1 IRQ */
#define EXTIRQW_P1_IRQ_IRQn          ((IRQn_Type) GPIO_P1_IRQn) /* EXTIRQW P1 IRQ */

#ifdef BSP_USING_SPI
void spi_w_gen_isr(void);
#ifdef BSP_USING_SPI0
#define VECTOR_NUMBER_SPIW0_IRQ ((IRQn_Type) SPI_IRQn) /* SPIW IRQ */
#define SPIW0_IRQ_IRQn          ((IRQn_Type) SPI_IRQn) /* SPIW IRQ */
#endif  /* BSP_USING_SPI0 */

#ifdef BSP_USING_SPI1
#define VECTOR_NUMBER_SPIW1_IRQ ((IRQn_Type) SPI2_IRQn) /* SPIW2 IRQ */
#define SPIW1_IRQ_IRQn          ((IRQn_Type) SPI2_IRQn) /* SPIW2 IRQ */
#endif  /* BSP_USING_SPI1 */
#endif  /* BSP_USING_SPI */

#ifdef BSP_USING_HW_I2C
void i2c_master_w_gen_isr(void);
#ifdef BSP_USING_HW_I2C0
#define VECTOR_NUMBER_I2CW0_IRQ ((IRQn_Type) I2C_IRQn) /* I2CW IRQ */
#define I2CW0_IRQ_IRQn          ((IRQn_Type) I2C_IRQn) /* I2CW IRQ */
#endif /* BSP_USING_HW_I2C0 */

#ifdef BSP_USING_HW_I2C1
#define VECTOR_NUMBER_I2CW1_IRQ ((IRQn_Type) I2C2_IRQn) /* I2CW2 IRQ */
#define I2CW1_IRQ_IRQn          ((IRQn_Type) I2C2_IRQn) /* I2CW2 IRQ */
#endif /* BSP_USING_HW_I2C1 */
#endif /* BSP_USING_HW_I2C */

#ifdef BSP_USING_TIM_W
void r_tim_w_generic_isr(void);
#ifdef BSP_USING_TIM0
#define VECTOR_NUMBER_TIMW0_IRQ ((IRQn_Type) TIMER_IRQn)
#define TIMW0_IRQ_IRQn          ((IRQn_Type) TIMER_IRQn)
#endif
#ifdef BSP_USING_TIM1
#define VECTOR_NUMBER_TIMW1_IRQ ((IRQn_Type) TIMER2_IRQn)
#define TIMW1_IRQ_IRQn          ((IRQn_Type) TIMER2_IRQn)
#endif
#ifdef BSP_USING_TIM2
#define VECTOR_NUMBER_TIMW2_IRQ ((IRQn_Type) TIMER3_IRQn)
#define TIMW2_IRQ_IRQn          ((IRQn_Type) TIMER3_IRQn)
#endif
#ifdef BSP_USING_TIM3
#define VECTOR_NUMBER_TIMW3_IRQ ((IRQn_Type) TIMER4_IRQn)
#define TIMW3_IRQ_IRQn          ((IRQn_Type) TIMER4_IRQn)
#endif
#ifdef BSP_USING_TIM4
#define VECTOR_NUMBER_TIMW4_IRQ ((IRQn_Type) TIMER5_IRQn)
#define TIMW4_IRQ_IRQn          ((IRQn_Type) TIMER5_IRQn)
#endif
#ifdef BSP_USING_TIM5
#define VECTOR_NUMBER_TIMW5_IRQ ((IRQn_Type) TIMER6_IRQn)
#define TIMW5_IRQ_IRQn          ((IRQn_Type) TIMER6_IRQn)
#endif
#ifdef BSP_USING_TIM6
#define VECTOR_NUMBER_TIMW6_IRQ ((IRQn_Type) TIMER7_IRQn)
#define TIMW6_IRQ_IRQn          ((IRQn_Type) TIMER7_IRQn)
#endif
#ifdef BSP_USING_TIM7
#define VECTOR_NUMBER_TIMW7_IRQ ((IRQn_Type) TIMER8_IRQn)
#define TIMW7_IRQ_IRQn          ((IRQn_Type) TIMER8_IRQn)
#endif
#endif /* BSP_USING_TIM_W */

#ifdef __cplusplus
}
#endif

#endif /* VECTOR_DATA_H */
