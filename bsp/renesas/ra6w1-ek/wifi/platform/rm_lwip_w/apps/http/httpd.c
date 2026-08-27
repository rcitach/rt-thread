/**
 * @file
 * LWIP HTTP server implementation
 */

/*
 * Copyright (c) 2001-2003 Swedish Institute of Computer Science.
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
 * Author: Adam Dunkels <adam@sics.se>
 *         Simon Goldschmidt
 *
 */

/**
 *
 * This httpd supports for a
 * rudimentary server-side-include facility which will replace tags of the form
 * <!--#tag--> in any file whose extension is .shtml, .shtm or .ssi with
 * strings provided by an include handler whose pointer is provided to the
 * module via function http_set_ssi_handler().
 * Additionally, a simple common
 * gateway interface (CGI) handling mechanism has been added to allow clients
 * to hook functions to particular request URIs.
 *
 * To enable SSI support, define label LWIP_HTTPD_SSI in lwipopts.h.
 * To enable CGI support, define label LWIP_HTTPD_CGI in lwipopts.h.
 *
 * By default, the server assumes that HTTP headers are already present in
 * each file stored in the file system.  By defining LWIP_HTTPD_DYNAMIC_HEADERS in
 * lwipopts.h, this behavior can be changed such that the server inserts the
 * headers automatically based on the extension of the file being served.  If
 * this mode is used, be careful to ensure that the file system image used
 * does not already contain the header information.
 *
 * File system images without headers can be created using the makefsfile
 * tool with the -h command line option.
 *
 *
 * Notes about valid SSI tags
 * --------------------------
 *
 * The following assumptions are made about tags used in SSI markers:
 *
 * 1. No tag may contain '-' or whitespace characters within the tag name.
 * 2. Whitespace is allowed between the tag leadin "<!--#" and the start of
 *    the tag name and between the tag name and the leadout string "-->".
 * 3. The maximum tag name length is LWIP_HTTPD_MAX_TAG_NAME_LEN, currently 8 characters.
 *
 * Notes on CGI usage
 * ------------------
 *
 * The simple CGI support offered here works with GET method requests only
 * and can handle up to 16 parameters encoded into the URI. The handler
 * function may not write directly to the HTTP output but must return a
 * filename that the HTTP server will send to the browser as a response to
 * the incoming CGI request.
 *
 *
 *
 * The list of supported file types is quite short, so if makefsdata complains
 * about an unknown extension, make sure to add it (and its doctype) to
 * the 'g_psHTTPHeaders' list.
 */
#include "lwip/init.h"
#include "rm_httpd_opts.h"
#include "rm_httpd.h"
#include "lwip/debug.h"
#include "lwip/stats.h"
#include "lwip/apps/fs.h"
#include "httpd_structs.h"
#include "lwip/def.h"

#include "lwip/altcp.h"
#include "lwip/altcp_tcp.h"
#if HTTPD_ENABLE_HTTPS
 #include "lwip/altcp_tls.h"
#endif
#ifdef LWIP_HOOK_FILENAME
 #include LWIP_HOOK_FILENAME
#endif
#if LWIP_HTTPD_TIMING
 #include "lwip/sys.h"
#endif                                 /* LWIP_HTTPD_TIMING */

#if SUPPORT_FSP_RM_FS_W
 #include "rm_lwip_w_fs_parse.h"
 #include "common_data.h"
 #include "lfs_lock.h"
#endif

#include <string.h>                    /* memset */
#include <stdlib.h>                    /* atoi */
#include <stdio.h>
#include <stdint.h>
#include <limits.h>

#if !defined HTTPD_DEBUG || defined __DOXYGEN__
 #define HTTPD_DEBUG         LWIP_DBG_ON // LWIP_DBG_OFF
#endif

#if SUPPORT_FSP_RM_FS_W
 #define MAX_NAME_LEN        200
 #define MS_TO_TICKS_1000    pdMS_TO_TICKS(1000)

/* Removed duplicate helper `http_remove_zero_files_in_dir` (use `fs_remove_zero_files_in_dir` in fs.c). */
#endif                                 /* SUPPORT_FSP_RM_FS_W */

#if LWIP_TCP && LWIP_CALLBACK_API

/** Minimum length for a valid HTTP/0.9 request: "GET /\r\n" -> 7 bytes */
 #define MIN_REQ_LEN                     7

 #define CRLF                            "\r\n"
 #if LWIP_HTTPD_SUPPORT_11_KEEPALIVE
  #define HTTP11_CONNECTIONKEEPALIVE     "Connection: keep-alive"
  #define HTTP11_CONNECTIONKEEPALIVE2    "Connection: Keep-Alive"
 #endif

 #if LWIP_HTTPD_DYNAMIC_FILE_READ
  #define HTTP_IS_DYNAMIC_FILE(hs)    ((hs)->buf != NULL)
 #else
  #define HTTP_IS_DYNAMIC_FILE(hs)    0
 #endif

/* This defines checks whether tcp_write has to copy data or not */

 #ifndef HTTP_IS_DATA_VOLATILE

/** tcp_write does not have to copy data when sent from rom-file-system directly */
  #define HTTP_IS_DATA_VOLATILE(hs)        (HTTP_IS_DYNAMIC_FILE(hs) ? TCP_WRITE_FLAG_COPY : 0)
 #endif

/** Default: dynamic headers are sent from ROM (non-dynamic headers are handled like file data) */
 #ifndef HTTP_IS_HDR_VOLATILE
  #define HTTP_IS_HDR_VOLATILE(hs, ptr)    0
 #endif

/* Return values for http_send_*() */
 #define HTTP_DATA_TO_SEND_FREED       3
 #define HTTP_DATA_TO_SEND_BREAK       2
 #define HTTP_DATA_TO_SEND_CONTINUE    1
 #define HTTP_NO_DATA_TO_SEND          0

 #if SUPPORT_FSP_RM_FS_W

// Configuration (tune if needed)
  #define BOUNDARY_MAX_LEN             128  // Reduced to save stack - boundaries are typically ~50 chars
  #define TAIL_MAX_LEN                 80   // Must be >= BOUNDARY_MAX_LEN for proper overlap detection
  #define PATH_MAX_LEN                 256
  #define TCP_PAYLOAD                  1536 // assume max TCP payload ~1500
  #define MAX_UPLOAD_FILE_SIZE         102400
  #define HEADER_LENGTH                256
  #define FS_WRITE_BUFFER_SIZE         4096
  #define UPLOAD_PACKET_BUF            16384
  #define UPLOAD_COPY_BUF              8192
 #endif

typedef struct
{
    const char * name;
    u8_t         shtml;
} default_filename;

static const default_filename httpd_default_filenames[] =
{
    {"/index.shtml", 1      },
    {"/index.ssi",   1      },
    {"/index.shtm",  1      },
    {"/index.html",  0      },
    {"/index.htm",   0      }
};

 #define NUM_DEFAULT_FILENAMES    LWIP_ARRAYSIZE(httpd_default_filenames)

 #if LWIP_HTTPD_SUPPORT_REQUESTLIST

/** HTTP request is copied here from pbufs for simple parsing */
static char httpd_req_buf[LWIP_HTTPD_MAX_REQ_LENGTH + 1];
 #endif                                /* LWIP_HTTPD_SUPPORT_REQUESTLIST */

 #if LWIP_HTTPD_SUPPORT_POST
  #if LWIP_HTTPD_POST_MAX_RESPONSE_URI_LEN > LWIP_HTTPD_MAX_REQUEST_URI_LEN
   #define LWIP_HTTPD_URI_BUF_LEN    LWIP_HTTPD_POST_MAX_RESPONSE_URI_LEN
  #endif
 #endif
 #ifndef LWIP_HTTPD_URI_BUF_LEN
  #define LWIP_HTTPD_URI_BUF_LEN     LWIP_HTTPD_MAX_REQUEST_URI_LEN
 #endif
 #if LWIP_HTTPD_URI_BUF_LEN

/* Filename for response file to send when POST is finished or
 * search for default files when a directory is requested. */
static char http_uri_buf[LWIP_HTTPD_URI_BUF_LEN + 1];
 #endif
 #if SUPPORT_FSP_RM_FS_W

/* Write context */

static volatile int write_in_progress = 0;
static char         current_upload_path[PATH_MAX_LEN];
static char         boundary_str[BOUNDARY_MAX_LEN];
static char         boundary_tail[BOUNDARY_MAX_LEN];
static lfs_file_t   upload_file;       /* Global file object */

/* Assume wrappers: lfs_mutex_take(timeout), lfs_mutex_give(), lfs_mutex_get() */
static int in_file = 0;                /* Flag for "inside file content" */

/* Upload status tracking */
typedef enum
{
    UPLOAD_STATUS_NONE = 0,
    UPLOAD_STATUS_SUCCESS,
    UPLOAD_STATUS_ERROR_FS_FULL,
    UPLOAD_STATUS_ERROR_WRITE_FAILED,
    UPLOAD_STATUS_ERROR_CORRUPTED
} upload_status_t;

static upload_status_t last_upload_status = UPLOAD_STATUS_NONE;
static char            last_uploaded_filename[PATH_MAX_LEN] = {0};

/* Upload progress tracking */
static volatile unsigned long upload_total_bytes      = 0;
static volatile unsigned long upload_received_bytes   = 0;
static volatile int           upload_progress_percent = 0;

/* LittleFS mutex moved to shared helper 'lfs_lock' (see lfs_lock.h/.c) */

/* LittleFS health state: set to 0 when a filesystem operation fails in a
 * way that indicates the filesystem (or underlying block driver) is
 * unhealthy. When clear, further attempts to call LittleFS APIs will be
 * refused to avoid reboots from repeated failing calls. */
static volatile int lfs_fs_healthy = 1;

/* Recovery policy for LittleFS: attempt to remount a bounded number of
 * times with backoff. Tune these macros as needed for your platform. */
  #ifndef LFS_RECOVERY_MAX_ATTEMPTS
   #define LFS_RECOVERY_MAX_ATTEMPTS     3
  #endif
  #ifndef LFS_RECOVERY_BASE_DELAY_MS
   #define LFS_RECOVERY_BASE_DELAY_MS    1000
  #endif

typedef struct
{
    char buffer[FS_WRITE_BUFFER_SIZE] __attribute__((aligned(8))); /* 8-byte aligned for flash */
    int  buffer_len;
    int  total_written;
} fs_write_ctx_t;

static fs_write_ctx_t write_ctx __attribute__((aligned(8))); /* Ensure struct is aligned */

/* Timestamp of last upload activity (ms since boot). Updated on any
 * buffered write or flush. Used to abort stalled uploads after timeout. */
static uint32_t upload_last_activity_ms = 0;

/* Shared upload parsing state required by flush_write_buffer() and
 * the streaming POST handler. These were previously declared as
 * function-local statics inside `httpd_post_receive_data` which made
 * them invisible to other functions (causing undeclared identifier
 * compiler errors). Move the minimal needed pieces to file scope. */
static char current_upload_path[PATH_MAX_LEN] __attribute__((aligned(8))) = {0};
static int  boundary_tail_len = 0;

/* Global upload file handle used by the POST upload path and by poll/timeout
 * handlers so it must be visible across functions. */
static lfs_file_t upload_file;

/* Ensure the global LFS mutex symbol is visible in this translation
 * unit in case include ordering prevents it from being seen. */
extern SemaphoreHandle_t lfs_mutex;

 #endif

 #if LWIP_HTTPD_DYNAMIC_HEADERS

/* The number of individual strings that comprise the headers sent before each
 * requested file.
 */
  #define NUM_FILE_HDR_STRINGS                     5
  #define HDR_STRINGS_IDX_HTTP_STATUS              0 /* e.g. "HTTP/1.0 200 OK\r\n" */
  #define HDR_STRINGS_IDX_SERVER_NAME              1 /* e.g. "Server: "HTTPD_SERVER_AGENT"\r\n" */
  #define HDR_STRINGS_IDX_CONTENT_LEN_KEEPALIVE    2 /* e.g. "Content-Length: xy\r\n" and/or "Connection: keep-alive\r\n" */
  #define HDR_STRINGS_IDX_CONTENT_LEN_NR           3 /* the byte count, when content-length is used */
  #define HDR_STRINGS_IDX_CONTENT_TYPE             4 /* the content type (or default answer content type including default document) */

/* The dynamically generated Content-Length buffer needs space for CRLF + NULL */
  #define LWIP_HTTPD_MAX_CONTENT_LEN_OFFSET        3
  #ifndef LWIP_HTTPD_MAX_CONTENT_LEN_SIZE

/* The dynamically generated Content-Length buffer shall be able to work with
 * ~953 MB (9 digits) */
   #define LWIP_HTTPD_MAX_CONTENT_LEN_SIZE         (9 + LWIP_HTTPD_MAX_CONTENT_LEN_OFFSET)
  #endif
 #endif                                /* LWIP_HTTPD_DYNAMIC_HEADERS */

 #if LWIP_HTTPD_SSI

  #define HTTPD_LAST_TAG_PART    0xFFFF

enum tag_check_state
{
    TAG_NONE,                          /* Not processing an SSI tag */
    TAG_LEADIN,                        /* Tag lead in "<!--#" being processed */
    TAG_FOUND,                         /* Tag name being read, looking for lead-out start */
    TAG_LEADOUT,                       /* Tag lead out "-->" being processed */
    TAG_SENDING                        /* Sending tag replacement string */
};

struct http_ssi_state
{
    const char * parsed;                                                /* Pointer to the first unparsed byte in buf. */
  #if !LWIP_HTTPD_SSI_INCLUDE_TAG
    const char * tag_started;                                           /* Pointer to the first opening '<' of the tag. */
  #endif /* !LWIP_HTTPD_SSI_INCLUDE_TAG */
    const char * tag_end;                                               /* Pointer to char after the closing '>' of the tag. */
    u32_t        parse_left;                                            /* Number of unparsed bytes in buf. */
    u16_t        tag_index;                                             /* Counter used by tag parsing state machine */
    u16_t        tag_insert_len;                                        /* Length of insert in string tag_insert */
  #if LWIP_HTTPD_SSI_MULTIPART
    u16_t tag_part;                                                     /* Counter passed to and changed by tag insertion function to insert multiple times */
  #endif /* LWIP_HTTPD_SSI_MULTIPART */
    u8_t                 tag_type;                                      /* index into http_ssi_tag_desc array */
    u8_t                 tag_name_len;                                  /* Length of the tag name in string tag_name */
    char                 tag_name[LWIP_HTTPD_MAX_TAG_NAME_LEN + 1];     /* Last tag name extracted */
    char                 tag_insert[LWIP_HTTPD_MAX_TAG_INSERT_LEN + 1]; /* Insert string for tag_name */
    enum tag_check_state tag_state;                                     /* State of the tag processor */
};

struct http_ssi_tag_description
{
    const char * lead_in;
    const char * lead_out;
};

 #endif                                /* LWIP_HTTPD_SSI */

struct http_state
{
 #if LWIP_HTTPD_KILL_OLD_ON_CONNECTIONS_EXCEEDED
    struct http_state * next;
 #endif                                /* LWIP_HTTPD_KILL_OLD_ON_CONNECTIONS_EXCEEDED */
 #if SUPPORT_FSP_RM_FS_W
    struct lfs_file_data   file_handle;
    struct lfs_file_data * handle;
 #else
    struct fs_file   file_handle;
    struct fs_file * handle;
 #endif                                /* SUPPORT_FSP_RM_FS_W */
    const char * file;                 /* Pointer to first unsent byte in buf. */

    struct altcp_pcb * pcb;
 #if LWIP_HTTPD_SUPPORT_REQUESTLIST
    struct pbuf * req;
 #endif                                               /* LWIP_HTTPD_SUPPORT_REQUESTLIST */

 #if LWIP_HTTPD_DYNAMIC_FILE_READ
    char * buf;                                       /* File read buffer. */
    int    buf_len;                                   /* Size of file read buffer, buf. */
 #endif                                               /* LWIP_HTTPD_DYNAMIC_FILE_READ */
    u32_t left;                                       /* Number of unsent bytes in buf. */
    u8_t  retries;
 #if LWIP_HTTPD_SUPPORT_11_KEEPALIVE
    u8_t keepalive;
 #endif                                               /* LWIP_HTTPD_SUPPORT_11_KEEPALIVE */
 #if LWIP_HTTPD_SSI
    struct http_ssi_state * ssi;
 #endif                                               /* LWIP_HTTPD_SSI */
 #if LWIP_HTTPD_CGI
    char * params[LWIP_HTTPD_MAX_CGI_PARAMETERS];     /* Params extracted from the request URI */
    char * param_vals[LWIP_HTTPD_MAX_CGI_PARAMETERS]; /* Values for each extracted param */
 #endif                                               /* LWIP_HTTPD_CGI */
 #if LWIP_HTTPD_DYNAMIC_HEADERS
    const char * hdrs[NUM_FILE_HDR_STRINGS];          /* HTTP headers to be sent. */
    char         hdr_content_len[LWIP_HTTPD_MAX_CONTENT_LEN_SIZE];
    u16_t        hdr_pos;                             /* The position of the first unsent header byte in the
                                                       * current string */
    u16_t hdr_index;                                  /* The index of the hdr string currently being sent. */
 #endif /* LWIP_HTTPD_DYNAMIC_HEADERS */
 #if LWIP_HTTPD_TIMING
    u32_t time_started;
 #endif /* LWIP_HTTPD_TIMING */
 #if LWIP_HTTPD_SUPPORT_POST
    u32_t post_content_len_left;
  #if LWIP_HTTPD_POST_MANUAL_WND
    u32_t unrecved_bytes;
    u8_t  no_auto_wnd;
    u8_t  post_finished;
  #endif                               /* LWIP_HTTPD_POST_MANUAL_WND */
 #endif                                /* LWIP_HTTPD_SUPPORT_POST*/
};

 #if HTTPD_USE_MEM_POOL
LWIP_MEMPOOL_DECLARE(HTTPD_STATE, MEMP_NUM_PARALLEL_HTTPD_CONNS, sizeof(struct http_state), "HTTPD_STATE")
  #if LWIP_HTTPD_SSI
LWIP_MEMPOOL_DECLARE(HTTPD_SSI_STATE,
                     MEMP_NUM_PARALLEL_HTTPD_SSI_CONNS,
                     sizeof(struct http_ssi_state),
                     "HTTPD_SSI_STATE")
   #define HTTP_FREE_SSI_STATE(x)    LWIP_MEMPOOL_FREE(HTTPD_SSI_STATE, (x))
   #define HTTP_ALLOC_SSI_STATE()    (struct http_ssi_state *) LWIP_MEMPOOL_ALLOC(HTTPD_SSI_STATE)
  #endif                               /* LWIP_HTTPD_SSI */
  #define HTTP_ALLOC_HTTP_STATE()    (struct http_state *) LWIP_MEMPOOL_ALLOC(HTTPD_STATE)
  #define HTTP_FREE_HTTP_STATE(x)    LWIP_MEMPOOL_FREE(HTTPD_STATE, (x))
 #else /* HTTPD_USE_MEM_POOL */
  #define HTTP_ALLOC_HTTP_STATE()    (struct http_state *) mem_malloc(sizeof(struct http_state))
  #define HTTP_FREE_HTTP_STATE(x)    mem_free(x)
  #if LWIP_HTTPD_SSI
   #define HTTP_ALLOC_SSI_STATE()    (struct http_ssi_state *) mem_malloc(sizeof(struct http_ssi_state))
   #define HTTP_FREE_SSI_STATE(x)    mem_free(x)
  #endif                               /* LWIP_HTTPD_SSI */
 #endif                                /* HTTPD_USE_MEM_POOL */

static err_t http_close_conn(struct altcp_pcb * pcb, struct http_state * hs);
static err_t http_close_or_abort_conn(struct altcp_pcb * pcb, struct http_state * hs, u8_t abort_conn);
static err_t http_find_file(struct http_state * hs, const char * uri, int is_09);
static err_t http_init_file(struct http_state * hs,
                            struct fs_file    * file,
                            int                 is_09,
                            const char        * uri,
                            u8_t                tag_check,
                            char              * params);

 #if SUPPORT_FSP_RM_FS_W

/* Prototype for LittleFS-based open implemented in fs.c */
err_t user_fs_open(struct lfs_file_data * p_file, const char * p_path);

 #endif
static err_t http_poll(void * arg, struct altcp_pcb * pcb);
static u8_t  http_check_eof(struct altcp_pcb * pcb, struct http_state * hs);

 #if LWIP_HTTPD_FS_ASYNC_READ
static void http_continue(void * connection);

 #endif                                /* LWIP_HTTPD_FS_ASYNC_READ */

 #if LWIP_HTTPD_SSI

/* SSI insert handler function pointer. */
static tSSIHandler httpd_ssi_handler;
  #if !LWIP_HTTPD_SSI_RAW
static int           httpd_num_tags;
static const char ** httpd_tags;
  #endif                               /* !LWIP_HTTPD_SSI_RAW */

/* Define the available tag lead-ins and corresponding lead-outs.
 * ATTENTION: for the algorithm below using this array, it is essential
 * that the lead in differs in the first character! */
const struct http_ssi_tag_description http_ssi_tag_desc[] =
{
    {"<!--#", "-->"      },
    {"/*#",   "*/"       }
};

 #endif                                /* LWIP_HTTPD_SSI */

 #if LWIP_HTTPD_CGI

/* CGI handler information */
static const tCGI * httpd_cgis;
static int          httpd_num_cgis;
static int          http_cgi_paramcount;
  #define http_cgi_params        hs->params
  #define http_cgi_param_vals    hs->param_vals
 #elif LWIP_HTTPD_CGI_SSI
