#include "includes.h"

#include "crypto.h"
#include "random.h"
#include "common.h"
#include "mbedtls/private_access.h"
#include "mbedtls/platform.h"
#include "mbedtls/bignum.h"
#include "mbedtls/md5.h"
#include "mbedtls/sha1.h"
#include "mbedtls/sha256.h"
#include "mbedtls/sha512.h"
#include "mbedtls/aes.h"
#include "mbedtls/ccm.h"
#include "mbedtls/gcm.h"
#include "mbedtls/ecp.h"
#include "mbedtls/ecdh.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/md.h"

extern bool hsu_hmac_sha512(const uint8_t key[], const size_t key_len, int nb_elem,
			    const uint8_t *addr[], const size_t *len, uint32_t *mac);
extern bool hsu_hmac_sha384(const uint8_t key[], const size_t key_len, int nb_elem,
			    const uint8_t *addr[], const size_t *len, uint32_t *mac);
extern bool hsu_hmac_sha256(const uint8_t key[], const size_t key_len, int nb_elem,
			    const uint8_t *addr[], const size_t *len, uint32_t *mac);
extern bool hsu_hmac_sha1(const uint8_t key[], const size_t key_len, int nb_elem,
			  const uint8_t *addr[], const size_t *len, uint32_t *mac);
extern bool hsu_sha1(int nb_elem, const uint8_t *addr[], const size_t *len, uint32_t *sha);
extern bool hsu_sha256(int nb_elem, const uint8_t *addr[], const size_t *len, uint32_t *sha);
extern bool hsu_sha384(int nb_elem, const uint8_t *addr[], const size_t *len, uint32_t *sha);
extern bool hsu_sha512(int nb_elem, const uint8_t *addr[], const size_t *len, uint32_t *sha);

/* For removing compile errors ... */
#pragma GCC diagnostic ignored "-Wempty-body"

#define CRYPTO_MBEDTLS_RET_CHK(f)   \
    do {                            \
        if ((ret = (f)) != 0)       \
            goto cleanup;           \
    } while(0)

static mbedtls_mpi *crypto_mbedtls_compute_y_sqr(mbedtls_ecp_group *grp, const mbedtls_mpi *x);

#if 0 /* it's for debugging. */
void crypto_bignum_hexdump(const char *title, const struct crypto_bignum *a)
{
    unsigned char buffer[128] = {0x00,};

    size_t len = mbedtls_mpi_size((mbedtls_mpi *)a);

    crypto_bignum_to_bin(a, buffer, 128, 0);

    ra6wx_crypto_prt("Hexdump: %s(%ld)\n", title, len);

    ra6wx_crypto_prt("0x%02x", buffer[0]);
    for (int idx = 1 ; idx < len ; idx++) {
        ra6wx_crypto_prt("%02x", buffer[idx]);
        if ((idx+1)%16==0) ra6wx_crypto_prt("\n");
    }

    ra6wx_crypto_prt("\n");

    return ;
}
#endif

int md5_vector(size_t num_elem, const u8 *addr[], const size_t *len, u8 *mac)
{
    mbedtls_md5_context ctx;
    size_t i;
    int ret = 0;

    mbedtls_md5_init(&ctx);

    ret = mbedtls_md5_starts(&ctx);
    if (ret) {
        goto cleanup;
    }

    for (i = 0; i < num_elem ; i++)
        mbedtls_md5_update(&ctx, addr[i], len[i]);

    mbedtls_md5_finish(&ctx, mac);

cleanup:

    mbedtls_md5_free(&ctx);

    return ret ? -1 : 0;
}

int sha1_vector(size_t num_elem, const u8 *addr[], const size_t *len, u8 *mac)
{
    mbedtls_sha1_context ctx;
    size_t i;
    int ret = 0;

    ret = hsu_sha1((int16) num_elem, addr, len, (uint32_t *)mac) ? 0 : -1;
    if (ret) {
        mbedtls_sha1_init(&ctx);

        CRYPTO_MBEDTLS_RET_CHK(mbedtls_sha1_starts(&ctx));

        for (i = 0; i < num_elem; i++) {
            CRYPTO_MBEDTLS_RET_CHK(mbedtls_sha1_update(&ctx, addr[i], len[i]));
        }

        CRYPTO_MBEDTLS_RET_CHK(mbedtls_sha1_finish(&ctx, mac));

cleanup:
        mbedtls_sha1_free(&ctx);
    }

    return ret ? -1 : 0;
}

int sha256_vector(size_t num_elem, const u8 *addr[], const size_t *len, u8 *mac)
{
    mbedtls_sha256_context ctx;
    size_t i;
    int ret = 0;

    ret = (hsu_sha256((int) num_elem, addr, len, (uint32_t *)mac) ? 0 : -1);
    if (ret) {
        mbedtls_sha256_init(&ctx);

        CRYPTO_MBEDTLS_RET_CHK(mbedtls_sha256_starts(&ctx, 0));

        for (i = 0; i < num_elem; i++) {
            CRYPTO_MBEDTLS_RET_CHK(mbedtls_sha256_update(&ctx, addr[i], len[i]));
        }

        CRYPTO_MBEDTLS_RET_CHK(mbedtls_sha256_finish(&ctx, mac));

cleanup:
        mbedtls_sha256_free(&ctx);
    }

    return ret ? -1 : 0;
}

int sha384_vector(size_t num_elem, const u8 *addr[], const size_t *len, u8 *mac)
{
    mbedtls_sha512_context ctx;
    size_t i;
    int ret = 0;

    ret = hsu_sha384((int) num_elem, addr, len, (uint32_t *)mac) ? 0 : -1;
    if (ret) {
        mbedtls_sha512_init(&ctx);

        CRYPTO_MBEDTLS_RET_CHK(mbedtls_sha512_starts(&ctx, 1));

        for (i = 0; i < num_elem; i++) {
            CRYPTO_MBEDTLS_RET_CHK(mbedtls_sha512_update(&ctx, addr[i], len[i]));
        }

        CRYPTO_MBEDTLS_RET_CHK(mbedtls_sha512_finish(&ctx, mac));

cleanup:
        mbedtls_sha512_free(&ctx);
    }

    return ret ? -1 : 0;
}

int sha512_vector(size_t num_elem, const u8 *addr[], const size_t *len, u8 *mac)
{
    mbedtls_sha512_context ctx;
    size_t i;
    int ret = 0;

    ret = hsu_sha512((int) num_elem, addr, len, (uint32_t *)mac) ? 0 : -1;
    if (ret) {
        mbedtls_sha512_init(&ctx);

        CRYPTO_MBEDTLS_RET_CHK(mbedtls_sha512_starts(&ctx, 0));

        for (i = 0; i < num_elem; i++) {
            CRYPTO_MBEDTLS_RET_CHK(mbedtls_sha512_update(&ctx, addr[i], len[i]));
        }

        CRYPTO_MBEDTLS_RET_CHK(mbedtls_sha512_finish(&ctx, mac));

cleanup:
        mbedtls_sha512_free(&ctx);
    }

    return ret ? -1 : 0;
}

static int hmac_vector(mbedtls_md_type_t md_type,
        const u8 *key, size_t key_len, size_t num_elem,
        const u8 *addr[], const size_t *len, u8 *mac)
{
    const mbedtls_md_info_t *md_info = NULL;
    mbedtls_md_context_t ctx;

    unsigned int i = 0;
    int ret = 0;

    md_info = mbedtls_md_info_from_type(md_type);
    if (!md_info) {
        return -1;
    }

    mbedtls_md_init(&ctx);

    CRYPTO_MBEDTLS_RET_CHK(mbedtls_md_setup(&ctx, md_info, 1));
    if (ret) {
        goto cleanup;
    }

    CRYPTO_MBEDTLS_RET_CHK(mbedtls_md_hmac_starts(&ctx, key, key_len));

    for (i = 0 ; i < num_elem ; i++) {
        CRYPTO_MBEDTLS_RET_CHK(mbedtls_md_hmac_update(&ctx, addr[i], len[i]));
    }

    CRYPTO_MBEDTLS_RET_CHK(mbedtls_md_hmac_finish(&ctx, mac));

cleanup:

    mbedtls_md_free(&ctx);

    return (ret == 0 ? 0 : -1);
}

