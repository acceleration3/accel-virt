/* The pointer the Direct3D user-mode driver's DDI thunks reach its root
 * through (RootBinding), defined beside them in umd_entry_points.cpp. */
#ifndef AV_D3D_UMD_BINDING_HPP
#define AV_D3D_UMD_BINDING_HPP

#include "icd/common/root_binding.hpp"

namespace av::d3d {

class UmdRoot;

using UmdBinding = icd::RootBinding<UmdRoot>;

extern UmdBinding umd_binding;

}  // namespace av::d3d

#endif  // AV_D3D_UMD_BINDING_HPP