static char * http_cgi_params[LWIP_HTTPD_MAX_CGI_PARAMETERS];     /* Params extracted from the request URI */
static char * http_cgi_param_vals[LWIP_HTTPD_MAX_CGI_PARAMETERS]; /* Values for each extracted param */
 #endif /* LWIP_HTTPD_CGI */

/** define LWIP_HTTPD_EXAMPLE_GENERATEDFILES to 1 to enable this file system */
 #ifndef LWIP_HTTPD_EXAMPLE_SIMPLEPOST
  #define LWIP_HTTPD_EXAMPLE_SIMPLEPOST          1
  #define LWIP_HTTPD_EXAMPLE_SIMPLEPOST_LOGIN    0
 #endif

 #if LWIP_HTTPD_EXAMPLE_SIMPLEPOST

  #if LWIP_HTTPD_EXAMPLE_SIMPLEPOST_LOGIN
   #define USER_PASS_BUFSIZE    16

static void * current_connection;
static void * valid_connection;
static char   last_user[USER_PASS_BUFSIZE];
  #endif                               /* LWIP_HTTPD_EXAMPLE_SIMPLEPOST_LOGIN */

/* The RA6W1 does not support file systems.
 * A callback is invoked when data is received. */
static altcp_user_recv_fn user_recv_fn;

  #if SUPPORT_FSP_RM_FS_W

/* Try to remount LittleFS with exponential backoff. Returns 0 on success,
 * negative on failure. This function takes the LFS mutex while remounting
 * to avoid races with concurrent filesystem operations. */
static int attempt_lfs_recovery (void)
{
    int attempt;
    int rc = -1;
    int delay_ms;

    for (attempt = 1; attempt <= LFS_RECOVERY_MAX_ATTEMPTS; ++attempt)
    {
        delay_ms = LFS_RECOVERY_BASE_DELAY_MS * attempt;
        LWIP_DEBUGF(HTTPD_DEBUG, ("[RECOV] Attempt %d to remount LittleFS", attempt));

        /* Try unmount first to ensure clean state */
        if (lfs_mutex_take(pdMS_TO_TICKS(5000)) == pdTRUE)
        {
            lfs_unmount(&g_rm_littlefs0_lfs);
            rc = lfs_mount(&g_rm_littlefs0_lfs, &g_rm_littlefs0_lfs_cfg);
            lfs_mutex_give();
        }
        else
        {
            /* If we can't get the mutex, sleep and retry */
            LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_LEVEL_WARNING, ("[RECOV] Failed to acquire LFS mutex for remount"));
            rc = -1;
        }

        if (rc == 0)
        {
            LWIP_DEBUGF(HTTPD_DEBUG, ("[RECOV] LittleFS remount succeeded"));
            lfs_fs_healthy = 1;

            return 0;
        }

        LWIP_DEBUGF(HTTPD_DEBUG, ("[RECOV] Remount attempt %d failed (rc=%d), sleeping %d ms", attempt, rc, delay_ms));
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }

    LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_LEVEL_WARNING,
                ("[RECOV] LittleFS recovery failed after %d attempts", LFS_RECOVERY_MAX_ATTEMPTS));
    lfs_fs_healthy = 0;

    return -1;
}

  #endif                               /* SUPPORT_FSP_RM_FS_W */

err_t httpd_post_begin (void       * connection,
                        const char * uri,
                        const char * http_request,
                        u16_t        http_request_len,
                        int          content_len,
                        char       * response_uri,
                        u16_t        response_uri_len,
                        u8_t       * post_auto_wnd)
{
  #if SUPPORT_FSP_RM_FS_W
    if (strcmp(uri, "/post") == 0)
    {
        LWIP_DEBUGF(HTTPD_DEBUG, ("POST Begin: Upload, length=%d", content_len));
        in_file = 0;                   /* Flag for "inside file content" */
        return ERR_OK;
    }
  #endif /* SUPPORT_FSP_RM_FS_W */

  #if !LWIP_HTTPD_EXAMPLE_SIMPLEPOST_LOGIN
    LWIP_UNUSED_ARG(connection);
  #endif                               /* !LWIP_HTTPD_EXAMPLE_SIMPLEPOST_LOGIN */
    LWIP_UNUSED_ARG(http_request);
    LWIP_UNUSED_ARG(http_request_len);
    LWIP_UNUSED_ARG(content_len);
    LWIP_UNUSED_ARG(response_uri);
    LWIP_UNUSED_ARG(response_uri_len);

  #if LWIP_HTTPD_CGI
    if (!memcmp(uri, "/login.cgi", 11))
    {
   #if LWIP_HTTPD_EXAMPLE_SIMPLEPOST_LOGIN
        if (current_connection != connection)
        {
            current_connection = connection;
            valid_connection   = NULL;

            /* default page is "login failed" */
            snprintf(response_uri, response_uri_len, "/loginfail.html");

            /* e.g. for large uploads to slow flash over a fast connection, you should
             * manually update the rx window. That way, a sender can only send a full
             * tcp window at a time. If this is required, set 'post_aut_wnd' to 0.
             * We do not need to throttle upload speed here, so: */
            *post_auto_wnd = 1;

            return ERR_OK;
        }

   #else
        *post_auto_wnd = 1;

        return ERR_OK;
   #endif                              /* LWIP_HTTPD_EXAMPLE_SIMPLEPOST_LOGIN */
    }

    return ERR_VAL;
  #else
    LWIP_UNUSED_ARG(response_uri);
    LWIP_UNUSED_ARG(response_uri_len);
    LWIP_UNUSED_ARG(post_auto_wnd);

    if ((!memcmp(uri, "/post", strlen("/post"))) || (!memcmp(uri, "/put", strlen("/put"))))
    {
   #if LWIP_HTTPD_EXAMPLE_SIMPLEPOST_LOGIN
        if (current_connection != connection)
        {
            current_connection = connection;
            valid_connection   = NULL;
            *post_auto_wnd     = 1;
        }

   #else
        *post_auto_wnd = 1;
   #endif                              /* LWIP_HTTPD_EXAMPLE_SIMPLEPOST_LOGIN */
        return ERR_OK;
    }
    return ERR_VAL;
  #endif                               /* LWIP_HTTPD_CGI */
}

  #if SUPPORT_FSP_RM_FS_W
static void init_write_ctx (void)
{
    memset(&write_ctx, 0, sizeof(write_ctx));
    write_ctx.buffer_len    = 0;
    write_ctx.total_written = 0;
    upload_last_activity_ms = sys_now();
}

/* Helper: flush write_ctx.buffer to LittleFS in chunks (safe) */
static int flush_write_buffer_to_lfs (lfs_file_t * file)
{
    const int LFS_CHUNK = 1000;
    int       remaining;
    int       offset        = 0;
    int       written_total = 0;
    int       this_chunk;
    int       r;

    if (!file)
    {
        LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_LEVEL_WARNING, ("[ERR] flush_write_buffer_to_lfs: NULL file"));

        return -1;
    }

    if (write_ctx.buffer_len == 0)
    {
        return 0;
    }

    if (!lfs_fs_healthy)
    {
        LWIP_DEBUGF(HTTPD_DEBUG, ("[WARN] LittleFS unhealthy - attempt recovery before write"));
        if (attempt_lfs_recovery() < 0)
        {
            LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_LEVEL_WARNING, ("[ERR] LittleFS recovery failed"));

            return -1;
        }
    }

    if (write_in_progress)
    {
        LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_LEVEL_WARNING, ("[ERR] flush called while write_in_progress"));

        return -1;
    }

    write_in_progress = 1;

    if (lfs_mutex)
    {
        if (lfs_mutex_take(MS_TO_TICKS_1000) != pdTRUE)
        {
            LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_LEVEL_WARNING, ("[ERR] Failed to take LFS mutex for write"));
            write_in_progress = 0;

            return -1;
        }
    }

    remaining = write_ctx.buffer_len;

    LWIP_DEBUGF(HTTPD_DEBUG,
                ("[WRITE] Writing %d bytes to flash (buf=%p)", write_ctx.buffer_len, (void *) write_ctx.buffer));

    while (remaining > 0)
    {
        this_chunk = remaining > LFS_CHUNK ? LFS_CHUNK : remaining;
        r          = lfs_file_write(&g_rm_littlefs0_lfs, file, write_ctx.buffer + offset, this_chunk);
        if (r < 0)
        {
            lfs_fs_healthy = 0;
            if (lfs_mutex)
            {
                lfs_mutex_give();
            }

            write_in_progress = 0;
            LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_LEVEL_WARNING, ("[ERR] lfs_file_write returned %d", r));

            return -1;
        }

        if (r == 0)
        {
            lfs_fs_healthy = 0;
            if (lfs_mutex)
            {
                lfs_mutex_give();
            }

            write_in_progress = 0;
            LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_LEVEL_WARNING, ("[ERR] lfs_file_write returned 0"));

            return -1;
        }

        offset        += r;
        written_total += r;
        remaining     -= r;
        if (r < this_chunk)
        {
            lfs_fs_healthy = 0;
            if (lfs_mutex)
            {
                lfs_mutex_give();
            }

            write_in_progress = 0;
            LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_LEVEL_WARNING, ("[ERR] Partial write: %d < %d", r, this_chunk));

            return -1;
        }

        taskYIELD();
    }

    if (lfs_mutex)
    {
        lfs_mutex_give();
    }

    write_ctx.total_written += written_total;
    write_ctx.buffer_len     = 0;
    upload_last_activity_ms  = sys_now();
    write_in_progress        = 0;

    LWIP_DEBUGF(HTTPD_DEBUG, ("[WRITE] Success, total_written=%d", write_ctx.total_written));

    return 0;
}

/* Buffered write: copy incoming data into write_ctx.buffer and flush when full */
static int buffered_write_to_ctx (lfs_file_t * file, const char * data, int len)
{
    int idx = 0;
    int space;
    int to_copy;

    if (!file)
    {
        LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_LEVEL_WARNING, ("[ERR] buffered_write_to_ctx: NULL file"));

        return -1;
    }

    if (!data || (len <= 0))
    {
        return 0;
    }

    if (!lfs_fs_healthy)
    {
        LWIP_DEBUGF(HTTPD_DEBUG, ("[WARN] LittleFS unhealthy - attempt recovery before buffered_write"));
        if (attempt_lfs_recovery() < 0)
        {
            LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_LEVEL_WARNING, ("[ERR] LittleFS recovery failed"));

            return -1;
        }
    }

    while (idx < len)
    {
        space = FS_WRITE_BUFFER_SIZE - write_ctx.buffer_len;
        if (space <= 0)
        {
            if (flush_write_buffer_to_lfs(file) < 0)
            {
                return -1;
            }

            space = FS_WRITE_BUFFER_SIZE - write_ctx.buffer_len;
            if (space <= 0)
            {
                LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_LEVEL_WARNING, ("[ERR] No space after flush"));

                return -1;
            }
        }

        to_copy = len - idx;
        if (to_copy > space)
        {
            to_copy = space;
        }

        memcpy(write_ctx.buffer + write_ctx.buffer_len, data + idx, to_copy);
        write_ctx.buffer_len += to_copy;
        idx += to_copy;
        upload_last_activity_ms = sys_now();
        if (write_ctx.buffer_len == FS_WRITE_BUFFER_SIZE)
        {
            if (flush_write_buffer_to_lfs(file) < 0)
            {
                return -1;
            }
        }
    }

    return 0;
}

/* Sync helper (reuse your existing) */
static int sync_file_and_close (lfs_file_t * file)
{
    int ret;

    if (!file)
    {
        return -1;
    }

    if (lfs_mutex)
    {
        if (lfs_mutex_take(pdMS_TO_TICKS(5000)) != pdTRUE)
        {
            LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_LEVEL_WARNING, ("[ERR] Failed to take LFS mutex for sync"));

            return -1;
        }
    }

    ret = lfs_file_sync(&g_rm_littlefs0_lfs, file);
    if (lfs_mutex)
    {
        lfs_mutex_give();
    }

    if (ret < 0)
    {
        lfs_fs_healthy = 0;
        LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_LEVEL_WARNING, ("[ERR] lfs_file_sync failed: %d", ret));

        return -1;
    }

    return 0;
}

static void cleanup_upload_and_remove (lfs_file_t * file, const char * path)
{
    /* Set upload status to write failed */
    last_upload_status = UPLOAD_STATUS_ERROR_WRITE_FAILED;

    if (file)
    {
        if (lfs_mutex)
        {
            if (lfs_mutex_take(MS_TO_TICKS_1000) == pdTRUE)
            {
                lfs_file_close(&g_rm_littlefs0_lfs, file);
                lfs_mutex_give();
            }
        }
        else
        {
            lfs_file_close(&g_rm_littlefs0_lfs, file);
        }
    }

    if (path && path[0])
    {
        if (lfs_mutex)
        {
            if (lfs_mutex_take(MS_TO_TICKS_1000) == pdTRUE)
            {
                lfs_remove(&g_rm_littlefs0_lfs, path);
                lfs_mutex_give();
            }
        }
        else
        {
            lfs_remove(&g_rm_littlefs0_lfs, path);
        }
    }

    in_file                = 0;
    current_upload_path[0] = '\0';
    boundary_tail_len      = 0;
}

/* Helper: safe memsearch (like memmem) - returns pointer in haystack or NULL */
static void * safe_memsearch (const void * p_haystack, size_t haylen, const void * p_needle, size_t needlelen)
{
    if ((needlelen == 0) || (haylen < needlelen))
    {
        return NULL;
    }

    const uint8_t * h = (const uint8_t *) p_haystack;
    const uint8_t * n = (const uint8_t *) p_needle;
    size_t          i;

    for (i = 0; i <= haylen - needlelen; ++i)
    {
        if ((h[i] == n[0]) && (memcmp(h + i, n, needlelen) == 0))
        {
            return (void *) (h + i);
        }
    }

    return NULL;
}

/* Trim trailing CRLF or LF only if actually present; return trimmed length */
static int trim_crlf_len (const char * buf, int len)
{
    if ((len >= 2) && (buf[len - 2] == '\r') && (buf[len - 1] == '\n'))
    {
        return len - 2;
    }

    if ((len >= 1) && (buf[len - 1] == '\n'))
    {
        return len - 1;
    }

    return len;
}

  #endif                               // SUPPORT_FSP_RM_FS_W

