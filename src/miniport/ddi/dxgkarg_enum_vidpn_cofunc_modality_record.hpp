/* DXGKARG_ENUMVIDPNCOFUNCMODALITY, as DxgkDdiEnumVidPnCofuncModality receives
 * it. EnumPivot is its source and target ids. A DDI record;
 * src/km/ddi_record_layout.cpp proves each offset. */
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

}  // namespace av

#endif  // AV_DXGKARG_ENUM_VIDPN_COFUNC_MODALITY_RECORD_HPP
