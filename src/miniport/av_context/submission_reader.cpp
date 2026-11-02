#include "miniport/av_context/submission_reader.hpp"

#include "core/platform/platform.hpp"
#include "core/trace/function_scope.hpp"
#include "proto/submission/ring_header.hpp"

namespace av {

SubmissionReader::SubmissionReader(Platform &platform) : platform_(platform), staging_{}
{
    AV_SCOPE("SubmissionReader::SubmissionReader");
}

Status SubmissionReader::read(const uint8_t *dma, size_t size, const RingHeader &ring,
                              SubmissionDescriptor &submission)
{
    AV_SCOPE("SubmissionReader::read");
    platform_.require_irql_at_most(Irql::dispatch, "SubmissionReader::read");
    if (dma == nullptr || size < SubmissionDescriptor::encoded_size) return Status::malformed;
    platform_.copy(staging_, dma, SubmissionDescriptor::encoded_size);
    SubmissionDescriptor candidate;
    const Status decoded = candidate.decode(staging_, SubmissionDescriptor::encoded_size);
    if (decoded != Status::ok) return decoded;
    const Status fits = candidate.validate_against(ring);
    if (fits != Status::ok) return fits;
    submission = candidate;
    return Status::ok;
}

}
