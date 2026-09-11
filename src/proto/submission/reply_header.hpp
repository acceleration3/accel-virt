/* The header of a context's reply slot (docs/protocol.md): the answer of the
 * last reply-generating command of a batch follows it, or, when the answer is
 * larger than the slot, nothing does and the header says how much was needed. */
#ifndef AV_REPLY_HEADER_HPP
#define AV_REPLY_HEADER_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ReplyHeader {
public:
    static constexpr size_t   encoded_size     = 32u;
    static constexpr uint32_t status_ok        = 0u;
    static constexpr uint32_t status_too_large = 1u;

    ReplyHeader();
    /* An answer of `size` bytes that follows. */
    ReplyHeader(uint64_t fence_id, uint32_t size);

    /* This answer as a slot holding `capacity` bytes can carry it: unchanged,
     * or too large with nothing following. */
    ReplyHeader within(uint32_t capacity) const;

    bool     too_large() const;
    uint64_t fence_id() const;
    uint32_t size() const;
    uint32_t required() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* Unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    static constexpr uint32_t magic = 0x594B5652u;  /* "RVKY" */

    ReplyHeader(uint32_t status, uint64_t fence_id, uint32_t size, uint32_t required);
    bool fields_valid() const;

    uint32_t status_;
    uint64_t fence_id_;
    uint32_t size_;
    uint32_t required_;
};

}  // namespace av

#endif  // AV_REPLY_HEADER_HPP
