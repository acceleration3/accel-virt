#include "icd/opencl/cl_program_notify.hpp"

#include "core/trace/function_scope.hpp"

namespace av::clicd {

ClProgramNotify::ClProgramNotify() : callback_(nullptr), user_data_(nullptr)
{
    AV_SCOPE("ClProgramNotify::ClProgramNotify");
}

ClProgramNotify::ClProgramNotify(Callback callback, void *user_data) : callback_(callback), user_data_(user_data)
{
    AV_SCOPE("ClProgramNotify::ClProgramNotify");
}

bool ClProgramNotify::valid() const
{
    AV_SCOPE("ClProgramNotify::valid");
    return callback_ != nullptr || user_data_ == nullptr;
}

void ClProgramNotify::tell(cl_program program) const
{
    AV_SCOPE("ClProgramNotify::tell");
    if (callback_ != nullptr && program != nullptr) callback_(program, user_data_);
}

}  // namespace av::clicd
