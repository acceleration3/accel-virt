/* One VkSwapchainKHR (feature 046, data-model.md): host images handed out in
 * turn, and a presented one put on its window - copied on the host into the
 * slot of the window's DXGI chain where one can be had (ChainedImage), and
 * otherwise copied back and blitted - paced for FIFO and, for IMMEDIATE, one
 * per refresh with the rest dropped before the copy.
 *
 * A present submits its copy into a free slot and hands the frame to the
 * process's present thread (VkPresentQueue), which waits for the copy's fence
 * and shows it, so the application builds its next frame while the host
 * copies this one. An image is the application's from its acquire to its
 * present, and the thread's from then until its copy is done; an acquire
 * waits for the thread while every image is taken and some are the thread's,
 * as a native swapchain bounds how far ahead an application runs. With no
 * present thread, a present copies and shows its frame itself. */
#ifndef AV_SWAPCHAIN_HPP
#define AV_SWAPCHAIN_HPP

#include "core/log/driver_log.hpp"
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
class LogSink;
}  // namespace av

namespace av {
class Kmt;
}  // namespace av

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

/* The process's present thread for Vulkan's presents. */
using VkPresentQueue = WindowPresentQueue<Swapchain, FenceReady>;
using VkFrame = HandedFrame<Swapchain, FenceReady::Mark>;

class Swapchain {
public:
    /* On `device`, whose guest context `connection` is: the adapter it paces
     * on and the wire a chain's slot is opened through. Its frames are shown
     * by `presents`. */
    Swapchain(WindowSurface &windows, WindowChains &chains, EntryLookup &entries, VkDevice device,
              VkPhysicalDevice physical, Kmt &kmt, LogSink &log, Connection &connection, VkPresentQueue &presents);
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
     * resized; VK_NOT_READY with none free and no timeout; VK_TIMEOUT once
     * the timeout passed with none coming free, and at once when no image is
     * the present thread's - only the thread frees one while this waits. */
    VkResult acquire(uint64_t timeout, uint32_t &index);
    /* `index`'s frame on its way to the window. VK_ERROR_UNKNOWN for an index
     * the application does not hold; VK_ERROR_OUT_OF_DATE_KHR after a resize,
     * and for a retired swapchain whose window's chain is its successor's,
     * and VK_ERROR_SURFACE_LOST_KHR for a gone window, with nothing drawn;
     * the device's error for a copy it refused. A frame the window then
     * refuses is logged by the present thread, and the next acquire or
     * present says why. */
    VkResult present(VkQueue queue, uint32_t index, uint32_t wait_count, const VkSemaphore *waits);
    /* The present thread is done with `frame`: its image is free again, and
     * a copy not seen to its end is waited for before its slot's next. Under
     * the queue's lock. */
    void     done(const VkFrame &frame, bool drawn);
    /* Replaced by a newer swapchain: it hands out nothing more, and gives back
     * its window's chain's slot before the newer one opens it. */
    void     retire();
    VkDevice device() const;
    uint64_t presented() const;
    PresentPacing &pacing();

    bool invariants_hold() const;

private:
    [[nodiscard]] VkResult judge(const VkSwapchainCreateInfoKHR &info, SurfaceProperties &surface) const;
    /* One log line naming what a create asked that this refuses, and the
     * value beside it: what is offered, or the other half of the pair. */
    void refused(const char *what, uint64_t asked, uint64_t beside) const;
    /* Who holds an image: nobody, the application, or the present thread. */
    enum class Holder : uint8_t { none, application, presents };

    /* Through the window's chain where one can be had for `format`, read
     * back otherwise. */
    VkResult               make_destination(void *window, VkFormat format, VkExtent2D extent);
    /* `index`, presented: a copy into a free slot handed to the thread. */
    VkResult               hand_over(VkQueue queue, uint32_t index, uint32_t wait_count, const VkSemaphore *waits);
    /* `index`, presented with no present thread: copied, waited for and
     * shown here. */
    VkResult               present_now(VkQueue queue, uint32_t index, uint32_t wait_count, const VkSemaphore *waits);
    /* `index`'s copy into `slot`, submitted. */
    VkResult               copy(VkQueue queue, uint32_t index, uint32_t wait_count, const VkSemaphore *waits,
                                uint32_t slot);
    /* The frame in `slot` as the queue shows it. */
    VkFrame                frame(uint32_t slot);
    /* Whether the application holds `index`. */
    bool                   holds(uint32_t index);
    /* `index` given to `holder`, under the queue's lock. */
    void                   give(uint32_t index, Holder holder);
    VkResult               window_state() const;
    /* Whether the images are BGRA, a DIB's own channel order. */
    bool                   shows_bgra() const;
    void                   release();

    WindowSurface &windows_;
    VkPresentQueue &presents_;
    PresentDevice  device_;
    ChainedImage   chained_;
    DriverLog      lines_;
    PresentPacing  pacing_;
    LogSink       &log_;
    uint64_t       luid_;
    VkDevice       app_device_;
    void          *window_;
    VkExtent2D     extent_;
    /* The application's own values, as integers (foreign_enum.hpp). */
    int32_t        format_;
    /* Under the queue's lock: the present thread frees an image. */
    Holder         holders_[PresentDevice::max_images];
    /* The image each copy slot's frame is of, while the thread holds it. */
    uint32_t       slot_images_[PresentDevice::slots];
    uint32_t       count_;
    uint32_t       next_;
    /* Whether frames go through the window's chain, which a retired
     * swapchain has given its slot of back. */
    bool           through_chain_;
    bool           retired_;
    uint64_t       presented_;
    /* A window, or its chain, that refuses one frame refuses the next: said
     * once each. */
    icd::OnceLatch window_refused_;
    icd::OnceLatch chain_refused_;

    friend struct SwapchainProbe;
};

}  // namespace av::icd

#endif  // AV_SWAPCHAIN_HPP
