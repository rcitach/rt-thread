/**
 ****************************************************************************************
 *
 * @file net_arp.c
 *
 * @brief ARP handling module
 *
 * Copyright (c) 2023 Renesas Electronics. All rights reserved.
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


#include <strings.h>
#include "rm_lwip_w_cfg.h" /* For RM_LWIP_W_CLEANED */
#include "net_arp.h"
#include "net_network_main.h"

#if LWIP_IPV4 && LWIP_ARP

#include "net_ip_handler.h"
#include "lwip/prot/iana.h"
#include "netif/ethernet.h"
#include "clib.h"
#include "rm_lwip_w_helper.h"
#if CFG_PMGR
#include "rm_pmgr_w_instance.h"
#include "sleep_mgmt_regs.h"
#include "rm_pmgr_w_rtm_internal.h"
#endif /* CFG_PMGR */

#if CFG_PMGR
extern int RM_PMGR_W_dpm_is_enabled(void);
extern int RM_PMGR_W_dpm_is_wakeup(void);
extern int RM_WIFI_dpm_supp_is_connected(void);
extern void RM_WIFI_dpm_arp_filter_set(unsigned long accept_addr, unsigned long subnet_addr);
extern void dpm_accept_tim_arp_resp(unsigned long gw_ip, unsigned char *gw_mac);
#endif /* CFG_PMGR */
extern void etharp_print_arp_table(u32_t iface);

err_t
etharp_send(struct netif *netif, const struct eth_addr *ethsrc_addr,
           const struct eth_addr *ethdst_addr,
           const struct eth_addr *hwsrc_addr, const ip4_addr_t *ipsrc_addr,
           const struct eth_addr *hwdst_addr, const ip4_addr_t *ipdst_addr,
           const u16_t opcode);
void etharping_init(void * param);

struct cmd_arping_param * etharping_arg;

UINT print_arp_table(UINT iface)
{
    etharp_print_arp_table(iface);

    return pdPASS;
}

int arp_entry_delete(int iface)
{
    struct netif *netif;

    netif = netif_get_by_index(RM_WIFI_NETIF_INDEX(iface));


    if (netif != NULL) {
        ARP_DEBUG_PRINT("WLAN[%d] ARP Entry deleted\n", iface);
        etharp_cleanup_netif (netif);
    } else {
        ARP_DEBUG_PRINT("\nWLAN[%d] Network Interface is NULL\n", iface);
        return pdFAIL;
    }

    return pdPASS;
}


extern err_t dhcp_etharp_request(struct netif *netif, const ip4_addr_t *ipaddr);
int dhcp_arp_request(int iface, ULONG ipaddr)
{
    struct netif *netif;

    if (iface != WLAN0_IFACE && iface != WLAN1_IFACE) {
        goto error;
    }

    netif = netif_get_by_index(RM_WIFI_NETIF_INDEX(iface));

    if (netif == NULL) {
        goto error;
    }

    if (ipaddr == 0) {
        dhcp_etharp_request(netif, netif_ip4_addr(netif));
    } else {
        struct ip4_addr dst_ip_addr;
        dst_ip_addr.addr=PP_HTONL(ipaddr);

        printf("[%s] %ld.%ld.%ld.%ld\n", __func__,
                ipaddr >> 24 & 0xff,
                ipaddr >> 16 & 0xff,
                ipaddr >>  8 & 0xff,
                ipaddr       & 0xff);

        dhcp_etharp_request(netif, &dst_ip_addr);
    }

    return pdPASS;

error:
    printf("\n[WLAN%d] Network Interface is NULL\n", iface);
    return pdFAIL;
}


int arp_request(ip4_addr_t ipaddr, int iface)
{
    err_t status;

    if (iface != WLAN0_IFACE && iface != WLAN1_IFACE) {
        goto error;
    }

    struct netif *netif;

    netif = netif_get_by_index(RM_WIFI_NETIF_INDEX(iface));

    if (netif == NULL) {
        goto error;
    }

    /* ARP */
    status = etharp_request(netif, &ipaddr);

    if (status != ERR_OK) {
        return pdFAIL;
    }

    return pdPASS;

error:
    printf("\n[WLAN%d] Network Interface is NULL\n", iface);
    return pdFAIL;
}

