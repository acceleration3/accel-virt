/* The Direct3D 9 device table (generated/d3d9_device_thunks.cpp). */
#ifndef AV_UM_D3D9_TABLES_HPP
#define AV_UM_D3D9_TABLES_HPP

#include "um/d3d/d3d9_ddi.hpp"

namespace av::d3d9 {

/* Every slot d3d9.dll calls through, and NULL in each it reads as a feature
 * this driver does not offer. */
void fill_device_funcs9(D3DDDI_DEVICEFUNCS &table);

}  // namespace av::d3d9

#endif  // AV_UM_D3D9_TABLES_HPP
