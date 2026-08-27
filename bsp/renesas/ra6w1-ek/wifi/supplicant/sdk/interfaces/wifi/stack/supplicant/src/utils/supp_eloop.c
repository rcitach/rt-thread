/*
 * Event loop based on select() loop
 * Copyright (c) 2002-2009, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 *
 * Copyright (c) 2023 Modified by Renesas Electronics.
 */

#include "includes.h"
#include <assert.h>

#include "supp_common.h"
#include "wpa_supplicant_i.h"
#include "trace.h"
#include "list.h"
#include "supp_eloop.h"

#include "supp_config.h"
#include "supp_scan.h"
#if CFG_PMGR
#include "sleep_mgmt_regs.h"
#include "rm_pmgr_w_instance.h"
#endif /* CFG_PMGR */

#include "common_def.h"
#include "lwip/sockets.h"

#if defined(CONFIG_ELOOP_POLL) && defined(CONFIG_ELOOP_EPOLL)
#error Do not define both of poll and epoll
#endif

#if defined(CONFIG_ELOOP_POLL) && defined(CONFIG_ELOOP_KQUEUE)
#error Do not define both of poll and kqueue
#endif

#if !defined(CONFIG_ELOOP_POLL) && !defined(CONFIG_ELOOP_EPOLL) && \
	!defined(CONFIG_ELOOP_KQUEUE)
#define CONFIG_ELOOP_SELECT
#endif

#ifdef CONFIG_ELOOP_POLL
#include <poll.h>
#endif /* CONFIG_ELOOP_POLL */

#ifdef CONFIG_ELOOP_EPOLL
#include <sys/epoll.h>
#endif /* CONFIG_ELOOP_EPOLL */

#ifdef CONFIG_ELOOP_KQUEUE
#include <sys/event.h>
#endif /* CONFIG_ELOOP_KQUEUE */

/* For Event Group */
EventGroupHandle_t	ra6w1_sp_event_group;

QueueHandle_t	ra6wx_drv_msg_tx_q;
QueueHandle_t	ra6wx_probe_msg_tx_q;
QueueHandle_t	ra6wx_cli_msg_tx_q;
QueueHandle_t	ra6wx_cli_msg_rx_q;

int ra6wx_sp_msg_q_created = 0;
ra6w1_cli_rsp_buf_t cli_rsp_buf;	/* Supplicant -> CLI */

UCHAR *ra6wx_drv_tx_msg_q_buf;
UCHAR *ra6wx_probe_msg_q_buf;

#ifdef CONFIG_SCAN_REPLY_OPTIMIZE
char	*cli_rx_ev_data = NULL;
#else
ra6wx_cli_rx_ev_data_t *cli_rx_ev_data;
#endif /* CONFIG_SCAN_REPLY_OPTIMIZE */

QueueHandle_t TO_SUPP_QUEUE;
QueueHandle_t TO_CLI_QUEUE;

struct ra6wx_eloop_data {
	struct dl_list timeout;
};

static struct ra6wx_eloop_data ra6wx_eloop;

struct eloop_timeout {
	struct dl_list list;
	struct os_reltime time;
	void *eloop_data;
	void *user_data;
	eloop_timeout_handler handler;
};

#ifndef CONFIG_MONITOR_THREAD_EVENT_CHANGE
EventGroupHandle_t wifi_monitor_event_group;			//MODIFY_SUPPLICANT_FOR_FREERTOS
#endif // CONFIG_MONITOR_THREAD_EVENT_CHANGE
QueueHandle_t wifi_monitor_event_q;						//MODIFY_SUPPLICANT_FOR_FREERTOS

extern BaseType_t start_ra6w1_wpa_supplicant(void);
extern void driver_fc80211_process_global_ev(ra6wx_drv_msg_buf_t *drv_msg_buf);
extern void wpa_supp_global_ctrl_iface_receive(struct wpa_global *global, char *buf);
#if !defined ( __DISABLE_DPM_MOD_IN_SDK__ ) && defined (__SUPPORT_IPV4__)
extern void do_dpm_autoarp_send(void);
#endif // __DISABLE_DPM_MOD_IN_SDK__ && __SUPPORT_IPV4__

struct eloop_sock {
	int sock;
	void *eloop_data;
	void *user_data;
	eloop_sock_handler handler;
	WPA_TRACE_REF(eloop);
	WPA_TRACE_REF(user);
	WPA_TRACE_INFO
};

struct eloop_signal {
	int sig;
	void *user_data;
	eloop_signal_handler handler;
	int signaled;
};

struct eloop_sock_table {
	size_t count;
	struct eloop_sock *table;
	eloop_event_type type;
	int changed;
};

struct eloop_data {
	int max_sock;

	size_t count; /* sum of all table counts */
#ifdef CONFIG_ELOOP_POLL
	size_t max_pollfd_map; /* number of pollfds_map currently allocated */
	size_t max_poll_fds; /* number of pollfds currently allocated */
	struct pollfd *pollfds;
	struct pollfd **pollfds_map;
#endif /* CONFIG_ELOOP_POLL */
#if defined(CONFIG_ELOOP_EPOLL) || defined(CONFIG_ELOOP_KQUEUE)
	int max_fd;
	struct eloop_sock *fd_table;
#endif /* CONFIG_ELOOP_EPOLL || CONFIG_ELOOP_KQUEUE */
#ifdef CONFIG_ELOOP_EPOLL
	int epollfd;
	size_t epoll_max_event_num;
	struct epoll_event *epoll_events;
#endif /* CONFIG_ELOOP_EPOLL */
#ifdef CONFIG_ELOOP_KQUEUE
	int kqueuefd;
	size_t kqueue_nevents;
	struct kevent *kqueue_events;
#endif /* CONFIG_ELOOP_KQUEUE */
	struct eloop_sock_table readers;
	struct eloop_sock_table writers;
	struct eloop_sock_table exceptions;

