/* D3D12DDI_ADAPTERFUNCS (d3d12umddi.h:2674), the eight slots OpenAdapter12
 * writes. D3D12Core zeroes all eight before the call and refuses the driver
 * unless slots two through seven are non-null afterwards (research.md §12), so
 * every one is written. A DDI record. */
#ifndef AV_D3D_D3D12DDI_ADAPTER_FUNCS_RECORD_HPP
#define AV_D3D_D3D12DDI_ADAPTER_FUNCS_RECORD_HPP

namespace av::d3d {

struct D3d12ddiAdapterFuncsRecord {
    void *pfnCalcPrivateDeviceSize;
    void *pfnCreateDevice;
    void *pfnCloseAdapter;
    void *pfnGetSupportedVersions;
    void *pfnGetCaps;
    void *pfnGetOptionalDDITables;
    void *pfnFillDDITable;
    void *pfnDestroyDevice;
};

}  // namespace av::d3d

#endif  // AV_D3D_D3D12DDI_ADAPTER_FUNCS_RECORD_HPP
