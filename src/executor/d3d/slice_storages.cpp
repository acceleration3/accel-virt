#include "executor/d3d/slice_storages.hpp"

#include "core/log/driver_log.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/d3d/d3d11_batch_progress.hpp"

namespace av {

SliceStorages::SliceStorages(BlockSource &blocks, DriverLog &lines)
    : storages_(blocks), bindings_(blocks), lines_(lines), retiring_(0u), reported_(false)
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

bool SliceStorages::attach(const SliceBindings::Binding &binding)
{
    AV_SCOPE("SliceStorages::attach");
    return bindings_.bind(binding) == Status::ok;
}

bool SliceStorages::leave(uint32_t resource, SliceBindings::Binding &was)
{
    AV_SCOPE("SliceStorages::leave");
    return bindings_.take(resource, was);
}

bool SliceStorages::next_bound_to(const SourceMemory &store, SliceBindings::Binding &was)
{
    AV_SCOPE("SliceStorages::next_bound_to");
    return bindings_.take_bound_to(store, was);
}

void SliceStorages::retire(const SourceMemory &store, const D3d11BatchProgress &progress)
{
    AV_SCOPE("SliceStorages::retire");
    SliceStorage *const storage = find(store);
    if (storage == nullptr || storage->retired()) return;
    storage->used_at(progress.next());
    storage->retire();
    retiring_++;
}

bool SliceStorages::holds(const SourceMemory &store)
{
    AV_SCOPE("SliceStorages::holds");
    return find(store) != nullptr;
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
    bindings_.clear();
    retiring_ = 0u;
    reported_ = false;
}

uint32_t SliceStorages::count() const
{
    AV_SCOPE("SliceStorages::count");
    return storages_.live();
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
    return retired == retiring_ && storages_.invariants_hold() && bindings_.invariants_hold();
}

}
