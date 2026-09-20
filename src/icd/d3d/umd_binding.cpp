#include "icd/d3d/umd_binding.hpp"

#include "core/trace/function_scope.hpp"

namespace av::d3d {

void UmdBinding::bind(UmdRoot &root)
{
    AV_SCOPE("UmdBinding::bind");
    root_ = &root;
}

void UmdBinding::unbind()
{
    AV_SCOPE("UmdBinding::unbind");
    root_ = nullptr;
}

bool UmdBinding::bound() const
{
    AV_SCOPE("UmdBinding::bound");
    return root_ != nullptr;
}

UmdRoot &UmdBinding::root() const
{
    AV_SCOPE("UmdBinding::root");
    return *root_;
}

}  // namespace av::d3d
