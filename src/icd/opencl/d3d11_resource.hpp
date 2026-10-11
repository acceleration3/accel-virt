/* What Direct3D 11 says of a resource an application hands
 * cl_khr_d3d11_sharing (docs/d3d11-sharing.md): which kind it is, its format,
 * its extent and how many subresources it has. D3d11Bridge fills one in from
 * the resource's own description, so portable code can judge a resource it
 * cannot include the header of.
 *
 * Subresources are numbered as D3D11CalcSubresource numbers them: a texture's
 * mip level plus its array slice times its mip count. A 3D texture has no
 * slices, and a buffer has the one subresource. */
#ifndef AV_CLICD_D3D11_RESOURCE_HPP
#define AV_CLICD_D3D11_RESOURCE_HPP

#include <stdint.h>

namespace av::clicd {

class D3d11Resource {
public:
    /* D3D11_RESOURCE_DIMENSION's three this extension takes; none for
     * anything else, a 1D texture among them. */
    enum class Kind : uint32_t { none, buffer, texture2d, texture3d };

    D3d11Resource();
    /* `bytes` is a buffer's size; the rest is a texture's description, its
     * format a DXGI_FORMAT. */
    D3d11Resource(Kind kind, uint32_t format, uint64_t bytes, uint32_t width, uint32_t height, uint32_t depth,
                  uint32_t mips, uint32_t layers, uint32_t samples);

    Kind     kind() const;
    uint32_t format() const;
    uint64_t bytes() const;
    /* More than one sample a texel: no OpenCL image is one. */
    bool     multisampled() const;
    /* Whether `subresource` is one of this resource's. */
    bool     holds(uint32_t subresource) const;
    /* The extent of `subresource`'s mip level, never less than one. */
    uint32_t width_of(uint32_t subresource) const;
    uint32_t height_of(uint32_t subresource) const;
    uint32_t depth_of(uint32_t subresource) const;

private:
    /* The mip level `subresource` is of. */
    uint32_t mip_of(uint32_t subresource) const;
    /* `extent` at `mip`, halved per level and never below one. */
    uint32_t at_mip(uint32_t extent, uint32_t mip) const;

    Kind     kind_;
    uint32_t format_;
    uint64_t bytes_;
    uint32_t width_;
    uint32_t height_;
    uint32_t depth_;
    uint32_t mips_;
    uint32_t layers_;
    uint32_t samples_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_D3D11_RESOURCE_HPP
