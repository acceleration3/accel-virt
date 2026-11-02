#ifndef AV_D3D9_WORK_HPP
#define AV_D3D9_WORK_HPP

#include <stdint.h>

namespace av {

class D3d9Entry;
class ImageSink;
class ScenarioSteps;
struct D3d9WorkProbe;

class D3d9Work {
public:
    static constexpr uint32_t extent = 64u;
    static constexpr uint32_t pixels = extent * extent;
    static constexpr uint32_t pixel_bytes = 4u;
    static constexpr uint8_t  colour[4] = {64u, 128u, 191u, 255u};

    D3d9Work(D3d9Entry &d3d9, ImageSink &images);
    ~D3d9Work();
    D3d9Work(const D3d9Work &) = delete;
    D3d9Work &operator=(const D3d9Work &) = delete;

    void     run(ScenarioSteps &steps, void *window);
    uint64_t adapter() const;

    bool invariants_hold() const;

private:
    uint32_t coloured() const;

    D3d9Entry  &d3d9_;
    ImageSink  &images_;
    uint64_t    adapter_;
    bool        made_;
    bool        read_;
    uint8_t     frame_[pixels * pixel_bytes];

    friend struct D3d9WorkProbe;
};

}

#endif
