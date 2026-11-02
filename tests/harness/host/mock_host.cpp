#include "harness/host/mock_host.hpp"

#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_request.hpp"
#include "harness/host/renderer_peer.hpp"
#include "device/renderer/forwarded_request.hpp"
#include "device/display/scanout_framebuffer.hpp"
#include "device/gpu/pci_address.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

namespace av {

namespace {

const char *const violation_names[] = {"main_loop_required", "worker_required", "unknown_handle", "held_mutex",
                                       "second_response", "overlapping_placement", "fence_misuse", "doorbell_misuse",
                                       "scanout_misuse"};

class StateLock {
public:
    explicit StateLock(pthread_mutex_t &mutex) : mutex_(mutex) { pthread_mutex_lock(&mutex_); }
    ~StateLock() { pthread_mutex_unlock(&mutex_); }
    StateLock(const StateLock &) = delete;
    StateLock &operator=(const StateLock &) = delete;

private:
    pthread_mutex_t &mutex_;
};

}

MockHost::MockHost()
    : main_loop_(pthread_self()), violations_{}, logged_(0u), last_log_{}, threads_{}, events_{}, mutexes_{}, gpus_{},
      gpu_count_(0u), gpu_buses_{}, gpu_on_pci_{}, files_{}, file_count_(0u), work_{}, work_count_(0u), fail_thread_(0u), fail_event_(0u), fail_mutex_(0u),
      fail_allocation_(0u), fail_placement_(0u), empty_mapping_(0u), allocations_{}, regions_{}, region_count_(0u),
      splits_{}, split_count_(0u),
      guest_mappings_(static_cast<GuestMapping *>(std::calloc(max_guest_mappings, sizeof(GuestMapping)))),
      placements_{}, placements_made_(0u), last_placement_{-1, 0u, 0u}, hostmem_size_(0u), resources_{}, resource_count_(0u), doorbells_{}, translate_(nullptr), translate_context_(nullptr), fence_connected_(false), fail_fence_connect_(0u), raised_{}, clock_(0u), clock_step_(0u), fail_bind_gpu_(0u), bound_gpu_(no_gpu), ranges_{},
      fail_reserve_(0u), fail_alias_(0u), counted_{},
      recordings_{}, fail_recording_create_(0u), fail_recording_append_(0u),
      fail_recording_truncate_(0u), display_heads_(0u), refuse_scanout_(0u), scanouts_{}, images_shown_{}, last_images_{},
      update_rects_{}, renderer_regions_{}, renderer_region_count_(0u), renderer_peer_(nullptr),
      renderer_running_(false), fail_renderer_start_(0u), renderer_starts_(0u), renderer_kills_(0u),
      renderer_datagrams_(0u), receive_timeout_(0u), forward_requests_(false), fail_event_descriptor_(0u), started_renderer_{},
      fail_export_send_(0u), export_sends_(0u), exports_closed_(0u)
{
    for (int &descriptor : counted_) descriptor = -1;
    pthread_mutex_init(&state_, nullptr);
    pthread_cond_init(&work_queued_, nullptr);
}

MockHost::~MockHost()
{
    TestRegistry &registry = TestRegistry::instance();
    for (std::uint32_t kind = 0u; kind < violation_kinds; kind++)
        if (violations_[kind] != 0u)
            registry.record_failure(__FILE__, __LINE__, "%u unclaimed %s violation(s)", violations_[kind],
                                    violation_names[kind]);
    if (live_threads() != 0u) registry.record_failure(__FILE__, __LINE__, "%u thread(s) never joined", live_threads());
    if (live_events() != 0u) registry.record_failure(__FILE__, __LINE__, "%u event(s) leaked", live_events());
    if (held_count_ != 0u)
        registry.record_failure(__FILE__, __LINE__, "%u held store(s) never fenced", held_count_);
    if (live_mutexes() != 0u) registry.record_failure(__FILE__, __LINE__, "%u mutex(es) leaked", live_mutexes());
    if (work_count_ != 0u)
        registry.record_failure(__FILE__, __LINE__, "%u main loop routine(s) never ran", work_count_);
    if (live_allocations() != 0u)
        registry.record_failure(__FILE__, __LINE__, "%u allocation(s) leaked", live_allocations());
    if (live_guest_mappings() != 0u)
        registry.record_failure(__FILE__, __LINE__, "%u guest mapping(s) leaked", live_guest_mappings());
    if (live_placements() != 0u)
        registry.record_failure(__FILE__, __LINE__, "%u hostmem placement(s) left", live_placements());
    if (live_descriptors() != 0u)
        registry.record_failure(__FILE__, __LINE__, "%u descriptor(s) never closed", live_descriptors());
    if (fence_connected_) registry.record_failure(__FILE__, __LINE__, "fence interrupts left connected");
    if (live_doorbells() != 0u)
        registry.record_failure(__FILE__, __LINE__, "%u doorbell(s) left connected", live_doorbells());
    if (live_ranges() != 0u)
        registry.record_failure(__FILE__, __LINE__, "%u reserved range(s) never released", live_ranges());
    if (live_recordings() != 0u)
        registry.record_failure(__FILE__, __LINE__, "%u recording(s) never closed", live_recordings());
    for (RecordingFile &recording : recordings_)
        if (recording.bytes != nullptr) std::free(recording.bytes);
    if (live_scanouts() != 0u)
        registry.record_failure(__FILE__, __LINE__, "%u display head(s) left showing something", live_scanouts());
    for (Range &range : ranges_)
        if (range.base != nullptr) munmap(range.base, range.bytes);
    for (void *block : allocations_) std::free(block);
    for (std::uint32_t index = 0u; index < max_guest_mappings; index++)
        if (guest_mappings_[index].bounce) std::free(guest_mappings_[index].pointer);
    std::free(guest_mappings_);
    for (std::uint32_t index = 0u; index < region_count_; index++) {
        if (regions_[index].bytes == nullptr) continue;
        munmap(regions_[index].bytes, regions_[index].mapped + page);
        close(regions_[index].fd);
    }
    for (LivePlacement &placement : placements_)
        if (placement.mapping != nullptr) ::munmap(placement.mapping, placement.asked.size);
    for (int descriptor : counted_)
        if (descriptor >= 0) ::close(descriptor);
    for (std::uint32_t index = 0u; index < renderer_region_count_; index++)
        if (renderer_regions_[index].descriptor >= 0) ::close(renderer_regions_[index].descriptor);
    for (Event &event : events_)
        if (event.live && event.descriptor >= 0) ::close(event.descriptor);
    pthread_cond_destroy(&work_queued_);
    pthread_mutex_destroy(&state_);
}

void MockHost::record(HostViolation kind, const char *function)
{
    StateLock lock(state_);
    violations_[static_cast<std::uint32_t>(kind)]++;
    std::fprintf(stderr, "    host violation %s: %s\n", violation_names[static_cast<std::uint32_t>(kind)], function);
}

bool MockHost::fail_now(std::uint32_t &countdown)
{
    if (countdown == 0u) return false;
    return --countdown == 0u;
}

void MockHost::log(const char *message)
{
    StateLock lock(state_);
    logged_++;
    std::snprintf(last_log_, sizeof last_log_, "%s", message);
    std::fprintf(stderr, "    host log: %s\n", message);
}

void MockHost::require_main_loop(const char *function)
{
    if (pthread_equal(pthread_self(), main_loop_) == 0) record(HostViolation::main_loop_required, function);
}

void MockHost::require_worker(const char *function)
{
    if (pthread_equal(pthread_self(), main_loop_) != 0) record(HostViolation::worker_required, function);
}

MockHost::Thread *MockHost::thread_of(void *thread)
{
    for (Thread &candidate : threads_)
        if (&candidate == thread && candidate.live) return &candidate;
    return nullptr;
}

MockHost::Event *MockHost::event_of(void *event)
{
    for (Event &candidate : events_)
        if (&candidate == event && candidate.live) return &candidate;
    return nullptr;
}

MockHost::Mutex *MockHost::mutex_of(void *mutex)
{
    for (Mutex &candidate : mutexes_)
        if (&candidate == mutex && candidate.live) return &candidate;
    return nullptr;
}

Status MockHost::start_thread(HostRoutine entry, void *context, void *&thread)
{
    thread = nullptr;
    Thread *slot = nullptr;
    {
        StateLock lock(state_);
        if (fail_now(fail_thread_)) return Status::no_memory;
        for (Thread &candidate : threads_)
            if (!candidate.live && slot == nullptr) slot = &candidate;
        if (slot == nullptr) return Status::no_memory;
        slot->entry = entry;
        slot->context = context;
        slot->live = true;
    }
    const auto run = [](void *record) -> void * {
        const Thread *const started = static_cast<Thread *>(record);
        started->entry(started->context);
        return nullptr;
    };
    if (pthread_create(&slot->handle, nullptr, run, slot) != 0) {
        StateLock lock(state_);
        slot->live = false;
        return Status::no_memory;
    }
    thread = slot;
    return Status::ok;
}

void MockHost::join_thread(void *thread)
{
    Thread *const slot = thread_of(thread);
    if (slot == nullptr || pthread_equal(slot->handle, pthread_self()) != 0) {
        record(HostViolation::unknown_handle, "MockHost::join_thread");
        return;
    }
    pthread_join(slot->handle, nullptr);
    StateLock lock(state_);
    slot->live = false;
}

Status MockHost::create_event(void *&event)
{
    StateLock lock(state_);
    event = nullptr;
    if (fail_now(fail_event_)) return Status::no_memory;
    for (Event &candidate : events_) {
        if (candidate.live) continue;
        pthread_mutex_init(&candidate.lock, nullptr);
        pthread_cond_init(&candidate.signalled, nullptr);
        candidate.set = false;
        candidate.live = true;
        candidate.descriptor = -1;
        candidate.sleepers = 0u;
        candidate.deadline = 0u;
        candidate.timed = false;
        event = &candidate;
        return Status::ok;
    }
    return Status::no_memory;
}

void MockHost::destroy_event(void *event)
{
    Event *const slot = event_of(event);
    if (slot == nullptr) {
        record(HostViolation::unknown_handle, "MockHost::destroy_event");
        return;
    }
    if (doorbell_of(event) != max_doorbells) {
        record(HostViolation::doorbell_misuse, "MockHost::destroy_event");
        return;
    }
    pthread_cond_destroy(&slot->signalled);
    pthread_mutex_destroy(&slot->lock);
    StateLock lock(state_);
    if (slot->descriptor >= 0) ::close(slot->descriptor);
    slot->descriptor = -1;
    slot->live = false;
}

void MockHost::signal_event(void *event)
{
    Event *const slot = event_of(event);
    if (slot == nullptr) {
        record(HostViolation::unknown_handle, "MockHost::signal_event");
        return;
    }
    pthread_mutex_lock(&slot->lock);
    slot->set = true;
    pthread_cond_signal(&slot->signalled);
    pthread_mutex_unlock(&slot->lock);
}

void MockHost::wait_event(void *event)
{
    Event *const slot = event_of(event);
    if (slot == nullptr) {
        record(HostViolation::unknown_handle, "MockHost::wait_event");
        return;
    }
    pthread_mutex_lock(&slot->lock);
    const bool sleeps = !slot->set;
    if (sleeps) slot->sleepers++;
    while (!slot->set) pthread_cond_wait(&slot->signalled, &slot->lock);
    if (sleeps) slot->sleepers--;
    slot->set = false;
    pthread_mutex_unlock(&slot->lock);
}

bool MockHost::wait_event_until(void *event, std::uint64_t deadline)
{
    Event *const slot = event_of(event);
    if (slot == nullptr) {
        record(HostViolation::unknown_handle, "MockHost::wait_event_until");
        return false;
    }
    pthread_mutex_lock(&slot->lock);
    const bool sleeps = !slot->set && clock_.load() < deadline;
    if (sleeps) slot->sleepers++;
    slot->deadline = deadline;
    slot->timed = sleeps;
    while (!slot->set && clock_.load() < deadline) pthread_cond_wait(&slot->signalled, &slot->lock);
    slot->timed = false;
    if (sleeps) slot->sleepers--;
    const bool signalled = slot->set;
    slot->set = false;
    pthread_mutex_unlock(&slot->lock);
    return signalled;
}

void MockHost::set_clock(std::uint64_t now)
{
    StateLock lock(state_);
    clock_.store(now);
    for (Event &slot : events_) {
        if (!slot.live) continue;
        pthread_mutex_lock(&slot.lock);
        pthread_cond_broadcast(&slot.signalled);
        pthread_mutex_unlock(&slot.lock);
    }
}

bool MockHost::next_deadline(std::uint64_t &deadline)
{
    StateLock lock(state_);
    bool found = false;
    for (Event &slot : events_) {
        if (!slot.live) continue;
        pthread_mutex_lock(&slot.lock);
        if (slot.timed && (!found || slot.deadline < deadline)) {
            deadline = slot.deadline;
            found = true;
        }
        pthread_mutex_unlock(&slot.lock);
    }
    return found;
}

bool MockHost::wait_for_deadline_after(std::uint64_t after, std::uint32_t milliseconds)
{
    const timespec pause = {0, 100000};
    for (std::uint32_t waited = 0u; waited <= milliseconds * 10u; waited++) {
        std::uint64_t deadline = 0u;
        if (next_deadline(deadline) && deadline > after) return true;
        nanosleep(&pause, nullptr);
    }
    return false;
}

Status MockHost::show_image(std::uint32_t head, const AvExportedImage &image)
{
    require_main_loop("MockHost::show_image");
    StateLock lock(state_);
    if (head >= max_scanout_heads) {
        record(HostViolation::scanout_misuse, "MockHost::show_image");
        return Status::invalid_parameter;
    }
    if (head >= display_heads_) return Status::not_supported;
    images_shown_[head]++;
    last_images_[head] = image;
    return Status::ok;
}

std::uint32_t MockHost::images_shown(std::uint32_t head) const
{
    return head < max_scanout_heads ? images_shown_[head] : 0u;
}

AvExportedImage MockHost::last_image(std::uint32_t head) const
{
    return head < max_scanout_heads ? last_images_[head] : AvExportedImage{};
}

std::uint32_t MockHost::doorbell_of(void *event)
{
    StateLock lock(state_);
    for (std::uint32_t index = 0u; index < max_doorbells; index++)
        if (doorbells_[index] == event) return index;
    return max_doorbells;
}

void MockHost::fence_stores()
{
    StateLock lock(state_);
    store_fences_++;
    if (held_count_ == 0u) return;
    for (std::uint32_t store = 0u; store < held_count_; store++)
        std::memcpy(held_stores_[store].at, held_stores_[store].bytes, held_stores_[store].size);
    held_count_ = 0u;
    if (watched_ != nullptr) std::memcpy(&watched_when_landed_, watched_, sizeof watched_when_landed_);
}

void MockHost::hold_store(void *at, const void *bytes, std::size_t size)
{
    StateLock lock(state_);
    if (held_count_ == max_held_stores || size > max_held_bytes) {
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "hold_store: %zu bytes past what is held", size);
        return;
    }
    HeldStore &held = held_stores_[held_count_++];
    held.at = at;
    held.size = size;
    std::memcpy(held.bytes, bytes, size);
}

