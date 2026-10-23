/* The copy in system memory of any texture of DXVK's that is shared, for a
 * process on another GPU
 * (docs/external-export.md, "Another GPU"): a linear, exportable image of the
 * texture's size and format on DXVK's own Vulkan device (DxvkVulkanDevice),
 * bound to memory that is host visible and not device local - GTT on a
 * discrete GPU - which the other GPU opens as a dma-buf and reads over PCIe
 * with no pass through the guest's network. The texture is copied into it on
 * the GPU, once per frame the consumer syncs. The texture holds it as private
 * data (TextureHold, as it holds an ExportableImage), so it goes with the
 * texture whatever the texture was made over. Under the Direct3D 11 device's
 * lock throughout. */
#ifndef AV_SYSTEM_MEMORY_TWIN_HPP
#define AV_SYSTEM_MEMORY_TWIN_HPP

#include "device/abi/av_executor.h"
#include "executor/shim/texture_hold.hpp"
#include "executor/shim/vulkan_image.hpp"

#include <stdint.h>

struct ID3D11DeviceContext4;
struct ID3D11Texture2D;

namespace av {

class DxvkVulkanDevice;

class SystemMemoryTwin final : public TextureHold {
public:
    /* Recorded before it is made, as an ExportableImage is. */
    explicit SystemMemoryTwin(ExportableImages &owner);
    ~SystemMemoryTwin();
    SystemMemoryTwin(const SystemMemoryTwin &) = delete;
    SystemMemoryTwin &operator=(const SystemMemoryTwin &) = delete;

    /* A twin of `source`, whose size, format and bind flags it takes. False,
     * with nothing made, where the GPU has no host-visible system memory, or
     * exports no linear image of them, or DXVK will not wrap one. */
    [[nodiscard]] bool make(DxvkVulkanDevice &vulkan, ID3D11Texture2D *source);
    bool               made() const;
    /* `source`, as it is once `writes` copies have run into it, copied into
     * the twin and the copy queued - unless the last copy was made at the
     * same count: a consumer asking again for a count that has not moved
     * would copy the whole frame again for nothing. */
    void               copy_from(ID3D11DeviceContext4 *context, ID3D11Texture2D *source, uint64_t writes);
    /* The twin as the consumer imports it, with a dma-buf of its own in
     * `described`, the caller's. False for a format no DRM format names. */
    [[nodiscard]] bool describe(AvExportedImage &described) const;
    /* The texture and the image let go of; the GPU's work with them must be
     * done. */
    void               free();

private:
    VulkanImage      image_;
    ID3D11Texture2D *texture_;
    uint32_t         width_;
    uint32_t         height_;
    uint32_t         format_;
    uint64_t         copied_writes_;
    bool             copied_;
};

}  // namespace av

#endif  // AV_SYSTEM_MEMORY_TWIN_HPP
