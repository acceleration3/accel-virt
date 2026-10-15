#include "icd/opencl/cl_context_notify.hpp"

#include "core/trace/function_scope.hpp"

namespace av::clicd {

ClContextNotify::ClContextNotify() : callback_(nullptr), user_data_(nullptr)
{
    AV_SCOPE("ClContextNotify::ClContextNotify");
}

ClContextNotify::ClContextNotify(Callback callback, void *user_data) : callback_(callback), user_data_(user_data)
{
    AV_SCOPE("ClContextNotify::ClContextNotify");
}

void ClContextNotify::say(const char *text) const
{
    AV_SCOPE("ClContextNotify::say");
    if (callback_ != nullptr) callback_(text, nullptr, 0u, user_data_);
}

}  // namespace av::clicd
