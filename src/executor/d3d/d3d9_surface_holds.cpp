#include "executor/d3d/d3d9_surface_holds.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d_headers.hpp"

namespace av {

D3d9SurfaceHolds::D3d9SurfaceHolds(BlockSource &blocks) : held_(blocks, Pool::paged), first_(0u), newest_(0u)
{
    AV_SCOPE("D3d9SurfaceHolds::D3d9SurfaceHolds");
}

D3d9SurfaceHolds::~D3d9SurfaceHolds()
{
    AV_SCOPE("D3d9SurfaceHolds::~D3d9SurfaceHolds");
    release_all();
}

Status D3d9SurfaceHolds::hold(IUnknown *surface, uint64_t value)
{
    AV_SCOPE("D3d9SurfaceHolds::hold");
    if (surface == nullptr || value == 0u || value <= newest_) return Status::invalid_parameter;
    const Status appended = held_.append(Held{surface, value});
    if (appended != Status::ok) return appended;
    surface->AddRef();
    newest_ = value;
    return Status::ok;
}

void D3d9SurfaceHolds::release_oldest()
{
    AV_SCOPE("D3d9SurfaceHolds::release_oldest");
    if (first_ == held_.size()) return;
    IUnknown *const gone = held_.at(first_).surface_;
    first_++;
    if (first_ == held_.size()) {
        held_.clear();
        first_ = 0u;
    }
    gone->Release();
}

void D3d9SurfaceHolds::release_all()
{
    AV_SCOPE("D3d9SurfaceHolds::release_all");
    while (first_ != held_.size()) release_oldest();
}

uint32_t D3d9SurfaceHolds::pending() const
{
    AV_SCOPE("D3d9SurfaceHolds::pending");
    return held_.size() - first_;
}

uint64_t D3d9SurfaceHolds::oldest() const
{
    AV_SCOPE("D3d9SurfaceHolds::oldest");
    return first_ == held_.size() ? 0u : held_.at(first_).value_;
}

bool D3d9SurfaceHolds::invariants_hold() const
{
    AV_SCOPE("D3d9SurfaceHolds::invariants_hold");
    if (!held_.invariants_hold() || first_ > held_.size()) return false;
    for (uint32_t index = first_; index < held_.size(); index++) {
        if (held_.at(index).surface_ == nullptr) return false;
        if (index > first_ && held_.at(index - 1u).value_ >= held_.at(index).value_) return false;
    }
    return first_ == held_.size() || held_.at(held_.size() - 1u).value_ == newest_;
}

}
