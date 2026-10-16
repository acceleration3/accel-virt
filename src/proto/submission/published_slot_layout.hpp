/* Where in a context's blob arena the host publishes what the guest reads
 * without asking (docs/protocol.md, "Error word"): the arena's tail, which
 * the guest never lends. The error word is its last eight bytes, aligned;
 * everything the guest lends ends before it. Both sides place the word by
 * this, from the arena's size each holds. */
#ifndef AV_PUBLISHED_SLOT_LAYOUT_HPP
#define AV_PUBLISHED_SLOT_LAYOUT_HPP

#include <stdint.h>

namespace av {

class PublishedSlotLayout {
public:
    static constexpr uint64_t error_word_bytes = sizeof(uint64_t);

    explicit PublishedSlotLayout(uint64_t arena_bytes);

    /* Whether the arena has room for the tail at all. */
    bool     holds_tail() const;
    /* The error word's offset from the arena's start, which is also how much
     * of the arena, from its start, may be lent; 0 when the arena cannot
     * hold the tail. */
    uint64_t error_word() const;

private:
    uint64_t arena_bytes_;
};

}  // namespace av

#endif  // AV_PUBLISHED_SLOT_LAYOUT_HPP
