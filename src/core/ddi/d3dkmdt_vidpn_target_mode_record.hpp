/* D3DKMDT_VIDPN_TARGET_MODE. A DDI record: WireFormatAndPreference is the
 * union whose low two bits are the D3DKMDT_MODE_PREFERENCE;
 * src/km/ddi_record_layout.cpp proves each offset. */
#ifndef AV_D3DKMDT_VIDPN_TARGET_MODE_RECORD_HPP
#define AV_D3DKMDT_VIDPN_TARGET_MODE_RECORD_HPP

#include "d3dkmdt_video_signal_info_record.hpp"

#include <stdint.h>

namespace av {

struct D3dkmdtVidPnTargetModeRecord {
    uint32_t                    Id;
    uint32_t                    IdPadding;
    D3dkmdtVideoSignalInfoRecord VideoSignalInfo;
    uint32_t                    WireFormatAndPreference;
    uint32_t                    MinimumVSyncFreqNumerator;
    uint32_t                    MinimumVSyncFreqDenominator;
    uint32_t                    Padding;
};

}  // namespace av

#endif  // AV_D3DKMDT_VIDPN_TARGET_MODE_RECORD_HPP
