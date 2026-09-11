#include "harness/host/mock_host.hpp"

#include "harness/framework/test_registry.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace av {

namespace {

const char *const violation_names[] = {"main_loop_required", "worker_required", "unknown_handle", "held_mutex"};

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
      gpu_count_(0u), work_{}, work_count_(0u), fail_thread_(0u), fail_event_(0u), fail_mutex_(0u)
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

void MockHost::gpu_uuid(std::uint32_t index, std::uint8_t *uuid)
{
    if (index >= gpu_count()) {
        record(HostViolation::unknown_handle, "MockHost::gpu_uuid");
        return;
    }
    std::memcpy(uuid, gpus_[index], uuid_bytes);
}

void MockHost::add_gpu(const std::uint8_t *uuid)
{
    StateLock lock(state_);
    if (gpu_count_ == max_gpus) {
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "too many GPUs");
        return;
    }
    std::memcpy(gpus_[gpu_count_++], uuid, uuid_bytes);
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
