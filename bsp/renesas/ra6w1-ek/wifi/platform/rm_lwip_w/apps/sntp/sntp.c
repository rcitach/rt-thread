/**
 * @file
 * SNTP client module
 */

/*
 * Copyright (c) 2007-2009 Frederic Bernon, Simon Goldschmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Frederic Bernon, Simon Goldschmidt
 *
 * Copyright (C) 2023-2024 Modified by Renesas Electronics
 *
 */

/**
 *
 * This is simple "SNTP" client for the lwIP raw API.
 * It is a minimal implementation of SNTPv4 as specified in RFC 4330.
 *
 * For a list of some public NTP servers, see this link:
 * http://support.ntp.org/bin/view/Servers/NTPPoolServers
 *
 * @todo:
 * - complete SNTP_CHECK_RESPONSE checks 3 and 4
 */

#include "FreeRTOS.h"
#include "osal.h"
#include "lwipopts.h"

#if defined(__SUPPORT_SNTP_CLIENT__)

 #include "rm_sntp.h"

 #include "lwip/opt.h"
 #include "lwip/timeouts.h"
 #include "lwip/udp.h"
 #include "lwip/dns.h"
 #include "lwip/ip_addr.h"
 #include "lwip/pbuf.h"
 #include "lwip/dhcp.h"

 #include "iface_defs.h"
 #include "common_def.h"

 #include <string.h>
 #include <time.h>

 #include "net_dhcp_client.h"
 #include "net_network_main.h"
 #if CFG_PMGR
  #include "rm_pmgr_w_instance.h"
 #endif                                /* CFG_PMGR */

 #include "supp_def.h"                 // For feature __LIGHT_SUPPLICANT__
 #include "rm_lwip_w_helper.h"
 #include "rm_vee_flash_w_rrq_nvram.h"
 #ifdef RM_MAP_PERSISTANT_W
  #include "rm_map_persistant_w.h"
 #endif

 #if CFG_RTC_W
  #include "r_rtc_w.h"
  #include "r_rtc_w_helper.h"
 #endif                                /* CFG_RTC_W */

 #pragma GCC diagnostic ignored "-Wconversion"
 #pragma GCC diagnostic ignored "-Wsign-conversion"

 #define MAX_SNTP_SERVER    3

 #if LWIP_UDP

/* Handle support for more than one server via SNTP_MAX_SERVERS */
  #if MAX_SNTP_SERVER > 1
   #define SNTP_SUPPORT_MULTIPLE_SERVERS    1
  #else                                /* NTP_MAX_SERVERS > 1 */
   #define SNTP_SUPPORT_MULTIPLE_SERVERS    0
  #endif /* NTP_MAX_SERVERS > 1 */

  #ifndef SNTP_SUPPRESS_DELAY_CHECK
   #if SNTP_UPDATE_DELAY < 15000
    #error \
    "SNTPv4 RFC 4330 enforces a minimum update time of 15 seconds (define SNTP_SUPPRESS_DELAY_CHECK to disable this error)!"
   #endif
  #endif

/* the various debug levels for this file */
  #define SNTP_DEBUG_TRACE              (SNTP_DEBUG | LWIP_DBG_TRACE)
  #define SNTP_DEBUG_STATE              (SNTP_DEBUG | LWIP_DBG_STATE)
  #define SNTP_DEBUG_WARN               (SNTP_DEBUG | LWIP_DBG_LEVEL_WARNING)
  #define SNTP_DEBUG_WARN_STATE         (SNTP_DEBUG | LWIP_DBG_LEVEL_WARNING | LWIP_DBG_STATE)
  #define SNTP_DEBUG_SERIOUS            (SNTP_DEBUG | LWIP_DBG_LEVEL_SERIOUS)

  #undef __DEBUG_SNTP_CLIENT__

  #define SNTP_ERR_KOD                  1

/* SNTP protocol defines */
  #define SNTP_MSG_LEN                  48

  #define SNTP_OFFSET_LI_VN_MODE        0
  #define SNTP_LI_MASK                  0xC0
  #define SNTP_LI_NO_WARNING            (0x00 << 6)
  #define SNTP_LI_LAST_MINUTE_61_SEC    (0x01 << 6)
  #define SNTP_LI_LAST_MINUTE_59_SEC    (0x02 << 6)
  #define SNTP_LI_ALARM_CONDITION       (0x03 << 6) /* (clock not synchronized) */

  #define SNTP_VERSION_MASK             0x38
  #define SNTP_VERSION                  (4 /* NTP Version 4*/ << 3)

  #define SNTP_MODE_MASK                0x07
  #define SNTP_MODE_CLIENT              0x03
  #define SNTP_MODE_SERVER              0x04
  #define SNTP_MODE_BROADCAST           0x05

  #define SNTP_OFFSET_STRATUM           1
  #define SNTP_STRATUM_KOD              0x00

  #define SNTP_OFFSET_ORIGINATE_TIME    24
  #define SNTP_OFFSET_RECEIVE_TIME      32
  #define SNTP_OFFSET_TRANSMIT_TIME     40

  #define DNS_QUERY_ERROR               1

/* Number of seconds between 1970 and Feb 7, 2036 06:28:16 UTC (epoch 1) */
  #define DIFF_SEC_1970_2036            ((u32_t) 2085978496L)

/** Convert NTP timestamp fraction to microseconds.
 */
  #ifndef SNTP_FRAC_TO_US
   #if LWIP_HAVE_INT64
    #define SNTP_FRAC_TO_US(f)    ((u32_t) (((u64_t) (f) * 1000000UL) >> 32))
   #else
    #define SNTP_FRAC_TO_US(f)    ((u32_t) (f) / 4295)
   #endif
  #endif                               /* !SNTP_FRAC_TO_US */

/* Configure behaviour depending on native, microsecond or second precision.
 * Treat NTP timestamps as signed two's-complement integers. This way,
 * timestamps that have the MSB set simply become negative offsets from
 * the epoch (Feb 7, 2036 06:28:16 UTC). Representable dates range from
 * 1968 to 2104.
 */
  #ifndef SNTP_SET_SYSTEM_TIME_NTP
   #ifdef SNTP_SET_SYSTEM_TIME_US
    #define SNTP_SET_SYSTEM_TIME_NTP(s, f) \
    SNTP_SET_SYSTEM_TIME_US((u32_t) ((s) + DIFF_SEC_1970_2036), SNTP_FRAC_TO_US(f))
   #else
    #define SNTP_SET_SYSTEM_TIME_NTP(s, f) \
    SNTP_SET_SYSTEM_TIME((u32_t) ((s) + DIFF_SEC_1970_2036))
   #endif
  #endif                               /* !SNTP_SET_SYSTEM_TIME_NTP */

/* Get the system time either natively as NTP timestamp or convert from
 * Unix time in seconds and microseconds. Take care to avoid overflow if the
 * microsecond value is at the maximum of 999999. Also add 0.5 us fudge to
 * avoid special values like 0, and to mask round-off errors that would
 * otherwise break round-trip conversion identity.
 */
  #ifndef SNTP_GET_SYSTEM_TIME_NTP
   #define SNTP_GET_SYSTEM_TIME_NTP(s, f)    do {           \
        u32_t sec_, usec_;                                  \
        SNTP_GET_SYSTEM_TIME(sec_, usec_);                  \
        (s) = (s32_t) (sec_ - DIFF_SEC_1970_2036);          \
        (f) = usec_ * 4295 - ((usec_ * 2143) >> 16) + 2147; \
} while (0)
  #endif                               /* !SNTP_GET_SYSTEM_TIME_NTP */

/* Start offset of the timestamps to extract from the SNTP packet */
  #define SNTP_OFFSET_TIMESTAMPS \
    (SNTP_OFFSET_TRANSMIT_TIME + 8 - sizeof(struct sntp_timestamps))

