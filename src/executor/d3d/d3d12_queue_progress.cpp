#include "executor/d3d/d3d12_queue_progress.hpp"

#include "core/log/log_line.hpp"
#include "core/trace/function_scope.hpp"

#include "executor/eventfd_wake.hpp"
#include "executor/shim/d3d_headers.hpp"

#include <stdio.h>

namespace av {

D3d12QueueProgress::D3d12QueueProgress(EventfdWake &wake, BlockSource &blocks)
    : wake_(wake), queues_(blocks, Pool::paged), draining_(blocks, Pool::paged), marked_(0u)
{
    AV_SCOPE("D3d12QueueProgress::D3d12QueueProgress");
}

uint32_t D3d12QueueProgress::index_of(uint32_t id) const
{
    AV_SCOPE("D3d12QueueProgress::index_of");
    for (uint32_t index = 0u; index < queues_.size(); index++)
        if (queues_.at(index).id_ == id) return index;
    return queues_.size();
}

void D3d12QueueProgress::release_completed()
{
    AV_SCOPE("D3d12QueueProgress::release_completed");
    uint32_t index = 0u;
    while (index < draining_.size()) {
        Queue &gone = draining_.at(index);
        if (gone.fence_->GetCompletedValue() < gone.signalled_) {
            index++;
            continue;
        }
        gone.fence_->Release();
        draining_.remove_swap(index);
    }
}

bool D3d12QueueProgress::executed(uint32_t id, ID3D12CommandQueue *queue, ID3D12Device *device)
{
    AV_SCOPE("D3d12QueueProgress::executed");
    if (queue == nullptr || device == nullptr) return false;
    release_completed();
    uint32_t index = index_of(id);
    if (index == queues_.size()) {
        ID3D12Fence *made = nullptr;
        if (FAILED(device->CreateFence(0u, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence),
                                       reinterpret_cast<void **>(&made))))
            return false;
        if (queues_.append(Queue{id, queue, made, 0u}) != Status::ok) {
            made->Release();
            return false;
        }
    }
    Queue &kept = queues_.at(index);
    kept.queue_ = queue;
    if (FAILED(queue->Signal(kept.fence_, marked_ + 1u))) return false;
    kept.signalled_ = ++marked_;
    return wake_.watch(*kept.fence_, marked_);
}

bool D3d12QueueProgress::after_siblings(uint32_t id, ID3D12CommandQueue *queue) const
{
    AV_SCOPE("D3d12QueueProgress::after_siblings");
    if (queue == nullptr) return false;
    for (const Queue &sibling : queues_) {
        if (sibling.id_ == id || sibling.fence_->GetCompletedValue() >= sibling.signalled_) continue;
        if (FAILED(queue->Wait(sibling.fence_, sibling.signalled_))) return false;
    }
    return true;
}

bool D3d12QueueProgress::mark_all()
{
    AV_SCOPE("D3d12QueueProgress::mark_all");
    bool signalled = true;
    for (Queue &kept : queues_) {
        if (FAILED(kept.queue_->Signal(kept.fence_, marked_ + 1u))) {
            signalled = false;
            continue;
        }
        kept.signalled_ = ++marked_;
    }
    return signalled;
}

void D3d12QueueProgress::forget(uint32_t id)
{
    AV_SCOPE("D3d12QueueProgress::forget");
    const uint32_t index = index_of(id);
    if (index == queues_.size()) return;
    Queue &gone = queues_.at(index);
    if (gone.fence_->GetCompletedValue() < gone.signalled_ && draining_.append(gone) == Status::ok) {
        draining_.at(draining_.size() - 1u).queue_ = nullptr;
        queues_.remove_swap(index);
        return;
    }
    gone.fence_->Release();
    queues_.remove_swap(index);
    release_completed();
}

void D3d12QueueProgress::log_stuck(DriverLog &lines) const
{
    AV_SCOPE("D3d12QueueProgress::log_stuck");
    for (const Queue &kept : queues_)
        LogLine(lines)
            .text("queue ")
            .number(kept.id_)
            .text(" progress signalled ")
            .number(kept.signalled_)
            .text(" completed ")
            .number(kept.fence_->GetCompletedValue())
            .write();
    for (const Queue &gone : draining_)
        LogLine(lines)
            .text("destroyed queue ")
            .number(gone.id_)
            .text(" progress signalled ")
            .number(gone.signalled_)
            .text(" completed ")
            .number(gone.fence_->GetCompletedValue())
            .write();
}

void D3d12QueueProgress::clear()
{
    AV_SCOPE("D3d12QueueProgress::clear");
    for (Queue &kept : queues_) kept.fence_->Release();
    queues_.clear();
    for (Queue &gone : draining_) gone.fence_->Release();
    draining_.clear();
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
    for (const Queue &kept : queues_) {
        const uint64_t completed = kept.fence_->GetCompletedValue();
        if (completed < kept.signalled_ && completed < reached) reached = completed;
    }
    for (const Queue &gone : draining_) {
        const uint64_t completed = gone.fence_->GetCompletedValue();
        if (completed < gone.signalled_ && completed < reached) reached = completed;
    }
    return reached;
}

bool D3d12QueueProgress::invariants_hold() const
{
    AV_SCOPE("D3d12QueueProgress::invariants_hold");
    if (!queues_.invariants_hold()) return false;
    for (const Queue &kept : queues_)
        if (kept.queue_ == nullptr || kept.fence_ == nullptr || kept.signalled_ > marked_) return false;
    if (!draining_.invariants_hold()) return false;
    for (const Queue &gone : draining_)
        if (gone.fence_ == nullptr || gone.signalled_ > marked_) return false;
    return true;
}

}