	struct dl_list timeout;

	size_t signal_count;
	struct eloop_signal *signals;
	int signaled;
	int pending_terminate;

	int terminate;
};

static struct eloop_data eloop;


#ifdef WPA_TRACE

static void eloop_sigsegv_handler(int sig)
{
	wpa_trace_show("eloop SIGSEGV");
	abort();
}

static void eloop_trace_sock_add_ref(struct eloop_sock_table *table)
{
	size_t i;

	if (table == NULL || table->table == NULL)
		return;
	for (i = 0; i < table->count; i++) {
		wpa_trace_add_ref(&table->table[i], eloop,
				  table->table[i].eloop_data);
		wpa_trace_add_ref(&table->table[i], user,
				  table->table[i].user_data);
	}
}


static void eloop_trace_sock_remove_ref(struct eloop_sock_table *table)
{
	size_t i;

	if (table == NULL || table->table == NULL)
		return;
	for (i = 0; i < table->count; i++) {
		wpa_trace_remove_ref(&table->table[i], eloop,
				     table->table[i].eloop_data);
		wpa_trace_remove_ref(&table->table[i], user,
				     table->table[i].user_data);
	}
}

#else /* WPA_TRACE */

#define eloop_trace_sock_add_ref(table) do { } while (0)
#define eloop_trace_sock_remove_ref(table) do { } while (0)

#endif /* WPA_TRACE */

#ifdef CONFIG_ELOOP_EPOLL
static int eloop_sock_queue(int sock, eloop_event_type type)
{
	struct epoll_event ev;

	os_memset(&ev, 0, sizeof(ev));
	switch (type) {
	case EVENT_TYPE_READ:
		ev.events = EPOLLIN;
		break;
	case EVENT_TYPE_WRITE:
		ev.events = EPOLLOUT;
		break;
	/*
	 * Exceptions are always checked when using epoll, but I suppose it's
	 * possible that someone registered a socket *only* for exception
	 * handling.
	 */
	case EVENT_TYPE_EXCEPTION:
		ev.events = EPOLLERR | EPOLLHUP;
		break;
	}
	ev.data.fd = sock;
	if (epoll_ctl(eloop.epollfd, EPOLL_CTL_ADD, sock, &ev) < 0) {
		wpa_printf(MSG_ERROR, "%s: epoll_ctl(ADD) for fd=%d failed: %s",
			   __func__, sock, strerror(errno));
		return -1;
	}
	return 0;
}
#endif /* CONFIG_ELOOP_EPOLL */


#ifdef CONFIG_ELOOP_KQUEUE

static short event_type_kevent_filter(eloop_event_type type)
{
	switch (type) {
	case EVENT_TYPE_READ:
		return EVFILT_READ;
	case EVENT_TYPE_WRITE:
		return EVFILT_WRITE;
	default:
		return 0;
	}
}


static int eloop_sock_queue(int sock, eloop_event_type type)
{
	struct kevent ke;

	EV_SET(&ke, sock, event_type_kevent_filter(type), EV_ADD, 0, 0, 0);
	if (kevent(eloop.kqueuefd, &ke, 1, NULL, 0, NULL) == -1) {
		wpa_printf(MSG_ERROR, "%s: kevent(ADD) for fd=%d failed: %s",
			   __func__, sock, strerror(errno));
		return -1;
	}
	return 0;
}

#endif /* CONFIG_ELOOP_KQUEUE */


