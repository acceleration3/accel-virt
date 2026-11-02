#include "miniport/av_context/ring_submitter.hpp"

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

Status RingSubmitter::judge_head(uint64_t tail)
{
    AV_SCOPE("RingSubmitter::judge_head");
    const uint64_t head = __atomic_load_n(reinterpret_cast<const uint64_t *>(ring_ + head_offset), __ATOMIC_ACQUIRE);
    if (shared_tail_) {
        const uint64_t now = __atomic_load_n(reinterpret_cast<const uint64_t *>(ring_ + tail_offset), __ATOMIC_ACQUIRE);
        if (now > tail) tail = now;
    }
    if (device_.observe(RingHeader(order_, tail, head)) == Status::ok) return Status::ok;
    failed_ = true;
    platform_.log("av: the device's ring head is inconsistent with the tail published");
    return Status::invalid_state;
}

void RingSubmitter::user_mode_publishes()
{
    AV_SCOPE("RingSubmitter::user_mode_publishes");
    platform_.require_irql_at_most(Irql::dispatch, "RingSubmitter::user_mode_publishes");
    shared_tail_ = true;
}

Status RingSubmitter::adopt_published_tail(const SubmissionDescriptor &submission, uint64_t &published)
{
    AV_SCOPE("RingSubmitter::adopt_published_tail");
    published = __atomic_load_n(reinterpret_cast<const uint64_t *>(ring_ + tail_offset), __ATOMIC_ACQUIRE);
    if (submission.begin() < tail_) return Status::invalid_parameter;
    if (published < submission.end() && published != submission.begin()) return Status::invalid_parameter;
    tail_ = submission.begin();
    return Status::ok;
}

Status RingSubmitter::submit(const SubmissionDescriptor &submission)
{
    AV_SCOPE("RingSubmitter::submit");
    platform_.require_irql_at_most(Irql::dispatch, "RingSubmitter::submit");
    if (ring_ == nullptr || failed_) return Status::invalid_state;
    uint64_t published = tail_;
    if (shared_tail_ && adopt_published_tail(submission, published) != Status::ok)
        return Status::invalid_parameter;
    if (submission.begin() != tail_ ||
        submission.validate_against(RingHeader(order_, tail_, tail_)) != Status::ok)
        return Status::invalid_parameter;
    if (judge_head(published) != Status::ok) return Status::invalid_state;
    if (shared_tail_ && submission.end() <= device_.head()) {
        tail_ = submission.end();
        return Status::ok;
    }
    if (submission.end() - device_.head() > RingHeader(order_, 0u, 0u).capacity()) return Status::no_space;
    if (fences_.record_submission(submission.fence_id()) != Status::ok) return Status::invalid_parameter;
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

bool RingSubmitter::invariants_hold() const
{
    AV_SCOPE("RingSubmitter::invariants_hold");
    return (published_ == 0u || tail_ != 0u) && (shared_tail_ || device_.tail() <= tail_);
}

}
