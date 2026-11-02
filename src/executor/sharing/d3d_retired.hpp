#ifndef AV_D3D_RETIRED_HPP
#define AV_D3D_RETIRED_HPP

#include "core/support/object_pool.hpp"

#include <stdint.h>

struct IUnknown;

namespace av {

class BlockSource;

class D3dRetired {
public:
    explicit D3dRetired(BlockSource &blocks);
    ~D3dRetired();
    D3dRetired(const D3dRetired &) = delete;
    D3dRetired &operator=(const D3dRetired &) = delete;

    void     retire(IUnknown *object);
    uint32_t take(IUnknown **objects, uint32_t capacity);
    uint32_t live() const;

private:
    struct Retired {
        explicit Retired(IUnknown *object);
        IUnknown *object_;
    };

    ObjectPool<Retired> retired_;
};

}

#endif