int hmac_sha1_vector(const u8 *key, size_t key_len, size_t num_elem,
        const u8 *addr[], const size_t *len, u8 *mac)
{
    if(!hsu_hmac_sha1(key, key_len, (int) num_elem, addr, len, (uint32_t *)mac)) {
        return hmac_vector(MBEDTLS_MD_SHA1, key, key_len, num_elem, addr, len, mac);
    }
    return 0;
}

int hmac_sha1(const u8 *key, size_t key_len, const u8 *data,
        size_t data_len, u8 *mac)
{
    if (!hsu_hmac_sha1(key, key_len, 1, &data, &data_len, (uint32_t *)mac)) {
        return hmac_vector(MBEDTLS_MD_SHA1, key, key_len, 1, &data, &data_len, mac);
    }
    return 0;
}

int hmac_sha256_vector(const u8 *key, size_t key_len, size_t num_elem,
        const u8 *addr[], const size_t *len, u8 *mac)
{
    if (!hsu_hmac_sha256(key, key_len, (int) num_elem, addr, len, (uint32_t *)mac)) {
        return hmac_vector(MBEDTLS_MD_SHA256, key, key_len, num_elem, addr, len, mac);
    }
    return 0;
}

int hmac_sha256(const u8 *key, size_t key_len, const u8 *data,
        size_t data_len, u8 *mac)
{
    if (!hsu_hmac_sha256(key, key_len, 1, &data, &data_len, (uint32_t *)mac)) {
        return hmac_vector(MBEDTLS_MD_SHA256, key, key_len, 1, &data, &data_len, mac);
    }
    return 0;
}

int hmac_sha384_vector(const u8 *key, size_t key_len, size_t num_elem,
        const u8 *addr[], const size_t *len, u8 *mac)
{
    if (!hsu_hmac_sha384(key, key_len, (int) num_elem, addr, len, (uint32_t *)mac)) {
        return hmac_vector(MBEDTLS_MD_SHA384, key, key_len, num_elem, addr, len, mac);
    }
    return 0;
}

int hmac_sha384(const u8 *key, size_t key_len, const u8 *data,
        size_t data_len, u8 *mac)
{
    if (!hsu_hmac_sha384(key, key_len, 1, &data, &data_len, (uint32_t *)mac)) {
        return hmac_vector(MBEDTLS_MD_SHA384, key, key_len, 1, &data, &data_len, mac);
    }
    return 0;
}

int hmac_sha512_vector(const u8 *key, size_t key_len, size_t num_elem,
        const u8 *addr[], const size_t *len, u8 *mac)
{
    if (!hsu_hmac_sha512(key, key_len, (int) num_elem, addr, len, (uint32_t *)mac)) {
        return hmac_vector(MBEDTLS_MD_SHA512, key, key_len, num_elem, addr, len, mac);
    }
    return 0;
}

int hmac_sha512(const u8 *key, size_t key_len, const u8 *data,
        size_t data_len, u8 *mac)
{
    if (!hsu_hmac_sha512(key, key_len, 1, &data, &data_len, (uint32_t *)mac)) {
        return hmac_vector(MBEDTLS_MD_SHA512, key, key_len, 1, &data, &data_len, mac);
    }
    return 0;
}

void * aes_encrypt_init(const u8 *key, size_t len)
{
    mbedtls_aes_context *ctx = NULL;
    int ret = 0;

    ctx = mbedtls_calloc(1, sizeof(mbedtls_aes_context));
    if (ctx == NULL) {
        return NULL;
    }

    mbedtls_aes_init(ctx);

    ret = mbedtls_aes_setkey_enc(ctx, key, len * 8);
    if (ret) {
        mbedtls_aes_free(ctx);
        mbedtls_free(ctx);
        return NULL;
    }

        return ctx;
}

int aes_encrypt(void *ctx, const u8 *plain, u8 *crypt)
{
    mbedtls_aes_context *aes = (mbedtls_aes_context *)ctx;
    int ret = 0;

    if (ctx == NULL) {
        return -1;
    }

    ret = mbedtls_aes_crypt_ecb(aes, MBEDTLS_AES_ENCRYPT, plain, crypt);
    if (ret) {
        return -1;
    }

    return 0;
}

void aes_encrypt_deinit(void *ctx)
{
    mbedtls_aes_context *aes = (mbedtls_aes_context *)ctx;

    if (ctx) {
        mbedtls_aes_free(aes);
        mbedtls_free(aes);
    }
}

void * aes_decrypt_init(const u8 *key, size_t len)
{
    mbedtls_aes_context *ctx = NULL;
    int ret = 0;

    ctx = mbedtls_calloc(1, sizeof(mbedtls_aes_context));
    if (ctx == NULL) {
        return NULL;
    }

    mbedtls_aes_init(ctx);

    ret = mbedtls_aes_setkey_dec(ctx, key, len * 8);
    if (ret) {
        mbedtls_aes_free(ctx);
        mbedtls_free(ctx);
        return NULL;
    }

    return ctx;
}

int aes_decrypt(void *ctx, const u8 *crypt, u8 *plain)
{
    mbedtls_aes_context *aes = (mbedtls_aes_context *)ctx;
    int ret = 0;

    if (ctx == NULL) {
        return -1;
    }

    ret = mbedtls_aes_crypt_ecb(aes, MBEDTLS_AES_DECRYPT, crypt, plain);
    if (ret) {
        return -1;
    }

    return 0;
}

void aes_decrypt_deinit(void *ctx)
{
    mbedtls_aes_context *aes = (mbedtls_aes_context *)ctx;

    if (ctx) {
        mbedtls_aes_free(aes);
        mbedtls_free(aes);
    }
}

int aes_128_encrypt_block(const u8 *key, const u8 *in, u8 *out)
{
    void *ctx;
    int ret = 0;

    ctx = aes_encrypt_init(key, 16);
    if (ctx == NULL) {
        return -1;
    }

    ret = aes_encrypt(ctx, in, out);

    aes_encrypt_deinit(ctx);

    return ret;
}

int aes_ctr_encrypt(const u8 *key, size_t key_len, const u8 *nonce, u8 *data, size_t data_len)
{
    int ret;
    mbedtls_aes_context ctx;
    //u8 stream_block[AES_BLOCK_SIZE] = {0x00,}; //unused param
    //u8 nonce_counter[AES_BLOCK_SIZE] = {0x00,};
    u8 stream_block[16] = {0x00,}; //unused param
    u8 nonce_counter[16] = {0x00,};
    size_t offset = 0;

    mbedtls_aes_init(&ctx);

        ret = mbedtls_aes_setkey_enc(&ctx, key, key_len * 8);
    if (ret) {
        mbedtls_aes_free(&ctx);
        return -1;
    }

    os_memcpy(nonce_counter, nonce, 16);

    ret = mbedtls_aes_crypt_ctr(&ctx, data_len, &offset, nonce_counter, stream_block, data, data);
    if (ret) {
        mbedtls_aes_free(&ctx);
        return -1;
    }

    mbedtls_aes_free(&ctx);
    return 0;
}

int aes_128_ctr_encrypt(const u8 *key, const u8 *nonce, u8 *data, size_t data_len)
{
    return aes_ctr_encrypt(key, 16, nonce, data, data_len);
}

