/* A session's GPU as a test drives it: batches are recorded one after
 * another, the test says when one is sent and when the GPU finishes, and a
 * wait sends the batch being recorded if it must, then finishes it - or
 * fails, once the session is lost. */
#ifndef AV_SCRIPTED_GPU_TIMELINE_HPP
#define AV_SCRIPTED_GPU_TIMELINE_HPP

#include "icd/d3d/gpu_timeline.hpp"

#include <cstdint>

namespace av {

class ScriptedGpuTimeline final : public d3d::GpuTimeline {
public:
    ScriptedGpuTimeline();

    std::uint64_t recording() const override;
    std::uint64_t completed() const override;
    bool          wait(std::uint64_t fence) override;

    /* The batch being recorded goes out, and the next one starts. */
    void          send();
    /* The GPU finishes every batch up to `fence`. */
    void          finish(std::uint64_t fence);
    void          lose();
    std::uint32_t waits() const;

private:
    std::uint64_t recording_;
    std::uint64_t completed_;
    std::uint32_t waits_;
    bool          lost_;
};

}  // namespace av

#endif  // AV_SCRIPTED_GPU_TIMELINE_HPP
