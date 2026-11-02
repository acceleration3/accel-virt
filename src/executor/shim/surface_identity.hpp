#ifndef AV_SURFACE_IDENTITY_HPP
#define AV_SURFACE_IDENTITY_HPP

#include <stdint.h>

struct ID3D11DeviceChild;
struct ID3D12Object;

namespace av {

class SurfaceIdentity {
public:
    explicit SurfaceIdentity(uint64_t surface);
    explicit SurfaceIdentity(ID3D11DeviceChild *resource);
    explicit SurfaceIdentity(ID3D12Object *resource);

    uint64_t surface() const;
    [[nodiscard]] bool mark(ID3D11DeviceChild *resource) const;
    [[nodiscard]] bool mark(ID3D12Object *resource) const;

private:
    uint64_t surface_;
};

}

#endif
