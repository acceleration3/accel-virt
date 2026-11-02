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

}

#endif