int aes_128_cbc_encrypt(const u8 *key, const u8 *iv, u8 *data, size_t data_len)
{
        int ret;
    mbedtls_aes_context ctx;
    //unsigned char iv_buf[AES_IV_SIZE] = {0x00,};
    unsigned char iv_buf[16] = {0x00,};

    mbedtls_aes_init(&ctx);

    ret = mbedtls_aes_setkey_enc(&ctx, key, 128);
    if (ret) {
        mbedtls_aes_free(&ctx);
        return -1;
    }

    os_memcpy(iv_buf, iv, 16);

    ret = mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_ENCRYPT, data_len, iv_buf, data, data);
    if (ret) {
        mbedtls_aes_free(&ctx);
        return -1;
    }

    return 0;
}

int aes_128_cbc_decrypt(const u8 *key, const u8 *iv, u8 *data, size_t data_len)
{
        int ret;
    mbedtls_aes_context ctx;
    //unsigned char iv_buf[AES_IV_SIZE] = {0x00,};
    unsigned char iv_buf[16] = {0x00,};

    mbedtls_aes_init(&ctx);

    ret = mbedtls_aes_setkey_dec(&ctx, key, 128);
    if (ret) {
        mbedtls_aes_free(&ctx);
        return -1;
    }

    os_memcpy(iv_buf, iv, 16);

    ret = mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_DECRYPT, data_len, iv_buf, data, data);
    if (ret) {
        mbedtls_aes_free(&ctx);
        return -1;
    }

    return 0;
}

int aes_gcm_ae(const u8 *key, size_t key_len,
        const u8 *iv, size_t iv_len,
        const u8 *plain, size_t plain_len,
        const u8 *aad, size_t aad_len,
        u8 *crypt, u8 *tag)
{
    int ret = 0;
    mbedtls_gcm_context ctx;
    mbedtls_cipher_id_t cipher = MBEDTLS_CIPHER_ID_AES;

    mbedtls_gcm_init(&ctx);

    ret = mbedtls_gcm_setkey(&ctx, cipher, key, key_len * 8);
    if (ret) {
        mbedtls_gcm_free(&ctx);
        return -1;
    }

    ret = mbedtls_gcm_crypt_and_tag(&ctx, MBEDTLS_GCM_ENCRYPT,
                plain_len,
                iv, iv_len,
                aad, aad_len,
                plain, crypt, 16, tag);
    if (ret) {
        mbedtls_gcm_free(&ctx);
        return -1;
    }

    mbedtls_gcm_free(&ctx);
    return 0;
}

int aes_gcm_ad(const u8 *key, size_t key_len,
        const u8 *iv, size_t iv_len,
        const u8 *crypt, size_t crypt_len,
        const u8 *aad, size_t aad_len, const u8 *tag,
        u8 *plain)
{
    int ret = 0;
    mbedtls_gcm_context ctx;
    mbedtls_cipher_id_t cipher = MBEDTLS_CIPHER_ID_AES;
    unsigned char tag_buf[16] = {0x00,};

    mbedtls_gcm_init(&ctx);

    ret = mbedtls_gcm_setkey(&ctx, cipher, key, key_len * 8);
    if (ret) {
        mbedtls_gcm_free(&ctx);
        return -1;
    }

    os_memcpy(tag_buf, tag, 16);

    ret = mbedtls_gcm_crypt_and_tag(&ctx, MBEDTLS_GCM_DECRYPT,
                crypt_len,
                iv, iv_len,
                aad, aad_len,
                crypt, plain, 16, tag_buf);
    if (ret) {
        mbedtls_gcm_free(&ctx);
        return -1;
    }

    mbedtls_gcm_free(&ctx);
    return 0;
}

int aes_gmac(const u8 *key, size_t key_len,
        const u8 *iv, size_t iv_len,
        const u8 *aad, size_t aad_len, u8 *tag)
{
    return aes_gcm_ae(key, key_len, iv, iv_len, NULL, 0, aad, aad_len, NULL, tag);
}

int aes_ccm_ae(const u8 *key, size_t key_len, const u8 *nonce,
        size_t M, const u8 *plain, size_t plain_len,
        const u8 *aad, size_t aad_len, u8 *crypt, u8 *auth)
{
    int ret = 0;
    mbedtls_ccm_context ctx;
    mbedtls_cipher_id_t cipher = MBEDTLS_CIPHER_ID_AES;

    mbedtls_ccm_init(&ctx);

    ret = mbedtls_ccm_setkey(&ctx, cipher, key, key_len * 8);
    if (ret) {
        mbedtls_ccm_free(&ctx);
        return -1;
    }

    ret = mbedtls_ccm_encrypt_and_tag(&ctx, plain_len,
                nonce, M,
                aad, aad_len,
                plain, crypt, auth, 8);
    if (ret) {
        mbedtls_ccm_free(&ctx);
        return -1;
    }

    mbedtls_ccm_free(&ctx);
    return 0;
}

int aes_ccm_ad(const u8 *key, size_t key_len, const u8 *nonce,
        size_t M, const u8 *crypt, size_t crypt_len,
        const u8 *aad, size_t aad_len, const u8 *auth,
        u8 *plain)
{
    int ret = 0;
    mbedtls_ccm_context ctx;
    mbedtls_cipher_id_t cipher = MBEDTLS_CIPHER_ID_AES;

    mbedtls_ccm_init(&ctx);

    ret = mbedtls_ccm_setkey(&ctx, cipher, key, key_len * 8);
    if (ret) {
        mbedtls_ccm_free(&ctx);
        return -1;
    }

    ret = mbedtls_ccm_auth_decrypt(&ctx, crypt_len,
                nonce, M,
                aad, aad_len,
                crypt, plain,
                auth, 8);
    if (ret) {
        mbedtls_ccm_free(&ctx);
        return -1;
    }

    mbedtls_ccm_free(&ctx);
    return 0;

}


/**
 * dh_init - Initialize Diffie-Hellman handshake
 * @dh: Selected Diffie-Hellman group
 * @priv: Pointer for returning Diffie-Hellman private key
 * Returns: Diffie-Hellman public value
 */
int crypto_dh_init(u8 generator, const u8 *prime, size_t prime_len, u8 *privkey,
                   u8 *pubkey)
{
    int ret = 0;
    size_t pubkey_len, pad;

    if (random_get_bytes(privkey, prime_len)) {
        return -1;
    }

    if (os_memcmp(privkey, prime, prime_len) > 0) {
        /* Make sure private value is smaller than prime */
        privkey[0] = 0;
    }

    pubkey_len = prime_len;

    ret = crypto_mod_exp(&generator, 1,
            privkey, prime_len,
            prime, prime_len,
            pubkey, &pubkey_len);
    if (ret) {
        return -1;
    }

    if (pubkey_len < prime_len) {
        pad = prime_len - pubkey_len;
        os_memmove(pubkey + pad, pubkey, pubkey_len);
        os_memset(pubkey, 0, pad);
    }

    return 0;
}

int crypto_dh_derive_secret(u8 generator, const u8 *prime, size_t prime_len,
        const u8 *privkey, size_t privkey_len,
        const u8 *pubkey, size_t pubkey_len,
        u8 *secret, size_t *len)
{
    (void) generator;
    return crypto_mod_exp(pubkey, pubkey_len, privkey, privkey_len,
            prime, prime_len, secret, len);
}