int arp_response(int iface, char *dst_ipaddr,  char *dst_macaddr)
{
    ARP_DEBUG_PRINT("arp_response()\n");

    struct netif *netif;
    ip4_addr_t sipaddr;
    struct eth_addr shwaddr;
    err_t status = pdPASS;
    UINT  idx, len;
    char *tmp_addr;

    len = strlen(dst_macaddr);
    tmp_addr = pvPortMalloc(3);
    memset(tmp_addr, 0, 3);
    netif = netif_get_by_index(RM_WIFI_NETIF_INDEX(iface));
    ipaddr_aton(dst_ipaddr, (ip_addr_t *)&sipaddr);

    if (len == 12 || len == 17) {
        for (idx = 0; idx < len; idx++) {
            if (len == 17 && ((idx+ 1)%3 == 0)) {
                idx++;
            }
        
            if (!isxdigit((int)(dst_macaddr[idx]))) {
                printf("MAC Addr is invalid.\n");
                vPortFree(tmp_addr);
                return pdFAIL;
            }
        }

        for(idx = 0; idx < 6; idx++) {
            if (len == 12) {
                strncpy(tmp_addr, dst_macaddr+(idx*2), 2);
            } else if (len == 17) {
                strncpy(tmp_addr, dst_macaddr+(idx*3), 2);
            }
            shwaddr.addr[idx] = strtoul(tmp_addr, NULL, 16);
        }
#ifdef RA6WX_ARP_DEBUG
        ARP_DEBUG_PRINT("Dst mac addr-%02x:%02x:%02x:%02x:%02x:%02x\n",
                shwaddr.addr[0], shwaddr.addr[1],
                shwaddr.addr[2], shwaddr.addr[3],
                shwaddr.addr[4], shwaddr.addr[5]);
#endif /* RA6WX_ARP_DEBUG */
    } else {
        printf("MAC Addr is invalid.\n");

        vPortFree(tmp_addr);

        return pdFAIL;
    }

    vPortFree(tmp_addr);
    status = etharp_send(netif,
                       (struct eth_addr *)netif->hwaddr, &shwaddr,
                       (struct eth_addr *)netif->hwaddr, netif_ip4_addr(netif),
                       &shwaddr, &sipaddr,
                       ARP_REPLY);
    if (status != ERR_OK) {
        return pdFAIL;
    }

    return pdPASS;
}

int garp_request(int iface, int check_ipconflict)
{
    struct netif *netif;

    netif = netif_get_by_index(RM_WIFI_NETIF_INDEX(iface));

    if (netif == NULL) {
        printf("Network Interface is NULL\n");
        return pdFAIL;
    }

    /* GARP */
    err_t status = etharp_gratuitous(netif);

    if (status == ERR_OK) {
        ARP_DEBUG_PRINT("\n[WLAN%d] Send GARP\n", iface);
    } else if (status == ERR_USE && check_ipconflict == pdTRUE) {
        printf("\n[WLAN%d] Address already in use.\n", iface);
    } else {
        printf("\n[WLAN%d] Send GARP Error: %d\n", iface, status);
        return pdFAIL;
    }

    return pdPASS;
}

UINT arp_send_for_ip_collision_avoid(int t_static_ip)
{
    UINT    status = pdFALSE;

#ifdef __SUPPORT_DHCPC_IP_TO_STATIC_IP__
    if (t_static_ip) {    // Temporarily use a static IPaddress.
        printf(">>> Send DHCP_ARP to check IP collision...\n");
        status = dhcp_arp_request(WLAN0_IFACE, 0);
    } else
#else
    RA6W1_UNUSED_ARG(t_static_ip);
#endif /* __SUPPORT_DHCPC_IP_TO_STATIC_IP__ */
    {
        // Default Static IP Address
        printf(">>> Send GARP to check IP collision...\n");
        status = garp_request(WLAN0_IFACE, pdTRUE);
    }

    return status;
}


