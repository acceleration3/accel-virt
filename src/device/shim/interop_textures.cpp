#include "device/shim/interop_textures.hpp"

#include "core/trace/function_scope.hpp"
#include "device/shim/d3d_headers.hpp"
#include "device/shim/dxvk_vulkan_device.hpp"
#include "proto/vulkan/exportable_image_info.hpp"
#include "device/shim/vkd3d_image_export.hpp"

#include <stdio.h>
#include <unistd.h>

namespace av {

namespace {

/* The private data a texture over an imported image carries: its slot. */
constexpr GUID slot_data = {0x5e1b7c42, 0x93d0, 0x4f6a, {0x8c, 0x2e, 0x71, 0x0b, 0xd4, 0x96, 0x3a, 0x58}};

}  // namespace

InteropTextures::Slot::Slot() : references_(0u), held_(false), image_(VK_NULL_HANDLE), memory_(VK_NULL_HANDLE)
{
    AV_SCOPE("InteropTextures::Slot::Slot");
}

int32_t InteropTextures::Slot::query_interface(const void *iid, void **object)
{
    AV_SCOPE("InteropTextures::Slot::query_interface");
    if (object == nullptr) return E_POINTER;
    *object = nullptr;
    if (iid == nullptr || !(*static_cast<const GUID *>(iid) == __uuidof(IUnknown))) return E_NOINTERFACE;
    *object = this;
    add_ref();
    return S_OK;
}

uint32_t InteropTextures::Slot::add_ref()
{
    AV_SCOPE("InteropTextures::Slot::add_ref");
    return ++references_;
}

uint32_t InteropTextures::Slot::release()
{
    AV_SCOPE("InteropTextures::Slot::release");
    if (references_ != 0u) references_--;
    return references_;
}

void InteropTextures::Slot::hold(VkImage image, VkDeviceMemory memory)
{
    AV_SCOPE("InteropTextures::Slot::hold");
    references_ = 0u;
    held_ = true;
    image_ = image;
    memory_ = memory;
}

bool InteropTextures::Slot::vacant() const
{
    AV_SCOPE("InteropTextures::Slot::vacant");
    return !held_;
}

bool InteropTextures::Slot::retired() const
{
    AV_SCOPE("InteropTextures::Slot::retired");
    return held_ && references_ == 0u;
}

void InteropTextures::Slot::empty(const DxvkVulkanDevice &vulkan)
{
    AV_SCOPE("InteropTextures::Slot::empty");
    vulkan.destroy_image(image_);
    vulkan.free_memory(memory_);
    references_ = 0u;
    held_ = false;
    image_ = VK_NULL_HANDLE;
    memory_ = VK_NULL_HANDLE;
}

InteropTextures::InteropTextures(DxvkVulkanDevice &vulkan) : vulkan_(vulkan), slots_{}
{
    AV_SCOPE("InteropTextures::InteropTextures");
}

InteropTextures::~InteropTextures()
{
    AV_SCOPE("InteropTextures::~InteropTextures");
    release();
}

/* The image is made as vkd3d-proton made its own, so the two lay the memory
 * out alike; memory that is not what this image asks for is a layout they do
 * not agree on, and is refused rather than shown wrong. Each refusal says why
 * in the host log, the only place it is seen: the guest learns only that its
 * buffer could not be shown. */
ID3D11Texture2D *InteropTextures::import(Vkd3dImageExport &image, const D3D11_TEXTURE2D_DESC1 &desc)
{
    AV_SCOPE("InteropTextures::import");
    const auto refuse = [](const char *why) {
        fprintf(stderr, "virtio-gpu-av: d3d11: an image cannot be imported: %s\n", why);
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
                "virtio-gpu-av: d3d11: an image cannot be imported: it needs %llu bytes of types 0x%x, and "
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
    return wrap(*slot, made, memory, desc);
}

/* Made as ExportableImageInfo describes, over memory of its own exported as a
 * dma-buf (bind_memory makes it dedicated): what OpenGL's import reads the
 * image's layout from. */
ID3D11Texture2D *InteropTextures::make(const D3D11_TEXTURE2D_DESC1 &desc)
{
    AV_SCOPE("InteropTextures::make");
    const ExportableImageInfo described(desc.Width, desc.Height, desc.MipLevels, desc.ArraySize,
                                        desc.SampleDesc.Count, static_cast<uint32_t>(desc.Format), desc.BindFlags);
    if (!described.supported() || !vulkan_.connected()) return nullptr;
    collect();
    Slot *const slot = vacant_slot();
    if (slot == nullptr) return nullptr;
    VkImage made = VK_NULL_HANDLE;
    if (!vulkan_.create_image(described.info(), made)) return nullptr;
    const VkMemoryRequirements needs = vulkan_.requirements(made);
    const uint32_t             type = vulkan_.memory_type(needs.memoryTypeBits);
    const VkExportMemoryAllocateInfo exported = {VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO, nullptr,
                                                 VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT};
    VkDeviceMemory memory = VK_NULL_HANDLE;
    if (type == VK_MAX_MEMORY_TYPES || !vulkan_.bind_memory(made, &exported, needs.size, type, memory)) {
        vulkan_.free_memory(memory);
        vulkan_.destroy_image(made);
        return nullptr;
    }
    return wrap(*slot, made, memory, desc);
}

bool InteropTextures::export_memory(ID3D11Texture2D *texture, int &fd)
{
    AV_SCOPE("InteropTextures::export_memory");
    fd = -1;
    if (texture == nullptr) return false;
    IUnknown *held = nullptr;
    UINT      size = sizeof held;
    if (FAILED(texture->GetPrivateData(slot_data, &size, &held)) || held == nullptr) return false;
    /* The reference the read took is the slot's own count, given straight back. */
    held->Release();
    for (Slot &slot : slots_)
        if (reinterpret_cast<IUnknown *>(&slot) == held && !slot.vacant())
            return vulkan_.memory_fd(slot.memory_, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, fd);
    return false;
}

ID3D11Texture2D *InteropTextures::wrap(Slot &slot, VkImage image, VkDeviceMemory memory,
                                       const D3D11_TEXTURE2D_DESC1 &desc)
{
    AV_SCOPE("InteropTextures::wrap");
    ID3D11Texture2D *texture = nullptr;
    slot.hold(image, memory);
    if (!vulkan_.wrap(desc, image, texture) ||
        FAILED(texture->SetPrivateDataInterface(slot_data, reinterpret_cast<IUnknown *>(&slot)))) {
        fprintf(stderr, "virtio-gpu-av: d3d11: DXVK made no texture over an image of its own device\n");
        if (texture != nullptr) texture->Release();
        slot.empty(vulkan_);
        return nullptr;
    }
    return texture;
}

void InteropTextures::collect()
{
    AV_SCOPE("InteropTextures::collect");
    bool any = false;
    for (const Slot &slot : slots_) any = any || slot.retired();
    if (!any) return;
    vulkan_.idle();
    for (Slot &slot : slots_)
        if (slot.retired()) slot.empty(vulkan_);
}

void InteropTextures::release()
{
    AV_SCOPE("InteropTextures::release");
    bool any = false;
    for (const Slot &slot : slots_) any = any || !slot.vacant();
    if (!any) return;
    vulkan_.idle();
    for (Slot &slot : slots_)
        if (!slot.vacant()) slot.empty(vulkan_);
}

uint32_t InteropTextures::held() const
{
    AV_SCOPE("InteropTextures::held");
    uint32_t count = 0u;
    for (const Slot &slot : slots_) count += slot.vacant() ? 0u : 1u;
    return count;
}

InteropTextures::Slot *InteropTextures::vacant_slot()
{
    AV_SCOPE("InteropTextures::vacant_slot");
    for (Slot &slot : slots_)
        if (slot.vacant()) return &slot;
    return nullptr;
}

}  // namespace av
