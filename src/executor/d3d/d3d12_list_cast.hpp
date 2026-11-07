#ifndef AV_D3D12_LIST_CAST_HPP
#define AV_D3D12_LIST_CAST_HPP

#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d_headers.hpp"

namespace av {

template <typename Interface>
class D3d12ListCast {
public:
    explicit D3d12ListCast(ID3D12GraphicsCommandList *list) : cast_(nullptr)
    {
        AV_SCOPE("D3d12ListCast::D3d12ListCast");
        if (list == nullptr || FAILED(list->QueryInterface(__uuidof(Interface), reinterpret_cast<void **>(&cast_))))
            cast_ = nullptr;
    }
    ~D3d12ListCast()
    {
        AV_SCOPE("D3d12ListCast::~D3d12ListCast");
        if (cast_ != nullptr) cast_->Release();
    }
    D3d12ListCast(const D3d12ListCast &) = delete;
    D3d12ListCast &operator=(const D3d12ListCast &) = delete;

    Interface *get() const
    {
        AV_SCOPE("D3d12ListCast::get");
        return cast_;
    }

private:
    Interface *cast_;
};

}

#endif
