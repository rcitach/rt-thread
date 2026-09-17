#include <rtthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lwip/sockets.h>
#include <lwip/inet.h>
#include <lwip/errno.h>

#define SPEED_BUF_MAX           1472U   /* 单包最大载荷（避免 IP 分片） */
#define SPEED_DEFAULT_PAYLOAD   1472U
#define SPEED_IO_TIMEOUT_MS     200U    /* send/recv 超时，避免整体卡死 */
#define SPEED_PROBE_DELAY_MS    50U     /* udp_rx 探针后等服务器就绪 */
#define SPEED_MAX_RETRY         100000U /* 连续重试上限（TX 队列长期拥塞时退出） */

/* 定义在预编译驱动库里（rwnx_driver_tx_queue.c），用于判断 TX 队列是否正常回收 */
extern int rwnx_free_txq_count(void);

/* 缓冲必须静态：msh 线程栈放不下 1.5 KB 以上的数组 */
static char s_speed_buf[SPEED_BUF_MAX];

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
    rt_uint32_t     rate_min;       /* 稳态窗口最小/最大速率（Kbps） */
    rt_uint32_t     rate_max;
};

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

/* 关闭当前窗口并打印；final!=0 时该窗口不计入 steady（末尾不完整窗口） */
static void speed_close_window(struct speed_meter *m, rt_tick_t now, int final)
{
    rt_tick_t delta = now - m->win_start;
    rt_uint64_t ms;
    rt_uint64_t elapsed_ms;
    rt_uint32_t kbps;

    if (delta <= 0)
    {
        return;
    }

    ms = speed_ticks_to_ms(delta);
    elapsed_ms = speed_ticks_to_ms(now - m->start);
    kbps = speed_kbps(m->win_bytes, ms);
    m->win_index++;

    printf("[speed] t=%u.%03us total=%u win=%u rate=%u.%03u Mbps%s\n",
           (unsigned)(elapsed_ms / 1000U), (unsigned)(elapsed_ms % 1000U),
           (unsigned)m->total, (unsigned)m->win_bytes,
           (unsigned)(kbps / 1000U), (unsigned)(kbps % 1000U),
           final ? " (partial)" : "");

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

static void speed_summary(const struct speed_meter *m, const char *mode)
{
    rt_uint64_t ms = speed_ticks_to_ms(rt_tick_get() - m->start);
    rt_uint32_t avg = speed_kbps(m->total, ms);

    printf("[speed] SUMMARY mode=%s total=%u bytes duration=%u.%03us "
           "avg=%u.%03u Mbps",
           mode, (unsigned)m->total,
           (unsigned)(ms / 1000U), (unsigned)(ms % 1000U),
           (unsigned)(avg / 1000U), (unsigned)(avg % 1000U));

    if (m->steady_windows != 0U)
    {
        rt_uint64_t s_ms = (m->steady_ticks * 1000ULL) / RT_TICK_PER_SECOND;
        rt_uint32_t steady = speed_kbps(m->steady_bytes, s_ms);

        printf(" steady=%u.%03u Mbps windows=%u min=%u.%03u max=%u.%03u",
               (unsigned)(steady / 1000U), (unsigned)(steady % 1000U),
               m->steady_windows,
               (unsigned)(m->rate_min / 1000U), (unsigned)(m->rate_min % 1000U),
               (unsigned)(m->rate_max / 1000U), (unsigned)(m->rate_max % 1000U));
    }

    printf(" retries=%u\n", m->retries);
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
    printf("usage: wifi_speed_test <mode> <server_ip> <port> <seconds> [payload]\n");
    printf("  mode    : tx | rx | udp_tx | udp_rx  (tcp_tx/tcp_rx 亦可)\n");
    printf("            tx/udp_tx = 板卡发送(上行), rx/udp_rx = 板卡接收(下行)\n");
    printf("  payload : 单次收发字节数, 默认 %u, 上限 %u (UDP 不要超过它)\n",
           (unsigned)SPEED_DEFAULT_PAYLOAD, (unsigned)SPEED_BUF_MAX);
    printf("  example : wifi_speed_test udp_tx 10.191.9.144 5002 10\n");
    printf("            wifi_speed_test udp_rx 10.191.9.144 5002 10 1472\n");
    printf("            wifi_speed_test tx     10.191.9.144 5002 30\n");
}

static int wifi_speed_test(int argc, char **argv)
{
    int fd;
    int port;
    int seconds;
    int payload;
    int udp = 0;
    int direction = 0;
    int nodelay = 1;
    int sndbuf = 32 * 1024;
    struct timeval timeout;
    struct sockaddr_in addr;
    struct speed_meter meter;
    rt_tick_t deadline;
    unsigned i;

    if ((argc != 5) && (argc != 6))
    {
        speed_usage();
        return -RT_EINVAL;
    }

    if (speed_mode_parse(argv[1], &udp, &direction) != RT_EOK)
    {
        speed_usage();
        return -RT_EINVAL;
    }

    port = atoi(argv[3]);
    seconds = atoi(argv[4]);
    payload = (argc == 6) ? atoi(argv[5]) : (int)SPEED_DEFAULT_PAYLOAD;

    if ((port <= 0) || (port > 65535) || (seconds <= 0) || (seconds > 3600))
    {
        printf("[speed] invalid port or seconds\n");
        return -RT_EINVAL;
    }
    if ((payload < 16) || ((unsigned)payload > SPEED_BUF_MAX))
    {
        printf("[speed] payload must be 16..%u\n", (unsigned)SPEED_BUF_MAX);
        return -RT_EINVAL;
    }

    printf("[speed] mode=%s server=%s:%d payload=%d duration=%ds\n",
           argv[1], argv[2], port, payload, seconds);

    fd = lwip_socket(AF_INET, udp ? SOCK_DGRAM : SOCK_STREAM, 0);
    if (fd < 0)
    {
        printf("[speed] socket() failed, errno=%d\n", errno);
        return -RT_ERROR;
    }

    timeout.tv_sec = 0;
    timeout.tv_usec = (int)SPEED_IO_TIMEOUT_MS * 1000;
    (void)lwip_setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    (void)lwip_setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    /* 尽力而为：lwIP 未启用 SO_SNDBUF/SO_RCVBUF 时这两个调用会失败，忽略即可 */
    (void)lwip_setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));
    (void)lwip_setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &sndbuf, sizeof(sndbuf));
    if (!udp)
    {
        /* 关闭 Nagle：避免小写入被合并，上行速率才有意义 */
        (void)lwip_setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u16_t)port);
    if (inet_aton(argv[2], &addr.sin_addr) == 0)
    {
        printf("[speed] bad server ip: %s\n", argv[2]);
        (void)lwip_close(fd);
        return -RT_EINVAL;
    }

    if (lwip_connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        printf("[speed] connect %s:%d failed, errno=%d\n", argv[2], port, errno);
        (void)lwip_close(fd);
        return -RT_ERROR;
    }

    for (i = 0U; i < sizeof(s_speed_buf); i++)
    {
        s_speed_buf[i] = (char)0x5a;
    }

    if (udp && (direction == 0))
    {
        /* 1 字节探针：udp-down 服务器据此学到板卡地址 */
        (void)lwip_send(fd, s_speed_buf, 1, 0);
        rt_thread_mdelay(SPEED_PROBE_DELAY_MS);
    }

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
            speed_close_window(&meter, now, 0);
        }

        n = direction ? lwip_send(fd, s_speed_buf, (size_t)payload, 0)
                      : lwip_recv(fd, s_speed_buf, (size_t)payload, 0);
        if (n < 0)
        {
            int err = errno;

            if ((err == EWOULDBLOCK) || (err == EAGAIN) || (err == ENOBUFS) ||
                (err == ENOMEM) || (err == EINTR) || (err == ETIMEDOUT))
            {
                /* 发送/接收暂时不可用（TX 队列满、窗口满、超时）：短暂让出再试 */
                meter.retries++;
                if (meter.retries > SPEED_MAX_RETRY)
                {
                    printf("[speed] abort: too many retries (socket/TX queue busy)\n");
                    break;
                }
                rt_thread_mdelay(1);
                continue;
            }

            printf("[speed] %s failed, errno=%d\n",
                   direction ? "send" : "recv", err);
            break;
        }
        if (n == 0)
        {
            printf("[speed] peer closed the connection\n");
            break;
        }

        meter.total += (rt_uint64_t)n;
        meter.win_bytes += (rt_uint64_t)n;
    }

    speed_close_window(&meter, rt_tick_get(), 1);
    speed_summary(&meter, argv[1]);

    if (!udp)
    {
        (void)lwip_shutdown(fd, SHUT_RDWR);
    }
    (void)lwip_close(fd);

    /* 驱动 TX 队列可用项（满值 32）：明显偏低说明 txq 未及时回收，速率会被压制 */
    printf("[speed] free_txq=%d\n", rwnx_free_txq_count());

    return RT_EOK;
}
MSH_CMD_EXPORT(wifi_speed_test,
               test Wi-Fi throughput: <tx|rx|udp_tx|udp_rx> ip port seconds [payload]);
