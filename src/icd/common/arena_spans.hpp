/* Carves a channel's arena into the spans its users own (docs/protocol.md,
 * "Error word and published slots"), once, as the channel opens and before
 * anything is lent from it. At its very end the error word, which the arena
 * never lends (KmtBlobArena); right below it the slots a driver binds for the
 * host to publish into, where PublishedSlotLayout puts them; below those the
 * fixed spans a driver keeps for itself, whole pages, each under the one
 * before; and from the arena's start up to the lowest of them, the leases.
 * Each is set aside off the top of what the transport lends
 * (ClientBlobArena::set_aside), so no lease ever reaches into one, while
 * `holds` still answers their offsets. A span that would leave the leases
 * no page is refused, and so is anything set aside past the tail's order.
 *
 * Direct3D 11's query results are slots; OpenCL's transfer staging is a
 * span, its buffers made over the leases' memory. */
#ifndef AV_ICD_ARENA_SPANS_HPP
#define AV_ICD_ARENA_SPANS_HPP

#include <stdint.h>

namespace av::icd {

class HostChannel;

class ArenaSpans {
public:
    static constexpr uint64_t page_bytes = 4096u;

    /* Over `channel`'s arena, nothing set aside: carves nothing for a channel
     * with no arena. */
    explicit ArenaSpans(HostChannel &channel);
    ArenaSpans(const ArenaSpans &) = delete;
    ArenaSpans &operator=(const ArenaSpans &) = delete;

    /* `count` published slots right below the error word, before any span:
     * the process's view of them and their offset. False with nothing set
     * aside otherwise, or when they do not fit. */
    [[nodiscard]] bool slots(uint32_t count, uint8_t *&view, uint64_t &offset);
    /* At least `bytes`, whole pages, below whatever is set aside: the
     * process's view and the bytes it holds. False with nothing set aside
     * when no page would be left to lend. */
    [[nodiscard]] bool span(uint64_t bytes, uint8_t *&view, uint64_t &length);
    /* The bytes from the arena's start the leases are lent from; 0 with no
     * arena. */
    uint64_t           lent() const;

    /* Nothing is set aside above the error word. */
    bool invariants_hold() const;

private:
    /* What lies from `start` up to what is set aside, set aside too. */
    [[nodiscard]] bool set_aside(uint64_t start, uint8_t *&view);

    HostChannel &channel_;
    /* The lowest offset set aside; the error word's while nothing is. */
    uint64_t     low_;
};

}  // namespace av::icd

#endif  // AV_ICD_ARENA_SPANS_HPP
