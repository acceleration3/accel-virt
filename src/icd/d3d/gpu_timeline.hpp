#ifndef AV_D3D_GPU_TIMELINE_HPP
#define AV_D3D_GPU_TIMELINE_HPP

#include <stdint.h>

namespace av::d3d {

class GpuTimeline {
public:
    GpuTimeline(const GpuTimeline &) = delete;
    GpuTimeline &operator=(const GpuTimeline &) = delete;

    virtual uint64_t recording() const = 0;
    virtual uint64_t completed() const = 0;
    [[nodiscard]] virtual bool wait(uint64_t fence) = 0;

protected:
    GpuTimeline() = default;
    ~GpuTimeline() = default;
};

}

#endif
