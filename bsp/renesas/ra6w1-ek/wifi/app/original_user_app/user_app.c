/**
 ****************************************************************************************
 *
 * @file user_app.c
 *
 * @brief User application
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

#include "sys_app_defs.h"
#include "common_def.h"
#include "lwip/sockets.h"
#include "rm_wifi.h"
#include "iface_defs.h"

#if (SIGMA_TEST_APP == 1)
#include "rm_sigma.h"
#endif

#if (dg_configSYSTEMVIEW == 1)
#include "SEGGER_SYSVIEW.h"
#include "SEGGER_SYSVIEW_FreeRTOS.h"
#endif

/******************************************************************************
 * External global functions
 ******************************************************************************/
#if defined ( __SUPPORT_HELLO_WORLD__ )
extern void hello_world_1(void *arg);
extern void hello_world_2(void *arg);
#endif  // __SUPPORT_HELLO_WORLD__

/******************************************************************************
 * External global variables
 ******************************************************************************/
#if defined ( __SUPPORT_WIFI_CONN_CB__ )
extern EventGroupHandle_t evt_grp_wifi_conn_notify;

/* Station mode */
extern short wifi_conn_fail_reason;
extern short wifi_disconn_reason;

/* AP mode */
extern short ap_wifi_conn_fail_reason;
extern short ap_wifi_disconn_reason;
#endif  // __SUPPORT_WIFI_CONN_CB__

/******************************************************************************
 * Local static functions
 ******************************************************************************/


// ============================================================================ //
/* nvram.setenv TIMER_TEST 1 */
#undef DPM_TIMER_USER_TEST

#if defined ( DPM_TIMER_USER_TEST )    // ======================================== //
extern UINT chk_network_ready(UCHAR iface);
#define FIVE_SEC_ALIVE
#if CFG_PMGR
void timer_test_cb(char *timer_name)
{
    pmgr_instance_ctrl_t * p_instance_ctrl = RM_PMGR_W_get_ctrl();

    RM_PMGR_W_dpm_sleep_ready_clear(DPM_REG_NAME_TIMER);
    printf(CYN_COL "[%s:%d] @@ %s @@\n" CLR_COL,__func__,__LINE__, timer_name);

#ifdef FIVE_SEC_ALIVE
    vTaskDelay(portCONVERT_MS_2_TICKS(5000));    // Alive 5 Sec
#else
    vTaskDelay(portCONVERT_MS_2_TICKS(100));
#endif
    RM_PMGR_W_dpm_sleep_ready_set(DPM_REG_NAME_TIMER);
}

void timer_test(void *pvParameters)
{
    int dpm_tid = 0;
    int idx = 0;
    int ret;
    pmgr_instance_ctrl_t * p_instance_ctrl = RM_PMGR_W_get_ctrl();

    struct netif *wlan0_netif = netif_get_by_index(WLAN0_IFACE + 2);
    
    ret = RM_PMGR_W_dpm_sleep_ready_clear(DPM_REG_NAME_TIMER);
    if (ret) {
        printf(" [%s] Failed RM_PMGR_W_dpm_sleep_ready_clear ret:%d \r\n", __func__, ret);
    }

    ret = RM_PMGR_W_dpm_wakeup_done(DPM_REG_NAME_TIMER);
    if (ret) {
        printf(" [%s] Failed RM_PMGR_W_dpm_wakeup_done ret:%d \r\n", __func__, ret);
    }

    if (RM_PMGR_W_dpm_is_wakeup() == 0) {
        dpm_tid = RM_PMGR_W_dpm_timer_create(DPM_REG_NAME_TIMER, "timer1", timer_test_cb, 10, 10);
        printf("[%s:%d] DPM timer created : Name = timer1, tid = %d, secs = 10\n",__func__,__LINE__, dpm_tid);
    }

finish:
    ret = RM_PMGR_W_dpm_sleep_ready_set(DPM_REG_NAME_TIMER);
    if (ret)
        printf(" [%s:%d] DPM SLEEP FAIL!!! (ret = %d)\n",__func__,__LINE__, ret);

    while (1) {
        vTaskDelay(portCONVERT_MS_2_TICKS(1000));
    }

    printf("\n\n [%s:%d] ======THREAD FINISH======\n\n",__func__,__LINE__);
}
#endif /* CFG_PMGR */
#endif /* DPM_TIMER_USER_TEST */

// ............................................................................ //



// UDP / TCP Tx/Rx test over IPv6 for Matter project /////////////////////////////

#undef  __TEST_UDP_SESSION_OVER_IPV6__
#undef  __TEST_TCP_SERVER_OVER_IPV6__
#undef  __TEST_TCP_CLIENT_OVER_IPV6__

#undef  __TEST_UDP_SESSION_OVER_IPV6_DPM__		// UDP_DPM_TEST
#undef  __TEST_TCP_SERVER_OVER_IPV6_DPM__
#undef  __TEST_TCP_CLIENT_OVER_IPV6_DPM__

// UDP / TCP Tx/Rx test over IPv4 for Matter project /////////////////////////////

#undef  __TEST_UDP_SESSION_OVER_IPV4__
#undef  __TEST_TCP_SERVER_OVER_IPV4__
#undef  __TEST_TCP_CLIENT_OVER_IPV4__



#define BUF_MAX_LEN        128
#define IPV6_ADDR_LEN      16
#define IPV4_ADDR_LEN      4

#if defined ( __TEST_UDP_SESSION_OVER_IPV6__ )   // ========================== //
#define UDP_SESS_OVER_IPV6 "UDP_IPV6"
#define UDPV6_SERVER_PORT    10190

