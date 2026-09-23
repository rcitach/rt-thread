#include <rtthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lwip/sockets.h>
#include <lwip/inet.h>
#include <lwip/errno.h>
#include <lwip/opt.h>
#include <lwip/etharp.h>
#include <lwip/tcpip.h>

#define SPEED_BUF_MAX           1472U   /* 单包最大载荷（避免 IP 分片） */
#define SPEED_DEFAULT_PAYLOAD   1472U
#define SPEED_IO_TIMEOUT_MS     200U    /* send/recv 超时，避免整体卡死 */
#define SPEED_PROBE_DELAY_MS    50U     /* udp_rx 探针后等服务器就绪 */
#define SPEED_MAX_RETRY         100000U /* 连续重试上限（TX 队列长期拥塞时退出） */
#define SPEED_THREAD_PRIORITY   16U     /* 低于 WLAN/以太网线程，避免阻塞驱动回收 */
#define SPEED_THREAD_STACK_SIZE 4096U
#define SPEED_THREAD_TIMESLICE  5U
#define SPEED_MODE_TEXT_MAX     16U
#define SPEED_IP_TEXT_MAX       16U
#define SPEED_U64_TEXT_SIZE     21U
#define SPEED_ARP_WAIT_MS       5000U
#define SPEED_ARP_POLL_MS       100U

/* 定义在预编译驱动库里（rwnx_driver_tx_queue.c），用于判断 TX 队列是否正常回收 */
extern int rwnx_free_txq_count(void);

#include "wifi_wlan_port.h"

struct speed_meter
{
    rt_tick_t       start;          /* 统计起点 */
    rt_tick_t       win_start;      /* 当前 1 秒窗口起点 */
    rt_uint64_t     total;          /* 累计字节 */
    rt_uint64_t     win_bytes;      /* 当前窗口字节 */
    rt_uint64_t     steady_bytes;   /* 稳态窗口累计字节 */
    rt_uint64_t     steady_ticks;   /* 稳态窗口累计 tick */
    unsigned        win_index;      /* 已关闭窗口序号 */
    unsigned        steady_windows; /* 计入稳态的窗口数 */
    unsigned        retries;        /* EAGAIN/ENOMEM 等重试次数 */
    unsigned        retry_wouldblock;
    unsigned        retry_timeout;
    unsigned        retry_memory;
    unsigned        retry_interrupt;
    rt_uint32_t     rate_min;       /* 稳态窗口最小/最大速率（Kbps） */
    rt_uint32_t     rate_max;
};

/* 每次测速独占一个上下文，避免 msh、多个测试实例共享发送缓冲。 */
struct speed_job
{
    char mode[SPEED_MODE_TEXT_MAX];
    char server_ip[SPEED_IP_TEXT_MAX];
    int port;
    int seconds;
    int payload;
    int udp;
    int direction;
    int quiet;
    int result;
    struct rt_semaphore done;
    char buffer[SPEED_BUF_MAX];
};

static struct rt_mutex s_speed_lock;

struct speed_neighbor
{
    struct rt_semaphore done;
    ip4_addr_t target;
    ip4_addr_t next_hop;
    rt_err_t result;
    const char *reason;
    int request;
};

/* Route and ARP state belong to the lwIP core. Never read them directly from
 * the speed-test thread, and never retain pointers into the ARP table. */
static void speed_neighbor_callback(void *parameter)
{
    struct speed_neighbor *check = parameter;
    struct netif *netif = ip4_route(&check->target);
    struct eth_addr *mac;
    const ip4_addr_t *entry_ip;

    check->result = -RT_ERROR;
    check->reason = "no_route";
    if (netif == RT_NULL || !netif_is_up(netif) || !netif_is_link_up(netif) ||
        ip4_addr_isany_val(*netif_ip4_addr(netif)))
        goto done;

    check->reason = "invalid_unicast_target";
    if (ip4_addr_isany_val(check->target) || ip4_addr_ismulticast(&check->target) ||
        ip4_addr_isbroadcast(&check->target, netif) ||
        ip4_addr_cmp(&check->target, netif_ip4_addr(netif)))
        goto done;

    check->next_hop = check->target;
    if (!ip4_addr_netcmp(&check->target, netif_ip4_addr(netif), netif_ip4_netmask(netif)) &&
        !ip4_addr_islinklocal(&check->target))
    {
        check->reason = "no_gateway";
        if (ip4_addr_isany_val(*netif_ip4_gw(netif)))
            goto done;
        check->next_hop = *netif_ip4_gw(netif);
    }

    if (etharp_find_addr(netif, &check->next_hop, &mac, &entry_ip) >= 0)
    {
        check->result = RT_EOK;
        check->reason = "arp_resolved";
    }
    else
    {
        check->result = -RT_ETIMEOUT;
        check->reason = "arp_unresolved";
        if (check->request)
            (void)etharp_query(netif, &check->next_hop, RT_NULL);
    }
done:
    /* Last access: the waiting worker owns the stack-allocated context. */
    rt_sem_release(&check->done);
}

