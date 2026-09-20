#include "icd/opencl/cl_icd_binding.hpp"

#include "core/trace/function_scope.hpp"

namespace av::clicd {

void ClIcdBinding::bind(ClIcdRoot &root)
{
    AV_SCOPE("ClIcdBinding::bind");
    root_ = &root;
}

void ClIcdBinding::unbind()
{
    AV_SCOPE("ClIcdBinding::unbind");
    root_ = nullptr;
}

bool ClIcdBinding::bound() const
{
    AV_SCOPE("ClIcdBinding::bound");
    return root_ != nullptr;
}

ClIcdRoot &ClIcdBinding::root() const
{
    AV_SCOPE("ClIcdBinding::root");
    return *root_;
}

}  // namespace av::clicd
