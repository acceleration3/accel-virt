#include "icd/opengl/swap_interval.hpp"

#include "core/trace/function_scope.hpp"

namespace av::glicd {

SwapInterval::SwapInterval() : blanks_(1u), waited_(0u)
{
    AV_SCOPE("SwapInterval::SwapInterval");
}

bool SwapInterval::ask(int32_t blanks)
{
    AV_SCOPE("SwapInterval::ask");
    if (blanks < 0 || static_cast<uint32_t>(blanks) > max_blanks) return false;
    blanks_ = static_cast<uint32_t>(blanks);
    return true;
}

uint32_t SwapInterval::blanks() const
{
    AV_SCOPE("SwapInterval::blanks");
    return blanks_;
}

void SwapInterval::waited_one()
{
    AV_SCOPE("SwapInterval::waited_one");
    waited_++;
}

uint64_t SwapInterval::waited() const
{
    AV_SCOPE("SwapInterval::waited");
    return waited_;
}

bool SwapInterval::invariants_hold() const
{
    AV_SCOPE("SwapInterval::invariants_hold");
    return blanks_ <= max_blanks;
}

}  // namespace av::glicd
