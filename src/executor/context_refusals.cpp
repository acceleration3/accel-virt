#include "executor/context_refusals.hpp"

#include "core/log/driver_log.hpp"
#include "core/log/log_line.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

ContextRefusals::ContextRefusals(DriverLog &log) : log_(log), seen_{}
{
    AV_SCOPE("ContextRefusals::ContextRefusals");
}

void ContextRefusals::refuse(Reason reason, const char *api, uint64_t detail)
{
    AV_SCOPE("ContextRefusals::refuse");
    const uint32_t index = static_cast<uint32_t>(reason);
    if (index >= static_cast<uint32_t>(Reason::count)) return;
    const uint64_t seen = __atomic_add_fetch(&seen_[index], 1u, __ATOMIC_RELAXED);
    if (seen > spoken_each && seen % then_every != 0u) return;
    LogLine(log_)
        .text("the ")
        .text(api)
        .text(" context is refused: ")
        .text(describe(reason))
        .text(" (")
        .number(detail)
        .text("), refusal ")
        .number(seen)
        .text(" of this kind")
        .write();
}

uint64_t ContextRefusals::refused(Reason reason) const
{
    AV_SCOPE("ContextRefusals::refused");
    const uint32_t index = static_cast<uint32_t>(reason);
    if (index >= static_cast<uint32_t>(Reason::count)) return 0u;
    return __atomic_load_n(&seen_[index], __ATOMIC_RELAXED);
}

const char *ContextRefusals::describe(Reason reason) const
{
    AV_SCOPE("ContextRefusals::describe");
    switch (reason) {
    case Reason::domain: return "the domain names no GPU this host runs, domain";
    case Reason::options: return "context options were asked of an API that takes none, options flags";
    case Reason::share_api: return "a shared context was asked of an API that cannot share, share api";
    case Reason::sub_api_mask: return "the GPU does not run this API, GPU sub-API mask";
    case Reason::no_backend: return "no backend serves this sub-API value, value";
    case Reason::share_gone: return "the context to share with is not live on this GPU, sharing";
    case Reason::bind: return "the backend would not start; host RAM refusals so far";
    case Reason::d3d11_device: return "the Direct3D 11 host device is absent or did not renew within 2000 ms, device present";
    default: return "unknown";
    }
}

}
