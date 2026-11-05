#include "executor/d3d/slice_storages.hpp"

#include "core/log/driver_log.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/d3d/slice_progress.hpp"

namespace av {

SliceStorages::SliceStorages(BlockSource &blocks, DriverLog &lines)
    : storages_(blocks), bindings_(blocks), lines_(lines), retiring_(0u), reported_(false)
{
    AV_SCOPE("SliceStorages::SliceStorages");
}

const VulkanBuffer *SliceStorages::bind(SliceImporter &importer, const SliceProgress &progress, SourceMemory &store)
{
    AV_SCOPE("SliceStorages::bind");
    reclaim();
    SliceStorage *storage = find(store, importer);
    if (storage == nullptr && storages_.create(storage, store, importer, progress) != Status::ok) return nullptr;
    if (storage->retired() || storage->refused()) return nullptr;
    if (!storage->imported() && !storage->import()) {
        if (!reported_) lines_.write("a block of slice memory could not be imported to draw from: its buffers are copied");
        reported_ = true;
        return nullptr;
    }
    storage->stamp();
    return &storage->buffer();
}

Status SliceStorages::attach(const SliceBindings::Binding &binding)
{
    AV_SCOPE("SliceStorages::attach");
    return bindings_.bind(binding);
}

bool SliceStorages::leave(uint32_t resource, SliceBindings::Binding &was)
{
    AV_SCOPE("SliceStorages::leave");
    return bindings_.take(resource, was);
}

bool SliceStorages::bound(uint32_t resource, SliceBindings::Binding &was) const
{
    AV_SCOPE("SliceStorages::bound");
    return bindings_.find(resource, was);
}

void SliceStorages::retire(const SourceMemory &store)
{
    AV_SCOPE("SliceStorages::retire");
    const auto live_in_store = [&store](const SliceStorage &held) { return held.holds_memory(store) && !held.retired(); };
    for (SliceStorage *storage = storages_.find_if(live_in_store); storage != nullptr;
         storage = storages_.find_if(live_in_store)) {
        storage->stamp();
        storage->retire();
        retiring_++;
    }
}

void SliceStorages::destroy_for(const SliceImporter &importer)
{
    AV_SCOPE("SliceStorages::destroy_for");
    uint32_t retired = 0u;
    storages_.destroy_if([&importer, &retired](const SliceStorage &storage) {
        if (!storage.imported_by(importer)) return false;
        retired += storage.retired() ? 1u : 0u;
        return true;
    });
    bindings_.take_all_of(importer);
    retiring_ -= retired;
}

bool SliceStorages::holds(const SourceMemory &store)
{
    AV_SCOPE("SliceStorages::holds");
    return storages_.find_if([&store](const SliceStorage &held) { return held.holds_memory(store); }) != nullptr;
}

void SliceStorages::reclaim()
{
    AV_SCOPE("SliceStorages::reclaim");
    if (retiring_ == 0u) return;
    const uint32_t before = storages_.live();
    storages_.destroy_if([](const SliceStorage &storage) { return storage.done(); });
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

SliceStorage *SliceStorages::find(const SourceMemory &store, const SliceImporter &importer)
{
    AV_SCOPE("SliceStorages::find");
    return storages_.find_if([&store, &importer](const SliceStorage &storage) { return storage.holds(store, importer); });
}

bool SliceStorages::invariants_hold() const
{
    AV_SCOPE("SliceStorages::invariants_hold");
    uint32_t retired = 0u;
    bool sound = true;
    storages_.for_each([&retired, &sound](const SliceStorage &storage) {
        retired += storage.retired() ? 1u : 0u;
        sound = sound && storage.invariants_hold();
    });
    return sound && retired == retiring_ && storages_.invariants_hold() && bindings_.invariants_hold();
}

}
