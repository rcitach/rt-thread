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
 *
 */

#include "rm_httpd_opts.h"
#include "lwip/def.h"
#include "lwip/apps/fs.h"
#include <string.h>
#include <stdio.h>

#if SUPPORT_FSP_RM_FS_W

 #include "lfs.h"
 #include "lwip/err.h"
 #include "rm_lwip_w_fs_parse.h"

#endif

#include HTTPD_FSDATA_FILE

/*-----------------------------------------------------------------------------------*/

#if LWIP_HTTPD_CUSTOM_FILES && SUPPORT_FSP_RM_FS_W
int  fs_open_custom(struct fs_file * file, const char * name);
void fs_close_custom(struct fs_file * file);

 #if LWIP_HTTPD_FS_ASYNC_READ
  #ifndef SUPPORT_FSP_RM_FS_W
u8_t fs_canread_custom(struct fs_file * file);
u8_t fs_wait_read_custom(struct fs_file * file, fs_wait_cb callback_fn, void * callback_arg);
int  fs_read_async_custom(struct fs_file * file, char * buffer, int count, fs_wait_cb callback_fn, void * callback_arg);

  #else                                /* LWIP_HTTPD_FS_ASYNC_READ */
int fs_read_custom(struct fs_file * file, char * buffer, int count);

  #endif
 #endif /* LWIP_HTTPD_FS_ASYNC_READ */
#endif                                 /* LWIP_HTTPD_CUSTOM_FILES && SUPPORT_FSP_RM_FS_W */

#if LWIP_HTTPD_CUSTOM_FILES && SUPPORT_FSP_RM_FS_W

/* Implement custom file handlers to provide dynamic endpoints such as
 * /get_values which returns JSON with current temp/humid values. */
int fs_open_custom (struct fs_file * file, const char * name)
{
    char    * p_buf;
    const int bufsize = 128;
    int       comp1;
    int       comp2;
    int       n;
    int       toread;

    if (!file || !name)
    {
        return 0;
    }

    comp1 = strcmp(name, "/get_values");
    comp2 = strcmp(name, "/get_values.json");

    /* Support both URIs */
    if ((comp1 == 0) || (comp2 == 0))
    {
        /* Allocate a small buffer and fill with JSON */
        p_buf = (char *) pvPortMalloc((size_t) bufsize);
        if (!p_buf)
        {
            return 0;
        }

        n = http_get_values_json(p_buf, bufsize);
        if (n <= 0)
        {
            vPortFree(p_buf);

            return 0;
        }

        file->data       = (const char *) p_buf;
        file->len        = n;
        file->index      = 0;
        file->pextension = NULL;
        file->flags      = 0;          /* dynamic headers are enabled in this build */
 #if LWIP_HTTPD_FILE_STATE
        file->state = fs_state_init(file, name);
 #endif

        return 1;
    }

    return 0;
}

void fs_close_custom (struct fs_file * file)
{
    if (!file)
    {
        return;
    }

    if (file->data)
    {
        vPortFree((void *) file->data);
        file->data = NULL;
    }

 #if LWIP_HTTPD_FILE_STATE
    fs_state_free(file, file->state);
 #endif
}

int fs_read_custom (struct fs_file * file, char * buffer, int count)
{
    int toread;

    if (!file || !buffer)
    {
        return FS_READ_EOF;
    }

    if (file->index >= file->len)
    {
        return FS_READ_EOF;
    }

    toread = file->len - file->index;
    if (toread > count)
    {
        toread = count;
    }

    MEMCPY(buffer, file->data + file->index, toread);
    file->index += toread;

    return toread;
}

#endif                                 /* LWIP_HTTPD_CUSTOM_FILES && SUPPORT_FSP_RM_FS_W */

#if SUPPORT_FSP_RM_FS_W

/* Extern: your mounted LittleFS context */
lfs_t * p_lfs = &g_rm_littlefs0_lfs;

/* Remove zero-length regular files in a directory. Uses thread-safe
 * LittleFS wrappers so no external mutex is required. Collects names
 * first to avoid modifying the directory while iterating. */
