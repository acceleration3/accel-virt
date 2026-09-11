#include "device/abi/table_host.hpp"

#include "proto/context/gpu_identity.hpp"

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

/* The name is cut at the field's end whatever the glue left there. */
GpuIdentity TableHost::gpu_identity(uint32_t index)
{
    RvkGpuProperties properties = {};
    ops_.gpu_properties(ops_.opaque, index, &properties);
    properties.name[sizeof properties.name - 1u] = '\0';
    return GpuIdentity(properties.vendor_id, properties.device_id, properties.device_type, properties.uuid,
                       properties.local_memory, properties.shared_memory, properties.name);
}

void *TableHost::allocate(size_t bytes)
{
    return ops_.allocate(ops_.opaque, bytes);
}

void TableHost::release(void *block)
{
    ops_.release(ops_.opaque, block);
}

void *TableHost::map_guest(uint64_t address, uint64_t &length, bool &direct)
{
    int from_ram = 0;
    void *const pointer = ops_.map_guest(ops_.opaque, address, &length, &from_ram);
    direct = from_ram != 0;
    return pointer;
}

void TableHost::unmap_guest(void *pointer, uint64_t length)
{
    ops_.unmap_guest(ops_.opaque, pointer, length);
}

void *TableHost::create_host_memory(uint64_t size, void *&pointer)
{
    pointer = nullptr;
    return ops_.create_host_memory(ops_.opaque, size, &pointer);
}

void TableHost::destroy_host_memory(void *memory)
{
    ops_.destroy_host_memory(ops_.opaque, memory);
}

uint64_t TableHost::hostmem_size()
{
    return ops_.hostmem_size(ops_.opaque);
}

Status TableHost::place_in_hostmem(void *memory, uint64_t offset)
{
    return ops_.place_in_hostmem(ops_.opaque, memory, offset) == 0 ? Status::ok : Status::no_memory;
}

void TableHost::remove_from_hostmem(uint64_t offset, uint64_t size)
{
    ops_.remove_from_hostmem(ops_.opaque, offset, size);
}

void TableHost::connect_doorbell(void *event, uint32_t index)
{
    ops_.connect_doorbell(ops_.opaque, event, index);
}

void TableHost::disconnect_doorbell(void *event, uint32_t index)
{
    ops_.disconnect_doorbell(ops_.opaque, event, index);
}

size_t TableHost::read_request(void *request, size_t offset, uint8_t *buffer, size_t size)
{
    return ops_.read_request(ops_.opaque, request, offset, buffer, size);
}

void TableHost::respond(void *request, const uint8_t *bytes, size_t size)
{
    ops_.respond(ops_.opaque, request, bytes, size);
}

bool TableHost::resource_exists(uint32_t id)
{
    return ops_.resource_exists(ops_.opaque, id) != 0;
}

}  // namespace av