err_t httpd_post_receive_data (void * connection, struct pbuf * p)
{
  #if SUPPORT_FSP_RM_FS_W
    struct http_state * hs = (struct http_state *) connection;
    struct lfs_info     info;
    static int          fs_check_done = 0;
    static char         packet_buf[UPLOAD_PACKET_BUF];
    int                 total_len;
    int                 copy_len;
    int                 copied;
    char              * p_disp;
    char              * p_fn;
    char                filename[PATH_MAX_LEN] = {0};
    struct lfs_info     st;
    int                 sret;
    int                 ret;
    char              * p_boundary;
    char              * p_eol;
    int                 blen;
    int                 copy_from;
    int                 start_idx;
    char                temp[BOUNDARY_MAX_LEN];
    char              * p_dash;
    int                 nlen;
    char              * p_start;
    char              * p_end;
    int                 content_len;
    int                 max_content;
    int                 remaining;
    int                 boundary_len;
    int                 keep_len;
    int                 seg_keep_len;
    int                 seglen;
    int                 search_len;
    char              * search_buf = NULL;
    int                 pos;
    int                 write_from_tail;
    int                 tail_take;
    int                 write_len;
    const char        * seg;
    struct pbuf       * q;
    int                 use_malloc = 0;
    char              * p_bpos;
    int                 remainder;
    char              * p_bpos_seg;

    if (!p)
    {
        LWIP_DEBUGF(HTTPD_DEBUG, ("[POST_RECEIVE] Called with NULL pbuf"));

        return ERR_OK;
    }

    LWIP_DEBUGF(HTTPD_DEBUG, ("[POST_RECEIVE] Called with pbuf, tot_len=%d, in_file=%d", p->tot_len, in_file)); /* Ensure FS mounted */
    if (!fs_check_done)
    {
        if (lfs_stat(&g_rm_littlefs0_lfs, "/", &info) < 0)
        {
            LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_LEVEL_WARNING, ("[ERR] Filesystem not accessible"));
            pbuf_free(p);

            return ERR_IF;
        }

        fs_check_done = 1;
        LWIP_DEBUGF(HTTPD_DEBUG, ("[FS] Filesystem check passed"));
    }

    /* Temporary copy buffer for processing this pbuf (owned memory) */
    /* We will iterate pbuf chain and copy each segment into local processing buffer */
    /* so we never depend on pbuf payload after this function returns. */
    total_len = p->tot_len;
    copy_len  = total_len > UPLOAD_PACKET_BUF ? UPLOAD_PACKET_BUF : total_len;
    copied    = pbuf_copy_partial(p, packet_buf, copy_len, 0);
    if (copied < 0)
    {
        copied = 0;
    }

    packet_buf[copied] = '\0';

    /* Stall detection (same as you had) */
    if (in_file)
    {
        u32_t now = sys_now();
        if ((upload_last_activity_ms != 0) && ((now - upload_last_activity_ms) > 5000u))
        {
            LWIP_DEBUGF(HTTPD_DEBUG,
                        ("[WARN] Upload stalled, aborting: %s",
                         current_upload_path[0] ? current_upload_path : "(none)"));
            cleanup_upload_and_remove(&upload_file, current_upload_path);
            pbuf_free(p);

            return ERR_ABRT;
        }
    }

    /* First-time header parsing (not in_file) */
    if (!in_file)
    {
        LWIP_DEBUGF(HTTPD_DEBUG, ("[POST_RECEIVE] First-time header parsing"));

        /* Reset any stale flags */
        write_in_progress      = 0;
        current_upload_path[0] = '\0';
        boundary_tail_len      = 0;

        /* Find Content-Disposition & filename */
        p_disp = strstr(packet_buf, "Content-Disposition:");
        if (p_disp)
        {
            LWIP_DEBUGF(HTTPD_DEBUG, ("[POST_RECEIVE] Found Content-Disposition"));
            p_fn = strstr(p_disp, "filename=");
            if (p_fn)
            {
                sscanf(p_fn, "filename=\"%255[^\"]\"", filename);
                LWIP_DEBUGF(HTTPD_DEBUG, ("[POST] Starting upload, file=%s", filename));

                /* Build target path */
                /* Ensure we never overflow current_upload_path: reserve 1 for leading '/', 1 for '\0' */
                snprintf(current_upload_path, sizeof(current_upload_path), "/%.*s",
                         (int) (sizeof(current_upload_path) - 2), filename);

                /* Reset only received bytes and progress - total_bytes already set in http_post_request */
                upload_received_bytes   = 0;
                upload_progress_percent = 0;
                printf("[UPLOAD] Starting: file=%s, total_bytes=%lu\n", filename, upload_total_bytes);
                LWIP_DEBUGF(HTTPD_DEBUG,
                            ("[POST_RECEIVE] Starting upload: file=%s, total_bytes=%lu",
                             filename, upload_total_bytes));

                /* Validate path length */
                if ((strlen(current_upload_path) == 0) || (strlen(current_upload_path) >= PATH_MAX_LEN))
                {
                    LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_LEVEL_WARNING, ("[ERR] Invalid path length"));
                    pbuf_free(p);

                    return ERR_VAL;
                }

                /* Prepare LittleFS open (with mutex) */
                if (!lfs_fs_healthy)
                {
                    if (attempt_lfs_recovery() < 0)
                    {
                        LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_LEVEL_WARNING, ("[ERR] LFS recovery failed"));
                        pbuf_free(p);

                        return ERR_IF;
                    }
                }

                if (!lfs_mutex_get())
                {
                    LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_LEVEL_WARNING, ("[ERR] Failed to create lfs mutex"));
                    pbuf_free(p);

                    return ERR_MEM;
                }

                if (lfs_mutex_take(MS_TO_TICKS_1000) != pdTRUE)
                {
                    LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_LEVEL_WARNING, ("[ERR] Failed to take LFS mutex for open"));
                    pbuf_free(p);

                    return ERR_TIMEOUT;
                }

                /* Remove zero-length stale file if necessary */
                sret = lfs_stat(&g_rm_littlefs0_lfs, current_upload_path, &st);
                if (sret == 0)
                {
                    if ((st.type == LFS_TYPE_REG) && (st.size == 0))
                    {
                        (void) lfs_remove(&g_rm_littlefs0_lfs, current_upload_path);
                    }
                }

                ret = lfs_file_open(&g_rm_littlefs0_lfs,
                                    &upload_file,
                                    current_upload_path,
                                    LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
                lfs_mutex_give();
                if (ret < 0)
                {
                    LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_LEVEL_WARNING, ("[ERR] lfs_file_open failed: %d", ret));
                    pbuf_free(p);

                    return ERR_VAL;
                }

                /* Extract multipart boundary: look for a leading `--` line */
                p_boundary = strstr(packet_buf, "\r\n--");
                if (!p_boundary)
                {
                    /* maybe boundary begins at start (some clients) */
                    p_boundary = strstr(packet_buf, "------");
                }

                if (p_boundary)
                {
                    p_eol = strstr(p_boundary + 2, "\r\n");
                    if (p_eol)
                    {
                        blen = (int) (p_eol - (p_boundary + 2)); /* exclude leading CRLF */
                        if ((blen > 0) && (blen < BOUNDARY_MAX_LEN))
                        {
                            start_idx =
                                (p_boundary[0] ==
                                 '\r') ? (int) ((p_boundary + 2) - packet_buf) : (int) (p_boundary - packet_buf);

                            /* ensure we extract starting at the `--` */
                            p_dash = strstr(packet_buf + start_idx, "--");
                            if (p_dash)
                            {
                                nlen = (int) (p_eol - p_dash);
                                if (nlen < BOUNDARY_MAX_LEN)
                                {
                                    memcpy(boundary_str, p_dash, nlen);
                                    boundary_str[nlen] = '\0';
                                }
                            }
                        }
                    }
                }

                /* Find start of file content: first double CRLF after headers */
                p_start = strstr(packet_buf, "\r\n\r\n");
                if (p_start)
                {
                    p_start += 4;      /* skip CRLFCRLF */
                    LWIP_DEBUGF(HTTPD_DEBUG, ("[POST] Content starts at offset %d", (int) (p_start - packet_buf)));

                    /* Search for boundary within the copied window */
                    if (boundary_str[0] != '\0')
                    {
                        p_end = strstr(p_start, boundary_str);
                        if (p_end)
                        {
                            content_len = (int) (p_end - p_start);
                            content_len = trim_crlf_len(p_start, content_len);

                            max_content = copied - (int) (p_start - packet_buf);
                            if ((content_len > 0) && (content_len <= max_content))
                            {
                                init_write_ctx();
                                if (buffered_write_to_ctx(&upload_file, p_start, content_len) < 0)
                                {
                                    cleanup_upload_and_remove(&upload_file, current_upload_path);
                                    pbuf_free(p);

                                    return ERR_ABRT;
                                }
                            }

                            /* final flush + sync + close */
                            if (flush_write_buffer_to_lfs(&upload_file) < 0)
                            {
                                cleanup_upload_and_remove(&upload_file, current_upload_path);
                                pbuf_free(p);

                                return ERR_ABRT;
                            }

                            if (sync_file_and_close(&upload_file) < 0)
                            {
                                cleanup_upload_and_remove(&upload_file, current_upload_path);
                                pbuf_free(p);

                                return ERR_ABRT;
                            }

                            if (lfs_mutex_take(MS_TO_TICKS_1000) == pdTRUE)
                            {
                                lfs_file_close(&g_rm_littlefs0_lfs, &upload_file);
                                lfs_mutex_give();
                            }

                            /* Upload completed successfully */
                            LWIP_DEBUGF(HTTPD_DEBUG, ("[UPLOAD_SUCCESS_PATH_1] Setting status to SUCCESS (1)"));
                            last_upload_status = UPLOAD_STATUS_SUCCESS;
                            strncpy(last_uploaded_filename, current_upload_path, sizeof(last_uploaded_filename) - 1);
                            last_uploaded_filename[sizeof(last_uploaded_filename) - 1] = '\0';

                            printf("[UPLOAD] Complete: 100%% (%lu bytes) - File: %s\n",
                                   upload_received_bytes,
                                   last_uploaded_filename);
                            LWIP_DEBUGF(HTTPD_DEBUG,
                                        ("[UPLOAD] Complete: 100%% (%lu bytes) - File: %s",
                                         upload_received_bytes, last_uploaded_filename));

                            in_file                = 0;
                            current_upload_path[0] = '\0';
                            boundary_tail_len      = 0;

                            /* Mark POST as complete so response page will be sent */
                            hs->post_content_len_left = 0;

                            LWIP_DEBUGF(HTTPD_DEBUG, ("[POST] Complete"));
                            pbuf_free(p);

                            return ERR_OK;
                        }
                        else
                        {
                            /* No boundary in this first packet: copy what we can to buffer, and save tail */
                            init_write_ctx();
                            remaining    = copied - (int) (p_start - packet_buf);
                            boundary_len = strlen(boundary_str);
                            keep_len     = boundary_len;
                            if (keep_len > remaining)
                            {
                                keep_len = remaining;
                            }

                            write_len = remaining - keep_len;
                            if (write_len > 0)
                            {
                                if (buffered_write_to_ctx(&upload_file, p_start, write_len) < 0)
                                {
                                    cleanup_upload_and_remove(&upload_file, current_upload_path);
                                    pbuf_free(p);

                                    return ERR_ABRT;
                                }
                            }

                            if (keep_len > 0)
                            {
                                memcpy(boundary_tail, p_start + write_len, keep_len);
                                boundary_tail_len = keep_len;
                            }
                            else
                            {
                                boundary_tail_len = 0;
                            }

                            in_file                 = 1;
                            upload_last_activity_ms = sys_now();
                        }
                    }
                    else
                    {
                        LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_LEVEL_WARNING, ("[ERR] boundary not parsed"));
                        cleanup_upload_and_remove(&upload_file, current_upload_path);
                        pbuf_free(p);

                        return ERR_VAL;
                    }
                }
                else
                {
                    /* No header/body separation in the copied window; wait for more data */
                    /* CRLFCRLF not found in header window */
                }
            }
            else
            {
                /* filename not found in Content-Disposition */
            }
        }
        else
        {
            /* Content-Disposition not found */
        }
    }
    else
    {
        /* Streaming: already in_file == 1 */
        LWIP_DEBUGF(HTTPD_DEBUG, ("[POST_RECEIVE] Streaming mode: in_file=1, boundary_str='%s'", boundary_str));
        boundary_len = strlen(boundary_str);

        /* iterate pbuf chain, but we already have packet_buf for first UP TO UPLOAD_PACKET_BUF bytes */
        /* We'll process the full pbuf chain segment-by-segment to handle large pbufs safely. */
        q = p;
        while (q)
        {
            seg    = (const char *) q->payload;
            seglen = q->len;

            /* If we have saved tail from previous packet, check junction first */
            if (boundary_tail_len > 0)
            {
                search_len = boundary_tail_len + seglen;

                /* limit local allocation: use stack up to UPLOAD_COPY_BUF or malloc if larger */
                use_malloc = 0;
                if (search_len <= UPLOAD_COPY_BUF)
                {
                    static char tmp[UPLOAD_COPY_BUF];
                    search_buf = tmp;
                }
                else
                {
                    search_buf = malloc(search_len + 1);
                    use_malloc = 1;
                    if (!search_buf)
                    {
                        LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_LEVEL_WARNING, ("[ERR] malloc failed"));
                        cleanup_upload_and_remove(&upload_file, current_upload_path);
                        pbuf_free(p);

                        return ERR_MEM;
                    }
                }

                memcpy(search_buf, boundary_tail, boundary_tail_len);
                memcpy(search_buf + boundary_tail_len, seg, seglen);
                search_buf[search_len] = '\0';

                LWIP_DEBUGF(HTTPD_DEBUG,
                            ("[BOUNDARY_CHECK] Checking junction: tail_len=%d, seg_len=%d, boundary='%s'",
                             boundary_tail_len, seglen, boundary_str));
                p_bpos = safe_memsearch(search_buf, search_len, boundary_str, boundary_len);
                if (p_bpos)
                {
                    LWIP_DEBUGF(HTTPD_DEBUG, ("[BOUNDARY_FOUND] At junction! Finalizing upload."));
                    pos             = (int) (p_bpos - search_buf);
                    write_from_tail = pos;
                    write_from_tail = trim_crlf_len(search_buf, write_from_tail);

                    /* Write portion from boundary_tail (if any) */
                    if (write_from_tail > 0)
                    {
                        tail_take = write_from_tail;
                        if (tail_take > boundary_tail_len)
                        {
                            tail_take = boundary_tail_len;
                        }

                        if (tail_take > 0)
                        {
                            if (buffered_write_to_ctx(&upload_file, boundary_tail, tail_take) < 0)
                            {
                                if (use_malloc)
                                {
                                    free(search_buf);
                                }

                                cleanup_upload_and_remove(&upload_file, current_upload_path);
                                pbuf_free(p);

                                return ERR_ABRT;
                            }
                        }

                        /* any remainder (pos - tail_take) is in seg beginning */
                        remainder = write_from_tail - tail_take;
                        if (remainder > 0)
                        {
                            if (buffered_write_to_ctx(&upload_file, seg, remainder) < 0)
                            {
                                if (use_malloc)
                                {
                                    free(search_buf);
                                }

                                cleanup_upload_and_remove(&upload_file, current_upload_path);
                                pbuf_free(p);

                                return ERR_ABRT;
                            }
                        }
                    }

                    if (use_malloc)
                    {
                        free(search_buf);
                    }

                    /* Final flush + sync + close */
                    if (flush_write_buffer_to_lfs(&upload_file) < 0)
                    {
                        cleanup_upload_and_remove(&upload_file, current_upload_path);
                        pbuf_free(p);

                        return ERR_ABRT;
                    }

                    if (sync_file_and_close(&upload_file) < 0)
                    {
                        cleanup_upload_and_remove(&upload_file, current_upload_path);
                        pbuf_free(p);

                        return ERR_ABRT;
                    }

                    if (lfs_mutex_take(MS_TO_TICKS_1000) == pdTRUE)
                    {
                        lfs_file_close(&g_rm_littlefs0_lfs, &upload_file);
                        lfs_mutex_give();
                    }

                    /* Upload completed successfully */
                    LWIP_DEBUGF(HTTPD_DEBUG, ("[UPLOAD_SUCCESS_PATH_3] Setting status to SUCCESS (1)"));
                    last_upload_status = UPLOAD_STATUS_SUCCESS;
                    strncpy(last_uploaded_filename, current_upload_path, sizeof(last_uploaded_filename) - 1);
                    last_uploaded_filename[sizeof(last_uploaded_filename) - 1] = '\0';

                    printf("[UPLOAD] Complete: 100%% (%lu bytes) - File: %s\n",
                           upload_received_bytes,
                           last_uploaded_filename);
                    LWIP_DEBUGF(HTTPD_DEBUG,
                                ("[UPLOAD] Complete: 100%% (%lu bytes) - File: %s",
                                 upload_received_bytes, last_uploaded_filename));

                    in_file                = 0;
                    boundary_tail_len      = 0;
                    current_upload_path[0] = '\0';

                    /* Mark POST as complete so response page will be sent */
                    hs->post_content_len_left = 0;

                    LWIP_DEBUGF(HTTPD_DEBUG, ("[POST] Complete"));
                    pbuf_free(p);

                    return ERR_OK;
                }

                /* No boundary at junction: commit the tail to file */
                LWIP_DEBUGF(HTTPD_DEBUG,
                            ("[BOUNDARY_CHECK] Not found at junction, writing tail_len=%d", boundary_tail_len));
                if (buffered_write_to_ctx(&upload_file, boundary_tail, boundary_tail_len) < 0)
                {
                    if (use_malloc)
                    {
                        free(search_buf);
                    }

                    cleanup_upload_and_remove(&upload_file, current_upload_path);
                    pbuf_free(p);

                    return ERR_ABRT;
                }

                boundary_tail_len = 0;
                if (use_malloc)
                {
                    free(search_buf);
                }
            }

            /* Search for boundary inside this segment */
            LWIP_DEBUGF(HTTPD_DEBUG, ("[BOUNDARY_CHECK] Searching in segment: len=%d", seglen));
            p_bpos_seg = safe_memsearch(seg, seglen, boundary_str, boundary_len);
            if (p_bpos_seg)
            {
                LWIP_DEBUGF(HTTPD_DEBUG, ("[BOUNDARY_FOUND] In segment! Finalizing upload."));
                pos       = (int) (p_bpos_seg - seg);
                write_len = pos;
                write_len = trim_crlf_len(seg, write_len);

                if (write_len > 0)
                {
                    if (buffered_write_to_ctx(&upload_file, seg, write_len) < 0)
                    {
                        cleanup_upload_and_remove(&upload_file, current_upload_path);
                        pbuf_free(p);

                        return ERR_ABRT;
                    }
                }

                /* flush & finish */
                if (flush_write_buffer_to_lfs(&upload_file) < 0)
                {
                    cleanup_upload_and_remove(&upload_file, current_upload_path);
                    pbuf_free(p);

                    return ERR_ABRT;
                }

                if (sync_file_and_close(&upload_file) < 0)
                {
                    cleanup_upload_and_remove(&upload_file, current_upload_path);
                    pbuf_free(p);

                    return ERR_ABRT;
                }

                if (lfs_mutex_take(MS_TO_TICKS_1000) == pdTRUE)
                {
                    lfs_file_close(&g_rm_littlefs0_lfs, &upload_file);
                    lfs_mutex_give();
                }

                /* Upload completed successfully */
                LWIP_DEBUGF(HTTPD_DEBUG, ("[UPLOAD_SUCCESS_PATH_2] Setting status to SUCCESS (1)"));
                last_upload_status = UPLOAD_STATUS_SUCCESS;
                strncpy(last_uploaded_filename, current_upload_path, sizeof(last_uploaded_filename) - 1);
                last_uploaded_filename[sizeof(last_uploaded_filename) - 1] = '\0';

                printf("[UPLOAD] Complete: 100%% (%lu bytes) - File: %s\n",
                       upload_received_bytes,
                       last_uploaded_filename);
                LWIP_DEBUGF(HTTPD_DEBUG,
                            ("[UPLOAD] Complete: 100%% (%lu bytes) - File: %s",
                             upload_received_bytes, last_uploaded_filename));

                in_file                = 0;
                boundary_tail_len      = 0;
                current_upload_path[0] = '\0';

                /* Mark POST as complete so response page will be sent */
                hs->post_content_len_left = 0;

                LWIP_DEBUGF(HTTPD_DEBUG, ("[POST] Complete"));
                pbuf_free(p);

                return ERR_OK;
            }

            /* No boundary: write most of the segment but keep trailing slice = boundary_len */
            seg_keep_len = boundary_len;
            if (seg_keep_len > seglen)
            {
                seg_keep_len = seglen;
            }

            int write_len = seglen - seg_keep_len;
            if (write_len > 0)
            {
                if (buffered_write_to_ctx(&upload_file, seg, write_len) < 0)
                {
                    cleanup_upload_and_remove(&upload_file, current_upload_path);
                    pbuf_free(p);

                    return ERR_ABRT;
                }
            }

            if (seg_keep_len > 0)
            {
                memcpy(boundary_tail, seg + write_len, seg_keep_len);
                boundary_tail_len = seg_keep_len;
            }
            else
            {
                boundary_tail_len = 0;
            }

            q = q->next;
        }                              /* end pbuf chain loop */

        /* After processing chain, update activity and let TCP know we accepted data */
        upload_last_activity_ms = sys_now();
    }

    /* Check if upload is complete: all expected bytes received and file is open */
    LWIP_DEBUGF(HTTPD_DEBUG,
                ("[POST_RECEIVE] Check complete: in_file=%d, total=%lu, received=%lu",
                 in_file, upload_total_bytes, upload_received_bytes));
    if (in_file && (upload_total_bytes > 0) && (upload_received_bytes >= upload_total_bytes))
    {
        LWIP_DEBUGF(HTTPD_DEBUG,
                    ("[UPLOAD] Detected completion: received=%lu, total=%lu",
                     upload_received_bytes, upload_total_bytes));

        /* Final flush and close for streaming upload that finished */
        if (flush_write_buffer_to_lfs(&upload_file) < 0)
        {
            cleanup_upload_and_remove(&upload_file, current_upload_path);
            pbuf_free(p);

            return ERR_ABRT;
        }

        if (sync_file_and_close(&upload_file) < 0)
        {
            cleanup_upload_and_remove(&upload_file, current_upload_path);
            pbuf_free(p);

            return ERR_ABRT;
        }

        if (lfs_mutex_take(pdMS_TO_TICKS(1000)) == pdTRUE)
        {
            lfs_file_close(&g_rm_littlefs0_lfs, &upload_file);
            lfs_mutex_give();
        }

        /* Upload completed successfully */
        last_upload_status = UPLOAD_STATUS_SUCCESS;
        strncpy(last_uploaded_filename, current_upload_path, sizeof(last_uploaded_filename) - 1);
        last_uploaded_filename[sizeof(last_uploaded_filename) - 1] = '\0';

        printf("[UPLOAD] Complete: 100%% (%lu bytes) - File: %s\n", upload_received_bytes, last_uploaded_filename);
        LWIP_DEBUGF(HTTPD_DEBUG,
                    ("[UPLOAD] Complete: 100%% (%lu bytes) - File: %s",
                     upload_received_bytes, last_uploaded_filename));
        in_file                = 0;
        boundary_tail_len      = 0;
        current_upload_path[0] = '\0';

        /* Mark POST as complete */
        if (hs)
        {
            struct http_state * http_hs = (struct http_state *) hs;
            http_hs->post_content_len_left = 0;
        }

        LWIP_DEBUGF(HTTPD_DEBUG, ("[POST] Upload complete via byte counter"));
    }

    /* Tell TCP stack we processed the data */
    if (hs && hs->pcb)
    {
        altcp_recved(hs->pcb, p->tot_len);
    }

    pbuf_free(p);
    taskYIELD();

    return ERR_OK;
  #endif                               /* SUPPORT_FSP_RM_FS_W */

  #if !LWIP_HTTPD_EXAMPLE_SIMPLEPOST_LOGIN
    LWIP_UNUSED_ARG(connection);
    LWIP_UNUSED_ARG(p);

    return ERR_OK;
  #else
    if (current_connection == connection)
    {
        u16_t token_user = pbuf_memfind(p, "user=", 5, 0);
        u16_t token_pass = pbuf_memfind(p, "pass=", 5, 0);
        if ((token_user != 0xFFFF) && (token_pass != 0xFFFF))
        {
            u16_t value_user = token_user + 5;
            u16_t value_pass = token_pass + 5;
            u16_t len_user   = 0;
            u16_t len_pass   = 0;
            u16_t tmp;

            /* find user len */
            tmp = pbuf_memfind(p, "&", 1, value_user);
            if (tmp != 0xFFFF)
            {
                len_user = tmp - value_user;
            }
            else
            {
                len_user = p->tot_len - value_user;
            }

            /* find pass len */
            tmp = pbuf_memfind(p, "&", 1, value_pass);
            if (tmp != 0xFFFF)
            {
                len_pass = tmp - value_pass;
            }
            else
            {
                len_pass = p->tot_len - value_pass;
            }

            if ((len_user > 0) && (len_user < USER_PASS_BUFSIZE) &&
                (len_pass > 0) && (len_pass < USER_PASS_BUFSIZE))
            {
                /* provide contiguous storage if p is a chained pbuf */
                char   buf_user[USER_PASS_BUFSIZE];
                char   buf_pass[USER_PASS_BUFSIZE];
                char * user = (char *) pbuf_get_contiguous(p, buf_user, sizeof(buf_user), len_user, value_user);
                char * pass = (char *) pbuf_get_contiguous(p, buf_pass, sizeof(buf_pass), len_pass, value_pass);
                if (user && pass)
                {
                    user[len_user] = 0;
                    pass[len_pass] = 0;
                    if (!strcmp(user, "lwip") && !strcmp(pass, "post"))
                    {
                        /* user and password are correct, create a "session" */
                        valid_connection = connection;
                        memcpy(last_user, user, sizeof(last_user));
                    }
                }
            }
        }

        /* not returning ERR_OK aborts the connection, so return ERR_OK unless the
         * connection is unknown */
        return ERR_OK;
    }
    return ERR_VAL;
  #endif                               /* LWIP_HTTPD_EXAMPLE_SIMPLEPOST_LOGIN */
}

void httpd_post_finished (void * connection, char * response_uri, u16_t response_uri_len)
{
  #if SUPPORT_FSP_RM_FS_W
    LWIP_DEBUGF(HTTPD_DEBUG, ("[POST_FINISHED] Called with status: %d", last_upload_status));

    /* Return appropriate response page based on upload status */
    switch (last_upload_status)
    {
        case UPLOAD_STATUS_SUCCESS:
        {
            snprintf(response_uri, response_uri_len, "/upload_success.html");
            LWIP_DEBUGF(HTTPD_DEBUG, ("[UPLOAD] Success: %s -> Response: %s", last_uploaded_filename, response_uri));
            break;
        }

        case UPLOAD_STATUS_ERROR_FS_FULL:
        {
            snprintf(response_uri, response_uri_len, "/upload_fs_full.html");
            LWIP_DEBUGF(HTTPD_DEBUG, ("[UPLOAD] Failed: Filesystem full -> Response: %s", response_uri));
            break;
        }

        case UPLOAD_STATUS_ERROR_WRITE_FAILED:
        {
            snprintf(response_uri, response_uri_len, "/upload_error.html");
            LWIP_DEBUGF(HTTPD_DEBUG, ("[UPLOAD] Failed: Write error -> Response: %s", response_uri));
            break;
        }

        case UPLOAD_STATUS_ERROR_CORRUPTED:
        {
            snprintf(response_uri, response_uri_len, "/upload_error.html");
            LWIP_DEBUGF(HTTPD_DEBUG, ("[UPLOAD] Failed: Filesystem corrupted -> Response: %s", response_uri));
            break;
        }

        default:
        {
            snprintf(response_uri, response_uri_len, "/upload.html");
            LWIP_DEBUGF(HTTPD_DEBUG, ("[UPLOAD] Unknown status -> Response: %s", response_uri));
            break;
        }
    }

    /* Reset status for next upload */
    last_upload_status        = UPLOAD_STATUS_NONE;
    last_uploaded_filename[0] = '\0';

    return;
  #endif                               /* SUPPORT_FSP_RM_FS_W */
  #if !LWIP_HTTPD_EXAMPLE_SIMPLEPOST_LOGIN
    LWIP_UNUSED_ARG(connection);
    snprintf(response_uri, response_uri_len, "/index.html");
  #else

    /* default page is "login failed" */
    snprintf(response_uri, response_uri_len, "/loginfail.html");
    if (current_connection == connection)
    {
        if (valid_connection == connection)
        {
            /* login succeeded */
            snprintf(response_uri, response_uri_len, "/session.html");
        }

        current_connection = NULL;
        valid_connection   = NULL;
    }
  #endif                               /* LWIP_HTTPD_EXAMPLE_SIMPLEPOST_LOGIN */
}

 #endif                                /* LWIP_HTTPD_EXAMPLE_SIMPLEPOST*/

 #if LWIP_HTTPD_KILL_OLD_ON_CONNECTIONS_EXCEEDED

