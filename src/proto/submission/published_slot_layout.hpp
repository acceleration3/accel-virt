/* Where in a context's blob arena the host publishes what the guest reads
 * without asking (docs/protocol.md, "Error word and published slots"): the
 * arena's tail, which the guest never lends. The error word is its last
 * eight bytes, aligned; slots a context binds - Direct3D 11's query results -
 * end before it, whole slots aligned to their size; everything the guest
 * lends ends before both. Both sides place the tail by this, from the
 * arena's size each holds.
 *
 * A slot is a sequence word, the payload's size and the payload. The host
 * writes the sequence 0 first, then the payload, then the sequence, each
 * behind a store fence, so a guest that reads the sequence it expects before
 * and after copying the payload copied it whole (PublishedSlots,
 * PublishedSlotReader). */
#ifndef AV_PUBLISHED_SLOT_LAYOUT_HPP
#define AV_PUBLISHED_SLOT_LAYOUT_HPP

#include <stdint.h>

namespace av {

class PublishedSlotLayout {
public:
    static constexpr uint64_t error_word_bytes = sizeof(uint64_t);
    static constexpr uint32_t slot_bytes = 128u;
    static constexpr uint32_t sequence_at = 0u;
    static constexpr uint32_t size_at = 8u;
    static constexpr uint32_t data_at = 16u;
    static constexpr uint32_t data_bytes = slot_bytes - data_at;
    /* The sequence of a slot being written, or never written. */
    static constexpr uint64_t unwritten = 0u;

    explicit PublishedSlotLayout(uint64_t arena_bytes);

    /* Whether the arena has room for the tail at all. */
    bool     holds_tail() const;
    /* The error word's offset from the arena's start, which is also how much
     * of the arena, from its start, may be lent; 0 when the arena cannot
     * hold the tail. */
    uint64_t error_word() const;
    /* Where `count` slots start when they end as close before the error word
     * as their alignment lets them; 0 when they do not fit after a first
     * slot's worth of the arena. */
    uint64_t slots_at(uint32_t count) const;
    /* Whether `count` slots at `offset` - a guest's bind, hostile - are whole
     * slots, aligned, that end before the error word. */
    bool     keeps_word_clear(uint64_t offset, uint32_t count) const;

private:
    uint64_t arena_bytes_;
};

}  // namespace av

#endif  // AV_PUBLISHED_SLOT_LAYOUT_HPP
