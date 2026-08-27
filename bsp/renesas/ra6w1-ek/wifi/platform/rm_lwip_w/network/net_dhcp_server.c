/**
 ****************************************************************************************
 *
 * @file net_dhcp_server.c
 *
 * @brief DHCP Server module
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
#include "bsp_api.h"

#if CFG_WIFI /* Compile only with WiFi stack */

#include "net_dhcp_server.h"
#include "sys_feature.h"
#include "dhcpserver_options.h"
#include "dhcpserver.h"
#include "lwip/err.h"
#include "rm_lwip_w_helper.h"

#ifndef ER_NOT_SUCCESSFUL
#define ER_NOT_SUCCESSFUL 0x11
#endif
#ifndef ER_SIZE_ERROR
#define ER_SIZE_ERROR 0x04
#endif
#ifndef ER_NOT_FOUND
#define ER_NOT_FOUND 0x16
#endif
#include "rm_vee_flash_w_rrq_nvram.h"
#ifdef RM_MAP_PERSISTANT_W
#include "rm_map_persistant_w.h"
#endif

#if !defined (__SUPPORT_IPV6__)
#pragma GCC diagnostic ignored "-Waggregate-return"
#endif // !__SUPPORT_IPV6__

#define IP_ADDRESS(a, b, c, d)    ((((ULONG)a) << 24) | (((ULONG)b) << 16) | (((ULONG)c) << 8) | ((ULONG)d))
#define NX_DHCP_NO_ADDRESS        IP_ADDRESS(0, 0, 0, 0)

#define NX_DHCP_IP_ADDRESS_MAX_LIST_SIZE      10

// keywords of stored data in nvram
static const char *DHCP_SERVER_CONF_START_IP      = DHCP_SERVER_START_IP;
static const char *DHCP_SERVER_CONF_END_IP        = DHCP_SERVER_END_IP;
static const char *DHCP_SERVER_CONF_LEASE_TIME    = DHCP_SERVER_LEASE_TIME;
static const char *DHCP_SERVER_CONF_DNS           = DHCP_SERVER_DNS;

#if defined ( __SUPPORT_IPV6__ )
static const char *DHCPV6_SERVER_CONF_START_IP    = DHCPV6_SERVER_START_IP;
static const char *DHCPV6_SERVER_CONF_END_IP      = DHCPV6_SERVER_END_IP;
static const char *DHCPV6_SERVER_CONF_LEASE_TIME  = DHCPV6_SERVER_LEASE_TIME;
static const char *DHCPV6_SERVER_CONF_DNS         = DHCPV6_SERVER_DNS;
#endif  // __SUPPORT_IPV6__

extern u8_t dhcps_is_running(void);

unsigned int get_current_ip_info(const unsigned int iface)
{
    RA6W1_UNUSED_ARG(iface);

    printf("=== [%s] Need to write FreeRTOS code for this function ...\n", __func__);

    return pdPASS;
}

unsigned int get_current_dns_ipaddr(unsigned long *dns_ipaddr, const unsigned int iface)
{
#if defined (__SUPPORT_IPV6__)
    (void) dns_ipaddr;
#endif // __SUPPORT_IPV6__

    RA6W1_UNUSED_ARG(iface);

    unsigned int status = ERR_OK;
#if !defined (__SUPPORT_IPV6__)
    ip4_addr_t dns_addr;

    dns_addr = dhcps_dns_getserver();

    *dns_ipaddr = dns_addr.addr;
#endif // !__SUPPORT_IPV6__
    return status;
}

unsigned int set_start_ip_address(char *value, int ver)
{
    RA6W1_UNUSED_ARG(ver);

    unsigned int status = ERR_OK;
    int result = 0;

#if defined ( __SUPPORT_IPV6__ )
    if (ver == 4) {
#ifdef RM_MAP_PERSISTANT_W
        result =  RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, DHCP_SERVER_CONF_START_IP, value);
#endif
    } else if (ver == 6) {
#ifdef RM_MAP_PERSISTANT_W
        result =  RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, DHCPV6_SERVER_CONF_START_IP, value);
#endif
    }
#else    // __SUPPORT_IPV6__
#ifdef RM_MAP_PERSISTANT_W
    result =  RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, DHCP_SERVER_CONF_START_IP, value);
#endif
#endif    // __SUPPORT_IPV6__

    if (result != FSP_SUCCESS) {
        status = ER_NOT_SUCCESSFUL;
    }

    return status;
}

