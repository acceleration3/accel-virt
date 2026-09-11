/* The adapter's VidPn sources: the private driver format dxgkrnl last set for
 * each, the mode committed to each, the primary each scans out and whether it
 * is visible, and whether dxgkrnl wants vsync interrupts. dxgkrnl serializes
 * the display DDIs that change them. The scanout address is stored atomically:
 * with FlipOnVSyncMmIo dxgkrnl sets it at device IRQL, and the vsync tick reads
 * it at DISPATCH. */
#ifndef AV_DISPLAY_SOURCES_HPP
#define AV_DISPLAY_SOURCES_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class DisplayMode;

class DisplaySources {
public:
    /* StartDevice reports one video present source and one child. */
    static constexpr uint32_t source_count = 1u;
    static constexpr uint32_t target_count = 1u;

    explicit DisplaySources(Platform &platform);
    DisplaySources(const DisplaySources &) = delete;
    DisplaySources &operator=(const DisplaySources &) = delete;

    [[nodiscard]] Status set_private_format(uint32_t source, uint32_t attribute);
    /* Zero for a source never set, or not the adapter's. */
    uint32_t             private_format(uint32_t source) const;
    /* A null mode leaves the source without one. */
    [[nodiscard]] Status commit(uint32_t source, const DisplayMode *mode);
    /* Null for a source with no mode, or not the adapter's. */
    const DisplayMode   *committed_mode(uint32_t source) const;
    void                 set_vsync(bool enabled);
    bool                 vsync_enabled() const;
    /* The primary's segment address; zero when the source shows none. */
    [[nodiscard]] Status set_scanout(uint32_t source, int64_t address);
    /* Zero for a source with no primary, or not the adapter's. */
    uint64_t             scanout_address(uint32_t source);
    [[nodiscard]] Status set_visible(uint32_t source, bool visible);
    bool                 visible(uint32_t source) const;

private:
    Platform          &platform_;
    uint32_t           private_formats_[source_count];
    const DisplayMode *committed_[source_count];
    bool               vsync_enabled_;
    uint64_t           scanout_addresses_[source_count];
    bool               visible_[source_count];
};

}  // namespace av

#endif  // AV_DISPLAY_SOURCES_HPP
