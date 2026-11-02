#include "icd/opencl/cl_object_table.hpp"

#include "core/platform/thread_lock_hold.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"

namespace av::clicd {

ClObjectTable::ClObjectTable(BlockSource &memory, const cl_icd_dispatch &table, ThreadLock &lock)
    : memory_(memory), tree_(memory), table_(table), lock_(lock)
{
    AV_SCOPE("ClObjectTable::ClObjectTable");
}

Status ClObjectTable::make(ClObject::Kind kind, uint64_t wire, GuestClContext *context,
                           ClObject *parent, ClObject *&object)
{
    AV_SCOPE("ClObjectTable::make");
    object = nullptr;
    if (wire == 0u) return Status::invalid_parameter;
    const ThreadLockHold held(lock_);
    if (parent != nullptr && !tree_.parent_fits(kind, parent)) return Status::invalid_parameter;
    ClObject    *made = nullptr;
    const Status created = tree_.make(made, table_, memory_, kind, wire, context, parent);
    if (created != Status::ok) return created;
    if (parent != nullptr) parent->adopt();
    object = made;
    return Status::ok;
}

ClObject *ClObjectTable::made(const void *pointer, ClObject::Kind kind) const
{
    AV_SCOPE("ClObjectTable::made");
    const ThreadLockHold held(lock_);
    ClObject *const object = tree_.made(pointer, kind);
    return object != nullptr && object->references() != 0u ? object : nullptr;
}

void ClObjectTable::destroy_held(ClObject &object)
{
    AV_SCOPE("ClObjectTable::destroy_held");
    if (!tree_.holds(&object)) return;
    ClObject *const parent = object.parent();
    static_cast<void>(tree_.destroy(object));
    if (parent != nullptr) parent->orphan();
}

void ClObjectTable::destroy(ClObject &object)
{
    AV_SCOPE("ClObjectTable::destroy");
    const ThreadLockHold held(lock_);
    destroy_held(object);
}

bool ClObjectTable::release(ClObject &object, GuestClContext *&freed)
{
    AV_SCOPE("ClObjectTable::release");
    uint32_t count = 0u;
    return release(object, freed, nullptr, 0u, count);
}

bool ClObjectTable::release(ClObject &object, GuestClContext *&freed, const void **gone, uint32_t room,
                            uint32_t &count)
{
    AV_SCOPE("ClObjectTable::release");
    freed = nullptr;
    count = 0u;
    const ThreadLockHold held(lock_);
    if (!tree_.holds(&object) || object.references() == 0u) return false;
    if (!object.release()) return true;
    for (ClObject *at = &object; at != nullptr && at->unheld();) {
        ClObject *const parent = at->parent();
        if (at->kind() == ClObject::Kind::context) freed = at->context();
        if (count < room) gone[count++] = at;
        destroy_held(*at);
        at = parent;
    }
    return true;
}

uint32_t ClObjectTable::live() const
{
    AV_SCOPE("ClObjectTable::live");
    const ThreadLockHold held(lock_);
    return tree_.live();
}

uint32_t ClObjectTable::live_of(ClObject::Kind kind) const
{
    AV_SCOPE("ClObjectTable::live_of");
    const ThreadLockHold held(lock_);
    uint32_t counted = 0u;
    tree_.nodes().for_each([&counted, kind](const ClObject &object) {
        if (object.kind() == kind) ++counted;
    });
    return counted;
}

bool ClObjectTable::queues_idle(const ClObject *context) const
{
    AV_SCOPE("ClObjectTable::queues_idle");
    if (context == nullptr) return true;
    const ThreadLockHold held(lock_);
    bool idle = true;
    tree_.for_children(*context, [&idle](const ClObject &object) {
        if (object.kind() == ClObject::Kind::command_queue && !object.known_idle()) idle = false;
    });
    return idle;
}

void ClObjectTable::report(StateWriter &writer) const
{
    AV_SCOPE("ClObjectTable::report");
    writer.open("objects", 0u);
    writer.number("live", live());
    writer.number("contexts", live_of(ClObject::Kind::context));
    writer.number("queues", live_of(ClObject::Kind::command_queue));
    writer.number("memory", live_of(ClObject::Kind::mem));
    writer.number("kernels", live_of(ClObject::Kind::kernel));
    writer.number("events", live_of(ClObject::Kind::event));
    writer.close();
}

bool ClObjectTable::invariants_hold() const
{
    AV_SCOPE("ClObjectTable::invariants_hold");
    const ThreadLockHold held(lock_);
    if (!tree_.invariants_hold()) return false;
    return tree_.nodes().all_of([](const ClObject &object) { return object.invariants_hold(); });
}

}