/* Round-trip delay arithmetic helpers */
  #if SNTP_COMP_ROUNDTRIP
   #if !LWIP_HAVE_INT64
    #error "SNTP round-trip delay compensation requires 64-bit arithmetic"
   #endif
   #define SNTP_SEC_FRAC_TO_S64(s, f) \
    ((s64_t) (((u64_t) (s) << 32) | (u32_t) (f)))
   #define SNTP_TIMESTAMP_TO_S64(t) \
    SNTP_SEC_FRAC_TO_S64(lwip_ntohl((t).sec), lwip_ntohl((t).frac))
  #endif                               /* SNTP_COMP_ROUNDTRIP */

/**
 * 64-bit NTP timestamp, in network byte order.
 */
struct sntp_time
{
    u32_t sec;
    u32_t frac;
};

/**
 * Timestamps to be extracted from the NTP header.
 */
struct sntp_timestamps
{
  #if SNTP_COMP_ROUNDTRIP || SNTP_CHECK_RESPONSE >= 2
    struct sntp_time orig;
    struct sntp_time recv;
  #endif
    struct sntp_time xmit;
};

/**
 * SNTP packet format (without optional fields)
 * Timestamps are coded as 64 bits:
 * - signed 32 bits seconds since Feb 07, 2036, 06:28:16 UTC (epoch 1)
 * - unsigned 32 bits seconds fraction (2^32 = 1 second)
 */
  #ifdef PACK_STRUCT_USE_INCLUDES
   #include "arch/bpstruct.h"
  #endif
PACK_STRUCT_BEGIN
struct sntp_msg
{
    PACK_STRUCT_FLD_8(u8_t li_vn_mode);
    PACK_STRUCT_FLD_8(u8_t stratum);
    PACK_STRUCT_FLD_8(u8_t poll);
    PACK_STRUCT_FLD_8(u8_t precision);
    PACK_STRUCT_FIELD(u32_t root_delay);
    PACK_STRUCT_FIELD(u32_t root_dispersion);
    PACK_STRUCT_FIELD(u32_t reference_identifier);
    PACK_STRUCT_FIELD(u32_t reference_timestamp[2]);
    PACK_STRUCT_FIELD(u32_t originate_timestamp[2]);
    PACK_STRUCT_FIELD(u32_t receive_timestamp[2]);
    PACK_STRUCT_FIELD(u32_t transmit_timestamp[2]);
} PACK_STRUCT_STRUCT;
PACK_STRUCT_END
  #ifdef PACK_STRUCT_USE_INCLUDES
   #include "arch/epstruct.h"
  #endif

/* function prototypes */
static void sntp_request(void * arg);
static void sntp_retry(void * arg);

/** The operating mode */
static u8_t sntp_opmode;

/** The UDP pcb used by the SNTP client */
static struct udp_pcb * sntp_pcb = NULL;

/** Names/Addresses of servers */
struct sntp_server
{
  #if SNTP_SERVER_DNS
    const char * name;
  #endif                               /* SNTP_SERVER_DNS */
    ip_addr_t addr;
  #if SNTP_MONITOR_SERVER_REACHABILITY

    /** Reachability shift register as described in RFC 5905 */
    u8_t reachability;
  #endif                               /* SNTP_MONITOR_SERVER_REACHABILITY */
};
static struct sntp_server sntp_servers[SNTP_MAX_SERVERS];

char sntp_names[3][32] = {0, };
int  sntp_name_idx;

  #define SNTP_REQ_COUNT_MAX    6
char    sntp_addr_str[32] = {0, };
uint8_t sntp_req_count;

  #if SNTP_GET_SERVERS_FROM_DHCP
static u8_t sntp_set_servers_from_dhcp;
  #endif                               /* SNTP_GET_SERVERS_FROM_DHCP */
  #if SNTP_SUPPORT_MULTIPLE_SERVERS

/** The currently used server (initialized to 0) */
static u8_t sntp_current_server;
  #else                                /* SNTP_SUPPORT_MULTIPLE_SERVERS */
   #define sntp_current_server    0
  #endif /* SNTP_SUPPORT_MULTIPLE_SERVERS */

  #if SNTP_RETRY_TIMEOUT_EXP
   #define SNTP_RESET_RETRY_TIMEOUT()    sntp_retry_timeout = SNTP_RETRY_TIMEOUT

/** Retry time, initialized with SNTP_RETRY_TIMEOUT and doubled with each retry. */
static u32_t sntp_retry_timeout;
  #else                                /* SNTP_RETRY_TIMEOUT_EXP */
   #define SNTP_RESET_RETRY_TIMEOUT()
   #define sntp_retry_timeout    SNTP_RETRY_TIMEOUT
  #endif /* SNTP_RETRY_TIMEOUT_EXP */

  #if SNTP_CHECK_RESPONSE >= 1

/** Saves the last server address to compare with response */
static ip_addr_t sntp_last_server_address;
  #endif                               /* SNTP_CHECK_RESPONSE >= 1 */

  #if SNTP_CHECK_RESPONSE >= 2

/** Saves the last timestamp sent (which is sent back by the server)
 * to compare against in response. Stored in network byte order. */
static struct sntp_time sntp_last_timestamp_sent;
  #endif                               /* SNTP_CHECK_RESPONSE >= 2 */

static sntp_cmd_param * sntp_param         = NULL;
static TaskHandle_t     sntpTaskHandle     = NULL;
static u32_t            sntp_update_period = 0;

  #define SNTP_CLIENT_MIN_UNICAST_POLL_INTERVAL    60          /*sec*/
  #define SNTP_CLIENT_MAX_UNICAST_POLL_INTERVAL    (36 * 3600) /*sec*/

extern int get_run_mode(void);

/* 2208988800 1970 - 1900 in seconds */
  #define JAN_1970                                 0x83aa7e80
unsigned int sntp_sync_flag = pdFALSE;

static void print_time_stamp (void)
{
  #if CFG_RTC_W
    rtc_w_instance_ctrl_t * rtc_w_ctrl = R_RTC_W_GetCtrl();
    struct tm               ts;
    char buf[32];

    if (rtc_w_ctrl != NULL)
    {
        R_RTC_W_CalendarTimeGet(rtc_w_ctrl, &ts);
        R_RTC_W_Time2Str(rtc_w_ctrl, &ts, buf, sizeof(buf), "%Y-%m-%d %H:%M:%S");
    }
    else
    {
        snprintf(buf, sizeof(buf), "RTC-NA");
    }
    printf(CYAN_COLOR "\n>>> SNTP Time sync : %s\n" CLEAR_COLOR, buf);
  #endif                               /* CFG_RTC_W */
}

static void sntp_set_with_server_time (u32_t * sec, u32_t * frac)
{
    unsigned long milliseconds;
    __time64_t    corrtime;

    corrtime     = (__time64_t) (*sec);
    milliseconds = (unsigned long) (*frac) + 10; /* 1ms sleep offset */

    /* set system time */
    corrtime = (corrtime - JAN_1970) + 1;        /* seconds: current time - 1900 + 1 sec */

    if (milliseconds <= 1000)
    {
        vTaskDelay((1000 - milliseconds) / 10);
    }

    sntp_calendar_system_time_set(&corrtime);

    print_time_stamp();

    sntp_sync_flag = pdTRUE;
  #if CFG_PMGR
    if (RM_PMGR_W_dpm_is_enabled())    /* Enable to enter DPM SLEEP */
    {
        unsigned long sntp_period = 0;
   #ifdef __DEBUG_SNTP_CLIENT__
        printf("[%s]" CYAN_COLOR " SET_DPM %s \n" CLEAR_COLOR, __func__, NET_SNTP);
   #endif                              /* __DEBUG_SNTP_CLIENT__ */

        RM_PMGR_W_rtm_static_get(RTM_STATIC_KEY_SNTP_PERIOD, NULL, (unsigned long long *) &sntp_period, NULL);

        RM_PMGR_W_rtm_static_set(RTM_STATIC_KEY_SNTP_TIMEOUT,
                                 (ULONG) ((R_BSP_SystemRtcCountGet() >> 15) +
                                          sntp_period),
                                 0);   /* update timeout */
        RM_PMGR_W_dpm_sleep_ready_set(NET_SNTP);

        return;
    }
  #endif                               /* CFG_PMGR */
}

