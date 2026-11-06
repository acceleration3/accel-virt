#ifndef AV_RECORDING_RELEASE_ROUTE_HPP
#define AV_RECORDING_RELEASE_ROUTE_HPP

#include "core/support/status.hpp"
#include "guest/transport/release_route.hpp"
#include "proto/escape/release_record.hpp"

#include <cstdint>

namespace av {

class RecordingReleaseRoute final : public ReleaseRoute {
public:
    RecordingReleaseRoute();

    Status release(const std::uint8_t *record, std::uint32_t size) override;
    void   waited_long() override;

    void                 answer(Status status);
    std::uint32_t        releases() const;
    std::uint32_t        long_waits() const;
    const ReleaseRecord &last() const;

private:
    Status        answer_;
    std::uint32_t releases_;
    std::uint32_t long_waits_;
    ReleaseRecord last_;
};

}

#endif
