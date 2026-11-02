#include "icd/opengl/batch_ready.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"

namespace av::glicd {

BatchReady::BatchReady(Kmt &kmt) : kmt_(kmt)
{
    AV_SCOPE("BatchReady::BatchReady");
}

icd::FrameWait BatchReady::wait(const BatchMark &mark)
{
    AV_SCOPE("BatchReady::wait");
    if (mark.wait(kmt_) == Status::ok) return icd::FrameWait::drawn;
    return kmt_.last_code() == BatchMark::wait_timed_out ? icd::FrameWait::later : icd::FrameWait::gone;
}

}
