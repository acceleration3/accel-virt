#include "executor/sharing/d3d_shares.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

D3dShares::D3dShares(HostShares &shares) : shares_(shares)
{
    AV_SCOPE("D3dShares::D3dShares");
}

Status D3dShares::publish_resource(uint32_t domain, uint64_t id, IUnknown *resource, ShareOrigin origin,
                                   SessionBackend &session)
{
    AV_SCOPE("D3dShares::publish_resource");
    if (resource == nullptr) return Status::invalid_parameter;
    return shares_.publish_direct3d(domain, id, origin, session,
                                    [resource](Share &share) { share.direct3d().keep_resource(resource); });
}

IUnknown *D3dShares::open_resource(uint32_t domain, uint64_t id, bool &live_of_presented)
{
    AV_SCOPE("D3dShares::open_resource");
    IUnknown *opened = nullptr;
    live_of_presented = false;
    static_cast<void>(shares_.with_direct3d(domain, id, [&](Share &share, D3dRetired &) {
        opened = share.direct3d().open(live_of_presented);
    }));
    return opened;
}

void D3dShares::expect_shown(uint32_t domain, uint64_t id)
{
    AV_SCOPE("D3dShares::expect_shown");
    static_cast<void>(
        shares_.with_direct3d(domain, id, [](Share &share, D3dRetired &) { share.direct3d().expect_shown(); }));
}

IUnknown *D3dShares::resource(uint32_t domain, uint64_t id)
{
    AV_SCOPE("D3dShares::resource");
    return referenced(domain, id, [](const D3dShareState &state) { return state.resource(); });
}

IUnknown *D3dShares::view(uint32_t domain, uint64_t id)
{
    AV_SCOPE("D3dShares::view");
    return referenced(domain, id, [](const D3dShareState &state) { return state.view(); });
}

IUnknown *D3dShares::shown(uint32_t domain, uint64_t id)
{
    AV_SCOPE("D3dShares::shown");
    return referenced(domain, id, [](const D3dShareState &state) { return state.shown(); });
}

bool D3dShares::keep_view(uint32_t domain, uint64_t id, IUnknown *view)
{
    AV_SCOPE("D3dShares::keep_view");
    if (view == nullptr) return false;
    return shares_.with_direct3d(
        domain, id, [view](Share &share, D3dRetired &retired) { share.direct3d().keep_view(view, retired); });
}

bool D3dShares::keep_shown(uint32_t domain, uint64_t id, IUnknown *copy)
{
    AV_SCOPE("D3dShares::keep_shown");
    if (copy == nullptr) return false;
    return shares_.with_direct3d(
        domain, id, [copy](Share &share, D3dRetired &retired) { share.direct3d().keep_shown(copy, retired); });
}

void D3dShares::written(uint32_t domain, uint64_t id)
{
    AV_SCOPE("D3dShares::written");
    static_cast<void>(
        shares_.with_direct3d(domain, id, [](Share &share, D3dRetired &) { share.direct3d().written(); }));
}

bool D3dShares::writes(uint32_t domain, uint64_t id, uint64_t &count)
{
    AV_SCOPE("D3dShares::writes");
    count = 0u;
    return shares_.with_direct3d(
        domain, id, [&count](Share &share, D3dRetired &) { count = share.direct3d().writes(); });
}

void D3dShares::lose_direct3d11()
{
    AV_SCOPE("D3dShares::lose_direct3d11");
    shares_.each_direct3d([](Share &share, D3dRetired &retired) { share.direct3d().retire_direct3d11(retired); });
}

void D3dShares::lose_direct3d12()
{
    AV_SCOPE("D3dShares::lose_direct3d12");
    shares_.each_direct3d([](Share &share, D3dRetired &retired) { share.direct3d().retire_direct3d12(retired); });
}

bool D3dShares::restore_resource(uint32_t domain, uint64_t id, IUnknown *resource)
{
    AV_SCOPE("D3dShares::restore_resource");
    if (resource == nullptr) return false;
    bool restored = false;
    static_cast<void>(shares_.with_direct3d(domain, id, [resource, &restored](Share &share, D3dRetired &) {
        if (share.origin() != ShareOrigin::d3d11 || share.direct3d().resource() != nullptr) return;
        share.direct3d().keep_resource(resource);
        restored = true;
    }));
    return restored;
}

uint32_t D3dShares::take_retired(IUnknown **objects, uint32_t capacity)
{
    AV_SCOPE("D3dShares::take_retired");
    uint32_t taken = 0u;
    shares_.with_retired([objects, capacity, &taken](D3dRetired &retired) { taken = retired.take(objects, capacity); });
    return taken;
}

}  // namespace av
