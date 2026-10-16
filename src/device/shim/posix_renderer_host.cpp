#include "device/shim/posix_renderer_host.hpp"

#include "core/trace/function_scope.hpp"
#include "device/renderer/doorbell_offers.hpp"
#include "device/renderer/forwarded_request.hpp"
#include "device/renderer/guest_memory_table.hpp"
#include "proto/context/gpu_identity.hpp"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

namespace av {

namespace {

/* A worker's wakeup: its own eventfd, and the doorbell QEMU registered with
 * KVM once the context is bound. */
struct RendererEvent {
    int own;
    int doorbell;
};

struct RendererThread {
    pthread_t   thread;
    HostRoutine entry;
    void       *context;
};

void *run_renderer_thread(void *started)
{
    AV_SCOPE("run_renderer_thread");
    RendererThread *const thread = static_cast<RendererThread *>(started);
    thread->entry(thread->context);
    return nullptr;
}

void drain(int descriptor)
{
    AV_SCOPE("drain");
    uint64_t count = 0u;
    while (read(descriptor, &count, sizeof count) < 0 && errno == EINTR) {
    }
}

void post(int descriptor)
{
    AV_SCOPE("post");
    const uint64_t one = 1u;
    while (write(descriptor, &one, sizeof one) < 0 && errno == EINTR) {
    }
}

}  // namespace

PosixRendererHost::PosixRendererHost(GuestMemoryTable &memory, DoorbellOffers &doorbells)
    : memory_(memory), doorbells_(doorbells), executor_(), main_loop_{}, main_loop_known_(false), wakeup_(-1),
      posted_lock_(PTHREAD_MUTEX_INITIALIZER), posted_first_(nullptr), posted_last_(nullptr),
      interrupt_lines_{}, interrupts_connected_(false)
{
    AV_SCOPE("PosixRendererHost::PosixRendererHost");
    for (int &line : interrupt_lines_) line = -1;
}

PosixRendererHost::~PosixRendererHost()
{
    AV_SCOPE("PosixRendererHost::~PosixRendererHost");
    drop_interrupt_lines();
    while (posted_first_ != nullptr) {
        Posted *const next = posted_first_->next_;
        free(posted_first_);
        posted_first_ = next;
    }
    if (wakeup_ >= 0) close(wakeup_);
}

Status PosixRendererHost::become_main_loop()
{
    AV_SCOPE("PosixRendererHost::become_main_loop");
    if (wakeup_ < 0) wakeup_ = eventfd(0u, EFD_CLOEXEC | EFD_NONBLOCK);
    if (wakeup_ < 0) return Status::no_memory;
    main_loop_ = pthread_self();
    main_loop_known_ = true;
    return Status::ok;
}

int PosixRendererHost::main_loop_wakeup() const
{
    AV_SCOPE("PosixRendererHost::main_loop_wakeup");
    return wakeup_;
}

/* The wakeup is drained before the list is taken: a routine posted after the
 * take signals it again, so none waits for an unrelated message. */
void PosixRendererHost::run_main_loop_work()
{
    AV_SCOPE("PosixRendererHost::run_main_loop_work");
    require_main_loop("PosixRendererHost::run_main_loop_work");
    if (wakeup_ >= 0) drain(wakeup_);
    pthread_mutex_lock(&posted_lock_);
    Posted *taken = posted_first_;
    posted_first_ = nullptr;
    posted_last_ = nullptr;
    pthread_mutex_unlock(&posted_lock_);
    while (taken != nullptr) {
        Posted *const next = taken->next_;
        taken->routine_(taken->context_);
        free(taken);
        taken = next;
    }
}

Status PosixRendererHost::map_region(int descriptor, uint64_t size, void *&base)
{
    AV_SCOPE("PosixRendererHost::map_region");
    base = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, descriptor, 0);
    if (base != MAP_FAILED) return Status::ok;
    base = nullptr;
    return Status::no_memory;
}

void PosixRendererHost::unmap_region(void *base, uint64_t size)
{
    AV_SCOPE("PosixRendererHost::unmap_region");
    munmap(base, size);
}

