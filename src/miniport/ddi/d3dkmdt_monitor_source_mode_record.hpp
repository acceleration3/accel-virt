/* D3DKMDT_MONITOR_SOURCE_MODE. A DDI record: ColorCoeffDynamicRanges is its
 * four channels; src/km/ddi_record_layout.cpp proves each offset. */
#ifndef AV_D3DKMDT_MONITOR_SOURCE_MODE_RECORD_HPP
#define AV_D3DKMDT_MONITOR_SOURCE_MODE_RECORD_HPP

#include "miniport/ddi/d3dkmdt_video_signal_info_record.hpp"

#include <stdint.h>

namespace av {

struct D3dkmdtMonitorSourceModeRecord {
    uint32_t                     Id;
    uint32_t                     IdPadding;
    D3dkmdtVideoSignalInfoRecord VideoSignalInfo;
    int32_t                      ColorBasis;
    uint32_t                     ColorCoeffDynamicRanges[4];
    int32_t                      Origin;
    int32_t                      Preference;
    uint32_t                     Padding;
};

}  // namespace av

#endif  // AV_D3DKMDT_MONITOR_SOURCE_MODE_RECORD_HPP