static int eloop_sock_table_add_sock(struct eloop_sock_table *table,
									int sock, eloop_sock_handler handler,
									void *eloop_data, void *user_data)
{
#ifdef CONFIG_ELOOP_EPOLL
	struct epoll_event *temp_events;
#endif /* CONFIG_ELOOP_EPOLL */
#ifdef CONFIG_ELOOP_KQUEUE
	struct kevent *temp_events;
#endif /* CONFIG_ELOOP_EPOLL */
#if defined(CONFIG_ELOOP_EPOLL) || defined(CONFIG_ELOOP_KQUEUE)
	struct eloop_sock *temp_table;
	size_t next;
#endif /* CONFIG_ELOOP_EPOLL || CONFIG_ELOOP_KQUEUE */
	struct eloop_sock *tmp;
	int new_max_sock;

	if (sock > eloop.max_sock)
		new_max_sock = sock;
	else
		new_max_sock = eloop.max_sock;

	if (table == NULL)
		return -1;

#ifdef CONFIG_ELOOP_POLL
	if ((size_t) new_max_sock >= eloop.max_pollfd_map) {
		struct pollfd **nmap;
		nmap = os_realloc_array(eloop.pollfds_map, new_max_sock + 50,
					sizeof(struct pollfd *));
		if (nmap == NULL)
			return -1;

		eloop.max_pollfd_map = new_max_sock + 50;
		eloop.pollfds_map = nmap;
	}

	if (eloop.count + 1 > eloop.max_poll_fds) {
		struct pollfd *n;
		size_t nmax = eloop.count + 1 + 50;

		n = os_realloc_array(eloop.pollfds, nmax,
				     sizeof(struct pollfd));
		if (n == NULL)
			return -1;

		eloop.max_poll_fds = nmax;
		eloop.pollfds = n;
	}
#endif /* CONFIG_ELOOP_POLL */
#if defined(CONFIG_ELOOP_EPOLL) || defined(CONFIG_ELOOP_KQUEUE)
	if (new_max_sock >= eloop.max_fd) {
		next = new_max_sock + 16;
		temp_table = os_realloc_array(eloop.fd_table, next,
					      sizeof(struct eloop_sock));
		if (temp_table == NULL)
			return -1;

		eloop.max_fd = next;
		eloop.fd_table = temp_table;
	}
#endif /* CONFIG_ELOOP_EPOLL || CONFIG_ELOOP_KQUEUE */

#ifdef CONFIG_ELOOP_EPOLL
	if (eloop.count + 1 > eloop.epoll_max_event_num) {
		next = eloop.epoll_max_event_num == 0 ? 8 :
			eloop.epoll_max_event_num * 2;
		temp_events = os_realloc_array(eloop.epoll_events, next,
					       sizeof(struct epoll_event));
		if (temp_events == NULL) {
			wpa_printf(MSG_ERROR, "%s: malloc for epoll failed: %s",
				   __func__, strerror(errno));
			return -1;
		}

		eloop.epoll_max_event_num = next;
		eloop.epoll_events = temp_events;
	}
#endif /* CONFIG_ELOOP_EPOLL */
#ifdef CONFIG_ELOOP_KQUEUE
	if (eloop.count + 1 > eloop.kqueue_nevents) {
		next = eloop.kqueue_nevents == 0 ? 8 : eloop.kqueue_nevents * 2;
		temp_events = os_malloc(next * sizeof(*temp_events));
		if (!temp_events) {
			wpa_printf(MSG_ERROR,
				   "%s: malloc for kqueue failed: %s",
				   __func__, strerror(errno));
			return -1;
		}

		os_free(eloop.kqueue_events);
		eloop.kqueue_events = temp_events;
		eloop.kqueue_nevents = next;
	}
#endif /* CONFIG_ELOOP_KQUEUE */

	eloop_trace_sock_remove_ref(table);
	tmp = os_realloc_array(table->table, table->count + 1,
			       sizeof(struct eloop_sock));
	if (tmp == NULL) {
		eloop_trace_sock_add_ref(table);
		return -1;
	}

	tmp[table->count].sock = sock;
	tmp[table->count].eloop_data = eloop_data;
	tmp[table->count].user_data = user_data;
	tmp[table->count].handler = handler;
	wpa_trace_record(&tmp[table->count]);
	table->count++;
	table->table = tmp;
	eloop.max_sock = new_max_sock;
	eloop.count++;
	table->changed = 1;
	eloop_trace_sock_add_ref(table);

#if defined(CONFIG_ELOOP_EPOLL) || defined(CONFIG_ELOOP_KQUEUE)
	if (eloop_sock_queue(sock, table->type) < 0)
		return -1;
	os_memcpy(&eloop.fd_table[sock], &table->table[table->count - 1],
		  sizeof(struct eloop_sock));
#endif /* CONFIG_ELOOP_EPOLL || CONFIG_ELOOP_KQUEUE */
	return 0;
}


static void eloop_sock_table_remove_sock(struct eloop_sock_table *table, int sock)
{
#ifdef CONFIG_ELOOP_KQUEUE
	struct kevent ke;
#endif /* CONFIG_ELOOP_KQUEUE */
	size_t i;

	if (table == NULL || table->table == NULL || table->count == 0)
		return;

	for (i = 0; i < table->count; i++) {
		if (table->table[i].sock == sock)
			break;
	}
	if (i == table->count)
		return;
	eloop_trace_sock_remove_ref(table);
	if (i != table->count - 1) {
		os_memmove(&table->table[i], &table->table[i + 1],
			   (table->count - i - 1) *
			   sizeof(struct eloop_sock));
	}
	table->count--;
	eloop.count--;
	table->changed = 1;
	eloop_trace_sock_add_ref(table);
#ifdef CONFIG_ELOOP_EPOLL
	if (epoll_ctl(eloop.epollfd, EPOLL_CTL_DEL, sock, NULL) < 0) {
		wpa_printf(MSG_ERROR, "%s: epoll_ctl(DEL) for fd=%d failed: %s",
			   __func__, sock, strerror(errno));
		return;
	}
	os_memset(&eloop.fd_table[sock], 0, sizeof(struct eloop_sock));
#endif /* CONFIG_ELOOP_EPOLL */
#ifdef CONFIG_ELOOP_KQUEUE
	EV_SET(&ke, sock, event_type_kevent_filter(table->type), EV_DELETE, 0,
	       0, 0);
	if (kevent(eloop.kqueuefd, &ke, 1, NULL, 0, NULL) < 0) {
		wpa_printf(MSG_ERROR, "%s: kevent(DEL) for fd=%d failed: %s",
			   __func__, sock, strerror(errno));
		return;
	}
	os_memset(&eloop.fd_table[sock], 0, sizeof(struct eloop_sock));
#endif /* CONFIG_ELOOP_KQUEUE */
}


#ifdef CONFIG_ELOOP_KQUEUE

