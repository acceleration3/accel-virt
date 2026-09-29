#include "icd/opencl/cl_noted_error.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/opencl/cl_icd_root.hpp"

namespace av::clicd {

ClNotedError::ClNotedError(ClIcdRoot &root, const char *entry_point, cl_int *application)
    : root_(root), entry_point_(entry_point), application_(application), error_(CL_SUCCESS)
{
    AV_SCOPE("ClNotedError::ClNotedError");
}

ClNotedError::~ClNotedError()
{
    AV_SCOPE("ClNotedError::~ClNotedError");
    if (application_ != nullptr) *application_ = error_;
    static_cast<void>(root_.noted(entry_point_, error_));
}

cl_int *ClNotedError::pointer()
{
    AV_SCOPE("ClNotedError::pointer");
    return &error_;
}

}  // namespace av::clicd
