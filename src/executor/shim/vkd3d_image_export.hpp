#ifndef AV_VKD3D_IMAGE_EXPORT_HPP
#define AV_VKD3D_IMAGE_EXPORT_HPP

#include <stdint.h>
#include <vulkan/vulkan.h>

namespace av {

class OwnedFd;

class Vkd3dImageExport {
public:
    static constexpr uint32_t max_view_formats = 16u;
    static constexpr uint32_t max_queue_families = 8u;

    Vkd3dImageExport();
    ~Vkd3dImageExport();
    Vkd3dImageExport(const Vkd3dImageExport &) = delete;
    Vkd3dImageExport &operator=(const Vkd3dImageExport &) = delete;

    void                     take(OwnedFd &memory);
    uint32_t                 memory_type() const;
    VkDeviceSize             size() const;
    const VkImageCreateInfo &image();
    bool                     whole() const;

private:
    int                             fd_;
    uint32_t                        memory_type_;
    uint64_t                        size_;
    VkImageCreateInfo               image_;
    uint32_t                        view_format_count_;
    VkFormat                        view_formats_[max_view_formats];
    uint32_t                        queue_family_count_;
    uint32_t                        queue_families_[max_queue_families];
    VkExternalMemoryImageCreateInfo external_;
    VkImageFormatListCreateInfo     formats_;
};

}

#endif
