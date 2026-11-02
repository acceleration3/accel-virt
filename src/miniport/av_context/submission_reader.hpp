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

    [[nodiscard]] Status read(const uint8_t *dma, size_t size, const RingHeader &ring,
                              SubmissionDescriptor &submission);

private:
    Platform &platform_;
    uint8_t   staging_[SubmissionDescriptor::encoded_size];
};

}

#endif
