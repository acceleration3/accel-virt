#include "icd/d3d/session_refusal.hpp"

#include "core/trace/function_scope.hpp"

namespace av::d3d {

SessionRefusal::SessionRefusal(bool exhausted) : exhausted_(exhausted)
{
    AV_SCOPE("SessionRefusal::SessionRefusal");
}

Hresult SessionRefusal::for_d3d() const
{
    AV_SCOPE("SessionRefusal::for_d3d");
    return exhausted_ ? hr_out_of_memory : hr_fail;
}

Hresult SessionRefusal::for_d3d9() const
{
    AV_SCOPE("SessionRefusal::for_d3d9");
    return exhausted_ ? hr_d3d9_out_of_video_memory : hr_fail;
}

}
