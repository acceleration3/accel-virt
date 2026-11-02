#include "harness/fullstack/node_fences.hpp"

#include "harness/framework/test_registry.hpp"

namespace av {

NodeFences::NodeFences() : next_(1u), completed_(0u), blanks_(0u), last_{} {}

std::uint32_t NodeFences::next() const { return next_; }

void NodeFences::commit(std::uint32_t context)
{
    if (context >= max_contexts) return;
    last_[context] = next_;
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
    case crtc_vsync: blanks_++; return;
    default: return;
    }
}

Status NodeFences::settled(std::uint32_t context) const
{
    if (context >= max_contexts) return Status::invalid_parameter;
    return last_[context] <= completed_ ? Status::ok : Status::not_found;
}

void NodeFences::forget(std::uint32_t context)
{
    if (context >= max_contexts) return;
    last_[context] = 0u;
}

std::uint32_t NodeFences::completed() const { return completed_; }
std::uint32_t NodeFences::vertical_blanks() const { return blanks_; }

}
