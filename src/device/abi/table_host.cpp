#include "device/abi/table_host.hpp"

namespace av {

TableHost::TableHost(const RvkHostOps &ops) : ops_(ops) {}

void TableHost::log(const char *message)
{
    ops_.log(ops_.opaque, message);
}

void TableHost::violated(const char *rule, const char *function)
{
    ops_.log(ops_.opaque, rule);
    ops_.log(ops_.opaque, function);
}

void TableHost::require_main_loop(const char *function)
{
    if (ops_.main_loop_held(ops_.opaque) == 0) violated("virtio-gpu-rvk: main loop required by", function);
}

void TableHost::require_worker(const char *function)
{
    if (ops_.main_loop_held(ops_.opaque) != 0) violated("virtio-gpu-rvk: worker required by", function);
}

Status TableHost::start_thread(HostRoutine entry, void *context, void *&thread)
{
    thread = ops_.start_thread(ops_.opaque, entry, context);
    return thread != nullptr ? Status::ok : Status::no_memory;
}

void TableHost::join_thread(void *thread)
{
    ops_.join_thread(ops_.opaque, thread);
}

Status TableHost::create_event(void *&event)
{
    event = ops_.create_event(ops_.opaque);
    return event != nullptr ? Status::ok : Status::no_memory;
}

void TableHost::destroy_event(void *event)
{
    ops_.destroy_event(ops_.opaque, event);
}

void TableHost::signal_event(void *event)
{
    ops_.signal_event(ops_.opaque, event);
}

void TableHost::wait_event(void *event)
{
    ops_.wait_event(ops_.opaque, event);
}

Status TableHost::create_mutex(void *&mutex)
{
    mutex = ops_.create_mutex(ops_.opaque);
    return mutex != nullptr ? Status::ok : Status::no_memory;
}

void TableHost::destroy_mutex(void *mutex)
{
    ops_.destroy_mutex(ops_.opaque, mutex);
}

void TableHost::lock_mutex(void *mutex)
{
    ops_.lock_mutex(ops_.opaque, mutex);
}

void TableHost::unlock_mutex(void *mutex)
{
    ops_.unlock_mutex(ops_.opaque, mutex);
}

void TableHost::run_on_main_loop(HostRoutine routine, void *context)
{
    ops_.run_on_main_loop(ops_.opaque, routine, context);
}

uint32_t TableHost::gpu_count()
{
    return ops_.gpu_count(ops_.opaque);
}

void TableHost::gpu_uuid(uint32_t index, uint8_t *uuid)
{
    ops_.gpu_uuid(ops_.opaque, index, uuid);
}

}  // namespace av
