#include "core/rvk/context_pages.hpp"

namespace av {

ContextPages::ContextPages() : ring_(), fence_(), reply_(), ring_view_(nullptr), fence_view_(nullptr) {}

ContextPages::ContextPages(const GuestPageList &ring, const GuestPageList &fence, const GuestPageList &reply,
                           uint8_t *ring_view, const uint8_t *fence_view)
    : ring_(ring), fence_(fence), reply_(reply), ring_view_(ring_view), fence_view_(fence_view) {}

GuestPageList  ContextPages::ring() const       { return ring_; }
GuestPageList  ContextPages::fence() const      { return fence_; }
GuestPageList  ContextPages::reply() const      { return reply_; }
uint8_t       *ContextPages::ring_view() const  { return ring_view_; }
const uint8_t *ContextPages::fence_view() const { return fence_view_; }

bool ContextPages::complete() const
{
    return ring_.complete() && fence_.complete() && fence_.count() == 1u && reply_.complete() &&
           ring_view_ != nullptr && fence_view_ != nullptr;
}

}  // namespace av
