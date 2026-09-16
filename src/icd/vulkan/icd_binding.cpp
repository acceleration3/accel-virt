#include "icd/vulkan/icd_binding.hpp"

#include "core/trace/function_scope.hpp"

namespace av::icd {

void     IcdBinding::bind(IcdRoot &root)
{
    AV_SCOPE("IcdBinding::bind");
    root_ = &root;
}
void     IcdBinding::unbind()
{
    AV_SCOPE("IcdBinding::unbind");
    root_ = nullptr;
}
bool     IcdBinding::bound() const
{
    AV_SCOPE("IcdBinding::bound");
    return root_ != nullptr;
}
IcdRoot &IcdBinding::root() const
{
    AV_SCOPE("IcdBinding::root");
    return *root_;
}

}  // namespace av::icd
