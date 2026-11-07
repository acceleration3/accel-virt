#include "executor/d3d/d3d12_descriptor_holds.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d_headers.hpp"

namespace av {

D3d12DescriptorHolds::D3d12DescriptorHolds(BlockSource &blocks, D3d12ObjectHolds &holds)
    : holds_(holds), slots_(blocks, Pool::paged)
{
    AV_SCOPE("D3d12DescriptorHolds::D3d12DescriptorHolds");
}

uint64_t D3d12DescriptorHolds::key_of(uint32_t heap, uint32_t index) const
{
    AV_SCOPE("D3d12DescriptorHolds::key_of");
    return (uint64_t{heap} << 32u) | index;
}

Status D3d12DescriptorHolds::put(uint64_t key, IUnknown *resource, IUnknown *counter, Slot &displaced)
{
    AV_SCOPE("D3d12DescriptorHolds::put");
    displaced = Slot{};
    Slot *const found = slots_.find(key);
    if (resource == nullptr && counter == nullptr) {
        if (found == nullptr) return Status::ok;
        displaced = *found;
        slots_.erase(key);
        return Status::ok;
    }
    if (found != nullptr) {
        displaced = *found;
        *found = Slot{resource, counter};
    } else {
        uint32_t     slot = 0u;
        const Status inserted = slots_.insert(key, Slot{resource, counter}, slot);
        if (inserted != Status::ok) return inserted;
    }
    if (resource != nullptr) resource->AddRef();
    if (counter != nullptr) counter->AddRef();
    return Status::ok;
}

void D3d12DescriptorHolds::release_all()
{
    AV_SCOPE("D3d12DescriptorHolds::release_all");
    slots_.for_each([](uint64_t, Slot &slot) {
        if (slot.resource_ != nullptr) slot.resource_->Release();
        if (slot.counter_ != nullptr) slot.counter_->Release();
    });
    slots_.clear();
}

bool D3d12DescriptorHolds::holds(uint32_t heap, uint32_t index, const IUnknown *resource) const
{
    AV_SCOPE("D3d12DescriptorHolds::holds");
    const Slot *const found = slots_.find(key_of(heap, index));
    return found != nullptr && (found->resource_ == resource || found->counter_ == resource);
}

IUnknown *D3d12DescriptorHolds::resource_at(uint32_t heap, uint32_t index) const
{
    AV_SCOPE("D3d12DescriptorHolds::resource_at");
    const Slot *const found = slots_.find(key_of(heap, index));
    return found != nullptr ? found->resource_ : nullptr;
}

uint32_t D3d12DescriptorHolds::count() const
{
    AV_SCOPE("D3d12DescriptorHolds::count");
    return slots_.count();
}

bool D3d12DescriptorHolds::invariants_hold() const
{
    AV_SCOPE("D3d12DescriptorHolds::invariants_hold");
    if (!slots_.invariants_hold()) return false;
    bool sound = true;
    slots_.for_each([&sound](uint64_t, const Slot &slot) {
        sound = sound && (slot.resource_ != nullptr || slot.counter_ != nullptr);
    });
    return sound;
}

}
