/* D3D12DDIARG_OPENADAPTER (d3d12umddi.h:2686), what D3D12Core.dll hands
 * OpenAdapter12. No Interface and no Version: D3D12 versions each table
 * instead. A DDI record. */
#ifndef AV_D3D_D3D12DDIARG_OPEN_ADAPTER_RECORD_HPP
#define AV_D3D_D3D12DDIARG_OPEN_ADAPTER_RECORD_HPP

namespace av::d3d {

struct D3d12ddiargOpenAdapterRecord {
    void       *hRTAdapter;
    void       *hAdapter;
    const void *pAdapterCallbacks;
    void       *pAdapterFuncs;
};

}  // namespace av::d3d

#endif  // AV_D3D_D3D12DDIARG_OPEN_ADAPTER_RECORD_HPP
