#include "icd/common/poll_pause.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"

namespace av::icd {

PollPause::PollPause(Kmt &kmt) : kmt_(kmt), frequency_(0u), started_(kmt.performance_counter(frequency_))
{
    AV_SCOPE("PollPause::PollPause");
}

bool PollPause::spinning() const
{
    AV_SCOPE("PollPause::spinning");
    uint64_t unused = 0u;
    return frequency_ != 0u && kmt_.performance_counter(unused) - started_ < frequency_ / spin_per_second;
}

void PollPause::rest()
{
    AV_SCOPE("PollPause::rest");
    kmt_.sleep_briefly();
}

void PollPause::between()
{
    AV_SCOPE("PollPause::between");
    if (!spinning()) rest();
}

}  // namespace av::icd
