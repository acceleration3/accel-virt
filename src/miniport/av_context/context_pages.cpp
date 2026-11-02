#include "miniport/av_context/context_pages.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ContextPages::ContextPages() : ring_(), fence_(), reply_(), ring_view_(nullptr), fence_view_(nullptr)
{
    AV_SCOPE("ContextPages::ContextPages");
}

ContextPages::ContextPages(const GuestPageList &ring, const GuestPageList &fence, const GuestPageList &reply,
                           uint8_t *ring_view, const uint8_t *fence_view)
    : ring_(ring), fence_(fence), reply_(reply), ring_view_(ring_view), fence_view_(fence_view)
    {
        AV_SCOPE("ContextPages::ContextPages");
    }

GuestPageList  ContextPages::ring() const
{
    AV_SCOPE("ContextPages::ring");
    return ring_;
}
GuestPageList  ContextPages::fence() const
{
    AV_SCOPE("ContextPages::fence");
    return fence_;
}
GuestPageList  ContextPages::reply() const
{
    AV_SCOPE("ContextPages::reply");
    return reply_;
}
uint8_t       *ContextPages::ring_view() const
{
    AV_SCOPE("ContextPages::ring_view");
    return ring_view_;
}
const uint8_t *ContextPages::fence_view() const
{
    AV_SCOPE("ContextPages::fence_view");
    return fence_view_;
}

bool ContextPages::complete() const
{
    AV_SCOPE("ContextPages::complete");
    return ring_.complete() && fence_.complete() && fence_.count() == 1u && reply_.complete() &&
           ring_view_ != nullptr && fence_view_ != nullptr;
}

}
