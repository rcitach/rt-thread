#include "cc_pal_types.h"
#include "cc_pal_memmap.h"

uint32_t CC_PalMemMap(CCDmaAddr_t physicalAddress,
                      uint32_t mapSize,
                      uint32_t **ppVirtBuffAddr)
{
    CC_UNUSED_PARAM(mapSize);

    if (ppVirtBuffAddr == NULL)
    {
        return 1U;
    }

    *ppVirtBuffAddr = (uint32_t *)physicalAddress;
    return 0U;
}

uint32_t CC_PalMemUnMap(uint32_t *pVirtBuffAddr, uint32_t mapSize)
{
    CC_UNUSED_PARAM(pVirtBuffAddr);
    CC_UNUSED_PARAM(mapSize);
    return 0U;
}