static rt_err_t speed_check_neighbor(const struct in_addr *server, int wait)
{
    struct speed_neighbor check;
    rt_tick_t start = rt_tick_get();
    rt_tick_t elapsed;
    rt_tick_t last_request = 0;
    rt_err_t result;
    int first = 1;
    char next_hop[16];

    memset(&check, 0, sizeof(check));
    ip4_addr_set_u32(&check.target, server->s_addr);
    result = rt_sem_init(&check.done, "spdarp", 0, RT_IPC_FLAG_FIFO);
    if (result != RT_EOK)
    {
        printf("[speed] ERROR operation=init_neighbor_check result=%d\n", result);
        return result;
    }

    do
    {
        elapsed = rt_tick_get() - start;
        check.request = wait && (first || elapsed - last_request >= RT_TICK_PER_SECOND);
        if (check.request)
            last_request = elapsed;
        first = 0;
        if (tcpip_callback(speed_neighbor_callback, &check) != ERR_OK)
        {
            check.reason = "callback_failed";
            result = -RT_ERROR;
            break;
        }
        /* A queued callback must finish before this context can be released. */
        while (rt_sem_take(&check.done, RT_WAITING_FOREVER) != RT_EOK)
        {
        }
        result = check.result;
        if (result != -RT_ETIMEOUT || !wait ||
            rt_tick_get() - start >= rt_tick_from_millisecond(SPEED_ARP_WAIT_MS))
            break;
        rt_thread_mdelay(SPEED_ARP_POLL_MS);
    } while (1);

    rt_sem_detach(&check.done);
    if (wait || result != RT_EOK)
    {
        ip4addr_ntoa_r(&check.next_hop, next_hop, sizeof(next_hop));
        printf("[speed] NEIGHBOR status=%s reason=%s next_hop=%s\n",
               result == RT_EOK ? "ready" : "failed", check.reason, next_hop);
    }
    return result;
}

static void speed_print_config(void)
{
    printf("[speed] DIAG status=begin\n");
    printf("[speed] SYSTEM tick_hz=%u\n", (unsigned)RT_TICK_PER_SECOND);
    printf("[speed] TASK priority=%u priority_order=lower_number_first stack_bytes=%u timeslice_ticks=%u\n",
           (unsigned)SPEED_THREAD_PRIORITY, (unsigned)SPEED_THREAD_STACK_SIZE,
           (unsigned)SPEED_THREAD_TIMESLICE);
    printf("[speed] LWIP core_lock=%u input_lock=%u single_tx_pbuf=%u\n",
           (unsigned)LWIP_TCPIP_CORE_LOCKING,
           (unsigned)LWIP_TCPIP_CORE_LOCKING_INPUT,
           (unsigned)LWIP_NETIF_TX_SINGLE_PBUF);
    printf("[speed] TCP mss_bytes=%u wnd_bytes=%u sndbuf_bytes=%u wnd_scale=%u rcv_scale=%u "
           "recv_default_bytes=%lu tcp_seg=%u tcp_mbox=%u checksum_copy=%u\n",
           (unsigned)TCP_MSS,
           (unsigned)TCP_WND,
           (unsigned)TCP_SND_BUF,
           (unsigned)LWIP_WND_SCALE,
           (unsigned)TCP_RCV_SCALE,
           (unsigned long)RECV_BUFSIZE_DEFAULT,
           (unsigned)MEMP_NUM_TCP_SEG,
           (unsigned)DEFAULT_TCP_RECVMBOX_SIZE,
           (unsigned)LWIP_CHECKSUM_ON_COPY);
    printf("[speed] TX_QUEUE free_txq=%d\n", rwnx_free_txq_count());
    printf("[speed] DIAG status=end\n");
}

