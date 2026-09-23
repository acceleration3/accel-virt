#include "device/shim/host_image_export.hpp"

#include "core/trace/function_scope.hpp"
#include "device/shim/d3d_headers.hpp"
#include "device/shim/dxvk_interop.hpp"

#include <unistd.h>

namespace av {

namespace {

constexpr uint32_t fourcc(char a, char b, char c, char d)
{
    return static_cast<uint32_t>(a) | static_cast<uint32_t>(b) << 8u | static_cast<uint32_t>(c) << 16u |
           static_cast<uint32_t>(d) << 24u;
}

/* The DRM format whose bytes are the DXGI format's, or 0. */
uint32_t drm_format(uint32_t format)
{
    switch (format) {
    case DXGI_FORMAT_B8G8R8A8_UNORM: return fourcc('A', 'R', '2', '4');
    case DXGI_FORMAT_B8G8R8X8_UNORM: return fourcc('X', 'R', '2', '4');
    case DXGI_FORMAT_R8G8B8A8_UNORM: return fourcc('A', 'B', '2', '4');
    default: return 0u;
    }
}

VkFormat vk_format(uint32_t format)
{
    switch (format) {
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8X8_UNORM: return VK_FORMAT_B8G8R8A8_UNORM;
    default: return VK_FORMAT_R8G8B8A8_UNORM;
    }
}

}  // namespace

HostImageExport::HostImageExport()
    : interop_(nullptr), physical_(VK_NULL_HANDLE), device_(VK_NULL_HANDLE), create_image_(nullptr),
      destroy_image_(nullptr), requirements_(nullptr), allocate_(nullptr), free_(nullptr), bind_(nullptr),
      layout_(nullptr), memory_fd_(nullptr), images_{}, width_(0u), height_(0u), format_(0u), next_(0u),
      unavailable_(false)
{
    AV_SCOPE("HostImageExport::HostImageExport");
    for (Image &image : images_) image.fd = -1;
}

HostImageExport::~HostImageExport()
{
    AV_SCOPE("HostImageExport::~HostImageExport");
    release();
}

bool HostImageExport::connect(ID3D11Device5 *device)
{
    AV_SCOPE("HostImageExport::connect");
    if (interop_ != nullptr) return true;
    IDXGIVkInteropDevice1 *interop = nullptr;
    if (FAILED(device->QueryInterface(__uuidof(IDXGIVkInteropDevice1), reinterpret_cast<void **>(&interop)))) return false;
    VkInstance instance = VK_NULL_HANDLE;
    interop->GetVulkanHandles(&instance, &physical_, &device_);
    const auto load = [this](const char *name) { return vkGetDeviceProcAddr(device_, name); };
    create_image_ = reinterpret_cast<PFN_vkCreateImage>(load("vkCreateImage"));
    destroy_image_ = reinterpret_cast<PFN_vkDestroyImage>(load("vkDestroyImage"));
    requirements_ = reinterpret_cast<PFN_vkGetImageMemoryRequirements>(load("vkGetImageMemoryRequirements"));
    allocate_ = reinterpret_cast<PFN_vkAllocateMemory>(load("vkAllocateMemory"));
    free_ = reinterpret_cast<PFN_vkFreeMemory>(load("vkFreeMemory"));
    bind_ = reinterpret_cast<PFN_vkBindImageMemory>(load("vkBindImageMemory"));
    layout_ = reinterpret_cast<PFN_vkGetImageSubresourceLayout>(load("vkGetImageSubresourceLayout"));
    memory_fd_ = reinterpret_cast<PFN_vkGetMemoryFdKHR>(load("vkGetMemoryFdKHR"));
    interop_ = interop;
    return memory_fd_ != nullptr;
}

/* Linear, so the display imports it with no modifier to agree on; device
 * local where the GPU allows it, so the copy and the display stay in VRAM. */
bool HostImageExport::make_image(ID3D11Device5 *device, Image &image)
{
    AV_SCOPE("HostImageExport::make_image");
    VkExternalMemoryImageCreateInfo external = {VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO, nullptr,
                                                VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT};
    VkImageCreateInfo created = {};
    created.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    created.pNext = &external;
    created.imageType = VK_IMAGE_TYPE_2D;
    created.format = vk_format(format_);
    created.extent = {width_, height_, 1u};
    created.mipLevels = 1u;
    created.arrayLayers = 1u;
    created.samples = VK_SAMPLE_COUNT_1_BIT;
    created.tiling = VK_IMAGE_TILING_LINEAR;
    created.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    created.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (create_image_(device_, &created, nullptr, &image.image) != VK_SUCCESS) return false;
    VkMemoryRequirements needs = {};
    requirements_(device_, image.image, &needs);
    VkPhysicalDeviceMemoryProperties memory = {};
    vkGetPhysicalDeviceMemoryProperties(physical_, &memory);
    uint32_t type = memory.memoryTypeCount;
    for (uint32_t index = 0u; index < memory.memoryTypeCount && type == memory.memoryTypeCount; index++)
        if ((needs.memoryTypeBits & (1u << index)) != 0u &&
            (memory.memoryTypes[index].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0u)
            type = index;
    for (uint32_t index = 0u; index < memory.memoryTypeCount && type == memory.memoryTypeCount; index++)
        if ((needs.memoryTypeBits & (1u << index)) != 0u) type = index;
    if (type == memory.memoryTypeCount) return false;
    VkMemoryDedicatedAllocateInfo dedicated = {VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO, nullptr, image.image,
                                               VK_NULL_HANDLE};
    VkExportMemoryAllocateInfo exported = {VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO, &dedicated,
                                           VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT};
    VkMemoryAllocateInfo allocation = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, &exported, needs.size, type};
    if (allocate_(device_, &allocation, nullptr, &image.memory) != VK_SUCCESS) return false;
    if (bind_(device_, image.image, image.memory, 0u) != VK_SUCCESS) return false;
    VkMemoryGetFdInfoKHR asked = {VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR, nullptr, image.memory,
                                  VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT};
    if (memory_fd_(device_, &asked, &image.fd) != VK_SUCCESS) return false;
    const VkImageSubresource plane = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u};
    VkSubresourceLayout laid = {};
    layout_(device_, image.image, &plane, &laid);
    image.stride = static_cast<uint32_t>(laid.rowPitch);
    image.offset = static_cast<uint32_t>(laid.offset);
    D3D11_TEXTURE2D_DESC1 described = {};
    described.Width = width_;
    described.Height = height_;
    described.MipLevels = 1u;
    described.ArraySize = 1u;
    described.Format = static_cast<DXGI_FORMAT>(format_);
    described.SampleDesc = {1u, 0u};
    described.Usage = D3D11_USAGE_DEFAULT;
    auto *const interop = static_cast<IDXGIVkInteropDevice1 *>(interop_);
    if (FAILED(interop->CreateTexture2DFromVkImage(&described, image.image, &image.texture))) return false;
    const D3D11_QUERY_DESC query = {D3D11_QUERY_EVENT, 0u};
    return SUCCEEDED(device->CreateQuery(&query, &image.done));
}

