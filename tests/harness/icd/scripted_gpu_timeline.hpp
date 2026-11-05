#ifndef AV_SCRIPTED_GPU_TIMELINE_HPP
#define AV_SCRIPTED_GPU_TIMELINE_HPP

#include "icd/common/gpu_timeline.hpp"

#include <cstdint>

namespace av {

class ScriptedGpuTimeline final : public icd::GpuTimeline {
public:
    ScriptedGpuTimeline();

    std::uint64_t recording() const override;
    std::uint64_t completed() const override;
    bool          wait(std::uint64_t fence) override;

    void          send();
    void          finish(std::uint64_t fence);
    void          lose();
    std::uint32_t waits() const;

private:
    std::uint64_t recording_;
    std::uint64_t completed_;
    std::uint32_t waits_;
    bool          lost_;
};

}

#endif