unsigned int set_end_ip_address(char *value, int ver)
{
    RA6W1_UNUSED_ARG(ver);

    unsigned int status = ERR_OK;
    int result = 0;

#if defined ( __SUPPORT_IPV6__ )
    if (ver == 4) {
#ifdef RM_MAP_PERSISTANT_W
        result = RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, DHCP_SERVER_CONF_END_IP, value);
#endif
    } else if (ver == 6) {
#ifdef RM_MAP_PERSISTANT_W
        result = RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, DHCPV6_SERVER_CONF_END_IP, value);
#endif
    }
#else    // __SUPPORT_IPV6__
#ifdef RM_MAP_PERSISTANT_W
   result =  RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, DHCP_SERVER_CONF_END_IP, value);
#endif
#endif    // __SUPPORT_IPV6__

    if (result != FSP_SUCCESS) {
        status = ER_NOT_SUCCESSFUL;
    }

    return status;
}

unsigned int set_range_ip_address_list(char *start, char *end, int ver)
{
    unsigned int status = ERR_OK;

#ifndef __SUPPORT_IPV6__
    if (ver == 6) {
        printf("Doesn't support IPv6 !!!\n");
        return status;
    }
#endif /* ! __SUPPORT_IPV6__ */
    // to convert ip address from string
#if defined ( __SUPPORT_IPV4__ )
    unsigned long start_ip = 0;
    unsigned long end_ip = 0;
    ip_addr_t tmp_addr;

    if (ver == 4) {
       ipaddr_aton(start, &tmp_addr);
       start_ip = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&tmp_addr)));
       memset(&tmp_addr, 0x00, sizeof(ip_addr_t));
       
       ipaddr_aton(end, &tmp_addr);
       end_ip = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&tmp_addr)));

        //to check range between end and start
        if (((end_ip - start_ip) >= NX_DHCP_IP_ADDRESS_MAX_LIST_SIZE) || (start_ip == NX_DHCP_NO_ADDRESS)) {

            printf("ERR: Failed to set range of IP_addr list(Max:%d).\n", NX_DHCP_IP_ADDRESS_MAX_LIST_SIZE);

            return 0x9B; // NX_DHCP_INVALID_IP_ADDRESS (todo: later to adapt to lwip dhcp error number for debug)
            
        }
    }
#endif // __SUPPORT_IPV4__
#if defined ( __SUPPORT_IPV6__ )
    if (ver == 6) {
        if (parse_IPv6_to_long(start, NULL, NULL) == 0 || parse_IPv6_to_long(end, NULL, NULL) == 0) {
            return -1; //return NX_DHCP_INVALID_IP_ADDRESS;
        }
    }
#endif    // __SUPPORT_IPV6__


    //to write start/end ip address
    status = set_start_ip_address(start, ver);

    if (status == ERR_OK) {
        status = set_end_ip_address(end, ver);
    } else {
        printf("ERR: Failed to set range of IP_addr list(0x%02X).\n", status);
    }

    return status;
}

#if defined ( __SUPPORT_IPV4__ )
unsigned int get_range_ip_address_list(unsigned long *start_ipaddr, unsigned long *end_ipaddr)
{
    unsigned int status = ERR_OK;
    unsigned long start_ip = 0;
    unsigned long end_ip = 0;
    char *nvram_string = NULL;
    ip_addr_t tmp_addr;

#if defined ( __SUPPORT_P2P__ )
    if (   get_run_mode() == WIFI_DEVICE_MODE_EXT_P2P
        || get_run_mode() == WIFI_DEVICE_MODE_EXT_P2P_STATION) {
        *start_ipaddr = DHCP_SERVER_P2P_ULONG_START_IP;
        *end_ipaddr = DHCP_SERVER_P2P_ULONG_END_IP;
        return status;
    }
#endif // __SUPPORT_P2P__

    // To get start ip address for list
#ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, DHCP_SERVER_CONF_START_IP, &nvram_string);
#endif

    if (nvram_string == NULL) {
        return ER_NOT_FOUND;
    } else {
        ipaddr_aton(nvram_string, &tmp_addr);
        start_ip = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&tmp_addr)));
        memset(&tmp_addr, 0x00, sizeof(ip_addr_t));
    }

    // To get end ip address for list
#ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, DHCP_SERVER_CONF_END_IP, &nvram_string);
#endif

    if (nvram_string == NULL) {
        return ER_NOT_FOUND;
    } else {
        ipaddr_aton(nvram_string, &tmp_addr);
        end_ip = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&tmp_addr)));
    }

    // To check validation
    if ( (end_ip - start_ip) > NX_DHCP_IP_ADDRESS_MAX_LIST_SIZE ) {
        status = ER_SIZE_ERROR;
    } else {
        *start_ipaddr = start_ip;
        *end_ipaddr = end_ip;
        status = ERR_OK;
    }

    return status;
}
#endif /* __SUPPORT_IPV4__  */