// #ifdef PREVENT_FOR_NEWLIB_NANO_PROBLEM   /* del by sjlee 20200908 */

/**
 * Warning!
 * When this code is included,
 * the ctime function of newlib-nano is linked
 * and an exception occurs in the rand() function.
 * - I don't know why -
 *
 * Solved by removing newlib-nano from eclipse linker option. However, the image size is increased by 50K.
 **/
  #if defined(LWIP_DEBUG) && !defined(sntp_format_time)

/* Debug print helper. */
static const char * sntp_format_time (s32_t sec)
{
    time_t ut;
    ut = (u32_t) ((u32_t) sec + DIFF_SEC_1970_2036);

    return ctime(&ut);
}

  #endif                               /* LWIP_DEBUG && !sntp_format_time */

// #endif   /* PREVENT_FOR_NEWLIB_NANO_PROBLEM */

/**
 * SNTP processing of received timestamp
 */
static void sntp_process (const struct sntp_timestamps * timestamps)
{
    // s32_t sec;
    u32_t sec;
    u32_t frac;

    // sec  = (s32_t)lwip_ntohl(timestamps->xmit.sec);
    sec  = lwip_ntohl(timestamps->xmit.sec);
    frac = lwip_ntohl(timestamps->xmit.frac);

  #if SNTP_COMP_ROUNDTRIP
   #if SNTP_CHECK_RESPONSE >= 2
    if ((timestamps->recv.sec != 0) || (timestamps->recv.frac != 0))
   #endif
    {
        s32_t dest_sec;
        u32_t dest_frac;
        u32_t step_sec;

        /* Get the destination time stamp, i.e. the current system time */
        SNTP_GET_SYSTEM_TIME_NTP(dest_sec, dest_frac);

        step_sec = (dest_sec < sec) ? ((u32_t) sec - (u32_t) dest_sec) :
                   ((u32_t) dest_sec - (u32_t) sec);

        /* In order to avoid overflows, skip the compensation if the clock step
         * is larger than about 34 years. */
        if ((step_sec >> 30) == 0)
        {
            s64_t t1, t2, t3, t4;

            t4 = SNTP_SEC_FRAC_TO_S64(dest_sec, dest_frac);
            t3 = SNTP_SEC_FRAC_TO_S64(sec, frac);
            t1 = SNTP_TIMESTAMP_TO_S64(timestamps->orig);
            t2 = SNTP_TIMESTAMP_TO_S64(timestamps->recv);

            /* Clock offset calculation according to RFC 4330 */
            t4 += ((t2 - t1) + (t3 - t4)) / 2;

            sec  = (s32_t) ((u64_t) t4 >> 32);
            frac = (u32_t) ((u64_t) t4);
        }
    }
  #endif                               /* SNTP_COMP_ROUNDTRIP */

    SNTP_SET_SYSTEM_TIME_NTP(sec, frac);
    LWIP_UNUSED_ARG(frac);             /* might be unused if only seconds are set */

    sntp_set_with_server_time(&sec, &frac);

// #ifdef PREVENT_FOR_NEWLIB_NANO_PROBLEM   /* del by sjlee 20200908 */
    LWIP_DEBUGF(SNTP_DEBUG_TRACE, ("sntp_process: %s, %" U32_F " us\n", sntp_format_time(sec), SNTP_FRAC_TO_US(frac)));

// #endif   /* PREVENT_FOR_NEWLIB_NANO_PROBLEM */
}

/**
 * Initialize request struct to be sent to server.
 */
static void sntp_initialize_request (struct sntp_msg * req)
{
    memset(req, 0, SNTP_MSG_LEN);
    req->li_vn_mode = SNTP_LI_NO_WARNING | SNTP_VERSION | SNTP_MODE_CLIENT;

  #if SNTP_CHECK_RESPONSE >= 2 || SNTP_COMP_ROUNDTRIP
    {
        s32_t secs;
        u32_t sec, frac;

        /* Get the transmit timestamp */
        SNTP_GET_SYSTEM_TIME_NTP(secs, frac);
        sec  = lwip_htonl((u32_t) secs);
        frac = lwip_htonl(frac);

   #if SNTP_CHECK_RESPONSE >= 2
        sntp_last_timestamp_sent.sec  = sec;
        sntp_last_timestamp_sent.frac = frac;
   #endif
        req->transmit_timestamp[0] = sec;
        req->transmit_timestamp[1] = frac;
    }
  #endif                               /* SNTP_CHECK_RESPONSE >= 2 || SNTP_COMP_ROUNDTRIP */
}

  #if SNTP_SUPPORT_MULTIPLE_SERVERS

/**
 * If Kiss-of-Death is received (or another packet parsing error),
 * try the next server or retry the current server and increase the retry
 * timeout if only one server is available.
 * (implicitly, SNTP_MAX_SERVERS > 1)
 *
 * @param arg is unused (only necessary to conform to sys_timeout)
 */
static void sntp_try_next_server (void * arg)
{
    u8_t old_server;
   #if SNTP_MAX_SERVERS > 1
    u8_t i;
   #endif                              /* SNTP_MAX_SERVERS > 1 */

    LWIP_UNUSED_ARG(arg);

    old_server = sntp_current_server;
   #if SNTP_MAX_SERVERS > 1
    for (i = 0; i < SNTP_MAX_SERVERS - 1; i++)
    {
        sntp_current_server++;
        if (sntp_current_server >= SNTP_MAX_SERVERS)
        {
            sntp_current_server = 0;
        }

        if (!ip_addr_isany(&sntp_servers[sntp_current_server].addr)
    #if SNTP_SERVER_DNS
            || (sntp_servers[sntp_current_server].name != NULL)
    #endif                             // SNTP_SERVER_DNS
            )
        {
            LWIP_DEBUGF(SNTP_DEBUG_STATE,
                        ("sntp_try_next_server: Sending request to server %"U16_F "\n",
                         (u16_t) sntp_current_server));

            /* new server: reset retry timeout */
            SNTP_RESET_RETRY_TIMEOUT();

            /* instantly send a request to the next server */
            sntp_request(NULL);

            return;
        }
    }
   #endif                              /* SNTP_MAX_SERVERS > 1 */
    /* no other valid server found */
    sntp_current_server = old_server;
    sntp_retry(NULL);
}

  #else                                /* SNTP_SUPPORT_MULTIPLE_SERVERS */
/* Always retry on error if only one server is supported */
   #define sntp_try_next_server    sntp_retry
  #endif /* SNTP_SUPPORT_MULTIPLE_SERVERS */

