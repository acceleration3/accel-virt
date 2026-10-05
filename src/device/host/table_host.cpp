#include "device/host/table_host.hpp"

#include "core/platform/lock_marks.hpp"
#include "core/trace/function_scope.hpp"
#include "device/display/scanout_framebuffer.hpp"
#include "device/gpu/pci_address.hpp"
#include "proto/context/gpu_identity.hpp"

namespace av {

namespace {

/* DRM_FORMAT_MOD_LINEAR: a scanout of guest pages is always linear, and the
 * tiled case arrives with the executor's image export (feature 058,
 * contracts/executor_scanout_export.md). */
constexpr uint64_t drm_format_mod_linear = 0u;

/* What the table's receive_renderer answers. */
constexpr int receive_ok = 0;
constexpr int receive_timed_out = 1;

}  // namespace

TableHost::TableHost(const AvHostOps &ops) : ops_(ops)
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
    if (ops_.main_loop_held(ops_.opaque) == 0) violated("virtio-gpu-av: main loop required by", function);
}

void TableHost::require_worker(const char *function)
{
    AV_SCOPE("TableHost::require_worker");
    if (ops_.main_loop_held(ops_.opaque) != 0) violated("virtio-gpu-av: worker required by", function);
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

bool TableHost::wait_event_until(void *event, uint64_t deadline)
{
    AV_SCOPE("TableHost::wait_event_until");
    return ops_.wait_event_until(ops_.opaque, event, deadline) != 0;
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
    AvGpuProperties properties = {};
    ops_.gpu_properties(ops_.opaque, index, &properties);
    properties.name[sizeof properties.name - 1u] = '\0';
    return GpuIdentity(properties.vendor_id, properties.device_id, properties.device_type, properties.uuid,
                       properties.local_memory, properties.shared_memory, properties.name);
}

Status TableHost::gpu_pci_address(uint32_t index, PciAddress &address)
{
    AV_SCOPE("TableHost::gpu_pci_address");
    AvGpuProperties properties = {};
    ops_.gpu_properties(ops_.opaque, index, &properties);
    if (properties.on_pci == 0u) return Status::not_found;
    address = PciAddress(properties.pci_domain, properties.pci_bus, properties.pci_device, properties.pci_function);
    return Status::ok;
}

Status TableHost::read_file(const char *path, char *buffer, size_t capacity, size_t &length)
{
    AV_SCOPE("TableHost::read_file");
    length = 0u;
    if (ops_.read_file == nullptr) return Status::not_found;
    size_t copied = 0u;
    if (ops_.read_file(ops_.opaque, path, buffer, capacity, &copied) != 0 || copied > capacity)
        return Status::not_found;
    length = copied;
    return Status::ok;
}

Status TableHost::list_directory(const char *path, char *buffer, size_t capacity, size_t &length)
{
    AV_SCOPE("TableHost::list_directory");
    length = 0u;
    if (ops_.list_directory == nullptr) return Status::not_found;
    size_t listed = 0u;
    if (ops_.list_directory(ops_.opaque, path, buffer, capacity, &listed) != 0 || listed > capacity)
        return Status::not_found;
    length = listed;
    return Status::ok;
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

Status TableHost::dmabuf_over_guest(void *const *runs, const uint64_t *sizes, uint32_t count,
                                    int &fd)
{
    AV_SCOPE("TableHost::dmabuf_over_guest");
    /* A table from before this op has nothing there; the caller imports a
     * host pointer instead, as it did before the op existed. */
    if (ops_.dmabuf_over_guest == nullptr) return Status::not_supported;
    return ops_.dmabuf_over_guest(ops_.opaque, runs, sizes, count, &fd) == 0 ? Status::ok
                                                                            : Status::not_supported;
}

void TableHost::close_dmabuf(int fd)
{
    AV_SCOPE("TableHost::close_dmabuf");
    if (ops_.close_dmabuf != nullptr) ops_.close_dmabuf(ops_.opaque, fd);
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

Status TableHost::create_recording(const char *path, void *&file)
{
    AV_SCOPE("TableHost::create_recording");
    file = ops_.create_recording(ops_.opaque, path);
    return file != nullptr ? Status::ok : Status::no_memory;
}

Status TableHost::append_recording(void *file, const uint8_t *bytes, size_t size)
{
    AV_SCOPE("TableHost::append_recording");
    return ops_.append_recording(ops_.opaque, file, bytes, size) == 0 ? Status::ok : Status::no_space;
}

Status TableHost::truncate_recording(void *file)
{
    AV_SCOPE("TableHost::truncate_recording");
    return ops_.truncate_recording(ops_.opaque, file) == 0 ? Status::ok : Status::no_space;
}

void TableHost::close_recording(void *file)
{
    AV_SCOPE("TableHost::close_recording");
    ops_.close_recording(ops_.opaque, file);
}

/* A table from before these four shows nothing, which is the same answer a
 * host with no GL display gives, and the guest then keeps the 2D path. */
bool TableHost::display_takes_dmabuf(uint32_t head)
{
    AV_SCOPE("TableHost::display_takes_dmabuf");
    if (ops_.display_takes_dmabuf == nullptr) return false;
    return ops_.display_takes_dmabuf(ops_.opaque, head) != 0;
}

Status TableHost::show_scanout(uint32_t head, int fd, const ScanoutFramebuffer &frame)
{
    AV_SCOPE("TableHost::show_scanout");
    if (ops_.show_scanout == nullptr) return Status::not_supported;
    const AvScanoutFrame plane = {frame.rect().width(),
                                   frame.rect().height(),
                                   frame.rect().x(),
                                   frame.rect().y(),
                                   frame.width(),
                                   frame.height(),
                                   frame.stride(),
                                   frame.offset(),
                                   frame.fourcc(),
                                   0u,
                                   drm_format_mod_linear};
    return ops_.show_scanout(ops_.opaque, head, fd, &plane) == 0 ? Status::ok : Status::not_supported;
}

void TableHost::update_scanout(uint32_t head, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    AV_SCOPE("TableHost::update_scanout");
    if (ops_.update_scanout != nullptr) ops_.update_scanout(ops_.opaque, head, x, y, width, height);
}

void TableHost::hide_scanout(uint32_t head)
{
    AV_SCOPE("TableHost::hide_scanout");
    if (ops_.hide_scanout != nullptr) ops_.hide_scanout(ops_.opaque, head);
}

Status TableHost::show_image(uint32_t head, const AvExportedImage &image)
{
    AV_SCOPE("TableHost::show_image");
    if (ops_.show_image == nullptr) return Status::not_supported;
    return ops_.show_image(ops_.opaque, head, &image) == 0 ? Status::ok : Status::not_supported;
}

/* A table from before renderer mode has none of these, which is a host that
 * runs every context in QEMU: no descriptors, no regions, no renderer. */
int TableHost::event_descriptor(void *event)
{
    AV_SCOPE("TableHost::event_descriptor");
    if (ops_.event_descriptor == nullptr) return -1;
    return ops_.event_descriptor(ops_.opaque, event);
}

int TableHost::fence_interrupt_descriptor(uint32_t line)
{
    AV_SCOPE("TableHost::fence_interrupt_descriptor");
    if (ops_.fence_interrupt_descriptor == nullptr) return -1;
    return ops_.fence_interrupt_descriptor(ops_.opaque, line);
}

uint32_t TableHost::guest_region_count()
{
    AV_SCOPE("TableHost::guest_region_count");
    if (ops_.guest_region_count == nullptr) return 0u;
    return ops_.guest_region_count(ops_.opaque);
}

Status TableHost::guest_region(uint32_t index, uint32_t &region, int &descriptor, uint64_t &size)
{
    AV_SCOPE("TableHost::guest_region");
    descriptor = -1;
    if (ops_.guest_region == nullptr) return Status::not_supported;
    uint32_t number = 0u;
    int      made = -1;
    uint64_t bytes = 0u;
    if (ops_.guest_region(ops_.opaque, index, &number, &made, &bytes) != 0) return Status::not_supported;
    region = number;
    descriptor = made;
    size = bytes;
    return Status::ok;
}

Status TableHost::locate_guest(const void *pointer, uint64_t length, uint32_t &region, uint64_t &offset)
{
    AV_SCOPE("TableHost::locate_guest");
    if (ops_.locate_guest == nullptr) return Status::not_found;
    uint32_t number = 0u;
    uint64_t at = 0u;
    if (ops_.locate_guest(ops_.opaque, pointer, length, &number, &at) != 0) return Status::not_found;
    region = number;
    offset = at;
    return Status::ok;
}

int TableHost::host_memory_descriptor(void *memory)
{
    AV_SCOPE("TableHost::host_memory_descriptor");
    if (ops_.host_memory_descriptor == nullptr) return -1;
    return ops_.host_memory_descriptor(ops_.opaque, memory);
}

Status TableHost::dmabuf_over_host_memory(void *memory, uint64_t offset, uint64_t size, int &fd)
{
    AV_SCOPE("TableHost::dmabuf_over_host_memory");
    fd = -1;
    if (ops_.dmabuf_over_host_memory == nullptr) return Status::not_supported;
    int made = -1;
    if (ops_.dmabuf_over_host_memory(ops_.opaque, memory, offset, size, &made) != 0) return Status::not_supported;
    fd = made;
    return Status::ok;
}

void TableHost::close_descriptor(int descriptor)
{
    AV_SCOPE("TableHost::close_descriptor");
    if (ops_.close_descriptor != nullptr && descriptor >= 0) ops_.close_descriptor(ops_.opaque, descriptor);
}

Status TableHost::start_renderer(const char *path)
{
    AV_SCOPE("TableHost::start_renderer");
    if (ops_.start_renderer == nullptr) return Status::not_supported;
    return ops_.start_renderer(ops_.opaque, path) == 0 ? Status::ok : Status::not_found;
}

Status TableHost::send_renderer(const uint8_t *bytes, size_t length, const int *descriptors, uint32_t count)
{
    AV_SCOPE("TableHost::send_renderer");
    if (ops_.send_renderer == nullptr) return Status::not_supported;
    return ops_.send_renderer(ops_.opaque, bytes, length, descriptors, count) == 0 ? Status::ok : Status::not_found;
}

/* A datagram longer than the buffer arrived cut, so it is refused whole and
 * the descriptors that came with it closed: the caller never sees half a
 * message. A table claiming more descriptors than it had room for is refused
 * the same way. */
Status TableHost::receive_renderer(uint8_t *buffer, size_t capacity, size_t &length, int *descriptors,
                                   uint32_t capacity_descriptors, uint32_t &count, uint32_t timeout_ms)
{
    AV_SCOPE("TableHost::receive_renderer");
    length = 0u;
    count = 0u;
    if (ops_.receive_renderer == nullptr) return Status::not_supported;
    size_t   got = 0u;
    uint32_t taken = 0u;
    const int answer = ops_.receive_renderer(ops_.opaque, buffer, capacity, &got, descriptors, capacity_descriptors,
                                             &taken, timeout_ms);
    if (answer == receive_timed_out) return Status::invalid_state;
    if (answer != receive_ok) return Status::not_found;
    if (got > capacity || taken > capacity_descriptors) {
        for (uint32_t index = 0u; index < taken && index < capacity_descriptors; index++)
            close_descriptor(descriptors[index]);
        return Status::too_large;
    }
    length = got;
    count = taken;
    return Status::ok;
}

void TableHost::kill_renderer()
{
    AV_SCOPE("TableHost::kill_renderer");
    if (ops_.kill_renderer != nullptr) ops_.kill_renderer(ops_.opaque);
}

Status TableHost::receive_export(int connection, uint8_t *buffer, size_t capacity, size_t &length)
{
    AV_SCOPE("TableHost::receive_export");
    length = 0u;
    if (ops_.receive_export == nullptr) return Status::not_found;
    size_t received = 0u;
    if (ops_.receive_export(ops_.opaque, connection, buffer, capacity, &received) != 0) return Status::not_found;
    length = received;
    return Status::ok;
}

Status TableHost::send_export(int connection, const uint8_t *bytes, size_t length, int descriptor)
{
    AV_SCOPE("TableHost::send_export");
    if (ops_.send_export == nullptr) return Status::not_found;
    return ops_.send_export(ops_.opaque, connection, bytes, length, descriptor) == 0 ? Status::ok : Status::not_found;
}

void TableHost::shutdown_export(int connection)
{
    AV_SCOPE("TableHost::shutdown_export");
    if (ops_.shutdown_export != nullptr) ops_.shutdown_export(ops_.opaque, connection);
}

void TableHost::close_export(int descriptor)
{
    AV_SCOPE("TableHost::close_export");
    if (ops_.close_export != nullptr) ops_.close_export(ops_.opaque, descriptor);
}

}  // namespace av