unsigned int set_lease_time(int value, int ver)
{
#if defined ( __SUPPORT_IPV6__ )
    //NX_DHCPV6_SERVER *dhcpv6_server = &ra6wx_dhcpv6_server;
#endif    // __SUPPORT_IPV6__

    unsigned int status = ERR_OK;
    int result = 0;

#ifndef __SUPPORT_IPV6__
    if (ver == 6)
    {
        printf("Doesn't support IPv6 !!!\n");
        return status;
    }
#endif /* ! __SUPPORT_IPV6__ */
#if !defined (__SUPPORT_IPV6__)
    /* write lease time into nvram & server. */
    if (ver == 4) {
        dhcps_set_option_info(IP_ADDRESS_LEASE_TIME, &value, sizeof(dhcps_time_t));
    }
#endif // !__SUPPORT_IPV6__

#if defined ( __SUPPORT_IPV6__ )

    if (ver == 6) {
        //dhcpv6_server->nx_dhcpv6_lifetime_config = value;
    }

#endif    // __SUPPORT_IPV6__

    if (status == ERR_OK) {
        if (ver == 4) {
#ifdef RM_MAP_PERSISTANT_W
            result = RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG,
                                                 DHCP_SERVER_CONF_LEASE_TIME, value);
#endif
        }

#if defined ( __SUPPORT_IPV6__ )
        else if (ver == 6) {
#ifdef RM_MAP_PERSISTANT_W
            result = RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG,
                                                 DHCPV6_SERVER_CONF_LEASE_TIME, value);
#endif
        }
#endif    // __SUPPORT_IPV6__

        if (result != 0) {
            status = ER_NOT_SUCCESSFUL;
        }
    }

    return status;
}

unsigned int get_lease_time(int *value)
{
    RA6W1_UNUSED_ARG(value);

    printf("=== [%s] Need to write FreeRTOS code for this function ...\n", __func__);

    return pdPASS;
}

unsigned int set_dns_information(char *value, int ver)
{
    RA6W1_UNUSED_ARG(ver);

    unsigned int status = ERR_OK;
    int result = 0;

#if defined ( __SUPPORT_IPV6__ )
    if (ver == 4) {
#ifdef RM_MAP_PERSISTANT_W
        result =  RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, DHCP_SERVER_CONF_DNS, value);
#endif
    } else if (ver == 6) {
#ifdef RM_MAP_PERSISTANT_W
        result =  RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, DHCPV6_SERVER_CONF_DNS, value);
#endif
    }
#else // __SUPPORT_IPV6__
#ifdef RM_MAP_PERSISTANT_W
    result = RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, DHCP_SERVER_CONF_DNS, value);
#endif
#endif // __SUPPORT_IPV6__

    if (result != FSP_SUCCESS) {
        status = ER_NOT_SUCCESSFUL;
    }

    return status;
}

unsigned int get_dns_information(unsigned long *value, const unsigned int iface)
{
#if defined ( __SUPPORT_IPV4__ )
    unsigned int status = ERR_OK;
    unsigned long dns_ip = 0;
    char *nvram_string = NULL;
    ip_addr_t tmp_addr;

#if defined ( __SUPPORT_P2P__ )
    if (get_run_mode() == WIFI_DEVICE_MODE_EXT_P2P || get_run_mode() == WIFI_DEVICE_MODE_EXT_P2P_STATION) {
        *value = DHCP_SERVER_P2P_STRING_DNS_IP;
        return status;
    }
#endif // __SUPPORT_P2P__

#ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, DHCP_SERVER_CONF_DNS, &nvram_string);
#endif

    if (!(nvram_string == NULL || strlen(nvram_string) == 0)) {
        ipaddr_aton(nvram_string, &tmp_addr);
        dns_ip = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&tmp_addr)));
    } else {
        status = get_current_dns_ipaddr(&dns_ip,  iface);
    }

    *value = dns_ip;

    return status;
#else
    return pdFALSE;
#endif /* __SUPPORT_IPV4__ */
}

unsigned int is_dhcp_server_running(void)
{
#if defined ( __SUPPORT_IPV4__ ) && defined ( __SUPPORT_IPV6__ )
	return dhcps_is_running();
#elif defined (__SUPPORT_IPV6__)
    return pdFALSE;
#elif defined (__SUPPORT_IPV4__)
    return dhcps_is_running();
#else
    return pdFALSE;
#endif //!__SUPPORT_IPV6__
}

