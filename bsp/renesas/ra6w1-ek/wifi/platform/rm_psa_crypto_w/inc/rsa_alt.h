/*
 * Copyright (c) 2001-2019, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR Arm’s non-OSI source license
 */

#ifndef MBEDTLS_RSA_ALT_H
 #define MBEDTLS_RSA_ALT_H
 #include "mbedtls/private_access.h"
 #include "mbedtls/build_info.h"
 #include "rsa_public.h"
 #include "rsa_private.h"
 #include "pki.h"
 #include "pki/rsa/rsa.h"
 #include "mbedtls/ctr_drbg.h"
 #include "pka.h"

 #ifdef __cplusplus
extern "C" {
 #endif

/**
 * \brief   The RSA context structure.
 *
 * \note    Direct manipulation of the members of this structure
 *          is deprecated. All manipulation should instead be done through
 *          the public interface functions.
 */
typedef struct mbedtls_rsa_context
{
    int    MBEDTLS_PRIVATE(ver);       /*!<  Reserved for internal purposes.
                                        *    Do not set this field in application
                                        *    code. Its meaning might change without
                                        *    notice. */
    size_t MBEDTLS_PRIVATE(len);       /*!<  The size of \p N in Bytes. */

    mbedtls_mpi MBEDTLS_PRIVATE(N);    /*!<  The public modulus. */
    mbedtls_mpi MBEDTLS_PRIVATE(E);    /*!<  The public exponent. */

    mbedtls_mpi MBEDTLS_PRIVATE(D);    /*!<  The private exponent. */
    mbedtls_mpi MBEDTLS_PRIVATE(P);    /*!<  The first prime factor. */
    mbedtls_mpi MBEDTLS_PRIVATE(Q);    /*!<  The second prime factor. */

    mbedtls_mpi MBEDTLS_PRIVATE(DP);   /*!<  <code>D % (P - 1)</code>. */
    mbedtls_mpi MBEDTLS_PRIVATE(DQ);   /*!<  <code>D % (Q - 1)</code>. */
    mbedtls_mpi MBEDTLS_PRIVATE(QP);   /*!<  <code>1 / (Q % P)</code>. */

    mbedtls_mpi MBEDTLS_PRIVATE(RN);   /*!<  cached <code>R^2 mod N</code>. */

    mbedtls_mpi MBEDTLS_PRIVATE(RP);   /*!<  cached <code>R^2 mod P</code>. */
    mbedtls_mpi MBEDTLS_PRIVATE(RQ);   /*!<  cached <code>R^2 mod Q</code>. */

    mbedtls_mpi MBEDTLS_PRIVATE(Vi);   /*!<  The cached blinding value. */
    mbedtls_mpi MBEDTLS_PRIVATE(Vf);   /*!<  The cached un-blinding value. */

    mbedtls_mpi MBEDTLS_PRIVATE(NP);   /*!< Barrett mod N tag NP for N-modulus */
    mbedtls_mpi MBEDTLS_PRIVATE(BQP);  /*!< Barrett mod Q tag QP for Q-factor  */
    mbedtls_mpi MBEDTLS_PRIVATE(BPP);  /*!< Barrett mod P tag PP for P-factor  */

    int MBEDTLS_PRIVATE(padding);      /*!< Selects padding mode:
                                        * MBEDTLS_RSA_PKCS_V15 for 1.5 padding and
                                        * MBEDTLS_RSA_PKCS_V21 for OAEP or PSS. */
    int MBEDTLS_PRIVATE(hash_id);      /*!< Hash identifier of mbedtls_md_type_t type,
                                        * as specified in md.h for use in the MGF
                                        * mask generating function used in the
                                        * EME-OAEP and EMSA-PSS encodings. */

    void * vendor_ctx;                 /*!< Vendor defined context. */

 #if defined(MBEDTLS_THREADING_C)

    /* Invariant: the mutex is initialized iff ver != 0. */
    mbedtls_threading_mutex_t MBEDTLS_PRIVATE(mutex); /*!<  Thread-safety mutex. */
 #endif
} mbedtls_rsa_context;

 #ifdef __cplusplus
}
 #endif

#endif                                 /*  MBEDTLS_RSA_ALT_H  */
