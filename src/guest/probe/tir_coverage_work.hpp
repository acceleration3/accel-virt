/* Target-independent rasterization the way Direct2D uses it, through the
 * whole stack: hairline strokes - quads a fraction of a pixel thin, at
 * offsets that put their edges on and between the sample rows of the
 * 16-sample pattern - drawn in one call with a rasterizer state forcing 16
 * samples onto a single-sample R32_UINT target, the pixel shader writing
 * SV_Coverage and the output merger OR'ing the masks in. Each texel read
 * back must be the mask a Direct3D rasterizer gives (CoverageTriangle), bit
 * for bit; a stack that rasterizes such a draw at one sample - as the host's
 * DXVK did, which lost rows of Affinity's hairlines - gets 0 or 1 where
 * D3D gets up to 16 bits, and the step fails with the texels that were
 * right. The device must still stand after.
 *
 * Only the system's runtime is judged: through it every draw reaches the
 * host's DXVK, which emulates the forced count. A layer's device - DXVK
 * beside the probe, as released - ignores ForcedSampleCount with a render
 * target bound, so its coverage is noted rather than judged. */
#ifndef AV_TIR_COVERAGE_WORK_HPP
#define AV_TIR_COVERAGE_WORK_HPP

#include <stdint.h>

namespace av {

class D3d11TirEntry;
class ScenarioSteps;
struct TirCoverageWorkProbe;

class TirCoverageWork {
public:
    static constexpr uint32_t extent = 64u;
    static constexpr uint32_t texels = extent * extent;
    static constexpr uint32_t samples = 16u;
    /* Horizontal and vertical strokes of each width, and slanted ones. */
    static constexpr uint32_t straight = 8u;
    static constexpr uint32_t slanted = 4u;
    /* Two triangles a stroke. */
    static constexpr uint32_t triangles = 2u * (2u * straight + slanted);
    static constexpr uint32_t vertices = 3u * triangles;

    explicit TirCoverageWork(D3d11TirEntry &d3d11);
    ~TirCoverageWork();
    TirCoverageWork(const TirCoverageWork &) = delete;
    TirCoverageWork &operator=(const TirCoverageWork &) = delete;

    /* Every step through `steps`, stopping at the first that failed: the
     * device, the pipeline, the draw, the readback, the coverage and the
     * device still standing. */
    void run(ScenarioSteps &steps);

    bool invariants_hold() const;

private:
    /* A quad from (x0, y0) to (x1, y1), in subpixels, as two triangles. */
    void quad(int32_t x0, int32_t y0, int32_t x1, int32_t y1);
    /* A stroke from (x0, y0) to (x1, y1), `width` subpixels across in x. */
    void stroke(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t width);
    void add(int32_t x, int32_t y);
    /* What D3D's rasterizer leaves in each texel: every triangle's mask
     * OR'd. */
    void expect();
    /* How many texels came back the mask D3D's rasterizer gives. */
    uint32_t right() const;

    D3d11TirEntry &d3d11_;
    bool           made_;
    bool           read_;
    uint32_t       added_;
    int32_t        x_[vertices];
    int32_t        y_[vertices];
    float          positions_[2u * vertices];
    uint32_t       texels_[texels];
    uint32_t       expected_[texels];

    friend struct TirCoverageWorkProbe;
};

}  // namespace av

#endif  // AV_TIR_COVERAGE_WORK_HPP