int crypto_mod_exp(const u8 *base, size_t base_len,
        const u8 *power, size_t power_len,
        const u8 *modulus, size_t modulus_len,
        u8 *result, size_t *result_len)
{
    //X(result) = A(base)^E(power) mod N(modulus)
    int ret = 0;
    mbedtls_mpi *A, *E, *N, *X;

    A = mbedtls_calloc(1, sizeof(mbedtls_mpi));
    E = mbedtls_calloc(1, sizeof(mbedtls_mpi));
    N = mbedtls_calloc(1, sizeof(mbedtls_mpi));
    X = mbedtls_calloc(1, sizeof(mbedtls_mpi));

    if (A == NULL || E == NULL || N == NULL || X == NULL) {
        ret = -1;
        goto end;
    }

    mbedtls_mpi_init(A);
    mbedtls_mpi_init(E);
    mbedtls_mpi_init(N);
    mbedtls_mpi_init(X);

    ret = mbedtls_mpi_read_binary(A, base, base_len);
    if (ret) {
        goto end;
    }

    ret = mbedtls_mpi_read_binary(E, power, power_len);
    if (ret) {
        goto end;
    }

    ret = mbedtls_mpi_read_binary(N, modulus, modulus_len);
    if (ret) {
        goto end;
    }

    //X = A^E mod N
    ret = mbedtls_mpi_exp_mod(X, A, E, N, NULL);
    if (ret) {
        goto end;
    }

    // Export MPI structure into unsigned binary data
    ret = mbedtls_mpi_write_binary(X, result, *result_len);
    if (ret) {
        goto end;
    }

    *result_len = mbedtls_mpi_size(X);

end:

    mbedtls_mpi_free(A);
    mbedtls_mpi_free(E);
    mbedtls_mpi_free(N);
    mbedtls_mpi_free(X);

    if (A) mbedtls_free(A);
    if (E) mbedtls_free(E);
    if (N) mbedtls_free(N);
    if (X) mbedtls_free(X);

    return ret;
}
struct crypto_bignum * crypto_bignum_init(void)
{
    mbedtls_mpi *bn = os_zalloc(sizeof(mbedtls_mpi));
    if (!bn) {
        return NULL;
    }

    mbedtls_mpi_init(bn);

    return (struct crypto_bignum *)bn;
}

struct crypto_bignum * crypto_bignum_init_set(const u8 *buf, size_t len)
{
    int ret = 0;
    mbedtls_mpi *bn = os_zalloc(sizeof(mbedtls_mpi));
    if (bn == NULL) {
        return NULL;
    }

    MBEDTLS_MPI_CHK(mbedtls_mpi_read_binary(bn, buf, len));

cleanup:

    if (ret) {
        os_free(bn);
        bn = NULL;
    }
    return (struct crypto_bignum *)bn;
}

struct crypto_bignum * crypto_bignum_init_uint(unsigned int val)
{

    mbedtls_mpi *bn = os_zalloc(sizeof(mbedtls_mpi));
    if (bn == NULL) {
        return NULL;
    }

    mbedtls_mpi_init(bn);
    mbedtls_mpi_lset(bn, (mbedtls_mpi_sint) val);

    return (struct crypto_bignum *) bn;
}

void crypto_bignum_deinit(struct crypto_bignum *n, int clear)
{
    (void) clear;
    if (!n)
        return ;

    mbedtls_mpi_free((mbedtls_mpi *)n);
    os_free((mbedtls_mpi *)n);

    return ;
}

int crypto_bignum_rand(struct crypto_bignum *r, const struct crypto_bignum *m)
{
    int ret = 0;
    size_t len = 0;

    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;

    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);
    len = mbedtls_mpi_size((const mbedtls_mpi *)m);

    MBEDTLS_MPI_CHK(mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0));

    MBEDTLS_MPI_CHK(mbedtls_mpi_fill_random((mbedtls_mpi *)r, len, mbedtls_ctr_drbg_random, &ctr_drbg));

    while (mbedtls_mpi_cmp_mpi((const mbedtls_mpi *)r, (const mbedtls_mpi *)m) >= 0) {
        MBEDTLS_MPI_CHK(mbedtls_mpi_shift_r((mbedtls_mpi *)r, 1));
    }

cleanup:

    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);

    return ret ? -1 : 0;
}

int crypto_bignum_to_bin(const struct crypto_bignum *a,
        u8 *buf, size_t buflen, size_t padlen)
{
    int num_bytes, offset;

    if (padlen > buflen) {
        return -1;
    }

    num_bytes = (int) mbedtls_mpi_size((mbedtls_mpi *) a);

    if ((size_t) num_bytes > buflen) {
        return -1;
    }

    if (padlen > (size_t) num_bytes) {
        offset = (int) padlen - num_bytes;
    } else {
        offset = 0;
    }

    os_memset(buf, 0, (size_t) offset);
    mbedtls_mpi_write_binary((mbedtls_mpi *) a,
        buf + offset, mbedtls_mpi_size((mbedtls_mpi *)a) );

    return num_bytes + offset;

}

int crypto_bignum_add(const struct crypto_bignum *a,
        const struct crypto_bignum *b,
        struct crypto_bignum *c)
{
    //c = a + b
    return mbedtls_mpi_add_mpi((mbedtls_mpi *) c,
            (const mbedtls_mpi *) a, (const mbedtls_mpi *) b) ?  -1 : 0;

}

int crypto_bignum_mod(const struct crypto_bignum *a,
        const struct crypto_bignum *b,
        struct crypto_bignum *c)
{
    //c = a mod b
    return mbedtls_mpi_mod_mpi((mbedtls_mpi *) c,
            (const mbedtls_mpi *) a, (const mbedtls_mpi *) b) ? -1 : 0;
}

int crypto_bignum_exptmod(const struct crypto_bignum *a,
        const struct crypto_bignum *b,
        const struct crypto_bignum *c,
        struct crypto_bignum *d)
{
    //d = a^b mod c
    return  mbedtls_mpi_exp_mod((mbedtls_mpi *) d,
            (const mbedtls_mpi *) a, (const mbedtls_mpi *) b,
            (const mbedtls_mpi *) c, NULL) ? -1 : 0;

}

int crypto_bignum_inverse(const struct crypto_bignum *a,
        const struct crypto_bignum *b,
        struct crypto_bignum *c)
{
    //c = a^-1 mod b
    return mbedtls_mpi_inv_mod((mbedtls_mpi *) c, (const mbedtls_mpi *) a,
            (const mbedtls_mpi *) b) ? -1 : 0;

}

int crypto_bignum_sub(const struct crypto_bignum *a,
        const struct crypto_bignum *b,
        struct crypto_bignum *c)
{
    //c = a - b
    return mbedtls_mpi_sub_mpi((mbedtls_mpi *) c,
            (const mbedtls_mpi *) a, (const mbedtls_mpi *) b) ?  -1 : 0;

}

int crypto_bignum_div(const struct crypto_bignum *a,
        const struct crypto_bignum *b,
        struct crypto_bignum *c)
{
    //c = a * b + r
    return mbedtls_mpi_div_mpi((mbedtls_mpi *) c, NULL,
            (const mbedtls_mpi *) a, (const mbedtls_mpi *) b) ?  -1 : 0;

}

#if 0 /* TODO: Use HW Engine directly for performance. */
int crypto_bignum_mulmod(const struct crypto_bignum *a,
        const struct crypto_bignum *b,
        const struct crypto_bignum *c,
        struct crypto_bignum *d)
{
    int ret = 0;

    extern int mbedtls_mpi_mul_mod_mpi( mbedtls_mpi *X, const mbedtls_mpi *A, const mbedtls_mpi *B, const mbedtls_mpi *P);

    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod_mpi((mbedtls_mpi *)d,
                (const mbedtls_mpi *)a,
                (const mbedtls_mpi *)b,
                (const mbedtls_mpi *)c));
cleanup:

    return ret ? -1 : 0;
}
#else
int crypto_bignum_mulmod(const struct crypto_bignum *a,
        const struct crypto_bignum *b,
        const struct crypto_bignum *c,
        struct crypto_bignum *d)
{
    int ret = 0;
    mbedtls_mpi temp;

    mbedtls_mpi_init(&temp);

    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&temp, (const mbedtls_mpi *)a, (const mbedtls_mpi *)b));

    MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi((mbedtls_mpi *)d, &temp, (mbedtls_mpi *)c));

cleanup:

    mbedtls_mpi_free(&temp);

    return ret ? -1 : 0;
}
#endif

int crypto_bignum_sqrmod(const struct crypto_bignum *a,
        const struct crypto_bignum *b,
        struct crypto_bignum *c)
{
    return crypto_bignum_mulmod(a, a, b, c);
}

