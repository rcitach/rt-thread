/*
 * SHA1-based key derivation function (PBKDF2) for IEEE 802.11i
 * Copyright (c) 2003-2005, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "supp_common.h"
#include "sha1.h"
#include "common.h"
#include "mbedtls/sha1.h"
#include "mbedtls/md.h"
#include "mbedtls/pkcs5.h"

/**
 * pbkdf2_sha1 - SHA1-based key derivation function (PBKDF2) for IEEE 802.11i
 * @passphrase: ASCII passphrase
 * @ssid: SSID
 * @ssid_len: SSID length in bytes
 * @iterations: Number of iterations to run
 * @buf: Buffer for the generated key
 * @buflen: Length of the buffer in bytes
 * Returns: 0 on success, -1 of failure
 *
 * This function is used to derive PSK for WPA-PSK. For this protocol,
 * iterations is set to 4096 and buflen to 32. This function is described in
 * IEEE Std 802.11-2004, Clause H.4. The main construction is from PKCS#5 v2.0.
 */
int pbkdf2_sha1(const char *passphrase, const u8 *ssid, size_t ssid_len,
		int iterations, u8 *buf, size_t buflen)
{
    int ret = 0;

    mbedtls_md_context_t ctx;
    const mbedtls_md_info_t *info_sha1;

    mbedtls_md_init(&ctx);

    info_sha1 = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
    if (info_sha1 == NULL) {
        return -1;
    }

    ret = mbedtls_md_setup(&ctx, info_sha1, 1);
    if (ret != 0) {
        goto cleanup;
    }

    ret = mbedtls_pkcs5_pbkdf2_hmac(&ctx,
            (const unsigned char *)passphrase, strlen(passphrase),
            ssid, ssid_len,
            (unsigned int) iterations,
            buflen, buf);
    if (ret != 0) {
        goto cleanup;
    }

cleanup:

    mbedtls_md_free(&ctx);

    return (ret ? -1 : 0);
}
