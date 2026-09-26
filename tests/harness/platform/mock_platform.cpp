#include "harness/platform/mock_platform.hpp"

#include "harness/platform/log_record.hpp"
#include "harness/platform/service_value.hpp"

#include "harness/framework/test_registry.hpp"

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace av {

namespace {

const char *const violation_names[] = {
    "irql_ceiling", "allocation_at_dispatch", "blocking_at_dispatch", "lock_order",
    "release_order", "lock_stack_overflow", "null_release", "broken_invariant",
    "time_overflow", "unmapped_register", "timer_misuse", "frame_misuse",
};

}  // namespace

MockPlatform::MockPlatform()
    : live_{}, live_count_(0u), allocation_sequence_(0u), contiguous_{}, contiguous_count_(0u),
      next_physical_address_(first_physical_address), fail_at_(0u),
      irql_(Irql::passive), held_{}, held_count_(0u), violations_{}, now_(0u),
      verbose_(std::getenv("AV_TEST_VERBOSE") != nullptr), logged_(0u), registration_status_(0u),
      registered_driver_object_(nullptr), registered_registry_path_(nullptr), registered_table_(nullptr),
      timers_{}, page_frames_{}, page_frame_count_(0u), register_windows_{},
      register_window_count_(0u) {}

MockPlatform::MockPlatform(std::uint32_t registration_status, LogRecord &record)
    : MockPlatform(registration_status)
{
    record_ = &record;
}

MockPlatform::MockPlatform(std::uint32_t registration_status, const ServiceValue &service)
    : MockPlatform(registration_status)
{
    service_ = &service;
}

MockPlatform::MockPlatform(std::uint32_t registration_status) : MockPlatform()
{
    registration_status_ = registration_status;
}

MockPlatform::~MockPlatform()
{
    TestRegistry &registry = TestRegistry::instance();
    for (std::uint32_t kind = 0u; kind < violation_kinds; kind++) {
        if (violations_[kind] != 0u)
            registry.record_failure(__FILE__, __LINE__, "%u unclaimed %s violation(s)",
                                    violations_[kind], violation_names[kind]);
    }
    if (live_count_ + contiguous_count_ != 0u)
        registry.record_failure(__FILE__, __LINE__, "%u allocation(s) leaked",
                                live_count_ + contiguous_count_);
    if (held_count_ != 0u)
        registry.record_failure(__FILE__, __LINE__, "%u lock(s) still held", held_count_);
    if (frame_view_count_ != 0u)
        registry.record_failure(__FILE__, __LINE__, "%u frame view(s) left mapped", frame_view_count_);
    if (process_mappings() != 0u)
        registry.record_failure(__FILE__, __LINE__, "%u process view(s) left mapped", process_mappings());
    if (bugcheck_routine_ != nullptr)
        registry.record_failure(__FILE__, __LINE__, "a bugcheck callback was never deregistered");
    for (const Timer &timer : timers_)
        if (timer.live) registry.record_failure(__FILE__, __LINE__, "a timer was never destroyed");
    for (std::uint32_t i = 0u; i < live_count_; i++) std::free(live_[i]);
    for (std::uint32_t i = 0u; i < contiguous_count_; i++) std::free(contiguous_[i].block);
}

void MockPlatform::record(Violation kind, const char *format, ...)
{
    violations_[static_cast<std::uint32_t>(kind)]++;
    std::va_list args;
    std::fprintf(stderr, "\n    violation %s: ", violation_names[static_cast<std::uint32_t>(kind)]);
    va_start(args, format);
    std::vfprintf(stderr, format, args);
    va_end(args);
    std::fprintf(stderr, "\n");
}

void *MockPlatform::allocate(std::size_t bytes, Pool)
{
    /* Allocating at DISPATCH is legal for non-paged pool on Windows, but this
     * driver's submission path must never do it: make it a failure. */
    if (irql_ >= Irql::dispatch) {
        record(Violation::allocation_at_dispatch, "%zu bytes at IRQL %u", bytes,
               static_cast<unsigned>(irql_));
        return nullptr;
    }
    allocation_sequence_++;
    if (fail_at_ != 0u && allocation_sequence_ == fail_at_) return nullptr;
    if (live_count_ == max_live) return nullptr;
    if (bytes == 0u) return nullptr;

    void *block = std::malloc(bytes);
    if (block == nullptr) return nullptr;
    /* Poison, so a caller that reads uninitialized memory is caught. */
    std::memset(block, 0xA5, bytes);
    live_[live_count_++] = block;
    return block;
}

void MockPlatform::release(void *block)
{
    if (block == nullptr) {
        record(Violation::null_release, "ExFreePoolWithTag bugchecks on NULL");
        return;
    }
    for (std::uint32_t i = 0u; i < live_count_; i++) {
        if (live_[i] == block) {
            live_[i] = live_[--live_count_];
            std::free(block);
            return;
        }
    }
    /* Freeing memory this platform never handed out is corruption; continuing
     * would free a foreign pointer. */
    std::fprintf(stderr, "\n    release of untracked pointer %p\n", block);
    std::abort();
}

void *MockPlatform::allocate_contiguous(std::size_t bytes)
{
    if (irql_ >= Irql::dispatch) {
        record(Violation::allocation_at_dispatch, "%zu contiguous bytes at IRQL %u", bytes,
               static_cast<unsigned>(irql_));
        return nullptr;
    }
    allocation_sequence_++;
    if (fail_at_ != 0u && allocation_sequence_ == fail_at_) return nullptr;
    if (bytes == 0u || contiguous_count_ == max_contiguous) return nullptr;
    void *block = std::malloc(bytes);
    if (block == nullptr) return nullptr;
    std::memset(block, 0xA5, bytes);
    const std::uint64_t address = next_physical_address_;
    /* A guard page between blocks, so an overrun never lands in a neighbour. */
    next_physical_address_ += ((bytes + 0xFFFu) & ~std::uint64_t{0xFFF}) + 0x1000u;
    contiguous_[contiguous_count_++] = ContiguousBlock{block, address, bytes};
    return block;
}

void MockPlatform::release_contiguous(void *block, std::size_t bytes)
{
    for (std::uint32_t i = 0u; i < contiguous_count_; i++) {
        if (contiguous_[i].block == block && contiguous_[i].bytes == bytes) {
            contiguous_[i] = contiguous_[--contiguous_count_];
            std::free(block);
            return;
        }
    }
    std::fprintf(stderr, "\n    release of untracked contiguous block %p (%zu bytes)\n", block, bytes);
    std::abort();
}

std::uint64_t MockPlatform::physical_address(void *block)
{
    for (std::uint32_t i = 0u; i < contiguous_count_; i++)
        if (contiguous_[i].block == block) return contiguous_[i].physical_address;
    std::fprintf(stderr, "\n    physical address of untracked block %p\n", block);
    std::abort();
}

void *MockPlatform::attach_page_frames(const std::uint64_t *frames, std::uint32_t page_count)
{
    if (page_frame_count_ == max_page_frames) {
        std::fprintf(stderr, "\n    more than %u MDLs attached\n", max_page_frames);
        std::abort();
    }
    page_frames_[page_frame_count_] = PageFrames{frames, page_count};
    return &page_frames_[page_frame_count_++];
}

/* A real MDL bounds the read by the span it describes; this one by the page
 * count it was attached with, and an unknown pointer is no MDL at all. */
Status MockPlatform::read_page_frames(void *mdl, std::uint32_t first, std::uint32_t count, std::uint64_t *frames)
{
    require_irql_at_most(Irql::apc, "Platform::read_page_frames");
    if (mdl == nullptr || frames == nullptr || count == 0u) return Status::invalid_parameter;
    for (std::uint32_t i = 0u; i < page_frame_count_; i++) {
        if (mdl != &page_frames_[i]) continue;
        const PageFrames &attached = page_frames_[i];
        if (first > attached.count || count > attached.count - first) return Status::invalid_parameter;
        for (std::uint32_t page = 0u; page < count; page++) frames[page] = attached.frames[first + page];
        return Status::ok;
    }
    return Status::invalid_parameter;
}

void *MockPlatform::translate_physical(std::uint64_t physical_address, std::size_t bytes) const
{
    for (std::uint32_t i = 0u; i < contiguous_count_; i++) {
        const ContiguousBlock &candidate = contiguous_[i];
        if (physical_address < candidate.physical_address) continue;
        const std::uint64_t offset = physical_address - candidate.physical_address;
        if (offset <= candidate.bytes && bytes <= candidate.bytes - offset)
            return static_cast<std::uint8_t *>(candidate.block) + offset;
    }
    return nullptr;
}

std::uint8_t *MockPlatform::map_frame(std::uint64_t frame)
{
    if (irql_ >= Irql::dispatch)
        record(Violation::blocking_at_dispatch, "map_frame at IRQL %u", static_cast<unsigned>(irql_));
    if ((frame & 0xFFFu) != 0u) record(Violation::frame_misuse, "map_frame of 0x%llx, not a page", static_cast<unsigned long long>(frame));
    frame_map_sequence_++;
    if (frame_fail_at_ != 0u && frame_map_sequence_ == frame_fail_at_) return nullptr;
    auto *const view = static_cast<std::uint8_t *>(translate_physical(frame, 0x1000u));
    if (view == nullptr) return nullptr;
    if (frame_view_count_ == max_frame_views) {
        std::fprintf(stderr, "\n    more than %u frame views\n", max_frame_views);
        std::abort();
    }
    frame_views_[frame_view_count_++] = view;
    return view;
}

void MockPlatform::unmap_frame(std::uint8_t *view)
{
    if (irql_ >= Irql::dispatch)
        record(Violation::blocking_at_dispatch, "unmap_frame at IRQL %u", static_cast<unsigned>(irql_));
    for (std::uint32_t i = 0u; i < frame_view_count_; i++) {
        if (frame_views_[i] != view) continue;
        frame_views_[i] = frame_views_[--frame_view_count_];
        return;
    }
    record(Violation::frame_misuse, "unmap_frame of a view never mapped");
}

std::uint32_t MockPlatform::mapped_frames() const { return frame_view_count_; }

void MockPlatform::fail_frame_map_at(std::uint32_t nth)
{
    frame_fail_at_ = nth == 0u ? 0u : frame_map_sequence_ + nth;
}

Status MockPlatform::map_to_process(std::uint64_t physical, std::uint64_t bytes, std::uint64_t &view,
                                    void *&mapping)
{
    view = 0u;
    mapping = nullptr;
    if (irql_ > Irql::passive)
        record(Violation::irql_ceiling, "map_to_process at IRQL %u", static_cast<unsigned>(irql_));
    if (bytes == 0u || ((physical | bytes) & 0xFFFu) != 0u)
        record(Violation::frame_misuse, "map_to_process of 0x%llx bytes at 0x%llx, not whole pages",
               static_cast<unsigned long long>(bytes), static_cast<unsigned long long>(physical));
    process_map_sequence_++;
    if (process_fail_at_ != 0u && process_map_sequence_ == process_fail_at_) return Status::no_memory;
    void *const backed =
        process_resolver_ != nullptr ? process_resolver_(process_resolver_context_, physical, bytes) : nullptr;
    for (ProcessView &slot : process_views_) {
        if (slot.live) continue;
        const std::uint64_t address = backed != nullptr ? reinterpret_cast<std::uintptr_t>(backed) : next_process_view_;
        slot = ProcessView{physical, bytes, address, true};
        if (backed == nullptr) next_process_view_ += (bytes + 0xFFFu) & ~std::uint64_t{0xFFFu};
        view = slot.view;
        mapping = &slot;
        return Status::ok;
    }
    std::fprintf(stderr, "\n    more than %u process views\n", max_process_views);
    std::abort();
}

void MockPlatform::unmap_from_process(void *mapping)
{
    if (irql_ > Irql::passive)
        record(Violation::irql_ceiling, "unmap_from_process at IRQL %u", static_cast<unsigned>(irql_));
    for (ProcessView &slot : process_views_) {
        if (&slot != mapping || !slot.live) continue;
        slot.live = false;
        return;
    }
    record(Violation::frame_misuse, "unmap_from_process of a mapping never made");
}

std::uint32_t MockPlatform::process_mappings() const
{
    std::uint32_t live = 0u;
    for (const ProcessView &slot : process_views_) live += slot.live ? 1u : 0u;
    return live;
}

bool MockPlatform::maps_into_process(std::uint64_t view, std::uint64_t physical, std::uint64_t bytes) const
{
    for (const ProcessView &slot : process_views_)
        if (slot.live && slot.view == view) return slot.physical == physical && slot.bytes == bytes;
    return false;
}

void MockPlatform::resolve_process_views(ProcessViewResolver resolver, void *context)
{
    process_resolver_ = resolver;
    process_resolver_context_ = context;
}

void MockPlatform::fail_process_map_at(std::uint32_t nth)
{
    process_fail_at_ = nth == 0u ? 0u : process_map_sequence_ + nth;
}

void MockPlatform::zero(void *dst, std::size_t bytes) { std::memset(dst, 0, bytes); }

void MockPlatform::copy(void *dst, const void *src, std::size_t bytes)
{
    std::memcpy(dst, src, bytes);
}

Irql MockPlatform::current_irql() const { return irql_; }

void MockPlatform::require_irql_at_most(Irql ceiling, const char *function)
{
    if (irql_ > ceiling)
        record(Violation::irql_ceiling, "%s requires <= %u, called at %u", function,
               static_cast<unsigned>(ceiling), static_cast<unsigned>(irql_));
}

void MockPlatform::push_lock(LockRank rank, Irql previous)
{
    if (held_count_ != 0u && rank <= held_[held_count_ - 1u].rank)
        record(Violation::lock_order, "rank %u acquired while holding rank %u",
               static_cast<unsigned>(rank),
               static_cast<unsigned>(held_[held_count_ - 1u].rank));
    if (held_count_ == lock_stack_depth) {
        record(Violation::lock_stack_overflow, "more than %u locks held",
               lock_stack_depth);
        return;
    }
    held_[held_count_++] = HeldLock{rank, previous};
}

bool MockPlatform::pop_lock(LockRank rank)
{
    if (held_count_ == 0u || held_[held_count_ - 1u].rank != rank) {
        record(Violation::release_order, "rank %u released but the top of the lock stack is %s",
               static_cast<unsigned>(rank), held_count_ == 0u ? "empty" : "another rank");
        return false;
    }
    held_count_--;
    return true;
}

void MockPlatform::on_acquire(StallCallback callback, void *context)
{
    acquire_callback_ = callback;
    acquire_context_ = context;
}

Irql MockPlatform::acquire_spin_lock(std::uintptr_t &, LockRank rank)
{
    if (acquire_callback_ != nullptr) acquire_callback_(acquire_context_);
    const Irql previous = irql_;
    push_lock(rank, previous);
    if (irql_ < Irql::dispatch) irql_ = Irql::dispatch;
    return previous;
}

void MockPlatform::release_spin_lock(std::uintptr_t &, LockRank rank, Irql previous)
{
    if (pop_lock(rank)) irql_ = previous;
}

bool MockPlatform::try_acquire_spin_lock(std::uintptr_t &word, LockRank rank, Irql &previous)
{
    if (holding_elsewhere_ && held_elsewhere_ == rank) return false;
    previous = acquire_spin_lock(word, rank);
    return true;
}

void MockPlatform::initialize_mutex(void *) {}

void MockPlatform::acquire_mutex(void *, LockRank rank)
{
    if (acquire_callback_ != nullptr) acquire_callback_(acquire_context_);
    if (irql_ >= Irql::dispatch)
        record(Violation::blocking_at_dispatch, "mutex rank %u acquired at IRQL %u",
               static_cast<unsigned>(rank), static_cast<unsigned>(irql_));
    push_lock(rank, irql_);
    if (irql_ < Irql::apc) irql_ = Irql::apc;
}

void MockPlatform::release_mutex(void *, LockRank rank)
{
    if (pop_lock(rank)) irql_ = held_[held_count_].previous;
}

bool MockPlatform::try_acquire_mutex(void *storage, LockRank rank)
{
    if (holding_elsewhere_ && held_elsewhere_ == rank) return false;
    acquire_mutex(storage, rank);
    return true;
}

void MockPlatform::initialize_wait_mutex(void *) {}

/* Blocks without raising the IRQL, so a holder may wait at PASSIVE. */
void MockPlatform::acquire_wait_mutex(void *, LockRank rank)
{
    if (acquire_callback_ != nullptr) acquire_callback_(acquire_context_);
    if (irql_ >= Irql::dispatch)
        record(Violation::blocking_at_dispatch, "wait mutex rank %u acquired at IRQL %u",
               static_cast<unsigned>(rank), static_cast<unsigned>(irql_));
    push_lock(rank, irql_);
}

void MockPlatform::release_wait_mutex(void *, LockRank rank) { static_cast<void>(pop_lock(rank)); }

bool MockPlatform::try_acquire_wait_mutex(void *storage, LockRank rank)
{
    if (holding_elsewhere_ && held_elsewhere_ == rank) return false;
    acquire_wait_mutex(storage, rank);
    return true;
}

void MockPlatform::hold_elsewhere(LockRank rank)
{
    holding_elsewhere_ = true;
    held_elsewhere_ = rank;
}

void MockPlatform::released_elsewhere() { holding_elsewhere_ = false; }

std::uint64_t MockPlatform::interrupt_time() const { return now_; }

/* A stall busy-waits in the kernel; here it only moves the clock. */
void MockPlatform::store_atomic(std::uint64_t &target, std::uint64_t value) { target = value; }

std::uint64_t MockPlatform::load_atomic(std::uint64_t &source) { return source; }

std::uint64_t MockPlatform::performance_counter(std::uint64_t &frequency) const
{
    frequency = 10000000u;
    return now_;
}

void MockPlatform::on_stall(StallCallback callback, void *context)
{
    stall_callback_ = callback;
    stall_context_ = context;
}

void MockPlatform::stall(std::uint32_t microseconds)
{
    advance_time(std::uint64_t{microseconds} * 10u);
    if (stall_callback_ != nullptr) stall_callback_(stall_context_);
}

void *MockPlatform::create_timer(TimerCallback callback, void *context)
{
    if (irql_ >= Irql::dispatch) {
        record(Violation::allocation_at_dispatch, "timer created at IRQL %u", static_cast<unsigned>(irql_));
        return nullptr;
    }
    allocation_sequence_++;
    if (fail_at_ != 0u && allocation_sequence_ == fail_at_) return nullptr;
    for (Timer &timer : timers_) {
        if (timer.live) continue;
        timer = Timer{callback, context, 0u, 0u, true, false};
        return &timer;
    }
    return nullptr;
}

MockPlatform::Timer *MockPlatform::find_timer(void *timer)
{
    for (Timer &candidate : timers_)
        if (&candidate == timer && candidate.live) return &candidate;
    record(Violation::timer_misuse, "%p is not a live timer", timer);
    return nullptr;
}

void MockPlatform::start_timer(void *timer, std::uint64_t period)
{
    if (irql_ > Irql::dispatch)
        record(Violation::irql_ceiling, "start_timer at IRQL %u", static_cast<unsigned>(irql_));
    Timer *found = find_timer(timer);
    if (found == nullptr) return;
    if (period == 0u) {
        record(Violation::timer_misuse, "a zero period");
        return;
    }
    found->period = period;
    found->due = now_ + period;
    found->running = true;
}

void MockPlatform::stop_timer(void *timer)
{
    if (irql_ > Irql::dispatch)
        record(Violation::irql_ceiling, "stop_timer at IRQL %u", static_cast<unsigned>(irql_));
    Timer *found = find_timer(timer);
    if (found != nullptr) found->running = false;
}

void MockPlatform::destroy_timer(void *timer)
{
    if (timer == nullptr) {
        record(Violation::null_release, "destroy_timer of null");
        return;
    }
    if (irql_ >= Irql::dispatch)
        record(Violation::blocking_at_dispatch, "destroy_timer waits, at IRQL %u", static_cast<unsigned>(irql_));
    Timer *found = find_timer(timer);
    if (found != nullptr) *found = Timer{};
}

void MockPlatform::run_timers(std::uint64_t ticks)
{
    const std::uint64_t end = now_ + ticks;
    for (;;) {
        Timer *next = nullptr;
        for (Timer &timer : timers_)
            if (timer.live && timer.running && timer.due <= end && (next == nullptr || timer.due < next->due))
                next = &timer;
        if (next == nullptr) break;
        now_ = next->due;
        next->due += next->period;
        const Irql previous = irql_;
        irql_ = Irql::dispatch;
        next->callback(next, next->context);
        irql_ = previous;
    }
    now_ = end;
}

void MockPlatform::advance_time(std::uint64_t ticks)
{
    if (ticks > ~std::uint64_t{0} - now_) {
        record(Violation::time_overflow, "advancing %llu ticks from %llu",
               static_cast<unsigned long long>(ticks), static_cast<unsigned long long>(now_));
        now_ = ~std::uint64_t{0};
        return;
    }
    now_ += ticks;
}

void MockPlatform::log(const char *message)
{
    logged_++;
    std::snprintf(last_log_, sizeof last_log_, "%s", message);
    if (record_ != nullptr) record_->write(message);
    if (verbose_) std::fprintf(stderr, "%s\n", message);
    const std::size_t length = std::strlen(message);
    if (lines_used_ + length + 2u < log_capacity) {
        std::memcpy(lines_ + lines_used_, message, length);
        lines_used_ += length;
        lines_[lines_used_++] = '\n';
        lines_[lines_used_] = '\0';
    }
}

void MockPlatform::log_at_bugcheck(const char *message)
{
    bugcheck_line_count_++;
    if (verbose_) std::fprintf(stderr, "bugcheck: %s\n", message);
    const std::size_t length = std::strlen(message);
    if (bugcheck_used_ + length + 2u < log_capacity) {
        std::memcpy(bugcheck_text_ + bugcheck_used_, message, length);
        bugcheck_used_ += length;
        bugcheck_text_[bugcheck_used_++] = '\n';
        bugcheck_text_[bugcheck_used_] = '\0';
    }
}

/* The kernel keeps one record per registration; a second without a
 * deregistration is a record inserted twice, which it bugchecks on. */
bool MockPlatform::register_bugcheck(void *buffer, std::size_t bytes, BugcheckRoutine routine)
{
    if (irql_ != Irql::passive)
        record(Violation::irql_ceiling, "register_bugcheck at IRQL %u", static_cast<unsigned>(irql_));
    if (refuse_bugcheck_) {
        refuse_bugcheck_ = false;
        return false;
    }
    if (bugcheck_routine_ != nullptr) {
        std::fprintf(stderr, "\n    bugcheck callback registered twice\n");
        std::abort();
    }
    bugcheck_buffer_ = buffer;
    bugcheck_bytes_ = bytes;
    bugcheck_routine_ = routine;
    return true;
}

void MockPlatform::deregister_bugcheck()
{
    bugcheck_buffer_ = nullptr;
    bugcheck_bytes_ = 0u;
    bugcheck_routine_ = nullptr;
}

void MockPlatform::refuse_bugcheck_registration() { refuse_bugcheck_ = true; }

bool MockPlatform::device_write_refused(const char *function)
{
    if (irql_ != Irql::passive)
        record(Violation::irql_ceiling, "%s at IRQL %u", function, static_cast<unsigned>(irql_));
    device_write_sequence_++;
    if (refuse_device_write_ == 0u || device_write_sequence_ != refuse_device_write_) return false;
    refuse_device_write_ = 0u;
    return true;
}

Status MockPlatform::set_device_description(void *physical_device_object, const std::uint16_t *text,
                                            std::uint32_t bytes)
{
    if (device_write_refused("set_device_description")) return Status::not_supported;
    const std::size_t units = bytes / sizeof(std::uint16_t);
    if (physical_device_object == nullptr || units == 0u || units > max_value_units || text[units - 1u] != 0u) {
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "a description PnP would refuse");
        return Status::invalid_parameter;
    }
    described_device_ = physical_device_object;
    std::memset(description_, 0, sizeof description_);
    std::memcpy(description_, text, bytes);
    device_writes_++;
    return Status::ok;
}