/** UDP recv callback for the sntp pcb */
static void sntp_recv (void * arg, struct udp_pcb * pcb, struct pbuf * p, const ip_addr_t * addr, u16_t port)
{
    struct sntp_timestamps timestamps;
    u8_t  mode;
    u8_t  stratum;
    err_t err;

    LWIP_UNUSED_ARG(arg);
    LWIP_UNUSED_ARG(pcb);

    err = ERR_ARG;
  #if SNTP_CHECK_RESPONSE >= 1

    /* check server address and port */
    if (((sntp_opmode != SNTP_OPMODE_POLL) || ip_addr_cmp(addr, &sntp_last_server_address)) &&
        (port == SNTP_PORT))
  #else                                /* SNTP_CHECK_RESPONSE >= 1 */
    LWIP_UNUSED_ARG(addr);
    LWIP_UNUSED_ARG(port);
  #endif /* SNTP_CHECK_RESPONSE >= 1 */
    {
        /* process the response */
        if (p->tot_len == SNTP_MSG_LEN)
        {
            mode = pbuf_get_at(p, SNTP_OFFSET_LI_VN_MODE) & SNTP_MODE_MASK;

            /* if this is a SNTP response... */
            if (((sntp_opmode == SNTP_OPMODE_POLL) && (mode == SNTP_MODE_SERVER)) ||
                ((sntp_opmode == SNTP_OPMODE_LISTENONLY) && (mode == SNTP_MODE_BROADCAST)))
            {
                stratum = pbuf_get_at(p, SNTP_OFFSET_STRATUM);

                if (stratum == SNTP_STRATUM_KOD)
                {
                    /* Kiss-of-death packet. Use another server or increase UPDATE_DELAY. */
                    err = SNTP_ERR_KOD;
                    LWIP_DEBUGF(SNTP_DEBUG_STATE, ("sntp_recv: Received Kiss-of-Death\n"));
                }
                else
                {
                    pbuf_copy_partial(p, &timestamps, sizeof(timestamps), SNTP_OFFSET_TIMESTAMPS);
  #if SNTP_CHECK_RESPONSE >= 2

                    /* check originate_timetamp against sntp_last_timestamp_sent */
                    if ((timestamps.orig.sec != sntp_last_timestamp_sent.sec) ||
                        (timestamps.orig.frac != sntp_last_timestamp_sent.frac))
                    {
                        LWIP_DEBUGF(SNTP_DEBUG_WARN, ("sntp_recv: Invalid originate timestamp in response\n"));
                    }
                    else
  #endif                               /* SNTP_CHECK_RESPONSE >= 2 */
                    /* @todo: add code for SNTP_CHECK_RESPONSE >= 3 and >= 4 here */
                    {
                        /* correct answer */
                        err = ERR_OK;
                    }
                }
            }
            else
            {
                LWIP_DEBUGF(SNTP_DEBUG_WARN, ("sntp_recv: Invalid mode in response: %"U16_F "\n", (u16_t) mode));

                /* wait for correct response */
                err = ERR_TIMEOUT;
            }
        }
        else
        {
            LWIP_DEBUGF(SNTP_DEBUG_WARN, ("sntp_recv: Invalid packet length: %"U16_F "\n", p->tot_len));
        }
    }

  #if SNTP_CHECK_RESPONSE >= 1
    else
    {
        /* packet from wrong remote address or port, wait for correct response */
        err = ERR_TIMEOUT;
    }
  #endif                               /* SNTP_CHECK_RESPONSE >= 1 */

    pbuf_free(p);

    if (err == ERR_OK)
    {
        /* correct packet received: process it it */
        sntp_process(&timestamps);

  #if SNTP_MONITOR_SERVER_REACHABILITY

        /* indicate that server responded */
        sntp_servers[sntp_current_server].reachability |= 1;
  #endif                               /* SNTP_MONITOR_SERVER_REACHABILITY */
        /* Set up timeout for next request (only if poll response was received)*/
        if (sntp_opmode == SNTP_OPMODE_POLL)
        {
            u32_t sntp_update_delay;
            sys_untimeout(sntp_try_next_server, NULL);
            sys_untimeout(sntp_request, NULL);

            /* Correct response, reset retry timeout */
            SNTP_RESET_RETRY_TIMEOUT();
            sntp_req_count = 0;

            if (sntp_update_period > 0)
            {
                sntp_update_delay = sntp_update_period;
            }
            else
            {
                sntp_update_delay = (u32_t) SNTP_UPDATE_DELAY;
            }

            sys_timeout(sntp_update_delay, sntp_request, NULL);

            LWIP_DEBUGF(SNTP_DEBUG_STATE,
                        ("sntp_recv: Scheduled next time request: %"U32_F " sec\n",
                         sntp_update_delay / 1000));
        }
    }
    else if (err == SNTP_ERR_KOD)
    {
        sntp_sync_flag = pdFALSE;

        /* KOD errors are only processed in case of an explicit poll response */
        if (sntp_opmode == SNTP_OPMODE_POLL)
        {
            /* Kiss-of-death packet. Use another server or increase UPDATE_DELAY. */
            sntp_try_next_server(NULL);
        }
    }
    else
    {
        sntp_sync_flag = pdFALSE;

        /* ignore any broken packet, poll mode: retry after timeout to avoid flooding */
    }
}

void sntp_sync (void)
{
    sys_untimeout(sntp_request, NULL);
    sys_timeout(0, sntp_request, NULL);
}

/**
 * Retry: send a new request (and increase retry timeout).
 *
 * @param arg is unused (only necessary to conform to sys_timeout)
 */
static void sntp_retry (void * arg)
{
    LWIP_UNUSED_ARG(arg);

    LWIP_DEBUGF(SNTP_DEBUG_STATE,
                ("sntp_retry: Next request will be sent in %"U32_F " ms\n",
                 (u32_t) sntp_retry_timeout));

    sntp_req_count++;

    if (sntp_req_count >= SNTP_REQ_COUNT_MAX)
    {
        u32_t sntp_update_delay;

        printf(CYAN_COLOR "Failed to sync SNTP\n" CLEAR_COLOR);

        sys_untimeout(sntp_try_next_server, NULL);
        sys_untimeout(sntp_request, NULL);

        /* Correct response, reset retry timeout */
        SNTP_RESET_RETRY_TIMEOUT();

        if (sntp_update_period > 0)
        {
            sntp_update_delay = sntp_update_period;
        }
        else
        {
            sntp_update_delay = (u32_t) SNTP_UPDATE_DELAY;
        }

        sntp_req_count = 0;
        sys_timeout(sntp_update_delay, sntp_request, arg);

        LWIP_DEBUGF(SNTP_DEBUG_STATE,
                    ("sntp_retry: Scheduled next time request: %"U32_F " sec\n",
                     sntp_update_delay / 1000));
  #if CFG_PMGR
        if (RM_PMGR_W_dpm_is_enabled())
        {
            unsigned long sntp_period = 0;
            RM_PMGR_W_rtm_static_get(RTM_STATIC_KEY_SNTP_PERIOD, NULL, (unsigned long long *) &sntp_period, NULL);

            RM_PMGR_W_rtm_static_set(RTM_STATIC_KEY_SNTP_TIMEOUT,
                                     (ULONG) ((R_BSP_SystemRtcCountGet() >> 15) +
                                              sntp_period),
                                     0); /* update timeout */

            RM_PMGR_W_dpm_sleep_ready_set(NET_SNTP);
        }
  #endif /* CFG_PMGR */

        return;
    }
    else
    {
        char * addr = NULL;

        memset(sntp_addr_str, 0, 32);

        if (sntp_req_count % MAX_SNTP_SERVER == 1)
        {
            // 2nd
            addr = sntp_get_server(SNTP_SVR_1);
        }
        else if (sntp_req_count % MAX_SNTP_SERVER == 2)
        {
            // 3rd
            addr = sntp_get_server(SNTP_SVR_2);
        }
        else
        {
            // 1st
            addr = sntp_get_server(SNTP_SVR_0);
        }

        if (addr)
        {
            strncpy(sntp_addr_str, addr, 32);
            OS_FREE(addr);
        }
    }

    sntp_setservername(0, sntp_addr_str);

    /* set up a timer to send a retry and increase the retry delay */
    if (arg == (void *) DNS_QUERY_ERROR)
    {
        sys_timeout(0, sntp_request, arg);
    }
    else
    {
        sys_timeout(0, sntp_request, NULL);
    }

  #if SNTP_RETRY_TIMEOUT_EXP
    {
        u32_t new_retry_timeout;

        /* increase the timeout for next retry */
        new_retry_timeout = sntp_retry_timeout << 1;

        /* limit to maximum timeout and prevent overflow */
        if ((new_retry_timeout <= SNTP_RETRY_TIMEOUT_MAX) &&
            (new_retry_timeout > sntp_retry_timeout))
        {
            sntp_retry_timeout = new_retry_timeout;
        }
        else
        {
            sntp_retry_timeout = SNTP_RETRY_TIMEOUT_MAX;
        }
    }
  #endif                               /* SNTP_RETRY_TIMEOUT_EXP */
}