static int eloop_sock_table_requeue(struct eloop_sock_table *table)
{
	size_t i;
	int r;

	r = 0;
	for (i = 0; i < table->count && table->table; i++) {
		if (eloop_sock_queue(table->table[i].sock, table->type) == -1)
			r = -1;
	}
	return r;
}

#endif /* CONFIG_ELOOP_KQUEUE */


int eloop_sock_requeue(void)
{
	int r = 0;

#ifdef CONFIG_ELOOP_KQUEUE
	close(eloop.kqueuefd);
	eloop.kqueuefd = kqueue();
	if (eloop.kqueuefd < 0) {
		wpa_printf(MSG_ERROR, "%s: kqueue failed: %s",
			   __func__, strerror(errno));
		return -1;
	}

	if (eloop_sock_table_requeue(&eloop.readers) < 0)
		r = -1;
	if (eloop_sock_table_requeue(&eloop.writers) < 0)
		r = -1;
	if (eloop_sock_table_requeue(&eloop.exceptions) < 0)
		r = -1;
#endif /* CONFIG_ELOOP_KQUEUE */

	return r;
}


static void eloop_sock_table_destroy(struct eloop_sock_table *table)
{
	if (table) {
		size_t i;

		for (i = 0; i < table->count && table->table; i++) {
			wpa_printf(MSG_INFO, "ELOOP: remaining socket: "
				   "sock=%d eloop_data=%p user_data=%p "
				   "handler=%p",
				   table->table[i].sock,
				   table->table[i].eloop_data,
				   table->table[i].user_data,
				   table->table[i].handler);
			wpa_trace_dump_funcname("eloop unregistered socket "
						"handler",
						table->table[i].handler);
			wpa_trace_dump("eloop sock", &table->table[i]);
		}
		os_free(table->table);
	}
}

static struct eloop_sock_table *eloop_get_sock_table(eloop_event_type type)
{
	switch (type) {
	case EVENT_TYPE_READ:
		return &eloop.readers;
	case EVENT_TYPE_WRITE:
		return &eloop.writers;
	case EVENT_TYPE_EXCEPTION:
		return &eloop.exceptions;
	}

	return NULL;
}


static int eloop_register_sock(int sock, eloop_event_type type,
			eloop_sock_handler handler,
			void *eloop_data, void *user_data)
{
	struct eloop_sock_table *table;

	assert(sock >= 0);
	table = eloop_get_sock_table(type);
	return eloop_sock_table_add_sock(table, sock, handler,
					 eloop_data, user_data);
}


static void eloop_unregister_sock(int sock, eloop_event_type type)
{
	struct eloop_sock_table *table;

	table = eloop_get_sock_table(type);
	eloop_sock_table_remove_sock(table, sock);
}

int eloop_register_read_sock(int sock, eloop_sock_handler handler,
			     void *eloop_data, void *user_data)
{
	return eloop_register_sock(sock, EVENT_TYPE_READ, handler,
				   eloop_data, user_data);
}

void eloop_unregister_read_sock(int sock)
{
	eloop_unregister_sock(sock, EVENT_TYPE_READ);
}

int eloop_register_timeout(unsigned int secs, unsigned int usecs,
			   eloop_timeout_handler handler,
			   void *eloop_data, void *user_data)
{
	struct eloop_timeout *timeout, *tmp;
	os_time_t now_sec;

	//TX_FUNC_START("");

	timeout = os_zalloc(sizeof(*timeout));
	if (timeout == NULL) {
		ra6wx_eloop_prt("[%s] os_zalloc fail\n", __func__);
		return -1;
	}

	if (os_get_reltime(&timeout->time) < 0) {
		os_free(timeout);

		ra6wx_eloop_prt("[%s] os_get_reltime fail\n", __func__);
		return -1;
	}
	now_sec = timeout->time.sec;
	timeout->time.sec += secs;
	if (timeout->time.sec < now_sec) {
		/*
		 * Integer overflow - assume long enough timeout to be assumed
		 * to be infinite, i.e., the timeout would never happen.
		 */
		ra6wx_eloop_prt("[%s] Too long timeout "
				"(secs=%u) to ever happen - ignore it\n",
					__func__, secs);
		os_free(timeout);
		return -1;
	}

#if 0
	ra6wx_eloop_prt("[%s] time.sec=%lu, now_sec=%lu\n",
				__func__, timeout->time.sec, now_sec);
#endif /* 0 */

	timeout->time.usec += usecs;
	while (timeout->time.usec >= 1000000) {
		timeout->time.sec++;
		timeout->time.usec -= 1000000;
	}
	timeout->eloop_data = eloop_data;
	timeout->user_data = user_data;
	timeout->handler = handler;

	/* Maintain timeouts in order of increasing time */
	dl_list_for_each(tmp, &ra6wx_eloop.timeout,
				struct eloop_timeout, list) {
		if (os_reltime_before(&timeout->time, &tmp->time)) {
			dl_list_add(tmp->list.prev, &timeout->list);

			return 0;
		}
	}

#if 0
	ra6wx_eloop_prt("[%s] Add timer to eloop.timeout\n", __func__);
#endif /* 0 */

	dl_list_add_tail(&ra6wx_eloop.timeout, &timeout->list);

	//TX_FUNC_END("");

	return 0;
}


static void eloop_remove_timeout(struct eloop_timeout *timeout)
{
	//TX_FUNC_START("");
	dl_list_del(&timeout->list);
	os_free(timeout);
	//TX_FUNC_END("");
}