void MockHost::watch(const void *word)
{
    StateLock lock(state_);
    watched_ = word;
}

std::uint32_t MockHost::store_fences()
{
    StateLock lock(state_);
    return store_fences_;
}

std::uint64_t MockHost::watched_when_landed()
{
    StateLock lock(state_);
    return watched_when_landed_;
}

std::uint64_t MockHost::monotonic_nanoseconds()
{
    StateLock lock(state_);
    const std::uint64_t now = clock_.load();
    clock_.store(now + clock_step_);
    return now;
}

Status MockHost::bind_gpu(std::uint32_t index)
{
    require_main_loop("MockHost::bind_gpu");
    StateLock lock(state_);
    if (fail_now(fail_bind_gpu_)) return Status::not_supported;
    bound_gpu_ = index;
    return Status::ok;
}

void MockHost::connect_doorbell(void *event, std::uint32_t index)
{
    require_main_loop("MockHost::connect_doorbell");
    bool connected = false;
    if (event_of(event) != nullptr && index < max_doorbells && doorbell_of(event) == max_doorbells) {
        StateLock lock(state_);
        connected = doorbells_[index] == nullptr;
        if (connected) doorbells_[index] = event;
    }
    if (!connected) record(HostViolation::doorbell_misuse, "MockHost::connect_doorbell");
}

