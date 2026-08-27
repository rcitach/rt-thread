/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include "bsp_api.h"

#if CFG_PMGR
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "rm_wifi_dpm.h"
#ifdef RM_MAP_PERSISTANT_W
#include "rm_map_persistant_w.h"
#include dg_configADNVPARAM_PROJ_FILE
#endif
#include "rm_pmgr_w_dpm_socket_internal.h"

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#define dpm_swab32(x) ((UINT32)(                \
    (((UINT32)(x) & (UINT32)0x000000ffUL) << 24) |        \
    (((UINT32)(x) & (UINT32)0x0000ff00UL) <<  8) |        \
    (((UINT32)(x) & (UINT32)0x00ff0000UL) >>  8) |        \
    (((UINT32)(x) & (UINT32)0xff000000UL) >> 24)))

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
enum dpm_enum_dbg_level {
    MSG_ERROR = 1, MSG_WARNING, MSG_INFO, MSG_DEBUG, MSG_EXCESSIVE, MSG_MSGDUMP
};

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/


/***********************************************************************************************************************
 * Global Variables
 **********************************************************************************************************************/
unsigned char dpm_dbg_cmd_flag = pdFALSE;


/***********************************************************************************************************************
 * Private Functions
 *********************************************************************************************************************/



/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/
void RM_WIFI_dpm_udp_port_filter_delete(unsigned short d_port)
{
    unsigned char index = 0;
    unsigned short    reg_port = 0;

    for (index = 0; index < DPM_MAX_UDP_FILTER ; index++) {
        reg_port = romac4rtos_get_udport(index);
        if(d_port == reg_port) {
            romac4rtos_set_udport(index, 0);
            return;
        }
    }
}

void RM_WIFI_dpm_tcp_port_delete(unsigned short d_port)
{
    unsigned char index = 0;
    unsigned short    reg_port = 0;

    for (index = 0; index < DPM_MAX_TCP_FILTER ; index++) {
        reg_port = romac4rtos_get_tcport(index);
        if(d_port == reg_port) {
#ifdef  ENABLE_DPM_DBG_LOG
            PRINTF("[DPM] %s Reg TCP Port(%d) is deleted !\n", __func__, d_port);
#endif  /* ENABLE_DPM_DBG_LOG */
            romac4rtos_set_tcport(index, 0);
            return;
        }
    }
}

/** UDP Port filter Primitive
 * udp filter max cnt < 7
 */
void RM_WIFI_dpm_udp_port_filter_set(unsigned short d_port)
{
    unsigned char no , index = 0;
    unsigned short    reg_port = 0;

#ifdef    ENABLE_DPM_DBG_LOG
    PRINTF(YELLOW_COLOR "[DPM] %s Reg UDP Port(%x) is Start \n" CLEAR_COLOR, __func__, d_port);
#endif    /* ENABLE_DPM_DBG_LOG */
    for(index = 0; index < DPM_MAX_UDP_FILTER ; index++) {
        reg_port = romac4rtos_get_udport(index);
        if(reg_port == 0) break;
        if(d_port == reg_port) {
#ifdef    ENABLE_DPM_DBG_LOG
            PRINTF("[DPM] %s Reg UDP Port(%x) is already registered\n", __func__, d_port);
#endif    /* ENABLE_DPM_DBG_LOG */
            return;
        }
    }

    no = index;

    if (no >= DPM_MAX_UDP_FILTER) {
#ifdef    ENABLE_DPM_DBG_LOG
        PRINTF("[DPM] %s Reg UDP Port number(%d) Over\n", __func__, no);
#endif    /* ENABLE_DPM_DBG_LOG */
        return;
    }

#ifdef    ENABLE_DPM_DBG_LOG
    PRINTF("\n[%s] New UDP filter[%d] d_port=%d\n", __func__, no, d_port);
#endif    /* ENABLE_DPM_DBG_LOG */
    romac4rtos_set_udport(no, d_port);

    return;
}

/** TCP Port filter Primitive
 * tcp filter max cnt < 7
 */
