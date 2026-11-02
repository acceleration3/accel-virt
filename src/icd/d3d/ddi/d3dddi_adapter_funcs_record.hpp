#ifndef AV_D3D_D3DDDI_ADAPTER_FUNCS_RECORD_HPP
#define AV_D3D_D3DDDI_ADAPTER_FUNCS_RECORD_HPP

namespace av::d3d {

struct D3dddiAdapterFuncsRecord {
    void *pfnGetCaps;
    void *pfnCreateDevice;
    void *pfnCloseAdapter;
};

}

#endif