int crypto_bignum_rshift(const struct crypto_bignum *a, int n,
        struct crypto_bignum *r)
{
    int res = 0;

    res = mbedtls_mpi_copy((mbedtls_mpi *)r, (const mbedtls_mpi *)a);
    if (res) {
        return -1;
    }

    res = mbedtls_mpi_shift_r((mbedtls_mpi *)r, (size_t) n);

    return res ? -1 : 0;
}

int crypto_bignum_cmp(const struct crypto_bignum *a,
        const struct crypto_bignum *b)
{
    return mbedtls_mpi_cmp_mpi((const mbedtls_mpi *) a, (const mbedtls_mpi *) b);
}

int crypto_bignum_bits(const struct crypto_bignum *a)
{
    return (int) mbedtls_mpi_bitlen((mbedtls_mpi *) a);
}

int crypto_bignum_is_zero(const struct crypto_bignum *a)
{
    return (mbedtls_mpi_cmp_int((const mbedtls_mpi *) a, 0) == 0);
}

int crypto_bignum_is_one(const struct crypto_bignum *a)
{
    return (mbedtls_mpi_cmp_int((const mbedtls_mpi *) a, 1) == 0);
}

int crypto_bignum_is_odd(const struct crypto_bignum *a)
{
    return (mbedtls_mpi_cmp_int((const mbedtls_mpi *) a, 0) != 0)
        && (mbedtls_mpi_get_bit((const mbedtls_mpi *) a, 0));
}


#if 0 /* TODO: Use HW Engine directly for performance. */
int crypto_bignum_legendre(const struct crypto_bignum *a,
                           const struct crypto_bignum *p)
{
    extern int mbedtls_mpi_legendre(const mbedtls_mpi *A, const mbedtls_mpi *P);
    int res = -2;

    res = mbedtls_mpi_legendre((const mbedtls_mpi *)a, (const mbedtls_mpi *)p);

    return res;
}
#else
int crypto_bignum_legendre(const struct crypto_bignum *a,
                           const struct crypto_bignum *p)
{
    mbedtls_mpi exp, tmp;
    int res = -2, ret;

    mbedtls_mpi_init(&exp);
    mbedtls_mpi_init(&tmp);

    /* exp = (p-1) / 2 */
    MBEDTLS_MPI_CHK(mbedtls_mpi_sub_int(&exp, (const mbedtls_mpi *) p, 1));
    MBEDTLS_MPI_CHK(mbedtls_mpi_shift_r(&exp, 1));
    MBEDTLS_MPI_CHK(mbedtls_mpi_exp_mod(&tmp, (const mbedtls_mpi *) a, &exp, (const mbedtls_mpi *) p, NULL));

    if (mbedtls_mpi_cmp_int(&tmp, 1) == 0) {
        res = 1;
    } else if (mbedtls_mpi_cmp_int(&tmp, 0) == 0
            /* The below check is workaround for the case where HW
             * does not behave properly for X ^ A mod M when X is
             * power of M. Instead of returning value 0, value M is
             * returned.*/
            || mbedtls_mpi_cmp_mpi(&tmp, (const mbedtls_mpi *)p) == 0) {
        res = 0;
    } else {
        res = -1;
    }

cleanup:
    mbedtls_mpi_free(&tmp);
    mbedtls_mpi_free(&exp);
    return(ret ? -2 : res);
}
#endif

int crypto_bignum_to_string(const struct crypto_bignum *a,
        u8 *buf, size_t buflen, size_t padlen)
{
    int num_bytes, offset;
    size_t outlen;

    if (padlen > buflen) {
        return -1;
    }

    num_bytes = (int) mbedtls_mpi_size((mbedtls_mpi *) a);

    if (padlen > (size_t) num_bytes) {
        offset = (int) padlen - num_bytes;
    } else {
        offset = 0;
    }

    os_memset(buf, 0, (size_t) offset);
    mbedtls_mpi_write_string((mbedtls_mpi *)a, 16, (char *)(buf + offset),
            mbedtls_mpi_size((mbedtls_mpi *)a), &outlen);

    return (int) outlen;
}

int crypto_bignum_addmod(const struct crypto_bignum *a,
        const struct crypto_bignum *b,
        const struct crypto_bignum *c,
        struct crypto_bignum *d)
{
    struct crypto_bignum *tmp = crypto_bignum_init();
    int ret = -1;
    MBEDTLS_MPI_CHK(mbedtls_mpi_add_mpi((mbedtls_mpi *) tmp, (const mbedtls_mpi *) a, (const mbedtls_mpi *) b));

    MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi((mbedtls_mpi *) d, (const mbedtls_mpi *) tmp, (const mbedtls_mpi *) c));

cleanup:

    crypto_bignum_deinit(tmp, 0);
    return ret ? -1 : 0;
}

struct crypto_ec {
    mbedtls_ecp_group group;
};

struct crypto_ec * crypto_ec_init(int group)
{
    struct crypto_ec *e = NULL;
    mbedtls_ecp_group_id grp_id = MBEDTLS_ECP_DP_NONE;

    switch (group) {
    case 19:
        grp_id = MBEDTLS_ECP_DP_SECP256R1;
        break;
    case 20:
        grp_id = MBEDTLS_ECP_DP_SECP384R1;
        break;
#ifndef __WPA3_WIFI_CERTFIED__
    case 21:
        grp_id = MBEDTLS_ECP_DP_SECP521R1;
        break;
#if 0 /* TODO : required to compute Y */
    case 25:
        grp_id = MBEDTLS_ECP_DP_SECP192R1;
        break;
    case 26:
        grp_id = MBEDTLS_ECP_DP_SECP224R1;
        break;
#endif
    case 28:
        grp_id = MBEDTLS_ECP_DP_BP256R1;
        break;
    case 29:
        grp_id = MBEDTLS_ECP_DP_BP384R1;
        break;
    case 30:
        grp_id = MBEDTLS_ECP_DP_BP512R1;
        break;
#endif  /* __WPA3_WIFI_CERTFIED__ */
    default:
        return NULL;
    }

    e = os_zalloc(sizeof(*e));
    if (!e)
        return NULL;

    mbedtls_ecp_group_init(&e->group);

    if (mbedtls_ecp_group_load(&e->group, grp_id)) {
        crypto_ec_deinit(e);
        e = NULL;
    }

    return e;
}

void crypto_ec_deinit(struct crypto_ec *e)
{
    if (!e)
        return ;

    mbedtls_ecp_group_free(&e->group);
    os_free(e);
}

struct crypto_ec_point *crypto_ec_point_init(struct crypto_ec *e)
{
    mbedtls_ecp_point *pt;
    if (e == NULL) {
        return NULL;
    }

    pt = os_zalloc(sizeof(mbedtls_ecp_point));

    if( pt == NULL) {
        return NULL;
    }

    mbedtls_ecp_point_init(pt);

    return (struct crypto_ec_point *) pt;
}

size_t crypto_ec_prime_len(struct crypto_ec *e)
{
    return mbedtls_mpi_size(&e->group.P);
}

size_t crypto_ec_prime_len_bits(struct crypto_ec *e)
{
    return mbedtls_mpi_bitlen(&e->group.P);
}

size_t crypto_ec_order_len(struct crypto_ec *e)
{
    return (mbedtls_mpi_bitlen(&e->group.N) + 7) / 8;
}

const struct crypto_bignum * crypto_ec_get_prime(struct crypto_ec *e)
{
    return (const struct crypto_bignum *)&e->group.P;
}

const struct crypto_bignum * crypto_ec_get_order(struct crypto_ec *e)
{
    return (const struct crypto_bignum *)&e->group.N;
}

const struct crypto_bignum * crypto_ec_get_a(struct crypto_ec *e)
{
    return (const struct crypto_bignum *)&e->group.A;
}

