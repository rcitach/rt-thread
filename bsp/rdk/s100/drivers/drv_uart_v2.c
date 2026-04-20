/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-04-20     rcitach      first version
 */


#include <rtthread.h>
#include <rtdevice.h>
#include "interrupt.h"

#ifdef RT_USING_SERIAL_V2

#define UART_MAX_COUNT                 (3u)
#define UART_SYS_CLK                   (200000000u)
#define UART_CONFIG_TIMEOUT            (0xffu)
#define UART_FCR_DEFVAL                (UART_FCR_FIFO_EN | UART_FCR_RXSR | UART_FCR_TXSR)

#define SYS_REG_UART_CTRL              ((volatile rt_uint32_t *)0x23660084u)

#define UART_4_BASE                    (0x23400000u)
#define UART_5_BASE                    (0x23410000u)
#define UART_6_BASE                    (0x23420000u)

#define UART_CTRL_UART4_RX_IN_MASK     (0x00000001u)
#define UART_CTRL_UART5_RX_IN_MASK     (0x00000010u)
#define UART_CTRL_UART6_RX_IN_MASK     (0x00000100u)

#define UART_FCR_FIFO_DIS              (0x00u)
#define UART_FCR_FIFO_EN               (0x01u)
#define UART_FCR_CLEAR_RCVR            (0x02u)
#define UART_FCR_CLEAR_XMIT            (0x04u)
#define UART_FCR_RXSR                  (0x02u)
#define UART_FCR_TXSR                  (0x04u)
#define UART_FCR_RX_TRIGGER_MASK       (0xC0u)
#define UART_FCR_RX_TRIGGER_8          (0x80u)

#define UART_LCR_WLS_MSK               (0x03u)
#define UART_LCR_WLS_BASE              (5u)
#define UART_LCR_STB                   (0x04u)
#define UART_LCR_PEN                   (0x08u)
#define UART_LCR_EPS                   (0x10u)
#define UART_LCR_DLAB                  (0x80u)

#define UART_LSR_DR                    (0x01u)
#define UART_LSR_THRE                  (0x20u)

#define UART_IIR_NO_INT                (0x01u)
#define UART_IIR_ID                    (0x0eu)
#define UART_IIR_THRI                  (0x02u)
#define UART_IIR_RDI                   (0x04u)
#define UART_IIR_RLSI                  (0x06u)
#define UART_IIR_BUSY_DETECT           (0x07u)
#define UART_IIR_CHAR_TIMEOUT          (0x0cu)

/*
 * These are the definitions for the Interrupt Enable Register
 */
#define UART_IER_MSI    (0x08U)      /**< Enable Modem status interrupt */
#define UART_IER_RLSI   (0x04U)      /**< Enable receiver line status interrupt */
#define UART_IER_THRI   (0x02U)      /**< Enable Transmitter holding register int. */
#define UART_IER_RDI    (0x01U)      /**< Enable receiver data interrupt */

#define UART4_IRQn                     (45)
#define UART5_IRQn                     (46)
#define UART6_IRQn                     (47)

#define UART_USR_BUSY                  (0x01u)
#define UART_USR_TFNF                  (0x02u)
#define UART_USR_RFNE                  (0x08u)

typedef struct
{
    volatile rt_uint32_t RBR;
    volatile rt_uint32_t IER;
    volatile rt_uint32_t FCR;
    volatile rt_uint32_t LCR;
    volatile rt_uint32_t MCR;
    volatile rt_uint32_t LSR;
    volatile rt_uint32_t MSR;
    volatile rt_uint32_t RESERVED1[21];
    volatile rt_uint32_t FAR;
    volatile rt_uint32_t TFR;
    volatile rt_uint32_t RFW;
    volatile rt_uint32_t USR;
    volatile rt_uint32_t TFL;
    volatile rt_uint32_t RFL;
    volatile rt_uint32_t RESERVED2[7];
    volatile rt_uint32_t HTX;
    volatile rt_uint32_t DMASA;
    volatile rt_uint32_t RESERVED3[5];
    volatile rt_uint32_t DLF;
} s100_uart_reg_t;

struct s100_uart
{
    struct rt_serial_device serial;
    s100_uart_reg_t *regs;
    rt_uint32_t rx_mask;
    int irqno;
    rt_uint16_t rx_bufsz;
    rt_uint16_t tx_bufsz;
    rt_uint32_t fcr_shadow;
    const char *name;
};