Status MockPlatform::write_driver_text(void *physical_device_object, const char *name, const std::uint16_t *text,
                                       std::uint32_t bytes)
{
    if (device_write_refused("write_driver_text")) return Status::not_supported;
    const std::size_t units = bytes / sizeof(std::uint16_t);
    DriverValue *value = physical_device_object != nullptr ? driver_value(name, true) : nullptr;
    if (value == nullptr || units == 0u || units > max_value_units || text[units - 1u] != 0u) {
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "a driver-key string the registry would refuse");
        return Status::invalid_parameter;
    }
    value->type = registry_text;
    std::memset(value->text, 0, sizeof value->text);
    std::memcpy(value->text, text, bytes);
    device_writes_++;
    return Status::ok;
}

std::uint32_t MockPlatform::read_service_u32(void *, const char *name, std::uint32_t fallback)
{
    return service_ != nullptr ? service_->read(name, fallback) : fallback;
}

Status MockPlatform::write_driver_u32(void *physical_device_object, const char *name, std::uint32_t number)
{
    if (device_write_refused("write_driver_u32")) return Status::not_supported;
    DriverValue *value = physical_device_object != nullptr ? driver_value(name, true) : nullptr;
    if (value == nullptr) return Status::invalid_parameter;
    value->type = registry_u32;
    value->number = number;
    device_writes_++;
    return Status::ok;
}

