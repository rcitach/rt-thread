/* ${REA_DISCLAIMER_PLACEHOLDER} */

/*******************************************************************************************************************//**
 * @addtogroup R_CC312_OPENABLE_W
 * @{
 **********************************************************************************************************************/

#ifndef __r_cc312_secureboot_h__
#define __r_cc312_secureboot_h__

//--------------------------------------------------------------------
//	Target System
//--------------------------------------------------------------------
#include "r_cc312_openable_w_cfg.h"
#include "r_cc312_common.h"

#define	TEST_DEBUG

/*******************************************************************************************************************//**
 * @brief Set Debug Mode
 *
 * @param[in]   mode			debug mode
 ***********************************************************************************************************************/
extern void	R_CC312_Debug_SecureBoot(uint32_t mode);


/*******************************************************************************************************************//**
 * @brief Set Debug Mode
 *
 * @retval  mode            current debug mode
 ***********************************************************************************************************************/
extern uint32_t	R_CC312_Debug_SecureBoot_Mode(void);


/*******************************************************************************************************************//**
 * @brief Get Error Mode
 *
 * @retval  code            lastest error mode
 ***********************************************************************************************************************/
extern uint32_t R_CC312_Debug_Get_ErrorCode(void);

/*******************************************************************************************************************//**
 * @brief Run SecureBoot
 *
 * @param[in]   taddress		flash memory offset of the image
 * @param[out]  jaddress        entry point address of the bootable image \n
 *                              if jaddress == NULL, then CPU will branch into the APP automatically.
 * @param[in]   stopproc		callback to safely finish a boot sequence before branching APP
 *  
 * @retval true			function succeeded
 * @retval false		function failed
 ***********************************************************************************************************************/
extern uint32_t	R_CC312_SecureBoot(uint32_t taddress, uint32_t *jaddress, void (*stopproc)(void *));

/*******************************************************************************************************************//**
 * @brief Run SecureDebug for DCU Protection
 *
 * @param[in]   fhandler		SFLASH handler
 * @param[out]  faddress		flash memory offset of the image
 *  
 * @retval true			function succeeded
 * @retval false		function failed
 * 
 * @note    This function runs internally during POR boot. \n
 *          So if you run this function in your app, it won't work.
 ***********************************************************************************************************************/
extern uint32_t	R_CC312_SecureDebug(void *fhandler, uint32_t faddress);

/*******************************************************************************************************************//**
 * @brief Check Soc-ID
 *
 * @retval In Secure-Lcs, SocID is returned
 * @retval otherwise curruent Lcs is returned.
 *
 ***********************************************************************************************************************/
extern uint32_t	R_CC312_SecureSocID(void);

/*******************************************************************************************************************//**
 * @brief Get Soc-ID
 *
 * @param[inout]  pSocID		    empty output buffer for storing the SocID
 *
 ***********************************************************************************************************************/
extern uint32_t R_CC312_SecureSocID_internal(uint8_t *pSocID);

/*******************************************************************************************************************//**
 * @brief Force the Fatal Error for Secure Boot (test only)
 *
 * @param[in]  rcRmaFlag		    RMA flag
 *
 * @retval Zero             function succeeded
 * @retval Non-Zero         function failed
 ***********************************************************************************************************************/
extern uint32_t	R_CC312_SecureBoot_Fatal(uint32_t *rcRmaFlag);

/*******************************************************************************************************************//**
 * @brief Manually run the RMA process to erase the OTP secrets. (test only)
 *
 * @retval true			function succeeded
 * @retval false		function failed
 *
 ***********************************************************************************************************************/
extern uint32_t R_CC312_SecureBoot_RMA(void);

/*******************************************************************************************************************//**
 * @brief Register CMPU to write HUKey & CMKeys in OTP.
 *
 * @param[in]   pCmpuData   data pointer of CMPU
 * @param[in]   rflag		TRNG test feature. Set '0' to avoid the collision of HUKey between products.
 *  
 * @retval true			function succeeded
 * @retval false		function failed
 ***********************************************************************************************************************/
extern uint32_t	R_CC312_SecureBoot_CMPU(uint8_t *pCmpuData, uint32_t rflag);

/*******************************************************************************************************************//**
 * @brief Register DMPU to write DMKeys in OTP.
 *
 * @param[in]   pDmpuData   data pointer of DMPU
 *  
 * @retval true			function succeeded
 * @retval false		function failed
 ***********************************************************************************************************************/
extern uint32_t	R_CC312_SecureBoot_DMPU(uint8_t *pDmpuData);

/*******************************************************************************************************************//**
 * @brief Get the OTP flag used to check whether to run a SecureBoot.
 *
 * @retval true			SecureBoot locked (enabled)
 * @retval false		SecureBoot unlocked (disabled)
 ***********************************************************************************************************************/
extern uint32_t	R_CC312_SecureBoot_GetLock(void);


/*******************************************************************************************************************//**
 * @brief Set the OTP flag to lock the SecureBoot.
 *
 * @retval true			function succeeded
 * @retval false		function failed
 ***********************************************************************************************************************/
extern uint32_t	R_CC312_SecureBoot_SetLock(void);

/*******************************************************************************************************************//**
 * @brief Lock/Unlock the OTP Protection.
 *
 * @param[in]   mode    lock value (1 : unlock for writing data, 0: lock)
 *  
 ***********************************************************************************************************************/
extern void	R_CC312_SecureBoot_OTPLock(uint32_t mode);


