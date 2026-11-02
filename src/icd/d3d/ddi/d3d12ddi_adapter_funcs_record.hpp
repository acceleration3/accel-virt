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

}

#endif
