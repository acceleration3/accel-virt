#include "harness/host/mock_host_ops.hpp"

#include "harness/host/mock_host.hpp"
#include "proto/context/gpu_identity.hpp"

#include <cstring>

namespace av {

namespace {

MockHost &host_of(void *opaque) { return *static_cast<MockHost *>(opaque); }

void *created(Status status, void *handle) { return status == Status::ok ? handle : nullptr; }

}

MockHostOps::MockHostOps(MockHost &host) : table_{}
{
    table_.opaque = &host;
    table_.log = [](void *opaque, const char *message) { host_of(opaque).log(message); };
    table_.main_loop_held = [](void *opaque) { return host_of(opaque).holds_main_loop() ? 1 : 0; };
    table_.start_thread = [](void *opaque, AvHostRoutine entry, void *context) {
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
    table_.wait_event_until = [](void *opaque, void *event, std::uint64_t deadline) {
        return host_of(opaque).wait_event_until(event, deadline) ? 1 : 0;
    };
    table_.show_image = [](void *opaque, std::uint32_t head, const AvExportedImage *image) {
        return host_of(opaque).show_image(head, *image) == Status::ok ? 0 : 1;
    };
    table_.create_mutex = [](void *opaque) {
        void *mutex = nullptr;
        return created(host_of(opaque).create_mutex(mutex), mutex);
    };
    table_.destroy_mutex = [](void *opaque, void *mutex) { host_of(opaque).destroy_mutex(mutex); };
    table_.lock_mutex = [](void *opaque, void *mutex) { host_of(opaque).lock_mutex(mutex); };
    table_.unlock_mutex = [](void *opaque, void *mutex) { host_of(opaque).unlock_mutex(mutex); };
    table_.try_lock_mutex = [](void *opaque, void *mutex) { return host_of(opaque).try_lock_mutex(mutex) ? 1 : 0; };
    table_.run_on_main_loop = [](void *opaque, AvHostRoutine routine, void *context) {
        host_of(opaque).run_on_main_loop(routine, context);
    };
    table_.gpu_count = [](void *opaque) { return host_of(opaque).gpu_count(); };
    table_.gpu_properties = [](void *opaque, std::uint32_t index, AvGpuProperties *properties) {
        const GpuIdentity identity = host_of(opaque).gpu_identity(index);
        *properties = AvGpuProperties{identity.vendor_id(), identity.device_id(), identity.device_type(), {},
                                       identity.local_memory(), identity.shared_memory(), {}, 0u, 0u, 0u, 0u, 0u};
        std::memcpy(properties->uuid, identity.uuid(), sizeof properties->uuid);
        std::memcpy(properties->name, identity.name(), std::strlen(identity.name()) + 1u);
        MockHost::PciBus bus = {};
        if (!host_of(opaque).gpu_bus(index, bus)) return;
        properties->pci_domain = bus.domain;
        properties->pci_bus = static_cast<std::uint8_t>(bus.bus);
        properties->pci_device = static_cast<std::uint8_t>(bus.device);
        properties->pci_function = static_cast<std::uint8_t>(bus.function);
        properties->on_pci = 1u;
    };
    table_.read_file = [](void *opaque, const char *path, char *buffer, std::size_t size, std::size_t *length) {
        return host_of(opaque).read_file(path, buffer, size, *length) == Status::ok ? 0 : 1;
    };
    table_.list_directory = [](void *opaque, const char *path, char *buffer, std::size_t size, std::size_t *length) {
        return host_of(opaque).list_directory(path, buffer, size, *length) == Status::ok ? 0 : 1;
    };
    table_.allocate = [](void *opaque, std::size_t size) { return host_of(opaque).allocate(size, av::Pool::paged); };
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
    table_.overlay_guest_run = [](void *opaque, void *run, std::uint64_t size, int fd, std::uint64_t offset) {
        return host_of(opaque).overlay_guest_run(run, size, fd, offset) == Status::ok ? 0 : 1;
    };
    table_.restore_guest_run = [](void *opaque, void *run, std::uint64_t size) {
        return host_of(opaque).restore_guest_run(run, size) == Status::ok ? 0 : 1;
    };
    table_.hostmem_size = [](void *opaque) { return host_of(opaque).hostmem_size(); };
    table_.place_in_hostmem = [](void *opaque, int fd, std::uint64_t size, std::uint64_t offset) {
        return host_of(opaque).place_in_hostmem(fd, size, offset) == Status::ok ? 0 : 1;
    };
    table_.remove_from_hostmem = [](void *opaque, std::uint64_t offset, std::uint64_t size) {
        host_of(opaque).remove_from_hostmem(offset, size);
    };
    table_.monotonic_nanoseconds = [](void *opaque) { return host_of(opaque).monotonic_nanoseconds(); };
    table_.bind_gpu = [](void *opaque, std::uint32_t index) {
        return host_of(opaque).bind_gpu(index) == Status::ok ? 0 : 1;
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
    table_.create_recording = [](void *opaque, const char *path) -> void * {
        void *file = nullptr;
        return host_of(opaque).create_recording(path, file) == Status::ok ? file : nullptr;
    };
    table_.append_recording = [](void *opaque, void *file, const std::uint8_t *bytes, std::size_t size) {
        return host_of(opaque).append_recording(file, bytes, size) == Status::ok ? 0 : 1;
    };
    table_.truncate_recording = [](void *opaque, void *file) {
        return host_of(opaque).truncate_recording(file) == Status::ok ? 0 : 1;
    };
    table_.close_recording = [](void *opaque, void *file) { host_of(opaque).close_recording(file); };
    table_.display_takes_dmabuf = [](void *opaque, std::uint32_t head) {
        return host_of(opaque).display_takes_dmabuf(head) ? 1 : 0;
    };
    table_.show_scanout = [](void *opaque, std::uint32_t head, const void *blob, const AvScanoutFrame *frame) {
        if (frame == nullptr || frame->reserved != 0u || frame->modifier != 0u) return 1;
        const MockHost::Plane plane = {frame->width,          frame->height,  frame->x,      frame->y,
                                       frame->backing_width,  frame->backing_height,
                                       frame->stride,         frame->offset,  frame->fourcc};
        return host_of(opaque).show_scanout_plane(head, static_cast<const std::uint8_t *>(blob), plane) == Status::ok ? 0 : 1;
    };
    table_.update_scanout = [](void *opaque, std::uint32_t head, std::uint32_t x, std::uint32_t y,
                               std::uint32_t width, std::uint32_t height) {
        host_of(opaque).update_scanout(head, x, y, width, height);
    };
    table_.hide_scanout = [](void *opaque, std::uint32_t head) { host_of(opaque).hide_scanout(head); };
    table_.event_descriptor = [](void *opaque, void *event) { return host_of(opaque).event_descriptor(event); };
    table_.fence_interrupt_descriptor = [](void *opaque, std::uint32_t line) {
        return host_of(opaque).fence_interrupt_descriptor(line);
    };
    table_.guest_region_count = [](void *opaque) { return host_of(opaque).guest_region_count(); };
    table_.guest_region = [](void *opaque, std::uint32_t index, std::uint32_t *region, int *descriptor,
                             std::uint64_t *size) {
        return host_of(opaque).guest_region(index, *region, *descriptor, *size) == Status::ok ? 0 : 1;
    };
    table_.locate_guest = [](void *opaque, const void *pointer, std::uint64_t length, std::uint32_t *region,
                             std::uint64_t *offset) {
        return host_of(opaque).locate_guest(pointer, length, *region, *offset) == Status::ok ? 0 : 1;
    };
    table_.close_descriptor = [](void *opaque, int descriptor) { host_of(opaque).close_descriptor(descriptor); };
    table_.start_renderer = [](void *opaque, const char *path) {
        return host_of(opaque).start_renderer(path) == Status::ok ? 0 : 1;
    };
    table_.send_renderer = [](void *opaque, const std::uint8_t *bytes, std::size_t length, const int *descriptors,
                              std::uint32_t count) {
        return host_of(opaque).send_renderer(bytes, length, descriptors, count) == Status::ok ? 0 : 1;
    };
    table_.receive_renderer = [](void *opaque, std::uint8_t *buffer, std::size_t capacity, std::size_t *length,
                                 int *descriptors, std::uint32_t capacity_descriptors, std::uint32_t *count,
                                 std::uint32_t timeout_ms) {
        const Status received = host_of(opaque).receive_renderer(buffer, capacity, *length, descriptors,
                                                                 capacity_descriptors, *count, timeout_ms);
        if (received == Status::ok) return 0;
        return received == Status::invalid_state ? 1 : 2;
    };
    table_.kill_renderer = [](void *opaque) { host_of(opaque).kill_renderer(); };
    table_.receive_export = [](void *opaque, int connection, std::uint8_t *buffer, std::size_t capacity,
                               std::size_t *length) {
        return host_of(opaque).receive_export(connection, buffer, capacity, *length) == Status::ok ? 0 : 1;
    };
    table_.send_export = [](void *opaque, int connection, const std::uint8_t *bytes, std::size_t length,
                            int descriptor) {
        return host_of(opaque).send_export(connection, bytes, length, descriptor) == Status::ok ? 0 : 1;
    };
    table_.shutdown_export = [](void *opaque, int connection) { host_of(opaque).shutdown_export(connection); };
    table_.close_export = [](void *opaque, int descriptor) { host_of(opaque).close_export(descriptor); };
    table_.duplicate_export = [](void *opaque, int descriptor) { return host_of(opaque).duplicate_export(descriptor); };
}

const AvHostOps &MockHostOps::table() const
{
    return table_;
}

}