/** global list of active HTTP connections, use to kill the oldest when
 *  running out of memory */
static struct http_state * http_connections;

static void http_add_connection (struct http_state * hs)
{
    /* add the connection to the list */
    hs->next         = http_connections;
    http_connections = hs;
}

static void http_remove_connection (struct http_state * hs)
{
    /* take the connection off the list */
    if (http_connections)
    {
        if (http_connections == hs)
        {
            http_connections = hs->next;
        }
        else
        {
            struct http_state * last;
            for (last = http_connections; last->next != NULL; last = last->next)
            {
                if (last->next == hs)
                {
                    last->next = hs->next;
                    break;
                }
            }
        }
    }
}

static void http_kill_oldest_connection (u8_t ssi_required)
{
    struct http_state * hs           = http_connections;
    struct http_state * hs_free_next = NULL;
    while (hs && hs->next)
    {
  #if LWIP_HTTPD_SSI
        if (ssi_required)
        {
            if (hs->next->ssi != NULL)
            {
                hs_free_next = hs;
            }
        }
        else
  #else                                /* LWIP_HTTPD_SSI */
        LWIP_UNUSED_ARG(ssi_required);
  #endif /* LWIP_HTTPD_SSI */
        {
            hs_free_next = hs;
        }

        LWIP_ASSERT("broken list", hs != hs->next);
        hs = hs->next;
    }

    if (hs_free_next != NULL)
    {
        LWIP_ASSERT("hs_free_next->next != NULL", hs_free_next->next != NULL);
        LWIP_ASSERT("hs_free_next->next->pcb != NULL", hs_free_next->next->pcb != NULL);

        /* send RST when killing a connection because of memory shortage */
        http_close_or_abort_conn(hs_free_next->next->pcb, hs_free_next->next, 1); /* this also unlinks the http_state from the list */
    }
}

 #else /* LWIP_HTTPD_KILL_OLD_ON_CONNECTIONS_EXCEEDED */

  #define http_add_connection(hs)
  #define http_remove_connection(hs)

 #endif                                /* LWIP_HTTPD_KILL_OLD_ON_CONNECTIONS_EXCEEDED */

 #if LWIP_HTTPD_SSI

/** Allocate as struct http_ssi_state. */
static struct http_ssi_state * http_ssi_state_alloc (void)
{
    struct http_ssi_state * ret = HTTP_ALLOC_SSI_STATE();
  #if LWIP_HTTPD_KILL_OLD_ON_CONNECTIONS_EXCEEDED
    if (ret == NULL)
    {
        http_kill_oldest_connection(1);
        ret = HTTP_ALLOC_SSI_STATE();
    }
  #endif                               /* LWIP_HTTPD_KILL_OLD_ON_CONNECTIONS_EXCEEDED */
    if (ret != NULL)
    {
        memset(ret, 0, sizeof(struct http_ssi_state));
    }

    return ret;
}

/** Free a struct http_ssi_state. */
static void http_ssi_state_free (struct http_ssi_state * ssi)
{
    if (ssi != NULL)
    {
        HTTP_FREE_SSI_STATE(ssi);
    }
}

 #endif                                /* LWIP_HTTPD_SSI */

/** Initialize a struct http_state.
 */
static void http_state_init (struct http_state * hs)
{
    /* Initialize the structure. */
    memset(hs, 0, sizeof(struct http_state));
 #if LWIP_HTTPD_DYNAMIC_HEADERS

    /* Indicate that the headers are not yet valid */
    hs->hdr_index = NUM_FILE_HDR_STRINGS;
 #endif                                /* LWIP_HTTPD_DYNAMIC_HEADERS */
}

/** Allocate a struct http_state. */
static struct http_state * http_state_alloc (void)
{
    struct http_state * ret = HTTP_ALLOC_HTTP_STATE();
 #if LWIP_HTTPD_KILL_OLD_ON_CONNECTIONS_EXCEEDED
    if (ret == NULL)
    {
        http_kill_oldest_connection(0);
        ret = HTTP_ALLOC_HTTP_STATE();
    }
 #endif                                /* LWIP_HTTPD_KILL_OLD_ON_CONNECTIONS_EXCEEDED */
    if (ret != NULL)
    {
        http_state_init(ret);
        http_add_connection(ret);
    }

    return ret;
}

/** Free a struct http_state.
 * Also frees the file data if dynamic.
 */
static void http_state_eof (struct http_state * hs)
{
    if (hs->handle)
    {
 #if LWIP_HTTPD_TIMING
        u32_t ms_needed = sys_now() - hs->time_started;
        u32_t needed    = LWIP_MAX(1, (ms_needed / 100));
        LWIP_DEBUGF(HTTPD_DEBUG_TIMING,
                    ("httpd: needed %"U32_F " ms to send file of %d bytes -> %"U32_F " bytes/sec\n",
                     ms_needed, hs->handle->len, ((((u32_t) hs->handle->len) * 10) / needed)));
 #endif                                /* LWIP_HTTPD_TIMING */
        fs_close((struct fs_file *) hs->handle);
        hs->handle = NULL;
    }

 #if LWIP_HTTPD_DYNAMIC_FILE_READ
    if (hs->buf != NULL)
    {
        mem_free(hs->buf);
        hs->buf = NULL;
    }
 #endif                                /* LWIP_HTTPD_DYNAMIC_FILE_READ */
 #if LWIP_HTTPD_SSI
    if (hs->ssi)
    {
        http_ssi_state_free(hs->ssi);
        hs->ssi = NULL;
    }
 #endif                                /* LWIP_HTTPD_SSI */
 #if LWIP_HTTPD_SUPPORT_REQUESTLIST
    if (hs->req)
    {
        pbuf_free(hs->req);
        hs->req = NULL;
    }
 #endif                                /* LWIP_HTTPD_SUPPORT_REQUESTLIST */
}

/** Free a struct http_state.
 * Also frees the file data if dynamic.
 */
static void http_state_free (struct http_state * hs)
{
    if (hs != NULL)
    {
        http_state_eof(hs);
        http_remove_connection(hs);
        HTTP_FREE_HTTP_STATE(hs);
    }
}

/** Call tcp_write() in a loop trying smaller and smaller length
 *
 * @param pcb altcp_pcb to send
 * @param ptr Data to send
 * @param length Length of data to send (in/out: on return, contains the
 *        amount of data sent)
 * @param apiflags directly passed to tcp_write
 * @return the return value of tcp_write
 */
static err_t http_write (struct altcp_pcb * pcb, const void * ptr, u16_t * length, u8_t apiflags)
{
    u16_t len, max_len;
    err_t err;
    LWIP_ASSERT("length != NULL", length != NULL);
    len = *length;
    if (len == 0)
    {
        return ERR_OK;
    }

    /* We cannot send more data than space available in the send buffer. */
    max_len = altcp_sndbuf(pcb);
    if (max_len < len)
    {
        len = max_len;
    }

 #ifdef HTTPD_MAX_WRITE_LEN

    /* Additional limitation: e.g. don't enqueue more than 2*mss at once */
    max_len = HTTPD_MAX_WRITE_LEN(pcb);
    if (len > max_len)
    {
        len = max_len;
    }
 #endif                                /* HTTPD_MAX_WRITE_LEN */
    do
    {
        LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("Trying to send %d bytes\n", len));
        err = altcp_write(pcb, ptr, len, apiflags);
        if (err == ERR_MEM)
        {
            if ((altcp_sndbuf(pcb) == 0) ||
                (altcp_sndqueuelen(pcb) >= TCP_SND_QUEUELEN))
            {
                /* no need to try smaller sizes */
                len = 1;
            }
            else
            {
                len /= 2;
            }

            LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("Send failed, trying less (%d bytes)\n", len));
        }
    } while ((err == ERR_MEM) && (len > 1));

    if (err == ERR_OK)
    {
        LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("Sent %d bytes\n", len));
        *length = len;
    }
    else
    {
        LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("Send failed with err %d (\"%s\")\n", err, lwip_strerr(err)));
        *length = 0;
    }

 #if LWIP_HTTPD_SUPPORT_11_KEEPALIVE

    /* ensure nagle is normally enabled (only disabled for persistent connections
     * when all data has been enqueued but the connection stays open for the next
     * request */
    altcp_nagle_enable(pcb);
 #endif

    return err;
}

/**
 * The connection shall be actively closed (using RST to close from fault states).
 * Reset the sent- and recv-callbacks.
 *
 * @param pcb the tcp pcb to reset callbacks
 * @param hs connection state to free
 */
static err_t http_close_or_abort_conn (struct altcp_pcb * pcb, struct http_state * hs, u8_t abort_conn)
{
    err_t err;
    LWIP_DEBUGF(HTTPD_DEBUG, ("Closing connection %p\n", (void *) pcb));

 #if LWIP_HTTPD_SUPPORT_POST
    if (hs != NULL)
    {
        if ((hs->post_content_len_left != 0)
  #if LWIP_HTTPD_POST_MANUAL_WND
            || ((hs->no_auto_wnd != 0) && (hs->unrecved_bytes != 0))
  #endif                               /* LWIP_HTTPD_POST_MANUAL_WND */
            )
        {
            /* make sure the post code knows that the connection is closed */
            http_uri_buf[0] = 0;
            httpd_post_finished(hs, http_uri_buf, LWIP_HTTPD_URI_BUF_LEN);
        }
    }
 #endif                                /* LWIP_HTTPD_SUPPORT_POST*/

    altcp_arg(pcb, NULL);
    altcp_recv(pcb, NULL);
    altcp_err(pcb, NULL);
    altcp_poll(pcb, NULL, 0);
    altcp_sent(pcb, NULL);
    if (hs != NULL)
    {
        http_state_free(hs);
    }

    if (abort_conn)
    {
        altcp_abort(pcb);

        return ERR_OK;
    }

    err = altcp_close(pcb);
    if (err != ERR_OK)
    {
        LWIP_DEBUGF(HTTPD_DEBUG, ("Error %d closing %p\n", err, (void *) pcb));

        /* error closing, try again later in poll */
        altcp_poll(pcb, http_poll, HTTPD_POLL_INTERVAL);
    }

    return err;
}

/**
 * Setting flags to call httpd_close from outside.
 */
u16_t httpd_close_flag = 0;
void http_server_stop (void)
{
    httpd_close_flag = 1;
}

/**
 * The connection shall be actively closed.
 * Reset the sent- and recv-callbacks.
 *
 * @param pcb the tcp pcb to reset callbacks
 * @param hs connection state to free
 */
static err_t http_close_conn (struct altcp_pcb * pcb, struct http_state * hs)
{
    return http_close_or_abort_conn(pcb, hs, 0);
}

/** End of file: either close the connection (Connection: close) or
 * close the file (Connection: keep-alive)
 */
static void http_eof (struct altcp_pcb * pcb, struct http_state * hs)
{
    /* HTTP/1.1 persistent connection? (Not supported for SSI) */
 #if LWIP_HTTPD_SUPPORT_11_KEEPALIVE
    if (hs->keepalive)
    {
        http_remove_connection(hs);

        http_state_eof(hs);
        http_state_init(hs);

        /* restore state: */
        hs->pcb       = pcb;
        hs->keepalive = 1;
        http_add_connection(hs);

        /* ensure nagle doesn't interfere with sending all data as fast as possible: */
        altcp_nagle_disable(pcb);
    }
    else
 #endif                                /* LWIP_HTTPD_SUPPORT_11_KEEPALIVE */
    {
        http_close_conn(pcb, hs);
    }
}

 #if LWIP_HTTPD_CGI || LWIP_HTTPD_CGI_SSI

/**
 * Extract URI parameters from the parameter-part of an URI in the form
 * "test.cgi?x=y" @todo: better explanation!
 * Pointers to the parameters are stored in hs->param_vals.
 *
 * @param hs http connection state
 * @param params pointer to the NULL-terminated parameter string from the URI
 * @return number of parameters extracted
 */
static int extract_uri_parameters (struct http_state * hs, char * params)
{
    char * pair;
    char * equals;
    int    loop;

    LWIP_UNUSED_ARG(hs);

    /* If we have no parameters at all, return immediately. */
    if (!params || (params[0] == '\0'))
    {
        return 0;
    }

    /* Get a pointer to our first parameter */
    pair = params;

    /* Parse up to LWIP_HTTPD_MAX_CGI_PARAMETERS from the passed string and ignore the
     * remainder (if any) */
    for (loop = 0; (loop < LWIP_HTTPD_MAX_CGI_PARAMETERS) && pair; loop++)
    {
        /* Save the name of the parameter */
        http_cgi_params[loop] = pair;

        /* Remember the start of this name=value pair */
        equals = pair;

        /* Find the start of the next name=value pair and replace the delimiter
         * with a 0 to terminate the previous pair string. */
        pair = strchr(pair, '&');
        if (pair)
        {
            *pair = '\0';
            pair++;
        }
        else
        {
            /* We didn't find a new parameter so find the end of the URI and
             * replace the space with a '\0' */
            pair = strchr(equals, ' ');
            if (pair)
            {
                *pair = '\0';
            }

            /* Revert to NULL so that we exit the loop as expected. */
            pair = NULL;
        }

        /* Now find the '=' in the previous pair, replace it with '\0' and save
         * the parameter value string. */
        equals = strchr(equals, '=');
        if (equals)
        {
            *equals = '\0';
            http_cgi_param_vals[loop] = equals + 1;
        }
        else
        {
            http_cgi_param_vals[loop] = NULL;
        }
    }

    return loop;
}

 #endif                                /* LWIP_HTTPD_CGI || LWIP_HTTPD_CGI_SSI */

 #if LWIP_HTTPD_SSI

/**
 * Insert a tag (found in an shtml in the form of "<!--#tagname-->" into the file.
 * The tag's name is stored in ssi->tag_name (NULL-terminated), the replacement
 * should be written to hs->tag_insert (up to a length of LWIP_HTTPD_MAX_TAG_INSERT_LEN).
 * The amount of data written is stored to ssi->tag_insert_len.
 *
 * @todo: return tag_insert_len - maybe it can be removed from struct http_state?
 *
 * @param hs http connection state
 */
