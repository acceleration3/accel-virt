#ifndef AV_MONITOR_MODES_HPP
#define AV_MONITOR_MODES_HPP

#include "core/platform/platform.hpp"
#include "core/support/nt_status.hpp"

#include <stdint.h>

namespace av {

class DisplayMode;
class DisplayModeList;
class DisplaySources;
struct DxgkargRecommendMonitorModesRecord;

class MonitorModes {
public:
    MonitorModes(Platform &platform, const DisplayModeList &modes, const DisplaySources &sources,
                 const DxgkargRecommendMonitorModesRecord &request);
    MonitorModes(const MonitorModes &) = delete;
    MonitorModes &operator=(const MonitorModes &) = delete;

    NtStatus recommend();

private:
    enum : int32_t {
        color_basis_srgb = 2,
        origin_driver = 5,
        preferred = 1,
        not_preferred = 2,
    };
    static constexpr uint32_t bits_per_colour_channel = 8u;

    [[nodiscard]] Status add(const DisplayMode &mode);

    Platform                                 &platform_;
    const DisplaySources                     &sources_;
    const DisplayModeList                    &modes_;
    const DxgkargRecommendMonitorModesRecord &request_;
};

}

#endif
