#include "device/executor/d3d12_fence_publisher.hpp"

#include "core/trace/function_scope.hpp"
#include "device/executor/guest_region.hpp"
#include "device/shim/d3d_headers.hpp"

namespace av {

D3d12FencePublisher::D3d12FencePublisher(GuestRegion &region)
    : region_(region), ids_{}, fences_{}, offsets_{}, written_{}, expected_{}, count_(0u)
{
    AV_SCOPE("D3d12FencePublisher::D3d12FencePublisher");
}

uint32_t D3d12FencePublisher::slot_of(uint32_t id) const
{
    AV_SCOPE("D3d12FencePublisher::slot_of");
    for (uint32_t slot = 0u; slot < count_; slot++)
        if (ids_[slot] == id) return slot;
    return capacity;
}

void D3d12FencePublisher::write(uint32_t slot, uint64_t value)
{
    AV_SCOPE("D3d12FencePublisher::write");
    if (!region_.publish(offsets_[slot], value)) return;
    written_[slot] = value;
}

bool D3d12FencePublisher::publish(uint32_t id, ID3D12Fence *fence, uint64_t offset)
{
    AV_SCOPE("D3d12FencePublisher::publish");
    if (offset == stop) {
        forget(id);
        return true;
    }
    if (fence == nullptr || offset % sizeof(uint64_t) != 0u || region_.at(offset, sizeof(uint64_t)) == nullptr)
        return false;
    uint32_t slot = slot_of(id);
    if (slot == capacity) {
        if (count_ == capacity) return false;
        slot = count_++;
        expected_[slot] = 0u;
    }
    ids_[slot] = id;
    fences_[slot] = fence;
    offsets_[slot] = offset;
    write(slot, fence->GetCompletedValue());
    return true;
}

void D3d12FencePublisher::expect(uint32_t id, uint64_t value)
{
    AV_SCOPE("D3d12FencePublisher::expect");
    const uint32_t slot = slot_of(id);
    if (slot != capacity && value > expected_[slot]) expected_[slot] = value;
}

void D3d12FencePublisher::forget(uint32_t id)
{
    AV_SCOPE("D3d12FencePublisher::forget");
    const uint32_t slot = slot_of(id);
    if (slot == capacity) return;
    count_--;
    ids_[slot] = ids_[count_];
    fences_[slot] = fences_[count_];
    offsets_[slot] = offsets_[count_];
    written_[slot] = written_[count_];
    expected_[slot] = expected_[count_];
}

bool D3d12FencePublisher::update()
{
    AV_SCOPE("D3d12FencePublisher::update");
    bool pending = false;
    for (uint32_t slot = 0u; slot < count_; slot++) {
        const uint64_t completed = fences_[slot]->GetCompletedValue();
        if (completed != written_[slot]) write(slot, completed);
        if (completed < expected_[slot]) pending = true;
    }
    return pending;
}

void D3d12FencePublisher::clear()
{
    AV_SCOPE("D3d12FencePublisher::clear");
    count_ = 0u;
}

uint32_t D3d12FencePublisher::count() const
{
    AV_SCOPE("D3d12FencePublisher::count");
    return count_;
}

bool D3d12FencePublisher::invariants_hold() const
{
    AV_SCOPE("D3d12FencePublisher::invariants_hold");
    if (count_ > capacity) return false;
    for (uint32_t slot = 0u; slot < count_; slot++)
        if (fences_[slot] == nullptr || offsets_[slot] % sizeof(uint64_t) != 0u) return false;
    return true;
}

}  // namespace av