static int speed_test_app_init(void)
{
    return rt_mutex_init(&s_speed_lock, "spdmtx", RT_IPC_FLAG_FIFO);
}
INIT_APP_EXPORT(speed_test_app_init);

/* Avoid unsupported %llu conversions and 32-bit counter truncation. */
static const char *speed_u64_text(rt_uint64_t value, char buffer[SPEED_U64_TEXT_SIZE])
{
    char *cursor = buffer + SPEED_U64_TEXT_SIZE - 1U;

    *cursor = '\0';
    do
    {
        *--cursor = (char)('0' + (unsigned)(value % 10U));
        value /= 10U;
    } while (value != 0U);

    return cursor;
}

/* bit/s 的千分之一，避免浮点 */
static rt_uint32_t speed_kbps(rt_uint64_t bytes, rt_uint64_t ms)
{
    if (ms == 0U)
    {
        return 0U;
    }

    return (rt_uint32_t)((bytes * 8ULL * 1000ULL) / (ms * 1000ULL));
}

static rt_uint64_t speed_ticks_to_ms(rt_tick_t ticks)
{
    return ((rt_uint64_t)ticks * 1000ULL) / RT_TICK_PER_SECOND;
}

/* Always account for the window, even with sample output disabled.
 * The final window is excluded from steady statistics. */
static void speed_close_window(struct speed_meter *m, rt_tick_t now, int final, int print_sample)
{
    rt_tick_t delta = now - m->win_start;
    rt_uint64_t ms;
    rt_uint64_t elapsed_ms;
    rt_uint32_t kbps;
    char total_text[SPEED_U64_TEXT_SIZE];
    char window_text[SPEED_U64_TEXT_SIZE];

    if (delta <= 0)
    {
        return;
    }

    ms = speed_ticks_to_ms(delta);
    elapsed_ms = speed_ticks_to_ms(now - m->start);
    kbps = speed_kbps(m->win_bytes, ms);
    m->win_index++;

    if (print_sample)
    {
        printf("[speed] SAMPLE elapsed_s=%u.%03u interval_ms=%u total_bytes=%s "
               "interval_bytes=%s rate_mbps=%u.%03u final=%u\n",
               (unsigned)(elapsed_ms / 1000U), (unsigned)(elapsed_ms % 1000U),
               (unsigned)ms, speed_u64_text(m->total, total_text),
               speed_u64_text(m->win_bytes, window_text),
               (unsigned)(kbps / 1000U), (unsigned)(kbps % 1000U),
               final ? 1U : 0U);
    }

    if ((final == 0) && (m->win_index > 1U))
    {
        m->steady_bytes += m->win_bytes;
        m->steady_ticks += (rt_uint64_t)delta;
        m->steady_windows++;
        if (m->steady_windows == 1U)
        {
            m->rate_min = kbps;
            m->rate_max = kbps;
        }
        else
        {
            if (kbps < m->rate_min)
            {
                m->rate_min = kbps;
            }
            if (kbps > m->rate_max)
            {
                m->rate_max = kbps;
            }
        }
    }

    m->win_bytes = 0U;
    m->win_start = now;
}

