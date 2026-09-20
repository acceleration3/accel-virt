#include "icd/opencl/cl_object_table.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"

namespace av::clicd {

ClObjectTable::ClObjectTable(BlockSource &memory, const cl_icd_dispatch &table)
    : pool_(memory), table_(table), live_(0u)
{
    AV_SCOPE("ClObjectTable::ClObjectTable");
}

Status ClObjectTable::make(ClObject::Kind kind, uint64_t wire, GuestClContext *context,
                           ClObject *parent, ClObject *&object)
{
    AV_SCOPE("ClObjectTable::make");
    object = nullptr;
    /* Zero is what the host answers when it made nothing, so it never names
     * an object. */
    if (wire == 0u) return Status::invalid_parameter;
    if (parent != nullptr) {
        if (static_cast<uint8_t>(parent->kind()) >= static_cast<uint8_t>(kind)) {
            return Status::invalid_parameter;
        }
        /* A parent this table does not hold would leave a subtree that cannot
         * be destroyed with it. */
        if (pool_.find(parent) == nullptr) return Status::invalid_parameter;
    }
    ClObject *made = nullptr;
    const Status created = pool_.create(made, table_, kind, wire, context, parent);
    if (created != Status::ok) return created;
    object = made;
    ++live_;
    return Status::ok;
}

ClObject *ClObjectTable::made(const void *pointer, ClObject::Kind kind) const
{
    AV_SCOPE("ClObjectTable::made");
    if (pointer == nullptr) return nullptr;
    /* find judges the address against what the pool holds; nothing is read
     * through it before that answers. */
    ClObject *const object = pool_.find(const_cast<void *>(pointer));
    if (object == nullptr) return nullptr;
    return object->kind() == kind ? object : nullptr;
}

void ClObjectTable::destroy(ClObject &object)
{
    AV_SCOPE("ClObjectTable::destroy");
    if (pool_.find(&object) == nullptr) return;
    /* The subtree first, at any depth, then the object itself: a child that
     * outlived its parent would hold a pointer to freed storage. */
    pool_.destroy_if([&object, this](const ClObject &held) {
        const bool below = &held != &object && held.descends_from(object);
        if (below) --live_;
        return below;
    });
    /* The find above is what says it is there, so the destroy cannot refuse;
     * asking again would be asking a question already answered. */
    static_cast<void>(pool_.destroy(&object));
    --live_;
}

bool ClObjectTable::release(ClObject &object)
{
    AV_SCOPE("ClObjectTable::release");
    if (pool_.find(&object) == nullptr) return false;
    if (!object.release()) return true;
    destroy(object);
    return true;
}

uint32_t ClObjectTable::live() const
{
    AV_SCOPE("ClObjectTable::live");
    return live_;
}

uint32_t ClObjectTable::live_of(ClObject::Kind kind) const
{
    AV_SCOPE("ClObjectTable::live_of");
    uint32_t counted = 0u;
    pool_.for_each([&counted, kind](const ClObject &held) {
        if (held.kind() == kind) ++counted;
    });
    return counted;
}

void ClObjectTable::report(StateWriter &writer) const
{
    AV_SCOPE("ClObjectTable::report");
    writer.open("objects", 0u);
    writer.number("live", live_);
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
    uint32_t counted = 0u;
    bool     sound = true;
    pool_.for_each([&counted, &sound, this](const ClObject &held) {
        ++counted;
        if (!held.invariants_hold()) sound = false;
        /* Every parent is still held: a subtree is destroyed with its root,
         * so a live child of a dead parent cannot happen. */
        if (held.parent() != nullptr && pool_.find(held.parent()) == nullptr) sound = false;
    });
    return sound && counted == live_;
}

}  // namespace av::clicd
