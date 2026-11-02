#ifndef AV_D3D_D3DDDI_ADAPTER_CALLBACKS_RECORD_HPP
#define AV_D3D_D3DDDI_ADAPTER_CALLBACKS_RECORD_HPP

namespace av::d3d {

struct D3dddiAdapterCallbacksRecord {
    void *pfnQueryAdapterInfoCb;
    void *pfnGetMultisampleMethodListCb;
    void *pfnQueryAdapterInfoCb2;
};

}

#endif
