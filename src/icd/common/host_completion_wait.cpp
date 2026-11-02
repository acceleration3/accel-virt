#include "icd/common/host_completion_wait.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"
#include "icd/common/completion_probe.hpp"

namespace av::icd {

HostCompletionWait::HostCompletionWait(const Kmt &kmt, uint64_t timeout)
    : kmt_(kmt), frequency_(0u), start_(kmt.performance_counter(frequency_)), wait_(timeout, start_, frequency_)
{
    AV_SCOPE("HostCompletionWait::HostCompletionWait");
}

bool HostCompletionWait::run(CompletionProbe &probe)
{
    AV_SCOPE("HostCompletionWait::run");
    uint64_t slice = 0u;
    while (wait_.next(kmt_.performance_counter(frequency_), slice)) {
        if (probe.ask(slice)) return true;
        probe.pause(slice);
    }
    return false;
}

}
