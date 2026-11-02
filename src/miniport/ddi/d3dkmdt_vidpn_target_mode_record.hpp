#ifndef AV_D3DKMDT_VIDPN_TARGET_MODE_RECORD_HPP
#define AV_D3DKMDT_VIDPN_TARGET_MODE_RECORD_HPP

#include "miniport/ddi/d3dkmdt_video_signal_info_record.hpp"

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

}

#endif
