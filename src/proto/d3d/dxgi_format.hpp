/* One DXGI_FORMAT value, described by its row in the one table every side
 * reads DXGI formats from: the guest's Direct3D and OpenCL drivers, the
 * executor's Direct3D sessions, its OpenCL backend and the images it exports.
 * Each fact about a format is written once, in that row: its element, its
 * typeless family, its sRGB or linear sibling, the D3DDDIFORMAT a surface of
 * it is scanned out as, the Vulkan format interop makes it as, the OpenCL
 * image format the extension names it by and the DRM format its bytes are.
 *
 * Every value up to A4B4G4R4_UNORM is known(), named or not; anything past it
 * is not, and answers 0 and false throughout. Vulkan, OpenCL, DRM and
 * D3DDDIFORMAT values are their headers' numbers, raw, since proto names no
 * API's header. */
#ifndef AV_DXGI_FORMAT_HPP
#define AV_DXGI_FORMAT_HPP

#include <stdint.h>

namespace av {

/* One row of the table, defined beside it. */
struct DxgiFormatRow;

class DxgiFormat {
public:
    /* UNKNOWN to A4B4G4R4_UNORM. */
    static constexpr uint32_t count = 192u;

    explicit DxgiFormat(uint32_t format);

    bool     known() const;
    uint32_t value() const;

    /* Bytes per element - a block-compressed format's block, a packed 4:2:2
     * format's pair - and the element's width and height in texels; 0 bytes
     * for a format with no single-plane layout of whole bytes. */
    uint32_t element_bytes() const;
    uint32_t element_width() const;
    uint32_t element_height() const;

    /* The typeless head DXGI numbers the format's family under (the two BGRA
     * families are named member by member); the format itself when it is in
     * no family. */
    uint32_t family() const;
    bool     typeless() const;
    /* A typeless family's head, whose members a view may be made in. */
    bool     family_head() const;
    bool     depth() const;
    bool     srgb() const;
    /* The sRGB format of a linear one and the linear one of an sRGB format;
     * 0 for a format with no such sibling. */
    uint32_t sibling() const;
    /* The typed format a multisampled texture of this one is resolved in:
     * itself when typed, the UNORM member of a four-byte colour family for its
     * typeless head, 0 for every other typeless format. */
    uint32_t resolved() const;

    /* The D3DDDIFORMAT a surface of it is scanned out as, 0 for one the
     * kernel presents in no format of its own. */
    uint32_t ddi_format() const;
    /* The VkFormat a texture of it shared with another API is made as, 0 for
     * one interop does not share. */
    uint32_t vk_format() const;
    /* Its sibling's VkFormat: the other view a shared texture is made mutable
     * to; 0 for none. */
    uint32_t vk_sibling() const;
    /* The OpenCL image format cl_khr_d3d11_sharing names it by: channel order
     * and type, both 0 for a format the extension's table leaves out. */
    uint32_t cl_order() const;
    uint32_t cl_type() const;
    /* Whether the executor makes an OpenCL image over a shared texture of it:
     * memory interop shares, read in the order OpenCL names - never an sRGB
     * format, which OpenCL would decode where Direct3D did not encode. A
     * format the guest names as an image but the host does not view as one
     * is opened as bytes, so the guest copies it instead. */
    bool     cl_image_on_host() const;
    /* The DRM format whose bytes in memory are this format's: what a host
     * image handed out as a dma-buf is named by. An sRGB format and its linear
     * sibling are one DRM format, the encoding the reader's to choose. 0 for
     * none. */
    uint32_t drm_fourcc() const;
    /* Whether a scanout's image is copied out to the host's display in this
     * format as it is. */
    bool     scanned_out() const;

private:
    uint32_t             format_;
    const DxgiFormatRow &row_;
};

}  // namespace av

#endif  // AV_DXGI_FORMAT_HPP