static void speed_summary(const struct speed_meter *m, const char *mode, rt_tick_t end,
                          const char *status, const char *reason)
{
    rt_uint64_t ms = speed_ticks_to_ms(end - m->start);
    rt_uint32_t avg = speed_kbps(m->total, ms);

    char total_text[SPEED_U64_TEXT_SIZE];

    printf("[speed] SUMMARY mode=%s status=%s reason=%s total_bytes=%s duration_s=%u.%03u "
           "avg_mbps=%u.%03u socket_retries=%u\n",
           mode, status, reason, speed_u64_text(m->total, total_text),
           (unsigned)(ms / 1000U), (unsigned)(ms % 1000U),
           (unsigned)(avg / 1000U), (unsigned)(avg % 1000U), m->retries);

    if (m->steady_windows != 0U)
    {
        rt_uint64_t s_ms = (m->steady_ticks * 1000ULL) / RT_TICK_PER_SECOND;
        rt_uint32_t steady = speed_kbps(m->steady_bytes, s_ms);

        printf("[speed] STEADY available=1 rate_mbps=%u.%03u windows=%u min_mbps=%u.%03u max_mbps=%u.%03u\n",
               (unsigned)(steady / 1000U), (unsigned)(steady % 1000U),
               m->steady_windows,
               (unsigned)(m->rate_min / 1000U), (unsigned)(m->rate_min % 1000U),
               (unsigned)(m->rate_max / 1000U), (unsigned)(m->rate_max % 1000U));
    }

    else
    {
        printf("[speed] STEADY available=0 windows=0\n");
    }

    printf("[speed] RETRIES scope=socket total=%u memory=%u timeout=%u wouldblock=%u interrupt=%u\n",
           m->retries, m->retry_memory, m->retry_timeout,
           m->retry_wouldblock, m->retry_interrupt);
}

static int speed_mode_parse(const char *name, int *udp, int *direction)
{
    if ((strcmp(name, "tx") == 0) || (strcmp(name, "tcp_tx") == 0))
    {
        *udp = 0;
        *direction = 1;
        return RT_EOK;
    }
    if ((strcmp(name, "rx") == 0) || (strcmp(name, "tcp_rx") == 0))
    {
        *udp = 0;
        *direction = 0;
        return RT_EOK;
    }
    if (strcmp(name, "udp_tx") == 0)
    {
        *udp = 1;
        *direction = 1;
        return RT_EOK;
    }
    if (strcmp(name, "udp_rx") == 0)
    {
        *udp = 1;
        *direction = 0;
        return RT_EOK;
    }

    return -RT_EINVAL;
}

static void speed_usage(void)
{
    printf("Wi-Fi throughput test\n"
           "  wifi_speed_test <mode> <server_ipv4> <port> <seconds> [payload_bytes] [--quiet]\n"
           "  wifi_speed_test diag\n"
           "  Modes: tx/tcp_tx, rx/tcp_rx, udp_tx, udp_rx\n"
           "  Direction: TX = board to server; RX = server to board\n"
           "  Port: 1..65535; duration: 1..3600 seconds\n");
    printf("  Payload: 16..%u bytes per socket call; default %u\n",
           (unsigned)SPEED_BUF_MAX, (unsigned)SPEED_DEFAULT_PAYLOAD);
    printf("  Rate: decimal Mbps, application bytes only\n"
           "  Steady: excludes first and final windows; unavailable for short tests\n"
           "  --quiet: suppress SAMPLE output; keep summary and steady statistics\n"
           "  UDP: wait up to 5 seconds for ARP before timing; TX counts socket acceptance, not delivery\n"
           "  Socket retries are not TCP retransmissions; wouldblock may mean receive timeout\n"
           "  Driver counters include other traffic and may wrap at 32 bits\n"
           "  diag: system, worker, lwIP settings and current TX queue\n"
           "  Example: wifi_speed_test udp_tx 192.168.1.10 5002 10\n");
}

static void speed_socket_option(int fd, int option, const char *name,
                                const void *value, socklen_t length)
{
    if (lwip_setsockopt(fd, SOL_SOCKET, option, value, length) != 0)
    {
        int err = errno;

        if (err == ENOPROTOOPT)
            printf("[speed] SOCKET option=%s status=unsupported errno=%d\n", name, err);
        else
            printf("[speed] WARN operation=setsockopt option=%s errno=%d\n", name, err);
    }
}

