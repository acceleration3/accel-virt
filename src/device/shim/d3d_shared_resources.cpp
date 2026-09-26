#include "device/shim/d3d_shared_resources.hpp"

#include "core/trace/function_scope.hpp"
#include "device/shim/d3d_headers.hpp"

namespace av {

D3dSharedResources::D3dSharedResources() : keys_{}, resources_{}, owners_{}, released_at_{}, count_(0u)
{
    AV_SCOPE("D3dSharedResources::D3dSharedResources");
}

bool D3dSharedResources::publish(uint64_t key, IUnknown *resource, const void *owner, uint64_t now)
{
    AV_SCOPE("D3dSharedResources::publish");
    if (key == 0u || resource == nullptr) return false;
    expire(now);
    for (uint32_t index = 0u; index < count_; index++) {
        if (keys_[index] != key) continue;
        if (released_at_[index] == held) return false;
        remove(index);   /* a key its creator let go of names the new resource */
        break;
    }
    if (count_ == capacity) {
        const uint32_t oldest = oldest_released();
        if (oldest == capacity) return false;
        remove(oldest);
    }
    resource->AddRef();
    keys_[count_] = key;
    resources_[count_] = resource;
    owners_[count_] = owner;
    released_at_[count_++] = held;
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

void D3dSharedResources::withdraw(IUnknown *resource, const void *owner, uint64_t now)
{
    AV_SCOPE("D3dSharedResources::withdraw");
    for (uint32_t index = 0u; index < count_; index++) {
        if (resources_[index] != resource || owners_[index] != owner || released_at_[index] != held) continue;
        released_at_[index] = now;
        break;
    }
    expire(now);
}

void D3dSharedResources::withdraw_all(const void *owner, uint64_t now)
{
    AV_SCOPE("D3dSharedResources::withdraw_all");
    for (uint32_t index = 0u; index < count_; index++)
        if (owners_[index] == owner && released_at_[index] == held) released_at_[index] = now;
    expire(now);
}

void D3dSharedResources::expire(uint64_t now)
{
    AV_SCOPE("D3dSharedResources::expire");
    for (uint32_t index = 0u; index < count_;) {
        const bool expired = released_at_[index] != held && now - released_at_[index] > released_for_ns;
        if (expired) remove(index);
        else index++;
    }
}

uint32_t D3dSharedResources::oldest_released() const
{
    AV_SCOPE("D3dSharedResources::oldest_released");
    uint32_t oldest = capacity;
    for (uint32_t index = 0u; index < count_; index++)
        if (released_at_[index] != held && (oldest == capacity || released_at_[index] < released_at_[oldest]))
            oldest = index;
    return oldest;
}

void D3dSharedResources::remove(uint32_t index)
{
    AV_SCOPE("D3dSharedResources::remove");
    resources_[index]->Release();
    count_--;
    keys_[index] = keys_[count_];
    resources_[index] = resources_[count_];
    owners_[index] = owners_[count_];
    released_at_[index] = released_at_[count_];
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