#if defined(BSP_USING_UART4)
#define S100_UART4_DESC                     \
    {                                       \
        .regs = (s100_uart_reg_t *)UART_4_BASE, \
        .rx_mask = UART_CTRL_UART4_RX_IN_MASK,  \
        .irqno = UART4_IRQn,                    \
        .rx_bufsz = BSP_UART4_RX_BUFSIZE,       \
        .tx_bufsz = BSP_UART4_TX_BUFSIZE,       \
        .name = "uart4",                        \
    }
#endif

#if defined(BSP_USING_UART5)
#define S100_UART5_DESC                     \
    {                                       \
        .regs = (s100_uart_reg_t *)UART_5_BASE, \
        .rx_mask = UART_CTRL_UART5_RX_IN_MASK,  \
        .irqno = UART5_IRQn,                    \
        .rx_bufsz = BSP_UART5_RX_BUFSIZE,       \
        .tx_bufsz = BSP_UART5_TX_BUFSIZE,       \
        .name = "uart5",                        \
    }
#endif

#if defined(BSP_USING_UART6)
#define S100_UART6_DESC                     \
    {                                       \
        .regs = (s100_uart_reg_t *)UART_6_BASE, \
        .rx_mask = UART_CTRL_UART6_RX_IN_MASK,  \
        .irqno = UART6_IRQn,                    \
        .rx_bufsz = BSP_UART6_RX_BUFSIZE,       \
        .tx_bufsz = BSP_UART6_TX_BUFSIZE,       \
        .name = "uart6",                        \
    }
#endif

static struct s100_uart s100_uarts[] =
{
#if defined(BSP_USING_UART4)
    S100_UART4_DESC,
#endif

#if defined(BSP_USING_UART5)
    S100_UART5_DESC,
#endif

#if defined(BSP_USING_UART6)
    S100_UART6_DESC,
#endif

};

static void s100_uart_config_default(struct s100_uart *uart)
{
    struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT;

    config.baud_rate = BAUD_RATE_921600;
    config.rx_bufsz = uart->rx_bufsz;
    config.tx_bufsz = uart->tx_bufsz;

    uart->serial.config = config;
}


static void s100_uart_rx_drain(struct s100_uart *uart)
{
    rt_bool_t rx_indicated = RT_FALSE;

    while ((uart->regs->USR & UART_USR_RFNE) != 0u)
    {
        rt_uint8_t ch = (rt_uint8_t)(uart->regs->RBR & 0xffu);

        if (uart->serial.serial_rx != RT_NULL)
        {
            rt_hw_serial_control_isr(&uart->serial, RT_HW_SERIAL_CTRL_PUTC, &ch);
            rx_indicated = RT_TRUE;
        }
    }

    if (rx_indicated != RT_FALSE)
    {
        rt_hw_serial_isr(&uart->serial, RT_SERIAL_EVENT_RX_IND);
    }
}

static void s100_uart_isr(int vector, void *param)
{
    struct s100_uart *uart = (struct s100_uart *)param;
    rt_uint32_t iir;

    RT_UNUSED(vector);

    /* IIR shares the FCR offset; read it with IIR semantics here. */
    iir = *((volatile rt_uint32_t *)&uart->regs->FCR) & 0x0fu;
    if ((iir & UART_IIR_NO_INT) != 0u)
    {
        return;
    }

    switch (iir & UART_IIR_ID)
    {
    case UART_IIR_RDI:
    case UART_IIR_CHAR_TIMEOUT:
        s100_uart_rx_drain(uart);
        break;
    case UART_IIR_RLSI:
        /* Reading LSR clears line status sources, then drain any pending data. */
        (void)uart->regs->LSR;
        s100_uart_rx_drain(uart);
        break;
    case UART_IIR_BUSY_DETECT:
        (void)uart->regs->USR;
        break;
    case UART_IIR_THRI:
    default:
        break;
    }
}

static void s100_uart_clear_irq(struct s100_uart *uart)
{
    uart->regs->IER &= ~(UART_IER_RDI | UART_IER_RLSI | UART_IER_THRI | UART_IER_MSI);
}

static void s100_uart_fcr_write(struct s100_uart *uart, rt_uint32_t val)
{
    uart->fcr_shadow = val;
    uart->regs->FCR = uart->fcr_shadow;
}

static void s100_uart_set_rx_trigger(struct s100_uart *uart, rt_uint32_t trigger)
{
    uart->fcr_shadow &= ~UART_FCR_CLEAR_RCVR;
    uart->fcr_shadow &= ~UART_FCR_CLEAR_XMIT;
    uart->fcr_shadow &= ~UART_FCR_RX_TRIGGER_MASK;
    uart->fcr_shadow |= trigger;
    uart->regs->FCR = uart->fcr_shadow;
}

