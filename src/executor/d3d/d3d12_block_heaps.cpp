#include "executor/d3d/d3d12_block_heaps.hpp"

#include "core/log/driver_log.hpp"
#include "core/log/log_line.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/d3d/d3d12_queue_progress.hpp"
#include "executor/source_memory.hpp"

namespace av {

D3d12BlockHeaps::D3d12BlockHeaps(BlockSource &blocks, const D3d12QueueProgress &progress, DriverLog &lines)
    : blocks_(blocks), windows_(blocks, Pool::paged), placed_(blocks, Pool::paged), progress_(progress),
      lines_(lines), abandoned_(0u)
{
    AV_SCOPE("D3d12BlockHeaps::D3d12BlockHeaps");
}

bool D3d12BlockHeaps::window_holds(const SourceMemory &store, uint64_t base, uint64_t size) const
{
    AV_SCOPE("D3d12BlockHeaps::window_holds");
    const uint64_t whole = store.size();
    const bool     aligned = base % window_alignment == 0u && size % window_alignment == 0u;
    if (size != 0u && aligned && base <= whole && size <= whole - base) return true;
    LogLine(lines_).text("a heap window of ").number(size).text(" bytes at ").number(base).text(" is not inside memory of ")
        .number(whole).text(" bytes in 64 KiB steps").write();
    return false;
}

D3d12BlockHeap *D3d12BlockHeaps::block_for(const D3d12HostDevice &device, SourceMemory &store, bool &fresh)
{
    AV_SCOPE("D3d12BlockHeaps::block_for");
    fresh = false;
    D3d12BlockHeap *found = blocks_.find_if([&store](const D3d12BlockHeap &block) { return block.holds(store); });
    if (found != nullptr) return found->abandoned() ? nullptr : found;
    const Status created = blocks_.create(found, store);
    if (created != Status::ok) {
        LogLine(lines_).text("a heap window's memory kept nowhere, status ").number(static_cast<uint32_t>(created)).write();
        return nullptr;
    }
    fresh = true;
    if (found->import(device)) return found;
    LogLine(lines_).text("the host did not open a heap over ").number(store.size()).text(" bytes").write();
    forget_fresh(found);
    return nullptr;
}

void D3d12BlockHeaps::forget_fresh(D3d12BlockHeap *block)
{
    AV_SCOPE("D3d12BlockHeaps::forget_fresh");
    static_cast<void>(blocks_.destroy(block));
}

ID3D12Heap *D3d12BlockHeaps::open(const D3d12HostDevice &device, SourceMemory &store, uint32_t heap, uint64_t base,
                                  uint64_t size)
{
    AV_SCOPE("D3d12BlockHeaps::open");
    if (!window_holds(store, base, size)) return nullptr;
    if (windows_.find(heap) != nullptr) {
        LogLine(lines_).text("a heap window made again under heap ").number(heap).write();
        return nullptr;
    }
    bool                  fresh = false;
    D3d12BlockHeap *const block = block_for(device, store, fresh);
    if (block == nullptr) return nullptr;
    uint32_t     slot = 0u;
    const Status kept = windows_.insert(heap, Window{block, base, size}, slot);
    if (kept != Status::ok) {
        LogLine(lines_).text("a heap window kept nowhere, status ").number(static_cast<uint32_t>(kept)).write();
        if (fresh) forget_fresh(block);
        return nullptr;
    }
    block->took_heap();
    return block->heap();
}

bool D3d12BlockHeaps::window(uint32_t heap, uint64_t &base, uint64_t &size) const
{
    AV_SCOPE("D3d12BlockHeaps::window");
    const Window *const found = windows_.find(heap);
    if (found == nullptr) return false;
    base = found->base_;
    size = found->size_;
    return true;
}

bool D3d12BlockHeaps::attach(uint32_t resource, uint32_t heap)
{
    AV_SCOPE("D3d12BlockHeaps::attach");
    const Window *const found = windows_.find(heap);
    if (found == nullptr) return false;
    uint32_t     slot = 0u;
    const Status kept = placed_.insert(resource, found->block_, slot);
    if (kept != Status::ok) {
        LogLine(lines_).text("a resource placed in a heap window kept nowhere, status ").number(static_cast<uint32_t>(kept)).write();
        return false;
    }
    found->block_->took_resource();
    return true;
}

void D3d12BlockHeaps::detach(uint32_t resource)
{
    AV_SCOPE("D3d12BlockHeaps::detach");
    D3d12BlockHeap *block = nullptr;
    if (placed_.take(resource, block)) block->let_go_resource(progress_.marked());
}

void D3d12BlockHeaps::release_heap(uint32_t heap)
{
    AV_SCOPE("D3d12BlockHeaps::release_heap");
    Window gone = {};
    if (windows_.take(heap, gone)) gone.block_->let_go_heap(progress_.marked());
}

void D3d12BlockHeaps::abandon(const SourceMemory &store)
{
    AV_SCOPE("D3d12BlockHeaps::abandon");
    D3d12BlockHeap *const block = blocks_.find_if([&store](const D3d12BlockHeap &held) { return held.holds(store); });
    if (block == nullptr || block->abandoned()) return;
    block->abandon(progress_.marked());
    abandoned_++;
    reclaim();
}

void D3d12BlockHeaps::reclaim()
{
    AV_SCOPE("D3d12BlockHeaps::reclaim");
    if (abandoned_ == 0u) return;
    const uint64_t reached = progress_.reached();
    const uint32_t before = blocks_.live();
    blocks_.destroy_if([reached](const D3d12BlockHeap &block) { return block.done(reached); });
    abandoned_ -= before - blocks_.live();
}

void D3d12BlockHeaps::destroy_all()
{
    AV_SCOPE("D3d12BlockHeaps::destroy_all");
    windows_.clear();
    placed_.clear();
    blocks_.destroy_all();
    abandoned_ = 0u;
}

uint32_t D3d12BlockHeaps::count() const
{
    AV_SCOPE("D3d12BlockHeaps::count");
    return blocks_.live();
}

bool D3d12BlockHeaps::invariants_hold() const
{
    AV_SCOPE("D3d12BlockHeaps::invariants_hold");
    uint32_t abandoned = 0u;
    bool     sound = blocks_.invariants_hold() && windows_.invariants_hold() && placed_.invariants_hold();
    blocks_.for_each([&abandoned, &sound](const D3d12BlockHeap &block) {
        abandoned += block.abandoned() ? 1u : 0u;
        sound = sound && block.invariants_hold();
    });
    windows_.for_each([this, &sound](uint32_t, const Window &held) {
        sound = sound && blocks_.find(held.block_) != nullptr && held.base_ <= held.block_->bytes() &&
                held.size_ <= held.block_->bytes() - held.base_;
    });
    placed_.for_each([this, &sound](uint32_t, D3d12BlockHeap *const &held) {
        sound = sound && blocks_.find(held) != nullptr;
    });
    return sound && abandoned == abandoned_;
}

}
