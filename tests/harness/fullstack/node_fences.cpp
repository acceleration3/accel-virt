#include "harness/fullstack/node_fences.hpp"

#include "harness/framework/test_registry.hpp"

namespace av {

NodeFences::NodeFences()
    : next_(1u), completed_(0u), faults_(0u), blanks_(0u), last_{}, lost_{}, submissions_{}, recorded_(0u) {}

std::uint32_t NodeFences::next() const { return next_; }

void NodeFences::commit(std::uint32_t context)
{
    if (context >= max_contexts) return;
    last_[context] = next_;
    submissions_[recorded_ % history] = Submission{next_, context};
    recorded_++;
    next_++;
}

void NodeFences::observe(const DxgkargcbNotifyInterruptDataRecord &data)
{
    switch (data.InterruptType) {
    case dma_completed:
        if (data.FenceId <= completed_ || data.FenceId >= next_)
            TestRegistry::instance().record_failure(__FILE__, __LINE__,
                                                    "fence %u completed after %u with %u the last submitted",
                                                    data.FenceId, completed_, next_ - 1u);
        if (data.FenceId > completed_) completed_ = data.FenceId;
        return;
    case dma_faulted:
        faults_++;
        lose_submitter_of(data.FenceId);
        return;
    case crtc_vsync: blanks_++; return;
    default: return;
    }
}

void NodeFences::lose_submitter_of(std::uint32_t fence)
{
    const std::uint32_t kept = recorded_ < history ? recorded_ : history;
    for (std::uint32_t index = 0u; index < kept; index++) {
        const Submission &submission = submissions_[index];
        if (submission.fence != fence || submission.context == no_context) continue;
        lost_[submission.context] = true;
        return;
    }
    TestRegistry::instance().record_failure(__FILE__, __LINE__, "fence %u faulted, and no context submitted it", fence);
}

Status NodeFences::settled(std::uint32_t context) const
{
    if (context >= max_contexts || lost_[context]) return Status::invalid_state;
    return last_[context] <= completed_ ? Status::ok : Status::not_found;
}

bool NodeFences::lost(std::uint32_t context) const { return context >= max_contexts || lost_[context]; }

void NodeFences::forget(std::uint32_t context)
{
    if (context >= max_contexts) return;
    last_[context] = 0u;
    lost_[context] = false;
    for (Submission &submission : submissions_)
        if (submission.context == context) submission.context = no_context;
}

std::uint32_t NodeFences::completed() const { return completed_; }
std::uint32_t NodeFences::faults() const { return faults_; }
std::uint32_t NodeFences::vertical_blanks() const { return blanks_; }

}  // namespace av