const struct crypto_bignum * crypto_ec_get_b(struct crypto_ec *e)
{
    return (const struct crypto_bignum *)&e->group.B;
}

void crypto_ec_point_deinit(struct crypto_ec_point *p, int clear)
{
    (void) clear;
    mbedtls_ecp_point_free((mbedtls_ecp_point *) p);
    os_free(p);
}

int crypto_ec_point_to_bin(struct crypto_ec *e,
        const struct crypto_ec_point *point, u8 *x, u8 *y)
{
    int len = (int) mbedtls_mpi_size(&e->group.P);

    if (x) {
        if(crypto_bignum_to_bin((struct crypto_bignum *) & ((mbedtls_ecp_point *) point)->MBEDTLS_PRIVATE(X),
                    x, (size_t) len, (size_t) len) < 0) {
            return -1;
        }

    }

    if (y) {
        if(crypto_bignum_to_bin((struct crypto_bignum *) & ((mbedtls_ecp_point *) point)->MBEDTLS_PRIVATE(Y),
                    y, (size_t) len, (size_t) len) < 0) {
            return -1;
        }
    }

    return 0;
}

struct crypto_ec_point *crypto_ec_point_from_bin(struct crypto_ec *e,
        const u8 *val)
{
    mbedtls_ecp_point *pt;
    int len;
    int ret;

    if (e == NULL) {
        return NULL;
    }

    len = (int) mbedtls_mpi_size(&e->group.P);

    pt = os_zalloc(sizeof(mbedtls_ecp_point));
    if (!pt) {
        return NULL;
    }

    mbedtls_ecp_point_init(pt);

    MBEDTLS_MPI_CHK(mbedtls_mpi_read_binary(&pt->MBEDTLS_PRIVATE(X), val, (size_t) len));
    MBEDTLS_MPI_CHK(mbedtls_mpi_read_binary(&pt->MBEDTLS_PRIVATE(Y), val + len, (size_t) len));
    MBEDTLS_MPI_CHK(mbedtls_mpi_lset((&pt->MBEDTLS_PRIVATE(Z)), 1));

#if 0   /* To remove compile warning */
    if (ret)
        ;
#endif  // 0

    return (struct crypto_ec_point *) pt;

cleanup:
    mbedtls_ecp_point_free(pt);
    os_free(pt);
    return NULL;
}

int crypto_ec_point_add(struct crypto_ec *e, const struct crypto_ec_point *a,
        const struct crypto_ec_point *b,
        struct crypto_ec_point *c)
{
    int ret;
    mbedtls_mpi one;

    mbedtls_mpi_init(&one);

    MBEDTLS_MPI_CHK(mbedtls_mpi_lset(&one, 1));
    MBEDTLS_MPI_CHK(mbedtls_ecp_muladd(&e->group, (mbedtls_ecp_point *)c,
            &one, (const mbedtls_ecp_point *)a ,
            &one, (const mbedtls_ecp_point *)b));

cleanup:
    mbedtls_mpi_free(&one);
    return ret ? -1 : 0;
}


int crypto_ec_point_mul(struct crypto_ec *e, const struct crypto_ec_point *p,
        const struct crypto_bignum *b,
        struct crypto_ec_point *res)
{
    int ret;
#if 0
    MBEDTLS_MPI_CHK(mbedtls_ecp_mul(&e->group,
                (mbedtls_ecp_point *) res,
                (const mbedtls_mpi *)b,
                (const mbedtls_ecp_point *)p,
                NULL, NULL));

cleanup:
    return ret ? -1 : 0;
#else
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;

    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);

    MBEDTLS_MPI_CHK(mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0));

    MBEDTLS_MPI_CHK(mbedtls_ecp_mul(&e->group,
                (mbedtls_ecp_point *)res,
                (const mbedtls_mpi *)b,
                (const mbedtls_ecp_point *)p,
                mbedtls_ctr_drbg_random, &ctr_drbg));

cleanup:

    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);

    return ret ? -1 : 0;

#endif
}

/*  Currently mbedtls does not have any function for inverse
 *  This function calculates inverse of a point.
 *  Set R = -P
 */
static int ecp_opp(const mbedtls_ecp_group *grp, mbedtls_ecp_point *R, const mbedtls_ecp_point *P)
{
    int ret = 0;

    /* Copy */
    if (R != P) {
        MBEDTLS_MPI_CHK(mbedtls_ecp_copy(R, P));
    }

    /* In-place opposite */
    if (mbedtls_mpi_cmp_int(&R->MBEDTLS_PRIVATE(Y), 0) != 0) {
        MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mpi(&R->MBEDTLS_PRIVATE(Y), &grp->P, &R->MBEDTLS_PRIVATE(Y)));
    }

cleanup:
    return ( ret );
}

int crypto_ec_point_invert(struct crypto_ec *e, struct crypto_ec_point *p)
{
    return ecp_opp(&e->group, (mbedtls_ecp_point *) p, (mbedtls_ecp_point *) p) ? -1 : 0;
}

int crypto_ec_point_solve_y_coord(struct crypto_ec *e,
        struct crypto_ec_point *p,
        const struct crypto_bignum *x, int y_bit)
{
    mbedtls_mpi temp;
    mbedtls_mpi *y_sqr = NULL, *y;
    mbedtls_mpi_init(&temp);
    int ret = 0;

    y = &((mbedtls_ecp_point *)p)->MBEDTLS_PRIVATE(Y);

    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&((mbedtls_ecp_point *)p)->MBEDTLS_PRIVATE(X),
            (const mbedtls_mpi *)x));

    /* Faster way to find sqrt
     * Works only with curves having prime p
     * such that p ??3 (mod 4)
     *  y_ = (y2 ^ ((p+1)/4)) mod p
     *
     *  if y_bit: y = p-y_
     *   else y = y_`
     */
    y_sqr = (mbedtls_mpi *) crypto_ec_point_compute_y_sqr(e, x);

    if (y_sqr) {
        MBEDTLS_MPI_CHK(mbedtls_mpi_add_int(&temp, &e->group.P, 1));
        MBEDTLS_MPI_CHK(mbedtls_mpi_div_int(&temp, NULL, &temp, 4));
        MBEDTLS_MPI_CHK(mbedtls_mpi_exp_mod(y, y_sqr, &temp, &e->group.P, NULL));

        int is_odd = y_bit ? 1 : 0;
        int y_is_odd = crypto_bignum_is_odd((const struct crypto_bignum *)y) ? 1 : 0;

        if ((is_odd && y_is_odd)
            || (!is_odd && !y_is_odd)) {
            ;   //mod calculation is not required.
        } else {
            MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mpi(y, &e->group.P, y));
        }
        MBEDTLS_MPI_CHK(mbedtls_mpi_lset(&((mbedtls_ecp_point *)p)->MBEDTLS_PRIVATE(Z), 1));
    } else {
        ret = 1;
    }

cleanup:

    mbedtls_mpi_free(&temp);
    mbedtls_mpi_free(y_sqr);
    os_free(y_sqr);
    return ret ? -1 : 0;
}

struct crypto_bignum *
crypto_ec_point_compute_y_sqr(struct crypto_ec *e,
        const struct crypto_bignum *x)
{
    return (struct crypto_bignum *)crypto_mbedtls_compute_y_sqr(&e->group, (const mbedtls_mpi *)x);
}

int crypto_ec_point_is_at_infinity(struct crypto_ec *e,
        const struct crypto_ec_point *p)
{
    (void) e;
    return mbedtls_ecp_is_zero((mbedtls_ecp_point *) p);
}

int crypto_ec_point_is_on_curve(struct crypto_ec *e,
        const struct crypto_ec_point *p)
{
    return (mbedtls_ecp_check_pubkey(&e->group, (const mbedtls_ecp_point *)p) == 0 ? 1 : 0);
}