Status MockPlatform::write_driver_u64(void *physical_device_object, const char *name, std::uint64_t number)
{
    if (device_write_refused("write_driver_u64")) return Status::not_supported;
    DriverValue *value = physical_device_object != nullptr ? driver_value(name, true) : nullptr;
    if (value == nullptr) return Status::invalid_parameter;
    value->type = registry_u64;
    value->number = number;
    device_writes_++;
    return Status::ok;
}

MockPlatform::DriverValue *MockPlatform::driver_value(const char *name, bool create)
{
    for (std::uint32_t index = 0u; index < driver_value_count_; index++)
        if (std::strcmp(driver_values_[index].name, name) == 0) return &driver_values_[index];
    if (!create || driver_value_count_ == max_driver_values || std::strlen(name) >= max_value_name) return nullptr;
    DriverValue &value = driver_values_[driver_value_count_++];
    std::memcpy(value.name, name, std::strlen(name) + 1u);
    return &value;
}

const MockPlatform::DriverValue *MockPlatform::find_driver_value(const char *name) const
{
    for (std::uint32_t index = 0u; index < driver_value_count_; index++)
        if (std::strcmp(driver_values_[index].name, name) == 0) return &driver_values_[index];
    return nullptr;
}

bool MockPlatform::units_are(const std::uint16_t *units, const char *ascii) const
{
    std::size_t index = 0u;
    for (; ascii[index] != '\0'; index++)
        if (units[index] != static_cast<unsigned char>(ascii[index])) return false;
    return units[index] == 0u;
}