static void get_tag_insert (struct http_state * hs)
{
  #if LWIP_HTTPD_SSI_RAW
    const char * tag;
  #else                                /* LWIP_HTTPD_SSI_RAW */
    int tag;
  #endif /* LWIP_HTTPD_SSI_RAW */
    size_t len;
    struct http_ssi_state * ssi;
  #if LWIP_HTTPD_SSI_MULTIPART
    u16_t current_tag_part;
  #endif                               /* LWIP_HTTPD_SSI_MULTIPART */

    LWIP_ASSERT("hs != NULL", hs != NULL);
    ssi = hs->ssi;
    LWIP_ASSERT("ssi != NULL", ssi != NULL);
  #if LWIP_HTTPD_SSI_MULTIPART
    current_tag_part = ssi->tag_part;
    ssi->tag_part    = HTTPD_LAST_TAG_PART;
  #endif                               /* LWIP_HTTPD_SSI_MULTIPART */
  #if LWIP_HTTPD_SSI_RAW
    tag = ssi->tag_name;
  #endif

    if (httpd_ssi_handler
  #if !LWIP_HTTPD_SSI_RAW
        && httpd_tags && httpd_num_tags
  #endif                               /* !LWIP_HTTPD_SSI_RAW */
        )
    {
        /* Debug: print tag being processed and number of registered tags */
        /* Find this tag in the list we have been provided. */
  #if LWIP_HTTPD_SSI_RAW
        {
  #else                                /* LWIP_HTTPD_SSI_RAW */
        for (tag = 0; tag < httpd_num_tags; tag++)
        {
            if (strcmp(ssi->tag_name, httpd_tags[tag]) == 0)
  #endif /* LWIP_HTTPD_SSI_RAW */
            {
                ssi->tag_insert_len = httpd_ssi_handler(tag,
                                                        ssi->tag_insert,
                                                        LWIP_HTTPD_MAX_TAG_INSERT_LEN
  #if LWIP_HTTPD_SSI_MULTIPART
                                                        ,
                                                        current_tag_part,
                                                        &ssi->tag_part
  #endif                               /* LWIP_HTTPD_SSI_MULTIPART */
  #if LWIP_HTTPD_FILE_STATE
                                                        ,
                                                        (hs->handle ? hs->handle->state : NULL)
  #endif                               /* LWIP_HTTPD_FILE_STATE */
                                                        );
  #if LWIP_HTTPD_SSI_RAW
                if (ssi->tag_insert_len != HTTPD_SSI_TAG_UNKNOWN)
  #endif                               /* LWIP_HTTPD_SSI_RAW */
                {
                    return;
                }
            }
        }
    }

    /* If we drop out, we were asked to serve a page which contains tags that
     * we don't have a handler for. Merely echo back the tags with an error
     * marker. */
  #define UNKNOWN_TAG1_TEXT    "<b>***UNKNOWN TAG "
  #define UNKNOWN_TAG1_LEN     18
  #define UNKNOWN_TAG2_TEXT    "***</b>"
  #define UNKNOWN_TAG2_LEN     7
    len =
        LWIP_MIN(sizeof(ssi->tag_name),
                 LWIP_MIN(strlen(ssi->tag_name),
                          LWIP_HTTPD_MAX_TAG_INSERT_LEN - (UNKNOWN_TAG1_LEN + UNKNOWN_TAG2_LEN)));
    MEMCPY(ssi->tag_insert, UNKNOWN_TAG1_TEXT, UNKNOWN_TAG1_LEN);
    MEMCPY(&ssi->tag_insert[UNKNOWN_TAG1_LEN], ssi->tag_name, len);
    MEMCPY(&ssi->tag_insert[UNKNOWN_TAG1_LEN + len], UNKNOWN_TAG2_TEXT, UNKNOWN_TAG2_LEN);
    ssi->tag_insert[UNKNOWN_TAG1_LEN + len + UNKNOWN_TAG2_LEN] = 0;

    len = strlen(ssi->tag_insert);
    LWIP_ASSERT("len <= 0xffff", len <= 0xffff);
    ssi->tag_insert_len = (u16_t) len;
}

 #endif                                /* LWIP_HTTPD_SSI */

 #if LWIP_HTTPD_DYNAMIC_HEADERS

/**
 * Generate the relevant HTTP headers for the given filename and write
 * them into the supplied buffer.
 */
static void get_http_headers (struct http_state * hs, const char * uri)
{
    size_t content_type;
    char * tmp;
    char * ext;
    char * vars;

    /* In all cases, the second header we send is the server identification
     * so set it here. */
    hs->hdrs[HDR_STRINGS_IDX_SERVER_NAME]           = g_psHTTPHeaderStrings[HTTP_HDR_SERVER];
    hs->hdrs[HDR_STRINGS_IDX_CONTENT_LEN_KEEPALIVE] = NULL;
    hs->hdrs[HDR_STRINGS_IDX_CONTENT_LEN_NR]        = NULL;

    /* Is this a normal file or the special case we use to send back the
     * default "404: Page not found" response? */
    if (uri == NULL)
    {
        hs->hdrs[HDR_STRINGS_IDX_HTTP_STATUS] = g_psHTTPHeaderStrings[HTTP_HDR_NOT_FOUND];
  #if LWIP_HTTPD_SUPPORT_11_KEEPALIVE
        if (hs->keepalive)
        {
            hs->hdrs[HDR_STRINGS_IDX_CONTENT_TYPE] = g_psHTTPHeaderStrings[DEFAULT_404_HTML_PERSISTENT];
        }
        else
  #endif
        {
            hs->hdrs[HDR_STRINGS_IDX_CONTENT_TYPE] = g_psHTTPHeaderStrings[DEFAULT_404_HTML];
        }

        /* Set up to send the first header string. */
        hs->hdr_index = 0;
        hs->hdr_pos   = 0;

        return;
    }

    /* We are dealing with a particular filename. Look for one other
     *  special case.  We assume that any filename with "404" in it must be
     *  indicative of a 404 server error whereas all other files require
     *  the 200 OK header. */
    if (strstr(uri, "404"))
    {
        hs->hdrs[HDR_STRINGS_IDX_HTTP_STATUS] = g_psHTTPHeaderStrings[HTTP_HDR_NOT_FOUND];
    }
    else if (strstr(uri, "400"))
    {
        hs->hdrs[HDR_STRINGS_IDX_HTTP_STATUS] = g_psHTTPHeaderStrings[HTTP_HDR_BAD_REQUEST];
    }
    else if (strstr(uri, "501"))
    {
        hs->hdrs[HDR_STRINGS_IDX_HTTP_STATUS] = g_psHTTPHeaderStrings[HTTP_HDR_NOT_IMPL];
    }
    else
    {
        hs->hdrs[HDR_STRINGS_IDX_HTTP_STATUS] = g_psHTTPHeaderStrings[HTTP_HDR_OK];
    }

    /* Determine if the URI has any variables and, if so, temporarily remove
     *  them. */
    vars = strchr(uri, '?');
    if (vars)
    {
        *vars = '\0';
    }

    /* Get a pointer to the file extension.  We find this by looking for the
     *  last occurrence of "." in the filename passed. */
    ext = NULL;
    tmp = strchr(uri, '.');
    while (tmp)
    {
        ext = tmp + 1;
        tmp = strchr(ext, '.');
    }

    if (ext != NULL)
    {
        /* Now determine the content type and add the relevant header for that. */
        for (content_type = 0; content_type < NUM_HTTP_HEADERS; content_type++)
        {
            /* Have we found a matching extension? */
            if (!lwip_stricmp(g_psHTTPHeaders[content_type].extension, ext))
            {
                break;
            }
        }
    }
    else
    {
        content_type = NUM_HTTP_HEADERS;
    }

    /* Reinstate the parameter marker if there was one in the original URI. */
    if (vars)
    {
        *vars = '?';
    }

  #if LWIP_HTTPD_OMIT_HEADER_FOR_EXTENSIONLESS_URI

    /* Does the URL passed have any file extension?  If not, we assume it
     * is a special-case URL used for control state notification and we do
     * not send any HTTP headers with the response. */
    if (!ext)
    {
        /* Force the header index to a value indicating that all headers
         * have already been sent. */
        hs->hdr_index = NUM_FILE_HDR_STRINGS;

        return;
    }
  #endif                               /* LWIP_HTTPD_OMIT_HEADER_FOR_EXTENSIONLESS_URI */
    /* Did we find a matching extension? */
    if (content_type < NUM_HTTP_HEADERS)
    {
        /* yes, store it */
        hs->hdrs[HDR_STRINGS_IDX_CONTENT_TYPE] = g_psHTTPHeaders[content_type].content_type;
    }
    else if (!ext)
    {
        /* no, no extension found -> use binary transfer to prevent the browser adding '.txt' on save */
        hs->hdrs[HDR_STRINGS_IDX_CONTENT_TYPE] = HTTP_HDR_APP;
    }
    else
    {
        /* No - use the default, plain text file type. */
        hs->hdrs[HDR_STRINGS_IDX_CONTENT_TYPE] = HTTP_HDR_DEFAULT_TYPE;
    }

    /* Set up to send the first header string. */
    hs->hdr_index = 0;
    hs->hdr_pos   = 0;
}

/* Add content-length header? */
static void get_http_content_length (struct http_state * hs)
{
    u8_t add_content_len = 0;

    LWIP_ASSERT("already been here?", hs->hdrs[HDR_STRINGS_IDX_CONTENT_LEN_KEEPALIVE] == NULL);

    add_content_len = 0;
  #if LWIP_HTTPD_SSI
    if (hs->ssi == NULL)               /* @todo: get maximum file length from SSI */
  #endif /* LWIP_HTTPD_SSI */
    {
        if ((hs->handle != NULL) && (hs->handle->flags & FS_FILE_FLAGS_HEADER_PERSISTENT))
        {
            add_content_len = 1;
        }
    }

    if (add_content_len)
    {
        size_t len;
        lwip_itoa(hs->hdr_content_len, (size_t) LWIP_HTTPD_MAX_CONTENT_LEN_SIZE, hs->handle->len);
        len = strlen(hs->hdr_content_len);
        if (len <= LWIP_HTTPD_MAX_CONTENT_LEN_SIZE - LWIP_HTTPD_MAX_CONTENT_LEN_OFFSET)
        {
            SMEMCPY(&hs->hdr_content_len[len], CRLF, 3);
            hs->hdrs[HDR_STRINGS_IDX_CONTENT_LEN_NR] = hs->hdr_content_len;
        }
        else
        {
            add_content_len = 0;
        }
    }

  #if LWIP_HTTPD_SUPPORT_11_KEEPALIVE
    if (add_content_len)
    {
        hs->hdrs[HDR_STRINGS_IDX_CONTENT_LEN_KEEPALIVE] = g_psHTTPHeaderStrings[HTTP_HDR_KEEPALIVE_LEN];
    }
    else
    {
        hs->hdrs[HDR_STRINGS_IDX_CONTENT_LEN_KEEPALIVE] = g_psHTTPHeaderStrings[HTTP_HDR_CONN_CLOSE];
        hs->keepalive = 0;
    }

  #else                                /* LWIP_HTTPD_SUPPORT_11_KEEPALIVE */
    if (add_content_len)
    {
        hs->hdrs[HDR_STRINGS_IDX_CONTENT_LEN_KEEPALIVE] = g_psHTTPHeaderStrings[HTTP_HDR_CONTENT_LENGTH];
    }
  #endif                               /* LWIP_HTTPD_SUPPORT_11_KEEPALIVE */
}

/** Sub-function of http_send(): send dynamic headers
 *
 * @returns: - HTTP_NO_DATA_TO_SEND: no new data has been enqueued
 *           - HTTP_DATA_TO_SEND_CONTINUE: continue with sending HTTP body
 *           - HTTP_DATA_TO_SEND_BREAK: data has been enqueued, headers pending,
 *                                      so don't send HTTP body yet
 *           - HTTP_DATA_TO_SEND_FREED: http_state and pcb are already freed
 */
static u8_t http_send_headers (struct altcp_pcb * pcb, struct http_state * hs)
{
    err_t err;
    u16_t len;
    u8_t  data_to_send = HTTP_NO_DATA_TO_SEND;
    u16_t hdrlen, sendlen;

    if (hs->hdrs[HDR_STRINGS_IDX_CONTENT_LEN_KEEPALIVE] == NULL)
    {
        /* set up "content-length" and "connection:" headers */
        get_http_content_length(hs);
    }

    /* How much data can we send? */
    len     = altcp_sndbuf(pcb);
    sendlen = len;

    while (len && (hs->hdr_index < NUM_FILE_HDR_STRINGS) && sendlen)
    {
        const void * ptr;
        u16_t        old_sendlen;
        u8_t         apiflags;

        /* How much do we have to send from the current header? */
        hdrlen = (u16_t) strlen(hs->hdrs[hs->hdr_index]);

        /* How much of this can we send? */
        sendlen = (len < (hdrlen - hs->hdr_pos)) ? len : (hdrlen - hs->hdr_pos);

        /* Send this amount of data or as much as we can given memory
         * constraints. */
        ptr         = (const void *) (hs->hdrs[hs->hdr_index] + hs->hdr_pos);
        old_sendlen = sendlen;
        apiflags    = HTTP_IS_HDR_VOLATILE(hs, ptr);
        if (hs->hdr_index == HDR_STRINGS_IDX_CONTENT_LEN_NR)
        {
            /* content-length is always volatile */
            apiflags |= TCP_WRITE_FLAG_COPY;
        }

        if (hs->hdr_index < NUM_FILE_HDR_STRINGS - 1)
        {
            apiflags |= TCP_WRITE_FLAG_MORE;
        }

        err = http_write(pcb, ptr, &sendlen, apiflags);
        if ((err == ERR_OK) && (old_sendlen != sendlen))
        {
            /* Remember that we added some more data to be transmitted. */
            data_to_send = HTTP_DATA_TO_SEND_CONTINUE;
        }
        else if (err != ERR_OK)
        {
            /* special case: http_write does not try to send 1 byte */
            sendlen = 0;
        }

        /* Fix up the header position for the next time round. */
        hs->hdr_pos += sendlen;
        len         -= sendlen;

        /* Have we finished sending this string? */
        if (hs->hdr_pos == hdrlen)
        {
            /* Yes - move on to the next one */
            hs->hdr_index++;

            /* skip headers that are NULL (not all headers are required) */
            while ((hs->hdr_index < NUM_FILE_HDR_STRINGS) &&
                   (hs->hdrs[hs->hdr_index] == NULL))
            {
                hs->hdr_index++;
            }

            hs->hdr_pos = 0;
        }
    }

    if ((hs->hdr_index >= NUM_FILE_HDR_STRINGS) && (hs->file == NULL))
    {
        /* When we are at the end of the headers, check for data to send
         * instead of waiting for ACK from remote side to continue
         * (which would happen when sending files from async read). */
        if (http_check_eof(pcb, hs))
        {
            data_to_send = HTTP_DATA_TO_SEND_BREAK;
        }
        else
        {

            /* At this point, for non-keepalive connections, hs is deallocated an
             * pcb is closed. */
            return HTTP_DATA_TO_SEND_FREED;
        }
    }

    /* If we get here and there are still header bytes to send, we send
     * the header information we just wrote immediately. If there are no
     * more headers to send, but we do have file data to send, drop through
     * to try to send some file data too. */
    if ((hs->hdr_index < NUM_FILE_HDR_STRINGS) || !hs->file)
    {
        LWIP_DEBUGF(HTTPD_DEBUG, ("tcp_output\n"));

        return HTTP_DATA_TO_SEND_BREAK;
    }

    return data_to_send;
}

 #endif                                /* LWIP_HTTPD_DYNAMIC_HEADERS */

/** Sub-function of http_send(): end-of-file (or block) is reached,
 * either close the file or read the next block (if supported).
 *
 * @returns: 0 if the file is finished or no data has been read
 *           1 if the file is not finished and data has been read
 */
static u8_t http_check_eof (struct altcp_pcb * pcb, struct http_state * hs)
{
    int bytes_left;
 #if LWIP_HTTPD_DYNAMIC_FILE_READ
    int count;
  #ifdef HTTPD_MAX_WRITE_LEN
    int max_write_len;
  #endif                               /* HTTPD_MAX_WRITE_LEN */
 #endif                                /* LWIP_HTTPD_DYNAMIC_FILE_READ */

    /* Do we have a valid file handle? */
    if (hs->handle == NULL)
    {
        /* No - close the connection. */
        http_eof(pcb, hs);

        return 0;
    }

    bytes_left = fs_bytes_left((struct fs_file *) hs->handle);
    if (bytes_left <= 0)
    {
        /* We reached the end of the file so this request is done. */
        LWIP_DEBUGF(HTTPD_DEBUG, ("End of file.\n"));
        http_eof(pcb, hs);

        return 0;
    }

 #if LWIP_HTTPD_DYNAMIC_FILE_READ

    /* Do we already have a send buffer allocated? */
    if (hs->buf)
    {
        /* Yes - get the length of the buffer */
        count = LWIP_MIN(hs->buf_len, bytes_left);
    }
    else
    {
        /* We don't have a send buffer so allocate one now */
        count = altcp_sndbuf(pcb);
        if (bytes_left < count)
        {
            count = bytes_left;
        }

  #ifdef HTTPD_MAX_WRITE_LEN

        /* Additional limitation: e.g. don't enqueue more than 2*mss at once */
        max_write_len = HTTPD_MAX_WRITE_LEN(pcb);
        if (count > max_write_len)
        {
            count = max_write_len;
        }
  #endif                               /* HTTPD_MAX_WRITE_LEN */
        do
        {
            hs->buf = (char *) mem_malloc((mem_size_t) count);
            if (hs->buf != NULL)
            {
                hs->buf_len = count;
                break;
            }

            count = count / 2;
        } while (count > 100);

        /* Did we get a send buffer? If not, return immediately. */
        if (hs->buf == NULL)
        {
            LWIP_DEBUGF(HTTPD_DEBUG, ("No buff\n"));

            return 0;
        }
    }

    /* Read a block of data from the file. */
    LWIP_DEBUGF(HTTPD_DEBUG, ("Trying to read %d bytes.\n", count));

  #if LWIP_HTTPD_FS_ASYNC_READ
    count = fs_read_async(hs->handle, hs->buf, count, http_continue, hs);
  #else                                /* LWIP_HTTPD_FS_ASYNC_READ */
    count = fs_read(hs->handle, hs->buf, count);
  #endif /* LWIP_HTTPD_FS_ASYNC_READ */
    if (count < 0)
    {
        if (count == FS_READ_DELAYED)
        {

            /* Delayed read, wait for FS to unblock us */
            return 0;
        }

        /* We reached the end of the file so this request is done.
         * @todo: close here for HTTP/1.1 when reading file fails */
        LWIP_DEBUGF(HTTPD_DEBUG, ("End of file.\n"));
        http_eof(pcb, hs);

        return 0;
    }

    /* Set up to send the block of data we just read */
    LWIP_DEBUGF(HTTPD_DEBUG, ("Read %d bytes.\n", count));
    hs->left = count;
    hs->file = hs->buf;
  #if LWIP_HTTPD_SSI
    if (hs->ssi)
    {
        hs->ssi->parse_left = count;
        hs->ssi->parsed     = hs->buf;
    }
  #endif                               /* LWIP_HTTPD_SSI */
 #else /* LWIP_HTTPD_DYNAMIC_FILE_READ */
    LWIP_ASSERT("SSI and DYNAMIC_HEADERS turned off but eof not reached", 0);
 #endif                                /* LWIP_HTTPD_SSI || LWIP_HTTPD_DYNAMIC_HEADERS */
    return 1;
}

/** Sub-function of http_send(): This is the normal send-routine for non-ssi files
 *
 * @returns: - 1: data has been written (so call tcp_output)
 *           - 0: no data has been written (no need to call tcp_output)
 */
static u8_t http_send_data_nonssi (struct altcp_pcb * pcb, struct http_state * hs)
{
    err_t err;
    u16_t len;
    u8_t  data_to_send = 0;

    /* We are not processing an SHTML file so no tag checking is necessary.
     * Just send the data as we received it from the file. */
    len = (u16_t) LWIP_MIN(hs->left, 0xffff);

    err = http_write(pcb, hs->file, &len, HTTP_IS_DATA_VOLATILE(hs));
    if (err == ERR_OK)
    {
        data_to_send = 1;
        hs->file    += len;
        hs->left    -= len;
    }

    return data_to_send;
}

 #if LWIP_HTTPD_SSI

/** Sub-function of http_send(): This is the send-routine for ssi files
 *
 * @returns: - 1: data has been written (so call tcp_output)
 *           - 0: no data has been written (no need to call tcp_output)
 */
static u8_t http_send_data_ssi (struct altcp_pcb * pcb, struct http_state * hs)
{
    err_t err = ERR_OK;
    u16_t len;
    u8_t  data_to_send = 0;
    u8_t  tag_type;

    struct http_ssi_state * ssi = hs->ssi;
    LWIP_ASSERT("ssi != NULL", ssi != NULL);

    /* We are processing an SHTML file so need to scan for tags and replace
     * them with insert strings. We need to be careful here since a tag may
     * straddle the boundary of two blocks read from the file and we may also
     * have to split the insert string between two tcp_write operations. *//* How much data could we send? */
    len = altcp_sndbuf(pcb);

    /* Do we have remaining data to send before parsing more? */
    if (ssi->parsed > hs->file)
    {
        len = (u16_t) LWIP_MIN(ssi->parsed - hs->file, 0xffff);

        err = http_write(pcb, hs->file, &len, HTTP_IS_DATA_VOLATILE(hs));
        if (err == ERR_OK)
        {
            data_to_send = 1;
            hs->file    += len;
            hs->left    -= len;
        }

        /* If the send buffer is full, return now. */
        if (altcp_sndbuf(pcb) == 0)
        {
            return data_to_send;
        }
    }

    LWIP_DEBUGF(HTTPD_DEBUG, ("State %d, %d left\n", ssi->tag_state, (int) ssi->parse_left));

    /* We have sent all the data that was already parsed so continue parsing
     * the buffer contents looking for SSI tags. */
    while (((ssi->tag_state == TAG_SENDING) || ssi->parse_left) && (err == ERR_OK))
    {
        if (len == 0)
        {
            return data_to_send;
        }

        switch (ssi->tag_state)
        {
            case TAG_NONE:
            {
                /* We are not currently processing an SSI tag so scan for the
                 * start of the lead-in marker. */
                for (tag_type = 0; tag_type < LWIP_ARRAYSIZE(http_ssi_tag_desc); tag_type++)
                {
                    if (*ssi->parsed == http_ssi_tag_desc[tag_type].lead_in[0])
                    {
                        /* We found what could be the lead-in for a new tag so change
                         * state appropriately. */
                        ssi->tag_type  = tag_type;
                        ssi->tag_state = TAG_LEADIN;
                        ssi->tag_index = 1;
  #if !LWIP_HTTPD_SSI_INCLUDE_TAG
                        ssi->tag_started = ssi->parsed;
  #endif                               /* !LWIP_HTTPD_SSI_INCLUDE_TAG */
                        break;
                    }
                }

                /* Move on to the next character in the buffer */
                ssi->parse_left--;
                ssi->parsed++;
                break;
            }

            case TAG_LEADIN:
            {
                /* We are processing the lead-in marker, looking for the start of
                 * the tag name. */

                /* Have we reached the end of the leadin? */
                if (http_ssi_tag_desc[ssi->tag_type].lead_in[ssi->tag_index] == 0)
                {
                    ssi->tag_index = 0;
                    ssi->tag_state = TAG_FOUND;
                }
                else
                {
                    /* Have we found the next character we expect for the tag leadin? */
                    if (*ssi->parsed == http_ssi_tag_desc[ssi->tag_type].lead_in[ssi->tag_index])
                    {
                        /* Yes - move to the next one unless we have found the complete
                         * leadin, in which case we start looking for the tag itself */
                        ssi->tag_index++;
                    }
                    else
                    {
                        /* We found an unexpected character so this is not a tag. Move
                         * back to idle state. */
                        ssi->tag_state = TAG_NONE;
                    }

  #if LWIP_HTTPD_DYNAMIC_FILE_READ && !LWIP_HTTPD_SSI_INCLUDE_TAG
                    if ((ssi->tag_state == TAG_NONE) &&
                        (ssi->parsed - hs->file < ssi->tag_index))
                    {
                        for (u16_t i = 0; i < ssi->tag_index; i++)
                        {
                            ssi->tag_insert[i] = http_ssi_tag_desc[ssi->tag_type].lead_in[i];
                        }

                        ssi->tag_insert_len = ssi->tag_index;
                        hs->file           += ssi->parsed - hs->file;
                        hs->left           -= ssi->parsed - hs->file;
                        ssi->tag_end        = hs->file;
                        ssi->tag_index      = 0;
                        ssi->tag_state      = TAG_SENDING;
                        break;
                    }
  #endif

                    /* Move on to the next character in the buffer */
                    ssi->parse_left--;
                    ssi->parsed++;
                }

                break;
            }

            case TAG_FOUND:
            {
                /* We are reading the tag name, looking for the start of the
                 * lead-out marker and removing any whitespace found. */

                /* Remove leading whitespace between the tag leading and the first
                 * tag name character. */
                if ((ssi->tag_index == 0) && ((*ssi->parsed == ' ') ||
                                              (*ssi->parsed == '\t') || (*ssi->parsed == '\n') ||
                                              (*ssi->parsed == '\r')))
                {
                    /* Move on to the next character in the buffer */
                    ssi->parse_left--;
                    ssi->parsed++;
                    break;
                }

                /* Have we found the end of the tag name? This is signalled by
                 * us finding the first leadout character or whitespace */
                if ((*ssi->parsed == http_ssi_tag_desc[ssi->tag_type].lead_out[0]) ||
                    (*ssi->parsed == ' ') || (*ssi->parsed == '\t') ||
                    (*ssi->parsed == '\n') || (*ssi->parsed == '\r'))
                {
                    if (ssi->tag_index == 0)
                    {
                        /* We read a zero length tag so ignore it. */
                        ssi->tag_state = TAG_NONE;
                    }
                    else
                    {
                        /* We read a non-empty tag so go ahead and look for the
                         * leadout string. */
                        ssi->tag_state = TAG_LEADOUT;
                        LWIP_ASSERT("ssi->tag_index <= 0xff", ssi->tag_index <= 0xff);
                        ssi->tag_name_len             = (u8_t) ssi->tag_index;
                        ssi->tag_name[ssi->tag_index] = '\0';
                        if (*ssi->parsed == http_ssi_tag_desc[ssi->tag_type].lead_out[0])
                        {
                            ssi->tag_index = 1;
                        }
                        else
                        {
                            ssi->tag_index = 0;
                        }
                    }
                }
                else
                {
                    /* This character is part of the tag name so save it */
                    if (ssi->tag_index < LWIP_HTTPD_MAX_TAG_NAME_LEN)
                    {
                        ssi->tag_name[ssi->tag_index++] = *ssi->parsed;
                    }
                    else
                    {
                        /* The tag was too long so ignore it. */
                        ssi->tag_state = TAG_NONE;
                    }
                }

                /* Move on to the next character in the buffer */
                ssi->parse_left--;
                ssi->parsed++;

                break;
            }

            /* We are looking for the end of the lead-out marker. */
            case TAG_LEADOUT:
            {
                /* Remove leading whitespace between the tag leading and the first
                 * tag leadout character. */
                if ((ssi->tag_index == 0) && ((*ssi->parsed == ' ') ||
                                              (*ssi->parsed == '\t') || (*ssi->parsed == '\n') ||
                                              (*ssi->parsed == '\r')))
                {
                    /* Move on to the next character in the buffer */
                    ssi->parse_left--;
                    ssi->parsed++;
                    break;
                }

                /* Have we found the next character we expect for the tag leadout? */
                if (*ssi->parsed == http_ssi_tag_desc[ssi->tag_type].lead_out[ssi->tag_index])
                {
                    /* Yes - move to the next one unless we have found the complete
                     * leadout, in which case we need to call the client to process
                     * the tag. */

                    /* Move on to the next character in the buffer */
                    ssi->parse_left--;
                    ssi->parsed++;
                    ssi->tag_index++;

                    if (http_ssi_tag_desc[ssi->tag_type].lead_out[ssi->tag_index] == 0)
                    {
                        /* Call the client to ask for the insert string for the
                         * tag we just found. */
  #if LWIP_HTTPD_SSI_MULTIPART
                        ssi->tag_part = 0; /* start with tag part 0 */
  #endif /* LWIP_HTTPD_SSI_MULTIPART */
                        get_tag_insert(hs);

                        /* Next time through, we are going to be sending data
                         * immediately, either the end of the block we start
                         * sending here or the insert string. */
                        ssi->tag_index = 0;
                        ssi->tag_state = TAG_SENDING;
                        ssi->tag_end   = ssi->parsed;
  #if !LWIP_HTTPD_SSI_INCLUDE_TAG
                        ssi->parsed = ssi->tag_started;
  #endif                               /* !LWIP_HTTPD_SSI_INCLUDE_TAG*/

                        /* If there is any unsent data in the buffer prior to the
                         * tag, we need to send it now. */
                        if (ssi->tag_end > hs->file)
                        {
                            /* How much of the data can we send? */
  #if LWIP_HTTPD_SSI_INCLUDE_TAG
                            len = (u16_t) LWIP_MIN(ssi->tag_end - hs->file, 0xffff);
  #else                                /* LWIP_HTTPD_SSI_INCLUDE_TAG*/
                            /* we would include the tag in sending */
                            len = (u16_t) LWIP_MIN(ssi->tag_started - hs->file, 0xffff);
  #endif /* LWIP_HTTPD_SSI_INCLUDE_TAG*/

                            err = http_write(pcb, hs->file, &len, HTTP_IS_DATA_VOLATILE(hs));
                            if (err == ERR_OK)
                            {
                                data_to_send = 1;
  #if !LWIP_HTTPD_SSI_INCLUDE_TAG
                                if (ssi->tag_started <= hs->file)
                                {
                                    /* pretend to have sent the tag, too */
                                    len += (u16_t) (ssi->tag_end - ssi->tag_started);
                                }
  #endif                               /* !LWIP_HTTPD_SSI_INCLUDE_TAG*/
                                hs->file += len;
                                hs->left -= len;
                            }
                        }
                    }
                }
                else
                {
                    /* We found an unexpected character so this is not a tag. Move
                     * back to idle state. */
                    ssi->parse_left--;
                    ssi->parsed++;
                    ssi->tag_state = TAG_NONE;
                }

                break;
            }

            /*
             * We have found a valid tag and are in the process of sending
             * data as a result of that discovery. We send either remaining data
             * from the file prior to the insert point or the insert string itself.
             */
            case TAG_SENDING:

                /* Do we have any remaining file data to send from the buffer prior
                 * to the tag? */
                if (ssi->tag_end > hs->file)
                {
                    /* How much of the data can we send? */
  #if LWIP_HTTPD_SSI_INCLUDE_TAG
                    len = (u16_t) LWIP_MIN(ssi->tag_end - hs->file, 0xffff);
  #else                                /* LWIP_HTTPD_SSI_INCLUDE_TAG*/
                    LWIP_ASSERT("hs->started >= hs->file", ssi->tag_started >= hs->file);

                    /* we would include the tag in sending */
                    len = (u16_t) LWIP_MIN(ssi->tag_started - hs->file, 0xffff);
  #endif                               /* LWIP_HTTPD_SSI_INCLUDE_TAG*/
                    if (len != 0)
                    {
                        err = http_write(pcb, hs->file, &len, HTTP_IS_DATA_VOLATILE(hs));
                    }
                    else
                    {
                        err = ERR_OK;
                    }

                    if (err == ERR_OK)
                    {
                        data_to_send = 1;
  #if !LWIP_HTTPD_SSI_INCLUDE_TAG
                        if (ssi->tag_started <= hs->file)
                        {
                            /* pretend to have sent the tag, too */
                            len += (u16_t) (ssi->tag_end - ssi->tag_started);
                        }
  #endif                               /* !LWIP_HTTPD_SSI_INCLUDE_TAG*/
                        hs->file += len;
                        hs->left -= len;
                    }
                }
                else
                {
  #if LWIP_HTTPD_SSI_MULTIPART
                    if (ssi->tag_index >= ssi->tag_insert_len)
                    {
                        /* Did the last SSIHandler have more to send? */
                        if (ssi->tag_part != HTTPD_LAST_TAG_PART)
                        {
                            /* If so, call it again */
                            ssi->tag_index = 0;
                            get_tag_insert(hs);
                        }
                    }
  #endif                               /* LWIP_HTTPD_SSI_MULTIPART */

                    /* Do we still have insert data left to send? */
                    if (ssi->tag_index < ssi->tag_insert_len)
                    {
                        /* We are sending the insert string itself. How much of the
                         * insert can we send? */
                        len = (ssi->tag_insert_len - ssi->tag_index);

                        /* Note that we set the copy flag here since we only have a
                         * single tag insert buffer per connection. If we don't do
                         * this, insert corruption can occur if more than one insert
                         * is processed before we call tcp_output. */
                        err = http_write(pcb, &(ssi->tag_insert[ssi->tag_index]), &len, HTTP_IS_TAG_VOLATILE(hs));
                        if (err == ERR_OK)
                        {
                            data_to_send    = 1;
                            ssi->tag_index += len;

                            /* Don't return here: keep on sending data */
                        }
                    }
                    else
                    {
  #if LWIP_HTTPD_SSI_MULTIPART
                        if (ssi->tag_part == HTTPD_LAST_TAG_PART)
  #endif                               /* LWIP_HTTPD_SSI_MULTIPART */
                        {
                            /* We have sent all the insert data so go back to looking for
                             * a new tag. */
                            LWIP_DEBUGF(HTTPD_DEBUG, ("Everything sent.\n"));
                            ssi->tag_index = 0;
                            ssi->tag_state = TAG_NONE;
  #if !LWIP_HTTPD_SSI_INCLUDE_TAG
                            ssi->parsed = ssi->tag_end;
  #endif                               /* !LWIP_HTTPD_SSI_INCLUDE_TAG*/
                        }
                    }

                    break;

                    default:
                    {
                        break;
                    }
                }
        }
    }

    /* If we drop out of the end of the for loop, this implies we must have
     * file data to send so send it now. In TAG_SENDING state, we've already
     * handled this so skip the send if that's the case. */
    if ((ssi->tag_state != TAG_SENDING) && (ssi->parsed > hs->file))
    {
  #if LWIP_HTTPD_DYNAMIC_FILE_READ && !LWIP_HTTPD_SSI_INCLUDE_TAG
        if ((ssi->tag_state != TAG_NONE) && (ssi->tag_started > ssi->tag_end))
        {
            /* If we found tag on the edge of the read buffer: just throw away the first part
             * (we have copied/saved everything required for parsing on later). */
            len              = (u16_t) (ssi->tag_started - hs->file);
            hs->left        -= (ssi->parsed - ssi->tag_started);
            ssi->parsed      = ssi->tag_started;
            ssi->tag_started = hs->buf;
        }
        else
  #endif                               /* LWIP_HTTPD_DYNAMIC_FILE_READ && !LWIP_HTTPD_SSI_INCLUDE_TAG */
        {
            len = (u16_t) LWIP_MIN(ssi->parsed - hs->file, 0xffff);
        }

        err = http_write(pcb, hs->file, &len, HTTP_IS_DATA_VOLATILE(hs));
        if (err == ERR_OK)
        {
            data_to_send = 1;
            hs->file    += len;
            hs->left    -= len;
        }
    }

    return data_to_send;
}

 #endif                                /* LWIP_HTTPD_SSI */

/**
 * Try to send more data on this pcb.
 *
 * @param pcb the pcb to send data
 * @param hs connection state
 */
static u8_t http_send (struct altcp_pcb * pcb, struct http_state * hs)
{
    u8_t data_to_send = HTTP_NO_DATA_TO_SEND;

    LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE,
                ("http_send: pcb=%p hs=%p left=%d\n", (void *) pcb,
                 (void *) hs, hs != NULL ? (int) hs->left : 0));

 #if LWIP_HTTPD_SUPPORT_POST && LWIP_HTTPD_POST_MANUAL_WND
    if (hs->unrecved_bytes != 0)
    {
        return 0;
    }
 #endif                                /* LWIP_HTTPD_SUPPORT_POST && LWIP_HTTPD_POST_MANUAL_WND */

    /* If we were passed a NULL state structure pointer, ignore the call. */
    if (hs == NULL)
    {
        return 0;
    }

 #if LWIP_HTTPD_FS_ASYNC_READ

    /* Check if we are allowed to read from this file.
     * (e.g. SSI might want to delay sending until data is available) */
    if (!fs_is_file_ready(hs->handle, http_continue, hs))
    {
        return 0;
    }
 #endif                                /* LWIP_HTTPD_FS_ASYNC_READ */

 #if LWIP_HTTPD_DYNAMIC_HEADERS

    /* Do we have any more header data to send for this file? */
    if (hs->hdr_index < NUM_FILE_HDR_STRINGS)
    {
        data_to_send = http_send_headers(pcb, hs);
        if ((data_to_send == HTTP_DATA_TO_SEND_FREED) ||
            ((data_to_send != HTTP_DATA_TO_SEND_CONTINUE) &&
             (hs->hdr_index < NUM_FILE_HDR_STRINGS)))
        {
            return data_to_send;
        }
    }
 #endif                                /* LWIP_HTTPD_DYNAMIC_HEADERS */

    /* Have we run out of file data to send? If so, we need to read the next
     * block from the file. */
    if (hs->left == 0)
    {
        if (!http_check_eof(pcb, hs))
        {
            return 0;
        }
    }

 #if LWIP_HTTPD_SSI
    if (hs->ssi)
    {
        data_to_send = http_send_data_ssi(pcb, hs);
    }
    else
 #endif                                /* LWIP_HTTPD_SSI */
    {
        data_to_send = http_send_data_nonssi(pcb, hs);
    }

    if ((hs->left == 0) && (fs_bytes_left((struct fs_file *) hs->handle) <= 0))
    {
        /* We reached the end of the file so this request is done.
         * This adds the FIN flag right into the last data segment. */
        LWIP_DEBUGF(HTTPD_DEBUG, ("End of file.\n"));
        http_eof(pcb, hs);

        return 0;
    }

    LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("send_data end.\n"));

    return data_to_send;
}

 #if LWIP_HTTPD_SUPPORT_EXTSTATUS