bool cmd_arping(int argc, char *argv[])
{
    etharping_arg = pvPortMalloc(sizeof(struct cmd_arping_param));
    memset(etharping_arg, 0, sizeof(struct cmd_arping_param));

    if (!ra6w1_network_main_is_wlaninit()) {
        printf("Wi-Fi is not initialized.\n");
        return pdTRUE;
    }

    /* parsing */
    for (int n = 0; n < argc; n++) {
        /* IP Address / Domain */
        if ((strlen(argv[n]) > 4) && (strchr(argv[n], '.') != NULL)) {
            if (is_in_valid_ip_class(argv[n])) {    /* check ip */
                ip4addr_aton(argv[n], &(etharping_arg->ipaddr));
            } else {
                printf("IP Address failed.\n");
                return true;
            }
        }

        /* -n */
        if (*argv[n] == '-' && (*(argv[n]+1) == 'n' || *(argv[n]+1) == 'c')) {
            etharping_arg->count = atol(argv[n + 1]);
        }

        /* -t */
        if (*argv[n] == '-' && *(argv[n]+1) == 't') {
            etharping_arg->count = 0xFFFFFFFF;
        }

        /* -w */
        if (*argv[n] == '-' && *(argv[n]+1) == 'w') {
            etharping_arg->wait = ctoi(argv[n + 1]);
        }

        /* -i */
        if (*argv[n] == '-' && *(argv[n]+1) == 'i') {
            etharping_arg->interval = ctoi(argv[n + 1]);
        }

        /* -I Interface */
        if (*argv[n] == '-' && *(argv[n]+1) == 'I') {
            if (strcasecmp(argv[n + 1], "WLAN0") == 0) {
                etharping_arg->ping_interface = WLAN0_IFACE;
            } else if (strcasecmp(argv[n + 1], "WLAN1") == 0) {
                etharping_arg->ping_interface = WLAN1_IFACE;
            } else {
                goto arping_help;
            }
        }
    }

    if ((argc < 2) || (strcmp(argv[1], "help") == 0) || (etharping_arg->ipaddr.addr == 0)) {
arping_help:
        printf("Usage: arping -I [interface][ip] -n [count]"
               "-w [timeout] -i [interval]\n"
               "\t-I [wlan0|wlan1]\tInterface name\n"
               "\t-n or -c count\tStop after sending count ARP REQUEST.\n"
               "\t-w timeout\tSpecify a timeout, in milliseconds, "
               "before arping exists regardless of how many packets "
               "have been sent or received. (Min:10ms)\n"
               "\t-i interval\tInterval in milliseconds to wait "
               "for each reply.(MIN:10ms)\n\n"
               "\tex) arping 172.16.0.1 -n 10 -w 1000  -i 1000\n");

        vPortFree(etharping_arg);
        return true;
    }
    etharping_init(etharping_arg);

    return true;
}


/** 
 * In DPM Abnormal routine, check the default gw field in the arp table
 * Success(Exist) Return 0  , Fail(Not exist) Return 1;
 */
extern unsigned int ra6w1_network_main_check_ip_addr(unsigned char iface);

UINT etharp_exist_defgw_addr(void)
{
    const ip4_addr_t *gw_addr;
    struct eth_addr *ethaddr;
    struct netif *netif;
    unsigned long gw_ulongaddr;
    const ip4_addr_t *ip_ret;

    /* Find the netif by interface number */
    /* Default Station address for DPM */
    netif = netif_get_by_index(RM_WIFI_NETIF_INDEX(WLAN0_IFACE));

    if (netif == NULL) {
        return pdFALSE;
    }

    /* check the default gw address */
    gw_addr = netif_ip4_gw(netif);
    gw_ulongaddr = ip4_addr_get_u32(gw_addr);

    /* check the ip address */
    if (ra6w1_network_main_check_ip_addr(0) == pdFALSE) {
        return pdFALSE;
    }

    if (gw_ulongaddr == IPADDR_ANY || gw_ulongaddr == IPADDR_NONE) {
        return pdFALSE;
    }

    if (etharp_find_addr(netif, gw_addr, &ethaddr, &ip_ret) >= 0) {
#if CFG_PMGR
        if (RM_PMGR_W_dpm_is_enabled()) {
        	dpm_accept_tim_arp_resp(gw_ulongaddr, ethaddr->addr);
        }
#endif /* CFG_PMGR */
        return pdTRUE;
    }

    return pdFALSE;
}

