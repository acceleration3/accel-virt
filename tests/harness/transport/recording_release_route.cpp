#include "harness/transport/recording_release_route.hpp"

namespace av {

RecordingReleaseRoute::RecordingReleaseRoute() : answer_(Status::ok), releases_(0u), long_waits_(0u), last_() {}

Status RecordingReleaseRoute::release(const std::uint8_t *record, std::uint32_t size)
{
    releases_++;
    if (answer_ != Status::ok) return answer_;
    return last_.decode(record, size);
}

void RecordingReleaseRoute::waited_long() { long_waits_++; }

void RecordingReleaseRoute::answer(Status status) { answer_ = status; }

std::uint32_t RecordingReleaseRoute::releases() const { return releases_; }

std::uint32_t RecordingReleaseRoute::long_waits() const { return long_waits_; }

const ReleaseRecord &RecordingReleaseRoute::last() const { return last_; }

}