/** Initialize a http connection with a file to send for an error message
 *
 * @param hs http connection state
 * @param error_nr HTTP error number
 * @return ERR_OK if file was found and hs has been initialized correctly
 *         another err_t otherwise
 */
static err_t http_find_error_file (struct http_state * hs, u16_t error_nr)
{
    const char * uri, * uri1, * uri2, * uri3;

    if (error_nr == 501)
    {
        uri1 = "/501.html";
        uri2 = "/501.htm";
        uri3 = "/501.shtml";
    }
    else
    {
        /* 400 (bad request is the default) */
        uri1 = "/400.html";
        uri2 = "/400.htm";
        uri3 = "/400.shtml";
    }

    if (fs_open(&hs->file_handle, uri1) == ERR_OK)
    {
        uri = uri1;
    }
    else if (fs_open(&hs->file_handle, uri2) == ERR_OK)
    {
        uri = uri2;
    }
    else if (fs_open(&hs->file_handle, uri3) == ERR_OK)
    {
        uri = uri3;
    }
    else
    {
        LWIP_DEBUGF(HTTPD_DEBUG,
                    ("Error page for error %"U16_F " not found\n",
                     error_nr));

        return ERR_ARG;
    }

    return http_init_file(hs, &hs->file_handle, 0, uri, 0, NULL);
}

 #else                                 /* LWIP_HTTPD_SUPPORT_EXTSTATUS */
  #define http_find_error_file(hs, error_nr)    ERR_ARG
 #endif /* LWIP_HTTPD_SUPPORT_EXTSTATUS */

/**
 * Get the file struct for a 404 error page.
 * Tries some file names and returns NULL if none found.
 *
 * @param uri pointer that receives the actual file name URI
 * @return file struct for the error page or NULL no matching file was found
 */
static struct fs_file * http_get_404_file (struct http_state * hs, const char ** uri)
{
    err_t err;

    *uri = "/404.html";
    err  = fs_open((struct fs_file *) &hs->file_handle, *uri);
    if (err != ERR_OK)
    {
        /* 404.html doesn't exist. Try 404.htm instead. */
        *uri = "/404.htm";
        err  = fs_open((struct fs_file *) &hs->file_handle, *uri);
        if (err != ERR_OK)
        {
            /* 404.htm doesn't exist either. Try 404.shtml instead. */
            *uri = "/404.shtml";
            err  = fs_open((struct fs_file *) &hs->file_handle, *uri);
            if (err != ERR_OK)
            {
                /* 404.htm doesn't exist either. Indicate to the caller that it should
                 * send back a default 404 page.
                 */
                *uri = NULL;

                return NULL;
            }
        }
    }

    return (struct fs_file *) &hs->file_handle;
}

 #if LWIP_HTTPD_SUPPORT_POST
static err_t http_handle_post_finished (struct http_state * hs)
{
  #if LWIP_HTTPD_POST_MANUAL_WND

    /* Prevent multiple calls to httpd_post_finished, since it might have already
     * been called before from httpd_post_data_recved(). */
    if (hs->post_finished)
    {
        return ERR_OK;
    }
    hs->post_finished = 1;
  #endif                               /* LWIP_HTTPD_POST_MANUAL_WND */
    /* application error or POST finished */
    /* NULL-terminate the buffer */
    http_uri_buf[0] = 0;
    httpd_post_finished(hs, http_uri_buf, LWIP_HTTPD_URI_BUF_LEN);

    return http_find_file(hs, http_uri_buf, 0);
}

/** Pass received POST body data to the application and correctly handle
 * returning a response document or closing the connection.
 * ATTENTION: The application is responsible for the pbuf now, so don't free it!
 *
 * @param hs http connection state
 * @param p pbuf to pass to the application
 * @return ERR_OK if passed successfully, another err_t if the response file
 *         hasn't been found (after POST finished)
 */
static err_t http_post_rxpbuf (struct http_state * hs, struct pbuf * p)
{
    err_t err;

    if (p != NULL)
    {
        /* adjust remaining Content-Length */
        if (hs->post_content_len_left < p->tot_len)
        {
            hs->post_content_len_left = 0;
        }
        else
        {
            hs->post_content_len_left -= p->tot_len;
        }
    }

  #if LWIP_HTTPD_SUPPORT_POST && LWIP_HTTPD_POST_MANUAL_WND

    /* prevent connection being closed if httpd_post_data_recved() is called nested */
    hs->unrecved_bytes++;
  #endif
    if (p != NULL)
    {
        LWIP_DEBUGF(HTTPD_DEBUG,
                    ("[POST_RXPBUF] Calling httpd_post_receive_data, tot_len=%d, post_content_len_left=%d",
                     p->tot_len, (int) hs->post_content_len_left));
        err = httpd_post_receive_data(hs, p);
        LWIP_DEBUGF(HTTPD_DEBUG, ("[POST_RXPBUF] httpd_post_receive_data returned, err=%d", err));
    }
    else
    {
        LWIP_DEBUGF(HTTPD_DEBUG, ("[POST_RXPBUF] Called with NULL pbuf"));
        err = ERR_OK;
    }

  #if LWIP_HTTPD_SUPPORT_POST && LWIP_HTTPD_POST_MANUAL_WND
    hs->unrecved_bytes--;
  #endif
    if (err != ERR_OK)
    {
        /* Ignore remaining content in case of application error */
        hs->post_content_len_left = 0;
    }

    if (hs->post_content_len_left == 0)
    {
  #if LWIP_HTTPD_SUPPORT_POST && LWIP_HTTPD_POST_MANUAL_WND
        if (hs->unrecved_bytes != 0)
        {
            return ERR_OK;
        }
  #endif                               /* LWIP_HTTPD_SUPPORT_POST && LWIP_HTTPD_POST_MANUAL_WND */
        /* application error or POST finished */
        LWIP_DEBUGF(HTTPD_DEBUG, ("[POST_RXPBUF] post_content_len_left=0, calling http_handle_post_finished"));

        return http_handle_post_finished(hs);
    }

    return ERR_OK;
}

/** Handle a post request. Called from http_parse_request when method 'POST'
 * is found.
 *
 * @param p The input pbuf (containing the POST header and body).
 * @param hs The http connection state.
 * @param data HTTP request (header and part of body) from input pbuf(s).
 * @param data_len Size of 'data'.
 * @param uri The HTTP URI parsed from input pbuf(s).
 * @param uri_end Pointer to the end of 'uri' (here, the rest of the HTTP
 *                header starts).
 * @return ERR_OK: POST correctly parsed and accepted by the application.
 *         ERR_INPROGRESS: POST not completely parsed (no error yet)
 *         another err_t: Error parsing POST or denied by the application
 */
static err_t http_post_request (struct pbuf       * inp,
                                struct http_state * hs,
                                char              * data,
                                u16_t               data_len,
                                char              * uri,
                                char              * uri_end)
{
    err_t err;

    /* search for end-of-header (first double-CRLF) */
    char * crlfcrlf = lwip_strnstr(uri_end + 1, CRLF CRLF, (size_t) (data_len - (uri_end + 1 - data)));
  #if SUPPORT_FSP_RM_FS_W
    unsigned long total_bytes;
    unsigned long min_free_bytes;
    signed int    used_blocks;
    unsigned long used_bytes;
    unsigned long free_bytes;
    unsigned long expected_file_size;
  #endif

    if (crlfcrlf != NULL)
    {
        /* search for "Content-Length: " */
  #define HTTP_HDR_CONTENT_LEN                  "Content-Length: "
  #define HTTP_HDR_CONTENT_LEN_LEN              16
  #define HTTP_HDR_CONTENT_LEN_DIGIT_MAX_LEN    10
        char * scontent_len = lwip_strnstr(uri_end + 1, HTTP_HDR_CONTENT_LEN, (size_t) (crlfcrlf - (uri_end + 1)));
        if (scontent_len != NULL)
        {
            char * scontent_len_end = lwip_strnstr(scontent_len + HTTP_HDR_CONTENT_LEN_LEN,
                                                   CRLF,
                                                   HTTP_HDR_CONTENT_LEN_DIGIT_MAX_LEN);
            if (scontent_len_end != NULL)
            {
                int    content_len;
                char * content_len_num = scontent_len + HTTP_HDR_CONTENT_LEN_LEN;
                content_len = atoi(content_len_num);
                if (content_len == 0)
                {
                    /* if atoi returns 0 on error, fix this */
                    if ((content_len_num[0] != '0') || (content_len_num[1] != '\r'))
                    {
                        content_len = -1;
                    }
                }

  #if SUPPORT_FSP_RM_FS_W
                if (content_len >= MAX_UPLOAD_FILE_SIZE)
                {
                    return ERR_VAL;
                }

                /* Check filesystem space before upload (5% threshold) */
                expected_file_size = (unsigned long) content_len;

                /* Set total upload size for progress tracking */
                upload_total_bytes      = expected_file_size;
                upload_received_bytes   = 0;
                upload_progress_percent = 0;

                /* Calculate total filesystem size in bytes */
                total_bytes = (unsigned long) g_rm_littlefs0_lfs_cfg.block_size *
                              (unsigned long) g_rm_littlefs0_lfs_cfg.block_count;

                /* Calculate minimum free space threshold (5% of total) */
                min_free_bytes = (total_bytes * 5) / 100;

                /* Acquire filesystem mutex with timeout for space check */
                if (!lfs_mutex_get())
                {
                    return ERR_MEM;
                }

                if (lfs_mutex_take(pdMS_TO_TICKS(500)) != pdTRUE)
                {
                    return ERR_TIMEOUT;
                }

                /* Get used blocks count */
                used_blocks = lfs_fs_size(&g_rm_littlefs0_lfs);
                if (used_blocks < 0)
                {
                    lfs_mutex_give();

                    return ERR_VAL;
                }

                /* Release mutex */
                lfs_mutex_give();

                /* Calculate free space */
                used_bytes = (unsigned long) used_blocks * (unsigned long) g_rm_littlefs0_lfs_cfg.block_size;
                if (used_bytes > total_bytes)
                {
                    free_bytes = 0;
                }
                else
                {
                    free_bytes = total_bytes - used_bytes;
                }

                /* Check if free space is below threshold */
                if (free_bytes < min_free_bytes)
                {
                    (void) ((free_bytes * 100) / total_bytes);

                    /* Set filesystem full status */
                    last_upload_status = UPLOAD_STATUS_ERROR_FS_FULL;

                    /* Return error to trigger httpd_post_finished with error page */
                    return ERR_VAL;
                }

                /* Check if uploading this file would violate the 5% threshold */
                if (expected_file_size > 0)
                {
                    unsigned long free_after_upload;

                    if (free_bytes > expected_file_size)
                    {
                        free_after_upload = free_bytes - expected_file_size;
                    }
                    else
                    {
                        free_after_upload = 0;
                    }

                    if (free_after_upload < min_free_bytes)
                    {
                        /* Set filesystem full status */
                        last_upload_status = UPLOAD_STATUS_ERROR_FS_FULL;

                        /* Return error to trigger httpd_post_finished with error page */
                        return ERR_VAL;
                    }
                }
  #endif                               /* SUPPORT_FSP_RM_FS_W */

                if (content_len >= 0)
                {
                    /* adjust length of HTTP header passed to application */
                    const char * hdr_start_after_uri = uri_end + 1;
                    u16_t        hdr_len             = (u16_t) LWIP_MIN(data_len, crlfcrlf + 4 - data);
                    u16_t        hdr_data_len        = (u16_t) LWIP_MIN(data_len, crlfcrlf + 4 - hdr_start_after_uri);
                    u8_t         post_auto_wnd       = 1;
                    http_uri_buf[0] = 0;

                    /* trim http header */
                    *crlfcrlf = 0;
                    err       = httpd_post_begin(hs,
                                                 uri,
                                                 hdr_start_after_uri,
                                                 hdr_data_len,
                                                 content_len,
                                                 http_uri_buf,
                                                 LWIP_HTTPD_URI_BUF_LEN,
                                                 &post_auto_wnd);
                    if (err == ERR_OK)
                    {
                        /* try to pass in data of the first pbuf(s) */
                        struct pbuf * q            = inp;
                        u16_t         start_offset = hdr_len;
  #if LWIP_HTTPD_POST_MANUAL_WND
                        hs->no_auto_wnd = !post_auto_wnd;
  #endif                               /* LWIP_HTTPD_POST_MANUAL_WND */
                        /* set the Content-Length to be received for this POST */
                        hs->post_content_len_left = (u32_t) content_len;

                        /* get to the pbuf where the body starts */
                        while ((q != NULL) && (q->len <= start_offset))
                        {
                            start_offset -= q->len;
                            q             = q->next;
                        }

                        if (q != NULL)
                        {
                            /* hide the remaining HTTP header */
                            pbuf_remove_header(q, start_offset);
  #if LWIP_HTTPD_POST_MANUAL_WND
                            if (!post_auto_wnd)
                            {
                                /* already tcp_recved() this data... */
                                hs->unrecved_bytes = q->tot_len;
                            }
  #endif                               /* LWIP_HTTPD_POST_MANUAL_WND */
                            pbuf_ref(q);

                            return http_post_rxpbuf(hs, q);
                        }
                        else if (hs->post_content_len_left == 0)
                        {
                            q = pbuf_alloc(PBUF_RAW, 0, PBUF_REF);

                            return http_post_rxpbuf(hs, q);
                        }
                        else
                        {
                            return ERR_OK;
                        }
                    }
                    else
                    {

                        /* return file passed from application */
                        return http_find_file(hs, uri, 0);
                    }
                }
                else
                {
                    LWIP_DEBUGF(HTTPD_DEBUG,
                                ("POST received invalid Content-Length: %s\n",
                                 content_len_num));

                    return ERR_ARG;
                }
            }
        }

        /* If we come here, headers are fully received (double-crlf), but Content-Length
         * was not included. Since this is currently the only supported method, we have
         * to fail in this case! */
        LWIP_DEBUGF(HTTPD_DEBUG, ("Error when parsing Content-Length\n"));

        return ERR_ARG;
    }

    /* if we come here, the POST is incomplete */
  #if LWIP_HTTPD_SUPPORT_REQUESTLIST

    return ERR_INPROGRESS;
  #else                                /* LWIP_HTTPD_SUPPORT_REQUESTLIST */
    return ERR_ARG;
  #endif /* LWIP_HTTPD_SUPPORT_REQUESTLIST */
}

  #if LWIP_HTTPD_POST_MANUAL_WND