/*
    Get mac address for input IP addr from arp table
    
    net_if_id  : [IN] WLAN0_IFACE or WLAN1_IFACE
    ipaddr_tmp : [IN] IP address
    ethaddr_tmp: [OUT] MAC address found for input IP address

    return : Operation result (pdTRUE / pdFALSE)
*/
unsigned int etharp_get_mac_from_ip(uint8_t net_if_id, ip4_addr_t * input_ip_addr, uint8_t * eth_addr)
{
    const ip4_addr_t * gw_addr;
    struct eth_addr * ethaddr;
    struct netif * netif;
    unsigned long ipaddr_tmp_long = 0, gw_addr_long = 0;
    const ip4_addr_t * ip_ret;
    int index;

    /* Find the netif by interface id */
    netif = netif_get_by_index(RM_WIFI_NETIF_INDEX(net_if_id));

    if (netif == NULL) {
        return pdFALSE;
    }

    /* Get the default gateway IP address */
    gw_addr = netif_ip4_gw(netif);
    gw_addr_long = ip4_addr_get_u32(gw_addr);
    
    /* Get the IP address in long */
    ipaddr_tmp_long = ip4_addr_get_u32(input_ip_addr); 

    /* Check the IP address' validity */
    if (ra6w1_network_main_check_ip_addr(net_if_id) == pdFALSE) {
        return pdFALSE;
    }

    if (ipaddr_tmp_long == IPADDR_ANY || ipaddr_tmp_long == IPADDR_NONE) {
        return pdFALSE;
    }

    if (gw_addr_long == IPADDR_ANY || gw_addr_long == IPADDR_NONE) {
        return pdFALSE;
    }

    index = etharp_find_addr(netif, input_ip_addr, &ethaddr, &ip_ret);

    if (index >= 0) {
        /* Found in arp_table */
        memcpy(eth_addr, &(ethaddr->addr[0]), 6);
        return pdTRUE;
    } else {
        /* Not found, hence get MAC address for default gateway */
        index = etharp_find_addr(netif, gw_addr, &ethaddr, &ip_ret);
        if (index >= 0) {
            memcpy(eth_addr, &(ethaddr->addr[0]), 6);            
            return pdTRUE;
        } else {
            return pdFALSE;
        }
    }
}

int do_autoarp_check(void)
{
    const ip4_addr_t *net_addr;
    unsigned long ulongaddr;

    struct netif *netif;

    if (etharp_exist_defgw_addr() == 0) {
        netif = netif_get_by_index(RM_WIFI_NETIF_INDEX(WLAN0_IFACE));

        if (netif == NULL) {
            printf("[%s] Netif is wrong\n", __func__);
            return -1;
        }

        /* check interface ip address */
        net_addr = netif_ip4_addr(netif);
        ulongaddr = ip4_addr_get_u32(net_addr);                    

        if (ulongaddr == IPADDR_ANY || ulongaddr == IPADDR_NONE) {
            return -1;
        }

        /* check the default gw address */
        net_addr = netif_ip4_gw(netif);
        ulongaddr = ip4_addr_get_u32(net_addr);                    

        if (ulongaddr == IPADDR_ANY || ulongaddr == IPADDR_NONE) {
            printf("[%s] gw ipaddr(%lx) is wrong\n", __func__ , ulongaddr);
            return -1;
        }

        if (etharp_request(netif, net_addr) == ERR_OK) {
#ifdef DEBUG_ARP_CHECK
#if CFG_PMGR
            if (!RM_PMGR_W_dpm_is_wakeup()) {
#endif /* CFG_PMGR */
                printf(YELLOW_COLOR " [%s] GW ARP Request\n" CLEAR_COLOR, __func__);
#if CFG_PMGR
            }
#endif /* CFG_PMGR */
#endif // DEBUG_ARP_CHECK
            return 0; //ARP Request OK  (ARP response status is unknown.)
        }
    }
#ifdef DEBUG_ARP_CHECK
#if CFG_PMGR
    else if (!RM_PMGR_W_dpm_is_wakeup()) {
        printf(YELLOW_COLOR " [%s] GW ARP Found\n" CLEAR_COLOR, __func__);
    }
#else
    else {
        printf(YELLOW_COLOR " [%s] GW ARP Found\n" CLEAR_COLOR, __func__);
    }
#endif /* CFG_PMGR */
#endif // DEBUG_ARP_CHECK

    return 0;
}

/* If we need to send ARP REQ BC for DPM  */ 
void do_dpm_autoarp_send(void)
{
    const ip4_addr_t *gw_addr;
    unsigned long gw_ulongaddr;
    struct netif *netif;

        netif = netif_get_by_index(RM_WIFI_NETIF_INDEX(WLAN0_IFACE));

    if (netif == NULL) {
        printf("[%s] Netif is wrong\n", __func__);
        return;
    }

    /* check the default gw address */
    gw_addr = netif_ip4_gw(netif);
    gw_ulongaddr = ip4_addr_get_u32(gw_addr);                    
    if (gw_ulongaddr == IPADDR_ANY || gw_ulongaddr == IPADDR_NONE) {
        printf("[%s] gw ipaddr(%lx) is wrong\n", __func__ , gw_ulongaddr);
        return ;
    }
    
    etharp_request(netif , gw_addr);

    return;
}