void MockPlatform::refuse_device_write_at(std::uint32_t nth)
{
    refuse_device_write_ = nth == 0u ? 0u : device_write_sequence_ + nth;
}

void *MockPlatform::described_device() const { return described_device_; }

bool MockPlatform::device_description_is(const char *ascii) const { return units_are(description_, ascii); }

std::uint32_t MockPlatform::device_writes() const { return device_writes_; }

bool MockPlatform::driver_text_is(const char *name, const char *ascii) const
{
    const DriverValue *value = find_driver_value(name);
    return value != nullptr && value->type == registry_text && units_are(value->text, ascii);
}

std::uint64_t MockPlatform::driver_number(const char *name) const
{
    const DriverValue *value = find_driver_value(name);
    return value != nullptr ? value->number : 0u;
}

std::uint32_t MockPlatform::driver_value_type(const char *name) const
{
    const DriverValue *value = find_driver_value(name);
    return value != nullptr ? value->type : 0u;
}

bool MockPlatform::bugcheck()
{
    if (bugcheck_routine_ == nullptr) return false;
    const Irql before = irql_;
    irql_ = Irql::isr;
    bugcheck_routine_(bugcheck_buffer_, static_cast<std::uint32_t>(bugcheck_bytes_));
    irql_ = before;
    return true;
}