static rt_err_t s100_uart_config_prepare(struct s100_uart *uart)
{
    rt_uint32_t timeout = UART_CONFIG_TIMEOUT;

    while (((uart->regs->USR & UART_USR_BUSY) != 0u) && (timeout != 0u))
    {
        s100_uart_fcr_write(uart, UART_FCR_FIFO_DIS);
        s100_uart_fcr_write(uart, UART_FCR_CLEAR_RCVR);
        s100_uart_fcr_write(uart, UART_FCR_CLEAR_XMIT);
        timeout--;
    }

    return (timeout != 0u) ? RT_EOK : -RT_ETIMEOUT;
}

static void s100_uart_set_baud(struct s100_uart *uart, rt_uint32_t baud_rate)
{
    rt_uint32_t baud_div_x64;
    rt_uint32_t baud_div_int;
    rt_uint32_t baud_div_fraction;

    baud_div_x64 = (UART_SYS_CLK * 4u) / baud_rate;
    baud_div_int = baud_div_x64 / 64u;
    if (baud_div_int == 0u)
    {
        baud_div_int = 1u;
    }
    baud_div_fraction = baud_div_x64 - (baud_div_int * 64u);

    uart->regs->LCR |= UART_LCR_DLAB;
    uart->regs->DLF = baud_div_fraction;
    uart->regs->RBR = baud_div_int & 0xffu;
    uart->regs->LCR &= ~UART_LCR_DLAB;
}

static rt_err_t s100_uart_set_lcr(struct s100_uart *uart, struct serial_configure *cfg)
{
    rt_uint32_t lcr = 0;

    switch (cfg->data_bits)
    {
    case DATA_BITS_5:
    case DATA_BITS_6:
    case DATA_BITS_7:
    case DATA_BITS_8:
        lcr |= (cfg->data_bits - UART_LCR_WLS_BASE) & UART_LCR_WLS_MSK;
        break;
    default:
        return -RT_EINVAL;
    }

    switch (cfg->stop_bits)
    {
    case STOP_BITS_1:
        break;
    case STOP_BITS_2:
        lcr |= UART_LCR_STB;
        break;
    default:
        return -RT_EINVAL;
    }

    switch (cfg->parity)
    {
    case PARITY_NONE:
        break;
    case PARITY_ODD:
        lcr |= UART_LCR_PEN;
        break;
    case PARITY_EVEN:
        lcr |= UART_LCR_PEN | UART_LCR_EPS;
        break;
    default:
        return -RT_EINVAL;
    }

    uart->regs->LCR &= ~(UART_LCR_WLS_MSK | UART_LCR_STB | UART_LCR_PEN | UART_LCR_EPS);
    uart->regs->LCR |= lcr;

    return RT_EOK;
}

static void s100_uart_set_fifo(struct s100_uart *uart)
{
    s100_uart_clear_irq(uart);
    s100_uart_fcr_write(uart, UART_FCR_DEFVAL);
    s100_uart_set_rx_trigger(uart, UART_FCR_RX_TRIGGER_8);
}

static rt_err_t s100_uart_configure(struct rt_serial_device *serial,
                                    struct serial_configure *cfg)
{


    struct s100_uart *uart;
    rt_err_t ret;

    RT_ASSERT(serial != RT_NULL);
    RT_ASSERT(cfg != RT_NULL);

    if ((cfg->baud_rate == 0u) || (cfg->flowcontrol != RT_SERIAL_FLOWCONTROL_NONE))
    {
        return -RT_EINVAL;
    }

    uart = (struct s100_uart *)serial->parent.user_data;

    (*(uint32_t *)SYS_REG_UART_CTRL) |= uart->rx_mask;
    uart->regs->MCR = 0u;

    ret = s100_uart_config_prepare(uart);
    if (ret == RT_EOK)
    {
        s100_uart_set_baud(uart, cfg->baud_rate);
        ret = s100_uart_set_lcr(uart, cfg);
        s100_uart_set_fifo(uart);
    }

    (*(uint32_t *)SYS_REG_UART_CTRL) &= ~uart->rx_mask;

    return ret;
}

