/* The DXGI slots a Direct3D 11 device presents through (d3d11_dxgi_thunks.cpp). */
#ifndef AV_UM_D3D11_DXGI_THUNKS_HPP
#define AV_UM_D3D11_DXGI_THUNKS_HPP

#include "um/d3d/d3d11_ddi.hpp"

namespace av::d3d11 {

/* Overwrites the presenting slots of a table the root already filled. */
void fill_dxgi_funcs(DXGI1_6_1_DDI_BASE_FUNCTIONS &table);
/* Every slot of the device table (generated/d3d11_device_thunks.cpp). */
void fill_device_funcs(D3DWDDM2_6DDI_DEVICEFUNCS &table);

}  // namespace av::d3d11

#endif  // AV_UM_D3D11_DXGI_THUNKS_HPP
