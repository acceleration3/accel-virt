#include "miniport/memory/shared_object_table.hpp"

#include "miniport/memory/allocation.hpp"
#include "core/platform/mutex_guard.hpp"
#include "core/platform/mutex_try_guard.hpp"
#include "core/platform/invariant_scope.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "proto/resources/shared_object_record.hpp"

namespace av {

SharedObjectTable::SharedObjectTable(Platform &platform)
    : platform_(platform), mutex_(platform, LockRank::shared_objects), objects_(platform), next_id_(1u),
      adapter_(0u)
    {
        AV_SCOPE("SharedObjectTable::SharedObjectTable");
    }

Status SharedObjectTable::initialize()
{
    AV_SCOPE("SharedObjectTable::initialize");
    platform_.require_irql_at_most(Irql::passive, "SharedObjectTable::initialize");
    MutexGuard lock(mutex_);
    InvariantScope<SharedObjectTable> guard(platform_, *this, "initialize");
    if (adapter_ != 0u) return Status::invalid_state;
    uint64_t       frequency = 0u;
    const uint64_t counter = platform_.performance_counter(frequency);
    adapter_ = (counter ^ (frequency << 32u)) | 1u;
    return Status::ok;
}

uint64_t SharedObjectTable::adapter() const
{
    AV_SCOPE("SharedObjectTable::adapter");
    return adapter_;
}

SharedObject *SharedObjectTable::find(uint64_t id)
{
    AV_SCOPE("SharedObjectTable::find");
    return objects_.find_if([id](const SharedObject &object) { return object.id() == id && !object.forgotten(); });
}

Status SharedObjectTable::mint(Allocation &allocation, SharedObjectRecord &record)
{
    AV_SCOPE("SharedObjectTable::mint");
    platform_.require_irql_at_most(Irql::apc, "SharedObjectTable::mint");
    MutexGuard lock(mutex_);
    InvariantScope<SharedObjectTable> guard(platform_, *this, "mint");
    if (adapter_ == 0u) return Status::invalid_state;
    if (record.assigned()) return Status::invalid_parameter;
    SharedObject *object = nullptr;
    const Status  created =
        objects_.create(object, platform_, next_id_, record.kind(), record.size(), allocation);
    if (created != Status::ok) return created;
    record.assign(next_id_, adapter_);
    next_id_++;
    return Status::ok;
}

Status SharedObjectTable::open_minted(uint64_t object_id, SharedObject *&found, SharedObjectRecord &minted)
{
    AV_SCOPE("SharedObjectTable::open_minted");
    platform_.require_irql_at_most(Irql::apc, "SharedObjectTable::open_minted");
    MutexGuard lock(mutex_);
    InvariantScope<SharedObjectTable> guard(platform_, *this, "open_minted");
    found = nullptr;
    if (object_id == 0u) return Status::invalid_parameter;
    SharedObject *const object = find(object_id);
    if (object == nullptr) return Status::not_found;
    const Status opened = object->open();
    if (opened != Status::ok) return opened;
    minted = SharedObjectRecord(object_id, object->kind(), adapter_, object->size());
    found = object;
    return Status::ok;
}

Status SharedObjectTable::open(const SharedObjectRecord &record, SharedObject *&found)
{
    AV_SCOPE("SharedObjectTable::open");
    platform_.require_irql_at_most(Irql::apc, "SharedObjectTable::open");
    found = nullptr;
    MutexGuard lock(mutex_);
    InvariantScope<SharedObjectTable> guard(platform_, *this, "open");
    if (!record.assigned()) return Status::invalid_parameter;
    if (record.adapter_id() != adapter_) return Status::not_found;
    SharedObject *const object = find(record.object_id());
    if (object == nullptr) return Status::not_found;
    if (!object->kind().same_as(record.kind()) || object->size() != record.size())
        return Status::invalid_parameter;
    const Status opened = object->open();
    if (opened != Status::ok) return opened;
    found = object;
    return Status::ok;
}

Status SharedObjectTable::close(SharedObject &object)
{
    AV_SCOPE("SharedObjectTable::close");
    platform_.require_irql_at_most(Irql::apc, "SharedObjectTable::close");
    MutexGuard lock(mutex_);
    InvariantScope<SharedObjectTable> guard(platform_, *this, "close");
    if (find(object.id()) != &object) return Status::invalid_parameter;
    return object.close();
}

void SharedObjectTable::forget(const Allocation &allocation)
{
    AV_SCOPE("SharedObjectTable::forget");
    platform_.require_irql_at_most(Irql::apc, "SharedObjectTable::forget");
    MutexGuard lock(mutex_);
    InvariantScope<SharedObjectTable> guard(platform_, *this, "forget");
    SharedObject *const named =
        objects_.find_if([&allocation](const SharedObject &object) { return object.names(allocation); });
    if (named != nullptr) named->forget();
}

uint32_t SharedObjectTable::take_released(uint64_t *ids, uint32_t capacity)
{
    AV_SCOPE("SharedObjectTable::take_released");
    platform_.require_irql_at_most(Irql::apc, "SharedObjectTable::take_released");
    if (ids == nullptr) return 0u;
    MutexGuard lock(mutex_);
    InvariantScope<SharedObjectTable> guard(platform_, *this, "take_released");
    uint32_t taken = 0u;
    objects_.destroy_if([ids, capacity, &taken](const SharedObject &object) {
        if (!object.forgotten() || taken == capacity) return false;
        ids[taken++] = object.id();
        return true;
    });
    return taken;
}

uint32_t SharedObjectTable::live() const
{
    AV_SCOPE("SharedObjectTable::live");
    uint32_t held = 0u;
    objects_.for_each([&held](const SharedObject &object) {
        if (!object.forgotten()) held++;
    });
    return held;
}

void SharedObjectTable::report(StateWriter &writer)
{
    AV_SCOPE("SharedObjectTable::report");
    platform_.require_irql_at_most(Irql::apc, "SharedObjectTable::report");
    const MutexTryGuard guard(mutex_);
    writer.open("shared_objects", 0u);
    writer.flag("busy", !guard.held());
    if (guard.held()) {
        writer.number("live", live());
        writer.number("released", objects_.live() - live());
        objects_.for_each([&](const SharedObject &object) {
            if (object.forgotten()) return;
            writer.open("object", object.id());
            writer.number("kind", object.kind().value());
            writer.number("size", object.size());
            writer.number("opens", object.opens());
            writer.close();
        });
    }
    writer.close();
}

bool SharedObjectTable::invariants_hold() const
{
    AV_SCOPE("SharedObjectTable::invariants_hold");
    return objects_.all_of([this](const SharedObject &object) {
        if (!object.invariants_hold() || object.id() >= next_id_) return false;
        uint32_t named = 0u;
        objects_.for_each([&](const SharedObject &other) {
            if (other.id() == object.id()) named++;
        });
        return named == 1u;
    });
}

}
