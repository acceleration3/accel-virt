#ifndef AV_DXGKARG_QUERY_ADAPTER_INFO_RECORD_HPP
#define AV_DXGKARG_QUERY_ADAPTER_INFO_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargQueryAdapterInfoRecord {
    int32_t  Type;
    uint32_t TypePadding;
    void    *pInputData;
    uint32_t InputDataSize;
    uint32_t InputDataSizePadding;
    void    *pOutputData;
    uint32_t OutputDataSize;
    uint32_t Flags;
    void    *hKmdProcessHandle;
};

}

#endif