/** Actually send an sntp request to a server.
 *
 * @param server_addr resolved IP address of the SNTP server
 */
static void sntp_send_request (const ip_addr_t * server_addr)
{
    struct pbuf * p;

    LWIP_ASSERT("server_addr != NULL", server_addr != NULL);

    p = pbuf_alloc(PBUF_TRANSPORT, SNTP_MSG_LEN, PBUF_RAM);
    if (p != NULL)
    {
  #if !defined(__LIGHT_SUPPLICANT__)
        if (get_run_mode() == WIFI_DEVICE_MODE_EXT_AP_STATION)
        {
            p->if_idx = RM_WIFI_NETIF_INDEX(WLAN0_IFACE);
        }
  #endif                               // __LIGHT_SUPPLICANT__

        struct sntp_msg * sntpmsg = (struct sntp_msg *) p->payload;
        LWIP_DEBUGF(SNTP_DEBUG_STATE, ("sntp_send_request: Sending request to server\n"));

        /* initialize request message */
        sntp_initialize_request(sntpmsg);

        /* send request */
        udp_sendto(sntp_pcb, p, server_addr, SNTP_PORT);

        /* free the pbuf after sending it */
        pbuf_free(p);
  #if SNTP_MONITOR_SERVER_REACHABILITY

        /* indicate new packet has been sent */
        sntp_servers[sntp_current_server].reachability <<= 1;
  #endif                               /* SNTP_MONITOR_SERVER_REACHABILITY */
        /* set up receive timeout: try next server or retry on timeout */
        sys_untimeout(sntp_try_next_server, NULL);
        sys_timeout((u32_t) SNTP_RECV_TIMEOUT, sntp_try_next_server, NULL);
  #if SNTP_CHECK_RESPONSE >= 1

        /* save server address to verify it in sntp_recv */
        ip_addr_copy(sntp_last_server_address, *server_addr);
  #endif                               /* SNTP_CHECK_RESPONSE >= 1 */
    }
    else
    {
        LWIP_DEBUGF(SNTP_DEBUG_SERIOUS,
                    ("sntp_send_request: Out of memory, trying again in %"U32_F " ms\n",
                     (u32_t) SNTP_RETRY_TIMEOUT));

        /* out of memory: set up a timer to send a retry */
        sys_untimeout(sntp_request, NULL);
        sys_timeout((u32_t) SNTP_RETRY_TIMEOUT, sntp_request, NULL);
    }
}

  #if SNTP_SERVER_DNS

/**
 * DNS found callback when using DNS names as server address.
 */
static void sntp_dns_found (const char * hostname, const ip_addr_t * ipaddr, void * arg)
{
    LWIP_UNUSED_ARG(hostname);
    LWIP_UNUSED_ARG(arg);

    if (ipaddr != NULL)
    {
        /* Address resolved, send request */
        LWIP_DEBUGF(SNTP_DEBUG_STATE, ("sntp_dns_found: Server address resolved, sending request\n"));
        sntp_servers[sntp_current_server].addr = *ipaddr;

        printf(">>> SNTP Server: %s (%s)\r\n", sntp_servers[sntp_current_server].name,
               ipaddr_ntoa(&(sntp_servers[sntp_current_server].addr)));

        sntp_send_request(ipaddr);
    }
    else
    {
        if (sntp_pcb != NULL)
        {
            /* DNS resolving failed -> try another server */
            LWIP_DEBUGF(SNTP_DEBUG_WARN_STATE,
                        ("sntp_dns_found: Failed to resolve server address resolved, trying next server\n"));

            printf("\n>>> DNS query error.\n");
            printf(">>> SNTP Server: %s ()\n", sntp_servers[sntp_current_server].name);

            sntp_try_next_server((void *) DNS_QUERY_ERROR);
        }
    }
}

  #endif                               /* SNTP_SERVER_DNS */

/**
 * Send out an sntp request.
 *
 * @param arg is unused (only necessary to conform to sys_timeout)
 */
static void sntp_request (void * arg)
{
    ip_addr_t sntp_server_address;
    err_t     err;

    LWIP_UNUSED_ARG(arg);

    /* initialize SNTP server address */
  #if SNTP_SERVER_DNS
    if (sntp_servers[sntp_current_server].name)
    {
        /* always resolve the name and rely on dns-internal caching & timeout */
        ip_addr_set_zero(&sntp_servers[sntp_current_server].addr);

        err = dns_gethostbyname(sntp_servers[sntp_current_server].name, &sntp_server_address, sntp_dns_found, arg);

        if (err == ERR_INPROGRESS)
        {
            /* DNS request sent, wait for sntp_dns_found being called */
            LWIP_DEBUGF(SNTP_DEBUG_STATE, ("sntp_request: Waiting for server address to be resolved.\n"));

            return;
        }
        else if (err == ERR_OK)
        {
            sntp_servers[sntp_current_server].addr = sntp_server_address;

            printf("\n>>> SNTP Server: %s (%s)\n", sntp_servers[sntp_current_server].name,
                   ipaddr_ntoa(&(sntp_server_address)));
        }
    }
    else
  #endif                               /* SNTP_SERVER_DNS */
    {
        sntp_server_address = sntp_servers[sntp_current_server].addr;

        printf("\n>>> SNTP Server: (%s)\n", ipaddr_ntoa(&(sntp_servers[sntp_current_server].addr)));

        err = (ip_addr_isany_val(sntp_server_address)) ? ERR_ARG : ERR_OK;
    }

    printf("Retry: SNTP Client - %d\n", sntp_req_count + 1);

    // DNS Query OK or IP Address OK

    if (err == ERR_OK)
    {
        LWIP_DEBUGF(SNTP_DEBUG_TRACE,
                    ("sntp_request: current server address is %s\n",
                     ipaddr_ntoa(&sntp_server_address)));

        sntp_send_request(&sntp_server_address);
    }
    else
    {
        /* address conversion failed, try another server */
        LWIP_DEBUGF(SNTP_DEBUG_WARN_STATE, ("sntp_request: Invalid server address, trying next server.\n"));
        sys_untimeout(sntp_try_next_server, NULL);
        sys_timeout((u32_t) SNTP_RETRY_TIMEOUT, sntp_try_next_server, NULL);
    }
}

/**
 * @ingroup sntp
 * Initialize this module.
 * Send out request instantly or after SNTP_STARTUP_DELAY(_FUNC).
 */
