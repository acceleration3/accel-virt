/* DXGKARG_RECOMMENDFUNCTIONALVIDPN, as DxgkDdiRecommendFunctionalVidPn
 * receives it. A DDI record; src/km/ddi_record_layout.cpp proves each offset.
 * */
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

}  // namespace av

#endif  // AV_DXGKARG_RECOMMEND_FUNCTIONAL_VIDPN_RECORD_HPP
