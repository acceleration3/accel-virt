/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "qemu/osdep.h"
#include "qemu/error-report.h"
#include "qemu/main-loop.h"
#include "qemu/thread.h"
#include "qemu/aio.h"
#include "qemu/iov.h"
#include <sys/ioctl.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include "qemu/rcu.h"
#include "system/accel-blocker.h"
#include "system/kvm.h"
#include "system/runstate.h"
#include "qemu/event_notifier.h"
#include "qapi/error.h"
#include "system/dma.h"
#include "system/memory.h"
#include "system/address-spaces.h"
#include "system/ramblock.h"
#include "ui/console.h"
#include "hw/virtio/virtio-gpu-bswap.h"
#include "hw/virtio/virtio-bus.h"
#include "hw/virtio/virtio-gpu-av.h"

#include <vulkan/vulkan.h>

typedef struct VirtIOGPUAvThread {
    QemuThread thread;
    AvHostRoutine entry;
    void *context;
} VirtIOGPUAvThread;

static void av_log(void *opaque, const char *message)
{
    warn_report("%s", message);
}

static int av_main_loop_held(void *opaque)
{
    return bql_locked();
}

static void *av_thread_main(void *opaque)
{
    VirtIOGPUAvThread *t = opaque;

    rcu_register_thread();
    t->entry(t->context);
    rcu_unregister_thread();
    return NULL;
}

static void *av_start_thread(void *opaque, AvHostRoutine entry, void *context)
{
    VirtIOGPUAvThread *t = g_new0(VirtIOGPUAvThread, 1);

    t->entry = entry;
    t->context = context;
    qemu_thread_create(&t->thread, "av-context", av_thread_main, t, QEMU_THREAD_JOINABLE);
    return t;
}

static void av_join_thread(void *opaque, void *thread)
{
    VirtIOGPUAvThread *t = thread;

    qemu_thread_join(&t->thread);
    g_free(t);
}

static void *av_create_event(void *opaque)
{
    EventNotifier *event = g_new0(EventNotifier, 1);

    if (event_notifier_init(event, 0) < 0) {
        g_free(event);
        return NULL;
    }
    return event;
}

static void av_destroy_event(void *opaque, void *event)
{
    event_notifier_cleanup(event);
    g_free(event);
}

static void av_signal_event(void *opaque, void *event)
{
    event_notifier_set(event);
}

static void av_wait_event(void *opaque, void *event)
{
    GPollFD wake = { .fd = event_notifier_get_fd(event), .events = G_IO_IN };

    while (g_poll(&wake, 1, -1) < 0 && errno == EINTR) {
    }
    event_notifier_test_and_clear(event);
}

static uint64_t av_monotonic_nanoseconds(void *opaque);

static int av_wait_event_until(void *opaque, void *event, uint64_t deadline)
{
    struct pollfd wake = { .fd = event_notifier_get_fd(event), .events = POLLIN };
    int ready;

    do {
        const uint64_t now = av_monotonic_nanoseconds(opaque);
        const uint64_t left = deadline > now ? deadline - now : 0;
        const struct timespec timeout = { .tv_sec = (time_t)(left / 1000000000u),
                                          .tv_nsec = (long)(left % 1000000000u) };
        ready = ppoll(&wake, 1, &timeout, NULL);
    } while (ready < 0 && errno == EINTR);
    if (ready <= 0) {
        return 0;
    }
    event_notifier_test_and_clear(event);
    return 1;
}

static void *av_create_mutex(void *opaque)
{
    QemuMutex *mutex = g_new0(QemuMutex, 1);

    qemu_mutex_init(mutex);
    return mutex;
}

static void av_destroy_mutex(void *opaque, void *mutex)
{
    qemu_mutex_destroy(mutex);
    g_free(mutex);
}

static void av_lock_mutex(void *opaque, void *mutex)
{
    qemu_mutex_lock(mutex);
}

static int av_try_lock_mutex(void *opaque, void *mutex)
{
    return qemu_mutex_trylock(mutex) == 0;
}

static void av_unlock_mutex(void *opaque, void *mutex)
{
    qemu_mutex_unlock(mutex);
}

static void av_run_on_main_loop(void *opaque, AvHostRoutine routine, void *context)
{
    aio_bh_schedule_oneshot(qemu_get_aio_context(), routine, context);
}

static uint32_t av_gpu_count(void *opaque)
{
    VirtIOGPUAv *r = opaque;

    return r->gpu_count;
}

static void av_gpu_properties(void *opaque, uint32_t index, AvGpuProperties *properties)
{
    VirtIOGPUAv *r = opaque;

    *properties = r->gpus[index];
}

static uint64_t av_monotonic_nanoseconds(void *opaque)
{
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * 1000000000u + (uint64_t)now.tv_nsec;
}

