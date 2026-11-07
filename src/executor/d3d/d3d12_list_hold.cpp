#include "executor/d3d/d3d12_list_hold.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

D3d12ListHold::D3d12ListHold(BlockSource &blocks) : held_(blocks, Pool::paged), mark_(0u)
{
    AV_SCOPE("D3d12ListHold::D3d12ListHold");
}

Status D3d12ListHold::add(const Held &held, bool &fresh)
{
    AV_SCOPE("D3d12ListHold::add");
    fresh = false;
    if (held.object_ == nullptr) return Status::invalid_parameter;
    if (holds(held.object_)) return Status::ok;
    const Status appended = held_.append(held);
    fresh = appended == Status::ok;
    return appended;
}

void D3d12ListHold::submitted(uint64_t mark)
{
    AV_SCOPE("D3d12ListHold::submitted");
    if (mark > mark_) mark_ = mark;
}

void D3d12ListHold::pop()
{
    AV_SCOPE("D3d12ListHold::pop");
    if (held_.size() != 0u) held_.truncate(held_.size() - 1u);
}

void D3d12ListHold::forget_mark()
{
    AV_SCOPE("D3d12ListHold::forget_mark");
    mark_ = 0u;
}

bool D3d12ListHold::holds(const IUnknown *object) const
{
    AV_SCOPE("D3d12ListHold::holds");
    for (uint32_t index = held_.size(); index != 0u; index--)
        if (held_.at(index - 1u).object_ == object) return true;
    return false;
}

uint64_t D3d12ListHold::mark() const
{
    AV_SCOPE("D3d12ListHold::mark");
    return mark_;
}

uint32_t D3d12ListHold::size() const
{
    AV_SCOPE("D3d12ListHold::size");
    return held_.size();
}

const D3d12ListHold::Held &D3d12ListHold::at(uint32_t index) const
{
    AV_SCOPE("D3d12ListHold::at");
    return held_.at(index);
}

bool D3d12ListHold::invariants_hold() const
{
    AV_SCOPE("D3d12ListHold::invariants_hold");
    if (!held_.invariants_hold()) return false;
    for (uint32_t index = 0u; index < held_.size(); index++) {
        if (held_.at(index).object_ == nullptr) return false;
        for (uint32_t other = index + 1u; other < held_.size(); other++)
            if (held_.at(other).object_ == held_.at(index).object_) return false;
    }
    return true;
}

}
