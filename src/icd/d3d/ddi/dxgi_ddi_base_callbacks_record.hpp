/* DXGI_DDI_BASE_CALLBACKS (dxgiddi.h:989) at WDDM 3.2. The driver keeps the
 * pointer to this table and never a copy of a row: the runtime may move any of
 * them whenever no thread is inside the driver (Supporting the DXGI DDI).
 * A DDI record. */
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

}  // namespace av::d3d

#endif  // AV_D3D_DXGI_DDI_BASE_CALLBACKS_RECORD_HPP