#if CFG_PMGR
void do_set_dpm_ip_net(void)
{
    const ip4_addr_t *ip_addr , *net_mask;
    unsigned long ip_ulongaddr;
    unsigned long netmask_ulong;
    struct netif *netif;

        netif = netif_get_by_index(RM_WIFI_NETIF_INDEX(WLAN0_IFACE));
        
    if (netif == NULL) {
        printf("[%s] Netif is wrong\n", __func__);
        return;
    }

    /* check the default gw address */
    ip_addr = netif_ip4_addr(netif);
    ip_ulongaddr = ip4_addr_get_u32(ip_addr);                    

    net_mask = netif_ip4_netmask(netif);
    netmask_ulong = ip4_addr_get_u32(net_mask);    

    if (ip_ulongaddr == IPADDR_ANY || ip_ulongaddr == IPADDR_NONE) {
        printf("[%s] gw ipaddr(%lx) is wrong\n", __func__ , ip_ulongaddr);
        return ;
    }

    RM_WIFI_dpm_arp_filter_set(ip_ulongaddr, netmask_ulong);
}
#endif /* CFG_PMGR */

//manage arp table for dpm
#ifdef RM_LWIP_W_CLEANED
extern void *get_arp_table();
struct rm_etharp_entry *arp_table;
#else
extern struct etharp_entry arp_table[ARP_TABLE_SIZE];
#endif /* RM_LWIP_W_CLEANED */

#if CFG_PMGR
void save_arp_table(void)
{
#ifdef RM_LWIP_W_CLEANED
    struct rm_etharp_entry    *_dpm_arp;
#else
    struct etharp_entry    *_dpm_arp;
#endif /* RM_LWIP_W_CLEANED */

    struct netif *netif;
    
    if (!RM_PMGR_W_dpm_is_enabled()) {
        return;
    }

#ifdef RM_LWIP_W_CLEANED
    arp_table = (struct rm_etharp_entry *)get_arp_table();
#endif /* RM_LWIP_W_CLEANED */
        netif = netif_get_by_index(RM_WIFI_NETIF_INDEX(WLAN0_IFACE));
        
    if (netif == NULL) {
        printf("[%s] Netif is wrong\n", __func__);
        return;
    }
    
    RM_PMGR_W_rtm_static_get(RTM_STATIC_KEY_ARP_INFO_PTR, NULL, NULL, (void**)(&_dpm_arp));
    if (_dpm_arp == NULL) {
        return;
    }

    if (ARP_ALLOC_SZ >= sizeof(arp_table)) {
#ifdef RM_LWIP_W_CLEANED
        memcpy(&_dpm_arp[0], arp_table, ARP_ALLOC_SZ);
#else
        memcpy(&_dpm_arp[0], &arp_table, ARP_ALLOC_SZ);
#endif /* RM_LWIP_W_CLEANED */
    } else {
#ifdef RM_LWIP_W_CLEANED
        memcpy(&_dpm_arp[0], arp_table,
            ((ARP_ALLOC_SZ / sizeof(struct rm_etharp_entry)) * sizeof(struct rm_etharp_entry)));
#else
        memcpy(&_dpm_arp[0], &arp_table,
            ((ARP_ALLOC_SZ / sizeof(struct etharp_entry)) * sizeof(struct etharp_entry)));
#endif /* RM_LWIP_W_CLEANED */
    }
    
    return;
}

