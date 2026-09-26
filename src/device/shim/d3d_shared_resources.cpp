#include "device/shim/d3d_shared_resources.hpp"

#include "core/trace/function_scope.hpp"
#include "device/shim/d3d_headers.hpp"

namespace av {

D3dSharedResources::D3dSharedResources() : keys_{}, resources_{}, count_(0u)
{
    AV_SCOPE("D3dSharedResources::D3dSharedResources");
}

bool D3dSharedResources::publish(uint64_t key, IUnknown *resource)
{
    AV_SCOPE("D3dSharedResources::publish");
    if (key == 0u || resource == nullptr || count_ == capacity) return false;
    for (uint32_t index = 0u; index < count_; index++)
        if (keys_[index] == key) return false;
    resource->AddRef();
    keys_[count_] = key;
    resources_[count_++] = resource;
    return true;
}

IUnknown *D3dSharedResources::find(uint64_t key) const
{
    AV_SCOPE("D3dSharedResources::find");
    if (key == 0u) return nullptr;
    for (uint32_t index = 0u; index < count_; index++) {
        if (keys_[index] != key) continue;
        resources_[index]->AddRef();
        return resources_[index];
    }
    return nullptr;
}

void D3dSharedResources::release(uint64_t key)
{
    AV_SCOPE("D3dSharedResources::release");
    for (uint32_t index = 0u; index < count_; index++) {
        if (keys_[index] != key) continue;
        remove(index);
        return;
    }
}

void D3dSharedResources::remove(uint32_t index)
{
    AV_SCOPE("D3dSharedResources::remove");
    resources_[index]->Release();
    count_--;
    keys_[index] = keys_[count_];
    resources_[index] = resources_[count_];
}

void D3dSharedResources::clear()
{
    AV_SCOPE("D3dSharedResources::clear");
    for (uint32_t index = 0u; index < count_; index++) resources_[index]->Release();
    count_ = 0u;
}

uint32_t D3dSharedResources::count() const
{
    AV_SCOPE("D3dSharedResources::count");
    return count_;
}

}  // namespace av
