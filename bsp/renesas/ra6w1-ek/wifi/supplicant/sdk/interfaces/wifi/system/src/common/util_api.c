/**
 ****************************************************************************************
 *
 * @file util_api.c
 *
 * @brief Utility APIs for user function
 *
 * Copyright (c) 2023-2024 Renesas Electronics. All rights reserved.
 *
 * This software ("Software") is owned by Renesas Electronics.
 *
 * By using this Software you agree that Renesas Electronics retains all
 * intellectual property and proprietary rights in and to this Software and any
 * use, reproduction, disclosure or distribution of the Software without express
 * written permission or a license agreement from Renesas Electronics is
 * strictly prohibited. This Software is solely for use on or in conjunction
 * with Renesas Electronics products.
 *
 * EXCEPT AS OTHERWISE PROVIDED IN A LICENSE AGREEMENT BETWEEN THE PARTIES, THE
 * SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. EXCEPT AS OTHERWISE
 * PROVIDED IN A LICENSE AGREEMENT BETWEEN THE PARTIES, IN NO EVENT SHALL
 * RENESAS ELECTRONICS BE LIABLE FOR ANY DIRECT, SPECIAL, INDIRECT, INCIDENTAL,
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THE SOFTWARE.
 *
 ****************************************************************************************
 */

#include "FreeRTOS.h"
#include "custom_config_sdk.h"

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>

#include "common_def.h"
#include "util_api.h"
#include "supp_config.h"
#ifndef RRQ61X_OSPI_W_ENABLED
#include "ad_flash.h"
#endif //!RRQ61X_OSPI_W_ENABLED
#include "lwip/err.h"
#include "common_compile_opt.h"
#include "net_sntp_client.h"

#if defined (__SUPPORT_WIFI_CONN_CB__)
#include "lwip/priv/tcp_priv.h"
#include "net_dhcp_server.h"
#include "dhcpserver.h"
#endif // __SUPPORT_WIFI_CONN_CB__

#ifdef __SUPPORT_REMOVE_MAC_NAME__
#include "app_provision.h"
#endif // __SUPPORT_REMOVE_MAC_NAME__

#if CFG_PMGR
#include "rm_pmgr_w_instance.h"
#endif /* CFG_PMGR */
#include "bsp_sflash_map_ra6w1.h"

#ifdef RRQ61X_OSPI_W_ENABLED
#include <string.h>
#include "r_ospi_w.h"
#include "r_spi_flash_api.h"
#endif //RRQ61X_OSPI_W_ENABLED

#ifndef MQTT_MOCK
#include "rm_wifi.h"

#include "rm_wifi_helper.h"
#endif /* MQTT_MOCK */
#include "rm_vee_flash_w_rrq_nvram.h"
#ifdef RM_MAP_PERSISTANT_W
#include "rm_map_persistant_w.h"
#endif

#undef  UTIL_DEBUG_LOG

//
//// SFLASH user area api /////////////////////////////////////////////////////
//
#ifdef RRQ61X_OSPI_W_ENABLED
static spi_flash_erase_command_t util_erase_command_list[] =
{
    #if ((0x20 > 0) && (4096 > 0))
    { .command = 0x20, .size = 4096 },
    #endif
    #if ((0x52 > 0) && (32768 > 0))
    { .command = 0x52, .size = 32768 },
    #endif
    #if (0xD8 > 0)
    { .command = 0xD8, .size = 65536 },
    #endif
};

ospi_w_xspi_command_set_t util_high_speed_command_set =
{
  .protocol = SPI_FLASH_PROTOCOL_1S_4S_4S,
  .command_bytes = OSPI_W_COMMAND_BYTES_1,
  .read_command = 0xEB,
  .page_program_command = 0x32,
  .write_enable_command = 0x06,
  .status_command = 0x05,
  .read_dummy_cycles = 0x02,
  .program_dummy_cycles = 0,
  .status_dummy_cycles = 0,

  .p_erase_command_list = NULL, /* Use the default commands spi_flash_cfg_t */
  .erase_command_list_length = 0,

};

