#include "miniport/av_context/guest_context.hpp"

#include "core/platform/spin_lock_guard.hpp"
#include "core/trace/function_scope.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/context_setup.hpp"

namespace av {

GuestContext::GuestContext(Platform &platform, RegisterWindow &doorbells)
    : platform_(platform), lock_(platform, LockRank::av_context), pages_(), sequence_(platform), fences_(),
      submitter_(platform, doorbells, fences_), observer_(platform, fences_), progress_(nullptr), order_(0u),
      doorbell_(0u),
      state_(State::idle)
      {
          AV_SCOPE("GuestContext::GuestContext");
      }

Status GuestContext::set_up(const CapsetInfo &capset, uint32_t context_id, const ContextSetup &setup,
                            const ContextPages &pages)
{
    AV_SCOPE("GuestContext::set_up");
    platform_.require_irql_at_most(Irql::passive, "GuestContext::set_up");
    if (state_ != State::idle) return Status::invalid_state;
    if (setup.validate_against(capset) != Status::ok || !pages.complete()) return Status::invalid_parameter;
    SpinLockGuard guard(lock_);
    /* Complete pages and a sequence left idle by the last teardown: begin_setup has nothing to refuse. */
    static_cast<void>(sequence_.begin_setup(context_id, setup, pages));
    pages_ = pages;
    fences_ = FenceProgress();
    order_ = setup.ring_order();
    doorbell_ = setup.doorbell();
    state_ = State::setting_up;
    return Status::ok;
}

Status GuestContext::next_request(uint8_t *bytes, size_t capacity, size_t &length)
{
    AV_SCOPE("GuestContext::next_request");
    platform_.require_irql_at_most(Irql::dispatch, "GuestContext::next_request");
    SpinLockGuard guard(lock_);
    return sequence_.next_request(bytes, capacity, length);
}

Status GuestContext::accept(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("GuestContext::accept");
    platform_.require_irql_at_most(Irql::dispatch, "GuestContext::accept");
    SpinLockGuard guard(lock_);
    const Status status = sequence_.accept(bytes, length);
    if (sequence_.finished()) finish_sequence();
    return status;
}

/* A setup attaches the ring and page only when every answer was OK. */
void GuestContext::finish_sequence()
{
    AV_SCOPE("GuestContext::finish_sequence");
    if (state_ == State::tearing_down) {
        state_ = State::torn_down;
        return;
    }
    if (state_ != State::setting_up) return;
    if (sequence_.refused()) {
        state_ = State::failed;
        return;
    }
    /* The pages and order were accepted at set_up, and nothing is attached
     * before a setup finishes: neither can refuse. */
    static_cast<void>(submitter_.attach(pages_.ring_view(), order_, doorbell_));
    static_cast<void>(observer_.attach(pages_.fence_view()));
    state_ = State::established;
}

void GuestContext::kick()
{
    AV_SCOPE("GuestContext::kick");
    platform_.require_irql_at_most(Irql::dispatch, "GuestContext::kick");
    SpinLockGuard guard(lock_);
    if (state_ == State::established) submitter_.kick();
}

Status GuestContext::submit(const SubmissionDescriptor &submission)
{
    AV_SCOPE("GuestContext::submit");
    platform_.require_irql_at_most(Irql::dispatch, "GuestContext::submit");
    SpinLockGuard guard(lock_);
    if (state_ != State::established) return Status::invalid_state;
    const Status status = submitter_.submit(submission);
    if (submitter_.failed()) state_ = State::failed;
    return status;
}

void GuestContext::user_mode_submits(uint64_t *progress)
{
    AV_SCOPE("GuestContext::user_mode_submits");
    platform_.require_irql_at_most(Irql::dispatch, "GuestContext::user_mode_submits");
    SpinLockGuard guard(lock_);
    progress_ = progress;
    submitter_.user_mode_publishes();
    fences_.user_mode_submits();
}

void GuestContext::driver_submits()
{
    AV_SCOPE("GuestContext::driver_submits");
    platform_.require_irql_at_most(Irql::dispatch, "GuestContext::driver_submits");
    SpinLockGuard guard(lock_);
    progress_ = nullptr;
    fences_.driver_submits();
}

bool GuestContext::forget_progress(const uint64_t *progress)
{
    AV_SCOPE("GuestContext::forget_progress");
    platform_.require_irql_at_most(Irql::dispatch, "GuestContext::forget_progress");
    SpinLockGuard guard(lock_);
    if (progress == nullptr || progress_ != progress) return false;
    progress_ = nullptr;
    fences_.driver_submits();
    return true;
}

Status GuestContext::serve()
{
    AV_SCOPE("GuestContext::serve");
    platform_.require_irql_at_most(Irql::dispatch, "GuestContext::serve");
    SpinLockGuard guard(lock_);
    if (state_ != State::established) return Status::invalid_state;
    const Status status = observer_.observe();
    if (observer_.failed()) state_ = State::failed;
    /* What the device has finished, where the process that submits for itself
     * reads it: the hardware queue's progress is the only thing it has to wait
     * on, since none of its submissions passed through the driver. A context
     * the device faulted - its host device lost - or whose page failed will
     * finish nothing more, and its progress is the counter's end, as a removed
     * device's monitored fences are signalled: a wait in the kernel on it
     * would otherwise never end, as the driver's own packets already do not. */
    const bool lost = observer_.faulted() || observer_.failed();
    if (progress_ != nullptr) *progress_ = lost ? UINT64_MAX : fences_.completed_fence();
    return status;
}

Status GuestContext::tear_down()
{
    AV_SCOPE("GuestContext::tear_down");
    platform_.require_irql_at_most(Irql::dispatch, "GuestContext::tear_down");
    SpinLockGuard guard(lock_);
    if (state_ != State::established && state_ != State::failed) return Status::invalid_state;
    submitter_.detach();
    observer_.detach();
    if (sequence_.begin_teardown() == Status::ok) {
        state_ = State::tearing_down;
        return Status::ok;
    }
    /* The device holds nothing of a setup refused at its first step; the
     * finished sequence still has to go idle for the next setup to begin. */
    sequence_.abandon();
    state_ = State::torn_down;
    return Status::ok;
}

void GuestContext::fail()
{
    AV_SCOPE("GuestContext::fail");
    platform_.require_irql_at_most(Irql::dispatch, "GuestContext::fail");
    SpinLockGuard guard(lock_);
    if (state_ == State::established) state_ = State::failed;
}

uint64_t GuestContext::next_begin() const
{
    AV_SCOPE("GuestContext::next_begin");
    uint64_t       tail = submitter_.tail();
    const uint8_t *ring = pages_.ring_view();
    if (ring == nullptr) return tail;
    const uint64_t header = __atomic_load_n(reinterpret_cast<const uint64_t *>(ring + 8u), __ATOMIC_ACQUIRE);
    return header > tail ? header : tail;
}

void GuestContext::abandon()
{
    AV_SCOPE("GuestContext::abandon");
    platform_.require_irql_at_most(Irql::dispatch, "GuestContext::abandon");
    SpinLockGuard guard(lock_);
    if (state_ == State::idle) return;
    submitter_.detach();
    observer_.detach();
    sequence_.abandon();
    state_ = State::torn_down;
}

Status GuestContext::release()
{
    AV_SCOPE("GuestContext::release");
    platform_.require_irql_at_most(Irql::passive, "GuestContext::release");
    if (state_ == State::idle) return Status::ok;
    if (state_ != State::torn_down) return Status::invalid_state;
    pages_ = ContextPages();
    state_ = State::idle;
    return Status::ok;
}

GuestContext::State GuestContext::state() const
{
    AV_SCOPE("GuestContext::state");
    return state_;
}
uint8_t            *GuestContext::ring() const
{
    AV_SCOPE("GuestContext::ring");
    return pages_.ring_view();
}
const uint8_t      *GuestContext::fence_page() const
{
    AV_SCOPE("GuestContext::fence_page");
    return pages_.fence_view();
}
uint64_t            GuestContext::tail() const
{
    AV_SCOPE("GuestContext::tail");
    return submitter_.tail();
}
uint64_t            GuestContext::completed() const
{
    AV_SCOPE("GuestContext::completed");
    return observer_.completed();
}
uint64_t            GuestContext::raised() const
{
    AV_SCOPE("GuestContext::raised");
    return fences_.raised();
}
bool                GuestContext::faulted() const
{
    AV_SCOPE("GuestContext::faulted");
    return observer_.faulted();
}
uint32_t            GuestContext::fault_code() const
{
    AV_SCOPE("GuestContext::fault_code");
    return observer_.fault_code();
}

/* Pages are held from setup until release, and an established context has
 * both its ring and its page to work on. */
bool GuestContext::invariants_hold() const
{
    AV_SCOPE("GuestContext::invariants_hold");
    if (state_ == State::idle) return !pages_.complete();
    return pages_.complete() && (state_ != State::established || (submitter_.attached() && observer_.attached()));
}

}  // namespace av
