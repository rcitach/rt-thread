/* Native eloop timeout ABI. Callbacks run with driver/EAPOL events in wifievt. */
#include "wifi_vendor.h"

#define WIFI_TIMEOUT_MAX (RT_TICK_MAX / 2 - 1)

struct wifi_wpa_timeout
{
    rt_list_t node;
    rt_tick_t deadline;
    eloop_timeout_handler handler;
    void *ctx;
    void *user;
};

static rt_list_t timeouts;
static struct rt_mutex timer_lock;
static struct rt_timer wake_timer;
static rt_mq_t event_queue;

static void wake_worker(void *parameter)
{
    ULONG wake[2] = {0, 0};
    (void)parameter;
    /* If full, the worker is already awake and checks timers after each item. */
    (void)rt_mq_send(event_queue, wake, sizeof(wake));
}

static rt_tick_t timeout_ticks(unsigned int secs, unsigned int usecs)
{
    uint64_t ticks = ((uint64_t)secs * 1000000 + usecs) * RT_TICK_PER_SECOND;
    ticks = (ticks + 999999) / 1000000;
    if (ticks > WIFI_TIMEOUT_MAX)
        ticks = WIFI_TIMEOUT_MAX;
    return ticks ? (rt_tick_t)ticks : 1;
}

/* Caller holds timer_lock. Deadlines stay within the signed tick half-range. */
static void rearm(void)
{
    rt_list_t *node;
    rt_tick_t wait = WIFI_TIMEOUT_MAX;
    rt_tick_t now = rt_tick_get();

    rt_timer_stop(&wake_timer);
    if (rt_list_isempty(&timeouts))
        return;
    rt_list_for_each(node, &timeouts)
    {
        struct wifi_wpa_timeout *t = rt_list_entry(node, struct wifi_wpa_timeout, node);
        rt_int32_t left = (rt_int32_t)(t->deadline - now);
        if (left <= 0)
        {
            wait = 1;
            break;
        }
        if ((rt_tick_t)left < wait)
            wait = (rt_tick_t)left;
    }
    rt_timer_control(&wake_timer, RT_TIMER_CTRL_SET_TIME, &wait);
    rt_timer_start(&wake_timer);
}

int wifi_wpa_timers_init(rt_mq_t queue)
{
    if (!queue)
        return -RT_EINVAL;
    if (event_queue)
        return event_queue == queue ? RT_EOK : -RT_EBUSY;
    rt_list_init(&timeouts);
    if (rt_mutex_init(&timer_lock, "wpatmr", RT_IPC_FLAG_PRIO) != RT_EOK)
        return -RT_ERROR;
    rt_timer_init(&wake_timer, "wpatmr", wake_worker, RT_NULL, 1,
                  RT_TIMER_FLAG_ONE_SHOT);
    event_queue = queue;
    return RT_EOK;
}

/* Keep cancellation and dispatch ordered by expiry, including across wrap. */
static void insert_timeout(struct wifi_wpa_timeout *timeout)
{
    rt_list_t *node;
    rt_tick_t now = rt_tick_get();
    rt_int32_t left = (rt_int32_t)(timeout->deadline - now);
    rt_list_for_each(node, &timeouts)
    {
        struct wifi_wpa_timeout *t = rt_list_entry(node, struct wifi_wpa_timeout, node);
        if (left < (rt_int32_t)(t->deadline - now))
            break;
    }
    rt_list_insert_before(node, &timeout->node);
}

int eloop_register_timeout(unsigned int secs, unsigned int usecs,
                           eloop_timeout_handler handler, void *ctx, void *user)
{
    struct wifi_wpa_timeout *t;
    if (!event_queue || !handler)
        return -1;
    t = rt_malloc(sizeof(*t));
    if (!t)
        return -1;
    t->deadline = rt_tick_get() + timeout_ticks(secs, usecs);
    t->handler = handler;
    t->ctx = ctx;
    t->user = user;
    rt_mutex_take(&timer_lock, RT_WAITING_FOREVER);
    insert_timeout(t);
    rearm();
    rt_mutex_release(&timer_lock);
    return 0;
}

static int matches(struct wifi_wpa_timeout *t, eloop_timeout_handler handler,
                   void *ctx, void *user)
{
    return t->handler == handler && (ctx == ELOOP_ALL_CTX || ctx == t->ctx) &&
           (user == ELOOP_ALL_CTX || user == t->user);
}

int eloop_cancel_timeout(eloop_timeout_handler handler, void *ctx, void *user)
{
    rt_list_t *node, *next;
    int count = 0;
    if (!event_queue)
        return 0;
    rt_mutex_take(&timer_lock, RT_WAITING_FOREVER);
    rt_list_for_each_safe(node, next, &timeouts)
    {
        struct wifi_wpa_timeout *t = rt_list_entry(node, struct wifi_wpa_timeout, node);
        if (matches(t, handler, ctx, user))
        {
            rt_list_remove(node);
            rt_free(t);
            count++;
        }
    }
    rearm();
    rt_mutex_release(&timer_lock);
    return count;
}

