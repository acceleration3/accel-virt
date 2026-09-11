/* The adapter's VidPn sources as far as allocations need them: the private
 * driver format attribute dxgkrnl last set for each. */
#ifndef AV_DISPLAY_SOURCES_HPP
#define AV_DISPLAY_SOURCES_HPP

#include "platform.hpp"
#include "status.hpp"

#include <stdint.h>

namespace av {

class DisplaySources {
public:
    /* StartDevice reports one video present source. */
    static constexpr uint32_t source_count = 1u;

    explicit DisplaySources(Platform &platform);
    DisplaySources(const DisplaySources &) = delete;
    DisplaySources &operator=(const DisplaySources &) = delete;

    [[nodiscard]] Status set_private_format(uint32_t source, uint32_t attribute);
    /* Zero for a source never set, or not the adapter's. */
    uint32_t             private_format(uint32_t source) const;

private:
    Platform &platform_;
    uint32_t  private_formats_[source_count];
};

}  // namespace av

#endif  // AV_DISPLAY_SOURCES_HPP
