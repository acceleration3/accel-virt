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

}

#endif
