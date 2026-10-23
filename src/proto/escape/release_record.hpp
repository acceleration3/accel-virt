/* The private data of a packet a process submits on one of its ordinary
 * contexts to release a batch it wrote into the ring of one of its av
 * contexts: the batch is not published by the process, and reaches the host
 * only when the scheduler hands the miniport this packet - after every wait
 * dxgkrnl queued on the context before it. The av context is named by its
 * ring's allocation handle, which only resolves in that process; the batch by
 * the submission descriptor the kernel path would have carried
 * (docs/protocol.md, "Release by packet").
 *
 * The packet completes once the host has completed the batch, as a follow
 * packet does (FollowRecord). */
#ifndef AV_RELEASE_RECORD_HPP
#define AV_RELEASE_RECORD_HPP

#include "core/support/status.hpp"
#include "proto/submission/submission_descriptor.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ReleaseRecord {
public:
    static constexpr size_t encoded_size = 8u + SubmissionDescriptor::encoded_size;

    ReleaseRecord();
    ReleaseRecord(uint32_t ring, const SubmissionDescriptor &batch);

    uint32_t                    ring() const;
    const SubmissionDescriptor &batch() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* The record is the prefix of `length` bytes, which may be more (see
     * FollowRecord::decode). malformed for too few bytes, another magic, ring 0, or a descriptor that is
     * none. Unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    static constexpr uint32_t magic = 0x00525641u;  /* "AVR" */

    uint32_t             ring_;
    SubmissionDescriptor batch_;
};

}  // namespace av

#endif  // AV_RELEASE_RECORD_HPP
