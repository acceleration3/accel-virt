#include "executor/d3d/slice_storages.hpp"

#include "core/log/driver_log.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/d3d/d3d11_batch_progress.hpp"

namespace av {

SliceStorages::SliceStorages(BlockSource &blocks, DriverLog &lines)
    : storages_(blocks), lines_(lines), retiring_(0u), reported_(false)
{
    AV_SCOPE("SliceStorages::SliceStorages");
}

const VulkanBuffer *SliceStorages::bind(D3d11HostDevice &device, SourceMemory &store, const D3d11BatchProgress &progress)
{
    AV_SCOPE("SliceStorages::bind");
    reclaim(progress);
    SliceStorage *storage = find(store);
    if (storage == nullptr && storages_.create(storage, store) != Status::ok) return nullptr;
    if (storage->retired() || storage->refused()) return nullptr;
    if (!storage->imported() && !storage->import(device)) {
        if (!reported_) lines_.write("a block of slice memory could not be imported to draw from: its buffers are copied");
        reported_ = true;
        return nullptr;
    }
    storage->used_at(progress.next());
    return &storage->buffer();
}

void SliceStorages::retire(const SourceMemory &store)
{
    AV_SCOPE("SliceStorages::retire");
    SliceStorage *const storage = find(store);
    if (storage == nullptr || storage->retired()) return;
    storage->retire();
    retiring_++;
}

void SliceStorages::reclaim(const D3d11BatchProgress &progress)
{
    AV_SCOPE("SliceStorages::reclaim");
    if (retiring_ == 0u) return;
    const uint64_t reached = progress.reached();
    const uint32_t before = storages_.live();
    storages_.destroy_if([reached](const SliceStorage &storage) { return storage.done(reached); });
    retiring_ -= before - storages_.live();
}

void SliceStorages::destroy_all()
{
    AV_SCOPE("SliceStorages::destroy_all");
    storages_.destroy_all();
    retiring_ = 0u;
    reported_ = false;
}

uint32_t SliceStorages::count() const
{
    AV_SCOPE("SliceStorages::count");
    return storages_.live();
}

uint32_t SliceStorages::retiring() const
{
    AV_SCOPE("SliceStorages::retiring");
    return retiring_;
}

SliceStorage *SliceStorages::find(const SourceMemory &store)
{
    AV_SCOPE("SliceStorages::find");
    return storages_.find_if([&store](const SliceStorage &storage) { return storage.holds(store); });
}

bool SliceStorages::invariants_hold() const
{
    AV_SCOPE("SliceStorages::invariants_hold");
    uint32_t retired = 0u;
    storages_.for_each([&retired](const SliceStorage &storage) { retired += storage.retired() ? 1u : 0u; });
    return retired == retiring_ && storages_.invariants_hold();
}

}
