#ifndef AV_SWAPCHAIN_HPP
#define AV_SWAPCHAIN_HPP

#include "icd/common/present_pacing.hpp"
#include "icd/common/handed_frame.hpp"
#include "icd/common/once_latch.hpp"
#include "icd/vulkan/chained_image.hpp"
#include "icd/vulkan/fence_ready.hpp"
#include "icd/vulkan/present_device.hpp"

#include <stddef.h>
#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {
class DriverLog;
}

namespace av {
class Kmt;
}

namespace av::icd {

class Connection;
class EntryLookup;
class SurfaceProperties;
class Swapchain;
class WindowChains;
class WindowSurface;
struct SwapchainProbe;
template <typename Source, typename Readiness>
class WindowPresentQueue;

using VkPresentQueue = WindowPresentQueue<Swapchain, FenceReady>;
using VkFrame = HandedFrame<Swapchain, FenceReady::Mark>;

class Swapchain {
public:
    Swapchain(WindowSurface &windows, WindowChains &chains, EntryLookup &entries, VkDevice device,
              uint64_t host_device, VkPhysicalDevice physical, Kmt &kmt, DriverLog &log, Connection &connection,
              VkPresentQueue &presents);
    ~Swapchain();
    Swapchain(const Swapchain &) = delete;
    Swapchain &operator=(const Swapchain &) = delete;

    VkResult initialize(const VkSwapchainCreateInfoKHR &info, void *window, SurfaceProperties &surface);
    VkResult images(uint32_t &count, VkImage *images) const;
    VkResult acquire(uint64_t timeout, uint32_t &index);
    VkResult present(VkQueue queue, uint32_t index, uint32_t wait_count, const VkSemaphore *waits);
    void     done(const VkFrame &frame, bool drawn);
    void     retire();
    VkDevice device() const;
    uint64_t presented() const;
    PresentPacing &pacing();

    bool invariants_hold() const;

private:
    [[nodiscard]] VkResult judge(const VkSwapchainCreateInfoKHR &info, SurfaceProperties &surface) const;
    void refused(const char *what, uint64_t asked, uint64_t beside) const;
    enum class Holder : uint8_t { none, application, presents };

    VkResult               make_destination(void *window, VkFormat format, VkExtent2D extent);
    VkResult               hand_over(VkQueue queue, uint32_t index, uint32_t wait_count, const VkSemaphore *waits);
    VkResult               present_now(VkQueue queue, uint32_t index, uint32_t wait_count, const VkSemaphore *waits);
    VkResult               copy(VkQueue queue, uint32_t index, uint32_t wait_count, const VkSemaphore *waits,
                                uint32_t slot);
    VkFrame                frame(uint32_t slot);
    bool                   holds(uint32_t index);
    void                   give(uint32_t index, Holder holder);
    VkResult               window_state() const;
    bool                   shows_bgra() const;
    void                   release();

    WindowSurface &windows_;
    VkPresentQueue &presents_;
    PresentDevice  device_;
    ChainedImage   chained_;
    DriverLog     &lines_;
    PresentPacing  pacing_;
    uint64_t       luid_;
    VkDevice       app_device_;
    void          *window_;
    VkExtent2D     extent_;
    int32_t        format_;
    Holder         holders_[PresentDevice::max_images];
    uint32_t       slot_images_[PresentDevice::slots];
    uint32_t       count_;
    uint32_t       next_;
    bool           through_chain_;
    bool           retired_;
    uint64_t       presented_;
    icd::OnceLatch window_refused_;
    icd::OnceLatch chain_refused_;

    friend struct SwapchainProbe;
};

}

#endif
