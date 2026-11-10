#ifndef AV_D3D12_INTERFACE_CAST_HPP
#define AV_D3D12_INTERFACE_CAST_HPP

#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d12_newer_interfaces.hpp"
#include "executor/shim/d3d_headers.hpp"

namespace av {

template <typename Interface>
class D3d12InterfaceCast {
public:
    explicit D3d12InterfaceCast(IUnknown *object) : cast_(nullptr)
    {
        AV_SCOPE("D3d12InterfaceCast::D3d12InterfaceCast");
        if (object == nullptr || FAILED(object->QueryInterface(__uuidof(Interface), reinterpret_cast<void **>(&cast_))))
            cast_ = nullptr;
    }
    ~D3d12InterfaceCast()
    {
        AV_SCOPE("D3d12InterfaceCast::~D3d12InterfaceCast");
        if (cast_ != nullptr) cast_->Release();
    }
    D3d12InterfaceCast(const D3d12InterfaceCast &) = delete;
    D3d12InterfaceCast &operator=(const D3d12InterfaceCast &) = delete;

    Interface *get() const
    {
        AV_SCOPE("D3d12InterfaceCast::get");
        return cast_;
    }

private:
    Interface *cast_;
};

}

#endif