static int speed_test_run(struct speed_job *job)
{
    int fd = -1;
    const char *mode = job->mode;
    const char *server_ip = job->server_ip;
    int port = job->port;
    int seconds = job->seconds;
    int payload = job->payload;
    int udp = job->udp;
    int direction = job->direction;
    int nodelay = 1;
    /*
     * Keep the send and receive windows independent.  The old code used the
     * 32 KiB send buffer for SO_RCVBUF as well.  At a 1472-byte UDP payload
     * that leaves room for only about 22 datagrams, so a short scheduling
     * delay made the lwIP UDP receive queue overflow even when the driver and
     * tcpip input path were healthy.
     */
    int sndbuf = 64 * 1024;
    int rcvbuf = 256 * 1024;
    struct timeval timeout;
    struct sockaddr_in addr;
    struct speed_meter meter;
    rt_tick_t deadline;
    rt_tick_t end;
    unsigned consecutive_retries = 0;
    unsigned calls = 0;
    struct wifi_wlan_stats stats;
    int result = RT_EOK;
    const char *status = "completed";
    const char *reason = "duration_elapsed";

    printf("[speed] START mode=%s direction=%s server=%s:%d payload_bytes=%d duration_s=%d sample_output=%u\n",
           mode, direction ? "uplink" : "downlink", server_ip, port, payload, seconds,
           job->quiet ? 0U : 1U);

    fd = lwip_socket(AF_INET, udp ? SOCK_DGRAM : SOCK_STREAM, 0);
    if (fd < 0)
    {
        printf("[speed] ERROR operation=socket errno=%d\n", errno);
        reason = "socket_error";
        goto setup_failed;
    }

    timeout.tv_sec = 0;
    timeout.tv_usec = (int)SPEED_IO_TIMEOUT_MS * 1000;
    speed_socket_option(fd, SO_SNDTIMEO, "SO_SNDTIMEO", &timeout, sizeof(timeout));
    speed_socket_option(fd, SO_RCVTIMEO, "SO_RCVTIMEO", &timeout, sizeof(timeout));
    speed_socket_option(fd, SO_SNDBUF, "SO_SNDBUF", &sndbuf, sizeof(sndbuf));
    speed_socket_option(fd, SO_RCVBUF, "SO_RCVBUF", &rcvbuf, sizeof(rcvbuf));
    {
        int effective_rcvbuf = 0;
        socklen_t effective_len = sizeof(effective_rcvbuf);
        if (lwip_getsockopt(fd, SOL_SOCKET, SO_RCVBUF,
                            &effective_rcvbuf, &effective_len) == 0)
        {
            printf("[speed] SOCKET requested_rcvbuf_bytes=%d effective_rcvbuf_bytes=%d\n",
                   rcvbuf, effective_rcvbuf);
        }
        else
        {
            printf("[speed] WARN operation=getsockopt option=SO_RCVBUF errno=%d\n",
                   errno);
        }
    }
    if (!udp)
    {
        /* 关闭 Nagle：避免小写入被合并，上行速率才有意义 */
        if (lwip_setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay)) != 0)
            printf("[speed] WARN operation=setsockopt option=TCP_NODELAY errno=%d\n", errno);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u16_t)port);
    if (inet_aton(server_ip, &addr.sin_addr) == 0)
    {
        printf("[speed] ERROR operation=validate reason=invalid_server_ipv4\n");
        result = -RT_EINVAL;
        reason = "invalid_server_ipv4";
        goto setup_failed;
    }

    if (lwip_connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        printf("[speed] ERROR operation=connect errno=%d\n", errno);
        reason = "connect_error";
        goto setup_failed;
    }

    if (udp)
    {
        result = speed_check_neighbor(&addr.sin_addr, 1);
        if (result != RT_EOK)
        {
            reason = "neighbor_unavailable";
            goto setup_failed;
        }
    }

    memset(job->buffer, 0x5a, sizeof(job->buffer));

    if (udp && (direction == 0))
    {
        /* 1 字节探针：udp-down 服务器据此学到板卡地址 */
        int n = lwip_send(fd, job->buffer, 1, 0);
        if (n != 1)
        {
            printf("[speed] ERROR operation=udp_probe errno=%d\n", n < 0 ? errno : 0);
            reason = "probe_error";
            goto setup_failed;
        }
        rt_thread_mdelay(SPEED_PROBE_DELAY_MS);
    }
    /* Count both data and reverse traffic (TCP ACKs) during the test. */
    wifi_wlan_stats_reset();

    memset(&meter, 0, sizeof(meter));
    meter.start = rt_tick_get();
    meter.win_start = meter.start;
    deadline = meter.start + (rt_tick_t)seconds * RT_TICK_PER_SECOND;

    while ((rt_int32_t)(rt_tick_get() - deadline) < 0)
    {
        rt_tick_t now = rt_tick_get();
        int n;

        if ((now - meter.win_start) >= RT_TICK_PER_SECOND)
        {
            if (udp && direction)
            {
                result = speed_check_neighbor(&addr.sin_addr, 0);
                if (result != RT_EOK)
                {
                    status = "failed";
                    reason = "neighbor_unavailable";
                    break;
                }
                now = rt_tick_get();
            }
            speed_close_window(&meter, now, 0, !job->quiet);
        }

        n = direction ? lwip_send(fd, job->buffer, (size_t)payload, 0)
                      : lwip_recv(fd, job->buffer, (size_t)payload, 0);
        if (n < 0)
        {
            int err = errno;

            if ((err == EWOULDBLOCK) || (err == EAGAIN) || (err == ENOBUFS) ||
                (err == ENOMEM) || (err == EINTR) || (err == ETIMEDOUT))
            {
                /* 原 FSP 工程使用 vTaskDelay(pdMS_TO_TICKS(1))。
                 * 原工程的 FreeRTOS tick 为 512 Hz，该表达式为 0 tick，
                 * 实际效果是让出当前任务而不是固定等待 1 ms。固定 1 ms
                 * 会把 TX 队列背压直接变成吞吐损失。 */
                meter.retries++;
                if (err == ENOBUFS || err == ENOMEM)
                    meter.retry_memory++;
                else if (err == EINTR)
                    meter.retry_interrupt++;
                else if (err == ETIMEDOUT)
                    meter.retry_timeout++;
                else
                    meter.retry_wouldblock++;
                if (++consecutive_retries > SPEED_MAX_RETRY)
                {
                    printf("[speed] ERROR operation=transfer reason=retry_limit errno=%d\n", err);
                    status = "failed";
                    reason = "retry_limit";
                    result = -RT_ETIMEOUT;
                    break;
                }
                rt_thread_yield();
                continue;
            }

            printf("[speed] ERROR operation=%s errno=%d\n",
                   direction ? "send" : "recv", err);
            status = "failed";
            reason = "socket_error";
            result = -RT_ERROR;
            break;
        }
        consecutive_retries = 0;
        if (n == 0 && !udp)
        {
            status = "incomplete";
            reason = "peer_closed";
            result = -RT_ERROR;
            break;
        }

        meter.total += (rt_uint64_t)n;
        meter.win_bytes += (rt_uint64_t)n;
        calls++;
    }

    end = rt_tick_get();
    /* Exclude logging and connection shutdown from the driver snapshot. */
    wifi_wlan_stats_get(&stats);
    if (udp && direction && result == RT_EOK)
    {
        result = speed_check_neighbor(&addr.sin_addr, 0);
        if (result != RT_EOK)
        {
            status = "failed";
            reason = "neighbor_unavailable";
        }
    }
    speed_close_window(&meter, end, 1, !job->quiet);
    speed_summary(&meter, mode, end, status, reason);
    printf("[speed] APP successful_calls=%u\n", calls);

    if (!udp)
    {
        (void)lwip_shutdown(fd, SHUT_RDWR);
    }
    (void)lwip_close(fd);

    if (!udp || direction == 0)
    {
        printf("[speed] RX_DRIVER scope=global_test packets=%u bytes=%u dropped=%u state_drop=%u input_reject=%u\n",
               (unsigned)stats.rx_packets, (unsigned)stats.rx_bytes,
               (unsigned)stats.rx_dropped,
               (unsigned)stats.rx_state_dropped,
               (unsigned)stats.rx_input_rejected);
    }

    if (!udp || direction != 0)
    {
        printf("[speed] TX_DRIVER scope=global_test packets=%u bytes=%u queue_reject=%u "
               "event_reject=%u copied=%u min_free_txq=%u\n",
               (unsigned)stats.tx_packets, (unsigned)stats.tx_bytes,
               (unsigned)stats.queue_rejected, (unsigned)stats.event_rejected,
               (unsigned)stats.tx_copied, (unsigned)stats.min_free_txq);
    }

    /* 驱动 TX 队列当前可用项；持续偏低说明 txq 未及时回收，速率会被压制。 */
    printf("[speed] END mode=%s status=%s reason=%s free_txq=%d\n",
           mode, status, reason, rwnx_free_txq_count());

    return result;