ospi_w_device_config_t                      util_hw_cfg = {
    .clk_div = OSPI_W_DEVICE_CLK_DIV_2,
    .bus_mode = OSPI_W_DEVICE_BUS_MODE_SINGLE,
    .access_mode = OSPI_W_DEVICE_ACCESS_MODE_AUTO,
    .clock_mode = OSPI_W_DEVICE_CLK_MODE_HIGH,
    .io2_dir = OSPI_W_DEVICE_IO_DIR_AUTO_SEL,
    .io2_value = OSPI_W_DEVICE_IO_VALUE_LOW,
    .io3_dir = OSPI_W_DEVICE_IO_DIR_AUTO_SEL,
    .io3_value = OSPI_W_DEVICE_IO_VALUE_LOW,
    .io4_7_dir = OSPI_W_DEVICE_IO_DIR_AUTO_SEL,
    .io4_7_value = OSPI_W_DEVICE_IO4_7_VALUE_0000,
    .hready_mode = OSPI_W_DEVICE_HREADY_MODE_WAIT,
    .sampling_edge = OSPI_W_DEVICE_SAMPLING_EDGE_NEG,
    .read_pipe = OSPI_W_DEVICE_READ_PIPE_ENABLE,
    .read_pipe_delay = OSPI_W_DEVICE_READ_PIPE_DELAY_7,
    .address_size = OSPI_W_DEVICE_ADDR_SIZE_24,
    .dummy_mode = OSPI_W_DEVICE_DUMMY_MODE_LAST_2_CLK,
    .slew_rate = OSPI_W_DEVICE_SLEW_RATE_0,
    .drive_current = OSPI_W_DEVICE_DRIVE_CURRENT_12,
    .manualmode_config.dir_change_mode = OSPI_W_DEVICE_DIR_CHANGE_MODE_DUMMY_ACCESS,
    .manualmode_config.mapped_addr_rd_acc_response = OSPI_W_DEVICE_MAPPED_ADDR_RD_ACC_RESPONSE_IGNORE,
    .automode_config.full_buffer_mode = OSPI_W_DEVICE_FULL_BUFFER_MODE_BLOCK,
    .automode_config.instruct_size = OSPI_W_DEVICE_INSTRUCT_SZ_1_BYTE,
    .automode_config.burst_len_limit = OSPI_W_DEVICE_BURST_LEN_LIMIT_UNSPECIFIED,
};

ospi_w_device_read_instr_config_t           util_read_instr_config = {
    .enable = 1,
    .instr = 0xEB,
    .instr_extra_byte = 0xA0,
    .instr_bus_mode = OSPI_W_DEVICE_BUS_MODE_SINGLE,
    .addr_bus_mode = OSPI_W_DEVICE_BUS_MODE_QUAD,
    .extra_byte_bus_mode = OSPI_W_DEVICE_BUS_MODE_QUAD,
    .dummy_bus_mode = OSPI_W_DEVICE_BUS_MODE_QUAD,
    .data_bus_mode = OSPI_W_DEVICE_BUS_MODE_QUAD,
    .extra_byte_cfg = OSPI_W_DEVICE_EXTRA_BYTE_ENABLE,
    .extra_byte_half_cfg = OSPI_W_DEVICE_EXTRA_BYTE_HALF_DISABLE,
    .dummy_bytes = 2,
    .instr_mode = OSPI_W_DEVICE_INSTR_MODE_SEND_ONCE,//OSPI_W_DEVICE_INSTR_MODE_SEND_ONCE,
    .idle_state_duration = OSPI_W_DEVICE_IDLE_STATE_DURATION_3,
};

ospi_w_device_wrap_burst_instr_config_t     util_wrap_burst_instr_config = {
    .enable = 0,
};

ospi_w_device_read_status_config_t          util_read_status_config = {
    .enable = 1,
    .instr = 0x05,
    .instr_bus_mode = OSPI_W_DEVICE_BUS_MODE_SINGLE,
    .receive_bus_mode = OSPI_W_DEVICE_BUS_MODE_SINGLE,
    .dummy_bus_mode = OSPI_W_DEVICE_BUS_MODE_SINGLE,
    .busy_pos = 0,
    .busy_val = OSPI_W_DEVICE_BUSY_HIGH,
    .read_stat_del = 0,

    .read_stat_reg_cnt = OSPI_W_DEVICE_READ_STATUS_REG_CNT_RESSTS,
    .dummy_bytes = 0,
    .dummy_val = 0,
};

