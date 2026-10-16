/* A triangle as a Direct3D rasterizer holds it, and which samples of one of
 * D3D's standard sample patterns it covers in a pixel: the answer target-
 * independent rasterization gives a pixel shader as SV_Coverage (TirCoverage-
 * Work), which Direct2D turns into its anti-aliasing.
 *
 * The rules are the functional spec's: vertices in n.8 fixed point (the
 * caller snaps them), a sample inside when every edge function is positive
 * there, or zero on a top or left edge (y pointing down: a top edge is
 * horizontal with the triangle below it, a left edge has the triangle to its
 * right), and a triangle of no area covering nothing. Exact in 64-bit
 * integers for vertices within 2^21 pixels of the origin. */
#ifndef AV_COVERAGE_TRIANGLE_HPP
#define AV_COVERAGE_TRIANGLE_HPP

#include <stdint.h>

namespace av {

class CoverageTriangle {
public:
    /* Subpixels per pixel: 8 fractional bits. */
    static constexpr int64_t subpixels = 256;

    /* The triangle `x`, `y`, in subpixels. */
    CoverageTriangle(const int32_t (&x)[3], const int32_t (&y)[3]);

    /* The samples of the `samples`-sample standard pattern (2, 4, 8 or 16)
     * covered in pixel `px`, `py`, sample i as bit i; 0 for any other count. */
    uint32_t coverage(uint32_t samples, uint32_t px, uint32_t py) const;

private:
    bool covers(int64_t x, int64_t y) const;

    int64_t x_[3];
    int64_t y_[3];
    bool    area_;
};

}  // namespace av

#endif  // AV_COVERAGE_TRIANGLE_HPP
