/* D3D10_2DDI_ADAPTERFUNCS (d3d10umddi.h:7554), the five slots OpenAdapter10_2
 * writes. A DDI record. */
#ifndef AV_D3D_D3D10_2DDI_ADAPTER_FUNCS_RECORD_HPP
#define AV_D3D_D3D10_2DDI_ADAPTER_FUNCS_RECORD_HPP

namespace av::d3d {

struct D3d102ddiAdapterFuncsRecord {
    void *pfnCalcPrivateDeviceSize;
    void *pfnCreateDevice;
    void *pfnCloseAdapter;
    void *pfnGetSupportedVersions;
    void *pfnGetCaps;
};

}  // namespace av::d3d

#endif  // AV_D3D_D3D10_2DDI_ADAPTER_FUNCS_RECORD_HPP
