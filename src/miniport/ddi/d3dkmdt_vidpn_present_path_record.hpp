/* D3DKMDT_VIDPN_PRESENT_PATH. A DDI record: nested structures are prefixed with
 * their names, the support bitfields are their UINTs, and the gamma ramp's
 * data union is its pointer; src/km/ddi_record_layout.cpp proves each offset. */
#ifndef AV_D3DKMDT_VIDPN_PRESENT_PATH_RECORD_HPP
#define AV_D3DKMDT_VIDPN_PRESENT_PATH_RECORD_HPP

#include <stdint.h>

namespace av {

struct D3dkmdtVidPnPresentPathRecord {
    uint32_t VidPnSourceId;
    uint32_t VidPnTargetId;
    int32_t  ImportanceOrdinal;
    int32_t  ContentTransformationScaling;
    uint32_t ContentTransformationScalingSupport;
    int32_t  ContentTransformationRotation;
    uint32_t ContentTransformationRotationSupport;
    int32_t  VisibleFromActiveTLOffsetCx;
    int32_t  VisibleFromActiveTLOffsetCy;
    int32_t  VisibleFromActiveBROffsetCx;
    int32_t  VisibleFromActiveBROffsetCy;
    int32_t  VidPnTargetColorBasis;
    uint32_t VidPnTargetColorCoeffDynamicRanges[4];
    int32_t  Content;
    int32_t  CopyProtectionType;
    uint32_t CopyProtectionAPSTriggerBits;
    uint8_t  CopyProtectionOEMCopyProtection[256];
    uint32_t CopyProtectionSupport;
    int32_t  GammaRampType;
    uint32_t GammaRampTypePadding;
    uint64_t GammaRampDataSize;
    void    *GammaRampData;
};

}  // namespace av

#endif  // AV_D3DKMDT_VIDPN_PRESENT_PATH_RECORD_HPP