void MockHost::disconnect_doorbell(void *event, std::uint32_t index)
{
    require_main_loop("MockHost::disconnect_doorbell");
    if (index >= max_doorbells || event == nullptr || doorbell_of(event) != index) {
        record(HostViolation::doorbell_misuse, "MockHost::disconnect_doorbell");
        return;
    }
    StateLock lock(state_);
    doorbells_[index] = nullptr;
}

Status MockHost::connect_fence_interrupts()
{
    require_main_loop("MockHost::connect_fence_interrupts");
    bool twice = false;
    {
        StateLock lock(state_);
        if (fail_now(fail_fence_connect_)) return Status::no_memory;
        twice = fence_connected_;
        fence_connected_ = true;
    }
    if (twice) record(HostViolation::fence_misuse, "MockHost::connect_fence_interrupts");
    return Status::ok;
}

void MockHost::disconnect_fence_interrupts()
{
    require_main_loop("MockHost::disconnect_fence_interrupts");
    bool connected = false;
    {
        StateLock lock(state_);
        connected = fence_connected_;
        fence_connected_ = false;
    }
    if (!connected) record(HostViolation::fence_misuse, "MockHost::disconnect_fence_interrupts");
}

void MockHost::raise_fence_interrupt(std::uint32_t line)
{
    bool raised = false;
    {
        StateLock lock(state_);
        raised = fence_connected_ && line < interrupt_lines;
        if (raised) raised_[line]++;
    }
    if (!raised) record(HostViolation::fence_misuse, "MockHost::raise_fence_interrupt");
}

void MockHost::fail_fence_connect_at(std::uint32_t nth) { StateLock lock(state_); fail_fence_connect_ = nth; }

bool MockHost::fence_interrupts_connected() { StateLock lock(state_); return fence_connected_; }

std::uint32_t MockHost::raised(std::uint32_t line)
{
    StateLock lock(state_);
    return line < interrupt_lines ? raised_[line] : 0u;
}

bool MockHost::ring_doorbell(std::uint32_t index)
{
    void *event = nullptr;
    {
        StateLock lock(state_);
        if (index < max_doorbells) event = doorbells_[index];
    }
    if (event != nullptr) signal_event(event);
    return event != nullptr;
}

Status MockHost::create_mutex(void *&mutex)
{
    StateLock lock(state_);
    mutex = nullptr;
    if (fail_now(fail_mutex_)) return Status::no_memory;
    for (Mutex &candidate : mutexes_) {
        if (candidate.live) continue;
        pthread_mutex_init(&candidate.lock, nullptr);
        candidate.locked = false;
        candidate.elsewhere = false;
        candidate.live = true;
        mutex = &candidate;
        return Status::ok;
    }
    return Status::no_memory;
}

void MockHost::destroy_mutex(void *mutex)
{
    Mutex *const slot = mutex_of(mutex);
    if (slot == nullptr) {
        record(HostViolation::unknown_handle, "MockHost::destroy_mutex");
        return;
    }
    if (slot->locked) {
        record(HostViolation::held_mutex, "MockHost::destroy_mutex");
        slot->locked = false;
        pthread_mutex_unlock(&slot->lock);
    }
    pthread_mutex_destroy(&slot->lock);
    StateLock lock(state_);
    slot->live = false;
}

void MockHost::lock_mutex(void *mutex)
{
    Mutex *const slot = mutex_of(mutex);
    if (slot == nullptr) {
        record(HostViolation::unknown_handle, "MockHost::lock_mutex");
        return;
    }
    pthread_mutex_lock(&slot->lock);
    slot->locked = true;
}

bool MockHost::try_lock_mutex(void *mutex)
{
    Mutex *const slot = mutex_of(mutex);
    if (slot == nullptr) {
        record(HostViolation::unknown_handle, "MockHost::try_lock_mutex");
        return false;
    }
    if (slot->elsewhere || pthread_mutex_trylock(&slot->lock) != 0) return false;
    slot->locked = true;
    return true;
}

void MockHost::hold_mutex_elsewhere(void *mutex, bool held)
{
    Mutex *const slot = mutex_of(mutex);
    if (slot == nullptr) {
        record(HostViolation::unknown_handle, "MockHost::hold_mutex_elsewhere");
        return;
    }
    slot->elsewhere = held;
}

void MockHost::unlock_mutex(void *mutex)
{
    Mutex *const slot = mutex_of(mutex);
    if (slot == nullptr) {
        record(HostViolation::unknown_handle, "MockHost::unlock_mutex");
        return;
    }
    if (!slot->locked) {
        record(HostViolation::held_mutex, "MockHost::unlock_mutex");
        return;
    }
    slot->locked = false;
    pthread_mutex_unlock(&slot->lock);
}

void MockHost::run_on_main_loop(HostRoutine routine, void *context)
{
    StateLock lock(state_);
    if (work_count_ == max_main_loop_work) {
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "main loop queue full");
        return;
    }
    work_[work_count_++] = MainLoopWork{routine, context};
    pthread_cond_broadcast(&work_queued_);
}

std::uint32_t MockHost::gpu_count()
{
    StateLock lock(state_);
    return gpu_count_;
}

GpuIdentity MockHost::gpu_identity(std::uint32_t index)
{
    if (index >= gpu_count()) {
        record(HostViolation::unknown_handle, "MockHost::gpu_identity");
        return GpuIdentity();
    }
    return gpus_[index];
}

Status MockHost::gpu_pci_address(std::uint32_t index, PciAddress &address)
{
    require_main_loop("MockHost::gpu_pci_address");
    if (index >= gpu_count()) {
        record(HostViolation::unknown_handle, "MockHost::gpu_pci_address");
        return Status::not_found;
    }
    PciBus bus = {};
    if (!gpu_bus(index, bus)) return Status::not_found;
    address = PciAddress(bus.domain, bus.bus, bus.device, bus.function);
    return Status::ok;
}

bool MockHost::gpu_bus(std::uint32_t index, PciBus &bus)
{
    StateLock lock(state_);
    if (index >= gpu_count_ || !gpu_on_pci_[index]) return false;
    bus = gpu_buses_[index];
    return true;
}

Status MockHost::read_file(const char *path, char *buffer, std::size_t capacity, std::size_t &length)
{
    require_main_loop("MockHost::read_file");
    length = 0u;
    for (std::uint32_t index = 0u; index < file_count_; index++) {
        if (std::strcmp(files_[index].path, path) != 0) continue;
        length = files_[index].length < capacity ? files_[index].length : capacity;
        std::memcpy(buffer, files_[index].text, length);
        return Status::ok;
    }
    return Status::not_found;
}

Status MockHost::list_directory(const char *path, char *buffer, std::size_t capacity, std::size_t &length)
{
    require_main_loop("MockHost::list_directory");
    length = 0u;
    const std::size_t prefix = std::strlen(path);
    bool              any = false;
    for (std::uint32_t index = 0u; index < file_count_; index++) {
        const char *const file = files_[index].path;
        if (std::strncmp(file, path, prefix) != 0 || file[prefix] != '/') continue;
        any = true;
        const char *const name = file + prefix + 1u;
        std::size_t       name_length = 0u;
        while (name[name_length] != '\0' && name[name_length] != '/') name_length++;
        bool listed = false;
        for (std::size_t at = 0u; at < length && !listed;) {
            std::size_t end = at;
            while (end < length && buffer[end] != '\n') end++;
            listed = end - at == name_length && std::strncmp(buffer + at, name, name_length) == 0;
            at = end + 1u;
        }
        if (listed) continue;
        if (length + name_length + 1u > capacity) break;
        std::memcpy(buffer + length, name, name_length);
        length += name_length;
        buffer[length++] = '\n';
    }
    return any ? Status::ok : Status::not_found;
}

