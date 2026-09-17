#include "icd/vulkan/frame_pacer.hpp"

#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"
#include "icd/common/log_sink.hpp"

namespace av::icd {

FramePacer::FramePacer(Kmt &kmt, LogSink &log, uint32_t adapter, uint32_t device)
    : kmt_(kmt), log_(log), adapter_(adapter), device_(device), blanks_(0u), failures_(0u)
{
    AV_SCOPE("FramePacer::FramePacer");
}

void FramePacer::wait()
{
    AV_SCOPE("FramePacer::wait");
    if (adapter_ != 0u && kmt_.wait_for_vertical_blank(adapter_, device_, primary_source) == Status::ok) {
        blanks_++;
        return;
    }
    if (failures_++ == 0u) log_.write("rvkvk: FIFO present unpaced: the adapter's vertical blank wait failed");
}

uint64_t FramePacer::blanks() const
{
    AV_SCOPE("FramePacer::blanks");
    return blanks_;
}

uint64_t FramePacer::failures() const
{
    AV_SCOPE("FramePacer::failures");
    return failures_;
}

/* A blank is only ever counted on an adapter: with none, every wait fails. */
bool FramePacer::invariants_hold() const
{
    AV_SCOPE("FramePacer::invariants_hold");
    return adapter_ != 0u || blanks_ == 0u;
}

}  // namespace av::icd