void RM_WIFI_dpm_tcp_port_filter_set(unsigned short d_port)
{
    unsigned char no , index = 0;
    unsigned short    reg_port = 0;

    for(index = 0; index < DPM_MAX_TCP_FILTER ; index++) {
        reg_port = romac4rtos_get_tcport(index);
        if(reg_port == 0) break;
        if(d_port == reg_port) {
#ifdef    ENABLE_DPM_DBG_LOG
            PRINTF("[DPM] %s Reg TCP Port(%d) is already registered\n", __func__, d_port);
#endif    /* ENABLE_DPM_DBG_LOG */
            return;
        }
    }

    no = index;

    if (no >= DPM_MAX_TCP_FILTER) {
#ifdef    ENABLE_DPM_DBG_LOG
        PRINTF("[DPM] %s Reg TCP Port number(%d) Over\n", __func__, no);
#endif    /* ENABLE_DPM_DBG_LOG */
        return;
    }

#ifdef    ENABLE_DPM_DBG_LOG
    PRINTF("\n[%s] New TCP filter[%d] d_port=%d\n", __func__, no, d_port);
#endif    /* ENABLE_DPM_DBG_LOG */
    romac4rtos_set_tcport(no, d_port);

    return;
}

int RM_WIFI_dpm_udp_port_hole_punch_set(int period /* keep period times */,
                                        uint32_t dst_ip,
                                        uint32_t *dst_ip6,
                                        unsigned short src_port,
                                        unsigned short dest_port)
{
    /* Check if there's an empmty slot in the UDPH array, get an empty slot idx */
    if (ptim_udph_conf_free_slot_get() == -1) 
    {
        printf("[%s:%d] UDPH config slot is full ! \n", __func__, __LINE__);
        return pdFAIL;
    }

    return ptim_udph_conf_add(dst_ip, dst_ip6, src_port, dest_port, period);
}

/**
 * TIM wakeup count setting
 */
void RM_WIFI_dpm_ptim_wakeup_count_set(int dtim_period, int saveflag)
{
    if (dpm_dbg_cmd_flag == pdTRUE && RTM_FLAG_PTR->dpm_dbg_level >= MSG_EXCESSIVE) {
        PRINTF("- DPM Tim Wakeup time : %d * 100 msec\n", dtim_period);
    }

    if (dtim_period < MIN_DPM_TIM_WAKEUP || dtim_period > MAX_DPM_TIM_WAKEUP) {
        PRINTF("DPM TIM wake up range: 1 ~ 6000 \n");
        return;
    }
    RTM_FLAG_PTR->dpm_dtim_period = dtim_period;

    if (saveflag) {
        PRINTF("- DPM Tim Wakeup time : %d * 100 msec\n", dtim_period);
        /* If AP connection is done, you should reboot */
        
#ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                    WIFI_PROFILE_DPM_TIM_WAKEUP_COUNT, dtim_period);
#endif
        PRINTF("- AP Connection is already done. System reboot to apply...\n");
    }

    if (RM_PMGR_W_dpm_is_wakeup()) {
        romac4rtos_req_period(dtim_period, (RTM_FLAG_PTR->dpm_keepalive_time_msec / 100));
        romac4rtos_ready(pdTRUE, 0);
    }
}

/** Multicast mac address(last 3 bytes) is passing
 * mc_filter_cnt is 0 ~ 7
 */
void RM_WIFI_dpm_ptim_mc_filter_set( unsigned long before_mc_addr)
{
    unsigned char mc_filter_cnt;
    unsigned long mc_oui_addr, mc_addr , after_mc_addr;
#if 0
    struct dpm_param *dpmp = GET_DPMP();
#endif

    unsigned char index = 0;

    after_mc_addr = dpm_swab32(before_mc_addr);
    
    for(index = 0; index < DPM_ACCEPT_MC_CNT ; index++) {
        //mc_addr = dpm_get_env_mc_ip(GET_DPMP(), index);
        mc_addr = romac4rtos_get_mcip(index);
        if(mc_addr == 0) break;
        if(mc_addr == after_mc_addr) {
#ifdef    ENABLE_DPM_DBG_LOG
            PRINTF("[DPM] %s Reg MC IP(%08x) is already registered\n", __func__, before_mc_addr);
#endif    /* ENABLE_DPM_DBG_LOG */
            return;
        }
    }
    
    if (index >= DPM_ACCEPT_MC_CNT) {
#ifdef    ENABLE_DPM_DBG_LOG
        PRINTF("[DPM] %s Reg MC IP(%d:%08x) Over\n", __func__, index , before_mc_addr);
#endif    /* ENABLE_DPM_DBG_LOG */
        return;
    }
        
    mc_filter_cnt = index;
    mc_addr = after_mc_addr;

    PRINTF("[DPM] Set Multicast Address Filter(%dth %08lx)\n", mc_filter_cnt , mc_addr);

    /** DPM RX Filter Setting
    * Matched MC IP Enabe , Drop MC IP Disable
    */
#if 0
    /* Drop MC IP Enable */
    if((dpm_get_env_rx_filter(dpmp) & (DPM_F_DROP_MC_IP)))
        dpm_set_env_rx_filter(dpmp, (dpm_get_env_rx_filter(dpmp) & ~(DPM_F_DROP_MC_IP)));
#endif

#if 0
    if(!(dpm_get_env_rx_filter(dpmp) & (DPM_F_MATCHED_MC_IP)))
        dpm_set_env_rx_filter(dpmp, dpm_get_env_rx_filter(dpmp) | DPM_F_MATCHED_MC_IP);
#endif

    /* multicast ip filter set */
    //dpm_set_env_mc_ip(dpmp , mc_filter_cnt , mc_addr);
    romac4rtos_set_mcip(mc_filter_cnt , mc_addr);

    /* multicast mac filter set */
    mc_oui_addr = dpm_swab32(before_mc_addr & 0x00ffffff);
    //dpm_set_env_bcmc_accept_oui_n(dpmp , mc_filter_cnt , mc_oui_addr);
    romac4rtos_set_accept_oui(mc_filter_cnt , mc_oui_addr);

    return;
}

