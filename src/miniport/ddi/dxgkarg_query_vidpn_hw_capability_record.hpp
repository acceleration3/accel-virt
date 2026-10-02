/* DXGKARG_QUERYVIDPNHWCAPABILITY, as DxgkDdiQueryVidPnHWCapability receives
 * and answers it. A DDI record: VidPnHWCaps is the D3DKMDT_VIDPN_HW_CAPABILITY
 * bitfield's UINT; src/km/ddi_record_layout.cpp proves each offset. */
#ifndef AV_DXGKARG_QUERY_VIDPN_HW_CAPABILITY_RECORD_HPP
#define AV_DXGKARG_QUERY_VIDPN_HW_CAPABILITY_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargQueryVidPnHwCapabilityRecord {
    void    *hFunctionalVidPn;
    uint32_t SourceId;
    uint32_t TargetId;
    uint32_t VidPnHWCaps;
    uint32_t VidPnHWCapsPadding;
};

}  // namespace av

#endif  // AV_DXGKARG_QUERY_VIDPN_HW_CAPABILITY_RECORD_HPP