static void fs_remove_zero_files_in_dir (const char * dir)
{
    lfs_dir_t       d;
    struct lfs_info info;
    const int       MAX_DEL  = 16;
    const int       MAX_NAME = 256;
    char            delnames[MAX_DEL][MAX_NAME];
    int             delcount = 0;
    int             r;
    size_t          dirlen;

    if (!dir)
    {
        return;
    }

    /* Basic sanity: avoid very long or non-null-terminated dir strings */
    dirlen = strnlen(dir, 512);
    if ((dirlen == 0) || (dirlen >= 512))
    {
        return;
    }

    if (lfs_dir_open(&g_rm_littlefs0_lfs, &d, dir) < 0)
    {
        return;
    }

    while (true)
    {
        r = lfs_dir_read(&g_rm_littlefs0_lfs, &d, &info);
        if ((r < 0) || (r == 0))
        {
            break;
        }

        if ((strcmp(info.name, ".") == 0) || (strcmp(info.name, "..") == 0))
        {
            continue;
        }

        if ((info.type == LFS_TYPE_REG) && (info.size == 0))
        {
            if (delcount < MAX_DEL)
            {
                /* Defensive: ensure info.name is sane and copy safely */
                size_t namelen = strnlen(info.name, LFS_NAME_MAX);
                if ((namelen == 0) || (namelen >= (size_t) MAX_NAME))
                {
                    /* Skip suspiciously long or empty names */
                }
                else
                {
                    memcpy(delnames[delcount], info.name, namelen);
                    delnames[delcount][namelen] = '\0';
                    delcount++;
                }
            }
        }
    }

    lfs_dir_close(&g_rm_littlefs0_lfs, &d);

    for (int i = 0; i < delcount; i++)
    {
        char fullpath[512];

        if ((dir[0] == '/') && (dir[1] == '\0'))
        {
            snprintf(fullpath, sizeof(fullpath), "/%s", delnames[i]);
        }
        else
        {
            snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, delnames[i]);
        }

        (void) lfs_remove(&g_rm_littlefs0_lfs, fullpath);
    }
}

static err_t fs_validate_path (const char * p_path)
{
    if ((!p_path) || (p_path[0] != '/') || (strlen(p_path) >= 255))
    {
        return LFS_ERR_INVAL;
    }

    return LFS_ERR_OK;
}

