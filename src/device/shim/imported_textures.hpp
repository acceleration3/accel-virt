/* Direct3D 12 textures the host's Direct3D 11 device shows - a guest's
 * Direct3D 12 swapchain buffers, which the compositor opens by key: each made
 * again on DXVK's device over the memory vkd3d-proton exported
 * (Vkd3dImageExport) and wrapped as a Direct3D 11 texture. No copy is ever
 * made: both devices' images are the same memory.
 *
 * The image and its memory must outlive the texture, which DXVK destroys once
 * the last session and the last view holding it let go. So the texture holds
 * a slot of this table as private data, which DXVK releases as the texture
 * goes, and collect() frees the images of the slots it released once the GPU
 * is done with them. Under the Direct3D 11 device's lock throughout. */
#ifndef AV_IMPORTED_TEXTURES_HPP
#define AV_IMPORTED_TEXTURES_HPP

#include <stdint.h>
#include <vulkan/vulkan.h>

struct ID3D11Texture2D;
struct D3D11_TEXTURE2D_DESC1;

namespace av {

class DxvkVulkanDevice;
class Vkd3dImageExport;

class ImportedTextures {
public:
    static constexpr uint32_t capacity = 64u;

    explicit ImportedTextures(DxvkVulkanDevice &vulkan);
    ~ImportedTextures();
    ImportedTextures(const ImportedTextures &) = delete;
    ImportedTextures &operator=(const ImportedTextures &) = delete;

    /* `image` as a texture of DXVK's, described as `desc`, with a reference
     * the caller owns. Null when DXVK will not make the image, import its
     * memory or wrap it, or every slot is held. */
    ID3D11Texture2D *import(Vkd3dImageExport &image, const D3D11_TEXTURE2D_DESC1 &desc);
    /* The images whose textures have gone, freed once DXVK's GPU work is. */
    void             collect();
    /* Every image freed, its texture gone or not: the device is closing. */
    void             release();
    uint32_t         held() const;

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
        friend class ImportedTextures;

        uint32_t       references_;
        bool           held_;
        VkImage        image_;
        VkDeviceMemory memory_;
    };

    Slot *vacant_slot();

    DxvkVulkanDevice &vulkan_;
    Slot              slots_[capacity];
};

}  // namespace av

#endif  // AV_IMPORTED_TEXTURES_HPP
