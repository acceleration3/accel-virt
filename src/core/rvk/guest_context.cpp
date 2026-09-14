#include "core/rvk/guest_context.hpp"

#include "core/platform/spin_lock_guard.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/context_setup.hpp"

namespace av {

GuestContext::GuestContext(Platform &platform, RegisterWindow &doorbells)
    : platform_(platform), lock_(platform, LockRank::rvk_context), pages_(), sequence_(platform), fences_(),
      submitter_(platform, doorbells, fences_), observer_(platform, fences_), order_(0u), doorbell_(0u),
      state_(State::idle) {}

Status GuestContext::set_up(const CapsetInfo &capset, uint32_t context_id, const ContextSetup &setup,
                            const ContextPages &pages)
{
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
    platform_.require_irql_at_most(Irql::dispatch, "GuestContext::next_request");
    SpinLockGuard guard(lock_);
    return sequence_.next_request(bytes, capacity, length);
}

Status GuestContext::accept(const uint8_t *bytes, size_t length)
{
    platform_.require_irql_at_most(Irql::dispatch, "GuestContext::accept");
    SpinLockGuard guard(lock_);
    const Status status = sequence_.accept(bytes, length);
    if (sequence_.finished()) finish_sequence();
    return status;
}

/* A setup attaches the ring and page only when every answer was OK. */
void GuestContext::finish_sequence()
{
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

Status GuestContext::submit(const SubmissionDescriptor &submission)
{
    platform_.require_irql_at_most(Irql::dispatch, "GuestContext::submit");
    SpinLockGuard guard(lock_);
    if (state_ != State::established) return Status::invalid_state;
    const Status status = submitter_.submit(submission);
    if (submitter_.failed()) state_ = State::failed;
    return status;
}

Status GuestContext::serve()
{
    platform_.require_irql_at_most(Irql::dispatch, "GuestContext::serve");
    SpinLockGuard guard(lock_);
    if (state_ != State::established) return Status::invalid_state;
    const Status status = observer_.observe();
    if (observer_.failed()) state_ = State::failed;
    return status;
}

Status GuestContext::tear_down()
{
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

void GuestContext::abandon()
{
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
    platform_.require_irql_at_most(Irql::passive, "GuestContext::release");
    if (state_ == State::idle) return Status::ok;
    if (state_ != State::torn_down) return Status::invalid_state;
    pages_ = ContextPages();
    state_ = State::idle;
    return Status::ok;
}

GuestContext::State GuestContext::state() const { return state_; }
uint8_t            *GuestContext::ring() const { return pages_.ring_view(); }
const uint8_t      *GuestContext::fence_page() const { return pages_.fence_view(); }
uint64_t            GuestContext::tail() const { return submitter_.tail(); }
uint64_t            GuestContext::completed() const { return observer_.completed(); }
bool                GuestContext::faulted() const { return observer_.faulted(); }
uint32_t            GuestContext::fault_code() const { return observer_.fault_code(); }

/* Pages are held from setup until release, and an established context has
 * both its ring and its page to work on. */
bool GuestContext::invariants_hold() const
{
    if (state_ == State::idle) return !pages_.complete();
    return pages_.complete() && (state_ != State::established || (submitter_.attached() && observer_.attached()));
}

}  // namespace av
