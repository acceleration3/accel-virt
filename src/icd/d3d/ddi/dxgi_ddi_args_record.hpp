#ifndef AV_D3D_DXGI_DDI_ARGS_RECORD_HPP
#define AV_D3D_DXGI_DDI_ARGS_RECORD_HPP

#include <stdint.h>

namespace av::d3d {

struct DxgiDdiArgRecord {
    void *hDevice;
};

struct DxgiDdiArgGetGammaControlCapsRecord {
    void *hDevice;
    void *pGammaCapabilities;
};

struct DxgiGammaControlCapabilitiesRecord {
    int32_t ScaleAndOffsetSupported;
    float   MaxConvertedValue;
    float   MinConvertedValue;
    uint32_t NumGammaControlPoints;
};

struct DxgiDdiArgGetMultiplaneOverlayCapsRecord {
    void    *hDevice;
    uint32_t VidPnSourceId;
    uint32_t MaxPlanes;
    uint32_t MaxRGBPlanes;
    uint32_t MaxYUVPlanes;
    float    MaxStretchFactor;
    float    MaxShrinkFactor;
};

struct DxgiDdiArgGetMultiplaneOverlayGroupCapsRecord {
    void    *hDevice;
    uint32_t VidPnSourceId;
    uint32_t GroupIndex;
    uint32_t MaxPlanes;
    uint32_t MaxStretchFactor;
    uint32_t MaxShrinkFactor;
    uint32_t OverlayCaps;
};

struct DxgiDdiArgCheckPresentDurationSupportRecord {
    void    *hDevice;
    uint32_t VidPnSourceId;
    uint32_t DesiredPresentDuration;
    uint32_t ClosestSmallerDuration;
    uint32_t ClosestLargerDuration;
};

struct DxgiDdiArgCheckMultiplaneOverlayColorSpaceSupportRecord {
    void    *hDevice;
    uint32_t VidPnSourceId;
    uint32_t Format;
    uint32_t ColorSpace;
    int32_t  Supported;
};

struct DxgiDdiArgReclaimResourcesRecord {
    void       *hDevice;
    const void *pResources;
    int32_t    *pDiscarded;
    uint32_t    Resources;
};

struct DxgiDdiArgReclaimResources1Record {
    void       *hDevice;
    const void *pResources;
    uint32_t   *pResults;
    uint32_t    Resources;
};

}

#endif
