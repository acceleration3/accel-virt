/* DXGKARG_CREATECONTEXT, as DxgkDdiCreateContext receives and answers it. A
 * DDI record: Flags is the DXGK_CREATECONTEXTFLAGS Value and ContextInfo's
 * fields are prefixed with its name. src/km/ddi_record_layout.cpp proves each
 * offset. */
#ifndef AV_DXGKARG_CREATE_CONTEXT_RECORD_HPP
#define AV_DXGKARG_CREATE_CONTEXT_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargCreateContextRecord {
    void     *hContext;
    uint32_t NodeOrdinal;
    uint32_t EngineAffinity;
    uint32_t Flags;
    uint32_t FlagsPadding;
    void     *pPrivateDriverData;
    uint32_t PrivateDriverDataSize;
    uint32_t ContextInfoDmaBufferSize;
    uint32_t ContextInfoDmaBufferSegmentSet;
    uint32_t ContextInfoDmaBufferPrivateDataSize;
    uint32_t ContextInfoAllocationListSize;
    uint32_t ContextInfoPatchLocationListSize;
    uint32_t ContextInfoReserved;
    uint32_t ContextInfoCaps;
    uint32_t ContextInfoPagingCompanionNodeId;
    uint32_t Padding;
};

}  // namespace av

#endif  // AV_DXGKARG_CREATE_CONTEXT_RECORD_HPP
