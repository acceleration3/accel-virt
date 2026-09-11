/* What one present command does to its allocations. */
#ifndef AV_PRESENT_OPERATION_HPP
#define AV_PRESENT_OPERATION_HPP

#include <stdint.h>

namespace av {

enum class PresentOperation : uint32_t {
    blt = 1u,          /* source to destination */
    color_fill = 2u,   /* the colour into the destination */
    flip = 3u,         /* the source becomes the scanout */
};

}  // namespace av

#endif  // AV_PRESENT_OPERATION_HPP
