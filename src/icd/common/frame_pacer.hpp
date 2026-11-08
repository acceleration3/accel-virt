#ifndef AV_FRAME_PACER_HPP
#define AV_FRAME_PACER_HPP

#include "guest/kmt/beam_position.hpp"

#include <stdint.h>

namespace av {
class DriverLog;
}

namespace av {
class Kmt;
}

namespace av::icd {

struct FramePacerProbe;

class FramePacer {
public:
    FramePacer(Kmt &kmt, DriverLog &log, uint32_t adapter, uint32_t device);
    FramePacer(const FramePacer &) = delete;
    FramePacer &operator=(const FramePacer &) = delete;

    void     wait();
    bool     admits();
    uint32_t next_blank_milliseconds() const;
    uint64_t blanks() const;
    uint64_t failures() const;

    bool invariants_hold() const;

private:
    static constexpr uint32_t primary_source = 0u;
    static constexpr uint64_t refresh_slack_parts = 16u;

    uint64_t refresh_period(uint64_t now, uint64_t frequency);
    bool admits_on_schedule(uint64_t now, uint64_t period);

    Kmt       &kmt_;
    DriverLog &log_;
    uint32_t adapter_;
    uint32_t device_;
    uint64_t blanks_;
    uint64_t failures_;
    uint32_t refresh_millihz_;
    uint64_t refresh_read_ticks_;
    uint64_t due_;
    uint64_t     read_at_;
    BeamPosition read_position_;
    bool         blank_since_admitted_;

    friend struct FramePacerProbe;
};

}

#endif
