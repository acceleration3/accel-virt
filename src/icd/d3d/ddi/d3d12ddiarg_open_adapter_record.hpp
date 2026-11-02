#ifndef AV_D3D_D3D12DDIARG_OPEN_ADAPTER_RECORD_HPP
#define AV_D3D_D3D12DDIARG_OPEN_ADAPTER_RECORD_HPP

namespace av::d3d {

struct D3d12ddiargOpenAdapterRecord {
    void       *hRTAdapter;
    void       *hAdapter;
    const void *pAdapterCallbacks;
    void       *pAdapterFuncs;
};

}

#endif