static rt_err_t s100_uart_control(struct rt_serial_device *serial, int cmd, void *arg)
{
    struct s100_uart *uart;
    rt_ubase_t ctrl_arg = (rt_ubase_t) arg;

    RT_ASSERT(serial != RT_NULL);

    uart = (struct s100_uart *)serial->parent.user_data;

    if(ctrl_arg & (RT_DEVICE_FLAG_RX_BLOCKING | RT_DEVICE_FLAG_RX_NON_BLOCKING))
    {
        ctrl_arg = RT_DEVICE_FLAG_INT_RX;
    }
    else if(ctrl_arg & (RT_DEVICE_FLAG_TX_BLOCKING | RT_DEVICE_FLAG_TX_NON_BLOCKING))
    {
        ctrl_arg = RT_DEVICE_FLAG_INT_TX;
    }

    switch (cmd)
    {
        case RT_DEVICE_CTRL_CLR_INT:
            if (ctrl_arg == RT_DEVICE_FLAG_INT_RX)
            {
                /* disable rx irq */
                uart->regs->IER &= ~(UART_IER_RDI | UART_IER_RLSI);
            }
            else if (ctrl_arg == RT_DEVICE_FLAG_INT_TX)
            {
                /* disable tx irq */
                uart->regs->IER &= (~UART_IER_THRI);
            }
            break;

        case RT_DEVICE_CTRL_SET_INT:
            if (ctrl_arg == RT_DEVICE_FLAG_INT_RX)
            {
                /* enable rx irq */
                uart->regs->IER |= (UART_IER_RDI | UART_IER_RLSI);
                rt_hw_interrupt_umask(uart->irqno);

            } else if (ctrl_arg == RT_DEVICE_FLAG_INT_TX)
            {
                /* enable tx irq */
                uart->regs->IER |= UART_IER_THRI;
                rt_hw_interrupt_umask(uart->irqno);
            }

            break;
        case RT_DEVICE_CTRL_CONFIG:
            return s100_uart_control(serial, RT_DEVICE_CTRL_SET_INT, (void *)ctrl_arg);
        case RT_DEVICE_CTRL_CLOSE:
            uart->regs->IER &= ~(UART_IER_RDI | UART_IER_RLSI | UART_IER_THRI | UART_IER_MSI);
            rt_hw_interrupt_mask(uart->irqno);
            break;
        default:
            break;
    }
    return RT_EOK;
}

static int s100_uart_putc(struct rt_serial_device *serial, char c)
{

    struct s100_uart *uart;

    RT_ASSERT(serial != RT_NULL);

    uart = (struct s100_uart *)serial->parent.user_data;

    while ((uart->regs->LSR & UART_LSR_THRE) == 0u)
    {
    }

    uart->regs->RBR = (rt_uint8_t)c;
    return 1;
}

static int s100_uart_getc(struct rt_serial_device *serial)
{
    struct s100_uart *uart;

    RT_ASSERT(serial != RT_NULL);

    uart = (struct s100_uart *)serial->parent.user_data;

    if ((uart->regs->USR & UART_USR_RFNE) == 0u)
    {
        return -1;
    }

    return (int)(uart->regs->RBR & 0xffu);
}

static rt_ssize_t s100_uart_transmit(struct rt_serial_device *serial,
                                     rt_uint8_t *buf,
                                     rt_size_t size,
                                     rt_uint32_t tx_flag)
{
    rt_size_t i;

    RT_ASSERT(serial != RT_NULL);
    RT_ASSERT(buf != RT_NULL);
    RT_UNUSED(tx_flag);

    for (i = 0; i < size; i++)
    {
        while ((((struct s100_uart *)serial->parent.user_data)->regs->USR & UART_USR_TFNF) == 0u)
        {
        }
        s100_uart_putc(serial, (char)buf[i]);
    }

    return size;
}

static const struct rt_uart_ops s100_uart_ops =
{
    .configure = s100_uart_configure,
    .control = s100_uart_control,
    .putc = s100_uart_putc,
    .getc = s100_uart_getc,
    .transmit = s100_uart_transmit,
};

int rt_hw_uart_init(void)
{
    rt_err_t ret = RT_EOK;
    rt_size_t i;

    for(i = 0; i < sizeof(s100_uarts) / sizeof(s100_uarts[0]); i++)
    {
        s100_uarts[i].serial.ops = &s100_uart_ops;
        s100_uart_config_default(&s100_uarts[i]);
        s100_uarts[i].serial.parent.user_data = &s100_uarts[i];

        ret = rt_hw_serial_register(&s100_uarts[i].serial,
                                    s100_uarts[i].name,
                                    RT_DEVICE_FLAG_RDWR,
                                    (void*)&s100_uarts[i]);
        if (ret != RT_EOK)
        {
            return ret;
        }

        rt_hw_interrupt_install(s100_uarts[i].irqno,s100_uart_isr, &s100_uarts[i], s100_uarts[i].name);
    }

    return ret;
}
INIT_BOARD_EXPORT(rt_hw_uart_init);
#endif /* RT_USING_SERIAL_V2 */

