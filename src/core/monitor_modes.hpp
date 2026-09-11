/* DxgkDdiRecommendMonitorModes for the synthetic monitor: every mode the
 * adapter scans out, from the driver, the preferred mode preferred. */
#ifndef AV_MONITOR_MODES_HPP
#define AV_MONITOR_MODES_HPP

#include "nt_status.hpp"
#include "platform.hpp"

#include <stdint.h>

namespace av {

class DisplayMode;
class DisplayModeList;
struct DxgkargRecommendMonitorModesRecord;

class MonitorModes {
public:
    MonitorModes(Platform &platform, const DisplayModeList &modes, const DxgkargRecommendMonitorModesRecord &request);
    MonitorModes(const MonitorModes &) = delete;
    MonitorModes &operator=(const MonitorModes &) = delete;

    NtStatus recommend();

private:
    enum : int32_t {
        color_basis_srgb = 2,   /* D3DKMDT_CB_SRGB */
        origin_driver = 5,      /* D3DKMDT_MCO_DRIVER */
        preferred = 1,          /* D3DKMDT_MP_PREFERRED */
        not_preferred = 2,
    };
    static constexpr uint32_t bits_per_colour_channel = 8u;

    /* dxgkrnl releases a mode info it adds; one it refuses is still the driver's. */
    [[nodiscard]] Status add(const DisplayMode &mode);

    Platform                                 &platform_;
    const DisplayModeList                    &modes_;
    const DxgkargRecommendMonitorModesRecord &request_;
};

}  // namespace av

#endif  // AV_MONITOR_MODES_HPP
