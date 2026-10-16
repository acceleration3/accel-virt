/* The Direct3D half of one GPU's shares: the resources Direct3D sessions
 * publish as they create them and open by key, the Direct3D 11 views and flip
 * copies made of them, how many copies ran into each, and what a lost device
 * made - each share's D3dShareState, over HostShares' rows and under its lock.
 * Every object handed out comes with a reference of the caller's; every one
 * the shares let go of waits in D3dRetired for the Direct3D 11 device to
 * release it under its lock (take_retired). */
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

    /* A Direct3D resource, published under `id` as `session` created it, with
     * a reference of the share's own. conflict for an id the domain already
     * shares, invalid_parameter for id 0 or no resource. */
    [[nodiscard]] Status publish_resource(uint32_t domain, uint64_t id, IUnknown *resource, ShareOrigin origin,
                                          SessionBackend &session);
    /* What a Direct3D session opening `id` gets: the flip copy a compositor
     * reads, else the Direct3D 11 view, else the resource. Null for an
     * unknown id. */
    IUnknown *open_resource(uint32_t domain, uint64_t id);
    /* The resource `id` was published as, its Direct3D 11 view (or the
     * resource), and its flip copy; null for none. */
    IUnknown *resource(uint32_t domain, uint64_t id);
    IUnknown *view(uint32_t domain, uint64_t id);
    IUnknown *shown(uint32_t domain, uint64_t id);
    /* `view` kept as what a Direct3D 11 session opens `id` as, and `copy` as
     * its flip copy, with references of the share's own. False for an
     * unknown id. */
    bool      keep_view(uint32_t domain, uint64_t id, IUnknown *view);
    bool      keep_shown(uint32_t domain, uint64_t id, IUnknown *copy);
    /* One more copy into `id` ran on the host; how many have, false for an
     * unknown id. */
    void      written(uint32_t domain, uint64_t id);
    bool      writes(uint32_t domain, uint64_t id, uint64_t &count);

    /* The GPU's Direct3D 11 or 12 device was lost and is made again: every
     * share lets go of what was that device's (D3dShareState) to
     * take_retired, which the Direct3D 11 device drains under its lock before
     * the old device goes. The shares stay the guest's until it lets go; one
     * whose resource went opens nothing, as a shared handle from before a TDR
     * opens nothing on Windows. */
    void      lose_direct3d11();
    void      lose_direct3d12();
    /* `visit(domain, id, resource)` for every share Direct3D 11 published
     * whose resource it still holds, under the shares' lock: what the lost
     * device made, read before lose_direct3d11 lets it go, so it can be made
     * again (CarriedShare). The visit must not call back into the shares. */
    template <typename Visit>
    void      each_direct3d11(Visit visit);
    /* `resource`, made again on the device made in place of a lost one, kept
     * as what the share Direct3D 11 published as `id` is from now on, with a
     * reference of the share's own: false, nothing kept, for an id no share
     * names any more - the guest let it go meanwhile - or one that holds a
     * resource still. */
    [[nodiscard]] bool restore_resource(uint32_t domain, uint64_t id, IUnknown *resource);
    /* COM references the shares let go of, now the caller's to release under
     * the Direct3D 11 device's lock: up to `capacity`, the count answered. */
    uint32_t  take_retired(IUnknown **objects, uint32_t capacity);

private:
    /* What `pick` chooses of `id`'s objects, with a reference of the
     * caller's; null for an unknown id or none. */
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

}  // namespace av

#endif  // AV_D3D_SHARES_HPP
