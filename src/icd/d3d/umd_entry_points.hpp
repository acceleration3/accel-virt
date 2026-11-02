#ifndef AV_D3D_UMD_ENTRY_POINTS_HPP
#define AV_D3D_UMD_ENTRY_POINTS_HPP

namespace av::d3d {

struct D3d102ddiAdapterFuncsRecord;
struct D3d12ddiAdapterFuncsRecord;
struct D3dddiAdapterFuncsRecord;

void fill_d3d10_adapter_funcs(D3d102ddiAdapterFuncsRecord &table);
void fill_d3d12_adapter_funcs(D3d12ddiAdapterFuncsRecord &table);
void fill_d3d9_adapter_funcs(D3dddiAdapterFuncsRecord &table);

}

#endif
