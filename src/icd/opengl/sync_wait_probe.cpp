#include "icd/opengl/sync_wait_probe.hpp"

#include "core/trace/function_scope.hpp"

#include "gl_dispatch_client.h"

namespace av::glicd {

SyncWaitProbe::SyncWaitProbe(ClientTransport &wire, uint64_t sync, uint32_t flags)
    : wire_(wire), sync_(sync), flags_(flags), expired_(false), result_(gl_timeout_expired)
{
    AV_SCOPE("SyncWaitProbe::SyncWaitProbe");
}

bool SyncWaitProbe::ask(uint64_t slice)
{
    AV_SCOPE("SyncWaitProbe::ask");
    const uint32_t flags = expired_ ? flags_ & ~uint32_t{gl_sync_flush_commands_bit} : flags_;
    const uint32_t answered = glproto::av_glClientWaitSync(wire_, sync_, flags, slice);
    if (answered == gl_timeout_expired) {
        expired_ = true;
        return false;
    }
    result_ = expired_ && answered == gl_already_signaled ? uint32_t{gl_condition_satisfied} : answered;
    return true;
}

void SyncWaitProbe::pause(uint64_t)
{
    AV_SCOPE("SyncWaitProbe::pause");
}

uint32_t SyncWaitProbe::result() const
{
    AV_SCOPE("SyncWaitProbe::result");
    return result_;
}

}
