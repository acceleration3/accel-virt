/* DXGK_DISPLAY_INFORMATION, the frame buffer DxgkDdiStopDeviceAndReleasePostDisplayOwnership
 * hands to the basic display driver. A DDI record; src/km/ddi_record_layout.cpp
 * proves each offset. */
#ifndef AV_DXGK_DISPLAY_INFORMATION_RECORD_HPP
#define AV_DXGK_DISPLAY_INFORMATION_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkDisplayInformationRecord {
    uint32_t Width;
    uint32_t Height;
    uint32_t Pitch;
    int32_t  ColorFormat;
    int64_t  PhysicAddress;
    uint32_t TargetId;
    uint32_t AcpiId;
};

}  // namespace av

#endif  // AV_DXGK_DISPLAY_INFORMATION_RECORD_HPP
