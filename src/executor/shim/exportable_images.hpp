/* Direct3D 11 textures of DXVK's over Vulkan memory other APIs share, each an
 * image made on DXVK's own device (DxvkVulkanDevice) and wrapped:
 *
 * - imported: a guest's Direct3D 12 swapchain buffer, which the compositor
 *   opens by key, made again over the memory vkd3d-proton exported
 *   (Vkd3dImageExport). No copy is ever made: both devices' images are the
 *   same memory;
 * - made: a guest's shared texture, over memory exported as a dma-buf. As
 *   rows when the guest asks for them, for an API that reads the memory as
 *   rows (feature 053): the surface an OpenGL or Vulkan window is presented
 *   through, an OpenCL image's twin. Otherwise in a tiling of the GPU's own,
 *   for Direct3D to draw and sample at full speed: what DWM composes from and
 *   into, and what a mirror outside QEMU imports, told the tiling
 *   (docs/external-export.md).
 *
 * The image and its memory must outlive the texture, which DXVK destroys once
 * the last session and the last view holding it let go. So the texture holds
 * its ExportableImage as private data, which DXVK lets go of as the texture
 * goes, and collect() frees the retired images once the GPU is done with
 * them. There is no count past which a texture is made that nothing can
 * export: every window a mirror shows keeps a ring of three, made again at
 * each resize, and DWM keeps every capture buffer it ever opened. Under the
 * Direct3D 11 device's lock throughout, but for retired(). */
#ifndef AV_EXPORTABLE_IMAGES_HPP
#define AV_EXPORTABLE_IMAGES_HPP

#include "core/log/driver_log.hpp"
#include "core/support/object_pool.hpp"
#include "device/abi/av_executor.h"
#include "device/host/stdio_log.hpp"
#include "executor/shim/exportable_image.hpp"
#include "executor/shim/host_heap.hpp"

#include <stdint.h>
#include <vulkan/vulkan.h>

struct ID3D11Texture2D;
struct D3D11_TEXTURE2D_DESC1;

namespace av {

class OwnedFd;
class DxvkVulkanDevice;
class ExportableImageInfo;
class StateWriter;
class Vkd3dImageExport;

class ExportableImages {
public:
    explicit ExportableImages(DxvkVulkanDevice &vulkan);
    ~ExportableImages();
    ExportableImages(const ExportableImages &) = delete;
    ExportableImages &operator=(const ExportableImages &) = delete;

    /* `image` as a texture of DXVK's, described as `desc`, with a reference
     * the caller owns. Null when DXVK will not make the image, import its
     * memory or wrap it. */
    ID3D11Texture2D *import(Vkd3dImageExport &image, const D3D11_TEXTURE2D_DESC1 &desc);
    /* A texture described as `desc` over memory another API can open as
     * rows, with a reference the caller owns. Null for a description
     * ExportableImageInfo does not support, or one DXVK will not make or
     * wrap: DXVK then makes the texture itself. */
    ID3D11Texture2D *make_rows(const D3D11_TEXTURE2D_DESC1 &desc);
    /* The same, laid out by one of the GPU's tilings, which only an importer
     * told the modifier (export_image) reads; as rows where the GPU exports
     * no tiling. */
    ID3D11Texture2D *make_tiled(const D3D11_TEXTURE2D_DESC1 &desc);
    /* A texture described as `desc` over the dma-buf `descriptor` - an image
     * the compositor made as rows as ExportableImageInfo describes them
     * (CompositorDevice) - with a reference the caller owns. The descriptor
     * is the texture's memory's once it is made, and stays held otherwise. */
    ID3D11Texture2D *adopt(OwnedFd &descriptor, const D3D11_TEXTURE2D_DESC1 &desc);
    /* The same over memory laid out by `modifier`, its one plane at `offset`
     * with rows `pitch` apart: what carry() said of a texture made here. */
    ID3D11Texture2D *adopt(OwnedFd &descriptor, const D3D11_TEXTURE2D_DESC1 &desc, uint64_t modifier,
                           uint64_t offset, uint64_t pitch);
    /* A dma-buf of the memory of `texture`, made here, and how it is laid
     * out, for the same GPU to adopt again (CarriedShare); false for any
     * other texture. */
    [[nodiscard]] bool carry(ID3D11Texture2D *texture, OwnedFd &descriptor, uint64_t &modifier, uint64_t &offset,
                             uint64_t &pitch);
    /* A dma-buf of `texture`'s memory, for an API that reads it as rows;
     * false for a texture not made here as rows. */
    [[nodiscard]] bool export_memory(ID3D11Texture2D *texture, OwnedFd &descriptor);
    /* The bytes between `texture`'s rows, as its linear image is laid out
     * (ExportableImageInfo); false for a texture not made here as rows. */
    [[nodiscard]] bool row_pitch(ID3D11Texture2D *texture, uint32_t &pitch);
    /* The same dma-buf, described for a process outside QEMU to import with
     * no copy (docs/external-export.md): the image's size, the DRM format of
     * its pixels, the modifier it is laid out by, and where its one plane
     * starts and its pitch. False for a texture not made here, one no
     * modifier lays out, or one in a format no DRM format names. */
    [[nodiscard]] bool export_image(ID3D11Texture2D *texture, AvExportedImage &image);
    /* The images whose textures have gone, freed once DXVK's GPU work is. */
    void             collect();
    /* Every image freed, its texture gone or not: the device is closing. */
    void             release();
    /* An image's texture let go of it, on whatever thread destroyed the
     * texture: the next collect has work. */
    void             retired();
    /* Images held, and how many of them are waiting to be collected. */
    void             report(StateWriter &writer) const;

private:
    /* The image `texture` was made over, when it was made here; null for any
     * other texture. */
    ExportableImage *image_of(ID3D11Texture2D *texture);
    /* A record for an image about to be made; null, said in the log, when
     * none can be had. */
    ExportableImage *record();
    /* A record whose image was not made, given back. */
    void             discard(ExportableImage &made);
    /* A texture described as `desc` over the dma-buf `descriptor`, as
     * `described` lays it out by `modifier`. */
    ID3D11Texture2D *adopt_laid_out(OwnedFd &descriptor, const ExportableImageInfo &described, uint64_t modifier,
                                    const D3D11_TEXTURE2D_DESC1 &desc);
    /* `made`'s image, made and bound, as a texture described as `desc` and
     * laid out by `modifier`; null, with the record given back, when DXVK
     * would not wrap it. */
    ID3D11Texture2D *wrap(ExportableImage &made, uint64_t modifier, const D3D11_TEXTURE2D_DESC1 &desc);

    DxvkVulkanDevice           &vulkan_;
    HostHeap                    heap_;
    ObjectPool<ExportableImage> images_;
    /* Images let go of since the last collect: written by any thread. */
    uint32_t                    retired_;
    /* QEMU's stderr: a refused import is seen nowhere else. */
    StdioLog                    sink_;
    DriverLog                   lines_;
};

}  // namespace av

#endif  // AV_EXPORTABLE_IMAGES_HPP
