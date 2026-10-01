/* Turns a submitted DMA buffer into the batch a context's ring submitter acts
 * on.
 *
 * A guest leaves a `SubmissionDescriptor` (AVS) in the DMA buffer of a
 * `SubmitCommandVirtual`; today the miniport admits the buffer's address and
 * completes the fence from its own DPC without reading it. This reads it - but
 * copies the guest's bytes into private storage first, because the buffer is
 * the guest's and could change under a validation that read it in place
 * (feature 043). What it hands back is a descriptor already checked against the
 * ring it names. */
#ifndef AV_SUBMISSION_READER_HPP
#define AV_SUBMISSION_READER_HPP

#include "core/support/status.hpp"
#include "proto/submission/submission_descriptor.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class Platform;
class RingHeader;

class SubmissionReader {
public:
    explicit SubmissionReader(Platform &platform);
    SubmissionReader(const SubmissionReader &) = delete;
    SubmissionReader &operator=(const SubmissionReader &) = delete;

    /* Copies `size` bytes at `dma` once, decodes the descriptor, and checks it
     * fits `ring`. malformed for bytes no descriptor could be; invalid_state
     * for a batch longer than the ring. The decoded descriptor is `submission`
     * only on ok. */
    [[nodiscard]] Status read(const uint8_t *dma, size_t size, const RingHeader &ring,
                              SubmissionDescriptor &submission);

private:
    Platform &platform_;
    /* A descriptor is 32 bytes; a DMA buffer holding one is small, and this is
     * the private copy the guest cannot change after it is validated. */
    uint8_t   staging_[SubmissionDescriptor::encoded_size];
};

}  // namespace av

#endif  // AV_SUBMISSION_READER_HPP
