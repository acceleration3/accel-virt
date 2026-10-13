/* The scanout resource of every display source: what puts each source's
 * committed frame on the host's screen. One instance per adapter, injected
 * where the VidPn commit and the DPC can reach it. */
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

    /* Which way every source is backed, taken once from the capset the device
     * answered at StartDevice and never per frame: a device whose display can
     * show a dma-buf is scanned out of a blob over the primary's own pages,
     * and one that cannot keeps the 2D resource and its per-frame transfer
     * (feature 058, contracts/guest_scanout_gate.md). */
    void                 offer(const CapsetInfo &capset);
    /* Whether the blob backing was chosen, which is what a report says and a
     * test asks. */
    bool                 scans_out_blobs() const;
    /* Whether the device composes overlay planes, which the capset said: the
     * planes are then carried before each frame (PlaneScanout). */
    bool                 carries_planes() const;

    /* The resource `source` needs for the mode just committed. PASSIVE. */
    [[nodiscard]] Status configure(uint32_t source, const DisplayMode &mode);
    /* The host stops showing `source`. */
    [[nodiscard]] Status disable(uint32_t source);
    /* Every source's primary carried to the host, given where each one is and
     * whether a frame was presented since the last call; the sources say how
     * many the device has. A source whose flips reach the host as numbered
     * frames is carried on every call, since the device's display clock shows
     * a frame only if it has it before the blank; the rest only when `due`,
     * because carrying them has the host read the primary back. */
    [[nodiscard]] Status refresh(DisplaySources &sources, bool presented, bool due);
    /* Whether `source`'s flips reach the host as numbered frames, which its
     * vertical blanks then name (ScanoutResource::posts_frames). Dispatch at
     * most. */
    bool                 posts_frames(uint32_t source);
    /* StopDevice: the host holds nothing of ours afterwards. */
    void                 release();
    void                 forget_device();

    bool invariants_hold() const;

private:
    /* posts_frames() for a caller already holding the lock. */
    bool numbered(uint32_t source) const;

    Platform         &platform_;
    /* Held by the commit's configure and disable and by the DPC's refresh:
     * dxgkrnl goes on with a mode change as soon as the DPC tells it the last
     * flip retired, and the DPC may still be refreshing - and every resource's
     * invariants are read at the end of either. */
    SpinLock          lock_;
    ScanoutResource   resources_[DisplaySources::max_sources];
    /* Made for every source and used only where the capset said so: a source
     * is backed one way or the other, never both. */
    ScanoutBlobSource blobs_[DisplaySources::max_sources];
    bool              blob_scanout_;
    PlaneScanout      planes_;
    bool              planes_offered_;

    friend struct DisplayScanoutProbe;
};

}  // namespace av

#endif  // AV_DISPLAY_SCANOUT_HPP