static char *udpv6_test_str = "Hello World over IPv6 - UDP";
static void udp_over_IPv6(void *arg)
{
    int sock_fd;
    int status;
    char rx_buf[BUF_MAX_LEN];
    struct sockaddr_in6 srv_sock;
    struct sockaddr_in6 peer;
    socklen_t addrlen = sizeof(struct sockaddr_in6);

    while (ra6w1_network_main_check_network_ready(0) == pdFALSE
        || ra6w1_network_main_check_global_unicast_ip6_addr(0) == pdFALSE) {
        vTaskDelay(portCONVERT_MS_2_TICKS(1000));
    }

    printf("\n- Start UDP(v6) Socket \n");

    sock_fd = socket(PF_INET6, SOCK_DGRAM, 0); // IPPROTO_IP=0, IPPROTO_UDP=17
    if (sock_fd == -1) {
        printf("[%s] Failed to socket() !!!\n", __func__);
        vTaskDelete(NULL);
        return;
    }
 
    memset(&srv_sock, 0, sizeof(struct sockaddr_in6));
    memset(&peer, 0, sizeof(struct sockaddr_in6));
    srv_sock.sin6_family = AF_INET6;
    srv_sock.sin6_port = htons(UDPV6_SERVER_PORT);
    srv_sock.sin6_addr = in6addr_any;

    status = bind(sock_fd, (struct sockaddr *)&srv_sock, addrlen); // to use specific Port
    if (status < 0) {
        printf("[%s] Failed to bind() !!!\n", __func__);
        close(sock_fd); 
        vTaskDelete(NULL);
        return;
    }
 
    while (TRUE) {
        // Receive data from peer
        memset(rx_buf, 0, BUF_MAX_LEN);
        status = recvfrom(sock_fd, rx_buf, sizeof(rx_buf), 0, (struct sockaddr *)&peer, &addrlen);
        if (status < 0) {
            printf("[%s] Failed to recvfrom() !!!\n", __func__);
            close(sock_fd);
            vTaskDelete(NULL);
            return;
        }
        printf(">>> Receive from UDP Socket : %s\n", rx_buf);

        // Send test string
        status = sendto(sock_fd, udpv6_test_str, strlen(udpv6_test_str), 0,
                        (struct sockaddr *)&peer, sizeof(peer));
        if (status < 0) {
            printf("[%s] Failed to sendto() !!!\n", __func__);

            close(sock_fd);
            vTaskDelete(NULL);
            return;
        }
        printf("<<< Send to UDP Socket : %s\n", udpv6_test_str);
    }

    close(sock_fd);
    vTaskDelete(NULL);
}
#endif //  __TEST_UDP_SESSION_OVER_IPV6__

// UDP_DPM_TEST
#if defined ( __TEST_UDP_SESSION_OVER_IPV6_DPM__ )   // ========================== //
#if CFG_PMGR
#define UDP_SESS_OVER_IPV6_DPM "UDP_IPV6_DPM"
#define UDP_DPM_SVR_PORT_DEF	10196
#define	UDP_SVR_RX_TIMEOUT		1000000		// TEMPORARY 10 Sec -->1000sec(16min 40 sec)

static char *udpv6_test_str = "Hello World over IPv6 - UDP";
static void udp_over_IPv6_dpm(void *arg)
{
    int sock_fd;
    int status;
    char rx_buf[BUF_MAX_LEN];
    struct sockaddr_in6 srv_sock;
    struct sockaddr_in6 peer;
    socklen_t addrlen = sizeof(struct sockaddr_in6);
	struct timeval   tv;
    pmgr_instance_ctrl_t * p_instance_ctrl = RM_PMGR_W_get_ctrl();

    printf(YELLOW_COLOR "[%s] Start DPM UDP(v6) Socket \n" CLEAR_COLOR, __func__);

    sock_fd = socket(PF_INET6, SOCK_DGRAM, 0); // IPPROTO_IP=0, IPPROTO_UDP=17
    //sock_fd = socket_dpm(UDP_SESS_OVER_IPV6_DPM, PF_INET6, SOCK_DGRAM, 0); // IPPROTO_IP=0, IPPROTO_UDP=17
    if (sock_fd == -1) {
        printf("[%s] Failed to socket() !!!\n", __func__);
        vTaskDelete(NULL);
        return;
    }
 
    memset(&srv_sock, 0, sizeof(struct sockaddr_in6));
    memset(&peer, 0, sizeof(struct sockaddr_in6));
    srv_sock.sin6_family = AF_INET6;
    srv_sock.sin6_port = htons(UDP_DPM_SVR_PORT_DEF);
    srv_sock.sin6_addr = in6addr_any;

    status = bind(sock_fd, (struct sockaddr *)&srv_sock, addrlen); // to use specific Port
    if (status < 0) {
        printf("[%s] Failed to bind() !!!\n", __func__);
        close(sock_fd); 
        vTaskDelete(NULL);
        return;
    }

	tv.tv_sec = 0;
	tv.tv_usec = UDP_SVR_RX_TIMEOUT * 1000;

    status = setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, (const void *)&tv, sizeof(struct timeval));
    if (status) {
        printf(RED_COLOR "[%s:%d] setsockopt error %d \n" CLEAR_COLOR, __func__, __LINE__, status);
    }

	RM_WIFI_dpm_udp_port_filter_set(UDP_DPM_SVR_PORT_DEF);			// RM_WIFI_dpm_udp_port_filter_set
	RM_PMGR_W_dpm_rcv_ready_set(UDP_SESS_OVER_IPV6_DPM);		// Ready receive packet
 
    while (TRUE) {

        /* If dpm power down scheme was already started,
         * skip next udp rx operataion.
         */
        if (RM_PMGR_W_dpm_sleep_is_started()) {
            vTaskDelay(portCONVERT_MS_2_TICKS(100));
            continue;
        }

        // Receive data from peer
        printf(YELLOW_COLOR "[%s] Wate receive packet ...... \n" CLEAR_COLOR, __func__);
        memset(rx_buf, 0, BUF_MAX_LEN);
        status = recvfrom(sock_fd, rx_buf, sizeof(rx_buf), 0, (struct sockaddr *)&peer, &addrlen);

        RM_PMGR_W_dpm_sleep_ready_clear(UDP_SESS_OVER_IPV6_DPM);

        if (status < 0) {
            printf(RED_COLOR "[%s] Failed to recvfrom() status:%d !!!\n" CLEAR_COLOR, __func__, status);
			goto end_loop;
        }
		else {
        	printf(CYAN_COLOR ">>> Receive from UDP Socket : %s\n" CLEAR_COLOR, rx_buf);
		}
        struct netif *wlan0_netif = netif_get_by_index(WLAN0_IFACE + 2);
        nd6_display_neighbor_cache_netif(wlan0_netif);
        // Send test string
        status = sendto(sock_fd, udpv6_test_str, strlen(udpv6_test_str), 0,
                        (struct sockaddr *)&peer, sizeof(peer));
        if (status < 0) {
            printf("[%s] Failed to sendto() !!!\n", __func__);

            close(sock_fd);
            vTaskDelete(NULL);
            return;
        }
        printf(YELLOW_COLOR "<<< Send to UDP Socket : %s\n" CLEAR_COLOR, udpv6_test_str);
        vTaskDelay(10);
		// Set flag to permit goto DPM sleep mode
		RM_PMGR_W_dpm_sleep_ready_set(UDP_SESS_OVER_IPV6_DPM);
    }

