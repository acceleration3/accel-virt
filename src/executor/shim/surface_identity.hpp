/* The guest's shared id of a host Direct3D resource - the one the miniport
 * minted over its kernel allocation - kept on the resource itself as private
 * data, so that a present naming the resource by its session's id finds the
 * surface it is (docs/shared-surfaces.md). A session reaches only its own
 * objects, so it can name no other process's surface this way. */
#ifndef AV_SURFACE_IDENTITY_HPP
#define AV_SURFACE_IDENTITY_HPP

#include <stdint.h>

struct ID3D11DeviceChild;
struct ID3D12Object;

namespace av {

class SurfaceIdentity {
public:
    explicit SurfaceIdentity(uint64_t surface);
    /* What `resource` was marked with; 0 for a resource never marked. */
    explicit SurfaceIdentity(ID3D11DeviceChild *resource);
    explicit SurfaceIdentity(ID3D12Object *resource);

    uint64_t surface() const;
    /* `resource` marked as this surface; false where it will not keep it. */
    [[nodiscard]] bool mark(ID3D11DeviceChild *resource) const;
    [[nodiscard]] bool mark(ID3D12Object *resource) const;

private:
    uint64_t surface_;
};

}  // namespace av

#endif  // AV_SURFACE_IDENTITY_HPP
