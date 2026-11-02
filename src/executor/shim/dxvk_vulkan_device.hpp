#ifndef AV_DXVK_VULKAN_DEVICE_HPP
#define AV_DXVK_VULKAN_DEVICE_HPP

#include "executor/shim/dxvk_queues.hpp"
#include "executor/shim/queue_work.hpp"
#include "executor/shim/vulkan_device_table.hpp"
#include "executor/shim/vulkan_timeline.hpp"

#include <stdint.h>
#include <vulkan/vulkan.h>

struct ID3D11Device5;
struct ID3D11Texture2D;
struct D3D11_TEXTURE2D_DESC1;

namespace av {

class BorrowedQueue;
class OwnedFd;
class DxvkVulkanDevice {
public:
    DxvkVulkanDevice();
    ~DxvkVulkanDevice();
    DxvkVulkanDevice(const DxvkVulkanDevice &) = delete;
    DxvkVulkanDevice &operator=(const DxvkVulkanDevice &) = delete;

    [[nodiscard]] bool       connect(ID3D11Device5 *device);
    bool                     connected() const;
    void                     release();
    const VulkanDeviceTable &table() const;
    BorrowedQueue           &queue();

    [[nodiscard]] bool texture_image(ID3D11Texture2D *texture, VkImage &image, VkImageLayout &layout,
                                     VkImageCreateInfo &described) const;
    [[nodiscard]] bool wrap(const D3D11_TEXTURE2D_DESC1 &desc, VkImage image, ID3D11Texture2D *&texture) const;
    [[nodiscard]] bool import_timeline(OwnedFd &descriptor, VulkanTimeline &timeline) const;
    [[nodiscard]] bool wait(const VulkanTimeline &timeline, uint64_t value);
    [[nodiscard]] bool signal(const VulkanTimeline &timeline, uint64_t value);
    void               idle();
    [[nodiscard]] bool sync_file(int &fd);
private:
    IDXGIVkInteropDevice1 *interop_;
    VulkanDeviceTable      table_;
    DxvkD3d11Queue         queue_;
    QueueWork              work_;
    VkSemaphore            sync_;
    bool                   sync_spent_;
};

}

#endif
