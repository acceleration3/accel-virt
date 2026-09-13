/* One context's reply slot, read from the guest (docs/protocol.md). A
 * synchronous command asks for a reply; the device writes the answer's header
 * and bytes into the slot before it advances head past the batch, so a producer
 * that waited on the fence reads a finished answer here. The reader trusts the
 * slot only when its header names the fence awaited and the answer fits; the
 * bytes it hands back are valid only until the next command overwrites the slot
 * (contract B2). */
#ifndef AV_REPLY_READER_HPP
#define AV_REPLY_READER_HPP

#include "core/support/status.hpp"
#include "proto/submission/reply_header.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class WireReader;
struct ReplyReaderProbe;

class ReplyReader {
public:
    ReplyReader();
    /* The mapped reply slot: its header, then `capacity - 32` answer bytes. */
    ReplyReader(const uint8_t *slot, size_t capacity);

    /* The answer the device wrote for `expected_fence`, as a reader over its
     * bytes. invalid_state when unbound; malformed for a slot too small or a
     * header no device wrote or an answer that overruns the slot; not_found
     * when the slot holds another fence's answer, not yet this one's;
     * too_large when the answer did not fit, with `required` the size needed. */
    [[nodiscard]] Status read(uint64_t expected_fence, WireReader &answer);
    /* After ok, the answer's size; after too_large, the size a slot would need. */
    uint32_t required() const;

    bool invariants_hold() const;

private:
    const uint8_t *slot_;
    size_t         capacity_;
    uint32_t       required_;

    friend struct ReplyReaderProbe;
};

}  // namespace av

#endif  // AV_REPLY_READER_HPP