static void *av_allocate(void *opaque, size_t size)
{
    return g_try_malloc0(size);
}

static void av_release(void *opaque, void *block)
{
    g_free(block);
}

static AddressSpace *av_address_space(VirtIOGPUAv *r)
{
    AddressSpace *device = VIRTIO_DEVICE(r)->dma_as;

    return device ? device : &address_space_memory;
}

static void *av_map_guest(void *opaque, uint64_t address, uint64_t *length, int *direct)
{
    VirtIOGPUAv *r = opaque;
    dma_addr_t mapped = *length;
    ram_addr_t offset;
    void *pointer = dma_memory_map(av_address_space(r), address, &mapped, DMA_DIRECTION_TO_DEVICE,
                                   MEMTXATTRS_UNSPECIFIED);

    *length = pointer ? mapped : 0;
    *direct = pointer && memory_region_from_host(pointer, &offset) != NULL;
    return pointer;
}

static void av_unmap_guest(void *opaque, void *pointer, uint64_t length)
{
    VirtIOGPUAv *r = opaque;

    dma_memory_unmap(av_address_space(r), pointer, length, DMA_DIRECTION_TO_DEVICE, 0);
}

static void *av_reserve_range(void *opaque, uint64_t size)
{
    void *base = mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);

    return base == MAP_FAILED ? NULL : base;
}

static int av_ram_file(void *run, uint64_t size, int *fd, off_t *offset)
{
    ram_addr_t in_block;
    RAMBlock *block;

    RCU_READ_LOCK_GUARD();
    block = qemu_ram_block_from_host(run, false, &in_block);
    if (!block || (*fd = qemu_ram_get_fd(block)) < 0 || in_block + size > qemu_ram_get_used_length(block)) {
        return 1;
    }
    *offset = (off_t)(qemu_ram_get_fd_offset(block) + in_block);
    return 0;
}

static int av_alias_guest_run(void *opaque, void *run, uint64_t size, void *at)
{
    int fd;
    off_t offset;

    if (av_ram_file(run, size, &fd, &offset)) {
        return 1;
    }
    return mmap(at, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, offset) == MAP_FAILED;
}

static void *av_map_dmabuf_part(int fd, uint64_t size, uint64_t offset)
{
    char *whole = mmap(NULL, offset + size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (whole == MAP_FAILED) {
        return NULL;
    }
    if (offset) {
        munmap(whole, offset);
    }
    return whole + offset;
}

static int av_move_run(void *run, uint64_t size, void *staging)
{
    memcpy(staging, run, size);
    if (mremap(staging, size, size, MREMAP_MAYMOVE | MREMAP_FIXED, run) == MAP_FAILED) {
        munmap(staging, size);
        return 1;
    }
    return 0;
}

static int av_overlay_guest_run(void *opaque, void *run, uint64_t size, int fd, uint64_t offset)
{
    int ram_fd;
    off_t ram_offset;
    void *staging;

    if (av_ram_file(run, size, &ram_fd, &ram_offset) || !(staging = av_map_dmabuf_part(fd, size, offset))) {
        return 1;
    }
    return av_move_run(run, size, staging);
}

static int av_restore_guest_run(void *opaque, void *run, uint64_t size)
{
    int ram_fd;
    off_t ram_offset;
    void *staging;

    if (av_ram_file(run, size, &ram_fd, &ram_offset)) {
        return 1;
    }
    staging = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, ram_fd, ram_offset);
    if (staging == MAP_FAILED) {
        return 1;
    }
    return av_move_run(run, size, staging);
}

static int av_pause_guest(void *opaque)
{
    if (!bql_locked() || !kvm_enabled() || !runstate_is_running()) {
        return 1;
    }
    accel_ioctl_inhibit_begin();
    return 0;
}

static void av_resume_guest(void *opaque)
{
    accel_ioctl_inhibit_end();
}

static void av_release_range(void *opaque, void *base, uint64_t size)
{
    munmap(base, size);
}

static uint64_t av_hostmem_size(void *opaque)
{
    VirtIOGPUAv *r = opaque;

    return r->hostmem_mmap ? VIRTIO_GPU_BASE(r)->conf.hostmem : 0;
}

static int av_place_in_hostmem(void *opaque, int fd, uint64_t size, uint64_t offset)
{
    VirtIOGPUAv *r = opaque;

    if (!r->hostmem_mmap || fd < 0 || offset + size > VIRTIO_GPU_BASE(r)->conf.hostmem) {
        return 1;
    }
    return mmap((char *)r->hostmem_mmap + offset, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0) ==
           MAP_FAILED;
}

