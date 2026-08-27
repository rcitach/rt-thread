/*
 * Copyright (c) 2001-2019, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR Arm’s non-OSI source license
 */

#ifndef ECC_ALT_INTERNAL_H
#define ECC_ALT_INTERNAL_H

#include "common.h"
#include "mbedtls/ecp.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/ecdh.h"
#include "cc_pal_types_plat.h"
#include "cc_ecpki_types.h"

#if defined(MBEDTLS_ECDH_GEN_PUBLIC_ALT) || defined(MBEDTLS_ECDSA_GENKEY_ALT)

/*
 * Generate a keypair with configurable base point
 */
int cc_ecp_gen_keypair_base(mbedtls_ecp_group * grp,
                            const mbedtls_ecp_point * G,
                            mbedtls_mpi * d,
                            mbedtls_ecp_point * Q,
                            int (* f_rng)(void *, unsigned char *, size_t),
                            void * p_rng);

/*
 * Generate key pair, wrapper for conventional base point
 */
int cc_ecp_gen_keypair(mbedtls_ecp_group * grp,
                       mbedtls_mpi * d,
                       mbedtls_ecp_point * Q,
                       int (* f_rng)(void *, unsigned char *, size_t),
                       void * p_rng);

#endif                                 /* MBEDTLS_ECDH_GEN_PUBLIC_ALT || MBEDTLS_ECDSA_GENKEY_ALT*/

#if defined(MBEDTLS_ECDH_COMPUTE_SHARED_ALT)

/*
 * Multiplication R = m * P
 */
int cc_ecp_mul(mbedtls_ecp_group * grp,
               mbedtls_ecp_point * R,
               const mbedtls_mpi * m,
               const mbedtls_ecp_point * P,
               int (* f_rng)(void *, unsigned char *, size_t),
               void * p_rng);

#endif                                 /* MBEDTLS_ECDH_COMPUTE_SHARED_ALT */

/*
 *\brief         Curve types
 *
 */
typedef enum ecp_curve_type
{
    ECP_TYPE_NONE = 0,
    ECP_TYPE_SHORT_WEIERSTRASS,        /* y^2 = x^3 + a x + b      */
    ECP_TYPE_25519,                    /* MONTGOMERY : y^2 = x^3 + a x^2 + x  EDWARDS: x^2 + y^2 = 1 + dx^2y^2 (modp) */
} ecp_curve_type;

/**
 * \brief           mapping CC ECP return codes to mbedtls
 *
 */
int error_mapping_cc_to_mbedtls_ecc(CCError_t cc_error);

/**
 * \brief           get the cfurve type
 *
 */
static inline ecp_curve_type ecp_get_type (const mbedtls_ecp_group * grp)
{
    if (grp->G.MBEDTLS_PRIVATE(X).MBEDTLS_PRIVATE(p) == NULL)
    {
        return ECP_TYPE_NONE;
    }

    if (grp->G.MBEDTLS_PRIVATE(Y).MBEDTLS_PRIVATE(p) == NULL)
    {
        return ECP_TYPE_25519;
    }
    else
    {
        return ECP_TYPE_SHORT_WEIERSTRASS;
    }
}

/**
 * \brief           map mbedtls group id to CC domain id
 *
 */

int ecp_grp_id_to_domain_id(const mbedtls_ecp_group_id id, CCEcpkiDomainID_t * domain_id);

#endif                                 /* ECC_ALT_INTERNAL_H */
