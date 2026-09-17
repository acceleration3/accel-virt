#include "device/abi/table_host.hpp"

#include "core/platform/lock_marks.hpp"
#include "core/trace/function_scope.hpp"
#include "proto/context/gpu_identity.hpp"

namespace av {

TableHost::TableHost(const RvkHostOps &ops) : ops_(ops)
{
    AV_SCOPE("TableHost::TableHost");
}

void TableHost::log(const char *message)
{
    AV_SCOPE("TableHost::log");
    ops_.log(ops_.opaque, message);
}

void TableHost::violated(const char *rule, const char *function)
{
    AV_SCOPE("TableHost::violated");
    ops_.log(ops_.opaque, rule);
    ops_.log(ops_.opaque, function);
}

void TableHost::require_main_loop(const char *function)
{
    AV_SCOPE("TableHost::require_main_loop");
    if (ops_.main_loop_held(ops_.opaque) == 0) violated("virtio-gpu-rvk: main loop required by", function);
}

void TableHost::require_worker(const char *function)
{
    AV_SCOPE("TableHost::require_worker");
    if (ops_.main_loop_held(ops_.opaque) != 0) violated("virtio-gpu-rvk: worker required by", function);
}

Status TableHost::start_thread(HostRoutine entry, void *context, void *&thread)
{
    AV_SCOPE("TableHost::start_thread");
    thread = ops_.start_thread(ops_.opaque, entry, context);
    return thread != nullptr ? Status::ok : Status::no_memory;
}

void TableHost::join_thread(void *thread)
{
    AV_SCOPE("TableHost::join_thread");
    ops_.join_thread(ops_.opaque, thread);
}

Status TableHost::create_event(void *&event)
{
    AV_SCOPE("TableHost::create_event");
    event = ops_.create_event(ops_.opaque);
    return event != nullptr ? Status::ok : Status::no_memory;
}

void TableHost::destroy_event(void *event)
{
    AV_SCOPE("TableHost::destroy_event");
    ops_.destroy_event(ops_.opaque, event);
}

void TableHost::signal_event(void *event)
{
    AV_SCOPE("TableHost::signal_event");
    ops_.signal_event(ops_.opaque, event);
}

void TableHost::wait_event(void *event)
{
    AV_SCOPE("TableHost::wait_event");
    ops_.wait_event(ops_.opaque, event);
}

Status TableHost::create_mutex(void *&mutex)
{
    AV_SCOPE("TableHost::create_mutex");
    mutex = ops_.create_mutex(ops_.opaque);
    return mutex != nullptr ? Status::ok : Status::no_memory;
}

void TableHost::destroy_mutex(void *mutex)
{
    AV_SCOPE("TableHost::destroy_mutex");
    ops_.destroy_mutex(ops_.opaque, mutex);
}

void TableHost::lock_mutex(void *mutex)
{
    AV_SCOPE("TableHost::lock_mutex");
    const LockMarks marks(mutex);
    marks.waiting();
    ops_.lock_mutex(ops_.opaque, mutex);
    marks.acquired();
}

bool TableHost::try_lock_mutex(void *mutex)
{
    AV_SCOPE("TableHost::try_lock_mutex");
    if (ops_.try_lock_mutex(ops_.opaque, mutex) == 0) return false;
    LockMarks(mutex).acquired();
    return true;
}

void TableHost::unlock_mutex(void *mutex)
{
    AV_SCOPE("TableHost::unlock_mutex");
    LockMarks(mutex).released();
    ops_.unlock_mutex(ops_.opaque, mutex);
}

void TableHost::run_on_main_loop(HostRoutine routine, void *context)
{
    AV_SCOPE("TableHost::run_on_main_loop");
    ops_.run_on_main_loop(ops_.opaque, routine, context);
}

uint32_t TableHost::gpu_count()
{
    AV_SCOPE("TableHost::gpu_count");
    return ops_.gpu_count(ops_.opaque);
}

/* The name is cut at the field's end whatever the glue left there. */
GpuIdentity TableHost::gpu_identity(uint32_t index)
{
    AV_SCOPE("TableHost::gpu_identity");
    RvkGpuProperties properties = {};
    ops_.gpu_properties(ops_.opaque, index, &properties);
    properties.name[sizeof properties.name - 1u] = '\0';
    return GpuIdentity(properties.vendor_id, properties.device_id, properties.device_type, properties.uuid,
                       properties.local_memory, properties.shared_memory, properties.name);
}

void *TableHost::allocate(size_t bytes)
{
    AV_SCOPE("TableHost::allocate");
    return ops_.allocate(ops_.opaque, bytes);
}

void TableHost::release(void *block)
{
    AV_SCOPE("TableHost::release");
    ops_.release(ops_.opaque, block);
}

void *TableHost::map_guest(uint64_t address, uint64_t &length, bool &direct)
{
    AV_SCOPE("TableHost::map_guest");
    int from_ram = 0;
    void *const pointer = ops_.map_guest(ops_.opaque, address, &length, &from_ram);
    direct = from_ram != 0;
    return pointer;
}

void TableHost::unmap_guest(void *pointer, uint64_t length)
{
    AV_SCOPE("TableHost::unmap_guest");
    ops_.unmap_guest(ops_.opaque, pointer, length);
}

Status TableHost::reserve_range(uint64_t bytes, void *&base)
{
    AV_SCOPE("TableHost::reserve_range");
    base = ops_.reserve_range(ops_.opaque, bytes);
    return base != nullptr ? Status::ok : Status::no_memory;
}

Status TableHost::alias_guest_run(void *run, uint64_t bytes, void *at)
{
    AV_SCOPE("TableHost::alias_guest_run");
    return ops_.alias_guest_run(ops_.opaque, run, bytes, at) == 0 ? Status::ok : Status::not_supported;
}

void TableHost::release_range(void *base, uint64_t bytes)
{
    AV_SCOPE("TableHost::release_range");
    ops_.release_range(ops_.opaque, base, bytes);
}

void *TableHost::create_host_memory(uint64_t size, void *&pointer)
{
    AV_SCOPE("TableHost::create_host_memory");
    pointer = nullptr;
    return ops_.create_host_memory(ops_.opaque, size, &pointer);
}

void TableHost::destroy_host_memory(void *memory)
{
    AV_SCOPE("TableHost::destroy_host_memory");
    ops_.destroy_host_memory(ops_.opaque, memory);
}

uint64_t TableHost::hostmem_size()
{
    AV_SCOPE("TableHost::hostmem_size");
    return ops_.hostmem_size(ops_.opaque);
}

Status TableHost::place_in_hostmem(void *memory, uint64_t offset)
{
    AV_SCOPE("TableHost::place_in_hostmem");
    return ops_.place_in_hostmem(ops_.opaque, memory, offset) == 0 ? Status::ok : Status::no_memory;
}

void TableHost::remove_from_hostmem(uint64_t offset, uint64_t size)
{
    AV_SCOPE("TableHost::remove_from_hostmem");
    ops_.remove_from_hostmem(ops_.opaque, offset, size);
}

uint64_t TableHost::monotonic_nanoseconds()
{
    AV_SCOPE("TableHost::monotonic_nanoseconds");
    return ops_.monotonic_nanoseconds(ops_.opaque);
}

Status TableHost::read_hostmem(uint64_t offset, uint8_t *buffer, uint64_t size)
{
    AV_SCOPE("TableHost::read_hostmem");
    return ops_.read_hostmem(ops_.opaque, offset, buffer, size) == 0 ? Status::ok : Status::not_supported;
}

Status TableHost::bind_gpu(uint32_t index)
{
    AV_SCOPE("TableHost::bind_gpu");
    return ops_.bind_gpu(ops_.opaque, index) == 0 ? Status::ok : Status::not_supported;
}

void *TableHost::export_gpu_memory(uint64_t size, void *&pointer, int32_t &host_error)
{
    AV_SCOPE("TableHost::export_gpu_memory");
    pointer = nullptr;
    host_error = 0;
    return ops_.export_gpu_memory(ops_.opaque, size, &pointer, &host_error);
}

void TableHost::release_gpu_memory(void *memory)
{
    AV_SCOPE("TableHost::release_gpu_memory");
    ops_.release_gpu_memory(ops_.opaque, memory);
}

void *TableHost::import_guest_memory(void *pointer, uint64_t size, int32_t &host_error)
{
    AV_SCOPE("TableHost::import_guest_memory");
    host_error = 0;
    return ops_.import_guest_memory(ops_.opaque, pointer, size, &host_error);
}

void TableHost::release_guest_memory(void *memory)
{
    AV_SCOPE("TableHost::release_guest_memory");
    ops_.release_guest_memory(ops_.opaque, memory);
}

Status TableHost::read_imported_memory(void *memory, uint64_t offset, uint8_t *buffer, uint64_t size)
{
    AV_SCOPE("TableHost::read_imported_memory");
    return ops_.read_imported_memory(ops_.opaque, memory, offset, buffer, size) == 0 ? Status::ok
                                                                                     : Status::not_supported;
}

void TableHost::connect_doorbell(void *event, uint32_t index)
{
    AV_SCOPE("TableHost::connect_doorbell");
    ops_.connect_doorbell(ops_.opaque, event, index);
}

void TableHost::disconnect_doorbell(void *event, uint32_t index)
{
    AV_SCOPE("TableHost::disconnect_doorbell");
    ops_.disconnect_doorbell(ops_.opaque, event, index);
}

Status TableHost::connect_fence_interrupts()
{
    AV_SCOPE("TableHost::connect_fence_interrupts");
    return ops_.connect_fence_interrupts(ops_.opaque) == 0 ? Status::ok : Status::no_memory;
}

void TableHost::disconnect_fence_interrupts()
{
    AV_SCOPE("TableHost::disconnect_fence_interrupts");
    ops_.disconnect_fence_interrupts(ops_.opaque);
}

void TableHost::raise_fence_interrupt(uint32_t line)
{
    AV_SCOPE("TableHost::raise_fence_interrupt");
    ops_.raise_fence_interrupt(ops_.opaque, line);
}

size_t TableHost::read_request(void *request, size_t offset, uint8_t *buffer, size_t size)
{
    AV_SCOPE("TableHost::read_request");
    return ops_.read_request(ops_.opaque, request, offset, buffer, size);
}

void TableHost::respond(void *request, const uint8_t *bytes, size_t size)
{
    AV_SCOPE("TableHost::respond");
    ops_.respond(ops_.opaque, request, bytes, size);
}

bool TableHost::resource_exists(uint32_t id)
{
    AV_SCOPE("TableHost::resource_exists");
    return ops_.resource_exists(ops_.opaque, id) != 0;
}

}  // namespace av
