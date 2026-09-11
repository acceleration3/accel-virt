/* DXGKARG_RECOMMENDVIDPNTOPOLOGY, as DxgkDdiRecommendVidPnTopology receives
 * it. A DDI record; src/km/ddi_record_layout.cpp proves each offset. */
#ifndef AV_DXGKARG_RECOMMEND_VIDPN_TOPOLOGY_RECORD_HPP
#define AV_DXGKARG_RECOMMEND_VIDPN_TOPOLOGY_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargRecommendVidPnTopologyRecord {
    void     *hVidPn;
    uint32_t VidPnSourceId;
    int32_t  RequestReason;
    void     *hFallbackTopology;
};

}  // namespace av

#endif  // AV_DXGKARG_RECOMMEND_VIDPN_TOPOLOGY_RECORD_HPP