static void av_remove_from_hostmem(void *opaque, uint64_t offset, uint64_t size)
{
    VirtIOGPUAv *r = opaque;

    if (mmap((char *)r->hostmem_mmap + offset, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
             -1, 0) == MAP_FAILED) {
        warn_report("virtio-gpu-av: hostmem at 0x%" PRIx64 " could not be restored: %s", offset, strerror(errno));
    }
}

static int av_bind_gpu(void *opaque, uint32_t index)
{
    VirtIOGPUAv *r = opaque;

    if (index >= r->gpu_count || !r->instance) {
        return 1;
    }
    r->bound = r->physical[index];
    return 0;
}

static void av_connect_doorbell(void *opaque, void *event, uint32_t index)
{
    VirtIOGPUAv *r = opaque;

    memory_region_add_eventfd(&r->doorbells, (hwaddr)index * AV_DEVICE_DOORBELL_PAGE, 4, false, 0, event);
}

static void av_disconnect_doorbell(void *opaque, void *event, uint32_t index)
{
    VirtIOGPUAv *r = opaque;

    memory_region_del_eventfd(&r->doorbells, (hwaddr)index * AV_DEVICE_DOORBELL_PAGE, 4, false, 0, event);
}

static int av_connect_fence_interrupts(void *opaque)
{
    BusState *bus = qdev_get_parent_bus(DEVICE(opaque));
    VirtioBusClass *k = VIRTIO_BUS_GET_CLASS(bus);

    if (!k->set_guest_notifiers) {
        return 1;
    }
    return k->set_guest_notifiers(bus->parent, AV_DEVICE_FENCE_QUEUE_FIRST + AV_DEVICE_INTERRUPT_LINES, true) < 0;
}

static void av_disconnect_fence_interrupts(void *opaque)
{
    BusState *bus = qdev_get_parent_bus(DEVICE(opaque));
    VirtioBusClass *k = VIRTIO_BUS_GET_CLASS(bus);

    k->set_guest_notifiers(bus->parent, AV_DEVICE_FENCE_QUEUE_FIRST + AV_DEVICE_INTERRUPT_LINES, false);
}

static void av_raise_fence_interrupt(void *opaque, uint32_t line)
{
    VirtIOGPUAv *r = opaque;
    EventNotifier *notifier = virtio_queue_get_guest_notifier(r->fence_queues[line]);

    if (notifier->initialized) {
        event_notifier_set(notifier);
    }
}

static int av_event_descriptor(void *opaque, void *event)
{
    return event_notifier_get_fd(event);
}

static int av_fence_interrupt_descriptor(void *opaque, uint32_t line)
{
    VirtIOGPUAv *r = opaque;
    EventNotifier *notifier;

    if (line >= AV_DEVICE_INTERRUPT_LINES || !r->fence_queues[line]) {
        return -1;
    }
    notifier = virtio_queue_get_guest_notifier(r->fence_queues[line]);
    if (!notifier->initialized) {
        return -1;
    }
    return fcntl(event_notifier_get_fd(notifier), F_DUPFD_CLOEXEC, 0);
}

static int av_show_image(void *opaque, uint32_t head, const AvExportedImage *image)
{
    return virtio_gpu_av_show_image(opaque, head, image) ? 0 : 1;
}

static void av_close_descriptor(void *opaque, int descriptor)
{
    if (descriptor >= 0) {
        close(descriptor);
    }
}

static size_t av_read_request(void *opaque, void *request, size_t offset, uint8_t *buffer, size_t size)
{
    struct virtio_gpu_ctrl_command *cmd = request;

    return iov_to_buf(cmd->elem.out_sg, cmd->elem.out_num, offset, buffer, size);
}

static void av_respond(void *opaque, void *request, const uint8_t *bytes, size_t size)
{
    VirtIOGPUAv *r = opaque;
    struct {
        struct virtio_gpu_ctrl_hdr header;
        uint8_t body[232];
    } response = { 0 };

    g_assert(size >= sizeof(response.header) && size <= sizeof(response));
    memcpy(&response, bytes, size);
    virtio_gpu_ctrl_hdr_bswap(&response.header);
    virtio_gpu_ctrl_response(VIRTIO_GPU(r), request, &response.header, size);
}

static int av_resource_exists(void *opaque, uint32_t id)
{
    VirtIOGPUAv *r = opaque;

    return virtio_gpu_find_resource(VIRTIO_GPU(r), id) != NULL;
}

static void *av_create_recording(void *opaque, const char *path)
{
    int fd;

    (void)opaque;
    if (path == NULL || path[0] == '\0') {
        return NULL;
    }
    fd = qemu_open_old(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        return NULL;
    }
    return (void *)(intptr_t)(fd + 1);
}

