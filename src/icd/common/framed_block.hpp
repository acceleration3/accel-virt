/* Commands framed one after another into a block of the owner's, as the
 * device reads a batch: each is lent room right after a header's worth of
 * the block, and framing writes the header in front of what was written.
 * BatchedTransport, D3dCommandStream and Vulkan's CommandBatch each frame
 * into one; when the block goes to the host and what grows it are theirs. */
#ifndef AV_ICD_FRAMED_BLOCK_HPP
#define AV_ICD_FRAMED_BLOCK_HPP

#include "proto/submission/command_header.hpp"
#include "proto/wire/wire_writer.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av::icd {

struct FramedBlockProbe;

class FramedBlock {
public:
    static constexpr size_t header_bytes = CommandHeader::encoded_size;

    /* `block` holds `capacity` bytes; a null block holds none. */
    FramedBlock(uint8_t *block, size_t capacity);
    FramedBlock(const FramedBlock &) = delete;
    FramedBlock &operator=(const FramedBlock &) = delete;

    bool                 fits(size_t bytes) const;
    /* A writer over at most `room` bytes past a header's worth of what is
     * free; lent nothing when not even a header fits. */
    WireWriter           lend(size_t room);
    /* What the writer lent last wrote, framed as `command_id` with `flags`:
     * false, with nothing framed, when it failed or ran past the block. */
    [[nodiscard]] bool   frame(uint32_t command_id, const WireWriter &writer, uint8_t flags);
    /* Commands framed elsewhere, copied in; false, with nothing copied, when
     * they do not fit. */
    [[nodiscard]] bool   append(const uint8_t *bytes, size_t length);
    /* Everything framed, from the block's start; the block itself to an
     * owner that gives it back. */
    const uint8_t       *bytes() const;
    uint8_t             *bytes();
    size_t               used() const;
    size_t               capacity() const;
    void                 clear();
    /* The block is `block` of `capacity` from now on, already holding what
     * was framed in the one before. */
    void                 adopt(uint8_t *block, size_t capacity);

    bool invariants_hold() const;

private:
    friend struct FramedBlockProbe;

    uint8_t *block_;
    size_t   capacity_;
    size_t   used_;
};

}  // namespace av::icd

#endif  // AV_ICD_FRAMED_BLOCK_HPP