err_t user_fs_open (struct lfs_file_data * p_file, const char * p_path)
{
    int          json_len;
    const char * p_hdr_fmt =
        "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\n\r\n";
    int             hdr_len;
    int             total;
    char          * p_buf;
    int             offset = 0;
    int             length = -1;
    err_t           err;
    int             to_read;
    int             rlen;
    int             rc;
    struct lfs_info stinfo;
    int             sret;
    const char    * last;
    lfs_soff_t      fsz;
    size_t          len;
    char            parent[256];
    uint8_t         first_byte;
    const int       json_bufsize = 4096;
    char          * json_buf;

    /* Special-case: provide dynamic JSON at /get_values (served directly from RAM)
     * Return an HTTP/1.1 response including headers to avoid HTTP/0.9 behavior. */
    if (p_path && (strncmp(p_path, "/get_values", 11) == 0))
    {
        /* Build JSON for the requested URI by delegating parsing to http_svr. */
        json_buf = (char *) pvPortMalloc((size_t) json_bufsize);
        if (!json_buf)
        {
            return ERR_MEM;
        }

        json_len = http_get_values_for_uri(json_buf, json_bufsize, p_path);
        if (json_len <= 0)
        {
            vPortFree(json_buf);

            return LFS_ERR_NOENT;
        }

        /* Build a minimal HTTP/1.1 response header + body */
        hdr_len = snprintf(NULL, 0, p_hdr_fmt, json_len);
        if (hdr_len < 0)
        {
            return LFS_ERR_INVAL;
        }

        total = hdr_len + json_len;

        p_buf = (char *) pvPortMalloc((size_t) total);
        if (!p_buf)
        {
            vPortFree(json_buf);

            return ERR_MEM;
        }

        /* Fill header and JSON body */
        snprintf(p_buf, (size_t) hdr_len + 1, p_hdr_fmt, json_len);
        memcpy(p_buf + hdr_len, json_buf, (size_t) json_len);

        p_file->len        = total;
        p_file->index      = 0;
        p_file->data       = (const unsigned char *) p_buf;
        p_file->flags      = FS_FILE_FLAGS_HEADER_INCLUDED;
        p_file->pextension = NULL;

        return ERR_OK;
    }

    err = fs_validate_path(p_path);
    if (err != ERR_OK)
    {
        LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_LEVEL_WARNING,
                    ("[user_fs_open] invalid path '%s' rc=%d", p_path ? p_path : "(null)", (int) err));

        return err;
    }

    /* If the target path exists and is a zero-length regular file, remove it
     *  so the subsequent open will create a fresh file. Then remove other
     *  zero-length files in the parent directory as well. */
    sret = lfs_stat(&g_rm_littlefs0_lfs, p_path, &stinfo);
    if (sret == 0)
    {
        if ((stinfo.type == LFS_TYPE_REG) && (stinfo.size == 0))
        {
            (void) lfs_remove(&g_rm_littlefs0_lfs, p_path);
        }
    }

    last = strrchr(p_path, '/');
    if (!last)
    {
        parent[0] = '.';
        parent[1] = '\0';
    }
    else if (last == p_path)
    {
        parent[0] = '/';
        parent[1] = '\0';
    }
    else
    {
        len = (size_t) (last - p_path);
        if (len >= sizeof(parent))
        {
            len = sizeof(parent) - 1;
        }

        memcpy(parent, p_path, len);
        parent[len] = '\0';
    }

    /* Temporarily disable automatic removal of zero-length files here
     *  to help isolate a UsageFault observed in the field. This is
     *  non-invasive and can be reverted quickly if not needed. */

    /* fs_remove_zero_files_in_dir(parent); */

    rc = lfs_file_open(&g_rm_littlefs0_lfs, &p_file->lfs_file, p_path, LFS_O_RDONLY);
    if (rc < 0)
    {
        LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_LEVEL_WARNING,
                    ("[user_fs_open] lfs_file_open('%s') failed rc=%d", p_path, rc));

        return rc;
    }

    fsz = lfs_file_size(&g_rm_littlefs0_lfs, &p_file->lfs_file);
    if (fsz < 0)
    {
        lfs_file_close(&g_rm_littlefs0_lfs, &p_file->lfs_file);
        LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_LEVEL_WARNING,
                    ("[user_fs_open] lfs_file_size('%s') failed fsz=%d", p_path, (int) fsz));

        return fsz;
    }

    // This fixes an issue where lfs_file_size() moves the file position
    to_read = (int) fsz;

    if (fsz > 0)
    {
        // Read first byte
        rc = lfs_file_seek(&g_rm_littlefs0_lfs, &p_file->lfs_file, 0, LFS_SEEK_SET);
        if (rc < 0)
        {
            lfs_file_close(&g_rm_littlefs0_lfs, &p_file->lfs_file);

            return rc;
        }

        rlen = lfs_file_read(&g_rm_littlefs0_lfs, &p_file->lfs_file, &first_byte, 1);
        if (rlen != 1)
        {
            lfs_file_close(&g_rm_littlefs0_lfs, &p_file->lfs_file);

            return (rlen < 0) ? rlen : ERR_VAL;
        }

        // Allocate buffer for entire file
        p_buf = (uint8_t *) pvPortMalloc((size_t) to_read);
        if (!p_buf)
        {
            lfs_file_close(&g_rm_littlefs0_lfs, &p_file->lfs_file);

            return ERR_MEM;
        }

        // Copy first byte
        p_buf[0] = first_byte;

        // Seek to position 1 and read remaining bytes
        if (fsz > 1)
        {
            rc = lfs_file_seek(&g_rm_littlefs0_lfs, &p_file->lfs_file, 1, LFS_SEEK_SET);
            if (rc < 0)
            {
                vPortFree(p_buf);
                lfs_file_close(&g_rm_littlefs0_lfs, &p_file->lfs_file);

                return rc;
            }

            rlen = lfs_file_read(&g_rm_littlefs0_lfs, &p_file->lfs_file, p_buf + 1, (lfs_size_t) (fsz - 1));
            if (rlen < 0)
            {
                vPortFree(p_buf);
                lfs_file_close(&g_rm_littlefs0_lfs, &p_file->lfs_file);

                return rlen;
            }

            rlen = rlen + 1;           // Total bytes read
        }
        else
        {
            rlen = 1;
        }
    }
    else
    {
        p_buf = NULL;
        rlen  = 0;
    }

    if (rlen < 0)
    {
        if (p_buf)
        {
            vPortFree(p_buf);
        }

        lfs_file_close(&g_rm_littlefs0_lfs, &p_file->lfs_file);

        return rlen;
    }

    p_file->len        = (int) fsz;
    p_file->index      = 0;
    p_file->data       = (const unsigned char *) p_buf;
    p_file->flags      = FS_FILE_FLAGS_HEADER_PERSISTENT;
    p_file->pextension = NULL;

    lfs_file_close(&g_rm_littlefs0_lfs, &p_file->lfs_file);

    // DON'T free the buffer - it will be served to HTTP client
    // The buffer will be freed in user_fs_close()

    return ERR_OK;
}

/* Called when httpd wants more bytes */
int user_fs_read (struct lfs_file_data * p_file, char * p_buffer, int count)
{
    int r;

    if (!p_file)
    {
        return -1;
    }

    r = lfs_file_read(&g_rm_littlefs0_lfs, &p_file->lfs_file, p_buffer, count);
    if (r > 0)
    {
        p_file->index += r;
    }

    return r;
}

/* Close the file */
void user_fs_close (struct lfs_file_data * p_file)
{
    if (!p_file)
    {
        return;
    }

    // Free the buffer that was allocated in user_fs_open()
    if (p_file->data)
    {
        vPortFree((void *) p_file->data);
        p_file->data = NULL;
    }

    // Now close the file handle
    lfs_file_close(&g_rm_littlefs0_lfs, &p_file->lfs_file);
 #if LWIP_HTTPD_FILE_STATE
    fs_state_free(p_file, p_file->state);
 #endif
}

