#ifndef AV_D3D12_LIST_CAST_HPP
#define AV_D3D12_LIST_CAST_HPP

#include "executor/d3d/d3d12_interface_cast.hpp"

namespace av {

template <typename Interface>
using D3d12ListCast = D3d12InterfaceCast<Interface>;

}

#endif
