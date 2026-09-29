/* One VkSwapchainKHR (feature 046, data-model.md): host images handed out in
 * turn, and a presented one copied back and put on its window, paced for FIFO
 * and, for IMMEDIATE, one per refresh with the rest dropped before the copy.
 * Presents are synchronous, so an image is held only between its acquire and
 * its present. */
#ifndef AV_SWAPCHAIN_HPP
#define AV_SWAPCHAIN_HPP

#include "icd/common/frame_pacer.hpp"
#include "icd/vulkan/present_device.hpp"

#include <stddef.h>
#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {
class BlockSource;
class Kmt;
}  // namespace av

namespace av::icd {

class EntryLookup;
class LogSink;
class SurfaceProperties;
class WindowSurface;
struct SwapchainProbe;

class Swapchain {
public:
    Swapchain(BlockSource &memory, WindowSurface &windows, EntryLookup &entries, VkDevice device,
              VkPhysicalDevice physical, Kmt &kmt, LogSink &log, uint32_t adapter, uint32_t kmt_device);
    ~Swapchain();
    Swapchain(const Swapchain &) = delete;
    Swapchain &operator=(const Swapchain &) = delete;

    /* For `window`, as `surface` describes it: VK_ERROR_SURFACE_LOST_KHR for a
     * window that is gone; VK_ERROR_OUT_OF_DATE_KHR for an extent other than
     * the window's; VK_ERROR_INITIALIZATION_FAILED for a 0x0 window or
     * anything contracts/wsi.md refuses; the device's error otherwise, with
     * nothing made. */
    VkResult initialize(const VkSwapchainCreateInfoKHR &info, void *window, SurfaceProperties &surface);
    VkResult images(uint32_t &count, VkImage *images) const;
    /* The next image not held. VK_ERROR_OUT_OF_DATE_KHR once retired or
     * resized; VK_NOT_READY with none free and no timeout, VK_TIMEOUT with
     * one - nothing can free an image while this call waits. */
    VkResult acquire(uint64_t timeout, uint32_t &index);
    /* `index`'s frame on the window. VK_ERROR_UNKNOWN for an index not held;
     * VK_ERROR_OUT_OF_DATE_KHR after a resize and VK_ERROR_SURFACE_LOST_KHR for
     * a gone window, with nothing drawn; the device's error for a failed copy. */
    VkResult present(VkQueue queue, uint32_t index, uint32_t wait_count, const VkSemaphore *waits);
    /* Replaced by a newer swapchain: it hands out nothing more. */
    void     retire();
    VkDevice device() const;
    uint64_t presented() const;
    FramePacer &pacer();

    bool invariants_hold() const;

private:
    [[nodiscard]] VkResult judge(const VkSwapchainCreateInfoKHR &info, SurfaceProperties &surface) const;
    VkResult               window_state() const;
    void                   to_window_order(const uint8_t *rows) const;
    void                   release();

    BlockSource   &memory_;
    WindowSurface &windows_;
    PresentDevice  device_;
    FramePacer     pacer_;
    LogSink       &log_;
    VkDevice       app_device_;
    void          *window_;
    VkExtent2D     extent_;
    /* The application's own values, as integers (foreign_enum.hpp). */
    int32_t        format_;
    int32_t        mode_;
    bool           held_[PresentDevice::max_images];
    uint32_t       count_;
    uint32_t       next_;
    uint8_t       *frame_;
    bool           retired_;
    uint64_t       presented_;

    friend struct SwapchainProbe;
};

}  // namespace av::icd

#endif  // AV_SWAPCHAIN_HPP
