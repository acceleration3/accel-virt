#include "device/shim/vkd3d_image_export.hpp"

#include "core/trace/function_scope.hpp"

#include <unistd.h>

namespace av {

Vkd3dImageExport::Vkd3dImageExport()
    : fd_(-1), memory_type_(0u), size_(0u), image_{}, view_format_count_(0u), view_formats_{},
      queue_family_count_(0u), queue_families_{}, external_{}, formats_{}
{
    AV_SCOPE("Vkd3dImageExport::Vkd3dImageExport");
}

Vkd3dImageExport::~Vkd3dImageExport()
{
    AV_SCOPE("Vkd3dImageExport::~Vkd3dImageExport");
    if (fd_ >= 0) close(fd_);
}

int Vkd3dImageExport::take_fd()
{
    AV_SCOPE("Vkd3dImageExport::take_fd");
    const int taken = fd_;
    fd_ = -1;
    return taken;
}

uint32_t Vkd3dImageExport::memory_type() const
{
    AV_SCOPE("Vkd3dImageExport::memory_type");
    return memory_type_;
}

VkDeviceSize Vkd3dImageExport::size() const
{
    AV_SCOPE("Vkd3dImageExport::size");
    return size_;
}

bool Vkd3dImageExport::whole() const
{
    AV_SCOPE("Vkd3dImageExport::whole");
    return fd_ >= 0 && view_format_count_ <= max_view_formats && queue_family_count_ <= max_queue_families;
}

const VkImageCreateInfo &Vkd3dImageExport::image()
{
    AV_SCOPE("Vkd3dImageExport::image");
    external_ = {VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO, nullptr,
                 VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT};
    formats_ = {VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO, &external_, view_format_count_, view_formats_};
    image_.pNext = view_format_count_ != 0u ? static_cast<const void *>(&formats_) : &external_;
    image_.pQueueFamilyIndices = queue_family_count_ != 0u ? queue_families_ : nullptr;
    return image_;
}

}  // namespace av
