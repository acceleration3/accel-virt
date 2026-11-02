#ifndef AV_DXGKARG_RECOMMEND_FUNCTIONAL_VIDPN_RECORD_HPP
#define AV_DXGKARG_RECOMMEND_FUNCTIONAL_VIDPN_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargRecommendFunctionalVidPnRecord {
    uint32_t       NumberOfVidPnTargets;
    uint32_t       NumberOfVidPnTargetsPadding;
    const uint32_t *pVidPnTargetPrioritizationVector;
    void           *hRecommendedFunctionalVidPn;
    int32_t        RequestReason;
    uint32_t       RequestReasonPadding;
    void           *pPrivateDriverData;
    uint32_t       PrivateDriverDataSize;
    uint32_t       Padding;
};

}

#endif
