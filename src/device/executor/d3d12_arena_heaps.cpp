#include "device/executor/d3d12_arena_heaps.hpp"

#include "core/trace/function_scope.hpp"
#include "device/executor/guest_region.hpp"
#include "device/shim/d3d12_host_device.hpp"
#include "device/shim/d3d_headers.hpp"

namespace av {

namespace {

constexpr uint32_t none = ~0u;

}  // namespace

D3d12ArenaHeaps::D3d12ArenaHeaps() : heap_(nullptr), ranges_{}, count_(0u)
{
    AV_SCOPE("D3d12ArenaHeaps::D3d12ArenaHeaps");
}

D3d12ArenaHeaps::~D3d12ArenaHeaps()
{
    AV_SCOPE("D3d12ArenaHeaps::~D3d12ArenaHeaps");
    close();
}

bool D3d12ArenaHeaps::add(D3d12HostDevice &device, GuestRegion &region, uint32_t id, uint64_t base, uint64_t size)
{
    AV_SCOPE("D3d12ArenaHeaps::add");
    if (count_ == max_heaps || index_of(id) != none || region.at(base, size) == nullptr) return false;
    if (heap_ == nullptr) {
        heap_ = device.import_dmabuf_heap(region.at(0u, region.length()), region.descriptor(), region.length());
        if (heap_ == nullptr) return false;
    }
    ranges_[count_++] = Range{id, base, size};
    return true;
}

bool D3d12ArenaHeaps::place(uint32_t id, uint64_t offset, uint64_t bytes, uint64_t &at) const
{
    AV_SCOPE("D3d12ArenaHeaps::place");
    at = 0u;
    const uint32_t index = index_of(id);
    if (index == none) return false;
    const Range &range = ranges_[index];
    if (offset > range.size || bytes > range.size - offset) return false;
    at = range.base + offset;
    return true;
}

bool D3d12ArenaHeaps::remove(uint32_t id)
{
    AV_SCOPE("D3d12ArenaHeaps::remove");
    const uint32_t index = index_of(id);
    if (index == none) return false;
    ranges_[index] = ranges_[--count_];
    return true;
}

ID3D12Heap *D3d12ArenaHeaps::heap() const
{
    AV_SCOPE("D3d12ArenaHeaps::heap");
    return heap_;
}

void D3d12ArenaHeaps::close()
{
    AV_SCOPE("D3d12ArenaHeaps::close");
    if (heap_ != nullptr) heap_->Release();
    heap_ = nullptr;
    count_ = 0u;
}

uint32_t D3d12ArenaHeaps::index_of(uint32_t id) const
{
    AV_SCOPE("D3d12ArenaHeaps::index_of");
    for (uint32_t index = 0u; index < count_; index++)
        if (ranges_[index].id == id) return index;
    return none;
}

}  // namespace av
