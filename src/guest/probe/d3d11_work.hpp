#ifndef AV_D3D11_WORK_HPP
#define AV_D3D11_WORK_HPP

#include <stdint.h>

namespace av {

class D3d11Entry;
class ImageSink;
class ScenarioSteps;
struct D3d11WorkProbe;

class D3d11Work {
public:
    static constexpr uint32_t extent = 64u;
    static constexpr uint32_t pixels = extent * extent;
    static constexpr uint32_t pixel_bytes = 4u;
    static constexpr uint8_t colour[4] = {64u, 128u, 191u, 255u};

    D3d11Work(D3d11Entry &d3d11, ImageSink &images);
    ~D3d11Work();
    D3d11Work(const D3d11Work &) = delete;
    D3d11Work &operator=(const D3d11Work &) = delete;

    void     run(ScenarioSteps &steps, void *window);
    uint64_t adapter() const;
    uint32_t feature_level() const;

    bool invariants_hold() const;

private:
    uint32_t coloured() const;

    D3d11Entry &d3d11_;
    ImageSink  &images_;
    uint64_t    adapter_;
    uint32_t    feature_level_;
    bool        made_;
    bool        read_;
    uint8_t     frame_[pixels * pixel_bytes];

    friend struct D3d11WorkProbe;
};

}

#endif