void MockHost::place_gpu_on_pci(std::uint32_t index, const PciBus &bus)
{
    StateLock lock(state_);
    if (index >= max_gpus) {
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "no such GPU slot");
        return;
    }
    gpu_buses_[index] = bus;
    gpu_on_pci_[index] = true;
}

void MockHost::add_file(const char *path, const char *text, std::size_t length)
{
    if (file_count_ == max_files || std::strlen(path) >= max_path || length > max_file_bytes) {
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "the mock holds no such file");
        return;
    }
    HostFile &file = files_[file_count_++];
    std::memcpy(file.path, path, std::strlen(path) + 1u);
    std::memcpy(file.text, text, length);
    file.length = length;
}

void MockHost::add_gpu(const std::uint8_t *uuid)
{
    add_gpu(GpuIdentity(0x10DEu, 0x2684u, GpuIdentity::discrete_gpu, uuid, 24ull << 30, 32ull << 30, "mock gpu"));
}

void MockHost::add_gpu(const GpuIdentity &identity)
{
    StateLock lock(state_);
    if (gpu_count_ == max_gpus) {
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "too many GPUs");
        return;
    }
    gpus_[gpu_count_++] = identity;
}

void MockHost::fail_thread_start_at(std::uint32_t nth) { StateLock lock(state_); fail_thread_ = nth; }
void MockHost::fail_event_at(std::uint32_t nth) { StateLock lock(state_); fail_event_ = nth; }
void MockHost::fail_mutex_at(std::uint32_t nth) { StateLock lock(state_); fail_mutex_ = nth; }

bool MockHost::wait_for_sleepers(std::uint32_t threads, std::uint32_t milliseconds)
{
    const timespec pause = {0, 100000};
    for (std::uint32_t waited = 0u; waited <= milliseconds * 10u; waited++) {
        std::uint32_t sleeping = 0u;
        {
            StateLock lock(state_);
            for (Event &event : events_) {
                if (!event.live) continue;
                pthread_mutex_lock(&event.lock);
                sleeping += event.sleepers;
                pthread_mutex_unlock(&event.lock);
            }
        }
        if (sleeping >= threads) return true;
        nanosleep(&pause, nullptr);
    }
    return false;
}

bool MockHost::wait_for_main_loop_work(std::uint32_t work, std::uint32_t milliseconds)
{
    timespec deadline = {};
    clock_gettime(CLOCK_REALTIME, &deadline);
    const std::uint64_t nanoseconds = static_cast<std::uint64_t>(deadline.tv_nsec) + milliseconds * 1000000ull;
    deadline.tv_sec += static_cast<time_t>(nanoseconds / 1000000000ull);
    deadline.tv_nsec = static_cast<long>(nanoseconds % 1000000000ull);
    StateLock lock(state_);
    while (work_count_ < work)
        if (pthread_cond_timedwait(&work_queued_, &state_, &deadline) == ETIMEDOUT) return work_count_ >= work;
    return true;
}

std::uint32_t MockHost::run_main_loop()
{
    require_main_loop("MockHost::run_main_loop");
    std::uint32_t ran = 0u;
    for (;;) {
        MainLoopWork next = {};
        {
            StateLock lock(state_);
            if (work_count_ == 0u) return ran;
            next = work_[0];
            std::memmove(work_, work_ + 1, (work_count_ - 1u) * sizeof work_[0]);
            work_count_--;
        }
        next.routine(next.context);
        ran++;
    }
}

void *MockHost::allocate(std::size_t bytes, Pool)
{
    StateLock lock(state_);
    if (fail_now(fail_allocation_)) return nullptr;
    for (void *&slot : allocations_) {
        if (slot != nullptr) continue;
        slot = std::calloc(1u, bytes != 0u ? bytes : 1u);
        return slot;
    }
    return nullptr;
}

void MockHost::release(void *block)
{
    {
        StateLock lock(state_);
        for (void *&slot : allocations_) {
            if (slot != block || block == nullptr) continue;
            std::free(slot);
            slot = nullptr;
            return;
        }
    }
    record(HostViolation::unknown_handle, "MockHost::release");
}

void MockHost::require_irql_at_most(Irql, const char *) {}

MockHost::GuestRegion *MockHost::region_of(std::uint64_t address)
{
    for (std::uint32_t index = 0u; index < region_count_; index++)
        if (address >= regions_[index].base && address - regions_[index].base < regions_[index].size)
            return &regions_[index];
    return nullptr;
}

void *MockHost::map_guest(std::uint64_t address, std::uint64_t &length, bool &direct)
{
    require_main_loop("MockHost::map_guest");
    StateLock lock(state_);
    direct = false;
    if (fail_now(empty_mapping_)) {
        length = 0u;
        direct = true;
        return this;
    }
    GuestMapping *slot = nullptr;
    for (std::uint32_t index = 0u; index < max_guest_mappings && slot == nullptr; index++)
        if (guest_mappings_[index].pointer == nullptr) slot = &guest_mappings_[index];
    void *const translated = translate_ != nullptr && slot != nullptr ? translate_(translate_context_, address, length)
                                                                      : nullptr;
    if (translated != nullptr) {
        *slot = GuestMapping{translated, length, false};
        direct = true;
        return translated;
    }
    const GuestRegion *const region = region_of(address);
    if (region == nullptr || slot == nullptr) {
        length = 0u;
        return nullptr;
    }
    std::uint64_t available = region->size - (address - region->base);
    for (std::uint32_t index = 0u; index < split_count_; index++)
        if (splits_[index] > address && splits_[index] - address < available) available = splits_[index] - address;
    if (length > available) length = available;
    direct = region->bytes != nullptr;
    slot->bounce = !direct;
    slot->length = length;
    slot->pointer = direct ? region->bytes + (address - region->base) : std::calloc(1u, length + 1u);
    return slot->pointer;
}

void MockHost::unmap_guest(void *pointer, std::uint64_t length)
{
    require_main_loop("MockHost::unmap_guest");
    {
        StateLock lock(state_);
        for (std::uint32_t index = 0u; index < max_guest_mappings; index++) {
            GuestMapping &mapping = guest_mappings_[index];
            if (mapping.pointer != pointer || pointer == nullptr || mapping.length != length) continue;
            if (mapping.bounce) std::free(mapping.pointer);
            mapping = GuestMapping{};
            return;
        }
    }
    record(HostViolation::unknown_handle, "MockHost::unmap_guest");
}

