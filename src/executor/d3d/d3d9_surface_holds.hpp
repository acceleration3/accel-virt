#ifndef AV_D3D9_SURFACE_HOLDS_HPP
#define AV_D3D9_SURFACE_HOLDS_HPP

#include "core/support/growable_array.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

struct IUnknown;

namespace av {

class BlockSource;

class D3d9SurfaceHolds {
public:
    explicit D3d9SurfaceHolds(BlockSource &blocks);
    ~D3d9SurfaceHolds();
    D3d9SurfaceHolds(const D3d9SurfaceHolds &) = delete;
    D3d9SurfaceHolds &operator=(const D3d9SurfaceHolds &) = delete;

    [[nodiscard]] Status hold(IUnknown *surface, uint64_t value);
    void                 release_oldest();
    void                 release_all();

    uint32_t pending() const;
    uint64_t oldest() const;

    bool invariants_hold() const;

private:
    struct Held {
        IUnknown *surface_;
        uint64_t  value_;
    };

    GrowableArray<Held> held_;
    uint32_t            first_;
    uint64_t            newest_;
};

}

#endif
