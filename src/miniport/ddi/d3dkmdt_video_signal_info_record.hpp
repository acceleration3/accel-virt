/* D3DKMDT_VIDEO_SIGNAL_INFO, the timing of a target or monitor mode. A DDI
 * record: ScanLineOrdering is the union with AdditionalSignalInfo;
 * src/km/ddi_record_layout.cpp proves each offset. */
#ifndef AV_D3DKMDT_VIDEO_SIGNAL_INFO_RECORD_HPP
#define AV_D3DKMDT_VIDEO_SIGNAL_INFO_RECORD_HPP

#include <stdint.h>

namespace av {

struct D3dkmdtVideoSignalInfoRecord {
    int32_t  VideoStandard;
    uint32_t TotalSizeCx;
    uint32_t TotalSizeCy;
    uint32_t ActiveSizeCx;
    uint32_t ActiveSizeCy;
    uint32_t VSyncFreqNumerator;
    uint32_t VSyncFreqDenominator;
    uint32_t HSyncFreqNumerator;
    uint32_t HSyncFreqDenominator;
    uint32_t HSyncFreqDenominatorPadding;
    uint64_t PixelRate;
    uint32_t ScanLineOrdering;
    uint32_t Padding;
};

}  // namespace av

#endif  // AV_D3DKMDT_VIDEO_SIGNAL_INFO_RECORD_HPP
