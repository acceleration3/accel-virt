#ifndef AV_D3D_SHARES_HPP
#define AV_D3D_SHARES_HPP

#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/sharing/host_shares.hpp"

#include <stdint.h>

struct IUnknown;

namespace av {

class SessionBackend;

class D3dShares {
public:
    explicit D3dShares(HostShares &shares);
    D3dShares(const D3dShares &) = delete;
    D3dShares &operator=(const D3dShares &) = delete;

    [[nodiscard]] Status publish_resource(uint32_t domain, uint64_t id, IUnknown *resource, ShareOrigin origin,
                                          SessionBackend &session);
    IUnknown *open_resource(uint32_t domain, uint64_t id, bool &live_of_presented);
    void      expect_shown(uint32_t domain, uint64_t id);
    IUnknown *resource(uint32_t domain, uint64_t id);
    IUnknown *view(uint32_t domain, uint64_t id);
    IUnknown *shown(uint32_t domain, uint64_t id);
    bool      keep_view(uint32_t domain, uint64_t id, IUnknown *view);
    bool      keep_shown(uint32_t domain, uint64_t id, IUnknown *copy);
    void      written(uint32_t domain, uint64_t id);
    bool      writes(uint32_t domain, uint64_t id, uint64_t &count);

    void      lose_direct3d11();
    void      lose_direct3d12();
    template <typename Visit>
    void      each_direct3d11(Visit visit);
    [[nodiscard]] bool restore_resource(uint32_t domain, uint64_t id, IUnknown *resource);
    uint32_t  take_retired(IUnknown **objects, uint32_t capacity);

private:
    template <typename Pick>
    IUnknown *referenced(uint32_t domain, uint64_t id, Pick pick);

    HostShares &shares_;
};

template <typename Visit>
void D3dShares::each_direct3d11(Visit visit)
{
    AV_SCOPE("D3dShares::each_direct3d11");
    shares_.each_direct3d([&visit](Share &share, D3dRetired &) {
        if (share.origin() == ShareOrigin::d3d11 && share.direct3d().resource() != nullptr)
            visit(share.domain(), share.id(), share.direct3d().resource());
    });
}

template <typename Pick>
IUnknown *D3dShares::referenced(uint32_t domain, uint64_t id, Pick pick)
{
    AV_SCOPE("D3dShares::referenced");
    IUnknown *found = nullptr;
    static_cast<void>(shares_.with_direct3d(domain, id, [&found, &pick](Share &share, D3dRetired &) {
        found = share.direct3d().referenced(pick(share.direct3d()));
    }));
    return found;
}

}

#endif