setup_failed:
    if (fd >= 0)
        (void)lwip_close(fd);
    printf("[speed] END mode=%s status=failed reason=%s\n", mode, reason);
    return result == RT_EOK ? -RT_ERROR : result;
}

static void speed_test_thread_entry(void *parameter)
{
    struct speed_job *job = parameter;

    job->result = speed_test_run(job);
    /* This must be the last access to job: the caller owns its lifetime.
     * A dynamic RT-Thread thread exits and is reclaimed after returning. */
    rt_sem_release(&job->done);
}

static int wifi_speed_test(int argc, char **argv)
{
    struct speed_job *job;
    struct in_addr address;
    rt_thread_t thread;
    int udp, direction, port, seconds, payload;
    int quiet = 0;
    int result;

    if (argc == 2 && strcmp(argv[1], "diag") == 0)
    {
        speed_print_config();
        return RT_EOK;
    }

    if (argc > 1 && strcmp(argv[argc - 1], "--quiet") == 0)
    {
        quiet = 1;
        argc--;
    }

    if ((argc != 5 && argc != 6) ||
        speed_mode_parse(argv[1], &udp, &direction) != RT_EOK)
    {
        speed_usage();
        return -RT_EINVAL;
    }
    port = atoi(argv[3]);
    seconds = atoi(argv[4]);
    payload = argc == 6 ? atoi(argv[5]) : (int)SPEED_DEFAULT_PAYLOAD;
    if (port <= 0 || port > 65535 || seconds <= 0 || seconds > 3600 ||
        payload < 16 || payload > (int)SPEED_BUF_MAX)
    {
        printf("[speed] ERROR operation=validate reason=invalid_arguments\n");
        return -RT_EINVAL;
    }
    if (strlen(argv[2]) >= SPEED_IP_TEXT_MAX || !inet_aton(argv[2], &address))
    {
        printf("[speed] ERROR operation=validate reason=invalid_server_ipv4\n");
        return -RT_EINVAL;
    }

    /* Driver statistics are global, so only one test may reset/read them. */
    result = rt_mutex_take(&s_speed_lock, RT_WAITING_NO);
    if (result != RT_EOK)
    {
        printf("[speed] ERROR operation=start reason=test_busy\n");
        return result;
    }
    job = rt_calloc(1, sizeof(*job));
    if (!job)
    {
        result = -RT_ENOMEM;
        printf("[speed] ERROR operation=allocate_job result=%d\n", result);
        goto unlock;
    }
    strcpy(job->mode, udp ? (direction ? "udp_tx" : "udp_rx")
                          : (direction ? "tcp_tx" : "tcp_rx"));
    strcpy(job->server_ip, argv[2]);
    job->port = port;
    job->seconds = seconds;
    job->payload = payload;
    job->udp = udp;
    job->direction = direction;
    job->quiet = quiet;

    result = rt_sem_init(&job->done, "spdone", 0, RT_IPC_FLAG_FIFO);
    if (result != RT_EOK)
    {
        printf("[speed] ERROR operation=init_semaphore result=%d\n", result);
        goto free_job;
    }

    thread = rt_thread_create("wifispd", speed_test_thread_entry, job,
                              SPEED_THREAD_STACK_SIZE, SPEED_THREAD_PRIORITY,
                              SPEED_THREAD_TIMESLICE);
    if (!thread)
    {
        result = -RT_ENOMEM;
        printf("[speed] ERROR operation=create_thread result=%d\n", result);
        goto detach_sem;
    }
    result = rt_thread_startup(thread);
    if (result != RT_EOK)
    {
        printf("[speed] ERROR operation=start_thread result=%d\n", result);
        rt_thread_delete(thread);
        goto detach_sem;
    }

    /* Keep the command synchronous; all socket I/O runs in wifispd.
     * Do not free the context until the worker has closed its socket. */
    while (rt_sem_take(&job->done, RT_WAITING_FOREVER) != RT_EOK)
    {
    }
    result = job->result;
    /* Do not delete thread here: it returns and retires itself. */
detach_sem:
    rt_sem_detach(&job->done);
free_job:
    rt_free(job);
unlock:
    rt_mutex_release(&s_speed_lock);
    return result;
}

MSH_CMD_EXPORT(wifi_speed_test,
               test Wi-Fi throughput: <tx|rx|udp_tx|udp_rx> ip port seconds [payload] [--quiet] or diag);