static int av_append_recording(void *opaque, void *file, const uint8_t *bytes, size_t size)
{
    int fd = (int)(intptr_t)file - 1;

    (void)opaque;
    if (qemu_write_full(fd, bytes, size) != (ssize_t)size) {
        return 1;
    }
    return fdatasync(fd) == 0 ? 0 : 1;
}

static int av_truncate_recording(void *opaque, void *file)
{
    int fd = (int)(intptr_t)file - 1;

    (void)opaque;
    if (ftruncate(fd, 0) != 0) {
        return 1;
    }
    return lseek(fd, 0, SEEK_SET) == 0 ? 0 : 1;
}

static void av_close_recording(void *opaque, void *file)
{
    (void)opaque;
    qemu_close((int)(intptr_t)file - 1);
}

static QemuConsole *av_head_console(VirtIOGPUAv *r, uint32_t head)
{
    VirtIOGPUBase *b = VIRTIO_GPU_BASE(r);

    if (head >= (uint32_t)b->conf.max_outputs || head >= VIRTIO_GPU_MAX_SCANOUTS) {
        return NULL;
    }
    return b->scanout[head].con;
}

static int av_display_takes_dmabuf(void *opaque, uint32_t head)
{
    QemuConsole *con = av_head_console(opaque, head);

    return con != NULL && qemu_console_has_gl(con);
}

static int av_show_scanout(void *opaque, uint32_t head, const void *blob, const AvScanoutFrame *frame)
{
    VirtIOGPUAv *r = opaque;
    QemuConsole *con = av_head_console(r, head);
    pixman_format_code_t format;
    DisplaySurface *surface;
    uint8_t *pixels;

    if (!con || !frame || !blob) {
        return 1;
    }
    format = qemu_drm_format_to_pixman(frame->fourcc);
    if (!format) {
        return 1;
    }
    pixels = (uint8_t *)blob + frame->offset + (uint64_t)frame->y * frame->stride +
             (uint64_t)frame->x * (PIXMAN_FORMAT_BPP(format) / 8);
    surface = qemu_create_displaysurface_from(frame->width, frame->height, format, frame->stride, pixels);
    VIRTIO_GPU_BASE(r)->enable = 1;
    qemu_console_set_surface(con, surface);
    r->scanouts[head] = true;
    return 0;
}

static void av_drop_exported(VirtIOGPUAv *r, uint32_t head, uint32_t slot, QemuConsole *con)
{
    if (!r->exported[head][slot]) {
        return;
    }
    qemu_console_gl_release_dmabuf(con, r->exported[head][slot]);
    qemu_dmabuf_close(r->exported[head][slot]);
    g_clear_pointer(&r->exported[head][slot], qemu_dmabuf_free);
}

bool virtio_gpu_av_show_image(VirtIOGPUAv *r, uint32_t head, const AvExportedImage *image)
{
    QemuConsole *con = av_head_console(r, head);
    QemuDmaBuf *buffer;
    int32_t duplicate;
    uint32_t stride, offset;

    if (!con || !qemu_console_has_gl(con) || image->slot >= 2 || image->fd < 0) {
        return false;
    }
    buffer = r->exported[head][image->slot];
    if (buffer && (r->exported_fd[head][image->slot] != image->fd ||
                   qemu_dmabuf_get_width(buffer) != image->width || qemu_dmabuf_get_height(buffer) != image->height ||
                   qemu_dmabuf_get_fourcc(buffer) != image->fourcc ||
                   qemu_dmabuf_get_modifier(buffer) != image->modifier ||
                   qemu_dmabuf_get_strides(buffer, NULL)[0] != image->stride ||
                   qemu_dmabuf_get_offsets(buffer, NULL)[0] != image->offset)) {
        if (r->showing[head] == (int32_t)image->slot) {
            r->showing[head] = -1;
        }
        av_drop_exported(r, head, image->slot, con);
        buffer = NULL;
    }
    if (!buffer) {
        duplicate = dup(image->fd);
        if (duplicate < 0) {
            return false;
        }
        stride = image->stride;
        offset = image->offset;
        buffer = qemu_dmabuf_new(image->width, image->height, &offset, &stride, 0, 0, image->width, image->height,
                                 image->fourcc, image->modifier, &duplicate, 1, false, false);
        r->exported[head][image->slot] = buffer;
        r->exported_fd[head][image->slot] = image->fd;
    }
    if (r->showing[head] != (int32_t)image->slot) {
        VIRTIO_GPU_BASE(r)->enable = 1;
        if (r->showing[head] < 0) {
            fprintf(stderr, "virtio-gpu-av: head %u shows exported images\n", head);
        }
        qemu_console_gl_scanout_dmabuf(con, buffer);
        r->showing[head] = (int32_t)image->slot;
    }
    qemu_console_gl_update(con, 0, 0, image->width, image->height);
    return true;
}

