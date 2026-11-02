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
    static constexpr uint32_t straight = 8u;
    static constexpr uint32_t slanted = 4u;
    static constexpr uint32_t triangles = 2u * (2u * straight + slanted);
    static constexpr uint32_t vertices = 3u * triangles;

    explicit TirCoverageWork(D3d11TirEntry &d3d11);
    ~TirCoverageWork();
    TirCoverageWork(const TirCoverageWork &) = delete;
    TirCoverageWork &operator=(const TirCoverageWork &) = delete;

    void run(ScenarioSteps &steps);

    bool invariants_hold() const;

private:
    void quad(int32_t x0, int32_t y0, int32_t x1, int32_t y1);
    void stroke(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t width);
    void add(int32_t x, int32_t y);
    void expect();
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

}

#endif
