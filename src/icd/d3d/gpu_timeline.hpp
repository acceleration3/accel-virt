/* How far the host GPU has got with what a session sent, as memory the GPU
 * reads is reused by (docs/mapped-buffers-plan.md, 3.3).
 *
 * Fences number the session's batches. A command recorded now goes out in the
 * batch recording() names; memory it reads is free once completed() reaches
 * that fence. Virtual because the session's transport (SessionTimeline) is the
 * driver's answer and a test plays another. */
#ifndef AV_D3D_GPU_TIMELINE_HPP
#define AV_D3D_GPU_TIMELINE_HPP

#include <stdint.h>

namespace av::d3d {

class GpuTimeline {
public:
    GpuTimeline(const GpuTimeline &) = delete;
    GpuTimeline &operator=(const GpuTimeline &) = delete;

    /* The fence of the batch the commands recorded now will go out in. It
     * never decreases. */
    virtual uint64_t recording() const = 0;
    /* The highest fence the host GPU has finished. */
    virtual uint64_t completed() const = 0;
    /* Returns once completed() reaches `fence`, sending what is recorded first
     * when the fence is the batch still being recorded. False when it cannot
     * be waited for: the session is lost. */
    [[nodiscard]] virtual bool wait(uint64_t fence) = 0;

protected:
    GpuTimeline() = default;
    ~GpuTimeline() = default;
};

}  // namespace av::d3d

#endif  // AV_D3D_GPU_TIMELINE_HPP