bool HostImageExport::make(ID3D11Device5 *device, uint32_t width, uint32_t height, uint32_t format)
{
    AV_SCOPE("HostImageExport::make");
    if (width == width_ && height == height_ && format == format_ && images_[0].texture != nullptr) return true;
    for (Image &image : images_) drop(image);
    width_ = width;
    height_ = height;
    format_ = format;
    for (Image &image : images_) {
        if (make_image(device, image)) continue;
        for (Image &made : images_) drop(made);
        return false;
    }
    return true;
}

bool HostImageExport::stage(ID3D11Device5 *device, ID3D11DeviceContext4 *context, ID3D11Texture2D *source,
                            uint32_t &slot)
{
    AV_SCOPE("HostImageExport::stage");
    if (unavailable_) return false;
    D3D11_TEXTURE2D_DESC described = {};
    source->GetDesc(&described);
    if (drm_format(static_cast<uint32_t>(described.Format)) == 0u || described.SampleDesc.Count != 1u) return false;
    if (!connect(device) || !make(device, described.Width, described.Height, static_cast<uint32_t>(described.Format))) {
        unavailable_ = true;
        return false;
    }
    slot = next_;
    next_ = (next_ + 1u) % images;
    context->CopySubresourceRegion(images_[slot].texture, 0u, 0u, 0u, 0u, source, 0u, nullptr);
    context->End(images_[slot].done);
    context->Flush();
    return true;
}

bool HostImageExport::copied(ID3D11DeviceContext4 *context, uint32_t slot) const
{
    AV_SCOPE("HostImageExport::copied");
    BOOL done = FALSE;
    return context->GetData(images_[slot].done, &done, sizeof done, D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK && done;
}

void HostImageExport::describe(uint32_t slot, RvkExportedImage &image) const
{
    AV_SCOPE("HostImageExport::describe");
    const Image &shown = images_[slot];
    image.fd = shown.fd;
    image.slot = slot;
    image.width = width_;
    image.height = height_;
    image.stride = shown.stride;
    image.offset = shown.offset;
    image.fourcc = drm_format(format_);
    image.reserved = 0u;
    image.modifier = 0u;   /* DRM_FORMAT_MOD_LINEAR */
}

void HostImageExport::drop(Image &image)
{
    AV_SCOPE("HostImageExport::drop");
    if (image.done != nullptr) image.done->Release();
    if (image.texture != nullptr) image.texture->Release();
    if (image.fd >= 0) close(image.fd);
    if (image.image != VK_NULL_HANDLE) destroy_image_(device_, image.image, nullptr);
    if (image.memory != VK_NULL_HANDLE) free_(device_, image.memory, nullptr);
    image = Image{VK_NULL_HANDLE, VK_NULL_HANDLE, nullptr, nullptr, -1, 0u, 0u};
}

void HostImageExport::release()
{
    AV_SCOPE("HostImageExport::release");
    for (Image &image : images_) drop(image);
    if (interop_ != nullptr) static_cast<IDXGIVkInteropDevice1 *>(interop_)->Release();
    interop_ = nullptr;
}

}  // namespace av