void PosixRendererHost::close_descriptor(int descriptor)
{
    AV_SCOPE("PosixRendererHost::close_descriptor");
    if (descriptor >= 0) close(descriptor);
}

Status PosixRendererHost::load_executor(const char *path, uint32_t length, const AvExecutorOps *&ops)
{
    AV_SCOPE("PosixRendererHost::load_executor");
    ops = nullptr;
    char terminated[path_capacity] = {};
    if (path == nullptr || length == 0u || length >= path_capacity) return Status::not_found;
    memcpy(terminated, path, length);
    const Status opened = executor_.open(terminated);
    if (opened != Status::ok) return opened;
    ops = executor_.ops();
    return Status::ok;
}

void PosixRendererHost::take_interrupt_lines(const int *descriptors, uint32_t count)
{
    AV_SCOPE("PosixRendererHost::take_interrupt_lines");
    drop_interrupt_lines();
    for (uint32_t line = 0u; line < count && line < max_interrupt_lines; line++)
        interrupt_lines_[line] = descriptors[line];
    for (uint32_t line = max_interrupt_lines; line < count; line++)
        if (descriptors[line] >= 0) close(descriptors[line]);
}

void PosixRendererHost::drop_interrupt_lines()
{
    AV_SCOPE("PosixRendererHost::drop_interrupt_lines");
    for (int &line : interrupt_lines_) {
        if (line >= 0) close(line);
        line = -1;
    }
}

void PosixRendererHost::log(const char *message)
{
    AV_SCOPE("PosixRendererHost::log");
    fprintf(stderr, "av-renderer: %s\n", message);
}

bool PosixRendererHost::on_main_loop() const
{
    AV_SCOPE("PosixRendererHost::on_main_loop");
    return main_loop_known_ && pthread_equal(pthread_self(), main_loop_) != 0;
}

void PosixRendererHost::require_main_loop(const char *function)
{
    AV_SCOPE("PosixRendererHost::require_main_loop");
    if (!on_main_loop()) fprintf(stderr, "av-renderer: %s called off the control thread\n", function);
}

void PosixRendererHost::require_worker(const char *function)
{
    AV_SCOPE("PosixRendererHost::require_worker");
    if (on_main_loop()) fprintf(stderr, "av-renderer: %s called on the control thread\n", function);
}

Status PosixRendererHost::start_thread(HostRoutine entry, void *context, void *&thread)
{
    AV_SCOPE("PosixRendererHost::start_thread");
    thread = nullptr;
    RendererThread *const made = static_cast<RendererThread *>(calloc(1u, sizeof(RendererThread)));
    if (made == nullptr) return Status::no_memory;
    made->entry = entry;
    made->context = context;
    if (pthread_create(&made->thread, nullptr, &run_renderer_thread, made) != 0) {
        free(made);
        return Status::no_memory;
    }
    thread = made;
    return Status::ok;
}

void PosixRendererHost::join_thread(void *thread)
{
    AV_SCOPE("PosixRendererHost::join_thread");
    RendererThread *const joined = static_cast<RendererThread *>(thread);
    pthread_join(joined->thread, nullptr);
    free(joined);
}

Status PosixRendererHost::create_event(void *&event)
{
    AV_SCOPE("PosixRendererHost::create_event");
    event = nullptr;
    RendererEvent *const made = static_cast<RendererEvent *>(calloc(1u, sizeof(RendererEvent)));
    if (made == nullptr) return Status::no_memory;
    made->own = eventfd(0u, EFD_CLOEXEC | EFD_NONBLOCK);
    made->doorbell = -1;
    if (made->own < 0) {
        free(made);
        return Status::no_memory;
    }
    event = made;
    return Status::ok;
}

void PosixRendererHost::destroy_event(void *event)
{
    AV_SCOPE("PosixRendererHost::destroy_event");
    RendererEvent *const destroyed = static_cast<RendererEvent *>(event);
    close(destroyed->own);
    if (destroyed->doorbell >= 0) close(destroyed->doorbell);
    free(destroyed);
}

void PosixRendererHost::signal_event(void *event)
{
    AV_SCOPE("PosixRendererHost::signal_event");
    post(static_cast<RendererEvent *>(event)->own);
}