int crypto_ec_point_cmp(const struct crypto_ec *e,
        const struct crypto_ec_point *a,
        const struct crypto_ec_point *b)
{
    (void) e;
    return mbedtls_ecp_point_cmp((const mbedtls_ecp_point *) a,
            (const mbedtls_ecp_point *) b);
}

struct crypto_ecdh {
    mbedtls_ecdh_context ctx;
};

struct crypto_ecdh * crypto_ecdh_init(int group)
{
    int ret;
    struct crypto_ecdh *ecdh;
    mbedtls_ecp_group_id grp_id = MBEDTLS_ECP_DP_NONE;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;

    switch (group) {
    case 19:
        grp_id = MBEDTLS_ECP_DP_SECP256R1;
        break;
    case 20:
        grp_id = MBEDTLS_ECP_DP_SECP384R1;
        break;
    case 21:
        grp_id = MBEDTLS_ECP_DP_SECP521R1;
        break;
    case 25:
        grp_id = MBEDTLS_ECP_DP_SECP192R1;
        break;
    case 26:
        grp_id = MBEDTLS_ECP_DP_SECP224R1;
        break;
    case 28:
        grp_id = MBEDTLS_ECP_DP_BP256R1;
        break;
    case 29:
        grp_id = MBEDTLS_ECP_DP_BP384R1;
        break;
    case 30:
        grp_id = MBEDTLS_ECP_DP_BP512R1;
        break;
    default:
        return NULL;
    }

    ecdh = os_zalloc(sizeof(*ecdh));
    if (!ecdh)
        return NULL;

    mbedtls_ecdh_init(&ecdh->ctx);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);

#if defined(MBEDTLS_ECDH_LEGACY_CONTEXT)
    MBEDTLS_MPI_CHK(mbedtls_ecp_group_load(&(ecdh->ctx.MBEDTLS_PRIVATE(grp)), grp_id));
#else
    MBEDTLS_MPI_CHK(mbedtls_ecp_group_load(&(ecdh->ctx.MBEDTLS_PRIVATE(ctx).MBEDTLS_PRIVATE(mbed_ecdh).MBEDTLS_PRIVATE(grp)), grp_id));
#endif // MBEDTLS_ECDH_LEGACY_CONTEXT

    MBEDTLS_MPI_CHK(mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func,
                &entropy, NULL, 0));

    MBEDTLS_MPI_CHK(mbedtls_ecdh_setup(&ecdh->ctx, grp_id));

#if defined(MBEDTLS_ECDH_LEGACY_CONTEXT)
    MBEDTLS_MPI_CHK(mbedtls_ecdh_gen_public(&(ecdh->ctx.MBEDTLS_PRIVATE(grp)), &(ecdh->ctx.MBEDTLS_PRIVATE(d)),
                &(ecdh->ctx.MBEDTLS_PRIVATE(Q)), mbedtls_ctr_drbg_random, &ctr_drbg));
#else
    MBEDTLS_MPI_CHK(mbedtls_ecdh_gen_public(&(ecdh->ctx.MBEDTLS_PRIVATE(ctx).MBEDTLS_PRIVATE(mbed_ecdh).MBEDTLS_PRIVATE(grp)),
                &(ecdh->ctx.MBEDTLS_PRIVATE(ctx).MBEDTLS_PRIVATE(mbed_ecdh).MBEDTLS_PRIVATE(d)),
                &(ecdh->ctx.MBEDTLS_PRIVATE(ctx).MBEDTLS_PRIVATE(mbed_ecdh).MBEDTLS_PRIVATE(Q)),
                mbedtls_ctr_drbg_random, &ctr_drbg));
#endif // MBEDTLS_ECDH_LEGACY_CONTEXT

#if 0   /* To remove compile warning */
    if (ret)
        ;
#endif  // 0

done:
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);

    return ecdh;

cleanup:
    crypto_ecdh_deinit(ecdh);
    ecdh = NULL;
    goto done;
}

void crypto_ecdh_deinit(struct crypto_ecdh *ecdh)
{
    if (ecdh) {
        mbedtls_ecdh_free(&ecdh->ctx);
        os_free(ecdh);
    }
}

struct wpabuf * crypto_ecdh_get_pubkey(struct crypto_ecdh *ecdh, int inc_y)
{
    int ret;
    struct wpabuf *buf = NULL;
    size_t len = 0;

    mbedtls_ecp_group *grp = NULL;
    mbedtls_ecp_point *Q = NULL;

#if defined (MBEDTLS_ECDH_LEGACY_CONTEXT)
    grp = &(ecdh->ctx.MBEDTLS_PRIVATE(grp));
    Q = &(ecdh->ctx.MBEDTLS_PRIVATE(Q));
#else
    grp = &(ecdh->ctx.MBEDTLS_PRIVATE(ctx).MBEDTLS_PRIVATE(mbed_ecdh).MBEDTLS_PRIVATE(grp));
    Q = &(ecdh->ctx.MBEDTLS_PRIVATE(ctx).MBEDTLS_PRIVATE(mbed_ecdh).MBEDTLS_PRIVATE(Q));
#endif // MBEDTLS_ECDH_LEGACY_CONTEXT

    if (mbedtls_mpi_cmp_int(&Q->MBEDTLS_PRIVATE(Z), 0) == 0) {
        return NULL;
    }

    len = mbedtls_mpi_size(&grp->P);

    buf = wpabuf_alloc(inc_y ? len * 2 : len);
    if (!buf) {
        return NULL;
    }

    MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary(&Q->MBEDTLS_PRIVATE(X),
                wpabuf_put(buf, len), len));
    if (inc_y) {
        MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary(&Q->MBEDTLS_PRIVATE(Y),
                    wpabuf_put(buf, len), len));
    }

#if 0   /* To remove compile warning */
    if (ret)
        ;
#endif  // 0

done:
    return buf;

cleanup:
    wpabuf_free(buf);
    buf = NULL;
    goto done;
}

struct wpabuf * crypto_ecdh_set_peerkey(struct crypto_ecdh *ecdh, int inc_y,
                const u8 *key, size_t len)
{
    int ret;
    struct wpabuf *pubkey = NULL;
    struct wpabuf *secret = NULL;
#if defined (MBEDTLS_ECDH_LEGACY_CONTEXT)
    size_t key_len = mbedtls_mpi_size(&ecdh->ctx.MBEDTLS_PRIVATE(grp).P);
#else
    size_t key_len = mbedtls_mpi_size(&ecdh->ctx.MBEDTLS_PRIVATE(ctx).MBEDTLS_PRIVATE(mbed_ecdh).MBEDTLS_PRIVATE(grp).P);
#endif // MBEDTLS_ECDH_LEGACY_CONTEXT
    size_t need_key_len = inc_y ? 2 * key_len : key_len;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_mpi temp, *y_sqr = NULL;

    mbedtls_ecp_group *grp = NULL;
    mbedtls_ecp_point *Qp = NULL;

#if defined (MBEDTLS_ECDH_LEGACY_CONTEXT)
    grp = &(ecdh->ctx.MBEDTLS_PRIVATE(grp));
    Qp = &(ecdh->ctx.MBEDTLS_PRIVATE(Qp));
#else
    grp = &(ecdh->ctx.MBEDTLS_PRIVATE(ctx).MBEDTLS_PRIVATE(mbed_ecdh).MBEDTLS_PRIVATE(grp));
    Qp = &(ecdh->ctx.MBEDTLS_PRIVATE(ctx).MBEDTLS_PRIVATE(mbed_ecdh).MBEDTLS_PRIVATE(Qp));
#endif // MBEDTLS_ECDH_LEGACY_CONTEXT

    mbedtls_mpi_init(&temp);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);

    MBEDTLS_MPI_CHK(mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func,
            &entropy, NULL, 0));

    if (len < need_key_len)
        goto cleanup;

    pubkey = wpabuf_alloc(1 + 2 * key_len);
    if (!pubkey)
        goto cleanup;

    if (inc_y) {
        wpabuf_put_u8(pubkey, 0x04);
        wpabuf_put_data(pubkey, key, need_key_len);

        MBEDTLS_MPI_CHK(mbedtls_ecdh_read_public(&ecdh->ctx,
                    wpabuf_head_u8(pubkey), wpabuf_len(pubkey)));
    } else {
        MBEDTLS_MPI_CHK(mbedtls_mpi_read_binary(&Qp->MBEDTLS_PRIVATE(X), key, key_len));
        MBEDTLS_MPI_CHK(mbedtls_mpi_lset(&Qp->MBEDTLS_PRIVATE(Z), 1));

        y_sqr = crypto_mbedtls_compute_y_sqr(grp, &Qp->MBEDTLS_PRIVATE(X));
        if (y_sqr) {
            MBEDTLS_MPI_CHK(mbedtls_mpi_add_int(&temp, &grp->P, 1));
            MBEDTLS_MPI_CHK(mbedtls_mpi_div_int(&temp, NULL, &temp, 4));
            MBEDTLS_MPI_CHK(mbedtls_mpi_exp_mod(&Qp->MBEDTLS_PRIVATE(Y), y_sqr,
                        &temp, &grp->P, NULL));
        } else {
            goto cleanup;
        }
    }

    secret = wpabuf_alloc(key_len);
    if (!secret)
        goto cleanup;

    MBEDTLS_MPI_CHK(mbedtls_ecdh_calc_secret(&ecdh->ctx, &key_len,
                wpabuf_put(secret, key_len), key_len,
                mbedtls_ctr_drbg_random, &ctr_drbg));

