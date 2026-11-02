#ifndef AV_UM_D3D11_DXGI_THUNKS_HPP
#define AV_UM_D3D11_DXGI_THUNKS_HPP

#include "um/d3d/d3d11_ddi.hpp"

namespace av::d3d11 {
void fill_dxgi_funcs(DXGI1_6_1_DDI_BASE_FUNCTIONS &table);
void fill_device_funcs(D3DWDDM2_6DDI_DEVICEFUNCS &table);

}

#endif
