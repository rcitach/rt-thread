/* Minimal TCP throughput test for the Wi-Fi SDK/RT-Thread port. */
#include <rtthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lwip/sockets.h>
#include <lwip/inet.h>
#include <lwip/errno.h>

#define WIFI_SPEED_BUF_SIZE 512

extern void wifi_lwip_adapter_dump_stats(void);

static void print_rate(const char *mode, size_t bytes, rt_tick_t elapsed)
{
    rt_uint64_t ms = ((rt_uint64_t)elapsed * 1000U) / RT_TICK_PER_SECOND;
    rt_uint64_t kbps;

    if (ms == 0)
        ms = 1;
    kbps = ((rt_uint64_t)bytes * 8U * 1000U) / (ms * 1024U);
    printf("[wifi-speed] %s: %u bytes, %u ms, %u.%03u Mbps\n",
           mode, (unsigned)bytes, (unsigned)ms,
           (unsigned)(kbps / 1000U), (unsigned)(kbps % 1000U));
}

static int wifi_speed_test(int argc, char **argv)
{
    int fd, port, seconds, direction, udp;
    int consecutive_errors = 0;
    struct timeval timeout;
    struct sockaddr_in addr;
    char buffer[WIFI_SPEED_BUF_SIZE];
    size_t total = 0;
    rt_tick_t start, deadline;
    int errors = 0;

    if (argc != 5 || (strcmp(argv[1], "tx") && strcmp(argv[1], "rx") &&
                       strcmp(argv[1], "udp_tx") && strcmp(argv[1], "udp_rx")))
    {
        printf("usage: wifi_speed_test <tx|rx|udp_tx|udp_rx> <server_ip> <port> <seconds>\n");
        printf("  tx: send data to a TCP discard/echo server\n");
        printf("  rx: receive data from a TCP data server\n");
        printf("  udp_tx/udp_rx: send/receive UDP data\n");
        return -RT_ERROR;
    }

    port = atoi(argv[3]);
    seconds = atoi(argv[4]);
    if (port <= 0 || port > 65535 || seconds <= 0 || seconds > 3600)
        return -RT_EINVAL;
    udp = (strncmp(argv[1], "udp_", 4) == 0);
    direction = (strcmp(argv[1], "tx") == 0 || strcmp(argv[1], "udp_tx") == 0);

    fd = lwip_socket(AF_INET, udp ? SOCK_DGRAM : SOCK_STREAM, 0);
    if (fd < 0)
        return -RT_ERROR;

    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;
    (void)lwip_setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    (void)lwip_setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u16_t)port);
    addr.sin_addr.s_addr = inet_addr(argv[2]);
    if (lwip_connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        printf("[wifi-speed] connect failed\n");
        lwip_close(fd);
        return -RT_ERROR;
    }

    memset(buffer, 0x5a, sizeof(buffer));
    if (udp && !direction)
    {
        /* Let a UDP test server learn the board's address before RX starts. */
        (void)lwip_send(fd, buffer, 1, 0);
    }
    start = rt_tick_get();
    deadline = start + (rt_tick_t)seconds * RT_TICK_PER_SECOND;
    while ((rt_int32_t)(rt_tick_get() - deadline) < 0)
    {
        int n;
        if (direction)
            n = lwip_send(fd, buffer, sizeof(buffer), 0);
        else
            n = lwip_recv(fd, buffer, sizeof(buffer), 0);
        if (n < 0)
        {
            if (udp && direction && errno == ENOMEM)
            {
                errors++;
                consecutive_errors++;
                if (consecutive_errors >= 100)
                {
                    printf("[wifi-speed] TX queue unavailable, aborting\n");
                    break;
                }
                rt_thread_mdelay(2);
                continue;
            }
            printf("[wifi-speed] %s failed, errno=%d\n",
                   udp ? "UDP send/recv" : "TCP send/recv", errno);
            break;
        }
        if (n == 0)
            break;
        consecutive_errors = 0;
        total += (size_t)n;
    }

    print_rate(udp ? (direction ? "UDP TX" : "UDP RX") :
               (direction ? "TCP TX" : "TCP RX"),
               total, rt_tick_get() - start);
    if (errors != 0)
        printf("[wifi-speed] ENOMEM retries=%d\n", errors);
    lwip_shutdown(fd, SHUT_RDWR);
    lwip_close(fd);

    return RT_EOK;
}
MSH_CMD_EXPORT(wifi_speed_test, test Wi-Fi TCP throughput: tx/rx ip port seconds);


// wifi_wlan_test connect xiaomi 12345678

// wifi_speed_test udp_tx 10.191.9.144 5002 1