void usage_dhcpd(void)
{
    printf("\nUsage: DHCP Server\n");

    printf("\x1b[93m" "Name" "\x1b[0m\n");
    printf("\tdhcpd - DHCP Server\n");

    printf("\x1b[93m" "SYNOPSIS" "\x1b[0m\n");
    printf("\tdhcpd [OPTION]...\n");

    printf("\x1b[93m" "DESCRIPTION" "\x1b[0m\n");
    printf("\tDHCP Server\n");

#if defined ( __SUPPORT_P2P__ )
    if (get_run_mode() != WIFI_DEVICE_MODE_EXT_P2P && get_run_mode() != WIFI_DEVICE_MODE_EXT_P2P_STATION && get_run_mode() != WIFI_DEVICE_MODE_EXT_P2P_GO) 
#endif // __SUPPORT_P2P__
    {

        printf("\t\x1b[93m" "boot [on|off]" "\x1b[0m\n");
        printf("\t\tSelect a DHCP server running flag.\n");

        printf("\t\x1b[93m" "start" "\x1b[0m\n");
        printf("\t\tStart DHCP Server\n");

        printf("\t\x1b[93m" "stop" "\x1b[0m\n");
        printf("\t\tStop DHCP Server\n");

        printf("\t\x1b[93m" "range <Start IP ADDRESS> <END IP Address>" "\x1b[0m\n");
        printf("\t\tSet range of IP address list. The max IP Address list size is %d.\n",
                DHCP_IP_ADDRESS_MAX_LIST_SIZE);

        printf("\t\x1b[93m" "lease_time <Integer>" "\x1b[0m\n");
        printf("\t\tSet client lease time(min %d sec. ~ max %d sec.)\n", MIN_DHCP_SERVER_LEASE_TIME, MAX_DHCP_SERVER_LEASE_TIME);

#if defined ( __SUPPORT_MESH_PORTAL__ ) || defined (__SUPPORT_MESH__)
        printf("\t\x1b[93m" "dns <IP Address>" "\x1b[0m\n");
        printf("\t\tSet DNS information for DHCP Server (MESH Mode Only)\n");
#endif // __SUPPORT_MESH_PORTAL__ || __SUPPORT_MESH__
    }

    printf("\t\x1b[93m" "status" "\x1b[0m\n");
    printf("\t\tDisplay status of DHCP Server\n");

    printf("\t\x1b[93m" "lease [1|0]=Print Unassign Flag" "\x1b[0m\n");
    printf("\t\tDispaly IP Lease Table\n");

#if defined (__UNUSED_CODE__)
#if defined ( __SUPPORT_IPV6__ )
    printf("\t\x1b[93m" "-6" "\x1b[0m\n");
    printf("\t\tDHCPv6 Server Commands\n");
    printf("\t\tex) dhcpd -6 start\n");
    printf("\t\t    dhcpd -6 stop\n");
    printf("\t\t    dhcpd -6 range  <Start IPv6 ADDRESS> <END IPv6 Address>\n");
#if defined ( __SUPPORT_MESH_PORTAL__ ) || defined ( __SUPPORT_MESH__ )
    printf("\t\t    dhcpd -6 dns <IPv6 Address>\n");
#endif // __SUPPORT_MESH_PORTAL__ || __SUPPORT_MESH__
    printf("\t\t    dhcpd -6 status\n");
#endif    // __SUPPORT_IPV6__
#endif /* __UNUSED_CODE__ */

    printf("\t\x1b[93m" "help" "\x1b[0m\n");
    printf("\t\tDisplay help\n");

    return ;
}

int setDHCPServerBootStart(int flag)
{
    int ret;

    if (flag) {
        /* DHCP Server ON */
#ifdef RM_MAP_PERSISTANT_W
        ret = RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG,
                                          "USEDHCPD", pdTRUE);
#endif
    } else {
        /* DHCP Server OFF */
#ifdef RM_MAP_PERSISTANT_W
        ret =  RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, "USEDHCPD");
#endif
    }

    return ret;
}

int getDHCPServerBootStart(void)
{
    int flag = 0;
#ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG,
                               "USEDHCPD", &flag);
#endif

    if (flag == -1) {
        flag = 0;
    }

    return flag;
}

unsigned int get_dhcp_server_state(unsigned int iface, unsigned int ver)
{
    RA6W1_UNUSED_ARG(iface);
    RA6W1_UNUSED_ARG(ver);

    return is_dhcp_server_running();
}
#endif /* CFG_WIFI */

/* EOF */