#endif                                 // SUPPORT_FSP_RM_FS_W

/*-----------------------------------------------------------------------------------*/
err_t fs_open (struct fs_file * file, const char * name)
{
    const struct fsdata_file * f;

    if ((file == NULL) || (name == NULL))
    {
        return ERR_ARG;
    }

#if LWIP_HTTPD_CUSTOM_FILES && SUPPORT_FSP_RM_FS_W
    if (fs_open_custom(file, name))
    {
        file->is_custom_file = 1;

        return ERR_OK;
    }
    file->is_custom_file = 0;
#endif                                 /* LWIP_HTTPD_CUSTOM_FILES && SUPPORT_FSP_RM_FS_W */

    for (f = FS_ROOT; f != NULL; f = f->next)
    {
        if (!strcmp(name, (const char *) f->name))
        {
            file->data       = (const char *) f->data;
            file->len        = f->len;
            file->index      = f->len;
            file->pextension = NULL;
            file->flags      = f->flags;
#if HTTPD_PRECALCULATED_CHECKSUM
            file->chksum_count = f->chksum_count;
            file->chksum       = f->chksum;
#endif                                 /* HTTPD_PRECALCULATED_CHECKSUM */
#if LWIP_HTTPD_FILE_STATE
            file->state = fs_state_init(file, name);
#endif                                 /* #if LWIP_HTTPD_FILE_STATE */
            return ERR_OK;
        }
    }

    /* file not found */
    return ERR_VAL;
}

/*-----------------------------------------------------------------------------------*/
void fs_close (struct fs_file * file)
{
#if LWIP_HTTPD_CUSTOM_FILES
    if (file->is_custom_file)
    {
        fs_close_custom(file);
    }
#endif                                 /* LWIP_HTTPD_CUSTOM_FILES */
#if LWIP_HTTPD_FILE_STATE
    fs_state_free(file, file->state);
#endif                                 /* #if LWIP_HTTPD_FILE_STATE */
    LWIP_UNUSED_ARG(file);
}

/*-----------------------------------------------------------------------------------*/
#if LWIP_HTTPD_DYNAMIC_FILE_READ
 #if LWIP_HTTPD_FS_ASYNC_READ
int fs_read_async (struct fs_file * file, char * buffer, int count, fs_wait_cb callback_fn, void * callback_arg)
 #else                                 /* LWIP_HTTPD_FS_ASYNC_READ */
int fs_read (struct fs_file * file, char * buffer, int count)
 #endif /* LWIP_HTTPD_FS_ASYNC_READ */
{
    int read;
    if (file->index == file->len)
    {
        return FS_READ_EOF;
    }

 #if LWIP_HTTPD_FS_ASYNC_READ
    LWIP_UNUSED_ARG(callback_fn);
    LWIP_UNUSED_ARG(callback_arg);
 #endif                                /* LWIP_HTTPD_FS_ASYNC_READ */
 #if LWIP_HTTPD_CUSTOM_FILES
    if (file->is_custom_file)
    {
  #if LWIP_HTTPD_FS_ASYNC_READ

        return fs_read_async_custom(file, buffer, count, callback_fn, callback_arg);
  #else                                /* LWIP_HTTPD_FS_ASYNC_READ */
        return fs_read_custom(file, buffer, count);
  #endif /* LWIP_HTTPD_FS_ASYNC_READ */
    }
 #endif /* LWIP_HTTPD_CUSTOM_FILES */

    read = file->len - file->index;
    if (read > count)
    {
        read = count;
    }

    MEMCPY(buffer, (file->data + file->index), read);
    file->index += read;

    return read;
}

#endif                                 /* LWIP_HTTPD_DYNAMIC_FILE_READ */
/*-----------------------------------------------------------------------------------*/
#if LWIP_HTTPD_FS_ASYNC_READ
int fs_is_file_ready (struct fs_file * file, fs_wait_cb callback_fn, void * callback_arg)
{
    if (file != NULL)
    {
 #if LWIP_HTTPD_FS_ASYNC_READ
  #if LWIP_HTTPD_CUSTOM_FILES
        if (!fs_canread_custom(file))
        {
            if (fs_wait_read_custom(file, callback_fn, callback_arg))
            {
                return 0;
            }
        }

  #else                                /* LWIP_HTTPD_CUSTOM_FILES */
        LWIP_UNUSED_ARG(callback_fn);
        LWIP_UNUSED_ARG(callback_arg);
  #endif /* LWIP_HTTPD_CUSTOM_FILES */
 #endif /* LWIP_HTTPD_FS_ASYNC_READ */
    }

    return 1;
}

#endif                                 /* LWIP_HTTPD_FS_ASYNC_READ */
/*-----------------------------------------------------------------------------------*/
int fs_bytes_left (struct fs_file * file)
{
    return file->len - file->index;
}
