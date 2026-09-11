#include "harness/host/mock_host_ops.hpp"

#include "harness/host/mock_host.hpp"

namespace av {

namespace {

MockHost &host_of(void *opaque) { return *static_cast<MockHost *>(opaque); }

void *created(Status status, void *handle) { return status == Status::ok ? handle : nullptr; }

}  // namespace

MockHostOps::MockHostOps(MockHost &host) : table_{}
{
    table_.opaque = &host;
    table_.log = [](void *opaque, const char *message) { host_of(opaque).log(message); };
    table_.main_loop_held = [](void *opaque) { return host_of(opaque).holds_main_loop() ? 1 : 0; };
    table_.start_thread = [](void *opaque, RvkHostRoutine entry, void *context) {
        void *thread = nullptr;
        return created(host_of(opaque).start_thread(entry, context, thread), thread);
    };
    table_.join_thread = [](void *opaque, void *thread) { host_of(opaque).join_thread(thread); };
    table_.create_event = [](void *opaque) {
        void *event = nullptr;
        return created(host_of(opaque).create_event(event), event);
    };
    table_.destroy_event = [](void *opaque, void *event) { host_of(opaque).destroy_event(event); };
    table_.signal_event = [](void *opaque, void *event) { host_of(opaque).signal_event(event); };
    table_.wait_event = [](void *opaque, void *event) { host_of(opaque).wait_event(event); };
    table_.create_mutex = [](void *opaque) {
        void *mutex = nullptr;
        return created(host_of(opaque).create_mutex(mutex), mutex);
    };
    table_.destroy_mutex = [](void *opaque, void *mutex) { host_of(opaque).destroy_mutex(mutex); };
    table_.lock_mutex = [](void *opaque, void *mutex) { host_of(opaque).lock_mutex(mutex); };
    table_.unlock_mutex = [](void *opaque, void *mutex) { host_of(opaque).unlock_mutex(mutex); };
    table_.run_on_main_loop = [](void *opaque, RvkHostRoutine routine, void *context) {
        host_of(opaque).run_on_main_loop(routine, context);
    };
    table_.gpu_count = [](void *opaque) { return host_of(opaque).gpu_count(); };
    table_.gpu_uuid = [](void *opaque, std::uint32_t index, std::uint8_t *uuid) { host_of(opaque).gpu_uuid(index, uuid); };
}

const RvkHostOps &MockHostOps::table() const
{
    return table_;
}

}  // namespace av
