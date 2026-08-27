/*
 * Copyright (c) 2001-2019, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR Arm’s non-OSI source license
 */

#ifndef MBEDTLS_DHM_ALT_H
 #define MBEDTLS_DHM_ALT_H

 #include "pka.h"
 #include "pki.h"

 #ifdef __cplusplus
extern "C" {
 #endif

/**
 * \brief          The DHM context structure.
 */
typedef struct mbedtls_dhm_context
{
    mbedtls_mpi MBEDTLS_PRIVATE(P);    /*!<  The prime modulus. */
    mbedtls_mpi MBEDTLS_PRIVATE(G);    /*!<  The generator. */
    mbedtls_mpi MBEDTLS_PRIVATE(X);    /*!<  Our secret value. */
    mbedtls_mpi MBEDTLS_PRIVATE(GX);   /*!<  Our public key = \c G^X mod \c P. */
    mbedtls_mpi MBEDTLS_PRIVATE(GY);   /*!<  The public key of the peer = \c G^Y mod \c P. */
    mbedtls_mpi MBEDTLS_PRIVATE(K);    /*!<  The shared secret = \c G^(XY) mod \c P. */
    mbedtls_mpi MBEDTLS_PRIVATE(RP);   /*!<  The cached value = \c R^2 mod \c P. */
    mbedtls_mpi MBEDTLS_PRIVATE(Vi);   /*!<  The blinding value. */
    mbedtls_mpi MBEDTLS_PRIVATE(Vf);   /*!<  The unblinding value. */
    mbedtls_mpi MBEDTLS_PRIVATE(pX);   /*!<  The previous \c X. */
} mbedtls_dhm_context;

 #ifdef __cplusplus
}
 #endif

#endif                                 /* MBEDTLS_DHM_ALT_H  - include only once  */