end_loop:
	// Set flag to permit goto DPM sleep mode
	RM_PMGR_W_dpm_sleep_ready_set(UDP_SESS_OVER_IPV6_DPM);

    close(sock_fd);
    vTaskDelete(NULL);
}
#endif /* CFG_PMGR */
#endif //  __TEST_UDP_SESSION_OVER_IPV6_DPM__ 

#if defined ( __TEST_UDP_SESSION_OVER_IPV4__ )   // ========================== //

#define UDP_SESS_OVER_IPV4 "UDP_IPV4"
#define UDPV4_SERVER_PORT    20190

static char *udpv4_test_str = "Hello World over IPv4 - UDP";

static void udp_over_IPv4(void *arg)
{
    int sock_fd;
    int status;
    char rx_buf[BUF_MAX_LEN];
    struct sockaddr_in srv_sock;
    struct sockaddr_in peer;
    socklen_t addrlen = sizeof(struct sockaddr_in);

    while (ra6w1_network_main_check_network_ready(0) == pdFALSE) {
        vTaskDelay(portCONVERT_MS_2_TICKS(1000));
    }

    printf("\n- Start UDP(v4) Socket \n");

    sock_fd = socket(PF_INET, SOCK_DGRAM, 0); // IPPROTO_IP=0, IPPROTO_UDP=17
    if (sock_fd == -1) {
        printf("[%s] Failed to socket() !!!\n", __func__);
        vTaskDelete(NULL);
        return;
    }
 
    memset(&srv_sock, 0, sizeof(struct sockaddr_in));
    memset(&peer, 0, sizeof(struct sockaddr_in));
    srv_sock.sin_family = AF_INET;
    srv_sock.sin_port = htons(UDPV4_SERVER_PORT);
    srv_sock.sin_addr.s_addr = htonl(INADDR_ANY);

    status = bind(sock_fd, (struct sockaddr *)&srv_sock, addrlen); // to use specific Port
    if (status < 0) {
        printf("[%s] Failed to bind() !!!\n", __func__);
        close(sock_fd); 
        vTaskDelete(NULL);
        return;
    }

    while (TRUE) {
        // Receive data from peer
        memset(rx_buf, 0, BUF_MAX_LEN);
        status = recvfrom(sock_fd, rx_buf, sizeof(rx_buf), 0, (struct sockaddr *)&peer, &addrlen);
        if (status < 0) {
            printf("[%s] Failed to recvfrom() !!!\n", __func__);
            close(sock_fd);
            vTaskDelete(NULL);
            return;
        }
        printf(">>> Receive from UDP Socket : %s\n", rx_buf);

        // Send test string
        status = sendto(sock_fd, udpv4_test_str, strlen(udpv4_test_str), 0,
                        (struct sockaddr *)&peer, sizeof(peer));
        if (status < 0) {
            printf("[%s] Failed to sendto() !!!\n", __func__);

            close(sock_fd);
            vTaskDelete(NULL);
            return;
        }
        printf("<<< Send to UDP Socket : %s\n", udpv4_test_str);
    }

    close(sock_fd);
    vTaskDelete(NULL);
}
#endif //  __TEST_UDP_SESSION_OVER_IPV4__

#if defined ( __TEST_TCP_SERVER_OVER_IPV6__ )    // ========================== //
#define TCPS_OVER_IPV6    "TCPS_IPV6"
#define TCPV6_LOCAL_PORT    10193