ospi_w_device_erase_instr_config_t          util_erase_instr_config = {
    .enable = 1,
    .instr = 0x20,
    .instr_bus_mode = OSPI_W_DEVICE_BUS_MODE_SINGLE,
    .addr_bus_mode = OSPI_W_DEVICE_BUS_MODE_SINGLE,
    .hclk_cycles = 14,
    .cs_hi_cycles = 30,
};

ospi_w_device_write_enable_instr_config_t   util_write_enable_instr_config = {
    .enable = 1,
    .instr = 0x06,
    .instr_bus_mode = OSPI_W_DEVICE_BUS_MODE_SINGLE,
};

ospi_w_device_suspend_resume_instr_config_t util_suspend_resume_instr_config = {
    .enable = 1,
    .suspend_instr =  0x75,
    .resume_instr = 0x7A,
    .suspend_bus_mode = OSPI_W_DEVICE_BUS_MODE_SINGLE,
    .resume_bus_mode = OSPI_W_DEVICE_BUS_MODE_SINGLE,
    .read_stat_del = 128,
};

ospi_w_extended_cfg_t  ospi_exteneded_cfg =
{
    .channel =0,
    .p_timing_settings = NULL,
    .p_xspi_command_set_list = &util_high_speed_command_set,
    .xspi_command_set_list_length = 1,
    .p_autocalibration_preamble_pattern_addr = NULL,
    .data_latch_delay_clocks = 0,
#if OSPI_W_CFG_DMAC_SUPPORT_ENABLE
    transfer_instance_t const * p_lower_lvl_transfer;          ///< DMA Transfer instance used for data transmission
#endif
    // automode erase support
    .p_ospi_w_device_cfg = &util_hw_cfg,
    .p_read_instr_cfg = &util_read_instr_config,
    .p_wrap_burst_instr_cfg = &util_wrap_burst_instr_config,
    .p_read_status_instr_cfg = &util_read_status_config,
    .p_write_enable_instr_cfg = &util_write_enable_instr_config,
    .p_erase_instr_cfg = &util_erase_instr_config,
    .p_suspend_resume_instr_cfg = &util_suspend_resume_instr_config,
};
spi_flash_cfg_t util_ospi_cfg =
{
    .spi_protocol = SPI_FLASH_PROTOCOL_1S_1S_1S,
    .read_mode = SPI_FLASH_READ_MODE_FAST_READ_QUAD_IO,
    .address_bytes = SPI_FLASH_ADDRESS_BYTES_3,
    .dummy_clocks = SPI_FLASH_DUMMY_CLOCKS_2, /* RRQ61XXX EVK default  */
    .page_program_address_lines = SPI_FLASH_DATA_LINES_4,
    .page_size_bytes = 256,
    .write_status_bit = 0,
    .write_enable_bit = 1,
    .page_program_command = 0x32,
    .write_enable_command = 0x06,
    .status_command = 0x05,
    .read_command = 0xEB,

    .xip_enter_command = 0U,
    .xip_exit_command = 0U,

    .erase_command_list_length = sizeof(util_erase_command_list) / sizeof(util_erase_command_list[0]),
    .p_erase_command_list = &util_erase_command_list[0],
    .p_extend = &ospi_exteneded_cfg,
};
/** This structure encompasses everything that is needed to use an instance of this interface. */
extern const wifi_cfg_t g_wifi_cfg;
ospi_w_instance_ctrl_t util_ospi_ctrl;
spi_flash_instance_t util_ospi =
{ .p_ctrl = &util_ospi_ctrl, .p_cfg = &util_ospi_cfg, .p_api = &g_ospi_w_on_spi_flash, };
#endif //RRQ61X_OSPI_W_ENABLED

static void util_sflash_open(void)
{
#ifdef RRQ61X_OSPI_W_ENABLED
    R_OSPI_W_Open(&util_ospi_ctrl, &util_ospi_cfg);
#else
    ad_flash_init();
#endif //RRQ61X_OSPI_W_ENABLED
}

