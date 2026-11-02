#ifndef AV_GL_FRAMES_IN_FLIGHT_HPP
#define AV_GL_FRAMES_IN_FLIGHT_HPP

#include <stdint.h>

namespace av::glicd {

struct FramesInFlightProbe;

class FramesInFlight {
public:
    static constexpr uint32_t most = 64u;
    static constexpr uint64_t latency_ms = 50u;

    FramesInFlight();

    uint64_t end(uint64_t fence, uint64_t now, uint64_t frequency, uint64_t completed);
    void     forget();
    uint32_t unfinished() const;

    bool invariants_hold() const;

private:
    uint64_t fences_[most];
    uint64_t ended_at_[most];
    uint32_t oldest_;
    uint32_t count_;

    friend struct FramesInFlightProbe;
};

}

#endif