void virtio_gpu_av_show_surface(VirtIOGPUAv *r, uint32_t head)
{
    QemuConsole *con = av_head_console(r, head);

    if (!con || r->showing[head] < 0) {
        return;
    }
    fprintf(stderr, "virtio-gpu-av: head %u back to its 2D surface\n", head);
    qemu_console_gl_scanout_disable(con);
    r->showing[head] = -1;
}

static void av_update_scanout(void *opaque, uint32_t head, uint32_t x, uint32_t y, uint32_t width,
                               uint32_t height)
{
    VirtIOGPUAv *r = opaque;
    QemuConsole *con = av_head_console(r, head);

    if (con && r->scanouts[head]) {
        qemu_console_update(con, x, y, width, height);
    }
}

static void av_hide_scanout(void *opaque, uint32_t head)
{
    VirtIOGPUAv *r = opaque;
    QemuConsole *con = av_head_console(r, head);

    if (con && r->scanouts[head]) {
        qemu_console_set_surface(con, NULL);
        r->scanouts[head] = false;
    }
}

static int av_pci_revision(const AvGpuProperties *gpu)
{
    g_autofree char *path = g_strdup_printf("/sys/bus/pci/devices/%04x:%02x:%02x.%x/revision", gpu->pci_domain,
                                            gpu->pci_bus, gpu->pci_device, gpu->pci_function);
    g_autofree char *text = NULL;

    if (!gpu->on_pci || !g_file_get_contents(path, &text, NULL, NULL)) {
        return -1;
    }
    return (int)g_ascii_strtoll(text, NULL, 16);
}

static bool av_amdgpu_name(const AvGpuProperties *gpu, int revision, char *name, size_t size)
{
    g_autofree char *table = NULL;
    g_auto(GStrv) lines = NULL;

    if (gpu->vendor_id != 0x1002 || revision < 0 ||
        !g_file_get_contents("/usr/share/libdrm/amdgpu.ids", &table, NULL, NULL)) {
        return false;
    }
    lines = g_strsplit(table, "\n", -1);
    for (char **line = lines; *line; line++) {
        g_auto(GStrv) fields = g_strsplit(*line, ",", 3);
        if (g_strv_length(fields) == 3 && g_ascii_strtoull(fields[0], NULL, 16) == gpu->device_id &&
            g_ascii_strtoull(g_strstrip(fields[1]), NULL, 16) == (guint64)revision) {
            g_strlcpy(name, g_strstrip(fields[2]), size);
            return true;
        }
    }
    return false;
}

static bool av_pci_ids_name(const AvGpuProperties *gpu, char *name, size_t size)
{
    static const char *const databases[] = {"/usr/share/hwdata/pci.ids", "/usr/share/misc/pci.ids"};
    g_autofree char *vendor_line = g_strdup_printf("%04x  ", gpu->vendor_id);
    g_autofree char *device_line = g_strdup_printf("\t%04x  ", gpu->device_id);
    const char *vendor = gpu->vendor_id == 0x1002 ? "AMD" : gpu->vendor_id == 0x8086 ? "Intel(R)"
                       : gpu->vendor_id == 0x10de ? "NVIDIA" : NULL;
    const char *word = gpu->vendor_id == 0x8086 ? "Intel " : gpu->vendor_id == 0x10de ? "NVIDIA " : "AMD ";

    for (size_t d = 0; d < G_N_ELEMENTS(databases) && vendor; d++) {
        g_autofree char *text = NULL;
        g_auto(GStrv) lines = NULL;
        bool in_vendor = false;

        if (!g_file_get_contents(databases[d], &text, NULL, NULL)) {
            continue;
        }
        lines = g_strsplit(text, "\n", -1);
        for (char **line = lines; *line; line++) {
            if ((*line)[0] != '\t' && (*line)[0] != '#' && (*line)[0] != '\0') {
                in_vendor = g_str_has_prefix(*line, vendor_line);
                continue;
            }
            if (!in_vendor || !g_str_has_prefix(*line, device_line)) {
                continue;
            }
            const char *product = *line + strlen(device_line);
            const char *open = strrchr(product, '[');
            const char *close = open ? strchr(open, ']') : NULL;
            g_autofree char *picked = open && close ? g_strndup(open + 1, (gsize)(close - open - 1))
                                                    : g_strdup(product);
            const char *rest = g_str_has_prefix(picked, word) ? picked + strlen(word) : picked;
            g_autofree char *full = g_strdup_printf("%s %s", vendor, rest);
            g_strlcpy(name, full, size);
            return true;
        }
    }
    return false;
}

