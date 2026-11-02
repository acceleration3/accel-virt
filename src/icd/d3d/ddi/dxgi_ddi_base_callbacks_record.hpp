#ifndef AV_D3D_DXGI_DDI_BASE_CALLBACKS_RECORD_HPP
#define AV_D3D_DXGI_DDI_BASE_CALLBACKS_RECORD_HPP

namespace av::d3d {

struct DxgiDdiBaseCallbacksRecord {
    void *pfnPresentCb;
    void *pfnPresentMultiplaneOverlayCb;
    void *pfnPresentMultiplaneOverlay1Cb;
    void *pfnSubmitPresentBltToHwQueueCb;
    void *pfnSubmitPresentToHwQueueCb;
};

}

#endif
