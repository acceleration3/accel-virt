/* The escape record a process sends, once its av context exists, to learn
 * where the driver mapped the context's ring and reply slot into it - and,
 * from version 2, its fence page, read-only
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
    static constexpr uint32_t magic        = 0x00565641u;  /* "AVV" */

    /* The versions: 1 answers the ring and the reply slot, 2 the fence page
     * as well. A driver that predates 2 refuses it, and is asked again with 1. */
    static constexpr uint16_t without_fence = 1u;
    static constexpr uint16_t with_fence    = 2u;

    /* A request of the newest version. */
    ContextViewsRecord();
    explicit ContextViewsRecord(uint16_t version);
    uint16_t version() const;
    /* The process's address of the ring - its header, then its data - of the
     * reply slot, and of the fence page; 0 in a request, and the fence 0 in
     * any version-1 record. */
    uint64_t ring() const;
    uint64_t reply() const;
    uint64_t fence() const;
    /* This request with the kernel's answer filled in; a version-1 request
     * takes no fence. */
    ContextViewsRecord answered(uint64_t ring, uint64_t reply, uint64_t fence) const;
    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* A request only; unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);
    /* The kernel's answer to this request, read back from the bytes the
     * process sent it in: of this request's version, with its addresses -
     * two, or three from version 2 - none 0 and no two the same. malformed for
     * anything else, and unchanged unless ok. */
    [[nodiscard]] Status accept_answer(const uint8_t *bytes, size_t length);

private:
    /* Every field from the bytes, judging only what no record may carry. */
    [[nodiscard]] Status read(const uint8_t *bytes, size_t length);

    uint16_t version_;
    uint64_t ring_;
    uint64_t reply_;
    uint64_t fence_;
};

}  // namespace av

#endif  // AV_CONTEXT_VIEWS_RECORD_HPP
