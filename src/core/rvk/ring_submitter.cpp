#include "core/rvk/ring_submitter.hpp"

#include "core/platform/register_window.hpp"
#include "core/trace/function_scope.hpp"
#include "proto/submission/fence_progress.hpp"
#include "proto/submission/ring_header.hpp"
#include "proto/submission/submission_descriptor.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

RingSubmitter::RingSubmitter(Platform &platform, RegisterWindow &doorbells, FenceProgress &fences)
    : platform_(platform), doorbells_(doorbells), fences_(fences), ring_(nullptr), order_(0u), doorbell_(0u),
      tail_(0u), shared_tail_(false), published_(0u), device_(), failed_(false)
      {
          AV_SCOPE("RingSubmitter::RingSubmitter");
      }

Status RingSubmitter::attach(uint8_t *ring, uint8_t order, uint32_t doorbell)
{
    AV_SCOPE("RingSubmitter::attach");
    platform_.require_irql_at_most(Irql::dispatch, "RingSubmitter::attach");
    if (ring_ != nullptr) return Status::invalid_state;
    if (ring == nullptr || order < RingHeader::lowest_order || order > RingHeader::highest_order ||
        doorbell >= doorbell_pages)
        return Status::invalid_parameter;
    ring_ = ring;
    shared_tail_ = false;
    order_ = order;
    doorbell_ = doorbell;
    tail_ = 0u;
    published_ = 0u;
    device_ = RingProgress();
    failed_ = false;
    return Status::ok;
}

void RingSubmitter::detach()
{
    AV_SCOPE("RingSubmitter::detach");
    platform_.require_irql_at_most(Irql::dispatch, "RingSubmitter::detach");
    ring_ = nullptr;
}

/* The head is judged against the tail the ring holds. */
Status RingSubmitter::judge_head(uint64_t tail)
{
    AV_SCOPE("RingSubmitter::judge_head");
    /* One load: the device stores the head whole while this runs, and a copy a
     * byte at a time could see it half moved. */
    const uint64_t head = __atomic_load_n(reinterpret_cast<const uint64_t *>(ring_ + head_offset), __ATOMIC_ACQUIRE);
    /* A process publishing its own tail may have moved it, and the device
     * consumed past the tail read before, since then: read after the head, it
     * is never behind it. */
    if (shared_tail_) {
        const uint64_t now = __atomic_load_n(reinterpret_cast<const uint64_t *>(ring_ + tail_offset), __ATOMIC_ACQUIRE);
        if (now > tail) tail = now;
    }
    if (device_.observe(RingHeader(order_, tail, head)) == Status::ok) return Status::ok;
    failed_ = true;
    platform_.log("rvk: the device's ring head is inconsistent with the tail published");
    return Status::invalid_state;
}

void RingSubmitter::user_mode_publishes()
{
    AV_SCOPE("RingSubmitter::user_mode_publishes");
    platform_.require_irql_at_most(Irql::dispatch, "RingSubmitter::user_mode_publishes");
    shared_tail_ = true;
}

/* The header is written by a process, so it is judged before it is believed.
 * Two headers describe this submission and no others: the one a process
 * publishes before it stores, which already ends where the submission does
 * (SubmissionPath::submit, for a batch that falls back to the driver), and the
 * one left where the submission begins, which is a process that has stopped
 * publishing for itself and a ring the driver is writing again. A submission
 * that begins behind what this ring already holds is no submission of it. */
Status RingSubmitter::adopt_published_tail(const SubmissionDescriptor &submission, uint64_t &published)
{
    AV_SCOPE("RingSubmitter::adopt_published_tail");
    published = __atomic_load_n(reinterpret_cast<const uint64_t *>(ring_ + tail_offset), __ATOMIC_ACQUIRE);
    if (submission.begin() < tail_) return Status::invalid_parameter;
    /* Past the end is a process that published later batches before the
     * scheduler handed this one over. */
    if (published < submission.end() && published != submission.begin()) return Status::invalid_parameter;
    tail_ = submission.begin();
    return Status::ok;
}

Status RingSubmitter::submit(const SubmissionDescriptor &submission)
{
    AV_SCOPE("RingSubmitter::submit");
    platform_.require_irql_at_most(Irql::dispatch, "RingSubmitter::submit");
    if (ring_ == nullptr || failed_) return Status::invalid_state;
    /* A submission that falls back from a doorbell carries the tail the
     * process already published, which is ahead of the last one this object
     * wrote (US2 scenario 2). */
    uint64_t published = tail_;
    if (shared_tail_ && adopt_published_tail(submission, published) != Status::ok)
        return Status::invalid_parameter;
    if (submission.begin() != tail_ ||
        submission.validate_against(RingHeader(order_, tail_, tail_)) != Status::ok)
        return Status::invalid_parameter;
    if (judge_head(published) != Status::ok) return Status::invalid_state;
    /* The process publishes its tail before it stores, so a batch it fell back
     * to the driver with is one the device may already have taken - a worker
     * that looked at the ring for any reason after that tail moved. It runs
     * once either way, and this submission is the driver being told about work
     * already done: nothing is published and nothing is rung. */
    if (shared_tail_ && submission.end() <= device_.head()) {
        tail_ = submission.end();
        return Status::ok;
    }
    if (submission.end() - device_.head() > RingHeader(order_, 0u, 0u).capacity()) return Status::no_space;
    if (fences_.record_submission(submission.fence_id()) != Status::ok) return Status::invalid_parameter;
    /* One 8-byte store after the batch is whole: the device polls the tail,
     * and a byte-wise copy can be read half-written. The process publishes from another processor while this runs: a plain
     * store of what was read could put its newer tail back, hiding batches it
     * already sent without the kernel. Only a tail still short of this
     * submission is moved, and only if the process has not moved it since. */
    uint64_t *const tail = reinterpret_cast<uint64_t *>(ring_ + tail_offset);
    if (!shared_tail_) __atomic_store_n(tail, submission.end(), __ATOMIC_RELEASE);
    else if (published < submission.end())
        static_cast<void>(__atomic_compare_exchange_n(tail, &published, submission.end(), false, __ATOMIC_RELEASE,
                                                      __ATOMIC_RELAXED));
    tail_ = submission.end();
    published_++;
    doorbells_.write_u32(doorbell_ * doorbell_page_bytes, 1u);
    return Status::ok;
}

void RingSubmitter::kick()
{
    AV_SCOPE("RingSubmitter::kick");
    platform_.require_irql_at_most(Irql::dispatch, "RingSubmitter::kick");
    if (ring_ == nullptr || failed_) return;
    doorbells_.write_u32(doorbell_ * doorbell_page_bytes, 1u);
}

bool     RingSubmitter::attached() const
{
    AV_SCOPE("RingSubmitter::attached");
    return ring_ != nullptr;
}
uint64_t RingSubmitter::tail() const
{
    AV_SCOPE("RingSubmitter::tail");
    return tail_;
}
uint64_t RingSubmitter::head() const
{
    AV_SCOPE("RingSubmitter::head");
    return device_.head();
}
bool     RingSubmitter::failed() const
{
    AV_SCOPE("RingSubmitter::failed");
    return failed_;
}

/* Every submission moves the tail, and the device is never ahead of it. */
bool RingSubmitter::invariants_hold() const
{
    AV_SCOPE("RingSubmitter::invariants_hold");
    return (published_ == 0u || tail_ != 0u) && device_.tail() <= tail_;
}

}  // namespace av
