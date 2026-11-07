#ifndef AV_D3D12_OBJECT_HOLDS_HPP
#define AV_D3D12_OBJECT_HOLDS_HPP

#include "executor/d3d/d3d12_block_heap.hpp"
#include "executor/object_holds.hpp"
#include "executor/shim/d3d_headers.hpp"

namespace av {

using D3d12ObjectHolds = ObjectHolds<IUnknown, D3d12BlockHeap>;

}

#endif
