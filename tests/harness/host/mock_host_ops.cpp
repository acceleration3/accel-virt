#include "harness/host/mock_host_ops.hpp"

#include "harness/host/mock_host.hpp"
#include "proto/context/gpu_identity.hpp"

#include <cstring>

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
    table_.try_lock_mutex = [](void *opaque, void *mutex) { return host_of(opaque).try_lock_mutex(mutex) ? 1 : 0; };
    table_.run_on_main_loop = [](void *opaque, RvkHostRoutine routine, void *context) {
        host_of(opaque).run_on_main_loop(routine, context);
    };
    table_.gpu_count = [](void *opaque) { return host_of(opaque).gpu_count(); };
    table_.gpu_properties = [](void *opaque, std::uint32_t index, RvkGpuProperties *properties) {
        const GpuIdentity identity = host_of(opaque).gpu_identity(index);
        *properties = RvkGpuProperties{identity.vendor_id(), identity.device_id(), identity.device_type(), {},
                                       identity.local_memory(), identity.shared_memory(), {}};
        std::memcpy(properties->uuid, identity.uuid(), sizeof properties->uuid);
        std::memcpy(properties->name, identity.name(), std::strlen(identity.name()) + 1u);
    };
    table_.allocate = [](void *opaque, std::size_t size) { return host_of(opaque).allocate(size); };
    table_.release = [](void *opaque, void *block) { host_of(opaque).release(block); };
    table_.map_guest = [](void *opaque, std::uint64_t address, std::uint64_t *length, int *direct) {
        bool from_ram = false;
        void *const pointer = host_of(opaque).map_guest(address, *length, from_ram);
        *direct = from_ram ? 1 : 0;
        return pointer;
    };
    table_.unmap_guest = [](void *opaque, void *pointer, std::uint64_t length) { host_of(opaque).unmap_guest(pointer, length); };
    table_.reserve_range = [](void *opaque, std::uint64_t size) -> void * {
        void *base = nullptr;
        return host_of(opaque).reserve_range(size, base) == Status::ok ? base : nullptr;
    };
    table_.alias_guest_run = [](void *opaque, void *run, std::uint64_t size, void *at) {
        return host_of(opaque).alias_guest_run(run, size, at) == Status::ok ? 0 : 1;
    };
    table_.release_range = [](void *opaque, void *base, std::uint64_t size) { host_of(opaque).release_range(base, size); };
    table_.create_host_memory = [](void *opaque, std::uint64_t size, void **pointer) {
        void *view = nullptr;
        void *const memory = host_of(opaque).create_host_memory(size, view);
        *pointer = view;
        return memory;
    };
    table_.destroy_host_memory = [](void *opaque, void *memory) { host_of(opaque).destroy_host_memory(memory); };
    table_.hostmem_size = [](void *opaque) { return host_of(opaque).hostmem_size(); };
    table_.place_in_hostmem = [](void *opaque, void *memory, std::uint64_t offset) {
        return host_of(opaque).place_in_hostmem(memory, offset) == Status::ok ? 0 : 1;
    };
    table_.remove_from_hostmem = [](void *opaque, std::uint64_t offset, std::uint64_t size) {
        host_of(opaque).remove_from_hostmem(offset, size);
    };
    table_.monotonic_nanoseconds = [](void *opaque) { return host_of(opaque).monotonic_nanoseconds(); };
    table_.read_hostmem = [](void *opaque, std::uint64_t offset, std::uint8_t *buffer, std::uint64_t size) {
        return host_of(opaque).read_hostmem(offset, buffer, size) == Status::ok ? 0 : 1;
    };
    table_.bind_gpu = [](void *opaque, std::uint32_t index) {
        return host_of(opaque).bind_gpu(index) == Status::ok ? 0 : 1;
    };
    table_.export_gpu_memory = [](void *opaque, std::uint64_t size, void **pointer, std::int32_t *host_error) {
        return host_of(opaque).export_gpu_memory(size, *pointer, *host_error);
    };
    table_.release_gpu_memory = [](void *opaque, void *memory) { host_of(opaque).release_gpu_memory(memory); };
    table_.import_guest_memory = [](void *opaque, void *pointer, std::uint64_t size, std::int32_t *host_error) {
        return host_of(opaque).import_guest_memory(pointer, size, *host_error);
    };
    table_.release_guest_memory = [](void *opaque, void *memory) { host_of(opaque).release_guest_memory(memory); };
    table_.read_imported_memory = [](void *opaque, void *memory, std::uint64_t offset, std::uint8_t *buffer,
                                     std::uint64_t size) {
        return host_of(opaque).read_imported_memory(memory, offset, buffer, size) == Status::ok ? 0 : 1;
    };
    table_.connect_doorbell = [](void *opaque, void *event, std::uint32_t index) {
        host_of(opaque).connect_doorbell(event, index);
    };
    table_.disconnect_doorbell = [](void *opaque, void *event, std::uint32_t index) {
        host_of(opaque).disconnect_doorbell(event, index);
    };
    table_.connect_fence_interrupts = [](void *opaque) {
        return host_of(opaque).connect_fence_interrupts() == Status::ok ? 0 : 1;
    };
    table_.disconnect_fence_interrupts = [](void *opaque) { host_of(opaque).disconnect_fence_interrupts(); };
    table_.raise_fence_interrupt = [](void *opaque, std::uint32_t line) { host_of(opaque).raise_fence_interrupt(line); };
    table_.read_request = [](void *opaque, void *request, std::size_t offset, std::uint8_t *buffer, std::size_t size) {
        return host_of(opaque).read_request(request, offset, buffer, size);
    };
    table_.respond = [](void *opaque, void *request, const std::uint8_t *bytes, std::size_t size) {
        host_of(opaque).respond(request, bytes, size);
    };
    table_.resource_exists = [](void *opaque, std::uint32_t id) { return host_of(opaque).resource_exists(id) ? 1 : 0; };
}

const RvkHostOps &MockHostOps::table() const
{
    return table_;
}

}  // namespace av
