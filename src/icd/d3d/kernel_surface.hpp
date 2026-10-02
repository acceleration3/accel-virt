/* A surface a Direct3D device presents or shares, as the kernel sees it: the
 * private data its one allocation is made with - its layout and scanout for
 * the miniport, and an identity the miniport mints into it - and the
 * resource's own data, which marks the surface this driver's.
 *
 * Both user-mode drivers make them: a swap chain buffer from Direct3D 11 or
 * Direct3D 12 is the same thing to DWM, which opens it with Direct3D 11 and
 * finds the host's texture by the identity (D3dSharedResources). So what one
 * driver writes here is what the other reads back with open(). */
#ifndef AV_KERNEL_SURFACE_HPP
#define AV_KERNEL_SURFACE_HPP

#include "core/support/status.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/scanout_target.hpp"
#include "proto/resources/shared_object_record.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av::d3d {

class KernelSurface {
public:
    /* The allocation's private data: its descriptor, then the identity. */
    static constexpr size_t allocation_bytes = AllocationDescriptor::encoded_size + SharedObjectRecord::encoded_size;
    /* The resource's private data: this driver's mark, then a reserved word. */
    static constexpr size_t resource_bytes = 2u * sizeof(uint64_t);

    /* One to open() into. */
    KernelSurface();
    /* A surface `width` by `height` texels of the DXGI format `format`. */
    KernelSurface(uint32_t width, uint32_t height, uint32_t format);
    /* The same as a primary, scanned out at `scanout`. */
    KernelSurface(uint32_t width, uint32_t height, uint32_t format, const ScanoutTarget &scanout);
    KernelSurface(const KernelSurface &) = delete;
    KernelSurface &operator=(const KernelSurface &) = delete;

    /* Both private data written, the identity asked for and not yet minted;
     * invalid_parameter for a surface with no texels. */
    [[nodiscard]] Status make();
    /* What an opened resource carried: `resource`, its own data, and
     * `allocation`, its allocation's. not_found for a resource that is not
     * this driver's shared surface; malformed for data of another size. */
    [[nodiscard]] Status open(const void *resource, size_t resource_size, const void *allocation,
                              size_t allocation_size);

    /* The allocation's private data, which the kernel writes the minted
     * identity back into. */
    uint8_t        *allocation_data();
    const uint64_t *resource_data() const;
    /* Bytes a row of the allocation takes, and the whole allocation's, in
     * pages. */
    uint32_t        pitch() const;
    uint64_t        bytes() const;
    /* The identity the miniport minted into the allocation's data; 0 until it
     * has, and for a surface it minted none for. */
    uint64_t        key() const;
    /* Whether it is a primary, and the VidPn source a primary is scanned out
     * at; 0 for any other. */
    bool            primary() const;
    uint32_t        source() const;

private:
    static constexpr uint64_t mark = 0x53564B52u;   /* "RKVS" */

    /* The D3DDDIFORMAT the miniport's descriptors name the surface by. */
    uint32_t kernel_format() const;

    uint8_t       allocation_[allocation_bytes];
    uint64_t      resource_[2];
    uint32_t      width_;
    /* Rows of elements: a block-compressed surface's rows of blocks, which
     * is what the kernel's layout counts - pitch times rows is its bytes. */
    uint32_t      rows_;
    uint32_t      format_;
    uint32_t      pitch_;
    uint64_t      bytes_;
    ScanoutTarget scanout_;
    bool          primary_;
};

}  // namespace av::d3d

#endif  // AV_KERNEL_SURFACE_HPP