static bool av_integrated_name(const AvGpuProperties *gpu, char *name, size_t size)
{
    if (gpu->vendor_id == 0x1002) {
        g_strlcpy(name, "AMD Radeon(TM) Graphics", size);
        return true;
    }
    return av_pci_ids_name(gpu, name, size);
}

static void av_marketing_name(AvGpuProperties *gpu)
{
    char name[sizeof(gpu->name)] = {0};
    const int revision = av_pci_revision(gpu);

    if (av_amdgpu_name(gpu, revision, name, sizeof(name)) ||
        (gpu->device_type == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU && av_integrated_name(gpu, name, sizeof(name)))) {
        g_strlcpy(gpu->name, name, sizeof(gpu->name));
    }
}

static bool av_has_device_extension(VkPhysicalDevice device, const char *name)
{
    g_autofree VkExtensionProperties *extensions = NULL;
    uint32_t count = 0;

    if (vkEnumerateDeviceExtensionProperties(device, NULL, &count, NULL) != VK_SUCCESS) {
        return false;
    }
    extensions = g_new0(VkExtensionProperties, count);
    if (vkEnumerateDeviceExtensionProperties(device, NULL, &count, extensions) < 0) {
        return false;
    }
    for (uint32_t i = 0; i < count; i++) {
        if (strcmp(extensions[i].extensionName, name) == 0) {
            return true;
        }
    }
    return false;
}

static int av_read_file(void *opaque, const char *path, char *buffer, size_t size, size_t *length)
{
    g_autofree char *contents = NULL;
    gsize read = 0;

    if (!g_file_get_contents(path, &contents, &read, NULL)) {
        return 1;
    }
    *length = MIN(read, size);
    memcpy(buffer, contents, *length);
    return 0;
}

static int av_list_directory(void *opaque, const char *path, char *buffer, size_t size, size_t *length)
{
    GDir *directory = g_dir_open(path, 0, NULL);
    const char *name;

    if (directory == NULL) {
        return 1;
    }
    *length = 0;
    while ((name = g_dir_read_name(directory)) != NULL) {
        const size_t name_length = strlen(name);
        if (*length + name_length + 1 > size) {
            break;
        }
        memcpy(buffer + *length, name, name_length);
        *length += name_length;
        buffer[(*length)++] = '\n';
    }
    g_dir_close(directory);
    return 0;
}

static void av_read_gpus(VirtIOGPUAv *r)
{
    const VkApplicationInfo application = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "virtio-gpu-av",
        .apiVersion = VK_API_VERSION_1_1,
    };
    const VkInstanceCreateInfo create = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &application,
    };
    VkPhysicalDevice devices[VIRTIO_GPU_AV_MAX_GPUS];
    uint32_t count = VIRTIO_GPU_AV_MAX_GPUS;
    VkInstance instance;

    r->gpu_count = 0;
    if (vkCreateInstance(&create, NULL, &instance) != VK_SUCCESS) {
        return;
    }
    r->instance = instance;
    if (vkEnumeratePhysicalDevices(instance, &count, devices) < 0) {
        count = 0;
    }
    for (uint32_t i = 0; i < count; i++) {
        VkPhysicalDevicePCIBusInfoPropertiesEXT bus = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PCI_BUS_INFO_PROPERTIES_EXT,
        };
        const bool on_pci = av_has_device_extension(devices[i], VK_EXT_PCI_BUS_INFO_EXTENSION_NAME);
        VkPhysicalDeviceIDProperties id = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES,
            .pNext = on_pci ? &bus : NULL,
        };
        VkPhysicalDeviceProperties2 properties = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
            .pNext = &id,
        };
        VkPhysicalDeviceMemoryProperties memory;
        AvGpuProperties *gpu = &r->gpus[i];

        vkGetPhysicalDeviceProperties2(devices[i], &properties);
        vkGetPhysicalDeviceMemoryProperties(devices[i], &memory);
        *gpu = (AvGpuProperties) {
            .vendor_id = properties.properties.vendorID,
            .device_id = properties.properties.deviceID,
            .device_type = properties.properties.deviceType,
            .pci_domain = bus.pciDomain,
            .pci_bus = bus.pciBus,
            .pci_device = bus.pciDevice,
            .pci_function = bus.pciFunction,
            .on_pci = on_pci,
        };
        memcpy(gpu->uuid, id.deviceUUID, sizeof(gpu->uuid));
        g_strlcpy(gpu->name, properties.properties.deviceName, sizeof(gpu->name));
        av_marketing_name(gpu);
        for (uint32_t heap = 0; heap < memory.memoryHeapCount; heap++) {
            if (memory.memoryHeaps[heap].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                gpu->local_memory += memory.memoryHeaps[heap].size;
            } else {
                gpu->shared_memory += memory.memoryHeaps[heap].size;
            }
        }
    }
    for (uint32_t i = 0; i < count; i++) {
        r->physical[i] = devices[i];
    }
    r->gpu_count = count;
}

