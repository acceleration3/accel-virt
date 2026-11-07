#include "executor/d3d/d3d12_descriptor_holds.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d_headers.hpp"

namespace av {

D3d12DescriptorHolds::D3d12DescriptorHolds(BlockSource &blocks, D3d12ObjectHolds &holds) : holds_(holds), tables_(blocks)
{
    AV_SCOPE("D3d12DescriptorHolds::D3d12DescriptorHolds");
}

uint64_t D3d12DescriptorHolds::key_of(uint32_t index) const
{
    AV_SCOPE("D3d12DescriptorHolds::key_of");
    return index;
}

void D3d12DescriptorHolds::release_all()
{
    AV_SCOPE("D3d12DescriptorHolds::release_all");
    tables_.drain_all([](View &view) {
        if (view.first_ != nullptr) view.first_->Release();
        if (view.second_ != nullptr) view.second_->Release();
    });
}

bool D3d12DescriptorHolds::holds(uint32_t heap, uint32_t index, const IUnknown *resource) const
{
    AV_SCOPE("D3d12DescriptorHolds::holds");
    const View *const found = tables_.find(heap, key_of(index));
    return found != nullptr && (found->first_ == resource || found->second_ == resource);
}

IUnknown *D3d12DescriptorHolds::resource_at(uint32_t heap, uint32_t index) const
{
    AV_SCOPE("D3d12DescriptorHolds::resource_at");
    const View *const found = tables_.find(heap, key_of(index));
    return found != nullptr ? found->first_ : nullptr;
}

D3d12BlockHeap *D3d12DescriptorHolds::pin_at(uint32_t heap, uint32_t index) const
{
    AV_SCOPE("D3d12DescriptorHolds::pin_at");
    const View *const found = tables_.find(heap, key_of(index));
    return found != nullptr ? found->first_pin_ : nullptr;
}

uint32_t D3d12DescriptorHolds::count() const
{
    AV_SCOPE("D3d12DescriptorHolds::count");
    return tables_.count();
}

bool D3d12DescriptorHolds::invariants_hold() const
{
    AV_SCOPE("D3d12DescriptorHolds::invariants_hold");
    return tables_.invariants_hold();
}

}