/* Auto-reset, as the glue's is: both counters are read whole, and a signal
 * after the read is not lost because the worker drains its ring after this
 * returns. */
void PosixRendererHost::wait_event(void *event)
{
    AV_SCOPE("PosixRendererHost::wait_event");
    RendererEvent *const waited = static_cast<RendererEvent *>(event);
    pollfd       watched[2] = {{waited->own, POLLIN, 0}, {waited->doorbell, POLLIN, 0}};
    const nfds_t count = waited->doorbell >= 0 ? 2u : 1u;
    while (poll(watched, count, -1) < 0 && errno == EINTR) {
    }
    if ((watched[0].revents & POLLIN) != 0) drain(waited->own);
    if (count == 2u && (watched[1].revents & POLLIN) != 0) drain(waited->doorbell);
}

/* The renderer runs no display clock - the device in QEMU keeps it - but the
 * contract is the Host's, so the wait is the same poll bounded by the time left. */
bool PosixRendererHost::wait_event_until(void *event, uint64_t deadline)
{
    AV_SCOPE("PosixRendererHost::wait_event_until");
    constexpr uint64_t   nanoseconds_per_second = 1000000000u;
    RendererEvent *const waited = static_cast<RendererEvent *>(event);
    const uint64_t       now = monotonic_nanoseconds();
    const uint64_t       left = deadline > now ? deadline - now : 0u;
    const timespec       timeout = {static_cast<time_t>(left / nanoseconds_per_second),
                                    static_cast<long>(left % nanoseconds_per_second)};
    pollfd watched = {waited->own, POLLIN, 0};
    int    ready = ppoll(&watched, 1u, &timeout, nullptr);
    while (ready < 0 && errno == EINTR) ready = ppoll(&watched, 1u, &timeout, nullptr);
    if (ready <= 0 || (watched.revents & POLLIN) == 0) return false;
    drain(waited->own);
    return true;
}

Status PosixRendererHost::create_mutex(void *&mutex)
{
    AV_SCOPE("PosixRendererHost::create_mutex");
    mutex = nullptr;
    pthread_mutex_t *const made = static_cast<pthread_mutex_t *>(calloc(1u, sizeof(pthread_mutex_t)));
    if (made == nullptr) return Status::no_memory;
    if (pthread_mutex_init(made, nullptr) != 0) {
        free(made);
        return Status::no_memory;
    }
    mutex = made;
    return Status::ok;
}

void PosixRendererHost::destroy_mutex(void *mutex)
{
    AV_SCOPE("PosixRendererHost::destroy_mutex");
    pthread_mutex_destroy(static_cast<pthread_mutex_t *>(mutex));
    free(mutex);
}

void PosixRendererHost::lock_mutex(void *mutex)
{
    AV_SCOPE("PosixRendererHost::lock_mutex");
    pthread_mutex_lock(static_cast<pthread_mutex_t *>(mutex));
}

void PosixRendererHost::unlock_mutex(void *mutex)
{
    AV_SCOPE("PosixRendererHost::unlock_mutex");
    pthread_mutex_unlock(static_cast<pthread_mutex_t *>(mutex));
}

bool PosixRendererHost::try_lock_mutex(void *mutex)
{
    AV_SCOPE("PosixRendererHost::try_lock_mutex");
    return pthread_mutex_trylock(static_cast<pthread_mutex_t *>(mutex)) == 0;
}

/* A routine that cannot be queued is lost, as one QEMU could not schedule
 * would be; it is logged so the loss is not silent. */
void PosixRendererHost::run_on_main_loop(HostRoutine routine, void *context)
{
    AV_SCOPE("PosixRendererHost::run_on_main_loop");
    Posted *const posted = static_cast<Posted *>(calloc(1u, sizeof(Posted)));
    if (posted == nullptr) {
        log("a routine for the control thread could not be queued");
        return;
    }
    posted->routine_ = routine;
    posted->context_ = context;
    pthread_mutex_lock(&posted_lock_);
    if (posted_last_ != nullptr) posted_last_->next_ = posted;
    else posted_first_ = posted;
    posted_last_ = posted;
    pthread_mutex_unlock(&posted_lock_);
    if (wakeup_ >= 0) post(wakeup_);
}

