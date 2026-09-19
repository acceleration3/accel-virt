#include "device/memory/host_shared_objects.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "device/host/host.hpp"

namespace av {

HostSharedObjects::HostSharedObjects(Host &host) : host_(host), lock_(nullptr), objects_{}
{
    AV_SCOPE("HostSharedObjects::HostSharedObjects");
}

HostSharedObjects::~HostSharedObjects()
{
    AV_SCOPE("HostSharedObjects::~HostSharedObjects");
    for (HostSharedObject &object : objects_) object.give_back(host_);
    if (lock_ != nullptr) host_.destroy_mutex(lock_);
}

Status HostSharedObjects::initialize()
{
    AV_SCOPE("HostSharedObjects::initialize");
    host_.require_main_loop("HostSharedObjects::initialize");
    if (lock_ != nullptr) return Status::invalid_state;
    return host_.create_mutex(lock_) == Status::ok ? Status::ok : Status::no_memory;
}

HostSharedObject *HostSharedObjects::slot_of(uint64_t object_id)
{
    AV_SCOPE("HostSharedObjects::slot_of");
    for (HostSharedObject &object : objects_)
        if (object.holds(object_id)) return &object;
    return nullptr;
}

Status HostSharedObjects::publish(uint64_t object_id, uint32_t kind, uint32_t handle_kind, uint64_t size,
                                  int descriptor)
{
    AV_SCOPE("HostSharedObjects::publish");
    if (lock_ == nullptr) return Status::invalid_state;
    host_.lock_mutex(lock_);
    ScopeExit unlock([this] { host_.unlock_mutex(lock_); });
    ScopeExit close_refused([this, descriptor] { host_.close_dmabuf(descriptor); });
    if (slot_of(object_id) != nullptr) return Status::conflict;
    for (HostSharedObject &object : objects_) {
        if (object.live()) continue;
        const Status taken = object.take(object_id, kind, handle_kind, size, descriptor);
        if (taken == Status::ok) close_refused.dismiss();
        return taken;
    }
    return Status::no_space;
}

Status HostSharedObjects::open(uint64_t object_id, uint32_t kind, uint64_t claimed_size,
                               uint32_t &handle_kind, uint64_t &size, int &descriptor)
{
    AV_SCOPE("HostSharedObjects::open");
    handle_kind = 0u;
    size = 0u;
    descriptor = -1;
    if (lock_ == nullptr) return Status::invalid_state;
    host_.lock_mutex(lock_);
    ScopeExit unlock([this] { host_.unlock_mutex(lock_); });
    HostSharedObject *object = slot_of(object_id);
    if (object == nullptr) return Status::not_found;
    if (object->kind() != kind || object->size() != claimed_size) return Status::invalid_parameter;
    object->hold();
    handle_kind = object->handle_kind();
    size = object->size();
    descriptor = object->descriptor();
    return Status::ok;
}

void HostSharedObjects::close(uint64_t object_id)
{
    AV_SCOPE("HostSharedObjects::close");
    if (lock_ == nullptr) return;
    host_.lock_mutex(lock_);
    ScopeExit unlock([this] { host_.unlock_mutex(lock_); });
    HostSharedObject *object = slot_of(object_id);
    if (object != nullptr) (void)object->drop(host_);
}

void HostSharedObjects::release_all()
{
    AV_SCOPE("HostSharedObjects::release_all");
    if (lock_ == nullptr) return;
    host_.lock_mutex(lock_);
    ScopeExit unlock([this] { host_.unlock_mutex(lock_); });
    for (HostSharedObject &object : objects_) object.give_back(host_);
}

uint32_t HostSharedObjects::live() const
{
    AV_SCOPE("HostSharedObjects::live");
    uint32_t count = 0u;
    for (const HostSharedObject &object : objects_)
        if (object.live()) count++;
    return count;
}

void HostSharedObjects::report(StateWriter &writer)
{
    AV_SCOPE("HostSharedObjects::report");
    writer.open("shared", 0u);
    if (lock_ == nullptr || !host_.try_lock_mutex(lock_)) {
        /* Before initialize there is nothing to say, and while a worker holds
         * it a dump must not wait on that worker. */
        writer.flag("busy", lock_ != nullptr);
        writer.close();
        return;
    }
    ScopeExit unlock([this] { host_.unlock_mutex(lock_); });
    writer.number("live", live());
    for (const HostSharedObject &object : objects_) {
        if (!object.live()) continue;
        writer.open("object", object.id());
        writer.number("kind", object.kind());
        writer.number("handle_kind", object.handle_kind());
        writer.number("size", object.size());
        writer.number("holders", object.holders());
        writer.close();
    }
    writer.close();
}

bool HostSharedObjects::invariants_hold() const
{
    AV_SCOPE("HostSharedObjects::invariants_hold");
    for (uint32_t index = 0u; index < max_objects; index++) {
        if (!objects_[index].invariants_hold()) return false;
        if (!objects_[index].live()) continue;
        for (uint32_t other = index + 1u; other < max_objects; other++)
            if (objects_[other].holds(objects_[index].id())) return false;
    }
    return true;
}

}  // namespace av