Status MockHost::reserve_range(std::uint64_t bytes, void *&base)
{
    StateLock lock(state_);
    base = nullptr;
    if (fail_now(fail_reserve_)) return Status::no_memory;
    for (Range &range : ranges_) {
        if (range.base != nullptr) continue;
        void *const reserved = mmap(nullptr, bytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
        if (reserved == MAP_FAILED) return Status::no_memory;
        range = Range{reserved, bytes, 0u};
        base = reserved;
        return Status::ok;
    }
    return Status::no_memory;
}

MockHost::GuestRegion *MockHost::region_holding(void *pointer, std::uint64_t bytes)
{
    const auto *const byte = static_cast<std::uint8_t *>(pointer);
    for (std::uint32_t index = 0u; index < region_count_; index++) {
        GuestRegion &region = regions_[index];
        if (region.bytes == nullptr || byte < region.bytes) continue;
        const std::uint64_t offset = static_cast<std::uint64_t>(byte - region.bytes);
        if (offset < region.mapped && bytes <= region.mapped - offset) return &region;
    }
    return nullptr;
}

MockHost::Range *MockHost::range_holding(void *at, std::uint64_t bytes)
{
    const auto *const byte = static_cast<std::uint8_t *>(at);
    for (Range &range : ranges_) {
        const auto *const base = static_cast<std::uint8_t *>(range.base);
        if (range.base == nullptr || byte < base) continue;
        const std::uint64_t offset = static_cast<std::uint64_t>(byte - base);
        if (offset < range.bytes && bytes <= range.bytes - offset) return &range;
    }
    return nullptr;
}

Status MockHost::alias_guest_run(void *run, std::uint64_t bytes, void *at)
{
    if (!holds_range(at, bytes)) {
        record(HostViolation::unknown_handle, "MockHost::alias_guest_run");
        return Status::invalid_parameter;
    }
    StateLock lock(state_);
    if (fail_now(fail_alias_)) return Status::not_supported;
    Range *const range = range_holding(at, bytes);
    GuestRegion *const region = region_holding(run, bytes);
    if (region == nullptr || bytes == 0u || bytes % page != 0u) return Status::not_supported;
    const std::uint64_t offset = static_cast<std::uint64_t>(static_cast<std::uint8_t *>(run) - region->bytes);
    if (offset % page != 0u ||
        mmap(at, bytes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, region->fd, static_cast<off_t>(offset)) ==
            MAP_FAILED)
        return Status::not_supported;
    range->aliases++;
    return Status::ok;
}

void MockHost::release_range(void *base, std::uint64_t bytes)
{
    {
        StateLock lock(state_);
        for (Range &range : ranges_) {
            if (range.base != base || base == nullptr || range.bytes != bytes) continue;
            munmap(base, bytes);
            range = Range{};
            return;
        }
    }
    record(HostViolation::unknown_handle, "MockHost::release_range");
}

bool MockHost::holds_range(void *at, std::uint64_t bytes)
{
    StateLock lock(state_);
    return range_holding(at, bytes) != nullptr;
}

void MockHost::fail_reserve_at(std::uint32_t nth) { StateLock lock(state_); fail_reserve_ = nth; }
void MockHost::fail_alias_at(std::uint32_t nth) { StateLock lock(state_); fail_alias_ = nth; }
void MockHost::fail_overlay_at(std::uint32_t nth) { StateLock lock(state_); fail_overlay_ = nth; }
void MockHost::fail_restore_at(std::uint32_t nth) { StateLock lock(state_); fail_restore_ = nth; }

std::uint64_t MockHost::overlaid_bytes()
{
    StateLock lock(state_);
    return overlaid_;
}

namespace {

bool move_run(void *run, std::uint64_t bytes, int fd, std::uint64_t offset)
{
    void *const staging = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, static_cast<off_t>(offset));
    if (staging == MAP_FAILED) return false;
    std::memcpy(staging, run, bytes);
    munmap(staging, bytes);
    return mmap(run, bytes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, static_cast<off_t>(offset)) !=
           MAP_FAILED;
}

}

Status MockHost::overlay_guest_run(void *run, std::uint64_t bytes, int fd, std::uint64_t offset)
{
    StateLock lock(state_);
    if (fail_now(fail_overlay_)) return Status::not_supported;
    GuestRegion *const region = region_holding(run, bytes);
    if (region == nullptr || bytes == 0u || bytes % page != 0u || offset % page != 0u) return Status::not_supported;
    if (!move_run(run, bytes, fd, offset)) return Status::not_supported;
    overlaid_ += bytes;
    return Status::ok;
}

bool MockHost::pause_guest()
{
    StateLock lock(state_);
    guest_pauses_++;
    return true;
}

void MockHost::resume_guest()
{
    StateLock lock(state_);
    guest_resumes_++;
}

std::uint32_t MockHost::guest_pauses() const { return guest_pauses_; }
std::uint32_t MockHost::guest_resumes() const { return guest_resumes_; }

Status MockHost::restore_guest_run(void *run, std::uint64_t bytes)
{
    StateLock lock(state_);
    if (fail_now(fail_restore_)) return Status::not_supported;
    GuestRegion *const region = region_holding(run, bytes);
    if (region == nullptr || bytes > overlaid_) return Status::not_supported;
    const std::uint64_t offset = static_cast<std::uint64_t>(static_cast<std::uint8_t *>(run) - region->bytes);
    if (!move_run(run, bytes, region->fd, offset)) return Status::not_supported;
    overlaid_ -= bytes;
    return Status::ok;
}

int MockHost::open_descriptor()
{
    const int descriptor = fresh_descriptor();
    if (descriptor >= 0) count_descriptor(descriptor);
    return descriptor;
}

void MockHost::count_descriptor(int descriptor)
{
    StateLock lock(state_);
    for (int &slot : counted_) {
        if (slot >= 0) continue;
        slot = descriptor;
        return;
    }
    std::abort();
}

std::uint32_t MockHost::live_descriptors() const
{
    std::uint32_t live = 0u;
    for (int descriptor : counted_) live += descriptor >= 0 ? 1u : 0u;
    return live;
}

int MockHost::fresh_descriptor()
{
    int ends[2] = {-1, -1};
    if (::pipe2(ends, O_CLOEXEC) != 0) return -1;
    ::close(ends[1]);
    return ends[0];
}

bool MockHost::descriptor_open(int fd) const
{
    return fd >= 0 && ::fcntl(fd, F_GETFD) != -1;
}

bool MockHost::display_takes_dmabuf(std::uint32_t head)
{
    StateLock lock(state_);
    return head < display_heads_;
}

Status MockHost::show_scanout(std::uint32_t head, const std::uint8_t *blob, const ScanoutFramebuffer &frame)
{
    const Plane plane = {frame.rect().width(), frame.rect().height(), frame.rect().x(),  frame.rect().y(),
                         frame.width(),        frame.height(),        frame.stride(),    frame.offset(),
                         frame.fourcc()};
    return show_scanout_plane(head, blob, plane);
}

Status MockHost::show_scanout_plane(std::uint32_t head, const std::uint8_t *blob, const Plane &plane)
{
    StateLock lock(state_);
    if (head >= display_heads_ || head >= max_scanout_heads) return Status::not_supported;
    if (blob == nullptr) {
        violations_[static_cast<std::uint32_t>(HostViolation::scanout_misuse)]++;
        return Status::not_supported;
    }
    if (fail_now(refuse_scanout_)) return Status::not_supported;
    Scanout &scanout = scanouts_[head];
    scanout.shown = true;
    scanout.blob = blob;
    scanout.plane = plane;
    scanout.shows++;
    return Status::ok;
}

void MockHost::update_scanout(std::uint32_t head, std::uint32_t x, std::uint32_t y, std::uint32_t width,
                              std::uint32_t height)
{
    {
        StateLock lock(state_);
        if (head < max_scanout_heads && scanouts_[head].shown) {
            scanouts_[head].updates++;
            update_rects_[head][0] = x;
            update_rects_[head][1] = y;
            update_rects_[head][2] = width;
            update_rects_[head][3] = height;
            return;
        }
    }
    record(HostViolation::scanout_misuse, "MockHost::update_scanout");
}

void MockHost::hide_scanout(std::uint32_t head)
{
    {
        StateLock lock(state_);
        if (head >= max_scanout_heads) {
            record(HostViolation::scanout_misuse, "MockHost::hide_scanout");
            return;
        }
        const std::uint32_t shows = scanouts_[head].shows;
        const std::uint32_t updates = scanouts_[head].updates;
        const std::uint32_t hides = scanouts_[head].hides;
        scanouts_[head] = Scanout{};
        scanouts_[head].shows = shows;
        scanouts_[head].updates = updates;
        scanouts_[head].hides = hides + 1u;
    }
}

void MockHost::add_display_heads(std::uint32_t heads)
{
    StateLock lock(state_);
    display_heads_ = heads < max_scanout_heads ? heads : max_scanout_heads;
}

void MockHost::refuse_scanout_at(std::uint32_t nth) { StateLock lock(state_); refuse_scanout_ = nth; }