void sntp_init (void)
{
    char * sntp_server;
    char * addr;
    u32_t  period = 0;

    /* LWIP_ASSERT_CORE_LOCKED(); is checked by udp_new() */

    addr = sntp_get_server((sntp_svr_t) sntp_name_idx);

    strncpy(sntp_names[sntp_name_idx], addr, 32);
    OS_FREE(addr);
    sntp_server = sntp_names[sntp_name_idx];

    period = (u32_t) sntp_get_period();

    if (period > 0)
    {
        sntp_update_period = period * 1000;
    }
    else
    {
        sntp_update_period = SNTP_UPDATE_DELAY;
    }

    if (sntp_server == NULL)
    {
        if (sntp_name_idx == 1)
        {
            sntp_server = SNTP_SERVER_ADDRESS_1;
        }
        else if (sntp_name_idx == 2)
        {
            sntp_server = SNTP_SERVER_ADDRESS_2;
        }
        else
        {
            sntp_server = SNTP_SERVER_ADDRESS;
        }
    }

    sntp_name_idx = 0;

  #ifdef SNTP_SERVER_ADDRESS
   #if SNTP_SERVER_DNS
    sntp_setservername(0, sntp_server);
   #else
    #error SNTP_SERVER_ADDRESS string not supported SNTP_SERVER_DNS==0
   #endif
  #else
    ip4_addr_t sntp_server_ipaddr;
    ip4addr_aton(sntp_server, &sntp_server_ipaddr);
    sntp_setserver(0, &sntp_server_ipaddr);
  #endif                               /* SNTP_SERVER_ADDRESS */

    if (sntp_pcb == NULL)
    {
        sntp_pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
        LWIP_ASSERT("Failed to allocate udp pcb for sntp client", sntp_pcb != NULL);

        if (sntp_pcb != NULL)
        {
            if (get_run_mode() == WIFI_DEVICE_MODE_EXT_AP_STATION)
            {
                sntp_pcb->netif_idx = RM_WIFI_NETIF_INDEX(WLAN0_IFACE);
            }

            udp_recv(sntp_pcb, sntp_recv, NULL);

            if (sntp_opmode == SNTP_OPMODE_POLL)
            {
                SNTP_RESET_RETRY_TIMEOUT();
                sntp_request(NULL);
            }
            else if (sntp_opmode == SNTP_OPMODE_LISTENONLY)
            {
                ip_set_option(sntp_pcb, SOF_BROADCAST);
                udp_bind(sntp_pcb, IP_ANY_TYPE, SNTP_PORT);
            }
        }
    }
}

/**
 * @ingroup sntp
 * Stop this module.
 */
void sntp_stop (void)
{
    LWIP_ASSERT_CORE_LOCKED();
    if (sntp_pcb != NULL)
    {
  #if SNTP_MONITOR_SERVER_REACHABILITY
        u8_t i;
        for (i = 0; i < SNTP_MAX_SERVERS; i++)
        {
            sntp_servers[i].reachability = 0;
        }
  #endif                               /* SNTP_MONITOR_SERVER_REACHABILITY */
        sys_untimeout(sntp_request, NULL);
        sys_untimeout(sntp_try_next_server, NULL);
        udp_remove(sntp_pcb);
        sntp_pcb = NULL;
    }
}

/**
 * @ingroup sntp
 * Get enabled state.
 */
u8_t sntp_enabled (void)
{
    return (sntp_pcb != NULL) ? 1 : 0;
}

/**
 * @ingroup sntp
 * Sets the operating mode.
 * @param operating_mode one of the available operating modes
 */
void sntp_setoperatingmode (u8_t operating_mode)
{
    LWIP_ASSERT_CORE_LOCKED();
    LWIP_ASSERT("Invalid operating mode", operating_mode <= SNTP_OPMODE_LISTENONLY);
    LWIP_ASSERT("Operating mode must not be set while SNTP client is running", sntp_pcb == NULL);
    sntp_opmode = operating_mode;
}

/**
 * @ingroup sntp
 * Gets the operating mode.
 */
u8_t sntp_getoperatingmode (void)
{
    return sntp_opmode;
}

  #if SNTP_MONITOR_SERVER_REACHABILITY

/**
 * @ingroup sntp
 * Gets the server reachability shift register as described in RFC 5905.
 *
 * @param idx the index of the NTP server
 */
u8_t sntp_getreachability (u8_t idx)
{
    if (idx < SNTP_MAX_SERVERS)
    {
        return sntp_servers[idx].reachability;
    }

    return 0;
}

  #endif                               /* SNTP_MONITOR_SERVER_REACHABILITY */

  #if SNTP_GET_SERVERS_FROM_DHCP

/**
 * Config SNTP server handling by IP address, name, or DHCP; clear table
 * @param set_servers_from_dhcp enable or disable getting server addresses from dhcp
 */
void sntp_servermode_dhcp (int set_servers_from_dhcp)
{
    u8_t new_mode = set_servers_from_dhcp ? 1 : 0;
    LWIP_ASSERT_CORE_LOCKED();
    if (sntp_set_servers_from_dhcp != new_mode)
    {
        sntp_set_servers_from_dhcp = new_mode;
    }
}

  #endif                               /* SNTP_GET_SERVERS_FROM_DHCP */

/**
 * @ingroup sntp
 * Initialize one of the NTP servers by IP address
 *
 * @param idx the index of the NTP server to set must be < SNTP_MAX_SERVERS
 * @param server IP address of the NTP server to set
 */
void sntp_setserver (u8_t idx, const ip_addr_t * server)
{
    LWIP_ASSERT_CORE_LOCKED();
    if (idx < SNTP_MAX_SERVERS)
    {
        if (server != NULL)
        {
            sntp_servers[idx].addr = (*server);
        }
        else
        {
            ip_addr_set_zero(&sntp_servers[idx].addr);
        }

  #if SNTP_SERVER_DNS
        sntp_servers[idx].name = NULL;
  #endif
    }
}

  #if LWIP_DHCP && SNTP_GET_SERVERS_FROM_DHCP

/**
 * Initialize one of the NTP servers by IP address, required by DHCP
 *
 * @param num the index of the NTP server to set must be < SNTP_MAX_SERVERS
 * @param server IP address of the NTP server to set
 */
void dhcp_set_ntp_servers (u8_t num, const ip4_addr_t * server)
{
    LWIP_DEBUGF(SNTP_DEBUG_TRACE,
                "sntp: %s %u.%u.%u.%u as NTP server #%u via DHCP\n",
                ((sntp_set_servers_from_dhcp ? "Got" : "Rejected"),
                 ip4_addr1(server), ip4_addr2(server), ip4_addr3(server), ip4_addr4(server), num));
    if (sntp_set_servers_from_dhcp && num)
    {
        u8_t i;
        for (i = 0; (i < num) && (i < SNTP_MAX_SERVERS); i++)
        {
            ip_addr_t addr;
            ip_addr_copy_from_ip4(addr, server[i]);
            sntp_setserver(i, &addr);
        }

        for (i = num; i < SNTP_MAX_SERVERS; i++)
        {
            sntp_setserver(i, NULL);
        }
    }
}

  #endif                               /* LWIP_DHCP && SNTP_GET_SERVERS_FROM_DHCP */

/**
 * @ingroup sntp
 * Obtain one of the currently configured by IP address (or DHCP) NTP servers
 *
 * @param idx the index of the NTP server
 * @return IP address of the indexed NTP server or "ip_addr_any" if the NTP
 *         server has not been configured by address (or at all).
 */
const ip_addr_t * sntp_getserver (u8_t idx)
{
    if (idx < SNTP_MAX_SERVERS)
    {
        return &sntp_servers[idx].addr;
    }

    return IP_ADDR_ANY;
}

  #if SNTP_SERVER_DNS

/**
 * Initialize one of the NTP servers by name
 *
 * @param idx the index of the NTP server to set must be < SNTP_MAX_SERVERS
 * @param server DNS name of the NTP server to set, to be resolved at contact time
 */
