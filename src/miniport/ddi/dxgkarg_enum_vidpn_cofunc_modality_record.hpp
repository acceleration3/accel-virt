#ifndef AV_DXGKARG_ENUM_VIDPN_COFUNC_MODALITY_RECORD_HPP
#define AV_DXGKARG_ENUM_VIDPN_COFUNC_MODALITY_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargEnumVidPnCofuncModalityRecord {
    void     *hConstrainingVidPn;
    int32_t  EnumPivotType;
    uint32_t EnumPivotVidPnSourceId;
    uint32_t EnumPivotVidPnTargetId;
    uint32_t Padding;
};

}

#endif