void virtio_gpu_av_host_finish(VirtIOGPUAv *r)
{
    if (r->instance) {
        vkDestroyInstance(r->instance, NULL);
        r->instance = VK_NULL_HANDLE;
    }
}

void virtio_gpu_av_export_init(VirtIOGPUAv *r)
{
    r->export_listener = -1;
}

static void av_export_accept(void *opaque)
{
    VirtIOGPUAv *r = opaque;

    for (;;) {
        struct ucred peer = { 0 };
        socklen_t peer_size = sizeof(peer);
        const int connection = accept4(r->export_listener, NULL, NULL, SOCK_CLOEXEC);

        if (connection < 0) {
            return;
        }
        const int same_user = getsockopt(connection, SOL_SOCKET, SO_PEERCRED, &peer, &peer_size) == 0 &&
                              peer.uid == geteuid();
        if (av_device_export_accepted(r->core, connection, same_user) != 0) {
            close(connection);
        }
    }
}

static bool av_export_path_free(const char *path, Error **errp)
{
    struct sockaddr_un address = { .sun_family = AF_UNIX };
    struct stat file;
    int probe;
    int connected;

    if (lstat(path, &file) != 0) {
        return true;
    }
    if (!S_ISSOCK(file.st_mode)) {
        error_setg(errp, "virtio-gpu-av: export-socket '%s' exists and is not a socket", path);
        return false;
    }
    probe = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
    if (probe < 0) {
        error_setg_errno(errp, errno, "virtio-gpu-av: no socket to probe export-socket '%s'", path);
        return false;
    }
    memcpy(address.sun_path, path, strlen(path));
    connected = connect(probe, (struct sockaddr *)&address, sizeof(address));
    const int failure = errno;
    close(probe);
    if (connected == 0) {
        error_setg(errp, "virtio-gpu-av: export-socket '%s' is another process's, which is listening on it", path);
        return false;
    }
    if (failure == ECONNREFUSED && unlink(path) != 0) {
        error_setg_errno(errp, errno, "virtio-gpu-av: the stale export-socket '%s' cannot be removed", path);
        return false;
    }
    return true;
}

bool virtio_gpu_av_export_listen(VirtIOGPUAv *r, Error **errp)
{
    struct sockaddr_un address = { .sun_family = AF_UNIX };
    const char *path = r->export_socket;
    int listener;

    if (strlen(path) >= sizeof(address.sun_path)) {
        error_setg(errp, "virtio-gpu-av: export-socket '%s' is longer than a socket path can be", path);
        return false;
    }
    if (!av_export_path_free(path, errp)) {
        return false;
    }
    memcpy(address.sun_path, path, strlen(path));
    listener = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (listener < 0) {
        error_setg_errno(errp, errno, "virtio-gpu-av: no socket for export-socket '%s'", path);
        return false;
    }
    if (bind(listener, (struct sockaddr *)&address, sizeof(address)) != 0 || chmod(path, 0600) != 0 ||
        listen(listener, 8) != 0) {
        error_setg_errno(errp, errno, "virtio-gpu-av: export-socket '%s' cannot be listened on", path);
        close(listener);
        return false;
    }
    r->export_listener = listener;
    qemu_set_fd_handler(listener, av_export_accept, NULL, r);
    return true;
}

void virtio_gpu_av_export_finish(VirtIOGPUAv *r)
{
    if (r->export_listener < 0) {
        return;
    }
    qemu_set_fd_handler(r->export_listener, NULL, NULL, NULL);
    close(r->export_listener);
    r->export_listener = -1;
    unlink(r->export_socket);
}

static int av_receive_export(void *opaque, int connection, uint8_t *buffer, size_t capacity, size_t *length)
{
    char control[CMSG_SPACE(sizeof(int) * 4)] = { 0 };
    struct iovec vector = { .iov_base = buffer, .iov_len = capacity };
    struct msghdr message = {
        .msg_iov = &vector, .msg_iovlen = 1, .msg_control = control, .msg_controllen = sizeof(control),
    };
    ssize_t got;

    do {
        got = recvmsg(connection, &message, MSG_TRUNC | MSG_CMSG_CLOEXEC);
    } while (got < 0 && errno == EINTR);
    for (struct cmsghdr *header = CMSG_FIRSTHDR(&message); header; header = CMSG_NXTHDR(&message, header)) {
        if (header->cmsg_level != SOL_SOCKET || header->cmsg_type != SCM_RIGHTS) {
            continue;
        }
        const size_t carried = (header->cmsg_len - CMSG_LEN(0)) / sizeof(int);
        for (size_t index = 0; index < carried; index++) {
            int descriptor;
            memcpy(&descriptor, CMSG_DATA(header) + index * sizeof(int), sizeof(descriptor));
            close(descriptor);
        }
    }
    if (got <= 0) {
        return 1;
    }
    *length = (size_t)got;
    return 0;
}

