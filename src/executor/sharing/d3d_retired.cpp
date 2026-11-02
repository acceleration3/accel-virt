#include "executor/sharing/d3d_retired.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d_headers.hpp"

namespace av {

D3dRetired::Retired::Retired(IUnknown *object) : object_(object)
{
    AV_SCOPE("D3dRetired::Retired::Retired");
}

D3dRetired::D3dRetired(BlockSource &blocks) : retired_(blocks)
{
    AV_SCOPE("D3dRetired::D3dRetired");
}

D3dRetired::~D3dRetired()
{
    AV_SCOPE("D3dRetired::~D3dRetired");
    retired_.destroy_if([](const Retired &retired) {
        retired.object_->Release();
        return true;
    });
}

void D3dRetired::retire(IUnknown *object)
{
    AV_SCOPE("D3dRetired::retire");
    if (object == nullptr) return;
    Retired *retired = nullptr;
    if (retired_.create(retired, object) != Status::ok) object->Release();
}

uint32_t D3dRetired::take(IUnknown **objects, uint32_t capacity)
{
    AV_SCOPE("D3dRetired::take");
    uint32_t taken = 0u;
    retired_.destroy_if([objects, capacity, &taken](const Retired &retired) {
        if (taken == capacity) return false;
        objects[taken++] = retired.object_;
        return true;
    });
    return taken;
}

uint32_t D3dRetired::live() const
{
    AV_SCOPE("D3dRetired::live");
    return retired_.live();
}

}
