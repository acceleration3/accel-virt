#ifndef AV_D3D_DXGI1_6_1_DDI_BASE_FUNCTIONS_RECORD_HPP
#define AV_D3D_DXGI1_6_1_DDI_BASE_FUNCTIONS_RECORD_HPP

namespace av::d3d {

struct Dxgi161DdiBaseFunctionsRecord {
    void *pfnPresent;
    void *pfnGetGammaCaps;
    void *pfnSetDisplayMode;
    void *pfnSetResourcePriority;
    void *pfnQueryResourceResidency;
    void *pfnRotateResourceIdentities;
    void *pfnBlt;
    void *pfnResolveSharedResource;
    void *pfnBlt1;
    void *pfnOfferResources1;
    void *pfnReclaimResources;
    void *pfnGetMultiplaneOverlayCaps;
    void *pfnGetMultiplaneOverlayGroupCaps;
    void *pfnReserved1;
    void *pfnPresentMultiplaneOverlay;
    void *pfnReserved2;
    void *pfnPresent1;
    void *pfnCheckPresentDurationSupport;
    void *pfnTrimResidencySet;
    void *pfnCheckMultiplaneOverlayColorSpaceSupport;
    void *pfnPresentMultiplaneOverlay1;
    void *pfnReclaimResources1;
};

}

#endif
