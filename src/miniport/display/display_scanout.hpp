#ifndef AV_DISPLAY_SCANOUT_HPP
#define AV_DISPLAY_SCANOUT_HPP

#include "miniport/display/display_sources.hpp"
#include "miniport/display/plane_scanout.hpp"
#include "miniport/display/scanout_blob_source.hpp"
#include "miniport/display/scanout_resource.hpp"
#include "core/platform/platform.hpp"
#include "core/platform/spin_lock.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class CapsetInfo;
class DisplayMode;
class LogBudget;
class VirtioGpuDevice;
struct DisplayScanoutProbe;

class DisplayScanout {
public:
    DisplayScanout(Platform &platform, LogBudget &budget, ObjectStorage<VirtioGpuDevice> &device);
    DisplayScanout(const DisplayScanout &) = delete;
    DisplayScanout &operator=(const DisplayScanout &) = delete;

    void                 offer(const CapsetInfo &capset);
    bool                 scans_out_blobs() const;
    bool                 carries_planes() const;

    [[nodiscard]] Status configure(uint32_t source, const DisplayMode &mode);
    [[nodiscard]] Status disable(uint32_t source);
    [[nodiscard]] Status refresh(DisplaySources &sources, bool presented, bool due);
    bool                 posts_frames(uint32_t source);
    void                 release();
    void                 forget_device();
    void                 wait_for_refresh();

    bool invariants_hold() const;

private:
    bool numbered(uint32_t source) const;

    Platform         &platform_;
    SpinLock          lock_;
    ScanoutResource   resources_[DisplaySources::max_sources];
    ScanoutBlobSource blobs_[DisplaySources::max_sources];
    bool              blob_scanout_;
    PlaneScanout      planes_;
    bool              planes_offered_;

    friend struct DisplayScanoutProbe;
};

}

#endif