#if 0   /* To remove compile warning */
    if (ret)
        ;
#endif  // 0

done:

    if (y_sqr) {
        mbedtls_mpi_free(y_sqr);
        os_free(y_sqr);
    }

    mbedtls_mpi_free(&temp);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    wpabuf_free(pubkey);
    return secret;

cleanup:
    wpabuf_free(secret);
    secret = NULL;
    goto done;
}

static mbedtls_mpi *crypto_mbedtls_compute_y_sqr(mbedtls_ecp_group *grp, const mbedtls_mpi *x)
{
    int ret = 0;
    mbedtls_mpi *y_sqr = os_zalloc(sizeof(mbedtls_mpi));
    if (y_sqr == NULL) {
        return NULL;
    }

    mbedtls_mpi_init(y_sqr);

    // y_sqr = X (X^2 + A) + B = X^3 + A X + B
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(y_sqr, x, x));
    if (grp->MBEDTLS_PRIVATE(modp) == NULL) {   //MUL
        MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(y_sqr, y_sqr, &grp->P));
    } else {
        /* y_sqr->s < 0 is a much faster test, which fails only if N is 0 */
        if ((y_sqr->MBEDTLS_PRIVATE(s) < 0 && mbedtls_mpi_cmp_int(y_sqr, 0) != 0)
            || mbedtls_mpi_bitlen(y_sqr) > 2 * grp->pbits) {
            ret = MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
            goto cleanup;
        }

        MBEDTLS_MPI_CHK(grp->MBEDTLS_PRIVATE(modp(y_sqr)));

        /* y_sqr->s < 0 is a much faster test, which fails only if N is 0 */
        while (y_sqr->MBEDTLS_PRIVATE(s) < 0 && mbedtls_mpi_cmp_int(y_sqr, 0) != 0 ) {
            MBEDTLS_MPI_CHK( mbedtls_mpi_add_mpi(y_sqr, y_sqr, &grp->P));
        }

        while (mbedtls_mpi_cmp_mpi(y_sqr, &grp->P) >= 0) {
            /* we known P, N and the result are positive */
            MBEDTLS_MPI_CHK(mbedtls_mpi_sub_abs(y_sqr, y_sqr, &grp->P));
        }
    }

    // Special case for A = -3
    if (grp->A.MBEDTLS_PRIVATE(p) == NULL) {
        MBEDTLS_MPI_CHK(mbedtls_mpi_sub_int(y_sqr, y_sqr, 3));
        if (y_sqr->MBEDTLS_PRIVATE(s) < 0 && mbedtls_mpi_cmp_int(y_sqr, 0) != 0) {  //MOD_SUB
            MBEDTLS_MPI_CHK(mbedtls_mpi_add_mpi(y_sqr, y_sqr, &grp->P));
        }
    } else {
        MBEDTLS_MPI_CHK(mbedtls_mpi_add_mpi(y_sqr, y_sqr, &grp->A));
        if (mbedtls_mpi_cmp_mpi(y_sqr, &grp->P) >= 0 ) {    //MOD_ADD
            MBEDTLS_MPI_CHK(mbedtls_mpi_sub_abs(y_sqr, y_sqr, &grp->P));
        }
    }

    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(y_sqr, y_sqr, x));
    if (grp->MBEDTLS_PRIVATE(modp) == NULL) {   //MOD_MUL
        MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(y_sqr, y_sqr, &grp->P));
    } else {
        /* y_sqr->s < 0 is a much faster test, which fails only if N is 0 */
        if ((y_sqr->MBEDTLS_PRIVATE(s) < 0 && mbedtls_mpi_cmp_int(y_sqr, 0) != 0)
            || mbedtls_mpi_bitlen(y_sqr) > 2 * grp->pbits) {
            ret = MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
            goto cleanup;
        }

        MBEDTLS_MPI_CHK(grp->MBEDTLS_PRIVATE(modp(y_sqr)));

        /* y_sqr->s < 0 is a much faster test, which fails only if N is 0 */
        while (y_sqr->MBEDTLS_PRIVATE(s) < 0 && mbedtls_mpi_cmp_int(y_sqr, 0) != 0 ) {
            MBEDTLS_MPI_CHK( mbedtls_mpi_add_mpi(y_sqr, y_sqr, &grp->P));
        }

        while (mbedtls_mpi_cmp_mpi(y_sqr, &grp->P) >= 0) {
            /* we known P, N and the result are positive */
            MBEDTLS_MPI_CHK(mbedtls_mpi_sub_abs(y_sqr, y_sqr, &grp->P));
        }
    }

    MBEDTLS_MPI_CHK(mbedtls_mpi_add_mpi(y_sqr, y_sqr, &grp->B));
    if (mbedtls_mpi_cmp_mpi(y_sqr, &grp->P) >= 0) { //MOD_ADD
        MBEDTLS_MPI_CHK(mbedtls_mpi_sub_abs(y_sqr, y_sqr, &grp->P));
    }

cleanup:

    if (ret) {
        mbedtls_mpi_free(y_sqr);
        os_free(y_sqr);
        return NULL;
    } else {
        return y_sqr;
    }
}

#if 0 /* TODO: Not implemented yet. */
/*
 * It's not common function. It's for specified SAE connection.
 */
int crypto_bignum_is_quadratic_residue_blind(const struct crypto_bignum *r,
                    const struct crypto_bignum *p,
                    const struct crypto_bignum *qr,
                    const struct crypto_bignum *qnr,
                    const struct crypto_bignum *y_sqr,
                    int r_odd)
{
    extern int mbedtls_mpi_is_quadratic_residue_blind(const mbedtls_mpi *R,
                    const mbedtls_mpi *P,
                    const mbedtls_mpi *Q,
                    const mbedtls_mpi *Y,
                    mbedtls_mpi *OUT);

    const struct crypto_bignum *q = r_odd ? qr : qnr;
    const int check = r_odd ? 1 : -1;
    int ret = -2;

    ret = mbedtls_mpi_is_quadratic_residue_blind((const mbedtls_mpi *)r,
                    (const mbedtls_mpi *)p,
                    (const mbedtls_mpi *)q,
                    (const mbedtls_mpi *)y_sqr,
                    NULL);
    if (ret == -2) {
        ret = -1;
        goto cleanup;
    }

    ret = ret == check;

cleanup:

    return ret;
}
#endif