/**
 * @ingroup httpd
 * A POST implementation can call this function to update the TCP window.
 * This can be used to throttle data reception (e.g. when received data is
 * programmed to flash and data is received faster than programmed).
 *
 * @param connection A connection handle passed to httpd_post_begin for which
 *        httpd_post_finished has *NOT* been called yet!
 * @param recved_len Length of data received (for window update)
 */
void httpd_post_data_recved (void * connection, u16_t recved_len)
{
    struct http_state * hs = (struct http_state *) connection;
    if (hs != NULL)
    {
        if (hs->no_auto_wnd)
        {
            u16_t len = recved_len;
            if (hs->unrecved_bytes >= recved_len)
            {
                hs->unrecved_bytes -= recved_len;
            }
            else
            {
                LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_LEVEL_WARNING, ("httpd_post_data_recved: recved_len too big\n"));
                len                = (u16_t) hs->unrecved_bytes;
                hs->unrecved_bytes = 0;
            }

            if (hs->pcb != NULL)
            {
                if (len != 0)
                {
                    altcp_recved(hs->pcb, len);
                }

                if ((hs->post_content_len_left == 0) && (hs->unrecved_bytes == 0))
                {
                    /* finished handling POST */
                    http_handle_post_finished(hs);
                    http_send(hs->pcb, hs);
                }
            }
        }
    }
}

  #endif                               /* LWIP_HTTPD_POST_MANUAL_WND */

 #endif                                /* LWIP_HTTPD_SUPPORT_POST */

 #if LWIP_HTTPD_FS_ASYNC_READ

/** Try to send more data if file has been blocked before
 * This is a callback function passed to fs_read_async().
 */
static void http_continue (void * connection)
{
    struct http_state * hs = (struct http_state *) connection;
    LWIP_ASSERT_CORE_LOCKED();
    if (hs && (hs->pcb) && (hs->handle))
    {
        LWIP_ASSERT("hs->pcb != NULL", hs->pcb != NULL);
        LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("httpd_continue: try to send more data\n"));
        if (http_send(hs->pcb, hs))
        {
            /* If we wrote anything to be sent, go ahead and send it now. */
            LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("tcp_output\n"));
            altcp_output(hs->pcb);
        }
    }
}

 #endif                                /* LWIP_HTTPD_FS_ASYNC_READ */

/**
 * When data has been received in the correct state, try to parse it
 * as a HTTP request.
 *
 * @param inp the received pbuf
 * @param hs the connection state
 * @param pcb the altcp_pcb which received this packet
 * @return ERR_OK if request was OK and hs has been initialized correctly
 *         ERR_INPROGRESS if request was OK so far but not fully received
 *         another err_t otherwise
 */
static err_t http_parse_request (struct pbuf * inp, struct http_state * hs, struct altcp_pcb * pcb)
{
    char        * data = NULL;
    char        * crlf;
    u16_t         data_len;
    struct pbuf * p = inp;
 #if LWIP_HTTPD_SUPPORT_REQUESTLIST
    u16_t clen;
 #endif                                /* LWIP_HTTPD_SUPPORT_REQUESTLIST */
 #if LWIP_HTTPD_SUPPORT_POST
    err_t err;
 #endif                                /* LWIP_HTTPD_SUPPORT_POST */

    LWIP_UNUSED_ARG(pcb);              /* only used for post */
    LWIP_ASSERT("p != NULL", p != NULL);
    LWIP_ASSERT("hs != NULL", hs != NULL);

    if ((hs->handle != NULL) || (hs->file != NULL))
    {
        LWIP_DEBUGF(HTTPD_DEBUG, ("Received data while sending a file\n"));

        /* already sending a file */

        /* @todo: abort? */
        return ERR_USE;
    }

 #if LWIP_HTTPD_SUPPORT_REQUESTLIST
    LWIP_DEBUGF(HTTPD_DEBUG, ("Received %"U16_F " bytes\n", p->tot_len));

    /* first check allowed characters in this pbuf? */

    /* enqueue the pbuf */
    if (hs->req == NULL)
    {
        LWIP_DEBUGF(HTTPD_DEBUG, ("First pbuf\n"));
        hs->req = p;
    }
    else
    {
        LWIP_DEBUGF(HTTPD_DEBUG, ("pbuf enqueued\n"));
        pbuf_cat(hs->req, p);
    }

    /* increase pbuf ref counter as it is freed when we return but we want to
     * keep it on the req list */
    pbuf_ref(p);

    if (hs->req->next != NULL)
    {
        data_len = LWIP_MIN(hs->req->tot_len, LWIP_HTTPD_MAX_REQ_LENGTH);
        if (data_len >= MIN_REQ_LEN)
        {
            pbuf_copy_partial(hs->req, httpd_req_buf, data_len, 0);
            data = httpd_req_buf;
        }
    }
    else
 #endif                                /* LWIP_HTTPD_SUPPORT_REQUESTLIST */
    {
        data     = (char *) p->payload;
        data_len = p->len;
        if (p->len != p->tot_len)
        {
            LWIP_DEBUGF(HTTPD_DEBUG, ("Warning: incomplete header due to chained pbufs\n"));
        }
    }

    /* received enough data for minimal request? */
    if (data_len >= MIN_REQ_LEN)
    {
        /* wait for CRLF before parsing anything */
        crlf = lwip_strnstr(data, CRLF, data_len);
        if (crlf != NULL)
        {
 #if LWIP_HTTPD_SUPPORT_POST
            int is_post = 0;
 #endif                                /* LWIP_HTTPD_SUPPORT_POST */
            int    is_09 = 0;
            char * sp1, * sp2;
            u16_t  left_len, uri_len;
            LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("CRLF received, parsing request\n"));

            /* parse method */
            if (!strncmp(data, "GET ", 4))
            {
                sp1 = data + 3;

                /* received GET request */
                LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("Received GET request\"\n"));
 #if LWIP_HTTPD_SUPPORT_POST
            }
            else if (!strncmp(data, "POST ", 5))
            {
                /* store request type */
                is_post = 1;
                sp1     = data + 4;

                /* received GET request */
                LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("Received POST request\n"));
 #endif                                /* LWIP_HTTPD_SUPPORT_POST */
            }
            else
            {
                /* null-terminate the METHOD (pbuf is freed anyway wen returning) */
                data[4] = 0;

                /* unsupported method! */
                LWIP_DEBUGF(HTTPD_DEBUG,
                            ("Unsupported request method (not implemented): \"%s\"\n",
                             data));

                return http_find_error_file(hs, 501);
            }

            /* if we come here, method is OK, parse URI */
            left_len = (u16_t) (data_len - ((sp1 + 1) - data));
            sp2      = lwip_strnstr(sp1 + 1, " ", left_len);
 #if LWIP_HTTPD_SUPPORT_V09
            if (sp2 == NULL)
            {
                /* HTTP 0.9: respond with correct protocol version */
                sp2   = lwip_strnstr(sp1 + 1, CRLF, left_len);
                is_09 = 1;
  #if LWIP_HTTPD_SUPPORT_POST
                if (is_post)
                {
                    /* HTTP/0.9 does not support POST */
                    goto badrequest;
                }
  #endif                               /* LWIP_HTTPD_SUPPORT_POST */
            }
        }
 #endif                                /* LWIP_HTTPD_SUPPORT_V09 */
            uri_len = (u16_t) (sp2 - (sp1 + 1));
            if ((sp2 != 0) && (sp2 > sp1))
            {
                /* wait for CRLFCRLF (indicating end of HTTP headers) before parsing anything */
                if (lwip_strnstr(data, CRLF CRLF, data_len) != NULL)
                {
                    char * uri = sp1 + 1;
 #if LWIP_HTTPD_SUPPORT_11_KEEPALIVE

                    /* This is HTTP/1.0 compatible: for strict 1.1, a connection
                     * would always be persistent unless "close" was specified. */
                    if (!is_09 && (lwip_strnstr(data, HTTP11_CONNECTIONKEEPALIVE, data_len) ||
                                   lwip_strnstr(data, HTTP11_CONNECTIONKEEPALIVE2, data_len)))
                    {
                        hs->keepalive = 1;
                    }
                    else
                    {
                        hs->keepalive = 0;
                    }
 #endif                                /* LWIP_HTTPD_SUPPORT_11_KEEPALIVE */
                    /* null-terminate the METHOD (pbuf is freed anyway wen returning) */
                    *sp1         = 0;
                    uri[uri_len] = 0;
                    LWIP_DEBUGF(HTTPD_DEBUG,
                                ("Received \"%s\" request for URI: \"%s\"\n",
                                 data, uri));
 #if LWIP_HTTPD_SUPPORT_POST
                    if (is_post)
                    {
  #if LWIP_HTTPD_SUPPORT_REQUESTLIST
                        struct pbuf * q = hs->req;
  #else                                /* LWIP_HTTPD_SUPPORT_REQUESTLIST */
                        struct pbuf * q = inp;
  #endif /* LWIP_HTTPD_SUPPORT_REQUESTLIST */
                        err = http_post_request(q, hs, data, data_len, uri, sp2);
                        if (err != ERR_OK)
                        {
                            /* restore header for next try */
                            *sp1         = ' ';
                            *sp2         = ' ';
                            uri[uri_len] = ' ';
                        }

  #if SUPPORT_FSP_RM_FS_W
                        if (err == ERR_VAL)
                        {
                            /* Filesystem full - trigger POST finished to show error page */
                            LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_LEVEL_WARNING,
                                        ("Filesystem full - redirecting to error page"));

                            /* Set POST as complete to trigger httpd_post_finished */
                            hs->post_content_len_left = 0;

                            /* Call httpd_post_finished to show error page */
                            http_uri_buf[0] = 0;
                            httpd_post_finished(hs, http_uri_buf, LWIP_HTTPD_URI_BUF_LEN);

                            /* Find and return the error page */
                            return http_find_file(hs, http_uri_buf, 0);
                        }
  #endif
                        if (err == ERR_ARG)
                        {
                            goto badrequest;
                        }

                        return err;
                    }
                    else
 #endif                                /* LWIP_HTTPD_SUPPORT_POST */
                    {
                        return http_find_file(hs, uri, is_09);
                    }
                }
            }
            else
            {
                LWIP_DEBUGF(HTTPD_DEBUG, ("invalid URI\n"));
            }
        }
    }

 #if LWIP_HTTPD_SUPPORT_REQUESTLIST
    clen = pbuf_clen(hs->req);
    if ((hs->req->tot_len <= LWIP_HTTPD_REQ_BUFSIZE) &&
        (clen <= LWIP_HTTPD_REQ_QUEUELEN))
    {

        /* request not fully received (too short or CRLF is missing) */
        return ERR_INPROGRESS;
    }
    else
 #endif                                /* LWIP_HTTPD_SUPPORT_REQUESTLIST */
    {
 #if LWIP_HTTPD_SUPPORT_POST
badrequest:
 #endif                                /* LWIP_HTTPD_SUPPORT_POST */
        LWIP_DEBUGF(HTTPD_DEBUG, ("bad request\n"));

        /* could not parse request */
        return http_find_error_file(hs, 400);
    }
}

 #if LWIP_HTTPD_SSI && (LWIP_HTTPD_SSI_BY_FILE_EXTENSION == 1)

/* Check if SSI should be parsed for this file/URL
 * (With LWIP_HTTPD_SSI_BY_FILE_EXTENSION == 2, this function can be
 * overridden by an external implementation.)
 *
 * @return 1 for SSI, 0 for standard files
 */
static u8_t http_uri_is_ssi (struct fs_file * file, const char * uri)
{
    size_t loop;
    u8_t tag_check = 0;
    if (file != NULL)
    {
        /* See if we have been asked for an shtml file and, if so,
         *  enable tag checking. */
        const char * ext = NULL, * sub;
        char * param = (char *) strstr(uri, "?");
        if (param != NULL)
        {
            /* separate uri from parameters for now, set back later */
            *param = 0;
        }

        sub = uri;
        ext = uri;
        for (sub = strstr(sub, "."); sub != NULL; sub = strstr(sub, "."))
        {
            ext = sub;
            sub++;
        }

        for (loop = 0; loop < NUM_SHTML_EXTENSIONS; loop++)
        {
            if (!lwip_stricmp(ext, g_pcSSIExtensions[loop]))
            {
                tag_check = 1;
                break;
            }
        }

        if (param != NULL)
        {
            *param = '?';
        }
    }

    return tag_check;
}

 #endif                                /* LWIP_HTTPD_SSI */

/** Try to find the file specified by uri and, if found, initialize hs
 * accordingly.
 *
 * @param hs the connection state
 * @param uri the HTTP header URI
 * @param is_09 1 if the request is HTTP/0.9 (no HTTP headers in response)
 * @return ERR_OK if file was found and hs has been initialized correctly
 *         another err_t otherwise
 */
static err_t http_find_file (struct http_state * hs, const char * uri, int is_09)
{
    size_t loop;
    struct fs_file * file = NULL;
    char * params         = NULL;
    err_t err;
 #if LWIP_HTTPD_CGI
    int i;
 #endif                                /* LWIP_HTTPD_CGI */
 #if !LWIP_HTTPD_SSI
    const
 #endif                                /* !LWIP_HTTPD_SSI */
    /* By default, assume we will not be processing server-side-includes tags */
    u8_t tag_check = 0;

    /* Have we been asked for the default file (in root or a directory) ? */
 #if LWIP_HTTPD_MAX_REQUEST_URI_LEN
    size_t uri_len = strlen(uri);
    if ((uri_len > 0) && (uri[uri_len - 1] == '/') &&
        ((uri != http_uri_buf) || (uri_len == 1)))
    {
        size_t copy_len = LWIP_MIN(sizeof(http_uri_buf) - 1, uri_len - 1);
        if (copy_len > 0)
        {
            MEMCPY(http_uri_buf, uri, copy_len);
            http_uri_buf[copy_len] = 0;
        }

 #else                                 /* LWIP_HTTPD_MAX_REQUEST_URI_LEN */
    if ((uri[0] == '/') && (uri[1] == 0))
    {
 #endif /* LWIP_HTTPD_MAX_REQUEST_URI_LEN */

        /* Try each of the configured default filenames until we find one
         * that exists. */
        for (loop = 0; loop < NUM_DEFAULT_FILENAMES; loop++)
        {
            const char * file_name;
 #if LWIP_HTTPD_MAX_REQUEST_URI_LEN
            if (copy_len > 0)
            {
                size_t len_left = sizeof(http_uri_buf) - copy_len - 1;
                if (len_left > 0)
                {
                    size_t name_len      = strlen(httpd_default_filenames[loop].name);
                    size_t name_copy_len = LWIP_MIN(len_left, name_len);
                    MEMCPY(&http_uri_buf[copy_len], httpd_default_filenames[loop].name, name_copy_len);
                    http_uri_buf[copy_len + name_copy_len] = 0;
                }

                file_name = http_uri_buf;
            }
            else
 #endif                                /* LWIP_HTTPD_MAX_REQUEST_URI_LEN */
            {
                file_name = httpd_default_filenames[loop].name;
            }

            LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("Looking for %s...\n", file_name));
 #if SUPPORT_FSP_RM_FS_W
            err                   = user_fs_open(&hs->file_handle, file_name);
            hs->file              = hs->file_handle.data;
            hs->left              = hs->file_handle.len;
            hs->file_handle.index = hs->file_handle.len;
            hs->file_handle.flags = FS_FILE_FLAGS_HEADER_INCLUDED;

            /* Ensure the http_state knows about the dynamic handle so it will be
             * closed/freed later via http_state_eof -> fs_close. */
            hs->handle = &hs->file_handle;

            return err;
 #else
            err = fs_open((struct fs_file *) &hs->file_handle, file_name);
 #endif
            if (err == ERR_OK)
            {
                uri  = file_name;
                file = (struct fs_file *) &hs->file_handle;
                LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("Opened.\n"));
 #if LWIP_HTTPD_SSI
                tag_check = httpd_default_filenames[loop].shtml;
 #endif                                /* LWIP_HTTPD_SSI */
                break;
            }
        }
    }

    if (file == NULL)
    {
        /* No - we've been asked for a specific file. */
        /* First, isolate the base URI (without any parameters) */
        params = (char *) strchr(uri, '?');
        if (params != NULL)
        {
            /* URI contains parameters. NULL-terminate the base URI */
            *params = '\0';
            params++;
        }

 #if LWIP_HTTPD_CGI
        http_cgi_paramcount = -1;

        /* Does the base URI we have isolated correspond to a CGI handler? */
        if (httpd_num_cgis && httpd_cgis)
        {
            for (i = 0; i < httpd_num_cgis; i++)
            {
                if (strcmp(uri, httpd_cgis[i].pcCGIName) == 0)
                {
                    /*
                     * We found a CGI that handles this URI so extract the
                     * parameters and call the handler.
                     */
                    http_cgi_paramcount = extract_uri_parameters(hs, params);
                    uri                 =
                        httpd_cgis[i].pfnCGIHandler(i, http_cgi_paramcount, hs->params, hs->param_vals);
                    break;
                }
            }
        }
 #endif                                /* LWIP_HTTPD_CGI */

        LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("Opening %s\n", uri));
 #if SUPPORT_FSP_RM_FS_W
        err                   = user_fs_open(&hs->file_handle, uri);
        hs->file              = hs->file_handle.data;
        hs->left              = hs->file_handle.len;
        hs->file_handle.index = hs->file_handle.len;
        hs->file_handle.flags = FS_FILE_FLAGS_HEADER_INCLUDED;

        /* Ensure the http_state knows about the dynamic handle so it will be
         * closed/freed later via http_state_eof -> fs_close. */
        hs->handle = &hs->file_handle;

        return err;
 #else                                 /* SUPPORT_FSP_RM_FS_W */
        err = fs_open((struct fs_file *) &hs->file_handle, uri);
        if (err == ERR_OK)
        {
            file = &hs->file_handle;
        }
        else
        {
            file = http_get_404_file(hs, &uri);
        }
 #endif

 #if LWIP_HTTPD_SSI
        if (file != NULL)
        {
            if (file->flags & FS_FILE_FLAGS_SSI)
            {
                tag_check = 1;
            }
            else
            {
  #if LWIP_HTTPD_SSI_BY_FILE_EXTENSION
                tag_check = http_uri_is_ssi(file, uri);
  #endif                               /* LWIP_HTTPD_SSI_BY_FILE_EXTENSION */
            }
        }
 #endif                                /* LWIP_HTTPD_SSI */
    }

    if (file == NULL)
    {
        /* None of the default filenames exist so send back a 404 page */
        file = http_get_404_file(hs, &uri);
    }

    return http_init_file(hs, file, is_09, uri, tag_check, params);
}

