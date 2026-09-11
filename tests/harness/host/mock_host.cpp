#include "harness/host/mock_host.hpp"

#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_request.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace av {

namespace {

const char *const violation_names[] = {"main_loop_required", "worker_required", "unknown_handle", "held_mutex",
                                       "second_response", "overlapping_placement", "fence_misuse", "doorbell_misuse"};

class StateLock {
public:
    explicit StateLock(pthread_mutex_t &mutex) : mutex_(mutex) { pthread_mutex_lock(&mutex_); }
    ~StateLock() { pthread_mutex_unlock(&mutex_); }
    StateLock(const StateLock &) = delete;
    StateLock &operator=(const StateLock &) = delete;

private:
    pthread_mutex_t &mutex_;
};

}  // namespace

MockHost::MockHost()
    : main_loop_(pthread_self()), violations_{}, logged_(0u), threads_{}, events_{}, mutexes_{}, gpus_{},
      gpu_count_(0u), work_{}, work_count_(0u), fail_thread_(0u), fail_event_(0u), fail_mutex_(0u),
      fail_allocation_(0u), fail_host_memory_(0u), fail_placement_(0u), empty_mapping_(0u), allocations_{}, regions_{}, region_count_(0u),
      splits_{}, split_count_(0u),
      guest_mappings_(static_cast<GuestMapping *>(std::calloc(max_guest_mappings, sizeof(GuestMapping)))),
      host_blocks_{}, hostmem_size_(0u), resources_{}, resource_count_(0u), doorbells_{}, fence_connected_(false), fail_fence_connect_(0u), raised_{}
{
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
    if (live_mutexes() != 0u) registry.record_failure(__FILE__, __LINE__, "%u mutex(es) leaked", live_mutexes());
    if (work_count_ != 0u)
        registry.record_failure(__FILE__, __LINE__, "%u main loop routine(s) never ran", work_count_);
    if (live_allocations() != 0u)
        registry.record_failure(__FILE__, __LINE__, "%u allocation(s) leaked", live_allocations());
    if (live_guest_mappings() != 0u)
        registry.record_failure(__FILE__, __LINE__, "%u guest mapping(s) leaked", live_guest_mappings());
    if (live_host_memory() != 0u)
        registry.record_failure(__FILE__, __LINE__, "%u host memory block(s) leaked", live_host_memory());
    if (live_placements() != 0u)
        registry.record_failure(__FILE__, __LINE__, "%u hostmem placement(s) left", live_placements());
    if (fence_connected_) registry.record_failure(__FILE__, __LINE__, "fence interrupts left connected");
    if (live_doorbells() != 0u)
        registry.record_failure(__FILE__, __LINE__, "%u doorbell(s) left connected", live_doorbells());
    for (void *block : allocations_) std::free(block);
    for (std::uint32_t index = 0u; index < max_guest_mappings; index++)
        if (guest_mappings_[index].bounce) std::free(guest_mappings_[index].pointer);
    std::free(guest_mappings_);
    for (std::uint32_t index = 0u; index < region_count_; index++) std::free(regions_[index].bytes);
    for (HostBlock &block : host_blocks_)
        if (block.live) std::free(block.pointer);
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
    while (!slot->set) pthread_cond_wait(&slot->signalled, &slot->lock);
    slot->set = false;
    pthread_mutex_unlock(&slot->lock);
}

std::uint32_t MockHost::doorbell_of(void *event)
{
    StateLock lock(state_);
    for (std::uint32_t index = 0u; index < max_doorbells; index++)
        if (doorbells_[index] == event) return index;
    return max_doorbells;
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
        raised = fence_connected_ && line < max_doorbells;
        if (raised) raised_[line]++;
    }
    if (!raised) record(HostViolation::fence_misuse, "MockHost::raise_fence_interrupt");
}

void MockHost::fail_fence_connect_at(std::uint32_t nth) { StateLock lock(state_); fail_fence_connect_ = nth; }

bool MockHost::fence_interrupts_connected() { StateLock lock(state_); return fence_connected_; }

