#include "icd/common/poll_pause.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"

namespace av::icd {

PollPause::PollPause(Kmt &kmt) : kmt_(kmt), spin_(kmt, spin_us)
{
    AV_SCOPE("PollPause::PollPause");
}

bool PollPause::spinning()
{
    AV_SCOPE("PollPause::spinning");
    return spin_.lasts();
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

}
