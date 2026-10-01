/* D3DDDI_ADAPTERFUNCS (d3dumddi.h:4649), the three slots OpenAdapter writes
 * for d3d9.dll. A DDI record. */
#ifndef AV_D3D_D3DDDI_ADAPTER_FUNCS_RECORD_HPP
#define AV_D3D_D3DDDI_ADAPTER_FUNCS_RECORD_HPP

namespace av::d3d {

struct D3dddiAdapterFuncsRecord {
    void *pfnGetCaps;
    void *pfnCreateDevice;
    void *pfnCloseAdapter;
};

}  // namespace av::d3d

#endif  // AV_D3D_D3DDDI_ADAPTER_FUNCS_RECORD_HPP
