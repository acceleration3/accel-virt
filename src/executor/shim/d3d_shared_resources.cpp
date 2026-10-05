#include "executor/shim/d3d_shared_resources.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d_headers.hpp"

namespace av {

D3dSharedResources::D3dSharedResources() : keys_{}, resources_{}, shown_{}, writes_{}, count_(0u)
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
    writes_[count_] = 0u;
    shown_[count_] = nullptr;
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

bool D3dSharedResources::replace(uint64_t key, IUnknown *resource)
{
    AV_SCOPE("D3dSharedResources::replace");
    if (key == 0u || resource == nullptr) return false;
    for (uint32_t index = 0u; index < count_; index++) {
        if (keys_[index] != key) continue;
        resource->AddRef();
        resources_[index]->Release();
        resources_[index] = resource;
        return true;
    }
    return false;
}

bool D3dSharedResources::show(uint64_t key, IUnknown *shown)
{
    AV_SCOPE("D3dSharedResources::show");
    if (key == 0u || shown == nullptr) return false;
    for (uint32_t index = 0u; index < count_; index++) {
        if (keys_[index] != key) continue;
        shown->AddRef();
        if (shown_[index] != nullptr) shown_[index]->Release();
        shown_[index] = shown;
        return true;
    }
    return false;
}

IUnknown *D3dSharedResources::shown(uint64_t key) const
{
    AV_SCOPE("D3dSharedResources::shown");
    if (key == 0u) return nullptr;
    for (uint32_t index = 0u; index < count_; index++) {
        if (keys_[index] != key || shown_[index] == nullptr) continue;
        shown_[index]->AddRef();
        return shown_[index];
    }
    return nullptr;
}

void D3dSharedResources::written(uint64_t key)
{
    AV_SCOPE("D3dSharedResources::written");
    if (key == 0u) return;
    for (uint32_t index = 0u; index < count_; index++) {
        if (keys_[index] != key) continue;
        writes_[index]++;
        return;
    }
}

bool D3dSharedResources::writes(uint64_t key, uint64_t &count) const
{
    AV_SCOPE("D3dSharedResources::writes");
    count = 0u;
    if (key == 0u) return false;
    for (uint32_t index = 0u; index < count_; index++) {
        if (keys_[index] != key) continue;
        count = writes_[index];
        return true;
    }
    return false;
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
    if (shown_[index] != nullptr) shown_[index]->Release();
    count_--;
    keys_[index] = keys_[count_];
    writes_[index] = writes_[count_];
    resources_[index] = resources_[count_];
    shown_[index] = shown_[count_];
    shown_[count_] = nullptr;
}

void D3dSharedResources::clear()
{
    AV_SCOPE("D3dSharedResources::clear");
    for (uint32_t index = 0u; index < count_; index++) {
        resources_[index]->Release();
        if (shown_[index] != nullptr) shown_[index]->Release();
        shown_[index] = nullptr;
    }
    count_ = 0u;
}

uint32_t D3dSharedResources::count() const
{
    AV_SCOPE("D3dSharedResources::count");
    return count_;
}

}  // namespace av
