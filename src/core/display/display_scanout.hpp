/* The scanout resource of every display source: what puts each source's
 * committed frame on the host's screen. One instance per adapter, injected
 * where the VidPn commit and the DPC can reach it. */
#ifndef AV_DISPLAY_SCANOUT_HPP
#define AV_DISPLAY_SCANOUT_HPP

#include "core/display/display_sources.hpp"
#include "core/display/scanout_resource.hpp"
#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class DisplayMode;
class LogBudget;
class VirtioGpuDevice;
struct DisplayScanoutProbe;

class DisplayScanout {
public:
    DisplayScanout(Platform &platform, LogBudget &budget, ObjectStorage<VirtioGpuDevice> &device);
    DisplayScanout(const DisplayScanout &) = delete;
    DisplayScanout &operator=(const DisplayScanout &) = delete;

    /* The resource `source` needs for the mode just committed. */
    [[nodiscard]] Status configure(uint32_t source, const DisplayMode &mode);
    /* The host stops showing `source`. */
    [[nodiscard]] Status disable(uint32_t source);
    /* Every source's primary carried to the host, given where each one is and
     * whether a frame was presented since the last call; the sources say how
     * many the device has. */
    [[nodiscard]] Status refresh(DisplaySources &sources, bool presented);
    /* StopDevice: the host holds nothing of ours afterwards. */
    void                 release();
    void                 forget_device();

    bool invariants_hold() const;

private:
    Platform       &platform_;
    ScanoutResource resources_[DisplaySources::max_sources];

    friend struct DisplayScanoutProbe;
};

}  // namespace av

#endif  // AV_DISPLAY_SCANOUT_HPP