int eloop_cancel_timeout(eloop_timeout_handler handler,
			 void *eloop_data, void *user_data)
{
	struct eloop_timeout *timeout, *prev;
	int removed = 0;

	//TX_FUNC_START("");

	dl_list_for_each_safe(timeout, prev, &ra6wx_eloop.timeout,
			      struct eloop_timeout, list) {
		if (timeout->handler == handler) {
			if (   timeout->eloop_data == eloop_data
			    || eloop_data == ELOOP_ALL_CTX) {
				if (   timeout->user_data == user_data
				    || user_data == ELOOP_ALL_CTX) {
					eloop_remove_timeout(timeout);
					removed++;
				}

			}
		}
	}

	//TX_FUNC_END("");

	return removed;
}


int eloop_cancel_timeout_one(eloop_timeout_handler handler,
			     void *eloop_data, void *user_data,
			     struct os_reltime *remaining)
{
	struct eloop_timeout *timeout, *prev;
	int removed = 0;
	struct os_reltime now;

	os_get_reltime(&now);
	remaining->sec = remaining->usec = 0;

	dl_list_for_each_safe(timeout, prev, &ra6wx_eloop.timeout,
			      struct eloop_timeout, list) {
		if (timeout->handler == handler &&
		    (timeout->eloop_data == eloop_data) &&
		    (timeout->user_data == user_data)) {
			removed = 1;
			if (os_reltime_before(&now, &timeout->time))
				os_reltime_sub(&timeout->time, &now, remaining);
			eloop_remove_timeout(timeout);
			break;
		}
	}
	return removed;
}


int eloop_is_timeout_registered(eloop_timeout_handler handler,
				void *eloop_data, void *user_data)
{
	struct eloop_timeout *tmp;

	dl_list_for_each(tmp, &ra6wx_eloop.timeout, struct eloop_timeout, list) {
		if (tmp->handler == handler &&
		    tmp->eloop_data == eloop_data &&
		    tmp->user_data == user_data)
			return 1;
	}

	return 0;
}

int eloop_deplete_timeout(unsigned int req_secs,
				unsigned int req_usecs,
			  	eloop_timeout_handler handler,
				void *eloop_data,
				void *user_data)
{
	struct os_reltime now, requested, remaining;
	struct eloop_timeout *tmp;

	dl_list_for_each(tmp, &ra6wx_eloop.timeout, struct eloop_timeout,
									list) {
		if (tmp->handler == handler &&
		    tmp->eloop_data == eloop_data &&
		    tmp->user_data == user_data) {

			requested.sec = req_secs;
			requested.usec = req_usecs;

			os_get_reltime(&now);
			os_reltime_sub(&tmp->time, &now, &remaining);

			if (os_reltime_before(&requested, &remaining)) {
				eloop_cancel_timeout(handler,
							eloop_data,
							user_data);
				eloop_register_timeout(requested.sec,
							requested.usec,
							handler,
							eloop_data,
							user_data);
				return 1;
			}
			return 0;
		}
	}

	return -1;
}


int eloop_replenish_timeout(unsigned int req_secs, unsigned int req_usecs,
			    eloop_timeout_handler handler, void *eloop_data,
			    void *user_data)
{
	struct os_reltime now, requested, remaining;
	struct eloop_timeout *tmp;

	dl_list_for_each(tmp, &eloop.timeout, struct eloop_timeout, list) {
		if (tmp->handler == handler &&
		    tmp->eloop_data == eloop_data &&
		    tmp->user_data == user_data) {
			requested.sec = req_secs;
			requested.usec = req_usecs;
			os_get_reltime(&now);
			os_reltime_sub(&tmp->time, &now, &remaining);
			if (os_reltime_before(&remaining, &requested)) {
				eloop_cancel_timeout(handler, eloop_data,
						     user_data);
				eloop_register_timeout(requested.sec,
						       requested.usec,
						       handler, eloop_data,
						       user_data);
				return 1;
			}
			return 0;
		}
	}

	return -1;
}


void eloop_terminate(void)
{
	eloop.terminate = 1;
}


void eloop_destroy(void)
{
	struct eloop_timeout *timeout, *prev;
	struct os_reltime now;

	os_get_reltime(&now);
	dl_list_for_each_safe(timeout, prev, &eloop.timeout,
			      struct eloop_timeout, list) {
		int sec, usec;
		sec = timeout->time.sec - now.sec;
		usec = timeout->time.usec - now.usec;
		if (timeout->time.usec < now.usec) {
			sec--;
			usec += 1000000;
		}
		wpa_printf(MSG_INFO, "ELOOP: remaining timeout: %d.%06d "
			   "eloop_data=%p user_data=%p handler=%p",
			   sec, usec, timeout->eloop_data, timeout->user_data,
			   timeout->handler);
		wpa_trace_dump_funcname("eloop unregistered timeout handler",
					timeout->handler);
		wpa_trace_dump("eloop timeout", timeout);
		eloop_remove_timeout(timeout);
	}
	eloop_sock_table_destroy(&eloop.readers);
	eloop_sock_table_destroy(&eloop.writers);
	eloop_sock_table_destroy(&eloop.exceptions);
	os_free(eloop.signals);

#ifdef CONFIG_ELOOP_POLL
	os_free(eloop.pollfds);
	os_free(eloop.pollfds_map);
#endif /* CONFIG_ELOOP_POLL */
#if defined(CONFIG_ELOOP_EPOLL) || defined(CONFIG_ELOOP_KQUEUE)
	os_free(eloop.fd_table);
#endif /* CONFIG_ELOOP_EPOLL || CONFIG_ELOOP_KQUEUE */
#ifdef CONFIG_ELOOP_EPOLL
	os_free(eloop.epoll_events);
	close(eloop.epollfd);
#endif /* CONFIG_ELOOP_EPOLL */
#ifdef CONFIG_ELOOP_KQUEUE
	os_free(eloop.kqueue_events);
	close(eloop.kqueuefd);
#endif /* CONFIG_ELOOP_KQUEUE */
}


