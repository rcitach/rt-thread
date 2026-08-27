/**
 ****************************************************************************************
 *
 * @file net_ping.c
 *
 * @brief PING Client module
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
#include "sys_feature.h"
#include "rm_wifi.h"
#include "net_ping.h"
#include "ping.h"

#ifdef LWIP_DNS
#include "lwip/dns.h"
#endif    /*LWIP_DNS*/
#include "clib.h"
#ifdef __SUPPORT_SIGMA_TEST__
#include "ad_uart.h"
#endif /* __SUPPORT_SIGMA_TEST__ */

#include "rm_lwip_w_helper.h"

/* Global external variables for PING */
#if defined ( __SUPPORT_APP_CONSOLE_INPUT__ ) && CFG_CLI
extern void app_request_console_input_access(void);
extern void app_release_console_input_access(void);
#endif  // __SUPPORT_APP_CONSOLE_INPUT__
extern bool lwip_iface_is_up(int iface_flag);

static struct cmd_ping_param * ping_arg = NULL;
static void ping_arg_free(void)
{
   vPortFree(ping_arg);
   ping_arg = NULL;
}
static int ping_arg_alloc(void)
{
    if (ping_arg) {
        ping_arg_free();
    }

    ping_arg = pvPortMalloc(sizeof(struct cmd_ping_param));
    if (ping_arg == NULL) {
        printf("[%s:%d] mem alloc fail\n", __func__, __LINE__);
        return pdFALSE;
    }
    memset(ping_arg, 0, sizeof(struct cmd_ping_param));

    ping_arg->max_count = DEFAULT_PING_COUNT;        /* default count */
    ping_arg->ping_interface = NONE_IFACE;

    memset(ping_arg->domain_str, 0, PING_DOMAIN_LEN + 1);
    ping_arg->dns_query_wait_option = 2*1000; // 2000 msec.
    return pdTRUE;
}

extern void dns_req_resolved(const char* domain, const ip_addr_t *ipaddr, void *arg);
void ping_dns_found(const char* domain, const ip_addr_t *ipaddr, void *arg)
{
    LWIP_UNUSED_ARG(arg);
    LWIP_UNUSED_ARG(domain);
#if defined ( __SUPPORT_IPV4__ )
    if (ping_arg != NULL) {
        if (ipaddr != NULL && is_in_valid_ip_class(ipaddr_ntoa(ipaddr))) {
#ifdef RA6WX_PING_DEBUG
            PING_DEBUG_PRINT(">> Domain address Found: %s : %s\n", domain, ipaddr_ntoa(ipaddr));
#endif    /*RA6WX_PING_DEBUG*/
            ip_addr_copy(ping_arg->ipaddr, *ipaddr);
            
#if defined ( __DNS_2ND_CACHE_SUPPORT__ )
            dns_req_resolved(domain, ipaddr, arg);
#endif // __DNS_2ND_CACHE_SUPPORT__
            ping_init_for_console(ping_arg);
        } else {
            printf("DNS query failed.\n");
            ping_arg_free();
            return;
        }
    } else {
        printf("Ping parameter error!\n");
    }
#endif /* __SUPPORT_IPV4__ */
}

