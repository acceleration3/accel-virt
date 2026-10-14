#include "harness/icd/scripted_gpu_timeline.hpp"

namespace av {

ScriptedGpuTimeline::ScriptedGpuTimeline() : recording_(1u), completed_(0u), waits_(0u), lost_(false) {}

std::uint64_t ScriptedGpuTimeline::recording() const
{
    return recording_;
}

std::uint64_t ScriptedGpuTimeline::completed() const
{
    return completed_;
}

bool ScriptedGpuTimeline::wait(std::uint64_t fence)
{
    waits_++;
    if (lost_) return false;
    if (fence >= recording_) recording_ = fence + 1u;
    if (fence > completed_) completed_ = fence;
    return true;
}

void ScriptedGpuTimeline::send()
{
    recording_++;
}

void ScriptedGpuTimeline::finish(std::uint64_t fence)
{
    completed_ = fence;
}

void ScriptedGpuTimeline::lose()
{
    lost_ = true;
}

std::uint32_t ScriptedGpuTimeline::waits() const
{
    return waits_;
}

}  // namespace av