int eloop_terminated(void)
{
	return eloop.terminate || eloop.pending_terminate;
}


void eloop_wait_for_read_sock(int sock)
{
#ifdef CONFIG_ELOOP_POLL
	struct pollfd pfd;

	if (sock < 0)
		return;

	os_memset(&pfd, 0, sizeof(pfd));
	pfd.fd = sock;
	pfd.events = POLLIN;

	poll(&pfd, 1, -1);
#endif /* CONFIG_ELOOP_POLL */
#if defined(CONFIG_ELOOP_SELECT) || defined(CONFIG_ELOOP_EPOLL)
	/*
	 * We can use epoll() here. But epoll() requres 4 system calls.
	 * epoll_create1(), epoll_ctl() for ADD, epoll_wait, and close() for
	 * epoll fd. So select() is better for performance here.
	 */
	fd_set rfds;

	if (sock < 0)
		return;

	FD_ZERO(&rfds);
	FD_SET(sock, &rfds);
	select(sock + 1, &rfds, NULL, NULL, NULL);
#endif /* defined(CONFIG_ELOOP_SELECT) || defined(CONFIG_ELOOP_EPOLL) */
#ifdef CONFIG_ELOOP_KQUEUE
	int kfd;
	struct kevent ke1, ke2;

	kfd = kqueue();
	if (kfd == -1)
		return;
	EV_SET(&ke1, sock, EVFILT_READ, EV_ADD | EV_ONESHOT, 0, 0, 0);
	kevent(kfd, &ke1, 1, &ke2, 1, NULL);
	close(kfd);
#endif /* CONFIG_ELOOP_KQUEUE */
}

//MODIFY_SUPPLICANT_FOR_FREERTOS - start add
/*
 * Function : ra6wx_drv_event_handler
 *
 * - arguments	: ra6wx_drv_msg_buf_t *drv_msg_buf
 * - return	: NONE
 *
 * - Discription :
 *	FC80211 driver event handler for FCI supplicant.
 */
static void ra6wx_drv_event_handler(ra6wx_drv_msg_buf_t *drv_msg_buf)
{
	//TX_FUNC_START("");
	driver_fc80211_process_global_ev(drv_msg_buf);
	//TX_FUNC_END("");
}

static void ra6wx_cli_cmd_handler(struct wpa_global *global,
				ra6wx_cli_tx_msg_t *cli_cmd_buf)
{
	extern void wpa_supplicant_global_ctrl_iface_recv(
				struct wpa_global *global, char *buf);

	TX_FUNC_START("");
	wpa_supplicant_global_ctrl_iface_recv(global,
				cli_cmd_buf->cli_tx_buf->cmd_str);
	TX_FUNC_END("");
}

#ifdef CONFIG_RECEIVE_DHCP_EVENT
static void ra6wx_dhcp_event_handler(struct wpa_global *global,
						ULONG *data)
{	
	struct wpa_supplicant *wpa_s=NULL;

	for (wpa_s = global->ifaces; wpa_s ;
							wpa_s = wpa_s->next) {
		if(strcmp("sta0", wpa_s->ifname) == 0) {
			if (*data == 0x00000001)
			{
				ra6wx_eloop_prt("[%s] DHCP event received 1\n", __func__);
				wpa_supplicant_event(wpa_s, EVENT_DHCP_NO_RESPONSE, NULL);
			}
			else if (*data == 0x00000002)
			{
				ra6wx_eloop_prt("[%s] DHCP event received 2\n", __func__);
				wpa_supplicant_event(wpa_s, EVENT_DHCP_ACK_OK, NULL);			
			}
		}
	}
}
#endif /* CONFIG_RECEIVE_DHCP_EVENT */

void *init_supp_mempool(void *pointer)
{
	int size = 0;
#ifndef CONFIG_SCAN_REPLY_OPTIMIZE
	cli_rx_ev_data = os_malloc(sizeof(ra6wx_cli_rx_ev_data_t));
#endif	/* CONFIG_SCAN_REPLY_OPTIMIZE */
		pointer = (void *)((unsigned int)pointer + size);

		return pointer;
}

