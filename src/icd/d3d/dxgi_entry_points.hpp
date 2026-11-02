#ifndef AV_D3D_DXGI_ENTRY_POINTS_HPP
#define AV_D3D_DXGI_ENTRY_POINTS_HPP

#include "icd/d3d/hresult.hpp"

namespace av::d3d {

struct Dxgi161DdiBaseFunctionsRecord;

void fill_dxgi_base_functions(Dxgi161DdiBaseFunctionsRecord &table);

}

#endif