bool          MockPlatform::bugcheck_registered() const { return bugcheck_routine_ != nullptr; }
const void   *MockPlatform::bugcheck_buffer() const { return bugcheck_buffer_; }
std::size_t   MockPlatform::bugcheck_bytes() const { return bugcheck_bytes_; }
std::uint32_t MockPlatform::bugcheck_lines() const { return bugcheck_line_count_; }

bool MockPlatform::bugcheck_line(const char *text) const
{
    return std::strstr(bugcheck_text_, text) != nullptr;
}

std::int32_t MockPlatform::register_miniport(void *driver_object, void *registry_path, const DdiTable &table)
{
    registered_driver_object_ = driver_object;
    registered_registry_path_ = registry_path;
    registered_table_ = &table;
    return static_cast<std::int32_t>(registration_status_);
}

void MockPlatform::report_broken_invariant(const char *what, const char *function)
{
    record(Violation::broken_invariant, "%s failed in %s", what, function);
}

void MockPlatform::fail_allocation_at(std::uint32_t nth)
{
    fail_at_ = nth;
    allocation_sequence_ = 0u;
}

void MockPlatform::set_irql(Irql irql) { irql_ = irql; }

bool MockPlatform::claim(Violation kind)
{
    std::uint32_t &count = violations_[static_cast<std::uint32_t>(kind)];
    if (count == 0u) return false;
    count--;
    return true;
}