void sntp_setservername (u8_t idx, const char * server)
{
    LWIP_ASSERT_CORE_LOCKED();
    if (idx < SNTP_MAX_SERVERS)
    {
        sntp_servers[idx].name = server;
    }
}

/**
 * Obtain one of the currently configured by name NTP servers.
 *
 * @param idx the index of the NTP server
 * @return IP address of the indexed NTP server or NULL if the NTP
 *         server has not been configured by name (or at all)
 */
const char * sntp_getservername (u8_t idx)
{
    if (idx < SNTP_MAX_SERVERS)
    {
        return sntp_servers[idx].name;
    }

    return NULL;
}

  #endif                               /* SNTP_SERVER_DNS */

extern int netmode[2];

void sntp_thread (void * arg)
{
    LWIP_UNUSED_ARG(arg);

    if (netmode[WLAN0_IFACE] == DHCPCLIENT)
    {
        // wait until network is available
  #if CFG_PMGR
        if (RM_PMGR_W_dpm_is_wakeup() == pdTRUE)
        {
            if (ra6w1_network_main_check_dhcp_state(WLAN0_IFACE) != DHCP_STATE_OFF)
            {
                while (ra6w1_network_main_check_dhcp_state(WLAN0_IFACE) != DHCP_STATE_BOUND)
                {
                    vTaskDelay(portCONVERT_MS_2_TICKS(1000));
                }
            }
        }
        else
  #endif                               /* CFG_PMGR */
  #if LWIP_IPV4
        {
            while (ra6w1_network_main_check_dhcp_state(WLAN0_IFACE) != DHCP_STATE_BOUND)
            {
                vTaskDelay(portCONVERT_MS_2_TICKS(1000));
            }
        }

  #else                                // LWIP_IPV6
        {
            while (ra6w1_network_main_check_global_unicast_ip6_addr(0) == pdFALSE)
            {
                vTaskDelay(portCONVERT_MS_2_TICKS(1000));
            }
        }
  #endif                               // LWIP_IPV4
    }

    sntp_init();

    vTaskDelete(NULL);
}

void sntp_finsh (void)
{
    sntp_cmd_param * param = pvPortMalloc(sizeof(sntp_cmd_param));
    param->cmd = SNTP_STOP;
    sntp_run(param);
}

void sntp_run (void * arg)
{
  #if CFG_PMGR
    int status = 0;
  #endif                               /* CFG_PMGR */
    UINT use = 0;

    sntp_param = arg;

    extern int get_run_mode(void);

    if (sntp_param == NULL)
    {
        printf("SNTP Parameter is NULL\n");

        return;
    }

    switch (get_run_mode())
    {
        case WIFI_DEVICE_MODE_EXT_STATION:
        case WIFI_DEVICE_MODE_EXT_AP_STATION:
  #if defined(__SUPPORT_P2P__)
        case WIFI_DEVICE_MODE_EXT_P2P_STATION:
  #endif                               // __SUPPORT_P2P__
  #ifdef __SUPPORT_MESH__
        case WIFI_DEVICE_MODE_EXT_MESH_POINT:
        case WIFI_DEVICE_MODE_EXT_MESH_PORTAL:
  #endif                               /* __SUPPORT_MESH__ */
            {
                break;
            }

        default:
            vPortFree(sntp_param);
    }

    switch (sntp_param->cmd)
    {
        case SNTP_START:
        {
  #if CFG_PMGR
            if (RM_PMGR_W_dpm_is_enabled())
            {
                if (RM_PMGR_W_dpm_is_wakeup())
                {
                    RM_PMGR_W_rtm_static_get(RTM_STATIC_KEY_SNTP_USE, NULL, (unsigned long long *) &use, NULL);
                }
                else
                {
                    use = sntp_get_use();
                    RM_PMGR_W_rtm_static_set(RTM_STATIC_KEY_SNTP_USE, 0, use); /* set rtm */
                }

                if ((RM_PMGR_W_dpm_job_name_is_set(NET_SNTP) == DPM_NOT_REGISTERED) && use)
                {
                    status = RM_PMGR_W_dpm_job_name_set(NET_SNTP, 0);
                    if (status == DPM_REG_ERR)
                    {
                        printf("[%s] Failed to register DPM_SNTP\n", __func__);
                    }
                }
            }
            else
  #endif                                                                   /* CFG_PMGR */
            {
                use = sntp_get_use();
  #if CFG_PMGR
                RM_PMGR_W_rtm_static_set(RTM_STATIC_KEY_SNTP_USE, 0, use); /* set rtm */
  #endif                                                                   /* CFG_PMGR */
            }

            if (use)
            {
  #if CFG_PMGR
                unsigned long long systime_offset;
                unsigned long      sntp_timeout;
  #endif                               /* CFG_PMGR */
  #ifdef __DEBUG_SNTP_CLIENT__
   #if CFG_PMGR
                printf(YELLOW_COLOR "SNTP Timeout=%u/%u sec\n" CLEAR_COLOR,
                       (R_BSP_SystemRtcCountGet() >> 15),
                       RM_PMGR_W_rtm_static_get(RTM_STATIC_KEY_SNTP_TIMEOUT, NULL, &sntp_timeout, NULL));
   #endif                              /* CFG_PMGR */
  #endif                               /* __DEBUG_SNTP_CLIENT__ */

  #if CFG_PMGR
                if (RM_PMGR_W_dpm_is_wakeup())
                {
                    RM_PMGR_W_rtm_static_get(RTM_STATIC_KEY_SNTP_TIMEOUT,
                                             NULL,
                                             (unsigned long long *) &sntp_timeout,
                                             NULL);
                    RM_PMGR_W_rtm_static_get(RTM_STATIC_KEY_SYS_TIME_OFFSET, NULL, &systime_offset, NULL);
                    if ((sntp_timeout > (R_BSP_SystemRtcCountGet() >> 15)) && (systime_offset != 0))
                    {
                        RM_PMGR_W_dpm_job_name_clear(NET_SNTP);
                        if (sntp_param != NULL)
                        {
                            vPortFree(sntp_param);
                        }

                        return;
                    }
                    else
                    {
                        RM_PMGR_W_dpm_sleep_ready_clear(NET_SNTP);
   #ifdef __DEBUG_SNTP_CLIENT__
                        printf(YELLOW_COLOR "SNTP Update\n" CLEAR_COLOR);
   #endif                              /* __DEBUG_SNTP_CLIENT__ */
                    }
                }
                else
  #endif                               /* CFG_PMGR */
                {
  #ifdef __DEBUG_SNTP_CLIENT__
                    printf(CYAN_COLOR "SNTP Power On Start. \n" CLEAR_COLOR);
  #endif                               /* __DEBUG_SNTP_CLIENT__ */
  #if CFG_PMGR
                    RM_PMGR_W_dpm_sleep_ready_clear(NET_SNTP);
  #endif                               /* CFG_PMGR */
                }

                if (sntpTaskHandle == NULL)
                {
                    sntpTaskHandle = (TaskHandle_t)sys_thread_new("sntp_thread",
                                                    sntp_thread,
                                                    NULL,
                                                    DEFAULT_THREAD_STACKSIZE,
                                                    DEFAULT_THREAD_PRIO + 4);
                }
            }

            break;
        }

        case SNTP_SYNC:
        {
            sntp_sync();
            break;
        }

        case SNTP_STOP:
        {
            if (sntpTaskHandle != NULL)
            {
                sntp_stop();

                sntpTaskHandle = NULL;
            }

            break;
        }

        case SNTP_STATUS:
        {
            if (sntpTaskHandle != NULL)
            {
                printf("SNTP Status: Enabled\n");
            }
            else
            {
                printf("SNTP Status: Disabled\n");
            }

            {
                char * addr;
                addr = sntp_get_server(SNTP_SVR_0);
                printf("     Server: %s\n", addr);
                OS_FREE(addr);

                addr = sntp_get_server(SNTP_SVR_1);
                printf("     Server_1: %s\n", addr);
                OS_FREE(addr);

                addr = sntp_get_server(SNTP_SVR_2);
                printf("     Server_2: %s\n", addr);
                OS_FREE(addr);

                printf("     Period: %d sec\n", sntp_get_period());
            }
            break;
        }

        case SNTP_SERVER_ADDR:
        {
            sntp_set_server(sntp_param->sntp_svr_addr, SNTP_SVR_0);
            break;
        }

        case SNTP_SERVER_ADDR_1:
        {
            sntp_set_server(sntp_param->sntp_svr_addr, SNTP_SVR_1);
            break;
        }

        case SNTP_SERVER_ADDR_2:
        {
            sntp_set_server(sntp_param->sntp_svr_addr, SNTP_SVR_2);
            break;
        }

        case SNTP_PERIOD:
        {
            sntp_set_period(sntp_param->period);
            break;
        }

        default:
        {
            break;
        }
    }

    if (sntp_param != NULL)
    {
        vPortFree(sntp_param);
    }
}

