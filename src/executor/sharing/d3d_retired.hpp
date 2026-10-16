/* COM references the shares of one GPU let go of - a freed share's, a view or
 * a flip copy replaced, what a lost Direct3D device made - waiting for the
 * Direct3D 11 device to release them under its lock (take). Nothing is
 * released where they are let go of: that is under HostShares' lock, which
 * may be taken without the device's, and DXVK destroys a texture's memory as
 * its last reference goes. Guarded by HostShares' lock. */
#ifndef AV_D3D_RETIRED_HPP
#define AV_D3D_RETIRED_HPP

#include "core/support/object_pool.hpp"

#include <stdint.h>

struct IUnknown;

namespace av {

class BlockSource;

class D3dRetired {
public:
    /* Kept in `blocks`. */
    explicit D3dRetired(BlockSource &blocks);
    /* Whatever is left released: nothing is left to hand it to. */
    ~D3dRetired();
    D3dRetired(const D3dRetired &) = delete;
    D3dRetired &operator=(const D3dRetired &) = delete;

    /* `object`, with its reference, kept for take - or released now when
     * nothing can hold it. Nothing for null. */
    void     retire(IUnknown *object);
    /* Up to `capacity` of them into `objects`, now the caller's to release:
     * how many. */
    uint32_t take(IUnknown **objects, uint32_t capacity);
    uint32_t live() const;

private:
    struct Retired {
        explicit Retired(IUnknown *object);
        IUnknown *object_;
    };

    ObjectPool<Retired> retired_;
};

}  // namespace av

#endif  // AV_D3D_RETIRED_HPP
