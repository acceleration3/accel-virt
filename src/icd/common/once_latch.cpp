#include "icd/common/once_latch.hpp"

#include "core/trace/function_scope.hpp"

namespace av::icd {

OnceLatch::OnceLatch() : seen_(false)
{
    AV_SCOPE("OnceLatch::OnceLatch");
}

bool OnceLatch::first()
{
    AV_SCOPE("OnceLatch::first");
    if (seen_) return false;
    seen_ = true;
    return true;
}

}
