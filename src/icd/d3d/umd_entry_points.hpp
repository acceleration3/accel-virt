/* The adapter-table entry points the three Direct3D runtimes call
 * (contracts/umd_interface.md). Like the DXGI thunks beside them these are the
 * ABI boundary C requires: the runtime calls a bare function pointer with the
 * adapter handle it was given and nothing else, so each is one line into the
 * root behind UmdBinding and no logic lives here.
 *
 * D3D12Core clears all eight of its slots before the open and refuses the
 * driver unless the last six come back non-null (research.md §12), so both
 * tables are written whole. */
#ifndef AV_D3D_UMD_ENTRY_POINTS_HPP
#define AV_D3D_UMD_ENTRY_POINTS_HPP

namespace av::d3d {

struct D3d102ddiAdapterFuncsRecord;
struct D3d12ddiAdapterFuncsRecord;
struct D3dddiAdapterFuncsRecord;

void fill_d3d10_adapter_funcs(D3d102ddiAdapterFuncsRecord &table);
void fill_d3d12_adapter_funcs(D3d12ddiAdapterFuncsRecord &table);
void fill_d3d9_adapter_funcs(D3dddiAdapterFuncsRecord &table);

}  // namespace av::d3d

#endif  // AV_D3D_UMD_ENTRY_POINTS_HPP