extern int isvalidIPsubnetInterface(long ip, int iface);
bool cmd_ping_client(int argc, char *argv[])
{
    bool is_domain_addr = pdFALSE;
    char * domain_url = NULL;
    int n = 0;
    unsigned char is_v6_flag = pdFALSE;

#ifdef    __SUPPORT_IPV6__
    unsigned long    ipv6_dst[4];
    unsigned long    ipv6_src[4];
#endif    /* __SUPPORT_IPV6__ */
#if defined (LWIP_DNS)
    err_t ret;
#endif	/* (LWIP_DNS)*/
#if (defined ( __SUPPORT_IPV4__ ) && defined ( __SUPPORT_IPV6__ ))
    uint8_t zero_addr[16] = {0};
#endif

    RA6W1_UNUSED_ARG(domain_url);

    if (!ra6w1_network_main_is_wlaninit()) {
        printf("Wi-Fi is not initialized.\n");
        return pdTRUE;
    }

#if defined ( __SUPPORT_APP_CONSOLE_INPUT__ ) && CFG_CLI
    app_request_console_input_access();
#endif // __SUPPORT_APP_CONSOLE_INPUT__ && CFG_CLI

    if (ping_arg_alloc() == pdFALSE) {
        return pdTRUE;
    }
  
    /* IP version check */
    for (int m = 0; m < argc; m++) {
        /* -6 */
        if (*argv[m] == '-' && *(argv[m]+1) == '6') {
            is_v6_flag = 1;
        }
    }

    /* parsing */
    for (n = 0; n < argc; n++) {
        /* IPv4 Address / Domain */
        if ((strlen(argv[n]) > 4) && (strchr(argv[n], '.') != NULL)) {
#if defined (__SUPPORT_IPV4__)        
            if (is_in_valid_ip_class(argv[n])) {    /* check ip */
                ip4addr_aton(argv[n], (ip4_addr_t *)&(ping_arg->ipaddr));
            } else
#elif defined (__SUPPORT_IPV6__)
            ip6addr_aton(argv[n], &(ping_arg->ipaddr));
#endif // __SUPPORT_IPV6__
            {
                is_domain_addr = pdTRUE;
                domain_url = argv[n];
            }
        }

#if defined ( __SUPPORT_IPV6__ )
       /* IPv6 Address */
       if ((strlen(argv[n]) > 2) && (strchr(argv[n], ':') != NULL)) {
           if (parse_IPv6_to_long(argv[n], ipv6_dst, NULL) == 0) {
               printf("Invalid IPv6 Address\n");
               ping_arg_free();
               return pdTRUE;
           } else {
               is_v6_flag = 1; // ping -6
#if defined ( __SUPPORT_IPV4__ ) // (__SUPPORT_IPV4__) && (__SUPPORT_IPV6__)
               ping_arg->ipaddr.type = IPADDR_TYPE_V6;
#endif                    
           }

           ip6addr_aton(argv[n], (ip6_addr_t *)&(ping_arg->ipaddr));
       }

       /* -S */
       if (*argv[n] == '-' && *(argv[n]+1) == 'S') {
           if (!is_v6_flag) {
               printf("Not used in IPv4!");
               ping_arg_free();
               return pdTRUE;
           }

           if (parse_IPv6_to_long(argv[++n], ipv6_src, NULL) == 0) {
               printf("Invalid IPv6 Address (Source)\n");
               ping_arg_free();
               return pdTRUE;
           }
       }
#endif    /* __SUPPORT_IPV6__ */

        /* -n */
        if (*argv[n] == '-' && (*(argv[n]+1) == 'n' || *(argv[n]+1) == 'c')) {
            ping_arg->max_count = (uint64_t)atoll(argv[n + 1]);

        }

        /* -t */
        if (*argv[n] == '-' && *(argv[n]+1) == 't') {
            ping_arg->max_count = 0xFFFFFFFFFFFFFFFF;
        }

        /* -l */
        if (*argv[n] == '-' && *(argv[n]+1) == 'l') {
            ping_arg->len = ctoi(argv[n + 1]);
        }

        /* -w */
        if (*argv[n] == '-' && *(argv[n]+1) == 'w') {
            ping_arg->wait = ctoi(argv[n + 1]);
        }

        /* -i */
        if (*argv[n] == '-' && *(argv[n]+1) == 'i') {
            ping_arg->interval = ctoi(argv[n + 1]);
        }

        /* -I Interface */
        if (*argv[n] == '-' && *(argv[n]+1) == 'I') {
            if (strcasecmp(argv[n + 1], "WLAN0") == 0) {
                ping_arg->ping_interface = WLAN0_IFACE;
            } else if (strcasecmp(argv[n + 1], "WLAN1") == 0) {
                ping_arg->ping_interface = WLAN1_IFACE;
            } else {
                goto ping_help;
            }
        }
    }

    if (ping_arg->ping_interface == NONE_IFACE)
    {
#if defined (__SUPPORT_IPV4__)
    	if (isvalidIPsubnetInterface((PP_HTONL(ip_2_ip4(&ping_arg->ipaddr)->addr)), WLAN1_IFACE)
            && lwip_iface_is_up(WLAN1_IFACE)
        ) {
            ping_arg->ping_interface = WLAN1_IFACE;
        } else
#endif // __SUPPORT_IPV6__
        {
            ping_arg->ping_interface = WLAN0_IFACE;
        }
    }

#if CFG_WIFI
    if ( (ping_arg->ping_interface == WLAN0_IFACE )&&
         (get_run_mode() >= WIFI_DEVICE_MODE_EXT_AP))
    {
        switch (get_run_mode()) {
#ifdef __SUPPORT_P2P__
            case WIFI_DEVICE_MODE_EXT_P2P :
            case WIFI_DEVICE_MODE_EXT_AP :
            case WIFI_DEVICE_MODE_EXT_P2P_GO :
#else
            case WIFI_DEVICE_MODE_EXT_AP :
#endif /* __SUPPORT_P2P__ */
                ping_arg->ping_interface = WLAN1_IFACE;
                break;
        }
    }
#endif /* CFG_WIFI */

    if (   (argc < 2)
        || (strcmp(argv[1], "help") == 0)
        || (ping_arg->len > MAX_PING_SIZE)
#if (defined ( __SUPPORT_IPV4__ ) && defined ( __SUPPORT_IPV6__ ))
        || (   ((!memcmp(ping_arg->ipaddr.u_addr.ip6.addr, zero_addr, 16) && is_v6_flag == 0) || (!is_v6_flag))
            && (is_domain_addr == pdFALSE && (ping_arg->ipaddr.u_addr.ip4.addr == 0)))
#elif defined ( __SUPPORT_IPV6__ )
        || (ping_arg->ipaddr.addr == 0 && is_v6_flag == 0)
        || (!is_v6_flag)
#elif defined (__SUPPORT_IPV4__)
        || (is_domain_addr == pdFALSE && (ping_arg->ipaddr.addr == 0))
        || (is_v6_flag == pdTRUE)
#endif    // __SUPPORT_IPV6__
        ) {
ping_help:
        printf("Usage: ping -I [interface] [domain|ip] -n [count] -l [size] "
               "-w [timeout] -i [interval]\n"
               "\t-I [wlan0|wlan1]\tInterface name\n"
               "\t-n or -c count\tNumber of echo requests to send.\n"
               "\t-l size\t\tSend buffer size.(MAX:%d)\n"
               "\t-w timeout\tTime to wait for a respone in milliseconds (Min:10ms)\n"
#ifdef    __SUPPORT_IPV6__
               "\t-6 IPv6\n"
#endif    /* __SUPPORT_IPV6__ */
               "\t-i interval\tWait interval seconds between sending each packet.(MIN:10ms)\n\n"
               "\tex) ping -I wlan0 172.16.0.1 -l 1024 -n 10 -w 1000"
               " -i 1000\n"
#ifdef    __SUPPORT_IPV6__
               "\t    ping -6 fe80::1:2 -I wlan0"
#endif    /* __SUPPORT_IPV6__ */
               "\n\n", MAX_PING_SIZE);

        ping_arg_free();
#if defined ( __SUPPORT_APP_CONSOLE_INPUT__ ) && CFG_CLI
        app_release_console_input_access();
#endif // __SUPPORT_APP_CONSOLE_INPUT__ && CFG_CLI
        return pdTRUE;
    }

    if (ra6w1_network_main_check_network_ready(ping_arg->ping_interface) == pdFALSE) {
        printf("connect: Network is unreachable\n");
        ping_arg_free();
#if defined ( __SUPPORT_APP_CONSOLE_INPUT__ ) && CFG_CLI
        app_release_console_input_access();
#endif // __SUPPORT_APP_CONSOLE_INPUT__ && CFG_CLI
        return pdTRUE;
    }

    if (is_domain_addr == pdTRUE) {
#if defined (LWIP_DNS)
        ip_addr_t dns_ipaddr, result;
        dns_req_ctx_t ctx = {0};
        bool resolved = pdFALSE;

        ctx.out = &result;
        ctx.sem = xSemaphoreCreateBinary();

        if (!ctx.sem)
        {
            return pdFALSE;
        }
        xSemaphoreTake(ctx.sem, 0);

        PING_DEBUG_PRINT("DNS Query Domain:%s\n", domain_url);
        strncpy((char *)ping_arg->domain_str, domain_url, PING_DOMAIN_LEN);
        PING_DEBUG_PRINT("DNS query in progress.\n");

        // DNS Request
#if LWIP_DNS
        ret = dns_gethostbyname_addrtype(domain_url, &dns_ipaddr, dns_req_resolved, &ctx,
#if defined (__SUPPORT_IPV4__) && defined (__SUPPORT_IPV6__)
                                        is_v6_flag == pdTRUE ? LWIP_DNS_ADDRTYPE_IPV6 : LWIP_DNS_ADDRTYPE_DEFAULT
#elif  defined __SUPPORT_IPV4__
                                        LWIP_DNS_ADDRTYPE_IPV4
#elif  defined __SUPPORT_IPV6__
                                        LWIP_DNS_ADDRTYPE_IPV6
#endif // __SUPPORT_IPV4__ && __SUPPORT_IPV6__
                                        );
#endif /* LWIP_DNS */
        if (ERR_OK == ret)
        {
#if defined (__SUPPORT_IPV6__) && defined (__SUPPORT_IPV4__)
            if (ip_addr_isany_val(dns_ipaddr))
            {
#elif defined (__SUPPORT_IPV4__)
            if (ip4_addr_isany_val(*ip_2_ip4(&dns_ipaddr)))
            {
#elif defined (__SUPPORT_IPV6__)
            if (ip6_addr_isany_val(*ip_2_ip6(&dns_ipaddr)))
            {
#endif
                printf("[%s] Failed to query DNS. Invalid domain URL...\n", __func__);
                resolved = pdFALSE;
                goto End;
            } else
            {
                PING_DEBUG_PRINT("[%s] ret=%d dns_wait=%d\n", __func__, ret, ping_arg->dns_query_wait_option);
                PING_DEBUG_PRINT("DNS query success.:%s\n", ipaddr_ntoa(&dns_ipaddr));
#if defined (__SUPPORT_IPV6__) && defined (__SUPPORT_IPV4__)
                if (IP_IS_V4_VAL(dns_ipaddr))
                {
                    ip_addr_copy_from_ip4(ping_arg->ipaddr, dns_ipaddr.u_addr.ip4);
                } else if (IP_IS_V6_VAL(dns_ipaddr))
                {
                    ip_addr_copy_from_ip6(ping_arg->ipaddr, dns_ipaddr.u_addr.ip6);
                }

#elif defined (__SUPPORT_IPV4__)
                ip4_addr_copy(ping_arg->ipaddr, dns_ipaddr);
#elif defined (__SUPPORT_IPV6__)
                ip6_addr_copy(ping_arg->ipaddr, dns_ipaddr);
#endif
                resolved = pdTRUE;
                goto End;
            }
        } else if (ERR_INPROGRESS == ret)
        {
            if (xSemaphoreTake(ctx.sem, portCONVERT_MS_2_TICKS(ping_arg->dns_query_wait_option)) != pdTRUE)
            {
                printf("[%s] Failed to query DNS(Timeout)\n", __func__);
                resolved = pdFALSE;
                goto End;
            }
#if defined (__SUPPORT_IPV6__) && defined (__SUPPORT_IPV4__)
            if (IP_IS_V4_VAL(result))
            {
                ip_addr_copy_from_ip4(ping_arg->ipaddr, result.u_addr.ip4);
            } else if (IP_IS_V6_VAL(result))
            {
                ip_addr_copy_from_ip6(ping_arg->ipaddr, result.u_addr.ip6);
            }
#elif defined (__SUPPORT_IPV4__)
            ip4_addr_copy(ping_arg->ipaddr, result);
#elif defined (__SUPPORT_IPV6__)
            ip6_addr_copy(ping_arg->ipaddr, result);
#endif
            resolved = pdTRUE;
            goto End;
        } else
        {
            printf("ping: %s: Name or service not known\n", domain_url);
            ping_arg_free();
#if defined ( __SUPPORT_APP_CONSOLE_INPUT__ ) && CFG_CLI
            app_release_console_input_access();
#endif // __SUPPORT_APP_CONSOLE_INPUT__ && CFG_CLI
            resolved = pdFALSE;
            goto End;
        }
End:
        vSemaphoreDelete(ctx.sem);
        if (resolved)
        {
            ping_init_for_console(ping_arg);
            ping_arg = NULL;
        }
        return resolved;

#endif    /*LWIP_DNS*/
    } else // is_domain_addr == pdFALSE
    {
        ping_init_for_console(ping_arg);
        ping_arg = NULL;

        return pdTRUE;
    }
}

#define IP_VERSION_V4    0x4
#define IP_VERSION_V6    0x6
unsigned int ra6w1_ping_client(int iface,
                            char *domain_str,
                            unsigned long ipaddr,
                              unsigned long *ipv6dst,
                            unsigned long *ipv6src,
                            int len,
                            uint64_t max_count,
                            int wait,
                            int interval,
                            int nodisplay,
                            char *ping_result)
{
    RA6W1_UNUSED_ARG(ipv6dst);
    RA6W1_UNUSED_ARG(ipv6src);

    char    ip_str[40] = { 0x00, };
    char    my_ip_str[40] = { 0x00, };

#ifdef __SUPPORT_IPV6__
    int  ip_version = IP_VERSION_V6;
#endif

    bool is_domain_addr = pdFALSE;
    char * domain_url = NULL;
    struct cmd_ping_param *atcmd_ping_arg = NULL;
#if (defined ( __SUPPORT_IPV4__ ) && defined ( __SUPPORT_IPV6__ ))
    uint8_t zero_addr[16] = {0};
#endif

    if (len <= 0 || len > MAX_PING_SIZE) {
        len = DEFAULT_PING_SIZE;        /* default size */
    }

    if (max_count <= 0) {
        max_count = DEFAULT_PING_COUNT;        /* default count */
    }

    if (wait <= 0) {
        wait = DEFAULT_PING_WAIT;        /* default wait */
    }

    if (interval < MIN_INTERVAL) {
        interval = DEFAULT_INTERVAL;    /* default interval */
    }

#ifdef    __SUPPORT_IPV6__
    if (ipaddr == 0x0) {
        ip_version = IP_VERSION_V6;
    } else {
        ip_version = IP_VERSION_V4;
    }
#endif    /* __SUPPORT_IPV6__ */

    memset(ip_str, 0, sizeof(ip_str));
    memset(my_ip_str, 0, sizeof(my_ip_str));

    atcmd_ping_arg = pvPortMalloc(sizeof(struct cmd_ping_param));

    memset(atcmd_ping_arg, 0, sizeof(struct cmd_ping_param));
    memset(atcmd_ping_arg->domain_str, 0, PING_DOMAIN_LEN + 1);
    atcmd_ping_arg->dns_query_wait_option = portCONVERT_MS_2_TICKS(4000);

#ifdef  __SUPPORT_IPV6__
    if (ip_version == IP_VERSION_V6) { // Only __SUPPORT_IPV6__
        printf("=== [%s] #1 Need to write FreeRTOS code for this function ...\n", __func__);
    } else { // __SUPPORT_IPV4__ && __SUPPORT_IPV6__
        /* Conver IP address to IP */
#if (defined ( __SUPPORT_IPV4__ ) && defined ( __SUPPORT_IPV6__ ))
        longtoip((long)ipaddr, ip_str);
        if (is_in_valid_ip_class(ip_str) == pdTRUE) {   /* check ip */
            ip4addr_aton(ip_str, (ip4_addr_t *)&(atcmd_ping_arg->ipaddr));
        } else {
            is_domain_addr = pdTRUE;
            domain_url = domain_str;
        }
#endif /* #if (defined ( __SUPPORT_IPV4__ ) && defined ( __SUPPORT_IPV6__ )) */
    }
#else /* __SUPPORT_IPV6__ */
    // Only __SUPPORT_IPV4__
    /* Convert IP address to IP */
    longtoip((long)ipaddr, ip_str);

    if (is_in_valid_ip_class(ip_str) == pdTRUE) {   /* check ip */
        ip4addr_aton(ip_str, (ip4_addr_t *)&(atcmd_ping_arg->ipaddr));
    } else {
        is_domain_addr = pdTRUE;
        domain_url = domain_str;
    }
#endif

    atcmd_ping_arg->max_count = max_count;
    atcmd_ping_arg->len = len;
    atcmd_ping_arg->wait = wait;
    atcmd_ping_arg->interval = interval;
    atcmd_ping_arg->ping_interface = iface;

    if (is_domain_addr == pdTRUE) {
        ip_addr_t dns_ipaddr;

        strncpy((char *)atcmd_ping_arg->domain_str, domain_url, PING_DOMAIN_LEN);

        err_t ret = dns_gethostbyname(domain_url, &dns_ipaddr, ping_dns_found, NULL);

        if (ret == ERR_OK) {
#if defined (__SUPPORT_IPV4__) && defined (__SUPPORT_IPV6__)
            if (IP_IS_V4_VAL(dns_ipaddr)) {
                ip_addr_copy_from_ip4(atcmd_ping_arg->ipaddr, dns_ipaddr.u_addr.ip4);
                if (atcmd_ping_arg->ipaddr.u_addr.ip4.addr == 0) {
                    vPortFree(atcmd_ping_arg);
                    return pdFAIL;
                }
            } else if (IP_IS_V6_VAL(dns_ipaddr)) {
                ip_addr_copy_from_ip6(atcmd_ping_arg->ipaddr, dns_ipaddr.u_addr.ip6);
                if (!memcmp(atcmd_ping_arg->ipaddr.u_addr.ip6.addr, zero_addr, 16)) {
                    vPortFree(atcmd_ping_arg);
                    return pdFAIL;
                }
            }
#else // __SUPPORT_IPV4__ && __SUPPORT_IPV6__
#if defined (__SUPPORT_IPV6__)
            ip_addr_copy_from_ip6(ping_arg->ipaddr, dns_ipaddr);
#endif // __SUPPORT_IPV6__
#if defined (__SUPPORT_IPV4__)
            ip_addr_copy_from_ip4(ping_arg->ipaddr, dns_ipaddr);
#endif // __SUPPORT_IPV4__
            if (ping_arg->ipaddr.addr == 0) {
                return pdFAIL;
            }
#endif // __SUPPORT_IPV4__ && __SUPPORT_IPV6__
        } else {
            printf("[%s] DNS query fail ...\n", __func__);
            vPortFree(atcmd_ping_arg);
            return pdFAIL;
        }
    }

    atcmd_ping_arg->no_display = nodisplay;
    ping_init_for_console(atcmd_ping_arg);
    
    /* Wait until ping tx finished ... */
    while ((uint64_t)get_ping_send_cnt() < max_count) {
        vTaskDelay(portCONVERT_MS_2_TICKS(interval));
    }

    vTaskDelay(portCONVERT_MS_2_TICKS(100));

     /* Result ... */
    if (nodisplay == pdFALSE) {
        printf("\n--- %s ping statistics ---\n%lu packets transmitted, " \
               " %lu received, %u%%(%lu) packet loss, time %lums\n",
                        ping_arg->domain_str[0] != '\0' ? ping_arg->domain_str:ipaddr_ntoa(&ping_arg->ipaddr),
                        get_ping_send_cnt(),
                        get_ping_recv_cnt(),
                        (unsigned int)get_ping_loss_percent(),
                        get_ping_loss_cnt(),
                        get_ping_total_time_ms());

#ifdef __SUPPORT_SIGMA_TEST__
        snprintf(uart_buffer, sizeof(uart_buffer),
                 "\n--- %s ping statistics ---\n%lu packets transmitted, " \
                 " %llu received, %u%%(%lu) packet loss, time %lums\n",
                    ping_arg->domain_str[0] != '\0' ? ping_arg->domain_str:ipaddr_ntoa(&ping_arg->ipaddr),
                    get_ping_send_cnt(),
                    get_ping_recv_cnt(),
                    (unsigned int)get_ping_loss_percent(),
                    get_ping_loss_cnt(),
                    get_ping_total_time_ms());

        ad_uart_write(uart2, uart_buffer, strlen(uart_buffer));
#endif    /* __SUPPORT_SIGMA_TEST__ */

        printf("rtt min/avg/max = %lu/%lu/%lu ms\n\n",
                get_ping_min_time_ms(),
                get_ping_avg_time_ms(),
                get_ping_max_time_ms());
    } else if (nodisplay == pdTRUE) {    /* for AT-Command code */
        sprintf(ping_result, "%lu,%lu,%lu,%lu,%lu",
                            get_ping_send_cnt(),
                            get_ping_recv_cnt(),
                            get_ping_avg_time_ms(),
                            get_ping_min_time_ms(),
                            get_ping_max_time_ms());
    }

    return pdPASS;
}

/* EOF */
