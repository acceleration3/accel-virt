/* The misc flags of a Direct3D 11 resource as CreateResource carries them:
 * the D3D11_RESOURCE_MISC_* bits the host keeps, D3D11's own numbers, and one
 * bit of the protocol's beside them.
 *
 * A texture the guest shares is made over memory another API can open
 * (ExportableImages). Most are only ever Direct3D's - what DWM composes from
 * and into - and are laid out in a tiling of the GPU's own, which it draws
 * and samples several times faster than rows. One that an API of ours reads
 * as rows - a Vulkan or OpenGL window's chain slot, an OpenCL image's twin -
 * is asked for as rows: the code making it says so (SharedRowsRequest), since
 * nothing in a texture's description tells the two apart. */
#ifndef AV_RESOURCE_MISC_HPP
#define AV_RESOURCE_MISC_HPP

#include <stdint.h>

namespace av {

class ResourceMisc {
public:
    /* D3D11_RESOURCE_MISC_SHARED. */
    static constexpr uint32_t shared_bit = 0x2u;
    /* Laid out as rows: above every bit D3D11 defines. */
    static constexpr uint32_t rows_bit = 0x80000000u;

    explicit ResourceMisc(uint32_t flags);

    /* Shared, so made over memory another API can open. */
    bool     shared() const;
    /* Shared and asked for as rows. */
    bool     rows() const;

private:
    uint32_t flags_;
};

}  // namespace av

#endif  // AV_RESOURCE_MISC_HPP
