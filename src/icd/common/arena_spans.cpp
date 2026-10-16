#include "icd/common/arena_spans.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "icd/common/host_channel.hpp"
#include "proto/client/client_blob_arena.hpp"
#include "proto/submission/published_slot_layout.hpp"

namespace av::icd {

ArenaSpans::ArenaSpans(HostChannel &channel)
    : channel_(channel),
      low_(channel.arena_view() == nullptr ? 0u : PublishedSlotLayout(channel.arena_bytes()).error_word())
{
    AV_SCOPE("ArenaSpans::ArenaSpans");
}

bool ArenaSpans::slots(uint32_t count, uint8_t *&view, uint64_t &offset)
{
    AV_SCOPE("ArenaSpans::slots");
    const PublishedSlotLayout tail(channel_.arena_bytes());
    const uint64_t            at = tail.slots_at(count);
    if (low_ == 0u || low_ != tail.error_word() || at == 0u || !set_aside(at, view)) return false;
    offset = at;
    return true;
}

/* The part page under what is set aside goes with the span: a span is whole
 * pages, and nothing else could use it. */
bool ArenaSpans::span(uint64_t bytes, uint8_t *&view, uint64_t &length)
{
    AV_SCOPE("ArenaSpans::span");
    const uint64_t top = low_ / page_bytes * page_bytes;
    const uint64_t whole = (bytes + page_bytes - 1u) / page_bytes * page_bytes;
    if (bytes == 0u || whole < bytes || whole + page_bytes > top) return false;
    if (!set_aside(top - whole, view)) return false;
    length = whole;
    return true;
}

uint64_t ArenaSpans::lent() const
{
    AV_SCOPE("ArenaSpans::lent");
    return low_;
}

bool ArenaSpans::set_aside(uint64_t start, uint8_t *&view)
{
    AV_SCOPE("ArenaSpans::set_aside");
    ClientBlobArena      &blobs = channel_.transport().blobs();
    const ClientBlobLease aside = blobs.set_aside(static_cast<size_t>(low_ - start));
    if (!aside.taken()) return false;
    if (aside.offset() != start) {
        blobs.restore(aside);
        return false;
    }
    low_ = start;
    view = static_cast<uint8_t *>(aside.data());
    return true;
}

bool ArenaSpans::invariants_hold() const
{
    AV_SCOPE("ArenaSpans::invariants_hold");
    return channel_.arena_view() == nullptr ? low_ == 0u
                                            : low_ <= PublishedSlotLayout(channel_.arena_bytes()).error_word();
}

}  // namespace av::icd
