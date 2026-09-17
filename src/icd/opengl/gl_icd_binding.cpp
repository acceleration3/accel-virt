#include "icd/opengl/gl_icd_binding.hpp"

#include "core/trace/function_scope.hpp"

namespace av::glicd {

void       GlIcdBinding::bind(GlIcdRoot &root)
{
    AV_SCOPE("GlIcdBinding::bind");
    root_ = &root;
}
void       GlIcdBinding::unbind()
{
    AV_SCOPE("GlIcdBinding::unbind");
    root_ = nullptr;
}
bool       GlIcdBinding::bound() const
{
    AV_SCOPE("GlIcdBinding::bound");
    return root_ != nullptr;
}
GlIcdRoot &GlIcdBinding::root() const
{
    AV_SCOPE("GlIcdBinding::root");
    return *root_;
}

}  // namespace av::glicd
