#include "executor/shim/host_image_export.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "executor/shim/dxvk_vulkan_device.hpp"
#include "proto/d3d/dxgi_format.hpp"

#include <unistd.h>

namespace av {

namespace {

/* DRM_FORMAT_MOD_LINEAR, from drm_fourcc.h. */
constexpr uint64_t drm_format_mod_linear = 0u;

}  // namespace

HostImageExport::HostImageExport(DxvkVulkanDevice &vulkan)
    : vulkan_(vulkan), images_{}, width_(0u), height_(0u), format_(0u), next_(0u), unavailable_(false)
{
    AV_SCOPE("HostImageExport::HostImageExport");
    for (Image &image : images_) image.fd_ = -1;
}

HostImageExport::~HostImageExport()
{
    AV_SCOPE("HostImageExport::~HostImageExport");
    release();
}

/* Linear, so the display imports it with the one modifier every driver reads
 * alike - and under that modifier rather than VK_IMAGE_TILING_LINEAR, which a
 * driver may refuse a dma-buf for (RADV from Mesa 26.2). Device local where
 * the GPU allows it, so the copy and the display stay in VRAM. */
bool HostImageExport::make_image(ID3D11Device5 *device, Image &image)
{
    AV_SCOPE("HostImageExport::make_image");
    const uint64_t                                  linear = drm_format_mod_linear;
    const VkImageDrmFormatModifierListCreateInfoEXT modifiers = {
        VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT, nullptr, 1u, &linear};
    VkExternalMemoryImageCreateInfo external = {VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO, &modifiers,
                                                VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT};
    VkImageCreateInfo created = {};
    created.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    created.pNext = &external;
    created.imageType = VK_IMAGE_TYPE_2D;
    created.format = static_cast<VkFormat>(DxgiFormat(format_).vk_format());
    created.extent = {width_, height_, 1u};
    created.mipLevels = 1u;
    created.arrayLayers = 1u;
    created.samples = VK_SAMPLE_COUNT_1_BIT;
    created.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
    created.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    created.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (!vulkan_.table().create_image(created, image.image_)) return false;
    const VkMemoryRequirements needs = vulkan_.table().requirements(image.image_);
    const uint32_t             type = vulkan_.table().memory_type(needs.memoryTypeBits);
    if (type == VK_MAX_MEMORY_TYPES) return false;
    const VkExportMemoryAllocateInfo exported = {VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO, nullptr,
                                                 VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT};
    if (!vulkan_.table().bind_dedicated(image.image_, &exported, needs.size, type, image.memory_)) return false;
    if (!vulkan_.table().memory_fd(image.memory_, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, image.fd_)) return false;
    const VkSubresourceLayout laid = vulkan_.table().row_layout(image.image_);
    image.stride_ = static_cast<uint32_t>(laid.rowPitch);
    image.offset_ = static_cast<uint32_t>(laid.offset);
    D3D11_TEXTURE2D_DESC1 described = {};
    described.Width = width_;
    described.Height = height_;
    described.MipLevels = 1u;
    described.ArraySize = 1u;
    described.Format = static_cast<DXGI_FORMAT>(format_);
    described.SampleDesc = {1u, 0u};
    described.Usage = D3D11_USAGE_DEFAULT;
    if (!vulkan_.wrap(described, image.image_, image.texture_)) return false;
    const D3D11_QUERY_DESC query = {D3D11_QUERY_EVENT, 0u};
    return SUCCEEDED(device->CreateQuery(&query, &image.done_));
}

bool HostImageExport::make(ID3D11Device5 *device, uint32_t width, uint32_t height, uint32_t format)
{
    AV_SCOPE("HostImageExport::make");
    if (width == width_ && height == height_ && format == format_ && images_[0].texture_ != nullptr) return true;
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
    if (!DxgiFormat(static_cast<uint32_t>(described.Format)).scanned_out() || described.SampleDesc.Count != 1u)
        return false;
    if (!vulkan_.connected() || !make(device, described.Width, described.Height, static_cast<uint32_t>(described.Format))) {
        unavailable_ = true;
        return false;
    }
    slot = next_;
    next_ = (next_ + 1u) % images;
    context->CopySubresourceRegion(images_[slot].texture_, 0u, 0u, 0u, 0u, source, 0u, nullptr);
    context->End(images_[slot].done_);
    context->Flush();
    return true;
}

bool HostImageExport::copied(ID3D11DeviceContext4 *context, uint32_t slot) const
{
    AV_SCOPE("HostImageExport::copied");
    BOOL done = FALSE;
    return context->GetData(images_[slot].done_, &done, sizeof done, D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK && done;
}

void HostImageExport::describe(uint32_t slot, AvExportedImage &image) const
{
    AV_SCOPE("HostImageExport::describe");
    const Image &shown = images_[slot];
    image.fd = shown.fd_;
    image.slot = slot;
    image.width = width_;
    image.height = height_;
    image.stride = shown.stride_;
    image.offset = shown.offset_;
    image.fourcc = DxgiFormat(format_).drm_fourcc();
    image.reserved = 0u;
    image.modifier = 0u;   /* DRM_FORMAT_MOD_LINEAR */
}

void HostImageExport::drop(Image &image)
{
    AV_SCOPE("HostImageExport::drop");
    if (image.done_ != nullptr) image.done_->Release();
    if (image.texture_ != nullptr) image.texture_->Release();
    if (image.fd_ >= 0) close(image.fd_);
    vulkan_.table().destroy_image(image.image_);
    vulkan_.table().free_memory(image.memory_);
    image = Image{VK_NULL_HANDLE, VK_NULL_HANDLE, nullptr, nullptr, -1, 0u, 0u};
}

void HostImageExport::release()
{
    AV_SCOPE("HostImageExport::release");
    for (Image &image : images_) drop(image);
}

}  // namespace av
