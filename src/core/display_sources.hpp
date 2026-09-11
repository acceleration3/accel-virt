/* The adapter's VidPn sources: the private driver format dxgkrnl last set for
 * each, the mode committed to each, and whether dxgkrnl wants vsync
 * interrupts. dxgkrnl serializes the display DDIs that change them. */
#ifndef AV_DISPLAY_SOURCES_HPP
#define AV_DISPLAY_SOURCES_HPP

#include "platform.hpp"
#include "status.hpp"

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

private:
    Platform          &platform_;
    uint32_t           private_formats_[source_count];
    const DisplayMode *committed_[source_count];
    bool               vsync_enabled_;
};

}  // namespace av

#endif  // AV_DISPLAY_SOURCES_HPP