int restore_arp_table(void)
{
#ifdef RM_LWIP_W_CLEANED
    struct rm_etharp_entry    *_dpm_arp;
#else
    struct etharp_entry    *_dpm_arp;
#endif /* RM_LWIP_W_CLEANED */
    int i;
    struct netif *netif;

        netif = netif_get_by_index(RM_WIFI_NETIF_INDEX(WLAN0_IFACE));
        
    if (netif == NULL) {
        printf("[%s] Netif is wrong\n", __func__);
        return -1;
    }

    if (RM_PMGR_W_dpm_is_enabled() == pdTRUE) {
        RM_PMGR_W_rtm_static_get(RTM_STATIC_KEY_ARP_INFO_PTR, NULL, NULL, (void**)(&_dpm_arp));
        if (_dpm_arp == NULL) {
            return -1;
        }

        if (RM_PMGR_W_dpm_is_wakeup() == 1 && RM_WIFI_dpm_supp_is_connected() == 1) {
#ifdef RM_LWIP_W_CLEANED
            memcpy(arp_table, &_dpm_arp[0], ARP_ALLOC_SZ);
#else
            memcpy(&arp_table, &_dpm_arp[0], ARP_ALLOC_SZ);
#endif
            for (i = 0; i < ARP_TABLE_SIZE; i++) {
#ifdef RM_LWIP_W_CLEANED
                (arp_table + i)->q = NULL;
                if (netif)
                    (arp_table + i)->netif = netif;
                else
                    (arp_table + i)->netif = NULL;
#else
                arp_table[i].q = NULL;
                if (netif)
                    arp_table[i].netif = netif;
                else
                    arp_table[i].netif = NULL;
#endif /* RM_LWIP_W_CLEANED */
            }            
        } else {
            // ARP Table of Retention is clearing
            memset(_dpm_arp, 0, ARP_ALLOC_SZ);
        }
    }

    return 0;
}

void cleanup_arp_table(void)
{
#ifdef RM_LWIP_W_CLEANED
    struct rm_etharp_entry    *_dpm_arp;
#else
    struct etharp_entry    *_dpm_arp;
#endif /* RM_LWIP_W_CLEANED */

    if (RM_PMGR_W_dpm_is_enabled() == pdFALSE) {
        return;
    }

    RM_PMGR_W_rtm_static_get(RTM_STATIC_KEY_ARP_INFO_PTR, NULL, NULL, (void**)(&_dpm_arp));

    if (_dpm_arp == NULL) {
        return;
    }

    // ARP Table of Retention is clearing
    memset(_dpm_arp, 0, ARP_ALLOC_SZ);

    return;
}
#endif /* CFG_PMGR */

//Polling Thread in DPM
TaskHandle_t poll_state_chk_thd = NULL;
UINT ra6w1_arp_create_polling_state_check(UINT iface)
{
    BaseType_t    xRtn = 0;

#ifndef    __SUPPORT_SIGMA_TEST__
    if (iface == WLAN0_IFACE) {
        /* Network Interface Initialize */
        xRtn = xTaskCreate(polling_state_check,
                                "NetPollSts",
                                128 * 4,
                                (void *) NULL,
                                tskIDLE_PRIORITY + 10,
                                &poll_state_chk_thd);
        if (xRtn != pdPASS) {
            printf(RED_COLOR " [%s] Failed task create %s \r\n" CLEAR_COLOR, __func__, "NetPollSts");
            return pdFALSE;
        }
    }
#endif    /* __SUPPORT_SIGMA_TEST__ */

    return xRtn;
}

#else

UINT print_arp_table(UINT iface)
{
    return pdFAIL;
}

int arp_entry_delete(int iface)
{
    return pdFAIL;
}



int dhcp_arp_request(int iface, ULONG ipaddr)
{
    return pdFAIL;
}

#if LWIP_IPV4
int arp_request(ip4_addr_t ipaddr, int iface)
{
    return pdFAIL;
}
#endif

int arp_response(int iface, char *dst_ipaddr,  char *dst_macaddr)
{
    return pdFAIL;
}

int garp_request(int iface, int check_ipconflict)
{
    return pdFAIL;
}

UINT arp_send_for_ip_collision_avoid(int t_static_ip)
{
    return pdFAIL;}


bool cmd_arping(int argc, char *argv[])
{
    return pdFAIL;
}

UINT etharp_exist_defgw_addr(void)
{
    return pdFAIL;
}

int do_autoarp_check(void)
{
    return pdFAIL;
}

/* If we need to send ARP REQ BC for DPM  */ 
void do_dpm_autoarp_send(void)
{
    return;
}

void do_set_dpm_ip_net(void)
{
    return;
}


void save_arp_table(void)
{
    return;
}

int restore_arp_table(void)
{
    return pdFAIL;
}


void cleanup_arp_table(void)
{
    return;
}


UINT ra6w1_arp_create_polling_state_check(UINT iface)
{
    return pdFAIL;
}

#endif /* LWIP_IPV4 && LWIP_ARP */

/* EOF */
