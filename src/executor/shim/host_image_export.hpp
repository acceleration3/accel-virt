#ifndef AV_HOST_IMAGE_EXPORT_HPP
#define AV_HOST_IMAGE_EXPORT_HPP

#include "device/abi/av_executor.h"
#include "executor/d3d/retired_slots.hpp"
#include "executor/owned_fd.hpp"
#include "executor/shim/vulkan_image.hpp"

#include <stdint.h>
#include <vulkan/vulkan.h>

struct ID3D11Device5;
struct ID3D11DeviceContext4;
struct ID3D11Texture2D;
struct ID3D11Query;

namespace av {

class DxvkVulkanDevice;

class HostImageExport {
public:
    explicit HostImageExport(DxvkVulkanDevice &vulkan);
    ~HostImageExport();
    HostImageExport(const HostImageExport &) = delete;
    HostImageExport &operator=(const HostImageExport &) = delete;

    [[nodiscard]] bool stage(ID3D11Device5 *device, ID3D11DeviceContext4 *context, ID3D11Texture2D *source,
                             uint32_t &slot);
    bool copied(ID3D11DeviceContext4 *context, uint32_t slot) const;
    [[nodiscard]] bool describe(uint32_t slot, AvExportedImage &image) const;
    void release();

private:
    struct Image {
        VulkanImage      image_;
        ID3D11Texture2D *texture_;
        ID3D11Query     *done_;
        OwnedFd          fd_;
    };

    [[nodiscard]] bool make(ID3D11Device5 *device, uint32_t width, uint32_t height, uint32_t format);
    [[nodiscard]] bool make_image(ID3D11Device5 *device, Image &image);
    void               drop(Image &image);
    void               retire_current();
    void               collect();
    [[nodiscard]] bool vacant(uint32_t slot) const;
    [[nodiscard]] bool free_slot(uint32_t &slot);

    DxvkVulkanDevice          &vulkan_;
    Image                      slots_[RetiredSlots::capacity];
    RetiredSlots               retired_;
    uint32_t                   current_;
    uint32_t                   width_;
    uint32_t                   height_;
    uint32_t                   format_;
    bool                       unavailable_;
};

}

#endif