MockHost::Scanout MockHost::head(std::uint32_t head) const
{
    return head < max_scanout_heads ? scanouts_[head] : Scanout{};
}

std::uint32_t MockHost::live_scanouts() const
{
    std::uint32_t showing = 0u;
    for (const Scanout &scanout : scanouts_)
        if (scanout.shown) showing++;
    return showing;
}

void MockHost::last_update(std::uint32_t head, std::uint32_t &x, std::uint32_t &y, std::uint32_t &width,
                           std::uint32_t &height) const
{
    const std::uint32_t index = head < max_scanout_heads ? head : 0u;
    x = update_rects_[index][0];
    y = update_rects_[index][1];
    width = update_rects_[index][2];
    height = update_rects_[index][3];
}

std::uint32_t MockHost::live_ranges()
{
    StateLock lock(state_);
    std::uint32_t count = 0u;
    for (const Range &range : ranges_) count += range.base != nullptr ? 1u : 0u;
    return count;
}

std::uint32_t MockHost::live_aliases()
{
    StateLock lock(state_);
    std::uint32_t count = 0u;
    for (const Range &range : ranges_) count += range.aliases;
    return count;
}

std::uint64_t MockHost::hostmem_size()
{
    StateLock lock(state_);
    return hostmem_size_;
}

Status MockHost::place_in_hostmem(int fd, std::uint64_t size, std::uint64_t offset)
{
    require_main_loop("MockHost::place_in_hostmem");
    if (!descriptor_open(fd)) {
        record(HostViolation::unknown_handle, "MockHost::place_in_hostmem");
        return Status::invalid_parameter;
    }
    {
        StateLock lock(state_);
        for (const LivePlacement &other : placements_)
            if (other.mapping != nullptr && offset < other.asked.offset + other.asked.size &&
                other.asked.offset < offset + size)
                violations_[static_cast<std::uint32_t>(HostViolation::overlapping_placement)]++;
        if (fail_now(fail_placement_)) return Status::no_memory;
    }
    struct stat file = {};
    if (size == 0u || ::fstat(fd, &file) != 0 || static_cast<std::uint64_t>(file.st_size) < size)
        return Status::no_memory;
    void *const mapping = ::mmap(nullptr, static_cast<std::size_t>(size), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapping == MAP_FAILED) return Status::no_memory;
    StateLock lock(state_);
    for (LivePlacement &slot : placements_) {
        if (slot.mapping != nullptr) continue;
        slot = LivePlacement{mapping, Placement{fd, size, offset}};
        placements_made_++;
        last_placement_ = slot.asked;
        return Status::ok;
    }
    ::munmap(mapping, static_cast<std::size_t>(size));
    return Status::no_memory;
}

void MockHost::remove_from_hostmem(std::uint64_t offset, std::uint64_t size)
{
    require_main_loop("MockHost::remove_from_hostmem");
    {
        StateLock lock(state_);
        for (LivePlacement &slot : placements_) {
            if (slot.mapping == nullptr || slot.asked.offset != offset || slot.asked.size != size) continue;
            ::munmap(slot.mapping, static_cast<std::size_t>(size));
            slot = LivePlacement{};
            return;
        }
    }
    record(HostViolation::unknown_handle, "MockHost::remove_from_hostmem");
}

std::uint32_t MockHost::placements() const
{
    return placements_made_;
}

MockHost::Placement MockHost::last_placement() const
{
    return last_placement_;
}

std::size_t MockHost::read_request(void *request, std::size_t offset, std::uint8_t *buffer, std::size_t size)
{
    require_main_loop("MockHost::read_request");
    if (forward_requests_) return static_cast<const ForwardedRequest *>(request)->read(offset, buffer, size);
    return static_cast<MockRequest *>(request)->read(offset, buffer, size);
}

void MockHost::respond(void *request, const std::uint8_t *bytes, std::size_t size)
{
    require_main_loop("MockHost::respond");
    if (!static_cast<MockRequest *>(request)->answer(bytes, size))
        record(HostViolation::second_response, "MockHost::respond");
}

bool MockHost::resource_exists(std::uint32_t id)
{
    StateLock lock(state_);
    for (std::uint32_t index = 0u; index < resource_count_; index++)
        if (resources_[index] == id) return true;
    return false;
}

void MockHost::fail_allocation_at(std::uint32_t nth) { StateLock lock(state_); fail_allocation_ = nth; }
void MockHost::fail_placement_at(std::uint32_t nth) { StateLock lock(state_); fail_placement_ = nth; }
void MockHost::empty_mapping_at(std::uint32_t nth) { StateLock lock(state_); empty_mapping_ = nth; }
void MockHost::fail_bind_gpu_at(std::uint32_t nth) { StateLock lock(state_); fail_bind_gpu_ = nth; }
void MockHost::advance_clock_per_read(std::uint64_t nanoseconds) { StateLock lock(state_); clock_step_ = nanoseconds; }

