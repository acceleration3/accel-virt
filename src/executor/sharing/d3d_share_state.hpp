/* The Direct3D objects one share keeps on its GPU's Direct3D devices: the
 * resource as it was published, the Direct3D 11 view a session opens - made
 * over a Direct3D 12 resource, or the resource itself - and the copy a flip
 * presentation took, which a compositor opens in its place; and how many
 * copies into it and opens of it Direct3D made. Each reference is the
 * share's own; one let go of goes to the D3dRetired it is handed, never
 * released here (D3dRetired says why). Guarded by HostShares' lock. */
#ifndef AV_D3D_SHARE_STATE_HPP
#define AV_D3D_SHARE_STATE_HPP

#include "executor/sharing/share_origin.hpp"

#include <stdint.h>

struct IUnknown;

namespace av {

class D3dRetired;

class D3dShareState {
public:
    /* What a share of `origin` keeps: a lost device takes back what was its. */
    explicit D3dShareState(ShareOrigin origin);
    D3dShareState(const D3dShareState &) = delete;
    D3dShareState &operator=(const D3dShareState &) = delete;

    /* Each with no reference of the caller's; view() is the resource itself
     * where no view of its own was made. Null for none. */
    IUnknown *resource() const;
    IUnknown *view() const;
    IUnknown *shown() const;
    /* `object`, with a reference of the caller's added; null for null. */
    IUnknown *referenced(IUnknown *object) const;

    /* `resource` kept as what was published, with a reference of its own. */
    void      keep_resource(IUnknown *resource);
    /* `view` kept as what a Direct3D 11 session opens, with a reference of
     * its own; a view of its own it replaces is retired, the resource stays. */
    void      keep_view(IUnknown *view, D3dRetired &retired);
    /* `copy` kept as the flip copy, with a reference of its own; the one it
     * replaces is retired. */
    void      keep_shown(IUnknown *copy, D3dRetired &retired);
    /* What a Direct3D session opening the share gets - the flip copy, else
     * the view - with a reference of the caller's, counted as an open; null
     * when there is neither. */
    IUnknown *open();

    /* Every reference retired: the share is freed. */
    void      retire_all(D3dRetired &retired);
    /* What a lost device made retired: of Direct3D 11's, the view and the
     * flip copy - both always its - and the resource of a share it published;
     * of Direct3D 12's, the resource of a share it published alone - the view
     * made over it is Direct3D 11's, over memory the lost device's resource
     * only shared, and goes on opening as the surface it was. */
    void      retire_direct3d11(D3dRetired &retired);
    void      retire_direct3d12(D3dRetired &retired);

    /* One more copy into it ran on the host. */
    void      written();
    uint64_t  writes() const;
    uint64_t  opens() const;

private:
    /* `*held` retired and forgotten. */
    void      retire(IUnknown *&held, D3dRetired &retired);

    IUnknown   *resource_;
    IUnknown   *view_;
    IUnknown   *shown_;
    uint64_t    writes_;
    uint64_t    opens_;
    ShareOrigin origin_;
};

}  // namespace av

#endif  // AV_D3D_SHARE_STATE_HPP