std::uint32_t MockPlatform::unclaimed_violations() const
{
    std::uint32_t total = 0u;
    for (std::uint32_t kind = 0u; kind < violation_kinds; kind++) total += violations_[kind];
    return total;
}

std::uint32_t MockPlatform::live_allocations() const { return live_count_ + contiguous_count_; }
std::uint32_t MockPlatform::allocations_made() const { return allocation_sequence_; }
std::uint32_t MockPlatform::locks_held() const       { return held_count_; }
std::uint32_t MockPlatform::logged() const           { return logged_; }
const char   *MockPlatform::last_log() const         { return last_log_; }

bool MockPlatform::logged_line(const char *text) const { return std::strstr(lines_, text) != nullptr; }
void         *MockPlatform::registered_driver_object() const { return registered_driver_object_; }
void         *MockPlatform::registered_registry_path() const { return registered_registry_path_; }
const DdiTable *MockPlatform::registered_table() const { return registered_table_; }

}  // namespace av

namespace av {

void MockPlatform::attach_register_window(void *base, RegisterWindow &window)
{
    if (register_window_count_ == max_register_windows) {
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "more than %u register windows",
                                                max_register_windows);
        return;
    }
    register_windows_[register_window_count_++] = AttachedWindow{reinterpret_cast<std::uintptr_t>(base), &window};
}

