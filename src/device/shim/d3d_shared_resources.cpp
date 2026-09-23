#include "device/shim/d3d_shared_resources.hpp"

#include "core/trace/function_scope.hpp"
#include "device/shim/d3d_headers.hpp"

namespace av {

D3dSharedResources::D3dSharedResources() : entries_{}, count_(0u)
{
    AV_SCOPE("D3dSharedResources::D3dSharedResources");
}

bool D3dSharedResources::publish(uint64_t key, IUnknown *resource)
{
    AV_SCOPE("D3dSharedResources::publish");
    if (key == 0u || resource == nullptr || count_ == capacity) return false;
    for (uint32_t index = 0u; index < count_; index++)
        if (entries_[index].key == key) return false;
    resource->AddRef();
    entries_[count_++] = Entry{key, resource};
    return true;
}

IUnknown *D3dSharedResources::find(uint64_t key) const
{
    AV_SCOPE("D3dSharedResources::find");
    if (key == 0u) return nullptr;
    for (uint32_t index = 0u; index < count_; index++) {
        if (entries_[index].key != key) continue;
        entries_[index].resource->AddRef();
        return entries_[index].resource;
    }
    return nullptr;
}

void D3dSharedResources::withdraw(IUnknown *resource)
{
    AV_SCOPE("D3dSharedResources::withdraw");
    for (uint32_t index = 0u; index < count_; index++) {
        if (entries_[index].resource != resource) continue;
        entries_[index].resource->Release();
        entries_[index] = entries_[--count_];
        return;
    }
}

void D3dSharedResources::clear()
{
    AV_SCOPE("D3dSharedResources::clear");
    for (uint32_t index = 0u; index < count_; index++) entries_[index].resource->Release();
    count_ = 0u;
}

uint32_t D3dSharedResources::count() const
{
    AV_SCOPE("D3dSharedResources::count");
    return count_;
}

}  // namespace av
