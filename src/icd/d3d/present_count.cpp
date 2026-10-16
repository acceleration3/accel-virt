#include "icd/d3d/present_count.hpp"

#include "core/trace/function_scope.hpp"

namespace av::d3d {

PresentCount::PresentCount() : count_(0u)
{
    AV_SCOPE("PresentCount::PresentCount");
}

uint32_t PresentCount::next()
{
    AV_SCOPE("PresentCount::next");
    count_ = count_ + 1u == 0u ? 1u : count_ + 1u;
    return count_;
}

uint32_t PresentCount::last() const
{
    AV_SCOPE("PresentCount::last");
    return count_;
}

}  // namespace av::d3d