void MockPlatform::detach_register_windows(void *base, std::uint64_t length)
{
    const std::uintptr_t first = reinterpret_cast<std::uintptr_t>(base);
    std::uint32_t kept = 0u;
    for (std::uint32_t i = 0u; i < register_window_count_; i++) {
        const bool inside = register_windows_[i].base >= first && register_windows_[i].base - first < length;
        if (!inside) register_windows_[kept++] = register_windows_[i];
    }
    register_window_count_ = kept;
}

std::uint32_t MockPlatform::attached_register_windows() const { return register_window_count_; }

RegisterWindow *MockPlatform::route(void *address, std::uint32_t width, std::uint64_t &offset)
{
    const std::uintptr_t at = reinterpret_cast<std::uintptr_t>(address);
    for (std::uint32_t i = 0u; i < register_window_count_; i++) {
        const AttachedWindow &attached = register_windows_[i];
        const std::uint64_t length = attached.window->length();
        if (at < attached.base || at - attached.base > length || length - (at - attached.base) < width)
            continue;
        offset = at - attached.base;
        return attached.window;
    }
    record(Violation::unmapped_register, "%u-byte access at %p", width, address);
    return nullptr;
}

std::uint8_t MockPlatform::read_register_u8(void *address)
{
    std::uint64_t offset = 0u;
    RegisterWindow *window = route(address, 1u, offset);
    return window != nullptr ? window->read_u8(offset) : 0xFFu;
}