bool util_sflash_read(int sflash_addr, void *rd_buf, int len)
{
#ifdef RRQ61X_OSPI_W_ENABLED
    /* RRQ61X runs on the XIP it can read the flash contents through the memcpy() function */
    memcpy((void*)rd_buf, (void*)(sflash_addr|OSPI_W_AUTOMODE_BASE_ADD), len);
    return pdTRUE;

#else ///////////////////////////////////////////////////////////////////

    size_t read_size;

    if (rd_buf == NULL) {
        printf("[%s] Read buffer is NULL\n", __func__);
        return 0;
    }

#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
    R_WDOG_W_Freeze(g_wifi_cfg.p_watchdog_service->p_cfg->p_wdt->p_ctrl, true); // Stop watchdog
#endif
    ad_flash_init();

    read_size = ad_flash_read(sflash_addr, (uint8_t*)rd_buf, len);

#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
    R_WDOG_W_Freeze(g_wifi_cfg.p_watchdog_service->p_cfg->p_wdt->p_ctrl, false); // Start watchdog
#endif

    if (len != read_size) {
        printf("bytes read is wropng (%zu) \n", read_size);
        return pdFALSE;
    }

    return pdTRUE;
#endif //RRQ61X_OSPI_W_ENABLED
}

bool util_sflash_write(int sflash_addr, char *wr_buf, int len)
{
    int addr_offset = 0;
    int buff_offset = 0;
    int tot_len = 0;
    int write_len = 0;

    char *stash_buf = NULL;
    int stash_len = 0;

    addr_offset = sflash_addr;
    buff_offset = (int)wr_buf;
    tot_len = len;

#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
    R_WDOG_W_Freeze(g_wifi_cfg.p_watchdog_service->p_cfg->p_wdt->p_ctrl, true); // Stop watchdog
#endif

    util_sflash_open();

    while (tot_len > 0) {

        if (tot_len > FLASH_SECTOR_SIZE) {
            write_len = FLASH_SECTOR_SIZE;
        } else {
            write_len = tot_len;
        }

        /* Since erasing is always 4KB, stash the data erased on the last write */
        if (write_len < FLASH_SECTOR_SIZE) {
            stash_len = FLASH_SECTOR_SIZE - write_len;

            stash_buf = (char *)pvPortMalloc(stash_len + 1);
            if (stash_buf == NULL) {
            printf("[%s:%d] Failed to allocate buffer(%d bytes)\n", __func__, __LINE__,
                    stash_len + 1);
            goto finish;
            }
            memset(stash_buf, 0x00, stash_len + 1);
#ifdef RRQ61X_OSPI_W_ENABLED
            /* RRQ61X runs on the XIP it can read the flash contents through the memcpy() function */
            memcpy((void*)stash_buf, (void*)((addr_offset + write_len)|OSPI_W_AUTOMODE_BASE_ADD), stash_len);
#else
            ad_flash_read((addr_offset + write_len), (uint8_t*)stash_buf, stash_len);
#endif //RRQ61X_OSPI_W_ENABLED
        }

        /* Erase flash before writing */
#ifdef RRQ61X_OSPI_W_ENABLED
        if (R_OSPI_W_Erase(&util_ospi_ctrl, (uint8_t *)addr_offset, FLASH_SECTOR_SIZE) != FSP_SUCCESS) {
#else
        if (ad_flash_erase_region((uint32_t *)addr_offset, FLASH_SECTOR_SIZE) != pdTRUE) {
#endif //RRQ61X_OSPI_W_ENABLED
            printf("[%s:%d] Flash erase failed(addr=0x%x, size=%d)\n", __func__, __LINE__,
                addr_offset,
                FLASH_SECTOR_SIZE);
            goto finish;
        }

        /* Flash write */
#ifdef RRQ61X_OSPI_W_ENABLED
        if (R_OSPI_W_Write(&util_ospi_ctrl, (uint8_t *)buff_offset, (uint8_t*)addr_offset, (uint32_t)write_len) != FSP_SUCCESS) {
#else
        if (ad_flash_write((uint32_t*)addr_offset, (uint8_t *)buff_offset, (uint32_t)write_len) == 0) {
#endif //RRQ61X_OSPI_W_ENABLED
            printf("[%s:%d] Flash write failed(addr=0x%x, size=%d)\n", __func__, __LINE__,
                addr_offset,
                write_len);
            goto finish;
        }
        addr_offset += write_len;
        buff_offset += write_len;

        /* Stash pop */
        if (stash_len > 0) {
#ifdef RRQ61X_OSPI_W_ENABLED
            if (R_OSPI_W_Write(&util_ospi_ctrl, (uint8_t *)stash_buf, (uint8_t*)addr_offset, (uint32_t)stash_len) != FSP_SUCCESS) {
#else
            if (ad_flash_write((uint32_t*)addr_offset, (uint8_t *)stash_buf, (uint32_t)stash_len) == 0) {
#endif //RRQ61X_OSPI_W_ENABLED
                printf("[%s:%d] Flash write failed(addr=0x%x, size=%d)\n", __func__, __LINE__,
                    addr_offset,
                    stash_len);
                goto finish;
            }
            stash_len = 0;
        }
        tot_len -= write_len;
    }

finish:

#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
    R_WDOG_W_Freeze(g_wifi_cfg.p_watchdog_service->p_cfg->p_wdt->p_ctrl, false); // Start watchdog
#endif

    if (stash_buf) {
        vPortFree(stash_buf);
        stash_buf = NULL;
    }

    if (tot_len != 0) {
        printf("[%s:%d] Failed size = %d)\n", __func__, __LINE__, tot_len);
        return pdFALSE;
    }

    return pdTRUE;
}

bool util_sflash_erase(int sflash_addr, int len)
{
    UINT addr_offset = 0;
    UINT tot_len = 0;
    UINT write_len = 0;

    UCHAR *stash_buf = NULL;
    UINT stash_len = 0;

    addr_offset = sflash_addr;
    tot_len = (UINT)len;

#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
    R_WDOG_W_Freeze(g_wifi_cfg.p_watchdog_service->p_cfg->p_wdt->p_ctrl, true); // Stop watchdog
#endif

    util_sflash_open();

    while (tot_len > 0) {

        if (tot_len > FLASH_SECTOR_SIZE) {
            write_len = FLASH_SECTOR_SIZE;
        } else {
            write_len = tot_len;
        }

        /* Since erasing is always 4KB, stash the data erased on the last write */
        if (write_len < FLASH_SECTOR_SIZE) {
            stash_len = FLASH_SECTOR_SIZE - write_len;

            stash_buf = (UCHAR *)pvPortMalloc(stash_len + 1);
            if (stash_buf == NULL) {
            printf("[%s:%d] Failed to allocate buffer(%d bytes)\n", __func__, __LINE__,
                    stash_len + 1);
            goto finish;
            }
            memset(stash_buf, 0x00, stash_len + 1);
#ifdef RRQ61X_OSPI_W_ENABLED
            /* RRQ61X runs on the XIP it can read the flash contents through the memcpy() function */
            memcpy((void*)stash_buf, (void*)((addr_offset + write_len)|OSPI_W_AUTOMODE_BASE_ADD), stash_len);
#else
            ad_flash_read((addr_offset + write_len), (uint8_t*)stash_buf, stash_len);
#endif //RRQ61X_OSPI_W_ENABLED

        }

        /* Erase flash before writing */
#ifdef RRQ61X_OSPI_W_ENABLED
        if (R_OSPI_W_Erase(&util_ospi_ctrl, (uint8_t *)addr_offset, FLASH_SECTOR_SIZE) != FSP_SUCCESS) {
#else
        if (ad_flash_erase_region((uint32_t *)addr_offset, FLASH_SECTOR_SIZE) != pdTRUE) {
#endif //RRQ61X_OSPI_W_ENABLED
            printf("[%s:%d] Flash erase failed(addr=0x%x, size=%d)\n", __func__, __LINE__,
                addr_offset,
                FLASH_SECTOR_SIZE);
            goto finish;
        }

        /* Stash pop */
        addr_offset += write_len;
        if (stash_len > 0) {
#ifdef RRQ61X_OSPI_W_ENABLED
            if (R_OSPI_W_Write(&util_ospi_ctrl, (uint8_t *)stash_buf, (uint8_t*)addr_offset, (uint32_t)stash_len) != FSP_SUCCESS) {
    #else
            if (ad_flash_write((uint32_t*)addr_offset, (uint8_t *)stash_buf, (uint32_t)stash_len) == 0) {
#endif //RRQ61X_OSPI_W_ENABLED
                printf("[%s:%d] Flash write failed(addr=0x%x, size=%d)\n", __func__, __LINE__,
                    addr_offset,
                    stash_len);
                goto finish;
            }
            stash_len = 0;
        }
        tot_len -= write_len;
    }

finish:

#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
    R_WDOG_W_Freeze(g_wifi_cfg.p_watchdog_service->p_cfg->p_wdt->p_ctrl, false); // Start watchdog
#endif

    if (stash_buf) {
        vPortFree(stash_buf);
        stash_buf = NULL;
    }

    if (tot_len != 0) {
        printf("[%s:%d] Failed size = %d)\n", __func__, __LINE__, tot_len);
        return pdFALSE;
    }

    return pdTRUE;
}

bool util_sflash_copy(int dest_addr, int src_addr, int len)
{
    int  offset = 0, loop_cnt = 0, copy_len = 0, tmp_len = 0;
    char *buf = NULL;

    if ((dest_addr % FLASH_SECTOR_SIZE) || (src_addr % FLASH_SECTOR_SIZE)) {
        printf("[%s] Flash address offset must be 4Kbyte\n", __func__);
        return 0;
    }

    copy_len = len;
    loop_cnt = len / FLASH_SECTOR_SIZE;

    if (loop_cnt > 0) {
        tmp_len = FLASH_SECTOR_SIZE;
    } else {
        tmp_len = len;
    }

    if (len % FLASH_SECTOR_SIZE) {
        loop_cnt = loop_cnt + 1;
    }

    buf = (char *)pvPortMalloc(tmp_len + 1);
    if (buf == NULL) {
        printf("[%s] Fail to alloc memory(%dbytes)\n", __func__, tmp_len + 1);
        return 0;
    }

    while (loop_cnt--) {
        util_sflash_read(src_addr, buf, tmp_len);

        if (util_sflash_erase(dest_addr + offset, tmp_len) != pdTRUE) {
            printf("[%s] Erase failed\n", __func__);
            goto finish;
        }

        if (util_sflash_write((dest_addr + offset), buf, tmp_len) != pdTRUE) {
            printf("[%s] Write failed\n", __func__);
            goto finish;
        }
        offset += tmp_len;
        tmp_len = copy_len - tmp_len;
    }

finish:

    if (buf != NULL) {
        vPortFree(buf);
    }

    if (offset != len) {
        return pdFALSE;
    }

    return pdTRUE;
}

#ifndef MQTT_MOCK
#if defined (__DA16400_PORT__) // used by eembc, scan result sample
//// For get SCAN result API //////////////////////////////////////////////////

#define SCAN_BSSID_IDX       0
#define SCAN_FREQ_IDX        1
#define SCAN_SIGNAL_IDX      2
#define SCAN_FLGA_IDX        3
#define SCAN_SSID_IDX        4

#define HIDDEN_SSID_DETECTION_CHAR    '\t'

#endif /* __DA16400_PORT__  // used by eembc, scan result sample */

///////////////////////////////////////////////////////////////////////////////

//
//// Register Notify callback function for Wi-Fi connection /////////////////////
//

/*
 * Register Customer call-back functions
 */
void wifi_conn_fail_noti_to_atcmd_host(void)
{
    #if (ATCMD_IF_SUPPORT == 1)
    #if defined (__SUPPORT_MQTT__)
    extern void RM_ATCMD_W_CORE_NETWORK_MQTT_set_wfdap_state(int state);
    RM_ATCMD_W_CORE_NETWORK_MQTT_set_wfdap_state(FALSE);
    #endif // __SUPPORT_MQTT__
    #endif
}

///////////////////////////////////////////////////////////////////////////////
//
//// For Factory-Reset APIs //////////////////////////////////////////////////
//
//#include "nvedit.h"
#if CFG_WIFI
int is_in_softap_acs_mode(void)
{
    int tmp_res, tmp_freq;
    int res = pdFALSE;

    if (get_run_mode() != WIFI_DEVICE_MODE_EXT_AP) {
        return pdFALSE;
    }
#ifdef RM_MAP_PERSISTANT_W
    tmp_res = RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG,
		                         (char *)NVR_KEY_CHANNEL, &tmp_freq);
#endif

    if (tmp_res != FSP_SUCCESS) {
        // the key not existing .. softap is not fully set up yet
        return pdFALSE;
    }

    if (tmp_freq == 0) {
        res = pdTRUE;
    }

    return res;
}
#endif
// #endif    /* __SUPPORT_WIFI_CONCURRENT__ */

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#endif /* MQTT_MOCK */

/* EOF */
