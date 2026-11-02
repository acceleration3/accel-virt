#ifndef AV_D3D12_WORK_HPP
#define AV_D3D12_WORK_HPP

#include "guest/probe/app_pointer_check.hpp"

#include <stdint.h>

namespace av {

class D3d12Entry;
class ImageSink;
class ScenarioSteps;
struct D3d12WorkProbe;

class D3d12Work {
public:
    static constexpr uint32_t extent = 64u;
    static constexpr uint32_t pixels = extent * extent;
    static constexpr uint32_t pixel_bytes = 4u;
    static constexpr uint8_t colour[4] = {64u, 128u, 191u, 255u};
    static constexpr uint32_t watched_pages = 2u;

    D3d12Work(D3d12Entry &d3d12, ImageSink &images);
    ~D3d12Work();
    D3d12Work(const D3d12Work &) = delete;
    D3d12Work &operator=(const D3d12Work &) = delete;

    void     run(ScenarioSteps &steps);
    uint64_t adapter() const;
    uint32_t feature_level() const;
    uint32_t serialized_library() const;

    bool invariants_hold() const;

private:
    uint32_t coloured() const;
    void     run_application_memory(ScenarioSteps &steps);

    D3d12Entry &d3d12_;
    ImageSink  &images_;
    uint64_t    adapter_;
    uint32_t    feature_level_;
    uint32_t    serialized_library_;
    bool        made_;
    bool        read_;
    uint8_t     frame_[pixels * pixel_bytes];
    AppPointerCheck existing_;

    friend struct D3d12WorkProbe;
};

}

#endif