int sntp_get_period (void)
{
    int value;

  #ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, NVR_KEY_SNTP_SYNC_PERIOD, &value);
  #endif

    if (value == -1)
    {
        return SNTP_UPDATE_DELAY / 1000;
    }

    return value;
}

u8_t sntp_set_period (u32_t period)    /*sec*/
{
    if ((period != 0) && ((period < SNTP_CLIENT_MIN_UNICAST_POLL_INTERVAL) ||
                          (period > SNTP_CLIENT_MAX_UNICAST_POLL_INTERVAL)))
    {
        printf("Error: SNTP period(%d~%d secs) : %ld\n",
               SNTP_CLIENT_MIN_UNICAST_POLL_INTERVAL,
               SNTP_CLIENT_MAX_UNICAST_POLL_INTERVAL,
               period);

        return pdFAIL;
    }

  #ifdef RM_MAP_PERSISTANT_W
    if (RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, NVR_KEY_SNTP_SYNC_PERIOD,
                                      (int) period) != FSP_SUCCESS)
    {
        return pdFAIL;
    }
  #endif

    sntp_update_period = period * 1000; /*milliseconds*/

    if (sntpTaskHandle != NULL)
    {
        sys_untimeout(sntp_request, NULL);
        sys_timeout(sntp_update_period, sntp_request, NULL);
    }

    return pdPASS;
}

u8_t sntp_get_use (void)
{
    int use = 0;

  #ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, NVR_KEY_SNTP_C, &use);
  #endif

    if (use == -1)
    {
        return pdFALSE;
    }

    return (u8_t) use;
}

u8_t sntp_set_use (u8_t use)
{
    if (use == pdFALSE)
    {
  #ifdef RM_MAP_PERSISTANT_W
        if (RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, NVR_KEY_SNTP_C) != FSP_SUCCESS)
        {
            return pdFAIL;
        }
  #endif

        sntp_finsh();
    }
    else
    {
  #ifdef RM_MAP_PERSISTANT_W
        if (RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, NVR_KEY_SNTP_C,
                                          SNTPCLINET_ENABLE) != FSP_SUCCESS)
        {
            return pdFAIL;
        }
  #endif

        sntp_cmd_param * param = pvPortMalloc(sizeof(sntp_cmd_param));
        param->cmd = SNTP_START;
        sntp_run(param);
    }

    return pdPASS;
}

void sntp_sync_now (void)
{
    sntp_cmd_param * param = pvPortMalloc(sizeof(sntp_cmd_param));
    param->cmd = SNTP_SYNC;
    sntp_run(param);
}

/* Returned str pointer should be freed by caller */
char * sntp_get_server (sntp_svr_t svr_idx)
{
    char * svr_addr = NULL;
    svr_addr = (char *) OS_MALLOC(128);

  #ifdef RM_MAP_PERSISTANT_W
    char * tmp_svr = NULL;
  #endif

    if (svr_addr == NULL)
    {
        printf("Failed to mem_alloc for SNTP server address !!! \r\n");

        return NULL;
    }

    memset(svr_addr, 0, 128);

    switch (svr_idx)
    {
        case SNTP_SVR_1:
        {
  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                            ENV_GROUP_SYSCFG,
                                            NVR_KEY_SNTP_SERVER_DOMAIN_1,
                                            &tmp_svr);
  #endif

            break;
        }

        case SNTP_SVR_2:
        {
  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                            ENV_GROUP_SYSCFG,
                                            NVR_KEY_SNTP_SERVER_DOMAIN_2,
                                            &tmp_svr);
  #endif
            break;
        }

        case SNTP_SVR_0:
        default:
        {
  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                            ENV_GROUP_SYSCFG,
                                            NVR_KEY_SNTP_SERVER_DOMAIN,
                                            &tmp_svr);
  #endif
            break;
        }
    }

  #ifdef RM_MAP_PERSISTANT_W
    if (tmp_svr != NULL)
    {
        strcpy(svr_addr, tmp_svr);
    }
    else
  #endif
    {
        switch (svr_idx)
        {
            case SNTP_SVR_1:
            {
                strcpy(svr_addr, SNTP_SERVER_ADDRESS_1);
                break;
            }

            case SNTP_SVR_2:
            {
                strcpy(svr_addr, SNTP_SERVER_ADDRESS_2);
                break;
            }

            case SNTP_SVR_0:
            default:
            {
                strcpy(svr_addr, SNTP_SERVER_ADDRESS);
                break;
            }
        }
    }

    return svr_addr;
}

u8_t sntp_set_server (char * svraddr, sntp_svr_t svr_idx)
{
    ip_addr_t sntp_svr_ipaddr;

    if (strchr(svraddr, '.') != NULL)  /* IPv4 */
    {
        switch (svr_idx)
        {
            case SNTP_SVR_1:
            {
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_SYSCFG,
                                                 NVR_KEY_SNTP_SERVER_DOMAIN_1,
                                                 svraddr);
  #endif
                break;
            }

            case SNTP_SVR_2:
            {
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_SYSCFG,
                                                 NVR_KEY_SNTP_SERVER_DOMAIN_2,
                                                 svraddr);
  #endif
                break;
            }

            case SNTP_SVR_0:
            default:
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_SYSCFG,
                                                 NVR_KEY_SNTP_SERVER_DOMAIN,
                                                 svraddr);
  #endif
        }

        strncpy(sntp_names[svr_idx], svraddr, 32);

        if (svr_idx == 0)
        {
  #if LWIP_IPV4
            if (is_in_valid_ip_class(svraddr)) /* check ip */
            {
                ipaddr_aton(svraddr, &sntp_svr_ipaddr);
                sntp_setserver(0, &sntp_svr_ipaddr);
            }

   #if SNTP_SERVER_DNS
            else
            {
                sntp_setservername(0, svraddr);
            }
   #endif                              /*SNTP_SERVER_DNS*/
  #else // LWIP_IPV6
            ipaddr_aton(svraddr, &sntp_svr_ipaddr);
            sntp_setserver(0, &sntp_svr_ipaddr);
  #endif                               // LWIP_IPV4
        }

        return pdPASS;
    }
    else
    {
        return pdFAIL;
    }
}

 #endif                                /* LWIP_UDP */

#else
 #ifdef LWIP_HDR_APPS_RM_SNTP_H
unsigned char sntp_get_use (void)
{
    return pdFALSE;
}

 #endif                                /* LWIP_HDR_APPS_RM_SNTP_H */
#endif                                 // __SUPPORT_SNTP_CLIENT__

/*EOF*/