std::uint16_t MockPlatform::read_register_u16(void *address)
{
    std::uint64_t offset = 0u;
    RegisterWindow *window = route(address, 2u, offset);
    return window != nullptr ? window->read_u16(offset) : 0xFFFFu;
}

std::uint32_t MockPlatform::read_register_u32(void *address)
{
    std::uint64_t offset = 0u;
    RegisterWindow *window = route(address, 4u, offset);
    return window != nullptr ? window->read_u32(offset) : 0xFFFFFFFFu;
}

void MockPlatform::write_register_u8(void *address, std::uint8_t value)
{
    std::uint64_t offset = 0u;
    if (RegisterWindow *window = route(address, 1u, offset)) window->write_u8(offset, value);
}

void MockPlatform::write_register_u16(void *address, std::uint16_t value)
{
    std::uint64_t offset = 0u;
    if (RegisterWindow *window = route(address, 2u, offset)) window->write_u16(offset, value);
}

void MockPlatform::write_register_u32(void *address, std::uint32_t value)
{
    std::uint64_t offset = 0u;
    if (RegisterWindow *window = route(address, 4u, offset)) window->write_u32(offset, value);
}

void MockPlatform::write_register_u64(void *address, std::uint64_t value)
{
    std::uint64_t offset = 0u;
    if (RegisterWindow *window = route(address, 8u, offset)) window->write_u64(offset, value);
}

}  // namespace av