static void tcp_server_over_IPv6(void *arg)
{
    int sock_fd = -1, client_sock_fd = -1;
    struct sockaddr_in6 server_addr, client_addr;
    socklen_t client_addr_len;
    char str_addr[INET6_ADDRSTRLEN];
    int ret, flag;
    char rx_buf[128];
 
    while (ra6w1_network_main_check_network_ready(0) == pdFALSE
        || ra6w1_network_main_check_global_unicast_ip6_addr(0) == pdFALSE) {
        vTaskDelay(portCONVERT_MS_2_TICKS(1000));
    }

    printf("\n- Start TCP(v6) Server (Port : @%d)\n", TCPV6_LOCAL_PORT);

    /* Create socket for listening (client requests) */
    sock_fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    if (sock_fd == -1) {
        printf("[%s] Failed to create socket() ...\n", __func__);
        vTaskDelete(NULL);
        return;
    }
 
    /* Set socket to reuse address */
    flag = 1;
    ret = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    if (ret == -1) {
        printf("[%s] Failed to setsockopt() ...\n", __func__);
        close(sock_fd);
        vTaskDelete(NULL);
        return;
    }

    server_addr.sin6_family = AF_INET6;
    server_addr.sin6_addr = in6addr_any;
    server_addr.sin6_port = htons(TCPV6_LOCAL_PORT);
 
    /* Bind address and socket together */
    ret = bind(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (ret == -1) {
        printf("[%s] Failed to bind() ...\n", __func__);
        close(sock_fd);
        vTaskDelete(NULL);
        return;
    }
 
    /* Create listening queue (client requests) */
    ret = listen(sock_fd, 5);
    if (ret == -1) {
        printf("[%s] Failed to listen() ...\n", __func__);
        close(sock_fd);
        vTaskDelete(NULL);
        return;
    }
 
    client_addr_len = sizeof(client_addr);
 
    while(1) {
        /* Do TCP handshake with client */
        client_sock_fd = accept(sock_fd, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_sock_fd == -1) {
            printf("[%s] Failed to accept() ...\n", __func__);
            close(sock_fd);
            vTaskDelete(NULL);
            return;
        }
 
        inet_ntop(AF_INET6, &(client_addr.sin6_addr), str_addr, sizeof(str_addr));
        printf("- New connection from: %s@%d\n", str_addr, ntohs(client_addr.sin6_port));
 
        memset(rx_buf, 0, 128);

        /* Wait for data from client */
        ret = read(client_sock_fd, rx_buf, 128);
        if (ret == -1) {
            printf("[%s] Failed to read() ...\n", __func__);
            close(client_sock_fd);
            continue;
        }
        printf(">>> Receive from TCP Client : %s\n", rx_buf);

        /* Send response to client */
        strcpy(&rx_buf[strlen(rx_buf)], " - Echo");
        ret = write(client_sock_fd, rx_buf, strlen(rx_buf));
        if (ret == -1) {
            printf("[%s] Failed to write() ...\n", __func__);
            close(client_sock_fd);
            continue;
        }
        printf("<<< Send from TCP Client : %s\n", rx_buf);
 
        /* Do TCP teardown */
        ret = close(client_sock_fd);
        if (ret == -1) {
            printf("[%s] Failed to close() ...\n", __func__);
            client_sock_fd = -1;
        }
 
        printf("- Connection closed ...\n");
    }

    vTaskDelete(NULL);
}
#endif // __TEST_TCP_SERVER_OVER_IPV6__

#if defined ( __TEST_UDP_SESSION_OVER_IPV4__ )   // ========================== //

#define UDP_SESS_OVER_IPV4 "UDP_IPV4"
#define UDPV4_SERVER_PORT    20190

static char *udpv4_test_str = "Hello World over IPv4 - UDP";

static void udp_over_IPv4(void *arg)
{
    int sock_fd;
    int status;
    char rx_buf[BUF_MAX_LEN];
    struct sockaddr_in srv_sock;
    struct sockaddr_in peer;
    socklen_t addrlen = sizeof(struct sockaddr_in);

    while (ra6w1_network_main_check_network_ready(0) == pdFALSE) {
        vTaskDelay(portCONVERT_MS_2_TICKS(1000));
    }

    printf("\n- Start UDP(v4) Socket \n");

    sock_fd = socket(PF_INET, SOCK_DGRAM, 0); // IPPROTO_IP=0, IPPROTO_UDP=17
    if (sock_fd == -1) {
        printf("[%s] Failed to socket() !!!\n", __func__);
        vTaskDelete(NULL);
        return;
    }
 
    memset(&srv_sock, 0, sizeof(struct sockaddr_in));
    memset(&peer, 0, sizeof(struct sockaddr_in));
    srv_sock.sin_family = AF_INET;
    srv_sock.sin_port = htons(UDPV4_SERVER_PORT);
    srv_sock.sin_addr.s_addr = htonl(INADDR_ANY);

    status = bind(sock_fd, (struct sockaddr *)&srv_sock, addrlen); // to use specific Port
    if (status < 0) {
        printf("[%s] Failed to bind() !!!\n", __func__);
        close(sock_fd); 
        vTaskDelete(NULL);
        return;
    }

    while (TRUE) {
        // Receive data from peer
        memset(rx_buf, 0, BUF_MAX_LEN);
        status = recvfrom(sock_fd, rx_buf, sizeof(rx_buf), 0, (struct sockaddr *)&peer, &addrlen);
        if (status < 0) {
            printf("[%s] Failed to recvfrom() !!!\n", __func__);
            close(sock_fd);
            vTaskDelete(NULL);
            return;
        }
        printf(">>> Receive from UDP Socket : %s\n", rx_buf);

        // Send test string
        status = sendto(sock_fd, udpv4_test_str, strlen(udpv4_test_str), 0,
                        (struct sockaddr *)&peer, sizeof(peer));
        if (status < 0) {
            printf("[%s] Failed to sendto() !!!\n", __func__);

            close(sock_fd);
            vTaskDelete(NULL);
            return;
        }
        printf("<<< Send to UDP Socket : %s\n", udpv4_test_str);
    }

    close(sock_fd);
    vTaskDelete(NULL);
}
#endif //  __TEST_UDP_SESSION_OVER_IPV4__

#if defined ( __TEST_TCP_SERVER_OVER_IPV6_DPM__ )    // ========================== //
#define TCPS_OVER_IPV6    "TCPS_IPV6"
#define TCPV6_LOCAL_PORT    10193

static void tcp_server_over_IPv6(void *arg)
{
    int sock_fd = -1, client_sock_fd = -1;
    struct sockaddr_in6 server_addr, client_addr;
    socklen_t client_addr_len;
    char str_addr[INET6_ADDRSTRLEN];
    int ret, flag;
    char rx_buf[128];
 
    while (ra6w1_network_main_check_network_ready(0) == pdFALSE
        || ra6w1_network_main_check_global_unicast_ip6_addr(0) == pdFALSE) {
        vTaskDelay(portCONVERT_MS_2_TICKS(1000));
    }

    printf("\n- Start TCP(v6) Server (Port : @%d)\n", TCPV6_LOCAL_PORT);

    /* Create socket for listening (client requests) */
    sock_fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    if (sock_fd == -1) {
        printf("[%s] Failed to create socket() ...\n", __func__);
        vTaskDelete(NULL);
        return;
    }
 
    /* Set socket to reuse address */
    flag = 1;
    ret = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    if (ret == -1) {
        printf("[%s] Failed to setsockopt() ...\n", __func__);
        close(sock_fd);
        vTaskDelete(NULL);
        return;
    }

    server_addr.sin6_family = AF_INET6;
    server_addr.sin6_addr = in6addr_any;
    server_addr.sin6_port = htons(TCPV6_LOCAL_PORT);
 
    /* Bind address and socket together */
    ret = bind(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (ret == -1) {
        printf("[%s] Failed to bind() ...\n", __func__);
        close(sock_fd);
        vTaskDelete(NULL);
        return;
    }
 
    /* Create listening queue (client requests) */
    ret = listen(sock_fd, 5);
    if (ret == -1) {
        printf("[%s] Failed to listen() ...\n", __func__);
        close(sock_fd);
        vTaskDelete(NULL);
        return;
    }
 
    client_addr_len = sizeof(client_addr);
 
    while(1) {
        /* Do TCP handshake with client */
        client_sock_fd = accept(sock_fd, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_sock_fd == -1) {
            printf("[%s] Failed to accept() ...\n", __func__);
            close(sock_fd);
            vTaskDelete(NULL);
            return;
        }
 
        inet_ntop(AF_INET6, &(client_addr.sin6_addr), str_addr, sizeof(str_addr));
        printf("- New connection from: %s@%d\n", str_addr, ntohs(client_addr.sin6_port));
 
        memset(rx_buf, 0, 128);

        /* Wait for data from client */
        ret = read(client_sock_fd, rx_buf, 128);
        if (ret == -1) {
            printf("[%s] Failed to read() ...\n", __func__);
            close(client_sock_fd);
            continue;
        }
        printf(">>> Receive from TCP Client : %s\n", rx_buf);

        /* Send response to client */
        strcpy(&rx_buf[strlen(rx_buf)], " - Echo");
        ret = write(client_sock_fd, rx_buf, strlen(rx_buf));
        if (ret == -1) {
            printf("[%s] Failed to write() ...\n", __func__);
            close(client_sock_fd);
            continue;
        }
        printf("<<< Send from TCP Client : %s\n", rx_buf);
 
        /* Do TCP teardown */
        ret = close(client_sock_fd);
        if (ret == -1) {
            printf("[%s] Failed to close() ...\n", __func__);
            client_sock_fd = -1;
        }
 
        printf("- Connection closed ...\n");
    }

    vTaskDelete(NULL);
}
#endif // __TEST_TCP_SERVER_OVER_IPV6_DPM__

#if defined ( __TEST_TCP_SERVER_OVER_IPV4__ )    // ========================== //
#define TCPS_OVER_IPV4    "TCPS_IPV4"
#define TCPV4_LOCAL_PORT    20193

static void tcp_server_over_IPv4(void *arg)
{
    int sock_fd = -1, client_sock_fd = -1;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len;
    char str_addr[INET_ADDRSTRLEN];
    int ret, flag;
    char rx_buf[128];

    while (ra6w1_network_main_check_network_ready(0) == pdFALSE) {
        vTaskDelay(portCONVERT_MS_2_TICKS(1000));
    }

    printf("\n- Start TCP(v4) Server (Port : @%d)\n", TCPV4_LOCAL_PORT);

    /* Create socket for listening (client requests) */
    sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock_fd == -1) {
        printf("[%s] Failed to create socket() ...\n", __func__);
        vTaskDelete(NULL);
        return;
    }

    /* Set socket to reuse address */
    flag = 1;
    ret = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    if (ret == -1) {
        printf("[%s] Failed to setsockopt() ...\n", __func__);
        close(sock_fd);
        vTaskDelete(NULL);
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(TCPV4_LOCAL_PORT);

    /* Bind address and socket together */
    ret = bind(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (ret == -1) {
        printf("[%s] Failed to bind() ...\n", __func__);
        close(sock_fd);
        vTaskDelete(NULL);
        return;
    }

    /* Create listening queue (client requests) */
    ret = listen(sock_fd, 5);
    if (ret == -1) {
        printf("[%s] Failed to listen() ...\n", __func__);
        close(sock_fd);
        vTaskDelete(NULL);
        return;
    }

    client_addr_len = sizeof(client_addr);

    while(1) {
        /* Do TCP handshake with client */
        client_sock_fd = accept(sock_fd, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_sock_fd == -1) {
            printf("[%s] Failed to accept() ...\n", __func__);
            close(sock_fd);
            vTaskDelete(NULL);
            return;
        }

        inet_ntop(AF_INET, &(client_addr.sin_addr), str_addr, sizeof(str_addr));
        printf("- New connection from: %s@%d\n", str_addr, ntohs(client_addr.sin_port));

        memset(rx_buf, 0, 128);

        /* Wait for data from client */
        ret = read(client_sock_fd, rx_buf, 128);
        if (ret == -1) {
            printf("[%s] Failed to read() ...\n", __func__);
            close(client_sock_fd);
            continue;
        }
        printf(">>> Receive from TCP Client : %s\n", rx_buf);

        /* Send response to client */
        strcpy(&rx_buf[strlen(rx_buf)], " - Echo");
        ret = write(client_sock_fd, rx_buf, strlen(rx_buf));
        if (ret == -1) {
            printf("[%s] Failed to write() ...\n", __func__);
            close(client_sock_fd);
            continue;
        }
        printf("<<< Send from TCP Client : %s\n", rx_buf);

        /* Do TCP teardown */
        ret = close(client_sock_fd);
        if (ret == -1) {
            printf("[%s] Failed to close() ...\n", __func__);
            client_sock_fd = -1;
        }

        printf("- Connection closed ...\n");
    }

    vTaskDelete(NULL);
}
#endif // __TEST_TCP_SERVER_OVER_IPV4__

#if defined ( __TEST_TCP_CLIENT_OVER_IPV6__ )    // ========================== //

#define TCPC_OVER_IPV6     "TCPC_IPV6"
//#define TCP_SERVER_ADDR    "fe80::2509:b916:123a:12b0"  // Office-WLAN
#define TCPV6_SERVER_ADDR    "fe80::3268:1434:aadd:733d"  // Office-LAN
//#define TCP_SERVER_ADDR    "fe80::65fe:b813:2479:ab35"  // Home-WLAN
#define TCPV6_SERVER_PORT    10192

static char *tcpv6_test_str = "Hello World over IPv6 - TCP";
static void tcp_client_over_IPv6(void *arg)
{
    int sock_fd = -1;
    struct sockaddr_in6 remote;
    
    int ret;
    char dst_ip[IPV6_ADDR_LEN];
    char rx_buf[128];
    char rx_len;
 
    while (ra6w1_network_main_check_network_ready(0) == pdFALSE 
        || ra6w1_network_main_check_global_unicast_ip6_addr(0) == pdFALSE) {
        vTaskDelay(portCONVERT_MS_2_TICKS(1000));
    }

    printf("\n- Start TCP(v6) Client ... (Server : %s@%d)\n", TCPV6_SERVER_ADDR, TCPV6_SERVER_PORT);
    /* Create socket for communication with server */
    sock_fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP); // IPPROTO_IP=0, IPPROTO_TCP=6
    if (sock_fd == -1) {
        printf("[%s] Failed to socket() !!!\n", __func__);
        vTaskDelete(NULL);
        return;
    }

    /* Bind to server running on localhost */
    remote.sin6_family= AF_INET6;
    remote.sin6_port = htons(TCPV6_SERVER_PORT);
    inet_pton(AF_INET6, TCPV6_SERVER_ADDR, dst_ip);
    memcpy(remote.sin6_addr.s6_addr, dst_ip, IPV6_ADDR_LEN);

    /* Try to do TCP handshake with server */
    ret = connect(sock_fd, (struct sockaddr*)&remote, sizeof(remote));
    
    if (ret == -1) {
        printf("[%s] Failed to connect() !!!\n", __func__);
        close(sock_fd);
        vTaskDelete(NULL);
        return;
    }

    /* Wait for data from server */
    memset(rx_buf, 0, 128);
    ret = read(sock_fd, rx_buf, 128);
    if (ret == -1) {
        printf("[%s] Failed to read() !!!\n", __func__);
        close(sock_fd);
        vTaskDelete(NULL);
        return;
    }
    printf(">>> Received from TCP server : %s\n", rx_buf);
 
    /* Send data to server */
    ret = write(sock_fd, tcpv6_test_str, strlen(tcpv6_test_str));
    if (ret == -1) {
        printf("[%s] Failed to write() !!!\n", __func__);
        vTaskDelete(NULL);
        close(sock_fd);
        return;
    }
    printf("<<< Send to TCP server : %s\n", tcpv6_test_str);
 
    /* DO TCP teardown */
    ret = close(sock_fd);
    if (ret == -1) {
        printf("[%s] Failed to close() !!!\n", __func__);
    }

    vTaskDelete(NULL);
}
#endif // __TEST_TCP_CLIENT_OVER_IPV6__

#if defined ( __TEST_TCP_CLIENT_OVER_IPV6_DPM__ )    // ========================== //
#if CFG_PMGR
#define TCPC_OVER_IPV6_DPM     "TCPC_IPV6_DPM"
//#define TCP_SERVER_ADDR    "fe80::2509:b916:123a:12b0"  // Office-WLAN
#define TCPV6_SERVER_ADDR    "fd96:8dfc:9995::60a"  // Office-LAN
//#define TCP_SERVER_ADDR    "fe80::65fe:b813:2479:ab35"  // Home-WLAN
#define TCPV6_SERVER_PORT    20192
#define TCPV6_LOCAL_PORT    20193
#define	TCP_CLI_RX_TIMEOUT		1000000		// TEMPORARY 10 Sec -->1000sec(16min 40 sec)

static char *tcpv6_test_str = "Hello World over IPv6 - TCP";
static void tcp_client_over_IPv6_dpm(void *arg)
{
    int sock_fd = -1;
    struct sockaddr_in6 remote;
    struct sockaddr_in6 local;

    int ret;
    char dst_ip[IPV6_ADDR_LEN];
    char rx_buf[128];
    char rx_len;
    struct timeval   tv;
    pmgr_instance_ctrl_t * p_instance_ctrl = RM_PMGR_W_get_ctrl();

#if 0
    while (ra6w1_network_main_check_network_ready(0) == pdFALSE
        || ra6w1_network_main_check_global_unicast_ip6_addr(0) == pdFALSE) {
        vTaskDelay(portCONVERT_MS_2_TICKS(1000));
    }
#endif

    printf("\n- Start TCP(v6) Client ... (Server : %s@%d)\n", TCPV6_SERVER_ADDR, TCPV6_SERVER_PORT);
    /* Create socket for communication with server */
    sock_fd = socket_dpm(TCPC_OVER_IPV6_DPM, AF_INET6, SOCK_STREAM, IPPROTO_TCP); // IPPROTO_IP=0, IPPROTO_TCP=6
    if (sock_fd == -1) {
        printf("[%s] Failed to socket() !!!\n", __func__);
        vTaskDelete(NULL);
        return;
    }

    /* Bind to server running on localhost */
    remote.sin6_family= AF_INET6;
    remote.sin6_port = htons(TCPV6_SERVER_PORT);
    inet_pton(AF_INET6, TCPV6_SERVER_ADDR, dst_ip);
    memcpy(remote.sin6_addr.s6_addr, dst_ip, IPV6_ADDR_LEN);

    local.sin6_family = AF_INET6;
    local.sin6_addr = in6addr_any;
    local.sin6_port = htons(TCPV6_LOCAL_PORT);

    ret = bind(sock_fd, (struct sockaddr *)&local, sizeof(struct sockaddr_in6));
	if (ret != 0) {
		printf("[%s] Failed to socket() !!!\n", __func__);
		vTaskDelete(NULL);
		return;
	}

    /* Try to do TCP handshake with server */
    ret = connect(sock_fd, (struct sockaddr*)&remote, sizeof(remote));

    if (ret == -1) {
        printf("[%s] Failed to connect() !!!\n", __func__);
        close(sock_fd);
        vTaskDelete(NULL);
        return;
    }

    /* Wait for data from server */
	RM_WIFI_dpm_tcp_port_filter_set(TCPV6_LOCAL_PORT);			// RM_WIFI_dpm_udp_port_filter_set
	RM_PMGR_W_dpm_rcv_ready_set(TCPC_OVER_IPV6_DPM);		// Ready receive packet

	tv.tv_sec = 0;
	tv.tv_usec = TCP_CLI_RX_TIMEOUT * 1000;
	int status = 0;
	status = setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, (const void *)&tv, sizeof(struct timeval));
	if (status) {
		printf(RED_COLOR "[%s:%d] setsockopt error %d \n" CLEAR_COLOR, __func__, __LINE__, status);
	}

    while (TRUE) {
    	memset(rx_buf, 0, 128);
    	printf(ANSI_COLOR_LIGHT_CYAN">>> ready to read \n"ANSI_COLOR_DEFULT);
		ret = read(sock_fd, rx_buf, 128);
		if (ret == -1) {
			printf(ANSI_COLOR_RED"[%s] Failed to read() !!!\n"ANSI_COLOR_DEFULT, __func__);
			close(sock_fd);
			vTaskDelete(NULL);
			return;
		}
		printf(ANSI_COLOR_LIGHT_CYAN">>> Received from TCP server : %s\n"ANSI_COLOR_DEFULT, rx_buf);

		RM_PMGR_W_dpm_sleep_ready_clear(TCPC_OVER_IPV6_DPM);

		/* Send data to server */
		ret = write(sock_fd, tcpv6_test_str, strlen(tcpv6_test_str));
		if (ret == -1) {
			printf(ANSI_COLOR_RED"[%s] Failed to write() !!!\n"ANSI_COLOR_DEFULT, __func__);
			vTaskDelete(NULL);
			close(sock_fd);
			return;
		}
		printf(ANSI_COLOR_LIGHT_CYAN"<<< Send to TCP server : %s\n"ANSI_COLOR_DEFULT, tcpv6_test_str);
		RM_PMGR_W_dpm_sleep_ready_set(TCPC_OVER_IPV6_DPM);
    }

    RM_PMGR_W_dpm_sleep_ready_set(TCPC_OVER_IPV6_DPM);
    /* DO TCP teardown */
    ret = close(sock_fd);
    if (ret == -1) {
        printf("[%s] Failed to close() !!!\n", __func__);
    }

    vTaskDelete(NULL);
}
#endif /* CFG_PMGR */
#endif // __TEST_TCP_CLIENT_OVER_IPV6_DPM__

#if defined ( __TEST_TCP_CLIENT_OVER_IPV4__ )    // ========================== //

#define TCPC_OVER_IPV4       "TCPC_IPV4"
#define TCPV4_SERVER_ADDR    "192.168.1.199"  // Office-LAN
#define TCPV4_SERVER_PORT    20192

static char *tcpv4_test_str = "Hello World over IPv4- TCP";
static void tcp_client_over_IPv4(void *arg)
{
    int sock_fd = -1;
    struct sockaddr_in  remote;
    int ret;
    char dst_ip[IPV4_ADDR_LEN];
    char rx_buf[128];
    char rx_len;
 
    while (ra6w1_network_main_check_network_ready(0) == pdFALSE) {
        vTaskDelay(portCONVERT_MS_2_TICKS(1000));
    }

    printf("\n- Start TCP(v4) Client ... (Server : %s@%d)\n", TCPV4_SERVER_ADDR, TCPV4_SERVER_PORT);

    /* Create socket for communication with server */
    sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); // IPPROTO_IP=0, IPPROTO_TCP=6
    if (sock_fd == -1) {
        printf("[%s] Failed to socket() !!!\n", __func__);
        vTaskDelete(NULL);
        return;
    }

    /* Bind to server running on localhost */
    remote.sin_family= AF_INET;
    remote.sin_port= htons(TCPV4_SERVER_PORT);
    remote.sin_addr.s_addr = inet_addr(TCPV4_SERVER_ADDR);

    /* Try to do TCP handshake with server */
    ret = connect(sock_fd, (struct sockaddr*)&remote, sizeof(remote));

    if (ret == -1) {
        printf("[%s] Failed to connect() !!!\n", __func__);
        close(sock_fd);
        vTaskDelete(NULL);
        return;
    }

    /* Wait for data from server */
    memset(rx_buf, 0, 128);
    ret = read(sock_fd, rx_buf, 128);
    if (ret == -1) {
        printf("[%s] Failed to read() !!!\n", __func__);
        close(sock_fd);
        vTaskDelete(NULL);
        return;
    }
    printf(">>> Received from TCP server : %s\n", rx_buf);
 
    /* Send data to server */
    ret = write(sock_fd, tcpv4_test_str, strlen(tcpv4_test_str));
    if (ret == -1) {
        printf("[%s] Failed to write() !!!\n", __func__);
        vTaskDelete(NULL);
        close(sock_fd);
        return;
    }
    printf("<<< Send to TCP server : %s\n", tcpv4_test_str);
 
    /* DO TCP teardown */
    ret = close(sock_fd);
    if (ret == -1) {
        printf("[%s] Failed to close() !!!\n", __func__);
    }

    vTaskDelete(NULL);
}
#endif // __TEST_TCP_CLIENT_OVER_IPV4__

