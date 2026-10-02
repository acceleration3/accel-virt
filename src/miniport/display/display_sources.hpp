/* The adapter's VidPn sources: the private driver format dxgkrnl last set for
 * each, the mode committed to each, the primary each scans out and whether it
 * is visible, and whether dxgkrnl wants vsync interrupts. dxgkrnl serializes
 * the display DDIs that change them. The scanout address is stored atomically:
 * with FlipOnVSyncMmIo dxgkrnl sets it at device IRQL, and the DPC reads it at
 * DISPATCH. So is how the source was last flipped, which decides what a vsync
 * reports (FlipView).
 *
 * Each flip is numbered as it comes - the frame the device's display clock is
 * told to show it as (docs/protocol.md, "Display clock") - and the last few
 * are remembered, so the vsync for a blank can name the flip the device
 * actually latched rather than whichever came last. */
#ifndef AV_DISPLAY_SOURCES_HPP
#define AV_DISPLAY_SOURCES_HPP

#include "miniport/ddi/dxgk_multiplane_overlay_plane_record.hpp"
#include "miniport/display/flip_view.hpp"
#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class StateWriter;

class DisplayMode;
struct DisplaySourcesProbe;

class DisplaySources {
public:
    /* The most outputs this driver offers, which is what its arrays are sized
     * for; the device says how many it has. */
    static constexpr uint32_t max_sources = 4u;

    /* How a source was last flipped. */
    using Flip = FlipView::Kind;
    /* Flips remembered per source: a blank latches one of the last few. */
    static constexpr uint32_t remembered_flips = 8u;

    explicit DisplaySources(Platform &platform);
    DisplaySources(const DisplaySources &) = delete;
    DisplaySources &operator=(const DisplaySources &) = delete;

    /* The outputs the device reports: what StartDevice answers as its video
     * present sources and children. A device with none, or with more than this
     * driver offers, is clamped and the value logged by the caller. */
    [[nodiscard]] Status set_count(uint32_t outputs);
    /* One until the device has been asked. Any IRQL: every bound is this. */
    uint32_t             count() const;

    [[nodiscard]] Status set_private_format(uint32_t source, uint32_t attribute);
    /* Zero for a source never set, or not the adapter's. */
    uint32_t             private_format(uint32_t source) const;
    /* A null mode leaves the source without one. */
    [[nodiscard]] Status commit(uint32_t source, const DisplayMode *mode);
    /* Null for a source with no mode, or not the adapter's. */
    const DisplayMode   *committed_mode(uint32_t source) const;
    void                 set_vsync(bool enabled);
    bool                 vsync_enabled() const;
    /* The primary's segment address and the allocation it belongs to; a null
     * allocation means the source shows none. */
    [[nodiscard]] Status set_scanout(uint32_t source, int64_t address, void *primary);
    /* Zero for a source with no primary, or not the adapter's. */
    uint64_t             scanout_address(uint32_t source);
    /* Device IRQL at most: a multiplane flip. `base` is its enabled layer-0
     * plane, whose allocation the source then shows as a primary; null when
     * there is none, and the source shows nothing. */
    [[nodiscard]] Status set_multiplane_scanout(uint32_t source, const DxgkMultiplaneOverlayPlaneRecord *base);
    /* Device IRQL at most. */
    Flip                 flip(uint32_t source);
    /* Device IRQL at most: the number of the source's newest flip, 1 onwards
     * and skipping 0 when it wraps; 0 before its first. */
    uint32_t             frame(uint32_t source);
    /* Dispatch at most: flip `frame` of `source` as it was made, or - for 0,
     * a frame no longer remembered, or one being overwritten as it is read -
     * the source's flip as it is now. */
    FlipView             shown(uint32_t source, uint32_t frame);
    /* The allocation handle the driver gave dxgkrnl for the primary, which is
     * what knows where its pages are; null when the source shows none.
     * Dispatch at most: the DPC asks before it carries the frame. */
    void                *primary(uint32_t source);
    /* DestroyAllocation: a source showing that allocation shows nothing, so
     * the DPC never reads a handle that has been freed. */
    void                 forget_primary(void *primary);
    [[nodiscard]] Status set_visible(uint32_t source, bool visible);
    /* Dispatch at most: the DPC asks before carrying a frame. */
    bool                 visible(uint32_t source);

    /* One line per source: its format, committed mode, scanout and primary. */
    void report(StateWriter &writer);

    bool invariants_hold() const;

private:
    Platform          &platform_;
    uint32_t           count_;
    uint32_t           private_formats_[max_sources];
    const DisplayMode *committed_[max_sources];
    bool               vsync_enabled_;
    uint64_t           scanout_addresses_[max_sources];
    /* The primary's allocation handle, stored as an integer because it is
     * written at device IRQL and read from the DPC. */
    uint64_t           primaries_[max_sources];
    /* Written at PASSIVE, read from the DPC that carries the frame, so it is
     * atomic rather than a bool the reader might tear. */
    uint64_t           visible_[max_sources];
    /* A Flip, atomically. */
    uint64_t           flips_[max_sources];
    /* A flip made at device IRQL: numbered, and remembered. dxgkrnl
     * serializes a source's flips, so each source has one writer. */
    void               number(uint32_t source, const FlipView &flip);
    uint64_t           frames_[max_sources];
    /* The flips remembered, each beside its number. A number is 0 while its
     * flip is written, so a reader that finds the same number before and after
     * its copy copied the flip whole. */
    uint64_t           numbered_frames_[max_sources][remembered_flips];
    FlipView           numbered_flips_[max_sources][remembered_flips];
    /* The base plane's DXGK_MULTIPLANE_OVERLAY_ATTRIBUTES, written before the
     * flip kind that makes them read. */
    uint32_t           base_attributes_[max_sources][multiplane_overlay_attribute_words];

    friend struct DisplaySourcesProbe;
};

}  // namespace av

#endif  // AV_DISPLAY_SOURCES_HPP
