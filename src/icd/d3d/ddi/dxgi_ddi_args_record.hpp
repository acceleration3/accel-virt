/* The DXGI DDI argument structures this feature reads or writes (dxgiddi.h).
 *
 * Every one of them begins with the device handle, which is what the thunks
 * need in order to find the device at all - so `DxgiDdiArgRecord` is the view
 * every slot takes, and the wider records below are views onto the front of
 * the structures whose out fields 055 fills. A view is deliberately shorter
 * than the runtime's structure: what is past the last field named here belongs
 * to the runtime and is never written. DDI records. */
#ifndef AV_D3D_DXGI_DDI_ARGS_RECORD_HPP
#define AV_D3D_DXGI_DDI_ARGS_RECORD_HPP

#include <stdint.h>

namespace av::d3d {

struct DxgiDdiArgRecord {
    void *hDevice;
};

/* DXGI_DDI_ARG_GET_GAMMA_CONTROL_CAPS (dxgiddi.h:104). */
struct DxgiDdiArgGetGammaControlCapsRecord {
    void *hDevice;
    void *pGammaCapabilities;
};

/* The front of DXGI_GAMMA_CONTROL_CAPABILITIES (dxgitype.h): the four fields
 * before its 1025-entry control-point array, which a driver reporting no
 * control points must not touch. */
struct DxgiGammaControlCapabilitiesRecord {
    int32_t ScaleAndOffsetSupported;
    float   MaxConvertedValue;
    float   MinConvertedValue;
    uint32_t NumGammaControlPoints;
};

/* DXGI_DDI_ARG_GETMULTIPLANEOVERLAYCAPS (dxgiddi.h:432). The caps structure is
 * a flags word and four counts; all zero is "no overlay support". */
struct DxgiDdiArgGetMultiplaneOverlayCapsRecord {
    void    *hDevice;
    uint32_t VidPnSourceId;
    uint32_t MaxPlanes;
    uint32_t MaxRGBPlanes;
    uint32_t MaxYUVPlanes;
    float    MaxStretchFactor;
    float    MaxShrinkFactor;
};

/* DXGI_DDI_ARG_GETMULTIPLANEOVERLAYGROUPCAPS (dxgiddi.h:439). */
struct DxgiDdiArgGetMultiplaneOverlayGroupCapsRecord {
    void    *hDevice;
    uint32_t VidPnSourceId;
    uint32_t GroupIndex;
    uint32_t MaxPlanes;
    uint32_t MaxStretchFactor;
    uint32_t MaxShrinkFactor;
    uint32_t OverlayCaps;
};

/* DXGI_DDI_ARG_CHECKPRESENTDURATIONSUPPORT (dxgiddi.h:492): the two out
 * durations are zero when the adapter supports no custom present duration. */
struct DxgiDdiArgCheckPresentDurationSupportRecord {
    void    *hDevice;
    uint32_t VidPnSourceId;
    uint32_t DesiredPresentDuration;
    uint32_t ClosestSmallerDuration;
    uint32_t ClosestLargerDuration;
};

/* The front of DXGI_DDI_ARG_CHECKMULTIPLANEOVERLAYCOLORSPACESUPPORT
 * (dxgiddi.h): the out `Supported` follows the format and the colour space. */
struct DxgiDdiArgCheckMultiplaneOverlayColorSpaceSupportRecord {
    void    *hDevice;
    uint32_t VidPnSourceId;
    uint32_t Format;
    uint32_t ColorSpace;
    int32_t  Supported;
};

}  // namespace av::d3d

#endif  // AV_D3D_DXGI_DDI_ARGS_RECORD_HPP