// ============================================================================ //



/*============================================================
 *
 * Customer's applications ...
 *
 *============================================================*/


///////////////////////////////////////////////////////////////////////////////
////  Customer call-back function to notify WI-Fi connection status
///////////////////////////////////////////////////////////////////////////////

#if defined ( __SUPPORT_WIFI_CONN_CB__ )
static void user_wifi_conn(void *arg)
{
    RA6W1_UNUSED_ARG(arg);

    EventBits_t wifi_conn_ev_bits;

#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
    uint8_t task_wdog_id = WATCHDOG_SERVICE_W_NOT_REGISTERED_ID;
    g_wifi_cfg.p_watchdog_service->p_api->registerTask(g_wifi_cfg.p_watchdog_service->p_ctrl, g_wifi_cfg.p_watchdog_service->p_cfg, &task_wdog_id);
#endif

    while (true) {
#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
        g_wifi_cfg.p_watchdog_service->p_api->notify(g_wifi_cfg.p_watchdog_service->p_ctrl, g_wifi_cfg.p_watchdog_service->p_cfg, task_wdog_id);
        g_wifi_cfg.p_watchdog_service->p_api->suspend(g_wifi_cfg.p_watchdog_service->p_ctrl, task_wdog_id);
#endif

        wifi_conn_ev_bits = xEventGroupWaitBits(
                                       evt_grp_wifi_conn_notify,
                                       (
                                           (WIFI_CONN_SUCC_STA | WIFI_CONN_SUCC_SOFTAP)
                                         | (WIFI_CONN_FAIL_STA | WIFI_CONN_FAIL_SOFTAP)
                                         | (WIFI_DISCONN_STA   | WIFI_DISCONN_SOFTAP  )
                                       ),
                                       pdTRUE,
                                       pdFALSE,
                                       WIFI_CONN_NOTI_WAIT_TICK);

#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
        g_wifi_cfg.p_watchdog_service->p_api->resumeAndNotify(g_wifi_cfg.p_watchdog_service->p_ctrl, g_wifi_cfg.p_watchdog_service->p_cfg, task_wdog_id);
#endif

        if (wifi_conn_ev_bits & WIFI_CONN_SUCC_STA) {
            PRINTF("\n### User Call-back : Success to connect Wi-Fi ...\n");
        } else if (wifi_conn_ev_bits & WIFI_CONN_SUCC_SOFTAP) {
            PRINTF("\n### User Call-back : Success to connect Wi-Fi ...\n");
        }

        if (wifi_conn_ev_bits & WIFI_CONN_FAIL_STA) {
            PRINTF("\n### User Call-back : Failed to connect Wi-Fi ( reason_code = %d ) ...\n", wifi_conn_fail_reason);
        } else if (wifi_conn_ev_bits & WIFI_CONN_FAIL_SOFTAP) {
            PRINTF("\n### User Call-back : Failed to connect Wi-Fi ( reason_code = %d ) ...\n", ap_wifi_conn_fail_reason);
        }

        if (wifi_conn_ev_bits & WIFI_DISCONN_STA) {
            PRINTF("\n### User Call-back : Wi-Fi disconnected ( reason_code = %d ) ...\n", wifi_disconn_reason);
        } else if (wifi_conn_ev_bits & WIFI_DISCONN_SOFTAP) {
            PRINTF("\n### User Call-back : Disassociated - STA has left ( reason_code = %d ) ...\n", ap_wifi_disconn_reason);
        }
    }

#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
    g_wifi_cfg.p_watchdog_service->p_api->unregisterTask(g_wifi_cfg.p_watchdog_service->p_ctrl, task_wdog_id);
#endif

    vTaskDelete(NULL);
}
#endif // __SUPPORT_WIFI_CONN_CB__