/** Initialize a http connection with a file to send (if found).
 * Called by http_find_file and http_find_error_file.
 *
 * @param hs http connection state
 * @param file file structure to send (or NULL if not found)
 * @param is_09 1 if the request is HTTP/0.9 (no HTTP headers in response)
 * @param uri the HTTP header URI
 * @param tag_check enable SSI tag checking
 * @param params != NULL if URI has parameters (separated by '?')
 * @return ERR_OK if file was found and hs has been initialized correctly
 *         another err_t otherwise
 */
static err_t http_init_file (struct http_state * hs,
                             struct fs_file    * file,
                             int                 is_09,
                             const char        * uri,
                             u8_t                tag_check,
                             char              * params)
{
 #if !LWIP_HTTPD_SUPPORT_V09
    LWIP_UNUSED_ARG(is_09);
 #endif
    if (file != NULL)
    {
        /* file opened, initialise struct http_state */
 #if !LWIP_HTTPD_DYNAMIC_FILE_READ

        /* If dynamic read is disabled, file data must be in one piece and available now */
        LWIP_ASSERT("file->data != NULL", file->data != NULL);
 #endif

 #if LWIP_HTTPD_SSI
        if (tag_check)
        {
            struct http_ssi_state * ssi = http_ssi_state_alloc();
            if (ssi != NULL)
            {
                ssi->tag_index  = 0;
                ssi->tag_state  = TAG_NONE;
                ssi->parsed     = file->data;
                ssi->parse_left = file->len;
                ssi->tag_end    = file->data;
                hs->ssi         = ssi;
            }
        }

 #else                                 /* LWIP_HTTPD_SSI */
        LWIP_UNUSED_ARG(tag_check);
 #endif /* LWIP_HTTPD_SSI */
 #if SUPPORT_FSP_RM_FS_W
        hs->handle = (struct lfs_file_data *) file;
 #else
        hs->handle = file;
 #endif
 #if LWIP_HTTPD_CGI_SSI
        if (params != NULL)
        {
            /* URI contains parameters, call generic CGI handler */
            int count;
  #if LWIP_HTTPD_CGI
            if (http_cgi_paramcount >= 0)
            {
                count = http_cgi_paramcount;
            }
            else
  #endif
            {
                count = extract_uri_parameters(hs, params);
            }

            httpd_cgi_handler(file,
                              uri,
                              count,
                              http_cgi_params,
                              http_cgi_param_vals
  #if defined(LWIP_HTTPD_FILE_STATE) && LWIP_HTTPD_FILE_STATE
                              ,
                              file->state
  #endif                               /* LWIP_HTTPD_FILE_STATE */
                              );
        }

 #else /* LWIP_HTTPD_CGI_SSI */
        LWIP_UNUSED_ARG(params);
 #endif                                /* LWIP_HTTPD_CGI_SSI */
        hs->file = file->data;
        LWIP_ASSERT("File length must be positive!", (file->len >= 0));
 #if LWIP_HTTPD_CUSTOM_FILES
        if (file->is_custom_file && (file->data == NULL))
        {
            /* custom file, need to read data first (via fs_read_custom) */
            hs->left = 0;
        }
        else
 #endif                                /* LWIP_HTTPD_CUSTOM_FILES */
        {
            hs->left = (u32_t) file->len;
        }

        hs->retries = 0;
 #if LWIP_HTTPD_TIMING
        hs->time_started = sys_now();
 #endif                                /* LWIP_HTTPD_TIMING */
 #if !LWIP_HTTPD_DYNAMIC_HEADERS
        LWIP_ASSERT("HTTP headers not included in file system",
                    (hs->handle->flags & FS_FILE_FLAGS_HEADER_INCLUDED) != 0);
 #endif                                /* !LWIP_HTTPD_DYNAMIC_HEADERS */
 #if LWIP_HTTPD_SUPPORT_V09
        if (is_09 && ((hs->handle->flags & FS_FILE_FLAGS_HEADER_INCLUDED) != 0))
        {
            /* HTTP/0.9 responses are sent without HTTP header,
             * search for the end of the header. */
            char * file_start = lwip_strnstr(hs->file, CRLF CRLF, hs->left);
            if (file_start != NULL)
            {
                int diff = file_start + 4 - hs->file;
                hs->file += diff;
                hs->left -= (u32_t) diff;
            }
        }
 #endif                                /* LWIP_HTTPD_SUPPORT_V09*/
    }
    else
    {
        hs->handle  = NULL;
        hs->file    = NULL;
        hs->left    = 0;
        hs->retries = 0;
    }

 #if LWIP_HTTPD_DYNAMIC_HEADERS

    /* Determine the HTTP headers to send based on the file extension of
     * the requested URI. */
    if ((hs->handle == NULL) || ((hs->handle->flags & FS_FILE_FLAGS_HEADER_INCLUDED) == 0))
    {
        get_http_headers(hs, uri);
    }

 #else                                 /* LWIP_HTTPD_DYNAMIC_HEADERS */
    LWIP_UNUSED_ARG(uri);
 #endif /* LWIP_HTTPD_DYNAMIC_HEADERS */
 #if LWIP_HTTPD_SUPPORT_11_KEEPALIVE
    if (hs->keepalive)
    {
  #if LWIP_HTTPD_SSI
        if (hs->ssi != NULL)
        {
            hs->keepalive = 0;
        }
        else
  #endif                               /* LWIP_HTTPD_SSI */
        {
            if ((hs->handle != NULL) &&
                ((hs->handle->flags & (FS_FILE_FLAGS_HEADER_INCLUDED | FS_FILE_FLAGS_HEADER_PERSISTENT)) ==
                 FS_FILE_FLAGS_HEADER_INCLUDED))
            {
                hs->keepalive = 0;
            }
        }
    }
 #endif                                /* LWIP_HTTPD_SUPPORT_11_KEEPALIVE */
    return ERR_OK;
}

/**
 * The pcb had an error and is already deallocated.
 * The argument might still be valid (if != NULL).
 */
static void http_err (void * arg, err_t err)
{
    struct http_state * hs = (struct http_state *) arg;
    LWIP_UNUSED_ARG(err);

    LWIP_DEBUGF(HTTPD_DEBUG, ("http_err: %s", lwip_strerr(err)));

    if (hs != NULL)
    {
        http_state_free(hs);
    }
}

/**
 * Data has been sent and acknowledged by the remote host.
 * This means that more data can be sent.
 */
static err_t http_sent (void * arg, struct altcp_pcb * pcb, u16_t len)
{
    struct http_state * hs = (struct http_state *) arg;

    LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("http_sent %p\n", (void *) pcb));

    LWIP_UNUSED_ARG(len);

    if (hs == NULL)
    {
        return ERR_OK;
    }

    hs->retries = 0;

    http_send(pcb, hs);

    return ERR_OK;
}

/**
 * The poll function is called every 2nd second.
 * If there has been no data sent (which resets the retries) in 8 seconds, close.
 * If the last portion of a file has not been sent in 2 seconds, close.
 *
 * This could be increased, but we don't want to waste resources for bad connections.
 */
static err_t http_poll (void * arg, struct altcp_pcb * pcb)
{
    struct http_state * hs = (struct http_state *) arg;
    LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE,
                ("http_poll: pcb=%p hs=%p pcb_state=%s\n",
                 (void *) pcb, (void *) hs, tcp_debug_state_str(altcp_dbg_get_tcp_state(pcb))));

    if (hs == NULL)
    {
        err_t closed;

        /* arg is null, close. */
        LWIP_DEBUGF(HTTPD_DEBUG, ("http_poll: arg is NULL, close\n"));
        closed = http_close_conn(pcb, NULL);
        LWIP_UNUSED_ARG(closed);
 #if LWIP_HTTPD_ABORT_ON_CLOSE_MEM_ERROR
        if (closed == ERR_MEM)
        {
            altcp_abort(pcb);

            return ERR_ABRT;
        }
 #endif                                /* LWIP_HTTPD_ABORT_ON_CLOSE_MEM_ERROR */
        return ERR_OK;
    }
    else
    {
        /* Check for stalled upload activity: if an upload is in progress but
         * no activity observed for >5s, abort and cleanup. This check runs
         * periodically via the http_poll callback. */
 #if SUPPORT_FSP_RM_FS_W
        if (in_file)
        {
            u32_t now_ms = sys_now();
            if ((upload_last_activity_ms != 0) && ((now_ms - upload_last_activity_ms) > 5000u))
            {
                LWIP_DEBUGF(HTTPD_DEBUG,
                            ("[WARN] [UPLOAD] Stall timeout in poll: %u ms, aborting upload %s",
                             (unsigned int) (now_ms - upload_last_activity_ms),
                             current_upload_path[0] ? current_upload_path : "(none)"));
                cleanup_upload_and_remove(&upload_file, current_upload_path);
                in_file                = 0;
                current_upload_path[0] = '\0';
                boundary_tail_len      = 0;

                /* Close this HTTP connection since upload failed */
                http_close_conn(pcb, hs);

                return ERR_OK;
            }
        }
 #endif                                /* SUPPORT_FSP_RM_FS_W */

        hs->retries++;
        if (hs->retries == HTTPD_MAX_RETRIES)
        {
            LWIP_DEBUGF(HTTPD_DEBUG, ("http_poll: too many retries, close\n"));
            http_close_conn(pcb, hs);

            return ERR_OK;
        }

        /* If this connection has a file open, try to send some more data. If
         * it has not yet received a GET request, don't do this since it will
         * cause the connection to close immediately. */
        if (hs->handle)
        {
            LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("http_poll: try to send more data\n"));
            if (http_send(pcb, hs))
            {
                /* If we wrote anything to be sent, go ahead and send it now. */
                LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("tcp_output\n"));
                altcp_output(pcb);
            }
        }
    }

    return ERR_OK;
}

 #if SUPPORT_FSP_RM_FS_W
static err_t http_sent_close (void * p_arg, struct altcp_pcb * p_pcb, u16_t len)
{
    struct http_state * p_hs = (struct http_state *) p_arg;

    LWIP_UNUSED_ARG(len);
    LWIP_UNUSED_ARG(p_arg);
    http_close_conn(p_pcb, p_hs);

    return ERR_OK;
}

static void upload_size_err (struct altcp_pcb * p_pcb, struct http_state * p_hs)
{
    LWIP_UNUSED_ARG(p_hs);
    char hdr[HEADER_LENGTH];
    int hdr_len;
    const char * body = "<html><head><title>Upload Error</title></head><body><h2>Upload Failed</h2>"
                        "<p>The file size is too large<p></body></html>";

    hdr_len = snprintf(hdr,
                       sizeof(hdr),
                       "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n"
                       "Content-Length: %u\r\n\r\n",
                       (unsigned int) strlen(body));

    altcp_write(p_pcb, hdr, hdr_len, TCP_WRITE_FLAG_COPY);
    altcp_write(p_pcb, body, strlen(body), TCP_WRITE_FLAG_COPY);
    altcp_output(p_pcb);

    /* Delay closing until browser ACKs last segment */
    altcp_poll(p_pcb, http_poll, 1);    /* ensure poll is armed */
    altcp_recv(p_pcb, NULL);            /* disable further recv */
    altcp_sent(p_pcb, http_sent_close); /* close after ACK received */
}

 #endif

/**
 * Data has been received on this pcb.
 * For HTTP 1.0, this should normally only happen once (if the request fits in one packet).
 */
static err_t http_recv (void * arg, struct altcp_pcb * pcb, struct pbuf * p, err_t err)
{
    struct http_state * hs = (struct http_state *) arg;
    LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE,
                ("http_recv: pcb=%p pbuf=%p err=%s\n", (void *) pcb,
                 (void *) p, lwip_strerr(err)));

    if (httpd_close_flag == 1)
    {
        http_close_conn(pcb, hs);

        return ERR_OK;
    }

    if (p->tot_len != 0)
    {
        LWIP_DEBUGF(HTTPD_DEBUG, (">>> HTTP Receive\n- (len=%u):\n%s\n", p->tot_len, (const char *) p->payload));

        if (user_recv_fn != NULL)
        {
            user_recv_fn(p, err);
        }
    }

    if ((err != ERR_OK) || (p == NULL) || (hs == NULL))
    {
        /* error or closed by other side? */
        if (p != NULL)
        {
            /* Inform TCP that we have taken the data. */
            altcp_recved(pcb, p->tot_len);
            pbuf_free(p);
        }

        if (hs == NULL)
        {
            /* this should not happen, only to be robust */
            LWIP_DEBUGF(HTTPD_DEBUG, ("Error, http_recv: hs is NULL, close\n"));
        }

        http_close_conn(pcb, hs);

        return ERR_OK;
    }

 #if LWIP_HTTPD_SUPPORT_POST && LWIP_HTTPD_POST_MANUAL_WND
    if (hs->no_auto_wnd)
    {
        hs->unrecved_bytes += p->tot_len;
    }
    else
 #endif                                /* LWIP_HTTPD_SUPPORT_POST && LWIP_HTTPD_POST_MANUAL_WND */
    {
        /* Inform TCP that we have taken the data. */
        altcp_recved(pcb, p->tot_len);
    }

 #if LWIP_HTTPD_SUPPORT_POST
    if (hs->post_content_len_left > 0)
    {
        /* reset idle counter when POST data is received */
        hs->retries = 0;

        /* this is data for a POST, pass the complete pbuf to the application */
        http_post_rxpbuf(hs, p);

        /* pbuf is passed to the application, don't free it! */
        if (hs->post_content_len_left == 0)
        {
            /* all data received, send response or close connection */
            http_send(pcb, hs);
        }

        /* [20230703] Fixed an issue where POST data of 1460 bytes (MSS) or more was not received */
        if (p->tot_len != 0)
        {
            pbuf_free(p);
        }

        return ERR_OK;
    }
    else
 #endif                                /* LWIP_HTTPD_SUPPORT_POST */
    {
        if (hs->handle == NULL)
        {
            err_t parsed = http_parse_request(p, hs, pcb);
            LWIP_ASSERT("http_parse_request: unexpected return value",
                        parsed == ERR_OK ||
                        parsed == ERR_INPROGRESS || parsed == ERR_ARG || parsed == ERR_USE);
 #if LWIP_HTTPD_SUPPORT_REQUESTLIST
            if (parsed != ERR_INPROGRESS)
            {
                /* request fully parsed or error */
                if (hs->req != NULL)
                {
                    pbuf_free(hs->req);
                    hs->req = NULL;
                }
            }
 #endif                                /* LWIP_HTTPD_SUPPORT_REQUESTLIST */
            if (p->tot_len != 0)
            {
                // if (p != NULL) {
                pbuf_free(p);
            }

            if (parsed == ERR_OK)
            {
 #if LWIP_HTTPD_SUPPORT_POST
                if (hs->post_content_len_left == 0)
 #endif                                /* LWIP_HTTPD_SUPPORT_POST */
                {
                    LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE,
                                ("http_recv: data %p len %"S32_F "\n", (const void *) hs->file, hs->left));
                    http_send(pcb, hs);
                }
            }
            else if (parsed == ERR_ARG)
            {
                /* @todo: close on ERR_USE? */
                http_close_conn(pcb, hs);
            }

 #if SUPPORT_FSP_RM_FS_W
            else if (parsed == ERR_VAL)
            {
                upload_size_err(pcb, hs);
            }
 #endif
        }
        else
        {
            LWIP_DEBUGF(HTTPD_DEBUG, ("http_recv: already sending data\n"));

            /* already sending but still receiving data, we might want to RST here? */
            pbuf_free(p);
        }
    }

    return ERR_OK;
}

/**
 * A new incoming connection has been accepted.
 */
static err_t http_accept (void * arg, struct altcp_pcb * pcb, err_t err)
{
    struct http_state * hs;
    LWIP_UNUSED_ARG(err);
    LWIP_UNUSED_ARG(arg);
    LWIP_DEBUGF(HTTPD_DEBUG, ("http_accept %p / %p\n", (void *) pcb, arg));

    if ((err != ERR_OK) || (pcb == NULL))
    {
        return ERR_VAL;
    }

    /* Set priority */
    altcp_setprio(pcb, HTTPD_TCP_PRIO);

    /* Allocate memory for the structure that holds the state of the
     * connection - initialized by that function. */
    hs = http_state_alloc();
    if (hs == NULL)
    {
        LWIP_DEBUGF(HTTPD_DEBUG, ("http_accept: Out of memory, RST\n"));

        return ERR_MEM;
    }

    hs->pcb = pcb;

    /* Tell TCP that this is the structure we wish to be passed for our
     * callbacks. */
    altcp_arg(pcb, hs);

    /* Set up the various callback functions */
    altcp_recv(pcb, http_recv);
    altcp_err(pcb, http_err);
    altcp_poll(pcb, http_poll, HTTPD_POLL_INTERVAL);
    altcp_sent(pcb, http_sent);

    return ERR_OK;
}

static void httpd_init_pcb (struct altcp_pcb * pcb, u16_t port)
{
    err_t err;

    if (pcb)
    {
        altcp_setprio(pcb, HTTPD_TCP_PRIO);

        /* set SOF_REUSEADDR here to explicitly bind httpd to multiple interfaces */
        err = altcp_bind(pcb, IP_ANY_TYPE, port);
        LWIP_UNUSED_ARG(err);          /* in case of LWIP_NOASSERT */
        LWIP_ASSERT("httpd_init: tcp_bind failed", (err == ERR_OK) || (err == ERR_USE));
        pcb = altcp_listen(pcb);
        LWIP_ASSERT("httpd_init: tcp_listen failed", pcb != NULL);
        altcp_accept(pcb, http_accept);
    }
}

/**
 * @ingroup httpd
 * Initialize the httpd: set up a listening PCB and bind it to the defined port
 */
void httpd_init (altcp_user_recv_fn recv_fn)
{
    struct altcp_pcb * pcb;

 #if HTTPD_USE_MEM_POOL
    LWIP_MEMPOOL_INIT(HTTPD_STATE);
  #if LWIP_HTTPD_SSI
    LWIP_MEMPOOL_INIT(HTTPD_SSI_STATE);
  #endif
 #endif
    LWIP_DEBUGF(HTTPD_DEBUG, ("httpd_init\n"));

    /* LWIP_ASSERT_CORE_LOCKED(); is checked by tcp_new() */
    httpd_close_flag = 0;
    if (recv_fn != NULL)
    {
        user_recv_fn = recv_fn;
    }

    pcb = altcp_tcp_new_ip_type(IPADDR_TYPE_ANY);
    LWIP_ASSERT("httpd_init: tcp_new failed", pcb != NULL);
    httpd_init_pcb(pcb, HTTPD_SERVER_PORT);
}

 #if HTTPD_ENABLE_HTTPS

/**
 * @ingroup httpd
 * Initialize the httpd: set up a listening PCB and bind it to the defined port.
 * Also set up TLS connection handling (HTTPS).
 */
void httpd_inits (struct altcp_tls_config * conf, altcp_user_recv_fn recv_fn)
{
    httpd_close_flag = 0;
    if (recv_fn != NULL)
    {
        user_recv_fn = recv_fn;
    }

  #if LWIP_ALTCP_TLS
    struct altcp_pcb * pcb_tls = altcp_tls_new(conf, IPADDR_TYPE_ANY);
    LWIP_ASSERT("httpd_init: altcp_tls_new failed", pcb_tls != NULL);
    httpd_init_pcb(pcb_tls, HTTPD_SERVER_PORT_HTTPS);
  #else                                /* LWIP_ALTCP_TLS */
    LWIP_UNUSED_ARG(conf);
  #endif /* LWIP_ALTCP_TLS */
}

 #endif                                /* HTTPD_ENABLE_HTTPS */

 #if LWIP_HTTPD_SSI

/**
 * @ingroup httpd
 * Set the SSI handler function.
 *
 * @param ssi_handler the SSI handler function
 * @param tags an array of SSI tag strings to search for in SSI-enabled files
 * @param num_tags number of tags in the 'tags' array
 */
void http_set_ssi_handler (tSSIHandler ssi_handler, const char ** tags, int num_tags)
{
    LWIP_DEBUGF(HTTPD_DEBUG, ("http_set_ssi_handler\n"));

    LWIP_ASSERT("no ssi_handler given", ssi_handler != NULL);
    httpd_ssi_handler = ssi_handler;

  #if LWIP_HTTPD_SSI_RAW
    LWIP_UNUSED_ARG(tags);
    LWIP_UNUSED_ARG(num_tags);
  #else                                /* LWIP_HTTPD_SSI_RAW */
    LWIP_ASSERT("no tags given", tags != NULL);
    LWIP_ASSERT("invalid number of tags", num_tags > 0);

    httpd_tags     = tags;
    httpd_num_tags = num_tags;
  #endif                               /* !LWIP_HTTPD_SSI_RAW */
}

 #endif                                /* LWIP_HTTPD_SSI */

 #if LWIP_HTTPD_CGI

/**
 * @ingroup httpd
 * Set an array of CGI filenames/handler functions
 *
 * @param cgis an array of CGI filenames/handler functions
 * @param num_handlers number of elements in the 'cgis' array
 */
void http_set_cgi_handlers (const tCGI * cgis, int num_handlers)
{
    LWIP_ASSERT("no cgis given", cgis != NULL);
    LWIP_ASSERT("invalid number of handlers", num_handlers > 0);

    httpd_cgis     = cgis;
    httpd_num_cgis = num_handlers;
}

 #endif                                /* LWIP_HTTPD_CGI */

#endif                                 /* LWIP_TCP && LWIP_CALLBACK_API */
