#include "device/shim/imported_textures.hpp"

#include "core/trace/function_scope.hpp"
#include "device/shim/d3d_headers.hpp"
#include "device/shim/dxvk_vulkan_device.hpp"
#include "device/shim/vkd3d_image_export.hpp"

#include <stdio.h>
#include <unistd.h>

namespace av {

namespace {

/* The private data a texture over an imported image carries: its slot. */
constexpr GUID slot_data = {0x5e1b7c42, 0x93d0, 0x4f6a, {0x8c, 0x2e, 0x71, 0x0b, 0xd4, 0x96, 0x3a, 0x58}};

}  // namespace

ImportedTextures::Slot::Slot() : references_(0u), held_(false), image_(VK_NULL_HANDLE), memory_(VK_NULL_HANDLE)
{
    AV_SCOPE("ImportedTextures::Slot::Slot");
}

int32_t ImportedTextures::Slot::query_interface(const void *iid, void **object)
{
    AV_SCOPE("ImportedTextures::Slot::query_interface");
    if (object == nullptr) return E_POINTER;
    *object = nullptr;
    if (iid == nullptr || !(*static_cast<const GUID *>(iid) == __uuidof(IUnknown))) return E_NOINTERFACE;
    *object = this;
    add_ref();
    return S_OK;
}

uint32_t ImportedTextures::Slot::add_ref()
{
    AV_SCOPE("ImportedTextures::Slot::add_ref");
    return ++references_;
}

uint32_t ImportedTextures::Slot::release()
{
    AV_SCOPE("ImportedTextures::Slot::release");
    if (references_ != 0u) references_--;
    return references_;
}

void ImportedTextures::Slot::hold(VkImage image, VkDeviceMemory memory)
{
    AV_SCOPE("ImportedTextures::Slot::hold");
    references_ = 0u;
    held_ = true;
    image_ = image;
    memory_ = memory;
}

bool ImportedTextures::Slot::vacant() const
{
    AV_SCOPE("ImportedTextures::Slot::vacant");
    return !held_;
}

bool ImportedTextures::Slot::retired() const
{
    AV_SCOPE("ImportedTextures::Slot::retired");
    return held_ && references_ == 0u;
}

void ImportedTextures::Slot::empty(const DxvkVulkanDevice &vulkan)
{
    AV_SCOPE("ImportedTextures::Slot::empty");
    vulkan.destroy_image(image_);
    vulkan.free_memory(memory_);
    references_ = 0u;
    held_ = false;
    image_ = VK_NULL_HANDLE;
    memory_ = VK_NULL_HANDLE;
}

ImportedTextures::ImportedTextures(DxvkVulkanDevice &vulkan) : vulkan_(vulkan), slots_{}
{
    AV_SCOPE("ImportedTextures::ImportedTextures");
}

ImportedTextures::~ImportedTextures()
{
    AV_SCOPE("ImportedTextures::~ImportedTextures");
    release();
}

/* The image is made as vkd3d-proton made its own, so the two lay the memory
 * out alike; memory that is not what this image asks for is a layout they do
 * not agree on, and is refused rather than shown wrong. Each refusal says why
 * in the host log, the only place it is seen: the guest learns only that its
 * buffer could not be shown. */
ID3D11Texture2D *ImportedTextures::import(Vkd3dImageExport &image, const D3D11_TEXTURE2D_DESC1 &desc)
{
    AV_SCOPE("ImportedTextures::import");
    const auto refuse = [](const char *why) {
        fprintf(stderr, "virtio-gpu-rvk: d3d11: an image cannot be imported: %s\n", why);
    };
    collect();
    Slot *const slot = vacant_slot();
    if (slot == nullptr || !image.whole() || !vulkan_.connected()) {
        refuse(slot == nullptr ? "every slot is held" : !image.whole() ? "the export is part of an allocation"
                                                                        : "DXVK's Vulkan device is not reached");
        return nullptr;
    }
    VkImage made = VK_NULL_HANDLE;
    if (!vulkan_.create_image(image.image(), made)) {
        refuse("vkCreateImage refused vkd3d-proton's description");
        return nullptr;
    }
    const VkMemoryRequirements needs = vulkan_.requirements(made);
    if (needs.size > image.size() || (needs.memoryTypeBits & (1u << image.memory_type())) == 0u) {
        fprintf(stderr,
                "virtio-gpu-rvk: d3d11: an image cannot be imported: it needs %llu bytes of types 0x%x, and "
                "vkd3d-proton gave %llu of type %u\n",
                static_cast<unsigned long long>(needs.size), needs.memoryTypeBits,
                static_cast<unsigned long long>(image.size()), image.memory_type());
        vulkan_.destroy_image(made);
        return nullptr;
    }
    const int fd = image.take_fd();
    const VkImportMemoryFdInfoKHR imported = {VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR, nullptr,
                                              VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT, fd};
    VkDeviceMemory memory = VK_NULL_HANDLE;
    if (!vulkan_.bind_memory(made, &imported, image.size(), image.memory_type(), memory)) {
        refuse(memory == VK_NULL_HANDLE ? "the memory's fd was not imported" : "the imported memory was not bound");
        /* An import that failed never took the fd. */
        if (memory == VK_NULL_HANDLE) close(fd);
        vulkan_.free_memory(memory);
        vulkan_.destroy_image(made);
        return nullptr;
    }
    ID3D11Texture2D *texture = nullptr;
    slot->hold(made, memory);
    if (!vulkan_.wrap(desc, made, texture) ||
        FAILED(texture->SetPrivateDataInterface(slot_data, reinterpret_cast<IUnknown *>(slot)))) {
        refuse("DXVK made no texture over the image");
        if (texture != nullptr) texture->Release();
        slot->empty(vulkan_);
        return nullptr;
    }
    return texture;
}

void ImportedTextures::collect()
{
    AV_SCOPE("ImportedTextures::collect");
    bool any = false;
    for (const Slot &slot : slots_) any = any || slot.retired();
    if (!any) return;
    vulkan_.idle();
    for (Slot &slot : slots_)
        if (slot.retired()) slot.empty(vulkan_);
}

void ImportedTextures::release()
{
    AV_SCOPE("ImportedTextures::release");
    bool any = false;
    for (const Slot &slot : slots_) any = any || !slot.vacant();
    if (!any) return;
    vulkan_.idle();
    for (Slot &slot : slots_)
        if (!slot.vacant()) slot.empty(vulkan_);
}

uint32_t ImportedTextures::held() const
{
    AV_SCOPE("ImportedTextures::held");
    uint32_t count = 0u;
    for (const Slot &slot : slots_) count += slot.vacant() ? 0u : 1u;
    return count;
}

ImportedTextures::Slot *ImportedTextures::vacant_slot()
{
    AV_SCOPE("ImportedTextures::vacant_slot");
    for (Slot &slot : slots_)
        if (slot.vacant()) return &slot;
    return nullptr;
}

}  // namespace av
