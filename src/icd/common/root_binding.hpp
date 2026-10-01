/* The one pointer a driver's C entry points reach its root through
 * (constitution III). An entry point is a bare function pointer - a Vulkan or
 * OpenCL dispatch slot, an OpenGL hook the emitter calls by name, a Direct3D
 * DDI thunk - called with its API's arguments only and nowhere to put a
 * `this`. The root binds itself when it is constructed and unbinds when it is
 * destroyed: in a DLL that is the root in the exports' storage, in a test the
 * root under test. Every entry point was handed out by a bound root, so none
 * asks with nothing bound. Each driver names its own (IcdBinding,
 * GlIcdBinding, ClIcdBinding, UmdBinding) and defines one beside its entry
 * points. */
#ifndef AV_ICD_ROOT_BINDING_HPP
#define AV_ICD_ROOT_BINDING_HPP

#include "core/trace/function_scope.hpp"

namespace av::icd {

template <typename Root>
class RootBinding {
public:
    constexpr RootBinding();
    RootBinding(const RootBinding &) = delete;
    RootBinding &operator=(const RootBinding &) = delete;

    void  bind(Root &root);
    void  unbind();
    bool  bound() const;
    /* Only while bound. */
    Root &root() const;

private:
    Root *root_;
};

template <typename Root>
constexpr RootBinding<Root>::RootBinding() : root_(nullptr)
{}

template <typename Root>
void RootBinding<Root>::bind(Root &root)
{
    AV_SCOPE("RootBinding::bind");
    root_ = &root;
}

template <typename Root>
void RootBinding<Root>::unbind()
{
    AV_SCOPE("RootBinding::unbind");
    root_ = nullptr;
}

template <typename Root>
bool RootBinding<Root>::bound() const
{
    AV_SCOPE("RootBinding::bound");
    return root_ != nullptr;
}

template <typename Root>
Root &RootBinding<Root>::root() const
{
    AV_SCOPE("RootBinding::root");
    return *root_;
}

}  // namespace av::icd

#endif  // AV_ICD_ROOT_BINDING_HPP
