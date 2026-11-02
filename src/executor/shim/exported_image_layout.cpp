#include "executor/shim/exported_image_layout.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/vulkan_image.hpp"
#include "proto/d3d/dxgi_format.hpp"

namespace av {

ExportedImageLayout::ExportedImageLayout(const VulkanImage &image, uint32_t width, uint32_t height, uint32_t format,
                                         uint64_t modifier)
    : image_(image), width_(width), height_(height), format_(format), modifier_(modifier)
{
    AV_SCOPE("ExportedImageLayout::ExportedImageLayout");
}

bool ExportedImageLayout::describe(int fd, AvExportedImage &described) const
{
    AV_SCOPE("ExportedImageLayout::describe");
    const uint32_t fourcc = DxgiFormat(format_).drm_fourcc();
    if (fourcc == 0u) return false;
    const VkSubresourceLayout laid = image_.row_layout();
    described.fd = fd;
    described.width = width_;
    described.height = height_;
    described.stride = static_cast<uint32_t>(laid.rowPitch);
    described.offset = static_cast<uint32_t>(laid.offset);
    described.fourcc = fourcc;
    described.reserved = 0u;
    described.modifier = modifier_;
    return true;
}

}
