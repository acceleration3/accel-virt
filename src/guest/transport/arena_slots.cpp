#include "guest/transport/arena_slots.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/guest_bytes.hpp"

namespace av {

static_assert(GuestBytes::slot_count == 64u, "ArenaSlots keeps one bit per slot in a uint64_t");

ArenaSlots::ArenaSlots() : used_(0u)
{
    AV_SCOPE("ArenaSlots::ArenaSlots");
}

uint64_t ArenaSlots::span_of(uint64_t base, uint64_t bytes) const
{
    AV_SCOPE("ArenaSlots::span_of");
    if (bytes == 0u || base % GuestBytes::slot_bytes != 0u) return 0u;
    const uint64_t first = base >> GuestBytes::slot_shift;
    if (first >= GuestBytes::slot_count) return 0u;
    const uint64_t count = ((bytes - 1u) >> GuestBytes::slot_shift) + 1u;
    if (count > GuestBytes::slot_count - first) return 0u;
    const uint64_t ones = count == 64u ? ~uint64_t{0} : (uint64_t{1} << count) - 1u;
    return ones << first;
}

bool ArenaSlots::take_at(uint64_t base, uint64_t bytes)
{
    AV_SCOPE("ArenaSlots::take_at");
    const uint64_t span = span_of(base, bytes);
    if (span == 0u || (used_ & span) != 0u) return false;
    used_ |= span;
    return true;
}

bool ArenaSlots::take(uint64_t bytes, uint64_t &base)
{
    AV_SCOPE("ArenaSlots::take");
    for (uint64_t first = 1u; first < GuestBytes::slot_count; first++) {
        const uint64_t at = first << GuestBytes::slot_shift;
        if (!take_at(at, bytes)) continue;
        base = at;
        return true;
    }
    return false;
}

void ArenaSlots::give(uint64_t base, uint64_t bytes)
{
    AV_SCOPE("ArenaSlots::give");
    used_ &= ~span_of(base, bytes);
}

uint32_t ArenaSlots::taken() const
{
    AV_SCOPE("ArenaSlots::taken");
    return static_cast<uint32_t>(__builtin_popcountll(used_));
}

}
