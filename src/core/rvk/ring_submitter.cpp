#include "core/rvk/ring_submitter.hpp"

#include "core/platform/register_window.hpp"
#include "proto/submission/fence_progress.hpp"
#include "proto/submission/ring_header.hpp"
#include "proto/submission/submission_descriptor.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

RingSubmitter::RingSubmitter(Platform &platform, RegisterWindow &doorbells, FenceProgress &fences)
    : platform_(platform), doorbells_(doorbells), fences_(fences), ring_(nullptr), order_(0u), doorbell_(0u),
      tail_(0u), published_(0u), device_(), failed_(false) {}

Status RingSubmitter::attach(uint8_t *ring, uint8_t order, uint32_t doorbell)
{
    platform_.require_irql_at_most(Irql::dispatch, "RingSubmitter::attach");
    if (ring_ != nullptr) return Status::invalid_state;
    if (ring == nullptr || order < RingHeader::lowest_order || order > RingHeader::highest_order ||
        doorbell >= doorbell_pages)
        return Status::invalid_parameter;
    ring_ = ring;
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
    platform_.require_irql_at_most(Irql::dispatch, "RingSubmitter::detach");
    ring_ = nullptr;
}

/* The head is copied once and judged against the tail this driver published. */
Status RingSubmitter::judge_head()
{
    uint8_t bytes[sizeof(uint64_t)] = {};
    platform_.copy(bytes, ring_ + head_offset, sizeof bytes);
    WireReader reader(bytes, sizeof bytes);
    if (device_.observe(RingHeader(order_, tail_, reader.read_u64())) == Status::ok) return Status::ok;
    failed_ = true;
    platform_.log("rvk: the device's ring head is inconsistent with the tail published");
    return Status::invalid_state;
}

Status RingSubmitter::submit(const SubmissionDescriptor &submission)
{
    platform_.require_irql_at_most(Irql::dispatch, "RingSubmitter::submit");
    if (ring_ == nullptr || failed_) return Status::invalid_state;
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

bool     RingSubmitter::attached() const { return ring_ != nullptr; }
uint64_t RingSubmitter::tail() const   { return tail_; }
uint64_t RingSubmitter::head() const   { return device_.head(); }
bool     RingSubmitter::failed() const { return failed_; }

/* Every submission moves the tail, and the device is never ahead of it. */
bool RingSubmitter::invariants_hold() const
{
    return (published_ == 0u || tail_ != 0u) && device_.tail() <= tail_;
}

}  // namespace av
