#ifndef AV_D3D12_CLOCK_ORIGIN_HPP
#define AV_D3D12_CLOCK_ORIGIN_HPP

#include "icd/d3d/clock_resync_schedule.hpp"
#include "um/common/performance_clock.hpp"

#include <stdint.h>

namespace av {
class KeyedLog;
}

namespace av::d3d {
class D3dDeviceChannel;
class D3dSession;
}

namespace av::d3d12 {

class D3d12ClockOrigin {
public:
    D3d12ClockOrigin(d3d::D3dSession &session, KeyedLog &lines);
    D3d12ClockOrigin(const D3d12ClockOrigin &) = delete;
    D3d12ClockOrigin &operator=(const D3d12ClockOrigin &) = delete;

    void refresh(d3d::D3dDeviceChannel &wire, uint32_t queue);

private:
    d3d::D3dSession  &session_;
    KeyedLog         &lines_;
    PerformanceClock  clock_;
    d3d::ClockResyncSchedule schedule_;
};

}

#endif
