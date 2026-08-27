
/**
 ****************************************************************************************
 *
 * @file common_slave.h
 *
 * @brief host i/f for application
 *
 * Copyright (C) 2015-2023 Renesas Electronics.
 * This computer program includes Confidential, Proprietary Information
 * of Renesas Electronics. All Rights Reserved.
 *
 ****************************************************************************************
 */


#ifndef __COMMON_SLAVE_H__
#define __COMMON_SLAVE_H__

#define SPI_SLAVE_BUFFER_SIZE   (4096)
#define SPI_SLAVE_TX_BUFFER_SIZE   (4096)
#define spi_slaveTEMPLATE_TASK_PRIORITY         ( OS_TASK_PRIORITY_HIGHEST )

#define SPI_SLAVE_BUSWIDTH      (8)
#undef SPI_SLAVE_WRITE_SEQUENCE_GPIO

#undef  HOST_IF_USE_CRC

#undef SLAVE_DEBUG_EN
#ifdef SLAVE_DEBUG_EN
#define SLAVE_PRINTF(...)       printf(__VA_ARGS__)
#else
#define SLAVE_PRINTF(...)
#endif

typedef struct{
        uint16 preamble;
        uint16 length;
#ifdef HOST_IF_USE_CRC
        uint32 host_if_crc;
#endif
} host_if_header_t;

typedef struct{
        uint32 word_num;
        uint32 device_id;
        uint32 open_cs;
        uint32 close_cs;
        uint32 dma_ch;
} spi_flags_t;

typedef enum  _SLAVE_IF_EVENT_type_ {
        SLAVE_IF_EVT_SPI_RX_DONE = 0x1,
        SLAVE_IF_EVT_SPI_TX_DONE = 0x2,
        SLAVE_IF_EVT_SPI_TX_REQ = 0x4,
        SLAVE_IF_EVT_MAX = 0xffffffff
} SLAVE_IF_EVNET_TYPE ;

typedef void (*spi_slave_rx_callback)(uint8_t *buf, uint32_t transferred);

extern QueueHandle_t slave_int_evt;
extern QueueHandle_t slave_tx_int_evt;
extern QueueHandle_t master_rx_int_evt;
extern QueueHandle_t master_gpio_int_evt;



/**
 * \brief initialize spi slave
 *
 * \note
 *
 * \param [in] id HW_SPI1 or HW_SPI2
 *
 */
void init_spi_slave(HW_SPI_ID id);

/**
 * \brief register rx callback
 *
 * \note
 *
 * \param [in] cb callback function
 *
 */
void spi_slave_register_callback(spi_slave_rx_callback cb);

/**
 * \brief initialize slave to master GPIO interrupt pad
 *
 * \note
 *
 * \param [in] port GPIO port number
 * \param [in] pin GPIO pin number
 *
 */
void slave_interrupt_gpio_set(HW_GPIO_PORT port, HW_GPIO_PIN pin);

/**
 * \brief send data to host
 *
 * \note
 *
 * \param [in] buf data to send
 * \param [in] len length to send
 * \param [in] timeout if 0 return immediately, else wait w/ timeout value
 *
 */
void spi_slave_write(uint8_t *buf, uint32_t len, uint32_t timeout);

#endif //__COMMON_SLAVE_H__
