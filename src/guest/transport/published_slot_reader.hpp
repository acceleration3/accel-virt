/* Reads what the host publishes into slots of a context's arena tail
 * (docs/protocol.md, "Error word and published slots"; PublishedSlots is the
 * host's half). A slot's payload is taken only when the slot holds the
 * sequence asked for both before and after the copy: the host clears the
 * sequence before it writes a payload, so a copy it overwrote midway - the
 * slot taken for another value, or the same one again - is never answered. */
#ifndef AV_PUBLISHED_SLOT_READER_HPP
#define AV_PUBLISHED_SLOT_READER_HPP

#include <stdint.h>

namespace av {

class PublishedSlotReader {
public:
    PublishedSlotReader();
    PublishedSlotReader(const PublishedSlotReader &) = delete;
    PublishedSlotReader &operator=(const PublishedSlotReader &) = delete;

    /* `count` slots at `slots`, the process's view of them, cleared: nothing
     * is published in them until the host writes one. Null or none reads
     * nothing. */
    void take(uint8_t *slots, uint32_t count);
    bool reading() const;
    /* Slot `index` modulo the count's payload, `size` bytes into `payload`
     * (none for a null one), when it holds `sequence`: false otherwise, for
     * the unwritten sequence, past what a slot carries, and with no slots. */
    bool read(uint32_t index, uint64_t sequence, void *payload, uint32_t size) const;

    /* Slots are read only where there are some. */
    bool invariants_hold() const;

private:
    uint8_t *slots_;
    uint32_t count_;
};

}  // namespace av

#endif  // AV_PUBLISHED_SLOT_READER_HPP