static int create_supp_msg_queues () 
{
	if (ra6wx_sp_msg_q_created == 0) {
		ra6wx_eloop_prt("[%s] Create messge queues to / from supplicant\n", __func__);

		TO_SUPP_QUEUE = xQueueCreate(TO_SUPP_QUEUE_SIZE, sizeof(ULONG)*2);

		if (TO_SUPP_QUEUE == NULL) {
			ra6wx_eloop_prt("[%s] : TO_SUPP_QUEUE create fail\n", __func__);
			return -1;
		}

		TO_CLI_QUEUE = xQueueCreate(TO_CLI_QUEUE_SIZE, sizeof(ULONG)*2);

		if (TO_CLI_QUEUE == NULL) {
			ra6wx_eloop_prt("[%s] : TO_SUPP_QUEUE create fail\n", __func__);
			return -1;
		}

		ra6wx_sp_msg_q_created = 1;
	} else {
		ra6wx_eloop_prt("[%s] event flag group already created.\n", __func__);
	}

	return pdPASS;
}
/*
 * Function : ra6wx_eloop_init
 *
 * - arguments	: none
 * - return	: OK
 *
 * - Discription :
 *	Event loop init function for FCI supplicant.
 */
int ra6wx_eloop_init()
{
	UINT	status;

	status = create_supp_msg_queues();

	if (status !=  pdPASS) {
		ra6wx_eloop_prt("[%s] : event flag group create failed "
				"(stauts=%d)\n",
				__func__, status);
		return -1;
	}

	os_memset(&ra6wx_eloop, 0, sizeof(ra6wx_eloop));
	dl_list_init(&ra6wx_eloop.timeout);

	return 0;
}

/*
 * Function : ra6wx_eloop_run
 *
 * - arguments	: wpa_global
 * - return	: none
 *
 * - Discription :
 *	Event loop main function for FCI supplicant.
 */
extern void l2_packet_receive(ULONG data);
extern void fc80211_enable_drv_event(void);

void ra6wx_eloop_run(struct wpa_global *global, struct wpa_supplicant *wpa_s)
{
	UINT	status_q;
#ifdef CONFIG_SUPP_EVENT_DEPRECATED		
	ULONG	temp_pack[2];
#else
	ULONG	*temp_pack=NULL;
#endif /* CONFIG_SUPP_EVENT_DEPRECATED */

#if CFG_PMGR
	int	dpm_sts, dpm_retry_cnt = 0;
#endif /* CFG_PMGR */

	struct os_reltime tv, now;
	tv.sec = tv.usec = 0;

	fc80211_enable_drv_event();

	/* For Message Queue */
	ra6wx_drv_msg_buf_t *ra6wx_drv_msg_buf_p=NULL;
	ra6wx_cli_tx_msg_t *ra6wx_cli_cmd_buf_p=NULL;

	dpm_supplicant_done();

	while (1) {
		struct eloop_timeout *timeout=NULL;

#ifdef CHECK_SUPPLICANT_ERR
		status_q = NX_NOT_FOUND;
#endif	/*CHECK_SUPPLICANT_ERR*/

		timeout = dl_list_first(&ra6wx_eloop.timeout,
					struct eloop_timeout, list);
		if (timeout) {
			os_get_reltime(&now);

			/* check if some registered timeouts have occurred */
			if (os_reltime_before(&now, &timeout->time)) {
				/* Not occurred yet. Calculate remaining time */
				os_reltime_sub(&timeout->time, &now, &tv);
			} else { /* Some timeout has occurred! Call handler */
				void *eloop_data = timeout->eloop_data;
				void *user_data = timeout->user_data;

				eloop_timeout_handler handler = timeout->handler;
				eloop_remove_timeout(timeout);
				handler(eloop_data, user_data);
				tv.sec = tv.usec = 0;
			}
		}
		else {  // if timeout list is NULL, timer reset 
			tv.sec = tv.usec = 0;
		}

#if CFG_PMGR
		if (RM_PMGR_W_dpm_sleep_is_started()) {
			vTaskDelay(1);
			continue;
		}
#endif /* CFG_PMGR */

		status_q = xQueueReceive(TO_SUPP_QUEUE,
								 &temp_pack,
								 timeout ? (portCONVERT_MS_2_TICKS(tv.sec * 1000 + 4)) : portMAX_DELAY);

		if (status_q == pdTRUE) {
#if CFG_PMGR
dpm_clr_retry :
			if (dpm_retry_cnt++ < 5) {
				dpm_sts = RM_PMGR_W_dpm_sleep_ready_clear(REG_NAME_DPM_SUPPLICANT);
				
				switch (dpm_sts) {
					case DPM_SET_ERR :
						vTaskDelay(1);
						goto dpm_clr_retry;
						break;
					case DPM_SET_ERR_BLOCK :
						/* Don't need try continues */
						;
					case DPM_SET_OK :
						break;
				}
			}
			dpm_retry_cnt = 0;
#endif /* CFG_PMGR */

			if (temp_pack[0] == RA6WX_SP_DRV_FLAG) {
				ra6wx_drv_msg_buf_p = (ra6wx_drv_msg_buf_t *)temp_pack[1];
				ra6wx_drv_event_handler(ra6wx_drv_msg_buf_p);
				if (ra6wx_drv_msg_buf_p) {
					os_free(ra6wx_drv_msg_buf_p);
				}
			} else if (temp_pack[0] == RA6WX_SP_CLI_TX_FLAG) {
				ra6wx_cli_cmd_buf_p = (ra6wx_cli_tx_msg_t *)temp_pack[1];
#ifndef CONFIG_SUPP_EVENT_DEPRECATED
				if (temp_pack)
				{
					os_free(temp_pack);
				}
#endif /* CONFIG_SUPP_EVENT_DEPRECATED */
				ra6wx_cli_cmd_handler(global,ra6wx_cli_cmd_buf_p);

				if (ra6wx_cli_cmd_buf_p->cli_tx_buf)
				{
					os_free(ra6wx_cli_cmd_buf_p->cli_tx_buf);
				}
			} else if (temp_pack[0] == RA6WX_L2_PKT_RX_EV) {
				l2_packet_receive(temp_pack[1]);
#ifndef CONFIG_SUPP_EVENT_DEPRECATED
				os_free(temp_pack);
#endif /* CONFIG_SUPP_EVENT_DEPRECATED */

#if CFG_PMGR
				if (RM_PMGR_W_dpm_is_wakeup() && wpa_s->wpa_state == WPA_COMPLETED) {
					ra6wx_eap_prt("L2 Packet process completed, Set DPM Sleep !!! \n");

					/* * DPM EAPOL Update Patch
				 	 * In case of Some AP, After EAPOL Update, updated as different HW KEY ID , 
				 	 * so need to send ARP REQ for DPM Communication
		 		 	 */
#if !defined ( __DISABLE_DPM_MOD_IN_SDK__ ) && defined (__SUPPORT_IPV4__)
					do_dpm_autoarp_send();
#endif // __DISABLE_DPM_MOD_IN_SDK__ && __SUPPORT_IPV4__

					/* tcp check port is not affected */
					//set_dpm_tim_tcp_chkport_enable();

					RM_PMGR_W_dpm_sleep_ready_set(REG_NAME_DPM_KEY);
				}
#endif /* CFG_PMGR */
#ifdef CONFIG_RECEIVE_DHCP_EVENT
			} else if (temp_pack[0] == RA6WX_DHCP_EV) {
				ra6wx_dhcp_event_handler(global, &temp_pack[1]);
#ifndef CONFIG_SUPP_EVENT_DEPRECATED
				os_free(temp_pack);
#endif /* CONFIG_SUPP_EVENT_DEPRECATED */
#endif /* CONFIG_RECEIVE_DHCP_EVENT */
			}
			else if (temp_pack[0] == RA6WX_STOP_EV) {
#ifndef CONFIG_SUPP_EVENT_DEPRECATED
				os_free(temp_pack);
#endif /* CONFIG_SUPP_EVENT_DEPRECATED */
				break;
			}

#if CFG_PMGR
			/* Need to set dpm sleep flag at this time ... */
			if (wpa_s->wpa_state == WPA_COMPLETED) {
dpm_set_retry_1 :
				if (dpm_retry_cnt++ < 5) {
					dpm_sts = RM_PMGR_W_dpm_sleep_ready_set(REG_NAME_DPM_SUPPLICANT);
					switch (dpm_sts) {
					case DPM_SET_ERR :
						vTaskDelay(1);
						goto dpm_set_retry_1;
						break;
					case DPM_SET_ERR_BLOCK :
						/* Don't need try continues */
						;
					case DPM_SET_OK :
						break;
					}
				}
				dpm_retry_cnt = 0;
			}
#endif /* CFG_PMGR */
		} else if (status_q == pdFALSE) {
#if CFG_PMGR
			if (wpa_s->wpa_state == WPA_COMPLETED) {
dpm_set_retry_2 :
				if (dpm_retry_cnt++ < 5) {
					dpm_sts = RM_PMGR_W_dpm_sleep_ready_set(REG_NAME_DPM_SUPPLICANT);
					switch (dpm_sts) {
					case DPM_SET_ERR :
						vTaskDelay(1);
						goto dpm_set_retry_2;
						break;
					case DPM_SET_ERR_BLOCK :
						/* Don't need try continues */
						;
					case DPM_SET_OK :
						break;
					}
				}
				dpm_retry_cnt = 0;
			}
#endif /* CFG_PMGR */
		}
	}

	//printf(" >>> End Supplicant \n");
}

