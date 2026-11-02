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

}

#endif