/**********************************************************
 * Customer's application table
 **********************************************************/

const app_task_info_t    user_apps_table[] = {
/* name, func, stack_size, priority, net_chk_flag, dpm_flag, port_no, run_sys_mode */


/*
 * !!! Caution !!!
 *
 *     User applications should not affect the operation of the Sample code.
 */

/*  Task Name,          Funtion,         Stack Size, Task Priority,             NW Flag, DPM,   NW Port,    Sys_Mode  */
#if defined ( __SUPPORT_WIFI_CONN_CB__ )
  { WIFI_CONN,          user_wifi_conn,         256, (OS_TASK_PRIORITY_USER + 1), FALSE, FALSE, UNDEF_PORT, RUN_ALL_MODE   },
#endif  // __SUPPORT_WIFI_CONN_CB__

#if !defined(__SUPPORT_MATTER_IOT__)
#if defined ( __SUPPORT_HELLO_WORLD__ )
  { HELLO_WORLD_1,      hello_world_1,          128, (OS_TASK_PRIORITY_USER + 1), FALSE, FALSE, UNDEF_PORT, RUN_ALL_MODE   },
  { HELLO_WORLD_2,      hello_world_2,          128, (OS_TASK_PRIORITY_USER + 1), TRUE,  TRUE,  UNDEF_PORT, RUN_ALL_MODE   },
#endif  // __SUPPORT_HELLO_WORLD__
#endif  // (__SUPPORT_MATTER_IOT__)

#ifdef DPM_TIMER_USER_TEST
#if CFG_PMGR
  { DPM_TIMER_TEST,     timer_test,             256, (OS_TASK_PRIORITY_USER + 2), FALSE, TRUE,  UNDEF_PORT, RUN_STA_MODE   },
#endif /* CFG_PMGR */
#endif    // DPM_TIMER_USER_TEST

// For test sample ........................................
#if defined ( __TEST_UDP_SESSION_OVER_IPV6__ )
  { UDP_SESS_OVER_IPV6,  udp_over_IPv6,          256, (OS_TASK_PRIORITY_USER + 2), FALSE, TRUE, UNDEF_PORT, RUN_STA_MODE   },
#endif // __TEST_UDP_SESSION_OVER_IPV6__
#if defined ( __TEST_UDP_SESSION_OVER_IPV6_DPM__ )		// UDP_DPM_TEST
#if CFG_PMGR
  { UDP_SESS_OVER_IPV6_DPM,  udp_over_IPv6_dpm,  256, (OS_TASK_PRIORITY_USER + 2), TRUE, TRUE, UDP_DPM_SVR_PORT_DEF, RUN_STA_MODE   },
#endif /* CFG_PMGR */
#endif // __TEST_UDP_SESSION_OVER_IPV6_DPM__
#if defined ( __TEST_UDP_SESSION_OVER_IPV4__ )
  { UDP_SESS_OVER_IPV4,  udp_over_IPv4,          256, (OS_TASK_PRIORITY_USER + 2), FALSE, TRUE, UNDEF_PORT, RUN_STA_MODE   },
#endif // __TEST_UDP_SESSION_OVER_IPV4__
#if defined ( __TEST_TCP_SERVER_OVER_IPV6__ )
  { TCPS_OVER_IPV6,      tcp_server_over_IPv6,   256, (OS_TASK_PRIORITY_USER + 2), FALSE, TRUE, UNDEF_PORT, RUN_STA_MODE   },
#endif // __TEST_TCP_SERVER_OVER_IPV6__
#if defined ( __TEST_TCP_SERVER_OVER_IPV4__ )
  { TCPS_OVER_IPV4,      tcp_server_over_IPv4,   256, (OS_TASK_PRIORITY_USER + 2), FALSE, TRUE, UNDEF_PORT, RUN_STA_MODE   },
#endif // __TEST_TCP_SERVER_OVER_IPV4__
#if defined ( __TEST_TCP_CLIENT_OVER_IPV6__ )
  { TCPC_OVER_IPV6,      tcp_client_over_IPv6,   256, (OS_TASK_PRIORITY_USER + 2), FALSE, TRUE, UNDEF_PORT, RUN_STA_MODE   },
#endif // __TEST_TCP_CLIENT_OVER_IPV6__
#if defined ( __TEST_TCP_CLIENT_OVER_IPV6_DPM__ )
#if CFG_PMGR
  { TCPC_OVER_IPV6_DPM,      tcp_client_over_IPv6_dpm,   256, (OS_TASK_PRIORITY_USER + 2), TRUE, TRUE, TCPV6_LOCAL_PORT, RUN_STA_MODE   },
#endif /* CFG_PMGR */
#endif // __TEST_TCP_CLIENT_OVER_IPV6_DPM__
#if defined ( __TEST_TCP_CLIENT_OVER_IPV4__ )
  { TCPC_OVER_IPV4,      tcp_client_over_IPv4,   256, (OS_TASK_PRIORITY_USER + 2), FALSE, TRUE, UNDEF_PORT, RUN_STA_MODE   },
#endif // __TEST_TCP_CLIENT_OVER_IPV4__
#if (SIGMA_TEST_APP == 1)
  { "SIGMA",             sigma_init,           200 * OS_STACK_WORD_SIZE, (OS_TASK_PRIORITY_USER + 1), FALSE, FALSE, UNDEF_PORT, RUN_ALL_MODE   },
#endif
  { NULL,    NULL,    0, 0, FALSE, FALSE, UNDEF_PORT, 0    }
};

/* EOF */

