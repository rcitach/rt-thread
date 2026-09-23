/* generated configuration header file - do not edit */
#ifndef RM_PSA_CRYPTO_W_HWCFG_H_
#define RM_PSA_CRYPTO_W_HWCFG_H_
#ifdef __cplusplus
extern "C" {
#endif

#ifndef FSP_NOT_DEFINED
#define FSP_NOT_DEFINED      (0)
#endif

#define RM_PSA_CRYPTO_HW_SCE           FSP_NOT_DEFINED
#define RM_PSA_CRYPTO_HW_CC312         1

#if (RM_PSA_CRYPTO_HW_SCE != FSP_NOT_DEFINED)
#define RM_PSA_CRYPTO_HW_ENGINE        (1)
#elif (RM_PSA_CRYPTO_HW_CC312  != FSP_NOT_DEFINED)
#define RM_PSA_CRYPTO_HW_ENGINE        (2)
#else
# error "HW Crypto Engine is not defined"
#endif
#undef FSP_NOT_DEFINED

#if (RM_PSA_CRYPTO_HW_ENGINE == 2)
#define CC_IOT
#define CC_SB_SUPPORT_IOT
#define CC_SUPPORT_PKA_64_16
#undef  _INTERNAL_CC_NO_RSA_KG_SUPPORT
#define CC_DOUBLE_BUFFER_MAX_SIZE_IN_BYTES  (8192)
#define CC_MNG_MIN_BACKUP_SIZE_IN_BYTES     (512)
#define CC_SB_CERT_VERSION_MAJOR        (1)
#define CC_SB_CERT_VERSION_MINOR        (0)
#define CC_CONFIG_TRNG_MODE             (1)
#define CC_SB_INDIRECT_SRAM_ACCESS
#define CC_HW_VERSION                   (0xFF)
#define HASH_SHA_512_SUPPORTED
#define USE_MBEDTLS_CRYPTOCELL
#define ENABLE_AES_DRIVER               (1)
#define DLLI_MAX_BUFF_SIZE              (0x10000)
#endif

#ifdef __cplusplus
}
#endif
#endif /* RM_PSA_CRYPTO_W_HWCFG_H_ */
