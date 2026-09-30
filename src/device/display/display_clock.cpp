#include "device/display/display_clock.hpp"

#include "core/trace/function_scope.hpp"
#include "device/abi/entry_points.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "proto/display/vblank_record.hpp"

namespace av {

DisplayClock::DisplayClock(Host &host, FenceInterrupts &interrupts, HostRoutine show, void *show_context)
    : host_(host), interrupts_(interrupts), show_(show), show_context_(show_context), lock_(nullptr),
      wake_(nullptr), thread_(nullptr), stopping_(false), show_queued_(false), page_(host), schedules_(),
      latches_()
      {
          AV_SCOPE("DisplayClock::DisplayClock");
      }

DisplayClock::~DisplayClock()
{
    AV_SCOPE("DisplayClock::~DisplayClock");
    stop_thread();
    if (wake_ != nullptr) host_.destroy_event(wake_);
    if (lock_ != nullptr) host_.destroy_mutex(lock_);
}

Status DisplayClock::initialize()
{
    AV_SCOPE("DisplayClock::initialize");
    host_.require_main_loop("DisplayClock::initialize");
    if (lock_ != nullptr) return Status::invalid_state;
    Status status = host_.create_mutex(lock_);
    if (status == Status::ok) status = host_.create_event(wake_);
    if (status == Status::ok) return Status::ok;
    if (lock_ != nullptr) host_.destroy_mutex(lock_);
    lock_ = nullptr;
    return status;
}

Status DisplayClock::attach(uint64_t address)
{
    AV_SCOPE("DisplayClock::attach");
    host_.require_main_loop("DisplayClock::attach");
    if (lock_ == nullptr || wake_ == nullptr) return Status::invalid_state;
    if (address == 0u) {
        detach();
        return Status::ok;
    }
    host_.lock_mutex(lock_);
    const Status mapped = page_.map(address, heads);
    const uint64_t now = host_.monotonic_nanoseconds();
    for (uint32_t head = 0u; mapped == Status::ok && head < heads; head++) {
        if (!schedules_[head].running()) schedules_[head].start(now);
        page_.publish(head, VblankRecord(latches_[head].latched(), schedules_[head].sequence(),
                                         schedules_[head].last(), schedules_[head].refresh_millihz()));
    }
    host_.unlock_mutex(lock_);
    if (mapped != Status::ok || thread_ != nullptr) return mapped;
    stopping_ = false;
    const Status started = host_.start_thread(&RvkDisplayClockRun, this, thread_);
    if (started == Status::ok) return Status::ok;
    host_.lock_mutex(lock_);
    page_.release();
    host_.unlock_mutex(lock_);
    return Status::no_memory;
}

void DisplayClock::stop_thread()
{
    AV_SCOPE("DisplayClock::stop_thread");
    if (thread_ == nullptr) return;
    host_.lock_mutex(lock_);
    stopping_ = true;
    host_.unlock_mutex(lock_);
    host_.signal_event(wake_);
    host_.join_thread(thread_);
    thread_ = nullptr;
}

void DisplayClock::detach()
{
    AV_SCOPE("DisplayClock::detach");
    host_.require_main_loop("DisplayClock::detach");
    if (lock_ == nullptr) return;
    stop_thread();
    host_.lock_mutex(lock_);
    page_.release();
    for (FrameLatch &latch : latches_) latch.clear();
    show_queued_ = false;
    host_.unlock_mutex(lock_);
}

Status DisplayClock::retime(uint32_t head, uint32_t millihz)
{
    AV_SCOPE("DisplayClock::retime");
    host_.require_main_loop("DisplayClock::retime");
    if (lock_ == nullptr) return Status::invalid_state;
    if (head >= heads || millihz < VblankRecord::least_millihz || millihz > VblankRecord::most_millihz)
        return Status::invalid_parameter;
    host_.lock_mutex(lock_);
    schedules_[head].retime(millihz, host_.monotonic_nanoseconds());
    host_.unlock_mutex(lock_);
    /* The thread sleeps toward the old rate's deadline. */
    host_.signal_event(wake_);
    return Status::ok;
}

bool DisplayClock::latches(uint32_t head) const
{
    AV_SCOPE("DisplayClock::latches");
    host_.require_main_loop("DisplayClock::latches");
    return head < heads && page_.mapped();
}

void DisplayClock::post(uint32_t head, uint32_t frame, uint64_t key)
{
    AV_SCOPE("DisplayClock::post");
    host_.require_main_loop("DisplayClock::post");
    if (!latches(head)) return;
    host_.lock_mutex(lock_);
    latches_[head].post(frame, key);
    host_.unlock_mutex(lock_);
}

void DisplayClock::showing()
{
    AV_SCOPE("DisplayClock::showing");
    host_.require_main_loop("DisplayClock::showing");
    if (lock_ == nullptr) return;
    host_.lock_mutex(lock_);
    show_queued_ = false;
    host_.unlock_mutex(lock_);
}

bool DisplayClock::take_unshown(uint32_t head, uint64_t &key)
{
    AV_SCOPE("DisplayClock::take_unshown");
    host_.require_main_loop("DisplayClock::take_unshown");
    if (lock_ == nullptr || head >= heads) return false;
    host_.lock_mutex(lock_);
    const bool unshown = latches_[head].take_unshown(key);
    host_.unlock_mutex(lock_);
    return unshown;
}

void DisplayClock::run()
{
    AV_SCOPE("DisplayClock::run");
    uint64_t next = 0u;
    while (tick(host_.monotonic_nanoseconds(), next)) static_cast<void>(host_.wait_event_until(wake_, next));
}

/* A blank that passed while the thread slept is counted and published with the
 * rest; the display line is raised once for all of them. */
bool DisplayClock::pass(uint32_t head, uint64_t now)
{
    AV_SCOPE("DisplayClock::pass");
    VblankSchedule &schedule = schedules_[head];
    const uint64_t  before = schedule.sequence();
    if (schedule.advance(now) == 0u) return false;
    FrameLatch &latch = latches_[head];
    if (latch.latch() && !show_queued_) {
        show_queued_ = true;
        host_.run_on_main_loop(show_, show_context_);
    }
    page_.publish(head, VblankRecord(latch.latched(), schedule.sequence(), schedule.last(), schedule.refresh_millihz()));
    const uint32_t every = page_.interrupt_every(head);
    return every != 0u && schedule.sequence() / every != before / every;
}

bool DisplayClock::tick(uint64_t now, uint64_t &next)
{
    AV_SCOPE("DisplayClock::tick");
    host_.lock_mutex(lock_);
    if (stopping_) {
        host_.unlock_mutex(lock_);
        return false;
    }
    bool raise = false;
    next = schedules_[0].next();
    for (uint32_t head = 0u; head < heads; head++) {
        raise = pass(head, now) || raise;
        const uint64_t due = schedules_[head].next();
        next = due < next ? due : next;
    }
    host_.unlock_mutex(lock_);
    if (raise) interrupts_.raise(FenceInterrupts::display_line);
    return true;
}

bool DisplayClock::invariants_hold() const
{
    AV_SCOPE("DisplayClock::invariants_hold");
    if (!page_.invariants_hold()) return false;
    for (uint32_t head = 0u; head < heads; head++)
        if (!schedules_[head].invariants_hold() || !latches_[head].invariants_hold()) return false;
    return thread_ == nullptr || (lock_ != nullptr && wake_ != nullptr);
}

}  // namespace av
