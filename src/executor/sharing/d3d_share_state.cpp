#include "executor/sharing/d3d_share_state.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/sharing/d3d_retired.hpp"
#include "executor/shim/d3d_headers.hpp"

namespace av {

D3dShareState::D3dShareState(ShareOrigin origin)
    : resource_(nullptr), view_(nullptr), shown_(nullptr), writes_(0u), opens_(0u), origin_(origin)
{
    AV_SCOPE("D3dShareState::D3dShareState");
}

IUnknown *D3dShareState::resource() const
{
    AV_SCOPE("D3dShareState::resource");
    return resource_;
}

IUnknown *D3dShareState::view() const
{
    AV_SCOPE("D3dShareState::view");
    return view_ != nullptr ? view_ : resource_;
}

IUnknown *D3dShareState::shown() const
{
    AV_SCOPE("D3dShareState::shown");
    return shown_;
}

IUnknown *D3dShareState::referenced(IUnknown *object) const
{
    AV_SCOPE("D3dShareState::referenced");
    if (object != nullptr) object->AddRef();
    return object;
}

void D3dShareState::keep_resource(IUnknown *resource)
{
    AV_SCOPE("D3dShareState::keep_resource");
    resource_ = referenced(resource);
}

void D3dShareState::keep_view(IUnknown *view, D3dRetired &retired)
{
    AV_SCOPE("D3dShareState::keep_view");
    retire(view_, retired);
    view_ = referenced(view);
}

void D3dShareState::keep_shown(IUnknown *copy, D3dRetired &retired)
{
    AV_SCOPE("D3dShareState::keep_shown");
    retire(shown_, retired);
    shown_ = referenced(copy);
}

IUnknown *D3dShareState::open()
{
    AV_SCOPE("D3dShareState::open");
    IUnknown *const opened = shown_ != nullptr ? shown_ : view();
    if (opened == nullptr) return nullptr;
    opens_++;
    return referenced(opened);
}

void D3dShareState::retire(IUnknown *&held, D3dRetired &retired)
{
    AV_SCOPE("D3dShareState::retire");
    retired.retire(held);
    held = nullptr;
}

void D3dShareState::retire_all(D3dRetired &retired)
{
    AV_SCOPE("D3dShareState::retire_all");
    retire(resource_, retired);
    retire(view_, retired);
    retire(shown_, retired);
}

void D3dShareState::retire_direct3d11(D3dRetired &retired)
{
    AV_SCOPE("D3dShareState::retire_direct3d11");
    if (origin_ == ShareOrigin::d3d11) retire(resource_, retired);
    retire(view_, retired);
    retire(shown_, retired);
}

void D3dShareState::retire_direct3d12(D3dRetired &retired)
{
    AV_SCOPE("D3dShareState::retire_direct3d12");
    if (origin_ == ShareOrigin::d3d12) retire(resource_, retired);
}

void D3dShareState::written()
{
    AV_SCOPE("D3dShareState::written");
    writes_++;
}

uint64_t D3dShareState::writes() const
{
    AV_SCOPE("D3dShareState::writes");
    return writes_;
}

uint64_t D3dShareState::opens() const
{
    AV_SCOPE("D3dShareState::opens");
    return opens_;
}

}  // namespace av