void MockHost::add_guest_ram(std::uint64_t base, std::size_t size)
{
    StateLock lock(state_);
    const std::uint64_t mapped = (size + page - 1u) / page * page;
    const int           fd = memfd_create("mock guest ram", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    void *const reserved = mmap(nullptr, mapped + page, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (fd < 0 || ftruncate(fd, static_cast<off_t>(mapped)) != 0 ||
        fcntl(fd, F_ADD_SEALS, F_SEAL_SHRINK) != 0 || reserved == MAP_FAILED ||
        mmap(reserved, mapped, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0) == MAP_FAILED)
        std::abort();
    regions_[region_count_++] = GuestRegion{base, size, static_cast<std::uint8_t *>(reserved), fd, mapped};
}

void MockHost::split_guest_memory_at(std::uint64_t address)
{
    StateLock lock(state_);
    splits_[split_count_++] = address;
}

void MockHost::add_bounce_region(std::uint64_t base, std::uint64_t size)
{
    StateLock lock(state_);
    regions_[region_count_++] = GuestRegion{base, size, nullptr, -1, 0u};
}

std::uint8_t *MockHost::guest_ram(std::uint64_t address)
{
    StateLock lock(state_);
    const GuestRegion *const region = region_of(address);
    return region != nullptr && region->bytes != nullptr ? region->bytes + (address - region->base) : nullptr;
}

void MockHost::translate_guest_with(GuestTranslator translate, void *context)
{
    StateLock lock(state_);
    translate_ = translate;
    translate_context_ = context;
}

void MockHost::set_hostmem_size(std::uint64_t size) { StateLock lock(state_); hostmem_size_ = size; }

void MockHost::add_resource(std::uint32_t id)
{
    StateLock lock(state_);
    resources_[resource_count_++] = id;
}

void *MockHost::placed_at(std::uint64_t offset)
{
    StateLock lock(state_);
    for (const LivePlacement &slot : placements_)
        if (slot.mapping != nullptr && slot.asked.offset == offset) return slot.mapping;
    return nullptr;
}

std::uint32_t MockHost::live_allocations()
{
    StateLock lock(state_);
    std::uint32_t live = 0u;
    for (void *block : allocations_) live += block != nullptr ? 1u : 0u;
    return live;
}

std::uint32_t MockHost::live_guest_mappings()
{
    StateLock lock(state_);
    std::uint32_t live = 0u;
    for (std::uint32_t index = 0u; index < max_guest_mappings; index++)
        live += guest_mappings_[index].pointer != nullptr ? 1u : 0u;
    return live;
}

std::uint32_t MockHost::bound_gpu()
{
    StateLock lock(state_);
    return bound_gpu_;
}

std::uint32_t MockHost::live_placements()
{
    StateLock lock(state_);
    std::uint32_t live = 0u;
    for (const LivePlacement &slot : placements_) live += slot.mapping != nullptr ? 1u : 0u;
    return live;
}

bool MockHost::holds_main_loop() const
{
    return pthread_equal(pthread_self(), main_loop_) != 0;
}

bool MockHost::claim(HostViolation kind)
{
    StateLock lock(state_);
    std::uint32_t &count = violations_[static_cast<std::uint32_t>(kind)];
    if (count == 0u) return false;
    count--;
    return true;
}

std::uint32_t MockHost::unclaimed_violations()
{
    StateLock lock(state_);
    std::uint32_t total = 0u;
    for (std::uint32_t kind = 0u; kind < violation_kinds; kind++) total += violations_[kind];
    return total;
}

std::uint32_t MockHost::logged()
{
    StateLock lock(state_);
    return logged_;
}

const char *MockHost::last_log()
{
    StateLock lock(state_);
    return last_log_;
}

std::uint32_t MockHost::live_threads()
{
    StateLock lock(state_);
    std::uint32_t live = 0u;
    for (const Thread &thread : threads_) live += thread.live ? 1u : 0u;
    return live;
}

std::uint32_t MockHost::live_doorbells()
{
    StateLock lock(state_);
    std::uint32_t live = 0u;
    for (void *event : doorbells_) live += event != nullptr ? 1u : 0u;
    return live;
}

std::uint32_t MockHost::live_events()
{
    StateLock lock(state_);
    std::uint32_t live = 0u;
    for (const Event &event : events_) live += event.live ? 1u : 0u;
    return live;
}

std::uint32_t MockHost::live_mutexes()
{
    StateLock lock(state_);
    std::uint32_t live = 0u;
    for (const Mutex &mutex : mutexes_) live += mutex.live ? 1u : 0u;
    return live;
}

MockHost::RecordingFile *MockHost::recording_of(void *file)
{
    for (RecordingFile &recording : recordings_)
        if (&recording == file) return &recording;
    record(HostViolation::unknown_handle, "recording");
    return nullptr;
}

MockHost::RecordingFile *MockHost::recording_at(const char *path)
{
    for (RecordingFile &recording : recordings_)
        if (recording.bytes != nullptr && std::strncmp(recording.path, path, max_path) == 0) return &recording;
    return nullptr;
}

Status MockHost::create_recording(const char *path, void *&file)
{
    if (!holds_main_loop()) record(HostViolation::main_loop_required, "create_recording");
    StateLock lock(state_);
    if (fail_now(fail_recording_create_)) return Status::no_memory;
    RecordingFile *slot = nullptr;
    for (RecordingFile &recording : recordings_) {
        if (recording.bytes == nullptr) { slot = &recording; break; }
    }
    if (slot == nullptr) return Status::no_memory;
    slot->bytes = static_cast<std::uint8_t *>(std::calloc(1u, recording_capacity));
    if (slot->bytes == nullptr) return Status::no_memory;
    std::snprintf(slot->path, sizeof slot->path, "%s", path != nullptr ? path : "");
    slot->length = 0u;
    slot->live = true;
    file = slot;
    return Status::ok;
}

Status MockHost::append_recording(void *file, const std::uint8_t *bytes, std::size_t size)
{
    StateLock lock(state_);
    RecordingFile *const recording = recording_of(file);
    if (recording == nullptr || !recording->live) return Status::no_space;
    if (fail_now(fail_recording_append_)) return Status::no_space;
    if (size > recording_capacity - recording->length) return Status::no_space;
    std::memcpy(recording->bytes + recording->length, bytes, size);
    recording->length += size;
    return Status::ok;
}

Status MockHost::truncate_recording(void *file)
{
    StateLock lock(state_);
    RecordingFile *const recording = recording_of(file);
    if (recording == nullptr || !recording->live) return Status::no_space;
    if (fail_now(fail_recording_truncate_)) return Status::no_space;
    recording->length = 0u;
    return Status::ok;
}

void MockHost::close_recording(void *file)
{
    StateLock lock(state_);
    RecordingFile *const recording = recording_of(file);
    if (recording == nullptr) return;
    recording->live = false;
}

void MockHost::fail_recording_create_at(std::uint32_t nth) { StateLock lock(state_); fail_recording_create_ = nth; }
void MockHost::fail_recording_append_at(std::uint32_t nth) { StateLock lock(state_); fail_recording_append_ = nth; }
void MockHost::fail_recording_truncate_at(std::uint32_t nth)
{
    StateLock lock(state_);
    fail_recording_truncate_ = nth;
}

const std::uint8_t *MockHost::recording_bytes(const char *path)
{
    StateLock lock(state_);
    const RecordingFile *const recording = recording_at(path);
    return recording != nullptr ? recording->bytes : nullptr;
}

std::size_t MockHost::recording_length(const char *path)
{
    StateLock lock(state_);
    const RecordingFile *const recording = recording_at(path);
    return recording != nullptr ? recording->length : 0u;
}

std::uint32_t MockHost::live_recordings()
{
    std::uint32_t live = 0u;
    for (const RecordingFile &recording : recordings_) live += recording.live ? 1u : 0u;
    return live;
}

int MockHost::event_descriptor(void *event)
{
    require_main_loop("MockHost::event_descriptor");
    Event *const slot = event_of(event);
    if (slot == nullptr) {
        record(HostViolation::unknown_handle, "MockHost::event_descriptor");
        return -1;
    }
    StateLock lock(state_);
    if (fail_now(fail_event_descriptor_)) return -1;
    if (slot->descriptor < 0) slot->descriptor = fresh_descriptor();
    return slot->descriptor;
}

int MockHost::fence_interrupt_descriptor(std::uint32_t line)
{
    require_main_loop("MockHost::fence_interrupt_descriptor");
    {
        StateLock lock(state_);
        if (!fence_connected_ || line >= interrupt_lines) return -1;
    }
    return open_descriptor();
}

std::uint32_t MockHost::guest_region_count()
{
    require_main_loop("MockHost::guest_region_count");
    StateLock lock(state_);
    return renderer_region_count_;
}

Status MockHost::guest_region(std::uint32_t index, std::uint32_t &region, int &descriptor, std::uint64_t &size)
{
    require_main_loop("MockHost::guest_region");
    descriptor = -1;
    RendererRegion scripted = {};
    {
        StateLock lock(state_);
        if (index >= renderer_region_count_) return Status::not_supported;
        scripted = renderer_regions_[index];
    }
    if (scripted.pointer == nullptr) return Status::not_supported;
    const int duplicate = ::fcntl(scripted.descriptor, F_DUPFD_CLOEXEC, 0);
    if (duplicate < 0) return Status::not_supported;
    count_descriptor(duplicate);
    StateLock lock(state_);
    region = scripted.region;
    descriptor = duplicate;
    size = scripted.size;
    return Status::ok;
}

Status MockHost::locate_guest(const void *pointer, std::uint64_t length, std::uint32_t &region,
                              std::uint64_t &offset)
{
    require_main_loop("MockHost::locate_guest");
    StateLock lock(state_);
    const auto *const byte = static_cast<const std::uint8_t *>(pointer);
    for (std::uint32_t index = 0u; index < renderer_region_count_; index++) {
        const RendererRegion &scripted = renderer_regions_[index];
        const auto *const base = static_cast<const std::uint8_t *>(scripted.pointer);
        if (base == nullptr || byte < base) continue;
        const std::uint64_t at = static_cast<std::uint64_t>(byte - base);
        if (at >= scripted.size || length > scripted.size - at) continue;
        region = scripted.region;
        offset = at;
        return Status::ok;
    }
    return Status::not_found;
}

void MockHost::close_descriptor(int descriptor)
{
    if (!descriptor_open(descriptor)) {
        record(HostViolation::unknown_handle, "MockHost::close_descriptor");
        return;
    }
    StateLock lock(state_);
    for (int &slot : counted_)
        if (slot == descriptor) slot = -1;
    ::close(descriptor);
}

Status MockHost::start_renderer(const char *path)
{
    require_main_loop("MockHost::start_renderer");
    RendererPeer *peer = nullptr;
    {
        StateLock lock(state_);
        if (fail_now(fail_renderer_start_)) return Status::not_found;
        renderer_running_ = true;
        renderer_starts_++;
        std::snprintf(started_renderer_, sizeof started_renderer_, "%s", path != nullptr ? path : "");
        peer = renderer_peer_;
    }
    if (peer != nullptr) peer->started();
    return Status::ok;
}

Status MockHost::send_renderer(const std::uint8_t *bytes, std::size_t length, const int *descriptors,
                               std::uint32_t count)
{
    require_main_loop("MockHost::send_renderer");
    RendererPeer *peer = nullptr;
    {
        StateLock lock(state_);
        if (!renderer_running_) return Status::not_found;
        renderer_datagrams_++;
        peer = renderer_peer_;
    }
    if (peer == nullptr || count > max_renderer_descriptors) return Status::ok;
    int delivered[max_renderer_descriptors] = {};
    for (std::uint32_t index = 0u; index < count; index++)
        delivered[index] = descriptors[index] >= 0 ? ::fcntl(descriptors[index], F_DUPFD_CLOEXEC, 0) : -1;
    peer->deliver(bytes, length, delivered, count);
    return Status::ok;
}

Status MockHost::receive_renderer(std::uint8_t *buffer, std::size_t capacity, std::size_t &length, int *descriptors,
                                  std::uint32_t capacity_descriptors, std::uint32_t &count, std::uint32_t timeout_ms)
{
    require_main_loop("MockHost::receive_renderer");
    length = 0u;
    count = 0u;
    RendererPeer *peer = nullptr;
    {
        StateLock lock(state_);
        receive_timeout_ = timeout_ms;
        if (!renderer_running_) return Status::not_found;
        peer = renderer_peer_;
    }
    std::size_t   got = 0u;
    std::uint32_t taken = 0u;
    if (peer == nullptr || !peer->answer(buffer, capacity, got, descriptors, capacity_descriptors, taken))
        return Status::invalid_state;
    length = got;
    count = taken;
    for (std::uint32_t index = 0u; index < taken && index < capacity_descriptors; index++)
        if (descriptors[index] >= 0) count_descriptor(descriptors[index]);
    return Status::ok;
}

void MockHost::kill_renderer()
{
    require_main_loop("MockHost::kill_renderer");
    StateLock lock(state_);
    renderer_kills_++;
    renderer_running_ = false;
}

Status MockHost::receive_export(int connection, std::uint8_t *buffer, std::size_t capacity, std::size_t &length)
{
    length = 0u;
    alignas(cmsghdr) char control[CMSG_SPACE(sizeof(int) * 4u)] = {};
    iovec  vector = {buffer, capacity};
    msghdr message = {};
    message.msg_iov = &vector;
    message.msg_iovlen = 1u;
    message.msg_control = control;
    message.msg_controllen = sizeof control;
    ssize_t got = -1;
    do {
        got = ::recvmsg(connection, &message, MSG_TRUNC | MSG_CMSG_CLOEXEC);
    } while (got < 0 && errno == EINTR);
    for (cmsghdr *header = CMSG_FIRSTHDR(&message); header != nullptr; header = CMSG_NXTHDR(&message, header)) {
        if (header->cmsg_level != SOL_SOCKET || header->cmsg_type != SCM_RIGHTS) continue;
        const std::size_t carried = (header->cmsg_len - CMSG_LEN(0)) / sizeof(int);
        for (std::size_t index = 0u; index < carried; index++) {
            int descriptor = -1;
            std::memcpy(&descriptor, CMSG_DATA(header) + index * sizeof(int), sizeof descriptor);
            ::close(descriptor);
        }
    }
    if (got <= 0) return Status::not_found;
    length = static_cast<std::size_t>(got);
    return Status::ok;
}

Status MockHost::send_export(int connection, const std::uint8_t *bytes, std::size_t length, int descriptor)
{
    {
        StateLock lock(state_);
        if (fail_now(fail_export_send_)) return Status::not_found;
        export_sends_++;
    }
    alignas(cmsghdr) char control[CMSG_SPACE(sizeof(int))] = {};
    iovec  vector = {const_cast<std::uint8_t *>(bytes), length};
    msghdr message = {};
    message.msg_iov = &vector;
    message.msg_iovlen = 1u;
    if (descriptor >= 0) {
        message.msg_control = control;
        message.msg_controllen = sizeof control;
        cmsghdr *const header = CMSG_FIRSTHDR(&message);
        header->cmsg_level = SOL_SOCKET;
        header->cmsg_type = SCM_RIGHTS;
        header->cmsg_len = CMSG_LEN(sizeof(int));
        std::memcpy(CMSG_DATA(header), &descriptor, sizeof descriptor);
    }
    return ::sendmsg(connection, &message, MSG_DONTWAIT | MSG_NOSIGNAL) == static_cast<ssize_t>(length)
               ? Status::ok
               : Status::not_found;
}

void MockHost::shutdown_export(int connection)
{
    ::shutdown(connection, SHUT_RDWR);
}

int MockHost::duplicate_export(int descriptor)
{
    return ::fcntl(descriptor, F_DUPFD_CLOEXEC, 0);
}

void MockHost::close_export(int descriptor)
{
    {
        StateLock lock(state_);
        exports_closed_++;
        for (int &slot : counted_)
            if (slot == descriptor) slot = -1;
    }
    ::close(descriptor);
}

void MockHost::fail_export_send_at(std::uint32_t nth)
{
    StateLock lock(state_);
    fail_export_send_ = nth;
}

std::uint32_t MockHost::export_sends()
{
    StateLock lock(state_);
    return export_sends_;
}

std::uint32_t MockHost::exports_closed()
{
    StateLock lock(state_);
    return exports_closed_;
}

int MockHost::add_guest_region(std::uint32_t region, void *pointer, std::uint64_t size)
{
    StateLock lock(state_);
    if (renderer_region_count_ == max_renderer_regions) {
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "too many guest regions");
        return -1;
    }
    const int descriptor = fresh_descriptor();
    struct stat file = {};
    if (descriptor < 0 || ::fstat(descriptor, &file) != 0) std::abort();
    renderer_regions_[renderer_region_count_++] =
        RendererRegion{region, pointer, size, descriptor, static_cast<std::uint64_t>(file.st_dev),
                       static_cast<std::uint64_t>(file.st_ino)};
    return descriptor;
}

void *MockHost::guest_region_mapping(int descriptor, std::uint64_t size)
{
    struct stat file = {};
    if (descriptor < 0 || ::fstat(descriptor, &file) != 0) return nullptr;
    StateLock lock(state_);
    for (std::uint32_t index = 0u; index < renderer_region_count_; index++) {
        const RendererRegion &scripted = renderer_regions_[index];
        if (scripted.device != static_cast<std::uint64_t>(file.st_dev) ||
            scripted.inode != static_cast<std::uint64_t>(file.st_ino))
            continue;
        return size <= scripted.size ? scripted.pointer : nullptr;
    }
    return nullptr;
}

void MockHost::fail_event_descriptor_at(std::uint32_t nth) { StateLock lock(state_); fail_event_descriptor_ = nth; }
void MockHost::forward_requests() { StateLock lock(state_); forward_requests_ = true; }
void MockHost::connect_renderer(RendererPeer &peer) { StateLock lock(state_); renderer_peer_ = &peer; }
void MockHost::fail_renderer_start_at(std::uint32_t nth) { StateLock lock(state_); fail_renderer_start_ = nth; }
void MockHost::renderer_gone() { StateLock lock(state_); renderer_running_ = false; }
bool MockHost::renderer_running() { StateLock lock(state_); return renderer_running_; }
std::uint32_t MockHost::renderer_starts() { StateLock lock(state_); return renderer_starts_; }
std::uint32_t MockHost::renderer_kills() { StateLock lock(state_); return renderer_kills_; }
const char *MockHost::started_renderer() { StateLock lock(state_); return started_renderer_; }
std::uint32_t MockHost::renderer_datagrams() { StateLock lock(state_); return renderer_datagrams_; }
std::uint32_t MockHost::last_receive_timeout() { StateLock lock(state_); return receive_timeout_; }

}
