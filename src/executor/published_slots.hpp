/* What the host publishes into a context's arena tail for the guest to read
 * without asking (docs/protocol.md, "Error word and published slots"): the
 * error word, and the slots the guest bound - Direct3D 11's query results -
 * each written so the guest never takes it half-written. The region is the
 * memory source's mapping, which a driver may have made write-combined
 * (docs/host-memory.md): x86 orders such stores - with each other, and with
 * the batch head the device publishes after them - only at a store fence. */
#ifndef AV_PUBLISHED_SLOTS_HPP
#define AV_PUBLISHED_SLOTS_HPP

#include <stdint.h>

namespace av {

class GuestBytes;

class PublishedSlots {
public:
    /* Over `bytes`, the context's arena, which may be bound and unbound
     * while this lives. */
    explicit PublishedSlots(GuestBytes &bytes);
    PublishedSlots(const PublishedSlots &) = delete;
    PublishedSlots &operator=(const PublishedSlots &) = delete;

    /* `count` slots at `offset` of the region, as the guest bound them: false,
     * with none bound, for slots that are not whole and aligned before the
     * error word (PublishedSlotLayout::keeps_word_clear) or past the region. */
    [[nodiscard]] bool bind(uint64_t offset, uint32_t count);
    void               unbind();
    bool               bound() const;
    /* `size` bytes of `payload` into slot `index` modulo the count, with
     * `sequence` (never PublishedSlotLayout::unwritten) last. False with none
     * bound, a payload larger than a slot carries, or a region gone. */
    [[nodiscard]] bool write(uint32_t index, uint64_t sequence, const uint8_t *payload, uint32_t size);
    /* `value` as the error word, one aligned store the guest reads whole.
     * False for a region with no tail. */
    [[nodiscard]] bool publish_error_word(uint64_t value);

    /* Bound slots are whole slots. */
    bool invariants_hold() const;

private:
    GuestBytes &bytes_;
    uint64_t     offset_;
    uint32_t     count_;
};

}  // namespace av

#endif  // AV_PUBLISHED_SLOTS_HPP