/*******************************************************************************************************************//**
 * @brief Check if current state is a Secure-Lcs.
 *
 * @retval true			function succeeded
 * @retval false		function failed
 ***********************************************************************************************************************/
extern uint32_t	R_CC312_SecureBoot_SecureLCS(void);

/*******************************************************************************************************************//**
 * @brief Get the OTP flag used to check whether to run a SecureBoot (optional).
 *
 * @retval true			SecureBoot locked (enabled)
 * @retval false		SecureBoot unlocked (disabled)
 ***********************************************************************************************************************/
extern uint32_t	R_CC312_SecureBoot_GetLock2(void);

/*******************************************************************************************************************//**
 * @brief Set the OTP flag to lock the SecureBoot (optional).
 *
 * @retval true			function succeeded
 * @retval false		function failed
 ***********************************************************************************************************************/
extern uint32_t	R_CC312_SecureBoot_SetLock2(void);

/*******************************************************************************************************************//**
 * @brief  Decrypt SecureAsset.
 *
 * @param[in]       Owner			key type (1: CMKey - Kpicv, 2: DMKey - Kcp)
 * @param[in]       AssetID			unique id (part of nonce)
 * @param[in]       InAssetData		encrypted asset data
 * @param[in]       AssetSize		size of InAssetData
 * @param[out]      OutAssetData	decrypted asset data
 * @param[inout]    OutAssetSize	size of decrypted asset data
 *  
 * @retval Non-Zero		size of OutAssetData
 * @retval Zero 		function failed 
 ***********************************************************************************************************************/
extern uint32_t	R_CC312_Secure_Asset(uint32_t Owner, uint32_t AssetID
                , uint32_t *InAssetData, uint32_t AssetSize
                , uint32_t *OutAssetData, uint32_t *OutAssetSize);

/**
 * \brief KEY Type list
 *
 */
typedef enum  {
    ASSET_USER_KEY = 0,		/**< User Key */
    ASSET_ROOT_KEY = 1,		/**< HUK Key */
    ASSET_KCP_KEY = 2,		/**< DMKey - Kcp, asset(product) encryption key */
    ASSET_KCE_KEY = 3,		/**< DMKey - Kcp, code encryption key  */
    ASSET_KPICV_KEY = 4,	/**< CMKey - Kpicv, asset(product) encryption key */	
    ASSET_KCEICV_KEY = 5,	/**< CMKey - Kceicv, code encryption key  */
} AssetKeyType_t;

/**
 * \brief User Key Format
 *
 */
typedef struct {
    uint8_t	*pKey;			/**< key data */
    size_t	keySize;		/**< key size */
} AssetUserKeyData_t;

/**
 * \brief Encrypted RunTime Asset Info
 *
 */
typedef struct {
        uint32_t  token;		/**< ID for package provisioning */
        uint32_t  version;		/**< version info */
        uint32_t  assetSize;	/**< size of asset */
} AssetInfoData_t;

/**
 * \brief ID code for RunTime Asset
 *
 */
#define CC_RUNASSET_PROV_TOKEN     0x416E7572UL
/**
 * \brief Version code for RunTime Asset
 *
 */
#define CC_RUNASSET_PROV_VERSION   0x10000UL

/*******************************************************************************************************************//**
 * @brief  Encrypt RuntimeAsset.
 *
 * @param[in]       KeyType			key type (\sa AssetKeyType_t)
 * @param[in]       noncetype		method to generate a nonce (0xFFFFFFFF: PRNG, otherwise: TRNG)
 * @param[in]       KeyData			User Key data
 * @param[in]       AssetID			unique id of RuntimeAsset
 * @param[in]       title			unique tag of RuntimeAsset
 * @param[in]       InAssetData		plaintext of RuntimeAsset
 * @param[in]       AssetSize		size of the plaintext. it must be multiply of 16 bytes.
 * @param[inout]    OutAssetPkgData empty output buffer for storing the encrypted RuntimeAsset
 *  
 * @retval Non-Zero		size of OutAssetData
 * @retval Zero 		function failed 
 ***********************************************************************************************************************/
extern int32_t R_CC312_Secure_Asset_RuntimePack(AssetKeyType_t KeyType, uint32_t noncetype
		, AssetUserKeyData_t *KeyData, uint32_t AssetID, char *title
		, uint8_t *InAssetData, uint32_t AssetSize, uint8_t *OutAssetPkgData);

/*******************************************************************************************************************//**
 * @brief  Decrypt RuntimeAsset.
 *
 * @param[in]       KeyType			key type (\sa AssetKeyType_t)
 * @param[in]       KeyData			User Key data
 * @param[in]       AssetID			unique id of RuntimeAsset
 * @param[in]       InAssetPkgData	encrypted RuntimeAsset
 * @param[in]       AssetPkgSize	size of the encrypted RuntimeAsset
 * @param[inout]    OutAssetData	empty output buffer for storing the decrypted RuntimeAsset
 *  
 * @retval Non-Zero		size of OutAssetData
 * @retval Zero 		function failed 
 ***********************************************************************************************************************/
extern int32_t R_CC312_Secure_Asset_RuntimeUnpack(AssetKeyType_t KeyType
		, AssetUserKeyData_t *KeyData, uint32_t AssetID
		, uint8_t *InAssetPkgData, uint32_t AssetPkgSize, uint8_t *OutAssetData);

#endif /* __r_cc312_secureboot_h__ */
/*******************************************************************************************************************//**
 * @} (end addtogroup R_CC312_OPENABLE_W)
 **********************************************************************************************************************/