static int av_send_export(void *opaque, int connection, const uint8_t *bytes, size_t length, int descriptor)
{
    char control[CMSG_SPACE(sizeof(int))] = { 0 };
    struct iovec vector = { .iov_base = (void *)bytes, .iov_len = length };
    struct msghdr message = { .msg_iov = &vector, .msg_iovlen = 1 };
    ssize_t sent;

    if (descriptor >= 0) {
        message.msg_control = control;
        message.msg_controllen = sizeof(control);
        struct cmsghdr *header = CMSG_FIRSTHDR(&message);
        header->cmsg_level = SOL_SOCKET;
        header->cmsg_type = SCM_RIGHTS;
        header->cmsg_len = CMSG_LEN(sizeof(int));
        memcpy(CMSG_DATA(header), &descriptor, sizeof(descriptor));
    }
    do {
        sent = sendmsg(connection, &message, MSG_DONTWAIT | MSG_NOSIGNAL);
    } while (sent < 0 && errno == EINTR);
    return sent == (ssize_t)length ? 0 : 1;
}

static void av_shutdown_export(void *opaque, int connection)
{
    shutdown(connection, SHUT_RDWR);
}

static void av_close_export(void *opaque, int descriptor)
{
    close(descriptor);
}

static int av_duplicate_export(void *opaque, int descriptor)
{
    return fcntl(descriptor, F_DUPFD_CLOEXEC, 0);
}

void virtio_gpu_av_host_init(VirtIOGPUAv *r)
{
    r->host = (AvHostOps) {
        .opaque = r,
        .log = av_log,
        .main_loop_held = av_main_loop_held,
        .start_thread = av_start_thread,
        .join_thread = av_join_thread,
        .create_event = av_create_event,
        .destroy_event = av_destroy_event,
        .signal_event = av_signal_event,
        .wait_event = av_wait_event,
        .create_mutex = av_create_mutex,
        .destroy_mutex = av_destroy_mutex,
        .lock_mutex = av_lock_mutex,
        .unlock_mutex = av_unlock_mutex,
        .try_lock_mutex = av_try_lock_mutex,
        .run_on_main_loop = av_run_on_main_loop,
        .gpu_count = av_gpu_count,
        .gpu_properties = av_gpu_properties,
        .allocate = av_allocate,
        .release = av_release,
        .map_guest = av_map_guest,
        .unmap_guest = av_unmap_guest,
        .reserve_range = av_reserve_range,
        .alias_guest_run = av_alias_guest_run,
        .release_range = av_release_range,
        .overlay_guest_run = av_overlay_guest_run,
        .restore_guest_run = av_restore_guest_run,
        .pause_guest = av_pause_guest,
        .resume_guest = av_resume_guest,
        .hostmem_size = av_hostmem_size,
        .place_in_hostmem = av_place_in_hostmem,
        .remove_from_hostmem = av_remove_from_hostmem,
        .monotonic_nanoseconds = av_monotonic_nanoseconds,
        .bind_gpu = av_bind_gpu,
        .connect_doorbell = av_connect_doorbell,
        .disconnect_doorbell = av_disconnect_doorbell,
        .connect_fence_interrupts = av_connect_fence_interrupts,
        .disconnect_fence_interrupts = av_disconnect_fence_interrupts,
        .raise_fence_interrupt = av_raise_fence_interrupt,
        .read_request = av_read_request,
        .respond = av_respond,
        .resource_exists = av_resource_exists,
        .create_recording = av_create_recording,
        .append_recording = av_append_recording,
        .truncate_recording = av_truncate_recording,
        .close_recording = av_close_recording,
        .display_takes_dmabuf = av_display_takes_dmabuf,
        .show_scanout = av_show_scanout,
        .update_scanout = av_update_scanout,
        .hide_scanout = av_hide_scanout,
        .read_file = av_read_file,
        .list_directory = av_list_directory,
        .event_descriptor = av_event_descriptor,
        .fence_interrupt_descriptor = av_fence_interrupt_descriptor,
        .close_descriptor = av_close_descriptor,
        .wait_event_until = av_wait_event_until,
        .show_image = av_show_image,
        .receive_export = av_receive_export,
        .send_export = av_send_export,
        .shutdown_export = av_shutdown_export,
        .close_export = av_close_export,
        .duplicate_export = av_duplicate_export,
    };
    virtio_gpu_av_renderer_ops(&r->host);
    av_read_gpus(r);
}
