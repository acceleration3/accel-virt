#include "device/recording/recording_limit.hpp"

#include "core/trace/function_scope.hpp"
#include "device/recording/recording_header.hpp"

namespace av {

RecordingLimit::RecordingLimit()
    : ceiling_(default_ceiling), written_(0u), at_ceiling_(AtCeiling::stop)
{
    AV_SCOPE("RecordingLimit::RecordingLimit");
}

RecordingLimit::RecordingLimit(uint64_t ceiling, AtCeiling at_ceiling)
    : ceiling_(ceiling), written_(0u), at_ceiling_(at_ceiling)
{
    AV_SCOPE("RecordingLimit::RecordingLimit");
}

/* Subtraction rather than addition throughout: `written_` never passes
 * `ceiling_`, so neither side can wrap. */
LimitVerdict RecordingLimit::verdict(uint64_t bytes) const
{
    AV_SCOPE("RecordingLimit::verdict");
    if (ceiling_ == 0u) return LimitVerdict::fits;
    if (bytes <= ceiling_ - written_) return LimitVerdict::fits;
    if (at_ceiling_ == AtCeiling::stop) return LimitVerdict::stop;
    if (ceiling_ < RecordingHeader::encoded_size) return LimitVerdict::stop;
    if (bytes > ceiling_ - RecordingHeader::encoded_size) return LimitVerdict::stop;
    return LimitVerdict::wrap;
}

void RecordingLimit::took(uint64_t bytes)
{
    AV_SCOPE("RecordingLimit::took");
    written_ += bytes;
}

void RecordingLimit::wrapped()
{
    AV_SCOPE("RecordingLimit::wrapped");
    written_ = RecordingHeader::encoded_size;
}

uint64_t RecordingLimit::written() const
{
    AV_SCOPE("RecordingLimit::written");
    return written_;
}

uint64_t RecordingLimit::ceiling() const
{
    AV_SCOPE("RecordingLimit::ceiling");
    return ceiling_;
}

bool RecordingLimit::invariants_hold() const
{
    AV_SCOPE("RecordingLimit::invariants_hold");
    return ceiling_ == 0u || written_ <= ceiling_;
}

}  // namespace av