uint32_t PosixRendererHost::gpu_count()
{
    AV_SCOPE("PosixRendererHost::gpu_count");
    return 0u;
}

GpuIdentity PosixRendererHost::gpu_identity(uint32_t)
{
    AV_SCOPE("PosixRendererHost::gpu_identity");
    return GpuIdentity();
}

Status PosixRendererHost::gpu_pci_address(uint32_t, PciAddress &)
{
    AV_SCOPE("PosixRendererHost::gpu_pci_address");
    return Status::not_supported;
}

Status PosixRendererHost::read_file(const char *path, char *buffer, size_t capacity, size_t &length)
{
    AV_SCOPE("PosixRendererHost::read_file");
    length = 0u;
    const int descriptor = open(path, O_RDONLY | O_CLOEXEC);
    if (descriptor < 0) return Status::not_found;
    const ssize_t got = read(descriptor, buffer, capacity);
    close(descriptor);
    if (got < 0) return Status::not_found;
    length = static_cast<size_t>(got);
    return Status::ok;
}

Status PosixRendererHost::list_directory(const char *, char *, size_t, size_t &length)
{
    AV_SCOPE("PosixRendererHost::list_directory");
    length = 0u;
    return Status::not_supported;
}

uint64_t PosixRendererHost::monotonic_nanoseconds()
{
    AV_SCOPE("PosixRendererHost::monotonic_nanoseconds");
    constexpr uint64_t nanoseconds_per_second = 1000000000u;
    timespec now = {};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<uint64_t>(now.tv_sec) * nanoseconds_per_second + static_cast<uint64_t>(now.tv_nsec);
}

void PosixRendererHost::fence_stores()
{
    AV_SCOPE("PosixRendererHost::fence_stores");
    __builtin_ia32_sfence();
}

void *PosixRendererHost::allocate(size_t bytes, Pool)
{
    AV_SCOPE("PosixRendererHost::allocate");
    return calloc(1u, bytes);
}

void PosixRendererHost::release(void *block)
{
    AV_SCOPE("PosixRendererHost::release");
    free(block);
}

/* The renderer's threads have no IRQL: every ceiling holds. */
void PosixRendererHost::require_irql_at_most(Irql, const char *)
{
    AV_SCOPE("PosixRendererHost::require_irql_at_most");
}

/* An extent QEMU encoded lies inside one region, so the mapping is whole or
 * nothing. */
void *PosixRendererHost::map_guest(uint64_t address, uint64_t &length, bool &direct)
{
    AV_SCOPE("PosixRendererHost::map_guest");
    void *const pointer = memory_.resolve(address, length);
    direct = pointer != nullptr;
    if (pointer == nullptr) length = 0u;
    return pointer;
}

void PosixRendererHost::unmap_guest(void *, uint64_t)
{
    AV_SCOPE("PosixRendererHost::unmap_guest");
}

