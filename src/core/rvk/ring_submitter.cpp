#include "core/rvk/ring_submitter.hpp"

#include "core/platform/register_window.hpp"
#include "core/trace/function_scope.hpp"
#include "proto/submission/fence_progress.hpp"
#include "proto/submission/ring_header.hpp"
#include "proto/submission/submission_descriptor.hpp"
#include "proto/wire/wire_reader.hpp"
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

/* The head is copied once and judged against the tail this driver published. */
Status RingSubmitter::judge_head()
{
    AV_SCOPE("RingSubmitter::judge_head");
    uint8_t bytes[sizeof(uint64_t)] = {};
    platform_.copy(bytes, ring_ + head_offset, sizeof bytes);
    WireReader reader(bytes, sizeof bytes);
    if (device_.observe(RingHeader(order_, tail_, reader.read_u64())) == Status::ok) return Status::ok;
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

/* The header is written by a process, so it is judged before it is believed:
 * a submission that begins behind what this ring already holds, or whose end
 * is not the tail the process published, is no submission of this ring. The
 * process publishes the tail before it stores (SubmissionPath::submit), so a
 * batch that falls back to the driver is one the header already ends at. */
Status RingSubmitter::adopt_published_tail(const SubmissionDescriptor &submission)
{
    AV_SCOPE("RingSubmitter::adopt_published_tail");
    uint8_t bytes[sizeof(uint64_t)] = {};
    platform_.copy(bytes, ring_ + tail_offset, sizeof bytes);
    WireReader     reader(bytes, sizeof bytes);
    const uint64_t published = reader.read_u64();
    if (submission.begin() < tail_ || published != submission.end()) return Status::invalid_parameter;
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
    if (shared_tail_ && adopt_published_tail(submission) != Status::ok) return Status::invalid_parameter;
    if (submission.begin() != tail_ ||
        submission.validate_against(RingHeader(order_, tail_, tail_)) != Status::ok)
        return Status::invalid_parameter;
    if (judge_head() != Status::ok) return Status::invalid_state;
    if (submission.end() - device_.head() > RingHeader(order_, 0u, 0u).capacity()) return Status::no_space;
    if (fences_.record_submission(submission.fence_id()) != Status::ok) return Status::invalid_parameter;
    uint8_t    bytes[sizeof(uint64_t)] = {};
    WireWriter writer(bytes, sizeof bytes);
    writer.write_u64(submission.end());
    platform_.copy(ring_ + tail_offset, bytes, sizeof bytes);
    tail_ = submission.end();
    published_++;
    doorbells_.write_u32(doorbell_ * doorbell_page_bytes, 1u);
    return Status::ok;
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
