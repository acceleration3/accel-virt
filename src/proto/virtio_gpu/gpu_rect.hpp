/* A virtio-gpu rectangle as it arrives - four le32 - and the one question
 * asked of it: does it lie inside a framebuffer. A rectangle that did not
 * decode answers zero for every field, so a caller that ignored the status
 * reads nothing plausible. */
#ifndef AV_GPU_RECT_HPP
#define AV_GPU_RECT_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class WireReader;

class GpuRect {
public:
    static constexpr size_t encoded_size = 16u;

    GpuRect();

    /* malformed when fewer than encoded_size bytes remain, holding nothing. */
    [[nodiscard]] Status decode(WireReader &reader);
    /* Whether the rectangle lies inside `width` x `height` pixels, the sums
     * taken in 64 bits. A side of zero is inside nothing: an update of no
     * pixels is a mistake worth naming rather than a no-op. */
    bool                 inside(uint32_t width, uint32_t height) const;

    uint32_t x() const;
    uint32_t y() const;
    uint32_t width() const;
    uint32_t height() const;
    bool     decoded() const;

    bool invariants_hold() const;

private:
    uint32_t x_;
    uint32_t y_;
    uint32_t width_;
    uint32_t height_;
    bool     decoded_;
};

}  // namespace av

#endif  // AV_GPU_RECT_HPP
