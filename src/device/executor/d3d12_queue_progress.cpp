#include "device/executor/d3d12_queue_progress.hpp"

#include "core/trace/function_scope.hpp"

#include "device/executor/d3d12_fence_wake.hpp"
#include "device/shim/d3d_headers.hpp"

#include <stdio.h>

namespace av {

D3d12QueueProgress::D3d12QueueProgress(D3d12FenceWake &wake) : wake_(wake), ids_{}, queues_{}, fences_{}, signalled_{}, count_(0u), marked_(0u)
{
    AV_SCOPE("D3d12QueueProgress::D3d12QueueProgress");
}

uint32_t D3d12QueueProgress::slot_of(uint32_t id) const
{
    AV_SCOPE("D3d12QueueProgress::slot_of");
    for (uint32_t slot = 0u; slot < count_; slot++)
        if (ids_[slot] == id) return slot;
    return capacity;
}

bool D3d12QueueProgress::executed(uint32_t id, ID3D12CommandQueue *queue, ID3D12Device *device)
{
    AV_SCOPE("D3d12QueueProgress::executed");
    if (queue == nullptr || device == nullptr) return false;
    uint32_t slot = slot_of(id);
    if (slot == capacity) {
        if (count_ == capacity) return false;
        ID3D12Fence *made = nullptr;
        if (FAILED(device->CreateFence(0u, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence),
                                       reinterpret_cast<void **>(&made))))
            return false;
        slot = count_++;
        ids_[slot] = id;
        fences_[slot] = made;
        signalled_[slot] = 0u;
    }
    queues_[slot] = queue;
    if (FAILED(queue->Signal(fences_[slot], marked_ + 1u))) return false;
    signalled_[slot] = ++marked_;
    return wake_.watch(fences_[slot], marked_);
}

bool D3d12QueueProgress::after_siblings(uint32_t id, ID3D12CommandQueue *queue) const
{
    AV_SCOPE("D3d12QueueProgress::after_siblings");
    if (queue == nullptr) return false;
    for (uint32_t slot = 0u; slot < count_; slot++) {
        if (ids_[slot] == id || fences_[slot]->GetCompletedValue() >= signalled_[slot]) continue;
        if (FAILED(queue->Wait(fences_[slot], signalled_[slot]))) return false;
    }
    return true;
}

bool D3d12QueueProgress::mark_all()
{
    AV_SCOPE("D3d12QueueProgress::mark_all");
    bool signalled = true;
    for (uint32_t slot = 0u; slot < count_; slot++) {
        if (FAILED(queues_[slot]->Signal(fences_[slot], marked_ + 1u))) {
            signalled = false;
            continue;
        }
        signalled_[slot] = ++marked_;
    }
    return signalled;
}

void D3d12QueueProgress::forget(uint32_t id)
{
    AV_SCOPE("D3d12QueueProgress::forget");
    const uint32_t slot = slot_of(id);
    if (slot == capacity) return;
    fences_[slot]->Release();
    count_--;
    ids_[slot] = ids_[count_];
    queues_[slot] = queues_[count_];
    fences_[slot] = fences_[count_];
    signalled_[slot] = signalled_[count_];
    queues_[count_] = nullptr;
    fences_[count_] = nullptr;
}

void D3d12QueueProgress::log_stuck() const
{
    AV_SCOPE("D3d12QueueProgress::log_stuck");
    for (uint32_t slot = 0u; slot < count_; slot++)
        fprintf(stderr, "virtio-gpu-rvk: d3d12: queue %u progress signalled %llu completed %llu\n", ids_[slot],
                static_cast<unsigned long long>(signalled_[slot]),
                static_cast<unsigned long long>(fences_[slot]->GetCompletedValue()));
}

void D3d12QueueProgress::clear()
{
    AV_SCOPE("D3d12QueueProgress::clear");
    for (uint32_t slot = 0u; slot < count_; slot++) {
        fences_[slot]->Release();
        fences_[slot] = nullptr;
        queues_[slot] = nullptr;
    }
    count_ = 0u;
}

uint64_t D3d12QueueProgress::marked() const
{
    AV_SCOPE("D3d12QueueProgress::marked");
    return marked_;
}

uint64_t D3d12QueueProgress::reached() const
{
    AV_SCOPE("D3d12QueueProgress::reached");
    uint64_t reached = marked_;
    for (uint32_t slot = 0u; slot < count_; slot++) {
        const uint64_t completed = fences_[slot]->GetCompletedValue();
        if (completed < signalled_[slot] && completed < reached) reached = completed;
    }
    return reached;
}

bool D3d12QueueProgress::invariants_hold() const
{
    AV_SCOPE("D3d12QueueProgress::invariants_hold");
    if (count_ > capacity) return false;
    for (uint32_t slot = 0u; slot < count_; slot++)
        if (queues_[slot] == nullptr || fences_[slot] == nullptr || signalled_[slot] > marked_) return false;
    return true;
}

}  // namespace av
