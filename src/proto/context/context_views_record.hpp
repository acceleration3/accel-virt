/* The escape record a process sends, once its rvk context exists, to learn
 * where the driver mapped the context's ring and reply slot into it
 * (docs/protocol.md, "Context memory"). The memory is the driver's: a
 * process's allocation would move whenever VidMm evicted it, under a device
 * that still held its pages. So the process is told the addresses rather than
 * naming the memory itself.
 *
 * The kernel decodes it from memory the process wrote and writes its answer
 * back into the same bytes. An answer is not a request: a process cannot
 * pre-fill the addresses. */
#ifndef AV_CONTEXT_VIEWS_RECORD_HPP
#define AV_CONTEXT_VIEWS_RECORD_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ContextViewsRecord {
public:
    /* dxgkrnl hands a private escape this short to no driver; the size of
     * the other escape records does get through. */
    static constexpr size_t   encoded_size = 32u;
    static constexpr uint32_t magic        = 0x564B5652u;  /* "RVKV" */

    ContextViewsRecord();
    /* The process's address of the ring - its header, then its data - and of
     * the reply slot; 0 in a request. */
    uint64_t ring() const;
    uint64_t reply() const;
    /* This request with the kernel's answer filled in. */
    ContextViewsRecord answered(uint64_t ring, uint64_t reply) const;
    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* A request only; unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);
    /* The kernel's answer to this request, read back from the bytes the
     * process sent it in: two addresses, neither 0 and not the same. malformed
     * for anything else, and unchanged unless ok. */
    [[nodiscard]] Status accept_answer(const uint8_t *bytes, size_t length);

private:
    static constexpr uint16_t version = 1u;

    /* Every field from the bytes, judging only what no record may carry. */
    [[nodiscard]] Status read(const uint8_t *bytes, size_t length);

    uint64_t ring_;
    uint64_t reply_;
};

}  // namespace av

#endif  // AV_CONTEXT_VIEWS_RECORD_HPP
