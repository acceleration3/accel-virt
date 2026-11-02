#ifndef AV_D3D_UMD_BINDING_HPP
#define AV_D3D_UMD_BINDING_HPP

#include "icd/common/root_binding.hpp"

namespace av::d3d {

class UmdRoot;

using UmdBinding = icd::RootBinding<UmdRoot>;

extern UmdBinding umd_binding;

}

#endif
