/* generated configuration header file - do not edit */
#ifndef RM_VEE_FLASH_W_CFG_H_
#define RM_VEE_FLASH_W_CFG_H_
#ifdef __cplusplus
extern "C" {
#endif

#if  (1 == 1)
#define RM_VEE_USE_CERT 
#define RM_VEE_USE_ENV
#endif

#define RM_VEE_FLASH_W_CFG_PARAM_CHECKING_ENABLE ((BSP_CFG_PARAM_CHECKING_ENABLE))

#define RM_VEE_FLASH_W_CFG_REF_DATA_SUPPORT 1
#define RM_VEE_FLASH_W_REF_DATA_SIZE (1024)

#ifndef RM_VEE_FLASH_W_REFRESH_BUFFER_SIZE
#define RM_VEE_FLASH_W_REFRESH_BUFFER_SIZE        (32)
#endif
#ifndef OS_FREERTOS
#define OS_FREERTOS
#endif

#ifdef __cplusplus
}
#endif
#endif /* RM_VEE_FLASH_W_CFG_H_ */