Status PosixRendererHost::reserve_range(uint64_t bytes, void *&base)
{
    AV_SCOPE("PosixRendererHost::reserve_range");
    base = mmap(nullptr, bytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (base != MAP_FAILED) return Status::ok;
    base = nullptr;
    return Status::no_memory;
}

Status PosixRendererHost::alias_guest_run(void *run, uint64_t bytes, void *at)
{
    AV_SCOPE("PosixRendererHost::alias_guest_run");
    int      descriptor = -1;
    uint64_t offset = 0u;
    if (memory_.locate(run, bytes, descriptor, offset) != Status::ok) return Status::not_supported;
    const void *const mapped = mmap(at, bytes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, descriptor,
                                    static_cast<off_t>(offset));
    return mapped == MAP_FAILED ? Status::no_memory : Status::ok;
}

void PosixRendererHost::release_range(void *base, uint64_t bytes)
{
    AV_SCOPE("PosixRendererHost::release_range");
    munmap(base, bytes);
}

Status PosixRendererHost::overlay_guest_run(void *, uint64_t, int, uint64_t)
{
    AV_SCOPE("PosixRendererHost::overlay_guest_run");
    return Status::not_supported;
}

Status PosixRendererHost::restore_guest_run(void *, uint64_t)
{
    AV_SCOPE("PosixRendererHost::restore_guest_run");
    return Status::not_supported;
}

uint64_t PosixRendererHost::hostmem_size()
{
    AV_SCOPE("PosixRendererHost::hostmem_size");
    return 0u;
}

Status PosixRendererHost::place_in_hostmem(int, uint64_t, uint64_t)
{
    AV_SCOPE("PosixRendererHost::place_in_hostmem");
    return Status::not_supported;
}

void PosixRendererHost::remove_from_hostmem(uint64_t, uint64_t)
{
    AV_SCOPE("PosixRendererHost::remove_from_hostmem");
}

Status PosixRendererHost::bind_gpu(uint32_t)
{
    AV_SCOPE("PosixRendererHost::bind_gpu");
    return Status::not_supported;
}

/* The doorbell QEMU offered for page `index` joins the worker's wakeup. */
void PosixRendererHost::connect_doorbell(void *event, uint32_t index)
{
    AV_SCOPE("PosixRendererHost::connect_doorbell");
    RendererEvent *const connected = static_cast<RendererEvent *>(event);
    int                  descriptor = -1;
    if (doorbells_.take(index, descriptor) != Status::ok) return;
    if (connected->doorbell >= 0) close(connected->doorbell);
    connected->doorbell = descriptor;
}

void PosixRendererHost::disconnect_doorbell(void *event, uint32_t)
{
    AV_SCOPE("PosixRendererHost::disconnect_doorbell");
    RendererEvent *const disconnected = static_cast<RendererEvent *>(event);
    if (disconnected->doorbell >= 0) close(disconnected->doorbell);
    disconnected->doorbell = -1;
}

Status PosixRendererHost::connect_fence_interrupts()
{
    AV_SCOPE("PosixRendererHost::connect_fence_interrupts");
    interrupts_connected_ = true;
    return Status::ok;
}

void PosixRendererHost::disconnect_fence_interrupts()
{
    AV_SCOPE("PosixRendererHost::disconnect_fence_interrupts");
    interrupts_connected_ = false;
}

/* A write to a notifier QEMU has since torn down reaches an eventfd nothing
 * listens to, which is harmless. */
void PosixRendererHost::raise_fence_interrupt(uint32_t line)
{
    AV_SCOPE("PosixRendererHost::raise_fence_interrupt");
    if (!interrupts_connected_ || line >= max_interrupt_lines || interrupt_lines_[line] < 0) return;
    post(interrupt_lines_[line]);
}

size_t PosixRendererHost::read_request(void *request, size_t offset, uint8_t *buffer, size_t size)
{
    AV_SCOPE("PosixRendererHost::read_request");
    return static_cast<const ForwardedRequest *>(request)->read(offset, buffer, size);
}

/* The renderer answers QEMU with its own messages, never a virtio response. */
void PosixRendererHost::respond(void *, const uint8_t *, size_t)
{
    AV_SCOPE("PosixRendererHost::respond");
}

bool PosixRendererHost::resource_exists(uint32_t)
{
    AV_SCOPE("PosixRendererHost::resource_exists");
    return false;
}

Status PosixRendererHost::create_recording(const char *path, void *&file)
{
    AV_SCOPE("PosixRendererHost::create_recording");
    file = nullptr;
    const int descriptor = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (descriptor < 0) return Status::not_found;
    int *const kept = static_cast<int *>(calloc(1u, sizeof(int)));
    if (kept == nullptr) {
        close(descriptor);
        return Status::no_memory;
    }
    *kept = descriptor;
    file = kept;
    return Status::ok;
}

Status PosixRendererHost::append_recording(void *file, const uint8_t *bytes, size_t size)
{
    AV_SCOPE("PosixRendererHost::append_recording");
    const int descriptor = *static_cast<int *>(file);
    size_t    written = 0u;
    while (written < size) {
        const ssize_t now = write(descriptor, bytes + written, size - written);
        if (now < 0 && errno == EINTR) continue;
        if (now <= 0) return Status::no_space;
        written += static_cast<size_t>(now);
    }
    return Status::ok;
}

Status PosixRendererHost::truncate_recording(void *file)
{
    AV_SCOPE("PosixRendererHost::truncate_recording");
    const int descriptor = *static_cast<int *>(file);
    return ftruncate(descriptor, 0) == 0 && lseek(descriptor, 0, SEEK_SET) == 0 ? Status::ok : Status::no_space;
}

void PosixRendererHost::close_recording(void *file)
{
    AV_SCOPE("PosixRendererHost::close_recording");
    close(*static_cast<int *>(file));
    free(file);
}

bool PosixRendererHost::display_takes_dmabuf(uint32_t)
{
    AV_SCOPE("PosixRendererHost::display_takes_dmabuf");
    return false;
}

Status PosixRendererHost::show_scanout(uint32_t, const uint8_t *, const ScanoutFramebuffer &)
{
    AV_SCOPE("PosixRendererHost::show_scanout");
    return Status::not_supported;
}

Status PosixRendererHost::show_image(uint32_t, const AvExportedImage &)
{
    AV_SCOPE("PosixRendererHost::show_image");
    return Status::not_supported;
}

void PosixRendererHost::update_scanout(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)
{
    AV_SCOPE("PosixRendererHost::update_scanout");
}

void PosixRendererHost::hide_scanout(uint32_t)
{
    AV_SCOPE("PosixRendererHost::hide_scanout");
}

int PosixRendererHost::event_descriptor(void *event)
{
    AV_SCOPE("PosixRendererHost::event_descriptor");
    return static_cast<RendererEvent *>(event)->own;
}

int PosixRendererHost::fence_interrupt_descriptor(uint32_t)
{
    AV_SCOPE("PosixRendererHost::fence_interrupt_descriptor");
    return -1;
}

uint32_t PosixRendererHost::guest_region_count()
{
    AV_SCOPE("PosixRendererHost::guest_region_count");
    return 0u;
}

Status PosixRendererHost::guest_region(uint32_t, uint32_t &, int &descriptor, uint64_t &)
{
    AV_SCOPE("PosixRendererHost::guest_region");
    descriptor = -1;
    return Status::not_supported;
}

Status PosixRendererHost::locate_guest(const void *, uint64_t, uint32_t &, uint64_t &)
{
    AV_SCOPE("PosixRendererHost::locate_guest");
    return Status::not_found;
}

Status PosixRendererHost::start_renderer(const char *)
{
    AV_SCOPE("PosixRendererHost::start_renderer");
    return Status::not_supported;
}

Status PosixRendererHost::send_renderer(const uint8_t *, size_t, const int *, uint32_t)
{
    AV_SCOPE("PosixRendererHost::send_renderer");
    return Status::not_supported;
}

Status PosixRendererHost::receive_renderer(uint8_t *, size_t, size_t &length, int *, uint32_t, uint32_t &count,
                                           uint32_t)
{
    AV_SCOPE("PosixRendererHost::receive_renderer");
    length = 0u;
    count = 0u;
    return Status::not_supported;
}

void PosixRendererHost::kill_renderer()
{
    AV_SCOPE("PosixRendererHost::kill_renderer");
}

Status PosixRendererHost::receive_export(int, uint8_t *, size_t, size_t &length)
{
    AV_SCOPE("PosixRendererHost::receive_export");
    length = 0u;
    return Status::not_found;
}

Status PosixRendererHost::send_export(int, const uint8_t *, size_t, int)
{
    AV_SCOPE("PosixRendererHost::send_export");
    return Status::not_found;
}

void PosixRendererHost::shutdown_export(int)
{
    AV_SCOPE("PosixRendererHost::shutdown_export");
}

/* The renderer exports nothing (its plane composes no heads), so it has no
 * descriptor to duplicate. */
int PosixRendererHost::duplicate_export(int)
{
    AV_SCOPE("PosixRendererHost::duplicate_export");
    return -1;
}

void PosixRendererHost::close_export(int)
{
    AV_SCOPE("PosixRendererHost::close_export");
}

}  // namespace av