void RM_WIFI_dpm_ptim_mcv6_filter_set(uint16_t *mcipv6)
{
    unsigned char mc_filter_cnt;
    unsigned char index = 0;

    uint16_t *mc_addr;

    for (index = 0; index < DPM_ACCEPT_IPV6_MC_CNT ; index++) {
        mc_addr = romac4rtos_get_mcipv6(index);
        if (*mc_addr == 0) {
            break;
        }
        if (memcmp(mc_addr, mcipv6, 16) == 0) {
            PRINTF("[DPM] %s Reg MC IP is already registered\n", __func__);
            return;
        }
    }

    if (index >= DPM_ACCEPT_IPV6_MC_CNT) {
#ifdef    ENABLE_DPM_DBG_LOG
        PRINTF("[DPM] %s Reg MC IP(%d) Over\n", __func__, index);
#endif    /* ENABLE_DPM_DBG_LOG */
        return;
    }

    mc_filter_cnt = index;
    memcpy(mc_addr, mcipv6, 16);

    PRINTF("[DPM] Set Multicast Address Filter(%dth)\n", mc_filter_cnt);

    /* multicast ip filter set */
    romac4rtos_set_mcipv6(mc_filter_cnt , mc_addr);

    return;
}

void RM_WIFI_dpm_ptim_wakeup_count_set_from_nvram(void)
{
    int count;

#ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                               ENV_GROUP_WIFIPROFILE,
                               WIFI_PROFILE_DPM_TIM_WAKEUP_COUNT,
                               &count);
#endif

    if (count == -1) {
#ifdef __DPM_TIM_WAKEUP_TIME_MSEC__
        count = (DFLT_DPM_TIM_WAKEUP_COUNT * 102.4);
#else
        count = DFLT_DPM_TIM_WAKEUP_COUNT;
#endif /* __DPM_TIM_WAKEUP_TIME_MSEC__ */
    }
    RM_WIFI_dpm_ptim_wakeup_count_set(count, 0);
}

/**
 * IP Address Condition setting(to enter DPM)
 */
fsp_err_t RM_WIFI_dpm_ip_condition_set(unsigned char ip_condition)
{
    if ((ip_condition != PMGR_CONDITION_IPV4_MANDATORY)
            && (ip_condition != PMGR_CONDITION_IPV6_MANDATORY)
            && (ip_condition != PMGR_CONDITION_IPV46_MANDATORY)) {
        return FSP_ERR_INVALID_ARGUMENT;
    }

#ifdef RM_MAP_PERSISTANT_W
    if (RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG,
                                NVR_KEY_DPM_IP_CONDITION, ip_condition)){
        return FSP_ERR_WRITE_FAILED;
    }
#endif

    return FSP_SUCCESS;
}

void RM_WIFI_dpm_ip_condition_get(unsigned char * p_ip_condition)
{
#ifdef RM_MAP_PERSISTANT_W
    int ip_condition = PMGR_CONDITION_IPV4_MANDATORY;

    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG,
                                   NVR_KEY_DPM_IP_CONDITION, &ip_condition)) {
        *p_ip_condition = PMGR_CONDITION_IPV4_MANDATORY;
    } else {
        *p_ip_condition = (unsigned char)ip_condition;
    }
#endif
    return;
}

#else
/* Dummy functions for rwnx_drv / macsw */
void RM_WIFI_dpm_ptim_wakeup_count_set(int dtim_period, int saveflag)
{
    return;
}

void RM_WIFI_dpm_ptim_wakeup_count_set_from_nvram(void)
{
    return;
}

#endif /* CFG_PMGR */
