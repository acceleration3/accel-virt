/* DXGKARG_QUERYADAPTERINFO, as DxgkDdiQueryAdapterInfo receives it. A DDI
 * record; src/km/ddi_record_layout.cpp proves each offset. */
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

}  // namespace av

#endif  // AV_DXGKARG_QUERY_ADAPTER_INFO_RECORD_HPP