int eloop_cancel_timeout_one(eloop_timeout_handler handler, void *ctx,
                             void *user, struct os_reltime *remaining)
{
    rt_list_t *node;
    int found = 0;
    remaining->sec = remaining->usec = 0;
    if (!event_queue)
        return 0;
    rt_mutex_take(&timer_lock, RT_WAITING_FOREVER);
    rt_list_for_each(node, &timeouts)
    {
        struct wifi_wpa_timeout *t = rt_list_entry(node, struct wifi_wpa_timeout, node);
        if (matches(t, handler, ctx, user))
        {
            rt_int32_t left = (rt_int32_t)(t->deadline - rt_tick_get());
            if (left > 0)
            {
                remaining->sec = left / RT_TICK_PER_SECOND;
                remaining->usec = (uint64_t)(left % RT_TICK_PER_SECOND) * 1000000 / RT_TICK_PER_SECOND;
            }
            rt_list_remove(node);
            rt_free(t);
            found = 1;
            break;
        }
    }
    rearm();
    rt_mutex_release(&timer_lock);
    return found;
}

int eloop_is_timeout_registered(eloop_timeout_handler handler, void *ctx, void *user)
{
    rt_list_t *node;
    int found = 0;
    if (!event_queue)
        return 0;
    rt_mutex_take(&timer_lock, RT_WAITING_FOREVER);
    rt_list_for_each(node, &timeouts)
    {
        struct wifi_wpa_timeout *t = rt_list_entry(node, struct wifi_wpa_timeout, node);
        if (matches(t, handler, ctx, user))
        {
            found = 1;
            break;
        }
    }
    rt_mutex_release(&timer_lock);
    return found;
}

static int change_timeout(unsigned int secs, unsigned int usecs,
                          eloop_timeout_handler handler, void *ctx, void *user, int extend)
{
    rt_list_t *node;
    rt_tick_t ticks = timeout_ticks(secs, usecs);
    int result = -1;
    if (!event_queue)
        return -1;
    rt_mutex_take(&timer_lock, RT_WAITING_FOREVER);
    rt_list_for_each(node, &timeouts)
    {
        struct wifi_wpa_timeout *t = rt_list_entry(node, struct wifi_wpa_timeout, node);
        if (matches(t, handler, ctx, user))
        {
            rt_int32_t left = (rt_int32_t)(t->deadline - rt_tick_get());
            result = 0;
            if ((extend && left < (rt_int32_t)ticks) || (!extend && left > (rt_int32_t)ticks))
            {
                t->deadline = rt_tick_get() + ticks;
                rt_list_remove(&t->node);
                insert_timeout(t);
                rearm();
                result = 1;
            }
            break;
        }
    }
    rt_mutex_release(&timer_lock);
    return result;
}

int eloop_deplete_timeout(unsigned int secs, unsigned int usecs,
                          eloop_timeout_handler handler, void *ctx, void *user)
{
    return change_timeout(secs, usecs, handler, ctx, user, 0);
}

int eloop_replenish_timeout(unsigned int secs, unsigned int usecs,
                            eloop_timeout_handler handler, void *ctx, void *user)
{
    return change_timeout(secs, usecs, handler, ctx, user, 1);
}

/* Caller serializes this with WPA state changes using the WLAN control mutex. */
void wifi_wpa_timers_run(void)
{
    if (!event_queue)
        return;
    for (;;)
    {
        rt_list_t *node;
        struct wifi_wpa_timeout *due = RT_NULL;
        rt_mutex_take(&timer_lock, RT_WAITING_FOREVER);
        rt_list_for_each(node, &timeouts)
        {
            struct wifi_wpa_timeout *t = rt_list_entry(node, struct wifi_wpa_timeout, node);
            if ((rt_int32_t)(rt_tick_get() - t->deadline) >= 0)
            {
                due = t;
                rt_list_remove(node);
                break;
            }
        }
        rearm();
        rt_mutex_release(&timer_lock);
        if (!due)
            break;
        due->handler(due->ctx, due->user);
        rt_free(due);
    }
}

void wifi_wpa_timers_deinit(void)
{
    rt_list_t *node, *next;
    /* Only called during startup rollback, before the worker can use timers. */
    if (!event_queue)
        return;
    rt_timer_stop(&wake_timer);
    rt_mutex_take(&timer_lock, RT_WAITING_FOREVER);
    rt_list_for_each_safe(node, next, &timeouts)
    {
        struct wifi_wpa_timeout *t = rt_list_entry(node, struct wifi_wpa_timeout, node);
        rt_list_remove(node);
        rt_free(t);
    }
    rt_mutex_release(&timer_lock);
    rt_timer_detach(&wake_timer);
    rt_mutex_detach(&timer_lock);
    event_queue = RT_NULL;
}
