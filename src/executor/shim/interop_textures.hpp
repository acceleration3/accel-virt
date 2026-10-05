/* Direct3D 11 textures of DXVK's over Vulkan memory other APIs share, each an
 * image made on DXVK's own device (DxvkVulkanDevice) and wrapped:
 *
 * - imported: a guest's Direct3D 12 swapchain buffer, which the compositor
 *   opens by key, made again over the memory vkd3d-proton exported
 *   (Vkd3dImageExport). No copy is ever made: both devices' images are the
 *   same memory;
 * - made: a guest's shared texture, over memory exported as a dma-buf, which
 *   OpenGL and Vulkan open as theirs (feature 053) - the surface an OpenGL or
 *   Vulkan window is presented through.
 *
 * The image and its memory must outlive the texture, which DXVK destroys once
 * the last session and the last view holding it let go. So the texture holds
 * a slot of this table as private data, which DXVK releases as the texture
 * goes, and collect() frees the images of the slots it released once the GPU
 * is done with them. Under the Direct3D 11 device's lock throughout. */
#ifndef AV_INTEROP_TEXTURES_HPP
#define AV_INTEROP_TEXTURES_HPP

#include "core/log/driver_log.hpp"
#include "device/abi/av_executor.h"
#include "device/host/stdio_log.hpp"

#include <stdint.h>
#include <vulkan/vulkan.h>

struct ID3D11Texture2D;
struct D3D11_TEXTURE2D_DESC1;

namespace av {

class DxvkVulkanDevice;
class Vkd3dImageExport;

class InteropTextures {
public:
    static constexpr uint32_t capacity = 64u;

    explicit InteropTextures(DxvkVulkanDevice &vulkan);
    ~InteropTextures();
    InteropTextures(const InteropTextures &) = delete;
    InteropTextures &operator=(const InteropTextures &) = delete;

    /* `image` as a texture of DXVK's, described as `desc`, with a reference
     * the caller owns. Null when DXVK will not make the image, import its
     * memory or wrap it, or every slot is held. */
    ID3D11Texture2D *import(Vkd3dImageExport &image, const D3D11_TEXTURE2D_DESC1 &desc);
    /* A texture described as `desc` over memory another API can open, with a
     * reference the caller owns. Null for a description ExportableImageInfo
     * does not support, or one DXVK will not make or wrap: DXVK then makes the
     * texture itself. */
    ID3D11Texture2D *make(const D3D11_TEXTURE2D_DESC1 &desc);
    /* A dma-buf of `texture`'s memory, which the caller owns; false for a
     * texture this table did not make. */
    [[nodiscard]] bool export_memory(ID3D11Texture2D *texture, int &fd);
    /* The same dma-buf, described for a process outside QEMU to import with
     * no copy (docs/external-export.md): the image's size, the DRM format of
     * its pixels, and where its rows are as the driver laid them out. The
     * layout is the driver's own, kept with the memory - it was bound
     * dedicated - so the modifier is AV_EXPORT_MODIFIER_IMPLICIT. False for a
     * texture this table did not make, or one in a format no DRM format
     * names. */
    [[nodiscard]] bool export_image(ID3D11Texture2D *texture, AvExportedImage &image);
    /* The images whose textures have gone, freed once DXVK's GPU work is. */
    void             collect();
    /* Every image freed, its texture gone or not: the device is closing. */
    void             release();

private:
    /* One image and its memory, and what the texture over them holds: a COM
     * object as DXVK sees one. Its three virtual functions are IUnknown's, in
     * IUnknown's order and with its native types (DXVK's native build calls
     * them with the platform's own convention), which is all its vtable is -
     * declared without IUnknown, whose header this one must not pull in. */
    class Slot {
    public:
        Slot();
        virtual int32_t  query_interface(const void *iid, void **object);
        virtual uint32_t add_ref();
        virtual uint32_t release();

        void hold(VkImage image, VkDeviceMemory memory);
        bool vacant() const;
        /* Held, and let go of by the texture that held it. */
        bool retired() const;
        void empty(const DxvkVulkanDevice &vulkan);

    protected:
        /* Nothing is destroyed through the vtable DXVK holds, only as the
         * table's own slot. */
        ~Slot() = default;

    private:
        friend class InteropTextures;

        uint32_t       references_;
        bool           held_;
        VkImage        image_;
        VkDeviceMemory memory_;
    };

    Slot *vacant_slot();
    /* The slot holding `texture`'s image, when this table made it; null for
     * any other texture. */
    Slot *slot_of(ID3D11Texture2D *texture);
    /* A texture described as `desc` over `image` and `memory`, which `slot`
     * then holds; null, with both freed, when DXVK would not wrap it. */
    ID3D11Texture2D *wrap(Slot &slot, VkImage image, VkDeviceMemory memory, const D3D11_TEXTURE2D_DESC1 &desc);

    DxvkVulkanDevice &vulkan_;
    Slot              slots_[capacity];
    /* QEMU's stderr: a refused import is seen nowhere else. */
    StdioLog          sink_;
    DriverLog         lines_;
};

}  // namespace av

#endif  // AV_INTEROP_TEXTURES_HPP
