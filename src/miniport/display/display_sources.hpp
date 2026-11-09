#ifndef AV_DISPLAY_SOURCES_HPP
#define AV_DISPLAY_SOURCES_HPP

#include "miniport/ddi/dxgk_multiplane_overlay_plane_record.hpp"
#include "miniport/display/flip_log.hpp"
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
    static constexpr uint32_t max_sources = 4u;

    using Flip = FlipView::Kind;
    static constexpr uint32_t remembered_flips = 8u;

    explicit DisplaySources(Platform &platform);
    DisplaySources(const DisplaySources &) = delete;
    DisplaySources &operator=(const DisplaySources &) = delete;

    [[nodiscard]] Status set_count(uint32_t outputs);
    uint32_t             count() const;

    [[nodiscard]] Status set_private_format(uint32_t source, uint32_t attribute);
    uint32_t             private_format(uint32_t source) const;
    [[nodiscard]] Status commit(uint32_t source, const DisplayMode *mode);
    const DisplayMode   *committed_mode(uint32_t source) const;
    void                 set_vsync(bool enabled);
    bool                 vsync_enabled() const;
    [[nodiscard]] Status set_scanout(uint32_t source, int64_t address, void *primary);
    uint64_t             scanout_address(uint32_t source);
    [[nodiscard]] Status set_multiplane_scanout(uint32_t source, const DxgkMultiplaneOverlayPlaneRecord *base);
    [[nodiscard]] Status present_planes(uint32_t source, const OverlayPlane *planes, uint32_t count);
    void                 planes(uint32_t source, OverlayPlane *planes);
    Flip                 flip(uint32_t source);
    uint32_t             frame(uint32_t source);
    FlipView             shown(uint32_t source, uint32_t frame);
    void                 note_reported(const FlipView &shown, uint64_t sequence, uint64_t blank_ns);
    void                *primary(uint32_t source);
    void                 forget_primary(void *primary);
    [[nodiscard]] Status set_visible(uint32_t source, bool visible);
    bool                 visible(uint32_t source);

    void report(StateWriter &writer);

    bool invariants_hold() const;

private:
    Platform          &platform_;
    uint32_t           count_;
    uint32_t           private_formats_[max_sources];
    const DisplayMode *committed_[max_sources];
    bool               vsync_enabled_;
    uint64_t           scanout_addresses_[max_sources];
    uint64_t           primaries_[max_sources];
    uint64_t           visible_[max_sources];
    uint64_t           flips_[max_sources];
    void               number(uint32_t source, const FlipView &flip);
    uint64_t           frames_[max_sources];
    uint64_t           numbered_frames_[max_sources][remembered_flips];
    FlipView           numbered_flips_[max_sources][remembered_flips];
    uint32_t           base_attributes_[max_sources][multiplane_overlay_attribute_words];
    OverlayPlane       planes_[max_sources][OverlayPlane::max_planes];
    uint64_t           last_reported_;
    FlipLog            programmed_;
    FlipLog            reported_;

    friend struct DisplaySourcesProbe;
};

}

#endif
