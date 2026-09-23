/* The adapter's VidPn sources: the private driver format dxgkrnl last set for
 * each, the mode committed to each, the primary each scans out and whether it
 * is visible, and whether dxgkrnl wants vsync interrupts. dxgkrnl serializes
 * the display DDIs that change them. The scanout address is stored atomically:
 * with FlipOnVSyncMmIo dxgkrnl sets it at device IRQL, and the vsync tick reads
 * it at DISPATCH. So is how the source was last flipped, which decides what
 * that vsync reports: dxgmms2 retires a multiplane flip only on a
 * CRTC_VSYNC_WITH_MULTIPLANE_OVERLAY, and a single-plane one only on a
 * CRTC_VSYNC naming its address (VidSchiIsExpectedVSyncCookie, dxgmms2.sys
 * @14003a0d4). */
#ifndef AV_DISPLAY_SOURCES_HPP
#define AV_DISPLAY_SOURCES_HPP

#include "core/ddi/dxgk_multiplane_overlay_plane_record.hpp"
#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class StateWriter;

class DisplayMode;
struct DxgkMultiplaneOverlayVsyncInfoRecord;
struct DisplaySourcesProbe;

class DisplaySources {
public:
    /* The most outputs this driver offers, which is what its arrays are sized
     * for; the device says how many it has. */
    static constexpr uint32_t max_sources = 4u;

    /* How a source was last flipped. */
    enum class Flip : uint32_t {
        address = 0u,          /* SetVidPnSourceAddress */
        planes_disabled = 1u,  /* a multiplane flip with no enabled base plane: dxgmms2's disable-planes flip */
        base_plane = 2u,       /* a multiplane flip showing its base plane */
    };

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
    /* The layer a vsync reports for a base_plane flip: layer 0, enabled, at
     * the flipped address with the flip's attributes. */
    void                 base_layer(uint32_t source, DxgkMultiplaneOverlayVsyncInfoRecord &layer);
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
    /* The base plane's DXGK_MULTIPLANE_OVERLAY_ATTRIBUTES, written before the
     * flip kind that makes them read. */
    uint32_t           base_attributes_[max_sources][multiplane_overlay_attribute_words];

    friend struct DisplaySourcesProbe;
};

}  // namespace av

#endif  // AV_DISPLAY_SOURCES_HPP