int request_stop_supplicant(void)
{
#ifdef CONFIG_SUPP_EVENT_DEPRECATED
	ULONG   temp_pack[2];
#endif

	if (is_supplicant_done()) {

#ifdef CONFIG_SUPP_EVENT_DEPRECATED
		temp_pack[0] = RA6WX_STOP_EV;
		temp_pack[1] = 0;

		xQueueSend(TO_SUPP_QUEUE, &temp_pack, portMAX_DELAY);
#else
		ULONG *temp_pack;

		temp_pack = (void *) pvPortMalloc(sizeof(ULONG)*2);

		if (temp_pack == NULL) {
			return -1;
		}

		temp_pack[0] = RA6WX_STOP_EV;
		temp_pack[1] = 0;

		xQueueSend(TO_SUPP_QUEUE, &temp_pack, portMAX_DELAY);
#endif // CONFIG_SUPP_EVENT_DEPRECATED

		return 0;
	} else {
		printf(RED_COLOR " Supplicant not working. \n" CLEAR_COLOR);
		return 1;
	}
}

int request_start_supplicant(void)
{
	if (!is_supplicant_done()) {
	    if (start_ra6w1_wpa_supplicant() == pdFAIL) {
        	printf(">>> Supplicant start failed\n");
        	return pdFAIL;
    	} else {
        	printf(">>> Request Supplicant start \n");
        	return 0;
		}
	} else {
		printf(RED_COLOR " Supplicant already working. \n" CLEAR_COLOR);
		return 1;
	}
}
//MODIFY_SUPPLICANT_FOR_FREERTOS - end add

#ifdef CONFIG_ELOOP_SELECT
#undef CONFIG_ELOOP_SELECT
#endif /* CONFIG_ELOOP_SELECT */

/* EOF */