std::uint32_t MockHost::raised(std::uint32_t line)
{
    StateLock lock(state_);
    return line < max_doorbells ? raised_[line] : 0u;
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

void *MockHost::allocate(std::size_t bytes)
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
    const GuestRegion *const region = region_of(address);
    if (region == nullptr) {
        length = 0u;
        return nullptr;
    }
    std::uint64_t available = region->size - (address - region->base);
    for (std::uint32_t index = 0u; index < split_count_; index++)
        if (splits_[index] > address && splits_[index] - address < available) available = splits_[index] - address;
    if (length > available) length = available;
    GuestMapping *slot = nullptr;
    for (std::uint32_t index = 0u; index < max_guest_mappings && slot == nullptr; index++)
        if (guest_mappings_[index].pointer == nullptr) slot = &guest_mappings_[index];
    if (slot == nullptr) {
        length = 0u;
        return nullptr;
    }
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

MockHost::HostBlock *MockHost::block_of(void *memory)
{
    for (HostBlock &block : host_blocks_)
        if (&block == memory && block.live) return &block;
    return nullptr;
}

void *MockHost::create_host_memory(std::uint64_t size, void *&pointer)
{
    require_main_loop("MockHost::create_host_memory");
    StateLock lock(state_);
    pointer = nullptr;
    if (fail_now(fail_host_memory_)) return nullptr;
    for (HostBlock &block : host_blocks_) {
        if (block.live) continue;
        block = HostBlock{std::calloc(1u, size), size, true, false, 0u};
        pointer = block.pointer;
        return &block;
    }
    return nullptr;
}

void MockHost::destroy_host_memory(void *memory)
{
    require_main_loop("MockHost::destroy_host_memory");
    HostBlock *const block = block_of(memory);
    if (block == nullptr || block->placed) {
        record(HostViolation::unknown_handle, "MockHost::destroy_host_memory");
        return;
    }
    StateLock lock(state_);
    std::free(block->pointer);
    *block = HostBlock{};
}

std::uint64_t MockHost::hostmem_size()
{
    StateLock lock(state_);
    return hostmem_size_;
}

Status MockHost::place_in_hostmem(void *memory, std::uint64_t offset)
{
    require_main_loop("MockHost::place_in_hostmem");
    HostBlock *const block = block_of(memory);
    if (block == nullptr) {
        record(HostViolation::unknown_handle, "MockHost::place_in_hostmem");
        return Status::invalid_parameter;
    }
    for (const HostBlock &other : host_blocks_)
        if (other.live && other.placed && offset < other.offset + other.size && other.offset < offset + block->size)
            record(HostViolation::overlapping_placement, "MockHost::place_in_hostmem");
    StateLock lock(state_);
    if (fail_now(fail_placement_)) return Status::no_memory;
    block->placed = true;
    block->offset = offset;
    return Status::ok;
}

void MockHost::remove_from_hostmem(std::uint64_t offset, std::uint64_t size)
{
    require_main_loop("MockHost::remove_from_hostmem");
    for (HostBlock &block : host_blocks_) {
        if (!block.live || !block.placed || block.offset != offset || block.size != size) continue;
        block.placed = false;
        return;
    }
    record(HostViolation::unknown_handle, "MockHost::remove_from_hostmem");
}

std::size_t MockHost::read_request(void *request, std::size_t offset, std::uint8_t *buffer, std::size_t size)
{
    require_main_loop("MockHost::read_request");
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
void MockHost::fail_host_memory_at(std::uint32_t nth) { StateLock lock(state_); fail_host_memory_ = nth; }
void MockHost::fail_placement_at(std::uint32_t nth) { StateLock lock(state_); fail_placement_ = nth; }
void MockHost::empty_mapping_at(std::uint32_t nth) { StateLock lock(state_); empty_mapping_ = nth; }

void MockHost::add_guest_ram(std::uint64_t base, std::size_t size)
{
    StateLock lock(state_);
    regions_[region_count_++] = GuestRegion{base, size, static_cast<std::uint8_t *>(std::calloc(1u, size))};
}

void MockHost::split_guest_memory_at(std::uint64_t address)
{
    StateLock lock(state_);
    splits_[split_count_++] = address;
}

void MockHost::add_bounce_region(std::uint64_t base, std::uint64_t size)
{
    StateLock lock(state_);
    regions_[region_count_++] = GuestRegion{base, size, nullptr};
}

std::uint8_t *MockHost::guest_ram(std::uint64_t address)
{
    StateLock lock(state_);
    const GuestRegion *const region = region_of(address);
    return region != nullptr && region->bytes != nullptr ? region->bytes + (address - region->base) : nullptr;
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
    for (const HostBlock &block : host_blocks_)
        if (block.live && block.placed && block.offset == offset) return block.pointer;
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

std::uint32_t MockHost::live_host_memory()
{
    StateLock lock(state_);
    std::uint32_t live = 0u;
    for (const HostBlock &block : host_blocks_) live += block.live ? 1u : 0u;
    return live;
}

std::uint32_t MockHost::live_placements()
{
    StateLock lock(state_);
    std::uint32_t live = 0u;
    for (const HostBlock &block : host_blocks_) live += block.live && block.placed ? 1u : 0u;
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

}  // namespace av
