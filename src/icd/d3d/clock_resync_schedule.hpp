#ifndef AV_D3D_CLOCK_RESYNC_SCHEDULE_HPP
#define AV_D3D_CLOCK_RESYNC_SCHEDULE_HPP

#include <stdint.h>

namespace av::d3d {

class ClockResyncSchedule {
public:
    static constexpr uint64_t interval_nanoseconds = uint64_t{5} * 1000000000u;

    ClockResyncSchedule();
    ClockResyncSchedule(const ClockResyncSchedule &) = delete;
    ClockResyncSchedule &operator=(const ClockResyncSchedule &) = delete;

    bool due(uint64_t now_nanoseconds) const;
    void taken(uint64_t now_nanoseconds);

    bool invariants_hold() const;

private:
    uint64_t last_;
    bool     any_;
};

}

#endif
